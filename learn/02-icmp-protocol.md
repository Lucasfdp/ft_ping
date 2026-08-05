# Stage 2 — ICMP Protocol Deep Dive

**Prerequisite:** [01-sockets.md](01-sockets.md) · **Next:** [03-checksum.md](03-checksum.md)

---

**Plain version:** an ICMP message is an envelope — a "type" (what kind of message), a "code" (a
more specific sub-reason), a checksum for integrity, then message-specific data.

**Echo messages:** Echo Request = type `8`, code `0`. Echo Reply = type `0`, code `0`.

## Echo-specific fields

- **Identifier** — traditionally the sending process's PID. Lets your ping tell its own replies
  apart from another process's pings running on the same host at the same time.
- **Sequence number** — increments per packet sent. Lets you detect loss and reordering.
- **Payload data** — arbitrary bytes, often used to carry a timestamp and/or a fill pattern.

**Struct layout** (`struct icmphdr` in `netinet/ip_icmp.h`): `type` (1B), `code` (1B), `checksum`
(2B), `un.echo.id` (2B), `un.echo.sequence` (2B).

> **Names, expanded** — `icmphdr` = ICMP **h**ea**d**e**r** · `un` = **un**ion (one memory slot
> reused for different message types' fields) · `htons` = **h**ost **to** **n**etwork **s**hort,
> `ntohs` = **n**etwork **to** **h**ost **s**hort. "Short" means a 2-byte number; these swap which
> end of it goes first, because networks and machines don't always agree.

**Other ICMP types worth knowing** (for robustness and for the bonus): Destination Unreachable
(type 3), Time Exceeded (type 11 — directly relevant to the `-T`/`--ttl` bonus), Redirect (type 5).
If you get one of these instead of an Echo Reply, real ping reports it — it doesn't crash.

---

## Check questions

<details><summary>Q1: What's the identifier field for, given that IP addresses already identify who's talking to whom?</summary>

Multiple processes — even multiple ping instances — can be pinging the same target from the same
machine simultaneously. The identifier (traditionally the sender's PID) lets each instance filter
incoming replies down to just its own packets.
</details>

<details><summary>Q2: What should ft_ping do if it receives a Time Exceeded message instead of an Echo Reply?</summary>

Not crash. The subject is explicit that the program can never exit unexpectedly. It should report
the condition (this is exactly what `-v` is for) and keep running, not treat it as fatal.
</details>

---

## Exercise

Still no network I/O — just get the bytes right.

1. Write a `t_icmp_packet` struct (or use `struct icmphdr` + a payload array) and
   `printf("%zu\n", sizeof(...))`. Confirm the header is exactly **8 bytes**. If it isn't, you have
   a padding problem — find out why.
2. Fill in type=8, code=0, id=`getpid()`, seq=1, checksum=0. Hexdump the buffer byte by byte:
   ```c
   for (size_t i = 0; i < sizeof(pkt); i++) printf("%02x ", ((unsigned char *)&pkt)[i]);
   ```
3. Compare that hexdump against a real `tcpdump -x icmp` capture of a system `ping -c 1`. The first
   8 bytes after the IP header should line up field-for-field (checksum and payload will differ).
4. **Byte-order check:** `id` and `sequence` are multi-byte. Which ones need `htons()`? Write down
   your answer, then verify against the capture — a mismatched byte order is the single most common
   ft_ping bug and it's invisible until you compare hexdumps.

**Done when:** your hexdump's type/code/id/seq bytes match a real ping's, in the same positions and
same byte order.

---

## Reading

- **RFC 792** — read it fully this time. Pay attention to the Destination Unreachable (type 3),
  Time Exceeded (type 11) and Redirect (type 5) message formats, not just Echo
- `/usr/include/netinet/ip_icmp.h` — read the actual `struct icmphdr` and the `ICMP_*` type
  constants on your machine
- `man 3 htons` / `man 3 endian` — byte-order conversion
- **IANA ICMP Parameters** registry — the authoritative type/code list
