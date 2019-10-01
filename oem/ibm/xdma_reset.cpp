#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

struct AspeedXdmaOp
{
    uint64_t hostAddr; //!< the DMA address on the host side, configured by
                       //!< PCI subsystem.
    uint32_t len;      //!< the size of the transfer in bytes, it should be a
                       //!< multiple of 16 bytes
    uint32_t upstream; //!< boolean indicating the direction of the DMA
                       //!< operation, true means a transfer from BMC to host.
    uint32_t bmcAddr;
    uint32_t reserved;
};

constexpr auto xdmaDev = "/dev/aspeed-xdma";

int main()
{
    AspeedXdmaOp xdmaOp{};
    xdmaOp.upstream = 2; // 2 = RESET
    int fd = -1;
    fd = open(xdmaDev, O_RDWR);
    if (fd < 0)
    {
        return -1;
    }
    write(fd, &xdmaOp, sizeof(xdmaOp));
    return 0;
}
