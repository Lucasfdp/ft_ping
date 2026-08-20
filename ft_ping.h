#pragma once

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>   /* struct sockaddr_in, IPPROTO_ICMP */
#include <sys/socket.h>   /* socket(), AF_INET, SOCK_RAW */
#include <unistd.h>       /* close(), getpid() */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>       /* memcpy(), memset() */
#include <arpa/inet.h>    /* inet_ntop() */
#include <sys/time.h>     /* clock_gettime(), struct timespec */

/* 56 data bytes + 8 header bytes = the 64 that real ping reports */
#define PING_PAYLOAD_SIZE 56
/* seconds between pings, real ping's default interval */
#define PING_INTERVAL_SEC 1

typedef struct s_icmp_packet
{
    uint8_t   type;                        // 0
    uint8_t   code;                        // 1
    uint16_t  checksum;                    // 2-3
    uint16_t  id;                          // 4-5
    uint16_t  sequence;                    // 6-7
    uint8_t   payload[PING_PAYLOAD_SIZE];  // 8-63
}   t_icmp_packet;                         // 64 bytes

/* No padding is allowed anywhere in this struct: it is copied byte-for-byte
   onto the wire. If a future edit reorders members or widens a field, the
   compiler must reject it rather than silently sending garbage. */
_Static_assert(sizeof(t_icmp_packet) == 8 + PING_PAYLOAD_SIZE,
    "t_icmp_packet has padding or the wrong size - it must be exactly 64 bytes");

/* Running totals for the final summary line. Bundled into one struct so
   ft_rec_resp() stays at 4 parameters instead of piling on separate
   out-params for n_recv/sum/sum_sq/min/max. */
typedef struct s_ping_stats
{
    int    n_sent;
    int    n_recv;
    double sum;      /* sum of RTTs, ms - for the mean */
    double sum_sq;   /* sum of RTT^2, ms^2 - for mdev */
    double rtt_min;
    double rtt_max;
}   t_ping_stats;

uint16_t ft_checksum(const void *buf, size_t len);
int      resolve_host(const char *host, struct sockaddr_in *out);

void fatal_error(char *msg);

/* Blocks until a reply matching sent_id/sent_seq arrives and updates
   *stats, or until interrupted / a real socket error occurs.
   Returns: 0 = matched reply recorded, 1 = interrupted (EINTR, not an
   error), -1 = real recvfrom() failure (perror already printed). */
int    ft_rec_resp(int sockfd, uint16_t sent_id, uint16_t sent_seq,
    t_ping_stats *stats);
double elapsed_ms(const struct timespec *start, const struct timespec *end);
void   print_stats(const char *host, const t_ping_stats *stats);
