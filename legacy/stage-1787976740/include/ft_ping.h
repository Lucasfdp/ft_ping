#pragma once

/// @file ft_ping.h
/// @brief Umbrella header: include this one and you have the whole API.
///
/// Every .c file in the project includes exactly this. The real
/// declarations live in one header per module, so you can see at a glance
/// which layer a function belongs to without reading its implementation:
///
///   ping_types.h   shared structs and tunable constants
///   ping_cli.h     argv        -> t_flags            (src/cli)
///   ping_net.h     t_ping_ctx  -> the wire           (src/net)
///   ping_output.h  results     -> stdout             (src/output)
///   ping_core.h    program lifetime and the loop     (src/core)
///   ping_utils.h   clocks and fatal exits            (src/utils)
///
/// The system headers below are the ones nearly every module needs; a
/// module that needs something narrower (poll.h, signal.h, ctype.h)
/// includes it itself, next to the code that uses it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "ping_types.h"
#include "ping_utils.h"
#include "ping_cli.h"
#include "ping_net.h"
#include "ping_output.h"
#include "ping_core.h"
