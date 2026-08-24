# Stage 10 (Bonus) — Extra Flags

**Prerequisite:** [09-output-makefile.md](09-output-makefile.md) — **and a fully passing mandatory
part.** The subject gates bonus evaluation on a perfect mandatory part; work here counts for
nothing if anything above is incomplete.

**Updated for your actual flag set.** `--ip-timestamp` is parked (pending confirmation with a staff
member that it's not required for full bonus) and `-w`/`-W`/the `--ttl` long alias are dropped —
your design covers the same ground with `-t`, `-m`, and `-T` instead. If staff say `--ip-timestamp`
is needed after all, come back and this stage gets a Group 5 added for it.

---

| Flag | What it does | Ties back to |
|---|---|---|
| `-f` | Flood mode — fire the next packet as fast as possible (root only) | Stage 4/7 loop structure |
| `-l preload` | Send `preload` packets up front before settling into normal pacing | Stage 4/5 send-vs-receive decoupling |
| `-i wait` | Custom interval between packets — fractional seconds allowed | Stage 6 timing; replaces the hardcoded interval |
| `-m ttl` | Set the IP TTL on your outgoing **unicast** packets | New sockopt: `IP_TTL` |
| `-n` | Numeric only — skip reverse-DNS, print raw IPs | Stage 4 resolution |
| `-o` | Exit successfully after the first reply | Stage 7 loop/exit logic |
| `-Q` | Suppress ICMP *error* messages triggered by your own probes | Stage 7 error reporting |
| `-q` | Quiet — suppress every per-packet line, print only the startup + summary lines | Stage 7 `print_stats` |
| `-r` | Bypass routing — `SO_DONTROUTE`, send directly to a host on an attached network | New sockopt |
| `-p pattern` | Fill the payload with a user-supplied hex pattern instead of default data | Stage 2 payload |
| `-S src_addr` | Bind the socket to a specific local source address before sending | New: `bind()` on a raw socket |
| `-s packetsize` | Custom payload size — large values may trigger IP fragmentation | New concept: MTU/fragmentation |
| `-T ttl` | Set the IP TTL specifically for **multicast** destinations | New sockopt: `IP_MULTICAST_TTL` — a *different* option from `-m`'s |
| `-t timeout` | Total deadline in seconds — exit once reached, regardless of packet count | Stage 7 loop/exit logic |

> **Names, expanded** — `IPPROTO_IP` = **IP** **PROTO**col level: **IP** (the "which layer does
> this option belong to" argument of `setsockopt`) · `setsockopt` = **set** **sock**et **opt**ion ·
> `SO_DONTROUTE` = **S**ocket **O**ption: **DON'T ROUTE** ·
> `IP_TTL` = **IP** **T**ime **T**o **L**ive (counts *hops*, not seconds) ·
> `IP_MULTICAST_TTL` = **IP** **MULTICAST** **T**ime **T**o **L**ive — the hop limit applied only to
> packets sent to a multicast destination · `bind()` = attach a socket to a specific local address
> before using it, instead of letting the kernel pick one automatically ·
> **MTU** = **M**aximum **T**ransmission **U**nit, the largest packet a link will carry (~1500
> bytes on Ethernet). Exceed it and the packet gets split up — that's *fragmentation*.
> Full list in [GLOSSARY.md](GLOSSARY.md).

**`-m` and `-T` are not the same knob, even though both say "TTL."** `-m` sets `IP_TTL`, which
governs every ordinary (unicast) packet you send. `-T` sets `IP_MULTICAST_TTL`, a completely
separate sockopt the kernel only consults when the destination address is a multicast address
(224.0.0.0–239.255.255.255). Passing `-T` against a normal unicast host is harmless but has no
visible effect — that's expected, not a bug, and worth confirming against the reference
implementation's actual behavior in that case rather than assuming.

---

## Check questions

<details><summary>Q1: Why do -m and -T need two different setsockopt calls even though both are described as "TTL"?</summary>

They control the hop limit for two different classes of destination. `IP_TTL` (`-m`) applies to
ordinary unicast traffic; `IP_MULTICAST_TTL` (`-T`) is a separate option the kernel only applies
when the destination is a multicast address. Setting one doesn't touch the other — a unicast ping
with only `-T` set still uses the default TTL for its actual transmission.
</details>

<details><summary>Q2: Why might -s interact with a concept the mandatory part never forces you to touch?</summary>

Sending a payload large enough that the resulting IP packet exceeds the network's MTU can trigger
IP fragmentation — a lower-layer behavior the mandatory part never requires you to think about.
Worth knowing conceptually even if your implementation doesn't manually handle reassembly.
</details>

---

## Exercise

Implement in this order — each group reuses the last group's structure, and the first two groups
need no changes to your send/receive loop at all.

**Group 1 — pure sockopts and setup (no loop changes):** `-m`, `-T`, `-r`, `-S`, `-n`
- `-m 1` against a distant host should produce a **Time Exceeded** reply — your Stage 2 robustness
  path finally firing for real. Confirm you report it and keep running.
- `-T 1` only has a visible effect against a multicast destination — test it there, and separately
  confirm it does *not* alter unicast behavior when used against a normal host.
- `-S` needs a `bind()` call on your raw socket, using the parsed source address, before you start
  sending. Decide what happens if the address isn't one of the machine's own interfaces — `bind`
  will fail, and that failure needs to surface as a clean error, not a crash.
- `-n` should skip the reverse lookup entirely (verify: it must be *faster*, not just formatted
  differently).

**Group 2 — payload changes:** `-p`, `-s`
- `-p` parses **user-supplied hex from argv**. Validate strictly: reject non-hex characters, reject
  odd-length strings, bound the length against your payload buffer. Unbounded parsing straight into
  a fixed buffer is the classic overflow here — write the length check first.
- `-s 65507` (the IPv4 max, see Stage 9/10 reading) and `-s 0` are both edge cases. Neither may
  crash. Watch for `-s` interacting with the Stage 6 timestamp: if the payload is smaller than
  `sizeof(struct timespec)` you have nowhere to put it — decide and handle it.

**Group 3 — pacing and loop control:** `-o`, `-i`, `-l`, `-f`, `-t`
- Start with `-o` — it's the simplest of this group: break out of the loop right after the first
  successful reply instead of waiting for Ctrl+C.
- `-i` replaces your hardcoded interval, and it needs to support **fractional seconds** — `sleep()`
  only takes whole seconds, so this is where you switch to `usleep()` or `nanosleep()`.
- `-l` sends `preload` packets up front before settling into the normal per-`-i` pacing.
- `-f` (flood) requires root or must refuse cleanly — and must still honor Ctrl+C. Test it against
  `127.0.0.1` only; flooding a host you don't own is worth being careful about.
- `-t` is a **total** deadline, independent of packet count — track elapsed wall-clock time (Stage
  6's `CLOCK_MONOTONIC` approach) and break once it's exceeded. A per-iteration check between sends
  is enough precision for this; you don't need `select`/`poll` unless you want sub-second accuracy.

**Group 4 — output suppression:** `-Q`, `-q`
- `-q` suppresses **every** per-packet line (including anything `-v` would normally print) but
  keeps the startup line and the final summary.
- `-Q` suppresses only *ICMP error* replies caused by your own probes — ordinary echo replies still
  print. It's meaningless when `-q` is already set; decide and document what happens if both are
  passed together (real ping treats `-q` as taking priority — worth confirming against the
  reference rather than assuming).

**`--ip-timestamp` (parked):** not attempted until you've confirmed with staff whether it's needed.
If it turns out to be required: it's an IP header *option* (RFC 791 §3.1), not something exposed
via a simple `setsockopt`, so it means setting `IP_HDRINCL` and building the IP header yourself —
noticeably more work than every flag above. Flag it early if staff confirm it's in scope, since it
changes your time estimate for the bonus.

**Done when:** each group passes with valgrind clean and the mandatory-part output diff from Stage
9 still identical with no flags passed. **Re-run the Stage 9 diff after every group** — bonus work
regressing mandatory output is the standard way to lose the whole bonus.

---

## Reading

- `man 7 ip` — `IP_TTL`, `IP_MULTICAST_TTL`, `IP_HDRINCL`, `IP_OPTIONS`
- `man 2 bind` — for `-S`
- `man 3 usleep` / `man 2 nanosleep` — for `-i`'s fractional intervals
- `man 7 socket` — `SO_DONTROUTE`
- **RFC 791** §3.2 "Fragmentation and Reassembly" — for `-s`; §3.1 "Options" if `--ip-timestamp`
  turns out to be in scope
- `man 8 ping` — the reference semantics for every one of these flags; match its behaviour
- inetutils-2.0 `ping/ping.c` — its option handling, for output format parity
