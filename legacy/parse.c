#include "ft_ping.h"
#include <ctype.h>   /* isxdigit() */

//-f -l -n -w -W -p -r -s -T --ttl --ip-timestamp
// -f      Flood ping.  Outputs packets as fast as they come back or one hundred times per second, whichever is more.  For every
//              ECHO_REQUEST sent a period “.” is printed, while for every ECHO_REPLY received a backspace is printed.  This provides a
//              rapid display of how many packets are being dropped.  Only the super-user may use this option.  This can be very hard on a
//              network and should be used with caution.

// -l preload
//              If preload is specified, ping sends that many packets as fast as possible before falling into its normal mode of behavior.
//              Only the super-user may use this option.

// -i wait
//              Wait wait seconds between sending each packet.  The default is to wait for one second between each packet.  The wait time
//              may be fractional, but only the super-user may specify values less than 0.002 second.  This option is incompatible with the
//              -f option.

// -m ttl  Set the IP Time To Live for outgoing packets.  If not specified, the kernel uses the value of the net.inet.ip.ttl MIB
//              variable.

// -n      Numeric output only.  No attempt will be made to lookup symbolic names for host addresses.
// -o      Exit successfully after receiving one reply packet.

// -Q      Somewhat quiet output.  Don't display ICMP error messages that are in response to our query messages.  Originally, the -v
//              flag was required to display such errors, but -v displays all ICMP error messages.  On a busy machine, this output can be
//              overbearing.  Without the -Q flag, ping prints out any ICMP error messages caused by its own ECHO_REQUEST messages.

// -q      Quiet output.  Nothing is displayed except the summary lines at startup time and when finished.

// -r      Bypass the normal routing tables and send directly to a host on an attached network.  If the host is not on a directly-
//              attached network, an error is returned.  This option can be used to ping a local host through an interface that has no
//              route through it.
// -p pattern
//              You may specify up to 16 “pad” bytes to fill out the packet you send.  This is useful for diagnosing data-dependent
//              problems in a network.  For example, “-p ff” will cause the sent packet to be filled with all ones.
// -S src_addr
//              Use the following IP address as the source address in outgoing packets.  On hosts with more than one IP address, this
//              option can be used to force the source address to be something other than the IP address of the interface the probe packet
//              is sent on.  If the IP address is not one of this machine's interface addresses, an error is returned and nothing is sent.

// -s packetsize
//              Specify the number of data bytes to be sent.  The default is 56, which translates into 64 ICMP data bytes when combined
//              with the 8 bytes of ICMP header data.  This option cannot be used with ping sweeps.

// -T ttl  Set the IP Time To Live for multicasted packets.  This flag only applies if the ping destination is a multicast address.

// -t timeout
//              Specify a timeout, in seconds, before ping exits regardless of how many packets have been received.

// -v      Verbose output.  ICMP packets other than ECHO_RESPONSE that are received are listed.



/* Decodes a -p hex string ("ff", "deadbeef", ...) into `out`, up to 16
   bytes. Rejects: odd length, any non-hex character, or more than 32 hex
   characters (reference ping's documented 16-byte pad limit). All three
   failures share one message, since the fix is the same for the user in
   every case - "make it valid hex, <= 32 characters".
   Returns 1 and sets *out_len on success, 0 on any rejection. */
static int	decode_pattern(const char *hex, uint8_t *out, int *out_len)
{
	size_t	len;
	size_t	i;
	char	byte_str[3];

	len = strlen(hex);
	if (len == 0 || len % 2 != 0 || len > 32)
		return (0);
	i = 0;
	while (i < len)
	{
		if (!isxdigit((unsigned char)hex[i]))
			return (0);
		i++;
	}
	i = 0;
	while (i < len)
	{
		byte_str[0] = hex[i];
		byte_str[1] = hex[i + 1];
		byte_str[2] = '\0';
		out[i / 2] = (uint8_t)strtol(byte_str, NULL, 16);
		i += 2;
	}
	*out_len = (int)(len / 2);
	return (1);
}

int	parse_info(int ac, char **av, t_flags *flags)
{
	int		opt;
	long	tmp;
	double	d_tmp;

	opt = 0;
	tmp = 0;
	d_tmp = 0;
	opterr = 0;
	while ((opt = getopt(ac, av, "fl:i:m:noQqrp:S:s:T:t:v")) != -1)
	{
		switch (opt)
		{
			case 'f':
				if (geteuid() != 0)
					return (fprintf(stderr,
						"ft_ping: -f: only root may use flood ping\n"), 0);
				flags->flood = 1;
				break;

			case 'l':
			{
				if (!test_strton(optarg, 1, &tmp) || tmp < 0)
					return (fprintf(stderr, "ft_ping: invalid value '%s' for -%c\n", optarg, opt), 0); // error handling needed
				if (geteuid() != 0)
					return (fprintf(stderr,
						"ft_ping: -l: only root may use preload\n"), 0);
				flags->has_preload = 1;
				flags->preload = tmp;
				break;
			}

			case 'i':
			{
				if (!test_strton(optarg, 0, &d_tmp) || d_tmp <= 0)
					return (fprintf(stderr, "ft_ping: invalid value '%s' for -%c\n", optarg, opt), 0); // error handling needed
				if (d_tmp < 0.002 && geteuid() != 0)
					return (fprintf(stderr,
						"ft_ping: -i: intervals below 2ms require root\n"), 0);
				flags->has_interval = 1;
				flags->interval = d_tmp;
				break;
			}

			case 'm':
			{
				if (!test_strton(optarg, 1, &tmp) || tmp < 0 || tmp > 255)
					return (fprintf(stderr, "ft_ping: invalid value '%s' for -%c\n", optarg, opt), 0); // error handling needed
				flags->has_ttl = 1;
				flags->ttl = tmp;
				break;
			}

			case 'n':
				flags->numeric = 1;
				break;

			case 'o':
				flags->exit_on_reply = 1;
				break;

			case 'Q':
				flags->quiet_errors = 1;
				break;

			case 'q':
				flags->quiet = 1;
				break;

			case 'r':
				flags->bypass_routing = 1;
				break;

			case 'p':
				if (!decode_pattern(optarg, flags->pattern,
						&flags->pattern_len))
					return (fprintf(stderr,
						"ft_ping: patterns must be specified as hex "
						"digits (max 32 hex chars / 16 bytes)\n"), 0);
				flags->has_pattern = 1;
				break;

			case 'S':
				flags->has_source_addr = 1;
				flags->source_addr = optarg;
				break;

			case 's':
			{
				if (!test_strton(optarg, 1, &tmp) || tmp < 0 || tmp > 65507)
					return (fprintf(stderr, "ft_ping: invalid value '%s' for -%c\n", optarg, opt), 0); // error handling needed
				flags->has_packet_size = 1;
				flags->packet_size = tmp;
				break;
			}

			case 'T':
			{
				if (!test_strton(optarg, 1, &tmp) || tmp < 0 || tmp > 255)
					return (fprintf(stderr, "ft_ping: invalid value '%s' for -%c\n", optarg, opt), 0); // error handling needed
				flags->has_multicast_ttl = 1;
				flags->multicast_ttl = tmp;
				break;
			}

			case 't':
			{
				if (!test_strton(optarg, 1, &tmp) || tmp <= 0)
					return (fprintf(stderr, "ft_ping: invalid value '%s' for -%c\n", optarg, opt), 0); // error handling needed
				flags->has_timeout = 1;
				flags->timeout = tmp;
				break;
			}

			case 'v':
				flags->verbose = 1;
				break;

			case '?':
				fprintf(stderr, "unknown option: -%c\n", optopt);
				return (0);

			case ':':
				fprintf(stderr, "-%c needs an argument\n", optopt);
				return (0);
		}
	}
	if (flags->has_interval && flags->flood)
		return (fprintf(stderr, "invalid combination: -f + -i\n"), 0);
	if (flags->quiet) //Real Ping q silently wins over Q and v
	{
		flags->quiet_errors = 0;
		flags->verbose = 0;
	}
	if (optind >= ac)
		return (fprintf(stderr, "ft_ping: missing host operand\n"), 0);
	flags->host = av[optind];
	return (1);
}

int	test_strton(char *str, int type, void *num)
{
	char *end = NULL;

	errno = 0;
	if (type)
		*(long *)num = strtol(str, &end, 10);
	else
		*(double *)num = strtod(str, &end);
	if (end == str || *end != '\0' || errno == ERANGE)
		return (0);
	return (1);
}
