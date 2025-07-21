#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/utils.hpp"

namespace pldm::responder::oem_meta
{
/** @class PostCodeHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to store incoming postcode
 */
class PostCodeHandler : public FileHandler
{
  public:
    PostCodeHandler() = delete;
    PostCodeHandler(const PostCodeHandler&) = delete;
    PostCodeHandler(PostCodeHandler&&) = delete;
    PostCodeHandler& operator=(const PostCodeHandler&) = delete;
    PostCodeHandler& operator=(PostCodeHandler&&) = delete;

    explicit PostCodeHandler(
        pldm_tid_t tid,
        const std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>&
            configurations) : tid(tid), configurations(configurations)
    {}

    ~PostCodeHandler() = default;

    /** @brief Method to store postcode list
     *  @param[in] data - post code
     *  @return  PLDM status code
     */
    int write(const message& data) override;

  private:
    /** @brief The terminus ID of the message source*/
    pldm_tid_t tid = 0;

    /** @brief Get existing configurations with MctpEndpoint*/
    const std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>&
        configurations;
};

} // namespace pldm::responder::oem_meta
