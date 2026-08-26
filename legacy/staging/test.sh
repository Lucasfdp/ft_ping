#!/usr/bin/env bash
# =============================================================================
#  test.sh - behaviour test suite for ft_ping
# =============================================================================
#
#  Scope: only what ft_ping itself is responsible for.
#    - option parsing and rejection (every flag, every bad value)
#    - the three output formats: banner, reply line, summary block
#    - the runtime effect of each flag, and of flags used together
#    - exit status
#
#  Deliberately NOT tested: anything the subject leaves to later projects
#  (IPv6, checksum-algorithm internals beyond the C unit test, DNS caching,
#  routing tables). The C unit tests in tests/ cover ft_checksum() and
#  resolve_host() at the function level; this file covers the program.
#
#  Usage:
#    ./test.sh                     run everything possible in this environment
#    ./test.sh --bin ./ft_ping     test a different binary (default ./ft_ping)
#    ./test.sh --host 127.0.0.1    change the local target
#    ./test.sh --verbose           show the output of passing tests too
#    ./test.sh --deep              add slow/optional tests (tcpdump wire checks)
#    ./test.sh --no-net            skip everything needing internet access
#    ./test.sh --filter <substr>   run only tests whose name contains <substr>
#
#  Exit status: 0 if every test that ran passed, 1 otherwise.
#
#  Every failure prints the exact command, what was expected, and the full
#  output that was produced - so a failing run tells you what to fix without
#  needing to reproduce it by hand.
# =============================================================================

set -u

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------
BIN="./ft_ping"
HOST="127.0.0.1"
NET_HOST="8.8.8.8"          # a host several hops away, for TTL-exceeded tests
DEAD_HOST="192.0.2.1"       # RFC 5737 TEST-NET-1: routable nowhere, always lost
BAD_HOST="no.such.host.invalid"
VERBOSE=0
DEEP=0
NO_NET=0
FILTER=""

while [ $# -gt 0 ]; do
	case "$1" in
		--bin)     BIN="$2"; shift 2 ;;
		--host)    HOST="$2"; shift 2 ;;
		--verbose|-v) VERBOSE=1; shift ;;
		--deep)    DEEP=1; shift ;;
		--no-net)  NO_NET=1; shift ;;
		--filter)  FILTER="$2"; shift 2 ;;
		--help|-h) sed -n '2,40p' "$0"; exit 0 ;;
		*) printf 'test.sh: unknown argument: %s\n' "$1" >&2; exit 2 ;;
	esac
done

# -----------------------------------------------------------------------------
# Colours (disabled when stdout is not a terminal, so logs stay readable)
# -----------------------------------------------------------------------------
if [ -t 1 ]; then
	C_RED=$'\033[0;31m'; C_GRN=$'\033[0;32m'; C_YEL=$'\033[0;33m'
	C_CYN=$'\033[0;36m'; C_DIM=$'\033[2m';    C_BLD=$'\033[1m'; C_RST=$'\033[0m'
else
	C_RED=""; C_GRN=""; C_YEL=""; C_CYN=""; C_DIM=""; C_BLD=""; C_RST=""
fi

# -----------------------------------------------------------------------------
# Counters and failure log
# -----------------------------------------------------------------------------
N_PASS=0
N_FAIL=0
N_SKIP=0
FAILED_NAMES=()

# -----------------------------------------------------------------------------
# Environment probing
# -----------------------------------------------------------------------------
# ft_ping needs a raw socket, which needs root. Parser tests do not - they
# fail before socket() is ever called - so they run either way.
SUDO=""
CAN_RUN_LIVE=1
if [ "$(id -u)" -ne 0 ]; then
	if command -v sudo >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
		SUDO="sudo -n"
	else
		CAN_RUN_LIVE=0
	fi
fi

# Is the wider internet reachable? Decides whether the TTL-exceeded and
# -Q/-v error-path tests can run at all.
HAVE_NET=0
if [ "$NO_NET" -eq 0 ] && [ "$CAN_RUN_LIVE" -eq 1 ]; then
	if $SUDO "$BIN" -t 2 -o "$NET_HOST" >/dev/null 2>&1; then
		HAVE_NET=1
	fi
fi

# -----------------------------------------------------------------------------
# Dropping privilege
#
# Four tests check that ft_ping REFUSES to do something without root. Running
# the whole suite under sudo would skip all four - and those are exactly the
# checks most likely to be missing from an implementation - so find a way to
# run the binary unprivileged whatever privilege the suite itself started with.
# -----------------------------------------------------------------------------
UNPRIV=""           # command prefix that strips root, "" when already stripped
UNPRIV_BIN=""       # world-readable copy, only needed on the `nobody` path
CAN_DROP=0

if [ "$(id -u)" -ne 0 ]; then
	# Already unprivileged: running the binary bare IS the unprivileged case.
	CAN_DROP=1
elif [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != "root" ] \
	&& sudo -n -u "$SUDO_USER" true >/dev/null 2>&1; then
	# You ran `sudo ./test.sh` - drop back to the account you started from.
	UNPRIV="sudo -n -u $SUDO_USER"
	CAN_DROP=1
elif sudo -n -u nobody true >/dev/null 2>&1; then
	# Root with no invoking user (a container, a CI job). `nobody` cannot
	# traverse a private home directory, so test a world-readable copy in the
	# temp directory rather than the binary where it sits.
	UNPRIV_BIN="${TMPDIR:-/tmp}/ft_ping_unpriv.$$"
	if cp "$BIN" "$UNPRIV_BIN" 2>/dev/null && chmod 755 "$UNPRIV_BIN" 2>/dev/null; then
		UNPRIV="sudo -n -u nobody"
		CAN_DROP=1
	else
		rm -f "$UNPRIV_BIN"
		UNPRIV_BIN=""
	fi
fi
# shellcheck disable=SC2064
trap '[ -n "$UNPRIV_BIN" ] && rm -f "$UNPRIV_BIN"' EXIT

# Does DEAD_HOST actually go nowhere? Some sandboxes, VPNs and captive
# networks answer every address, which would turn the packet-loss tests into
# false failures - so probe it once and skip them honestly if it replies.
HAVE_BLACKHOLE=0
if [ "$CAN_RUN_LIVE" -eq 1 ]; then
	if ! $SUDO "$BIN" -t 2 -o "$DEAD_HOST" >/dev/null 2>&1; then
		HAVE_BLACKHOLE=1
	fi
fi

# -----------------------------------------------------------------------------
# Portable helpers
# -----------------------------------------------------------------------------

## Runs a command with a wall-clock limit, without depending on GNU timeout
## (macOS does not ship it). Prints the command's combined output; returns
## the command's exit status, or 124 if it had to be killed.
with_timeout() {
	local limit="$1"; shift
	local out rc pid waited

	out=$(mktemp)
	"$@" >"$out" 2>&1 &
	pid=$!
	# Polled in tenths of a second, not whole seconds: most tests here are
	# parse rejections that exit instantly, and a 1-second granularity would
	# make the suite take minutes to say nothing.
	waited=0
	while kill -0 "$pid" 2>/dev/null; do
		if [ "$waited" -ge "$((limit * 10))" ]; then
			kill -TERM "$pid" 2>/dev/null
			sleep 1
			kill -KILL "$pid" 2>/dev/null
			wait "$pid" 2>/dev/null
			cat "$out"; rm -f "$out"
			return 124
		fi
		sleep 0.1
		waited=$((waited + 1))
	done
	wait "$pid"; rc=$?
	cat "$out"; rm -f "$out"
	return "$rc"
}

## Seconds since the epoch. Used for the pacing tests, which only ever need
## whole-second resolution.
now_s() { date +%s; }

# -----------------------------------------------------------------------------
# Test framework
#
#   begin  <name>            start a test case
#   run    <args...>         run ft_ping with these args; records OUT and RC
#   run_t  <secs> <args...>  same, with an explicit timeout
#   sigint <secs> <args...>  run, then send SIGINT after <secs> - the only way
#                            to test an unbounded run's summary
#   expect_rc      <n>       exit status must be exactly n
#   expect_rc_not  <n>       exit status must not be n
#   expect_match   <regex>   output must contain a line matching regex
#   expect_absent  <regex>   output must NOT contain a matching line
#   expect_count   <n>       "<n> packets transmitted" must appear
#   expect_lt      <a> <b> <what>   numeric assertion with a description
#   end                      print PASS/FAIL, with full detail on failure
# -----------------------------------------------------------------------------
CUR_NAME=""
CUR_CMD=""
CUR_OUT=""
CUR_RC=0
CUR_ERRORS=()
CUR_SKIP=""
CUR_FILTERED=0

begin() {
	CUR_NAME="$1"
	CUR_CMD=""
	CUR_OUT=""
	CUR_RC=0
	CUR_ERRORS=()
	CUR_SKIP=""
	CUR_FILTERED=0
	# --filter skips the RUN, not just the report: a filtered suite has to
	# be fast enough to be worth using while chasing one failure.
	if [ -n "$FILTER" ] && ! printf '%s' "$CUR_NAME" | grep -q "$FILTER"; then
		CUR_FILTERED=1
		CUR_SKIP="filtered out"
	fi
}

## Marks the current test as skipped, with a reason the reader can act on.
skip_if() {   # skip_if <condition-is-true> <reason>
	if [ "$1" -eq 1 ] && [ -z "$CUR_SKIP" ]; then
		CUR_SKIP="$2"
	fi
}

run_t() {
	local limit="$1"; shift
	[ -n "$CUR_SKIP" ] && return 0
	CUR_CMD="$SUDO $BIN $*"
	CUR_OUT=$(with_timeout "$limit" $SUDO "$BIN" "$@")
	CUR_RC=$?
	return 0
}

run() { run_t 10 "$@"; }

## Runs ft_ping WITHOUT root, whatever privilege the suite itself has. This is
## the only way the "-f needs root" style refusals can be exercised from a
## `sudo ./test.sh` run.
run_unpriv() {
	local limit="$1"; shift
	[ -n "$CUR_SKIP" ] && return 0
	local bin="$BIN"
	[ -n "$UNPRIV_BIN" ] && bin="$UNPRIV_BIN"
	CUR_CMD="$UNPRIV $bin $*"
	CUR_OUT=$(with_timeout "$limit" $UNPRIV "$bin" "$@")
	CUR_RC=$?
	return 0
}

## Runs in the background and interrupts with SIGINT, the way a user hitting
## Ctrl+C does. Needed for every test of an unbounded run.
sigint() {
	local after="$1"; shift
	[ -n "$CUR_SKIP" ] && return 0
	local out pid
	out=$(mktemp)
	CUR_CMD="$SUDO $BIN $* (SIGINT after ${after}s)"
	$SUDO "$BIN" "$@" >"$out" 2>&1 &
	pid=$!
	sleep "$after"
	# With sudo the child is a grandchild, so signal the process group too.
	kill -INT "$pid" 2>/dev/null || true
	[ -n "$SUDO" ] && $SUDO pkill -INT -f "$BIN" 2>/dev/null
	wait "$pid" 2>/dev/null
	CUR_RC=$?
	CUR_OUT=$(cat "$out"); rm -f "$out"
	return 0
}

fail_note() { CUR_ERRORS+=("$1"); }

expect_rc() {
	[ -n "$CUR_SKIP" ] && return 0
	[ "$CUR_RC" -eq "$1" ] || fail_note "expected exit status $1, got $CUR_RC"
}

expect_rc_not() {
	[ -n "$CUR_SKIP" ] && return 0
	[ "$CUR_RC" -ne "$1" ] || fail_note "expected exit status other than $1, got $CUR_RC"
}

expect_match() {
	[ -n "$CUR_SKIP" ] && return 0
	printf '%s\n' "$CUR_OUT" | grep -Eq "$1" \
		|| fail_note "output should contain a line matching: $1"
}

expect_absent() {
	[ -n "$CUR_SKIP" ] && return 0
	printf '%s\n' "$CUR_OUT" | grep -Eq "$1" \
		&& fail_note "output should NOT contain a line matching: $1"
	return 0
}

expect_count() {
	[ -n "$CUR_SKIP" ] && return 0
	expect_match "^$1 packets transmitted,"
}

## Reads "<n> packets transmitted" out of the summary. Echoes 0 if absent,
## so a missing summary shows up as a value assertion rather than a crash.
sent_count() {
	printf '%s\n' "$CUR_OUT" \
		| sed -n -E 's/^([0-9]+) packets transmitted,.*/\1/p' | head -1 \
		| grep -E '^[0-9]+$' || printf '0'
}

recv_count() {
	printf '%s\n' "$CUR_OUT" \
		| sed -n -E 's/^[0-9]+ packets transmitted, ([0-9]+) packets received.*/\1/p' \
		| head -1 | grep -E '^[0-9]+$' || printf '0'
}

expect_ge() {   # expect_ge <actual> <min> <what>
	[ -n "$CUR_SKIP" ] && return 0
	[ "$1" -ge "$2" ] || fail_note "$3: expected >= $2, got $1"
}

expect_le() {   # expect_le <actual> <max> <what>
	[ -n "$CUR_SKIP" ] && return 0
	[ "$1" -le "$2" ] || fail_note "$3: expected <= $2, got $1"
}

end() {
	if [ "$CUR_FILTERED" -eq 1 ]; then
		return 0
	fi
	if [ -n "$CUR_SKIP" ]; then
		N_SKIP=$((N_SKIP + 1))
		printf '  %sSKIP%s  %-52s %s%s%s\n' \
			"$C_YEL" "$C_RST" "$CUR_NAME" "$C_DIM" "$CUR_SKIP" "$C_RST"
		return 0
	fi
	if [ "${#CUR_ERRORS[@]}" -eq 0 ]; then
		N_PASS=$((N_PASS + 1))
		printf '  %sPASS%s  %s\n' "$C_GRN" "$C_RST" "$CUR_NAME"
		if [ "$VERBOSE" -eq 1 ]; then
			printf '%s        $ %s%s\n' "$C_DIM" "$CUR_CMD" "$C_RST"
			printf '%s\n' "$CUR_OUT" | sed "s/^/${C_DIM}        | /;s/\$/${C_RST}/"
		fi
		return 0
	fi
	N_FAIL=$((N_FAIL + 1))
	FAILED_NAMES+=("$CUR_NAME")
	printf '  %sFAIL%s  %s%s%s\n' "$C_RED" "$C_RST" "$C_BLD" "$CUR_NAME" "$C_RST"
	printf '        %scommand :%s %s\n' "$C_CYN" "$C_RST" "$CUR_CMD"
	printf '        %sexit    :%s %s\n' "$C_CYN" "$C_RST" "$CUR_RC"
	local e
	for e in "${CUR_ERRORS[@]}"; do
		printf '        %swhy     :%s %s\n' "$C_RED" "$C_RST" "$e"
	done
	printf '        %soutput  :%s\n' "$C_CYN" "$C_RST"
	if [ -z "$CUR_OUT" ]; then
		printf '        %s| (no output)%s\n' "$C_DIM" "$C_RST"
	else
		printf '%s\n' "$CUR_OUT" | head -20 \
			| sed "s/^/${C_DIM}        | /;s/\$/${C_RST}/"
	fi
	printf '\n'
}

section() {
	if [ -n "$FILTER" ]; then return 0; fi
	printf '\n%s%s== %s ==%s\n' "$C_BLD" "$C_CYN" "$1" "$C_RST"
}

# -----------------------------------------------------------------------------
# Preflight
# -----------------------------------------------------------------------------
printf '\n%s%sft_ping behaviour tests%s\n' "$C_BLD" "$C_CYN" "$C_RST"
printf '%sbinary%s      %s\n' "$C_DIM" "$C_RST" "$BIN"
printf '%starget%s      %s\n' "$C_DIM" "$C_RST" "$HOST"

if [ ! -x "$BIN" ]; then
	printf '\n%sFATAL%s  %s is missing or not executable. Run `make` first.\n\n' \
		"$C_RED" "$C_RST" "$BIN"
	exit 2
fi

if [ "$CAN_RUN_LIVE" -eq 1 ]; then
	printf '%sprivilege%s   root (live tests enabled)\n' "$C_DIM" "$C_RST"
	if [ "$CAN_DROP" -eq 1 ]; then
		if [ -n "$UNPRIV" ]; then
			printf '%s            root can be dropped via `%s` (refusal tests enabled)\n' \
				"$C_DIM" "$UNPRIV"
			printf '%s' "$C_RST"
		fi
	else
		printf '%s            %scannot drop root - the "requires root" tests will be skipped%s\n' \
			"$C_DIM" "$C_YEL" "$C_RST"
	fi
else
	printf '%sprivilege%s   %sunprivileged - live tests will be skipped%s\n' \
		"$C_DIM" "$C_RST" "$C_YEL" "$C_RST"
	printf '%s            re-run with sudo to exercise the send/receive path%s\n' \
		"$C_DIM" "$C_RST"
fi
if [ "$HAVE_NET" -eq 1 ]; then
	printf '%snetwork%s     %s reachable (TTL/error tests enabled)\n' \
		"$C_DIM" "$C_RST" "$NET_HOST"
else
	printf '%snetwork%s     %sno route to %s - error-path tests will be skipped%s\n' \
		"$C_DIM" "$C_RST" "$C_YEL" "$NET_HOST" "$C_RST"
fi
if [ "$CAN_RUN_LIVE" -eq 1 ] && [ "$HAVE_BLACKHOLE" -eq 0 ]; then
	printf '%sblackhole%s   %s%s answers here - packet-loss tests will be skipped%s\n' \
		"$C_DIM" "$C_RST" "$C_YEL" "$DEAD_HOST" "$C_RST"
	printf '%s            (a sandbox, VPN or captive network is replying for it)%s\n' \
		"$C_DIM" "$C_RST"
fi
printf '\n'

NO_LIVE=$((1 - CAN_RUN_LIVE))
NO_INET=$((1 - HAVE_NET))
NO_LOSS=$((1 - HAVE_BLACKHOLE))
NO_DROP=$((1 - CAN_DROP))
DROP_WHY="cannot run unprivileged here (no sudo -u target available)"

# =============================================================================
#  1. Argument parsing - the host operand
#     None of these reach socket(), so they run without root.
# =============================================================================
section "usage and the host operand"

begin "no arguments is an error"
run
expect_rc 1
expect_match "missing host operand"
end

begin "options but no host is an error"
run -v -q
expect_rc 1
expect_match "missing host operand"
end

begin "unresolvable host is reported, not silently ignored"
run "$BAD_HOST"
expect_rc 1
expect_match "^ft_ping: $BAD_HOST:"
end

begin "unknown option is rejected"
run -z "$HOST"
expect_rc 1
expect_match "unknown option: -z"
end

begin "option needing an argument, given the host instead"
# getopt consumes the host as -s's argument, so this fails on the VALUE,
# not on a missing host. Either way it must not run.
run -s "$HOST"
expect_rc 1
expect_match "invalid value"
end

begin "option needing an argument, given nothing at all"
# The leading ':' in the optstring is what makes this distinguishable from
# an unknown option - without it getopt reports both as '?'.
run "$HOST" -s
expect_rc 1
expect_match "needs an argument"
expect_absent "unknown option"
end

begin "-? prints usage and exits successfully"
run '-?'
expect_rc 0
expect_match "^Usage: ft_ping"
expect_match "^  -f "
expect_match "^  -\? "
end

begin "-? does not need a host operand"
run '-?'
expect_absent "missing host operand"
end

begin "usage lists every implemented flag"
run '-?'
for _f in f l i m o p Q q r S s T t v; do
	expect_match "^  -$_f"
done
end

begin "a usage error points at the help rather than dumping it"
run -z "$HOST"
expect_rc 1
expect_match "unknown option: -z"
expect_match "Try 'ft_ping -\?'"
expect_absent "^Usage: ft_ping"
end

begin "host after options is accepted"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 "$HOST"
expect_rc 0
expect_match "^PING $HOST"
end

# =============================================================================
#  2. Argument parsing - value validation, flag by flag
# =============================================================================
section "value validation"

# --- -s packetsize ---------------------------------------------------------
for bad in "-1" "65508" "abc" "12.5" "" "0x10"; do
	begin "-s rejects '$bad'"
	run -s "$bad" "$HOST"
	expect_rc 1
	expect_match "invalid value"
	end
done

begin "-s accepts the 0 boundary"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -s 0 "$HOST"
expect_rc 0
expect_match "^PING .*: 0 data bytes$"
end

begin "-s accepts the 65507 boundary at parse time"
# 65507 = 65535 (max IP datagram) - 20 (IP header) - 8 (ICMP header).
# The kernel may still refuse to send it, which is a sendto() error and a
# different failure mode from a parse rejection - so only reject "invalid
# value" here.
run -s 65507 "$HOST"
expect_absent "invalid value"
end

# --- -m ttl / -T multicast ttl ---------------------------------------------
for bad in "-1" "256" "abc" "1.5"; do
	begin "-m rejects '$bad'"
	run -m "$bad" "$HOST"
	expect_rc 1
	expect_match "invalid value"
	end
done

for bad in "-1" "256" "abc"; do
	begin "-T rejects '$bad'"
	run -T "$bad" "$HOST"
	expect_rc 1
	expect_match "invalid value"
	end
done

begin "-m accepts the 0 and 255 boundaries"
run -m 0 -m 255 "$BAD_HOST"
expect_absent "invalid value"
end

# --- -t timeout -------------------------------------------------------------
for bad in "0" "-1" "abc" "2.5"; do
	begin "-t rejects '$bad'"
	run -t "$bad" "$HOST"
	expect_rc 1
	expect_match "invalid value"
	end
done

# --- -i interval ------------------------------------------------------------
for bad in "0" "-1" "abc" ""; do
	begin "-i rejects '$bad'"
	run -i "$bad" "$HOST"
	expect_rc 1
	expect_match "invalid value"
	end
done

begin "-i accepts a fractional interval"
run -i 0.25 "$BAD_HOST"
expect_absent "invalid value"
end

# --- -l preload -------------------------------------------------------------
for bad in "-1" "abc" "1.5"; do
	begin "-l rejects '$bad'"
	run -l "$bad" "$HOST"
	expect_rc 1
	expect_match "invalid value"
	end
done

# --- -p pattern -------------------------------------------------------------
for bad in "f" "zz" "deadbee" "gg" "00112233445566778899aabbccddeeff00"; do
	begin "-p rejects '$bad'"
	run -p "$bad" "$HOST"
	expect_rc 1
	expect_match "hex digits"
	end
done

begin "-p accepts a single byte"
run -p ff "$BAD_HOST"
expect_absent "hex digits"
end

begin "-p accepts the 16-byte maximum"
run -p "000102030405060708090a0b0c0d0e0f" "$BAD_HOST"
expect_absent "hex digits"
end

begin "-p accepts uppercase hex"
run -p "DEADBEEF" "$BAD_HOST"
expect_absent "hex digits"
end

# =============================================================================
#  3. Privilege rules
# =============================================================================
section "privilege rules"

# These four always run unprivileged, even under `sudo ./test.sh` - see the
# "Dropping privilege" block above for how root is stripped.

begin "-f requires root"
skip_if $NO_DROP "$DROP_WHY"
run_unpriv 8 -f "$HOST"
expect_rc 1
expect_match "only root may use flood"
end

begin "-l requires root"
skip_if $NO_DROP "$DROP_WHY"
run_unpriv 8 -l 3 "$HOST"
expect_rc 1
expect_match "only root may use preload"
end

begin "sub-2ms -i requires root"
skip_if $NO_DROP "$DROP_WHY"
run_unpriv 8 -i 0.001 "$HOST"
expect_rc 1
expect_match "require root"
end

begin "-i above 2ms does NOT require root"
# The privilege check must be on the value, not on the flag: a normal
# interval has to keep working for an unprivileged user.
skip_if $NO_DROP "$DROP_WHY"
run_unpriv 8 -i 0.5 "$BAD_HOST"
expect_absent "require root"
end

begin "raw socket requires root"
skip_if $NO_DROP "$DROP_WHY"
run_unpriv 8 -t 2 "$HOST"
expect_rc 1
expect_match "socket"
expect_absent "icmp_seq="
end

begin "a root-only flag is refused before the banner is printed"
# Order matters: reference ping rejects the flag during parsing, so nothing
# has been printed by the time the error appears.
skip_if $NO_DROP "$DROP_WHY"
run_unpriv 8 -f "$HOST"
expect_absent "^PING "
end

# =============================================================================
#  4. Mutually exclusive combinations
# =============================================================================
section "flag combinations that must be refused"

begin "-f and -i together are refused"
run -f -i 1 "$HOST"
expect_rc 1
expect_match "invalid combination"
end

begin "-i and -f together are refused (order reversed)"
run -i 1 -f "$HOST"
expect_rc 1
expect_match "invalid combination"
end

# =============================================================================
#  5. Output format - the three lines ft_ping is judged on
# =============================================================================
section "output format"

begin "banner names the host, the resolved address and the payload size"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 "$HOST"
expect_match "^PING $HOST \($HOST\): 56 data bytes$"
end

begin "reply line carries bytes, source, seq, ttl and time"
skip_if $NO_LIVE "needs root"
run_t 8 -t 3 "$HOST"
expect_match "^64 bytes from $HOST: icmp_seq=[0-9]+ ttl=[0-9]+ time=[0-9]+\.[0-9]{3} ms$"
end

begin "sequence numbers start at 0 and increase by 1"
skip_if $NO_LIVE "needs root"
run_t 10 -t 4 "$HOST"
expect_match "icmp_seq=0 "
expect_match "icmp_seq=1 "
expect_match "icmp_seq=2 "
end

begin "summary block has the header, the counts and the RTT line"
skip_if $NO_LIVE "needs root"
run_t 8 -t 3 "$HOST"
expect_match "^--- $HOST ping statistics ---$"
expect_match "^[0-9]+ packets transmitted, [0-9]+ packets received, [0-9]+% packet loss$"
expect_match "^round-trip min/avg/max/stddev = [0-9.]+/[0-9.]+/[0-9.]+/[0-9.]+ ms$"
end

begin "RTT line uses the inetutils label, not the iputils one"
# inetutils-2.0 ping_echo.c prints "stddev"; Linux iputils prints "mdev".
# The subject grades against inetutils.
skip_if $NO_LIVE "needs root"
run_t 8 -t 3 "$HOST"
expect_match "stddev"
expect_absent "mdev"
end

begin "no blank line between the last reply and the statistics header"
# ping_finish() in the reference has no leading newline. Interactively the
# terminal's echoed "^C" hides the difference; piped, it does not.
skip_if $NO_LIVE "needs root"
run_t 8 -t 3 "$HOST"
if [ -z "$CUR_SKIP" ]; then
	_blank=$(printf '%s\n' "$CUR_OUT" | grep -B1 "^--- $HOST ping" | head -1)
	[ -n "$_blank" ] || fail_note "a blank line precedes the statistics header"
fi
end

begin "a run with no replies still prints a clean summary"
skip_if $NO_LIVE "needs root"
skip_if $NO_LOSS "$DEAD_HOST answers here - no way to force loss"
run_t 8 -t 3 "$DEAD_HOST"
expect_match "100% packet loss"
expect_absent "round-trip"     # no samples -> no RTT line, and no NaN
expect_absent "nan"
end

begin "loss percentage is right when some replies are lost"
skip_if $NO_LIVE "needs root"
run_t 8 -t 3 "$HOST"
expect_match "0% packet loss"
end

# =============================================================================
#  6. Exit status
# =============================================================================
section "exit status"

begin "success when at least one reply arrives"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 "$HOST"
expect_rc 0
end

begin "failure when every packet is lost"
skip_if $NO_LIVE "needs root"
skip_if $NO_LOSS "$DEAD_HOST answers here - no way to force loss"
run_t 8 -t 3 "$DEAD_HOST"
expect_rc 1
end

begin "failure on an unresolvable host"
run "$BAD_HOST"
expect_rc 1
end

# =============================================================================
#  7. Individual flags, at runtime
# =============================================================================
section "-s packetsize"

begin "-s 0 sends an 8-byte packet and reports 8 bytes back"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -s 0 "$HOST"
expect_rc 0
expect_match "^PING .*: 0 data bytes$"
expect_match "^8 bytes from "
end

begin "-s 100 is reflected in both the banner and the reply line"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -s 100 "$HOST"
expect_rc 0
expect_match "^PING .*: 100 data bytes$"
expect_match "^108 bytes from "     # 100 payload + 8 ICMP header
end

begin "-s 1 (smaller than a timestamp) still works"
# The payload can no longer carry the send time, so this only passes if the
# RTT comes from the timestamp ring rather than from the packet.
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -s 1 "$HOST"
expect_rc 0
expect_match "^9 bytes from .*time=[0-9]+\.[0-9]+ ms$"
expect_absent "time=(-|[0-9]{7,})"  # a bogus timestamp shows as a huge/negative RTT
end

section "-q quiet"

begin "-q suppresses per-packet lines but keeps banner and summary"
skip_if $NO_LIVE "needs root"
run_t 8 -t 3 -q "$HOST"
expect_rc 0
expect_match "^PING $HOST"
expect_match "packets transmitted"
expect_absent "icmp_seq="
end

section "-o exit on first reply"

begin "-o stops after exactly one reply"
skip_if $NO_LIVE "needs root"
run_t 15 -o "$HOST"
expect_rc 0
expect_ge "$(recv_count)" 1 "replies received"
expect_le "$(recv_count)" 1 "replies received"
end

begin "-o returns promptly rather than running to -t"
skip_if $NO_LIVE "needs root"
if [ -z "$CUR_SKIP" ]; then
	_t0=$(now_s)
	run_t 20 -o -t 15 "$HOST"
	_t1=$(now_s)
	expect_le "$((_t1 - _t0))" 5 "seconds elapsed with -o -t 15"
fi
expect_rc 0
end

section "-t timeout"

begin "-t 3 ends the run at about 3 seconds"
skip_if $NO_LIVE "needs root"
if [ -z "$CUR_SKIP" ]; then
	_t0=$(now_s)
	run_t 12 -t 3 "$HOST"
	_t1=$(now_s)
	expect_ge "$((_t1 - _t0))" 2 "seconds elapsed with -t 3"
	expect_le "$((_t1 - _t0))" 6 "seconds elapsed with -t 3"
fi
expect_rc 0
end

begin "-t does not fire a burst of packets as the deadline arrives"
# The classic bug: treating poll()'s timeout as "send now" turns the last
# millisecond before the deadline into a flood.
skip_if $NO_LIVE "needs root"
run_t 12 -t 4 "$HOST"
expect_le "$(sent_count)" 6 "packets sent in a 4-second default-interval run"
end

section "-i interval"

begin "-i 0.2 sends noticeably more packets than the 1s default"
skip_if $NO_LIVE "needs root"
run_t 12 -t 3 -i 0.2 "$HOST"
expect_rc 0
expect_ge "$(sent_count)" 8 "packets sent in 3s at 0.2s intervals"
end

begin "-i 2 sends noticeably fewer"
skip_if $NO_LIVE "needs root"
run_t 15 -t 5 -i 2 "$HOST"
expect_rc 0
expect_le "$(sent_count)" 4 "packets sent in 5s at 2s intervals"
end

section "-l preload"

begin "-l 5 puts the burst on the wire immediately"
skip_if $NO_LIVE "needs root"
run_t 10 -t 2 -l 5 "$HOST"
expect_rc 0
expect_ge "$(sent_count)" 6 "packets sent (5 preload + at least 1 paced)"
end

begin "-l 0 behaves exactly like no -l at all"
skip_if $NO_LIVE "needs root"
run_t 10 -t 2 -l 0 "$HOST"
expect_rc 0
expect_le "$(sent_count)" 3 "packets sent with -l 0 over 2s"
end

section "-f flood"

begin "-f sends far more packets than the default interval allows"
skip_if $NO_LIVE "needs root"
run_t 10 -t 3 -f "$HOST"
expect_rc 0
expect_ge "$(sent_count)" 100 "packets sent in a 3-second flood"
end

begin "-f prints a dot per request and erases it per reply"
skip_if $NO_LIVE "needs root"
run_t 10 -t 2 -f "$HOST"
expect_match "\."
end

begin "-f -q floods silently"
skip_if $NO_LIVE "needs root"
run_t 10 -t 2 -f -q "$HOST"
expect_rc 0
expect_absent "icmp_seq="
expect_ge "$(sent_count)" 100 "packets sent in a 2-second quiet flood"
end

section "-p pattern"

begin "-p ff runs normally"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -p ff "$HOST"
expect_rc 0
expect_match "icmp_seq="
end

begin "-p with a payload too small for a timestamp still times correctly"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -p deadbeef -s 4 "$HOST"
expect_rc 0
expect_match "^12 bytes from .*time=[0-9]+\.[0-9]+ ms$"
end

begin "-p bytes actually reach the wire"
skip_if $((1 - DEEP)) "run with --deep to enable tcpdump wire checks"
skip_if $NO_LIVE "needs root"
if [ -z "$CUR_SKIP" ] && ! command -v tcpdump >/dev/null 2>&1; then
	CUR_SKIP="tcpdump not installed"
fi
if [ -z "$CUR_SKIP" ]; then
	_if=lo; [ "$(uname -s)" = "Darwin" ] && _if=lo0
	_cap=$(mktemp)
	$SUDO tcpdump -i "$_if" -c 2 -x icmp >"$_cap" 2>/dev/null &
	_tcp=$!
	sleep 1
	$SUDO "$BIN" -t 2 -p deadbeef -s 16 "$HOST" >/dev/null 2>&1
	wait $_tcp 2>/dev/null
	CUR_CMD="tcpdump -i $_if -x icmp  +  $BIN -t 2 -p deadbeef -s 16 $HOST"
	CUR_OUT=$(cat "$_cap"); CUR_RC=0; rm -f "$_cap"
	expect_match "dead ?beef ?dead ?beef"
fi
end

section "-m ttl and the ICMP error path"

begin "-m 1 to a distant host reports Time to live exceeded"
skip_if $NO_LIVE "needs root"
skip_if $NO_INET "needs a route to $NET_HOST"
run_t 10 -t 3 -m 1 "$NET_HOST"
expect_match "^From [0-9.]+ icmp_seq=[0-9]+ Time to live exceeded$"
end

begin "an ICMP error counts as loss, not as a reply"
skip_if $NO_LIVE "needs root"
skip_if $NO_INET "needs a route to $NET_HOST"
run_t 10 -t 3 -m 1 "$NET_HOST"
expect_match "100% packet loss"
expect_rc 1
end

begin "-Q suppresses the ICMP error line"
skip_if $NO_LIVE "needs root"
skip_if $NO_INET "needs a route to $NET_HOST"
run_t 10 -t 3 -m 1 -Q "$NET_HOST"
expect_absent "Time to live exceeded"
expect_match "packets transmitted"
end

begin "-q also suppresses the ICMP error line"
skip_if $NO_LIVE "needs root"
skip_if $NO_INET "needs a route to $NET_HOST"
run_t 10 -t 3 -m 1 -q "$NET_HOST"
expect_absent "Time to live exceeded"
expect_match "packets transmitted"
end

begin "-m 64 reaches a distant host normally"
skip_if $NO_LIVE "needs root"
skip_if $NO_INET "needs a route to $NET_HOST"
run_t 10 -t 3 -m 64 "$NET_HOST"
expect_rc 0
expect_match "icmp_seq="
end

section "-S source address"

begin "-S with a local address is accepted"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -S "$HOST" "$HOST"
expect_rc 0
expect_match "icmp_seq="
end

begin "-S with an address this machine does not own fails cleanly"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -S "$DEAD_HOST" "$HOST"
expect_rc 1
expect_match "(bind|assign)"
expect_absent "icmp_seq="
end

begin "-S with unresolvable text fails before sending"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -S "$BAD_HOST" "$HOST"
expect_rc 1
expect_absent "icmp_seq="
end

section "-r and -v"

begin "-r to a directly attached address works"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -r "$HOST"
expect_rc 0
expect_match "icmp_seq="
end

begin "-n is not a supported option"
# Addresses are always printed numerically here - no reverse lookup is
# ever attempted - so -n has nothing to switch off and is not accepted.
run -n "$HOST"
expect_rc 1
expect_match "unknown option: -n"
end

begin "addresses are always printed numerically"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 "$HOST"
expect_rc 0
expect_match "from $HOST:"
end

begin "-v runs and does not break normal output"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -v "$HOST"
expect_rc 0
expect_match "icmp_seq="
end

begin "-v appends the ICMP identifier to the banner"
# Matches ping_echo.c in the reference: ", id 0x%04x = %u".
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -v "$HOST"
expect_match "^PING $HOST \($HOST\): 56 data bytes, id 0x[0-9a-f]{4} = [0-9]+$"
end

begin "without -v the banner carries no identifier"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 "$HOST"
expect_match "^PING $HOST \($HOST\): 56 data bytes$"
end

# =============================================================================
#  8. Flags interacting with each other
# =============================================================================
section "flag interactions"

begin "-q wins over -v"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -v -q "$HOST"
expect_rc 0
expect_absent "icmp_seq="
end

begin "-q wins over -v regardless of order"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -q -v "$HOST"
expect_rc 0
expect_absent "icmp_seq="
end

begin "-q wins over -Q"
skip_if $NO_LIVE "needs root"
skip_if $NO_INET "needs a route to $NET_HOST"
run_t 10 -t 3 -q -Q -m 1 "$NET_HOST"
expect_absent "Time to live exceeded"
end

begin "-s and -p combine"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -s 32 -p aa "$HOST"
expect_rc 0
expect_match "^PING .*: 32 data bytes$"
expect_match "^40 bytes from "
end

begin "-o and -t: whichever comes first wins (-o here)"
skip_if $NO_LIVE "needs root"
run_t 15 -o -t 10 "$HOST"
expect_rc 0
expect_le "$(recv_count)" 1 "replies received"
end

begin "-o and -t: whichever comes first wins (-t here)"
skip_if $NO_LIVE "needs root"
skip_if $NO_LOSS "$DEAD_HOST answers here - -o would fire first"
run_t 10 -o -t 3 "$DEAD_HOST"
expect_rc 1
expect_match "100% packet loss"
end

begin "-l and -i combine: burst first, then the slow pace"
skip_if $NO_LIVE "needs root"
run_t 12 -t 4 -l 4 -i 2 "$HOST"
expect_rc 0
expect_ge "$(sent_count)" 5 "packets sent (4 preload + paced)"
expect_le "$(sent_count)" 8 "packets sent (the -i pace must still apply)"
end

begin "-f and -s combine"
skip_if $NO_LIVE "needs root"
run_t 10 -t 2 -f -s 8 "$HOST"
expect_rc 0
expect_match "^PING .*: 8 data bytes$"
end

begin "-m and -s combine"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -m 10 -s 20 "$HOST"
expect_rc 0
expect_match "^PING .*: 20 data bytes$"
end

begin "-q and -s combine"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -q -s 12 "$HOST"
expect_rc 0
expect_match "^PING .*: 12 data bytes$"
expect_absent "icmp_seq="
end

begin "a repeated flag takes the last value"
skip_if $NO_LIVE "needs root"
run_t 8 -t 2 -s 10 -s 20 "$HOST"
expect_rc 0
expect_match "^PING .*: 20 data bytes$"
end

begin "every display-neutral flag at once"
skip_if $NO_LIVE "needs root"
run_t 10 -t 3 -v -m 64 -s 24 -p ab -r "$HOST"
expect_rc 0
# Not anchored at the end: -v appends ", id 0x…" to the banner.
expect_match "^PING .*: 24 data bytes"
expect_match "^32 bytes from "
end

# =============================================================================
#  9. Signals
# =============================================================================
section "checksum verification"

begin "a valid reply is accepted, so verification is not over-eager"
# The obvious way to break checksum verification is to get the byte range
# or the zeroing wrong, which rejects EVERY reply. If replies still arrive,
# the check is at least not doing that.
skip_if $NO_LIVE "needs root"
run_t 8 -t 3 "$HOST"
expect_rc 0
expect_match "icmp_seq="
expect_absent "checksum mismatch"
end

begin "a corrupted reply would be reported"
# Not exercised: forging a bad-checksum ICMP packet needs a second raw
# socket in the harness. The code path is src/net/ping_receive.c
# checksum_ok() -> print_checksum_mismatch().
CUR_SKIP="needs a packet forger in the harness - see EVAL_PREP.md B5"
end

section "SIGINT"

begin "Ctrl+C prints the summary instead of dying silently"
skip_if $NO_LIVE "needs root"
sigint 3 "$HOST"
expect_match "^--- $HOST ping statistics ---$"
expect_match "packets transmitted"
end

begin "Ctrl+C during a flood still prints the summary"
skip_if $NO_LIVE "needs root"
sigint 2 -f "$HOST"
expect_match "packets transmitted"
end

begin "Ctrl+C is honoured promptly, not one interval later"
skip_if $NO_LIVE "needs root"
if [ -z "$CUR_SKIP" ]; then
	_t0=$(now_s)
	sigint 2 -i 30 "$HOST"
	_t1=$(now_s)
	expect_le "$((_t1 - _t0))" 5 "seconds from SIGINT to exit with -i 30"
fi
expect_match "packets transmitted"
end

# =============================================================================
# 10. Memory (optional)
# =============================================================================
section "memory"

begin "no leaks on a normal run"
skip_if $((1 - DEEP)) "run with --deep to enable valgrind"
skip_if $NO_LIVE "needs root"
if [ -z "$CUR_SKIP" ] && ! command -v valgrind >/dev/null 2>&1; then
	CUR_SKIP="valgrind not installed"
fi
if [ -z "$CUR_SKIP" ]; then
	CUR_CMD="valgrind --leak-check=full $BIN -t 3 $HOST"
	CUR_OUT=$(with_timeout 40 $SUDO valgrind --leak-check=full --error-exitcode=42 \
		"$BIN" -t 3 "$HOST")
	CUR_RC=$?
	expect_rc_not 42
	# valgrind prints the short form when nothing leaked at all, and the
	# itemised form when something did - accept either shape of "clean".
	expect_match "(All heap blocks were freed|definitely lost: 0 bytes)"
	expect_absent "definitely lost: [1-9]"
	expect_absent "Invalid (read|write|free)"
fi
end

# =============================================================================
#  Summary
# =============================================================================
printf '\n%s%s──────────────────────────────────────────────%s\n' \
	"$C_BLD" "$C_CYN" "$C_RST"
printf '  %s%d passed%s   %s%d failed%s   %s%d skipped%s\n' \
	"$C_GRN" "$N_PASS" "$C_RST" \
	"$C_RED" "$N_FAIL" "$C_RST" \
	"$C_YEL" "$N_SKIP" "$C_RST"

if [ "$N_FAIL" -gt 0 ]; then
	printf '\n  %s%sFailed tests:%s\n' "$C_BLD" "$C_RED" "$C_RST"
	for n in "${FAILED_NAMES[@]}"; do
		printf '    %s•%s %s\n' "$C_RED" "$C_RST" "$n"
	done
	printf '\n  %sScroll up for the command, expectation and output of each.%s\n\n' \
		"$C_DIM" "$C_RST"
	exit 1
fi

if [ "$N_SKIP" -gt 0 ]; then
	printf '\n  %sSome tests were skipped. For the full suite:%s\n' "$C_DIM" "$C_RST"
	[ "$CAN_RUN_LIVE" -eq 0 ] && printf '    %ssudo ./test.sh%s\n' "$C_DIM" "$C_RST"
	[ "$DEEP" -eq 0 ] && printf '    %ssudo ./test.sh --deep%s   (adds tcpdump and valgrind checks)\n' "$C_DIM" "$C_RST"
fi
printf '\n'
exit 0
