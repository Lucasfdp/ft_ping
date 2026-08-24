#include "ft_ping.h"
#include <sys/socket.h>   /* socket(), AF_INET, SOCK_RAW */
#include <netinet/in.h>   /* IPPROTO_ICMP */
#include <unistd.h>       /* close(), getpid() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>       /* memcpy(), memset() */
#include <arpa/inet.h>    /* inet_ntop() */
#include <sys/time.h>     /* struct timespec */
#include <time.h>         /* clock_gettime(), CLOCK_MONOTONIC */
#include <signal.h>       /* sig_atomic_t, sigaction(), SIGINT */
#include <math.h>         /* sqrt(), fmin(), INFINITY */
#include <poll.h>         /* poll(), struct pollfd, POLLIN */
#include <errno.h>        /* errno, EINTR */

static volatile sig_atomic_t	g_stop = 0;

static void	on_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

double	elapsed_ms(const struct timespec *start, const struct timespec *end)
{
	int64_t	start_ns;
	int64_t	end_ns;

	start_ns = (int64_t)start->tv_sec * 1000000000LL + start->tv_nsec;
	end_ns   = (int64_t)end->tv_sec   * 1000000000LL + end->tv_nsec;
	return ((double)(end_ns - start_ns) / 1000000.0);
}

/* One monotonic clock reading, expressed in milliseconds as a double.
   Everything in the loop (interval, deadline, poll timeout) is a
   millisecond quantity, so keeping one unit avoids the timeval
   borrow/carry normalisation the reference implementation has to do
   by hand. CLOCK_MONOTONIC, not CLOCK_REALTIME: an NTP step or a
   daylight-saving change must not move our deadlines. */
static double	now_ms(void)
{
	struct timespec	ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0);
}

/* How long to block in poll(), in the int milliseconds poll() wants.
   Clamped at both ends: a target already in the past becomes 0 (poll
   returns immediately) rather than a negative value, which poll()
   reads as "block forever". The upper clamp keeps the cast to int
   safe when the target is INFINITY (no -t deadline).

   ceil(), not truncation: 0.4 ms left must round UP to 1 ms. Truncating
   it to 0 makes poll() return instantly with the target still in the
   future, and the loop spins on that sub-millisecond remainder. */
static int	ms_until(double target, double now)
{
	double	d;

	d = target - now;
	if (d < 0.0)
		return (0);
	if (d > 3600000.0)
		return (3600000);
	return ((int)ceil(d));
}

/* Writes the authoritative send timestamp into the ring (ft_rec_resp()
   always reads from here, regardless of what's below) and then fills the
   payload one of two ways:
   - -p given: the pattern repeats to cover payload_len. No timestamp
     copy goes into the payload here - that would overwrite the user's
     bytes and defeat the flag's purpose (diagnosing data-dependent link
     problems with a known, fixed byte pattern).
   - no -p: the old behaviour - a second copy of the timestamp "when it
     fits", then an incrementing filler. Costs nothing and matches
     reference ping's on-the-wire bytes for anyone diffing captures. */
static void	fill_payload(t_ping_ctx *ctx, uint16_t seq)
{
	uint8_t	*payload;
	size_t	i;

	clock_gettime(CLOCK_MONOTONIC, &ctx->send_ts[seq % PING_TS_RING]);
	payload = ctx->pkt + sizeof(t_icmp_hdr);
	if (ctx->flags.has_pattern)
	{
		i = 0;
		while (i < ctx->payload_len)
		{
			payload[i] = ctx->flags.pattern[i % (size_t)ctx->flags.pattern_len];
			i++;
		}
		return ;
	}
	i = 0;
	if (ctx->payload_len >= sizeof(struct timespec))
	{
		memcpy(payload, &ctx->send_ts[seq % PING_TS_RING],
			sizeof(struct timespec));
		i = sizeof(struct timespec);
	}
	while (i < ctx->payload_len)
	{
		payload[i] = (uint8_t)i;
		i++;
	}
}

/* Builds and transmits exactly one echo request, then returns. It does
   NOT wait for the reply - that is the whole point of the split: the
   caller decides when the next send is due, and the reply is collected
   independently whenever it happens to arrive.
   Returns 0 on success, -1 on a real sendto() failure. */

/// @brief 
/// @param sockfd 
/// @param ctx 
/// @param seq 
/// @param stats 
/// @return 
static int	send_one(int sockfd, t_ping_ctx *ctx, uint16_t seq,
	t_ping_stats *stats)
{
	t_icmp_hdr	*hdr;
	size_t		pktlen;

	pktlen = sizeof(t_icmp_hdr) + ctx->payload_len;
	hdr = (t_icmp_hdr *)ctx->pkt;
	hdr->type = 8;
	hdr->code = 0;
	hdr->id = ctx->id;
	hdr->sequence = htons(seq);
	fill_payload(ctx, seq);
	hdr->checksum = 0;   /* MUST be zero before checksumming */
	hdr->checksum = ft_checksum(ctx->pkt, pktlen);
	if (sendto(sockfd, ctx->pkt, pktlen, 0,
			(struct sockaddr *)&ctx->dst, sizeof ctx->dst) == -1)
		return (perror("Send"), -1);
	stats->n_sent++;
	if (ctx->flags.flood && !ctx->flags.quiet)
	{
		putchar('.');       /* -f prints a dot per request... */
		fflush(stdout);     /* ...and stdout is block-buffered when piped */
	}
	return (0);
}

/* n_sent == 0 (Ctrl+C before the first send even completes) and
   n_recv == 0 (every reply lost) both need to print a clean summary
   instead of dividing by zero. */
void	print_stats(const char *host, const t_ping_stats *stats)
{
	double	loss_pct;
	double	mean;
	double	variance;

	loss_pct = 0.0;
	if (stats->n_sent > 0)
		loss_pct = 100.0 * (stats->n_sent - stats->n_recv) / stats->n_sent;
	printf("\n--- %s ping statistics ---\n", host);
	printf("%d packets transmitted, %d packets received, %.0f%% packet loss\n",
		stats->n_sent, stats->n_recv, loss_pct);
	if (stats->n_recv == 0)
		return ;
	mean = stats->sum / stats->n_recv;
	variance = stats->sum_sq / stats->n_recv - mean * mean;
	if (variance < 0.0)     /* floating-point noise can push this slightly negative */
		variance = 0.0;
	printf("round-trip min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
		stats->rtt_min, mean, stats->rtt_max, sqrt(variance));
}

/* One place to free every heap buffer t_ping_ctx owns, so every exit path
   in main() - malloc failure, socket() failure, sigaction() failure, or
   the normal end of the run - releases the same three allocations the
   same way. free() on a NULL pointer is a no-op, so this is safe to call
   even if only some of the three ever got allocated. */
static void	free_ctx(t_ping_ctx *ctx)
{
	free(ctx->pkt);
	free(ctx->send_ts);
	free(ctx->recvbuf);
}

/* One-time socket setup for the four flags that map straight onto a
   setsockopt()/bind() call and need no involvement from the send/receive
   loop at all. Called once, after socket() but before any packet is
   sent - a bad -S address must exit cleanly here, not mid-run.
   Returns 0 on success, -1 on the first failure (message already
   printed). */
static int	configure_socket(int sockfd, const t_flags *f)
{
	int					on;
	struct sockaddr_in	src;

	if (f->has_ttl && setsockopt(sockfd, IPPROTO_IP, IP_TTL,
			&f->ttl, sizeof f->ttl) == -1)
		return (perror("setsockopt IP_TTL"), -1);
	if (f->has_multicast_ttl && setsockopt(sockfd, IPPROTO_IP,
			IP_MULTICAST_TTL, &f->multicast_ttl,
			sizeof f->multicast_ttl) == -1)
		return (perror("setsockopt IP_MULTICAST_TTL"), -1);
	if (f->bypass_routing)
	{
		on = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof on) == -1)
			return (perror("setsockopt SO_DONTROUTE"), -1);
	}
	if (f->has_source_addr)
	{
		memset(&src, 0, sizeof src);
		if (resolve_host(f->source_addr, &src) == -1)
			return (-1);   /* resolve_host() already printed the error */
		if (bind(sockfd, (struct sockaddr *)&src, sizeof src) == -1)
			return (perror("bind"), -1);
	}
	return (0);
}

int	main(int ac, char **av)
{
	int					sockfd;
	uint16_t			seq;
	int					rc;
	int					i;
	struct sigaction	sa;
	struct pollfd		pfd;
	t_ping_stats		stats;
	t_ping_ctx			ctx;
	double				interval;
	double				deadline;
	double				last_send;
	double				next_send;
	double				now;

	memset(&ctx, 0, sizeof ctx);
	if (!parse_info(ac, av, &ctx.flags))
		return (EXIT_FAILURE);
	seq = 0;   /* real ping starts at 0 too on Linux; kept as-is */
	ctx.id = htons((uint16_t)getpid());
	memset(&stats, 0, sizeof stats);
	if (resolve_host(ctx.flags.host, &ctx.dst) == -1)
		return (EXIT_FAILURE);
	inet_ntop(AF_INET, &ctx.dst.sin_addr, ctx.ipstr, sizeof ctx.ipstr);

	/* --- packet sizing: -s overrides the default, everything else derives --- */
	ctx.payload_len = PING_PAYLOAD_SIZE;
	if (ctx.flags.has_packet_size)
		ctx.payload_len = (size_t)ctx.flags.packet_size;
	ctx.recvbuf_len = ctx.payload_len + 128;   /* payload + generous header slack */
	if (ctx.recvbuf_len < 1024)
		ctx.recvbuf_len = 1024;                /* never smaller than the old fixed buffer */
	ctx.pkt = malloc(sizeof(t_icmp_hdr) + ctx.payload_len);
	ctx.send_ts = calloc(PING_TS_RING, sizeof(struct timespec));
	ctx.recvbuf = malloc(ctx.recvbuf_len);
	if (!ctx.pkt || !ctx.send_ts || !ctx.recvbuf)
	{
		perror("malloc");
		free_ctx(&ctx);
		return (EXIT_FAILURE);
	}
	printf("PING %s (%s): %zu data bytes\n", ctx.flags.host, ctx.ipstr,
		ctx.payload_len);
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd == -1)
	{
		perror("socket");
		free_ctx(&ctx);
		return (EXIT_FAILURE);
	}
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sigint;   /* sa_flags left at 0: no SA_RESTART, so a
	                                blocked poll() returns EINTR on Ctrl+C */
	if (sigaction(SIGINT, &sa, NULL) == -1)
	{
		perror("sigaction");
		close(sockfd);
		free_ctx(&ctx);
		return (EXIT_FAILURE);
	}
	if (configure_socket(sockfd, &ctx.flags) == -1)
	{
		close(sockfd);
		free_ctx(&ctx);
		return (EXIT_FAILURE);
	}

	/* --- pacing parameters, fixed for the whole run ------------------ */
	interval = 1000.0 * (ctx.flags.has_interval ? ctx.flags.interval : PING_INTERVAL_SEC);
	if (ctx.flags.flood)
		interval = PING_FLOOD_INTERVAL_MS;      /* -f: the 100/s floor */
	deadline = INFINITY;
	if (ctx.flags.has_timeout)                   /* -t: one absolute instant */
		deadline = now_ms() + 1000.0 * ctx.flags.timeout;

	/* --- -l: the preload burst, sent unpaced before normal service --- */
	i = 0;
	while (i < ctx.flags.preload && !g_stop)
	{
		if (send_one(sockfd, &ctx, seq++, &stats) == -1)
			g_stop = 1;
		i++;
	}
	/* The first paced packet goes out before the loop, so the loop body
	   always has a "last_send" to schedule the next one from. */
	if (!g_stop && send_one(sockfd, &ctx, seq++, &stats) == -1)
		g_stop = 1;
	last_send = now_ms();

	pfd.fd = sockfd;
	pfd.events = POLLIN;    /* the only event we care about: readable */
	while (!g_stop)
	{
		now = now_ms();
		if (now >= deadline)                                /* -t reached */
			break ;
		next_send = last_send + interval;
		/* Block until whichever comes first: a packet arrives, the next
		   send is due, or the -t deadline expires. */
		rc = poll(&pfd, 1, ms_until(fmin(next_send, deadline), now));
		if (rc < 0)
		{
			if (errno == EINTR)     /* SIGINT landed while blocked here */
				continue ;          /* -> re-test g_stop at the loop head */
			perror("poll");
			break ;
		}
		if (rc > 0)                 /* readable: drain everything queued */
		{
			if (ft_rec_resp(sockfd, &ctx, &stats) == -1)
				break ;
			if (ctx.flags.exit_on_reply && stats.n_recv > 0)    /* -o */
				break ;
		}
		/* Sending is decided by the CLOCK, never by poll()'s return value.
		   poll() returning 0 only means "nothing arrived in that window" -
		   it can expire against the -t deadline rather than against the
		   send schedule, and treating that as "send now" turns the last
		   fraction of a millisecond before the deadline into a flood. */
		now = now_ms();
		if (now >= deadline)
			break ;
		if (now >= next_send)
		{
			if (send_one(sockfd, &ctx, seq++, &stats) == -1)
				break ;
			last_send = now_ms();
		}
	}
	print_stats(ctx.flags.host, &stats);
	close(sockfd);
	free_ctx(&ctx);
	if (stats.n_recv == 0)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
