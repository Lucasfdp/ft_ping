#include "ft_ping.h"
#include <netdb.h>   /* getaddrinfo(), gai_strerror(), EAI_SYSTEM */

/// @file ping_resolve.c
/// @brief Host text -> a destination address.

/// @brief Resolves an IPv4 literal or FQDN into a sockaddr_in.
///
/// Used for both the destination operand and the -S source address, which
/// is why it takes plain text and knows nothing about the context.
///
/// @param host Text to resolve.
/// @param out  Receives family, port 0, and the address.
/// @return 0 on success, -1 on failure (message already printed).
int	resolve_host(const char *host, struct sockaddr_in *out)
{
	struct addrinfo	hints;
	struct addrinfo	*res;
	int				rc;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;		/* IPv4 only -> the copy below is safe */
	hints.ai_socktype = SOCK_RAW;	/* collapse the per-socktype duplicates */
	rc = getaddrinfo(host, NULL, &hints, &res);
	if (rc != 0)
	{
		/* EAI_SYSTEM is the one code that reports through errno instead
		   of carrying its own message. */
		dprintf(2, "ft_ping: %s: %s\n", host,
			rc == EAI_SYSTEM ? strerror(errno) : gai_strerror(rc));
		return (-1);	/* res is untouched on failure - do NOT free it */
	}
	memcpy(out, res->ai_addr, sizeof *out);
	freeaddrinfo(res);
	return (0);
}
