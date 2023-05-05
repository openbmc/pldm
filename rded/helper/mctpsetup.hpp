#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

std::mutex m_mutex;
std::map<std::string, int> network_id_map;
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
 * @param[in] mctp_link_string - command line string returned after executing
 * "mctp link show"
 * @param[out] name - Name on which MCTP is to be set. e.g. "mctpserial0"
 * @param[out] count - this is the index that is appended to the name, i.e "0"
 * in "mctpserial0"
 *
 */
int extract_new_name(std::string mctp_link_string, std::string* name,
                     std::string* count)
{
    size_t pos = 0;
    std::string token;
    std::string delimiter = "\n";
    std::vector<std::string> mctp_ports;
    while ((pos = mctp_link_string.find(delimiter)) != std::string::npos)
    {
        token = mctp_link_string.substr(0, pos);
        mctp_ports.push_back(token);
        mctp_link_string.erase(0, pos + delimiter.length());
    }
    if (!mctp_link_string.empty())
    {
        mctp_ports.push_back(mctp_link_string);
    }
    std::string last_entry = mctp_ports.back();
    if ((pos = last_entry.find("mctpserial")) != std::string::npos)
    {
        *name = last_entry.substr(pos, 11);
        *count = last_entry.substr(pos + 10, 1);
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
int create_net_id_from_udev_id(std::string udevid)
{
    // Creates a net id for a udev id by adding all the digits in the path
    // 1_1_4_1 = 7
    std::string delimiter = "_";
    int net_id = 0;
    size_t pos = 0;
    std::string token;
    while ((pos = udevid.find(delimiter)) != std::string::npos)
    {
        token = udevid.substr(0, pos);
        net_id = net_id + stoi(token);
        udevid.erase(0, pos + delimiter.length());
    }
    if (!udevid.empty())
        net_id = net_id + stoi(udevid);
    return net_id;
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
int setup_one_port(std::string port, std::string udevid)
{
    // TODO(b/284167548): Find an efficient way to set up MCTP without using CLI
    // commands This might help us get rid of sleeps

    // Wait for some time until the previous MCTP is set up if any
    sleep(2);

    // Mutex lock so that another device setup doesn't hinder the MCTP setup
    m_mutex.lock();
    int retryCounter = 0;
    bool setup_complete = false;
    // lock mutex
    int network_id;
    while (retryCounter < 4)
    {
        retryCounter++;
        std::cerr << "Triggering MCTP setup on port: " << port
                  << " with retry counter: " << retryCounter << "\n";
        // Executing link command
        std::string link_cmd = "mctp link serial " + port + " &";
        std::cerr << "Executing: " << link_cmd << "\n";
        int sys_rc = system(link_cmd.c_str());
        if (sys_rc)
        {
            std::cerr << "Error in setting up mctp link with return code: "
                      << std::to_string(sys_rc) << "\n";
            return -1;
        }
        // MCTP link command takes some time to finish - hence a sleep of 5s
        sleep(5);
        // Extract name:
        std::string mctp_name;
        std::string count;
        std::string mctp_link_shw = "mctp link show";
        std::string link_result = exec(mctp_link_shw.c_str());
        std::cerr << "link_result: " << link_result << "\n";
        sys_rc = extract_new_name(link_result, &mctp_name, &count);
        if (sys_rc)
        {
            std::cerr
                << "No mctp serial connection found. MCTP Link failed...\n";
            continue;
        }

        network_id = create_net_id_from_udev_id(udevid);

        std::string up_cmd = "mctp link set " + mctp_name + " network " +
                             std::to_string(network_id) + " up";
        std::cerr << "Executing upcmd: " << up_cmd << "\n";
        std::string network_set_response = exec(up_cmd.c_str());
        if (network_set_response.find("invalid") != std::string::npos)
        {
            std::cerr << "Network setup failure... retrying\n";
            continue;
        }
        // Setting network id for a port takes some time to setup
        sleep(5);
        // Setting up local addresses
        std::string local_addr_cmd = "mctp addr add 8 dev " + mctp_name;
        std::string route_cmd = "mctp route add 9 via " + mctp_name;
        std::cerr << "Executing local addr: " << local_addr_cmd << "\n";
        exec(local_addr_cmd.c_str());
        std::cerr << "Executing route_cmd: " << route_cmd << "\n";
        exec(route_cmd.c_str());
        // Setting up routes and local address takes time
        sleep(5);
        setup_complete = true;
        break;
    }
    // unlock mutex
    m_mutex.unlock();
    if (setup_complete)
    {
        return network_id;
    }
    return -1;
}
