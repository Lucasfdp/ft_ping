#include "ft_ping.h"

/// @file ping_print_usage.c
/// @brief The help text, and the one-line hint that points at it.

/// @brief Prints the usage block on stdout.
///
/// stdout, not stderr: this is what the user ASKED for when they typed -?,
/// so it is output rather than a diagnostic and should survive a pipe into
/// `less`. The error paths that merely point at it write to stderr instead.
void	print_usage(void)
{
	printf("Usage: ft_ping [OPTION...] HOST\n");
	printf("Send ICMP ECHO_REQUEST packets to a network host.\n\n");
	printf("  -f             flood ping (root only)\n");
	printf("  -l PRELOAD     send PRELOAD packets as fast as possible "
		"first (root only)\n");
	printf("  -i WAIT        seconds between packets (default 1, "
		"fractional allowed)\n");
	printf("  -m TTL         set the IP Time To Live for outgoing packets\n");
	printf("  -o             exit successfully after the first reply\n");
	printf("  -p PATTERN     up to 16 pad bytes, as hex digits "
		"(e.g. -p ff)\n");
	printf("  -Q             do not show ICMP errors caused by our own "
		"packets\n");
	printf("  -q             quiet: only the banner and the summary\n");
	printf("  -r             bypass the routing table\n");
	printf("  -S SRC_ADDR    use SRC_ADDR as the source address\n");
	printf("  -s PACKETSIZE  data bytes to send (default 56, max 65507)\n");
	printf("  -T TTL         set the IP Time To Live for multicast "
		"packets\n");
	printf("  -t TIMEOUT     exit after TIMEOUT seconds regardless\n");
	printf("  -v             verbose: also report ICMP errors caused by "
		"other processes\n");
	printf("  -?             give this help list\n\n");
	printf("A raw socket is required, so ft_ping must run as root or with "
		"cap_net_raw.\n");
}

/// @brief The one-line pointer printed after a usage error.
///
/// Separate from print_usage() on purpose: dumping the whole help block
/// after every typo buries the error message that actually matters. The
/// error goes first, this points at the rest.
void	print_usage_hint(void)
{
	fprintf(stderr, "Try 'ft_ping -?' for more information.\n");
}
