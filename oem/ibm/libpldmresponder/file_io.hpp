#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/ibm/requester/dbus_to_file_handler.hpp"
#include "oem_ibm_handler.hpp"
#include "pldmd/PldmRespInterface.hpp"
#include "pldmd/handler.hpp"
#include "requester/handler.hpp"

#include <fcntl.h>
#include <libpldm/base.h>
#include <libpldm/file_io.h>
#include <libpldm/host.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

PHOSPHOR_LOG2_USING;

using namespace sdeventplus;
using namespace sdeventplus::source;

namespace pldm
{
namespace responder
{
namespace dma
{

constexpr auto xdmaDev = "/dev/aspeed-xdma";

struct IOPart
{
    uint32_t length;
    uint32_t offset;
    uint64_t address;
};

// The minimum data size of dma transfer in bytes
constexpr uint32_t minSize = 16;

constexpr size_t maxSize = DMA_MAXSIZE;

namespace fs = std::filesystem;

/**
 * @class DMA
 *
 * Expose API to initiate transfer of data by DMA
 *
 * This class only exposes the public API transferDataHost to transfer data
 * between BMC and host using DMA. This allows for mocking the
 * transferDataHost for unit testing purposes.
 */

class DMA
{
    /** @brief DMA constructor is private to avoid object creation
     * without lengh.
     */
    DMA() {}

  public:
    /** @brief DMA constructor
     * @param length - using length allocating shared memory to transfer data.
     */
    DMA(uint32_t length)
    {
        responseReceived = false;
        memAddr = nullptr;
        xdmaFd = -1;
        SourceFd = -1;
        rc = 0;
        iotPtr = nullptr;
        iotPtrbc = nullptr;
        timer = nullptr;
        m_length = length;
        static const size_t pageSize = getpagesize();
        uint32_t numPages = m_length / pageSize;
        pageAlignedLength = numPages * pageSize;
        if (m_length > pageAlignedLength)
        {
            pageAlignedLength += pageSize;
        }
    }

    /** @brief DMA destructor
     */
    ~DMA()
    {
        if (iotPtr != nullptr)
        {
            iotPtrbc = iotPtr.release();
        }

        if (iotPtrbc != nullptr)
        {
            delete iotPtrbc;
            iotPtrbc = nullptr;
        }

        if (timer != nullptr)
        {
            auto time = timer.release();
            delete time;
        }

        if (xdmaFd > 0)
        {
            close(xdmaFd);
            xdmaFd = -1;
        }
        if (SourceFd > 0)
        {
            close(SourceFd);
            SourceFd = -1;
        }
    }

    /** @brief Method to fetch the shared memory file descriptor for data
     * transfer
     * @param[in] nonBlokingMode - flag to open socket as non blocking mode
     * @param[in] Reopensocket - flag to reopen socket socket
     * @return returns shared memory file descriptor
     *
     */
    int getDMAFd(bool nonBlokingMode, bool Reopensocket)
    {
        if (Reopensocket && xdmaFd > 0)
        {
            close(xdmaFd);
            xdmaFd = -1;
        }
        if (xdmaFd > 0)
        {
            return xdmaFd;
        }
        if (nonBlokingMode)
        {
            xdmaFd = open(xdmaDev, O_RDWR | O_NONBLOCK);
        }
        else
        {
            xdmaFd = open(xdmaDev, O_RDWR);
        }
        if (xdmaFd < 0)
        {
            rc = -errno;
        }
        return xdmaFd;
    }

    /// @brief function will keep one copy of fd for exception case so it can
    /// close it.
    /// @param[in] fd- source path file descriptor
    void setDMASourceFd(int32_t fd)
    {
        SourceFd = fd;
    }

    /// @brief function will keep one copy of shared memory fd for exception
    /// case so it can close it.
    /// @param[in] fd - xdma shared memory path file descriptor
    void setXDMASourceFd(int fd)
    {
        xdmaFd = fd;
    }

    /// @brief function will return pagealignedlength to allocate memory for
    /// data transfer.
    int32_t getpageAlignedLength()
    {
        return pageAlignedLength;
    }

    ///
    /// @brief function will return shared memory address
    /// from XDMA drive path
    void* getXDMAsharedlocation()
    {
        memAddr = mmap(nullptr, pageAlignedLength, PROT_WRITE | PROT_READ,
                       MAP_SHARED, xdmaFd, 0);
        if (MAP_FAILED == memAddr)
        {
            rc = -errno;
            return MAP_FAILED;
        }

        return memAddr;
    }

    /** @brief Method to initialize IO instance for event loop
     * @param[in] ioptr -  pointer to manage eventloop
     */
    void insertIOInstance(std::unique_ptr<IO>&& ioptr)
    {
        iotPtr = std::move(ioptr);
    }

    /** @brief Method to initialize timer for each tranfer
     * @param[in] event -
     * @param[in] callback -
     * @return returns true if timer creation success else false
     *
     */
    bool initTimer(
        sdeventplus::Event& event,
        fu2::unique_function<void(Timer&, Timer::TimePoint)>&& callback)
    {
        try
        {
            timer = std::make_unique<Timer>(
                event, (Clock(event).now() + std::chrono::seconds{20}),
                std::chrono::seconds{1}, std::move(callback));
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Failed to start the timer for event loop. error = "
                      << e.what() << "\n";
            return false;
        }
        return true;
    }

    /** @brief Method to delete cyclic dependecy while deleting object
     *  DMA interface and IO event loop has cyclic dependecy
     *
     * @return void
     */
    void deleteIOInstance()
    {
        if (timer != nullptr)
        {
            auto time = timer.release();
            delete time;
        }
        if (iotPtr != nullptr)
        {
            iotPtrbc = iotPtr.release();
        }
    }

    /** @brief Method to set value for response received
     *
     *
     * @return returns void
     *
     */
    void setResponseReceived(bool bresponse)
    {
        responseReceived = bresponse;
    }

    /** @brief Method to get value of responseReceived to know tranfer
     * success/fail.
     *
     *
     * @return returns true if transfer success else false.
     *
     */
    bool getResponseReceived()
    {
        return responseReceived;
    }

    /** @brief API to transfer data between BMC and host using DMA
     *
     * @param[in] path     - pathname of the file to transfer data from or
     * to
     * @param[in] offset   - offset in the file
     * @param[in] length   - length of the data to transfer
     * @param[in] address  - DMA address on the host
     * @param[in] upstream - indicates direction of the transfer; true
     * indicates transfer to the host
     *
     * @return returns 0 on success, negative errno on failure
     */
    int32_t transferDataHost(int fd, uint32_t offset, uint32_t length,
                             uint64_t address, bool upstream);

    /** @brief API to transfer data on to unix socket from host using DMA
     *
     * @param[in] path     - pathname of the file to transfer data from or
     * to
     * @param[in] length   - length of the data to transfer
     * @param[in] address  - DMA address on the host
     *
     * @return returns 0 on success, negative errno on failure
     */
    int transferHostDataToSocket(int fd, uint32_t length, uint64_t address);

  private:
    bool responseReceived;
    void* memAddr;
    int xdmaFd;
    int32_t SourceFd;
    uint32_t pageAlignedLength;
    int rc;
    std::unique_ptr<IO> iotPtr;
    IO* iotPtrbc;
    std::unique_ptr<Timer> timer;
    uint32_t m_length;
};

/** @brief Transfer the data between BMC and host using DMA.
 *
 *  There is a max size for each DMA operation, transferAll API abstracts this
 *  and the requested length is broken down into multiple DMA operations if the
 *  length exceed max size.
 *
 * @tparam[in] T - DMA interface type
 * @param[in] intf - interface passed to invoke DMA transfer
 * @param[in] command  - PLDM command
 * @param[in] path     - pathname of the file to transfer data from or to
 * @param[in] offset   - offset in the file
 * @param[in] length   - length of the data to transfer
 * @param[in] address  - DMA address on the host
 * @param[in] upstream - indicates direction of the transfer; true indicates
 *                       transfer to the host
 * @param[in] instanceId - Message's instance id
 * @param[in] responseHdr- contain response interface related data
 * @return PLDM response message
 */

template <class DMAInterface>
Response transferAll(std::shared_ptr<DMAInterface> intf, int32_t file,
                     uint32_t offset, uint32_t length, uint64_t address,
                     bool upstream, ResponseHdr& responseHdr,
                     sdeventplus::Event& event)
{
    uint key = responseHdr.key;
    uint8_t instance_id = responseHdr.instance_id;
    uint8_t command = responseHdr.command;
    if (nullptr == intf)
    {
        Response response(sizeof(pldm_msg_hdr) + command, 0);
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        encode_rw_file_memory_resp(instance_id, command, PLDM_ERROR, 0,
                                   responsePtr);
        if (responseHdr.respInterface != nullptr)
        {
            responseHdr.respInterface->sendPLDMRespMsg(response, key);
        }
        close(file);
        return {};
    }
    intf->setDMASourceFd(file);
    uint32_t origLength = length;
    static auto& bus = pldm::utils::DBusHandler::getBus();
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
    std::weak_ptr<dma::DMA> wInterface = intf;

    static IOPart part;
    part.length = length;
    part.offset = offset;
    part.address = address;

    auto timerCb = [=](Timer& /*source*/, Timer::TimePoint /*time*/) {
        if (!intf->getResponseReceived())
        {
            error(
                "EventLoop Timeout..!! Terminating data tranfer operation.\n");
            Response response(sizeof(pldm_msg_hdr) + command, 0);
            auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
            encode_rw_file_memory_resp(instance_id, command, PLDM_ERROR, 0,
                                       responsePtr);
            if (responseHdr.respInterface != nullptr)
            {
                responseHdr.respInterface->sendPLDMRespMsg(response, key);
            }
            intf->deleteIOInstance();
            (static_cast<std::shared_ptr<dma::DMA>>(intf)).reset();
        }
        return;
    };

    int xdmaFd = intf->getDMAFd(true, true);
    auto callback = [=](IO&, int, uint32_t revents) {
        if (!(revents & (EPOLLIN | EPOLLOUT)))
        {
            return;
        }
        auto weakPtr = wInterface.lock();
        Response response(sizeof(pldm_msg_hdr) + command, 0);
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        int rc = 0;

        while (part.length > dma::maxSize)
        {
            rc = weakPtr->transferDataHost(file, part.offset, dma::maxSize,
                                           part.address, upstream);

            part.length -= dma::maxSize;
            part.offset += dma::maxSize;
            part.address += dma::maxSize;
            if (rc < 0)
            {
                encode_rw_file_memory_resp(instance_id, command, PLDM_ERROR, 0,
                                           responsePtr);
                if (responseHdr.respInterface != nullptr)
                {
                    responseHdr.respInterface->sendPLDMRespMsg(response, key);
                }
                weakPtr->deleteIOInstance();
                (static_cast<std::shared_ptr<dma::DMA>>(wInterface)).reset();
                return;
            }
        }
        rc = weakPtr->transferDataHost(file, part.offset, part.length,
                                       part.address, upstream);
        if (rc < 0)
        {
            encode_rw_file_memory_resp(instance_id, command, PLDM_ERROR, 0,
                                       responsePtr);
            if (responseHdr.respInterface != nullptr)
            {
                responseHdr.respInterface->sendPLDMRespMsg(response, key);
            }
            weakPtr->deleteIOInstance();
            (static_cast<std::shared_ptr<dma::DMA>>(wInterface)).reset();
            return;
        }
        if (static_cast<int>(part.length) == rc)
        {
            weakPtr->setResponseReceived(true);
            encode_rw_file_memory_resp(instance_id, command, PLDM_SUCCESS,
                                       origLength, responsePtr);
            if (responseHdr.respInterface != nullptr)
            {
                responseHdr.respInterface->sendPLDMRespMsg(response, key);
            }
            weakPtr->deleteIOInstance();
            (static_cast<std::shared_ptr<dma::DMA>>(wInterface)).reset();
            return;
        }
    };

    try
    {
        if (intf->initTimer(event, std::move(timerCb)) == false)
        {
            Response response(sizeof(pldm_msg_hdr) + command, 0);
            auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
            error("Failed to start the event timer.\n");
            encode_rw_file_memory_resp(instance_id, command, PLDM_ERROR, 0,
                                       responsePtr);
            if (responseHdr.respInterface != nullptr)
            {
                responseHdr.respInterface->sendPLDMRespMsg(response, key);
            }
            intf->deleteIOInstance();
            (static_cast<std::shared_ptr<dma::DMA>>(intf)).reset();
            return {};
        }
        intf->insertIOInstance(std::move(std::make_unique<IO>(
            event, xdmaFd, EPOLLIN | EPOLLOUT, std::move(callback))));
    }
    catch (const std::runtime_error& e)
    {
        Response response(sizeof(pldm_msg_hdr) + command, 0);
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        error("Failed to start the event loop. error ={ERR_EXCEP} ",
              "ERR_EXCEP", e.what());
        encode_rw_file_memory_resp(instance_id, command, PLDM_ERROR, 0,
                                   responsePtr);
        if (responseHdr.respInterface != nullptr)
        {
            responseHdr.respInterface->sendPLDMRespMsg(response, key);
        }
        intf->deleteIOInstance();
        (static_cast<std::shared_ptr<dma::DMA>>(intf)).reset();
    }
    return {};
}

} // namespace dma

namespace oem_ibm
{
static constexpr auto dumpObjPath = "/xyz/openbmc_project/dump/resource/entry/";
static constexpr auto resDumpEntry = "com.ibm.Dump.Entry.Resource";

static constexpr auto certObjPath = "/xyz/openbmc_project/certs/ca/";
static constexpr auto certAuthority =
    "xyz.openbmc_project.PLDM.Provider.Certs.Authority.CSR";
class Handler : public CmdHandler
{
  public:
    Handler(oem_platform::Handler* oemPlatformHandler, int hostSockFd,
            uint8_t hostEid, pldm::InstanceIdDb* instanceIdDb,
            pldm::requester::Handler<pldm::requester::Request>* handler,
            pldm::response_api::Transport* respInterface) :
        oemPlatformHandler(oemPlatformHandler),
        hostSockFd(hostSockFd), hostEid(hostEid), instanceIdDb(instanceIdDb),
        handler(handler), responseHdr({0, 0, respInterface, 0, -1})
    {
        handlers.emplace(PLDM_READ_FILE_INTO_MEMORY,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->readFileIntoMemory(request, payloadLength);
        });
        handlers.emplace(PLDM_WRITE_FILE_FROM_MEMORY,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->writeFileFromMemory(request, payloadLength);
        });
        handlers.emplace(PLDM_WRITE_FILE_BY_TYPE_FROM_MEMORY,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->writeFileByTypeFromMemory(request, payloadLength);
        });
        handlers.emplace(PLDM_READ_FILE_BY_TYPE_INTO_MEMORY,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->readFileByTypeIntoMemory(request, payloadLength);
        });
        handlers.emplace(PLDM_READ_FILE_BY_TYPE,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->readFileByType(request, payloadLength);
        });
        handlers.emplace(PLDM_WRITE_FILE_BY_TYPE,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->writeFileByType(request, payloadLength);
        });
        handlers.emplace(PLDM_GET_FILE_TABLE,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->getFileTable(request, payloadLength);
        });
        handlers.emplace(PLDM_READ_FILE,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->readFile(request, payloadLength);
        });
        handlers.emplace(PLDM_WRITE_FILE,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->writeFile(request, payloadLength);
        });
        handlers.emplace(PLDM_FILE_ACK,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->fileAck(request, payloadLength);
        });
        handlers.emplace(PLDM_HOST_GET_ALERT_STATUS,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->getAlertStatus(request, payloadLength);
        });
        handlers.emplace(PLDM_NEW_FILE_AVAILABLE,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->newFileAvailable(request, payloadLength);
        });

        resDumpMatcher = std::make_unique<sdbusplus::bus::match_t>(
            pldm::utils::DBusHandler::getBus(),
            sdbusplus::bus::match::rules::interfacesAdded() +
                sdbusplus::bus::match::rules::argNpath(0, dumpObjPath),
            [this, hostSockFd, hostEid, instanceIdDb,
             handler](sdbusplus::message_t& msg) {
            std::map<std::string,
                     std::map<std::string, std::variant<std::string, uint32_t>>>
                interfaces;
            sdbusplus::message::object_path path;
            msg.read(path, interfaces);
            std::string vspstring;
            std::string password;

            for (auto& interface : interfaces)
            {
                if (interface.first == resDumpEntry)
                {
                    for (const auto& property : interface.second)
                    {
                        if (property.first == "VSPString")
                        {
                            vspstring = std::get<std::string>(property.second);
                        }
                        else if (property.first == "Password")
                        {
                            password = std::get<std::string>(property.second);
                        }
                    }
                    dbusToFileHandlers
                        .emplace_back(
                            std::make_unique<
                                pldm::requester::oem_ibm::DbusToFileHandler>(
                                hostSockFd, hostEid, instanceIdDb, path,
                                handler))
                        ->processNewResourceDump(vspstring, password);
                    break;
                }
            }
            });
        vmiCertMatcher = std::make_unique<sdbusplus::bus::match_t>(
            pldm::utils::DBusHandler::getBus(),
            sdbusplus::bus::match::rules::interfacesAdded() +
                sdbusplus::bus::match::rules::argNpath(0, certObjPath),
            [this, hostSockFd, hostEid, instanceIdDb,
             handler](sdbusplus::message_t& msg) {
            std::map<std::string,
                     std::map<std::string, std::variant<std::string, uint32_t>>>
                interfaces;
            sdbusplus::message::object_path path;
            msg.read(path, interfaces);
            std::string csr;

            for (auto& interface : interfaces)
            {
                if (interface.first == certAuthority)
                {
                    for (const auto& property : interface.second)
                    {
                        if (property.first == "CSR")
                        {
                            csr = std::get<std::string>(property.second);
                            auto fileHandle =
                                sdbusplus::message::object_path(path)
                                    .filename();

                            dbusToFileHandlers
                                .emplace_back(
                                    std::make_unique<pldm::requester::oem_ibm::
                                                         DbusToFileHandler>(
                                        hostSockFd, hostEid, instanceIdDb, path,
                                        handler))
                                ->newCsrFileAvailable(csr, fileHandle);
                            break;
                        }
                    }
                    break;
                }
            }
            });
    }

    /** @brief Handler for readFileIntoMemory command
     *
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */
    Response readFileIntoMemory(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for writeFileIntoMemory command
     *
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */
    Response writeFileFromMemory(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for writeFileByTypeFromMemory command
     *
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */

    Response writeFileByTypeFromMemory(const pldm_msg* request,
                                       size_t payloadLength);

    /** @brief Handler for readFileByTypeIntoMemory command
     *
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */
    Response readFileByTypeIntoMemory(const pldm_msg* request,
                                      size_t payloadLength);

    /** @brief Handler for writeFileByType command
     *
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */
    Response readFileByType(const pldm_msg* request, size_t payloadLength);

    Response writeFileByType(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for GetFileTable command
     *
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message payload
     *
     *  @return PLDM response message
     */
    Response getFileTable(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for readFile command
     *
     *  @param[in] request - PLDM request msg
     *  @param[in] payloadLength - length of the message payload
     *
     *  @return PLDM response message
     */
    Response readFile(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for writeFile command
     *
     *  @param[in] request - PLDM request msg
     *  @param[in] payloadLength - length of the message payload
     *
     *  @return PLDM response message
     */
    Response writeFile(const pldm_msg* request, size_t payloadLength);

    Response fileAck(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for getAlertStatus command
     *
     *  @param[in] request - PLDM request msg
     *  @param[in] payloadLength - length of the message payload
     *
     *  @return PLDM response message
     */
    Response getAlertStatus(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for newFileAvailable command
     *
     *  @param[in] request - PLDM request msg
     *  @param[in] payloadLength - length of the message payload
     *
     *  @return PLDM response message
     */
    Response newFileAvailable(const pldm_msg* request, size_t payloadLength);

  private:
    oem_platform::Handler* oemPlatformHandler;
    int hostSockFd;
    uint8_t hostEid;
    pldm::InstanceIdDb* instanceIdDb;
    using DBusInterfaceAdded = std::vector<std::pair<
        std::string,
        std::vector<std::pair<std::string, std::variant<std::string>>>>>;
    std::unique_ptr<pldm::requester::oem_ibm::DbusToFileHandler>
        dbusToFileHandler; //!< pointer to send request to Host
    std::unique_ptr<sdbusplus::bus::match_t>
        resDumpMatcher;    //!< Pointer to capture the interface added signal
                           //!< for new resource dump
    std::unique_ptr<sdbusplus::bus::match_t>
        vmiCertMatcher;    //!< Pointer to capture the interface added signal
                           //!< for new csr string
    /** @brief PLDM request handler */
    pldm::requester::Handler<pldm::requester::Request>* handler;
    std::vector<std::unique_ptr<pldm::requester::oem_ibm::DbusToFileHandler>>
        dbusToFileHandlers;
    ResponseHdr responseHdr;
};

} // namespace oem_ibm
} // namespace responder
} // namespace pldm
