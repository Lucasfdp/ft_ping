# Stage 1 — Sockets, From Basics to Raw

**Prerequisite:** [00-what-ping-does.md](00-what-ping-does.md) · **Next:** [02-icmp-protocol.md](02-icmp-protocol.md)

---

**Plain version:** different socket "types" exist for different jobs — `SOCK_STREAM` behaves like
TCP (connection-oriented), `SOCK_DGRAM` behaves like UDP (connectionless), and `SOCK_RAW` gives you
direct access to a lower-level protocol like IP or ICMP itself.

**`socket()` refresher:** `socket(domain, type, protocol)` — `domain` (e.g. `AF_INET` for IPv4),
`type` (e.g. `SOCK_RAW`), `protocol` (e.g. `IPPROTO_ICMP`) together tell the kernel exactly which
protocol you want under that domain+type combination.

> **Names, expanded** — `AF_INET` = **A**ddress **F**amily: **INET**ernet · `SOCK_STREAM` =
> **SOCK**et, **STREAM** · `SOCK_DGRAM` = **SOCK**et, **D**ata**GRAM** · `SOCK_RAW` = **SOCK**et,
> **RAW** · `IPPROTO_ICMP` = **IP** **PROTO**col: **ICMP**. Full list in
> [GLOSSARY.md](GLOSSARY.md).

## Raw socket specifics

- `socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)` gives you a socket for sending/receiving ICMP
  messages directly. By default the kernel still builds the IP header for you on send.
- `IP_HDRINCL` (**IP** **H**ea**D**e**R** **INCL**uded): a socket option meaning "I will build the
  entire IP header myself." Not required
  for the mandatory part (you're only handing over an ICMP payload), but you'll want it if you
  need to touch IP-level fields directly — relevant later for the `--ip-timestamp` bonus.
- **Privileges:** raw sockets need `CAP_NET_RAW` (**CAP**ability, **NET**work, **RAW**) on Linux
  — in practice, running as root, a
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

**Short answer:** because a `SOCK_RAW` socket can, in principle, be used to see or inject
arbitrary IP-level traffic — a security-sensitive capability the OS gates behind root or
`CAP_NET_RAW`.

**That sentence, unpacked:**

- **inject** — put packets on the network that *you composed yourself*, byte by byte. Normally you
  hand the system your data and it builds the packet around it. Injecting means you wrote the
  packet, including the parts that are supposed to describe you.
- **arbitrary** — anything at all, with no restrictions. Not "packets from a fixed menu" — any
  packet you can imagine, including malformed or dishonest ones.
- **IP-level** — at the layer that handles addressing and routing between machines, underneath the
  layer that normally keeps programs separated from each other.
- **traffic** — just packets moving over the network.
- **capability** — one *specific* permission rather than blanket admin rights. Linux split "root
  can do everything" into a list of individual powers that can be granted one at a time.
- **gates** — restricts; a check you must pass.

**In plain terms:** a raw socket lets your program read packets that weren't meant for it, and
send packets that lie about where they came from. That's the dangerous part, and it's why the
system asks who you are first.

**What it actually enables** — the three things the OS is guarding against:

1. **Reading other people's mail.** A raw socket receives packets your program isn't the intended
   recipient of. Your reply filter in Stage 5 exists because of this — they arrive whether you
   want them or not. Extend that beyond ICMP and you have a wiretap.
2. **Lying about who you are.** Normally the system fills in your machine's address as the sender
   and you can't touch it. Build the packet yourself and that field is yours to write — including
   with someone else's address, so replies go to *them*. This is the basis of spoofing and the
   main reason the restriction exists.
3. **Sending packets that shouldn't exist.** Contradictory or nonsensical field combinations that
   some systems have historically handled very badly.

**The irony:** ping needs none of this. It needs one narrow ability — send an ICMP Echo, read the
reply — but the mechanism granting it is coarse, so it arrives bundled with all of the above. That
mismatch is exactly why modern Linux added the unprivileged `SOCK_DGRAM` + `IPPROTO_ICMP` path
described above: a narrow gate for the one legitimate use.

**Acronyms:** `SOCK_RAW` = **SOCK**et, **RAW**. `CAP_NET_RAW` = **CAP**ability, **NET**work,
**RAW**. `EPERM` = **E**rror: **PERM**ission denied — the error you get without it.
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
