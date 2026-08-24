#include "ft_ping.h"
#include <time.h>   /* clock_gettime(), CLOCK_MONOTONIC */

/// @file ping_receive.c
/// @brief The receive path: drain the socket, match what is ours, time it.
///
/// Two kinds of test happen in here, and mixing them up is the classic way
/// to write a receive loop that mis-parses:
///   Gate   = "do the bytes I am about to read actually exist?"
///            -> always a shortage check (<), always `continue`.
///   Filter = "is this packet mine?"
///            -> id compared RAW, since both sides are already in network
///               order; byte-swapping it a second time would match nothing.
///
/// The sequence number is NOT part of the filter. Sends no longer wait for
/// replies, so by the time a reply lands the counter has already moved on;
/// matching on it would reject every packet. The id alone identifies our
/// process's traffic. The sequence number IS still used - to look the send
/// timestamp up in ctx->send_ts, rather than reading it back out of the
/// payload, which -s or -p may have left without one.

/// @brief Decides what a negative recvfrom() return means.
///
/// Two of the three cases are normal and end the drain quietly: the socket
/// running dry (the MSG_DONTWAIT contract) and a signal landing mid-call.
///
/// @return 0 if the caller should stop draining and report success,
///         -1 on a real failure (message already printed).
static int	classify_recv_error(void)
{
	if (errno == EAGAIN || errno == EWOULDBLOCK)
		return (0);		/* socket drained - the normal way out */
	if (errno == EINTR)
		return (0);		/* SIGINT landed here; the loop will see the flag */
	return (perror("recvfrom"), -1);
}

/// @brief Bounds-checks the three headers a datagram must have and points
///        @p ip and @p reply at them.
///
/// A raw ICMP socket hands us the IP header too, and its length is
/// variable (options), so the ICMP header's offset has to be computed from
/// ip_hl rather than assumed.
///
/// @param ctx   Holds the receive buffer just written by recvfrom().
/// @param n     Bytes recvfrom() returned.
/// @param ip    Receives a pointer to the IP header.
/// @param reply Receives a pointer to the ICMP header.
/// @return The IP header length in bytes, or -1 if the packet is too short
///         or its ip_hl is nonsense (caller should skip the packet).
static int	locate_icmp(const t_ping_ctx *ctx, ssize_t n, struct ip **ip,
		t_icmp_hdr **reply)
{
	int	ihl;

	if (n < (ssize_t)sizeof(struct ip))				/* gate 1: IP hdr present */
		return (-1);
	*ip = (struct ip *)ctx->recvbuf;
	ihl = (*ip)->ip_hl * 4;
	if (ihl < (int)sizeof(struct ip))				/* gate 2: ip_hl is sane */
		return (-1);
	if (n < ihl + (int)sizeof(t_icmp_hdr))			/* gate 3: ICMP hdr present */
		return (-1);
	*reply = (t_icmp_hdr *)(ctx->recvbuf + ihl);
	return (ihl);
}

/// @brief Times a matched reply and folds it into the running statistics.
///
/// min/max are seeded from the first reply rather than from sentinel
/// values, which is why both comparisons carry the n_recv == 1 test.
///
/// @param ctx   Holds the send-timestamp ring.
/// @param stats Counters and accumulators, updated in place.
/// @param seq   Sequence number of the reply, host byte order.
/// @return The round-trip time in milliseconds.
static double	record_rtt(const t_ping_ctx *ctx, t_ping_stats *stats,
		uint16_t seq)
{
	struct timespec	now_ts;
	double			rtt;

	clock_gettime(CLOCK_MONOTONIC, &now_ts);
	rtt = elapsed_ms(&ctx->send_ts[seq % PING_TS_RING], &now_ts);
	stats->n_recv++;
	stats->sum += rtt;
	stats->sum_sq += rtt * rtt;
	if (stats->n_recv == 1 || rtt < stats->rtt_min)
		stats->rtt_min = rtt;
	if (stats->n_recv == 1 || rtt > stats->rtt_max)
		stats->rtt_max = rtt;
	return (rtt);
}

/// @brief Prints one reply, honouring the display flags.
///
/// The byte count reported is what actually arrived minus the IP header we
/// skipped past - never ip_len, whose byte order and meaning on a raw
/// socket differ between platforms.
///
/// @param ctx        Flags, for -q and -f.
/// @param ip         The reply's IP header, for the source address and TTL.
/// @param icmp_bytes Bytes of ICMP received, i.e. n - ihl.
/// @param seq        Sequence number, host byte order.
/// @param rtt        Round-trip time in ms.
static void	report_reply(const t_ping_ctx *ctx, struct ip *ip,
		ssize_t icmp_bytes, uint16_t seq, double rtt)
{
	char	src[INET_ADDRSTRLEN];

	if (ctx->flags.quiet)					/* -q: summary lines only */
		return ;
	if (ctx->flags.flood)					/* -f: erase one sent dot */
	{
		print_flood_recv();
		return ;
	}
	inet_ntop(AF_INET, &ip->ip_src, src, sizeof src);
	print_reply(icmp_bytes, src, seq, ip->ip_ttl, rtt);
}

/// @brief Drains every ICMP packet currently queued on the socket,
///        recording the ones that are replies to us.
///
/// Called only after poll() has reported the socket readable, and it must
/// never block: MSG_DONTWAIT turns "nothing left to read" into an
/// EAGAIN/EWOULDBLOCK return instead of a sleep. Without it, the second
/// iteration would block on a packet that may never come.
///
/// @param sockfd Raw ICMP socket.
/// @param ctx    Receive buffer, our id, timestamp ring and flags.
/// @param stats  Counters and RTT accumulators, updated in place.
/// @return 0 when the socket is drained, -1 on a real recvfrom() failure.
int	ping_receive(int sockfd, const t_ping_ctx *ctx, t_ping_stats *stats)
{
	struct ip	*ip;
	t_icmp_hdr	*reply;
	ssize_t		n;
	int			ihl;
	uint16_t	seq;
	double		rtt;

	while (1)
	{
		n = recvfrom(sockfd, ctx->recvbuf, ctx->recvbuf_len, MSG_DONTWAIT,
				NULL, NULL);
		if (n < 0)
			return (classify_recv_error());
		ihl = locate_icmp(ctx, n, &ip, &reply);
		if (ihl < 0)
			continue ;						/* truncated or malformed - skip */
		if (reply->type != 0 || reply->code != 0)	/* 0/0 = ICMP echo reply */
		{
			/* An error is not a reply: it is reported but never counted,
			   so it shows up as loss in the summary - same as a packet
			   that never came back at all. */
			handle_icmp_error(ctx, ip, ihl, reply, n);
			continue ;
		}
		if (reply->id != ctx->id)			/* filter: somebody else's probe */
			continue ;
		seq = ntohs(reply->sequence);
		rtt = record_rtt(ctx, stats, seq);
		report_reply(ctx, ip, n - ihl, seq, rtt);
	}
}
