#include "ft_ping.h"
#include <signal.h>   /* sigaction(), sig_atomic_t, SIGINT */

/// @file ping_signal.c
/// @brief The stop flag, and the only handler that sets it.
///
/// The flag is file-static and reached only through the two accessors
/// below, so nothing outside this file can set it by accident and the
/// handler stays as small as a signal handler must be.

/// Set from signal context, read from the loop - hence both qualifiers:
/// volatile so the compiler re-reads it every iteration, sig_atomic_t so
/// the write cannot be observed half-done.
static volatile sig_atomic_t	g_stop = 0;

/// @brief The SIGINT handler. Records the request and returns immediately.
///
/// Everything else - printing the summary, closing the socket, freeing the
/// buffers - happens back in the loop, because none of it is safe to do
/// from signal context.
///
/// @param sig The delivered signal number, unused.
static void	on_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

/// @brief Installs the SIGINT handler that ends the run.
///
/// sa_flags is deliberately left at 0, i.e. NO SA_RESTART: a poll() that
/// is blocked when Ctrl+C arrives must return EINTR so the loop can notice
/// the stop request at once, instead of transparently resuming its wait
/// and only reacting up to a full interval later.
///
/// @return 0 on success, -1 on failure (message already printed).
int	signal_install(void)
{
	struct sigaction	sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sigint;
	if (sigaction(SIGINT, &sa, NULL) == -1)
		return (perror("sigaction"), -1);
	return (0);
}

/// @brief Has a stop been requested (SIGINT, or a fatal send error)?
/// @return Non-zero once the run should end.
int	ping_should_stop(void)
{
	return (g_stop != 0);
}

/// @brief Requests that the run end at the next opportunity.
///
/// Used by the send path as well as the handler, so a failed sendto()
/// unwinds through exactly the same route as Ctrl+C does.
void	ping_request_stop(void)
{
	g_stop = 1;
}
