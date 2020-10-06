#pragma once

#include <string>

namespace pldm
{
namespace responder
{
namespace utils
{

/** @brief Setup UNIX socket
 *
 *  @param[in] socketInterface - unix socket path
 *  @return PLDM completion code
 */
int setupUnixSocket(const std::string& socketInterface);

/** @brief Write data on UNIX socket
 *
 *  @param[in] sock - unix socket
 *  @param[in] buf -  data buffer
 *  @param[in] blockSize - block size of data to write
 *  @return PLDM completion code
 */
int writeToUnixSocket(const int sock, const char* buf,
                      const uint64_t blockSize);
} // namespace utils
} // namespace responder
} // namespace pldm
