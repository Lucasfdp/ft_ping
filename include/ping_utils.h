#pragma once

/// @file ping_utils.h
/// @brief Small helpers with no knowledge of ICMP: clocks and fatal exits.

#include <time.h>

/* ---- src/utils/ping_time.c ---------------------------------------- */

/// @brief Difference between two timespecs, in milliseconds.
/// @param start Earlier reading.
/// @param end   Later reading.
/// @return end - start, in ms, fractional part preserved.
double	elapsed_ms(const struct timespec *start, const struct timespec *end);

/// @brief One monotonic clock reading, expressed in milliseconds.
/// @return Milliseconds since an unspecified fixed point; only differences
///         between two calls are meaningful.
double	now_ms(void);

/// @brief How long to block in poll(), in the int milliseconds it wants.
/// @param target Absolute instant we want to wake at, in now_ms() units.
/// @param now    Current instant, from now_ms().
/// @return Clamped, rounded-up millisecond count, always >= 0.
int		ms_until(double target, double now);

/* ---- src/utils/ping_error.c --------------------------------------- */

/// @brief Prints a message to stderr and exits the process with status 1.
/// @param msg Text to print; a newline is appended.
void	fatal_error(char *msg);
