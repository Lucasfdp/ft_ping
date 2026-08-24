#pragma once

/// @file ping_cli.h
/// @brief Command-line decoding: argv in, a validated t_flags out.
///
/// Nothing in this module touches a socket or the clock. It either fills
/// t_flags completely and returns success, or prints one diagnostic and
/// returns failure - the caller never has to inspect a half-filled struct.

#include <stdint.h>
#include "ping_types.h"

/* ---- src/cli/ping_parse_args.c ------------------------------------ */

/// @brief Parses every option and the host operand into @p flags.
///
/// Validates each value as it is read (range, root privilege, mutually
/// exclusive combinations) so the rest of the program can trust the
/// struct without re-checking. On success @c flags->host points into
/// @p av and must not be freed.
///
/// @param ac    argc as received by main().
/// @param av    argv as received by main().
/// @param flags Zeroed struct to fill.
/// @return 1 on success, 0 on any rejection (message already printed).
int	parse_info(int ac, char **av, t_flags *flags);

/* ---- src/cli/ping_parse_num.c ------------------------------------- */

/// @brief Strict string-to-number conversion for option arguments.
///
/// Stricter than strtol()/strtod() alone: trailing junk ("5x"), an empty
/// string, and out-of-range values are all rejected rather than silently
/// accepted as a partial parse.
///
/// @param str  The option argument to convert.
/// @param type Non-zero for long (strtol), 0 for double (strtod).
/// @param num  Points to a long or a double, matching @p type.
/// @return 1 if the whole string converted cleanly, 0 otherwise.
int	test_strton(char *str, int type, void *num);

/* ---- src/cli/ping_parse_pattern.c --------------------------------- */

/// @brief Decodes a -p hex string ("ff", "deadbeef") into raw bytes.
///
/// Rejects odd length, any non-hex character, and more than 32 hex
/// characters (the documented 16-byte pad limit).
///
/// @param hex     NUL-terminated hex text from optarg.
/// @param out     Buffer of at least PING_PATTERN_MAX bytes.
/// @param out_len Receives the number of bytes written, 1..16.
/// @return 1 on success, 0 on any rejection.
int	decode_pattern(const char *hex, uint8_t *out, int *out_len);
