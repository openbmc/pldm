#pragma once

#include <memory>

/* GCC-15 doesn't support `start_lifetime_as` yet, but GCC-16 can treat
 * raw reinterpret_cast (and placement new) as undefined behavior.
 * Provide a polyfill for the standard `std::start_lifetime_as` function
 * as backwards compatibility until we migrate to GCC-16 fully.
 */
#ifndef __cpp_lib_start_lifetime_as
// NOLINTBEGIN(cert-dcl58-cpp)
namespace std
{
template <class T>
T* start_lifetime_as(void* p) noexcept
{
    return reinterpret_cast<T*>(p);
}

template <class T>
const T* start_lifetime_as(const void* p) noexcept
{
    return reinterpret_cast<const T*>(p);
}
} // namespace std
// NOLINTEND(cert-dcl58-cpp)
#endif
