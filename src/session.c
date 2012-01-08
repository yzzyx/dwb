/*
 * Copyright (c) 2010-2012 Stefan Bolte <portix@gmx.net>
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

#include <stdlib.h>
#include "dwb.h"
#include "util.h"
#include "view.h"
#include "session.h"

typedef struct _SessionTab {
  GList *gl;
  unsigned int lock;
} SessionTab;

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

void
session_load_status_callback(WebKitWebView *wv, GParamSpec *p, SessionTab *tab) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(wv);
  switch (status) {
    case WEBKIT_LOAD_FINISHED:  VIEW(tab->gl)->status->lockprotect = tab->lock;
                                dwb_tab_label_set_text(tab->gl, NULL);
    case WEBKIT_LOAD_FAILED:    g_signal_handlers_disconnect_by_func(wv, (GFunc)session_load_status_callback, tab);
                                g_free(tab);
                                break;
    default: return;
  }
}


/* session_load_webview(WebKitWebView *, char *, int *){{{*/
static void
session_load_webview(GList *gl, char *uri, int last, int lock_status) {
  if (last > 0) {
    for (int j=0; j<last; j++) {
      webkit_web_view_go_back(WEBVIEW(gl));
    }
  }
  else {
    dwb_load_uri(gl, uri);
  }
  if (lock_status > 0) {
    SessionTab *tab = dwb_malloc(sizeof(SessionTab));
    tab->gl = gl;
    tab->lock = lock_status;
    g_signal_connect(WEBVIEW(gl), "notify::load-status", G_CALLBACK(session_load_status_callback), tab);
  }
}/*}}}*/

/* session_list {{{*/
void
session_list() {
  char *path = util_build_path();
  dwb.files.session = util_check_directory(g_build_filename(path, dwb.misc.profile, "session", NULL));
  char **content = session_get_groups();
  if (content == NULL) {
    fprintf(stderr, "No sessions found for profile: %s\n", dwb.misc.profile);
    exit(EXIT_SUCCESS);
  }
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
  if (group == NULL) {
    return false;
  }
  char  **lines = g_strsplit(group, "\n", -1);
  g_free(group);
  GList *currentview = NULL, *lastview = NULL;
  WebKitWebBackForwardList *bf_list = NULL;
  int last = 1;
  char *uri = NULL;
  char *end;
  int locked_state = 0;

  int length = g_strv_length(lines) - 1;
  for (int i=1; i<=length; i++) {
    char **line = g_strsplit(lines[i], " ", 4);
    if (line[0] && line[1] && line[2]) {
      int current = strtol(line[0], &end, 10);
      if (current == 0 && *end == '|') {
        locked_state = strtol(end+1, NULL, 10);
      }

      if (current <= last) {
        currentview = view_add(NULL, false);
        bf_list = webkit_web_view_get_back_forward_list(WEBVIEW(currentview));
        if (lastview) {
          session_load_webview(lastview, uri, last, locked_state);
          locked_state = 0;
        }
        lastview = currentview;
      }
      if (bf_list != NULL) {
        WebKitWebHistoryItem *item = webkit_web_history_item_new_with_data(line[1], line[2]);
        webkit_web_back_forward_list_add_item(bf_list, item);
      }
      last = current;
      FREE(uri);
      uri = g_strdup(line[1]);
    }
    if (i == length && lastview)
      session_load_webview(lastview, uri, last, locked_state);
    g_strfreev(line);
  }
  g_strfreev(lines);
  //gtk_widget_show_all(dwb.gui.window);

  if (!dwb.state.views) 
    view_add(NULL, false);

  //dwb_unfocus();
  dwb_focus(dwb.state.fview);
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
        g_string_append_printf(buffer, "%d", i);
        if (i == 0) 
          g_string_append_printf(buffer, "|%d", VIEW(l)->status->lockprotect);
        g_string_append_printf(buffer, " %s %s\n", 
            webkit_web_history_item_get_uri(item), webkit_web_history_item_get_title(item));
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
