#pragma once

#include "xyz/openbmc_project/PLDM/Pdr/server.hpp"

#include <iostream>
#include <map>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <vector>

#include "libpldm/pdr.h"
#include "libpldm/platform.h"

namespace pldm
{
namespace dbus_api
{
using PdrIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::PLDM::server::Pdr>;

class Pdr : public PdrIntf
{
  public:
    Pdr() = delete;
    Pdr(const Pdr&) = delete;
    Pdr& operator=(const Pdr&) = delete;
    Pdr(Pdr&&) = delete;
    Pdr& operator=(Pdr&&) = delete;
    virtual ~Pdr() = default;

    Pdr(sdbusplus::bus::bus& bus, const std::string& path,
        const pldm_pdr* repo) :
        PdrIntf(bus, path.c_str(), repo),
        pdrRepo(repo)
    {
        std::cerr << "constructor" << std::endl;
    };

    std::vector<uint8_t> findStateEffecterPDR(uint8_t /*tid*/,
                                              uint16_t containerId,
                                              uint16_t entityType,
                                              uint16_t entityInstance,
                                              uint16_t stateSetId) override;

  private:
    const pldm_pdr* pdrRepo;
};

} // namespace dbus_api
} // namespace pldm
