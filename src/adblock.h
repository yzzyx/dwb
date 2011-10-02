#ifndef ADBLOCK_H
#define ADBLOCK_H

#include "dwb.h"

void adblock_init();
void adblock_resource_request_cb(WebKitWebView *, WebKitWebFrame *, WebKitWebResource *, 
    WebKitNetworkRequest  *, WebKitNetworkResponse *, GList *gl);
void adblock_connect(GList *gl);
#endif
