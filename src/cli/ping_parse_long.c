#include "ft_ping.h"

/// @file ping_parse_long.c
/// @brief The handful of `--long` options, extracted before getopt() runs.
///
/// getopt() understands single letters only. getopt_long() would handle these,
/// but it is a GNU extension and this project keeps to the functions the
/// subject's list is certain to allow - so long options are lifted out of argv
/// here and getopt() never sees them.
///
/// Doing it as a pre-pass rather than inside the switch has one more benefit:
/// the short-option table stays a flat, readable one-case-per-flag block,
/// which is the thing it is good at.

/// @brief Removes argv[i] and shifts everything after it down one place.
///
/// argv is modifiable by the standard, and the strings themselves are not
/// touched - only the array of pointers to them - so nothing is copied and
/// nothing leaks.
///
/// @param ac Current argument count.
/// @param av The argument vector to compact.
/// @param i  Index to remove.
/// @return The new argument count.
static int	drop_arg(int ac, char **av, int i)
{
	while (i + 1 < ac)
	{
		av[i] = av[i + 1];
		i++;
	}
	av[ac - 1] = NULL;
	return (ac - 1);
}

/// @brief Applies one recognised long option.
/// @param arg   The token, including its leading "--".
/// @param flags Flags to update.
/// @return 1 if the token was recognised, 0 if it was not.
static int	apply_long_opt(const char *arg, t_flags *flags)
{
	if (!strcmp(arg, "--help"))
	{
		print_usage();
		exit(EXIT_SUCCESS);
	}
	if (!strcmp(arg, "--bsd-flood"))
	{
		flags->bsd_flood = 1;
		return (1);
	}
	return (0);
}

/// @brief Lifts every recognised long option out of argv before getopt runs.
///
/// Scanning stops at a bare "--", which is the conventional end-of-options
/// marker: everything after it is an operand, even if it looks like a flag.
/// A host really named "--bsd-flood" is not a realistic worry, but honouring
/// "--" costs one line and keeps the convention intact.
///
/// @param ac    Argument count as received by main().
/// @param av    Argument vector; compacted in place.
/// @param flags Flags to update.
/// @return The reduced argument count, or -1 on an unrecognised long option
///         (message already printed).
int	extract_long_opts(int ac, char **av, t_flags *flags)
{
	int	i;

	i = 1;
	while (i < ac)
	{
		if (!strcmp(av[i], "--"))
			break ;
		if (strncmp(av[i], "--", 2) != 0)
		{
			i++;
			continue ;
		}
		if (!apply_long_opt(av[i], flags))
		{
			fprintf(stderr, "ft_ping: unrecognized option '%s'\n", av[i]);
			return (print_usage_hint(), -1);
		}
		ac = drop_arg(ac, av, i);
		/* i is NOT advanced: the shift moved a new token into this slot. */
	}
	return (ac);
}
