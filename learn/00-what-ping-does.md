# Stage 0 — What Ping Actually Does

**Prerequisite:** none. **Next:** [01-sockets.md](01-sockets.md) · Acronyms: [GLOSSARY.md](GLOSSARY.md)

---

**Plain version:** your machine sends a tiny "are you there?" packet to another machine. If it's
reachable, that machine sends back "yes, I'm here." You measure how long the round trip took.

**Where this sits in the stack:** this isn't TCP (**T**ransmission **C**ontrol **P**rotocol) and it
isn't UDP (**U**ser **D**atagram **P**rotocol). It's a separate protocol called
**ICMP** — **I**nternet **C**ontrol **M**essage **P**rotocol — that rides directly on top of IP
(**I**nternet **P**rotocol). Protocol number 1,
not a transport-layer protocol at all. That's the whole reason ping can't just open a normal socket
the way a chat client or web browser would.

**Why root, historically:** reading/writing packets below the normal socket abstraction requires a
**raw socket**, and raw sockets are a privileged operation on most OSes, since they let a process
see or craft traffic outside its own connections.

---

## Check questions

<details><summary>Q1: Why can't ping just use a normal TCP or UDP socket?</summary>

There's no "ICMP port" and no connection to open — ICMP is a network-layer protocol, not a
transport-layer one. You need a socket type that lets you operate directly in terms of IP-level
packets, which is what raw sockets are for.
</details>

<details><summary>Q2: What does "round-trip time" measure, end to end?</summary>

The time between sending the Echo Request and receiving the matching Echo Reply — the full trip
there and back, including processing/queuing delay on both ends and the network in between.
</details>

---

## Exercise

No code yet. Observe the real thing:

1. Run `ping -c 3 127.0.0.1` and read every field of the output out loud. Name what each one is.
2. In a second terminal, run `sudo tcpdump -i any -n icmp` while pinging. You should see paired
   `ICMP echo request` / `ICMP echo reply` lines.
3. Write down, from the tcpdump output alone: how many bytes is the ICMP message? What's the `id`
   and `seq`? Those exact fields come back in Stage 2.

**Done when:** you can point at a tcpdump line and say which host sent it and why.

---

## Reading

- **RFC 792** §"Echo or Echo Reply Message" — 1 page, read it now, it's the whole contract
- `man 8 ping` — skim the description and the options list; you'll implement a subset
- `man 1 tcpdump` — just the `-n` and expression syntax
