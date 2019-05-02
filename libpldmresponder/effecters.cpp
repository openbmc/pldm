#include "effecters.hpp"

namespace pldm
{

namespace responder
{

namespace effecter
{

Id nextId()
{
    static Id id = 0;
    return ++id;
}

} // namespace effecter
} // namespace responder
} // namespace pldm
