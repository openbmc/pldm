#include "libpldmresponder/base.hpp"
#include "libpldmresponder/platform.hpp"
#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/bios.hpp"
#include "mctp-lpc-setup.hpp"

#ifdef PLDMTOOL
#include "pldmtool-receiver.hpp"
#endif

#include <systemd/sd-event.h>

#include <cstdlib>

int main(int argc, char** argv)
{
    pldm::responder::base::registerHandlers();
    pldm::responder::platform::registerHandlers();
    pldm::responder::fileio::registerHandlers();
    pldm::responder::bios::registerHandlers();

    sd_event* loop = nullptr;
    sd_event_default(&loop);

#ifdef PLDMTOOL
    pldm::tool::receiver::setup(loop);
#endif

    mctp_lpc::setup();
    sd_event_loop(loop);

#ifdef PLDMTOOL
    pldm::tool::receiver::teardown();
#endif

    return EXIT_SUCCESS;
}
