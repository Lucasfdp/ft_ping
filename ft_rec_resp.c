#include "ft_ping.h"
#include <netinet/ip.h>   /* struct ip */
#include <errno.h>        /* errno, EINTR */
#include <time.h>         /* clock_gettime(), CLOCK_MONOTONIC */

/* Reads until a packet arrives that is actually a reply to OUR request.
   Gates  = "do the bytes I am about to read exist?" -> always a shortage (<).
   Filter = "is this packet mine?" -> id/seq compared RAW (both are already in
            network order), never byte-swapped a second time.
   A failed gate or filter is not an error: a raw socket delivers every ICMP
   packet the host receives, so we skip it and keep listening. Only a failed
   syscall, or a signal landing while blocked, returns. */
int	ft_rec_resp(int sockfd, uint16_t sent_id, uint16_t sent_seq,
	t_ping_stats *stats)
{
	_Alignas(struct ip) char	buf[1024];
	char						src[INET_ADDRSTRLEN];
	struct ip					*ip;
	t_icmp_packet				*reply;
	ssize_t						n;
	int							ihl;
	struct timespec				send_ts;
	struct timespec				now_ts;
	double						rtt;

	while (1)
	{
		n = recvfrom(sockfd, buf, sizeof buf, 0, NULL, NULL);
		if (n < 0)
		{
			if (errno == EINTR)     /* SIGINT landed while blocked here */
				return (1);
			return (perror("recvfrom"), -1);
		}
		if (n < (ssize_t)sizeof(struct ip))        /* gate 1: IP header present */
			continue;
		ip = (struct ip *)buf;
		ihl = ip->ip_hl * 4;
		if (ihl < (int)sizeof(struct ip))          /* gate 2: ihl is sane */
			continue;
		if (n < ihl + 8)                           /* gate 3: ICMP header present */
			continue;
		reply = (t_icmp_packet *)(buf + ihl);
		if (reply->type != 0 || reply->code != 0)  /* 0 = ICMP_ECHOREPLY */
			continue;
		if (reply->id != sent_id)
			continue;
		if (reply->sequence != sent_seq)
			continue;
		memcpy(&send_ts, reply->payload, sizeof send_ts);
		clock_gettime(CLOCK_MONOTONIC, &now_ts);
		rtt = elapsed_ms(&send_ts, &now_ts);
		stats->n_recv++;
		stats->sum += rtt;
		stats->sum_sq += rtt * rtt;
		if (stats->n_recv == 1 || rtt < stats->rtt_min)
			stats->rtt_min = rtt;
		if (stats->n_recv == 1 || rtt > stats->rtt_max)
			stats->rtt_max = rtt;
		inet_ntop(AF_INET, &ip->ip_src, src, sizeof src);
		printf("%zd bytes from %s: icmp_seq=%u ttl=%u time=%.3f ms\n",
			n - ihl, src, ntohs(reply->sequence), ip->ip_ttl, rtt);
		return (0);
	}
}
