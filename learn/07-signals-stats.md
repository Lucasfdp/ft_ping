# Stage 7 — Signal Handling & Statistics

**Prerequisite:** [06-timing-rtt.md](06-timing-rtt.md) · **Next:** [08-cli-errors.md](08-cli-errors.md)

---

**Plain version:** real ping runs until you interrupt it (Ctrl+C), then prints a summary instead
of just dying.

**`sigaction()` vs `signal()`:** `sigaction` is the more robust, portable way to install a handler
— it lets you control flags precisely (e.g. whether an interrupted syscall restarts or not).

**Async-signal-safety:** don't call non-reentrant functions (`printf`, `malloc`, …) from inside a
signal handler. The standard pattern: set a `volatile sig_atomic_t` flag in the handler, and check
/ act on it from your main loop.

**Stats to accumulate as you go:** packets transmitted, packets received, and every individual RTT
value (needed to compute min/avg/max and **mdev** — mean deviation — at the end).

> **Names, expanded** — `SIGINT` = **SIG**nal: **INT**errupt (what Ctrl+C sends) ·
> `sig_atomic_t` = a **sig**nal-safe **atomic** **t**ype, one that can't be caught half-written ·
> `SA_RESTART` = **S**ig**a**ction flag: **RESTART** the interrupted syscall ·
> `EINTR` = **E**rror: **INTR**errupted · *reentrant* = safe to call again while an earlier call is
> still running, which is what a signal handler needs.

---

## Check questions

<details><summary>Q1: Why shouldn't you call printf() directly inside your SIGINT handler?</summary>

Signal handlers can interrupt your program at any point, including mid-way through a non-reentrant
function call like an in-progress printf. Calling printf from the handler risks corrupting shared
state (like stdio's internal buffers). The safe pattern is to set a flag and let normal program
flow do the actual printing.
</details>

<details><summary>Q2: What's the difference between "average RTT" and "mdev" in ping's summary?</summary>

Average is the mean of all round-trip times. Mdev measures how much individual RTTs vary from that
average — low mdev means consistent latency, high mdev means jittery latency even if the average
looks fine.
</details>

---

## Exercise

1. Install SIGINT with `sigaction`. The handler body is **exactly one line**:
   ```c
   static volatile sig_atomic_t g_stop = 0;
   static void on_sigint(int sig) { (void)sig; g_stop = 1; }
   ```
   Nothing else. No printf, no malloc, no free.
2. **`SA_RESTART` is a real decision, not a default.** Without it, a blocking `recvfrom()` returns
   `-1`/`EINTR` when the signal lands — which is how you break out of the loop promptly. With it,
   the syscall restarts and you stay blocked. Pick deliberately, and either way your `recvfrom`
   error handling must treat `EINTR` as "not a real error" rather than exiting.
3. Accumulate stats **incrementally** — running sum and sum-of-squares, or a growable array. Don't
   assume a fixed max packet count; ping can run for days.
   ```
   mdev = sqrt(sum_sq/n - (sum/n)^2)
   ```
   Guard against `n == 0` before dividing, and clamp a tiny negative under the sqrt to 0 (floating
   point can produce it).
4. **Test the zero-reply case:** ping an unreachable address, hit Ctrl+C immediately. You must
   print `0 packets received, 100% packet loss` and **no** min/avg/max line — not `nan` or a
   divide-by-zero crash. This is the case that segfaults naive implementations.
5. Test: Ctrl+C after 1 packet, after 10, and while a reply is in flight.

**Done when:** Ctrl+C always prints a complete summary and exits cleanly, including with zero
replies received.

---

## Reading

- `man 2 sigaction` — the whole page; especially `SA_RESTART` and the `sa_mask` field
- `man 7 signal-safety` — **the** authoritative list of async-signal-safe functions. Read the list
  and note that `printf` is not on it
- `man 7 signal` — which syscalls are interruptible, and `EINTR` semantics
- `man 2 signal` — read the "Portability" section explaining why `sigaction` is preferred
- inetutils-2.0 `ping/ping.c` — its `print_stats` / final summary function, for exact wording
