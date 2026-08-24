# ft_ping — Bonus Flags Implementation Plan

Handoff spec for turning the Stage 10 flag list into working code. Derived from the current
tree (`main.c`, `ft_rec_resp.c`, `parse.c`, `ft_ping.h`, `ft_checksum.c`) as of this plan.

**Parsing is already done.** `parse.c` fills `t_flags` correctly for all 14 flags. Nothing in
this plan touches `getopt`. What's missing is that `t_flags` never reaches the loop, and the
loop's current shape cannot express five of the flags.

---

## Verdict up front

Stage 10 says "the first two groups need no changes to your send/receive loop at all."
That is not true of your code, and following it as written will strand you:

- `-s` / `-p` (Group 2) change `t_icmp_packet`, which the loop's `sizeof pkt` in `sendto()`
  and `ft_checksum()` depends on. That's a loop change.
- `-l`, `-i`, `-f`, `-t` (Group 3) are impossible in the current loop, because
  `ft_rec_resp()` blocks in `recvfrom()` with no timeout. Send and receive are welded
  together one-for-one.

There is a **Phase 0** below that has to land before any flag. It is also a mandatory-part
bug fix, so it earns its keep either way.

---

## Phase 0 — prerequisites (do these first)

### P0.1 — `100% packet loss` currently hangs forever  [BLOCKER]

`ft_rec_resp()` loops on `recvfrom()` until a *matching* reply arrives. If the reply never
comes, it never returns. `ping 192.0.2.1` (reserved, never answers) hangs with no output
until Ctrl+C, and every subsequent packet is never sent — `n_sent` stays at 1, so your loss
percentage is structurally incapable of being correct.

Stage 7 asked you to test the zero-reply case. It passes only because Ctrl+C rescues it.

**Fix:** give the receive path a deadline. Two options:

| Approach | How | Trade-off |
|---|---|---|
| `SO_RCVTIMEO` sockopt | `setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv)` | 3 lines, no restructure. Timeout is per-`recvfrom` call, so a stream of foreign ICMP packets can extend the real wait past the deadline. |
| `poll()` before `recvfrom()` | `poll(&pfd, 1, ms_remaining)`, recompute `ms_remaining` each iteration from a monotonic deadline | ~15 lines. Exact deadline, and it's the same primitive `-i` / `-f` / `-t` need in Group 3. |

**Recommendation: `poll()`.** You will end up writing it for Group 3 regardless; doing it
now means one restructure instead of two.

### P0.2 — Restructure the loop into send-scheduling + receive-draining  [BLOCKER]

Target shape (pseudocode, replacing the `while (!g_stop)` body in `main.c`):

```
next_send  = now()
deadline   = flags.has_timeout ? now() + flags.timeout : NEVER
while (!g_stop) {
    if (now() >= deadline)                    break;          // -t
    if (should_send_now(&flags, next_send))   { send_one(); next_send = schedule_next(&flags); }
    wait_ms = min(next_send, deadline) - now()
    if (poll(&pfd, 1, wait_ms) > 0)           recv_one();     // non-blocking drain
    if (flags.exit_on_reply && stats.n_recv)  break;          // -o
}
```

Consequences to handle deliberately:

- **`seq++` moves to the send path.** Today it only advances on `rc == 0` (a matched reply).
  Once sends and receives are decoupled, sequence numbers must advance per *send*, or a
  single loss desynchronises every later packet.
- **`ft_rec_resp()` loses its `sent_id` / `sent_seq` parameters.** It can no longer match one
  specific in-flight packet; it matches *any* of ours by `id`, then looks the sequence up.
  New signature: `int ft_rec_resp(int sockfd, const t_ping_ctx *ctx, t_ping_stats *stats)`.
- **`ft_rec_resp()` must not block.** It reads what `poll()` said was ready, then returns.

### P0.3 — Send-time table, replacing the timestamp-in-payload  [BLOCKER for `-s`]

`fill_payload()` writes a `struct timespec` into the first 16 bytes of the payload, and
`ft_rec_resp()` reads it back. With `-s 0` … `-s 15` there is nowhere to put it.

**Fix:** keep send timestamps in a fixed ring indexed by sequence:

```c
#define PING_TS_RING 65536                  /* one slot per uint16 seq */
struct timespec *send_ts;                   /* calloc'd once, indexed [seq % PING_TS_RING] */
```

65536 × 16 bytes = 1 MB. If that's too much for your taste, a 1024-entry ring plus a
"sequence too old to time" branch is equally defensible — real ping keeps a bitmap. Either
way this also fixes duplicate-reply and out-of-order-reply timing, which the payload
approach gets wrong.

Keep writing the timestamp into the payload as well *when it fits* — it costs nothing and
matches reference ping's on-the-wire bytes.

### P0.4 — Variable-length packet  [BLOCKER for `-s`, `-p`]

`t_icmp_packet` is a fixed 64-byte struct with a `_Static_assert`. Replace with a header
struct plus a runtime-sized buffer:

```c
typedef struct s_icmp_hdr {
    uint8_t  type; uint8_t  code; uint16_t checksum;
    uint16_t id;   uint16_t sequence;
} t_icmp_hdr;                                        /* exactly 8 bytes */
_Static_assert(sizeof(t_icmp_hdr) == 8, "ICMP header must be 8 bytes");
```

Allocate once, after parsing: `pkt = malloc(8 + payload_len)` where
`payload_len = flags.has_packet_size ? flags.packet_size : PING_PAYLOAD_SIZE`.
Every `sizeof pkt` becomes `8 + payload_len` — in `ft_checksum()`, in `sendto()`, and in
`fill_payload()`'s loop bound. `PING_PAYLOAD_SIZE` stays as the default, not the size.

Max: `-s 65507` → 65515-byte ICMP message. Your `recvfrom` buffer in `ft_rec_resp.c` is
`char buf[1024]`; it must grow to at least `65535` or replies to large probes get truncated
and silently fail the size gates. Make it a heap buffer sized from `payload_len`.

### P0.5 — Thread the config through

Bundle what the loop and the receive path both need, so signatures stop growing:

```c
typedef struct s_ping_ctx {
    t_flags            flags;
    struct sockaddr_in dst;
    char               ipstr[INET_ADDRSTRLEN];
    const char        *host;        /* av[optind] as typed */
    uint8_t           *pkt;         /* header + payload */
    size_t             payload_len;
    struct timespec   *send_ts;
} t_ping_ctx;
```

### P0.6 — Housekeeping (small, do them in the same commit)

- Delete `print_flags()` — debug scaffolding, it will corrupt your Stage 9 output diff.
- `av[optind]` is used with no `optind < ac` check. `./ft_ping -v` with no host is a
  null-pointer deref today.  [SHOULD-FIX]
- `parse.c` ends with four lines of shell (`sudo ping-ref -c 3 …`) pasted below
  `test_strton()`. It doesn't compile as C. Move it to a comment or a script.  [BLOCKER]

---

## Group 1 — sockopts and setup

No loop changes. All of these go in a `configure_socket(int sockfd, const t_flags *f)`
called between `socket()` and the loop, in that order.

### `-m ttl` → `IP_TTL`

```c
if (f->has_ttl && setsockopt(sockfd, IPPROTO_IP, IP_TTL, &f->ttl, sizeof f->ttl) == -1)
    return (perror("setsockopt IP_TTL"), -1);
```

`setsockopt` wants an `int`; `flags->ttl` already is one. Parsing already bounds it to
0–255.

**Test gap:** the module tells you `-m 1` against a distant host produces a *Time Exceeded*
reply. Your `ft_rec_resp()` currently does `if (reply->type != 0 || reply->code != 0)
continue;` — it silently discards every ICMP error. So `-m 1` will look like 100% loss until
you build the error path in **Group 4**. Either accept that Group 1's test is deferred, or
pull the error-reporting work forward.

### `-T ttl` → `IP_MULTICAST_TTL`

Same call, option `IP_MULTICAST_TTL`. Note the kernel expects an `unsigned char` for this
one on some platforms and an `int` on Linux; passing `int` is accepted on Linux and macOS.
No visible effect on a unicast destination — verify that against `ping-ref` rather than
assuming it's a bug in your code.

### `-r` → `SO_DONTROUTE`

```c
int on = 1;
setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof on);
```

Level is `SOL_SOCKET`, not `IPPROTO_IP`. Failure mode is at `sendto()` time
(`ENETUNREACH` / `EHOSTUNREACH`) when the target isn't on an attached network — your
existing `perror("Send")` covers it, but the message should match reference ping's.

### `-S src_addr` → `bind()`

```c
struct sockaddr_in src = {0};
src.sin_family = AF_INET;
if (inet_pton(AF_INET, f->source_addr, &src.sin_addr) != 1)
    return (fprintf(stderr, "ft_ping: bad source address %s\n", f->source_addr), -1);
if (bind(sockfd, (struct sockaddr *)&src, sizeof src) == -1)
    return (perror("bind"), -1);
```

Decision to make: reference ping accepts a *hostname* for `-S`, not just a literal. Reusing
`resolve_host()` gets you that for free — recommended, since it's already written and
IPv4-only. Binding an address the machine doesn't own fails with `EADDRNOTAVAIL`; that must
exit cleanly before any packet is sent, not mid-loop.

### `-n` → skip reverse DNS

**This flag is currently a no-op, because you never do a forward-confirmed reverse lookup at
all.** `main.c` prints `av[optind]` verbatim, and `ft_rec_resp.c` prints `inet_ntop()` of the
source. Both are already numeric.

To make `-n` mean something you first have to add the lookup it suppresses:

1. In `ft_rec_resp()`, before printing the reply line, call `getnameinfo()` on `ip->ip_src`
   and print `host (ip)` when it resolves, bare `ip` when it doesn't.
2. Gate that call on `!flags.numeric`.
3. Verify by timing: `-n` against an address with no PTR record must be *faster*, not just
   differently formatted. `getnameinfo()` on an unresolvable address blocks for seconds.

Sequencing note: this makes each reply line dependent on a blocking DNS call inside your
receive path. Consider caching the result per source address — reference ping does.

---

## Group 2 — payload

Depends on **P0.3** and **P0.4**.

### `-s packetsize`

Once P0.4 lands this is almost free: `payload_len` comes from the flag. What still needs
attention:

- **`-s 0`** — 8-byte packet, no payload. `fill_payload()` must be a no-op, the timestamp
  goes only to the ring (P0.3), and `ft_rec_resp()`'s gate `if (n < ihl + 8)` is exactly
  right already.
- **Odd sizes expose a checksum bug.**  [SHOULD-FIX] `ft_checksum()`'s trailing-byte branch
  is `sum += ((uint16_t)p[0]) << 8;`. That is correct on big-endian only. Your words are
  read with `memcpy` into native order, so on x86/ARM the final odd byte must land in the
  *low* half:

  ```c
  if (len == 1) { word = 0; memcpy(&word, p, 1); sum += word; }
  ```

  This is portable both ways and is what RFC 1071's "pad with zero" actually means. Test
  with `-s 57`; the reply will be dropped by the kernel or the peer today.
- **Byte count in the output line.** `ft_rec_resp()` prints `n - ihl`, which is already
  size-derived — no change needed. Confirm `-s 100` prints `108 bytes from …`.
- **`-s 65507`** — the IPv4 maximum. This *will* fragment. `sendto()` may return `EMSGSIZE`
  on some paths; treat it as a clean error, not a crash. Also see the `recvfrom` buffer note
  in P0.4.

### `-p pattern`

Write the length check **before** the parse loop. This is the classic overflow in this
project.

```
1. Reject if strlen(pattern) is odd            -> "ft_ping: patterns must be specified as hex digits"
2. Reject any char not in [0-9a-fA-F]
3. Cap at 16 pad bytes (32 hex chars) - reference ping's documented limit
4. Decode to uint8_t pad[16], pad_len = strlen/2
5. Fill: payload[i] = pad[i % pad_len] for i in [0, payload_len)
```

The pattern **repeats** to fill the payload; it does not fill once and leave the rest zero.
`-p ff` must produce an all-ones payload.

Interaction with P0.3: when a pattern is given, do *not* also write the timestamp into the
payload — it would overwrite the user's bytes and defeat the flag's purpose (diagnosing
data-dependent link problems). Timestamps come from the ring in that case.

Validation belongs in `parse.c` next to the other flags, not in the loop — parse failures
must exit before the socket is opened. Move the decode into `parse_info()` and store
`uint8_t pattern[16]` + `int pattern_len` in `t_flags` rather than the raw `char *`.

---

## Group 3 — pacing and exit conditions

Depends on **P0.2**. All five of these are conditions on the loop skeleton from P0.2; none
of them need new syscalls beyond `poll()`.

### `-o` — exit on first reply

One line in the skeleton (`if (flags.exit_on_reply && stats.n_recv) break;`). Do it first
to prove the restructure works. Note it must still print the full summary via
`print_stats()` and exit `0`.

### `-i wait` — fractional interval

Replaces `sleep(PING_INTERVAL_SEC)`. In the P0.2 skeleton there is no `sleep()` at all —
the interval becomes `next_send = last_send + interval`, and `poll()`'s timeout is derived
from it. That gives you fractional seconds for free and keeps the receive path live during
the wait, which `sleep()` does not.

If you keep an explicit sleep instead, it must be `nanosleep()`; `sleep()` takes whole
seconds only. Recommend against it: sleeping deaf means replies arriving during the gap sit
in the socket buffer and their RTT is measured late.

Guard: reference ping requires root for intervals below 0.002 s. `parse.c` already rejects
`<= 0`; add the sub-2ms euid check there.

### `-l preload` — burst then settle

```
for (i = 0; i < flags.preload; i++) send_one();   /* before entering the loop */
```

Sent as fast as possible, no interval, no waiting for replies — which is exactly why P0.2
must land first. Root-only per the BSD man page you pasted into `parse.c`; check
`geteuid() != 0` and refuse cleanly.

### `-f` — flood

Three distinct behaviours, all of which need the P0.2 skeleton:

1. **Pacing:** send as fast as replies come back, with a floor of 100 packets/second. In
   skeleton terms: `next_send = now()` after each reply, and a 10 ms cap otherwise.
2. **Output:** print `.` on every send, backspace (`\b`) on every reply, and *nothing else*
   per packet. `fflush(stdout)` after each — otherwise stdio buffering makes it useless.
3. **Permission:** root-only. `geteuid() != 0` → refuse with reference ping's wording.

`parse.c` already rejects `-f` with `-i`. Ctrl+C must still work — with `poll()` that comes
free, since `g_stop` is checked every iteration. Test against `127.0.0.1` only.

### `-t timeout` — total deadline

`deadline = start + flags.timeout` computed once with `CLOCK_MONOTONIC`, checked at the top
of each iteration and used to clamp `poll()`'s timeout so you don't overshoot by an interval.
Independent of packet count. Exit code follows reference ping: `0` if any reply was
received, `1` otherwise.

---

## Group 4 — output suppression

This group's real content is **building the ICMP error path**, which doesn't exist yet.
`-Q` and `-q` are two `if`s on top of it.

### Prerequisite: report ICMP errors caused by our probes

`ft_rec_resp()` line 44 discards everything that isn't an echo reply. To report errors you
need to look *inside* the error packet:

```
ICMP error packet layout:
  [IP hdr][ICMP hdr type=3/11/…][original IP hdr][first 8 bytes of original datagram]
                                                  ^ our ICMP header: type, code, id, seq
```

Steps in `ft_rec_resp()` when `reply->type != 0`:

1. Gate: `n >= ihl + 8 + sizeof(struct ip) + 8` — otherwise the quote is truncated, skip it.
2. Parse the quoted IP header, take its `ip_hl * 4`, read the quoted ICMP header after it.
3. Match the quoted `id` against ours. If it doesn't match, this error was somebody else's
   probe — print it only under `-v`, never otherwise.
4. Print `From <src> icmp_seq=<quoted seq> <description>` where the description comes from a
   `type`/`code` lookup table (`Time to live exceeded`, `Destination Host Unreachable`, …).
5. **This is not a reply.** Do not touch `n_recv` or the RTT accumulators. It counts as loss.

This is what makes `-m 1` (Group 1) observable and `-v` meaningful, so it is arguably the
highest-value item in Group 4 rather than the last.

### `-q` — quiet

Suppresses **every** per-packet line: reply lines, error lines, `-v` lines, and `-f`'s dots.
Keeps the `PING host (ip)` startup line and the whole `print_stats()` summary. Implement as
a single guard at each print site, not by redirecting stdout.

`parse.c` already zeroes `quiet_errors` and `verbose` when `quiet` is set — that matches
reference ping, and it means the `-q` guard is the only one you need at the error print
site.

### `-Q` — quiet errors only

Suppresses only the ICMP-error lines from step 4 above. Echo replies still print. One
condition on the same print site.

---

## Recommended order

| # | Work | Why here |
|---|---|---|
| 1 | P0.6 housekeeping (shell text in `parse.c`, `print_flags`, `optind` guard) | Minutes, and one of them is a compile error |
| 2 | P0.1 + P0.2 loop restructure | Unblocks Group 3, fixes the mandatory-part loss hang |
| 3 | P0.5 context struct, P0.3 timestamp ring, P0.4 variable packet | Signature churn — do it in one pass |
| 4 | Group 4's error path | Makes `-m`, `-v`, `-T` testable |
| 5 | Group 1 sockopts | Trivial once the error path exists |
| 6 | Group 3 (`-o` → `-i` → `-l` → `-t` → `-f`) | Skeleton already in place |
| 7 | Group 2 (`-s`, then `-p`) | Independent; the checksum fix lands here |
| 8 | Group 4's `-q` / `-Q` guards | Last, once every print site exists |

**After every step:** re-run the Stage 9 diff with no flags passed. Bonus work regressing
mandatory output is the standard way to lose the whole bonus.

```sh
sudo ping-ref -c 3 8.8.8.8 > ref.txt 2>&1
sudo timeout 3 ./ft_ping 8.8.8.8 > mine.txt 2>&1
diff <(sed -E 's/[0-9]+\.[0-9]+ ms//' ref.txt) <(sed -E 's/[0-9]+\.[0-9]+ ms//' mine.txt)
```

---

## Nitpicks

- `ft_ping.h` includes `<sys/errno.h>` — non-portable spelling; `<errno.h>` is the standard one.
- `elapsed_ms()` is declared in the header but `fill_payload()` and `print_flags()` are `static`
  in `main.c` while `print_stats()` isn't. Pick one convention.
- `PING_INTERVAL_SEC` becomes dead once `-i` lands — keep it as the *default* value that `-i`
  overrides, rather than deleting it.
- `t_flags` will have `has_x`/`x` pairs for eight options. A `uint16_t set_mask` with one bit
  per flag would be tighter, but the current form is clearer to an evaluator — leaving it is fine.
