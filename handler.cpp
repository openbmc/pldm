#include "handler.hpp"

#include <cassert>

namespace pldm
{

namespace responder
{

Response CmdHandler::onlyCCResponse(const pldm_msg* request, uint8_t cc)
{
    Response response(sizeof(pldm_msg), 0);
    auto ptr = reinterpret_cast<pldm_msg*>(response.data());
    auto rc = encode_only_cc_resp(request->hdr.instance_id, request->hdr.type,
                                  request->hdr.command, cc, ptr);
    assert(rc == PLDM_SUCCESS);
    return response;
}

} // namespace responder

} // namespace pldm