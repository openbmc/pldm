#include "mctp.h"
#include "base.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

const uint8_t MCTP_MSG_TYPE_PLDM = 1;

int pldm_mctp_init()
{
	int fd = -1;
	int rc = -1;

	fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (-1 == fd) {
		return fd;
	}

	const char path[] = "\0mctp-mux";
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, path, sizeof(path) - 1);
	rc = connect(fd, (struct sockaddr *)&addr,
		     sizeof(path) + sizeof(addr.sun_family) - 1);
	if (-1 == rc) {
		return rc;
	}
	rc = write(fd, &MCTP_MSG_TYPE_PLDM, sizeof(MCTP_MSG_TYPE_PLDM));
	if (-1 == rc) {
		return rc;
	}

	return fd;
}

int mctp_recv(int mctp_fd, uint8_t **pldm_resp_msg, size_t *resp_msg_len)
{
	ssize_t length = recv(mctp_fd, NULL, 0, MSG_PEEK | MSG_TRUNC);
	if (0 == length) {
		return -1;
	} else if (length <= -1) {
		return -1;
	} else {
		*pldm_resp_msg = malloc(length);
		ssize_t bytes = recv(mctp_fd, *pldm_resp_msg, length, 0);
		if (length != bytes) {
			free(*pldm_resp_msg);
			return -1;
		}
		*resp_msg_len = bytes;
		return 0;
	}
}

int pldm_send_recv(mctp_eid_t eid, int mctp_fd, uint8_t *pldm_req_msg,
		   size_t req_msg_len, uint8_t **pldm_resp_msg,
		   size_t *resp_msg_len)
{
	int rc = -1;

	struct pldm_msg_hdr *hdr = (struct pldm_msg_hdr *)pldm_req_msg;
	if ((hdr->request != PLDM_REQUEST) &&
	    (hdr->request != PLDM_ASYNC_REQUEST_NOTIFY)) {
		return -1;
	}
	uint8_t instance_id = hdr->instance_id;

	rc = pldm_send(eid, mctp_fd, pldm_req_msg, req_msg_len);
	if (-1 == rc) {
		return rc;
	}

	while (1) {
		rc = mctp_recv(mctp_fd, pldm_resp_msg, resp_msg_len);
		if (0 != rc) {
			return rc;
		}
		if (*resp_msg_len < sizeof(eid) + sizeof(MCTP_MSG_TYPE_PLDM) +
					sizeof(struct pldm_msg_hdr)) {
			free(*pldm_resp_msg);
			return -1;
		}
		if (((*pldm_resp_msg)[0] != eid) ||
		    ((*pldm_resp_msg)[1] != MCTP_MSG_TYPE_PLDM)) {
			free(*pldm_resp_msg);
			continue;
		}
		uint8_t *pldm_resp = (uint8_t *)(*pldm_resp_msg) + sizeof(eid) +
				     sizeof(MCTP_MSG_TYPE_PLDM);
		hdr = (struct pldm_msg_hdr *)pldm_resp;
		if (hdr->request != PLDM_RESPONSE) {
			free(*pldm_resp_msg);
			continue;
		}
		if (hdr->instance_id != instance_id) {
			free(*pldm_resp_msg);
			continue;
		}
		break;
	}

	return rc;
}

int pldm_send(mctp_eid_t eid, int mctp_fd, uint8_t *pldm_msg, size_t msg_len)
{
	uint8_t hdr[2] = {eid, MCTP_MSG_TYPE_PLDM};

	struct iovec iov[2];
	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof(hdr);
	iov[1].iov_base = pldm_msg;
	iov[1].iov_len = msg_len;

	struct msghdr msg = {0};
	msg.msg_iov = iov;
	msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

	return sendmsg(mctp_fd, &msg, 0);
}
