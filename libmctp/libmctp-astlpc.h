/* SPDX-License-Identifier: Apache-2.0 */

#ifndef _LIBMCTP_ASTLPCL_H
#define _LIBMCTP_ASTLPCL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libmctp.h"

struct mctp_binding_astlpc;

struct mctp_binding_astlpc *mctp_astlpc_init(void);
int mctp_astlpc_get_fd(struct mctp_binding_astlpc *astlpc);
void mctp_astlpc_register_bus(struct mctp_binding_astlpc *astlpc,
		struct mctp *mctp, mctp_eid_t eid);
int mctp_astlpc_poll(struct mctp_binding_astlpc *astlpc);

#ifdef __cplusplus
}
#endif

#endif /* _LIBMCTP_ASTLPCL_H */
