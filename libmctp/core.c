/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef pr_fmt
#define pr_fmt(fmt) "core: " fmt

#include "libmctp-alloc.h"
#include "libmctp-log.h"
#include "libmctp.h"

/* Internal data structures */

struct mctp_bus {
	mctp_eid_t eid;
	struct mctp_binding *binding;

	/* todo: routing */
};

struct mctp_msg_ctx {
	uint8_t src;
	uint8_t tag;
	uint8_t last_seq;
	void *buf;
	size_t buf_size;
	size_t buf_alloc_size;
};

struct mctp {
	/* todo: multiple busses */
	struct mctp_bus busses[1];

	struct mctp_pktbuf *tx_queue_head;
	struct mctp_pktbuf *tx_queue_tail;

	/* Message RX callback */
	mctp_rx_fn message_rx;
	void *message_rx_data;

	/* Message reassembly.
	 * @todo: flexible context count
	 */
	struct mctp_msg_ctx msg_ctxs[16];
};

#ifndef BUILD_ASSERT
#define BUILD_ASSERT(x)                                                        \
	do {                                                                   \
		(void)sizeof(char[0 - (!(x))]);                                \
	} while (0)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

struct mctp_pktbuf *mctp_pktbuf_alloc(uint8_t len)
{
	struct mctp_pktbuf *buf;

	BUILD_ASSERT(MCTP_PKTBUF_SIZE <= 0xff);

	/* todo: pools */
	buf = __mctp_alloc(sizeof(*buf));

	buf->start = MCTP_PKTBUF_BINDING_PAD;
	buf->end = buf->start + len;
	buf->mctp_hdr_off = buf->start;

	return buf;
}

void mctp_pktbuf_free(struct mctp_pktbuf *pkt) { __mctp_free(pkt); }

struct mctp_hdr *mctp_pktbuf_hdr(struct mctp_pktbuf *pkt)
{
	return (void *)pkt->data + pkt->mctp_hdr_off;
}

void *mctp_pktbuf_data(struct mctp_pktbuf *pkt)
{
	return (void *)pkt->data + pkt->mctp_hdr_off + sizeof(struct mctp_hdr);
}

uint8_t mctp_pktbuf_size(struct mctp_pktbuf *pkt)
{
	return pkt->end - pkt->start;
}

void *mctp_pktbuf_alloc_start(struct mctp_pktbuf *pkt, uint8_t size)
{
	assert(size <= pkt->start);
	pkt->start -= size;
	return pkt->data + pkt->start;
}

void *mctp_pktbuf_alloc_end(struct mctp_pktbuf *pkt, uint8_t size)
{
	void *buf;

	assert(size < (MCTP_PKTBUF_SIZE - pkt->end));
	buf = pkt->data + pkt->end;
	pkt->end += size;
	return buf;
}

int mctp_pktbuf_push(struct mctp_pktbuf *pkt, void *data, uint8_t len)
{
	void *p;

	assert(pkt->end + len <= MCTP_PKTBUF_SIZE);

	if (pkt->end + len > MCTP_PKTBUF_SIZE)
		return -1;

	p = pkt->data + pkt->end;

	pkt->end += len;
	memcpy(p, data, len);

	return 0;
}

/* Message reassembly */
static struct mctp_msg_ctx *mctp_msg_ctx_lookup(struct mctp *mctp, uint8_t src,
						uint8_t tag)
{
	unsigned int i;

	/* @todo: better lookup, if we add support for more outstanding
	 * message contexts */
	for (i = 0; i < ARRAY_SIZE(mctp->msg_ctxs); i++) {
		struct mctp_msg_ctx *ctx = &mctp->msg_ctxs[i];
		if (ctx->src == src && ctx->tag == tag)
			return ctx;
	}

	return NULL;
}

static struct mctp_msg_ctx *mctp_msg_ctx_create(struct mctp *mctp, uint8_t src,
						uint8_t tag)
{
	struct mctp_msg_ctx *ctx;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mctp->msg_ctxs); i++) {
		struct mctp_msg_ctx *tmp = &mctp->msg_ctxs[i];
		if (!tmp->src) {
			ctx = tmp;
			break;
		}
	}

	if (!ctx)
		return NULL;

	ctx->src = src;
	ctx->tag = tag;

	return ctx;
}

static void mctp_msg_ctx_drop(struct mctp_msg_ctx *ctx) { ctx->src = 0; }

static void mctp_msg_ctx_reset(struct mctp_msg_ctx *ctx) { ctx->buf_size = 0; }

static int mctp_msg_ctx_add_pkt(struct mctp_msg_ctx *ctx,
				struct mctp_pktbuf *pkt)
{
	size_t len;

	len = mctp_pktbuf_size(pkt) - sizeof(struct mctp_hdr);

	if (ctx->buf_size + len > ctx->buf_alloc_size) {
		size_t new_alloc_size;

		/* @todo: finer-grained allocation, size limits */
		if (!ctx->buf_alloc_size) {
			new_alloc_size = 4096;
		} else {
			new_alloc_size = ctx->buf_alloc_size * 2;
		}
		ctx->buf = __mctp_realloc(ctx->buf, new_alloc_size);
		ctx->buf_alloc_size = new_alloc_size;
	}

	memcpy(ctx->buf + ctx->buf_size, mctp_pktbuf_data(pkt), len);
	ctx->buf_size += len;

	return 0;
}

/* Core API functions */
struct mctp *mctp_init(void)
{
	struct mctp *mctp;

	mctp = __mctp_alloc(sizeof(*mctp));
	memset(mctp, 0, sizeof(*mctp));

	return mctp;
}

int mctp_set_rx_all(struct mctp *mctp, mctp_rx_fn fn, void *data)
{
	mctp->message_rx = fn;
	mctp->message_rx_data = data;
	return 0;
}

static struct mctp_bus *find_bus_for_eid(struct mctp *mctp, mctp_eid_t dest
					 __attribute__((unused)))
{
	return &mctp->busses[0];
}

unsigned long mctp_register_bus(struct mctp *mctp, struct mctp_binding *binding,
				mctp_eid_t eid)
{
	assert(!mctp->busses[0].binding);
	mctp->busses[0].binding = binding;
	mctp->busses[0].eid = eid;
	return 0;
}

void mctp_bus_rx(struct mctp *mctp, unsigned long bus_id,
		 struct mctp_pktbuf *pkt)
{
	struct mctp_bus *bus = &mctp->busses[bus_id];
	struct mctp_msg_ctx *ctx;
	struct mctp_hdr *hdr;
	uint8_t flags, seq, tag;
	size_t len;
	void *p;
	int rc;

	hdr = mctp_pktbuf_hdr(pkt);

	if (hdr->dest != bus->eid)
		/* @todo: non-local packet routing */
		return;

	flags = hdr->flags_seq_tag & (MCTP_HDR_FLAG_SOM | MCTP_HDR_FLAG_EOM);
	tag = (hdr->flags_seq_tag >> MCTP_HDR_TAG_SHIFT) & MCTP_HDR_TAG_MASK;
	seq = (hdr->flags_seq_tag >> MCTP_HDR_SEQ_SHIFT) & MCTP_HDR_SEQ_MASK;

	switch (flags) {
	case MCTP_HDR_FLAG_SOM | MCTP_HDR_FLAG_EOM:
		/* single-packet message - send straight up to rx function,
		 * no need to create a message context */
		len = pkt->end - pkt->mctp_hdr_off - sizeof(struct mctp_hdr);
		p = pkt->data + pkt->mctp_hdr_off + sizeof(struct mctp_hdr),
		mctp->message_rx(bus->eid, mctp->message_rx_data, p, len);
		break;

	case MCTP_HDR_FLAG_SOM:
		/* start of a new message - start the new context for
		 * future message reception. If an existing context is
		 * already present, drop it. */
		ctx = mctp_msg_ctx_lookup(mctp, hdr->src, tag);
		if (ctx) {
			mctp_msg_ctx_reset(ctx);
		} else {
			ctx = mctp_msg_ctx_create(mctp, hdr->src, tag);
			if (((ctx->last_seq + 1) % 4) != seq) {
				mctp_msg_ctx_drop(ctx);
				return;
			}
		}

		rc = mctp_msg_ctx_add_pkt(ctx, pkt);
		if (rc) {
			mctp_msg_ctx_drop(ctx);
		} else {
			ctx->last_seq = seq;
		}

		break;

	case MCTP_HDR_FLAG_EOM:
		ctx = mctp_msg_ctx_lookup(mctp, hdr->src, tag);
		if (!ctx)
			return;

		if (((ctx->last_seq + 1) % 4) != seq) {
			mctp_msg_ctx_drop(ctx);
			return;
		}

		rc = mctp_msg_ctx_add_pkt(ctx, pkt);
		if (!rc) {
			mctp->message_rx(bus->eid, mctp->message_rx_data,
					 ctx->buf, ctx->buf_size);
		}

		mctp_msg_ctx_drop(ctx);
		break;
	}
}

static int mctp_packet_tx(struct mctp *mctp __attribute__((unused)),
			  struct mctp_bus *bus, struct mctp_pktbuf *pkt)
{
	return bus->binding->tx(bus->binding, pkt);
}

int mctp_message_tx(struct mctp *mctp, mctp_eid_t eid, void *msg,
		    size_t msg_len)
{
	struct mctp_pktbuf *pkt, *tmp;
	struct mctp_hdr *hdr;
	struct mctp_bus *bus;
	size_t pkt_len, p;

	bus = find_bus_for_eid(mctp, eid);

	/* queue up packets, each of max MCTP_MTU size */
	for (p = 0; p < msg_len;) {
		pkt_len = msg_len - p;
		if (pkt_len > MCTP_MTU)
			pkt_len = MCTP_MTU;

		pkt = mctp_pktbuf_alloc(pkt_len + sizeof(*hdr));
		hdr = mctp_pktbuf_hdr(pkt);

		/* todo: tags */
		hdr->ver = bus->binding->version & 0xf;
		hdr->dest = eid;
		hdr->src = bus->eid;
		hdr->flags_seq_tag = MCTP_HDR_FLAG_SOM | MCTP_HDR_FLAG_EOM |
				     (0 << MCTP_HDR_SEQ_SHIFT) |
				     MCTP_HDR_FLAG_TO |
				     (0 << MCTP_HDR_TAG_SHIFT);

		memcpy(mctp_pktbuf_data(pkt), msg + p, pkt_len);

		/* add to tx queue */
		if (mctp->tx_queue_tail)
			mctp->tx_queue_tail->next = pkt;
		else
			mctp->tx_queue_head = pkt;
		mctp->tx_queue_tail = pkt;

		p += pkt_len;
	}

	/* send queued packets */
	for (pkt = mctp->tx_queue_head; pkt;) {
		mctp_prdebug("sending pkt, len %d", mctp_pktbuf_size(pkt));
		mctp_packet_tx(mctp, bus, pkt);
		tmp = pkt->next;
		mctp_pktbuf_free(pkt);
		pkt = tmp;
	}

	mctp->tx_queue_tail = NULL;
	mctp->tx_queue_head = NULL;

	return 0;
}
