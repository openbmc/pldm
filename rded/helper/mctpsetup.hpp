#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

std::mutex m_mutex;
/**
 * @brief Execute a CLI command and return the command line result
 */
std::string exec(const char* cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    return result;
}
/**
 * @brief Extract the name from the command line result
 * The result returns a string of the format
 * "dev mctpserial0 index 13 address 0x(no-addr) net 7 mtu 68 up"
 *
 * To up link mctpserial0 for MCTP we need to extract the added "mctpserial0"
 *
 * @param[in] mctpLinkString - command line string returned after executing
 * "mctp link show"
 * @param[out] name - Name on which MCTP is to be set. e.g. "mctpserial0"
 * @param[out] count - this is the index that is appended to the name, i.e "0"
 * in "mctpserial0"
 *
 */
int extractNewName(std::string mctpLinkString, std::string* name,
                     std::string* count)
{
    size_t pos = 0;
    std::string token;
    std::string delimiter = "\n";
    std::vector<std::string> mctpPorts;
    while ((pos = mctpLinkString.find(delimiter)) != std::string::npos)
    {
        token = mctpLinkString.substr(0, pos);
        mctpPorts.push_back(token);
        mctpLinkString.erase(0, pos + delimiter.length());
    }
    if (!mctpLinkString.empty())
    {
        mctpPorts.push_back(mctpLinkString);
    }
    std::string lastEntry = mctpPorts.back();
    if ((pos = lastEntry.find("mctpserial")) != std::string::npos)
    {
        *name = lastEntry.substr(pos, 11);
        *count = lastEntry.substr(pos + 10, 1);
        return 0;
    }
    return -1;
}
/**
 * @brief Creating a unique network id from the udevid for a particular RDE
 * device
 *
 * Currently this takes the udev id and adds the digits in it to create a net id
 * For instance, for udev_id "1_1_4_1" the network id would be "7" (Sum of all
 * digits)
 *
 * @param[in] udevid - Udev id of the device
 */
int createNetIdFromUdevId(std::string udevid)
{
    // Creates a net id for a udev id by adding all the digits in the path
    // 1_1_4_1 = 7
    std::string delimiter = "_";
    int netId = 0;
    size_t pos = 0;
    std::string token;
    while ((pos = udevid.find(delimiter)) != std::string::npos)
    {
        token = udevid.substr(0, pos);
        netId = netId + stoi(token);
        udevid.erase(0, pos + delimiter.length());
    }
    if (!udevid.empty())
        netId = netId + stoi(udevid);
    return netId;
}
/**
 * @brief Sets up MCTP on on "mctpserial" port
 *
 * **Currently this has some random sleeps attached as it takes some time to
 * bring MCTP up. This function is only executed when a device reboots or the
 * system boots**
 *
 * @param[in] port - the port at which MCTP is to be setup, format- "/dev/tty*"
 * @param[in] udevid - udev id of the RDE device, which is a unique identifier
 * and remains same for the device even after reboots
 */
int setupOnePort(std::string port, std::string udevid)
{
    // TODO(b/284167548): Find an efficient way to set up MCTP without using CLI
    // commands This might help us get rid of sleeps

    // Wait for some time until the previous MCTP is set up if any
    sleep(2);

    // Mutex lock so that another device setup doesn't hinder the MCTP setup
    m_mutex.lock();
    int retryCounter = 0;
    bool setupComplete = false;
    // lock mutex
    int networkId;
    while (retryCounter < 4)
    {
        retryCounter++;
        std::cerr << "Triggering MCTP setup on port: " << port
                  << " with retry counter: " << retryCounter << "\n";
        // Executing link command
        std::string linkCmd = "mctp link serial " + port + " &";
        std::cerr << "Executing: " << linkCmd << "\n";
        int sysRc = system(linkCmd.c_str());
        if (sysRc)
        {
            std::cerr << "Error in setting up mctp link with return code: "
                      << std::to_string(sysRc) << "\n";
            return -1;
        }
        // MCTP link command takes some time to finish - hence a sleep of 5s
        sleep(5);
        // Extract name:
        std::string mctpName;
        std::string count;
        std::string mctpLinkShow = "mctp link show";
        std::string linkResult = exec(mctpLinkShow.c_str());
        std::cerr << "linkResult: " << linkResult << "\n";
        sysRc = extractNewName(linkResult, &mctpName, &count);
        if (sysRc)
        {
            std::cerr
                << "No mctp serial connection found. MCTP Link failed...\n";
            continue;
        }

        networkId = createNetIdFromUdevId(udevid);

        std::string upCmd = "mctp link set " + mctpName + " network " +
                             std::to_string(networkId) + " up";
        std::cerr << "Executing upcmd: " << upCmd << "\n";
        std::string networkSetResponse = exec(upCmd.c_str());
        if (networkSetResponse.find("invalid") != std::string::npos)
        {
            std::cerr << "Network setup failure... retrying\n";
            continue;
        }
        // Setting network id for a port takes some time to setup
        sleep(5);
        // Setting up local addresses
        std::string localAddrCmd = "mctp addr add 8 dev " + mctpName;
        std::string routeCmd = "mctp route add 9 via " + mctpName;
        std::cerr << "Executing local addr: " << localAddrCmd << "\n";
        exec(localAddrCmd.c_str());
        std::cerr << "Executing routeCmd: " << routeCmd << "\n";
        exec(routeCmd.c_str());
        // Setting up routes and local address takes time
        sleep(5);
        setupComplete = true;
        break;
    }
    // unlock mutex
    m_mutex.unlock();
    if (setupComplete)
    {
        return networkId;
    }
    return -1;
}
