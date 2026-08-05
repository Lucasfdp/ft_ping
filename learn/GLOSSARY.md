# Glossary — every acronym, expanded

An unexpanded acronym is a magic word you memorise. An expanded one is usually self-explanatory.
Every abbreviation used anywhere in these notes is broken down here, letter by letter.

Jump to: [Protocols](#protocols) · [Socket constants](#socket-constants) · [Socket options](#socket-options) ·
[Header fields](#header-fields) · [Errors](#error-codes) · [Signals](#signals-and-time) ·
[Statistics](#statistics-and-output) · [Tools & standards](#tools-and-standards) ·
[Plain-English phrases](#plain-english-for-common-phrases)

---

## Protocols

| Written | Stands for | What it means |
|---|---|---|
| **IP** | **I**nternet **P**rotocol | Gets a packet from one machine to another across networks. Handles addressing and routing, promises nothing about delivery. |
| **ICMP** | **I**nternet **C**ontrol **M**essage **P**rotocol | The network's own messaging system — error reports and diagnostics *about* IP, carried inside IP. What ping uses. |
| **TCP** | **T**ransmission **C**ontrol **P**rotocol | Reliable ordered byte stream between two programs. Retransmits what gets lost. |
| **UDP** | **U**ser **D**atagram **P**rotocol | Send one message, hope it arrives. No retransmission, no ordering. |
| **ARP** | **A**ddress **R**esolution **P**rotocol | Maps an IP address to a hardware address on the local network. |
| **DNS** | **D**omain **N**ame **S**ystem | Turns `google.com` into an IP address. |
| **NTP** | **N**etwork **T**ime **P**rotocol | Keeps your clock synced with the internet — and can jump your clock, which is why Stage 6 avoids wall-clock time. |

---

## Socket constants

| Written | Stands for | What it means |
|---|---|---|
| **`AF_INET`** | **A**ddress **F**amily: **INET**ernet | "Use IPv4 addresses." (`AF_INET6` for IPv6.) |
| **`SOCK_STREAM`** | **SOCK**et, **STREAM** | Continuous byte stream. TCP-style. |
| **`SOCK_DGRAM`** | **SOCK**et, **D**ata**GRAM** | One message at a time, boundaries preserved. UDP-style. |
| **`SOCK_RAW`** | **SOCK**et, **RAW** | Unprocessed access — you build and read packets yourself. |
| **`IPPROTO_ICMP`** | **IP** **PROTO**col: **ICMP** | Which protocol you want under that family and type. |
| **`INADDR_ANY`** | **IN**ternet **ADDR**ess: **ANY** | "Any local address" — bind to all interfaces. |
| **`CAP_NET_RAW`** | **CAP**ability, **NET**work, **RAW** | The single permission allowing raw network sockets. Held by root, or granted individually. |

**"Capability"** here means one specific power, rather than blanket admin rights. Linux split
"root can do anything" into a list of separate permissions so a program can be given exactly the
one it needs. `CAP_NET_RAW` is the raw-socket one.

---

## Socket options

| Written | Stands for | What it means |
|---|---|---|
| **`IP_HDRINCL`** | **IP** **H**ea**D**e**R** **INCL**uded | "I'm building the IP header myself, don't build one for me." |
| **`IP_TTL`** | **IP** **T**ime **T**o **L**ive | Sets the hop counter on packets you send. The `-T` flag. |
| **`IP_OPTIONS`** | **IP** **OPTIONS** | The optional extra fields at the end of an IP header. |
| **`SO_DONTROUTE`** | **S**ocket **O**ption: **DON'T ROUTE** | Skip the routing table, send only to a directly attached network. The `-r` flag. |
| **`SO_RCVBUF`** | **S**ocket **O**ption: **R**e**C**ei**V**e **BUF**fer | How much incoming data the kernel will hold for you. |
| **`MSG_TRUNC`** | **M**e**S**sa**G**e **TRUNC**ated | Flag meaning "the packet was bigger than your buffer; the rest is gone." |

---

## Header fields

| Written | Stands for | What it means |
|---|---|---|
| **IHL** | **I**nternet **H**eader **L**ength | How long the IP header is — **counted in 4-byte groups**, so multiply by 4. Usually 5, meaning 20 bytes. |
| **TTL** | **T**ime **T**o **L**ive | Despite the name it counts *hops*, not seconds. Each router decrements it; at zero the packet dies and you get a Time Exceeded message. |
| **TOS** | **T**ype **O**f **S**ervice | Priority/handling hints. Largely repurposed as DSCP now. |
| **DSCP** | **D**ifferentiated **S**ervices **C**ode **P**oint | Modern replacement for TOS — traffic-class marking. |
| **MTU** | **M**aximum **T**ransmission **U**nit | The biggest packet a network link will carry, typically 1500 bytes on Ethernet. Exceed it and the packet gets split up. Relevant to the `-s` flag. |
| **FQDN** | **F**ully **Q**ualified **D**omain **N**ame | A complete hostname including all its domains: `www.example.com`, not just `www`. |

---

## Error codes

Error constants all start with **`E`** for **E**rror.

| Written | Stands for | When you see it |
|---|---|---|
| **`EPERM`** | **E**rror: **PERM**ission denied | Opening a raw socket without root or `CAP_NET_RAW`. |
| **`EINTR`** | **E**rror: **INTR**errupted | A signal arrived mid-syscall. **Not a real error** — retry or exit deliberately. Stage 7. |
| **`EAGAIN`** | **E**rror: try **AGAIN** | Nothing to read right now on a non-blocking socket. |
| **`EACCES`** | **E**rror: **ACCES**s denied | Permission problem, distinct from `EPERM` in origin. |
| **`EINVAL`** | **E**rror: **INVAL**id argument | You passed something nonsensical. |
| **`EMSGSIZE`** | **E**rror: **M**e**S**sa**G**e **SIZE** | Packet too big to send, often an MTU issue. |
| **`errno`** | **err**or **n**umber | The global holding the last error. Only meaningful *immediately* after a failed call. |
| **`perror`** | **p**rint **error** | Prints your message followed by the text of the current `errno`. |
| **`strerror`** | **str**ing for **error** | Returns the error text as a string, so you can format it yourself. |

---

## Signals and time

| Written | Stands for | What it means |
|---|---|---|
| **`SIGINT`** | **SIG**nal: **INT**errupt | What Ctrl+C sends. |
| **`SIGALRM`** | **SIG**nal: **AL**a**RM** | Timer expiry signal. |
| **`SA_RESTART`** | **S**igaction **A**ction: **RESTART** | Flag deciding whether an interrupted syscall resumes automatically or returns `EINTR`. A real decision, not a default. |
| **`sig_atomic_t`** | **sig**nal **atomic** **t**ype | A type guaranteed to be read/written in one indivisible step, so a signal can't catch it half-updated. The only thing safe to touch in a handler. |
| **`CLOCK_MONOTONIC`** | **CLOCK**, **MONOTONIC** (only ever increases) | A clock that never jumps backwards. Correct for measuring durations. |
| **`CLOCK_REALTIME`** | **CLOCK**, **REAL** wall-clock **TIME** | Actual date and time — can jump when NTP corrects it. Wrong for durations. |
| **`timespec`** | **time** **spec**ification | Struct holding seconds + nanoseconds. |
| **Async-signal-safe** | **Asynchronous**-signal-safe | Safe to call from inside a signal handler. `printf` is **not**. |

---

## Statistics and output

| Written | Stands for | What it means |
|---|---|---|
| **RTT** | **R**ound-**T**rip **T**ime | Time from sending a request to receiving its reply. |
| **mdev** | **m**ean **dev**iation | How much individual RTTs stray from the average. Low = steady, high = jittery. |
| **stddev** | **st**andard **dev**iation | Similar idea; ping reports mdev specifically. |
| **DUP!** | **DUP**licate | The same sequence number came back more than once. |

---

## Tools and standards

| Written | Stands for | What it means |
|---|---|---|
| **RFC** | **R**equest **F**or **C**omments | The documents defining internet protocols. Despite the modest name, these *are* the standards. RFC 792 defines ICMP. |
| **IANA** | **I**nternet **A**ssigned **N**umbers **A**uthority | Keeps the official registry of protocol numbers, ICMP types, port numbers. |
| **ASan** | **A**ddress **San**itizer | Compiler feature (`-fsanitize=address`) that catches buffer overruns and use-after-free at runtime. |
| **LCOV** | **L**inux **C**overage tool | Test-coverage report format. |
| **`-MMD -MP`** | **M**ake **M**ake **D**ependencies / **M**ake **P**hony | Compiler flags generating header dependency files automatically, so `make` rebuilds correctly when a `.h` changes. |
| **setuid** | **set** **u**ser **id**entity | A binary that runs as its owner rather than whoever launched it. One way to get raw-socket permission. |
| **sysctl** | **sys**tem **c**on**t**ro**l** | Runtime kernel settings, e.g. `net.ipv4.ping_group_range`. |

---

## Plain English for common phrases

**"Inject traffic"** — put packets on the network that you composed yourself, byte by byte,
rather than handing data to the system and letting it build the packet around it.

**"Arbitrary"** — anything at all, no restrictions. Not "from a fixed set of options."

**"IP-level"** / **"network-layer"** — operating at the layer that moves packets between
machines, underneath the layer that normally keeps programs separated from each other.

**"Traffic"** — packets moving over a network. Same usage as road traffic.

**"Gated behind"** — restricted by; you must pass a permission check to get it.

**"Demultiplexing"** — sorting one incoming stream into the right destination. Ports do this for
TCP and UDP. ICMP has no ports, so **you do it by hand** with the identifier and sequence number.

**"Encapsulation"** — wrapping one protocol's message inside another's, like a letter in an
envelope. Your ICMP message is encapsulated in an IP packet.

**"Byte order" / "endianness"** — which end of a multi-byte number goes first. Networks agreed on
one order ("network byte order"); your machine may use the other. `htons` = **h**ost **to**
**n**etwork **s**hort, `ntohs` = **n**etwork **to** **h**ost **s**hort.

**"Reentrant"** — safe to call again while an earlier call is still in progress. Signal handlers
need this because they interrupt code at arbitrary points.

**"One's complement"** — flip every bit (0 becomes 1, 1 becomes 0). The checksum is stored flipped,
which is what makes "add everything up and check for zero" work on the receiving end.

**"End-around carry"** — when addition overflows past 16 bits, add the overflow back into the
bottom instead of discarding it.

**"Pseudo-header"** — extra fields (source IP, destination IP, protocol, length) that TCP and UDP
mix into their checksums. **ICMP does not use one** — a common source of confusion.

**"Spoofing"** — putting someone else's address in the "from" field so replies go to them.
