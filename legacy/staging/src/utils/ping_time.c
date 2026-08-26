#include "ft_ping.h"
#include <time.h>   /* clock_gettime(), CLOCK_MONOTONIC */

/// @file ping_time.c
/// @brief The three clock helpers the whole program schedules against.
///
/// One unit throughout: milliseconds as a double. Interval, deadline and
/// poll timeout are all the same kind of quantity, so nothing here has to
/// do the timeval borrow/carry normalisation the reference implementation
/// does by hand.
///
/// CLOCK_MONOTONIC, never CLOCK_REALTIME: an NTP step or a daylight-saving
/// change must not be able to move a deadline.

/// @brief Difference between two timespecs, in milliseconds.
/// @param start Earlier reading.
/// @param end   Later reading.
/// @return end - start, in ms.
double	elapsed_ms(const struct timespec *start, const struct timespec *end)
{
	int64_t	start_ns;
	int64_t	end_ns;

	/* Collapse both readings to a single integer nanosecond count first:
	   subtracting sec and nsec separately would need a manual borrow when
	   end->tv_nsec < start->tv_nsec. */
	start_ns = (int64_t)start->tv_sec * 1000000000LL + start->tv_nsec;
	end_ns = (int64_t)end->tv_sec * 1000000000LL + end->tv_nsec;
	return ((double)(end_ns - start_ns) / 1000000.0);
}

/// @brief One monotonic clock reading, expressed in milliseconds.
/// @return Milliseconds since an unspecified fixed point.
double	now_ms(void)
{
	struct timespec	ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0);
}

/// @brief How long to block in poll(), in the int milliseconds it wants.
///
/// Clamped at both ends, and rounded UP rather than truncated - each of
/// those three details fixes a specific failure:
///   - a target already in the past becomes 0 (poll returns immediately)
///     rather than a negative value, which poll() reads as "block forever";
///   - the upper clamp keeps the cast to int safe when the target is
///     INFINITY, i.e. when no -t deadline was given;
///   - ceil(), because 0.4 ms left must round up to 1 ms. Truncating it to
///     0 makes poll() return instantly with the target still in the future,
///     and the loop spins on that sub-millisecond remainder.
///
/// @param target Absolute instant we want to wake at, in now_ms() units.
/// @param now    Current instant, from now_ms().
/// @return Milliseconds to wait, in [0, PING_POLL_MAX_MS].
int	ms_until(double target, double now)
{
	double	d;

	d = target - now;
	if (d < 0.0)
		return (0);
	if (d > PING_POLL_MAX_MS)
		return ((int)PING_POLL_MAX_MS);
	return ((int)ceil(d));
}
