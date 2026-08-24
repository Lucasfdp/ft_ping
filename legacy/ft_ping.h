#pragma once

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>   /* struct sockaddr_in, IPPROTO_ICMP */
#include <sys/socket.h>   /* socket(), AF_INET, SOCK_RAW */
#include <unistd.h>       /* close(), getpid() */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/errno.h>
#include <string.h>       /* memcpy(), memset() */
#include <arpa/inet.h>    /* inet_ntop() */
#include <sys/time.h>     /* clock_gettime(), struct timespec */

/* 56 data bytes + 8 header bytes = the 64 that real ping reports by
   default. This is now only the DEFAULT payload size - -s overrides it
   at runtime, so it no longer sizes any struct. */
#define PING_PAYLOAD_SIZE 56
/* seconds between pings, real ping's default interval. -i overrides it. */
#define PING_INTERVAL_SEC 1.0
/* -f floor: 100 packets/second, matching inetutils ping.c's flood intvl */
#define PING_FLOOD_INTERVAL_MS 10.0
/* One send-timestamp slot per possible uint16_t sequence number, indexed
   [seq % PING_TS_RING]. Needed once the payload can be too small (-s) or
   user-controlled (-p) to carry the timestamp itself. */
#define PING_TS_RING 65536

typedef struct s_icmp_hdr
{
    uint8_t   type;      // 0
    uint8_t   code;      // 1
    uint16_t  checksum;  // 2-3
    uint16_t  id;        // 4-5
    uint16_t  sequence;  // 6-7
}   t_icmp_hdr;           // 8 bytes - payload is a separate, variable-length buffer now

/* No padding is allowed: this header is copied byte-for-byte onto the
   wire. If a future edit reorders members or widens a field, the
   compiler must reject it rather than silently sending garbage. */
_Static_assert(sizeof(t_icmp_hdr) == 8,
    "t_icmp_hdr has padding or the wrong size - it must be exactly 8 bytes");

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

typedef struct s_flags
{
    int		flood;				// -f
    int		has_preload;		// -l <preload>
    int		preload;

    int		has_interval;		// -i <wait>
    double	interval;

    int		has_ttl;			// -m <ttl>
    int		ttl;

    int		numeric;			// -n
    int		exit_on_reply;		// -o
    int		quiet_errors;		// -Q
    int		quiet;				// -q
    int		bypass_routing;		// -r

    int		has_pattern;		// -p <pattern>
    uint8_t	pattern[16];		// decoded bytes, repeats to fill the payload
    int		pattern_len;		// 1..16 - how many of the above are used

    int		has_source_addr;	// -S <src_addr>
    char	*source_addr;

    int		has_packet_size;	// -s <packetsize>
    int		packet_size;

    int		has_multicast_ttl;	// -T <ttl>
    int		multicast_ttl;

    int		has_timeout;		// -t <timeout>
    int		timeout;

    int		verbose;			// -v

    char    *host;
}	t_flags;

/* Everything the send path and the receive path both need, bundled so
   send_one()/ft_rec_resp() take one pointer instead of a growing list of
   separate arguments. Built once in main(), after parsing and resolving,
   then passed around read-only (ft_rec_resp() takes it as const). */
typedef struct s_ping_ctx
{
    t_flags             flags;
    struct sockaddr_in  dst;
    char                ipstr[INET_ADDRSTRLEN];
    uint16_t            id;           /* our pid-derived ICMP id, network order */
    uint8_t             *pkt;         /* header + payload, malloc'd once: 8 + payload_len */
    size_t              payload_len;  /* bytes of payload, NOT counting the 8-byte header */
    struct timespec     *send_ts;     /* PING_TS_RING entries, calloc'd once */
    uint8_t             *recvbuf;     /* scratch buffer for recvfrom() */
    size_t              recvbuf_len;
}   t_ping_ctx;

uint16_t ft_checksum(const void *buf, size_t len);
int      resolve_host(const char *host, struct sockaddr_in *out);

void fatal_error(char *msg);

/* Non-blocking. Call only when poll() says the socket is readable: it
   drains every queued packet, records the ones whose id is ours, and
   returns as soon as the socket is empty.
   Returns: 0 = drained (zero or more replies recorded), -1 = real
   recvfrom() failure (perror already printed). */
int    ft_rec_resp(int sockfd, const t_ping_ctx *ctx, t_ping_stats *stats);
double elapsed_ms(const struct timespec *start, const struct timespec *end);
void   print_stats(const char *host, const t_ping_stats *stats);
int	test_strton(char *str, int type, void *num);
int	parse_info(int ac, char **av, t_flags *flags);
