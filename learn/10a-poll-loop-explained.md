# The poll() loop, line by line

Companion to `10-bonus-implementation-plan.md`. This explains the Phase 0 rewrite that was
just applied to `main.c`, `ft_rec_resp.c` and `ft_ping.h` — what each piece does and, more
importantly, why it has to be that way.

---

## 1. What was actually wrong

The old loop:

```c
while (!g_stop)
{
    build packet with sequence = seq
    sendto()
    stats.n_sent++
    ft_rec_resp(sockfd, pkt.id, pkt.sequence, &stats)   /* blocks until THIS reply */
    seq++
    sleep(1)
}
```

It reads cleanly, and it encodes three assumptions that are all false:

| Assumption | Reality |
|---|---|
| Every request gets a reply | Packets are lost. `ft_rec_resp` then blocks in `recvfrom()` with nothing to return to, forever. |
| The reply arrives before the next send is due | On a slow link the RTT exceeds the interval. Ping is supposed to keep sending anyway. |
| Replies arrive in order, one per request | Duplicates, reordering, and late replies all happen. The old code discarded anything whose sequence wasn't the one it was waiting for. |

The first one is the killer: `./ft_ping 240.0.0.1` sent exactly one packet and then hung with
no output. `n_sent` stayed at 1, so "100% packet loss" was arithmetically unreachable.

The fix is to stop treating "send" and "receive" as two halves of one step. They become two
independent events on one timeline, and the loop's only job is to decide which one is next.

---

## 2. `now_ms()` — one clock, one unit

```c
static double	now_ms(void)
{
	struct timespec	ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0);
}
```

**Why `CLOCK_MONOTONIC` and not `CLOCK_REALTIME`.** Realtime is wall-clock: NTP can step it
backwards, and a timezone or DST change moves it. A deadline computed against it can silently
jump into the past or the future. Monotonic only ever counts forward from an arbitrary origin
(boot, usually). You can't format it as a date — which is fine, we only ever subtract two
readings.

**Why a `double` of milliseconds.** Everything the loop reasons about is a duration: the
interval, the deadline, the poll timeout. `poll()` wants `int` milliseconds. If you keep
`struct timeval`s around you end up doing this, which is straight out of
`inetutils-2.0/ping/ping.c:428`:

```c
resp_time.tv_usec = last.tv_usec + intvl.tv_usec - now.tv_usec;
while (resp_time.tv_usec < 0) { resp_time.tv_usec += 1000000; resp_time.tv_sec--; }
while (resp_time.tv_usec >= 1000000) { resp_time.tv_usec -= 1000000; resp_time.tv_sec++; }
```

That is manual borrow-and-carry normalisation, and it is where sign bugs live. A double
holding milliseconds does the same arithmetic with `+` and `-`. A `double` has 53 bits of
mantissa, so it represents whole milliseconds exactly well past any uptime you'll ever see —
precision is not a concern here.

Your existing `elapsed_ms()` stays as-is: it works on the two `struct timespec`s used for RTT
measurement, which is a different job.

---

## 3. `ms_until()` — converting a target instant into a poll timeout

```c
static int	ms_until(double target, double now)
{
	double	d;

	d = target - now;
	if (d < 0.0)
		return (0);
	if (d > 3600000.0)
		return (3600000);
	return ((int)ceil(d));
}
```

Three guards, each protecting against a specific `poll()` behaviour:

**`d < 0.0 → 0`.** `poll()` interprets a *negative* timeout as "block forever". If the target
is already in the past and you passed the negative difference through, poll would sleep
indefinitely — the exact hang you're removing. Zero means "return immediately", which is what
"already due" should mean.

**`d > 3600000.0 → 3600000`.** With no `-t`, the deadline is `INFINITY`. `(int)INFINITY` is
undefined behaviour in C — on x86 it produces `INT_MIN`, which is negative, which is
block-forever again. The clamp keeps the cast defined. One hour is arbitrary; anything sane
works, because a timeout expiring early is harmless (the loop just re-evaluates).

**`ceil()` and not truncation.** This one cost me a debugging pass, and it's worth
understanding because it's the subtle one.

With a plain `(int)d` cast, `0.4` ms becomes `0`. `poll(…, 0)` returns immediately with
nothing ready. The loop then re-reads the clock, finds the target *still* 0.39 ms in the
future, computes `0` again, and spins. In the first version of this loop that spin also fired
a `sendto()` on each pass, so `./ft_ping -i 0.2 -t 1 127.0.0.1` transmitted **158 packets in
one second** instead of five — and non-deterministically, because it depended on how much
sub-millisecond remainder was left when the deadline was reached.

`ceil()` rounds a partial millisecond up to a whole one, so poll always makes forward
progress. Rounding up means you can overshoot a target by up to 1 ms; that is the correct
trade, because overshooting costs a millisecond of accuracy while undershooting costs a busy
loop.

---

## 4. `send_one()` — the extracted send half

The body is your old loop's first half, unchanged apart from three things:

```c
static int	send_one(int sockfd, struct sockaddr_in *dst, uint16_t seq,
	const t_flags *flags, t_ping_stats *stats)
```

1. **It takes `seq` as a parameter** instead of reading a loop variable. Callers pass `seq++`,
   so the sequence number advances **per transmission**. In the old loop `seq++` only ran
   after a reply was matched — meaning one lost packet froze the sequence number and every
   subsequent packet went out with a duplicate. With sends and receives decoupled that would
   have been fatal: you'd have no way to tell replies apart.
2. **It returns instead of waiting.** That's the whole point of the split.
3. **It prints the `-f` dot.** `putchar('.')` then `fflush(stdout)` — the flush matters
   because stdout is line-buffered on a terminal but *block*-buffered through a pipe, and a
   dot is not a newline. Without the flush, `./ft_ping -f host | cat` shows nothing for 4 KB.
   `inetutils` sidesteps this globally with `setvbuf(stdout, NULL, _IOLBF, 0)` at
   `ping.c:299`; an explicit flush is equivalent here and more local.

---

## 5. Pacing setup — two numbers computed once

```c
interval = 1000.0 * (flags.has_interval ? flags.interval : PING_INTERVAL_SEC);
if (flags.flood)
	interval = PING_FLOOD_INTERVAL_MS;      /* 10.0 */
deadline = INFINITY;
if (flags.has_timeout)
	deadline = now_ms() + 1000.0 * flags.timeout;
```

**`interval` is a duration; `deadline` is an instant.** Keeping that distinction straight is
most of the loop's correctness. `interval` is relative and constant. `deadline` is absolute
and computed once — if you recomputed it each iteration from "time remaining" it would drift.

**`-f` is just an interval of 10 ms.** The man page says "as fast as they come back or one
hundred times per second, whichever is more", which sounds like reply-driven pacing. Look at
what the reference actually implements (`ping.c:411`):

```c
if (options & OPT_FLOOD) { intvl.tv_sec = 0; intvl.tv_usec = 10000; }
```

It only implements the 100/second floor. Since you're diffed against `ping-ref`, match what
it does, not what the man page describes.

**`INFINITY` as "no deadline"** removes a branch. `fmin(next_send, INFINITY)` is `next_send`,
and `now >= INFINITY` is always false, so the no-`-t` case needs no special casing anywhere.
It's from `<math.h>`, already included for `sqrt`.

---

## 6. The preload burst and the priming send

```c
i = 0;
while (i < flags.preload && !g_stop)
{
	if (send_one(sockfd, &dst, seq++, &flags, &stats) == -1)
		g_stop = 1;
	i++;
}
if (!g_stop && send_one(sockfd, &dst, seq++, &flags, &stats) == -1)
	g_stop = 1;
last_send = now_ms();
```

**`-l` is literally "call send_one N times with no waiting".** That's it — and it's only
expressible now, because in the old loop a send was inseparable from waiting for its reply.
This is the clearest demonstration of why Phase 0 had to come first.

**Why one send happens before the loop.** The loop schedules the *next* send as
`last_send + interval`, so `last_send` needs a value. Two ways to seed it:

- Send once up front and set `last_send = now_ms()` — what's here, and what the reference
  does (`ping.c:419`).
- Set `last_send = -INFINITY` and let the loop's first pass find a send immediately due.

The first is slightly more code but keeps `last_send` meaning exactly "when the last packet
actually went out", with no sentinel value to reason about. `-l` also has to precede it
regardless, so there's already pre-loop send code.

**On error we set `g_stop` rather than returning.** Falling through to `print_stats()` means
even a failed run prints a summary, which is what ping does.

---

## 7. The loop

```c
pfd.fd = sockfd;
pfd.events = POLLIN;
while (!g_stop)
{
	now = now_ms();
	if (now >= deadline)
		break ;
	next_send = last_send + interval;

	rc = poll(&pfd, 1, ms_until(fmin(next_send, deadline), now));

	if (rc < 0)
	{
		if (errno == EINTR)
			continue ;
		perror("poll");
		break ;
	}
	if (rc > 0)
	{
		if (ft_rec_resp(sockfd, pkt_id, &flags, &stats) == -1)
			break ;
		if (flags.exit_on_reply && stats.n_recv > 0)
			break ;
	}
	now = now_ms();
	if (now >= deadline)
		break ;
	if (now >= next_send)
	{
		if (send_one(sockfd, &dst, seq++, &flags, &stats) == -1)
			break ;
		last_send = now_ms();
	}
}
```

### `struct pollfd` and what `poll()` returns

`pfd.fd` is the socket, `pfd.events = POLLIN` means "tell me when this is readable".
(`poll()` writes what actually happened into `pfd.revents`; with a single fd and a single
event, checking the return value is enough.) The `1` is the array length — poll takes an
array of fds, we happen to have one.

Return value, which is the entire control flow of the loop:

| `rc` | Meaning | What we do |
|---|---|---|
| `> 0` | that many fds are ready — the socket has data | drain it |
| `== 0` | the timeout expired with nothing ready | nothing, *directly* — see below |
| `< 0` | error, details in `errno` | `EINTR` is not an error; anything else is |

### `fmin(next_send, deadline)` — the timeout is the nearest of two futures

The loop only ever wants to be woken for one of three reasons: a packet arrived, the next
send is due, or the run should end. The first is what `poll()` watches the fd for. The other
two are instants in time, and poll only accepts *one* timeout — so we pass the earlier of
them. Whichever fires first, we wake up in time to handle it.

This is why `poll()` is a better fit than `SO_RCVTIMEO`. A receive timeout can only express
"give up on reading after N ms". `poll()`'s timeout is a general-purpose alarm that happens to
also be watching a socket, so one call serves the interval *and* the deadline.

### `EINTR → continue`, not `break`

`poll()` returns `-1`/`EINTR` when a signal handler ran while it was blocked. That's how Ctrl+C
gets you out of a 1-second sleep promptly — your `sigaction` deliberately leaves `sa_flags`
at 0 so `SA_RESTART` is off (Stage 7, step 2).

But `EINTR` alone doesn't mean *stop*. It means "a signal happened". `g_stop` is the single
source of truth for stopping, and it's tested at the loop head — so `continue` routes back to
that test. If SIGINT was the signal, the loop exits there. If it was something else
(`SIGWINCH` from resizing your terminal, say), the loop just carries on, which is correct.
`break` here would make resizing your terminal end the ping.

### The important part: sending is decided by the clock, not by `rc == 0`

The natural way to write this is `else { send_one(); }` on the `rc == 0` branch — poll timed
out, so the send must be due. **That is wrong, and it's the bug I hit while testing this.**

`poll()` returning 0 means "the timeout I was given expired". But that timeout was
`min(next_send, deadline)`. When the deadline is the nearer of the two, poll expiring means
*the run is over*, not *a send is due*. The first version of this loop sent a packet in that
case, and because the last remaining fraction of a millisecond kept producing a 0 ms timeout
(section 3), it sent as many as the CPU allowed:

```
[dbg] last_send=1313736.613 now=1313736.613 wait=0
[dbg] last_send=1313736.630 now=1313736.630 wait=0
[dbg] last_send=1313736.636 now=1313736.636 wait=0     ← ~10 packets in 0.05 ms
```

So: re-read the clock after poll returns, and send only if `now >= next_send`. Now poll's
return value answers exactly one question — "did anything arrive?" — and the schedule answers
the other. Two independent conditions, two independent checks.

Re-reading `now` after `poll()` is required regardless: poll may have blocked for a full
interval, so the pre-poll reading is stale by definition.

### Result

```
-i 0.2 -t 1   →  5 packets     (was 30-158, non-deterministic)
-f -t 1       →  99 packets    (the 100/s floor)
-l 5 -t 2     →  7 packets     (5 burst + 2 paced)
240.0.0.1     →  4 sent, 0 received, 100% packet loss   (was: hang after 1 packet)
```

---

## 8. `ft_rec_resp()` — two changes

### `MSG_DONTWAIT`

```c
n = recvfrom(sockfd, buf, sizeof buf, MSG_DONTWAIT, NULL, NULL);
if (n < 0)
{
	if (errno == EAGAIN || errno == EWOULDBLOCK)
		return (0);         /* socket drained - normal exit */
	...
}
```

The function still loops, because one `poll()` wakeup can correspond to several queued
packets — a raw ICMP socket receives every ICMP packet the host sees, so foreign traffic gets
interleaved with your replies. The loop drains them all in one pass.

Without `MSG_DONTWAIT`, the *second* iteration blocks. Say poll wakes you for one packet that
turns out to be someone else's ping; the loop `continue`s, calls `recvfrom` again, and there's
nothing there — so it sleeps, and you're right back to the original hang. `MSG_DONTWAIT` turns
"nothing to read" into an immediate `EAGAIN`, which is the loop's exit condition.

(`EAGAIN` and `EWOULDBLOCK` are the same value on Linux and macOS, but POSIX permits them to
differ, so both are checked. That's the conventional form.)

### The sequence filter is gone

```c
if (reply->id != sent_id)
	continue;
/* the `reply->sequence != sent_seq` check is deleted */
```

The old code matched `id` *and* `sequence` because it was waiting for one specific packet.
Now sends don't wait, so by the time a reply lands, `seq` has already advanced past it —
matching on it would reject everything. `id` is `htons(getpid())`, which identifies *this
process's* traffic, and that's the right granularity now.

`pkt_id` is computed once in `main` and passed in, rather than recomputed. It's compared raw
against `reply->id` — both are already in network byte order, so neither is byte-swapped
(swapping both sides of an equality twice is harmless, but "never swap what you only compare"
is the clearer rule).

**What this costs you, and the fix that's coming.** Without the sequence match, the RTT still
comes from the timestamp inside the reply's payload, which is correct per-packet. But
duplicates now get counted as separate replies, and `-s` with a payload smaller than
`sizeof(struct timespec)` will have nowhere to put that timestamp. Both are solved by the
send-time ring in **P0.3** of the plan — a `struct timespec` array indexed by
`ntohs(reply->sequence)`, so RTT comes from a table lookup rather than the wire.

### `-q` and `-f` output

```c
if (flags->quiet)
	continue;
if (flags->flood)
{
	putchar('\b');
	fflush(stdout);
	continue;
}
```

Stats are accumulated *before* these guards — `-q` suppresses printing, not counting. The
backspace is `-f`'s other half: `send_one` prints a dot per request, a reply erases one, so
the run-length of dots on screen is the number of packets currently outstanding. That's the
whole point of flood mode's display.

---

## 9. Smaller changes in `main`

- **`av[optind]` → `flags.host`.** `parse_info` now validates `optind < ac` and stores the
  host, so `main` never touches `optind` and `./ft_ping -v` with no host is no longer a null
  dereference.
- **`print_flags()` deleted.** Debug scaffolding; it would have corrupted the Stage 9 diff.
- **Exit code.** `main` now returns `EXIT_FAILURE` when `n_recv == 0`, matching ping's
  documented behaviour (0 if anything was received, non-zero otherwise). Verify the exact
  value against `ping-ref`; `inetutils` returns the OR of per-host statuses.
- **`parse_info`'s duplicated `return (1);`** removed.

---

## 10. What this unlocks, and what's still open

Working now: `-t`, `-i` (fractional), `-l`, `-o`, `-f` (pacing + dots), `-q`.

Still to do, in plan order:

1. **Group 4's ICMP error path** — `ft_rec_resp` still `continue`s past every non-echo-reply
   (marked `TODO` in the file). Needed before `-m`, `-v`, `-Q` mean anything.
2. **P0.3 / P0.4** — send-time ring and the variable-length packet, which `-s` and `-p` need.
3. **Group 1 sockopts** — trivial once the error path exists.
4. **`-f` root check** — `geteuid() != 0` must refuse cleanly. Not yet enforced.

Re-run the Stage 9 diff with no flags before moving on.
