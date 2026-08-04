# Stage 10 (Bonus) — Extra Flags

**Prerequisite:** [09-output-makefile.md](09-output-makefile.md) — **and a fully passing mandatory
part.** The subject gates bonus evaluation on a perfect mandatory part; work here counts for
nothing if anything above is incomplete.

---

| Flag | What it does | Ties back to |
|---|---|---|
| `-f` | Flood mode — fire the next packet immediately (on reply or with no wait), dot-per-packet output | Stage 4/7 loop structure |
| `-l` | Preload — send N packets up front before waiting for any replies | Stage 4/5 send-vs-receive decoupling |
| `-n` | Numeric only — skip reverse-DNS on output, print raw IPs | Stage 4 resolution |
| `-w` | Deadline — total run time before forced exit, regardless of packet count | Stage 7 loop/exit logic |
| `-W` | Per-packet reply timeout | Stage 7 loop/exit logic |
| `-p` | Pattern — fill payload bytes with a specified hex pattern instead of default data | Stage 2 payload |
| `-r` | Bypass routing — `SO_DONTROUTE`, send directly to a host on an attached network | New sockopt |
| `-s` | Packet size — larger payloads may trigger IP fragmentation | New concept: MTU/fragmentation |
| `-T`/`--ttl` | Set IP TTL via `setsockopt(IPPROTO_IP, IP_TTL, ...)` — can deliberately induce a Time Exceeded reply | Stage 2 ICMP error types |
| `--ip-timestamp` | Attach an IP-level timestamp *option* to outgoing packets | Stage 1 `IP_HDRINCL` |

`--ip-timestamp` is the one that genuinely forces you back into `IP_HDRINCL` territory: IP
timestamp is an IP header *option*, not something exposed via a simple `setsockopt`, so producing
it means building the IP header yourself.

---

## Check questions

<details><summary>Q1: Which bonus flag most directly requires revisiting IP_HDRINCL, and why?</summary>

`--ip-timestamp`. IP timestamp is an IP header option, not something you can set via a simple
setsockopt call — producing it means constructing the IP header yourself with IP_HDRINCL set,
rather than letting the kernel build it.
</details>

<details><summary>Q2: Why might -s interact with a concept the mandatory part never forces you to touch?</summary>

Sending a payload large enough that the resulting IP packet exceeds the network's MTU can trigger
IP fragmentation — a lower-layer behavior the mandatory part never requires you to think about.
Worth knowing conceptually even if your implementation doesn't manually handle reassembly.
</details>

---

## Exercise

Implement in this order — each group reuses the last group's structure.

**Group 1 — pure sockopts (easiest, no loop changes):** `-T/--ttl`, `-r`, `-n`
- `-T 1` against a distant host should produce a **Time Exceeded** reply. This is your Stage 2
  robustness path finally firing for real — confirm you report it and keep running.
- `-n` should skip the reverse lookup entirely (verify: it must be *faster*, not just formatted
  differently).

**Group 2 — payload changes:** `-p`, `-s`
- `-p` parses **user-supplied hex from argv**. Validate strictly: reject non-hex characters, reject
  odd-length strings, bound the length against your payload buffer. Unbounded parsing straight into
  a fixed buffer is the classic overflow here — write the length check first.
- `-s 65500` and `-s 0` are both edge cases. Neither may crash. Watch for `-s` interacting with the
  Stage 6 timestamp: if the payload is smaller than `sizeof(struct timespec)` you have nowhere to
  put it — decide and handle it.

**Group 3 — loop restructuring:** `-w`, `-W`, `-l`, `-f`
- These force send and receive apart. Convert the blocking `recvfrom` to `select`/`poll` with a
  timeout rather than layering on `alarm()`.
- `-f` requires root or it must refuse cleanly — and it must still honour Ctrl+C. Test it against
  `127.0.0.1` only; flooding a host you don't own is worth being careful about.
- Extract the timing constants: `#define FLOOD_MIN_INTERVAL_US 10000` etc.

**Group 4 — `--ip-timestamp`:** last, alone. Set `IP_HDRINCL`, build the IP header yourself,
compute the IP header checksum (your Stage 3 function works on it unchanged), and place the option
with correct 4-byte alignment and padding.

**Done when:** each group passes with valgrind clean and the mandatory-part output diff from Stage
9 still identical with no flags passed. **Re-run the Stage 9 diff after every group** — bonus work
regressing mandatory output is the standard way to lose the whole bonus.

---

## Reading

- `man 7 ip` — `IP_TTL`, `IP_HDRINCL`, `IP_OPTIONS`
- `man 7 socket` — `SO_DONTROUTE`
- **RFC 791** §3.1 "Options" — Internet Timestamp option format, alignment and overflow rules
- **RFC 791** §3.2 "Fragmentation and Reassembly" — for `-s`
- `man 2 select` / `man 2 poll` — for Group 3
- `man 8 ping` — the reference semantics for every one of these flags; match its behaviour
- inetutils-2.0 `ping/ping.c` — its option handling, for output format parity
