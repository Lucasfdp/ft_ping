# Stage 5 — Receiving & Parsing the Reply

**Prerequisite:** [04-building-sending.md](04-building-sending.md) · **Next:** [06-timing-rtt.md](06-timing-rtt.md)

---

**Plain version:** because you're on a raw socket, `recvfrom()` doesn't hand you just the ICMP
message — it hands you the *entire IP packet*, IP header included, wrapped around that ICMP reply.

**IP header parsing:** you need a struct for the IP header (`struct ip` in `netinet/ip.h`, or your
own), specifically its **IHL** (Internet Header Length) field — this tells you how many 32-bit
words long the IP header actually is *for this specific packet* (it varies when IP options are
present), so you know exactly where the ICMP payload starts.

**Matching your own reply:** after skipping the IP header, parse the remaining bytes as your ICMP
header struct, check type/code, and compare identifier + sequence number against what you sent —
confirming it's actually a reply to *your* request, not another process's ping or a stray packet.

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
