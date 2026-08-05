# ft_ping — Learning Path

Deep dive across all concepts including socket fundamentals. Scope: mandatory part + bonus flags.

Each stage file is self-contained: plain-language explanation first, then the precise terms you'll
see in headers, man pages and RFCs. Every stage ends with **check questions** (answers hidden in
`<details>` — try before revealing), a **hands-on exercise**, and a **reading list**.

**Work one file at a time.** Don't open the next stage until the current one's exercise passes.

**Acronyms are always expanded.** Every constant, flag and abbreviation is broken down letter by
letter on first use, and collected in **[GLOSSARY.md](GLOSSARY.md)** — including plain-English
translations of phrases like "inject arbitrary IP-level traffic" and "demultiplexing". If a term
appears anywhere in these notes without an expansion, that's a bug; it belongs in the glossary.

## Stages

| # | File | Topic |
|---|---|---|
| 0 | [00-what-ping-does.md](00-what-ping-does.md) | What ping actually does, where ICMP sits in the stack |
| 1 | [01-sockets.md](01-sockets.md) | Sockets from basics to raw, privileges |
| 2 | [02-icmp-protocol.md](02-icmp-protocol.md) | ICMP header, echo request/reply, other types |
| 3 | [03-checksum.md](03-checksum.md) | The Internet checksum algorithm |
| 4 | [04-building-sending.md](04-building-sending.md) | Packet construction, resolution, `sendto()` |
| 5 | [05-receiving-parsing.md](05-receiving-parsing.md) | `recvfrom()`, IP header parsing, reply matching |
| 6 | [06-timing-rtt.md](06-timing-rtt.md) | Timestamps, `clock_gettime`, RTT |
| 7 | [07-signals-stats.md](07-signals-stats.md) | `sigaction`, async-signal-safety, statistics |
| 8 | [08-cli-errors.md](08-cli-errors.md) | Argument parsing, robust error handling |
| 9 | [09-output-makefile.md](09-output-makefile.md) | Exact output formatting, Makefile rules |
| 10 | [10-bonus-flags.md](10-bonus-flags.md) | Bonus flags and the concepts they pull in |

## Suggested Build Order

1. **Stages 0–3** — concepts only, no code. Get the ICMP header + checksum model solid before
   writing anything.
2. **Stages 4–5** — one working send/receive round trip, no loop yet. Goal: a single successful
   ping to a known-good host (e.g. `127.0.0.1` or a local gateway).
3. **Stages 6–7** — turn it into a real loop: RTT measurement, Ctrl+C summary.
4. **Stages 8–9** — arg parsing, error hardening, exact output formatting, Makefile polish.
5. **Stage 10** — bonus flags, only once the mandatory part is fully passing (the subject gates
   bonus evaluation on a perfect mandatory part).

## Global reading

- **RFC 792** — Internet Control Message Protocol (the whole spec; it's short)
- **RFC 1071** — Computing the Internet Checksum
- **RFC 791** — Internet Protocol (IP header layout, options)
- `man 7 raw`, `man 7 ip`, `man 7 socket`, `man 2 socket`
- inetutils-2.0 source — the reference implementation you're graded against
