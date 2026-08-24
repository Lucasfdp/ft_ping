#pragma once

/// @file ping_net.h
/// @brief Everything that touches the network: name resolution, socket
///        setup, the send path, the receive path, and ICMP semantics.

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>    /* ssize_t */
#include <netinet/in.h>   /* struct sockaddr_in */
#include <netinet/ip.h>   /* struct ip */
#include "ping_types.h"

/* ---- src/net/ping_checksum.c -------------------------------------- */

/// @brief Internet checksum, RFC 1071.
///
/// Sums every 16-bit word, folds any carry out of the low 16 bits back
/// in, then takes the one's complement. The result is already in network
/// byte order and can be stored into t_icmp_hdr::checksum directly.
///
/// @param buf Start of the bytes to checksum.
/// @param len Length in bytes; an odd length is zero-padded implicitly.
/// @return The 16-bit checksum.
uint16_t	ft_checksum(const void *buf, size_t len);

/* ---- src/net/ping_resolve.c --------------------------------------- */

/// @brief Resolves an IPv4 literal or FQDN into a sockaddr_in.
/// @param host Text to resolve.
/// @param out  Receives family, port 0, and the address.
/// @return 0 on success, -1 on failure (message already printed).
int			resolve_host(const char *host, struct sockaddr_in *out);

/* ---- src/net/ping_socket.c ---------------------------------------- */

/// @brief Creates the raw ICMP socket and applies every option that maps
///        straight onto setsockopt()/bind().
///
/// Done once, before any packet is sent: a bad -S address must exit
/// cleanly here rather than mid-run. Requires root (SOCK_RAW).
///
/// @param flags Parsed options; -m, -T, -r and -S are consumed here.
/// @return The socket fd, or -1 on failure (message already printed).
int			socket_open(const t_flags *flags);

/* ---- src/net/ping_send.c ------------------------------------------ */

/// @brief Builds and transmits exactly one echo request, then returns.
///
/// It does NOT wait for the reply - that is the whole point of the
/// split: the caller decides when the next send is due, and the reply is
/// collected independently whenever it happens to arrive.
///
/// @param sockfd Raw ICMP socket from socket_open().
/// @param ctx    Packet buffer, destination and timestamp ring.
/// @param seq    Sequence number for this probe, host byte order.
/// @param stats  n_sent is incremented on success.
/// @return 0 on success, -1 on a real sendto() failure.
int			send_one(int sockfd, t_ping_ctx *ctx, uint16_t seq,
				t_ping_stats *stats);

/* ---- src/net/ping_receive.c --------------------------------------- */

/// @brief Drains every ICMP packet currently queued on the socket,
///        recording the ones that are replies to us.
///
/// Non-blocking. Call only when poll() says the socket is readable: it
/// reads until the socket is empty and then returns, so one readable
/// event never leaves a second queued packet behind.
///
/// @param sockfd Raw ICMP socket.
/// @param ctx    Read-only: receive buffer, our id, timestamp ring, flags.
/// @param stats  Reply counters and RTT accumulators, updated in place.
/// @return 0 when the socket is drained (zero or more replies recorded),
///         -1 on a real recvfrom() failure (message already printed).
int			ping_receive(int sockfd, const t_ping_ctx *ctx,
				t_ping_stats *stats);

/* ---- src/net/ping_icmp_error.c ------------------------------------ */

/// @brief Handles a non-echo-reply ICMP packet that may have been caused
///        by one of our own probes.
///
/// This is NOT a reply: the stats counters are left untouched, so it
/// counts as loss in the final summary - same as a packet that never
/// came back at all. Prints one line unless -q/-Q suppress it.
///
/// @param ctx   Our id (to confirm the quoted probe is ours) and flags.
/// @param ip    The error packet's own IP header, start of recvbuf.
/// @param ihl   Length of that IP header in bytes.
/// @param reply The error's ICMP header; type/code identify the error.
/// @param n     Total bytes recvfrom() returned, for bounds checks.
void		handle_icmp_error(const t_ping_ctx *ctx, struct ip *ip,
				int ihl, t_icmp_hdr *reply, ssize_t n);

/// @brief Maps an ICMP error type/code pair to a human description.
///
/// Uses reference ping's wording where it matters. Not RFC-exhaustive:
/// it covers the codes you actually see pinging real hosts.
///
/// @param type ICMP type, one of 3, 4, 5, 11, 12.
/// @param code ICMP code within that type.
/// @return A static string, never NULL, never freed.
const char	*icmp_error_desc(uint8_t type, uint8_t code);
