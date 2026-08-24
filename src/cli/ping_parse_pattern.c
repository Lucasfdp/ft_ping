#include "ft_ping.h"
#include <ctype.h>    /* isxdigit() */
#include <stdlib.h>   /* strtol() */

/// @file ping_parse_pattern.c
/// @brief -p: hex text on the command line -> raw pad bytes.

/// @brief Rejects anything that is not a well-formed hex byte string.
///
/// All three failures share one message at the call site, because the fix
/// is the same for the user in every case: "make it valid hex, <= 32
/// characters".
///
/// @param hex NUL-terminated text from optarg.
/// @param len Its length, already measured by the caller.
/// @return 1 if @p hex is an even run of 1..32 hex digits, 0 otherwise.
static int	is_valid_hex(const char *hex, size_t len)
{
	size_t	i;

	/* Even length only: every byte needs exactly two digits. The 32-digit
	   ceiling is the documented 16-byte pad limit of reference ping. */
	if (len == 0 || len % 2 != 0 || len > PING_PATTERN_MAX * 2)
		return (0);
	i = 0;
	while (i < len)
	{
		if (!isxdigit((unsigned char)hex[i]))
			return (0);
		i++;
	}
	return (1);
}

/// @brief Decodes a -p hex string ("ff", "deadbeef") into raw bytes.
///
/// Validation happens in full before the first byte is written, so a
/// rejected pattern never leaves half of @p out modified.
///
/// @param hex     NUL-terminated hex text from optarg.
/// @param out     Buffer of at least PING_PATTERN_MAX bytes.
/// @param out_len Receives the number of bytes written, 1..16.
/// @return 1 on success, 0 on any rejection.
int	decode_pattern(const char *hex, uint8_t *out, int *out_len)
{
	size_t	len;
	size_t	i;
	char	byte_str[3];

	len = strlen(hex);
	if (!is_valid_hex(hex, len))
		return (0);
	i = 0;
	while (i < len)
	{
		/* Two digits at a time into a NUL-terminated scratch string, so
		   strtol() converts exactly one byte and cannot run past it. */
		byte_str[0] = hex[i];
		byte_str[1] = hex[i + 1];
		byte_str[2] = '\0';
		out[i / 2] = (uint8_t)strtol(byte_str, NULL, 16);
		i += 2;
	}
	*out_len = (int)(len / 2);
	return (1);
}
