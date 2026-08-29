#pragma once

/// @file ping_types.h
/// @brief Every data type and tunable constant the program shares.
///
/// Nothing in here executes: it is the vocabulary the other modules are
/// written in. Split out of the umbrella header so a module that only
/// needs the types (a unit test, for instance) does not drag in every
/// prototype in the project.

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>   /* struct sockaddr_in, INET_ADDRSTRLEN */
#include <sys/time.h>     /* struct timespec */

/* ===================================================================
   Tunables
   =================================================================== */

/// 56 data bytes + 8 header bytes = the 64 that real ping reports by
/// default. This is only the DEFAULT payload size - -s overrides it at
/// runtime, so it no longer sizes any struct.
#define PING_PAYLOAD_SIZE 56

/// Seconds between pings, real ping's default interval. -i overrides it.
#define PING_INTERVAL_SEC 1.0

/// -f floor: 100 packets/second, matching inetutils ping.c's flood intvl.
#define PING_FLOOD_INTERVAL_MS 10.0

/// One send-timestamp slot per possible uint16_t sequence number, indexed
/// [seq % PING_TS_RING]. Needed once the payload can be too small (-s) or
/// user-controlled (-p) to carry the timestamp itself.
#define PING_TS_RING 65536

/// Longest single poll() block, in ms. Keeps the cast to int safe when the
/// -t deadline is absent and the target instant is INFINITY.
#define PING_POLL_MAX_MS 3600000.0

/// Floor for the receive scratch buffer, whatever -s asks for. Matches the
/// fixed 1024-byte stack buffer this code used before -s existed.
#define PING_RECVBUF_MIN 1024

/// Slack added on top of the payload when sizing the receive buffer: an
/// inbound packet carries an IP header (up to 60 bytes) plus 8 bytes of
/// ICMP header, and an ICMP error carries a second quoted pair on top.
#define PING_RECVBUF_SLACK 128

/// Largest -p pattern, in bytes, per the reference ping man page.
#define PING_PATTERN_MAX 16

/* ===================================================================
   Wire format
   =================================================================== */

/// @brief The 8-byte ICMP echo header exactly as it appears on the wire.
///
/// The payload is a separate, variable-length buffer - it is NOT part of
/// this struct, because -s makes its length a runtime value.
typedef struct s_icmp_hdr
{
	uint8_t		type;		///< byte 0   - 8 = echo request, 0 = echo reply
	uint8_t		code;		///< byte 1   - 0 for both echo types
	uint16_t	checksum;	///< bytes 2-3 - RFC 1071, over header + payload
	uint16_t	id;			///< bytes 4-5 - our pid, identifies our traffic
	uint16_t	sequence;	///< bytes 6-7 - per-probe counter, network order
}	t_icmp_hdr;

/* No padding is allowed: this header is copied byte-for-byte onto the
   wire. If a future edit reorders members or widens a field, the compiler
   must reject it rather than silently sending garbage. */
_Static_assert(sizeof(t_icmp_hdr) == 8,
	"t_icmp_hdr has padding or the wrong size - it must be exactly 8 bytes");

/* ===================================================================
   Runtime state
   =================================================================== */

/// @brief Running totals for the final summary line.
///
/// Bundled into one struct so the receive path stays at three parameters
/// instead of piling on separate out-params for n_recv/sum/sum_sq/min/max.
typedef struct s_ping_stats
{
	int		n_sent;		///< echo requests handed to sendto() successfully
	int		n_recv;		///< echo replies matched to our id
	double	sum;		///< sum of RTTs, ms - for the mean
	double	sum_sq;		///< sum of RTT^2, ms^2 - for stddev
	double	rtt_min;	///< fastest RTT seen, ms (valid once n_recv > 0)
	double	rtt_max;	///< slowest RTT seen, ms (valid once n_recv > 0)
}	t_ping_stats;

/// @brief Every command-line option, decoded and validated.
///
/// Each option that takes a value gets a paired has_* flag rather than a
/// sentinel value, so "not given" is never confused with a legitimate 0.
typedef struct s_flags
{
	int		flood;					///< -f
	int		bsd_flood;				///< --bsd-flood: also send on every reply
	int		has_preload;			///< -l <preload> given?
	int		preload;				///< -l  unpaced packets to send first

	int		has_interval;			///< -i <wait> given?
	double	interval;				///< -i  seconds between packets

	int		has_ttl;				///< -m <ttl> given?
	int		ttl;					///< -m  IP TTL for outgoing packets

	int		exit_on_reply;			///< -o
	int		quiet_errors;			///< -Q
	int		quiet;					///< -q
	int		bypass_routing;			///< -r

	int		has_pattern;			///< -p <pattern> given?
	uint8_t	pattern[PING_PATTERN_MAX];	///< -p  decoded bytes, repeated to fill
	int		pattern_len;			///< -p  1..16 - how many of the above are used

	int		has_source_addr;		///< -S <src_addr> given?
	char	*source_addr;			///< -S  points into argv, never freed

	int		has_packet_size;		///< -s <packetsize> given?
	int		packet_size;			///< -s  payload bytes, 0..65507

	int		has_multicast_ttl;		///< -T <ttl> given?
	int		multicast_ttl;			///< -T  TTL for multicast destinations

	int		has_timeout;			///< -t <timeout> given?
	int		timeout;				///< -t  seconds before we exit regardless

	int		verbose;				///< -v

	char	*host;					///< the operand, points into argv
}	t_flags;

/// @brief Everything the send path and the receive path both need.
///
/// Bundled so those functions take one pointer instead of a growing list
/// of separate arguments. Built once by ctx_init(), after parsing and
/// resolving, then passed around read-only (the receive path takes it as
/// const). Owns three heap buffers, all released by ctx_destroy().
typedef struct s_ping_ctx
{
	t_flags				flags;						///< decoded command line
	struct sockaddr_in	dst;						///< resolved destination
	char				ipstr[INET_ADDRSTRLEN];		///< dst as printable text
	uint16_t			id;							///< our pid-derived ICMP id, network order
	uint8_t				*pkt;						///< header + payload, 8 + payload_len bytes
	size_t				payload_len;				///< payload bytes, NOT counting the 8-byte header
	struct timespec		*send_ts;					///< PING_TS_RING send timestamps, indexed by seq
	uint8_t				*recvbuf;					///< scratch buffer for recvfrom()
	size_t				recvbuf_len;				///< capacity of recvbuf, in bytes
}	t_ping_ctx;
