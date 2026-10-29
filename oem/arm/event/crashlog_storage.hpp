#pragma once

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pldm::oem_arm::crashlog::storage
{
inline bool validName(std::string_view name)
{
    constexpr std::string_view prefix = "agi_cpu_crashdump_";
    if (name == "agi_cpu_crashdump")
    {
        return true;
    }
    if (!name.starts_with(prefix) || name.size() == prefix.size())
    {
        return false;
    }
    for (auto ch : name.substr(prefix.size()))
    {
        if (ch < '0' || ch > '9')
        {
            return false;
        }
    }
    return true;
}

class Fd
{
  public:
    explicit Fd(int value) : value(value) {}
    ~Fd()
    {
        if (value >= 0)
        {
            close(value);
        }
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const
    {
        return value;
    }

  private:
    int value;
};

inline bool syncDirectory(const std::filesystem::path& path)
{
    Fd directory(
        open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    return directory.get() >= 0 && fsync(directory.get()) == 0;
}

inline bool syncFile(const std::filesystem::path& path)
{
    Fd file(open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    struct stat status{};
    return file.get() >= 0 && fstat(file.get(), &status) == 0 &&
           S_ISREG(status.st_mode) && fsync(file.get()) == 0;
}

inline void prepareRoot(const std::filesystem::path& root)
{
    // Synchronize a newly created state parent as well: syncing crashlogs
    // alone does not persist the /var/lib/pldm directory entry on first use.
    const auto parent = root.parent_path();
    if (!std::filesystem::is_directory(parent.parent_path()) ||
        (std::filesystem::create_directories(parent) &&
         !syncDirectory(parent.parent_path())))
    {
        throw std::runtime_error("Cannot persist crashlog state parent");
    }
    if (mkdir(root.c_str(), 0700) < 0 && errno != EEXIST)
    {
        throw std::runtime_error("Cannot create crashlog state directory");
    }
    struct stat status{};
    if (lstat(root.c_str(), &status) < 0 || !S_ISDIR(status.st_mode) ||
        status.st_uid != geteuid() || chmod(root.c_str(), 0700) < 0 ||
        !syncDirectory(root.parent_path()))
    {
        throw std::runtime_error(
            "Unsafe or unsynchronized crashlog state directory");
    }
}

inline std::filesystem::path createWorkDirectory(
    const std::filesystem::path& root)
{
    prepareRoot(root);
    auto pattern = (root / "collection-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    if (!mkdtemp(name.data()) || !syncDirectory(root))
    {
        throw std::runtime_error("Cannot reserve crashlog work directory");
    }
    return name.data();
}

// Local BMC collection identifier. It is not the MCP source request/dump ID.
// A failed reservation consumes an ID rather than risking reuse after restart.
inline uint32_t reserveSourceId(const std::filesystem::path& root)
{
    prepareRoot(root);
    Fd counter(open((root / "next-id").c_str(),
                    O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600));
    struct stat status{};
    if (counter.get() < 0 || fstat(counter.get(), &status) < 0 ||
        !S_ISREG(status.st_mode) || status.st_uid != geteuid() ||
        flock(counter.get(), LOCK_EX) < 0)
    {
        throw std::runtime_error("Cannot lock crashlog collection identifier");
    }
    // Another process may have advanced the counter while flock waited.
    if (fstat(counter.get(), &status) < 0)
    {
        throw std::runtime_error("Cannot read locked collection identifier");
    }
    uint32_t value = 1;
    if (status.st_size != 0 &&
        (status.st_size != sizeof(value) ||
         pread(counter.get(), &value, sizeof(value), 0) != sizeof(value)))
    {
        throw std::runtime_error("Invalid crashlog identifier state");
    }
    if (value == 0 || value == std::numeric_limits<uint32_t>::max())
    {
        throw std::runtime_error("Crashlog identifier space exhausted");
    }
    auto next = value + 1;
    if (pwrite(counter.get(), &next, sizeof(next), 0) != sizeof(next) ||
        fsync(counter.get()) < 0 || !syncDirectory(root))
    {
        throw std::runtime_error(
            "Cannot persist crashlog collection identifier");
    }
    return value;
}
} // namespace pldm::oem_arm::crashlog::storage
