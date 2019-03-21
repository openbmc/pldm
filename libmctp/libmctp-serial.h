/* SPDX-License-Identifier: Apache-2.0 */

#ifndef _LIBMCTP_SERIAL_H
#define _LIBMCTP_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libmctp.h"

struct mctp_binding_serial;

struct mctp_binding_serial *mctp_serial_init(void);
int mctp_serial_get_fd(struct mctp_binding_serial *serial);
void mctp_serial_register_bus(struct mctp_binding_serial *serial,
		struct mctp *mctp, mctp_eid_t eid);
int mctp_serial_read(struct mctp_binding_serial *serial);
int mctp_serial_open_path(struct mctp_binding_serial *serial,
		const char *path);
void mctp_serial_open_fd(struct mctp_binding_serial *serial, int fd);

#ifdef __cplusplus
}
#endif

#endif /* _LIBMCTP_SERIAL_H */
