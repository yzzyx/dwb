#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "dwb.h"
#include "util.h"
#include "view.h"

/* dwb_session_get_groups()                 return  char  ** (alloc){{{*/
static char **
dwb_session_get_groups() {
  char **groups = NULL;
  char *content = dwb_util_get_file_content(dwb.files.session);
  if (content) {
    groups = g_regex_split_simple("^g:", content, G_REGEX_MULTILINE, G_REGEX_MATCH_NOTEMPTY);
    free(content);
  }
  return groups;
}/*}}}*/

/* dwb_session_get_group(const char *)     return char* (alloc){{{*/
static char *
dwb_session_get_group(const char *name) {
  char *content = NULL;

  char **groups = dwb_session_get_groups();
  if (groups) {
    int i=1;
    char *group = g_strconcat(name, "\n", NULL);
    while (groups[i]) {
      if (g_str_has_prefix(groups[i], group)) {
        content = g_strdup(groups[i]);
      }
      i++;
    }
    g_strfreev(groups);
    FREE(group);
  }
  return content;
}/*}}}*/

/* dwb_session_load_webview(WebKitWebView *, char *, int *){{{*/
static void
dwb_session_load_webview(WebKitWebView *web, char *uri, int last) {
  if (last > 0) {
    for (int j=0; j<last; j++) {
      webkit_web_view_go_back(web);
    }
  }
  else {
    webkit_web_view_load_uri(web, uri);
  }
}/*}}}*/

/* dwb_session_list {{{*/
void
dwb_session_list() {
  char *path = dwb_util_build_path();
  dwb.files.session = g_build_filename(path, "session", NULL);
  char **content = dwb_session_get_groups();
  int i=1;
  while (content[i]) {
    char **group = g_strsplit(content[i], "\n", -1);
    fprintf(stdout, "%d: %s\n", i++, group[0]);
    g_strfreev(group);
  }
  g_strfreev(content);
  FREE(path);

  exit(EXIT_SUCCESS);
}/*}}}*/

/* dwb_session_restore(const char *name) {{{*/
gboolean
dwb_session_restore(const char *name) {
  char *group = dwb_session_get_group(name);
  if (!group) {
    return false;
  }
  char  **lines = g_strsplit(group, "\n", -1);
  WebKitWebView *web, *lastweb = NULL;
  WebKitWebBackForwardList *bf_list;
  int last = 1;
  char *uri = NULL;

  int length = g_strv_length(lines) - 1;
  for (int i=1; i<=length; i++) {
    char **line = g_strsplit(lines[i], " ", 4);
    if (line[0] && line[1] && line[2]) {
      int current = strtol(line[0], NULL, 10);
      if (current <= last) {
        dwb_add_view(NULL);
        web = CURRENT_WEBVIEW();
        bf_list = webkit_web_back_forward_list_new_with_web_view(web);
        if (lastweb) {
          dwb_session_load_webview(lastweb, uri, last);
        }
        lastweb = web;
      }
      WebKitWebHistoryItem *item = webkit_web_history_item_new_with_data(line[1], line[2]);
      webkit_web_back_forward_list_add_item(bf_list, item);
      last = current;
      FREE(uri);
      uri = g_strdup(line[1]);
    }
    if (i == length && lastweb)
      dwb_session_load_webview(lastweb, uri, last);
    g_strfreev(line);
  }
  g_strfreev(lines);
  gtk_widget_show_all(dwb.gui.window);

  if (!dwb.state.views) 
    dwb_add_view(NULL);

  if (dwb.state.layout & Maximized && dwb.state.views) {
    gtk_widget_hide(dwb.gui.right);
    for (GList *l = dwb.state.views->next; l; l=l->next) {
      gtk_widget_hide(((View*)l->data)->vbox);
    }
  }
  dwb_update_layout();
  return true;
}/*}}}*/

/* dwb_session_save(const char *) {{{*/
gboolean  
dwb_session_save(const char *name) {
  if (!name) {
    name = "default";
  }
  GString *buffer = g_string_new(NULL);
  g_string_append_printf(buffer, "g:%s\n", name);

  int view=0;
  for (GList *l = g_list_last(dwb.state.views); l; l=l->prev, view++) {
    WebKitWebView *web = WEBVIEW(l);
    WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(web);
    for (int i= -webkit_web_back_forward_list_get_back_length(bf_list); i<=webkit_web_back_forward_list_get_forward_length(bf_list); i++) {
      WebKitWebHistoryItem *item = webkit_web_back_forward_list_get_nth_item(bf_list, i);
      if (item) {
        const char *uri = webkit_web_history_item_get_uri(item);
        const char *title = webkit_web_history_item_get_title(item);
        g_string_append_printf(buffer, "%d %s %s\n", i, uri, title);
      }
    }
  }
  char **groups = dwb_session_get_groups();
  if (groups) {
    int i=1;
    char *group = g_strconcat(name,  "\n", NULL);
    while(groups[i]) {
      if (!g_str_has_prefix(groups[i], group)) {
        g_string_append_printf(buffer, "g:%s", groups[i]);
      }
      i++;
    }
    FREE(group);
    g_strfreev(groups);
  }
  dwb_util_set_file_content(dwb.files.session, buffer->str);
  g_string_free(buffer, true);
  //dwb_exit();
  return true;
}/*}}}*/
