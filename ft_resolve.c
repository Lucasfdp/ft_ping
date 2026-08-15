#include "ft_ping.h"
#include <sys/types.h>
#include <sys/socket.h>   /* AF_INET, SOCK_RAW */
#include <netdb.h>        /* getaddrinfo(), gai_strerror() */
#include <string.h>       /* memcpy(), memset(), strerror() */
#include <stdio.h>        /* dprintf() */
#include <errno.h>        /* errno, for EAI_SYSTEM */

/* Resolves `host` (IPv4 literal or FQDN) into *out.
   Returns 0 on success, -1 on failure (message already printed). */
int	resolve_host(const char *host, struct sockaddr_in *out)
{
	struct addrinfo	hints;
	struct addrinfo	*res;
	int				rc;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;      /* IPv4 only -> the cast below is safe */
	hints.ai_socktype = SOCK_RAW;   /* collapse the per-socktype duplicates */

	rc = getaddrinfo(host, NULL, &hints, &res);
	if (rc != 0)
	{
		/* EAI_SYSTEM is the one code that reports through errno instead */
		dprintf(2, "ft_ping: %s: %s\n", host,
			rc == EAI_SYSTEM ? strerror(errno) : gai_strerror(rc));
		return (-1);   /* res is untouched on failure - do NOT free it */
	}
	memcpy(out, res->ai_addr, sizeof *out);
	freeaddrinfo(res);
	return (0);
}
