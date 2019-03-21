/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/aspeed-lpc-ctrl.h>

#define pr_fmt(x) "astlpc: " x

#include "libmctp.h"
#include "libmctp-alloc.h"
#include "libmctp-log.h"
#include "libmctp-astlpc.h"

struct mctp_binding_astlpc {
	struct mctp_binding	binding;
	void			*lpc_map_base;
	union {
		void			*lpc_map;
		struct mctp_lpcmap_hdr	*lpc_hdr;
	};
	int			kcs_fd;
	uint8_t			kcs_status;

	bool			running;

	/* temporary transmit buffer */
	uint8_t			txbuf[256];
};

#ifndef container_of
#define container_of(ptr, type, member) \
    (type *)((char *)(ptr) - (char *)&((type *)0)->member)
#endif

#define binding_to_astlpc(b) \
	container_of(b, struct mctp_binding_astlpc, binding)

#define MCTP_MAGIC	0x4d435450
#define BMC_VER_MIN	1
#define BMC_VER_CUR	1

struct mctp_lpcmap_hdr {
	uint32_t	magic;

	uint16_t	bmc_ver_min;
	uint16_t	bmc_ver_cur;
	uint16_t	host_ver_min;
	uint16_t	host_ver_cur;
	uint16_t	negotiated_ver;
	uint16_t	pad0;

	uint32_t	rx_offset;
	uint32_t	rx_size;
	uint32_t	tx_offset;
	uint32_t	tx_size;
} __attribute__((packed));

/* layout of TX/RX areas */
static const uint32_t	rx_offset = 0x100;
static const uint32_t	rx_size   = 0x100;
static const uint32_t	tx_offset = 0x200;
static const uint32_t	tx_size   = 0x100;

/* kernel interface */
static const char *kcs_path = "/dev/mctp0";
static const char *lpc_path = "/dev/aspeed-lpc-ctrl";

#define LPC_WIN_SIZE                (1 * 1024 * 1024)

enum {
	KCS_REG_DATA = 0,
	KCS_REG_STATUS = 1,
};

#define KCS_STATUS_BMC_READY		0x80
#define KCS_STATUS_CHANNEL_ACTIVE	0x40
#define KCS_STATUS_IBF			0x02
#define KCS_STATUS_OBF			0x01

static int mctp_astlpc_kcs_set_status(struct mctp_binding_astlpc *astlpc,
		uint8_t status)
{
	int rc;

	rc = pwrite(astlpc->kcs_fd, &status, 1, KCS_REG_STATUS);
	if (rc != 1) {
		warn("KCS status write failed");
		return -1;
	}
	return 0;
}

static int mctp_astlpc_kcs_send(struct mctp_binding_astlpc *astlpc,
		uint8_t data)
{
	uint8_t status;
	int rc;

	for (;;) {
		rc = pread(astlpc->kcs_fd, &status, 1, KCS_REG_STATUS);
		if (rc != 1) {
			warn("KCE status read failed");
			return -1;
		}
		if (!(status & KCS_STATUS_OBF))
			break;
		/* todo: timeout */
	}

	rc = pwrite(astlpc->kcs_fd, &data, 1, KCS_REG_DATA);
	if (rc != 1) {
		warn("KCS data write failed");
		return -1;
	}

	return 0;
}

static int mctp_binding_astlpc_tx(struct mctp_binding *b,
		struct mctp_pktbuf *pkt)
{
	struct mctp_binding_astlpc *astlpc = binding_to_astlpc(b);
	uint32_t len;

	len = mctp_pktbuf_size(pkt);
	if (len > rx_size - 4) {
		warnx("invalid TX len 0x%x", len);
		return -1;
	}

	*(uint32_t *)(astlpc->lpc_map + rx_offset) = htobe32(len);

	memcpy(astlpc->lpc_map + rx_offset + 4, mctp_pktbuf_hdr(pkt), len);

	mctp_binding_set_tx_enabled(b, false);

	mctp_astlpc_kcs_send(astlpc, 0x1);
	return 0;
}

static void mctp_astlpc_init_channel(struct mctp_binding_astlpc *astlpc)
{
	/* todo: actual version negotiation */
	astlpc->lpc_hdr->negotiated_ver = htobe16(1);
	mctp_astlpc_kcs_set_status(astlpc,
			KCS_STATUS_BMC_READY | KCS_STATUS_CHANNEL_ACTIVE |
			KCS_STATUS_OBF);

	mctp_binding_set_tx_enabled(&astlpc->binding, true);
}

static void mctp_astlpc_rx_start(struct mctp_binding_astlpc *astlpc)
{
	struct mctp_pktbuf *pkt;
	uint32_t len;

	len = htobe32(*(uint32_t *)(astlpc->lpc_map + tx_offset));
	if (len > tx_size - 4) {
		warn("invalid RX len 0x%x", len);
		return;
	}

	if (len > MCTP_MTU + sizeof(struct mctp_hdr)) {
		warn("invalid RX len 0x%x", len);
		return;
	}

	pkt = mctp_pktbuf_alloc(len);
	if (!pkt)
		goto out_complete;

	memcpy(mctp_pktbuf_hdr(pkt), astlpc->lpc_map + tx_offset + 4, len);

	{
		struct mctp_hdr *hdr = mctp_pktbuf_hdr(pkt);
		fprintf(stderr, "RX packet: len %d src %d dest %d flags 0x%02x",
				len, hdr->src, hdr->dest, hdr->flags_seq_tag);
	}

	mctp_bus_rx(&astlpc->binding, pkt);

out_complete:
	mctp_astlpc_kcs_send(astlpc, 0x2);
}

static void mctp_astlpc_tx_complete(struct mctp_binding_astlpc *astlpc)
{
	mctp_binding_set_tx_enabled(&astlpc->binding, true);
}

int mctp_astlpc_poll(struct mctp_binding_astlpc *astlpc)
{
	uint8_t kcs_regs[2], data;
	int rc;

	rc = pread(astlpc->kcs_fd, kcs_regs, 2, 0);
	if (rc < 0) {
		warn("KCS read error");
		return -1;
	} else if (rc != 2) {
		warn("KCS short read (%d)", rc);
		return -1;
	}

	if (!(kcs_regs[KCS_REG_STATUS] & KCS_STATUS_IBF))
		return 0;

	data = kcs_regs[KCS_REG_DATA];
	switch (data) {
	case 0x0:
		mctp_astlpc_init_channel(astlpc);
		break;
	case 0x1:
		mctp_astlpc_rx_start(astlpc);
		break;
	case 0x2:
		mctp_astlpc_tx_complete(astlpc);
		break;
	default:
		warn("unknown message 0x%x", data);
	}
	return 0;
}

int mctp_astlpc_get_fd(struct mctp_binding_astlpc *astlpc)
{
	return astlpc->kcs_fd;
}


void mctp_astlpc_register_bus(struct mctp_binding_astlpc *astlpc,
		struct mctp *mctp, mctp_eid_t eid)
{
	mctp_register_bus(mctp, &astlpc->binding, eid);
}

static int mctp_astlpc_init_bmc(struct mctp_binding_astlpc *astlpc)
{
	uint8_t status;
	int rc;

	astlpc->lpc_hdr->magic = htobe32(MCTP_MAGIC);
	astlpc->lpc_hdr->bmc_ver_min = htobe16(BMC_VER_MIN);
	astlpc->lpc_hdr->bmc_ver_cur = htobe16(BMC_VER_CUR);

	astlpc->lpc_hdr->rx_offset = htobe32(rx_offset);
	astlpc->lpc_hdr->rx_size = htobe32(rx_size);
	astlpc->lpc_hdr->tx_offset = htobe32(tx_offset);
	astlpc->lpc_hdr->tx_size = htobe32(tx_size);

	/* set status indicating that the BMC is now active */
	status = 0x80;
	rc = pwrite(astlpc->kcs_fd, &status, 1, KCS_REG_STATUS);
	if (rc != 1) {
		warn("KCS write failed");
		rc = -1;
	} else {
		rc = 0;
	}

	return rc;
}

static int mctp_astlpc_init_lpc(struct mctp_binding_astlpc *astlpc)
{
	struct aspeed_lpc_ctrl_mapping map = {
		.window_type = ASPEED_LPC_CTRL_WINDOW_MEMORY,
		.window_id = 0, /* There's only one */
		.flags = 0,
		.addr = 0,
		.offset = 0,
		.size = 0
	};
	int fd, rc;

	fd = open(lpc_path, O_RDWR | O_SYNC);
	if (fd < 0) {
		warn("LPC open (%s) failed", lpc_path);
		return -1;
	}

	rc = ioctl(fd, ASPEED_LPC_CTRL_IOCTL_GET_SIZE, &map);
	if (rc) {
		warn("LPC GET_SIZE failed");
		close(fd);
		return -1;
	}

	astlpc->lpc_map_base = mmap(NULL, map.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (astlpc->lpc_map_base == MAP_FAILED) {
		warn("LPC mmap failed");
		rc = -1;
	} else {
		astlpc->lpc_map = astlpc->lpc_map_base +
			map.size - LPC_WIN_SIZE;
	}

	close(fd);

	return rc;
}

static int mctp_astlpc_init_kcs(struct mctp_binding_astlpc *astlpc)
{
	astlpc->kcs_fd = open(kcs_path, O_RDWR);
	if (astlpc->kcs_fd < 0)
		return -1;

	return 0;
}

struct mctp_binding_astlpc *mctp_astlpc_init(void)
{
	struct mctp_binding_astlpc *astlpc;
	int rc;

	astlpc = __mctp_alloc(sizeof(*astlpc));
	memset(astlpc, 0, sizeof(*astlpc));
	astlpc->binding.name = "astlpc";
	astlpc->binding.version = 1;
	astlpc->binding.tx = mctp_binding_astlpc_tx;

	rc = mctp_astlpc_init_lpc(astlpc);
	if (rc) {
		free(astlpc);
		return NULL;
	}

	rc = mctp_astlpc_init_kcs(astlpc);
	if (rc) {
		free(astlpc);
		return NULL;
	}

	rc = mctp_astlpc_init_bmc(astlpc);
	if (rc) {
		free(astlpc);
		return NULL;
	}

	return astlpc;
}

