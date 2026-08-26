# ft_ping — Evaluation Preparation

> A study document, not a cheat sheet. The goal is that you can **explain the
> mechanism**, not recite lines. Every code snippet below is real code from this
> repository — if you can point at it and say *why it is that way*, the
> evaluation is a conversation rather than an interrogation.

**Contents**

- [Part 0 — Divergences from the reference](#part-0--divergences-from-the-reference-and-how-they-were-resolved)
- [Part 1 — The 90-second explanation](#part-1--the-90-second-explanation)
- [Part 2 — The six pillars](#part-2--the-six-pillars)
- [Part 3 — Life of one packet](#part-3--life-of-one-packet)
- [Part 4 — Architecture map](#part-4--architecture-map)
- [Part 5 — The event loop in detail](#part-5--the-event-loop-in-detail)
- [Part 6 — The data structures](#part-6--the-data-structures)
- [Part 7 — Flag-by-flag reference](#part-7--flag-by-flag-reference)
- [Part 8 — Errors and the "never crash" clause](#part-8--errors-and-the-never-crash-clause)
- [Part 9 — Memory](#part-9--memory)
- [Part 10 — How this is tested](#part-10--how-this-is-tested)
- [Part 11 — Live demo script](#part-11--live-demo-script)
- [Part 12 — Questions an evaluator could ask](#part-12--questions-an-evaluator-could-ask)
- [Part 13 — Whiteboard cheat sheet](#part-13--whiteboard-cheat-sheet)
- [Part 14 — Known limitations, stated honestly](#part-14--known-limitations-stated-honestly)
- [Part 15 — What transfers to your next project](#part-15--what-transfers-to-your-next-project)

---

## Part 0 — Divergences from the reference, and how they were resolved

The subject grades output against **inetutils-2.0**, whose source is vendored in
`inetutils-2.0/ping/`. Reading it turned up seven places where this
implementation disagreed with the reference. **Six are now fixed**; the seventh
is a deliberate, defensible difference.

Being able to say *"I read the reference implementation and diffed my output
against it"* is one of the strongest things you can say in this evaluation.
That is the story of this section — tell it that way.

Re-verify any time you touch output:

```bash
ping-ref -c 3 127.0.0.1 | cat -A > /tmp/ref.txt
./ft_ping -t 3 127.0.0.1 | cat -A > /tmp/mine.txt
diff /tmp/ref.txt /tmp/mine.txt
```

| # | Divergence | Status | Locked in by |
|---|---|---|---|
| 0.1 | `mdev` should be `stddev` | **fixed** | `test_stats`, `test.sh` |
| 0.2 | Loss % must truncate, not round | **fixed** | `test_stats` (66 not 67) |
| 0.3 | No blank line before the statistics header | **fixed** | `test_stats`, `test.sh` |
| 0.4 | `-?` must print usage | **fixed** | `test.sh` (4 cases) |
| 0.5 | `time=` omitted when the payload is tiny | **kept**, deliberately | — |
| 0.6 | Inbound checksums were not verified | **fixed** | `test.sh` (regression guard) |
| 0.7 | `-v` should print the ICMP id in the banner | **fixed** | `test.sh` (2 cases) |

### 0.1 `mdev` → `stddev`

`inetutils-2.0/ping/ping_echo.c`, `echo_finish()`:

```c
printf ("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
        ping_stat->tmin, avg, ping_stat->tmax, nsqrt (vari, 0.0005));
```

The **maths was already identical** — both compute `sqrt(E[x²] − E[x]²)`. Only
the label differed: `mdev` is the Linux `iputils` name, `stddev` is the
BSD/inetutils name, and inetutils is the graded reference.

> **The detail that makes this a good answer:** strictly, "mdev" means *mean
> deviation* and "stddev" means *standard deviation* — different statistics.
> iputils computes the standard deviation and labels it `mdev` anyway. So the
> two references print the same number under different names, and the fix was
> genuinely one word.

### 0.2 Loss percentage truncates

`inetutils-2.0/ping/ping.c`, `ping_finish()`:

```c
printf ("%d%% packet loss",
        (int) (((ping->ping_num_xmit - ping->ping_num_recv) * 100) /
               ping->ping_num_xmit));
```

Integer division — it **truncates**. The old code used `%.0f` on a double, which
**rounds to nearest**. They agree at 33.3% and disagree at 66.6%: reference
prints `66`, the old code printed `67`.

Now, in `src/output/ping_print_report.c`:

```c
static int	loss_percent(const t_ping_stats *stats)
{
	if (stats->n_sent <= 0)
		return (0);
	return ((stats->n_sent - stats->n_recv) * 100 / stats->n_sent);
}
```

The same change brought across the reference's forged-traffic branch, because
more replies than probes cannot produce a sensible percentage:

```c
if (stats->n_sent > 0 && stats->n_recv > stats->n_sent)
	printf("-- somebody is printing forged packets!");
else
	printf("%d%% packet loss", loss_percent(stats));
```

> That branch is not decoration — it is the observable symptom of H4 and H18
> below. If someone forges replies at you, this is the line that says so.

### 0.3 No leading newline before the statistics header

`ping_finish()` in the reference goes straight to the header with no `\n` before
it, and nothing in `ping_run()` or `echo_finish()` prints one either. The blank
line you see interactively is the terminal echoing `^C` at that position —
which is why this is invisible until output is piped to a file.

`print_stats()` now starts with:

```c
printf("--- %s ping statistics ---\n", host);
```

### 0.4 `-?` prints usage

The fix was in the optstring, not the switch:

```c
while ((opt = getopt(ac, av, ":fl:i:m:oQqrp:S:s:T:t:v")) != -1)
```

> **Why the leading colon matters.** Without it, getopt reports *both* "unknown
> option" and "missing argument" as `'?'`, so the two cases are
> indistinguishable — and a bare `-?` cannot be recognised as a request for
> help. With it, getopt returns `':'` for a missing argument and reserves `'?'`
> for options that genuinely do not exist. That one character fixed a real
> usability bug as well: `./ft_ping host -s` used to say *"unknown option: -s"*
> when `-s` obviously exists.

```c
case '?':
	if (optopt == '?')
		return (print_usage(), exit(EXIT_SUCCESS), 0);
	fprintf(stderr, "unknown option: -%c\n", optopt);
	return (print_usage_hint(), 0);

case ':':
	fprintf(stderr, "-%c needs an argument\n", optopt);
	return (print_usage_hint(), 0);
```

`print_usage()` writes to **stdout** — the user asked for it, so it is output,
not a diagnostic, and it survives a pipe into `less`. Errors write to stderr and
print only the one-line pointer, because dumping the whole help block after
every typo buries the message that matters.

### 0.5 `time=` when the payload is too small — kept as is

`inetutils-2.0/ping/ping_common.h`:

```c
#define PING_TIMING(s)  ((s) >= sizeof (struct timeval))
```

The reference prints `time=…`, and the whole round-trip summary line, **only**
when the payload was large enough to have carried a timestamp. With `-s 4` it
prints neither. This implementation always prints both, because its timestamps
live in a ring in its own memory rather than in the packet.

**This is the one divergence left standing, on purpose.** The reference omits
the figure because it genuinely does not have it; this implementation does have
it, and it is correct. Suppressing a correct measurement to imitate a limitation
would be worse software.

> Say exactly that if it comes up. It only shows up under the bonus `-s` flag
> with a payload under 16 bytes, and "I know it differs, here is why I chose to"
> is a far stronger answer than not having noticed.

### 0.6 Inbound checksums are now verified

The reference verifies. `inetutils-2.0/libicmp/icmp_echo.c`:

```c
/* Recompute checksum */
cksum = icmp->icmp_cksum;
icmp->icmp_cksum = 0;
icmp->icmp_cksum = icmp_cksum ((unsigned char *) icmp, bufsize - hlen);
if (icmp->icmp_cksum != cksum)
```

and `libping.c` reports it: `checksum mismatch from %s`.

The receive path checked type, code and identifier but never re-ran the
checksum. It does now, in `src/net/ping_receive.c`:

```c
static int	checksum_ok(t_icmp_hdr *reply, ssize_t icmp_bytes)
{
	uint16_t	claimed;
	uint16_t	computed;

	claimed = reply->checksum;
	reply->checksum = 0;
	computed = ft_checksum(reply, (size_t)icmp_bytes);
	reply->checksum = claimed;
	return (computed == claimed);
}
```

called before anything else is read out of the packet:

```c
if (!checksum_ok(reply, n - ihl))
{
	inet_ntop(AF_INET, &ip->ip_src, src, sizeof src);
	print_checksum_mismatch(src);
	continue ;
}
```

> **Three things to be able to say about this.**
>
> 1. **The checksum is a two-ended contract.** The sender computes it over a
>    message whose checksum field reads zero, so the receiver must reproduce
>    exactly that state — save, zero, recompute, compare.
> 2. **The field is restored afterwards**, because the ICMP-error path reads the
>    same buffer after this returns.
> 3. **Do not assume the kernel did it for you.** A raw socket exists precisely
>    to see traffic a cooked socket would have filtered.
>
> If asked *"you compute a checksum on the way out — what do you do with the one
> on the way in?"*, the answer is now the good one.

### 0.7 `-v` prints the identifier in the banner

`inetutils-2.0/ping/ping_echo.c`:

```c
printf ("PING %s (%s): %zu data bytes", ...);
if (options & OPT_VERBOSE)
  printf (", id 0x%04x = %u", ping->ping_ident, ping->ping_ident);
printf ("\n");
```

Reproduced in `print_ping_banner()`:

```c
if (ctx->flags.verbose)
{
	id = ntohs(ctx->id);
	printf(", id 0x%04x = %u", id, id);
}
```

```
$ ./ft_ping -v -t 2 127.0.0.1
PING 127.0.0.1 (127.0.0.1): 56 data bytes, id 0x0e14 = 3604
```

> **Why both hex and decimal:** hex is how you read the field off a `tcpdump`
> capture, decimal is what `ps` shows you for the PID. Printing both lets you
> confirm at a glance that your identifier really is your process id — which is
> exactly the thing `-v` is for.
>
> Note the `ntohs()`: `ctx->id` is stored in network order so inbound packets
> can be compared against it raw, so it has to be converted back to be read by
> a human.

---

## Part 1 — The 90-second explanation

If the evaluator opens with *"so, what does it do?"*, this is the answer. Do not
start with code.

> ft_ping measures whether a host is reachable and how long a round trip to it
> takes. It does that with ICMP, a protocol that sits directly on top of IP and
> exists for control and diagnostics rather than for carrying user data.
>
> The program opens a **raw socket**, which is a socket that speaks IP directly
> instead of TCP or UDP. It builds an **ICMP echo request** by hand — eight
> bytes of header plus a payload — computes a checksum over it, and sends it to
> the destination. By convention, any host receiving an echo request sends back
> an **echo reply** containing the same identifier, the same sequence number and
> the same payload.
>
> The program notes the time it sent each request. When a reply comes back it
> looks up that send time and subtracts, giving the round-trip time. At the end
> it prints how many it sent, how many came back, and the minimum, average,
> maximum and standard deviation of the round-trip times.
>
> The whole thing is one process, no threads. The send schedule is driven by a
> clock and the receive path is driven by `poll()`, so neither one ever waits
> for the other.

That last paragraph is the part that separates a working ft_ping from a good
one. Have it ready.

---

## Part 2 — The six pillars

### 2.1 Raw sockets, and why you need root

A normal socket hands you a byte stream (`SOCK_STREAM`) or datagrams
(`SOCK_DGRAM`), and the kernel builds every header. A **raw socket** gives you
the layer underneath: you supply the ICMP message, and the kernel builds only
the IP header around it.

```c
sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
```

- `AF_INET` — IPv4 address family.
- `SOCK_RAW` — no transport layer; you write protocol payloads directly.
- `IPPROTO_ICMP` — tells the kernel which protocol number (1) to stamp into the
  IP header on send, and which inbound packets to deliver to you on receive.

**Why root?** A raw socket can forge arbitrary IP traffic and can read every
packet of that protocol arriving at the host — including replies to *other*
processes' probes. That is a spoofing and a snooping primitive, so it is gated
behind `CAP_NET_RAW`, which in practice means root or a binary with the
capability set:

```bash
sudo setcap cap_net_raw+ep ./ft_ping   # then it runs unprivileged
```

**Asymmetry to remember:** on **send** you provide only the ICMP bytes; on
**receive** the kernel hands you the **IP header as well**. That asymmetry is
why the receive path starts by skipping `ip_hl * 4` bytes and the send path
never thinks about IP at all.

### 2.2 ICMP: the protocol

ICMP is IP protocol number 1. It has no ports — the only demultiplexing
information is inside the ICMP message itself. Echo request/reply is one of
about a dozen message types.

```
ICMP echo header — 8 bytes, identical for request and reply
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     Type      |     Code      |          Checksum             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identifier            |       Sequence Number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Payload (variable)                       |
```

- **Type 8, code 0** — echo request (what you send).
- **Type 0, code 0** — echo reply (what you expect back).
- **Identifier** — chosen by the sender, echoed unchanged. This is how you tell
  your traffic from another process's.
- **Sequence** — chosen by the sender, echoed unchanged. This is how you tell
  *which* of your probes a reply belongs to.
- **Payload** — arbitrary. The responder must echo it back byte for byte, which
  is what makes `-p` a useful diagnostic.

Your struct mirrors that layout exactly, and refuses to compile if it ever
stops doing so:

```c
typedef struct s_icmp_hdr
{
	uint8_t		type;		///< byte 0   - 8 = echo request, 0 = echo reply
	uint8_t		code;		///< byte 1   - 0 for both echo types
	uint16_t	checksum;	///< bytes 2-3 - RFC 1071, over header + payload
	uint16_t	id;			///< bytes 4-5 - our pid, identifies our traffic
	uint16_t	sequence;	///< bytes 6-7 - per-probe counter, network order
}	t_icmp_hdr;

_Static_assert(sizeof(t_icmp_hdr) == 8,
	"t_icmp_hdr has padding or the wrong size - it must be exactly 8 bytes");
```

**Why the assertion matters.** This struct is memcpy'd onto the wire. If someone
later reorders the fields or widens one, the compiler is free to insert padding,
and you would silently transmit garbage that no host would answer. The
`_Static_assert` turns a runtime mystery into a compile error. It happens to
hold without `__attribute__((packed))` here because the layout is already
naturally aligned — two 1-byte fields followed by three 2-byte fields — but the
assertion is what *guarantees* it rather than hoping.

### 2.3 The Internet checksum (RFC 1071)

Three steps: sum the message as a sequence of 16-bit words; fold any carry that
accumulated above bit 15 back into the low half; take the one's complement.

```c
uint16_t	ft_checksum(const void *buf, size_t len)
{
	const uint8_t	*p;
	uint32_t		sum;
	uint16_t		word;

	p = buf;
	sum = 0;
	while (len > 1)
	{
		memcpy(&word, p, 2);
		sum += word;
		p += 2;
		len -= 2;
	}
	if (len == 1)
	{
		word = 0;
		memcpy(&word, p, 1);
		sum += word;
	}
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ((uint16_t)~sum);
}
```

Four details worth being able to defend:

1. **`uint32_t sum`, not `uint16_t`.** The accumulator has to be wider than the
   words so carries are *captured* rather than lost. Folding them back in
   afterwards is what makes it a one's-complement sum.
2. **The fold is a `while`, not an `if`.** Adding the carry back can itself
   produce a carry. Rare, but a single `if` is a real bug.
3. **`memcpy`, not a `uint16_t *` cast.** `buf` is not guaranteed 2-byte
   aligned. A misaligned load is undefined behaviour and traps on some
   architectures. `memcpy` of two bytes compiles to the same instruction when
   alignment permits, so it costs nothing.
4. **The odd trailing byte is zero-padded conceptually, not physically.**
   Copying it into the low half of a zeroed word reproduces exactly what a real
   pair would have contributed on a little-endian host — you never write past
   the buffer.

**Why the result needs no `htons()`.** Swapping every 16-bit word of the input
swaps the sum, and therefore swaps the complement. The checksum is
*byte-order agnostic*: compute it on either endianness and the bytes that land
on the wire are the same. This is a deliberate property of RFC 1071 and a great
thing to be able to explain.

**Why you must zero the field first:**

```c
hdr->checksum = 0;
hdr->checksum = ft_checksum(ctx->pkt, pktlen);
```

The checksum covers the header, including its own field. The sender computes it
over a message where that field reads zero; the receiver recomputes over the
whole message *including* the checksum, and a correct message sums to `0xFFFF`,
whose complement is 0. Leaving a stale value in the field before computing would
produce a checksum nobody can verify.

### 2.4 Identity — is this reply mine?

ICMP has no ports. Every ICMP packet arriving at the machine is delivered to
**every** raw ICMP socket. If two pings run at once, each sees both
conversations. The identifier is the only thing separating them.

```c
ctx->id = htons((uint16_t)getpid());
```

The PID is a per-process value that no other process on the box shares — a
cheap, collision-free identifier. It is converted **once**, at startup, so
every later comparison is against bytes already in network order:

```c
if (reply->id != ctx->id)
    continue;
```

Comparing the raw bytes on both sides means no per-packet `ntohs()`, and no
chance of accidentally swapping one side twice.

**The sequence number is deliberately *not* part of the filter.** Sends no
longer wait for replies, so by the time a reply lands the counter has already
moved on; filtering on it would reject everything. The sequence number is still
used — to index the timestamp ring — but identity is the id alone.

### 2.5 Time — monotonic clocks and RTT

```c
clock_gettime(CLOCK_MONOTONIC, &ctx->send_ts[seq % PING_TS_RING]);
```

**`CLOCK_MONOTONIC`, never `CLOCK_REALTIME`.** The realtime clock can jump: NTP
steps it, a daylight-saving change moves it, an administrator sets it. Any of
those would produce a negative RTT or a deadline that never expires. The
monotonic clock only ever moves forward at roughly one second per second, which
is the only property RTT and deadlines need.

**Timestamps live in a ring, not in the payload.** This is a design decision
worth defending, because the reference implementation does the opposite:

```c
#define PING_TS_RING 65536
ctx->send_ts = calloc(PING_TS_RING, sizeof(struct timespec));
```

One slot per possible `uint16_t` sequence number — 65536 × 16 bytes ≈ **1 MB**,
allocated once. Three reasons it beats storing the timestamp in the payload:

1. **`-s` can make the payload too small to hold one.** `-s 4` leaves four
   bytes; a `struct timespec` is sixteen.
2. **`-p` overwrites the payload by design.** The whole point of the flag is a
   known byte pattern. Writing a timestamp into it would defeat it.
3. **You should never trust a remote host's arithmetic.** The payload of a reply
   is data a *remote machine* handed you. A hostile or broken host could echo
   anything, and you would compute an RTT from it. The ring is your own memory;
   nothing on the network can corrupt it.

`calloc`, not `malloc`: a reply carrying a sequence number you never sent would
otherwise read uninitialised memory as its send time.

The subtraction is done in integer nanoseconds and converted once:

```c
start_ns = (int64_t)start->tv_sec * 1000000000LL + start->tv_nsec;
end_ns = (int64_t)end->tv_sec * 1000000000LL + end->tv_nsec;
return ((double)(end_ns - start_ns) / 1000000.0);
```

Collapsing to one integer first avoids the manual borrow you would need when
`end->tv_nsec < start->tv_nsec`.

### 2.6 Doing two things at once, without threads

This is the heart of the program. The naive ping is:

```c
while (1) {
    send_one();
    recvfrom();      /* blocks until something arrives */
    sleep(1);
}
```

That has a fatal flaw: **`recvfrom()` blocks forever when a packet is lost.**
The program hangs on the first dropped packet and never sends again — so it can
never report loss, which is the one thing ping exists to report.

The fix is to stop coupling them. `poll()` waits for *whichever comes first*:

```c
rc = poll(&pfd, 1, ms_until(fmin(sched->last_send + sched->interval,
                sched->deadline), now));
```

The timeout is the earliest of "next send is due" and "the `-t` deadline
expires". `poll()` returns when a packet arrives, or when that instant is
reached, whichever happens first. Nothing blocks indefinitely, nothing
busy-waits, and one thread handles both directions.
---

## Part 3 — Life of one packet

Be able to walk this without notes. It is the single most likely request.

```
main()                                     src/main.c
 └─ ctx_init()                             src/core/ping_context.c
     ├─ parse_info()                       src/cli/ping_parse_args.c
     │   └─ getopt() loop → t_flags
     ├─ htons(getpid())                    → ctx.id
     ├─ resolve_host()                     src/net/ping_resolve.c
     │   └─ getaddrinfo() → struct sockaddr_in
     ├─ inet_ntop()                        → ctx.ipstr, for printing
     ├─ ctx_size_buffers()                 payload_len from -s, recvbuf_len
     └─ ctx_alloc_buffers()                pkt, send_ts, recvbuf
 ├─ print_ping_banner()                    "PING host (ip): 56 data bytes"
 ├─ socket_open()                          src/net/ping_socket.c
 │   ├─ socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)
 │   └─ configure_socket()                 -m, -T, -r, -S
 ├─ signal_install()                       src/core/ping_signal.c
 └─ ping_run()                             src/core/ping_loop.c
     ├─ sched_init()                       interval from -i/-f, deadline from -t
     ├─ send_preload()                     -l burst, unpaced
     ├─ send_one()                         the first paced packet
     └─ loop { loop_once() }
         ├─ poll()                         wait for packet OR next send OR deadline
         ├─ ping_receive()                 src/net/ping_receive.c   (if readable)
         │   ├─ recvfrom(MSG_DONTWAIT)     drain until EAGAIN
         │   ├─ locate_icmp()              bounds-check IP + ICMP headers
         │   ├─ handle_icmp_error()        src/net/ping_icmp_error.c (type != 0)
         │   ├─ record_rtt()               ring lookup, accumulate stats
         │   └─ report_reply()             → print_reply()
         └─ send_if_due()                  clock says it is time → send_one()
 ├─ print_stats()                          src/output/ping_print_report.c
 ├─ close(sockfd)
 └─ ctx_destroy()
```

### On the way out — `send_one()`

```c
int	send_one(int sockfd, t_ping_ctx *ctx, uint16_t seq, t_ping_stats *stats)
{
	t_icmp_hdr	*hdr;
	size_t		pktlen;

	/* Step 1: header fields. id is already in network order (set once in
	   ctx_init), sequence is converted here. */
	pktlen = sizeof(t_icmp_hdr) + ctx->payload_len;
	hdr = (t_icmp_hdr *)ctx->pkt;
	hdr->type = 8;			/* 8 = ICMP echo request */
	hdr->code = 0;
	hdr->id = ctx->id;
	hdr->sequence = htons(seq);
	/* Step 2: payload, and the authoritative send timestamp with it. */
	fill_payload(ctx, seq);
	/* Step 3: checksum, over header AND payload, so it must come last -
	   and the field MUST read zero while it is being computed. */
	hdr->checksum = 0;
	hdr->checksum = ft_checksum(ctx->pkt, pktlen);
	/* Step 4: hand it to the kernel. */
	if (sendto(sockfd, ctx->pkt, pktlen, 0,
			(struct sockaddr *)&ctx->dst, sizeof ctx->dst) == -1)
		return (perror("Send"), -1);
	stats->n_sent++;
	if (ctx->flags.flood && !ctx->flags.quiet)
		print_flood_send();
	return (0);
}
```

**Order is not arbitrary.** The checksum covers header *and* payload, so it must
be computed after the payload is filled and after every header field is final.
Compute it earlier and you send a packet nobody will answer.

### On the way back — the gates

The kernel gives you the IP header too, and its length is variable because IP
options exist. Nothing can be read at a fixed offset.

```c
static int	locate_icmp(const t_ping_ctx *ctx, ssize_t n, struct ip **ip,
		t_icmp_hdr **reply)
{
	int	ihl;

	if (n < (ssize_t)sizeof(struct ip))				/* gate 1: IP hdr present */
		return (-1);
	*ip = (struct ip *)ctx->recvbuf;
	ihl = (*ip)->ip_hl * 4;
	if (ihl < (int)sizeof(struct ip))				/* gate 2: ip_hl is sane */
		return (-1);
	if (n < ihl + (int)sizeof(t_icmp_hdr))			/* gate 3: ICMP hdr present */
		return (-1);
	*reply = (t_icmp_hdr *)(ctx->recvbuf + ihl);
	return (ihl);
}
```

The vocabulary here is worth stealing for other projects:

- A **gate** asks *"do the bytes I am about to read exist?"* It is always a
  shortage check (`<`), and failing it means skip the packet.
- A **filter** asks *"is this packet mine?"* Failing it also means skip, but for
  a completely different reason.

Mixing the two is how buffer over-reads get written. Note `ip_hl * 4` — the
field counts 32-bit words, so 5 means a 20-byte header.

**Why gate 2 exists at all:** `ip_hl` is four bits taken straight off the wire.
A malformed or hostile packet can set it to 0, and `ihl` would be 0, and
`recvbuf + 0` would reinterpret the IP header as an ICMP header. The check that
it is at least `sizeof(struct ip)` closes that.

### The drain loop

```c
while (1)
{
	n = recvfrom(sockfd, ctx->recvbuf, ctx->recvbuf_len, MSG_DONTWAIT,
			NULL, NULL);
	if (n < 0)
		return (classify_recv_error());
	ihl = locate_icmp(ctx, n, &ip, &reply);
	if (ihl < 0)
		continue ;						/* truncated or malformed - skip */
	if (reply->type != 0 || reply->code != 0)	/* 0/0 = ICMP echo reply */
	{
		handle_icmp_error(ctx, ip, ihl, reply, n);
		continue ;
	}
	if (reply->id != ctx->id)			/* filter: somebody else's probe */
		continue ;
	seq = ntohs(reply->sequence);
	rtt = record_rtt(ctx, stats, seq);
	report_reply(ctx, ip, n - ihl, seq, rtt);
}
```

**Why a loop rather than one `recvfrom()` per poll.** `poll()` is
*level-triggered*: it reports "readable", not "one packet available". Two
packets can arrive during a single wait — under `-f` this is the normal case.
Reading one per wakeup would let the queue grow without bound. So drain until
the socket is empty.

**Why `MSG_DONTWAIT` is load-bearing.** Draining means calling `recvfrom()` one
more time than there are packets — the last call is the one that discovers the
queue is empty. Without the flag that call **blocks**, and you have reinvented
the hang you restructured the program to remove. With it, the empty queue
returns `EAGAIN`, which is the loop's normal exit:

```c
static int	classify_recv_error(void)
{
	if (errno == EAGAIN || errno == EWOULDBLOCK)
		return (0);		/* socket drained - the normal way out */
	if (errno == EINTR)
		return (0);		/* SIGINT landed here; the loop will see the flag */
	return (perror("recvfrom"), -1);
}
```

Three outcomes from one negative return: **normal end**, **interrupted**, and
**actual failure**. Only the third is an error.

### Reporting the byte count

```c
report_reply(ctx, ip, n - ihl, seq, rtt);
```

`n - ihl` — what actually arrived, minus the IP header. Deliberately **not**
`ip_len`: on a raw socket that field's byte order and meaning differ between
Linux and the BSDs (macOS historically hands it over in host order, sometimes
with the header length already subtracted). `n` comes from `recvfrom()` and
means the same thing everywhere.

---

## Part 4 — Architecture map

```
include/                one header per module + an umbrella
  ft_ping.h             includes the other six; every .c includes just this
  ping_types.h          structs and tunables, no functions
  ping_cli.h            argv       → t_flags
  ping_net.h            t_ping_ctx → the wire
  ping_output.h         results    → stdout
  ping_core.h           program lifetime and the loop
  ping_utils.h          clocks, fatal exit

src/
  main.c                the order of the run, and nothing else
  cli/
    ping_parse_args.c   the getopt switch, one case per flag
    ping_parse_num.c    strict string → long/double
    ping_parse_pattern.c -p hex decoding
  net/
    ping_socket.c       socket() + the four setsockopt/bind flags
    ping_send.c         build, checksum, sendto
    ping_receive.c      drain, bounds-check, match, time
    ping_icmp_error.c   non-echo-reply ICMP and its quoted probe
    ping_checksum.c     RFC 1071
    ping_resolve.c      getaddrinfo
  output/
    ping_print_packet.c per-packet lines
    ping_print_report.c banner and summary
    ping_print_usage.c  the -? help block
  core/
    ping_context.c      build and tear down t_ping_ctx
    ping_loop.c         the event loop
    ping_signal.c       the stop flag and its handler
  utils/
    ping_time.c         elapsed_ms, now_ms, ms_until
    ping_error.c        fatal_error

tests/                  C unit tests: checksum, resolve, stats
test.sh                 116 behaviour tests over the built binary
```

**The rule that produced this layout:** a file answers one question. If you
cannot name a file's question in a short sentence, it is doing two jobs.

**Why output is its own module.** The network layer decides *what happened*; the
output layer decides *how it looks*. Notice that nothing in `output/` reads the
flags — the `-q` and `-f` decisions are made at the call sites that own them:

```c
static void	report_reply(const t_ping_ctx *ctx, struct ip *ip,
		ssize_t icmp_bytes, uint16_t seq, double rtt)
{
	char	src[INET_ADDRSTRLEN];

	if (ctx->flags.quiet)					/* -q: summary lines only */
		return ;
	if (ctx->flags.flood)					/* -f: erase one sent dot */
	{
		print_flood_recv();
		return ;
	}
	inet_ntop(AF_INET, &ip->ip_src, src, sizeof src);
	print_reply(icmp_bytes, src, seq, ip->ip_ttl, rtt);
}
```

One decision, one place. The alternative — every print function re-checking
`quiet` — is how display rules end up half-applied.

**Why `main()` is fifteen lines.** It is the table of contents for the program:

```c
int	main(int ac, char **av)
{
	t_ping_ctx		ctx;
	t_ping_stats	stats;
	int				sockfd;

	if (ctx_init(&ctx, ac, av) == -1)
		return (EXIT_FAILURE);
	print_ping_banner(&ctx);
	sockfd = socket_open(&ctx.flags);
	if (sockfd == -1)
		return (ctx_destroy(&ctx), EXIT_FAILURE);
	if (signal_install() == -1)
		return (close(sockfd), ctx_destroy(&ctx), EXIT_FAILURE);
	ping_run(sockfd, &ctx, &stats);
	print_stats(ctx.flags.host, &stats);
	close(sockfd);
	ctx_destroy(&ctx);
	if (stats.n_recv == 0)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}
```

Read it to learn **what** happens and in what order; open a module to learn
**how**.

---

## Part 5 — The event loop in detail

### The schedule

```c
typedef struct s_sched
{
	double		interval;	///< ms between paced sends (-i, or -f's floor)
	double		deadline;	///< absolute instant -t expires, INFINITY if unset
	double		last_send;	///< when the previous packet actually went out
	uint16_t	seq;		///< sequence number for the NEXT probe
}	t_sched;
```

Everything is milliseconds as a `double`, in `now_ms()`'s units. One unit
throughout means no `timeval` borrow/carry normalisation anywhere — compare that
with `inetutils-2.0/ping/ping.c`, which does it by hand.

```c
static void	sched_init(t_sched *sched, const t_flags *f)
{
	sched->interval = 1000.0 * PING_INTERVAL_SEC;
	if (f->has_interval)
		sched->interval = 1000.0 * f->interval;
	if (f->flood)
		sched->interval = PING_FLOOD_INTERVAL_MS;	/* -f: the 100/s floor */
	sched->deadline = INFINITY;
	if (f->has_timeout)
		sched->deadline = now_ms() + 1000.0 * f->timeout;
	sched->last_send = 0.0;
	sched->seq = 0;
}
```

`-t` becomes **one absolute instant**, computed once. The alternative —
recomputing "how much time is left" every iteration — accumulates the cost of
each iteration into the deadline and drifts.

`INFINITY` as "no deadline" is what lets the rest of the loop be branch-free
about whether `-t` was given: `fmin(next_send, INFINITY)` is just `next_send`.

### One iteration

```c
static int	loop_once(int sockfd, t_ping_ctx *ctx, t_sched *sched,
		t_ping_stats *stats)
{
	struct pollfd	pfd;
	double			now;
	int				rc;

	now = now_ms();
	if (now >= sched->deadline)						/* -t reached */
		return (-1);
	pfd.fd = sockfd;
	pfd.events = POLLIN;				/* the only event we care about */
	/* Wake for whichever comes first: a packet, the next send, or -t. */
	rc = poll(&pfd, 1, ms_until(fmin(sched->last_send + sched->interval,
					sched->deadline), now));
	if (rc < 0)
	{
		if (errno == EINTR)				/* SIGINT landed while blocked here */
			return (0);					/* -> re-test the stop flag above */
		return (perror("poll"), -1);
	}
	if (rc > 0)							/* readable: drain everything queued */
	{
		if (ping_receive(sockfd, ctx, stats) == -1)
			return (-1);
		if (ctx->flags.exit_on_reply && stats->n_recv > 0)	/* -o */
			return (-1);
	}
	return (send_if_due(sockfd, ctx, sched, stats));
}
```

### The one bug this design exists to prevent

```c
static int	send_if_due(int sockfd, t_ping_ctx *ctx, t_sched *sched,
		t_ping_stats *stats)
{
	double	now;

	now = now_ms();
	if (now >= sched->deadline)						/* -t reached */
		return (-1);
	if (now < sched->last_send + sched->interval)	/* not due yet */
		return (0);
	if (send_one(sockfd, ctx, sched->seq++, stats) == -1)
		return (-1);
	sched->last_send = now_ms();
	return (0);
}
```

**Sending is decided by the clock, never by `poll()`'s return value.**

`poll()` returning 0 means only *"nothing arrived in that window"*. That window
can have been bounded by the `-t` deadline rather than by the send schedule. If
you treat every `poll() == 0` as "time to send", then in the last fraction of a
millisecond before a deadline the loop spins: poll returns instantly, you send,
poll returns instantly, you send. A `-t` run ends in a burst of hundreds of
packets. Asking the clock instead makes that impossible.

**Why `last_send` is re-read after the send** rather than reusing `now`:
`sendto()` itself takes time. Pacing from the instant *before* the syscall makes
every interval short by however long the call lasted, and the error accumulates.

### Rounding up in the poll timeout

```c
int	ms_until(double target, double now)
{
	double	d;

	d = target - now;
	if (d < 0.0)
		return (0);
	if (d > PING_POLL_MAX_MS)
		return ((int)PING_POLL_MAX_MS);
	return ((int)ceil(d));
}
```

Three clamps, three distinct bugs prevented:

- **Negative → 0.** `poll()` reads a negative timeout as *block forever*. A
  target already in the past would hang the program.
- **Upper clamp.** With no `-t`, the target is `INFINITY`; casting that to `int`
  is undefined behaviour.
- **`ceil`, not truncation.** With 0.4 ms left, truncation gives 0, `poll()`
  returns immediately with the target still in the future, and the loop spins on
  that sub-millisecond remainder. Rounding up guarantees forward progress.

### The signal handler

```c
static volatile sig_atomic_t	g_stop = 0;

static void	on_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

int	signal_install(void)
{
	struct sigaction	sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_sigint;
	if (sigaction(SIGINT, &sa, NULL) == -1)
		return (perror("sigaction"), -1);
	return (0);
}
```

- **`volatile`** — without it the compiler may cache the flag in a register and
  the loop never observes the change.
- **`sig_atomic_t`** — the only type the standard guarantees can be written
  from a handler without a torn read.
- **One assignment and nothing else.** Only *async-signal-safe* functions may be
  called from a handler. `printf` is not: it takes a lock on the stream, and if
  the signal arrived while the main flow already held that lock, the process
  deadlocks. Setting a flag and returning is the standard pattern; the summary
  is printed by `main()` afterwards.
- **`sa_flags` deliberately left at 0** — i.e. **no `SA_RESTART`**. With
  `SA_RESTART`, a `poll()` interrupted by the signal is silently restarted and
  the loop would not notice the stop until the current wait finished. Under
  `-i 30` that is a thirty-second delay before Ctrl+C takes effect. Without it,
  `poll()` returns `EINTR`, the loop head re-tests the flag, and the program
  exits at once. `test.sh` has a test for exactly this.

The flag is file-static and reached only through accessors, so the send path can
request a stop through the same route the handler uses:

```c
int  ping_should_stop(void) { return (g_stop != 0); }
void ping_request_stop(void) { g_stop = 1; }
```
---

## Part 6 — The data structures

Three structs, each with one job.

### `t_flags` — what the user asked for

Every option that takes a value gets a paired `has_*` flag:

```c
int		has_ttl;				///< -m <ttl> given?
int		ttl;					///< -m  IP TTL for outgoing packets
```

**Why not a sentinel value?** Because `-m 0` is a legal TTL, `-s 0` is a legal
payload size and `-l 0` is a legal preload. Any sentinel you pick is a value
somebody can legitimately pass. Two fields make "not given" and "given as zero"
distinguishable, and cost one `int`.

### `t_ping_ctx` — everything both paths need

```c
typedef struct s_ping_ctx
{
	t_flags				flags;
	struct sockaddr_in	dst;
	char				ipstr[INET_ADDRSTRLEN];
	uint16_t			id;
	uint8_t				*pkt;
	size_t				payload_len;
	struct timespec		*send_ts;
	uint8_t				*recvbuf;
	size_t				recvbuf_len;
}	t_ping_ctx;
```

Built once by `ctx_init()`, then treated as read-only — the receive path takes
it as `const t_ping_ctx *`, which the compiler enforces. Bundling is what keeps
`send_one()` at four parameters instead of nine.

### `t_ping_stats` — the running totals

```c
typedef struct s_ping_stats
{
	int		n_sent;
	int		n_recv;
	double	sum;		/* sum of RTTs, ms - for the mean */
	double	sum_sq;		/* sum of RTT^2, ms^2 - for stddev */
	double	rtt_min;
	double	rtt_max;
}	t_ping_stats;
```

**Why sums and not a list of samples.** Standard deviation can be computed from
running totals alone:

```
variance = E[x²] − E[x]²  =  sum_sq/n − (sum/n)²
```

So memory is **constant** no matter how long the run lasts. Store every sample
instead and a flood ping for an hour is an unbounded allocation. This is the
same trick used by the reference implementation, and it is worth naming: it is
called a *streaming* or *online* algorithm.

**The floating-point trap, and why the guard exists:**

```c
variance = stats->sum_sq / stats->n_recv - mean * mean;
if (variance < 0.0)
	variance = 0.0;
```

Mathematically `E[x²] ≥ E[x]²` always, so variance cannot be negative. In
floating point, when every sample is nearly identical, the two terms are nearly
equal and their difference is dominated by rounding error — it can land at
`-1e-18`. `sqrt()` of that is `NaN`, and your summary line prints `nan`.
Pinning it to zero costs one comparison.

*(This is the classic weakness of the "sum of squares" method. The alternative —
Welford's online algorithm — is numerically stable but needs a different update
rule. Worth mentioning if an evaluator pushes: you know why the guard is there
and what the more robust option is called.)*

**Why min/max are seeded from the first sample:**

```c
if (stats->n_recv == 1 || rtt < stats->rtt_min)
	stats->rtt_min = rtt;
```

Not from `INFINITY`/`0.0` sentinels — `n_recv == 1` is the seed condition. It
reads as one rule instead of a magic initial value that must be kept in sync
with the struct's zeroing.

---

## Part 7 — Flag-by-flag reference

Fourteen flags. For each: what it does, where it lives, and the thing an
evaluator is most likely to poke at.

### `-f` — flood

*Outputs packets as fast as they come back or 100 times per second, whichever is
more. A `.` per request, a backspace per reply. Root only.*

**Parse** — `src/cli/ping_parse_args.c`:

```c
case 'f':
	if (!require_root(opt, "flood ping"))
		return (0);
	flags->flood = 1;
	break ;
```

**Pacing** — `src/core/ping_loop.c`:

```c
if (f->flood)
	sched->interval = PING_FLOOD_INTERVAL_MS;	/* -f: the 100/s floor */
```

**Display** — `src/output/ping_print_packet.c`:

```c
void	print_flood_send(void)
{
	putchar('.');
	fflush(stdout);
}

void	print_flood_recv(void)
{
	putchar('\b');
	fflush(stdout);
}
```

> **Why `fflush`.** `stdout` is line-buffered on a terminal but *block*-buffered
> when piped. Without the flush, `./ft_ping -f host | cat` shows nothing until
> the buffer fills or the program ends — which defeats the point of a live
> display.
>
> **Why the dots and backspaces work.** Every request leaves a `.`; every reply
> erases one. What remains on screen at any moment is the number of packets
> still outstanding. Visual packet loss with no arithmetic.
>
> **Honest divergence:** the man page says *"as fast as they come back or 100
> per second, whichever is more"*. This implementation does the 100/s floor but
> does **not** additionally send on every reply. On a fast local link real
> flood ping goes faster than this one. Say so if asked — it is a known,
> deliberate simplification, not an oversight.

### `-l <preload>` — burst before pacing

*Send that many packets as fast as possible before falling into normal mode.
Root only.*

**Loop** — `src/core/ping_loop.c`:

```c
static void	send_preload(int sockfd, t_ping_ctx *ctx, t_sched *sched,
		t_ping_stats *stats)
{
	int	i;

	i = 0;
	while (i < ctx->flags.preload && !ping_should_stop())
	{
		if (send_one(sockfd, ctx, sched->seq++, stats) == -1)
			ping_request_stop();
		i++;
	}
}
```

> **The gotcha:** the burst deliberately ignores the schedule — no `poll()`, no
> interval — and `sched->last_send` is only set *after* it, from the first paced
> packet. So the preload does not push the normal schedule around.
>
> Note it advances the same `sched->seq`, so sequence numbers stay contiguous
> across the boundary.

### `-i <wait>` — interval

*Wait this many seconds between packets. Fractional allowed; below 0.002 s needs
root. Incompatible with `-f`.*

```c
case 'i':
	if (!test_strton(optarg, 0, &d_tmp) || d_tmp <= 0)
		return (invalid_value(optarg, opt));
	if (d_tmp < 0.002 && geteuid() != 0)
		return (fprintf(stderr,
				"ft_ping: -i: intervals below 2ms require root\n"), 0);
	flags->has_interval = 1;
	flags->interval = d_tmp;
	break ;
```

> **The privilege check is on the value, not the flag.** `-i 0.5` works for
> anyone; only sub-2 ms does not. Getting this backwards — refusing `-i`
> outright to non-root — is a common mistake, and `test.sh` has a test for it
> (*"-i above 2ms does NOT require root"*).
>
> **Why incompatible with `-f`:** `-f` defines its own interval. Accepting both
> would silently ignore one of them, so it is rejected in `finalise_flags()`.

### `-m <ttl>` — IP time to live

*Set the IP TTL for outgoing packets.*

**Socket setup** — `src/net/ping_socket.c`:

```c
if (f->has_ttl && setsockopt(sockfd, IPPROTO_IP, IP_TTL,
		&f->ttl, sizeof f->ttl) == -1)
	return (perror("setsockopt IP_TTL"), -1);
```

> **What TTL actually is.** Not a duration — a *hop count*. Every router
> decrements it and discards the packet at zero, sending back an ICMP **Time
> Exceeded** (type 11). It exists so a routing loop cannot circulate a packet
> forever.
>
> **Why this is the best flag to demo.** `./ft_ping -m 1 8.8.8.8` makes your
> first-hop router generate the error, which exercises the entire ICMP-error
> path — quoted headers and all — on demand. It is also how `traceroute` works:
> send with TTL 1, 2, 3… and record who complains each time.

### `-T <ttl>` — multicast TTL

*Set the IP TTL for multicasted packets. Only applies to a multicast
destination.*

```c
if (f->has_multicast_ttl && setsockopt(sockfd, IPPROTO_IP,
		IP_MULTICAST_TTL, &f->multicast_ttl,
		sizeof f->multicast_ttl) == -1)
	return (perror("setsockopt IP_MULTICAST_TTL"), -1);
```

> **Why a separate option from `-m`.** Multicast TTL is a *scope* control rather
> than a loop guard: 0 stays on the host, 1 stays on the local subnet, 32 the
> site, 255 unrestricted. The kernel keeps it in a different socket option, so
> setting `IP_TTL` would not affect multicast at all.

### `-o` — exit on first reply

*Exit successfully after receiving one reply packet.*

```c
if (rc > 0)
{
	if (ping_receive(sockfd, ctx, stats) == -1)
		return (-1);
	if (ctx->flags.exit_on_reply && stats->n_recv > 0)	/* -o */
		return (-1);
}
```

> **Checked after the drain, not inside it**, so the reply that satisfies `-o`
> is fully counted and printed before the loop ends. Returning `-1` here means
> "stop looping" — `ping_run()` still returns normally and `print_stats()` still
> runs.
>
> This is the flag that makes ft_ping usable in a script: `if ./ft_ping -o -t 2
> host; then …`.

### `-Q` — quiet errors

*Don't display ICMP error messages that are in response to our own queries.*

```c
if (ctx->flags.quiet || ctx->flags.quiet_errors)
	return ;					/* -q or -Q suppresses this line */
```

> **`-Q` versus `-v` versus neither** — three levels, and the evaluator may well
> ask you to distinguish them:
>
> | | errors caused by *your* probes | errors caused by *other* processes' probes |
> |---|---|---|
> | `-Q` | hidden | hidden |
> | default | shown | hidden |
> | `-v` | shown | shown |

### `-q` — quiet

*Nothing displayed except the summary lines.*

```c
if (flags->quiet)
{
	flags->quiet_errors = 0;
	flags->verbose = 0;
}
```

> **`-q` silently wins over `-Q` and `-v`, in either order**, matching real
> ping. It is resolved at *parse* time rather than at each print site, so the
> rest of the program never has to reason about the precedence. The two
> order-independence tests in `test.sh` exist for this.

### `-r` — bypass routing

*Send directly to a host on an attached network, ignoring the routing table.*

```c
if (f->bypass_routing)
{
	on = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof on) == -1)
		return (perror("setsockopt SO_DONTROUTE"), -1);
}
```

> **What it is for.** Diagnosing a link where the routing table is the suspect —
> you can reach a neighbour on the same wire even if no route to it exists. If
> the destination is *not* directly attached, the send fails rather than being
> routed.

### `-p <pattern>` — payload pad bytes

*Up to 16 pad bytes to fill out the packet. Useful for diagnosing
data-dependent problems.*

**Decode** — `src/cli/ping_parse_pattern.c`:

```c
int	decode_pattern(const char *hex, uint8_t *out, int *out_len)
{
	size_t	len;
	size_t	i;
	char	byte_str[3];

	len = strlen(hex);
	if (!is_valid_hex(hex, len))
		return (0);
	i = 0;
	while (i < len)
	{
		byte_str[0] = hex[i];
		byte_str[1] = hex[i + 1];
		byte_str[2] = '\0';
		out[i / 2] = (uint8_t)strtol(byte_str, NULL, 16);
		i += 2;
	}
	*out_len = (int)(len / 2);
	return (1);
}
```

**Fill** — `src/net/ping_send.c`:

```c
static void	fill_pattern(t_ping_ctx *ctx, uint8_t *payload)
{
	size_t	i;

	i = 0;
	while (i < ctx->payload_len)
	{
		payload[i] = ctx->flags.pattern[i % (size_t)ctx->flags.pattern_len];
		i++;
	}
}
```

> **Validation completes before the first byte is written**, so a rejected
> pattern never leaves half of `out` modified.
>
> **Two digits into a NUL-terminated scratch string**, so `strtol` converts
> exactly one byte and cannot run past it.
>
> **What the flag is actually for.** Some links and some hardware fail on
> specific bit patterns — long runs of ones, or of zeros, defeat the clock
> recovery in certain physical layers. `-p ff` fills the packet with all ones so
> you can find out. This is why the flag deliberately does *not* write a
> timestamp into the payload: that would corrupt the very thing under test.

### `-S <src_addr>` — source address

*Use this IP as the source address in outgoing packets.*

```c
if (f->has_source_addr)
{
	memset(&src, 0, sizeof src);
	if (resolve_host(f->source_addr, &src) == -1)
		return (-1);	/* resolve_host() already printed the error */
	if (bind(sockfd, (struct sockaddr *)&src, sizeof src) == -1)
		return (perror("bind"), -1);
}
```

> **`bind()` is what makes it work.** Binding fixes the local address, and the
> kernel then stamps that into every outgoing packet. There is no
> "set source address" socket option — this *is* the mechanism.
>
> **Why `resolve_host()` is reused** rather than `inet_pton()`: it accepts a
> hostname as well as a literal, and it already prints a correctly-formatted
> error. One function, two callers.
>
> **Why it fails for an address you do not own:** you cannot bind to an address
> no interface has. The error is `EADDRNOTAVAIL` — *"Cannot assign requested
> address"*. `test.sh` checks that this fails cleanly and sends nothing.

### `-s <packetsize>` — payload size

*Number of data bytes to send. Default 56, which becomes 64 ICMP bytes with the
8-byte header.*

```c
case 's':
	/* 65507 = 65535 (max IP datagram) - 20 (IP hdr) - 8 (ICMP). */
	if (!opt_long(optarg, opt, 0, 65507, &tmp))
		return (0);
	flags->has_packet_size = 1;
	flags->packet_size = (int)tmp;
	break ;
```

**Sizing** — `src/core/ping_context.c`:

```c
static void	ctx_size_buffers(t_ping_ctx *ctx)
{
	ctx->payload_len = PING_PAYLOAD_SIZE;
	if (ctx->flags.has_packet_size)
		ctx->payload_len = (size_t)ctx->flags.packet_size;
	ctx->recvbuf_len = ctx->payload_len + PING_RECVBUF_SLACK;
	if (ctx->recvbuf_len < PING_RECVBUF_MIN)
		ctx->recvbuf_len = PING_RECVBUF_MIN;
}
```

> **Where 65507 comes from** — be ready to derive it: an IPv4 datagram is at most
> 65535 bytes; the IP header is 20 without options; the ICMP header is 8.
> 65535 − 20 − 8 = 65507.
>
> **Why the receive buffer has a floor.** With `-s 0` the payload is nothing, but
> an inbound ICMP *error* still carries its own IP header, its ICMP header, and a
> quoted copy of your original IP + ICMP headers. Sizing the receive buffer from
> the payload alone would truncate those. `PING_RECVBUF_MIN` (1024) is the floor.
>
> **What large `-s` demonstrates.** Above the interface MTU (typically 1500) the
> packet is fragmented by IP and reassembled by the receiver. Try `-s 2000` and
> watch it in `tcpdump` — that is IP fragmentation, a layer the mandatory part
> never makes you think about.

### `-t <timeout>` — deadline

*Exit after this many seconds regardless of how many packets were received.*

```c
sched->deadline = INFINITY;
if (f->has_timeout)
	sched->deadline = now_ms() + 1000.0 * f->timeout;
```

> Checked in **two** places per iteration — before the `poll()` and again in
> `send_if_due()` — because the deadline can fall *during* the wait. Checking
> only before would let one more packet out after expiry.

### `-v` — verbose

*ICMP packets other than echo reply that are received are listed.*

```c
if (inner_icmp->id != ctx->id && !ctx->flags.verbose)
	return ;					/* somebody else's probe - -v only */
```

**Banner** — `src/output/ping_print_report.c`:

```c
if (ctx->flags.verbose)
{
	id = ntohs(ctx->id);
	printf(", id 0x%04x = %u", id, id);
}
```

```
PING 127.0.0.1 (127.0.0.1): 56 data bytes, id 0x0e14 = 3604
```

> **Two distinct effects.** The banner gains the ICMP identifier, in hex (how
> you read it off a `tcpdump` capture) and decimal (what `ps` shows for the
> PID) — so you can confirm at a glance that your identifier really is your
> process id. And the "was this error caused by *my* probe?" filter is lifted,
> so ICMP errors provoked by other processes on the machine are reported too.
>
> Since a raw ICMP socket receives *every* ICMP packet arriving at the host,
> that second effect is genuinely useful: on a busy machine it shows you the
> whole conversation.
>
### Rejected combinations

```c
static int	finalise_flags(int ac, char **av, t_flags *flags)
{
	if (flags->has_interval && flags->flood)
		return (fprintf(stderr, "invalid combination: -f + -i\n"), 0);
	if (flags->quiet)
	{
		flags->quiet_errors = 0;
		flags->verbose = 0;
	}
	if (optind >= ac)
		return (fprintf(stderr, "ft_ping: missing host operand\n"), 0);
	flags->host = av[optind];
	return (1);
}
```

> Cross-option rules can only be checked once every option has been seen — `-i 1
> -f` and `-f -i 1` must both be rejected, and a rule inside the switch would
> only catch one order.

---

## Part 8 — Errors and the "never crash" clause

The subject is explicit: the program must never exit unexpectedly — no segfault,
no bus error, no uncaught exception. Four categories, four strategies.

**1. Bad input** — rejected during parsing, before anything is allocated or
opened. One message, exit 1, nothing printed first.

**2. Failed syscalls** — reported with `perror` and unwound:

```c
if (sendto(...) == -1)
	return (perror("Send"), -1);
```

`perror` prints your prefix plus `strerror(errno)`, so the user sees
`Send: Network is unreachable` rather than a bare failure.

**3. Malformed packets from the network** — the most important category,
because it is the only one an attacker controls. Every field read from the wire
is bounds-checked before it is used. Nothing is trusted:

- `n` versus `sizeof(struct ip)` — is there an IP header at all?
- `ip_hl * 4` versus `sizeof(struct ip)` — is the claimed header length sane?
- `n` versus `ihl + 8` — is there an ICMP header?
- and inside an ICMP error, the same three questions again for the *quoted*
  headers:

```c
static t_icmp_hdr	*quoted_probe(t_icmp_hdr *reply, int ihl, ssize_t n)
{
	struct ip	*inner_ip;
	int			inner_ihl;
	int			quote_off;

	quote_off = ihl + (int)sizeof(t_icmp_hdr);
	if (n < quote_off + (int)sizeof(struct ip))		/* quoted IP hdr, min size */
		return (NULL);
	inner_ip = (struct ip *)((uint8_t *)reply + sizeof(t_icmp_hdr));
	inner_ihl = inner_ip->ip_hl * 4;
	if (inner_ihl < (int)sizeof(struct ip))			/* quoted ip_hl is sane */
		return (NULL);
	if (n < quote_off + inner_ihl + (int)sizeof(t_icmp_hdr))
		return (NULL);								/* quoted ICMP hdr truncated */
	return ((t_icmp_hdr *)((uint8_t *)inner_ip + inner_ihl));
}
```

> **Why routers may quote less than you expect.** RFC 792 asks for the IP header
> plus 8 bytes; RFC 1812 asks for as much as fits. Implementations vary. A
> truncated quote is normal traffic, not an attack — so it is skipped silently
> rather than reported.

**4. Signals** — a flag, and nothing else, from the handler.

### What an ICMP error looks like on the wire

```
+---------------------------+  ← recvbuf
| IP header (ihl bytes)     |    the ERROR packet's own IP header
+---------------------------+
| ICMP header (8 bytes)     |    type/code say what went wrong
+---------------------------+
| quoted IP header          |  ← the packet YOU sent, echoed back
+---------------------------+
| quoted ICMP header (8 B)  |  ← your type / id / sequence
+---------------------------+
```

The quoted id is how you confirm the error was caused by *you*. The quoted
sequence is how you report *which* probe provoked it. And crucially:

```c
/* This is NOT a reply: the stats counters are deliberately left untouched,
   so the probe it refers to still counts as loss in the final summary -
   same as a packet that never came back at all. */
```

An error is **not** a reply. `n_recv` is not incremented, no RTT is recorded,
and the probe counts as lost. That is why `./ft_ping -t 3 -m 1 8.8.8.8` prints
error lines *and* 100% packet loss, and exits 1.

---

## Part 9 — Memory

Three heap allocations, all made once, all freed in one place.

| Buffer | Size | Why heap, not stack |
|---|---|---|
| `pkt` | `8 + payload_len` | `-s` makes the size a runtime value |
| `send_ts` | `65536 × sizeof(struct timespec)` ≈ 1 MB | far too large for a stack frame |
| `recvbuf` | `max(payload_len + 128, 1024)` | sized from `-s` at runtime |

```c
static int	ctx_alloc_buffers(t_ping_ctx *ctx)
{
	ctx->pkt = malloc(sizeof(t_icmp_hdr) + ctx->payload_len);
	ctx->send_ts = calloc(PING_TS_RING, sizeof(struct timespec));
	ctx->recvbuf = malloc(ctx->recvbuf_len);
	if (!ctx->pkt || !ctx->send_ts || !ctx->recvbuf)
		return (perror("malloc"), -1);
	return (0);
}

void	ctx_destroy(t_ping_ctx *ctx)
{
	free(ctx->pkt);
	free(ctx->send_ts);
	free(ctx->recvbuf);
	ctx->pkt = NULL;
	ctx->send_ts = NULL;
	ctx->recvbuf = NULL;
}
```

> **All three checked together, all three freed together.** A partial failure
> never leaves a half-built context, because `free(NULL)` is a no-op and
> `ctx_destroy()` handles the case where only one or two succeeded.
>
> **Why `malloc` removed an alignment problem.** The receive buffer used to be a
> fixed stack array that needed `_Alignas` so `struct ip` could be read out of
> it without a misaligned access. `malloc` is *guaranteed* to return memory
> aligned for any type, so the alignment specifier went away with the array.

Proof, not assertion:

```bash
sudo valgrind --leak-check=full --track-fds=yes ./ft_ping -t 3 127.0.0.1
# ==…== total heap usage: 5 allocs, 5 frees, 1,053,824 bytes allocated
# ==…== All heap blocks were freed -- no leaks are possible
```

*(5 allocations rather than 3 — `getaddrinfo` allocates internally and is freed
by `freeaddrinfo`. Know that, in case the number is queried.)*

---

## Part 10 — How this is tested

Two layers, because they catch different things.

**C unit tests** (`tests/`, `make test`) — functions in isolation: `test_checksum`,
`test_resolve` and `test_stats`. The checksum test uses the self-verifying
property: insert the checksum into the message,
re-run it over the whole thing, and a correct implementation yields zero.

```c
uint16_t c = ft_checksum(buf, sizeof buf);
memcpy(buf + 2, &c, 2);
assert(ft_checksum(buf, sizeof buf) == 0);
```

That single assertion is worth more than a table of expected values, because it
tests the *property* rather than one input.

**Behaviour tests** (`test.sh`, 116 cases) — the built binary, from outside:
every flag rejected for every bad value, every output format, exit status, each
flag's runtime effect, flag interactions, SIGINT handling, and privilege
refusals.

Three details worth mentioning if testing comes up:

1. **It probes its environment first.** No root, no internet, or an address that
   answers when it should not — each becomes an honest `SKIP` with a reason,
   never a false `FAIL`.
2. **The privilege tests drop root rather than skipping.** Run under `sudo`, the
   suite re-runs the four "requires root" refusals as `sudo -u $SUDO_USER` — the
   checks most likely to be missing are the ones a root-only run would silently
   skip.
3. **Failures print the command, the exit status, every unmet expectation and
   the output.** A failing run tells you what to fix without reproducing it.

```
  FAIL  -s 100 is reflected in the banner
        command : ./ft_ping -t 2 -s 100 127.0.0.1
        exit    : 0
        why     : output should contain a line matching: ^PING .*: 100 data bytes$
        output  :
        | PING 127.0.0.1 (127.0.0.1): 56 data bytes
```

---

## Part 11 — Live demo script

Rehearse this. Ten commands, in this order, each showing one thing.

```bash
# 1. It builds clean, with warnings as errors.
make re

# 2. Baseline against the reference — the output should be identical
#    except for the timings.
ping-ref -c 3 127.0.0.1
sudo ./ft_ping -t 4 127.0.0.1

# 3. Name resolution, not just literals.
sudo ./ft_ping -t 3 google.com

# 4. Loss is reported rather than hanging — the bug the loop exists to avoid.
sudo ./ft_ping -t 4 192.0.2.1          # RFC 5737, routable nowhere
echo "exit status: $?"                 # 1

# 5. Ctrl+C prints the summary instead of dying.
sudo ./ft_ping 127.0.0.1               # then hit Ctrl+C

# 6. The ICMP error path, on demand.
sudo ./ft_ping -t 4 -m 1 8.8.8.8       # "Time to live exceeded", 100% loss

# 7. Payload size, and what it does to the reported byte count.
sudo ./ft_ping -t 2 -s 0 127.0.0.1     # 8 bytes from …
sudo ./ft_ping -t 2 -s 100 127.0.0.1   # 108 bytes from …

# 8. What actually goes on the wire.
sudo tcpdump -i lo -c 2 -x icmp &
sudo ./ft_ping -t 2 -p deadbeef -s 16 127.0.0.1

# 9. No leaks, no leaked descriptors.
sudo valgrind --leak-check=full --track-fds=yes ./ft_ping -t 3 127.0.0.1

# 10. Help, and the reference-parity fixes.
./ft_ping '-?'                         # usage, exit 0, no root needed
./ft_ping -t 2 -v 127.0.0.1 | head -1  # banner carries the ICMP id
./ft_ping -t 2 127.0.0.1 | cat -A | tail -3   # stddev, no blank line

# 11. The whole test suite: 3 unit binaries + 116 behaviour cases.
make test
sudo ./test.sh --deep
```

**If something fails, say what you see before you touch the keyboard.** "That's
the deadline check firing twice — let me show you where" is a much better
answer than silently editing.
---

## Part 12 — Questions an evaluator could ask

Answers are collapsed. Read the question, answer it out loud, *then* open the
answer. Reading them straight through teaches you much less.

### Easy — the foundations

<details><summary><b>E1.</b> What is ICMP and where does it sit in the stack?</summary>

Internet Control Message Protocol, IP protocol number 1. It sits directly on top
of IP, at the same level as TCP and UDP rather than above them. It carries
control and diagnostic messages — "host unreachable", "time exceeded", "echo
request" — not user data. It has no ports, because it is not multiplexing
application traffic.
</details>

<details><summary><b>E2.</b> What exactly does ping send and what does it expect back?</summary>

It sends an **ICMP echo request**: type 8, code 0, followed by an identifier, a
sequence number and an arbitrary payload. It expects an **echo reply**: type 0,
code 0, with the same identifier, the same sequence number and the same payload
echoed back unchanged.
</details>

<details><summary><b>E3.</b> Why does ft_ping need root?</summary>

It uses a raw socket (`SOCK_RAW`), which allows constructing arbitrary IP-level
traffic and reading every packet of that protocol arriving at the host. That is
both a spoofing and a snooping capability, so the kernel gates it behind
`CAP_NET_RAW` — root, or a binary granted the capability with
`setcap cap_net_raw+ep`.
</details>

<details><summary><b>E4.</b> Why does the default show "64 bytes" when the default payload is 56?</summary>

56 bytes of payload plus the 8-byte ICMP header. The reported figure is the
whole ICMP message, header included; the `-s` value is payload only. `-s 100`
therefore reports 108.
</details>

<details><summary><b>E5.</b> What is the identifier for?</summary>

Telling your traffic apart from everyone else's. ICMP has no ports, so every
ICMP packet arriving at the machine is delivered to every raw ICMP socket. The
identifier — the PID here — is the only thing marking a reply as yours.
</details>

<details><summary><b>E6.</b> What is the sequence number for?</summary>

Identifying *which* of your probes a reply belongs to. It is what lets you match
a reply to its send time so you can compute an RTT, and it is what tells you
which specific packet was lost.
</details>

<details><summary><b>E7.</b> What is TTL?</summary>

A hop count, not a duration. Every router that forwards the packet decrements
it; at zero the packet is discarded and the router sends back an ICMP Time
Exceeded. It exists so a routing loop cannot circulate a packet forever. The TTL
shown on a reply is the value remaining when it reached you, which is why a
distant host shows a lower number.
</details>

<details><summary><b>E8.</b> What is RTT and how do you measure it?</summary>

Round-trip time: send instant to reply instant. Record a monotonic timestamp
when the request goes out, take another when the reply arrives, subtract. It
includes both directions plus however long the remote host took to turn the
packet around.
</details>

<details><summary><b>E9.</b> What does your program's exit status mean?</summary>

0 if at least one reply was received; 1 for any setup error (bad arguments,
unresolvable host, socket failure) and also when every packet was lost. That
matches ping's convention and makes `if ./ft_ping -o -t 2 host; then …` a valid
reachability test in a script.
</details>

<details><summary><b>E10.</b> What does <code>htons()</code> do and where do you use it?</summary>

Host TO Network Short: converts a 16-bit value from the host's byte order to
network byte order, which is big-endian. It is used on the identifier (once, at
startup) and on the sequence number (once per send). On a big-endian machine it
is a no-op; writing it anyway is what makes the code portable.
</details>

<details><summary><b>E11.</b> How do you turn "google.com" into an address?</summary>

`getaddrinfo()`, with `ai_family = AF_INET` to constrain it to IPv4 and
`ai_socktype = SOCK_RAW` to collapse the otherwise duplicated per-socktype
results. It handles both dotted-quad literals and FQDNs through one call, and it
returns a linked list that must be released with `freeaddrinfo()` — but only on
success; on failure the list was never allocated.
</details>

<details><summary><b>E12.</b> What does <code>-q</code> do?</summary>

Suppresses everything except the opening banner and the closing summary — no
per-packet lines, no ICMP error lines. It also silently overrides `-Q` and `-v`,
in either order, which is resolved once at parse time.
</details>

<details><summary><b>E13.</b> Why 127.0.0.1 for testing?</summary>

The loopback interface. It never leaves the machine, so it needs no network,
has sub-millisecond and very stable RTTs, and always answers — which makes it
the right target for testing everything except loss and the ICMP error paths.
</details>

<details><summary><b>E14.</b> What is <code>perror</code> doing for you?</summary>

Printing your prefix, then a colon, then `strerror(errno)` for the failure that
just happened. So `perror("bind")` on an address you do not own prints
`bind: Cannot assign requested address` — the user learns what went wrong, not
just that something did.
</details>

<details><summary><b>E15.</b> Why is the ICMP header a struct rather than byte offsets?</summary>

Readability and type safety: `hdr->sequence` says what it is, where
`buf[6] << 8 | buf[7]` does not. The trade is that the compiler is free to add
padding, which is why the `_Static_assert` on `sizeof == 8` is there to make
that impossible to do silently.
</details>

<details><summary><b>E16.</b> What is a socket, in one sentence?</summary>

A file descriptor that represents one endpoint of a network communication, which
you read from and write to with the same kinds of calls you use on files —
except it also carries an address, a protocol and a set of options.
</details>

<details><summary><b>E17.</b> Why <code>sendto()</code> rather than <code>send()</code>?</summary>

The socket is never connected, so it has no default destination — `sendto()`
supplies one per call. You *could* `connect()` a raw socket and then use
`send()`, but keeping it unconnected is what lets the same socket receive ICMP
from any source, which the error path depends on.
</details>

<details><summary><b>E18.</b> What is the payload actually for?</summary>

Three things. It sets the packet size, so you can test how a link behaves with
large or fragmented packets. It is echoed back unchanged, so corrupting it
proves a data-dependent link fault — that is what `-p` is for. And historically
it carried the send timestamp, which is what real ping does and this
implementation deliberately does not.
</details>

---

### Medium — design decisions

<details><summary><b>M1.</b> Why <code>poll()</code> instead of a plain blocking <code>recvfrom()</code>?</summary>

Because a blocking `recvfrom()` waits forever when a packet is lost. The program
would hang on the first drop and never send again — so it could never report
loss, which is the main thing ping exists to do.

`poll()` waits for *whichever comes first*: a packet arriving, the next send
falling due, or the `-t` deadline. Nothing blocks indefinitely and nothing
busy-waits.
</details>

<details><summary><b>M2.</b> Why <code>MSG_DONTWAIT</code>, when <code>poll()</code> already said the socket was readable?</summary>

Because the receive path drains the socket in a loop, and draining means calling
`recvfrom()` one more time than there are packets — the last call is the one
that discovers the queue is empty. Without the flag, that call blocks, and you
have reinvented the hang you were avoiding. With it, an empty queue returns
`EAGAIN`, which is the loop's normal exit.
</details>

<details><summary><b>M3.</b> Why drain in a loop at all?</summary>

`poll()` is level-triggered: it reports "readable", not "exactly one packet
available". Two or more packets can arrive during a single wait — under `-f`
that is the normal case. Handling one per wakeup would let the receive queue
grow without bound and the reported RTTs would drift ever further behind
reality.
</details>

<details><summary><b>M4.</b> Why <code>CLOCK_MONOTONIC</code> and not <code>CLOCK_REALTIME</code>?</summary>

The realtime clock can jump — NTP steps it, a daylight-saving change moves it,
an administrator sets it. Any of those produces a negative RTT or a deadline
that never expires. The monotonic clock only moves forward, which is the only
property RTTs and deadlines need. It has no meaningful absolute value, but
nothing here needs one.
</details>

<details><summary><b>M5.</b> Why must the checksum field be zero before you compute the checksum?</summary>

The checksum covers the header including its own field. Both ends have to agree
on what that field contained during the computation, and the convention is zero.
The receiver then recomputes over the whole message *with* the checksum in
place; a correct message sums to `0xFFFF` and complements to 0. Leaving a stale
value there produces a checksum nobody can verify.
</details>

<details><summary><b>M6.</b> Why doesn't the checksum need <code>htons()</code>?</summary>

Because the one's-complement sum is byte-order agnostic: swapping every 16-bit
word of the input swaps the sum, and therefore swaps the complement. Compute it
on a little-endian or a big-endian host and the bytes that land on the wire are
identical. That is a deliberate property of RFC 1071, not an accident.
</details>

<details><summary><b>M7.</b> Why <code>ip_hl * 4</code>?</summary>

`ip_hl` is a 4-bit field counting 32-bit words, so the byte length is four times
it. A header with no options is 5 words — 20 bytes. It has to be read from the
packet rather than assumed, because IP options make the header length variable,
and nothing after it can be read at a fixed offset.
</details>

<details><summary><b>M8.</b> Why do you check that <code>ip_hl</code> is sane?</summary>

It is four bits taken straight off the wire, so a malformed or hostile packet
can set it to 0. Then `ihl` is 0, and `recvbuf + 0` reinterprets the IP header
as an ICMP header — reading fields that are not what you think they are. The
check that it is at least `sizeof(struct ip)` closes that.
</details>

<details><summary><b>M9.</b> You store send timestamps in a 1 MB ring instead of in the payload. Why?</summary>

Three reasons. `-s` can make the payload smaller than a `struct timespec`, so
there may be nowhere to put one. `-p` deliberately overwrites the payload, and
writing a timestamp into it would defeat the flag. And most importantly, the
payload of a reply is data a *remote machine* handed back — a hostile or broken
host could echo anything and you would compute an RTT from it. The ring is your
own memory; nothing on the network can corrupt it.

The cost is 65536 × 16 bytes ≈ 1 MB, allocated once. That is one slot per
possible sequence number, so the index is just `seq % 65536`.
</details>

<details><summary><b>M10.</b> Why <code>calloc</code> for the ring and <code>malloc</code> for the others?</summary>

A reply carrying a sequence number you never sent would read its slot anyway.
With `malloc` that slot is uninitialised memory, and the RTT would be garbage —
and reading uninitialised memory is undefined behaviour that valgrind will
flag. `calloc` makes those slots a defined zero. The other two buffers are fully
written before they are read, so zeroing them would be wasted work.
</details>

<details><summary><b>M11.</b> How do you compute standard deviation without storing every sample?</summary>

From running sums. Keep `sum` and `sum_sq`; then
`variance = sum_sq/n − (sum/n)²`. Memory is constant regardless of how long the
run lasts — which matters, because a flood ping can send hundreds of thousands
of packets.
</details>

<details><summary><b>M12.</b> Why the <code>if (variance &lt; 0.0) variance = 0.0;</code> guard?</summary>

Mathematically variance cannot be negative. In floating point, when every sample
is nearly identical, `E[x²]` and `E[x]²` are nearly equal and their difference
is dominated by rounding error — it can land slightly below zero. `sqrt()` of a
negative is `NaN`, and the summary line would print `nan`. One comparison
prevents it.

The numerically stable alternative is Welford's online algorithm, which avoids
the cancellation entirely.
</details>

<details><summary><b>M13.</b> Why is <code>SA_RESTART</code> deliberately not set?</summary>

With `SA_RESTART`, a `poll()` interrupted by SIGINT is transparently restarted,
so the loop would not notice the stop request until the current wait finished.
Under `-i 30` that is up to thirty seconds between Ctrl+C and the program
exiting. Without it, `poll()` returns `EINTR`, the loop head re-tests the stop
flag, and it exits immediately. `test.sh` tests exactly this case.
</details>

<details><summary><b>M14.</b> Why does the signal handler only set a flag?</summary>

Only *async-signal-safe* functions may be called from a handler. `printf` is not
— it takes a lock on the stream, and if the signal arrives while the main flow
already holds that lock, the process deadlocks. Even `malloc` is unsafe for the
same reason. Setting a `volatile sig_atomic_t` and returning is the standard
pattern; the real work happens back in `main()`.
</details>

<details><summary><b>M15.</b> Why <code>volatile sig_atomic_t</code> specifically?</summary>

`volatile` stops the compiler caching the flag in a register, which would mean
the loop never observes the handler's write. `sig_atomic_t` is the only type the
standard guarantees can be written from a handler and read from the main flow
without a torn value. Neither alone is sufficient.
</details>

<details><summary><b>M16.</b> Why is the reported byte count <code>n - ihl</code> and not <code>ip_len</code>?</summary>

Because `ip_len` on a raw socket is not portable. On Linux it is the full
datagram length in network byte order; on the BSDs and macOS it has
historically been host byte order, sometimes with the header length already
subtracted. `n` is `recvfrom()`'s return value and means exactly the same thing
everywhere, so `n - ihl` is the ICMP bytes on every platform.
</details>

<details><summary><b>M17.</b> Why is the sequence number not part of the "is this mine?" filter?</summary>

Because sends no longer wait for replies. By the time a reply lands, the counter
has already advanced to the next probe, so comparing against the current value
would reject everything. The identifier alone establishes ownership; the
sequence number is used afterwards, to index the timestamp ring.
</details>

<details><summary><b>M18.</b> What happens if two pings run on the same machine at once?</summary>

Both raw sockets receive both conversations — every ICMP packet arriving at the
host is delivered to every raw ICMP socket. Each process filters on its own
identifier, so each counts only its own replies. Without that filter, both would
double-count.
</details>

<details><summary><b>M19.</b> How does <code>-S</code> work? There is no "set source address" option.</summary>

`bind()`. Binding the socket to a local address fixes it, and the kernel then
stamps that address into every outgoing packet. That is the whole mechanism. It
fails with `EADDRNOTAVAIL` for an address no interface owns, which is correct —
you cannot legitimately send from an address you do not have.
</details>

<details><summary><b>M20.</b> Where does 65507 come from?</summary>

An IPv4 datagram is at most 65535 bytes. Subtract the 20-byte IP header (no
options) and the 8-byte ICMP header: 65535 − 20 − 8 = 65507. Anything larger
cannot be expressed in the IP total-length field.
</details>

<details><summary><b>M21.</b> What happens with <code>-s 2000</code> on a normal Ethernet link?</summary>

IP fragments it. The MTU is typically 1500 bytes, so the datagram is split
across multiple frames, each with its own IP header and a fragment offset, and
the receiver reassembles them before handing the ICMP message up. You see one
reply; `tcpdump` shows several frames. If a single fragment is lost, the whole
datagram is lost.
</details>

<details><summary><b>M22.</b> What exactly is inside an ICMP error message?</summary>

Its own IP header, its own 8-byte ICMP header giving the type and code, and then
a **quoted copy** of the packet that provoked it — the original IP header plus at
least the first 8 bytes of its payload, which for an echo request is exactly the
ICMP header carrying the identifier and sequence number. That quote is how you
confirm the error was caused by you and which probe it refers to. RFC 792 asks
for 8 bytes; RFC 1812 asks for as much as fits, so implementations vary and the
quote must be bounds-checked rather than assumed.
</details>

---

### Hard — where it gets interesting

<details><summary><b>H1.</b> You use the PID as the identifier. When does that break?</summary>

When two processes can have the same PID. Inside separate PID namespaces —
containers — process 1 exists in each, so two ft_pings in two containers on the
same host can pick the same identifier. If they share a network namespace, each
would count the other's replies.

It is also truncated: `getpid()` returns a `pid_t`, which is 32 bits, and the
identifier field is 16, so any two PIDs differing only above bit 15 collide.

Real ping on Linux sidesteps this with `SOCK_DGRAM`/`IPPROTO_ICMP`, where the
kernel assigns and rewrites the identifier per socket and guarantees uniqueness.
A stronger fix here would be to randomise the identifier and check it against
inbound traffic at startup.
</details>

<details><summary><b>H2.</b> Do you verify the checksum of packets you receive?</summary>

Yes — and it was the last real gap in the receive path, so it is worth knowing
the shape of it.

```c
static int	checksum_ok(t_icmp_hdr *reply, ssize_t icmp_bytes)
{
	uint16_t	claimed;
	uint16_t	computed;

	claimed = reply->checksum;
	reply->checksum = 0;
	computed = ft_checksum(reply, (size_t)icmp_bytes);
	reply->checksum = claimed;
	return (computed == claimed);
}
```

The checksum is a **two-ended contract**: the sender computes it over a message
whose checksum field reads zero, so the receiver has to reproduce exactly that
state — save the field, zero it, recompute, compare. The field is then restored,
because the ICMP-error path reads the same buffer afterwards.

It runs before anything else is read out of the packet, and a mismatch is
reported (`checksum mismatch from …`, matching `libping.c` in the reference) and
the packet dropped — everything after that point would be reading fields just
proven untrustworthy.

**Do not assume the kernel filtered these for you.** A raw socket exists
precisely to see traffic a cooked socket would have discarded.

</details>

<details><summary><b>H3.</b> The sequence number is 16 bits. What happens after 65536 packets?</summary>

It wraps to 0, which is correct and intended — the ring has exactly 65536 slots,
so `seq % 65536` is the identity and each sequence number maps to its own slot
for one full cycle.

The failure mode is a reply arriving more than 65536 packets after its request,
by which time its slot has been overwritten by a newer send. You would compute
an RTT against the wrong timestamp. At the flood rate of 100/s that requires a
reply delayed by about 11 minutes, which is far beyond any real network. It is
bounded and understood rather than eliminated.
</details>

<details><summary><b>H4.</b> A reply arrives for a sequence number you never sent. What happens?</summary>

It passes the type, code and identifier checks — an attacker who guessed your
PID could construct it — and then indexes a ring slot that `calloc` zeroed. The
RTT computed is "now minus the epoch of the monotonic clock", which is a large
positive number, and it is folded into the statistics.

The counters go wrong, but nothing crashes and nothing is read out of bounds,
because the ring is exactly the size of the sequence-number space. Verifying the
checksum (H2) raises the bar; tracking which sequence numbers are outstanding —
which is what the reference's `_PING_SET`/`_PING_TST` bitmap does — would close
it properly and would also let you detect duplicates.
</details>

<details><summary><b>H5.</b> Why not <code>SO_RCVTIMEO</code> on a blocking socket instead of <code>poll()</code>?</summary>

It would work for the simple case, but it is worse in three ways. It gives you a
timeout per `recvfrom()` call rather than for the whole wait, so a stream of
packets each arriving just under the timeout could keep you there indefinitely.
It cannot express "wake me at this absolute instant", which is what the `-t`
deadline needs. And it does not generalise — the moment you have a second
descriptor to watch, `poll()` is the only structure that works.
</details>

<details><summary><b>H6.</b> Why not use threads — one sending, one receiving?</summary>

It would work and it is how some tools are built, but it buys nothing here and
costs correctness. The statistics would become shared mutable state needing a
mutex; the print ordering between the two threads would become
non-deterministic; and signal delivery in a multithreaded process is
considerably subtler.

`poll()` gives the same concurrency — waiting on several things at once — inside
a single flow of control, which is easier to reason about and easier to prove
correct. Concurrency and parallelism are different problems, and this one only
needs concurrency.
</details>

<details><summary><b>H7.</b> <code>poll()</code> can set <code>POLLERR</code> or <code>POLLHUP</code>. Do you handle them?</summary>

Not explicitly — the code tests `rc > 0` and does not inspect `revents`. That is
survivable rather than ideal: if `POLLERR` is set, the following `recvfrom()`
returns the pending error, which `classify_recv_error()` reports and the loop
ends on.

The honest answer is that it should read `pfd.revents` and distinguish
`POLLIN` from an error condition, so the diagnostic names the real problem
rather than surfacing as a confusing `recvfrom` failure.
</details>

<details><summary><b>H8.</b> Can <code>sendto()</code> be interrupted by a signal, and would you notice?</summary>

Yes — it can fail with `EINTR`. The code treats any `-1` from `sendto()` as a
fatal send error, prints it and stops the run. Since the only signal handled is
SIGINT, and SIGINT means stop anyway, the outcome is right by luck rather than
by design: the run ends, the summary prints, nothing is corrupted.

A more careful version would distinguish `EINTR` (retry or stop quietly) from a
genuine failure such as `ENETUNREACH` (report it).
</details>

<details><summary><b>H9.</b> Your <code>-f</code> is not the man page's <code>-f</code>. Explain.</summary>

The man page specifies *"as fast as they come back, or one hundred times per
second, whichever is more"* — two mechanisms, a rate floor and a send-on-reply
trigger. This implementation has only the floor: a fixed 10 ms interval.

The consequence is that on a fast link — loopback, where the RTT is tens of
microseconds — real flood ping goes far faster than this one, because it sends a
new packet the instant each reply lands. Adding it would mean calling
`send_if_due()`-style logic from the receive path as well.

It is a deliberate simplification with a known effect, and saying that is a much
better answer than being surprised by the question.
</details>

<details><summary><b>H10.</b> Why is the ICMP error path not counted as a reply?</summary>

Because the probe it refers to did not come back — something in the middle told
you *why*, but the round trip never completed. Counting it as received would
report 0% loss on a run where nothing actually reached the destination.

So `-m 1` to a distant host prints error lines, reports 100% packet loss, and
exits 1. All three are correct together.
</details>

<details><summary><b>H11.</b> Why is the <code>-t</code> deadline checked twice per iteration?</summary>

Because the deadline can fall *during* the `poll()` wait. Checking only before
the wait would let one more packet out after expiry; checking only after would
enter a wait that is already pointless. The check before the wait bounds the
wait, and the check inside `send_if_due()` catches expiry that happened while
waiting.
</details>

<details><summary><b>H12.</b> What is the bug that would appear if you sent whenever <code>poll()</code> returned 0?</summary>

A burst at the end of every `-t` run. `poll()` returning 0 means only "nothing
arrived in that window", and that window can be bounded by the deadline rather
than by the send schedule. In the last fraction of a millisecond before the
deadline, `poll()` returns immediately, you send, `poll()` returns immediately,
you send — hundreds of packets in the final millisecond.

The fix is the design rule: **sending is decided by the clock, never by poll's
return value.**
</details>

<details><summary><b>H13.</b> Why does <code>ms_until()</code> round up rather than truncate?</summary>

With 0.4 ms remaining, truncation gives a 0 ms timeout. `poll()` returns
instantly with the target still in the future, the loop recomputes, gets 0
again, and spins on that sub-millisecond remainder — burning CPU without
progress. Rounding up guarantees that each wait actually crosses the target.

The same function also clamps negatives to 0, because `poll()` reads a negative
timeout as "block forever", and clamps the top, because casting `INFINITY` to
`int` is undefined behaviour.
</details>

<details><summary><b>H14.</b> Why is <code>last_send</code> re-read from the clock after the send instead of reusing <code>now</code>?</summary>

Because `sendto()` takes time. Pacing from the instant *before* the syscall
makes every interval short by however long the call lasted, and since each
interval is measured from the previous one, that error accumulates across the
run. Re-reading after the send keeps the schedule anchored to when the packet
actually left.
</details>

<details><summary><b>H15.</b> How would you make this run without root?</summary>

Three options, in increasing order of intrusiveness:

1. `setcap cap_net_raw+ep ./ft_ping` — grants only the one capability, no setuid
   root, no other privilege.
2. `SOCK_DGRAM` with `IPPROTO_ICMP`, permitted for group IDs in
   `net.ipv4.ping_group_range` on Linux and available on macOS. The kernel then
   builds and manages the identifier, and does **not** hand you the IP header on
   receive — so the parsing code would need a second path.
3. setuid root, which is the historical approach and by far the worst: the whole
   program runs privileged rather than just the socket call.

Option 1 is the right answer for this program, because the privilege is needed
for exactly one syscall.
</details>

<details><summary><b>H16.</b> Why is the receive buffer floored at 1024 bytes when <code>-s 0</code> asks for nothing?</summary>

Because what you *receive* is not what you send. An inbound ICMP error carries
its own IP header, its own ICMP header, and a quoted copy of your original IP and
ICMP headers — easily a couple of hundred bytes even for a zero-length probe.
Sizing the receive buffer from the payload alone would truncate every error into
uselessness.
</details>

<details><summary><b>H17.</b> How would you prove the ICMP error path works, without waiting for a real failure?</summary>

`./ft_ping -m 1 8.8.8.8`. Setting TTL to 1 guarantees the first router discards
the packet and returns Time Exceeded, so the entire error path — bounds checks,
quoted-header walk, identifier match, formatting — runs on demand and
repeatably. Same trick traceroute is built on.
</details>

<details><summary><b>H18.</b> What is the worst thing a malicious host on your network could do to this program?</summary>

Not much, and that is deliberate. Every field read off the wire is bounds-checked
before use, so malformed packets are skipped rather than crashing anything —
that is what the gate/filter split exists for.

What it *could* do is skew the statistics: guess the identifier (the PID is not
secret) and inject forged replies, inflating `n_recv` and corrupting the RTT
figures. Verifying the checksum raises the cost, and tracking outstanding
sequence numbers would let you reject a reply to a probe you never sent. Neither
is implemented, and both are worth naming as the honest boundary of what the
program defends against.
</details>

---

### Brutal — the ones that separate answers from understanding

<details><summary><b>B1.</b> What would you change if you started again?</summary>

Two things are still open, in priority order:

1. **A bitmap of outstanding sequence numbers**, like the reference's
   `_PING_SET`/`_PING_TST`. It closes the forged-reply hole, detects duplicates
   (`(DUP!)` in real ping), and costs 8 KB — far less than the 1 MB timestamp
   ring it would sit beside.
2. **Read `pfd.revents`** rather than only `poll()`'s return value, so an error
   condition on the socket names itself instead of surfacing as a confusing
   `recvfrom` failure.

A third — verifying inbound checksums — was open until I diffed this against
`inetutils-2.0` and fixed it. That diff is worth describing as its own answer:
reading the reference implementation found seven divergences, six of which are
now fixed and one of which I kept deliberately (see Part 0).

What I would keep unchanged is the clock-driven send / event-driven receive
split. Everything good about the program follows from it.
</details>

<details><summary><b>B2.</b> How would you add IPv6?</summary>

ICMPv6 is a different protocol, not a variant. Concretely: `AF_INET6`,
`IPPROTO_ICMPV6`, echo request becomes type 128 and reply type 129, and the
socket is `SOCK_RAW` with `sockaddr_in6`.

The structural difference that catches people: **the ICMPv6 checksum covers a
pseudo-header** built from the source and destination addresses, so it cannot be
computed from the message alone — but the kernel computes it for you on ICMPv6
raw sockets, so you leave the field zero. And on receive, no IP header is
delivered, so the `ip_hl` skip disappears entirely.

Given this layout, it is a new `src/net/ping6_*.c` alongside the existing one
plus a branch in `ctx_init()` — `resolve_host()`, the loop, the statistics and
all of the output layer are unchanged.
</details>

<details><summary><b>B3.</b> How would you add <code>-c &lt;count&gt;</code>?</summary>

One field in `t_flags`, one condition in the loop. The subtlety is *when* to
stop: naively you exit once `n_sent == count`, but then the last packet's reply
has not had time to arrive and you always report one lost.

Real ping sends the count, then waits — for the remaining replies, up to a
timeout of roughly one interval or a linger deadline. So it is a two-phase loop:
send until the count is met, then stop sending and keep receiving until either
`n_recv == n_sent` or a deadline. The `t_sched` struct already has the shape for
that; `deadline` gets reused as a linger deadline once the count is met.
</details>

<details><summary><b>B4.</b> How would you deterministically test packet loss?</summary>

Not with a distant unreachable address — that is what `test.sh` does today with
192.0.2.1, and it is why the suite has to probe first and skip when a sandbox
answers for it.

Deterministically, you inject loss locally:

```bash
tc qdisc add dev lo root netem loss 50%    # Linux traffic control
sudo ./ft_ping -t 10 127.0.0.1
tc qdisc del dev lo root netem
```

`netem` drops a configured percentage on the interface, so you can assert
"between 30% and 70% loss" and know the code path is real. The equivalent on
macOS is `dnctl`/`pfctl`.
</details>

<details><summary><b>B5.</b> Your test suite passes 116 cases. What does it not test?</summary>

Honestly:

- **Fragmentation** — `-s 2000` is never exercised end to end.
- **Concurrency** — two ft_pings at once, to confirm the identifier filter really
  separates them.
- **Malformed inbound packets** — everything in the gate/filter logic is
  untested, and so is the checksum-mismatch path, because nothing in the suite
  forges a bad packet. A raw socket in the harness could inject one, and that is
  the highest-value test still missing. `test.sh` marks it as an explicit SKIP
  rather than pretending it is covered.
- **Long-run behaviour** — sequence wraparound at 65536 packets.
- **Real loss**, for the reason in B4.

Knowing the gaps is worth more than the count.
</details>

<details><summary><b>B6.</b> Why should anyone believe your RTT numbers?</summary>

Two independent checks. Against the reference implementation: run `ping-ref` and
`ft_ping` to the same host at the same time and the figures agree within the
tolerance the subject allows. And against a packet capture: `tcpdump` timestamps
the request and the reply independently of the program, so the delta it shows
should match what ft_ping reports.

The measurement itself is honest by construction — a monotonic clock read
immediately before `sendto()` and immediately after `recvfrom()`. What it
*includes* is worth stating: kernel queuing on both ends and the remote host's
turnaround, not just wire time.
</details>

<details><summary><b>B7.</b> Where is the biggest source of error in your timing?</summary>

Scheduler latency on the receiving side. The reply can sit in the socket queue
after the kernel has received it, while the process waits to be scheduled — so
the timestamp is taken later than the packet actually arrived. On a loaded
machine that is easily hundreds of microseconds, which is an order of magnitude
larger than a loopback RTT.

That is also why the *minimum* RTT over many samples is the most trustworthy
figure: it is the sample least contaminated by scheduling noise. Hardware
timestamping (`SO_TIMESTAMPING`) is the real fix, and it is what precision tools
use.
</details>

<details><summary><b>B8.</b> Explain your program to someone who knows C but nothing about networks.</summary>

*(There is no hidden answer here — this is the rehearsal. Say it out loud, aim
for ninety seconds, and use no term you have not defined.)*

A workable shape: a letter with a return address and a reference number → the
post office refuses to let ordinary people write their own envelopes, which is
why it needs root → you note when you posted it → replies come back in any
order, so the reference number is how you match them → and you cannot stand at
the letterbox waiting, because some letters never arrive, which is what `poll()`
solves.
</details>

---

## Part 13 — Whiteboard cheat sheet

Four things worth being able to draw. Practise them once on paper.

**1. The packet, layered**

```
+----------------+----------------+---------------------------+
|   IP header    |  ICMP header   |         payload           |
|   20 bytes     |    8 bytes     |     56 bytes (default)    |
|  (kernel's)    |    (yours)     |        (yours)            |
+----------------+----------------+---------------------------+
                 |<--------- what "64 bytes" counts --------->|
|<------------- what sendto() puts on the wire ------------->|
      ^
      kernel adds this on send;
      kernel GIVES it to you on receive  ← the asymmetry
```

**2. The exchange**

```
  ft_ping                                        target host
     |                                                |
     |  type 8, id=PID, seq=0, payload  ------------> |
     |  t0 = clock_gettime(MONOTONIC)                 |
     |                                                | swap type to 0,
     |                                                | recompute checksum,
     |                                                | echo payload back
     |  <------------  type 0, id=PID, seq=0, payload |
     |  t1 = clock_gettime(MONOTONIC)                 |
     |  rtt = t1 - t0                                 |
```

**3. The loop**

```
        ┌──────────────────────────────────────┐
        │  now >= deadline?  ── yes ──> stop   │
        └──────────────┬───────────────────────┘
                       │ no
        ┌──────────────▼───────────────────────┐
        │  poll(fd, min(next_send, deadline))  │
        └──────┬─────────────────┬─────────────┘
        packet │                 │ timeout / signal
      ┌────────▼──────┐   ┌──────▼────────────────────┐
      │ drain socket  │   │ (nothing arrived)         │
      │ match id      │   └──────┬────────────────────┘
      │ time it       │          │
      └────────┬──────┘          │
               └────────┬────────┘
        ┌───────────────▼──────────────────────┐
        │ CLOCK says a send is due? ─ yes ─> send_one()
        └──────────────────────────────────────┘
                       │ no
                       └──> back to the top
```

**4. An ICMP error**

```
+-----------+-----------+-------------+--------------+
| IP hdr    | ICMP hdr  | quoted IP   | quoted ICMP  |
| (router's)| type 11   | hdr (yours) | hdr (yours)  |
|           | code 0    |             | id | seq     |
+-----------+-----------+-------------+--------------+
                                        ^^^^^^^^^^
                              this is how you know it was your packet
```

---

## Part 14 — Known limitations, stated honestly

Volunteering these is a strength. An evaluator who finds one you did not mention
wonders what else you missed; one you raise yourself reads as command of the
material.

| # | Limitation | Severity | Why it is that way |
|---|---|---|---|
| 1 | No bitmap of outstanding sequence numbers | Design gap | Means no duplicate detection, and forged replies still count |
| 2 | Identifier is a truncated PID | Weakness | Collides across PID namespaces and above 16 bits |
| 3 | `-f` has the rate floor but not send-on-reply | Divergence | Deliberate simplification |
| 4 | `poll()`'s `revents` is not inspected | Rough edge | Errors surface as `recvfrom` failures |
| 5 | `time=` printed even for tiny payloads | Divergence | Deliberate — the figure is correct and available; see 0.5 |
| 6 | RTT includes scheduler latency | Inherent | Software timestamping; see B7 |
| 7 | IPv4 only | Scope | The subject's mandatory part is IPv4 |
| 8 | Malformed-packet paths are untested | Test gap | Needs a packet forger in the harness; see B5 |

**Resolved after diffing against inetutils-2.0** — worth mentioning as work
done rather than work outstanding: inbound checksum verification, the
`stddev` label, integer loss truncation, the leading newline, `-?` usage output,
and the `-v` identifier in the banner. All six are covered in Part 0 and locked
in by tests.

---

## Part 15 — What transfers to your next project

The point of the project is not ping. It is these, and they will all come back.

**1. Never block on something that may never happen.** The `recvfrom()` hang is
the same bug as a read on a socket whose peer died, a `wait()` on a child that
is stuck, a lock held by a crashed thread. The pattern is always the same: make
the wait bounded, and make it wait on *several* things at once. `poll()` is the
tool; the habit is the lesson.

**2. Separate "does this data exist?" from "do I want this data?"** Gates and
filters. Every parser you write from here — a protocol, a file format, a network
message — has both, and conflating them is how over-reads get written. Bounds
first, semantics second, always in that order.

**3. Data from outside the process is hostile until proven otherwise.** Not
because everyone is an attacker, but because "malformed" and "malicious" produce
identical code paths. The discipline that makes ping robust is the same
discipline that makes a file parser safe.

**4. Streaming statistics.** Sums instead of samples, constant memory instead of
linear. This shows up in every metrics system, every profiler, every monitoring
agent. And the floating-point cancellation trap comes with it — know that
Welford exists before you need it.

**5. Signals are a concurrency problem.** A handler runs *between two
instructions* of your main flow. Everything about async-signal-safety is really
about shared state and reentrancy — the same reasoning threads require, in a
setting where you cannot take a lock.

**6. Choose the clock deliberately.** Monotonic for durations and deadlines,
realtime for timestamps humans will read. Getting this wrong produces bugs that
appear twice a year at daylight-saving transitions and are essentially
undebuggable after the fact.

**7. Read the reference implementation.** Every one of the divergences in Part 0
came from reading `inetutils-2.0/ping/` rather than guessing. When there is a
canonical implementation of what you are building, it is documentation that
cannot be out of date.

---

*Rehearse Parts 1, 3 and 11 out loud. Everything else is there for when a
question goes somewhere you did not expect.*
