#include "ft_ping.h"

/// @file ping_print_report.c
/// @brief The two lines that bracket a run: the opening banner and the
///        closing statistics block.
///
/// Every format string here is matched against inetutils-2.0, which is what
/// the subject grades output against - see ping_echo.c:print_echo() and
/// ping.c:ping_finish() in the vendored copy under inetutils-2.0/.

/// @brief The opening line: "PING host (1.2.3.4): 56 data bytes".
///
/// Prints the host exactly as the user typed it alongside what it resolved
/// to, so a surprising resolution is visible immediately rather than after
/// the first reply.
///
/// With -v the ICMP identifier is appended, matching the reference:
///   PING 8.8.8.8 (8.8.8.8): 56 data bytes, id 0x1f4e = 8014
/// It is printed in both hex and decimal because hex is what you read off a
/// packet capture and decimal is what `ps` shows you for the PID.
///
/// @param ctx Host text, resolved address, payload length and flags.
void	print_ping_banner(const t_ping_ctx *ctx)
{
	unsigned int	id;

	printf("PING %s (%s): %zu data bytes", ctx->flags.host, ctx->ipstr,
		ctx->payload_len);
	if (ctx->flags.verbose)
	{
		/* ctx->id is stored in network order for raw comparison against
		   inbound packets; convert back to print it the way a human and a
		   packet capture would read it. */
		id = ntohs(ctx->id);
		printf(", id 0x%04x = %u", id, id);
	}
	printf("\n");
}

/// @brief Percentage of probes that never came back, truncated.
///
/// Integer arithmetic, matching ping.c:ping_finish() in the reference:
///
///     (int) (((ping_num_xmit - ping_num_recv) * 100) / ping_num_xmit)
///
/// Truncation is not a rounding preference, it is the observable behaviour
/// being copied: lose 2 of 3 packets and the reference prints 66, while a
/// float formatted with %.0f rounds to 67.
///
/// @param stats Final counters.
/// @return Loss percentage, 0 when nothing was sent.
static int	loss_percent(const t_ping_stats *stats)
{
	if (stats->n_sent <= 0)
		return (0);
	return ((stats->n_sent - stats->n_recv) * 100 / stats->n_sent);
}

/// @brief The counts line: transmitted, received, and loss.
///
/// More replies than probes is impossible on an honest network, so the
/// reference treats it as evidence of forged traffic rather than printing a
/// negative percentage. Reproduced here for the same reason - it is a real
/// diagnostic, and a nonsense number would be a worse one.
///
/// @param stats Final counters.
static void	print_counts(const t_ping_stats *stats)
{
	printf("%d packets transmitted, %d packets received, ",
		stats->n_sent, stats->n_recv);
	if (stats->n_sent > 0 && stats->n_recv > stats->n_sent)
		printf("-- somebody is printing forged packets!");
	else
		printf("%d%% packet loss", loss_percent(stats));
	printf("\n");
}

/// @brief The closing summary: counts, loss percentage and RTT figures.
///
/// Both degenerate cases are handled without dividing by zero:
///   n_sent == 0  Ctrl+C arrived before the first send completed;
///   n_recv == 0  every reply was lost - the loss line still prints, the
///                RTT line is simply omitted.
///
/// No leading newline before the header: ping_finish() in the reference has
/// none either. Interactively you cannot tell, because the terminal echoes
/// "^C" in that position - but piped to a file the difference is visible,
/// and that is how output gets diffed.
///
/// The deviation is derived from the running sums rather than from a stored
/// list of samples: variance = E[x^2] - E[x]^2 needs only sum and sum_sq, so
/// the memory cost is constant however long the run lasts.
///
/// The label is "stddev", not "mdev": inetutils and the BSDs print stddev,
/// Linux's iputils prints mdev, and inetutils is the graded reference. The
/// number is identical either way.
///
/// @param host  Host text exactly as the user typed it.
/// @param stats Final counters and accumulators.
void	print_stats(const char *host, const t_ping_stats *stats)
{
	double	mean;
	double	variance;

	printf("--- %s ping statistics ---\n", host);
	print_counts(stats);
	if (stats->n_recv == 0)
		return ;
	mean = stats->sum / stats->n_recv;
	variance = stats->sum_sq / stats->n_recv - mean * mean;
	/* Floating-point noise can push a mathematically-zero variance
	   slightly negative, and sqrt() of that is NaN. */
	if (variance < 0.0)
		variance = 0.0;
	printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
		stats->rtt_min, mean, stats->rtt_max, sqrt(variance));
}
