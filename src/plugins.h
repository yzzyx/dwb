#ifndef PLUGINS_H
#define PLUGINS_H

GtkWidget * dwb_plugins_create_cb(WebKitWebView *wv, char *mimetype, char *uri, GHashTable *param, GList *gl);
#endif
