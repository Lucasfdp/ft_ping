#include "ft_ping.h"
#include <limits.h>   /* LONG_MAX */
#include <unistd.h>   /* getopt(), optarg, optind, optopt, geteuid() */

/// @file ping_parse_args.c
/// @brief The option table: one getopt() switch, one case per flag.
///
/// The switch is intentionally left flat and long. It is the one place a
/// reader goes to answer "what does -X do here?", and folding it into
/// helpers would trade that directness for nothing. What HAS been factored
/// out is the parts that were pure repetition: the numeric range check and
/// its error message (opt_long / invalid_value below) and the post-parse
/// consistency rules (finalise_flags).
///
/// Flag reference, from the ping(8) man page:
///
///   -f            Flood ping. Outputs packets as fast as they come back or
///                 one hundred times per second, whichever is more. For
///                 every ECHO_REQUEST sent a period "." is printed, while
///                 for every ECHO_REPLY received a backspace is printed.
///                 This provides a rapid display of how many packets are
///                 being dropped. Only the super-user may use this option.
///                 This can be very hard on a network and should be used
///                 with caution.
///   -l preload    Send that many packets as fast as possible before
///                 falling into normal mode. Super-user only.
///   -i wait       Wait `wait` seconds between sending each packet. The
///                 default is one second. The wait time may be fractional,
///                 but only the super-user may specify values less than
///                 0.002 second. Incompatible with -f.
///   -m ttl        Set the IP Time To Live for outgoing packets. If not
///                 specified, the kernel uses net.inet.ip.ttl.
///   -o            Exit successfully after receiving one reply packet.
///   -Q            Somewhat quiet: don't display ICMP error messages that
///                 are in response to our own query messages.
///   -q            Quiet: nothing is displayed except the summary lines at
///                 startup time and when finished.
///   -r            Bypass the normal routing tables and send directly to a
///                 host on an attached network. If the host is not on a
///                 directly-attached network, an error is returned.
///   -p pattern    Up to 16 "pad" bytes to fill out the packet you send.
///                 Useful for diagnosing data-dependent problems in a
///                 network; "-p ff" fills the packet with all ones.
///   -S src_addr   Use this IP address as the source address in outgoing
///                 packets. If it is not one of this machine's interface
///                 addresses, an error is returned and nothing is sent.
///   -s packetsize Number of data bytes to send. Default 56, which becomes
///                 64 ICMP bytes once the 8-byte header is added.
///   -T ttl        Set the IP Time To Live for multicasted packets. Only
///                 applies if the destination is a multicast address.
///   -t timeout    Exit after this many seconds regardless of how many
///                 packets have been received.
///   -v            Verbose. ICMP packets other than ECHO_RESPONSE that are
///                 received are listed.

/// @brief The single diagnostic every numeric option shares.
/// @param arg The rejected argument text.
/// @param opt The option letter it belonged to.
/// @return Always 0, so callers can `return (invalid_value(...));`.
static int	invalid_value(const char *arg, int opt)
{
	fprintf(stderr, "ft_ping: invalid value '%s' for -%c\n", arg, opt);
	return (0);
}

/// @brief Reads an option argument as a long and range-checks it.
///
/// Collapses the "convert, then bounds-check, then print the same message"
/// block that -l, -m, -s, -T and -t each used to spell out in full.
///
/// @param arg optarg for this option.
/// @param opt The option letter, for the error message.
/// @param lo  Lowest accepted value, inclusive.
/// @param hi  Highest accepted value, inclusive.
/// @param out Receives the value only when the call succeeds.
/// @return 1 on success, 0 on rejection (message already printed).
static int	opt_long(char *arg, int opt, long lo, long hi, long *out)
{
	long	val;

	if (!test_strton(arg, 1, &val) || val < lo || val > hi)
		return (invalid_value(arg, opt));
	*out = val;
	return (1);
}

/// @brief The two options that are refused outright to a non-root user.
///
/// Checked here rather than at first use so the program fails before it
/// prints its banner, the way reference ping does.
///
/// @param opt The option letter, 'f' or 'l'.
/// @param what Short description used in the message.
/// @return 1 if we are root, 0 otherwise (message already printed).
static int	require_root(int opt, const char *what)
{
	if (geteuid() == 0)
		return (1);
	fprintf(stderr, "ft_ping: -%c: only root may use %s\n", opt, what);
	return (0);
}

/// @brief Cross-option rules that can only be checked once parsing is done.
///
/// Three things happen here, in this order:
///   1. -f and -i are mutually exclusive: -f defines its own interval, so
///      accepting both would silently ignore one of them.
///   2. -q wins over -Q and -v, silently, exactly as real ping does - a
///      quiet run stays quiet even if a louder flag came after it.
///   3. The host operand is required and is whatever getopt() left behind.
///      It points into argv and is never freed.
///
/// @param ac    argc as received by main().
/// @param av    argv as received by main().
/// @param flags Flags filled by the switch, adjusted in place.
/// @return 1 on success, 0 on rejection (message already printed).
static int	finalise_flags(int ac, char **av, t_flags *flags)
{
	if (flags->has_interval && flags->flood)
		return (fprintf(stderr, "invalid combination: -f + -i\n"), 0);
	if (flags->quiet)
	{
		flags->quiet_errors = 0;
		flags->verbose = 0;
	}
	if (optind >= ac)
		return (fprintf(stderr, "ft_ping: missing host operand\n"), 0);
	flags->host = av[optind];
	return (1);
}

/// @brief Parses every option and the host operand into @p flags.
/// @param ac    argc as received by main().
/// @param av    argv as received by main().
/// @param flags Zeroed struct to fill.
/// @return 1 on success, 0 on any rejection (message already printed).
int	parse_info(int ac, char **av, t_flags *flags)
{
	int		opt;
	long	tmp;
	double	d_tmp;

	tmp = 0;
	d_tmp = 0;
	/* Silence getopt's own message: every rejection below prints its own,
	   prefixed "ft_ping:" so it reads like the rest of our output. */
	opterr = 0;
	while ((opt = getopt(ac, av, ":fl:i:m:oQqrp:S:s:T:t:v")) != -1)
	{
		switch (opt)
		{
			case 'f':
				if (!require_root(opt, "flood ping"))
					return (0);
				flags->flood = 1;
				break ;

			case 'l':
				/* Value first, privilege second: a malformed count is
				   worth reporting even to a user who could not use -l. */
				if (!opt_long(optarg, opt, 0, LONG_MAX, &tmp))
					return (0);
				if (!require_root(opt, "preload"))
					return (0);
				flags->has_preload = 1;
				flags->preload = (int)tmp;
				break ;

			case 'i':
				if (!test_strton(optarg, 0, &d_tmp) || d_tmp <= 0)
					return (invalid_value(optarg, opt));
				/* Sub-2ms intervals are a flood by another name, so they
				   carry the same privilege requirement -f does. */
				if (d_tmp < 0.002 && geteuid() != 0)
					return (fprintf(stderr,
							"ft_ping: -i: intervals below 2ms require root\n"), 0);
				flags->has_interval = 1;
				flags->interval = d_tmp;
				break ;

			case 'm':
				if (!opt_long(optarg, opt, 0, 255, &tmp))
					return (0);
				flags->has_ttl = 1;
				flags->ttl = (int)tmp;
				break ;

			case 'o':
				flags->exit_on_reply = 1;
				break ;

			case 'Q':
				flags->quiet_errors = 1;
				break ;

			case 'q':
				flags->quiet = 1;
				break ;

			case 'r':
				flags->bypass_routing = 1;
				break ;

			case 'p':
				if (!decode_pattern(optarg, flags->pattern,
						&flags->pattern_len))
					return (fprintf(stderr,
							"ft_ping: patterns must be specified as hex "
							"digits (max 32 hex chars / 16 bytes)\n"), 0);
				flags->has_pattern = 1;
				break ;

			case 'S':
				/* Not resolved here: a source address is only meaningful
				   against a socket, so socket_open() validates it. */
				flags->has_source_addr = 1;
				flags->source_addr = optarg;
				break ;

			case 's':
				/* 65507 = 65535 (max IP datagram) - 20 (IP hdr) - 8 (ICMP). */
				if (!opt_long(optarg, opt, 0, 65507, &tmp))
					return (0);
				flags->has_packet_size = 1;
				flags->packet_size = (int)tmp;
				break ;

			case 'T':
				if (!opt_long(optarg, opt, 0, 255, &tmp))
					return (0);
				flags->has_multicast_ttl = 1;
				flags->multicast_ttl = (int)tmp;
				break ;

			case 't':
				if (!opt_long(optarg, opt, 1, LONG_MAX, &tmp))
					return (0);
				flags->has_timeout = 1;
				flags->timeout = (int)tmp;
				break ;

			case 'v':
				flags->verbose = 1;
				break ;

			case '?':
				return (fprintf(stderr, "unknown option: -%c\n", optopt), 0);

			case ':':
				return (fprintf(stderr, "-%c needs an argument\n", optopt), 0);
		}
	}
	return (finalise_flags(ac, av, flags));
}
