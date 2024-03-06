#pragma once

#include "bios_attribute.hpp"

namespace pldm::responder::bios
{
class BIOSBootConfigSettingAttribute : public BIOSAttribute
{
  public:
    enum class SupportedOrderedAndFailThroughModes : uint8_t
    {
        UNORDERED_LIMITED_FAIL_THROUGH = 0x00,
        UNORDERED_FAIL_THROUGH = 0x01,
        ORDERED_LIMITED_FAIL_THROUGH = 0x02,
        ORDERED_FAIL_THROUGH = 0x03,
        UNORDERED = 0x04,
        ORDERED = 0x05,
        LIMITED_FAIL_THROUGH = 0x06,
        FAIL_THROUGH = 0x07,
        ALL = 0x08
    };

    inline static const std::map<std::string,
                                 SupportedOrderedAndFailThroughModes>
        supportedOrderedAndFailThroughModesTranslator{
            {"UnorderedAndLimitedFailThrough",
             SupportedOrderedAndFailThroughModes::
                 UNORDERED_LIMITED_FAIL_THROUGH},
            {"UnorderedAndFailThrough",
             SupportedOrderedAndFailThroughModes::UNORDERED_FAIL_THROUGH},
            {"OrderedAndLimitedFailThrough",
             SupportedOrderedAndFailThroughModes::ORDERED_LIMITED_FAIL_THROUGH},
            {"OrderedAndFailThrough",
             SupportedOrderedAndFailThroughModes::ORDERED_FAIL_THROUGH},
            {"Unordered", SupportedOrderedAndFailThroughModes::UNORDERED},
            {"Ordered", SupportedOrderedAndFailThroughModes::ORDERED},
            {"LimitedFailThrough",
             SupportedOrderedAndFailThroughModes::LIMITED_FAIL_THROUGH},
            {"FailThrough", SupportedOrderedAndFailThroughModes::FAIL_THROUGH},
            {"All", SupportedOrderedAndFailThroughModes::ALL}};

    enum class OrderAndFailThroughMode : uint8_t
    {
        UNORDERED_LIMITED_FAIL_THROUGH = 0x00,
        UNORDERED_FAIL_THROUGH = 0x01,
        ORDERED_LIMITED_FAIL_THROUGH = 0x02,
        ORDERED_FAIL_THROUGH = 0x03
    };

    enum class BootConfigType : uint8_t
    {
        UNKNOWN = 0x00,
        DEFAULT = 0x01,
        NEXT = 0x02,
        DEFAULT_NEXT = 0x03,
        ONETIME = 0x04,
        DEFAULT_ONETIME = 0x05
    };

    inline static const std::map<std::string, BootConfigType>
        bootConfigTranslator{
            {"Unknown", BootConfigType::UNKNOWN},
            {"Default", BootConfigType::DEFAULT},
            {"Next", BootConfigType::NEXT},
            {"Default_Next", BootConfigType::DEFAULT_NEXT},
            {"Onetime", BootConfigType::ONETIME},
            {"Default_Onetime", BootConfigType::DEFAULT_ONETIME}};

    /** @brief Construct a bios boot config setting attribute
     *  @param[in] entry - Json Object
     *  @param[in] dbusHandler - Dbus Handler
     */
    BIOSBootConfigSettingAttribute(const Json& entry,
                                   pldm::utils::DBusHandler* const dbusHandler);

    /** @brief Set Attribute value On Dbus according to the attribute value
     *         entry
     *  @param[in] attrValueEntry - The attribute value entry
     *  @param[in] attrEntry - The attribute entry corresponding to the
     *                         attribute value entry
     *  @param[in] stringTable - The string table
     */
    void
        setAttrValueOnDbus(const pldm_bios_attr_val_table_entry* attrValueEntry,
                           const pldm_bios_attr_table_entry* attrEntry,
                           const BIOSStringTable& stringTable) override;

    /** @brief Method to update the value for an Boot Config Setting attribute
     *  @param[in,out] newValue - An attribute value table row with the new
     * value for the attribute
     *  @param[in] attrHdl - attribute handle
     *  @param[in] attrType - attribute type
     *  @param[in] newPropVal - The new value
     *  @return PLDM Success or failure status
     */
    int updateAttrVal(Table& newValue, uint16_t attrHdl, uint8_t attrType,
                      const pldm::utils::PropertyValue& newPropVal) override;

    /** @brief Construct corresponding entries at the end of the attribute table
     *         and attribute value tables
     *  @param[in] stringTable - The string Table
     *  @param[in,out] attrTable - The attribute table
     *  @param[in,out] attrValueTable - The attribute value table
     *  @param[in,out] optAttributeValue - init value of the attribute
     */
    void constructEntry(const BIOSStringTable& stringTable, Table& attrTable,
                        Table& attrValueTable,
                        std::optional<std::variant<int64_t, std::string,
                                                   std::vector<std::string>>>
                            optAttributeValue = std::nullopt) override;

    /** @brief Generate attribute entry by the spec DSP0247_1.0.0 Table 14
     *  @param[in] attributevalue - attribute value
     *  @param[in,out] attrValueEntry - attribute entry
     */
    void generateAttributeEntry(
        const std::variant<int64_t, std::string, std::vector<std::string>>&
            attributevalue,
        Table& attrValueEntry) override;

  private:
    std::vector<uint16_t> getPossibleValuesHandles(
        const BIOSStringTable& stringTable,
        const std::vector<std::string>& possibleBootSources);

    void populateValueDisplayNamesMap(uint16_t attrHandle);

    std::vector<uint8_t> getDefaultIndices();

    /**
     * @brief Returns the index of the given value in the list of possible
     * values.
     * @param[in] value The value to search for.
     * @param[in] possibleValues The vector of possible values.
     * @return The index of the value in the possibleValues vector, or an
     * invalid index if not found.
     */
    uint8_t getValueIndex(const std::string& value,
                          const std::vector<std::string>& possibleValues);

    /** @brief Build the map of dbus value to pldm boot config setting value
     *  @param[in] dbusVals - The dbus values in the json's dbus section
     */
    void buildValMap(const Json& dbusVals);

    using ValMap = std::map<pldm::utils::PropertyValue, std::string>;

    /** @brief Map of value on dbus and pldm */
    ValMap valMap;

    std::vector<std::string> possibleBootSources;
    std::vector<std::string> defaultBootSources;
    std::vector<std::string> valueDisplayNames;
    table::attribute::BootConfigSettingField bootConfigSettingInfo;
};

} // namespace pldm::responder::bios
