#include "ft_ping.h"
#include <stdlib.h>   /* strtol(), strtod() */

/// @file ping_parse_num.c
/// @brief Strict numeric conversion for option arguments.

/// @brief Converts a whole string to a long or a double, or fails.
///
/// strtol()/strtod() alone are too permissive for option arguments: they
/// happily return 5 for "5x" and 0 for "abc". The three checks below turn
/// a partial parse into a rejection:
///   - end == str   nothing converted at all ("abc", "");
///   - *end != '\0' trailing junk after the number ("5x", "1.0 ");
///   - ERANGE       the value does not fit the target type.
///
/// @param str  The option argument to convert.
/// @param type Non-zero for long (strtol), 0 for double (strtod).
/// @param num  Points to a long or a double, matching @p type.
/// @return 1 if the whole string converted cleanly, 0 otherwise.
int	test_strton(char *str, int type, void *num)
{
	char	*end;

	end = NULL;
	/* errno is only meaningful after the call if we clear it first:
	   the conversion functions never set it to 0 on success. */
	errno = 0;
	if (type)
		*(long *)num = strtol(str, &end, 10);
	else
		*(double *)num = strtod(str, &end);
	if (end == str || *end != '\0' || errno == ERANGE)
		return (0);
	return (1);
}
