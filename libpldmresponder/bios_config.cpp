#include "bios_config.hpp"

#include "bios_enum_attribute.hpp"
#include "bios_integer_attribute.hpp"
#include "bios_string_attribute.hpp"
#include "bios_table.hpp"
#include "common/bios_utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/BIOSConfig/Manager/server.hpp>

#include <filesystem>
#include <fstream>

#ifdef OEM_IBM
#include "oem/ibm/libpldmresponder/platform_oem_ibm.hpp"
#endif

PHOSPHOR_LOG2_USING;

using namespace pldm::utils;

namespace pldm
{
namespace responder
{
namespace bios
{
namespace
{
using BIOSConfigManager =
    sdbusplus::xyz::openbmc_project::BIOSConfig::server::Manager;

constexpr auto attributesJsonFile = "bios_attrs.json";

constexpr auto stringTableFile = "stringTable";
constexpr auto attrTableFile = "attributeTable";
constexpr auto attrValueTableFile = "attributeValueTable";

} // namespace

BIOSConfig::BIOSConfig(
    const char* jsonDir, const char* tableDir, DBusHandler* const dbusHandler,
    int /* fd */, uint8_t eid, pldm::InstanceIdDb* instanceIdDb,
    pldm::requester::Handler<pldm::requester::Request>* handler,
    pldm::responder::platform_config::Handler* platformConfigHandler,
    pldm::responder::bios::Callback requestPLDMServiceName) :
    jsonDir(jsonDir), tableDir(tableDir), dbusHandler(dbusHandler), eid(eid),
    instanceIdDb(instanceIdDb), handler(handler),
    platformConfigHandler(platformConfigHandler),
    requestPLDMServiceName(requestPLDMServiceName)
{
    fs::create_directories(tableDir);
    removeTables();

#ifdef SYSTEM_SPECIFIC_BIOS_JSON
    checkSystemTypeAvailability();
#else
    initBIOSAttributes(sysType, false);
#endif

    listenPendingAttributes();
}

void BIOSConfig::checkSystemTypeAvailability()
{
    if (platformConfigHandler)
    {
        auto systemType = platformConfigHandler->getPlatformName();
        if (systemType.has_value())
        {
            // Received System Type from Entity Manager
            sysType = systemType.value();
            initBIOSAttributes(sysType, true);
        }
        else
        {
            platformConfigHandler->registerSystemTypeCallback(
                std::bind(&BIOSConfig::initBIOSAttributes, this,
                          std::placeholders::_1, std::placeholders::_2));
        }
    }
}

void BIOSConfig::initBIOSAttributes(const std::string& systemType,
                                    bool registerService)
{
    sysType = systemType;
    fs::path dir{jsonDir / sysType};
    if (!fs::exists(dir))
    {
        error("System specific bios attribute directory {DIR} does not exit",
              "DIR", dir);
        if (registerService)
        {
            requestPLDMServiceName();
        }
        return;
    }
    constructAttributes();
    buildTables();
    if (registerService)
    {
        requestPLDMServiceName();
    }
}

void BIOSConfig::buildTables()
{
    auto stringTable = buildAndStoreStringTable();
    if (stringTable)
    {
        buildAndStoreAttrTables(*stringTable);
    }
}

std::optional<Table> BIOSConfig::getBIOSTable(pldm_bios_table_types tableType)
{
    fs::path tablePath;
    switch (tableType)
    {
        case PLDM_BIOS_STRING_TABLE:
            tablePath = tableDir / stringTableFile;
            break;
        case PLDM_BIOS_ATTR_TABLE:
            tablePath = tableDir / attrTableFile;
            break;
        case PLDM_BIOS_ATTR_VAL_TABLE:
            tablePath = tableDir / attrValueTableFile;
            break;
    }
    return loadTable(tablePath);
}

int BIOSConfig::setBIOSTable(uint8_t tableType, const Table& table,
                             bool updateBaseBIOSTable)
{
    fs::path stringTablePath(tableDir / stringTableFile);
    fs::path attrTablePath(tableDir / attrTableFile);
    fs::path attrValueTablePath(tableDir / attrValueTableFile);

    if (!pldm_bios_table_checksum(table.data(), table.size()))
    {
        return PLDM_INVALID_BIOS_TABLE_DATA_INTEGRITY_CHECK;
    }

    if (tableType == PLDM_BIOS_STRING_TABLE)
    {
        storeTable(stringTablePath, table);
    }
    else if (tableType == PLDM_BIOS_ATTR_TABLE)
    {
        BIOSTable biosStringTable(stringTablePath.c_str());
        if (biosStringTable.isEmpty())
        {
            return PLDM_INVALID_BIOS_TABLE_TYPE;
        }

        auto rc = checkAttributeTable(table);
        if (rc != PLDM_SUCCESS)
        {
            return rc;
        }

        storeTable(attrTablePath, table);
    }
    else if (tableType == PLDM_BIOS_ATTR_VAL_TABLE)
    {
        BIOSTable biosStringTable(stringTablePath.c_str());
        BIOSTable biosStringValueTable(attrTablePath.c_str());
        if (biosStringTable.isEmpty() || biosStringValueTable.isEmpty())
        {
            return PLDM_INVALID_BIOS_TABLE_TYPE;
        }

        auto rc = checkAttributeValueTable(table);
        if (rc != PLDM_SUCCESS)
        {
            return rc;
        }

        storeTable(attrValueTablePath, table);
    }
    else
    {
        return PLDM_INVALID_BIOS_TABLE_TYPE;
    }

    if ((tableType == PLDM_BIOS_ATTR_VAL_TABLE) && updateBaseBIOSTable)
    {
        updateBaseBIOSTableProperty();
    }

    return PLDM_SUCCESS;
}

int BIOSConfig::checkAttributeTable(const Table& table)
{
    using namespace pldm::bios::utils;
    auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
    for (auto entry :
         BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(table.data(), table.size()))
    {
        auto attrNameHandle =
            pldm_bios_table_attr_entry_decode_string_handle(entry);

        auto stringEnty = pldm_bios_table_string_find_by_handle(
            stringTable->data(), stringTable->size(), attrNameHandle);
        if (stringEnty == nullptr)
        {
            return PLDM_INVALID_BIOS_ATTR_HANDLE;
        }

        auto attrType = static_cast<pldm_bios_attribute_type>(
            pldm_bios_table_attr_entry_decode_attribute_type(entry));

        switch (attrType)
        {
            case PLDM_BIOS_ENUMERATION:
            case PLDM_BIOS_ENUMERATION_READ_ONLY:
            {
                uint8_t pvNum;
                // Preconditions are upheld therefore no error check necessary
                pldm_bios_table_attr_entry_enum_decode_pv_num(entry, &pvNum);
                std::vector<uint16_t> pvHandls(pvNum);
                // Preconditions are upheld therefore no error check necessary
                pldm_bios_table_attr_entry_enum_decode_pv_hdls(
                    entry, pvHandls.data(), pvHandls.size());
                uint8_t defNum;
                pldm_bios_table_attr_entry_enum_decode_def_num(entry, &defNum);
                std::vector<uint8_t> defIndices(defNum);
                pldm_bios_table_attr_entry_enum_decode_def_indices(
                    entry, defIndices.data(), defIndices.size());

                for (size_t i = 0; i < pvHandls.size(); i++)
                {
                    auto stringEntry = pldm_bios_table_string_find_by_handle(
                        stringTable->data(), stringTable->size(), pvHandls[i]);
                    if (stringEntry == nullptr)
                    {
                        return PLDM_INVALID_BIOS_ATTR_HANDLE;
                    }
                }

                for (size_t i = 0; i < defIndices.size(); i++)
                {
                    auto stringEntry = pldm_bios_table_string_find_by_handle(
                        stringTable->data(), stringTable->size(),
                        pvHandls[defIndices[i]]);
                    if (stringEntry == nullptr)
                    {
                        return PLDM_INVALID_BIOS_ATTR_HANDLE;
                    }
                }
                break;
            }
            case PLDM_BIOS_INTEGER:
            case PLDM_BIOS_INTEGER_READ_ONLY:
            case PLDM_BIOS_STRING:
            case PLDM_BIOS_STRING_READ_ONLY:
            case PLDM_BIOS_PASSWORD:
            case PLDM_BIOS_PASSWORD_READ_ONLY:
                break;
            default:
                return PLDM_INVALID_BIOS_ATTR_HANDLE;
        }
    }

    return PLDM_SUCCESS;
}

int BIOSConfig::checkAttributeValueTable(const Table& table)
{
    using namespace pldm::bios::utils;
    auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
    auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);

    baseBIOSTableMaps.clear();

    for (auto tableEntry :
         BIOSTableIter<PLDM_BIOS_ATTR_VAL_TABLE>(table.data(), table.size()))
    {
        AttributeName attributeName{};
        AttributeType attributeType{};
        ReadonlyStatus readonlyStatus{};
        DisplayName displayName{};
        Description description{};
        MenuPath menuPath{};
        CurrentValue currentValue{};
        DefaultValue defaultValue{};
        std::vector<ValueDisplayName> valueDisplayNames;
        std::map<uint16_t, std::vector<std::string>> valueDisplayNamesMap;
        Option options{};

        auto attrValueHandle =
            pldm_bios_table_attr_value_entry_decode_attribute_handle(
                tableEntry);
        auto attrType = static_cast<pldm_bios_attribute_type>(
            pldm_bios_table_attr_value_entry_decode_attribute_type(tableEntry));

        auto attrEntry = pldm_bios_table_attr_find_by_handle(
            attrTable->data(), attrTable->size(), attrValueHandle);
        if (attrEntry == nullptr)
        {
            return PLDM_INVALID_BIOS_ATTR_HANDLE;
        }
        auto attrHandle =
            pldm_bios_table_attr_entry_decode_attribute_handle(attrEntry);
        auto attrNameHandle =
            pldm_bios_table_attr_entry_decode_string_handle(attrEntry);

        auto stringEntry = pldm_bios_table_string_find_by_handle(
            stringTable->data(), stringTable->size(), attrNameHandle);
        if (stringEntry == nullptr)
        {
            return PLDM_INVALID_BIOS_ATTR_HANDLE;
        }
        auto strLength =
            pldm_bios_table_string_entry_decode_string_length(stringEntry);
        std::vector<char> buffer(strLength + 1 /* sizeof '\0' */);
        // Preconditions are upheld therefore no error check necessary
        pldm_bios_table_string_entry_decode_string(stringEntry, buffer.data(),
                                                   buffer.size());

        attributeName = std::string(buffer.data(), buffer.data() + strLength);

        if (!biosAttributes.empty())
        {
            readonlyStatus =
                biosAttributes[attrHandle % biosAttributes.size()]->readOnly;
            description =
                biosAttributes[attrHandle % biosAttributes.size()]->helpText;
            displayName =
                biosAttributes[attrHandle % biosAttributes.size()]->displayName;
            valueDisplayNamesMap =
                biosAttributes[attrHandle % biosAttributes.size()]
                    ->valueDisplayNamesMap;
        }

        switch (attrType)
        {
            case PLDM_BIOS_ENUMERATION:
            case PLDM_BIOS_ENUMERATION_READ_ONLY:
            {
                if (valueDisplayNamesMap.contains(attrHandle))
                {
                    const std::vector<ValueDisplayName>& vdn =
                        valueDisplayNamesMap[attrHandle];
                    valueDisplayNames.insert(valueDisplayNames.end(),
                                             vdn.begin(), vdn.end());
                }
                auto getValue =
                    [](uint16_t handle, const Table& table) -> std::string {
                    auto stringEntry = pldm_bios_table_string_find_by_handle(
                        table.data(), table.size(), handle);

                    auto strLength =
                        pldm_bios_table_string_entry_decode_string_length(
                            stringEntry);
                    std::vector<char> buffer(strLength + 1 /* sizeof '\0' */);
                    // Preconditions are upheld therefore no error check
                    // necessary
                    pldm_bios_table_string_entry_decode_string(
                        stringEntry, buffer.data(), buffer.size());

                    return std::string(buffer.data(),
                                       buffer.data() + strLength);
                };

                attributeType = "xyz.openbmc_project.BIOSConfig.Manager."
                                "AttributeType.Enumeration";

                uint8_t pvNum;
                // Preconditions are upheld therefore no error check necessary
                pldm_bios_table_attr_entry_enum_decode_pv_num(attrEntry,
                                                              &pvNum);
                std::vector<uint16_t> pvHandls(pvNum);
                // Preconditions are upheld therefore no error check necessary
                pldm_bios_table_attr_entry_enum_decode_pv_hdls(
                    attrEntry, pvHandls.data(), pvHandls.size());

                // get possible_value
                for (size_t i = 0; i < pvHandls.size(); i++)
                {
                    options.push_back(
                        std::make_tuple("xyz.openbmc_project.BIOSConfig."
                                        "Manager.BoundType.OneOf",
                                        getValue(pvHandls[i], *stringTable),
                                        valueDisplayNames[i]));
                }

                auto count =
                    pldm_bios_table_attr_value_entry_enum_decode_number(
                        tableEntry);
                std::vector<uint8_t> handles(count);
                pldm_bios_table_attr_value_entry_enum_decode_handles(
                    tableEntry, handles.data(), handles.size());

                // get current_value
                for (size_t i = 0; i < handles.size(); i++)
                {
                    currentValue = getValue(pvHandls[handles[i]], *stringTable);
                }

                uint8_t defNum;
                // Preconditions are upheld therefore no error check necessary
                pldm_bios_table_attr_entry_enum_decode_def_num(attrEntry,
                                                               &defNum);
                std::vector<uint8_t> defIndices(defNum);
                pldm_bios_table_attr_entry_enum_decode_def_indices(
                    attrEntry, defIndices.data(), defIndices.size());

                // get default_value
                for (size_t i = 0; i < defIndices.size(); i++)
                {
                    defaultValue =
                        getValue(pvHandls[defIndices[i]], *stringTable);
                }

                break;
            }
            case PLDM_BIOS_INTEGER:
            case PLDM_BIOS_INTEGER_READ_ONLY:
            {
                attributeType = "xyz.openbmc_project.BIOSConfig.Manager."
                                "AttributeType.Integer";
                currentValue = static_cast<int64_t>(
                    pldm_bios_table_attr_value_entry_integer_decode_cv(
                        tableEntry));

                uint64_t lower, upper, def;
                uint32_t scalar;
                pldm_bios_table_attr_entry_integer_decode(
                    attrEntry, &lower, &upper, &scalar, &def);
                options.push_back(std::make_tuple(
                    "xyz.openbmc_project.BIOSConfig.Manager."
                    "BoundType.LowerBound",
                    static_cast<int64_t>(lower), attributeName));
                options.push_back(std::make_tuple(
                    "xyz.openbmc_project.BIOSConfig.Manager."
                    "BoundType.UpperBound",
                    static_cast<int64_t>(upper), attributeName));
                options.push_back(std::make_tuple(
                    "xyz.openbmc_project.BIOSConfig.Manager."
                    "BoundType.ScalarIncrement",
                    static_cast<int64_t>(scalar), attributeName));
                defaultValue = static_cast<int64_t>(def);
                break;
            }
            case PLDM_BIOS_STRING:
            case PLDM_BIOS_STRING_READ_ONLY:
            {
                attributeType = "xyz.openbmc_project.BIOSConfig.Manager."
                                "AttributeType.String";
                variable_field currentString;
                pldm_bios_table_attr_value_entry_string_decode_string(
                    tableEntry, &currentString);
                currentValue = std::string(
                    reinterpret_cast<const char*>(currentString.ptr),
                    currentString.length);
                auto min = pldm_bios_table_attr_entry_string_decode_min_length(
                    attrEntry);
                auto max = pldm_bios_table_attr_entry_string_decode_max_length(
                    attrEntry);
                uint16_t def;
                // Preconditions are upheld therefore no error check necessary
                pldm_bios_table_attr_entry_string_decode_def_string_length(
                    attrEntry, &def);
                std::vector<char> defString(def + 1);
                pldm_bios_table_attr_entry_string_decode_def_string(
                    attrEntry, defString.data(), defString.size());
                options.push_back(
                    std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                    "BoundType.MinStringLength",
                                    static_cast<int64_t>(min), attributeName));
                options.push_back(
                    std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                    "BoundType.MaxStringLength",
                                    static_cast<int64_t>(max), attributeName));
                defaultValue = defString.data();
                break;
            }
            case PLDM_BIOS_PASSWORD:
            case PLDM_BIOS_PASSWORD_READ_ONLY:
            {
                attributeType = "xyz.openbmc_project.BIOSConfig.Manager."
                                "AttributeType.Password";
                break;
            }
            default:
                return PLDM_INVALID_BIOS_ATTR_HANDLE;
        }
        baseBIOSTableMaps.emplace(
            std::move(attributeName),
            std::make_tuple(attributeType, readonlyStatus, displayName,
                            description, menuPath, currentValue, defaultValue,
                            std::move(options)));
    }

    return PLDM_SUCCESS;
}

void BIOSConfig::updateBaseBIOSTableProperty()
{
    constexpr static auto biosConfigPath =
        "/xyz/openbmc_project/bios_config/manager";
    constexpr static auto biosConfigInterface =
        "xyz.openbmc_project.BIOSConfig.Manager";
    constexpr static auto biosConfigPropertyName = "BaseBIOSTable";
    constexpr static auto dbusProperties = "org.freedesktop.DBus.Properties";

    if (baseBIOSTableMaps.empty())
    {
        return;
    }

    try
    {
        auto& bus = dbusHandler->getBus();
        auto service =
            dbusHandler->getService(biosConfigPath, biosConfigInterface);
        auto method = bus.new_method_call(service.c_str(), biosConfigPath,
                                          dbusProperties, "Set");
        std::variant<BaseBIOSTable> value = baseBIOSTableMaps;
#ifdef OEM_IBM
        const fs::path bootSideDirPath = "/var/lib/pldm/bootSide";
        for (const auto& [attrName, biostabObj] : baseBIOSTableMaps)
        {
            // The additional check to see if /var/lib/pldm/bootSide file exists
            // is added to make sure we are doing the fw_boot_side setting after
            // the base bios table is initialised.
            if ((attrName == "fw_boot_side") && fs::exists(bootSideDirPath))
            {
                BiosAttributeList biosAttrList;
                std::string nextBootSide =
                    std::get<std::string>(std::get<5>(biostabObj));
                std::string currNextBootSide = getBiosAttrValue("fw_boot_side");
                if (currNextBootSide != nextBootSide)
                {
                    biosAttrList.push_back(
                        std::make_pair(attrName, nextBootSide));
                    setBiosAttr(biosAttrList);
                }
            }
        }
#endif
        method.append(biosConfigInterface, biosConfigPropertyName, value);
        bus.call_noreply(method, dbusTimeout);
    }
    catch (const std::exception& e)
    {
        error("Failed to update BaseBIOSTable property, error - {ERROR}",
              "ERROR", e);
    }
}

void BIOSConfig::constructAttributes()
{
    info("Bios Attribute file path: {PATH}", "PATH",
         (jsonDir / sysType / attributesJsonFile));
    load(jsonDir / sysType / attributesJsonFile, [this](const Json& entry) {
        std::string attrType = entry.at("attribute_type");
        if (attrType == "string")
        {
            constructAttribute<BIOSStringAttribute>(entry);
        }
        else if (attrType == "integer")
        {
            constructAttribute<BIOSIntegerAttribute>(entry);
        }
        else if (attrType == "enum")
        {
            constructAttribute<BIOSEnumAttribute>(entry);
        }
    });
}

void BIOSConfig::buildAndStoreAttrTables(const Table& stringTable)
{
    BIOSStringTable biosStringTable(stringTable);

    if (biosAttributes.empty())
    {
        return;
    }

    BaseBIOSTable biosTable{};
    constexpr auto biosObjPath = "/xyz/openbmc_project/bios_config/manager";
    constexpr auto biosInterface = "xyz.openbmc_project.BIOSConfig.Manager";

    try
    {
        auto& bus = dbusHandler->getBus();
        auto service = dbusHandler->getService(biosObjPath, biosInterface);
        auto method =
            bus.new_method_call(service.c_str(), biosObjPath,
                                "org.freedesktop.DBus.Properties", "Get");
        method.append(biosInterface, "BaseBIOSTable");
        auto reply = bus.call(method, dbusTimeout);
        std::variant<BaseBIOSTable> varBiosTable{};
        reply.read(varBiosTable);
        biosTable = std::get<BaseBIOSTable>(varBiosTable);
    }
    // Failed to read the BaseBIOSTable, so update the BaseBIOSTable with the
    // default values populated from the BIOS JSONs to keep PLDM and
    // bios-settings-manager in sync
    catch (const std::exception& e)
    {
        error("Failed to read BaseBIOSTable property, error - {ERROR}", "ERROR",
              e);
    }

    Table attrTable, attrValueTable;

    for (auto& attr : biosAttributes)
    {
        try
        {
            auto iter = biosTable.find(attr->name);
            if (iter == biosTable.end())
            {
                attr->constructEntry(biosStringTable, attrTable, attrValueTable,
                                     std::nullopt);
            }
            else
            {
                attr->constructEntry(
                    biosStringTable, attrTable, attrValueTable,
                    std::get<static_cast<uint8_t>(Index::currentValue)>(
                        iter->second));
            }
        }
        catch (const std::exception& e)
        {
            error(
                "Failed to construct table entry for attribute '{ATTRIBUTE}', error - {ERROR}",
                "ATTRIBUTE", attr->name, "ERROR", e);
        }
    }

    table::appendPadAndChecksum(attrTable);
    table::appendPadAndChecksum(attrValueTable);
    setBIOSTable(PLDM_BIOS_ATTR_TABLE, attrTable);
    setBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE, attrValueTable);
}

std::optional<Table> BIOSConfig::buildAndStoreStringTable()
{
    std::set<std::string> strings;
    load(jsonDir / sysType / attributesJsonFile, [&strings](const Json& entry) {
        if (entry.at("attribute_type") == "enum")
        {
            strings.emplace(entry.at("attribute_name"));
            auto possibleValues = entry.at("possible_values");
            for (auto& pv : possibleValues)
            {
                strings.emplace(pv);
            }
        }
        else
        {
            strings.emplace(entry.at("attribute_name"));
        }
    });

    if (strings.empty())
    {
        return std::nullopt;
    }

    Table table;
    for (const auto& elem : strings)
    {
        table::string::constructEntry(table, elem);
    }

    table::appendPadAndChecksum(table);
    setBIOSTable(PLDM_BIOS_STRING_TABLE, table);
    return table;
}

void BIOSConfig::storeTable(const fs::path& path, const Table& table)
{
    BIOSTable biosTable(path.c_str());
    biosTable.store(table);
}

std::optional<Table> BIOSConfig::loadTable(const fs::path& path)
{
    BIOSTable biosTable(path.c_str());
    if (biosTable.isEmpty())
    {
        return std::nullopt;
    }

    Table table;
    biosTable.load(table);
    return table;
}

void BIOSConfig::load(const fs::path& filePath, ParseHandler handler)
{
    std::ifstream file;
    Json jsonConf;
    if (fs::exists(filePath))
    {
        try
        {
            file.open(filePath);
            jsonConf = Json::parse(file);
            auto entries = jsonConf.at("entries");
            for (auto& entry : entries)
            {
                try
                {
                    handler(entry);
                }
                catch (const std::exception& e)
                {
                    error(
                        "Failed to parse JSON config file at path '{PATH}', error - {ERROR}",
                        "PATH", filePath, "ERROR", e);
                }
            }
        }
        catch (const std::exception& e)
        {
            error("Failed to parse JSON config file '{PATH}', error - {ERROR}",
                  "PATH", filePath, "ERROR", e);
        }
    }
}

std::string BIOSConfig::decodeStringFromStringEntry(
    const pldm_bios_string_table_entry* stringEntry)
{
    auto strLength =
        pldm_bios_table_string_entry_decode_string_length(stringEntry);
    std::vector<char> buffer(strLength + 1 /* sizeof '\0' */);
    // Preconditions are upheld therefore no error check necessary
    pldm_bios_table_string_entry_decode_string(stringEntry, buffer.data(),
                                               buffer.size());
    return std::string(buffer.data(), buffer.data() + strLength);
}

std::string BIOSConfig::displayStringHandle(
    uint16_t handle, uint8_t index, const std::optional<Table>& attrTable,
    const std::optional<Table>& stringTable)
{
    auto attrEntry = pldm_bios_table_attr_find_by_handle(
        attrTable->data(), attrTable->size(), handle);
    uint8_t pvNum;
    int rc = pldm_bios_table_attr_entry_enum_decode_pv_num(attrEntry, &pvNum);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to decode BIOS table possible values for attribute entry, response code '{RC}'",
            "RC", rc);
        throw std::runtime_error(
            "Failed to decode BIOS table possible values for attribute entry");
    }

    std::vector<uint16_t> pvHandls(pvNum);
    // Preconditions are upheld therefore no error check necessary
    pldm_bios_table_attr_entry_enum_decode_pv_hdls(attrEntry, pvHandls.data(),
                                                   pvHandls.size());

    std::string displayString = std::to_string(pvHandls[index]);

    auto stringEntry = pldm_bios_table_string_find_by_handle(
        stringTable->data(), stringTable->size(), pvHandls[index]);

    auto decodedStr = decodeStringFromStringEntry(stringEntry);

    return decodedStr + "(" + displayString + ")";
}

void BIOSConfig::traceBIOSUpdate(
    const pldm_bios_attr_val_table_entry* attrValueEntry,
    const pldm_bios_attr_table_entry* attrEntry, bool isBMC)
{
    auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
    auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);

    auto [attrHandle,
          attrType] = table::attribute_value::decodeHeader(attrValueEntry);

    auto attrHeader = table::attribute::decodeHeader(attrEntry);
    BIOSStringTable biosStringTable(*stringTable);
    auto attrName = biosStringTable.findString(attrHeader.stringHandle);

    switch (attrType)
    {
        case PLDM_BIOS_ENUMERATION:
        case PLDM_BIOS_ENUMERATION_READ_ONLY:
        {
            auto count = pldm_bios_table_attr_value_entry_enum_decode_number(
                attrValueEntry);
            std::vector<uint8_t> handles(count);
            pldm_bios_table_attr_value_entry_enum_decode_handles(
                attrValueEntry, handles.data(), handles.size());

            for (uint8_t handle : handles)
            {
                auto nwVal = displayStringHandle(attrHandle, handle, attrTable,
                                                 stringTable);
                auto chkBMC = isBMC ? "true" : "false";
                info(
                    "BIOS attribute '{ATTRIBUTE}' updated to value '{VALUE}' by BMC '{CHECK_BMC}'",
                    "ATTRIBUTE", attrName, "VALUE", nwVal, "CHECK_BMC", chkBMC);
            }
            break;
        }
        case PLDM_BIOS_INTEGER:
        case PLDM_BIOS_INTEGER_READ_ONLY:
        {
            auto value =
                table::attribute_value::decodeIntegerEntry(attrValueEntry);
            auto chkBMC = isBMC ? "true" : "false";
            info(
                "BIOS attribute '{ATTRIBUTE}' updated to value '{VALUE}' by BMC '{CHECK_BMC}'",
                "ATTRIBUTE", attrName, "VALUE", value, "CHECK_BMC", chkBMC);
            break;
        }
        case PLDM_BIOS_STRING:
        case PLDM_BIOS_STRING_READ_ONLY:
        {
            auto value =
                table::attribute_value::decodeStringEntry(attrValueEntry);
            auto chkBMC = isBMC ? "true" : "false";
            info(
                "BIOS attribute '{ATTRIBUTE}' updated to value '{VALUE}' by BMC '{CHECK_BMC}'",
                "ATTRIBUTE", attrName, "VALUE", value, "CHECK_BMC", chkBMC);
            break;
        }
        default:
            break;
    };
}

int BIOSConfig::checkAttrValueToUpdate(
    const pldm_bios_attr_val_table_entry* attrValueEntry,
    const pldm_bios_attr_table_entry* attrEntry, Table&)

{
    auto [attrHandle,
          attrType] = table::attribute_value::decodeHeader(attrValueEntry);

    switch (attrType)
    {
        case PLDM_BIOS_ENUMERATION:
        case PLDM_BIOS_ENUMERATION_READ_ONLY:
        {
            auto value =
                table::attribute_value::decodeEnumEntry(attrValueEntry);
            auto [pvHdls,
                  defIndex] = table::attribute::decodeEnumEntry(attrEntry);
            if (!(value.size() == 1))
            {
                return PLDM_ERROR_INVALID_LENGTH;
            }
            if (value[0] >= pvHdls.size())
            {
                error(
                    "Invalid index '{INDEX}' encountered for Enum type BIOS attribute",
                    "INDEX", value[0]);
                return PLDM_ERROR_INVALID_DATA;
            }
            return PLDM_SUCCESS;
        }
        case PLDM_BIOS_INTEGER:
        case PLDM_BIOS_INTEGER_READ_ONLY:
        {
            auto value =
                table::attribute_value::decodeIntegerEntry(attrValueEntry);
            auto [lower, upper, scalar,
                  def] = table::attribute::decodeIntegerEntry(attrEntry);

            if (value < lower || value > upper)
            {
                error(
                    "Out of range index '{ATTRIBUTE_VALUE}' encountered for Integer type BIOS attribute for the lower bound '{LOWER}', the upper bound '{UPPER}' and the scalar value '{SCALAR}'.",
                    "ATTRIBUTE_VALUE", value, "LOWER", lower, "UPPER", upper,
                    "SCALAR", scalar);
                return PLDM_ERROR_INVALID_DATA;
            }
            return PLDM_SUCCESS;
        }
        case PLDM_BIOS_STRING:
        case PLDM_BIOS_STRING_READ_ONLY:
        {
            auto stringConf = table::attribute::decodeStringEntry(attrEntry);
            auto value =
                table::attribute_value::decodeStringEntry(attrValueEntry);
            if (value.size() < stringConf.minLength ||
                value.size() > stringConf.maxLength)
            {
                error(
                    "Invalid length '{LENGTH}' encountered for string type BIOS attribute value '{ATTRIBUTE_VALUE}' when minimum string entry length '{MIN_LEN}' and maximum string entry length '{MAX_LEN}'",
                    "ATTRIBUTE_VALUE", value, "LENGTH", value.size(), "MIN_LEN",
                    stringConf.minLength, "MAX_LEN", stringConf.maxLength);
                return PLDM_ERROR_INVALID_LENGTH;
            }
            return PLDM_SUCCESS;
        }
        default:
            error("ReadOnly or Unsupported type '{TYPE}'", "TYPE", attrType);
            return PLDM_ERROR;
    };
}

int BIOSConfig::setAttrValue(const void* entry, size_t size, bool isBMC,
                             bool updateDBus, bool updateBaseBIOSTable)
{
    auto attrValueTable = getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);
    auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);
    auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
    if (!attrValueTable || !attrTable || !stringTable)
    {
        return PLDM_BIOS_TABLE_UNAVAILABLE;
    }

    auto attrValueEntry =
        reinterpret_cast<const pldm_bios_attr_val_table_entry*>(entry);

    auto attrValHeader = table::attribute_value::decodeHeader(attrValueEntry);

    auto attrEntry =
        table::attribute::findByHandle(*attrTable, attrValHeader.attrHandle);
    if (!attrEntry)
    {
        return PLDM_ERROR;
    }

    auto rc = checkAttrValueToUpdate(attrValueEntry, attrEntry, *stringTable);
    if (rc != PLDM_SUCCESS)
    {
        return rc;
    }

    auto destTable =
        table::attribute_value::updateTable(*attrValueTable, entry, size);

    if (!destTable)
    {
        return PLDM_ERROR;
    }

    try
    {
        auto attrHeader = table::attribute::decodeHeader(attrEntry);

        BIOSStringTable biosStringTable(*stringTable);
        auto attrName = biosStringTable.findString(attrHeader.stringHandle);
        auto iter = std::find_if(
            biosAttributes.begin(), biosAttributes.end(),
            [&attrName](const auto& attr) { return attr->name == attrName; });

        if (iter == biosAttributes.end())
        {
            return PLDM_ERROR;
        }
        if (updateDBus)
        {
            (*iter)->setAttrValueOnDbus(attrValueEntry, attrEntry,
                                        biosStringTable);
        }
    }
    catch (const std::exception& e)
    {
        error("Set attribute value error - {ERROR}", "ERROR", e);
        return PLDM_ERROR;
    }

    setBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE, *destTable, updateBaseBIOSTable);

    traceBIOSUpdate(attrValueEntry, attrEntry, isBMC);

    return PLDM_SUCCESS;
}

void BIOSConfig::removeTables()
{
    try
    {
        fs::remove(tableDir / stringTableFile);
        fs::remove(tableDir / attrTableFile);
        fs::remove(tableDir / attrValueTableFile);
    }
    catch (const std::exception& e)
    {
        error("Remove the tables error - {ERROR}", "ERROR", e);
    }
}

void BIOSConfig::processBiosAttrChangeNotification(
    const DbusChObjProperties& chProperties, uint32_t biosAttrIndex)
{
    const auto& dBusMap = biosAttributes[biosAttrIndex]->getDBusMap();
    const auto& propertyName = dBusMap->propertyName;
    const auto& attrName = biosAttributes[biosAttrIndex]->name;

    const auto it = chProperties.find(propertyName);
    if (it == chProperties.end())
    {
        return;
    }

    PropertyValue newPropVal = it->second;
    auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
    if (!stringTable.has_value())
    {
        error("BIOS string table unavailable");
        return;
    }
    BIOSStringTable biosStringTable(*stringTable);
    uint16_t attrNameHdl{};
    try
    {
        attrNameHdl = biosStringTable.findHandle(attrName);
    }
    catch (const std::invalid_argument& e)
    {
        error(
            "Missing handle for attribute '{ATTRIBUTE}' in BIOS String Table, error - '{ERROR}'",
            "ATTRIBUTE", attrName, "ERROR", e);
        return;
    }

    auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);
    if (!attrTable.has_value())
    {
        error("BIOS Attribute table not present");
        return;
    }
    const struct pldm_bios_attr_table_entry* tableEntry =
        table::attribute::findByStringHandle(*attrTable, attrNameHdl);
    if (tableEntry == nullptr)
    {
        error(
            "Failed to find attribute {ATTRIBUTE} in BIOS Attribute table with attribute handle '{ATTR_HANDLE}'",
            "ATTRIBUTE", attrName, "ATTR_HANDLE", attrNameHdl);
        return;
    }

    auto [attrHdl, attrType,
          stringHdl] = table::attribute::decodeHeader(tableEntry);

    auto attrValueSrcTable = getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

    if (!attrValueSrcTable.has_value())
    {
        error("Attribute value table not present");
        return;
    }

    Table newValue;
    auto rc = biosAttributes[biosAttrIndex]->updateAttrVal(
        newValue, attrHdl, attrType, newPropVal);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to update the attribute value table for attribute handle '{ATTR_HANDLE}' and  attribute type '{TYPE}'",
            "ATTR_HANDLE", attrHdl, "TYPE", attrType);
        return;
    }
    auto destTable = table::attribute_value::updateTable(
        *attrValueSrcTable, newValue.data(), newValue.size());
    if (destTable.has_value())
    {
        storeTable(tableDir / attrValueTableFile, *destTable);
    }

    rc = setAttrValue(newValue.data(), newValue.size(), true, false);
    if (rc != PLDM_SUCCESS)
    {
        error(
            "Failed to setAttrValue on base bios table and dbus, response code '{RC}'",
            "RC", rc);
    }
}

uint16_t BIOSConfig::findAttrHandle(const std::string& attrName)
{
    auto stringTable = getBIOSTable(PLDM_BIOS_STRING_TABLE);
    auto attrTable = getBIOSTable(PLDM_BIOS_ATTR_TABLE);

    BIOSStringTable biosStringTable(*stringTable);
    pldm::bios::utils::BIOSTableIter<PLDM_BIOS_ATTR_TABLE> attrTableIter(
        attrTable->data(), attrTable->size());
    auto stringHandle = biosStringTable.findHandle(attrName);

    for (auto entry : pldm::bios::utils::BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(
             attrTable->data(), attrTable->size()))
    {
        auto header = table::attribute::decodeHeader(entry);
        if (header.stringHandle == stringHandle)
        {
            return header.attrHandle;
        }
    }

    throw std::invalid_argument("Unknown attribute Name");
}

void BIOSConfig::constructPendingAttribute(
    const PendingAttributes& pendingAttributes)
{
    std::vector<uint16_t> listOfHandles{};

    for (auto& attribute : pendingAttributes)
    {
        std::string attributeName = attribute.first;
        auto& [attributeType, attributevalue] = attribute.second;

        auto iter = std::find_if(biosAttributes.begin(), biosAttributes.end(),
                                 [&attributeName](const auto& attr) {
                                     return attr->name == attributeName;
                                 });

        if (iter == biosAttributes.end())
        {
            error("Wrong attribute name {NAME}", "NAME", attributeName);
            continue;
        }

        Table attrValueEntry(sizeof(pldm_bios_attr_val_table_entry), 0);
        auto entry = reinterpret_cast<pldm_bios_attr_val_table_entry*>(
            attrValueEntry.data());

        auto handler = findAttrHandle(attributeName);
        auto type =
            BIOSConfigManager::convertAttributeTypeFromString(attributeType);

        if (type != BIOSConfigManager::AttributeType::Enumeration &&
            type != BIOSConfigManager::AttributeType::String &&
            type != BIOSConfigManager::AttributeType::Integer)
        {
            error("Attribute type '{TYPE}' not supported", "TYPE",
                  attributeType);
            continue;
        }

        const auto [attrType, readonlyStatus, displayName, description,
                    menuPath, currentValue, defaultValue,
                    option] = baseBIOSTableMaps.at(attributeName);

        entry->attr_handle = htole16(handler);

        // Need to verify that the current value has really changed
        if (attributeType == attrType && attributevalue != currentValue)
        {
            listOfHandles.emplace_back(htole16(handler));
        }

        (*iter)->generateAttributeEntry(attributevalue, attrValueEntry);

        setAttrValue(attrValueEntry.data(), attrValueEntry.size(), true);
    }

    if (listOfHandles.size())
    {
#ifdef OEM_IBM
        auto rc = pldm::responder::platform::sendBiosAttributeUpdateEvent(
            eid, instanceIdDb, listOfHandles, handler);
        if (rc != PLDM_SUCCESS)
        {
            return;
        }
#endif
    }
}

void BIOSConfig::listenPendingAttributes()
{
    constexpr auto objPath = "/xyz/openbmc_project/bios_config/manager";
    constexpr auto objInterface = "xyz.openbmc_project.BIOSConfig.Manager";

    using namespace sdbusplus::bus::match::rules;
    auto updateBIOSMatch = std::make_unique<sdbusplus::bus::match_t>(
        pldm::utils::DBusHandler::getBus(),
        propertiesChanged(objPath, objInterface),
        [this](sdbusplus::message_t& msg) {
            constexpr auto propertyName = "PendingAttributes";

            using Value =
                std::variant<std::string, PendingAttributes, BaseBIOSTable>;
            using Properties = std::map<DbusProp, Value>;

            Properties props{};
            std::string intf;
            msg.read(intf, props);

            auto valPropMap = props.find(propertyName);
            if (valPropMap == props.end())
            {
                return;
            }

            PendingAttributes pendingAttributes =
                std::get<PendingAttributes>(valPropMap->second);
            this->constructPendingAttribute(pendingAttributes);
        });

    biosAttrMatch.emplace_back(std::move(updateBIOSMatch));
}

} // namespace bios
} // namespace responder
} // namespace pldm
