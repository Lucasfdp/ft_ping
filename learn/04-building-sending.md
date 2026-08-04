# Stage 4 — Building & Sending the Packet

**Prerequisite:** [03-checksum.md](03-checksum.md) · **Next:** [05-receiving-parsing.md](05-receiving-parsing.md)

---

**Plain version:** fill a struct representing the ICMP header (plus any payload bytes), compute
the checksum over it, then hand the whole buffer to `sendto()` along with the destination address.

**Resolving the destination:** `getaddrinfo()` turns a hostname or IP string into a `sockaddr` you
can pass to `sendto()`. The subject requires handling FQDNs but says not to do DNS resolution "in
the packet return" — meaning: resolve the target once, up front, and don't do a fresh lookup as
part of handling each reply either.

**`sendto()` on a raw ICMP socket:** no destination port needed — ICMP has none. Just a
`sockaddr_in` with the target IP filled in.

---

## Check questions

<details><summary>Q1: When should hostname resolution happen — once at startup, or per packet?</summary>

Once, at startup. Resolve the target to an IP address a single time and reuse it for every packet
you send. There's no reason to re-resolve per packet, and the subject's phrasing about the packet
return points at not doing lookups while handling replies either.
</details>

<details><summary>Q2: Why doesn't sendto() need a port number here?</summary>

Ports are a transport-layer (TCP/UDP) concept for multiplexing connections on a host. ICMP
operates at the network layer and has no notion of ports — the type/code/identifier fields serve
the "which conversation is this" role instead.
</details>

---

## Exercise

First real packet on the wire. **Send only — don't try to receive yet.**

1. Wire up `getaddrinfo()` with `hints.ai_family = AF_INET`, `hints.ai_socktype = SOCK_RAW`.
   Check the return value against `0` and report failures with `gai_strerror()` (**not** `perror` —
   `getaddrinfo` doesn't set `errno`). Always `freeaddrinfo()` on every exit path.
2. Test resolution with: an IPv4 literal (`127.0.0.1`), an FQDN (`google.com`), and garbage
   (`not.a.real.host.invalid`). All three must behave — the third must print a clean error, not
   crash or leak.
3. Build the packet from Stage 2, checksum it with Stage 3, `sendto()` it. Check the return value:
   it returns bytes sent, or `-1`.
4. Confirm with `sudo tcpdump -n icmp` in another terminal that your packet actually appeared —
   **and that the host replied.** You'll see the reply in tcpdump even though your program isn't
   reading it yet. That reply proves your checksum is right: a bad checksum gets silently dropped
   by the receiver and you'll see a request with no reply.

**Done when:** tcpdump shows your request *and* a matching reply. If there's no reply, go back to
Stage 3 — it's almost always the checksum or byte order.

---

## Reading

- `man 3 getaddrinfo` — the whole page, especially the `hints` fields, `gai_strerror`, and the
  `freeaddrinfo` ownership rules
- `man 2 sendto` — return value semantics and the errno list
- `man 7 ip` — the `sockaddr_in` layout, `INADDR_*` constants
- `man 3 inet_ntop` — for printing the resolved address back out (you need it for the header line
  in Stage 9)
