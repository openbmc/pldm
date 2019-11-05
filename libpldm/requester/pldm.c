#include "pldm.h"
#include "base.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

const uint8_t MCTP_MSG_TYPE_PLDM = 1;

int pldm_open()
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

static int mctp_recv(mctp_eid_t eid, int mctp_fd, uint8_t **pldm_resp_msg,
		     size_t *resp_msg_len)
{
	ssize_t min_len = sizeof(eid) + sizeof(MCTP_MSG_TYPE_PLDM) +
			  sizeof(struct pldm_msg_hdr);
	ssize_t length = recv(mctp_fd, NULL, 0, MSG_PEEK | MSG_TRUNC);
	if (length <= 0) {
		return -1;
	} else if (length < min_len) {
		/* read and discard */
		uint8_t buf[length];
		recv(mctp_fd, buf, length, 0);
		return -1;
	} else {
		struct iovec iov[2];
		size_t mctp_prefix_len =
		    sizeof(eid) + sizeof(MCTP_MSG_TYPE_PLDM);
		uint8_t mctp_prefix[mctp_prefix_len];
		size_t pldm_len = length - mctp_prefix_len;
		iov[0].iov_len = mctp_prefix_len;
		iov[0].iov_base = mctp_prefix;
		*pldm_resp_msg = malloc(pldm_len);
		iov[1].iov_len = pldm_len;
		iov[1].iov_base = *pldm_resp_msg;
		struct msghdr msg = {0};
		msg.msg_iov = iov;
		msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);
		ssize_t bytes = recvmsg(mctp_fd, &msg, 0);
		if (length != bytes) {
			free(*pldm_resp_msg);
			return -1;
		}
		if ((mctp_prefix[0] != eid) ||
		    (mctp_prefix[1] != MCTP_MSG_TYPE_PLDM)) {
			free(*pldm_resp_msg);
			return -1;
		}
		*resp_msg_len = pldm_len;
		return 0;
	}
}

int pldm_recv_any(mctp_eid_t eid, int mctp_fd, uint8_t **pldm_resp_msg,
		  size_t *resp_msg_len)
{
	int rc = mctp_recv(eid, mctp_fd, pldm_resp_msg, resp_msg_len);
	if (0 != rc) {
		return rc;
	}

	struct pldm_msg_hdr *hdr = (struct pldm_msg_hdr *)(*pldm_resp_msg);
	if (hdr->request != PLDM_RESPONSE) {
		free(*pldm_resp_msg);
		return -1;
	}

	uint8_t pldm_rc = 0;
	if (*resp_msg_len < (sizeof(struct pldm_msg_hdr) + sizeof(pldm_rc))) {
		free(*pldm_resp_msg);
		return -1;
	}

	return 0;
}

int pldm_recv(mctp_eid_t eid, int mctp_fd, uint8_t instance_id,
	      uint8_t **pldm_resp_msg, size_t *resp_msg_len)
{
	int rc = pldm_recv_any(eid, mctp_fd, pldm_resp_msg, resp_msg_len);
	if (0 != rc) {
		return rc;
	}

	struct pldm_msg_hdr *hdr = (struct pldm_msg_hdr *)(*pldm_resp_msg);
	if (hdr->instance_id != instance_id) {
		free(*pldm_resp_msg);
		return -1;
	}

	return 0;
}

int pldm_send_recv(mctp_eid_t eid, int mctp_fd, const uint8_t *pldm_req_msg,
		   size_t req_msg_len, uint8_t **pldm_resp_msg,
		   size_t *resp_msg_len)
{
	int rc = -1;

	struct pldm_msg_hdr *hdr = (struct pldm_msg_hdr *)pldm_req_msg;
	if ((hdr->request != PLDM_REQUEST) &&
	    (hdr->request != PLDM_ASYNC_REQUEST_NOTIFY)) {
		return -1;
	}

	rc = pldm_send(eid, mctp_fd, pldm_req_msg, req_msg_len);
	if (-1 == rc) {
		return rc;
	}

	uint8_t instance_id = hdr->instance_id;
	while (1) {
		rc = pldm_recv(eid, mctp_fd, instance_id, pldm_resp_msg,
			       resp_msg_len);
		if (!rc) {
			break;
		}
	}

	return rc;
}

int pldm_send(mctp_eid_t eid, int mctp_fd, const uint8_t *pldm_req_msg,
	      size_t req_msg_len)
{
	uint8_t hdr[2] = {eid, MCTP_MSG_TYPE_PLDM};

	struct iovec iov[2];
	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof(hdr);
	iov[1].iov_base = (uint8_t *)pldm_req_msg;
	iov[1].iov_len = req_msg_len;

	struct msghdr msg = {0};
	msg.msg_iov = iov;
	msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

	return sendmsg(mctp_fd, &msg, 0);
}
