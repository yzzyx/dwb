#include "dwb.h"
#include "html.h"
#include "util.h"

#define INFO_FILE "info.html"
#define SETTINGS_FILE "settings.html"
#define HEAD_FILE "head.html"

typedef struct _HtmlTable HtmlTable;

struct _HtmlTable {
  const char *uri;
  const char *title;
  const char *file;
  int type;
  void (*func)(WebKitWebView *, HtmlTable *);
};

void dwb_html_bookmarks(WebKitWebView *, HtmlTable *);
void dwb_html_history(WebKitWebView *, HtmlTable *);
void dwb_html_quickmarks(WebKitWebView *, HtmlTable *);
void dwb_html_settings(WebKitWebView *, HtmlTable *);

static HtmlTable table[] = {
  { "dwb://bookmarks",  "Bookmarks",    INFO_FILE,      0, dwb_html_bookmarks },
  { "dwb://quickmarks", "Quickmarks",   INFO_FILE,      0, dwb_html_quickmarks },
  { "dwb://history",    "History",      INFO_FILE,      0, dwb_html_history },
  //{ "dwb://settings",   "Settings",     SETTINGS_FILE,  0, dwb_html_settings },
};

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
    g_string_append_printf(content, filecontent, panel);
    webkit_web_frame_load_alternate_string(webkit_web_view_get_main_frame(wv), content->str, t->uri, "about:blank");
    g_string_free(content, true);
    g_free(filecontent);
    FREE(path);
  }
}
void
dwb_html_navigation(WebKitWebView *wv, GList *gl, HtmlTable *table) {
  int i=0;
  GString *panels = g_string_new(NULL);
  for (GList *l = gl; l; l=l->next, i++, i%=2) {
    Navigation *n = l->data;
    g_string_append_printf(panels, "<div class='dwb_line%d'><div><a href='%s'>%s</a></div></div>\n", i, n->first, n->second);
  }
  dwb_html_load_page(wv, table, panels->str);
  g_string_free(panels, true);
}
void
dwb_html_bookmarks(WebKitWebView *wv, HtmlTable *table) {
  dwb.fc.bookmarks = g_list_sort(dwb.fc.bookmarks, (GCompareFunc)dwb_util_navigation_compare_second);
  dwb_html_navigation(wv, dwb.fc.bookmarks, table);
}
void
dwb_html_history(WebKitWebView *wv, HtmlTable *table) {
  dwb_html_navigation(wv, dwb.fc.history, table);
}
void
dwb_html_settings(WebKitWebView *wv, HtmlTable *table) {
  dwb_html_load_page(wv, table, "blub");
}
void
dwb_html_quickmarks(WebKitWebView *wv, HtmlTable *table) {
  int i=0;
  GString *panels = g_string_new(NULL);
  for (GList *gl = dwb.fc.quickmarks; gl; gl=gl->next, i++, i%=2) {
    Quickmark *q = gl->data;
    g_string_append_printf(panels, "<div class='dwb_line%d'><div class=dwb_qm>%s</div><div><a href='%s'>%s</a></div></div>\n", i, q->key, q->nav->first, q->nav->second);
  }
  dwb_html_load_page(wv, table, panels->str);
  g_string_free(panels, true);
}

gboolean 
dwb_html_load(WebKitWebView *wv, const char *uri) {
  for (int i=0; i<LENGTH(table); i++) {
    if (!strcmp(table[i].uri, uri)) {
      table[i].func(wv, &table[i]);
      return true;
    }
  }
  return false;
}
