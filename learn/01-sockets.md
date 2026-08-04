# Stage 1 — Sockets, From Basics to Raw

**Prerequisite:** [00-what-ping-does.md](00-what-ping-does.md) · **Next:** [02-icmp-protocol.md](02-icmp-protocol.md)

---

**Plain version:** different socket "types" exist for different jobs — `SOCK_STREAM` behaves like
TCP (connection-oriented), `SOCK_DGRAM` behaves like UDP (connectionless), and `SOCK_RAW` gives you
direct access to a lower-level protocol like IP or ICMP itself.

**`socket()` refresher:** `socket(domain, type, protocol)` — `domain` (e.g. `AF_INET` for IPv4),
`type` (e.g. `SOCK_RAW`), `protocol` (e.g. `IPPROTO_ICMP`) together tell the kernel exactly which
protocol you want under that domain+type combination.

## Raw socket specifics

- `socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)` gives you a socket for sending/receiving ICMP
  messages directly. By default the kernel still builds the IP header for you on send.
- `IP_HDRINCL`: a socket option meaning "I will build the entire IP header myself." Not required
  for the mandatory part (you're only handing over an ICMP payload), but you'll want it if you
  need to touch IP-level fields directly — relevant later for the `--ip-timestamp` bonus.
- **Privileges:** raw sockets need `CAP_NET_RAW` on Linux — in practice, running as root, a
  setuid-root binary, or a binary with `setcap cap_net_raw+ep` applied. Note: modern Linux also
  supports *unprivileged* ICMP via `SOCK_DGRAM` + `IPPROTO_ICMP` under a sysctl
  (`net.ipv4.ping_group_range`) — worth knowing exists, but 42's ft_ping expects the raw-socket
  route, so check what your eval VM/cluster setup actually permits.

---

## Check questions

<details><summary>Q1: What's the practical difference between SOCK_DGRAM and SOCK_RAW here?</summary>

SOCK_DGRAM lets the kernel handle transport-layer framing for you (as it does for UDP) — but
there's no "ICMP transport" in the traditional sense. SOCK_RAW gives you access closer to the
protocol level: you construct/read full ICMP messages yourself, while the kernel still fills in
the IP header by default unless you set IP_HDRINCL.
</details>

<details><summary>Q2: Why does ft_ping need elevated privileges at all?</summary>

Because a SOCK_RAW socket can, in principle, be used to see or inject arbitrary IP-level traffic —
a security-sensitive capability the OS gates behind root or CAP_NET_RAW.
</details>

---

## Exercise

Write a ~20-line `main()` that does nothing but open the socket and exit:

1. `socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)`, check for `-1`, `perror` + exit non-zero on failure.
2. Run it as a normal user — confirm you get `Operation not permitted` (EPERM). **This is the
   error path you must handle gracefully in Stage 8, so see it now.**
3. Run it with `sudo` — confirm it succeeds.
4. Check your eval environment: does `cat /proc/sys/net/ipv4/ping_group_range` allow unprivileged
   ICMP? Note the answer; it decides whether you can test without sudo.

**Done when:** the same binary prints a clean error unprivileged and exits 0 under sudo — no
segfault, no silent failure.

---

## Reading

- `man 2 socket` — the three-argument contract, and the full errno list
- `man 7 raw` — read this one carefully; it describes exactly what a raw ICMP socket sends and
  receives, and documents `IP_HDRINCL`
- `man 7 ip` — the socket options table (`IP_TTL`, `IP_HDRINCL`, `IP_OPTIONS` all come back later)
- `man 7 capabilities` — search for `CAP_NET_RAW`
- `man 8 setcap` — if you want to test without sudo
