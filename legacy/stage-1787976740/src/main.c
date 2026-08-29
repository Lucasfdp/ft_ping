#include "ft_ping.h"

/// @file main.c
/// @brief Program entry point: the order of the run, and nothing else.
///
/// Every step below is one call into a module that owns it. Read this
/// function to learn WHAT happens and in which order; open the module to
/// learn how.
///
///   ctx_init          argv -> parsed flags, resolved host, buffers
///   print_ping_banner the "PING host (ip): N data bytes" line
///   socket_open       raw ICMP socket, plus -m / -T / -r / -S
///   signal_install    Ctrl+C ends the run cleanly
///   ping_run          preload burst, then paced sends and reply collection
///   print_stats       the closing summary
///
/// Exit status follows ping's convention: failure when nothing came back,
/// so a script can test reachability with `if ./ft_ping -t 2 host`.

/// @brief Entry point.
/// @param ac Argument count.
/// @param av Argument vector.
/// @return EXIT_SUCCESS if at least one reply was received, EXIT_FAILURE
///         on any setup error or when every packet was lost.
int	main(int ac, char **av)
{
	t_ping_ctx		ctx;
	t_ping_stats	stats;
	int				sockfd;

	if (ctx_init(&ctx, ac, av) == -1)
		return (EXIT_FAILURE);
	print_ping_banner(&ctx);
	sockfd = socket_open(&ctx.flags);
	if (sockfd == -1)
		return (ctx_destroy(&ctx), EXIT_FAILURE);
	if (signal_install() == -1)
		return (close(sockfd), ctx_destroy(&ctx), EXIT_FAILURE);
	ping_run(sockfd, &ctx, &stats);
	print_stats(ctx.flags.host, &stats);
	close(sockfd);
	ctx_destroy(&ctx);
	if (stats.n_recv == 0)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
