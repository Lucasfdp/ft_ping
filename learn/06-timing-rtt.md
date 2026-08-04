# Stage 6 — Timing & RTT

**Prerequisite:** [05-receiving-parsing.md](05-receiving-parsing.md) · **Next:** [07-signals-stats.md](07-signals-stats.md)

---

**Plain version:** record a timestamp when you send, record another when the reply arrives, and
diff them.

**Where to put the send timestamp:** commonly embedded directly in the ICMP payload — so on the
receiving side you pull it back out and diff it against "now," without needing to separately track
timestamps per sequence number in a lookup table (though that's also valid).

**Precision:** `clock_gettime(CLOCK_MONOTONIC, ...)` is generally preferred over `gettimeofday()`
for measuring elapsed time, since it isn't affected by wall-clock adjustments (NTP sync, manual
clock changes) mid-measurement.

**Tolerance:** the subject allows ±30ms slack on the RTT line versus the reference implementation
— don't over-engineer beyond what `clock_gettime` naturally gives you.

---

## Check questions

<details><summary>Q1: Why is CLOCK_MONOTONIC generally better than wall-clock time for RTT?</summary>

Wall-clock time can jump forward or backward due to NTP sync or manual adjustment while your
program runs, corrupting an elapsed-time calculation. CLOCK_MONOTONIC only ever moves forward at a
steady rate, making it safe for measuring durations.
</details>

<details><summary>Q2: Why embed the timestamp in the payload rather than a local variable?</summary>

Multiple pings can be in flight at once (especially in flood mode). Tying the timestamp to the
specific packet means that when its reply comes back, you can compute RTT for that exact packet
without a separate lookup table keyed by sequence number.
</details>

---

## Exercise

1. Write `double elapsed_ms(const struct timespec *start, const struct timespec *end)`. Handle the
   nanosecond borrow correctly (`tv_nsec` can go negative — normalise it). Unit-test it with
   hand-built timespecs: 1s exactly, 999999999ns, and a pair that requires the borrow.
2. Extract the timeout/interval magic numbers now, before they multiply:
   ```c
   #define PING_INTERVAL_SEC   1
   #define PING_PAYLOAD_SIZE  56
   ```
   Any bare number in your timing code is a maintenance bug waiting to happen.
3. `memcpy` a `struct timespec` into the first bytes of the payload before checksumming. On
   receive, `memcpy` it back out (don't cast the buffer pointer — alignment). Diff against now.
4. **Sanity-check against reality:** ping localhost — RTT should be well under 1ms. Ping a host
   ~100ms away and compare your number against system `ping` running simultaneously. You're
   allowed ±30ms; if you're off by 10x, you've mixed up ns/µs/ms.
5. **Trust boundary:** the payload comes back off the network. A hostile or broken responder can
   return garbage in those bytes, yielding an absurd RTT. Decide what you do — real ping just
   prints it, but you should *know* that's the choice you made.

**Done when:** localhost RTT is sub-millisecond, a remote host's RTT is within 30ms of system ping,
and the borrow test passes.

---

## Reading

- `man 2 clock_gettime` — `CLOCK_MONOTONIC` vs `CLOCK_REALTIME` vs `CLOCK_MONOTONIC_RAW`
- `man 7 time` — the conceptual difference between the clocks; short and worth it
- `man 2 gettimeofday` — read the NOTES explaining why it's discouraged for intervals
- inetutils-2.0 `ping/ping_common.c` — see how the reference implementation stores and diffs its
  timestamp; you're graded against its output
