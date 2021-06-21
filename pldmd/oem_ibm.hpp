#pragma once

#include "libpldm/pdr.h"

#include "../oem/ibm/libpldmresponder/file_io.hpp"
#include "../oem/ibm/libpldmresponder/fru_oem_ibm.hpp"
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
        pldm_pdr* repo, pldm::InstanceIdDb& instanceIdDb,
        sdeventplus::Event& event, Invoker& invoker,
        HostPDRHandler* hostPDRHandler,
        pldm::host_effecters::HostEffecterParser* hostEffecterParser,
        DbusToPLDMEvent* dbusToPLDMEventHandler,
        pldm::responder::platform_config::Handler* platformConfigHandler,
        pldm::requester::Handler<pldm::requester::Request>* reqHandler,
        pldm_entity_association_tree* entityTree,
        pldm_entity_association_tree* bmcEntityTree) :
        dBusIntf(dBusIntf),
        mctp_fd(mctp_fd), mctp_eid(mctp_eid), repo(repo),
        instanceIdDb(instanceIdDb), event(event), invoker(invoker),
        hostPDRHandler(hostPDRHandler), hostEffecterParser(hostEffecterParser),
        dbusToPLDMEventHandler(dbusToPLDMEventHandler),
        platformConfigHandler(std::move(platformConfigHandler)),
        reqHandler(reqHandler), entityTree(entityTree),
        bmcEntityTree(bmcEntityTree)
    {
        createOemFruHandler();
        createFruHandler();
        createOemIbmFruHandler();
        createCodeUpdate();
        createOemPlatformHandler();

        if (mctp_eid)
        {
            createHostPDRHandler();
            createHostEffecterParser();
            createDbusToPLDMEventHandler();
        }

        createPlatformHandler();
        createOemIbmPlatformHandler();
        registerHandler();
    }

    std::unique_ptr<platform::Handler> getPlatfromHandler()
    {
        return std::move(platformHandler);
    }

    std::unique_ptr<fru::Handler> getFruHandler()
    {
        return std::move(fruHandler);
    }

    std::shared_ptr<HostPDRHandler> getHostPDRHandler()
    {
        return hostPDRHandler;
    }

    std::unique_ptr<oem_platform::Handler> getOemPlatformHandler()
    {
        return std::move(oemPlatformHandler);
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
            dBusIntf, codeUpdate.get(), mctp_fd, mctp_eid, instanceIdDb, event,
            reqHandler);
        codeUpdate->setOemPlatformHandler(oemPlatformHandler.get());
    }

    void createHostPDRHandler()
    {
        hostPDRHandler = std::make_shared<HostPDRHandler>(
            mctp_fd, mctp_eid, event, repo, EVENTS_JSONS_DIR, entityTree,
            bmcEntityTree, instanceIdDb, reqHandler, oemPlatformHandler.get());
    }

    void createHostEffecterParser()
    {
        hostEffecterParser =
            std::make_unique<pldm::host_effecters::HostEffecterParser>(
                &instanceIdDb, mctp_fd, repo,
                const_cast<pldm::utils::DBusHandler*>(dBusIntf), HOST_JSONS_DIR,
                reqHandler);
    }

    void createDbusToPLDMEventHandler()
    {
        dbusToPLDMEventHandler = std::make_unique<DbusToPLDMEvent>(
            mctp_fd, mctp_eid, instanceIdDb, reqHandler);
    }

    void createPlatformHandler()
    {
        platformHandler = std::make_unique<platform::Handler>(
            dBusIntf, mctp_eid, &instanceIdDb, PDR_JSONS_DIR, repo,
            hostPDRHandler.get(), dbusToPLDMEventHandler.get(),
            fruHandler.get(), oemPlatformHandler.get(),
            platformConfigHandler.get(), reqHandler, event, true);
    }

    void createOemIbmPlatformHandler()
    {
        pldm::responder::oem_ibm_platform::Handler* oemIbmPlatformHandler =
            dynamic_cast<pldm::responder::oem_ibm_platform::Handler*>(
                oemPlatformHandler.get());
        oemIbmPlatformHandler->setPlatformHandler(platformHandler.get());
    }

    void createOemFruHandler()
    {
        oemFruHandler = std::make_unique<oem_ibm_fru::Handler>(repo);
    }

    void createFruHandler()
    {
        fruHandler = std::make_unique<fru::Handler>(
            FRU_JSONS_DIR, FRU_MASTER_JSON, repo, entityTree, bmcEntityTree,
            oemFruHandler.get());
    }

    void createOemIbmFruHandler()
    {
        pldm::responder::oem_ibm_fru::Handler* oemIbmFruHandler =
            dynamic_cast<pldm::responder::oem_ibm_fru::Handler*>(
                oemFruHandler.get());
        oemIbmFruHandler->setIBMFruHandler(fruHandler.get());
    }

    void registerHandler()
    {
        invoker.registerHandler(
            PLDM_OEM, std::make_unique<pldm::responder::oem_ibm::Handler>(
                          oemPlatformHandler.get(), mctp_fd, mctp_eid,
                          &instanceIdDb, reqHandler));
    }

  private:
    const pldm::utils::DBusHandler* dBusIntf;

    int mctp_fd;

    uint8_t mctp_eid;

    pldm_pdr* repo;

    pldm::InstanceIdDb& instanceIdDb;

    sdeventplus::Event& event;

    Invoker& invoker;

    std::shared_ptr<HostPDRHandler> hostPDRHandler;

    std::unique_ptr<pldm::host_effecters::HostEffecterParser>
        hostEffecterParser;

    std::unique_ptr<DbusToPLDMEvent> dbusToPLDMEventHandler;

    std::unique_ptr<fru::Handler> fruHandler;

    std::unique_ptr<platform_config::Handler> platformConfigHandler;

    requester::Handler<requester::Request>* reqHandler;

    std::unique_ptr<oem_platform::Handler> oemPlatformHandler;

    std::unique_ptr<oem_fru::Handler> oemFruHandler;

    std::unique_ptr<pldm::responder::CodeUpdate> codeUpdate;

    std::unique_ptr<platform::Handler> platformHandler;

    pldm_entity_association_tree* entityTree;

    pldm_entity_association_tree* bmcEntityTree;
};

} // namespace oem_ibm
} // namespace pldm
