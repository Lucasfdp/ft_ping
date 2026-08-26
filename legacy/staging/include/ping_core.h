#pragma once

/// @file ping_core.h
/// @brief Program lifetime: build the context, install the signal
///        handler, run the send/receive loop, tear the context down.
///
/// This is the layer main() is written against. It owns the ordering of
/// the run; the cli, net and output modules own the individual steps.

#include <stdint.h>
#include "ping_types.h"

/// @brief The send schedule, the only state the event loop mutates.
///
/// All four fields are millisecond quantities in now_ms()'s units (or a
/// counter), so the loop never has to normalise a timeval by hand.
typedef struct s_sched
{
	double		interval;	///< ms between paced sends (-i, or -f's floor)
	double		deadline;	///< absolute instant -t expires, INFINITY if unset
	double		last_send;	///< when the previous packet actually went out
	uint16_t	seq;		///< sequence number for the NEXT probe
}	t_sched;

/* ---- src/core/ping_context.c -------------------------------------- */

/// @brief Turns argv into a ready-to-use context.
///
/// Parses the command line, resolves the host, derives every size from
/// -s, and allocates the three buffers the context owns. Either the
/// context comes back complete or nothing is left allocated.
///
/// @param ctx Uninitialised struct; zeroed here before anything else.
/// @param ac  argc as received by main().
/// @param av  argv as received by main().
/// @return 0 on success, -1 on failure (message already printed).
int		ctx_init(t_ping_ctx *ctx, int ac, char **av);

/// @brief Frees every heap buffer the context owns.
///
/// One place for all three allocations, so every exit path in main()
/// releases them the same way. free(NULL) is a no-op, so this is safe
/// even when only some of the three were ever allocated.
///
/// @param ctx Context to release; the struct itself is not freed.
void	ctx_destroy(t_ping_ctx *ctx);

/* ---- src/core/ping_signal.c --------------------------------------- */

/// @brief Installs the SIGINT handler that ends the run.
///
/// Deliberately does NOT set SA_RESTART: a blocked poll() must return
/// EINTR on Ctrl+C so the loop can notice the stop request immediately
/// instead of finishing its current wait.
///
/// @return 0 on success, -1 on failure (message already printed).
int		signal_install(void);

/// @brief Has a stop been requested (SIGINT, or a fatal send error)?
/// @return Non-zero once the run should end.
int		ping_should_stop(void);

/// @brief Requests that the run end at the next opportunity.
///
/// Used by the send path as well as the signal handler, so a failed
/// sendto() unwinds through exactly the same route as Ctrl+C.
void	ping_request_stop(void);

/* ---- src/core/ping_loop.c ----------------------------------------- */

/// @brief Runs the whole ping session: preload burst, then paced sends
///        interleaved with reply collection, until a stop condition hits.
///
/// Returns normally on every stop condition (Ctrl+C, -t deadline, -o
/// first reply, poll/send/recv failure); @p stats is complete and ready
/// to print either way.
///
/// @param sockfd Raw ICMP socket from socket_open().
/// @param ctx    Fully built context.
/// @param stats  Zeroed here, then filled as the run proceeds.
void	ping_run(int sockfd, t_ping_ctx *ctx, t_ping_stats *stats);
