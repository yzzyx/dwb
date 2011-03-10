#include "dwb.h"
#include "html.h"
#include "util.h"

typedef struct _HtmlTable HtmlTable;

struct _HtmlTable {
  const char *uri;
  const char *title;
  const char *state;
  void (*func)(WebKitWebView *, HtmlTable *);
};

void dwb_html_bookmarks(WebKitWebView *, HtmlTable *);
void dwb_html_history(WebKitWebView *, HtmlTable *);
void dwb_html_quickmarks(WebKitWebView *, HtmlTable *);

static HtmlTable table[] = {
  { "dwb://bookmarks",  "Bookmarks",  "aiii",   dwb_html_bookmarks },
  { "dwb://quickmarks", "Quickmarks",    "iaii",   dwb_html_quickmarks },
  { "dwb://history",    "History", "iiai",   dwb_html_history },
  { "dwb://settings",    "Settings", "iiia",   dwb_html_history },
};

void
dwb_html_load_page(WebKitWebView *wv, HtmlTable *t, char *panel) {
  char *filecontent, *content;
  char *path = dwb_util_get_data_file("info.html");
  if (path) {
    g_file_get_contents(path, &filecontent, NULL, NULL);
    content = g_strdup_printf(filecontent, t->title, t->state[0], t->state[1], t->state[2], t->state[3], panel);
    webkit_web_frame_load_alternate_string(webkit_web_view_get_main_frame(wv), content, t->uri, "about:blank");
    g_free(content);
    g_free(filecontent);
    g_free(path);
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
dwb_html_quickmarks(WebKitWebView *wv, HtmlTable *table) {
  int i=0;
  GString *panels = g_string_new(NULL);
  for (GList *gl = dwb.fc.quickmarks; gl; gl=gl->next, i++, i%=2) {
    Quickmark *q = gl->data;
    g_string_append_printf(panels, "<div class='dwb_line%d'><div class=qm>%s</div><div><a href='%s'>%s</a></div></div>\n", i, q->key, q->nav->first, q->nav->second);
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
