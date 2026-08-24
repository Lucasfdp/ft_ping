#include "ft_ping.h"

/// @file ping_print_packet.c
/// @brief The per-packet lines.
///
/// None of these functions consult the flags: the caller has already
/// decided that this line should be printed. Keeping the decision at the
/// call site and the formatting here means the display rules live in one
/// place each, instead of being half-checked in two.

/// @brief The per-reply line: "64 bytes from 1.2.3.4: icmp_seq=0 ...".
/// @param icmp_bytes Bytes of ICMP (header + payload), i.e. total - IP hdr.
/// @param src        Source address as printable text.
/// @param seq        Sequence number, host byte order.
/// @param ttl        TTL field lifted from the reply's IP header.
/// @param rtt        Round-trip time in milliseconds.
void	print_reply(ssize_t icmp_bytes, const char *src, uint16_t seq,
		uint8_t ttl, double rtt)
{
	printf("%zd bytes from %s: icmp_seq=%u ttl=%u time=%.3f ms\n",
		icmp_bytes, src, seq, ttl, rtt);
}

/// @brief The per-error line: "From 1.2.3.4 icmp_seq=2 Time to live ...".
///
/// The sequence number comes from the QUOTED probe, not from the error
/// packet itself - it identifies which of our packets provoked this.
///
/// @param src  Source of the ICMP error, as printable text.
/// @param seq  Sequence number of the quoted probe, host byte order.
/// @param type ICMP type of the error.
/// @param code ICMP code of the error.
void	print_icmp_error(const char *src, uint16_t seq, uint8_t type,
		uint8_t code)
{
	printf("From %s icmp_seq=%u %s\n", src, seq,
		icmp_error_desc(type, code));
}

/// @brief -f: one dot per echo request sent.
///
/// Flushed explicitly: stdout is block-buffered when piped, so a flood run
/// would otherwise show nothing at all until it ended.
void	print_flood_send(void)
{
	putchar('.');
	fflush(stdout);
}

/// @brief -f: a backspace per reply, erasing one of print_flood_send()'s
///        dots.
///
/// What is left on screen is therefore the packets still outstanding - the
/// whole point of the flag.
void	print_flood_recv(void)
{
	putchar('\b');
	fflush(stdout);
}
