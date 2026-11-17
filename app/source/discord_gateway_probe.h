// SPDX-License-Identifier: MIT
#ifndef DISCORD_GATEWAY_PROBE_H
#define DISCORD_GATEWAY_PROBE_H
#include <stddef.h>
typedef void (*Dnx2LogFn)(const char *fmt, ...);
typedef struct {
    unsigned tcp_ok, tls_ok, http_ok, websocket_ok, hello_ok;
    unsigned http_status, hello_interval_ms;
    unsigned last_result, last_errno;
} Dnx2DiscordProbe;
Dnx2DiscordProbe dnx2DiscordGatewayProbe(Dnx2LogFn log);
Dnx2DiscordProbe dnx2DiscordRemoteAuthProbe(Dnx2LogFn log);
#endif
