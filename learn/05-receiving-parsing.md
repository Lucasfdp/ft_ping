# Stage 5 — Receiving & Parsing the Reply

**Prerequisite:** [04-building-sending.md](04-building-sending.md) · **Next:** [06-timing-rtt.md](06-timing-rtt.md)

---

**Plain version:** because you're on a raw socket, `recvfrom()` doesn't hand you just the ICMP
message — it hands you the *entire IP packet*, IP header included, wrapped around that ICMP reply.

**IP header parsing:** you need a struct for the IP header (`struct ip` in `netinet/ip.h`, or your
own), specifically its **IHL** field — **I**nternet **H**eader **L**ength. This tells you how many 32-bit
words long the IP header actually is *for this specific packet* (it varies when IP options are
present), so you know exactly where the ICMP payload starts.

**Matching your own reply:** after skipping the IP header, parse the remaining bytes as your ICMP
header struct, check type/code, and compare identifier + sequence number against what you sent —
confirming it's actually a reply to *your* request, not another process's ping or a stray packet.

> **Names, expanded** — `recvfrom` = **rec**ei**v**e **from** (returns the sender's address too) ·
> **IHL** = **I**nternet **H**eader **L**ength · **TTL** = **T**ime **T**o **L**ive (counts *hops*,
> not seconds — each router decrements it) · `ICMP_ECHOREPLY` = the ICMP **ECHO REPLY** type
> constant, value 0 · `MSG_TRUNC` = **M**e**S**sa**G**e **TRUNC**ated, the flag meaning "the packet
> was bigger than your buffer and the rest is gone" · **RTT** = **R**ound-**T**rip **T**ime.
> Full list in [GLOSSARY.md](GLOSSARY.md).
>
> **"Datagram"** here just means one self-contained packet, as opposed to a continuous stream.
> **"Granularity"** means the size of the unit you work in — a raw socket works in whole packets.

---

## Check questions

<details><summary>Q1: Why does recvfrom() hand you more than the ICMP message you expected?</summary>

A raw ICMP socket receives at IP-layer granularity — you get the full IP datagram (header +
payload), not a transport-filtered stream. The ICMP message sits after the IP header, not as the
entire buffer.
</details>

<details><summary>Q2: Why can't you assume the IP header is always exactly 20 bytes?</summary>

IP header length is variable because of optional header fields. The IHL field gives the actual
length in 32-bit words for this specific packet — hardcoding 20 bytes breaks on any packet
carrying options.
</details>

---

## Exercise

Close the loop: one send, one receive, one printed line.

1. `recvfrom()` into a buffer comfortably larger than any expected packet. **Check the return
   length before touching anything** — a short read that you parse as a full IP header is a
   straight buffer overread.
2. Extract IHL: `ip_hl * 4` gives the byte offset. Print it. On a plain reply it'll be 20; note
   that you computed it rather than assumed it.
3. **Validate before you index.** In order: is `n >= sizeof(struct ip)`? Is `ihl >= 20`? Is
   `n >= ihl + sizeof(struct icmphdr)`? Only then read the ICMP header. Skipping any of these is a
   crash the eval will find.
4. Filter: `type == ICMP_ECHOREPLY (0)`, `id == your id`, `seq == expected`. **Deliberately break
   the id check** (hardcode a wrong id) and confirm you correctly reject the reply — that proves
   the filter actually runs.
5. Print a single line resembling
   `64 bytes from 127.0.0.1: icmp_seq=1 ttl=64` (TTL comes from the IP header's `ip_ttl`). RTT
   comes in Stage 6.
6. Run it under ASan against localhost and a real host.

**Done when:** a single successful ping round trip prints, and ASan is clean on both a good reply
and a truncated/wrong-id one.

> **Gotcha:** on Linux the raw socket delivers *every* ICMP packet the host receives, not just
> yours. Your filter isn't optional politeness — without it you'll print other processes' replies.

---

## Reading

- `man 2 recvfrom` — return-value semantics, especially the `0` and `-1` cases and `MSG_TRUNC`
- `man 7 raw` — re-read the "the IP header is always included" paragraph now that it matters
- `/usr/include/netinet/ip.h` — the real `struct ip`; note `ip_hl` is a bitfield and its position
  depends on endianness (read the `#if __BYTE_ORDER` block)
- **RFC 791** §3.1 — the IP header diagram, specifically IHL and Options

---

## Stage 5 completion note — what still differs from real `ping`

Stage 5 is **done**: one send, one receive, one printed line, verified against a `tcpdump`
capture. The items below are known gaps, deliberately left for later stages. Recorded here so
they don't get lost between now and the eval.

### 1. `icmp_seq` starts at 0, real ping starts at 1

`main.c` initialises `int seq = 0;` and the first packet goes out with `sequence = htons(0)`,
so the output reads `icmp_seq=0`. Real `ping` numbers its first packet **1** — every reference
output, every man page example, and the subject's own sample output start at `icmp_seq=1`.

Fix is one character (`int seq = 1;`), but do it **when the send loop lands in Stage 7**, not
now — the loop is where `seq` starts being incremented, and changing the initialiser in
isolation now means touching it twice.

Note that nothing in the receive path needs to change: `sent_seq` is passed in from `main` and
compared raw, so it follows whatever `main` sends.

### 2. Uninitialised padding is leaking onto the wire

Visible in the capture. `fill_payload()` does `memcpy(pkt->payload, &tv, sizeof tv)` with
`struct timeval tv` as a local. On macOS/arm64 `struct timeval` is 16 bytes: 8 for `tv_sec`,
4 for `tv_usec`, **4 bytes of padding**. `gettimeofday()` writes the first two fields and
leaves the padding alone, so payload bytes 12–15 are whatever was on the stack:

```
0x0020:  0000 0000 5120 0600 0100 0000 1011 1213
                             ^^^^^^^^^ stack garbage, sent to 8.8.8.8
```

The `memset(&pkt, 0, sizeof pkt)` in `main` does not help — the `memcpy` overwrites those
zeroed bytes with the padding afterwards. ASan will not catch this (it is an uninitialised
*read*, not an out-of-bounds one); valgrind reports it as *"syscall param socketcall.sendto(msg)
points to uninitialised byte(s)"*, and some evaluators do run valgrind.

Fix: `struct timeval tv = {0};` before the `gettimeofday()` call.

### 3. Header line is missing the byte count

Currently: `PING 8.8.8.8 (8.8.8.8)`
Real ping: `PING 8.8.8.8 (8.8.8.8) 56(84) bytes of data.`

Cosmetic, but the subject asks the output to resemble the real thing. Belongs with the rest of
the output work in Stage 9.

### 4. Deferred by design (not bugs)

- **No RTT** — `time=X ms` needs the send timestamp read back out of the payload. Stage 6.
- **No timeout** — a reply that never matches blocks in `recvfrom` forever. This is *correct*
  behaviour for Stage 5 and is exactly what the broken-id test demonstrates; the timeout arrives
  with the loop in Stage 6.
- **Single packet** — no 1-second send loop, no `SIGINT` handler, no final statistics block.
  Stages 6 and 7.

### Verified this stage

Decoded from the `tcpdump -x` capture of a real exchange with 8.8.8.8:

| Field | Value | Check |
|---|---|---|
| IHL | `0x45` → 5 → 20 bytes | computed, not assumed |
| Total length | `0x0054` = 84 = 20 + 64 | matches `bytes_read` |
| ICMP type | `08` request / `00` reply | filter matches on 0 |
| id | `0x4a06` = 18950 | identical both directions |
| seq | `0x0000` | identical both directions |
| Checksum | `0x1f36` → `0x2736` | delta is exactly `0x0800`, the type change — arithmetic is correct |
| Payload fill | starts `10 11 12 13` at offset 16 | matches real ping's pattern |
