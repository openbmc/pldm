
#include "common/flight_recorder.hpp"
#include "common/instance_id.hpp"
#include "common/transport.hpp"
#include "common/utils.hpp"
#include "fw-update/manager.hpp"
#include "invoker.hpp"
#include "platform-mc/dbus_to_terminus_effecters.hpp"
#include "platform-mc/manager.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "requester/request.hpp"

#include <err.h>
#include <getopt.h>
#include <libpldm/base.h>
#include <libpldm/bios.h>
#include <libpldm/pdr.h>
#include <libpldm/platform.h>
#include <libpldm/transport.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/io.hpp>
#include <sdeventplus/source/signal.hpp>
#include <stdplus/signal.hpp>

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef MULTI_THREADED
#define THREAD_WORKDER_COUNT 32
#endif

PHOSPHOR_LOG2_USING;

#ifdef LIBPLDMRESPONDER
#include "dbus_impl_pdr.hpp"
#include "host-bmc/dbus_to_event_handler.hpp"
#include "host-bmc/host_condition.hpp"
#include "host-bmc/host_pdr_handler.hpp"
#include "libpldmresponder/base.hpp"
#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/fru.hpp"
#include "libpldmresponder/oem_handler.hpp"
#include "libpldmresponder/platform.hpp"
#include "libpldmresponder/platform_config.hpp"
#include "xyz/openbmc_project/PLDM/Event/server.hpp"
#endif

#ifdef OEM_IBM
#include "oem_ibm.hpp"
#endif

#ifdef OEM_AMPERE
#include "oem/ampere/oem_ampere.hpp"
#endif

constexpr const char* PLDMService = "xyz.openbmc_project.PLDM";

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

void requestPLDMServiceName()
{
    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        bus.request_name(PLDMService);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to request D-Bus name {NAME} with error {ERROR}.", "NAME",
              PLDMService, "ERROR", e);
    }
}

static std::optional<Response> processRxMsg(
    const std::vector<uint8_t>& requestMsg, Invoker& invoker,
    requester::Handler<requester::Request>& handler,
    fw_update::Manager* fwManager, pldm_tid_t tid)
{
    uint8_t eid = tid;

    pldm_header_info hdrFields{};
    auto hdr = reinterpret_cast<const pldm_msg_hdr*>(requestMsg.data());
    if (PLDM_SUCCESS != unpack_pldm_header(hdr, &hdrFields))
    {
        error("Empty PLDM request header");
        return std::nullopt;
    }

    if (PLDM_RESPONSE != hdrFields.msg_type)
    {
        Response response;
        auto request = reinterpret_cast<const pldm_msg*>(hdr);
        size_t requestLen = requestMsg.size() - sizeof(struct pldm_msg_hdr);
        try
        {
            if (hdrFields.pldm_type != PLDM_FWUP)
            {
                response =
                    invoker.handle(tid, hdrFields.pldm_type, hdrFields.command,
                                   request, requestLen);
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
            auto responseHdr = new (response.data()) pldm_msg_hdr;
            pldm_header_info header{};
            header.msg_type = PLDM_RESPONSE;
            header.instance = hdrFields.instance;
            header.pldm_type = hdrFields.pldm_type;
            header.command = hdrFields.command;
            if (PLDM_SUCCESS != pack_pldm_header(&header, responseHdr))
            {
                error(
                    "Failed to add response header for processing Rx, error - {ERROR}",
                    "ERROR", e);
                return std::nullopt;
            }
            response.insert(response.end(), completion_code);
        }
        return response;
    }
    else if (PLDM_RESPONSE == hdrFields.msg_type)
    {
        auto response = reinterpret_cast<const pldm_msg*>(hdr);
        size_t responseLen = requestMsg.size() - sizeof(struct pldm_msg_hdr);
        handler.handleResponse(eid, hdrFields.instance, hdrFields.pldm_type,
                               hdrFields.command, response, responseLen);
    }
    return std::nullopt;
}

#ifdef MULTI_THREADED

struct WorkItem
{
    std::vector<uint8_t> request;
    pldm_tid_t tid;
};

class WorkerPool
{
  public:
    WorkerPool(size_t n, 
               std::queue<WorkItem>& readyQ, std::mutex& readyMtx,
               int resultEfd) :
        readyQ(readyQ), readyMtx(readyMtx), resultEfd(resultEfd), stop(false)
    {
        for (size_t i = 0; i < n; ++i)
        {
            workers.emplace_back([this] { this->run(); });
        }
    }

    ~WorkerPool()
    {
        {
            std::lock_guard<std::mutex> lk(m);
            stop = true;
        }
        cv.notify_all();
        for (auto& t : workers)
        {
            if (t.joinable())
                t.join();
        }
    }

    void enqueue(WorkItem&& w)
    {
        {
            std::lock_guard<std::mutex> lk(m);
            q.push(std::move(w));
        }
        cv.notify_one();
    }

  private:
    void run()
    {
        while (true)
        {
            WorkItem item;
            {
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [&] { return stop || !q.empty(); });
                if (stop && q.empty())
                {
                    return;
                }
                item = std::move(q.front());
                q.pop();
            }

            // Forward message back to main loop for processing and sending
            {
                std::lock_guard<std::mutex> lg(readyMtx);
                readyQ.push(std::move(item));
            }
            uint64_t one = 1;
            (void)!write(resultEfd, &one, sizeof(one));
        }
    }

    std::vector<std::thread> workers;
    std::mutex m;
    std::condition_variable cv;
    std::queue<WorkItem> q;

    std::queue<WorkItem>& readyQ;
    std::mutex& readyMtx;
    int resultEfd;

    bool stop;
};

#endif // MULTI_THREADED

void optionUsage(void)
{
    info("Usage: pldmd [options]");
    info("Options:");
    info(" [--verbose] - would enable verbosity");
#ifdef MULTI_THREADED
    info(" [-t #] - number of worker threads to process PLDM requests");
#endif
}

int main(int argc, char** argv)
{
    bool verbose = false;
    static struct option long_options[] = {
        {"verbose", no_argument, nullptr, 'v'},
#ifdef MULTI_THREADED
        {"threads", required_argument, nullptr, 't'},
#endif
        {nullptr, 0, nullptr, 0}};
#ifdef MULTI_THREADED
    // Worker thread pool for processing PLDM requests off the I/O thread.
    size_t workerCount = THREAD_WORKDER_COUNT;
    if (const char* env = std::getenv("PLDMD_WORKERS"))
    {
        try
        {
            unsigned long v = std::stoul(env);
            if (v >= 1 && v <= 64)
                workerCount = static_cast<size_t>(v);
        }
        catch (...)
        {
            perror("PLDMD_WORKERS environment variable not parsed - ignoring");
        }
    }

    auto argflag = getopt_long(argc, argv, "vt:", long_options, nullptr);
#else
    auto argflag = getopt_long(argc, argv, "v", long_options, nullptr);
#endif

    switch (argflag)
    {
        case 'v':
            verbose = true;
            break;
#ifdef MULTI_THREADED
        case 't':
            workerCount = std::max(1, std::min(64, std::stoi(optarg)));
            break;
#endif
        case -1:
            break;
        default:
            optionUsage();
            exit(EXIT_FAILURE);
    }
    // Setup PLDM requester transport
    auto hostEID = pldm::utils::readHostEID();
    /* To maintain current behaviour until we have the infrastructure to find
     * and use the correct TIDs */
    pldm_tid_t TID = hostEID;
    PldmTransport pldmTransport{};
    auto event = Event::get_default();
    auto& bus = pldm::utils::DBusHandler::getBus();
    sdbusplus::server::manager_t objManager(bus,
                                            "/xyz/openbmc_project/software");
    sdbusplus::server::manager_t sensorObjManager(
        bus, "/xyz/openbmc_project/sensors");

    InstanceIdDb instanceIdDb;
    sdbusplus::server::manager_t inventoryManager(
        bus, "/xyz/openbmc_project/inventory");

    Invoker invoker{};
    requester::Handler<requester::Request> reqHandler(&pldmTransport, event,
                                                      instanceIdDb, verbose);

    std::unique_ptr<pldm_pdr, decltype(&pldm_pdr_destroy)> pdrRepo(
        pldm_pdr_init(), pldm_pdr_destroy);
    if (!pdrRepo)
    {
        throw std::runtime_error("Failed to instantiate PDR repository");
    }
    DBusHandler dbusHandler;

    std::unique_ptr<platform_mc::Manager> platformManager =
        std::make_unique<platform_mc::Manager>(event, reqHandler, instanceIdDb);

    std::unique_ptr<pldm::host_effecters::HostEffecterParser>
        hostEffecterParser =
            std::make_unique<pldm::host_effecters::HostEffecterParser>(
                &instanceIdDb, pldmTransport.getEventSource(), pdrRepo.get(),
                &dbusHandler, HOST_JSONS_DIR, &reqHandler,
                platformManager.get());

#ifdef LIBPLDMRESPONDER
    using namespace pldm::state_sensor;
    dbus_api::Host dbusImplHost(bus, "/xyz/openbmc_project/pldm");
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
    std::unique_ptr<DbusToPLDMEvent> dbusToPLDMEventHandler;
    std::unique_ptr<platform_config::Handler> platformConfigHandler{};
    platformConfigHandler =
        std::make_unique<platform_config::Handler>(PDR_JSONS_DIR);

    if (hostEID)
    {
        hostPDRHandler = std::make_shared<HostPDRHandler>(
            pldmTransport.getEventSource(), hostEID, event, pdrRepo.get(),
            EVENTS_JSONS_DIR, entityTree.get(), bmcEntityTree.get(),
            instanceIdDb, &reqHandler);

        // HostFirmware interface needs access to hostPDR to know if host
        // is running
        dbusImplHost.setHostPdrObj(hostPDRHandler);

        dbusToPLDMEventHandler = std::make_unique<DbusToPLDMEvent>(
            pldmTransport.getEventSource(), hostEID, instanceIdDb, &reqHandler);
    }

    auto fruHandler = std::make_unique<fru::Handler>(
        FRU_JSONS_DIR, FRU_MASTER_JSON, pdrRepo.get(), entityTree.get(),
        bmcEntityTree.get());

    // FRU table is built lazily when a FRU command or Get PDR command is
    // handled. To enable building FRU table, the FRU handler is passed to the
    // Platform handler.

    pldm::responder::platform::EventMap addOnEventHandlers{
        {PLDM_CPER_EVENT,
         {[&platformManager](const pldm_msg* request, size_t payloadLength,
                             uint8_t formatVersion, uint8_t tid,
                             size_t eventDataOffset) {
             return platformManager->handleCperEvent(
                 request, payloadLength, formatVersion, tid, eventDataOffset);
         }}},
        {PLDM_MESSAGE_POLL_EVENT,
         {[&platformManager](const pldm_msg* request, size_t payloadLength,
                             uint8_t formatVersion, uint8_t tid,
                             size_t eventDataOffset) {
             return platformManager->handlePldmMessagePollEvent(
                 request, payloadLength, formatVersion, tid, eventDataOffset);
         }}},
        {PLDM_SENSOR_EVENT,
         {[&platformManager](const pldm_msg* request, size_t payloadLength,
                             uint8_t formatVersion, uint8_t tid,
                             size_t eventDataOffset) {
             return platformManager->handleSensorEvent(
                 request, payloadLength, formatVersion, tid, eventDataOffset);
         }}}};

    auto platformHandler = std::make_unique<platform::Handler>(
        &dbusHandler, hostEID, &instanceIdDb, PDR_JSONS_DIR, pdrRepo.get(),
        hostPDRHandler.get(), dbusToPLDMEventHandler.get(), fruHandler.get(),
        platformConfigHandler.get(), &reqHandler, event, true,
        addOnEventHandlers);

    auto biosHandler = std::make_unique<bios::Handler>(
        pldmTransport.getEventSource(), hostEID, &instanceIdDb, &reqHandler,
        platformConfigHandler.get(), requestPLDMServiceName);

    auto baseHandler = std::make_unique<base::Handler>(event);

#ifdef OEM_AMPERE
    pldm::oem_ampere::OemAMPERE oemAMPERE(
        &dbusHandler, pldmTransport.getEventSource(), pdrRepo.get(),
        instanceIdDb, event, invoker, hostPDRHandler.get(),
        platformHandler.get(), fruHandler.get(), baseHandler.get(),
        biosHandler.get(), platformManager.get(), &reqHandler);
#endif

#ifdef OEM_IBM
    pldm::oem_ibm::OemIBM oemIBM(
        &dbusHandler, pldmTransport.getEventSource(), hostEID, pdrRepo.get(),
        instanceIdDb, event, invoker, hostPDRHandler.get(),
        platformHandler.get(), fruHandler.get(), baseHandler.get(),
        &reqHandler);
#endif

    invoker.registerHandler(PLDM_BIOS, std::move(biosHandler));
    invoker.registerHandler(PLDM_PLATFORM, std::move(platformHandler));
    invoker.registerHandler(PLDM_FRU, std::move(fruHandler));
    invoker.registerHandler(PLDM_BASE, std::move(baseHandler));

    dbus_api::Pdr dbusImplPdr(bus, "/xyz/openbmc_project/pldm", pdrRepo.get());
    sdbusplus::xyz::openbmc_project::PLDM::server::Event dbusImplEvent(
        bus, "/xyz/openbmc_project/pldm");

#endif

    std::unique_ptr<fw_update::Manager> fwManager =
        std::make_unique<fw_update::Manager>(&dbusHandler, event, reqHandler,
                                             instanceIdDb);
    std::unique_ptr<MctpDiscovery> mctpDiscoveryHandler =
        std::make_unique<MctpDiscovery>(
            bus, std::initializer_list<MctpDiscoveryHandlerIntf*>{
                     fwManager.get(), platformManager.get()});

#ifdef MULTI_THREADED
    // Mailbox from worker threads to sd-event loop
    static std::mutex readyMtx;
    static std::queue<WorkItem> readyQ;

    // Eventfd to wake the sd-event loop when workers enqueue items
    const int resultEfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (resultEfd < 0)
    { // unlikely to happen, but handle gracefully
        perror("eventfd");
        exit(1);
    }

    // IO source to drain eventfd and process requests on the loop thread
    IO resultIo(
        event, resultEfd, EPOLLIN, IO::Callback{[&](IO&, int, uint32_t) {
            // Drain the counter, nothing expected to be there, but lets be sure
            uint64_t cnt;
            while (read(resultEfd, &cnt, sizeof(cnt)) > 0)
            {}

            // Pop-and-handle all queued items (now on sd-event thread)
            while (true)
            {
                WorkItem item;
                {
                    std::lock_guard<std::mutex> lg(readyMtx);
                    if (readyQ.empty())
                        break;
                    item = std::move(readyQ.front());
                    readyQ.pop();
                }

                auto response = processRxMsg(item.request, invoker, reqHandler,
                                             fwManager.get(), item.tid);
                if (response.has_value())
                {
                    FlightRecorder::GetInstance().saveRecord(*response, true);
                    if (verbose)
                    {
                        printBuffer(Tx, *response);
                    }
                    int rc = pldmTransport.sendMsg(item.tid, response->data(),
                                                   response->size());
                    if (rc != PLDM_REQUESTER_SUCCESS)
                    {
                        warning(
                            "Failed to send pldmTransport message for TID '{TID}', response code '{RETURN_CODE}'",
                            "TID", item.tid, "RETURN_CODE", rc);
                    }
                }
            }
        }});

    static std::mutex transportMtx; // guard transport send path
    WorkerPool workerPool(workerCount, readyQ, readyMtx, resultEfd);

    auto callback = [verbose, &pldmTransport, TID,
                     &workerPool](IO& io, int fd, uint32_t revents) mutable {
#else
    auto callback = [verbose, &invoker, &reqHandler, &fwManager, &pldmTransport,
                     TID](IO& io, int fd, uint32_t revents) mutable {
#endif
        if (revents & (POLLHUP | POLLERR))
        {
            warning("Transport Socket hang-up or error. IO Exiting.");
            io.get_event().exit(0);
            return;
        }

        else if (!(revents & EPOLLIN))
        {
            return;
        }
        if (fd < 0)
        {
            return;
        }

        int returnCode = 0;
        void* requestMsg;
        size_t recvDataLength;
        returnCode = pldmTransport.recvMsg(TID, requestMsg, recvDataLength);

        if (returnCode == PLDM_REQUESTER_SUCCESS)
        {
            std::vector<uint8_t> requestMsgVec(
                static_cast<uint8_t*>(requestMsg),
                static_cast<uint8_t*>(requestMsg) + recvDataLength);
            FlightRecorder::GetInstance().saveRecord(requestMsgVec, false);
            if (verbose)
            {
                printBuffer(Rx, requestMsgVec);
            }
#ifdef MULTI_THREADED
            // enqueue for background processing and response
            workerPool.enqueue(WorkItem{std::move(requestMsgVec), TID});
#else
            // process message and send response
            auto response = processRxMsg(requestMsgVec, invoker, reqHandler,
                                         fwManager.get(), TID);
            if (response.has_value())
            {
                FlightRecorder::GetInstance().saveRecord(*response, true);
                if (verbose)
                {
                    printBuffer(Tx, *response);
                }

                returnCode = pldmTransport.sendMsg(TID, (*response).data(),
                                                   (*response).size());
                if (returnCode != PLDM_REQUESTER_SUCCESS)
                {
                    warning(
                        "Failed to send pldmTransport message for TID '{TID}', response code '{RETURN_CODE}'",
                        "TID", TID, "RETURN_CODE", returnCode);
                }
            }
#endif
        }
        // TODO check that we get here if mctp-demux dies?
        else if (returnCode == PLDM_REQUESTER_RECV_FAIL)
        {
            // MCTP daemon has closed the socket this daemon is connected to.
            // This may or may not be an error scenario, in either case the
            // recovery mechanism for this daemon is to restart, and hence exit
            // the event loop, that will cause this daemon to exit with a
            // failure code.
            error(
                "MCTP daemon closed the socket, IO exiting with response code '{RC}'",
                "RC", returnCode);
            io.get_event().exit(0);
        }
        else
        {
            warning(
                "Failed to receive PLDM request for pldmTransport, response code '{RETURN_CODE}'",
                "RETURN_CODE", returnCode);
        }
        /* Free requestMsg after using */
        free(requestMsg);
    };

    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
#ifndef SYSTEM_SPECIFIC_BIOS_JSON
    try
    {
        bus.request_name(PLDMService);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to request D-Bus name {NAME} with error {ERROR}.", "NAME",
              PLDMService, "ERROR", e);
    }
#endif
    IO io(event, pldmTransport.getEventSource(), EPOLLIN, std::move(callback));
#ifdef LIBPLDMRESPONDER
    if (hostPDRHandler)
    {
        hostPDRHandler->setHostFirmwareCondition();
    }
#endif
    stdplus::signal::block(SIGUSR1);
    sdeventplus::source::Signal sigUsr1(
        event, SIGUSR1,
        [](Signal& signal, const struct signalfd_siginfo* info) {
            interruptFlightRecorderCallBack(signal, info);
        });
    int returnCode = event.loop();
    if (returnCode)
    {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
