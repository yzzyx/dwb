/*
 * Copyright (c) 2010-2011 Stefan Bolte <portix@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "session.h"

/* session_get_groups()                 return  char  ** (alloc){{{*/
static char **
session_get_groups() {
  char **groups = NULL;
  char *content = util_get_file_content(dwb.files.session);
  if (content) {
    groups = g_regex_split_simple("^g:", content, G_REGEX_MULTILINE, G_REGEX_MATCH_NOTEMPTY);
    g_free(content);
  }
  return groups;
}/*}}}*/

/* session_get_group(const char *)     return char* (alloc){{{*/
static char *
session_get_group(const char *name) {
  char *content = NULL;

  char **groups = session_get_groups();
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

/* session_load_webview(WebKitWebView *, char *, int *){{{*/
static void
session_load_webview(WebKitWebView *web, char *uri, int last) {
  if (last > 0) {
    for (int j=0; j<last; j++) {
      webkit_web_view_go_back(web);
    }
  }
  else {
    webkit_web_view_load_uri(web, uri);
  }
}/*}}}*/

/* session_list {{{*/
void
session_list() {
  char *path = util_build_path();
  dwb.files.session = g_build_filename(path, "session", NULL);
  char **content = session_get_groups();
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

/* session_restore(const char *name) {{{*/
gboolean
session_restore(const char *name) {
  char *group = session_get_group(name);
  if (!group) {
    return false;
  }
  char  **lines = g_strsplit(group, "\n", -1);
  WebKitWebView *web, *lastweb = NULL;
  WebKitWebBackForwardList *bf_list = NULL;
  int last = 1;
  char *uri = NULL;

  int length = g_strv_length(lines) - 1;
  for (int i=1; i<=length; i++) {
    char **line = g_strsplit(lines[i], " ", 4);
    if (line[0] && line[1] && line[2]) {
      int current = strtol(line[0], NULL, 10);
      if (current <= last) {
        web = WEBVIEW(view_add(NULL, false));
        bf_list = webkit_web_back_forward_list_new_with_web_view(web);
        if (lastweb) {
          session_load_webview(lastweb, uri, last);
        }
        lastweb = web;
      }
      if (bf_list != NULL) {
        WebKitWebHistoryItem *item = webkit_web_history_item_new_with_data(line[1], line[2]);
        webkit_web_back_forward_list_add_item(bf_list, item);
      }
      last = current;
      FREE(uri);
      uri = g_strdup(line[1]);
    }
    if (i == length && lastweb)
      session_load_webview(lastweb, uri, last);
    g_strfreev(line);
  }
  g_strfreev(lines);
  gtk_widget_show_all(dwb.gui.window);

  if (!dwb.state.views) 
    view_add(NULL, false);

  if (dwb.state.layout & MAXIMIZED && dwb.state.views) {
    gtk_widget_hide(dwb.gui.right);
    for (GList *l = dwb.state.views->next; l; l=l->next) {
      gtk_widget_hide(((View*)l->data)->vbox);
    }
  }
  dwb_unfocus();
  dwb_focus(dwb.state.views);
  dwb_update_layout(false);
  FREE(uri);
  return true;
}/*}}}*/

/* session_save(const char *) {{{*/
gboolean  
session_save(const char *name) {
  if (!name) {
    name = "default";
  }
  GString *buffer = g_string_new(NULL);
  g_string_append_printf(buffer, "g:%s\n", name);

  for (GList *l = g_list_first(dwb.state.views); l; l=l->next) {
    WebKitWebView *web = WEBVIEW(l);
    WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(web);
    for (int i= -webkit_web_back_forward_list_get_back_length(bf_list); i<=webkit_web_back_forward_list_get_forward_length(bf_list); i++) {
      WebKitWebHistoryItem *item = webkit_web_back_forward_list_get_nth_item(bf_list, i);
      if (item) {
        g_string_append_printf(buffer, "%d %s %s\n", 
            i, webkit_web_history_item_get_uri(item), webkit_web_history_item_get_title(item));
      }
    }
  }
  char **groups = session_get_groups();
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
  util_set_file_content(dwb.files.session, buffer->str);
  g_string_free(buffer, true);
  return true;
}/*}}}*/
