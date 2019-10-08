#ifndef MCTP_H
#define MCTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef uint8_t mctp_eid_t;

int pldm_mctp_init();
int pldm_send_recv(mctp_eid_t eid, int mctp_fd, uint8_t *pldm_req_msg,
		   size_t req_msg_len, uint8_t **pldm_resp_msg,
		   size_t *resp_msg_len);
int pldm_send(mctp_eid_t eid, int mctp_fd, uint8_t *pldm_msg, size_t msg_len);

#ifdef __cplusplus
}
#endif

#endif /* MCTP_H */
