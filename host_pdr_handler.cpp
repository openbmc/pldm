#include "config.h"

#include "host_pdr_handler.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

#include "libpldm/requester/pldm.h"

namespace pldm
{

using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr auto fruJson = "host_frus.json";
const Json emptyJson{};
const std::vector<Json> emptyJsonList{};

HostPDRHandler::HostPDRHandler(int mctp_fd, uint8_t mctp_eid,
                               sdeventplus::Event& event, pldm_pdr* repo,
                               pldm_entity_association_tree* entityTree,
                               Requester& requester) :
    mctp_fd(mctp_fd),
    mctp_eid(mctp_eid), event(event), repo(repo), entityTree(entityTree),
    requester(requester)
{
    fs::path hostFruJson(fs::path(HOST_JSONS_DIR) / fruJson);
    if (fs::exists(hostFruJson))
    {
        try
        {
            std::ifstream jsonFile(hostFruJson);
            auto data = Json::parse(jsonFile, nullptr, false);
            if (data.is_discarded())
            {
                std::cerr << "Parsing Host FRU json file failed" << std::endl;
            }
            else
            {
                auto entities = data.value("entities", emptyJsonList);
                for (auto& entity : entities)
                {
                    EntityType entityType = entity.value("entity_type", 0);
                    auto parent = entity.value("parent", emptyJson);
                    pldm_entity p{};
                    p.entity_type = parent.value("entity_type", 0);
                    p.entity_instance_num = parent.value("entity_instance", 0);
                    parents.emplace(entityType, std::move(p));
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Parsing Host FRU json file failed, exception = "
                      << e.what() << std::endl;
        }
    }
}

void HostPDRHandler::fetchPDR(std::vector<uint32_t>&& recordHandles)
{
    pdrRecordHandles.clear();
    pdrRecordHandles = std::move(recordHandles);

    // Defer the actual fetch of PDRs from the host (by queuing the call on the
    // main event loop). That way, we can respond to the platform event msg from
    // the host firmware.
    pdrFetchEvent = std::make_unique<sdeventplus::source::Defer>(
        event, std::bind(std::mem_fn(&HostPDRHandler::_fetchPDR), this,
                         std::placeholders::_1));
}

void HostPDRHandler::_fetchPDR(sdeventplus::source::EventBase& /*source*/)
{
    pdrFetchEvent.reset();

    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_PDR_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    for (auto recordHandle : pdrRecordHandles)
    {
        auto instanceId = requester.getInstanceId(mctp_eid);

        auto rc =
            encode_get_pdr_req(instanceId, recordHandle, 0, PLDM_GET_FIRSTPART,
                               UINT16_MAX, 0, request, PLDM_GET_PDR_REQ_BYTES);
        if (rc != PLDM_SUCCESS)
        {
            requester.markFree(mctp_eid, instanceId);
            std::cerr << "Failed to encode_get_pdr_req, rc = " << rc
                      << std::endl;
            return;
        }

        uint8_t* responseMsg = nullptr;
        size_t responseMsgSize{};
        auto requesterRc =
            pldm_send_recv(mctp_eid, mctp_fd, requestMsg.data(),
                           requestMsg.size(), &responseMsg, &responseMsgSize);
        std::unique_ptr<uint8_t, decltype(std::free)*> responseMsgPtr{
            responseMsg, std::free};
        requester.markFree(mctp_eid, instanceId);
        if (requesterRc != PLDM_REQUESTER_SUCCESS)
        {
            std::cerr << "Failed to send msg to fetch pdrs, rc = "
                      << requesterRc << std::endl;
            return;
        }

        auto responsePtr =
            reinterpret_cast<struct pldm_msg*>(responseMsgPtr.get());
        uint8_t completionCode{};
        uint32_t nextRecordHandle{};
        uint32_t nextDataTransferHandle{};
        uint8_t transferFlag{};
        uint16_t respCount{};
        uint8_t transferCRC{};
        rc = decode_get_pdr_resp(
            responsePtr, responseMsgSize - sizeof(pldm_msg_hdr),
            &completionCode, &nextRecordHandle, &nextDataTransferHandle,
            &transferFlag, &respCount, nullptr, 0, &transferCRC);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Failed to decode_get_pdr_resp, rc = " << rc
                      << std::endl;
        }
        else
        {
            std::vector<uint8_t> pdr(respCount, 0);
            rc = decode_get_pdr_resp(
                responsePtr, responseMsgSize - sizeof(pldm_msg_hdr),
                &completionCode, &nextRecordHandle, &nextDataTransferHandle,
                &transferFlag, &respCount, pdr.data(), respCount, &transferCRC);
            if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
            {
                std::cerr << "Failed to decode_get_pdr_resp: "
                          << "rc=" << rc
                          << ", cc=" << static_cast<unsigned>(completionCode)
                          << std::endl;
            }
            else
            {
                auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(pdr.data());
                if (pdrHdr->type == PLDM_PDR_ENTITY_ASSOCIATION)
                {
                    mergeEntityAssociations(pdr);
                }
                else
                {
                    pldm_pdr_add(repo, pdr.data(), respCount, 0, true);
                }
            }
        }
    }
}

bool HostPDRHandler::getParent(EntityType type, pldm_entity& parent)
{
    auto found = parents.find(type);
    if (found != parents.end())
    {
        parent.entity_type = found->second.entity_type;
        parent.entity_instance_num = found->second.entity_instance_num;
        return true;
    }

    return false;
}

void HostPDRHandler::mergeEntityAssociations(const std::vector<uint8_t>& pdr)
{
    size_t numEntities{};
    pldm_entity* entities = nullptr;
    bool merged = false;
    auto entityPdr = reinterpret_cast<pldm_pdr_entity_association*>(
        const_cast<uint8_t*>(pdr.data()) + sizeof(pldm_pdr_hdr));

    pldm_entity_association_pdr_extract(pdr.data(), pdr.size(), &numEntities,
                                        &entities);
    for (size_t i = 0; i < numEntities; ++i)
    {
        pldm_entity parent{};
        if (getParent(entities[i].entity_type, parent))
        {
            auto node = pldm_entity_association_tree_find(entityTree, &parent);
            if (node)
            {
                pldm_entity_association_tree_add(entityTree, &entities[i], node,
                                                 entityPdr->association_type);
                merged = true;
            }
        }
    }
    free(entities);

    if (merged)
    {
        pldm_entity_association_pdr_add(entityTree, repo, true);
    }
}

} // namespace pldm
