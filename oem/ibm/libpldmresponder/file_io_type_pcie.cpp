#include "file_io_type_pcie.hpp"

#include <fcntl.h>
#include <libpldm/base.h>
#include <libpldm/oem/ibm/file_io.h>
#include <sys/stat.h>

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <cstring>
#include <utility>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{

std::unordered_map<uint8_t, std::string> linkStateMap{
    {0x00, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Operational"},
    {0x01, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Degraded"},
    {0x02, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Unused"},
    {0x03, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Unused"},
    {0x04, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Failed"},
    {0x05, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Open"},
    {0x06, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Inactive"},
    {0x07, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Unused"},
    {0xFF, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Status.Unknown"}};

std::unordered_map<uint8_t, std::string> linkSpeed{
    {0x00, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Generations.Gen1"},
    {0x01, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Generations.Gen2"},
    {0x02, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Generations.Gen3"},
    {0x03, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Generations.Gen4"},
    {0x04, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Generations.Gen5"},
    {0x10, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Generations.Unknown"},
    {0xFF, "xyz.openbmc_project.Inventory.Item.PCIeSlot.Generations.Unknown"}};

std::unordered_map<uint8_t, size_t> linkWidth{
    {0x01, 1},  {0x02, 2},        {0x04, 4}, {0x08, 8},
    {0x10, 16}, {0xFF, UINT_MAX}, {0x00, 0}};

std::unordered_map<uint8_t, double> cableLengthMap{
    {0x00, 0},  {0x01, 2},  {0x02, 3},
    {0x03, 10}, {0x04, 20}, {0xFF, std::numeric_limits<double>::quiet_NaN()}};

std::unordered_map<uint8_t, std::string> cableTypeMap{
    {0x00, "optical"}, {0x01, "copper"}, {0xFF, "Unknown"}};

std::unordered_map<uint8_t, std::string> cableStatusMap{
    {0x00, "xyz.openbmc_project.Inventory.Item.Cable.Status.Inactive"},
    {0x01, "xyz.openbmc_project.Inventory.Item.Cable.Status.Running"},
    {0x02, "xyz.openbmc_project.Inventory.Item.Cable.Status.PoweredOff"},
    {0xFF, "xyz.openbmc_project.Inventory.Item.Cable.Status.Unknown"}};

constexpr auto topologyFile = "topology";
constexpr auto cableInfoFile = "cableinfo";

// Slot location code structure contains multiple slot location code
// suffix structures.
// Each slot location code suffix structure is as follows
// {Slot location code suffix size(uint8_t),
//  Slot location code suffix(variable size)}
constexpr auto sizeOfSuffixSizeDataMember = 1;

// Each slot location structure contains
// {
//   Number of slot location codes (1byte),
//   Slot location code Common part size (1byte)
//   Slot location common part (Var)
// }
constexpr auto slotLocationDataMemberSize = 2;

constexpr bool isRangeValid(size_t containerSize, size_t offset, size_t length)
{
    return offset <= containerSize && length <= containerSize - offset;
}

namespace fs = std::filesystem;

std::unordered_map<uint16_t, bool> PCIeInfoHandler::receivedFiles;
std::unordered_map<LinkId, std::tuple<LinkStatus, linkTypeData, LinkSpeed,
                                      LinkWidth, PcieHostBridgeLoc, LocalPort,
                                      RemotePort, IoSlotLocation, LinkId>>
    PCIeInfoHandler::topologyInformation;
std::unordered_map<
    CableLinkNum, std::tuple<LinkId, LocalPortLocCode, IoSlotLocationCode,
                             CablePartNum, CableLength, CableType, CableStatus>>
    PCIeInfoHandler::cableInformation;
std::unordered_map<LinkId, linkTypeData> PCIeInfoHandler::linkTypeInfo;

PCIeInfoHandler::PCIeInfoHandler(uint32_t fileHandle, uint16_t fileType,
                                 fs::path pciePath) :
    FileHandler(fileHandle), infoType(fileType), pciePath(std::move(pciePath))
{
    receivedFiles.emplace(infoType, false);
}

int PCIeInfoHandler::writeFromMemory(
    uint32_t offset, uint32_t length, uint64_t address,
    oem_platform::Handler* /*oemPlatformHandler*/)
{
    if (!fs::exists(pciePath))
    {
        fs::create_directories(pciePath);
        fs::permissions(pciePath,
                        fs::perms::others_read | fs::perms::owner_write);
    }

    fs::path infoFile(fs::path(pciePath) / topologyFile);
    if (infoType == PLDM_FILE_TYPE_CABLE_INFO)
    {
        infoFile = (fs::path(pciePath) / cableInfoFile);
    }

    try
    {
        std::ofstream pcieData(infoFile, std::ios::out | std::ios::binary);
        auto rc = transferFileData(infoFile, false, offset, length, address);
        if (rc != PLDM_SUCCESS)
        {
            error("TransferFileData failed in PCIeTopology with error {ERROR}",
                  "ERROR", rc);
            return rc;
        }
        return PLDM_SUCCESS;
    }
    catch (const std::exception& e)
    {
        error("Create/Write data to the File type {TYPE}, failed {ERROR}",
              "TYPE", infoType, "ERROR", e);
        return PLDM_ERROR;
    }
}

int PCIeInfoHandler::write(const char* buffer, uint32_t, uint32_t& length,
                           oem_platform::Handler* /*oemPlatformHandler*/)
{
    fs::path infoFile(fs::path(pciePath) / topologyFile);
    if (infoType == PLDM_FILE_TYPE_CABLE_INFO)
    {
        infoFile = (fs::path(pciePath) / cableInfoFile);
    }

    try
    {
        std::ofstream pcieData(infoFile, std::ios::out | std::ios::binary |
                                             std::ios::app);

        if (!buffer)
        {
            pcieData.write(buffer, length);
        }
        pcieData.close();
    }
    catch (const std::exception& e)
    {
        error("Create/Write data to the File type {TYPE}, failed {ERROR}",
              "TYPE", infoType, "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

int PCIeInfoHandler::fileAck(uint8_t /*fileStatus*/)
{
    receivedFiles[infoType] = true;
    try
    {
        if (receivedFiles.at(PLDM_FILE_TYPE_CABLE_INFO) &&
            receivedFiles.at(PLDM_FILE_TYPE_PCIE_TOPOLOGY))
        {
            receivedFiles.clear();
            // parse the topology data and cache the information
            // for further processing
            parseTopologyData();
        }
    }
    catch (const std::out_of_range& e)
    {
        info("Received only one of the topology file");
    }
    return PLDM_SUCCESS;
}

bool PCIeInfoHandler::parseTopologyData()
{
    int fd = open((fs::path(pciePath) / topologyFile).string().c_str(),
                  O_RDONLY, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("Topology file not present");
        return false;
    }
    pldm::utils::CustomFD topologyFd(fd);
    // Reading the statistics of the topology file, to get the size.
    // stat sb is the out parameter provided to fstat
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("Could not get topology file size");
        return false;
    }

    constexpr size_t topologyHeaderSize = offsetof(topologyBlob, pciLinkEntry);
    if (sb.st_size < static_cast<off_t>(topologyHeaderSize))
    {
        error("Topology file is smaller than its header");
        return false;
    }

    auto topologyCleanup = [sb](void* fileInMemory) {
        munmap(fileInMemory, sb.st_size);
    };

    // memory map the topology file into pldm memory
    void* fileInMemory =
        mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, topologyFd(), 0);
    if (MAP_FAILED == fileInMemory)
    {
        error("mmap on topology file failed with error {RC}", "RC", -errno);
        return false;
    }

    std::unique_ptr<void, decltype(topologyCleanup)> topologyPtr(
        fileInMemory, topologyCleanup);

    auto pcieLinkList = reinterpret_cast<struct topologyBlob*>(fileInMemory);
    uint16_t numOfLinks = 0;
    if (!pcieLinkList)
    {
        error("Parsing of topology file failed : pcieLinkList is null");
        return false;
    }

    // The elements in the structure that were being parsed from the obtained
    // files via write() and writeFromMemory() were written by IBM enterprise
    // host firmware which runs on big-endian format. The DMA engine in BMC
    // (aspeed-xdma device driver) which is responsible for exposing the data
    // shared by the host in the shared VGA memory has no idea of the structure
    // of the data it's copying from host. So it's up to the consumers of the
    // data to deal with the endianness once it has been copied to user space &
    // and as pldm is the first application layer that interprets the data
    // provided by host reading from the sysfs interface of xdma-engine & also
    // since pldm application runs on BMC which could be little/big endian, its
    // essential to swap the endianness to agreed big-endian format (htobe)
    // before using the data.

    numOfLinks = htobe16(pcieLinkList->numPcieLinkEntries);

    // To fetch the PCIe link from the topology data, move past the header.
    struct pcieLinkEntry* singleEntryData =
        reinterpret_cast<struct pcieLinkEntry*>(
            reinterpret_cast<uint8_t*>(pcieLinkList) + topologyHeaderSize);

    if (!singleEntryData)
    {
        error("Parsing of topology file failed : single_link is null");
        return false;
    }

    // iterate over every pcie link and get the link specific attributes
    for ([[maybe_unused]] const auto& link :
         std::views::iota(0) | std::views::take(numOfLinks))
    {
        constexpr size_t entryHeaderSize =
            offsetof(pcieLinkEntry, pciLinkEntryLocCode);
        const auto* fileStart = static_cast<const uint8_t*>(fileInMemory);
        const auto* entryStart =
            reinterpret_cast<const uint8_t*>(singleEntryData);
        const size_t entryOffset = entryStart - fileStart;
        const size_t fileSize = static_cast<size_t>(sb.st_size);
        if (!isRangeValid(fileSize, entryOffset, entryHeaderSize))
        {
            error("PCIe topology entry header exceeds file bounds");
            return false;
        }
        const size_t entryLength = htobe16(singleEntryData->entryLength);
        if (entryLength < entryHeaderSize ||
            !isRangeValid(fileSize, entryOffset, entryLength))
        {
            error("Invalid PCIe topology entry length {LENGTH}", "LENGTH",
                  entryLength);
            return false;
        }
        auto singleEntryDataCharStream =
            reinterpret_cast<char*>(singleEntryData);
        const auto validEntryRange =
            [entryLength](uint16_t offset, uint8_t length) {
                return isRangeValid(entryLength, htobe16(offset), length);
            };

        // get the link id
        auto linkId = htobe16(singleEntryData->linkId);

        // get parent link id
        auto parentLinkId = htobe16(singleEntryData->parentLinkId);

        // get link status
        auto linkStatus = singleEntryData->linkStatus;

        // get link type
        auto type = singleEntryData->linkType;
        if (type != pldm::responder::linkTypeData::Unknown)
        {
            linkTypeInfo[linkId] = type;
        }

        // get link speed
        auto linkSpeed = singleEntryData->linkSpeed;

        // get link width
        auto width = singleEntryData->linkWidth;

        // get the PCIe Host Bridge Location
        size_t pcieLocCodeSize = singleEntryData->pcieHostBridgeLocCodeSize;
        if (!validEntryRange(singleEntryData->pcieHostBridgeLocCodeOff,
                             singleEntryData->pcieHostBridgeLocCodeSize) ||
            !validEntryRange(singleEntryData->topLocalPortLocCodeOff,
                             singleEntryData->topLocalPortLocCodeSize) ||
            !validEntryRange(singleEntryData->bottomLocalPortLocCodeOff,
                             singleEntryData->bottomLocalPortLocCodeSize) ||
            !validEntryRange(singleEntryData->topRemotePortLocCodeOff,
                             singleEntryData->topRemotePortLocCodeSize) ||
            !validEntryRange(singleEntryData->bottomRemotePortLocCodeOff,
                             singleEntryData->bottomRemotePortLocCodeSize))
        {
            error("PCIe topology location code exceeds entry bounds");
            return false;
        }
        std::vector<char> pcieHostBridgeLocation(
            singleEntryDataCharStream +
                htobe16(singleEntryData->pcieHostBridgeLocCodeOff),
            singleEntryDataCharStream +
                htobe16(singleEntryData->pcieHostBridgeLocCodeOff) +
                static_cast<int>(pcieLocCodeSize));
        std::string pcieHostBridgeLocationCode(pcieHostBridgeLocation.begin(),
                                               pcieHostBridgeLocation.end());

        // get the local port - top location
        size_t localTopPortLocSize = singleEntryData->topLocalPortLocCodeSize;
        std::vector<char> localTopPortLocation(
            singleEntryDataCharStream +
                htobe16(singleEntryData->topLocalPortLocCodeOff),
            singleEntryDataCharStream +
                htobe16(singleEntryData->topLocalPortLocCodeOff) +
                static_cast<int>(localTopPortLocSize));
        std::string localTopPortLocationCode(localTopPortLocation.begin(),
                                             localTopPortLocation.end());

        // get the local port - bottom location
        size_t localBottomPortLocSize =
            singleEntryData->bottomLocalPortLocCodeSize;
        std::vector<char> localBottomPortLocation(
            singleEntryDataCharStream +
                htobe16(singleEntryData->bottomLocalPortLocCodeOff),
            singleEntryDataCharStream +
                htobe16(singleEntryData->bottomLocalPortLocCodeOff) +
                static_cast<int>(localBottomPortLocSize));
        std::string localBottomPortLocationCode(localBottomPortLocation.begin(),
                                                localBottomPortLocation.end());

        // get the remote port - top location
        size_t remoteTopPortLocSize = singleEntryData->topRemotePortLocCodeSize;
        std::vector<char> remoteTopPortLocation(
            singleEntryDataCharStream +
                htobe16(singleEntryData->topRemotePortLocCodeOff),
            singleEntryDataCharStream +
                htobe16(singleEntryData->topRemotePortLocCodeOff) +
                static_cast<int>(remoteTopPortLocSize));
        std::string remoteTopPortLocationCode(remoteTopPortLocation.begin(),
                                              remoteTopPortLocation.end());

        // get the remote port - bottom location
        size_t remoteBottomLocSize =
            singleEntryData->bottomRemotePortLocCodeSize;
        std::vector<char> remoteBottomPortLocation(
            singleEntryDataCharStream +
                htobe16(singleEntryData->bottomRemotePortLocCodeOff),
            singleEntryDataCharStream +
                htobe16(singleEntryData->bottomRemotePortLocCodeOff) +
                static_cast<int>(remoteBottomLocSize));
        std::string remoteBottomPortLocationCode(
            remoteBottomPortLocation.begin(), remoteBottomPortLocation.end());

        const size_t slotOffset = htobe16(singleEntryData->slotLocCodesOffset);
        if (!isRangeValid(entryLength, slotOffset, slotLocationDataMemberSize))
        {
            error("PCIe topology slot data exceeds entry bounds");
            return false;
        }
        struct slotLocCode* slotData = reinterpret_cast<struct slotLocCode*>(
            reinterpret_cast<uint8_t*>(singleEntryData) + slotOffset);
        // get the Slot location code common part
        size_t numOfSlots = slotData->numSlotLocCodes;
        size_t slotLocCodeCompartSize = slotData->slotLocCodesCmnPrtSize;
        size_t suffixOffset =
            slotOffset + slotLocationDataMemberSize + slotLocCodeCompartSize;
        if (!isRangeValid(entryLength, slotOffset + slotLocationDataMemberSize,
                          slotLocCodeCompartSize))
        {
            error("PCIe topology slot common data exceeds entry bounds");
            return false;
        }
        std::vector<char> slotLocation(
            reinterpret_cast<char*>(slotData->slotLocCodesCmnPrt),
            (reinterpret_cast<char*>(slotData->slotLocCodesCmnPrt) +
             static_cast<int>(slotLocCodeCompartSize)));
        std::string slotLocationCode(slotLocation.begin(), slotLocation.end());

        uint8_t* suffixData =
            reinterpret_cast<uint8_t*>(slotData) + slotLocationDataMemberSize +
            slotData->slotLocCodesCmnPrtSize;

        // create the full slot location code by combining common part and
        // suffix part
        std::string slotSuffixLocationCode;
        std::vector<std::string> slotFinaLocationCode{};
        for ([[maybe_unused]] const auto& slot :
             std::views::iota(0) | std::views::take(numOfSlots))
        {
            if (!isRangeValid(entryLength, suffixOffset,
                              sizeOfSuffixSizeDataMember))
            {
                error("PCIe topology slot suffix exceeds entry bounds");
                return false;
            }
            struct slotLocCodeSuf* slotLocSufData =
                reinterpret_cast<struct slotLocCodeSuf*>(suffixData);

            size_t slotLocCodeSuffixSize = slotLocSufData->slotLocCodeSz;
            if (!isRangeValid(entryLength,
                              suffixOffset + sizeOfSuffixSizeDataMember,
                              slotLocCodeSuffixSize))
            {
                error("PCIe topology slot suffix exceeds entry bounds");
                return false;
            }
            if (slotLocCodeSuffixSize > 0)
            {
                std::vector<char> slotSuffixLocation(
                    reinterpret_cast<char*>(slotLocSufData) + 1,
                    reinterpret_cast<char*>(slotLocSufData) + 1 +
                        static_cast<int>(slotLocCodeSuffixSize));
                std::string slotSuffLocationCode(slotSuffixLocation.begin(),
                                                 slotSuffixLocation.end());

                slotSuffixLocationCode = slotSuffLocationCode;
            }
            std::string slotFullLocationCode =
                slotLocationCode + slotSuffixLocationCode;
            slotFinaLocationCode.push_back(slotFullLocationCode);

            // move the pointer to next slot
            suffixData += sizeOfSuffixSizeDataMember + slotLocCodeSuffixSize;
            suffixOffset += sizeOfSuffixSizeDataMember + slotLocCodeSuffixSize;
        }

        // store the information into a map
        topologyInformation[linkId] = std::make_tuple(
            linkStateMap[linkStatus], type, linkSpeed, linkWidth[width],
            pcieHostBridgeLocationCode,
            std::make_pair(localTopPortLocationCode,
                           localBottomPortLocationCode),
            std::make_pair(remoteTopPortLocationCode,
                           remoteBottomPortLocationCode),
            slotFinaLocationCode, parentLinkId);

        // move the pointer to next link
        singleEntryData = reinterpret_cast<struct pcieLinkEntry*>(
            reinterpret_cast<uint8_t*>(singleEntryData) + entryLength);
    }
    // Need to call cable info at the end , because we dont want to parse
    // cable info without parsing the successful topology successfully
    // Having partial information is of no use.
    return parseCableInfo();
}

bool PCIeInfoHandler::parseCableInfo()
{
    int fd = open((fs::path(pciePath) / cableInfoFile).string().c_str(),
                  O_RDONLY, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("CableInfo file not present");
        return false;
    }
    pldm::utils::CustomFD cableInfoFd(fd);
    struct stat sb;

    if (fstat(fd, &sb) == -1)
    {
        perror("Could not get cableinfo file size");
        return false;
    }

    constexpr size_t cableHeaderSize =
        offsetof(cableAttributesList, pciLinkCableAttr);
    if (sb.st_size < static_cast<off_t>(cableHeaderSize))
    {
        error("Cable information file is smaller than its header");
        return false;
    }

    auto cableInfoCleanup = [sb](void* fileInMemory) {
        munmap(fileInMemory, sb.st_size);
    };

    void* fileInMemory =
        mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, cableInfoFd(), 0);

    if (MAP_FAILED == fileInMemory)
    {
        int rc = -errno;
        error("mmap on cable ifno file failed, RC={RC}", "RC", rc);
        return false;
    }

    std::unique_ptr<void, decltype(cableInfoCleanup)> cablePtr(
        fileInMemory, cableInfoCleanup);

    auto cableList =
        reinterpret_cast<struct cableAttributesList*>(fileInMemory);

    // get number of cable links
    auto numOfCableLinks = htobe16(cableList->numOfCables);

    struct pcieLinkCableAttr* cableData =
        reinterpret_cast<struct pcieLinkCableAttr*>(
            (reinterpret_cast<uint8_t*>(cableList)) + cableHeaderSize);

    if (!cableData)
    {
        error("Cable info parsing failed , cableData = nullptr");
        return false;
    }

    // iterate over each pci cable link
    for (const auto& cable :
         std::views::iota(0) | std::views::take(numOfCableLinks))
    {
        constexpr size_t entryHeaderSize =
            offsetof(pcieLinkCableAttr, cableAttrLocCode);
        const auto* fileStart = static_cast<const uint8_t*>(fileInMemory);
        const auto* entryStart = reinterpret_cast<const uint8_t*>(cableData);
        const size_t entryOffset = entryStart - fileStart;
        const size_t fileSize = static_cast<size_t>(sb.st_size);
        if (!isRangeValid(fileSize, entryOffset, entryHeaderSize))
        {
            error("PCIe cable entry header exceeds file bounds");
            return false;
        }
        pcieLinkCableAttr entryData{};
        std::memcpy(&entryData, entryStart, entryHeaderSize);
        const size_t entryLength = htobe16(entryData.entryLength);
        if (entryLength < entryHeaderSize ||
            !isRangeValid(fileSize, entryOffset, entryLength))
        {
            error("Invalid PCIe cable entry length {LENGTH}", "LENGTH",
                  entryLength);
            return false;
        }
        if (!isRangeValid(entryLength,
                          htobe16(entryData.hostPortLocationCodeOffset),
                          entryData.hostPortLocationCodeSize) ||
            !isRangeValid(entryLength,
                          htobe16(entryData.ioEnclosurePortLocationCodeOffset),
                          entryData.ioEnclosurePortLocationCodeSize) ||
            !isRangeValid(entryLength, htobe16(entryData.cablePartNumberOffset),
                          entryData.cablePartNumberSize))
        {
            error("PCIe cable location code exceeds entry bounds");
            return false;
        }
        // get the link id
        auto linkId = htobe16(entryData.linkId);
        char* cableDataPtr = reinterpret_cast<char*>(cableData);

        std::string localPortLocCode(
            cableDataPtr + htobe16(entryData.hostPortLocationCodeOffset),
            entryData.hostPortLocationCodeSize);

        std::string ioSlotLocationCode(
            cableDataPtr + htobe16(entryData.ioEnclosurePortLocationCodeOffset),
            entryData.ioEnclosurePortLocationCodeSize);

        std::string cablePartNum(
            cableDataPtr + htobe16(entryData.cablePartNumberOffset),
            entryData.cablePartNumberSize);

        // cache the data into a map
        cableInformation[cable] = std::make_tuple(
            linkId, localPortLocCode, ioSlotLocationCode, cablePartNum,
            cableLengthMap[entryData.cableLength],
            cableTypeMap[entryData.cableType],
            cableStatusMap[entryData.cableStatus]);
        // move the cable data pointer

        cableData = reinterpret_cast<struct pcieLinkCableAttr*>(
            reinterpret_cast<uint8_t*>(cableData) + entryLength);
    }
    return true;
}

} // namespace responder
} // namespace pldm
