#pragma once

#include <memory>

/*
 * C++23 [obj.lifetime] `std::start_lifetime_as` is not yet implemented in
 * GCC-15 (no `__cpp_lib_start_lifetime_as`).  GCC-16+ may treat a bare
 * `reinterpret_cast` as undefined behaviour because the compiler's assumed
 * aliasing rules let it ignore that a new object now occupies the storage.
 *
 * The standard specifies that `start_lifetime_as` is equivalent to:
 *   return std::launder(reinterpret_cast<T*>(p));
 *
 * See:
 *   - https://wg21.link/P0593R6  (implicit object creation)
 *   - https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/n4928.pdf
 *     [obj.lifetime]
 *
 * This polyfill can be dropped once we move to a compiler that provides the
 * real `std::start_lifetime_as`.
 */
#ifndef __cpp_lib_start_lifetime_as
// NOLINTBEGIN(cert-dcl58-cpp)
namespace std
{
template <class T>
T* start_lifetime_as(void* p) noexcept
{
    return std::launder(reinterpret_cast<T*>(p));
}

template <class T>
const T* start_lifetime_as(const void* p) noexcept
{
    return std::launder(reinterpret_cast<const T*>(p));
}
} // namespace std
// NOLINTEND(cert-dcl58-cpp)
#endif
