#include "ft_ping.h"
#include <poll.h>   /* poll(), struct pollfd, POLLIN */

/// @file ping_loop.c
/// @brief The event loop: sends on a clock, receives on an event.
///
/// The central idea of the whole program is that those two are
/// INDEPENDENT. A send never waits for a reply, and a reply is collected
/// whenever it turns up. poll() blocks until whichever comes first - a
/// packet arrives, the next send falls due, or the -t deadline expires -
/// so the loop never busy-waits and never sleeps through an event.

/// @brief Fixes the pacing parameters for the whole run.
///
/// -f overrides -i rather than combining with it (the two are rejected
/// together at parse time anyway) and -t is converted once, here, into a
/// single absolute instant instead of being re-derived every iteration.
///
/// @param sched Schedule to initialise; seq starts at 0, as real ping does.
/// @param f     Parsed options.
static void	sched_init(t_sched *sched, const t_flags *f)
{
	sched->interval = 1000.0 * PING_INTERVAL_SEC;
	if (f->has_interval)
		sched->interval = 1000.0 * f->interval;
	if (f->flood)
		sched->interval = PING_FLOOD_INTERVAL_MS;	/* -f: the 100/s floor */
	sched->deadline = INFINITY;
	if (f->has_timeout)
		sched->deadline = now_ms() + 1000.0 * f->timeout;
	sched->last_send = 0.0;
	sched->seq = 0;
}

/// @brief -l: the preload burst, sent unpaced before normal service.
///
/// Deliberately ignores the schedule: the point of -l is to put N packets
/// on the wire as fast as the kernel accepts them. A send failure here
/// stops the run the same way Ctrl+C would.
///
/// @param sockfd Raw ICMP socket.
/// @param ctx    Fully built context.
/// @param sched  seq is advanced once per packet.
/// @param stats  n_sent is updated per packet.
static void	send_preload(int sockfd, t_ping_ctx *ctx, t_sched *sched,
		t_ping_stats *stats)
{
	int	i;

	i = 0;
	while (i < ctx->flags.preload && !ping_should_stop())
	{
		if (send_one(sockfd, ctx, sched->seq++, stats) == -1)
			ping_request_stop();
		i++;
	}
}

/// @brief Sends the next packet if - and only if - the clock says it is due.
///
/// Sending is decided by the CLOCK, never by poll()'s return value. poll()
/// returning 0 only means "nothing arrived in that window"; it can expire
/// against the -t deadline rather than against the send schedule, and
/// treating that as "send now" turns the last fraction of a millisecond
/// before the deadline into a flood.
///
/// @param sockfd Raw ICMP socket.
/// @param ctx    Fully built context.
/// @param sched  Read for the schedule, updated on an actual send.
/// @param stats  n_sent is updated on an actual send.
/// @return 0 to keep looping, -1 if the run should end (deadline reached
///         or the send failed).
static int	send_if_due(int sockfd, t_ping_ctx *ctx, t_sched *sched,
		t_ping_stats *stats)
{
	double	now;

	now = now_ms();
	if (now >= sched->deadline)						/* -t reached */
		return (-1);
	if (now < sched->last_send + sched->interval)	/* not due yet */
		return (0);
	if (send_one(sockfd, ctx, sched->seq++, stats) == -1)
		return (-1);
	/* Re-read the clock rather than reusing `now`: sendto() itself took
	   time, and pacing from before the call would drift the interval
	   short by however long the syscall lasted. */
	sched->last_send = now_ms();
	return (0);
}

/// @brief --bsd-flood: sends one packet per reply just collected.
///
/// This is the half of the BSD man page's flood description that the timer
/// alone does not provide - *"as fast as they come back"*, on top of the
/// 100/s floor. It is opt-in because inetutils-2.0, which the subject grades
/// against, does NOT do it: in its ping_run() the readable branch receives
/// and the timeout branch sends, and the two are mutually exclusive.
///
/// The effect is that the rate becomes bound by the round-trip time rather
/// than by the host's timer resolution. On loopback that is tens of
/// microseconds instead of 10 ms, so it also removes this program's
/// sensitivity to a coarse timer inside a VM.
///
/// It cannot run away: exactly one packet is sent per reply received, so the
/// number in flight stays bounded by what the network gives back. If replies
/// stop, this sends nothing and the 10 ms floor in send_if_due() takes over -
/// which is precisely the behaviour the man page describes.
///
/// last_send is updated, so the floor measures from the last ACTUAL
/// transmission. A rate above 100/s simply means the timer never fires.
///
/// CAVEAT worth knowing before using this against a real host: the send
/// timestamps live in a ring of 65536 slots indexed by sequence number, so a
/// reply is only matched correctly if it arrives before its slot is reused.
/// At the timer's 100/s that window is about eleven minutes. At the rates
/// this flag reaches on loopback - hundreds of thousands per second - it
/// collapses to a fraction of a second, and a reply delayed beyond it would
/// be timed against the wrong send. Loopback replies are immediate so it does
/// not arise there, but it is the reason this is opt-in and not the default.
///
/// @param sockfd  Raw ICMP socket.
/// @param ctx     Fully built context.
/// @param sched   seq and last_send are advanced per packet.
/// @param stats   n_sent is updated per packet.
/// @param replies How many replies the drain just recorded.
/// @return 0 to keep looping, -1 if the run should end.
static int	send_per_reply(int sockfd, t_ping_ctx *ctx, t_sched *sched,
		t_ping_stats *stats, int replies)
{
	while (replies > 0 && !ping_should_stop())
	{
		if (now_ms() >= sched->deadline)			/* -t reached mid-burst */
			return (-1);
		if (send_one(sockfd, ctx, sched->seq++, stats) == -1)
			return (-1);
		sched->last_send = now_ms();
		replies--;
	}
	return (0);
}

/// @brief One pass of the loop: wait, then collect, then maybe send.
///
/// @param sockfd Raw ICMP socket.
/// @param ctx    Fully built context.
/// @param sched  Current schedule.
/// @param stats  Counters, updated by both halves.
/// @return 0 to keep looping, -1 if the run should end.
static int	loop_once(int sockfd, t_ping_ctx *ctx, t_sched *sched,
		t_ping_stats *stats)
{
	struct pollfd	pfd;
	double			now;
	int				rc;
	int				before;

	now = now_ms();
	if (now >= sched->deadline)						/* -t reached */
		return (-1);
	pfd.fd = sockfd;
	pfd.events = POLLIN;				/* the only event we care about */
	/* Wake for whichever comes first: a packet, the next send, or -t. */
	rc = poll(&pfd, 1, ms_until(fmin(sched->last_send + sched->interval,
					sched->deadline), now));
	if (rc < 0)
	{
		if (errno == EINTR)				/* SIGINT landed while blocked here */
			return (0);					/* -> re-test the stop flag above */
		return (perror("poll"), -1);
	}
	if (rc > 0)							/* readable: drain everything queued */
	{
		/* How many replies the drain recorded, taken as a delta rather than
		   returned by ping_receive(): the count is only of interest to
		   --bsd-flood, and the network layer should not grow a parameter for
		   a pacing decision that belongs to the loop. */
		before = stats->n_recv;
		if (ping_receive(sockfd, ctx, stats) == -1)
			return (-1);
		if (ctx->flags.exit_on_reply && stats->n_recv > 0)	/* -o */
			return (-1);
		if (ctx->flags.bsd_flood && send_per_reply(sockfd, ctx, sched, stats,
				stats->n_recv - before) == -1)
			return (-1);
	}
	return (send_if_due(sockfd, ctx, sched, stats));
}

/// @brief Runs the whole ping session.
///
/// The first paced packet goes out before the loop starts, so the loop
/// body always has a last_send to schedule the next one from and never has
/// to special-case "nothing sent yet".
///
/// Returns normally on every stop condition (Ctrl+C, -t deadline, -o first
/// reply, poll/send/recv failure); @p stats is complete and ready to print
/// either way.
///
/// @param sockfd Raw ICMP socket from socket_open().
/// @param ctx    Fully built context.
/// @param stats  Zeroed here, then filled as the run proceeds.
void	ping_run(int sockfd, t_ping_ctx *ctx, t_ping_stats *stats)
{
	t_sched	sched;

	memset(stats, 0, sizeof *stats);
	sched_init(&sched, &ctx->flags);
	send_preload(sockfd, ctx, &sched, stats);
	if (!ping_should_stop()
		&& send_one(sockfd, ctx, sched.seq++, stats) == -1)
		ping_request_stop();
	sched.last_send = now_ms();
	while (!ping_should_stop())
	{
		if (loop_once(sockfd, ctx, &sched, stats) == -1)
			break ;
	}
}
