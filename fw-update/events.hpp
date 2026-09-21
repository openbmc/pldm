#pragma once

#include <sdbusplus/message/native_types.hpp>

#include <optional>
#include <string>

namespace pldm::fw_update::events
{

/* Commits the xyz.openbmc_project.Software.Update events of a firmware
   update, as defined in Update.events.yaml of phosphor-dbus-interfaces.

   targetPath is the software object the update reports against, nullopt for
   an update that names none, which reports nothing. imageIdentifier is the
   PackageVersionString of the package, empty before the header is parsed. */

/** @brief The package has been associated with the target
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateTargetDetermined(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

/** @brief The component image transfer to the FD is starting
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateTransferringToComponent(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

/** @brief The FD is verifying the transferred component image
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateVerifyingAtComponent(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

/** @brief The FD is applying the component image
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateInstallingOnComponent(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

/** @brief The update of the target completed successfully
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateUpdateSuccessful(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

/** @brief The image failed verification
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateVerificationFailed(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

/** @brief The component image failed to transfer to the FD
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateTransferFailed(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

/** @brief The FD failed to apply or activate the component image
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateActivateFailed(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

/** @brief The package or component does not apply to the target
 *
 *  @param[in] targetPath - Software object updated, nullopt to report
 *                          nothing
 *  @param[in] imageIdentifier - PackageVersionString of the package
 */
void generateUpdateNotApplicable(
    const std::optional<sdbusplus::object_path>& targetPath,
    const std::string& imageIdentifier);

} // namespace pldm::fw_update::events
