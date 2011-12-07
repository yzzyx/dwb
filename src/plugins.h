#ifndef PLUGINS_H
#define PLUGINS_H

#include "dwb.h"

void plugins_connect(GList *);
void plugins_disconnect(GList *);
void plugins_free(Plugins *p);
Plugins * plugins_new(void);
#endif
