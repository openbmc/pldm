#pragma once

#include "update_manager.hpp"

#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <filesystem>
#include <memory>

namespace pldm::fw_update
{

using UpdateIntf = sdbusplus::xyz::openbmc_project::Software::server::Update;
using ApplyTimeIntf =
    sdbusplus::xyz::openbmc_project::Software::server::ApplyTime;

class CodeUpdater : public UpdateIntf
{
  public:
    CodeUpdater() = delete;
    CodeUpdater(const CodeUpdater&) = delete;
    CodeUpdater(CodeUpdater&&) = delete;
    CodeUpdater& operator=(const CodeUpdater&) = delete;
    CodeUpdater& operator=(CodeUpdater&&) = delete;
    ~CodeUpdater() = default;

    CodeUpdater(sdbusplus::bus::bus& bus, const std::string& path,
                std::shared_ptr<UpdateManager> updateManager) :
        UpdateIntf(bus, path.c_str()),
        updateManager(updateManager),
        objPath(path)
    {}

  private:
    virtual sdbusplus::message::object_path
        startUpdate(sdbusplus::message::unix_fd image,
                    ApplyTimeIntf::RequestedApplyTimes applyTime) override;

    bool writeToSstream(int imageFd, std::stringstream& path);

    std::shared_ptr<UpdateManager> updateManager;

    std::string objPath;
};

} // namespace pldm::fw_update
                              