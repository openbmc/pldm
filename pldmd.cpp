#include "libpldmresponder/base.hpp"

int main(int argc, char** argv)
{
    pldm::responder::base::registerHandlers();
}
