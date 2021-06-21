#pragma once

#include "libpldm/pdr.h"

#include "../oem/ibm/libpldmresponder/file_io.hpp"
#include "../oem/ibm/libpldmresponder/oem_ibm_handler.hpp"
#include "common/utils.hpp"
#include "dbus_impl_requester.hpp"
#include "host-bmc/dbus_to_event_handler.hpp"
#include "invoker.hpp"
#include "libpldmresponder/fru.hpp"
#include "requester/request.hpp"

namespace pldm
{
namespace oem_ibm
{

using namespace pldm::state_sensor;
using namespace pldm::dbus_api;

class OemIBM
{
  public:
    OemIBM() = delete;
    OemIBM(const Pdr&) = delete;
    OemIBM& operator=(const OemIBM&) = delete;
    OemIBM(OemIBM&&) = delete;
    OemIBM& operator=(OemIBM&&) = delete;
    virtual ~OemIBM() = default;

  public:
    explicit OemIBM(
        const pldm::utils::DBusHandler* dBusIntf, int mctp_fd, uint8_t mctp_eid,
        pldm_pdr* repo, Requester& requester, sdeventplus::Event& event,
        Invoker& invoker, HostPDRHandler* hostPDRHandler,
        DbusToPLDMEvent* dbusToPLDMEventHandler, fru::Handler* fruHandler,
        pldm::requester::Handler<pldm::requester::Request>* reqHandler) :
        dBusIntf(dBusIntf),
        mctp_fd(mctp_fd), mctp_eid(mctp_eid), repo(repo), requester(requester),
        event(event), invoker(invoker), hostPDRHandler(hostPDRHandler),
        dbusToPLDMEventHandler(dbusToPLDMEventHandler), fruHandler(fruHandler),
        reqHandler(reqHandler)
    {
        createCodeUpdate();
        createOemPlatformHandler();
        createPlatformHandler();
        createOemIbmPlatformHandler();
        registerHandler();
    }

    std::unique_ptr<platform::Handler> getPlatfromHandler()
    {
        return std::move(platformHandler);
    }

  private:
    void createCodeUpdate()
    {
        codeUpdate = std::make_unique<pldm::responder::CodeUpdate>(dBusIntf);
        codeUpdate->clearDirPath(LID_STAGING_DIR);
    }

    void createOemPlatformHandler()
    {
        oemPlatformHandler = std::make_unique<oem_ibm_platform::Handler>(
            dBusIntf, codeUpdate.get(), mctp_fd, mctp_eid, requester, event,
            reqHandler);
        codeUpdate->setOemPlatformHandler(oemPlatformHandler.get());
    }

    void createPlatformHandler()
    {
        platformHandler = std::make_unique<platform::Handler>(
            dBusIntf, PDR_JSONS_DIR, repo, hostPDRHandler,
            dbusToPLDMEventHandler, fruHandler, oemPlatformHandler.get(), event,
            true);
    }

    void createOemIbmPlatformHandler()
    {
        pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
            dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
                oemPlatformHandler.get());
        oemIbmPlatformHandler->setPlatformHandler(platformHandler.get());
    }

    void registerHandler()
    {
        invoker.registerHandler(
            PLDM_OEM, std::make_unique<pldm::responder::oem_ibm::Handler>(
                          oemPlatformHandler.get(), mctp_fd, mctp_eid,
                          &requester, reqHandler));
    }

  private:
    const pldm::utils::DBusHandler* dBusIntf;

    int mctp_fd;

    uint8_t mctp_eid;

    pldm_pdr* repo;

    Requester& requester;

    sdeventplus::Event& event;

    Invoker& invoker;

    HostPDRHandler* hostPDRHandler;

    DbusToPLDMEvent* dbusToPLDMEventHandler;

    fru::Handler* fruHandler;

    requester::Handler<requester::Request>* reqHandler;

    std::unique_ptr<oem_platform::Handler> oemPlatformHandler;

    std::unique_ptr<pldm::responder::CodeUpdate> codeUpdate;

    std::unique_ptr<platform::Handler> platformHandler;
};

} // namespace oem_ibm
} // namespace pldm
