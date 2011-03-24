#include "dwb.h"

typedef struct {
  GList *gl;
  char *id;
} Plugin;
static void 
dwb_plugins_get_embed_values(char *key, char *value, GString **gl) {
  g_string_append_printf(*gl, " %s=\"%s\" ", key, value);
}

void
show(GtkWidget *plugin, Plugin *p) {
  GtkAllocation a;
  gtk_widget_get_allocation(plugin, &a);
  char *command = g_strdup_printf("DwbPlugin.remove('%s', '%d', '%d', '%d', '%d');", p->id, a.x, a.y, a.width, a.height);
  dwb_execute_script(WEBVIEW(p->gl), command, false);
  g_free(p->id);
  g_free(p);
  gtk_widget_destroy(plugin);
}

GtkWidget *
dwb_plugins_create_cb(WebKitWebView *wv, char *mimetype, char *uri, GHashTable *param, GList *gl) {
  GtkWidget *ret = NULL;
  if (!strcmp(mimetype, "application/x-shockwave-flash")) {
    Plugin *p = malloc(sizeof(Plugin));
    p->gl = gl;

    GString *string = g_string_new(NULL);
    g_hash_table_foreach(param, (GHFunc)dwb_plugins_get_embed_values, &string);
    p->id = g_strdup(g_hash_table_lookup(param, "id"));

    char *value = NULL;
    char *command = g_strdup_printf("DwbPlugin.init('%s', '%s')", p->id, string->str);
    value = dwb_execute_script(wv, command, true);
    if (!value || strcmp(value, "__dwb__blocked__")) {
      ret = gtk_event_box_new();
      g_signal_connect_after(ret, "show", G_CALLBACK(show), p);
    }
    g_string_free(string, true);
    FREE(value);
    g_free(command);
  }
  return ret;
}
