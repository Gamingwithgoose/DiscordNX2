// SPDX-License-Identifier: MIT
#include "wifi_browser_probe.h"
#include <switch/applets/web.h>
#include <string.h>

int dnx2WifiBrowserProbe(Dnx2WifiBrowserLog log) {
    if(!log)return 0;
    log("Wi-Fi browser applet initialization begin",0);
    Result rc=appletInitialize();
    log("Wi-Fi browser applet initialization end",rc);
    if(R_FAILED(rc))return 0;
    WebWifiConfig config;
    WebWifiReturnValue reply={0};
    reply.res=~0u;
    Uuid network={0};
    // NULL connection-test override avoids setting a network profile/revision.
    // No captive DNS, connection-test spoofing, or persistent network changes.
    webWifiCreate(&config,NULL,"https://example.com/",network,0);
    log("Wi-Fi browser launch begin: Example Domain HTTPS page",0);
    rc=webWifiShow(&config,&reply);
    log("Wi-Fi browser launch API returned",rc);
    if(R_SUCCEEDED(rc))log("Wi-Fi browser applet reply status",reply.res);
    appletExit();
    memset(&config,0,sizeof(config));
    return R_SUCCEEDED(rc);
}
