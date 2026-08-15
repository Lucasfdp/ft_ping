# Convenience wrapper around run.sh. Everything here is optional — run.sh works alone.
#
# Native arch is detected rather than hardcoded, so this file behaves the same on an
# Apple Silicon Mac, an Intel Mac, and an x86_64 or arm64 Linux box.

RUN      := ./run.sh
UNAME_M  := $(shell uname -m)
ifeq ($(filter arm64 aarch64,$(UNAME_M)),)
NATIVE   := amd64
else
NATIVE   := arm64
endif

.PHONY: help build build-amd64 build-all shell amd64 check check-amd64 diff clean fix-perms

help:
	@echo "ft_ping docker environment  (native arch: $(NATIVE))"
	@echo
	@echo "  make build         build the native image      ($(NATIVE))"
	@echo "  make build-amd64   build the x86_64 image      (eval parity)"
	@echo "  make build-all     build both"
	@echo "  make shell         interactive shell, native   <- daily driver"
	@echo "  make amd64         interactive shell, x86_64   <- before eval"
	@echo "  make check         verify the native environment"
	@echo "  make check-amd64   verify the x86_64 environment"
	@echo "  make clean         remove both images"
	@echo "  make fix-perms     chown build artifacts back to you (Linux hosts)"

build:
	@$(RUN) --platform $(NATIVE) --build -- true

build-amd64:
	@$(RUN) --platform amd64 --build -- true

build-all: build build-amd64

shell:
	@$(RUN) --platform $(NATIVE)

amd64:
	@$(RUN) --platform amd64

check:
	@$(RUN) --platform $(NATIVE) --check

check-amd64:
	@$(RUN) --platform amd64 --check

clean:
	-docker image rm ft_ping-env:arm64 ft_ping-env:amd64 2>/dev/null || true

# On a Linux host the container runs as root, so anything it writes into the bind
# mount ends up root-owned on your machine. macOS/Docker Desktop maps ownership for
# you, so this is a no-op there.
fix-perms:
	@if [ "$$(uname -s)" = "Linux" ]; then \
		sudo chown -R "$$(id -u):$$(id -g)" .. && echo "ownership restored"; \
	else \
		echo "not needed on $$(uname -s)"; \
	fi
