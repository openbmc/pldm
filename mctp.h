/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mctp: userspace utility for managing the kernel MCTP stack.
 *
 * Interim definitions for the MCTP kernel interface. While the MCTP kernel
 * API is going through upstream, system toolchains may not yet have the
 * required definitions for mctp tools. This header provides fallback
 * implementations (protected by #ifdef guards) of those new interfaces.
 *
 * Copyright (c) 2021 Code Construct
 * Copyright (c) 2021 Google
 */

#ifndef _MCTP_H
#define _MCTP_H

#include "config.h"

#if HAVE_LINUX_MCTP_H
#include <linux/mctp.h>
#endif
#include <linux/if_link.h>

#ifndef AF_MCTP
#define AF_MCTP 45
#endif

/* MCTP serial line discipline */
#ifndef N_MCTP
#define N_MCTP 28
#endif

#include <stdint.h>

#ifndef MCTP_NET_ANY

typedef uint8_t mctp_eid_t;

struct mctp_addr
{
    mctp_eid_t s_addr;
};

struct sockaddr_mctp
{
    unsigned short int smctp_family;
    unsigned short __smctp_pad0;
    int smctp_network;
    struct mctp_addr smctp_addr;
    uint8_t smctp_type;
    uint8_t smctp_tag;
    uint8_t __smctp_pad1;
};

#define MCTP_NET_ANY 0
#define MCTP_ADDR_ANY 0xff
#define MCTP_TAG_OWNER 0x08

#endif /* MCTP_NET_ANY */

#ifndef SIOCMCTPALLOCTAG
#define SIOCMCTPALLOCTAG SIOCPROTOPRIVATE
struct mctp_ioc_tag_ctl
{
    mctp_eid_t peer_addr;
    __u8 tag;
    __u16 flags;
};

#endif

#ifndef MAX_ADDR_LEN
#define MAX_ADDR_LEN 32
#endif

/* Added in v5.16 */
#ifndef MCTP_OPT_ADDR_EXT

#define MCTP_OPT_ADDR_EXT 1

struct sockaddr_mctp_ext
{
    struct sockaddr_mctp smctp_base;
    int smctp_ifindex;
    uint8_t smctp_halen;
    uint8_t __smctp_pad0[3];
    unsigned char smctp_haddr[MAX_ADDR_LEN];
};

#endif /* MCTP_OPT_ADDR_EXT */

#ifndef IFLA_MCTP_MAX

/* From if_link.h */
enum
{
    IFLA_MCTP_UNSPEC,
    IFLA_MCTP_NET,
    __IFLA_MCTP_MAX,
};

#define IFLA_MCTP_MAX (__IFLA_MCTP_MAX - 1)

#endif /* IFLA_MCTP_MAX */

/* setsockopt(2) options */
#ifndef SOL_MCTP
#define SOL_MCTP 285
#endif

#ifndef RTNLGRP_MCTP_IFADDR
#define RTNLGRP_MCTP_IFADDR 34
#endif

#endif /* _MCTP_H */
