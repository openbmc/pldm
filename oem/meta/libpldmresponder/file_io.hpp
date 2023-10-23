#pragma once

#include "common/utils.hpp"
#include "pldmd/handler.hpp"

#include <libpldm/oem/meta/file_io.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{
namespace oem_meta
{

static std::map<uint8_t, int> tidToSlotMap;

constexpr auto decodeDataMaxLength = 32;

enum pldm_oem_meta_file_io_type : uint8_t
{
    POST_CODE = 0x00,
};

int setupTidToSlotMappingTable();

class Handler : public CmdHandler
{
  public:
    Handler()
    {
        handlers.emplace(OEM_META_PLDM_WRITE_FILE,
                         [this](const pldm_msg* request, size_t payloadLength) {
            return this->writeFileIO(request, payloadLength);
        });

        if (setupTidToSlotMappingTable() != PLDM_SUCCESS)
        {
            error("Fail to setup tid to slot mapping table");
        }
    }

  private:
    /** @brief Handler for writeFileIO command
     *
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */
    Response writeFileIO(const pldm_msg* request, size_t payloadLength);
};

} // namespace oem_meta
} // namespace responder
} // namespace pldm
