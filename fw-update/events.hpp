#pragma once

#include <sdbusplus/message/native_types.hpp>

#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace pldm::fw_update
{

/** @class Events
 *
 *  Commits the xyz.openbmc_project.Software.Update events of a firmware
 *  update, as defined in Update.events.yaml of phosphor-dbus-interfaces.
 */
class Events
{
  public:
    Events() = delete;
    Events(const Events&) = delete;
    Events(Events&&) = delete;
    Events& operator=(const Events&) = delete;
    Events& operator=(Events&&) = delete;
    ~Events() = default;

    /** @brief Constructor
     *
     *  @param[in] targetPath - Object path of the software object updated
     */
    explicit Events(sdbusplus::object_path targetPath) :
        targetPath(std::move(targetPath))
    {}

    /** @brief The package has been associated with the target
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateTargetDetermined(const std::string& imageIdentifier);

    /** @brief The component image transfer to the FD is starting
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateTransferringToComponent(const std::string& imageIdentifier);

    /** @brief The FD is verifying the transferred component image
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateVerifyingAtComponent(const std::string& imageIdentifier);

    /** @brief The FD is applying the component image
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateInstallingOnComponent(const std::string& imageIdentifier);

    /** @brief The update of the target completed successfully
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateUpdateSuccessful(const std::string& imageIdentifier);

    /** @brief The image failed verification
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateVerificationFailed(const std::string& imageIdentifier);

    /** @brief The component image failed to transfer to the FD
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateTransferFailed(const std::string& imageIdentifier);

    /** @brief The FD failed to apply or activate the component image
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateActivateFailed(const std::string& imageIdentifier);

    /** @brief The package or component does not apply to the target
     *
     *  @param[in] imageIdentifier - PackageVersionString of the package,
     *                               empty before the package header is parsed
     */
    void generateUpdateNotApplicable(const std::string& imageIdentifier);

  private:
    /** @brief Generate an event declaring TargetName before ImageIdentifier
     *
     *  @param[in] eventName - Name of the event, for journal messages
     *  @param[in] imageIdentifier - Identifier of the image applied
     */
    template <typename EventType>
    void generateTargetFirst(std::string_view eventName,
                             const std::string& imageIdentifier);

    /** @brief Generate an event declaring ImageIdentifier before TargetName
     *
     *  @param[in] eventName - Name of the event, for journal messages
     *  @param[in] imageIdentifier - Identifier of the image applied
     */
    template <typename EventType>
    void generateImageFirst(std::string_view eventName,
                            const std::string& imageIdentifier);

    /** @brief Report a commit that did not reach phosphor-logging
     *
     *  @param[in] eventName - Name of the event that failed to commit
     *  @param[in] e - The failure
     */
    void logCommitFailure(std::string_view eventName,
                          const std::exception& e) const;

    /** @brief Object path of the software object updated */
    sdbusplus::object_path targetPath;
};

} // namespace pldm::fw_update
