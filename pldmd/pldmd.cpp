#include "common/flight_recorder.hpp"
#include "common/utils.hpp"
#include "dbus_impl_requester.hpp"
#include "fw-update/manager.hpp"
#include "instance_id.hpp"
#include "invoker.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "requester/request.hpp"

#include <err.h>
#include <getopt.h>
#include <libpldm/base.h>
#include <libpldm/bios.h>
#include <libpldm/pdr.h>
#include <libpldm/platform.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/io.hpp>
#include <sdeventplus/source/signal.hpp>
#include <stdplus/signal.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

#ifdef LIBPLDMRESPONDER
#include "dbus_impl_pdr.hpp"
#include "host-bmc/dbus_to_event_handler.hpp"
#include "host-bmc/dbus_to_host_effecters.hpp"
#include "host-bmc/host_condition.hpp"
#include "host-bmc/host_pdr_handler.hpp"
#include "libpldmresponder/base.hpp"
#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/fru.hpp"
#include "libpldmresponder/oem_handler.hpp"
#include "libpldmresponder/platform.hpp"
#include "xyz/openbmc_project/PLDM/Event/server.hpp"
#endif

#ifdef OEM_IBM
#include "libpldmresponder/bios_oem_ibm.hpp"
#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/oem_ibm_handler.hpp"
#endif

constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;

using namespace pldm;
using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm::responder;
using namespace pldm::utils;
using sdeventplus::source::Signal;
using namespace pldm::flightrecorder;

void interruptFlightRecorderCallBack(Signal& /*signal*/,
                                     const struct signalfd_siginfo*)
{
    error("Received SIGUR1(10) Signal interrupt");
    // obtain the flight recorder instance and dump the recorder
    FlightRecorder::GetInstance().playRecorder();
}

static std::optional<Response>
    processRxMsg(const std::vector<uint8_t>& requestMsg, Invoker& invoker,
                 requester::Handler<requester::Request>& handler,
                 fw_update::Manager* fwManager)
{
    using type = uint8_t;
    uint8_t eid = requestMsg[0];
    pldm_header_info hdrFields{};
    auto hdr = reinterpret_cast<const pldm_msg_hdr*>(
        requestMsg.data() + sizeof(eid) + sizeof(type));
    if (PLDM_SUCCESS != unpack_pldm_header(hdr, &hdrFields))
    {
        error("Empty PLDM request header");
        return std::nullopt;
    }

    if (PLDM_RESPONSE != hdrFields.msg_type)
    {
        Response response;
        auto request = reinterpret_cast<const pldm_msg*>(hdr);
        size_t requestLen = requestMsg.size() - sizeof(struct pldm_msg_hdr) -
                            sizeof(eid) - sizeof(type);
        try
        {
            if (hdrFields.pldm_type != PLDM_FWUP)
            {
                response = invoker.handle(hdrFields.pldm_type,
                                          hdrFields.command, request,
                                          requestLen);
            }
            else
            {
                response = fwManager->handleRequest(eid, hdrFields.command,
                                                    request, requestLen);
            }
        }
        catch (const std::out_of_range& e)
        {
            uint8_t completion_code = PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
            response.resize(sizeof(pldm_msg_hdr));
            auto responseHdr = reinterpret_cast<pldm_msg_hdr*>(response.data());
            pldm_header_info header{};
            header.msg_type = PLDM_RESPONSE;
            header.instance = hdrFields.instance;
            header.pldm_type = hdrFields.pldm_type;
            header.command = hdrFields.command;
            if (PLDM_SUCCESS != pack_pldm_header(&header, responseHdr))
            {
                error("Failed adding response header");
                return std::nullopt;
            }
            response.insert(response.end(), completion_code);
        }
        return response;
    }
    else if (PLDM_RESPONSE == hdrFields.msg_type)
    {
        auto response = reinterpret_cast<const pldm_msg*>(hdr);
        size_t responseLen = requestMsg.size() - sizeof(struct pldm_msg_hdr) -
                             sizeof(eid) - sizeof(type);
        handler.handleResponse(eid, hdrFields.instance, hdrFields.pldm_type,
                               hdrFields.command, response, responseLen);
    }
    return std::nullopt;
}

void optionUsage(void)
{
    error("Usage: pldmd [options]");
    error("Options:");
    error(" [--verbose] - would enable verbosity");
}

int main(int argc, char** argv)
{
    bool verbose = false;
    static struct option long_options[] = {{"verbose", no_argument, 0, 'v'},
                                           {0, 0, 0, 0}};

    auto argflag = getopt_long(argc, argv, "v", long_options, nullptr);
    switch (argflag)
    {
        case 'v':
            verbose = true;
            break;
        case -1:
            break;
        default:
            optionUsage();
            exit(EXIT_FAILURE);
    }

    /* Create local socket. */
    int returnCode = 0;
    int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (-1 == sockfd)
    {
        returnCode = -errno;
        error("Failed to create the socket, RC= {RC}", "RC", returnCode);
        exit(EXIT_FAILURE);
    }
    socklen_t optlen;
    int currentSendbuffSize;

    // Get Current send buffer size
    optlen = sizeof(currentSendbuffSize);

    int res = getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &currentSendbuffSize,
                         &optlen);
    if (res == -1)
    {
        error("Error in obtaining the default send buffer size, Error : {ERR}",
              "ERR", strerror(errno));
    }
    auto event = Event::get_default();
    auto& bus = pldm::utils::DBusHandler::getBus();
    sdbusplus::server::manager_t objManager(bus,
                                            "/xyz/openbmc_project/software");

    InstanceIdDb instanceIdDb;
    dbus_api::Requester dbusImplReq(bus, "/xyz/openbmc_project/pldm",
                                    instanceIdDb);

    Invoker invoker{};
    requester::Handler<requester::Request> reqHandler(
        sockfd, event, instanceIdDb, currentSendbuffSize, verbose);

#ifdef LIBPLDMRESPONDER
    using namespace pldm::state_sensor;
    dbus_api::Host dbusImplHost(bus, "/xyz/openbmc_project/pldm");
    std::unique_ptr<pldm_pdr, decltype(&pldm_pdr_destroy)> pdrRepo(
        pldm_pdr_init(), pldm_pdr_destroy);
    if (!pdrRepo)
    {
        throw std::runtime_error("Failed to instantiate PDR repository");
    }
    std::unique_ptr<pldm_entity_association_tree,
                    decltype(&pldm_entity_association_tree_destroy)>
        entityTree(pldm_entity_association_tree_init(),
                   pldm_entity_association_tree_destroy);
    if (!entityTree)
    {
        throw std::runtime_error(
            "Failed to instantiate general PDR entity association tree");
    }
    std::unique_ptr<pldm_entity_association_tree,
                    decltype(&pldm_entity_association_tree_destroy)>
        bmcEntityTree(pldm_entity_association_tree_init(),
                      pldm_entity_association_tree_destroy);
    if (!bmcEntityTree)
    {
        throw std::runtime_error(
            "Failed to instantiate BMC PDR entity association tree");
    }
    std::shared_ptr<HostPDRHandler> hostPDRHandler;
    std::unique_ptr<pldm::host_effecters::HostEffecterParser>
        hostEffecterParser;
    std::unique_ptr<DbusToPLDMEvent> dbusToPLDMEventHandler;
    DBusHandler dbusHandler;
    auto hostEID = pldm::utils::readHostEID();
    if (hostEID)
    {
        hostPDRHandler = std::make_shared<HostPDRHandler>(
            sockfd, hostEID, event, pdrRepo.get(), EVENTS_JSONS_DIR,
            entityTree.get(), bmcEntityTree.get(), instanceIdDb, &reqHandler);
        // HostFirmware interface needs access to hostPDR to know if host
        // is running
        dbusImplHost.setHostPdrObj(hostPDRHandler);

        hostEffecterParser =
            std::make_unique<pldm::host_effecters::HostEffecterParser>(
                &instanceIdDb, sockfd, pdrRepo.get(), &dbusHandler,
                HOST_JSONS_DIR, &reqHandler);
        dbusToPLDMEventHandler = std::make_unique<DbusToPLDMEvent>(
            sockfd, hostEID, instanceIdDb, &reqHandler);
    }
    std::unique_ptr<oem_platform::Handler> oemPlatformHandler{};
    std::unique_ptr<oem_bios::Handler> oemBiosHandler{};

#ifdef OEM_IBM
    std::unique_ptr<pldm::responder::CodeUpdate> codeUpdate =
        std::make_unique<pldm::responder::CodeUpdate>(&dbusHandler);
    codeUpdate->clearDirPath(LID_STAGING_DIR);
    oemPlatformHandler = std::make_unique<oem_ibm_platform::Handler>(
        &dbusHandler, codeUpdate.get(), sockfd, hostEID, instanceIdDb, event,
        &reqHandler);
    codeUpdate->setOemPlatformHandler(oemPlatformHandler.get());
    invoker.registerHandler(PLDM_OEM, std::make_unique<oem_ibm::Handler>(
                                          oemPlatformHandler.get(), sockfd,
                                          hostEID, &instanceIdDb, &reqHandler));
    oemBiosHandler = std::make_unique<oem::ibm::bios::Handler>(&dbusHandler);
#endif

    auto biosHandler = std::make_unique<bios::Handler>(
        sockfd, hostEID, &instanceIdDb, &reqHandler, oemBiosHandler.get());
    auto fruHandler = std::make_unique<fru::Handler>(
        FRU_JSONS_DIR, FRU_MASTER_JSON, pdrRepo.get(), entityTree.get(),
        bmcEntityTree.get());
    // FRU table is built lazily when a FRU command or Get PDR command is
    // handled. To enable building FRU table, the FRU handler is passed to the
    // Platform handler.
    auto platformHandler = std::make_unique<platform::Handler>(
        &dbusHandler, PDR_JSONS_DIR, pdrRepo.get(), hostPDRHandler.get(),
        dbusToPLDMEventHandler.get(), fruHandler.get(),
        oemPlatformHandler.get(), event, true);
#ifdef OEM_IBM
    pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
        dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
            oemPlatformHandler.get());
    oemIbmPlatformHandler->setPlatformHandler(platformHandler.get());

#endif

    invoker.registerHandler(PLDM_BIOS, std::move(biosHandler));
    invoker.registerHandler(PLDM_PLATFORM, std::move(platformHandler));
    invoker.registerHandler(
        PLDM_BASE,
        std::make_unique<base::Handler>(hostEID, instanceIdDb, event,
                                        oemPlatformHandler.get(), &reqHandler));
    invoker.registerHandler(PLDM_FRU, std::move(fruHandler));
    dbus_api::Pdr dbusImplPdr(bus, "/xyz/openbmc_project/pldm", pdrRepo.get());
    sdbusplus::xyz::openbmc_project::PLDM::server::Event dbusImplEvent(
        bus, "/xyz/openbmc_project/pldm");

#endif

    pldm::utils::CustomFD socketFd(sockfd);

    struct sockaddr_un addr
    {};
    addr.sun_family = AF_UNIX;
    const char path[] = "\0mctp-mux";
    memcpy(addr.sun_path, path, sizeof(path) - 1);
    int result = connect(socketFd(), reinterpret_cast<struct sockaddr*>(&addr),
                         sizeof(path) + sizeof(addr.sun_family) - 1);
    if (-1 == result)
    {
        returnCode = -errno;
        error("Failed to connect to the socket, RC= {RC}", "RC", returnCode);
        exit(EXIT_FAILURE);
    }

    result = write(socketFd(), &MCTP_MSG_TYPE_PLDM, sizeof(MCTP_MSG_TYPE_PLDM));
    if (-1 == result)
    {
        returnCode = -errno;
        error("Failed to send message type as pldm to mctp, RC= {RC}", "RC",
              returnCode);
        exit(EXIT_FAILURE);
    }

    std::unique_ptr<fw_update::Manager> fwManager =
        std::make_unique<fw_update::Manager>(event, reqHandler, instanceIdDb);
    std::unique_ptr<MctpDiscovery> mctpDiscoveryHandler =
        std::make_unique<MctpDiscovery>(bus, fwManager.get());

    auto callback = [verbose, &invoker, &reqHandler, currentSendbuffSize,
                     &fwManager](IO& io, int fd, uint32_t revents) mutable {
        if (!(revents & EPOLLIN))
        {
            return;
        }

        // Outgoing message.
        struct iovec iov[2]{};

        // This structure contains the parameter information for the response
        // message.
        struct msghdr msg
        {};

        int returnCode = 0;
        ssize_t peekedLength = recv(fd, nullptr, 0, MSG_PEEK | MSG_TRUNC);
        if (0 == peekedLength)
        {
            // MCTP daemon has closed the socket this daemon is connected to.
            // This may or may not be an error scenario, in either case the
            // recovery mechanism for this daemon is to restart, and hence exit
            // the event loop, that will cause this daemon to exit with a
            // failure code.
            io.get_event().exit(0);
        }
        else if (peekedLength <= -1)
        {
            returnCode = -errno;
            error("recv system call failed, RC= {RC}", "RC", returnCode);
        }
        else
        {
            std::vector<uint8_t> requestMsg(peekedLength);
            auto recvDataLength = recv(
                fd, static_cast<void*>(requestMsg.data()), peekedLength, 0);
            if (recvDataLength == peekedLength)
            {
                FlightRecorder::GetInstance().saveRecord(requestMsg, false);
                if (verbose)
                {
                    printBuffer(Rx, requestMsg);
                }

                if (MCTP_MSG_TYPE_PLDM != requestMsg[1])
                {
                    // Skip this message and continue.
                }
                else
                {
                    // process message and send response
                    auto response = processRxMsg(requestMsg, invoker,
                                                 reqHandler, fwManager.get());
                    if (response.has_value())
                    {
                        FlightRecorder::GetInstance().saveRecord(*response,
                                                                 true);
                        if (verbose)
                        {
                            printBuffer(Tx, *response);
                        }

                        iov[0].iov_base = &requestMsg[0];
                        iov[0].iov_len = sizeof(requestMsg[0]) +
                                         sizeof(requestMsg[1]);
                        iov[1].iov_base = (*response).data();
                        iov[1].iov_len = (*response).size();

                        msg.msg_iov = iov;
                        msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);
                        if (currentSendbuffSize >= 0 &&
                            (size_t)currentSendbuffSize < (*response).size())
                        {
                            int oldBuffSize = currentSendbuffSize;
                            currentSendbuffSize = (*response).size();
                            int res = setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                                                 &currentSendbuffSize,
                                                 sizeof(currentSendbuffSize));
                            if (res == -1)
                            {
                                error(
                                    "Responder : Failed to set the new send buffer size [bytes] : {CURR_SND_BUF_SIZE}",
                                    "CURR_SND_BUF_SIZE", currentSendbuffSize);
                                error(
                                    "from current size [bytes] : {OLD_BUF_SIZE}, Error : {ERR}",
                                    "OLD_BUF_SIZE", oldBuffSize, "ERR",
                                    strerror(errno));
                                return;
                            }
                        }

                        int result = sendmsg(fd, &msg, 0);
                        if (-1 == result)
                        {
                            returnCode = -errno;
                            error("sendto system call failed, RC= {RC}", "RC",
                                  returnCode);
                        }
                    }
                }
            }
            else
            {
                error(
                    "Failure to read peeked length packet. peekedLength = {PEEK_LEN}, recvDataLength= {RECV_LEN}",
                    "PEEK_LEN", peekedLength, "RECV_LEN", recvDataLength);
            }
        }
    };

    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
    bus.request_name("xyz.openbmc_project.PLDM");
    IO io(event, socketFd(), EPOLLIN, std::move(callback));
#ifdef LIBPLDMRESPONDER
    if (hostPDRHandler)
    {
        hostPDRHandler->setHostFirmwareCondition();
    }
#endif
    stdplus::signal::block(SIGUSR1);
    sdeventplus::source::Signal sigUsr1(
        event, SIGUSR1, std::bind_front(&interruptFlightRecorderCallBack));
    returnCode = event.loop();

    if (shutdown(sockfd, SHUT_RDWR))
    {
        error("Failed to shutdown the socket");
    }
    if (returnCode)
    {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
