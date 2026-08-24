#include "ft_ping.h"

/// @file ping_icmp_error.c
/// @brief Non-echo-reply ICMP: deciding whether it was caused by us, and
///        saying what it means.
///
/// Layout of what ping_receive() just read into ctx->recvbuf when one of
/// these arrives:
///
///   IP header (ihl bytes)           <- the error packet's own IP header
///   ICMP header (8 bytes, `reply`)  <- type/code identify the error
///   quoted IP header                <- the ORIGINAL packet we sent, echoed
///   quoted ICMP header (8 bytes)    <- our original type/id/sequence
///
/// The quoted id is how we confirm the error was actually caused by US and
/// not by some other process's probe sharing the network.

/// @brief Types that legitimately carry a quoted copy of the datagram that
///        triggered them (RFC 792 / RFC 1122).
///
/// Anything else non-zero - a stray echo REQUEST from another host on the
/// segment, for instance - has no quoted header of ours to match against,
/// so it is silently ignored rather than mis-parsed as if it did.
///
/// @param type The ICMP type field of the received packet.
/// @return Non-zero if the packet quotes the datagram that caused it.
static int	is_icmp_error_type(uint8_t type)
{
	return (type == 3 || type == 4 || type == 5
		|| type == 11 || type == 12);
}

/// @brief Maps an ICMP error type/code pair to a human description.
///
/// Reference ping's wording where it matters. Not RFC-exhaustive: it
/// covers the codes you actually see pinging real hosts, and falls back to
/// the type's generic name for the rest.
///
/// @param type ICMP type, one of 3, 4, 5, 11, 12.
/// @param code ICMP code within that type.
/// @return A static string, never NULL, never freed.
const char	*icmp_error_desc(uint8_t type, uint8_t code)
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
	return ("Parameter problem");		/* only type 12 reaches here */
}

/// @brief Walks past the error's own headers to the probe it quotes.
///
/// Every step is bounds-checked against @p n before it is taken, because
/// routers are free to quote less than the full 8 bytes we want and a
/// truncated quote must be skipped, not read past.
///
/// @param reply The error's ICMP header.
/// @param ihl   Length of the error's own IP header.
/// @param n     Total bytes recvfrom() returned.
/// @return Pointer to our quoted ICMP header, or NULL if it is not fully
///         present.
static t_icmp_hdr	*quoted_probe(t_icmp_hdr *reply, int ihl, ssize_t n)
{
	struct ip	*inner_ip;
	int			inner_ihl;
	int			quote_off;

	quote_off = ihl + (int)sizeof(t_icmp_hdr);
	if (n < quote_off + (int)sizeof(struct ip))		/* quoted IP hdr, min size */
		return (NULL);
	inner_ip = (struct ip *)((uint8_t *)reply + sizeof(t_icmp_hdr));
	inner_ihl = inner_ip->ip_hl * 4;
	if (inner_ihl < (int)sizeof(struct ip))			/* quoted ip_hl is sane */
		return (NULL);
	if (n < quote_off + inner_ihl + (int)sizeof(t_icmp_hdr))
		return (NULL);								/* quoted ICMP hdr truncated */
	return ((t_icmp_hdr *)((uint8_t *)inner_ip + inner_ihl));
}

/// @brief Handles a non-echo-reply ICMP packet that may have been caused
///        by one of our own probes.
///
/// This is NOT a reply: the stats counters are deliberately left untouched,
/// so the probe it refers to still counts as loss in the final summary -
/// same as a packet that never came back at all.
///
/// @param ctx   Our id (to confirm the quoted probe is ours) and flags.
/// @param ip    The error packet's own IP header, start of recvbuf.
/// @param ihl   Length of that IP header in bytes.
/// @param reply The error's ICMP header; type/code identify the error.
/// @param n     Total bytes recvfrom() returned, for bounds checks.
void	handle_icmp_error(const t_ping_ctx *ctx, struct ip *ip, int ihl,
		t_icmp_hdr *reply, ssize_t n)
{
	t_icmp_hdr	*inner_icmp;
	char		src[INET_ADDRSTRLEN];

	if (!is_icmp_error_type(reply->type))
		return ;						/* not a type we can quote-parse */
	inner_icmp = quoted_probe(reply, ihl, n);
	if (!inner_icmp)
		return ;						/* quote truncated - nothing to match */
	if (inner_icmp->id != ctx->id && !ctx->flags.verbose)
		return ;						/* somebody else's probe - -v only */
	if (ctx->flags.quiet || ctx->flags.quiet_errors)
		return ;						/* -q or -Q suppresses this line */
	inet_ntop(AF_INET, &ip->ip_src, src, sizeof src);
	print_icmp_error(src, ntohs(inner_icmp->sequence), reply->type,
		reply->code);
}
