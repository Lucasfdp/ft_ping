# Stage 8 — CLI Parsing & Robust Error Handling

*CLI = **C**ommand-**L**ine **I**nterface — the arguments and flags the user types after the
program name.*

**Prerequisite:** [07-signals-stats.md](07-signals-stats.md) · **Next:** [09-output-makefile.md](09-output-makefile.md)

---

**Plain version:** `-v` means verbose (surface packet-level problems instead of silently ignoring
them), `-?` means print usage and exit.

**Non-negotiable per the subject:** the program must never crash unexpectedly (no segfault, bus
error, double free). Every syscall that can fail — `socket`, `sendto`, `recvfrom`, `getaddrinfo`,
… — needs its return value checked, with a clean error message and controlled exit.

**Design pattern:** centralize error reporting (a small `ft_error()`/`perror`-style helper) so
every failure path behaves consistently.

> **Names, expanded** — `perror` = **p**rint **error** · `errno` = **err**or **n**umber, the global
> holding the last failure (only meaningful *immediately* after a failed call) · `EPERM` =
> **E**rror: **PERM**ission denied · `EINVAL` = **E**rror: **INVAL**id argument · `getopt` =
> **get** **opt**ion. All error constants start with **E** for **E**rror.
> Full list in [GLOSSARY.md](GLOSSARY.md).

---

## Check questions

<details><summary>Q1: Why does the subject tie -v to surfacing packet-level problems specifically?</summary>

Problems that show up while parsing individual replies (e.g. a TTL-induced error) shouldn't halt
the whole program — expected behavior is to report them (when `-v` is active) and keep running, not
treat them as fatal.
</details>

<details><summary>Q2: What's the benefit of one centralized error helper over scattered perror() calls?</summary>

Consistency and maintainability — every error path outputs in the same format, and if you need to
change behavior later (exit codes, `-v` gating) you change it in one place instead of hunting down
every call site.
</details>

---

## Exercise

1. Write the arg parser by hand (`getopt` is usually allowed for ft_ping, but check your subject's
   allowed-functions list before relying on it). Support `-v` and `-?` minimum.
2. Write the centralized helper up front, before you need it:
   ```c
   void ft_error(const char *ctx);          /* perror-style, uses errno */
   void ft_fatal(const char *fmt, ...);     /* message + controlled exit */
   void ft_verbose(const char *fmt, ...);   /* no-op unless -v */
   ```
   Then convert every existing error site to use them. Grep your source for `perror(` and
   `exit(` afterwards — anything not in these three functions is a leak in the abstraction.
3. **Adversarial input pass.** Every one of these must produce a clean message and controlled exit,
   never a crash:

   | Input | Expected |
   |---|---|
   | `./ft_ping` (no args) | usage error, non-zero exit |
   | `./ft_ping ""` | clean resolution error |
   | `./ft_ping -z` | unknown option error |
   | `./ft_ping 999.999.999.999` | clean resolution error |
   | `./ft_ping $(python3 -c 'print("a"*5000)')` | no buffer overflow |
   | `./ft_ping -- -v` | handled sanely |
   | `./ft_ping host1 host2` | matches reference behaviour |

4. **Leak check every exit path**, not just the happy one:
   `valgrind --leak-check=full --show-leak-kinds=all ./ft_ping ...` on each row above, plus on the
   Ctrl+C path. `freeaddrinfo` and `close(sock)` are the two things people miss on error exits.
5. Confirm exit codes match reference ping (`echo $?`): 0 on replies received, non-zero on total
   loss or error.

**Done when:** every row in the table exits cleanly with zero valgrind errors and zero leaks.

---

## Reading

- `man 3 getopt` — including the `optopt`/`opterr` error-reporting mechanics
- `man 3 perror` and `man 3 errno` — and the caveat that `errno` is only meaningful immediately
  after a failed call
- `man 3 strerror` / `strerror_r` — for building your own message format
- `man 1 valgrind` — `--leak-check=full`, `--show-leak-kinds=all`, `--track-origins=yes`
- Your ft_ping subject PDF — re-read the allowed-functions list and the "never crash" clause
