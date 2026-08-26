#include "ft_ping.h"

/// @file ping_socket.c
/// @brief Creating the raw ICMP socket and applying the four options that
///        are pure socket configuration.
///
/// -m, -T, -r and -S never appear again after this file: they map straight
/// onto setsockopt()/bind() and need no involvement from the send or
/// receive path at all.

/// @brief Applies -m, -T, -r and -S to an already-open socket.
///
/// Called before the first packet is sent, so a bad value fails the run
/// cleanly here rather than halfway through it. Returns on the FIRST
/// failure: leaving the socket partly configured would mean sending
/// packets that only half obey the command line.
///
/// @param sockfd The raw socket to configure.
/// @param f      Parsed options.
/// @return 0 on success, -1 on the first failure (message already printed).
static int	configure_socket(int sockfd, const t_flags *f)
{
	int					on;
	struct sockaddr_in	src;

	if (f->has_ttl && setsockopt(sockfd, IPPROTO_IP, IP_TTL,
			&f->ttl, sizeof f->ttl) == -1)
		return (perror("setsockopt IP_TTL"), -1);
	if (f->has_multicast_ttl && setsockopt(sockfd, IPPROTO_IP,
			IP_MULTICAST_TTL, &f->multicast_ttl,
			sizeof f->multicast_ttl) == -1)
		return (perror("setsockopt IP_MULTICAST_TTL"), -1);
	if (f->bypass_routing)
	{
		on = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof on) == -1)
			return (perror("setsockopt SO_DONTROUTE"), -1);
	}
	if (f->has_source_addr)
	{
		/* bind() is what makes -S take effect: it fixes the source address
		   the kernel stamps into every outgoing packet. Resolving it here
		   also means a bogus -S is caught before we print anything. */
		memset(&src, 0, sizeof src);
		if (resolve_host(f->source_addr, &src) == -1)
			return (-1);	/* resolve_host() already printed the error */
		if (bind(sockfd, (struct sockaddr *)&src, sizeof src) == -1)
			return (perror("bind"), -1);
	}
	return (0);
}

/// @brief Creates the raw ICMP socket and configures it.
///
/// SOCK_RAW is a privileged operation: without root, socket() fails here
/// with EPERM and the run ends before anything else is attempted.
///
/// @param flags Parsed options; -m, -T, -r and -S are consumed here.
/// @return The socket fd, or -1 on failure (message already printed).
int	socket_open(const t_flags *flags)
{
	int	sockfd;

	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd == -1)
		return (perror("socket"), -1);
	/* Close on failure here, not in the caller: this function owns the fd
	   until it successfully hands it back. */
	if (configure_socket(sockfd, flags) == -1)
		return (close(sockfd), -1);
	return (sockfd);
}
