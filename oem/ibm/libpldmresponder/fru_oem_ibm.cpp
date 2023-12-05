#include "fru_oem_ibm.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{
namespace oem_ibm_fru
{
void pldm::responder::oem_ibm_fru::Handler::setFruHandler(
    pldm::responder::fru::Handler* handler)
{
    fruHandler = handler;
}
} // namespace oem_ibm_fru
} // namespace responder
} // namespace pldm
