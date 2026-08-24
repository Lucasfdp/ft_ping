#include "ft_ping.h"

/// @file ping_error.c
/// @brief The unrecoverable-error exit.
///
/// Deliberately not used on the normal paths: anything that happens after
/// the context is built has buffers and a socket to release, so those
/// paths return an error code up to main() instead of exiting here.

/// @brief Prints a message to stderr and exits the process with status 1.
///
/// dprintf() on fd 2 rather than fprintf(stderr): no buffering to flush,
/// so the message is on screen even if we are dying with a corrupted
/// stdio state.
///
/// @param msg Text to print; a newline is appended.
void	fatal_error(char *msg)
{
	dprintf(2, "%s\n", msg);
	exit(1);
}
