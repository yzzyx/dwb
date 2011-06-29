#include "dwb.h"
#include "html.h"
#include "util.h"

#define INFO_FILE "info.html"
#define SETTINGS_FILE "settings.html"
#define HEAD_FILE "head.html"
#define KEY_FILE "keys.html"

typedef struct _HtmlTable HtmlTable;

struct _HtmlTable {
  const char *uri;
  const char *title;
  const char *file;
  int type;
  void (*func)(GList *, HtmlTable *);
};

void dwb_html_bookmarks(GList *, HtmlTable *);
void dwb_html_history(GList *, HtmlTable *);
void dwb_html_quickmarks(GList *, HtmlTable *);
void dwb_html_settings(GList *, HtmlTable *);
void dwb_html_startpage(GList *, HtmlTable *);
void dwb_html_keys(GList *, HtmlTable *);

static HtmlTable table[] = {
  { "dwb://bookmarks",  "Bookmarks",    INFO_FILE,      0, dwb_html_bookmarks },
  { "dwb://quickmarks", "Quickmarks",   INFO_FILE,      0, dwb_html_quickmarks },
  { "dwb://history",    "History",      INFO_FILE,      0, dwb_html_history },
  { "dwb://keys",       "Keys",         INFO_FILE,      0, dwb_html_keys },
  { "dwb://settings",   "Settings",     INFO_FILE,      0, dwb_html_settings },
  { "dwb://startpage",   NULL,           NULL,           0, dwb_html_startpage },
};

static char current_uri[BUFFER_LENGTH];
void
dwb_html_load_page(WebKitWebView *wv, HtmlTable *t, char *panel) {
  char *filecontent;
  GString *content = g_string_new(NULL);
  char *path = dwb_util_get_data_file(t->file);
  char *headpath = dwb_util_get_data_file(HEAD_FILE);

  if (path && headpath) {
    /* load head */
    g_file_get_contents(headpath, &filecontent, NULL, NULL);
    g_string_append_printf(content, filecontent, t->title);
    g_free(filecontent);
    FREE(headpath);
    /* load content */
    g_file_get_contents(path, &filecontent, NULL, NULL);
    if (panel) 
      g_string_append_printf(content, filecontent, panel);
    webkit_web_frame_load_alternate_string(webkit_web_view_get_main_frame(wv), content->str, current_uri, current_uri);
    g_string_free(content, true);
    g_free(filecontent);
    FREE(path);
  }
}
void
dwb_html_navigation(GList *gl, GList *data, HtmlTable *table) {
  int i=0;
  GString *panels = g_string_new(NULL);
  for (GList *l = data; l; l=l->next, i++, i%=2) {
    Navigation *n = l->data;
    g_string_append_printf(panels, "<div class='dwb_line%d'><div><a href='%s'>%s</a></div></div>\n", i, n->first, n->second);
  }
  dwb_html_load_page(WEBVIEW(gl), table, panels->str);
  g_string_free(panels, true);
}
void
dwb_html_bookmarks(GList *gl, HtmlTable *table) {
  dwb.fc.bookmarks = g_list_sort(dwb.fc.bookmarks, (GCompareFunc)dwb_util_navigation_compare_second);
  dwb_html_navigation(gl, dwb.fc.bookmarks, table);
}
void
dwb_html_history(GList *gl, HtmlTable *table) {
  dwb_html_navigation(gl, dwb.fc.history, table);
}
gboolean
dwb_html_settings_changed_cb(WebKitDOMElement *el, WebKitDOMEvent *ev, WebKitWebView *wv) {
  char buffer[10];
  memset(buffer, '\0', 10);
  char *id = webkit_dom_html_element_get_id(WEBKIT_DOM_HTML_ELEMENT(el));
  char *value = 0;
  char *type = webkit_dom_element_get_attribute(el, "type");
  PRINT_DEBUG("id: %s value: %s wv: %s", id, value, type);
  if (!strcmp(type, "checkbox")) {
    if (webkit_dom_html_input_element_get_checked(WEBKIT_DOM_HTML_INPUT_ELEMENT(el)) ) 
      strcpy(buffer, "true");
    else 
      strcpy(buffer, "false");
    value = buffer;
  }
  else 
    value = webkit_dom_html_input_element_get_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(el));
  dwb.state.setting_apply = APPLY_GLOBAL;
  dwb_set_setting(id, value);
  return true;
}
void
dwb_html_settings_fill(char *key, WebSettings *s, WebKitWebView *wv) {
  char *value = dwb_util_arg_to_char(&s->arg, s->type);
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  WebKitDOMElement *e = webkit_dom_document_get_element_by_id(doc, key);
  PRINT_DEBUG("%s %s", key, value);
  if (s->type == BOOLEAN) 
    webkit_dom_html_input_element_set_checked(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), s->arg.b);
  else if (value) {
    webkit_dom_html_input_element_set_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), value);
    g_free(value);
  }
  webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(e), "change", G_CALLBACK(dwb_html_settings_changed_cb), false, wv);
}
void 
dwb_html_settings_load_cb(WebKitWebView *wv, GParamSpec *p, HtmlTable *table) {
  if (webkit_web_view_get_load_status(wv) == WEBKIT_LOAD_FINISHED) {
    g_hash_table_foreach(dwb.settings, (GHFunc)dwb_html_settings_fill, wv);
    g_signal_handlers_disconnect_by_func(wv, dwb_html_settings_load_cb, table);
  }
}
void
dwb_html_settings(GList *gl, HtmlTable *table) {
  char *content;
  WebKitWebView *wv = WEBVIEW(gl);
  g_signal_connect(wv, "notify::load-status", G_CALLBACK(dwb_html_settings_load_cb), table);
  char *path = dwb_util_get_data_file(SETTINGS_FILE);
  g_file_get_contents(path, &content, NULL, NULL);
  dwb_html_load_page(wv, table, content);
  g_free(path);
  g_free(content);
}

static gboolean
dwb_html_key_changed_cb(WebKitDOMElement *target, WebKitDOMEvent *e, gpointer data) {
  char *value = webkit_dom_html_input_element_get_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(target));
  char *id = webkit_dom_html_element_get_id(WEBKIT_DOM_HTML_ELEMENT(target));
  dwb_set_key(id, value);
  return true;
}
void 
dwb_html_keys_load_cb(WebKitWebView *wv, GParamSpec *p, HtmlTable *table) {
  KeyMap *km;
  Navigation n;
  WebKitDOMElement *input;
  char *mod, *value;

  if (webkit_web_view_get_load_status(wv) == WEBKIT_LOAD_FINISHED) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    for (GList *l = dwb.keymap; l; l=l->next) {
      km = l->data; 
      n = km->map->n;
      input = webkit_dom_document_get_element_by_id(doc, n.first);
      if (input != NULL) {
        mod = dwb_modmask_to_string(km->mod);
        value = g_strdup_printf("%s %s", mod, km->key ? km->key : "");
        webkit_dom_html_input_element_set_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(input), value);
        webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(input), "change", G_CALLBACK(dwb_html_key_changed_cb), false, wv);
        FREE(mod);
        g_free(value);
      }
    }
    g_signal_handlers_disconnect_by_func(wv, dwb_html_keys_load_cb, table);
  }
}
void
dwb_html_keys(GList *gl, HtmlTable *table) {
  char *content;
  WebKitWebView *wv = WEBVIEW(gl);
  g_signal_connect(wv, "notify::load-status", G_CALLBACK(dwb_html_keys_load_cb), table);
  char *path = dwb_util_get_data_file(KEY_FILE);
  g_file_get_contents(path, &content, NULL, NULL);
  dwb_html_load_page(wv, table, content);
  g_free(path);
  g_free(content);
}
void
dwb_html_quickmarks(GList *gl, HtmlTable *table) {
  int i=0;
  GString *panels = g_string_new(NULL);
  for (GList *gl = dwb.fc.quickmarks; gl; gl=gl->next, i++, i%=2) {
    Quickmark *q = gl->data;
    g_string_append_printf(panels, "<div class='dwb_line%d'><div class=dwb_qm>%s</div><div><a href='%s'>%s</a></div></div>\n", i, q->key, q->nav->first, q->nav->second);
  }
  dwb_html_load_page(WEBVIEW(gl), table, panels->str);
  g_string_free(panels, true);
}
void
dwb_html_startpage(GList *gl, HtmlTable *table) {
  dwb_open_startpage(gl);
}

gboolean 
dwb_html_load(GList *gl, const char *uri) {
  for (int i=0; i<LENGTH(table); i++) {
    if (!strncmp(table[i].uri, uri, strlen(table[i].uri))) {
      strncpy(current_uri, uri, BUFFER_LENGTH - 1);
      table[i].func(gl, &table[i]);
      return true;
    }
  }
  return false;
}
