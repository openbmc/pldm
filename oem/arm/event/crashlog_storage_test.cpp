#include "crashlog_storage.hpp"

#include <iostream>
#include <mutex>
#include <set>
#include <thread>
namespace st = pldm::oem_arm::crashlog::storage;
void require(bool v)
{
    if (!v)
        throw std::runtime_error("storage invariant failed");
}
template <class F>
void rejects(F f)
{
    bool rejected = false;
    try
    {
        f();
    }
    catch (...)
    {
        rejected = true;
    }
    require(rejected);
}
int main()
{
    require(st::validName("agi_cpu_crashdump_12"));
    require(st::validName("agi_cpu_crashdump"));
    for (auto bad : {"agi_cpu_crashdump_", "../agi_cpu_crashdump_1",
                     "agi_cpu_crashdump_1/x", "agi_cpu_crashdump_1x"})
        require(!st::validName(bad));
    require(!st::validName(std::string("agi_cpu_crashdump_1\0x", 20)));
    auto base = std::filesystem::temp_directory_path() /
                std::string("pft-store-" + std::to_string(getpid()));
    auto root = base / "state";
    require(st::reserveSourceId(root) == 1);
    require(st::reserveSourceId(root) == 2);
    auto a = st::createWorkDirectory(root), b = st::createWorkDirectory(root);
    require(a != b);
    struct stat statbuf{};
    require(stat(a.c_str(), &statbuf) == 0 && (statbuf.st_mode & 0777) == 0700);
    std::set<uint32_t> ids;
    std::mutex lock;
    auto worker = [&] {
        for (int i = 0; i < 20; ++i)
        {
            auto id = st::reserveSourceId(root);
            std::lock_guard g(lock);
            require(ids.insert(id).second);
        }
    };
    std::thread t1(worker), t2(worker);
    t1.join();
    t2.join();
    require(ids.size() == 40 && st::reserveSourceId(root) == 43);
    std::filesystem::create_directory_symlink(root, base / "badroot");
    rejects([&] { st::prepareRoot(base / "badroot"); });
    std::filesystem::remove(root / "next-id");
    std::filesystem::create_symlink(a, root / "next-id");
    rejects([&] { st::reserveSourceId(root); });
    std::filesystem::remove(root / "next-id");
    {
        st::Fd fd(open((root / "next-id").c_str(), O_CREAT | O_WRONLY, 0600));
        require(write(fd.get(), "x", 1) == 1);
    }
    rejects([&] { st::reserveSourceId(root); });
    std::filesystem::remove_all(base);
    std::cout
        << "Storage checks passed, including concurrent reservations and symlink rejection\n";
}
