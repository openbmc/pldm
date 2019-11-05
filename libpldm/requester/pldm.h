#ifndef MCTP_H
#define MCTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef uint8_t mctp_eid_t;

/**
 * @brief Connect to the MCTP socket and provide an fd to it. The fd can be
 *        used to pass as input to other APIs below, or can be polled.
 *
 * @return fd on success, -1 on error (errno may be set)
 */
int pldm_open();

/**
 * @brief Send a PLDM request message. Wait for corresponding response message,
 *        which once received, is returned to the caller.
 *
 * @param[in] eid - destination MCTP eid
 * @param[in] fd - MCTP socket fd
 * @param[in] pldm_req_msg - caller owned pointer to PLDM request msg
 * @param[in] req_msg_len - size of PLDM request msg
 * @param[out] pldm_resp_msg - *pldm_resp_msg will point to PLDM response msg,
 *             this function allocates memory, caller to free(*pldm_resp_msg) on
 *             success.
 * @param[out] resp_msg_len - caller owned pointer that will be made point to
 *             the size of the PLDM response msg.
 *
 * @return 0 on success, -1 on error (errno may be set)
 */
int pldm_send_recv(mctp_eid_t eid, int mctp_fd, const uint8_t *pldm_req_msg,
		   size_t req_msg_len, uint8_t **pldm_resp_msg,
		   size_t *resp_msg_len);

/**
 * @brief Send a PLDM request message, don't wait for response. Essentially an
 *        async API. A user of this would typically have added the MCTP fd to an
 *        event loop for polling. Once there's data available, the user would
 *        invoke pldm_recv().
 *
 * @param[in] eid - destination MCTP eid
 * @param[in] fd - MCTP socket fd
 * @param[in] pldm_req_msg - caller owned pointer to PLDM request msg
 * @param[in] req_msg_len - size of PLDM request msg
 *
 * @return 0 on success, -1 on error (errno may be set)
 */
int pldm_send(mctp_eid_t eid, int mctp_fd, const uint8_t *pldm_req_msg,
	      size_t req_msg_len);

/**
 * @brief Read MCTP socket. If there's data vailable, return success only if
 *        data is a PLDM response message that matches eid and instance_id.
 *
 * @param[in] eid - destination MCTP eid
 * @param[in] fd - MCTP socket fd
 * @param[in] instance_id - PLDM instance id of previously sent PLDM request msg
 * @param[out] pldm_resp_msg - *pldm_resp_msg will point to PLDM response msg,
 *             this function allocates memory, caller to free(*pldm_resp_msg) on
 *             success.
 * @param[out] resp_msg_len - caller owned pointer that will be made point to
 *             the size of the PLDM response msg.
 *
 * @return 0 on success, -1 on error (errno may be set). -1 is returned even
 *         when data was read, but didn't match eid or instance_id.
 */
int pldm_recv(mctp_eid_t eid, int mctp_fd, uint8_t instance_id,
	      uint8_t **pldm_resp_msg, size_t *resp_msg_len);

/**
 * @brief Read MCTP socket. If there's data vailable, return success only if
 *        data is a PLDM response message.
 *
 * @param[in] eid - destination MCTP eid
 * @param[in] fd - MCTP socket fd
 * @param[out] pldm_resp_msg - *pldm_resp_msg will point to PLDM response msg,
 *             this function allocates memory, caller to free(*pldm_resp_msg) on
 *             success.
 * @param[out] resp_msg_len - caller owned pointer that will be made point to
 *             the size of the PLDM response msg.
 *
 * @return 0 on success, -1 on error (errno may be set). -1 is returned even
 *         when data was read, but wasn't a PLDM response message
 */
int pldm_recv_any(mctp_eid_t eid, int mctp_fd, uint8_t **pldm_resp_msg,
		  size_t *resp_msg_len);

#ifdef __cplusplus
}
#endif

#endif /* MCTP_H */
