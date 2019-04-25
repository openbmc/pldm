#include <systemd/sd-event.h>

namespace pldm
{
namespace tool
{
namespace receiver
{

void setup(sd_event* loop);
void teardown();

} // namespace receiver
} // namespace tool
} // namespace pldm
