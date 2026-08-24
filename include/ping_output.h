#pragma once

/// @file ping_output.h
/// @brief Every line the program prints on stdout.
///
/// Kept apart from the network modules on purpose: the send and receive
/// paths decide WHAT happened, these functions decide how it LOOKS. The
/// -q / -f display rules live at the call sites that own them, so each
/// function here prints unconditionally when called.

#include <stdint.h>
#include <sys/types.h>
#include "ping_types.h"

/* ---- src/output/ping_print_packet.c ------------------------------- */

/// @brief The per-reply line: "64 bytes from 1.2.3.4: icmp_seq=0 ...".
/// @param icmp_bytes Bytes of ICMP (header + payload), i.e. total - IP hdr.
/// @param src        Source address as printable text.
/// @param seq        Sequence number, host byte order.
/// @param ttl        TTL field lifted from the reply's IP header.
/// @param rtt        Round-trip time in milliseconds.
void	print_reply(ssize_t icmp_bytes, const char *src, uint16_t seq,
			uint8_t ttl, double rtt);

/// @brief The per-error line: "From 1.2.3.4 icmp_seq=2 Time to live ...".
/// @param src  Source of the ICMP error, as printable text.
/// @param seq  Sequence number lifted from the QUOTED probe, host order.
/// @param type ICMP type of the error.
/// @param code ICMP code of the error.
void	print_icmp_error(const char *src, uint16_t seq, uint8_t type,
			uint8_t code);

/// @brief -f: one dot per echo request sent.
///
/// Flushes explicitly, because stdout is block-buffered when piped and a
/// flood run would otherwise show nothing until it ended.
void	print_flood_send(void);

/// @brief -f: a backspace per reply, erasing one of print_flood_send()'s
///        dots so the dots left on screen are the packets still missing.
void	print_flood_recv(void);

/* ---- src/output/ping_print_report.c ------------------------------- */

/// @brief The opening line: "PING host (1.2.3.4): 56 data bytes".
/// @param ctx Host text, resolved address and payload length.
void	print_ping_banner(const t_ping_ctx *ctx);

/// @brief The closing summary: counts, loss percentage and RTT figures.
///
/// Handles both degenerate cases without dividing by zero: n_sent == 0
/// (Ctrl+C before the first send completed) and n_recv == 0 (every reply
/// lost) each still print a clean summary.
///
/// @param host  Host text exactly as the user typed it.
/// @param stats Final counters and accumulators.
void	print_stats(const char *host, const t_ping_stats *stats);
