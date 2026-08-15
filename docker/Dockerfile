# ft_ping test environment — matches the 42 evaluation platform (Debian/Ubuntu + glibc).
#
# Do NOT swap this for Alpine. Alpine uses musl libc, whose <netinet/ip_icmp.h> and
# <netinet/ip.h> differ from glibc's — glibc's struct icmphdr even contains a field
# literally named __glibc_reserved. Code that compiles here must compile on the
# evaluation machine, so the libc has to match. "Lighter" is the wrong axis to optimise.
#
# Pinned, not :latest — a floating base tag means your environment silently changes
# under you mid-project, which is the opposite of what a test environment is for.
#
# Builds for both linux/amd64 and linux/arm64 unchanged; select with the --platform
# flag at build time (run.sh does this for you). TARGETARCH is set automatically by
# buildkit and is recorded below so `docker inspect` tells you what you actually built.
FROM ubuntu:22.04
ARG TARGETARCH
LABEL org.opencontainers.image.title="ft_ping test environment" \
      org.opencontainers.image.description="glibc Linux matching the 42 eval platform" \
      ft_ping.arch="${TARGETARCH}"

ARG DEBIAN_FRONTEND=noninteractive
ARG INETUTILS_VERSION=2.0

# Toolchain + the diagnostic tools the learn/ exercises actually call for.
#   tcpdump      Stages 0, 2 and 4 all require capturing your own packets
#   iputils-ping the system ping you compare RTT against in Stage 6
#   libcap2-bin  provides setcap, for the Stage 1 unprivileged-socket exercise
#   iproute2     `ip addr` / `ip route`, to find a gateway to ping
#   dnsutils     dig, for testing the FQDN resolution path in Stage 4
#   gdb          because valgrind alone won't tell you where you are
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        make \
        gdb \
        valgrind \
        tcpdump \
        iputils-ping \
        iproute2 \
        dnsutils \
        libcap2-bin \
        curl \
        ca-certificates \
        git \
        vim \
        less \
    && rm -rf /var/lib/apt/lists/*

# Deliberately a separate layer, despite the usual "merge consecutive RUN" rule:
# this step is slow and network-dependent, and keeping it separate means a failure
# here doesn't force you to re-run the apt layer above. That is worth one extra layer.
#
# Built from source on purpose. Ubuntu 22.04's inetutils-ping package is version 2.2,
# but the subject grades you against 2.0 — and the output formatting differs between
# them. Installing the package would mean diffing against the wrong reference.
RUN curl -fsSL -o /tmp/inetutils.tar.gz \
        "https://ftp.gnu.org/gnu/inetutils/inetutils-${INETUTILS_VERSION}.tar.gz" \
    && tar xf /tmp/inetutils.tar.gz -C /tmp \
    && cd "/tmp/inetutils-${INETUTILS_VERSION}" \
    && ./configure --disable-servers --disable-doc >/dev/null \
    && make -j"$(nproc)" >/dev/null \
    && cp ping/ping /usr/local/bin/ping-ref \
    && cd / && rm -rf /tmp/inetutils*

WORKDIR /workspace

# Fail loudly and early if raw sockets are unavailable, instead of letting you
# debug a permissions problem as if it were a bug in your code.
COPY check-env.sh /usr/local/bin/check-env
RUN chmod +x /usr/local/bin/check-env

CMD ["/bin/bash"]
