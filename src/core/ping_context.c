#include "ft_ping.h"

/// @file ping_context.c
/// @brief Building and tearing down the one struct everything else reads.
///
/// Everything that happens exactly once, before the first packet, lives
/// here: parse, resolve, derive the sizes, allocate. The result is a
/// context the rest of the program can treat as read-only.

/// @brief Derives every size the run needs from -s (or the default).
///
/// Two numbers come out of this:
///   payload_len  what we SEND - -s if given, else PING_PAYLOAD_SIZE.
///   recvbuf_len  what we can RECEIVE - the payload plus header slack,
///                floored at PING_RECVBUF_MIN so a tiny -s never leaves us
///                unable to read a normal-sized ICMP error back.
///
/// @param ctx Context whose flags are already parsed.
static void	ctx_size_buffers(t_ping_ctx *ctx)
{
	ctx->payload_len = PING_PAYLOAD_SIZE;
	if (ctx->flags.has_packet_size)
		ctx->payload_len = (size_t)ctx->flags.packet_size;
	ctx->recvbuf_len = ctx->payload_len + PING_RECVBUF_SLACK;
	if (ctx->recvbuf_len < PING_RECVBUF_MIN)
		ctx->recvbuf_len = PING_RECVBUF_MIN;
}

/// @brief Allocates the three buffers the context owns.
///
/// All three are checked together and released together, so a partial
/// failure never leaves the caller with a half-built context.
///
///   pkt      one outgoing packet, reused for every send.
///   send_ts  one timestamp slot per possible sequence number. calloc(),
///            not malloc(): a reply for a sequence we never sent would
///            otherwise read uninitialised memory as its send time.
///   recvbuf  scratch space for recvfrom(). malloc() already returns
///            memory aligned for any type, so the _Alignas the old stack
///            buffer needed is gone along with the buffer itself.
///
/// @param ctx Context with its sizes already derived.
/// @return 0 on success, -1 on failure (message already printed).
static int	ctx_alloc_buffers(t_ping_ctx *ctx)
{
	ctx->pkt = malloc(sizeof(t_icmp_hdr) + ctx->payload_len);
	ctx->send_ts = calloc(PING_TS_RING, sizeof(struct timespec));
	ctx->recvbuf = malloc(ctx->recvbuf_len);
	if (!ctx->pkt || !ctx->send_ts || !ctx->recvbuf)
		return (perror("malloc"), -1);
	return (0);
}

/// @brief Turns argv into a ready-to-use context.
///
/// Order matters and is fixed: parse (so we know the host and -s), resolve
/// (so a bad host fails before we print or allocate anything), derive the
/// sizes, then allocate. Either the context comes back complete or nothing
/// is left allocated.
///
/// @param ctx Uninitialised struct; zeroed here before anything else.
/// @param ac  argc as received by main().
/// @param av  argv as received by main().
/// @return 0 on success, -1 on failure (message already printed).
int	ctx_init(t_ping_ctx *ctx, int ac, char **av)
{
	memset(ctx, 0, sizeof *ctx);
	if (!parse_info(ac, av, &ctx->flags))
		return (-1);
	/* htons() once, here: the id then matches raw against the id in every
	   inbound packet, with no per-packet byte swapping on either side. */
	ctx->id = htons((uint16_t)getpid());
	if (resolve_host(ctx->flags.host, &ctx->dst) == -1)
		return (-1);
	inet_ntop(AF_INET, &ctx->dst.sin_addr, ctx->ipstr, sizeof ctx->ipstr);
	ctx_size_buffers(ctx);
	if (ctx_alloc_buffers(ctx) == -1)
		return (ctx_destroy(ctx), -1);
	return (0);
}

/// @brief Frees every heap buffer the context owns.
///
/// One place for all three allocations, so every exit path in main()
/// releases them the same way. free(NULL) is a no-op, so this is safe even
/// when only some of the three were ever allocated - which is exactly the
/// case ctx_alloc_buffers() hands it on failure.
///
/// @param ctx Context to release; the struct itself is not freed.
void	ctx_destroy(t_ping_ctx *ctx)
{
	free(ctx->pkt);
	free(ctx->send_ts);
	free(ctx->recvbuf);
	ctx->pkt = NULL;
	ctx->send_ts = NULL;
	ctx->recvbuf = NULL;
}
