// SPDX-License-Identifier: MIT
#ifndef DNX2_WIFI_BROWSER_PROBE_H
#define DNX2_WIFI_BROWSER_PROBE_H
#include <switch.h>
typedef void (*Dnx2WifiBrowserLog)(const char *stage,Result result);
/* Opens a fixed public test page. Never changes DNS or handles credentials.
 * Return 1 means the applet API returned successfully, NOT that a page opened. */
int dnx2WifiBrowserProbe(Dnx2WifiBrowserLog log);
#endif
