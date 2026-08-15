#!/usr/bin/env bash
# Smoke test for the ft_ping container. Run this FIRST, before blaming your code.
#
# Exits non-zero if anything that would stop ft_ping working is broken, so it can
# be used in a Makefile target or CI step, not just read by a human.
set -uo pipefail

pass() { printf '  \033[32mok\033[0m   %s\n' "$1"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$1"; FAILED=1; }
warn() { printf '  \033[33mwarn\033[0m %s\n' "$1"; }
note() { printf '       %s\n' "$1"; }
FAILED=0

echo "ft_ping environment check"
echo

# --- platform ----------------------------------------------------------------
echo "platform"
ARCH="$(uname -m)"
printf '  container arch: %s   ' "$ARCH"
case "$ARCH" in
    x86_64)  echo "(matches the 42 evaluation machine)" ;;
    aarch64) echo "(arm64 — native on Apple Silicon)" ;;
    *)       echo "(unexpected)" ;;
esac

# Detect emulation. QEMU sets this; Rosetta usually leaves a marker binfmt entry.
EMULATED="no"
if [ -f /proc/sys/fs/binfmt_misc/status ] 2>/dev/null; then
    if grep -qs . /proc/sys/fs/binfmt_misc/qemu-x86_64 2>/dev/null; then EMULATED="qemu"; fi
fi
[ -e /proc/sys/fs/binfmt_misc/rosetta ] && EMULATED="rosetta"
if [ "$EMULATED" != "no" ]; then
    note "running under emulation ($EMULATED)"
fi

# Capture first rather than piping inline: `set -o pipefail` is active, and ldd exits
# non-zero when `head -1` closes the pipe early (SIGPIPE), which made the fallback fire
# even on success.
LIBC_VER="$(ldd --version 2>/dev/null | sed -n '1p')"
printf '  libc: %s\n' "${LIBC_VER:-unknown}"
if printf '%s' "$LIBC_VER" | grep -qi musl; then
    fail "musl libc detected — headers differ from the glibc eval machine"
    note "this image should be glibc-based (Debian/Ubuntu), not Alpine"
else
    pass "glibc (matches the evaluation platform)"
fi
echo

# --- endianness --------------------------------------------------------------
# Worth proving rather than asserting: your htons/ntohs reasoning depends on it.
echo "byte order"
printf '#include <stdio.h>\n#include <stdint.h>\nint main(void){uint16_t v=1;printf("%%s\\n", *(unsigned char*)&v ? "little" : "big");return 0;}\n' > /tmp/_endian.c
if cc /tmp/_endian.c -o /tmp/_endian 2>/dev/null; then
    E="$(/tmp/_endian)"
    if [ "$E" = "little" ]; then
        pass "little-endian — same as x86_64, so htons/ntohs behave identically"
    else
        warn "big-endian — unusual; byte-order bugs will present differently here"
    fi
fi
rm -f /tmp/_endian /tmp/_endian.c
echo

# --- the thing that actually matters -----------------------------------------
echo "raw socket capability"
# Read the EFFECTIVE capability set from /proc, not `capsh --print | grep cap_net_raw`.
# That grep matches the "Current IAB:" line, which lists every capability by name
# (negated with a leading '!') whether you hold it or not — so it reports success
# even with an empty effective set. CAP_NET_RAW is bit 13.
CAP_EFF=$(awk '/^CapEff:/ {print $2}' /proc/self/status)
if [ -n "$CAP_EFF" ] && (( 0x$CAP_EFF & (1 << 13) )); then
    pass "CAP_NET_RAW present in the effective set (CapEff=$CAP_EFF)"
else
    fail "CAP_NET_RAW absent from the effective set (CapEff=${CAP_EFF:-unknown})"
    note "add --cap-add=NET_RAW to docker run, and don't pass --user"
fi

cat > /tmp/_rawtest.c <<'EOF'
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
int main(void) {
    int s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s == -1) { perror("socket"); return 1; }
    close(s);
    return 0;
}
EOF
if cc -Wall -Wextra -Werror /tmp/_rawtest.c -o /tmp/_rawtest 2>/dev/null && /tmp/_rawtest; then
    pass "socket(AF_INET, SOCK_RAW, IPPROTO_ICMP) succeeds"
else
    fail "cannot open a raw ICMP socket — ft_ping cannot work in this container"
fi
rm -f /tmp/_rawtest /tmp/_rawtest.c
echo

# --- network -----------------------------------------------------------------
echo "outbound ICMP"
if ping -c 1 -W 2 127.0.0.1 >/dev/null 2>&1; then
    pass "loopback ping works — this is enough for Stages 0-9"
else
    fail "loopback ping fails — something is very wrong"
fi
GW="$(ip route 2>/dev/null | awk '/^default/ {print $3; exit}')"
if [ -n "$GW" ] && ping -c 1 -W 2 "$GW" >/dev/null 2>&1; then
    pass "gateway ping works ($GW)"
else
    warn "gateway unreachable${GW:+ ($GW)}"
fi
if ping -c 1 -W 3 8.8.8.8 >/dev/null 2>&1; then
    pass "external ping works"
else
    warn "external ping failed — Docker Desktop's network stack often filters ICMP"
    note "develop against 127.0.0.1 and the gateway; verify externally on real Linux"
fi
echo

# --- tooling -----------------------------------------------------------------
echo "tooling"
for t in gcc make gdb tcpdump setcap dig; do
    command -v "$t" >/dev/null && pass "$t" || fail "$t missing"
done

if command -v valgrind >/dev/null; then
    if valgrind --error-exitcode=99 /bin/true >/dev/null 2>&1; then
        pass "valgrind (working)"
    else
        warn "valgrind present but failed a trivial run"
        note "expected under emulation on Apple Silicon — use the native arm64"
        note "image for memory checking, or -fsanitize=address instead"
    fi
else
    fail "valgrind missing"
fi

# Compile and RUN a real program: -fsanitize=address can accept the flag but fail to
# link libasan, and an empty translation unit has no main to link at all.
echo 'int main(void){return 0;}' > /tmp/_asan.c
if cc -fsanitize=address /tmp/_asan.c -o /tmp/_asan 2>/dev/null && /tmp/_asan 2>/dev/null; then
    pass "AddressSanitizer available (works under emulation too)"
else
    warn "AddressSanitizer unavailable"
fi
rm -f /tmp/_asan /tmp/_asan.c
echo

# --- reference implementation ------------------------------------------------
echo "reference ping (Stage 9 diffing)"
if command -v ping-ref >/dev/null; then
    REFVER="$(ping-ref --version 2>&1 | head -1)"
    pass "ping-ref: $REFVER"
    if echo "$REFVER" | grep -q '2\.0'; then
        pass "version is inetutils 2.0, as the subject requires"
    else
        fail "version is NOT 2.0 — you would be matching the wrong output format"
    fi
else
    fail "ping-ref missing — Stage 9 output diffing will not work"
fi
echo

if [ "$FAILED" -eq 0 ]; then
    echo "environment looks good."
else
    echo "environment has problems — fix these before writing code."
    exit 1
fi
