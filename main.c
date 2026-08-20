#include "ft_ping.h"
#include <sys/socket.h>   /* socket(), AF_INET, SOCK_RAW */
#include <netinet/in.h>   /* IPPROTO_ICMP */
#include <unistd.h>       /* close(), getpid(), sleep() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>       /* memcpy(), memset() */
#include <arpa/inet.h>    /* inet_ntop() */
#include <sys/time.h>     /* struct timespec */
#include <time.h>         /* clock_gettime(), CLOCK_MONOTONIC */
#include <signal.h>       /* sig_atomic_t, sigaction(), SIGINT */
#include <math.h>         /* sqrt() */

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

static void	fill_payload(t_icmp_packet *pkt)
{
	struct timespec	ts;
	size_t			i;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	memcpy(pkt->payload, &ts, sizeof ts);
	i = sizeof ts;
	while (i < PING_PAYLOAD_SIZE)
	{
		pkt->payload[i] = (uint8_t)i;
		i++;
	}
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

int	main(int ac, char **av)
{
	int					sockfd;
	int					seq;
	int					rc;
	t_icmp_packet		pkt;
	struct sockaddr_in	dst;
	char				ipstr[INET_ADDRSTRLEN];
	struct sigaction	sa;
	t_ping_stats		stats;

	if (ac != 2)
	{
		dprintf(2, "Error, wrong number of arguments\n");
		return (1);
	}
	seq = 0;   /* real ping starts at 0 too on Linux; kept as-is */
	memset(&stats, 0, sizeof stats);
	if (resolve_host(av[1], &dst) == -1)
		return (EXIT_FAILURE);
	inet_ntop(AF_INET, &dst.sin_addr, ipstr, sizeof ipstr);
	printf("PING %s (%s)\n", av[1], ipstr);
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd == -1)
	{
		perror("socket");
		return (EXIT_FAILURE);
	}
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sigint;   /* sa_flags left at 0: no SA_RESTART, so a
	                                blocked recvfrom() returns EINTR on Ctrl+C */
	if (sigaction(SIGINT, &sa, NULL) == -1)
	{
		perror("sigaction");
		close(sockfd);
		return (EXIT_FAILURE);
	}
	while (!g_stop)
	{
		memset(&pkt, 0, sizeof pkt);
		pkt.type = 8;
		pkt.code = 0;
		pkt.id = htons((uint16_t)getpid());
		pkt.sequence = htons(seq);
		fill_payload(&pkt);
		pkt.checksum = 0;   /* MUST be zero before checksumming - see below */
		pkt.checksum = ft_checksum(&pkt, sizeof pkt);
		if (sendto(sockfd, &pkt, sizeof pkt, 0, (struct sockaddr *)&dst, sizeof dst) == -1)
		{
			perror("Send");
			break ;
		}
		stats.n_sent++;
		rc = ft_rec_resp(sockfd, pkt.id, pkt.sequence, &stats);
		if (rc != 0)     /* -1 = real error, 1 = interrupted: either way, stop */
			break ;
		seq++;
		sleep(PING_INTERVAL_SEC);
	}
	print_stats(av[1], &stats);
	close(sockfd);
	return (EXIT_SUCCESS);
}
