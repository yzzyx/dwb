#include "dwb.h"

static void 
dwb_plugins_get_embed_values(char *key, char *value, GString **gl) {
  g_string_append_printf(*gl, " %s=\"%s\" ", key, value);
}

GtkWidget *
dwb_plugins_create_cb(WebKitWebView *wv, char *mimetype, char *uri, GHashTable *param, GList *gl) {
  GtkWidget *ret = NULL;
  if (!strcmp(mimetype, "application/x-shockwave-flash")) {
    GString *string = g_string_new(NULL);
    g_hash_table_foreach(param, (GHFunc)dwb_plugins_get_embed_values, &string);
    char *id = g_hash_table_lookup(param, "id");
    char *command = g_strdup_printf("DwbPlugin.remove('%s', '%s')", id, string->str);
    g_string_free(string, true);
    char *value;
    dwb_execute_script(wv, command, &value);
    g_free(command);
    if (value && !strcmp(value, "__dwb__blocked__")) {
      g_free(value);
    }
    else {
      ret = gtk_event_box_new();
    }
  }
  return ret;
}
