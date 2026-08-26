#include "ft_ping.h"

/// @file ping_checksum.c
/// @brief The RFC 1071 Internet checksum.

/// @brief Internet checksum, RFC 1071.
///
/// Three steps: sum every 16-bit word, fold the carry that accumulated
/// above bit 15 back into the low half, then take the one's complement.
/// Because the sum is byte-order agnostic (swapping every word swaps the
/// result), the value comes out already in network order and can be stored
/// straight into t_icmp_hdr::checksum.
///
/// @param buf Start of the bytes to checksum.
/// @param len Length in bytes; an odd length is zero-padded implicitly.
/// @return The 16-bit checksum.
uint16_t	ft_checksum(const void *buf, size_t len)
{
	const uint8_t	*p;
	uint32_t		sum;
	uint16_t		word;

	p = buf;
	sum = 0;
	/* memcpy() rather than a uint16_t* cast: buf is not guaranteed to be
	   2-byte aligned, and the cast would be undefined behaviour on the
	   architectures that care. */
	while (len > 1)
	{
		memcpy(&word, p, 2);
		sum += word;
		p += 2;
		len -= 2;
	}
	if (len == 1)
	{
		/* The implicit RFC 1071 zero-pad byte. Copying into the LOW byte
		   of a zeroed word matches the 2-byte loop's byte order: first
		   byte in -> low half on a little-endian host, so the trailing
		   byte lands exactly where a real pair would have put it. */
		word = 0;
		memcpy(&word, p, 1);
		sum += word;
	}
	/* Loop, not a single fold: adding the carry back can itself carry. */
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ((uint16_t)~sum);
}
