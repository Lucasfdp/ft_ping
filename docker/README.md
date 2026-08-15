# ft_ping test container

A glibc Linux environment matching the 42 evaluation platform, so macOS/Linux
differences don't surprise you at eval time. Works on Apple Silicon, Intel Macs, and
Linux hosts, on either CPU architecture.

## Quick start

```sh
cd docker
./run.sh --build --check      # build the native image and verify it
./run.sh                      # interactive shell in /workspace
```

Or via make, if you prefer:

```sh
make build && make check
make shell
```

`run.sh` finds the project root itself, so you can invoke it from anywhere.

---

## Which platform should I use?

You're on an **M2, so your native architecture is arm64**, but the evaluation machine is
x86_64. That's a real difference, but a narrower one than it sounds.

| | native arm64 | emulated amd64 | real Linux x86_64 |
|---|---|---|---|
| Speed | fast | slow | fast |
| Byte order | little-endian | little-endian | little-endian |
| glibc headers | identical | identical | identical |
| valgrind | works | unreliable | works |
| Output formatting | identical | identical | identical |
| Verdict | **daily driver** | pre-eval spot check | final verification |

**Use arm64 for essentially everything.** Both architectures are little-endian, so all
your `htons`/`ntohs` reasoning behaves identically — the byte-order bugs you're worried
about will reproduce on arm64 exactly as they would on x86_64. The struct layouts in
`netinet/ip.h` and `netinet/ip_icmp.h` come from the same glibc and are the same size on
both.

```sh
./run.sh                       # arm64, native, fast
make shell
```

**Use amd64 before you submit**, as a paranoia check on struct sizes and output format:

```sh
./run.sh --platform amd64 --build --check
./run.sh --platform amd64
make amd64
```

Speed it up in Docker Desktop → Settings → General → **Use Rosetta for x86_64/amd64
emulation**. Without it you get QEMU, which is considerably slower.

**One thing that genuinely breaks under emulation: valgrind.** Both QEMU and Rosetta
conflict with valgrind's dynamic recompilation. `check-env` detects this and tells you.
Do all your Stage 8 memory checking on the native arm64 image, or use
`-fsanitize=address`, which works fine on both.

---

## Host differences

**macOS (any Mac).** Container traffic goes through Docker Desktop's VM and a userspace
network proxy. RTT figures are not representative of a real network, and external ICMP is
often filtered outright. This does not block you — Stages 0 through 9 only need
`127.0.0.1` and your gateway. Just don't tune timing behaviour against numbers from here.

**Linux host.** `run.sh` enables `--network host` automatically, which puts the container
on the real network stack: genuine ICMP, meaningful RTT. Override with `--no-host-net` if
you'd rather not.

**Linux host, file ownership.** The container runs as root (it needs to, for raw
sockets), so build artifacts land in the bind mount root-owned. `make fix-perms` chowns
them back. macOS maps ownership for you, so this is a no-op there.

---

## First thing, every session

```sh
./run.sh --check
```

Verifies raw sockets work, glibc not musl, endianness, ICMP reachability, valgrind
actually runs, and that the reference ping is the right version. **Run this before
concluding you have a bug** — a permissions problem that looks like a logic bug will cost
you an afternoon. It exits non-zero on failure, so you can wire it into CI.

---

## Diffing against the reference (Stage 9)

The image builds inetutils-2.0 from source and installs it as `ping-ref`:

```sh
ping-ref -c 3 8.8.8.8 > ref.txt 2>&1
./ft_ping -c 3 8.8.8.8 > mine.txt 2>&1
diff <(sed -E 's/[0-9]+\.[0-9]+ ms//' ref.txt) <(sed -E 's/[0-9]+\.[0-9]+ ms//' mine.txt)
cat -A ref.txt    # whitespace is graded; diff can hide it
```

Built from source deliberately: Ubuntu 22.04's `inetutils-ping` package is **2.2**, and
the subject grades against **2.0**. The output formatting differs between them, so
installing the package would mean carefully matching the wrong target.

---

## Gotchas

**Build artifacts collide across platforms.** The bind mount is shared between macOS, the
arm64 container and the amd64 container. `make` compares timestamps, not architectures —
so it will happily try to link a Mach-O object against an ELF one, or an arm64 `.o`
against x86_64, and fail confusingly. **Run `make fclean` whenever you switch platforms.**

**Don't add `--user`.** It strips CAP_NET_RAW and raw sockets stop working. If root-owned
files on a Linux host bother you, use `make fix-perms` rather than dropping privileges.

**Don't swap to Alpine.** musl libc's `<netinet/ip_icmp.h>` differs from glibc's — glibc's
`struct icmphdr` even has a field named `__glibc_reserved`. `check-env` fails loudly if it
ever finds musl. Image size is the wrong thing to optimise here.

**`--rm` is intentional.** With the toolchain baked into the image, containers are
disposable. Anything worth keeping belongs in the Dockerfile.

---

## Files

| File | Purpose |
|---|---|
| `Dockerfile` | The image. Builds unchanged for amd64 and arm64. |
| `run.sh` | Platform-aware launcher. Detects host OS and arch. |
| `check-env.sh` | Smoke test, baked into the image as `check-env`. |
| `Makefile` | Optional convenience targets around `run.sh`. |
| `.dockerignore` | Keeps the build context to just `check-env.sh`. |
