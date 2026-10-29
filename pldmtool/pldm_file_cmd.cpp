#include "pldm_file_cmd.hpp"

#include "common/utils.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <CLI/CLI.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>

#include <array>
#include <cerrno>
#include <cstring>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace pldmtool
{
namespace file
{

namespace
{

constexpr const char* PLDMService = "xyz.openbmc_project.PLDM";
constexpr const char* FileInterface = "xyz.openbmc_project.PLDM.File";
constexpr const char* FileRootPath = "/xyz/openbmc_project/pldm/file";
constexpr const char* DBusProperties = "org.freedesktop.DBus.Properties";

/** @brief Hex-dump bytes to stdout, 16 bytes per row */
static void hexDump(const std::vector<uint8_t>& data, uint64_t baseOffset = 0)
{
    constexpr size_t rowWidth = 16;
    for (size_t i = 0; i < data.size(); i += rowWidth)
    {
        std::cout << std::hex << std::setw(8) << std::setfill('0')
                  << baseOffset + i << "  ";
        for (size_t j = i; j < i + rowWidth; ++j)
        {
            if (j < data.size())
            {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(data[j]) << " ";
            }
            else
            {
                std::cout << "   ";
            }
            if (j == i + 7)
            {
                std::cout << " ";
            }
        }
        std::cout << " |";
        for (size_t j = i; j < i + rowWidth && j < data.size(); ++j)
        {
            char c = static_cast<char>(data[j]);
            std::cout << (std::isprint(c) ? c : '.');
        }
        std::cout << "|\n";
    }
    std::cout << std::dec;
}

/** @brief Fetch a single string property via D-Bus Properties.Get */
static std::string getStringProp(
    sdbusplus::bus_t& bus, const std::string& objPath, const std::string& prop)
{
    try
    {
        auto m = bus.new_method_call(PLDMService, objPath.c_str(),
                                     DBusProperties, "Get");
        m.append(FileInterface, prop);
        auto reply = bus.call(m);
        std::variant<std::string> val;
        reply.read(val);
        return std::get<std::string>(val);
    }
    catch (...)
    {
        return {};
    }
}

/** @brief Fetch a single uint64 property via D-Bus Properties.Get */
static uint64_t getUint64Prop(sdbusplus::bus_t& bus, const std::string& objPath,
                              const std::string& prop)
{
    try
    {
        auto m = bus.new_method_call(PLDMService, objPath.c_str(),
                                     DBusProperties, "Get");
        m.append(FileInterface, prop);
        auto reply = bus.call(m);
        std::variant<uint64_t> val;
        reply.read(val);
        return std::get<uint64_t>(val);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::string("Cannot read file size: ") + e.what());
    }
}

/** @brief Get all PLDM File object paths via the ObjectMapper subtree query */
static std::vector<std::string> listFileObjects()
{
    pldm::utils::DBusHandler handler;
    try
    {
        auto subtree = handler.getSubtree(FileRootPath, 0, {FileInterface});
        std::vector<std::string> paths;
        paths.reserve(subtree.size());
        for (const auto& [objPath, _] : subtree)
        {
            paths.emplace_back(objPath);
        }
        return paths;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to list PLDM file objects: " << e.what() << "\n";
        return {};
    }
}

/** @brief Print a numbered list of files with Name, Purpose and Size */
static std::vector<std::string> printFileList()
{
    auto bus = sdbusplus::bus::new_default();
    auto paths = listFileObjects();

    if (paths.empty())
    {
        std::cout << "No PLDM file objects found.\n";
        return paths;
    }

    std::cout << std::left << std::setw(4) << "#" << std::setw(24) << "Name"
              << std::setw(20) << "Purpose" << std::setw(12) << "Size"
              << "Object Path\n";
    std::cout << std::string(80, '-') << "\n";

    int idx = 0;
    for (const auto& path : paths)
    {
        auto name = getStringProp(bus, path, "Name");
        auto purpose = getStringProp(bus, path, "Purpose");
        auto size = getUint64Prop(bus, path, "CurrentSize");

        // strip the enum prefix (e.g.
        // "xyz.openbmc_project.PLDM.File.PurposeType.")
        auto dot = purpose.rfind('.');
        if (dot != std::string::npos)
        {
            purpose = purpose.substr(dot + 1);
        }

        std::cout << std::left << std::setw(4) << idx++ << std::setw(24)
                  << (name.empty() ? "(unknown)" : name) << std::setw(20)
                  << (purpose.empty() ? "-" : purpose) << std::setw(12) << size
                  << path << "\n";
    }
    return paths;
}

/** @brief Open a file session and stream data to stdout (hex) or a file */
static void doRead(const std::string& objPath, uint32_t offset, uint32_t length,
                   bool exclusivity, const std::string& outputFile)
{
    auto bus = sdbusplus::bus::new_default();

    const auto size = getUint64Prop(bus, objPath, "CurrentSize");
    if (offset > size || (length != 0 && length > size - offset))
    {
        throw std::runtime_error("Requested range exceeds current file size");
    }
    const uint64_t expected = length == 0 ? size - offset : length;

    auto method = bus.new_method_call(PLDMService, objPath.c_str(),
                                      FileInterface, "Open");
    method.append(size_t{offset}, size_t{length}, exclusivity);

    sdbusplus::message_t reply;
    try
    {
        reply = bus.call(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        throw std::runtime_error(std::string("D-Bus Open failed: ") + e.what());
    }

    sdbusplus::message::unix_fd socketFd;
    try
    {
        reply.read(socketFd);
    }
    catch (const sdbusplus::exception_t& e)
    {
        throw std::runtime_error(
            std::string("Invalid Open reply: ") + e.what());
    }

    // The reply owns socketFd. Duplicate it for independent RAII ownership.
    pldm::utils::CustomFD input(fcntl(socketFd.fd, F_DUPFD_CLOEXEC, 0));
    if (input() < 0)
    {
        throw std::runtime_error("Cannot duplicate transfer descriptor");
    }
    pldm::utils::CustomFD output(
        outputFile.empty()
            ? -1
            : open(outputFile.c_str(),
                   O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
                   0600));
    if (!outputFile.empty() && output() < 0)
    {
        throw std::runtime_error("Cannot open output file");
    }
    uint64_t received = 0;
    std::array<uint8_t, 4096> buffer{};
    while (true)
    {
        pollfd watch{input(), POLLIN, 0};
        auto ready = poll(&watch, 1, 10000);
        if (ready < 0 && errno == EINTR)
        {
            continue;
        }
        if (ready <= 0 || (watch.revents & (POLLERR | POLLNVAL)))
        {
            throw std::runtime_error("Transfer socket timed out or failed");
        }
        auto count = recv(input(), buffer.data(), buffer.size(), MSG_DONTWAIT);
        if (count < 0 && (errno == EINTR || errno == EAGAIN))
        {
            continue;
        }
        if (count < 0)
        {
            throw std::runtime_error("Transfer socket read failed");
        }
        if (count == 0)
        {
            break;
        }
        if (static_cast<uint64_t>(count) > expected - received)
        {
            throw std::runtime_error("Transfer exceeds requested byte count");
        }
        received += count;
        if (output() >= 0)
        {
            ssize_t written = 0;
            while (written < count)
            {
                auto result =
                    write(output(), buffer.data() + written, count - written);
                if (result < 0 && errno == EINTR)
                {
                    continue;
                }
                if (result <= 0)
                {
                    throw std::runtime_error("Output write failed");
                }
                written += result;
            }
        }
        else
        {
            hexDump(std::vector<uint8_t>(buffer.begin(),
                                         buffer.begin() + count),
                    received - count);
        }
    }
    if (received != expected || (output() >= 0 && fsync(output()) < 0))
    {
        throw std::runtime_error(
            "Incomplete transfer or output synchronization failed");
    }
    std::cerr << "Read " << received << " bytes from " << objPath << "\n";
    // Legacy File.Open has no per-transfer terminal-status return value.
}

} // namespace

/** @brief ListFiles: show all available PLDM file objects */
class ListFiles
{
  public:
    ListFiles() = delete;
    explicit ListFiles(CLI::App* app)
    {
        app->callback([&]() { printFileList(); });
    }
};

/** @brief ReadFile: read by D-Bus object path or by index from ListFiles */
class ReadFile
{
  public:
    ReadFile() = delete;
    explicit ReadFile(CLI::App* app)
    {
        app->add_option("-p,--object-path", objPath,
                        "D-Bus object path of the PLDM File");
        app->add_option(
            "-i,--index", index,
            "Index from 'pldmtool file ListFiles' (use instead of -p)");
        app->add_option("-o,--offset", offset,
                        "Byte offset to start reading from (default: 0)");
        app->add_option(
            "-l,--length", length,
            "Number of bytes to read, 0 means whole file (default: 0)");
        app->add_option(
            "-f,--output-file", outputFile,
            "Write raw bytes to this path instead of hex-dumping to stdout");
        app->add_flag("-x,--exclusive", exclusivity,
                      "Request exclusive access to the file");

        app->callback([&]() {
            try
            {
                exec();
            }
            catch (const std::exception& e)
            {
                throw CLI::RuntimeError(e.what(), 1);
            }
            catch (const std::error_condition& e)
            {
                throw CLI::RuntimeError(e.message(), 1);
            }
        });
    }

  private:
    std::string objPath;
    int index = -1;
    uint32_t offset = 0;
    uint32_t length = 0;
    std::string outputFile;
    bool exclusivity = false;

    void exec()
    {
        std::string target = objPath;

        if (target.empty())
        {
            if (index < 0)
            {
                // No path and no index: print the list and prompt
                auto paths = printFileList();
                if (paths.empty())
                {
                    return;
                }
                std::cout << "\nEnter index to read: ";
                std::cin >> index;
            }

            // Resolve index to path
            auto paths = listFileObjects();
            if (index < 0 || static_cast<size_t>(index) >= paths.size())
            {
                throw CLI::RuntimeError("File index is out of range", 1);
            }
            target = paths[index];
        }

        doRead(target, offset, length, exclusivity, outputFile);
    }
};

void registerCommand(CLI::App& app)
{
    auto file =
        app.add_subcommand("file", "PLDM File Transfer (DSP0242) commands");
    file->require_subcommand(1);

    auto listFiles = file->add_subcommand(
        "ListFiles", "List all PLDM File Descriptor objects on D-Bus");
    static ListFiles listFilesCmd(listFiles);

    auto readFile = file->add_subcommand(
        "ReadFile", "Read data from a PLDM File Descriptor "
                    "(-p <path> | -i <index from ListFiles>)");
    static ReadFile readFileCmd(readFile);
}

} // namespace file
} // namespace pldmtool
