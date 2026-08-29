#include "ft_ping.h"
#include <time.h>   /* clock_gettime(), CLOCK_MONOTONIC */

/// @file ping_send.c
/// @brief The send path: fill the packet, checksum it, hand it to the
///        kernel, and return without waiting for anything.

/// @brief Fills the payload with the -p pattern, repeated to length.
///
/// No timestamp copy goes in: that would overwrite the user's bytes and
/// defeat the flag's purpose, which is diagnosing data-dependent link
/// problems with a known, fixed byte pattern.
///
/// @param ctx     Packet buffer and the decoded pattern.
/// @param payload Start of the payload, i.e. ctx->pkt + 8.
static void	fill_pattern(t_ping_ctx *ctx, uint8_t *payload)
{
	size_t	i;

	i = 0;
	while (i < ctx->payload_len)
	{
		payload[i] = ctx->flags.pattern[i % (size_t)ctx->flags.pattern_len];
		i++;
	}
}

/// @brief Fills the payload the default way: a timestamp, then filler.
///
/// The timestamp copy is only made when the payload is large enough to
/// hold it, which -s can make untrue. It is never read back - the receive
/// path always uses ctx->send_ts - but it costs nothing and keeps our
/// bytes on the wire identical to reference ping's for anyone diffing a
/// packet capture.
///
/// @param ctx     Payload length and the timestamp ring.
/// @param payload Start of the payload, i.e. ctx->pkt + 8.
/// @param seq     Sequence number, used to index the ring.
static void	fill_default(t_ping_ctx *ctx, uint8_t *payload, uint16_t seq)
{
	size_t	i;

	i = 0;
	if (ctx->payload_len >= sizeof(struct timespec))
	{
		memcpy(payload, &ctx->send_ts[seq % PING_TS_RING],
			sizeof(struct timespec));
		i = sizeof(struct timespec);
	}
	/* Incrementing filler for the remainder: a recognisable, non-constant
	   pattern, so a corrupted byte is obvious in a capture. */
	while (i < ctx->payload_len)
	{
		payload[i] = (uint8_t)i;
		i++;
	}
}

/// @brief Records the send timestamp, then fills the payload.
///
/// The ring write happens first and unconditionally: it is the
/// authoritative timestamp, the one the receive path reads, whatever the
/// payload ends up containing.
///
/// @param ctx Context whose pkt buffer and timestamp ring are written.
/// @param seq Sequence number of the probe being built.
static void	fill_payload(t_ping_ctx *ctx, uint16_t seq)
{
	uint8_t	*payload;

	clock_gettime(CLOCK_MONOTONIC, &ctx->send_ts[seq % PING_TS_RING]);
	payload = ctx->pkt + sizeof(t_icmp_hdr);
	if (ctx->flags.has_pattern)
	{
		fill_pattern(ctx, payload);
		return ;
	}
	fill_default(ctx, payload, seq);
}

/// @brief Builds and transmits exactly one echo request, then returns.
///
/// It does NOT wait for the reply - that is the whole point of the split:
/// the caller decides when the next send is due, and the reply is
/// collected independently whenever it happens to arrive.
///
/// @param sockfd Raw ICMP socket from socket_open().
/// @param ctx    Packet buffer, destination and timestamp ring.
/// @param seq    Sequence number for this probe, host byte order.
/// @param stats  n_sent is incremented on success.
/// @return 0 on success, -1 on a real sendto() failure.
int	send_one(int sockfd, t_ping_ctx *ctx, uint16_t seq, t_ping_stats *stats)
{
	t_icmp_hdr	*hdr;
	size_t		pktlen;

	/* Step 1: header fields. id is already in network order (set once in
	   ctx_init), sequence is converted here. */
	pktlen = sizeof(t_icmp_hdr) + ctx->payload_len;
	hdr = (t_icmp_hdr *)ctx->pkt;
	hdr->type = 8;			/* 8 = ICMP echo request */
	hdr->code = 0;
	hdr->id = ctx->id;
	hdr->sequence = htons(seq);
	/* Step 2: payload, and the authoritative send timestamp with it. */
	fill_payload(ctx, seq);
	/* Step 3: checksum, over header AND payload, so it must come last -
	   and the field MUST read zero while it is being computed. */
	hdr->checksum = 0;
	hdr->checksum = ft_checksum(ctx->pkt, pktlen);
	/* Step 4: hand it to the kernel. */
	if (sendto(sockfd, ctx->pkt, pktlen, 0,
			(struct sockaddr *)&ctx->dst, sizeof ctx->dst) == -1)
		return (perror("Send"), -1);
	stats->n_sent++;
	if (ctx->flags.flood && !ctx->flags.quiet)
		print_flood_send();
	return (0);
}
