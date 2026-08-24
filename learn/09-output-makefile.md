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

> **Names, expanded** — `fclean` = **f**ull **clean** · `OBJS` = **obj**ect file**s** (the `.o`
> files, compiled but not yet linked together) · **GNU** = a recursive joke, "**G**NU's **N**ot
> **U**nix" — the project behind `make`, `gcc` and the inetutils package you're diffing against ·
> **DNS** = **D**omain **N**ame **S**ystem; *reverse* DNS is the backwards lookup, address → name ·
> **RTT** = **R**ound-**T**rip **T**ime. Full list in [GLOSSARY.md](GLOSSARY.md).
>
> **"Relink"** means re-running the final step that joins the compiled `.o` files into one binary.
> **"Dependency tracking"** means the Makefile knowing which files each `.o` was built from, so it
> can tell what genuinely needs redoing.

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

1. **You don't need to build the reference yourself** — your Docker image already does this at
   build time and installs it as `ping-ref` (see `docker/Dockerfile` and `docker/check-env.sh`,
   which fails loudly if it's missing). Confirm it's there first:
   ```sh
   which ping-ref && ping-ref --version
   ```
   Only rebuild it manually if that comes back empty. If you do rebuild by hand, don't scope the
   build to just `ping/` — ping also needs other internal libs (e.g. `libicmp/`), so that fails
   with `No rule to make target '../libicmp/libicmp.a'`. You also need `--disable-ftp` at configure
   time: without it, `make` tries to build `ftp`, which fails to link on modern glibc (`undefined
   reference to rpl_glob`/`rpl_globfree`, a gnulib/glibc mismatch you don't need to fight). `-k`
   ("keep going") is a safety net on top, in case that flag ever goes unrecognized:
   ```sh
   curl -O https://ftp.gnu.org/gnu/inetutils/inetutils-2.0.tar.gz
   tar xf inetutils-2.0.tar.gz && cd inetutils-2.0
   ./configure --disable-servers --disable-ftp
   make -k -j"$(nproc)" || true
   test -f ping/ping   # confirms ping itself actually built
   ```
2. Capture both and diff, normalising only the exempt lines. **Note:** `-c count` isn't one of the
   flags you're implementing (you went with `-t timeout`/`-o` instead), so bound the reference with
   its own `-c` and bound yours from the outside with the shell's `timeout`:
   ```sh
   sudo ping-ref -c 3 8.8.8.8 > ref.txt 2>&1
   sudo timeout 3 ./ft_ping 8.8.8.8 > mine.txt 2>&1
   diff <(sed -E 's/[0-9]+\.[0-9]+ ms//' ref.txt) \
        <(sed -E 's/[0-9]+\.[0-9]+ ms//' mine.txt)
   ```
   Once `-t` is working (Stage 10), `sudo ./ft_ping -t 3 8.8.8.8` is the closer match — same idea,
   through your own flag instead of a shell wrapper.
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
   `-MMD` = **M**ake **M**ake **D**ependencies (emit a `.d` file listing which headers each `.c`
   actually included). `-MP` = **M**ake **P**hony targets, so a renamed or deleted header
   doesn't break the build. `CFLAGS` = **C** compiler **flags**.
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
