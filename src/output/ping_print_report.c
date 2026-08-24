#include "ft_ping.h"

/// @file ping_print_report.c
/// @brief The two lines that bracket a run: the opening banner and the
///        closing statistics block.

/// @brief The opening line: "PING host (1.2.3.4): 56 data bytes".
///
/// Prints the host exactly as the user typed it alongside what it resolved
/// to, so a surprising resolution is visible immediately rather than after
/// the first reply.
///
/// @param ctx Host text, resolved address and payload length.
void	print_ping_banner(const t_ping_ctx *ctx)
{
	printf("PING %s (%s): %zu data bytes\n", ctx->flags.host, ctx->ipstr,
		ctx->payload_len);
}

/// @brief The closing summary: counts, loss percentage and RTT figures.
///
/// Both degenerate cases are handled without dividing by zero:
///   n_sent == 0  Ctrl+C arrived before the first send completed;
///   n_recv == 0  every reply was lost - the loss line still prints, the
///                RTT line is simply omitted.
///
/// mdev is derived from the running sums rather than from a stored list of
/// samples: variance = E[x^2] - E[x]^2 needs only sum and sum_sq, so the
/// memory cost is constant however long the run lasts.
///
/// @param host  Host text exactly as the user typed it.
/// @param stats Final counters and accumulators.
void	print_stats(const char *host, const t_ping_stats *stats)
{
	double	loss_pct;
	double	mean;
	double	variance;

	loss_pct = 0.0;
	if (stats->n_sent > 0)
		loss_pct = 100.0 * (stats->n_sent - stats->n_recv) / stats->n_sent;
	printf("\n--- %s ping statistics ---\n", host);
	printf("%d packets transmitted, %d packets received, %.0f%% packet loss\n",
		stats->n_sent, stats->n_recv, loss_pct);
	if (stats->n_recv == 0)
		return ;
	mean = stats->sum / stats->n_recv;
	variance = stats->sum_sq / stats->n_recv - mean * mean;
	/* Floating-point noise can push a mathematically-zero variance
	   slightly negative, and sqrt() of that is NaN. */
	if (variance < 0.0)
		variance = 0.0;
	printf("round-trip min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
		stats->rtt_min, mean, stats->rtt_max, sqrt(variance));
}
