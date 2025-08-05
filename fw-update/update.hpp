#pragma once

#include <sys/mman.h>

#include <xyz/openbmc_project/Software/ApplyTime/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <spanstream>
#include <system_error>

namespace pldm
{

namespace fw_update
{

class UpdateManager;

/* RAII wrapper for mmap cleanup */
class MappedMemory
{
  private:
    void* data_;
    size_t size_;

  public:
    MappedMemory(int fd, size_t size) : data_(MAP_FAILED), size_(size)
    {
        data_ = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data_ == MAP_FAILED)
        {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to mmap firmware image");
        }
    }

    ~MappedMemory()
    {
        if (data_ != MAP_FAILED)
        {
            munmap(data_, size_);
        }
    }

    MappedMemory(const MappedMemory&) = delete;
    MappedMemory& operator=(const MappedMemory&) = delete;

    MappedMemory(MappedMemory&& other) noexcept :
        data_(other.data_), size_(other.size_)
    {
        other.data_ = MAP_FAILED;
        other.size_ = 0;
    }

    const std::uint8_t* data() const
    {
        return static_cast<const std::uint8_t*>(data_);
    }

    size_t size() const noexcept
    {
        return size_;
    }
};

/* Wrapper that owns both the mapped memory and the span stream
 * so that the memory stays alive while the stream is in use.
 */
class MappedStreamHolder
{
  private:
    std::shared_ptr<MappedMemory> mapped;
    std::ispanstream spanStream;

  public:
    MappedStreamHolder(int fd, size_t size) :
        mapped(std::make_shared<MappedMemory>(fd, size)),
        spanStream(std::span<const char>(
            reinterpret_cast<const char*>(mapped->data()), mapped->size()))
    {}

    std::istream& stream()
    {
        return spanStream;
    }

    size_t size() const noexcept
    {
        return mapped->size();
    }

    const std::uint8_t* data() const noexcept
    {
        return mapped->data();
    }
};

using UpdateIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Update>;
using ApplyTimeIntf =
    sdbusplus::xyz::openbmc_project::Software::server::ApplyTime;

/** @class Update
 *
 *  Concrete implementation of xyz.openbmc_project.Software.Update D-Bus
 *  interface
 */
class Update : public UpdateIntf
{
  public:
    /** @brief Constructor
     *
     *  @param[in] bus - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] updateManager - Reference to FW update manager
     */
    Update(sdbusplus::bus::bus& bus, const std::string& path,
           UpdateManager* updateManager) :
        UpdateIntf(bus, path.c_str()), updateManager(updateManager),
        objPath(path)
    {}

    virtual sdbusplus::message::object_path startUpdate(
        sdbusplus::message::unix_fd image,
        ApplyTimeIntf::RequestedApplyTimes applyTime) override;

    ~Update() noexcept override = default;

  private:
    UpdateManager* updateManager;
    const std::string objPath;
};

} // namespace fw_update

} // namespace pldm
