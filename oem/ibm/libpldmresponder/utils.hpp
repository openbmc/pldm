#pragma once

#include <cstdint>
#include <string>

namespace pldm
{
namespace responder
{
namespace utils
{

/** @brief Setup UNIX socket
 *  This function creates listening socket in non-blocking mode and allows only
 *  one socket connection. returns accepted socket after accepting connection
 *  from peer.
 *
 *  @param[in] socketInterface - unix socket path
 *  @return   on success returns accepted socket fd
 *            on failure returns -1
 */
int setupUnixSocket(const std::string& socketInterface);

/** @brief Write data on UNIX socket
 *  This function writes given data to a non-blocking socket.
 *  Irrespective of block size, this function make sure of writing given data
 *  on unix socket.
 *
 *  @param[in] sock - unix socket
 *  @param[in] buf -  data buffer
 *  @param[in] blockSize - size of data to write
 *  @return   on success retruns  0
 *            on failure returns -1

 */
int writeToUnixSocket(const int sock, const char* buf,
                      const uint64_t blockSize);

/** @brief checks if a pcie adapter is IBM specific
 *         cable card
 *  @param[in] objPath - FRU object path
 *
 *  @return bool - true if IBM specific card
 */
bool checkIfIBMCableCard(const std::string& objPath);

} // namespace utils
} // namespace responder
} // namespace pldm
