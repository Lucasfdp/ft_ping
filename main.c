#include "ft_ping.h"
#include <sys/socket.h>   /* socket(), AF_INET, SOCK_RAW */
#include <netinet/in.h>   /* IPPROTO_ICMP */
#include <unistd.h>       /* close(), getpid() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>       /* memcpy(), memset() */
#include <arpa/inet.h>    /* inet_ntop() */
#include <sys/time.h>     /* gettimeofday(), struct timeval */

/* First sizeof(struct timeval) bytes = send time, so the echoed reply carries
   its own send timestamp back and Stage 6 needs no state table.
   The rest is real ping's incrementing fill pattern. */
static void	fill_payload(t_icmp_packet *pkt)
{
	struct timeval	tv;
	size_t			i;

	gettimeofday(&tv, NULL);
	memcpy(pkt->payload, &tv, sizeof tv);
	i = sizeof tv;
	while (i < PING_PAYLOAD_SIZE)
	{
		pkt->payload[i] = (uint8_t)i;
		i++;
	}
}

int	main(int ac, char **av)
{
	if (ac != 2)
	{
		dprintf(2, "Error, wrong number of arguments\n");
		return (1);
	}
	int					sockfd;
	int					seq = 0;
	t_icmp_packet		pkt;
	struct sockaddr_in	dst;
	char				ipstr[INET_ADDRSTRLEN];

	if (resolve_host(av[1], &dst) == -1)
		return (EXIT_FAILURE);
	inet_ntop(AF_INET, &dst.sin_addr, ipstr, sizeof ipstr);
	printf("PING %s (%s)\n", av[1], ipstr);

	memset(&pkt, 0, sizeof pkt);
	pkt.type = 8;
	pkt.code = 0;
	pkt.id = htons((uint16_t)getpid());
	pkt.sequence = htons(seq);
	fill_payload(&pkt);
	pkt.checksum = 0;   /* MUST be zero before checksumming - see below */
	pkt.checksum = ft_checksum(&pkt, sizeof pkt);
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	
	if (sockfd == -1)
	{
		perror("socket");
		return (EXIT_FAILURE);
	}
	if (sendto(sockfd, &pkt, sizeof pkt, 0, (struct sockaddr *)&dst, sizeof dst) == -1)
	{
		perror("send");
		return (EXIT_FAILURE);
	}
	close(sockfd);
	return (EXIT_SUCCESS);
}
