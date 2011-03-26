#include "dwb.h"

typedef struct {
  GList *gl;
  char *id;
  GtkWidget *plugin;
} Plugin;
static void 
dwb_plugins_get_embed_values(char *key, char *value, GString **gl) {
  g_string_append_printf(*gl, " %s=\"%s\" ", key, value);
}

static gboolean
dwb_plugins_remove(Plugin *p) {
  GtkAllocation a;
  gtk_widget_get_allocation(p->plugin, &a);
  if (a.x != -1 && a.y != -1) {
    gtk_widget_destroy(p->plugin);
    char *command = g_strdup_printf("DwbPlugin.remove('%s', '%d', '%d', '%d', '%d');", p->id, a.x, a.y, a.width, a.height);
    dwb_execute_script(WEBVIEW(p->gl), command, false);
    g_free(command);
    g_free(p->id);
    g_free(p);
    return false;
  }
  return  true;
}
static void
dwb_plugins_realize_cb(GtkWidget *plugin, Plugin *p) {
  GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(VIEW(p->gl)->scroll));
  double value = gtk_adjustment_get_value(adj);
  gtk_adjustment_set_value(adj, value + 1);
  gtk_adjustment_set_value(adj, value - 1);
  g_timeout_add(20, (GSourceFunc)dwb_plugins_remove, p); 
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
      p->plugin = ret;
      g_signal_connect_after(ret, "realize", G_CALLBACK(dwb_plugins_realize_cb), p);
    }
    g_string_free(string, true);
    FREE(value);
    g_free(command);
  }
  return ret;
}
