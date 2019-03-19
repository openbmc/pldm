#include "registration.hpp"
#ifdef PLDMTOOL
#include "pldmtool-receiver.hpp"
#endif

#include <systemd/sd-event.h>

#include <cstdlib>

int main(int argc, char** argv)
{
    pldm::responder::base::registerHandlers();

    sd_event* loop = nullptr;
    sd_event_default(&loop);

#ifdef PLDMTOOL
    pldm::tool::receiver::setup(loop);
#endif

    sd_event_loop(loop);

#ifdef PLDMTOOL
    pldm::tool::receiver::teardown();
#endif

    return EXIT_SUCCESS;
}
