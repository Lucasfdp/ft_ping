# =============================================================================
#  ft_ping
#
#  Source layout:
#    include/        one header per module, plus the ft_ping.h umbrella
#    src/cli/        argv        -> t_flags
#    src/net/        t_ping_ctx  -> the wire
#    src/output/     results     -> stdout
#    src/core/       program lifetime and the event loop
#    src/utils/      clocks and fatal exits
#    tests/          standalone unit tests, one binary each
#
#  Sources are DISCOVERED, not listed: adding a .c anywhere under src/ picks
#  it up with no edit here. Object files mirror the source tree under obj/.
# =============================================================================

NAME		:=	ft_ping
TESTS		:=	test_checksum test_resolve test_stats

# ==========================
# Compiler detection
# ==========================
# `cc` is not guaranteed to exist. It is a traditional alias, and a minimal
# Debian/Ubuntu VM - a 42 image, a container, a fresh cloud box - can ship
# gcc or clang without ever creating it.
#
# When it is missing, the compile rule below dies with a bare
#
#     make: *** [Makefile:NN: obj/src/cli/ping_parse_pattern.o] Error 127
#
# 127 is the shell's code for "command not found", but make reports the
# TARGET rather than the missing command, so the message reads as though the
# object file were the problem. It is not: the compiler is.
#
# So find one, and if there is genuinely none, say something actionable
# instead of failing halfway through a build. A command-line override always
# wins over everything here:  make CC=clang
CC			:=	$(firstword $(foreach c,cc gcc clang,\
					$(if $(shell command -v $(c) 2>/dev/null),$(c))))
ifeq ($(strip $(CC)),)
$(error No C compiler found - looked for cc, gcc and clang on your PATH. \
Install one with `sudo apt install build-essential` (Debian/Ubuntu) or \
`xcode-select --install` (macOS), or point this at yours: make CC=/path/to/gcc)
endif

CFLAGS		:=	-Wall -Wextra -Werror -MMD -MP
CPPFLAGS	:=	-Iinclude
LDLIBS		:=	-lm   # sqrt() in print_stats()

SRC_DIR		:=	src
TEST_DIR	:=	tests
OBJ_DIR		:=	obj

# ==========================
# Source discovery
# ==========================
SRCS		:=	$(shell find $(SRC_DIR) -name '*.c')
OBJS		:=	$(SRCS:%.c=$(OBJ_DIR)/%.o)

# Each test links against the module(s) it exercises, never against main.c.
CK_SRCS		:=	$(TEST_DIR)/test_checksum.c $(SRC_DIR)/net/ping_checksum.c
CK_OBJS		:=	$(CK_SRCS:%.c=$(OBJ_DIR)/%.o)

RS_SRCS		:=	$(TEST_DIR)/test_resolve.c $(SRC_DIR)/net/ping_resolve.c
RS_OBJS		:=	$(RS_SRCS:%.c=$(OBJ_DIR)/%.o)

ST_SRCS		:=	$(TEST_DIR)/test_stats.c $(SRC_DIR)/output/ping_print_report.c
ST_OBJS		:=	$(ST_SRCS:%.c=$(OBJ_DIR)/%.o)

ALL_OBJS	:=	$(sort $(OBJS) $(CK_OBJS) $(RS_OBJS) $(ST_OBJS))

TOTAL		:=	$(words $(SRCS))
COMPILED	:=	0
MAKEFLAGS	+=	--no-print-directory

# ==========================
# Colours
# ==========================
GREEN		= \033[0;32m
BLUE		= \033[0;34m
CYAN		= \033[0;36m
YELLOW		= \033[0;33m
RED			= \033[0;31m
MAGENTA		= \033[0;35m
BOLD		= \033[1m
RESET		= \033[0m

# ==========================
# Targets
# ==========================
all: $(NAME)

$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -o $(NAME)
	@$(MAKE) banner
	@printf "$(GREEN)$(BOLD) [ft_ping compiled successfully]$(RESET)\n\n"

# Compile with progress bar. The mkdir handles the mirrored obj/ subtree,
# so a new src/<module>/ directory needs no rule of its own.
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(eval COMPILED := $(shell echo $$(($(COMPILED) + 1))))
	@$(eval PCT := $(shell echo $$(($(COMPILED) * 100 / $(TOTAL)))))
	@$(eval FILLED := $(shell if [ $(COMPILED) -ge $(TOTAL) ]; then echo 20; else echo $$(($(COMPILED) * 20 / $(TOTAL))); fi))
	@$(eval EMPTY := $(shell echo $$(( 20 - $(FILLED)))))
	@printf "\r$(CYAN)  Compiling $(BOLD)%-30s$(RESET)$(CYAN) [" "$(notdir $<)"
	@i=0; while [ $$i -lt $(FILLED) ]; do printf "$(GREEN)█$(RESET)"; i=$$((i+1)); done
	@i=0; while [ $$i -lt $(EMPTY) ]; do printf "░"; i=$$((i+1)); done
	@printf "$(CYAN)] $(BOLD)%3d%%$(RESET)" $(PCT)
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# ==========================
# Unit tests (C)
# ==========================
test: $(TESTS)
	@printf "$(YELLOW)$(BOLD)\n  Running unit tests...$(RESET)\n\n"
	@for t in $(TESTS); do printf "$(CYAN)  == %s ==$(RESET)\n" "$$t"; ./$$t || exit 1; done
	@printf "$(GREEN)$(BOLD)\n  [All tests passed]$(RESET)\n\n"

test_checksum: $(CK_OBJS)
	@$(CC) $(CFLAGS) $(CK_OBJS) $(LDLIBS) -o $@

test_resolve: $(RS_OBJS)
	@$(CC) $(CFLAGS) $(RS_OBJS) $(LDLIBS) -o $@

test_stats: $(ST_OBJS)
	@$(CC) $(CFLAGS) $(ST_OBJS) $(LDLIBS) -o $@

# ==========================
# Behaviour tests (shell)
# ==========================
# Full flag matrix against the built binary. Pass FLAGS to test.sh through
# ARGS, e.g. `make check-behaviour ARGS="-v"` or ARGS="--host 127.0.0.1".
check-behaviour: $(NAME)
	@chmod +x ./test.sh
	@./test.sh $(ARGS)

# ==========================
# Valgrind
# ==========================
valgrind: $(NAME)
	@printf "$(YELLOW)$(BOLD)\n  Running valgrind on $(NAME) (needs sudo for raw sockets)...$(RESET)\n\n"
	sudo valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--track-fds=yes \
		--error-exitcode=1 \
		--verbose \
		./$(NAME) 127.0.0.1

# ==========================
# Debug build
# ==========================
debug: CFLAGS += -g3 -fsanitize=address
debug: re
	@printf "$(RED)$(BOLD)  [Debug build with ASan ready]$(RESET)\n\n"

# ==========================
# Run
# ==========================
run: $(NAME)
	@printf "$(GREEN)$(BOLD)  Starting ft_ping (needs sudo for raw sockets)...$(RESET)\n\n"
	sudo ./$(NAME) $(ARGS)

# ==========================
# Check style / warnings
# ==========================
check: CFLAGS += -Wpedantic -Wshadow -Wconversion
check: re
	@printf "$(MAGENTA)$(BOLD)  [Strict warning check complete]$(RESET)\n\n"

# ==========================
# Toolchain report
# ==========================
# Run this first on any machine that will not build. It answers the questions
# a bare "Error 127" cannot: which compiler was found, whether the optional
# tools the test targets need are present, and what make itself is.
env:
	@printf "$(CYAN)$(BOLD)\n  Toolchain$(RESET)\n\n"
	@printf "    %-14s %s\n" "compiler"  "$(CC)"
	@printf "    %-14s %s\n" "path"      "$$(command -v $(CC) 2>/dev/null || echo '(not found)')"
	@printf "    %-14s %s\n" "version"   "$$($(CC) --version 2>/dev/null | head -1 || echo '(unknown)')"
	@printf "    %-14s %s\n" "make"      "$$($(MAKE) --version 2>/dev/null | head -1)"
	@printf "    %-14s %s\n" "uname"     "$$(uname -srm)"
	@printf "$(CYAN)$(BOLD)\n  Optional tools$(RESET)\n\n"
	@for t in valgrind tcpdump sudo ping-ref clear; do \
		p=$$(command -v $$t 2>/dev/null); \
		if [ -n "$$p" ]; then \
			printf "    $(GREEN)✓$(RESET) %-12s %s\n" "$$t" "$$p"; \
		else \
			printf "    $(YELLOW)-$(RESET) %-12s (not installed)\n" "$$t"; \
		fi; \
	done
	@printf "$(CYAN)$(BOLD)\n  Sources found$(RESET)\n\n"
	@printf "    %-14s %s\n" ".c under src/" "$(TOTAL)"
	@if [ "$(TOTAL)" -eq 0 ]; then \
		printf "    $(RED)no sources found - are you running make from the repo root?$(RESET)\n"; \
	fi
	@printf "\n"

# ==========================
# Count lines of code
# ==========================
cloc:
	@printf "$(CYAN)$(BOLD)\n  Lines of code:$(RESET)\n\n"
	@find $(SRC_DIR) include $(TEST_DIR) -name "*.c" -o -name "*.h" | \
		xargs wc -l | sort -rn | head -30
	@printf "\n"

# ==========================
# Show the source tree
# ==========================
list:
	@printf "$(CYAN)$(BOLD)\n  Sources ($(TOTAL) files):$(RESET)\n"
	@for f in $(SRCS); do printf "    $(GREEN)→$(RESET) $$f\n"; done
	@printf "$(CYAN)$(BOLD)\n  Headers:$(RESET)\n"
	@for f in include/*.h; do printf "    $(BLUE)→$(RESET) $$f\n"; done
	@printf "$(CYAN)$(BOLD)\n  Tests:$(RESET)\n"
	@for f in $(TEST_DIR)/*.c test.sh; do printf "    $(YELLOW)→$(RESET) $$f\n"; done
	@printf "\n"

# ==========================
# Push to GitHub
# ==========================
# Usage: make push MSG="commit message"
MSG			?=	update: $(shell date "+%Y-%m-%d %H:%M")
BRANCH		:=	$(shell git rev-parse --abbrev-ref HEAD 2>/dev/null)

push:
	@printf "$(YELLOW)$(BOLD)\n  Pushing to GitHub ($(BRANCH))...$(RESET)\n\n"
	@git add -A
	@if git diff --cached --quiet; then \
		printf "$(CYAN)  Nothing to commit, pushing anyway...$(RESET)\n"; \
	else \
		git commit -m "$(MSG)"; \
	fi
	@git push origin $(BRANCH)
	@printf "$(GREEN)$(BOLD)\n  [Pushed to $(BRANCH)]$(RESET)\n\n"

# ==========================
# Banner
# ==========================
banner:
	@# `clear` lives in ncurses-bin, which a minimal VM may not have - and a
	@# missing command here would fail the build with the same Error 127 the
	@# compiler check above exists to explain. Skip it rather than die for a
	@# cosmetic reason.
	@if [ -t 1 ] && [ -n "$$TERM" ] && command -v clear >/dev/null 2>&1; then clear; fi
	@printf "\n$(CYAN)"
	@sleep 0.05
	@printf "%s\n" "    ______              _            "
	@sleep 0.15
	@printf "%s\n" "   / __/ /_      ____  (_)___  ____ _"
	@sleep 0.15
	@printf "%s\n" "  / /_/ __/     / __ \\/ / __ \\/ __ \`/"
	@sleep 0.15
	@printf "%s\n" " / __/ /_      / /_/ / / / / / /_/ / "
	@sleep 0.15
	@printf "%s\n" "/_/  \\__/_____/ .___/_/_/ /_/\\__, /  "
	@sleep 0.15
	@printf "%s\n" "       /_____/_/            /____/   "
	@printf "$(RESET)\n"

# ==========================
# Clean
# ==========================
clean:
	@rm -rf $(OBJ_DIR)
	@printf "$(BLUE)\n  Cleansed$(RESET)\n"

fclean: clean
	@rm -f $(NAME) $(TESTS)
	@printf "$(BLUE)  More Cleansed$(RESET)\n\n"

re: fclean all

-include $(ALL_OBJS:.o=.d)

# ==========================
# Help
# ==========================
help:
	@printf "\n$(CYAN)$(BOLD)  ft_ping — available commands$(RESET)\n\n"
	@printf "  $(GREEN)make$(RESET)                  — build ft_ping\n"
	@printf "  $(GREEN)make re$(RESET)               — clean and rebuild\n"
	@printf "  $(GREEN)make clean$(RESET)            — remove object files\n"
	@printf "  $(GREEN)make fclean$(RESET)           — remove objects, binary, and test binaries\n"
	@printf "  $(GREEN)make test$(RESET)             — build and run the C unit tests\n"
	@printf "  $(GREEN)make check-behaviour$(RESET)  — run test.sh, the full flag matrix\n"
	@printf "  $(GREEN)make run$(RESET)              — build and run ft_ping (sudo, pass ARGS=\"...\")\n"
	@printf "  $(GREEN)make debug$(RESET)            — build with AddressSanitizer and debug symbols\n"
	@printf "  $(GREEN)make valgrind$(RESET)         — run ft_ping under valgrind (sudo)\n"
	@printf "  $(GREEN)make check$(RESET)            — rebuild with extra pedantic warnings\n"
	@printf "  $(GREEN)make env$(RESET)              — report the toolchain (run this if the build fails)\n"
	@printf "  $(GREEN)make cloc$(RESET)             — count lines of code per file\n"
	@printf "  $(GREEN)make list$(RESET)             — list the source tree\n"
	@printf "  $(GREEN)make push$(RESET)             — commit and push to GitHub (pass MSG=\"...\")\n"
	@printf "  $(GREEN)make help$(RESET)             — show this message\n"
	@printf "\n$(CYAN)  Usage:$(RESET)\n"
	@printf "  $(YELLOW)sudo ./ft_ping <host> [options]$(RESET)\n\n"

.PHONY: all test check-behaviour clean fclean re valgrind debug run check env cloc list banner push help
