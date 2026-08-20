#include "ft_ping.h"
#include <string.h>   /* memcpy() */

/* Internet checksum (RFC 1071): sum every 16-bit word, fold any carry out
   of the low 16 bits back in, then take the one's complement. */
uint16_t	ft_checksum(const void *buf, size_t len)
{
	const uint8_t	*p;
	uint32_t		sum;
	uint16_t		word;

	p = buf;
	sum = 0;
	while (len > 1)
	{
		memcpy(&word, p, 2);
		sum += word;
		p += 2;
		len -= 2;
	}
	if (len == 1)
		sum += ((uint16_t)p[0]) << 8;
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ((uint16_t)~sum);
}
