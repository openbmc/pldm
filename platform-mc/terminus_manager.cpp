#include "terminus_manager.hpp"
#include "manager.hpp"

namespace pldm
{
namespace platform_mc
{

void TerminusManager::discoverTerminus(const std::vector<mctp_eid_t>& eids)
{
    _eids = eids;
    manager->stopSensorPolling();
    scanTerminus();
}

void TerminusManager::scanTerminus()
{
    deferredScanTerminusEvent = std::make_unique<sdeventplus::source::Defer>(
        event,
        std::bind(std::mem_fn(&TerminusManager::processScanTerminusEvent), this,
                  std::placeholders::_1));
}

void TerminusManager::processScanTerminusEvent(
    sdeventplus::source::EventBase& /* source */)
{
    deferredScanTerminusEvent.reset();
    if (!_eids.empty())

    {
        auto eid = _eids.back();
        _eids.pop_back();
        sendGetTID(eid);
    }
    else
    {
        _terminuses = terminuses;
        initTerminus();
    }
}

void TerminusManager::sendGetTID(mctp_eid_t eid)
{
    auto instanceId = requester.getInstanceId(eid);
    Request requestMsg(sizeof(pldm_msg_hdr));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    auto rc = encode_get_tid_req(instanceId, request);
    if (rc)
    {
        requester.markFree(eid, instanceId);
        std::cerr << "encode_get_tid_req failed, EID=" << unsigned(eid)
                  << ", RC=" << rc << "\n";
        return scanTerminus();
    }

    rc = handler.registerRequest(
        eid, instanceId, PLDM_BASE, PLDM_GET_TID, std::move(requestMsg),
        std::move(std::bind_front(&TerminusManager::handleRespGetTID, this)));
    if (rc)
    {
        std::cerr << "Failed to send GetTID request, EID=" << unsigned(eid)
                  << ", RC=" << rc << "\n ";
        return scanTerminus();
    }
}

void TerminusManager::handleRespGetTID(mctp_eid_t eid, const pldm_msg* response,
                                       size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        std::cerr << "No response received for getTID, EID=" << unsigned(eid)
                  << "\n";
        return scanTerminus();
    }

    uint8_t assignedTid = 0;
    uint8_t tid;
    uint8_t completionCode;
    auto rc = decode_get_tid_resp(response, respMsgLen, &completionCode, &tid);
    if (rc)
    {
        std::cerr << "decode_get_tid_resp response failed, EID=" << unsigned(eid)
                  << ", RC=" << rc << "\n";
        return scanTerminus();
    }

    if (completionCode)
    {
        std::cerr << "GetTID response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return scanTerminus();
    }

    if (tid > 0x00 && tid < 0xff)
    {
        auto mappedEID = getMappedEID(tid);
        if (mappedEID && mappedEID.value() == eid)
        {
            assignedTid = tid;
        }
    }

    if (!assignedTid)
    {
        auto t = mapTID(eid);
        if (!t)
        {
            std::cerr << "mapTID() returned null." << std::endl;
            return scanTerminus();
        }
        assignedTid = t.value();
    }
    sendSetTID(eid, assignedTid);
}

void TerminusManager::sendSetTID(mctp_eid_t eid, uint8_t tid)
{
    auto instanceId = requester.getInstanceId(eid);
    Request requestMsg(sizeof(pldm_msg_hdr));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    request->hdr.request = PLDM_REQUEST;
    request->hdr.datagram = 0;
    request->hdr.reserved = 0;
    request->hdr.instance_id = instanceId;
    request->hdr.header_ver = PLDM_CURRENT_VERSION;
    request->hdr.type = PLDM_BASE;
    request->hdr.command = PLDM_SET_TID;
    request->payload[0] = tid;

    auto rc = handler.registerRequest(
        eid, instanceId, PLDM_BASE, PLDM_SET_TID, std::move(requestMsg),
        std::move(std::bind_front(&TerminusManager::handleRespSetTID, this)));
    if (rc)
    {
        std::cerr << "Failed to send SetTID request, EID=" << unsigned(eid)
                  << ", RC=" << rc << "\n ";
        auto t = getMappedTID(eid);
        if (t)
        {
            unmapTID(t.value());
        }
        return scanTerminus();
    }
}

void TerminusManager::handleRespSetTID(mctp_eid_t eid, const pldm_msg* response,
                                       size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        std::cerr << "No response received for setTID, EID=" << unsigned(eid)
                  << "\n";
        return scanTerminus();
    }

    auto tid = getMappedTID(eid);
    if (!tid)
    {
        return scanTerminus();
    }

    uint8_t completionCode = response->payload[0];
    if (completionCode)
    {
        std::cerr << "setTID response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";

        unmapTID(tid.value());
    }
    else
    {
        auto terminus = std::make_shared<Terminus>(eid, tid.value(), event,
                                                   handler, requester, this);
        terminuses.emplace_back(std::move(terminus));
    }
    return scanTerminus();
}

void TerminusManager::initTerminus()
{
    deferredInitTerminusEvent = std::make_unique<sdeventplus::source::Defer>(
        event,
        std::bind(std::mem_fn(&TerminusManager::processInitTerminusEvent), this,
                  std::placeholders::_1));
}

void TerminusManager::processInitTerminusEvent(
    sdeventplus::source::EventBase& /* source */)
{
    deferredInitTerminusEvent.reset();
    if (!_terminuses.empty())
    {
        auto t = _terminuses.back();
        _terminuses.pop_back();
        t->fetchPDRs();
    }
    else
    {
        manager->startSensorPolling();
    }
}

std::optional<uint8_t> TerminusManager::getMappedEID(uint8_t tid)
{
    if (tidPool.at(tid))
    {
        return tidPool.at(tid);
    }
    return std::nullopt;
}

std::optional<uint8_t> TerminusManager::getMappedTID(uint8_t eid)
{
    for (size_t i = 1; i < tidPool.size(); i++)
    {
        if (tidPool.at(i) == eid)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<uint8_t> TerminusManager::mapTID(uint8_t eid)
{
    for (size_t i = 1; i < tidPool.size(); i++)
    {
        if (tidPool.at(i) == 0x0)
        {
            tidPool.at(i) = eid;
            return i;
        }
    }
    return std::nullopt;
}

void TerminusManager::unmapTID(uint8_t tid)
{
    tidPool.at(tid) = 0x0;
}

} // namespace platform_mc
} // namespace pldm