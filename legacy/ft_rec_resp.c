#include "ft_ping.h"
#include <netinet/ip.h>   /* struct ip */
#include <errno.h>        /* errno, EINTR, EAGAIN, EWOULDBLOCK */
#include <time.h>         /* clock_gettime(), CLOCK_MONOTONIC */
#include <sys/socket.h>   /* recvfrom(), MSG_DONTWAIT */

/* Types that legitimately carry a quoted copy of the datagram that
   triggered them (RFC 792 / RFC 1122). Anything else non-zero - a stray
   echo REQUEST from some other host on the segment, for instance - has
   no quoted header of ours to match against, so it's silently ignored
   rather than mis-parsed as if it did. */
static int	is_icmp_error_type(uint8_t type)
{
	return (type == 3 || type == 4 || type == 5
		|| type == 11 || type == 12);
}

/* type/code -> human description, reference ping's wording where it
   matters (Group 4's two named examples: "Time to live exceeded" and
   "Destination Host Unreachable"). Not RFC-exhaustive - covers the
   codes you'll actually see pinging real hosts. */
static const char	*icmp_error_desc(uint8_t type, uint8_t code)
{
	if (type == 3)
	{
		if (code == 0)
			return ("Destination Network Unreachable");
		if (code == 1)
			return ("Destination Host Unreachable");
		if (code == 2)
			return ("Destination Protocol Unreachable");
		if (code == 3)
			return ("Destination Port Unreachable");
		if (code == 4)
			return ("Frag needed and DF set");
		if (code == 13)
			return ("Communication Administratively Prohibited");
		return ("Destination Unreachable");
	}
	if (type == 4)
		return ("Source Quench");
	if (type == 5)
		return ("Redirect");
	if (type == 11)
	{
		if (code == 0)
			return ("Time to live exceeded");
		return ("Frag reassembly time exceeded");
	}
	return ("Parameter problem");   /* only type 12 reaches here */
}

/* Reports an ICMP error caused by one of our own probes. This is NOT a
   reply: n_recv and the RTT accumulators are left untouched, so it
   counts as loss in the final summary - same as a packet that never
   came back at all.

   Layout of what ft_rec_resp() just read into ctx->recvbuf:
     IP header (ihl bytes)          <- the error packet's own IP header
     ICMP header (8 bytes, `reply`) <- type/code identify the error
     quoted IP header               <- the ORIGINAL packet we sent, echoed
     quoted ICMP header (8 bytes)   <- our original type/id/sequence
   The quoted id is how we confirm this error was actually caused by US
   and not some other process's probe sharing the network. */
static void	handle_icmp_error(const t_ping_ctx *ctx, struct ip *ip,
	int ihl, t_icmp_hdr *reply, ssize_t n)
{
	struct ip	*inner_ip;
	t_icmp_hdr	*inner_icmp;
	int			inner_ihl;
	int			quote_off;
	char		src[INET_ADDRSTRLEN];

	if (!is_icmp_error_type(reply->type))
		return ;                                     /* not a type we quote-parse */
	quote_off = ihl + (int)sizeof(t_icmp_hdr);
	if (n < quote_off + (int)sizeof(struct ip))       /* quoted IP hdr, min size */
		return ;                                      /* truncated - skip it */
	inner_ip = (struct ip *)((uint8_t *)reply + sizeof(t_icmp_hdr));
	inner_ihl = inner_ip->ip_hl * 4;
	if (inner_ihl < (int)sizeof(struct ip))
		return ;
	if (n < quote_off + inner_ihl + (int)sizeof(t_icmp_hdr))
		return ;                                      /* quoted ICMP hdr truncated */
	inner_icmp = (t_icmp_hdr *)((uint8_t *)inner_ip + inner_ihl);
	if (inner_icmp->id != ctx->id && !ctx->flags.verbose)
		return ;                     /* somebody else's probe - -v only */
	if (ctx->flags.quiet || ctx->flags.quiet_errors)
		return ;                     /* -q or -Q suppresses this line */
	inet_ntop(AF_INET, &ip->ip_src, src, sizeof src);
	printf("From %s icmp_seq=%u %s\n", src, ntohs(inner_icmp->sequence),
		icmp_error_desc(reply->type, reply->code));
}

/* Drains every ICMP packet currently queued on the socket, recording the
   ones that are replies to us.

   Called only after poll() has reported the socket readable, and it must
   never block: MSG_DONTWAIT turns "nothing left to read" into an
   EAGAIN/EWOULDBLOCK return instead of a sleep. Without it, the second
   iteration of this loop would block on a packet that may never come -
   which is exactly the hang this restructure exists to remove.

   Reads into ctx->recvbuf, a heap buffer sized in main() from the -s
   payload length (never smaller than the old fixed 1024). malloc()
   already returns memory aligned for any type, so the _Alignas the old
   stack buffer needed is gone along with the buffer itself.

   Gates  = "do the bytes I am about to read exist?" -> always a shortage (<).
   Filter = "is this packet mine?" -> id compared RAW (already in network
            order), never byte-swapped a second time.

   The sequence number is NOT part of the filter any more. Sends no longer
   wait for replies, so by the time a reply lands `seq` has already moved
   on; matching on it would reject every packet. id alone identifies our
   process's traffic. The sequence number IS still used, though - to look
   the send timestamp up in ctx->send_ts instead of reading it back out of
   the payload, since the payload is no longer guaranteed to carry one.

   Returns 0 (socket drained, however many replies that was) or -1 on a
   real recvfrom() failure. */
int	ft_rec_resp(int sockfd, const t_ping_ctx *ctx, t_ping_stats *stats)
{
	char			src[INET_ADDRSTRLEN];
	struct ip		*ip;
	t_icmp_hdr		*reply;
	ssize_t			n;
	int				ihl;
	uint16_t		seq;
	struct timespec	now_ts;
	double			rtt;

	while (1)
	{
		n = recvfrom(sockfd, ctx->recvbuf, ctx->recvbuf_len, MSG_DONTWAIT,
				NULL, NULL);
		if (n < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return (0);         /* socket drained - normal exit */
			if (errno == EINTR)     /* SIGINT landed here */
				return (0);
			return (perror("recvfrom"), -1);
		}
		if (n < (ssize_t)sizeof(struct ip))        /* gate 1: IP header present */
			continue;
		ip = (struct ip *)ctx->recvbuf;
		ihl = ip->ip_hl * 4;
		if (ihl < (int)sizeof(struct ip))          /* gate 2: ihl is sane */
			continue;
		if (n < ihl + (int)sizeof(t_icmp_hdr))     /* gate 3: ICMP header present */
			continue;
		reply = (t_icmp_hdr *)(ctx->recvbuf + ihl);
		if (reply->type != 0 || reply->code != 0)  /* 0 = ICMP_ECHOREPLY */
		{
			handle_icmp_error(ctx, ip, ihl, reply, n);
			continue;                              /* error, not a reply - not counted below */
		}
		if (reply->id != ctx->id)
			continue;
		seq = ntohs(reply->sequence);
		clock_gettime(CLOCK_MONOTONIC, &now_ts);
		rtt = elapsed_ms(&ctx->send_ts[seq % PING_TS_RING], &now_ts);
		stats->n_recv++;
		stats->sum += rtt;
		stats->sum_sq += rtt * rtt;
		if (stats->n_recv == 1 || rtt < stats->rtt_min)
			stats->rtt_min = rtt;
		if (stats->n_recv == 1 || rtt > stats->rtt_max)
			stats->rtt_max = rtt;
		if (ctx->flags.quiet)                       /* -q: summary lines only */
			continue;
		if (ctx->flags.flood)
		{
			putchar('\b');      /* -f: a reply erases one of send_one's dots */
			fflush(stdout);
			continue;
		}
		inet_ntop(AF_INET, &ip->ip_src, src, sizeof src);
		printf("%zd bytes from %s: icmp_seq=%u ttl=%u time=%.3f ms\n",
			n - ihl, src, seq, ip->ip_ttl, rtt);
	}
}
