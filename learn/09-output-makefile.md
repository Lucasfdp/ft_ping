# Stage 9 — Output Formatting & the Makefile

**Prerequisite:** [08-cli-errors.md](08-cli-errors.md) · **Next:** [10-bonus-flags.md](10-bonus-flags.md)

---

**Plain version:** the eval explicitly diffs your output's indentation against inetutils-2.0's
ping (except the RTT line and reverse-DNS line, which are exempted) — formatting is graded, not
cosmetic.

**Practical step:** build inetutils-2.0's ping locally and diff your output side-by-side rather
than guessing at spacing.

**Makefile:** standard 42 rules (`all`, `clean`, `fclean`, `re`), and it must only
recompile/relink what actually changed — proper dependency tracking (object files depending on
their `.c` and relevant `.h` files), not a rule that always rebuilds everything.

---

## Check questions

<details><summary>Q1: Why call out exact indentation explicitly in the subject?</summary>

The project is human-evaluated against a specific reference implementation's output. Matching
format precisely — beyond "functionally correct" — is part of what's graded, so guessing at
spacing risks losing points even with fully correct logic underneath.
</details>

<details><summary>Q2: What does "recompile only if necessary" mean in practice?</summary>

Each object file's rule lists its real source/header dependencies, so running `make` twice with no
changes does nothing, and editing one `.c` file only recompiles/relinks that file plus the final
binary — not the whole project.
</details>

---

## Exercise

### Part A — output diffing

1. Build the reference:
   ```sh
   curl -O https://ftp.gnu.org/gnu/inetutils/inetutils-2.0.tar.gz
   tar xf inetutils-2.0.tar.gz && cd inetutils-2.0
   ./configure && make
   ```
2. Capture both and diff, normalising only the exempt lines:
   ```sh
   sudo ./inetutils-2.0/ping/ping -c 3 8.8.8.8 > ref.txt 2>&1
   sudo ./ft_ping -c 3 8.8.8.8 > mine.txt 2>&1
   diff <(sed -E 's/[0-9]+\.[0-9]+ ms//' ref.txt) \
        <(sed -E 's/[0-9]+\.[0-9]+ ms//' mine.txt)
   ```
3. **Diff whitespace explicitly** — `diff` can hide it. Run `cat -A ref.txt` and `cat -A mine.txt`
   and compare trailing spaces and tabs. This is the actual graded detail.
4. Diff the error paths too: unknown host, unreachable host, `-v` output.

### Part B — Makefile

5. Use auto-generated dependencies rather than hand-listing headers (hand-listing goes stale
   silently):
   ```make
   CFLAGS += -Wall -Wextra -Werror -MMD -MP
   -include $(OBJS:.o=.d)
   ```
6. **Verify the incremental rule actually works:**
   - `make` → builds. `make` again → prints "nothing to be done". If it rebuilds, your rule is wrong.
   - `touch` one `.c` → only that object recompiles + relink.
   - `touch` a `.h` → every object that includes it recompiles. **This is the one hand-written
     Makefiles get wrong**, which is why `-MMD` matters.
7. `make re` from clean, then `make fclean` — confirm no `.o`, `.d`, or binary left behind.

**Done when:** the diff is empty apart from exempt lines, `cat -A` matches, and all three Makefile
checks in step 6 behave.

---

## Reading

- inetutils-2.0 source: `ping/ping.c` and `ping/ping_echo.c` — the actual `printf` format strings
  you're being diffed against. Read them rather than guessing
- `man 3 printf` — field width and precision specifiers (`%.3f`, `%-*s`)
- GNU Make manual, "Generating Prerequisites Automatically" — explains the `-MMD -MP` pattern
- `man 1 diff` — `-u`, and `man 1 cat` for `-A`
