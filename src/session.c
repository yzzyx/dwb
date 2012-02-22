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
#include <string.h>
#include "dwb.h"
#include "util.h"
#include "view.h"
#include "session.h"

static char *_session_name;
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
session_get_group(const char *name, gboolean *is_marked) {
  char *content = NULL;
  gboolean mark;
  int l = strlen (name);
  const char *group;

  char **groups = session_get_groups();
  if (groups) {
    int i=1;
    while (groups[i]) {
      group = groups[i];
      mark = false;
      if (*groups[i] == '*') {
        group++;
        mark = true;
      }
      if (strlen(group) > l+1 && !strncmp(group, name, l) && group[l] == '\n') {
        content = g_strdup(groups[i]);
        *is_marked = mark;
      }
      i++;
    }
    g_strfreev(groups);
  }
  return content;
}/*}}}*/

/* session_save_file (const char *group, const char *content, gboolean * mark_group) {{{*/
static void
session_save_file(const char *groupname, const char *content, gboolean mark) {
  if (groupname == NULL || content == NULL)
    return;

  gboolean set = false;
  GString *buffer = g_string_new(NULL);
  char *group = NULL;

  char **groups = session_get_groups();
  if (groups) {
    int i=1;
    group = g_strconcat(groupname,  "\n", NULL);
    int l = strlen(group);
    while(groups[i]) {
      if (*groups[i] == '*') {
        if (!strncmp(groups[i]+1, group, l-1)) {
            g_string_append_printf(buffer, "g:%s%s\n%s", mark ? "*" : "", groupname, content);
            set = true;
        }
        else 
          g_string_append_printf(buffer, "g:%s", groups[i]+1);
       
      }
      else if (strncmp(groups[i], group, l)) {
        g_string_append_printf(buffer, "g:%s", groups[i]);
      }
      i++;
    }
  }
  if (!set) 
    g_string_append_printf(buffer, "g:%s%s\n%s", mark ? "*" : "", groupname, content);
  util_set_file_content(dwb.files.session, buffer->str);
  g_string_free(buffer, true);
  g_free(group);
  g_strfreev(groups);
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
    webkit_web_view_go_back_or_forward(WEBVIEW(gl), -last);
  }
  else {
    WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(WEBVIEW(gl));
    webkit_web_view_go_to_back_forward_item(WEBVIEW(gl), webkit_web_back_forward_list_get_nth_item(bf_list, 0));
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
  g_free(path);

  exit(EXIT_SUCCESS);
}/*}}}*/

/* session_restore(const char *name) {{{*/
gboolean
session_restore(char *name, gboolean force) {
  gboolean is_marked = false;
  gboolean ret = false;
  char *uri = NULL;
  GList *currentview = NULL, *lastview = NULL;
  WebKitWebBackForwardList *bf_list = NULL;
  int last = 1;
  char *end;
  int locked_state = 0;

  char *group = session_get_group(name, &is_marked);
  if (is_marked && !force) {
    fprintf(stderr, "Warning: Session '%s' will not be restored.\n", name);
    fprintf(stderr, "There is already a restored session open with name '%s'.\n", name);
    fputs("To force opening a saved session use -f or --force.\n", stderr);
    return false;
  }
  _session_name = name;
  if (group == NULL) {
    return false;
  }
  char *group_begin = strchr(group, '\n');
  session_save_file(name, group_begin+1, true);
  char  **lines = g_strsplit(group, "\n", -1);
  g_free(group);

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
      g_free(uri);
      uri = g_strdup(line[1]);
    }
    if (i == length && lastview)
      session_load_webview(lastview, uri, last, locked_state);
    g_strfreev(line);
  }
  g_strfreev(lines);
  ret = true;

  if (!dwb.state.views) {
    view_add(NULL, false);
    dwb_open_startpage(dwb.state.fview);
  }
  dwb_focus(dwb.state.fview);
  g_free(uri);
  return ret;
}/*}}}*/

/* session_save(const char *) {{{*/
gboolean  
session_save(const char *name, gboolean force) {
  if (!name) {
    if (_session_name) 
      name = _session_name;
    else if (force) 
      name = "default";
    else 
      return false;
  }
  GString *buffer = g_string_new(NULL);

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
  session_save_file(name, buffer->str, false);
  g_free(_session_name);
  g_string_free(buffer, true);
  return true;
}/*}}}*/
