/* test_resolve.c - unit tests for resolve_host()
 *
 *   make test_resolve && ./test_resolve
 *
 * No root needed: resolve_host() opens no sockets.
 * Failing cases legitimately print "ft_ping: ..." on stderr - that IS the
 * behaviour under test. Run `./test_resolve 2>/dev/null` for clean output.
 */

#include "ft_ping.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

static int	g_pass = 0;
static int	g_fail = 0;
static int	g_skip = 0;

/* expect_ip == NULL  -> success, any address (DNS-dependent)
   expect_ip == ""    -> must FAIL to resolve
   otherwise          -> must resolve to exactly that dotted-quad          */
static void	check(const char *label, const char *host, const char *expect_ip,
		int needs_dns)
{
	struct sockaddr_in	out;
	char				got[INET_ADDRSTRLEN];
	int					rc;

	memset(&out, 0xAA, sizeof out);   /* poison: catch partial writes */
	rc = resolve_host(host, &out);

	if (expect_ip && expect_ip[0] == '\0')
	{
		if (rc == -1)
			printf("  PASS  %-28s rejected as expected\n", label), g_pass++;
		else
			printf("  FAIL  %-28s resolved but should have failed\n", label),
				g_fail++;
		return ;
	}
	if (rc != 0)
	{
		if (needs_dns)
			printf("  SKIP  %-28s no DNS in this environment\n", label),
				g_skip++;
		else
			printf("  FAIL  %-28s should have resolved\n", label), g_fail++;
		return ;
	}
	if (out.sin_family != AF_INET)
	{
		printf("  FAIL  %-28s sin_family is %d, want AF_INET (%d)\n",
			label, out.sin_family, AF_INET);
		g_fail++;
		return ;
	}
	if (out.sin_port != 0)
	{
		printf("  FAIL  %-28s sin_port is %u, want 0 (service was NULL)\n",
			label, ntohs(out.sin_port));
		g_fail++;
		return ;
	}
	inet_ntop(AF_INET, &out.sin_addr, got, sizeof got);
	if (expect_ip && strcmp(got, expect_ip) != 0)
	{
		printf("  FAIL  %-28s got %s, want %s\n", label, got, expect_ip);
		g_fail++;
		return ;
	}
	printf("  PASS  %-28s -> %s\n", label, got);
	g_pass++;
}

int	main(int ac, char **av)
{
	struct sockaddr_in	out;
	char				ip[INET_ADDRSTRLEN];
	int					i;

	/* ad-hoc mode: ./test_resolve google.com 1.1.1.1 ... */
	if (ac > 1)
	{
		i = 1;
		while (i < ac)
		{
			if (resolve_host(av[i], &out) == 0)
			{
				inet_ntop(AF_INET, &out.sin_addr, ip, sizeof ip);
				printf("%-32s -> %s\n", av[i], ip);
			}
			else
				printf("%-32s -> (failed)\n", av[i]);
			i++;
		}
		return (0);
	}

	printf("-- literals (no DNS) ------------------------------------\n");
	check("IPv4 loopback literal", "127.0.0.1", "127.0.0.1", 0);
	check("IPv4 public literal", "8.8.8.8", "8.8.8.8", 0);
	check("all-zeros literal", "0.0.0.0", "0.0.0.0", 0);
	check("broadcast literal", "255.255.255.255", "255.255.255.255", 0);

	printf("\n-- must be rejected -------------------------------------\n");
	check("empty string", "", "", 0);
	check("octet out of range", "256.1.1.1", "", 0);
	check("nonexistent TLD", "not.a.real.host.invalid", "", 1);
	check("trailing junk", "127.0.0.1.5", "", 0);

	printf("\n-- needs working DNS ------------------------------------\n");
	check("localhost", "localhost", "127.0.0.1", 1);
	check("FQDN", "google.com", NULL, 1);
	check("FQDN with trailing dot", "google.com.", NULL, 1);

	printf("\n%d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
	return (g_fail != 0);
}
