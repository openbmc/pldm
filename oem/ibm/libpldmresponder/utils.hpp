#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace pldm
{
namespace responder
{
namespace utils
{
namespace fs = std::filesystem;
using Json = nlohmann::json;

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

/** @brief Converts a binary file to json data
 *  This function converts bson data stored in a binary file to
 *  nlohmann json data
 *
 *  @param[in] path     - binary file path to fetch the bson data
 *
 *  @return   on success returns nlohmann::json object
 */
Json convertBinFileToJson(const fs::path& path);

/** @brief Converts a json data in to a binary file
 *  This function converts the json data to a binary json(bson)
 *  format and copies it to specified destination file.
 *
 *  @param[in] jsonData - nlohmann json data
 *  @param[in] path     - destination path to store the bson data
 *
 *  @return   None
 */
void convertJsonToBinaryFile(const Json& jsonData, const fs::path& path);

/** @brief Clear License Status
 *  This function clears all the license status to "Unknown" during
 *  reset reload operation or when host is coming down to off state.
 *  During the genesis mode, it skips the license status update.
 *
 *  @return   None
 */
void clearLicenseStatus();

/** @brief Create or update the d-bus license data
 *  This function creates or updates the d-bus license details. If the input
 *  input flag is 1, then new license data will be created and if the the input
 *  flag is 2 license status will be cleared.
 *
 *  @param[in] flag - input flag, 1 : create and 2 : clear
 *
 *  @return   on success returns PLDM_SUCCESS
 *            on failure returns -1
 */
int createOrUpdateLicenseDbusPaths(const uint8_t& flag);

/** @brief Create or update the license bjects
 *  This function creates or updates the license objects as per the data passed
 *  from host.
 *
 *  @return   on success returns PLDM_SUCCESS
 *            on failure returns -1
 */
int createOrUpdateLicenseObjs();

/** @brief checks if a pcie adapter is IBM specific
 *         cable card
 *  @param[in] objPath - FRU object path
 *
 *  @return bool - true if IBM specific card
 */
bool checkIfIBMCableCard(const std::string& objPath);

/** @brief checks whether the fru is actually present
 *  @param[in] objPath - the fru object path
 *
 *  @return bool to indicate presence or absence
 */
bool checkFruPresence(const char* objPath);

/** @brief finds the ports under an adapter
 *  @param[in] cardObjPath - D-Bus object path for the adapter
 *  @param[out] portObjects - the ports under the adapter
 */
void findPortObjects(const std::string& cardObjPath,
                     std::vector<std::string>& portObjects);
} // namespace utils
} // namespace responder
} // namespace pldm
