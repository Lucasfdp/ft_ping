#!/usr/bin/env bash
# Platform-aware launcher for the ft_ping test container.
#
# Written for bash 3.2 — the version macOS still ships. No associative arrays,
# no ${var,,}, no mapfile. It has to run on the Mac as well as on Linux.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_BASE="ft_ping-env"

HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"

# Normalise: macOS says arm64, Linux says aarch64, both mean the same thing.
case "$HOST_ARCH" in
    arm64|aarch64) NATIVE_ARCH="arm64" ;;
    x86_64|amd64)  NATIVE_ARCH="amd64" ;;
    *) echo "unsupported host architecture: $HOST_ARCH" >&2; exit 1 ;;
esac

PLATFORM="native"
DO_BUILD=0
FORCE_BUILD=0
RUN_CHECK=0
HOST_NET="auto"
CMD=""

usage() {
    cat <<EOF
Usage: ./run.sh [options] [-- command...]

Options:
  --platform ARCH   native (default), arm64, or amd64
                    native  = $NATIVE_ARCH here; fastest, use for daily work
                    amd64   = matches the 42 evaluation machine; use to verify
                              output formatting and struct layout before eval
  --build           build the image if it is missing
  --rebuild         force a rebuild even if the image exists
  --check           run check-env inside the container and exit
  --host-net        force host networking (Linux hosts: real ICMP, real RTT)
  --no-host-net     force bridge networking
  -h, --help        this message

Examples:
  ./run.sh                          # interactive shell, native arch
  ./run.sh --build --check          # first-time setup and verification
  ./run.sh --platform amd64         # x86_64 shell for eval-parity checks
  ./run.sh -- make re && ./ft_ping 127.0.0.1
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --platform) PLATFORM="${2:-}"; shift 2 ;;
        --build)    DO_BUILD=1; shift ;;
        --rebuild)  DO_BUILD=1; FORCE_BUILD=1; shift ;;
        --check)    RUN_CHECK=1; DO_BUILD=1; shift ;;
        --host-net) HOST_NET="yes"; shift ;;
        --no-host-net) HOST_NET="no"; shift ;;
        -h|--help)  usage; exit 0 ;;
        --)         shift; CMD="$*"; break ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

[ "$PLATFORM" = "native" ] && PLATFORM="$NATIVE_ARCH"
case "$PLATFORM" in
    amd64|arm64) ;;
    *) echo "--platform must be native, amd64 or arm64 (got: $PLATFORM)" >&2; exit 1 ;;
esac

IMAGE="${IMAGE_BASE}:${PLATFORM}"

command -v docker >/dev/null || { echo "docker not found on PATH" >&2; exit 1; }
docker info >/dev/null 2>&1 || { echo "docker daemon not reachable — is Docker Desktop running?" >&2; exit 1; }

# --- host-specific behaviour -------------------------------------------------
#
# Linux host: --network host puts the container on the real network stack, so
# ICMP is genuine and RTT figures mean something.
#
# macOS host: there is always a VM in the path. Docker Desktop supports host
# networking in recent versions, but it is the *VM's* host stack, not your Mac's,
# so it buys much less. Left off by default to avoid implying otherwise.
NET_ARGS=""
if [ "$HOST_NET" = "yes" ] || { [ "$HOST_NET" = "auto" ] && [ "$HOST_OS" = "Linux" ]; }; then
    NET_ARGS="--network host"
fi

# --- emulation warning -------------------------------------------------------
EMULATED=0
[ "$PLATFORM" != "$NATIVE_ARCH" ] && EMULATED=1

if [ "$DO_BUILD" -eq 1 ]; then
    if [ "$FORCE_BUILD" -eq 1 ] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
        echo ">> building $IMAGE (linux/$PLATFORM)"
        [ "$EMULATED" -eq 1 ] && echo "   emulated build — this will take a while"
        docker build --platform "linux/$PLATFORM" -t "$IMAGE" "$SCRIPT_DIR"
    fi
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "image $IMAGE not found. Run: ./run.sh --platform $PLATFORM --build" >&2
    exit 1
fi

echo ">> host: $HOST_OS/$NATIVE_ARCH   container: linux/$PLATFORM$([ "$EMULATED" -eq 1 ] && echo "  (emulated)")"
[ -n "$NET_ARGS" ] && echo ">> host networking enabled"
if [ "$EMULATED" -eq 1 ] && [ "$HOST_OS" = "Darwin" ]; then
    echo ">> note: valgrind is unreliable under emulation on Apple Silicon."
    echo "         Do memory checking on the native arm64 image; use this one for"
    echo "         output-format parity only."
fi

# shellcheck disable=SC2086  # NET_ARGS is intentionally word-split
if [ "$RUN_CHECK" -eq 1 ]; then
    exec docker run --rm --platform "linux/$PLATFORM" \
        --cap-add=NET_RAW $NET_ARGS \
        -v "$PROJECT_ROOT":/workspace -w /workspace \
        "$IMAGE" check-env
elif [ -n "$CMD" ]; then
    exec docker run -it --rm --platform "linux/$PLATFORM" \
        --cap-add=NET_RAW $NET_ARGS \
        -v "$PROJECT_ROOT":/workspace -w /workspace \
        "$IMAGE" /bin/bash -lc "$CMD"
else
    exec docker run -it --rm --platform "linux/$PLATFORM" \
        --cap-add=NET_RAW $NET_ARGS \
        -v "$PROJECT_ROOT":/workspace -w /workspace \
        "$IMAGE"
fi
