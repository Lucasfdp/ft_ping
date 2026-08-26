/* test_stats.c - unit tests for print_stats()
 *
 *   make test_stats && ./test_stats
 *
 * No root needed: print_stats() only formats numbers.
 *
 * These exist because the summary block is the part of the output most
 * likely to drift from the reference implementation, and the drift is
 * invisible until someone diffs it. Every expectation below is taken from
 * inetutils-2.0: ping.c:ping_finish() for the counts line and
 * ping_echo.c:echo_finish() for the round-trip line.
 */

#include "ft_ping.h"
#include <stdio.h>
#include <string.h>

static int	g_pass = 0;
static int	g_fail = 0;

/* Runs print_stats() with stdout redirected into a temporary file, so its
   output can be asserted on, then puts the real stdout back.

   dup()/dup2() on the file DESCRIPTOR, not freopen() on the FILE*: freopen
   cannot restore the original stream afterwards without reopening something
   by name, and "/dev/tty" does not exist in a container or a CI job - which
   silently swallowed the rest of this program's output when it was written
   that way. Saving fd 1 and restoring it works anywhere. */
static void	capture(const t_ping_stats *stats, char *out, size_t outlen)
{
	int		saved_fd;
	FILE	*tmp;
	long	len;

	memset(out, 0, outlen);
	tmp = tmpfile();
	if (!tmp)
		return ;
	fflush(stdout);
	saved_fd = dup(STDOUT_FILENO);
	dup2(fileno(tmp), STDOUT_FILENO);
	print_stats("host", stats);
	fflush(stdout);
	dup2(saved_fd, STDOUT_FILENO);
	close(saved_fd);
	rewind(tmp);
	len = (long)fread(out, 1, outlen - 1, tmp);
	out[len > 0 ? len : 0] = '\0';
	fclose(tmp);
}

static void	check(const char *label, const char *haystack,
		const char *needle, int want_present)
{
	int	found;

	found = (strstr(haystack, needle) != NULL);
	if (found == want_present)
	{
		printf("  PASS  %-46s\n", label);
		g_pass++;
		return ;
	}
	printf("  FAIL  %-46s\n", label);
	printf("        expected %s: \"%s\"\n",
		want_present ? "to contain" : "NOT to contain", needle);
	printf("        actual output:\n");
	printf("        | %s", haystack);
	g_fail++;
}

static t_ping_stats	make(int sent, int recv, double lo, double hi)
{
	t_ping_stats	s;

	memset(&s, 0, sizeof s);
	s.n_sent = sent;
	s.n_recv = recv;
	s.rtt_min = lo;
	s.rtt_max = hi;
	/* Two samples at lo and hi reproduce a realistic sum/sum_sq pair; for
	   recv != 2 the exact figures do not matter to what is asserted. */
	s.sum = lo + hi;
	s.sum_sq = lo * lo + hi * hi;
	return (s);
}

int	main(void)
{
	char			out[4096];
	t_ping_stats	s;

	printf("-- labels (graded against inetutils-2.0) ----------------\n");
	s = make(2, 2, 1.0, 3.0);
	capture(&s, out, sizeof out);
	check("uses stddev, the inetutils label", out, "stddev", 1);
	check("does not use mdev, the iputils label", out, "mdev", 0);
	check("header wording matches", out, "--- host ping statistics ---", 1);
	check("no blank line before the header", out, "\n---", 0);

	printf("\n-- loss percentage truncates, never rounds -------------\n");
	s = make(3, 1, 1.0, 1.0);          /* 2 lost of 3 = 66.66% */
	capture(&s, out, sizeof out);
	check("2 of 3 lost prints 66, not 67", out, "66% packet loss", 1);

	s = make(3, 2, 1.0, 1.0);          /* 1 lost of 3 = 33.33% */
	capture(&s, out, sizeof out);
	check("1 of 3 lost prints 33", out, "33% packet loss", 1);

	s = make(6, 1, 1.0, 1.0);          /* 5 lost of 6 = 83.33% */
	capture(&s, out, sizeof out);
	check("5 of 6 lost prints 83", out, "83% packet loss", 1);

	printf("\n-- degenerate cases ------------------------------------\n");
	s = make(0, 0, 0.0, 0.0);          /* Ctrl+C before the first send */
	capture(&s, out, sizeof out);
	check("nothing sent: no divide by zero", out,
		"0 packets transmitted, 0 packets received, 0% packet loss", 1);
	check("nothing sent: no round-trip line", out, "round-trip", 0);

	s = make(4, 0, 0.0, 0.0);          /* every reply lost */
	capture(&s, out, sizeof out);
	check("total loss prints 100%", out, "100% packet loss", 1);
	check("total loss: no round-trip line", out, "round-trip", 0);
	check("total loss: never prints nan", out, "nan", 0);

	printf("\n-- numerical robustness --------------------------------\n");
	s = make(2, 2, 1.0, 1.0);          /* zero variance, the NaN trap */
	capture(&s, out, sizeof out);
	check("identical samples give 0.000, not nan", out, "/0.000 ms", 1);
	check("identical samples: no nan anywhere", out, "nan", 0);

	printf("\n-- forged traffic --------------------------------------\n");
	s = make(2, 3, 1.0, 1.0);          /* more replies than probes */
	capture(&s, out, sizeof out);
	check("more replies than probes is called out", out,
		"forged packets", 1);
	check("...instead of a negative percentage", out, "-50%", 0);

	printf("\n%d passed, %d failed\n", g_pass, g_fail);
	return (g_fail != 0);
}
