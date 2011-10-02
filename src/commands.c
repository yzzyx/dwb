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

#include "commands.h"

static int inline dwb_floor(double x) { 
  return x >= 0 ? (int) x : (int) x - 1;
}
static int inline modulo(int x, int y) {
  return x - dwb_floor((double)x/y)  * y;
}

/* commands.h {{{*/
/* commands_simple_command(keyMap *km) {{{*/
void 
commands_simple_command(KeyMap *km) {
  gboolean (*func)(void *, void *) = km->map->func;
  Arg *arg = &km->map->arg;
  arg->e = NULL;
  int ret;

  if (dwb.state.mode & AUTO_COMPLETE) {
    completion_clean_autocompletion();
  }

  if ((ret = func(km, arg)) == STATUS_OK) {
    if (km->map->hide == NEVER_SM) {
      dwb_set_normal_message(dwb.state.fview, false, "%s:", km->map->n.second);
    }
    else if (km->map->hide == ALWAYS_SM) {
      CLEAR_COMMAND_TEXT(dwb.state.fview);
      gtk_widget_hide(dwb.gui.entry);
    }
  }
  else if (ret == STATUS_ERROR) {
    dwb_set_error_message(dwb.state.fview, arg->e ? arg->e : km->map->error);
  }
  dwb_clean_key_buffer();
}/*}}}*/

/* commands_add_view(KeyMap *, Arg *) {{{*/
void 
commands_add_view(KeyMap *km, Arg *arg) {
  view_add(arg, false);
}/*}}}*/

/* commands_set_setting {{{*/
DwbStatus 
commands_set_setting(KeyMap *km, Arg *arg) {
  dwb.state.mode = SETTINGS_MODE;
  dwb_focus_entry();
  return STATUS_OK;
}/*}}}*/

/* commands_set_key {{{*/
DwbStatus 
commands_set_key(KeyMap *km, Arg *arg) {
  dwb.state.mode = KEY_MODE;
  dwb_focus_entry();
  return STATUS_OK;
}/*}}}*/

/* commands_focus_input {{{*/
DwbStatus
commands_focus_input(KeyMap *km, Arg *a) {
  char *value;
  DwbStatus ret = STATUS_OK;

  if ((value = dwb_execute_script(MAIN_FRAME(), "DwbHintObj.focusInput()", true)) && !strcmp(value, "_dwb_no_input_")) {
    ret = STATUS_ERROR;
  }
  FREE(value);
  
  return ret;
}/*}}}*/

/* commands_add_search_field(KeyMap *km, Arg *) {{{*/
DwbStatus
commands_add_search_field(KeyMap *km, Arg *a) {
  char *value;
  if ( (value = dwb_execute_script(MAIN_FRAME(), "DwbHintObj.addSearchEngine()", true)) ) {
    if (!strcmp(value, "_dwb_no_hints_")) {
      return STATUS_ERROR;
    }
  }
  dwb.state.mode = SEARCH_FIELD_MODE;
  dwb_set_normal_message(dwb.state.fview, false, "Enter a Keyword for marked search:");
  dwb_focus_entry();
  FREE(value);
  return STATUS_OK;

}/*}}}*/

DwbStatus 
commands_insert_mode(KeyMap *km, Arg *a) {
  return dwb_change_mode(INSERT_MODE);
}

/* commands_toggle_property {{{*/
DwbStatus 
commands_toggle_property(KeyMap *km, Arg *a) {
  char *prop = a->p;
  gboolean value;
  WebKitWebSettings *settings = webkit_web_view_get_settings(CURRENT_WEBVIEW());
  g_object_get(settings, prop, &value, NULL);
  g_object_set(settings, prop, !value, NULL);
  dwb_set_normal_message(dwb.state.fview, true, "Property %s set to %s", prop, !value ? "true" : "false");
  return STATUS_OK;
}/*}}}*/

/* commands_toggle_proxy {{{*/
DwbStatus
commands_toggle_proxy(KeyMap *km, Arg *a) {
  WebSettings *s = g_hash_table_lookup(dwb.settings, "proxy");
  s->arg.b = !s->arg.b;

  dwb_set_proxy(NULL, s);

  return STATUS_OK;
}/*}}}*/

/*commands_find {{{*/
DwbStatus  
commands_find(KeyMap *km, Arg *arg) { 
  View *v = CURRENT_VIEW();
  dwb.state.mode = FIND_MODE;
  dwb.state.forward_search = arg->b;
  if (v->status->search_string) {
    g_free(v->status->search_string);
    v->status->search_string = NULL;
  }
  dwb_focus_entry();
  return STATUS_OK;
}/*}}}*/

/*commands_resize_master {{{*/
DwbStatus  
commands_resize_master(KeyMap *km, Arg *arg) { 
  DwbStatus ret = STATUS_OK;
  int inc = dwb.state.nummod == 0 ? arg->n : dwb.state.nummod * arg->n;
  int size = dwb.state.size + inc;
  if (size > 90 || size < 10) {
    size = size > 90 ? 90 : 10;
    ret = STATUS_ERROR;
  }
  dwb_resize(size);
  return ret;
}/*}}}*/

/* commands_show_hints {{{*/
DwbStatus
commands_show_hints(KeyMap *km, Arg *arg) {
  if (dwb.state.nv == OPEN_NORMAL)
    dwb.state.nv = arg->n;
  if (dwb.state.mode != HINT_MODE) {
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
    dwb_execute_script(MAIN_FRAME(), "DwbHintObj.showHints()", false);
    dwb.state.mode = HINT_MODE;
    dwb_focus_entry();
  }
  return STATUS_OK;
}/*}}}*/

/* commands_show_keys(KeyMap *km, Arg *arg){{{*/
DwbStatus 
commands_show_keys(KeyMap *km, Arg *arg) {
  html_load(dwb.state.fview, "dwb://keys");
  return STATUS_OK;
}/*}}}*/

/* commands_show_settings(KeyMap *km, Arg *a) {{{*/
DwbStatus
commands_show_settings(KeyMap *km, Arg *arg) {
  html_load(dwb.state.fview, "dwb://settings");
  return STATUS_OK;
}/*}}}*/

/* commands_allow_cookie {{{*/
DwbStatus
commands_allow_cookie(KeyMap *km, Arg *arg) {
  if (dwb.state.last_cookies) {
    int count = 0;
    GString *buffer = g_string_new(NULL);
    for (GSList *l = dwb.state.last_cookies; l; l=l->next) {
      SoupCookie *c = l->data;
      const char *domain = soup_cookie_get_domain(c);
      if ( ! dwb.fc.cookies_allow || ! g_list_find_custom(dwb.fc.cookies_allow, domain, (GCompareFunc) strcmp) ) {
        dwb.fc.cookies_allow = g_list_append(dwb.fc.cookies_allow, g_strdup(domain));
        util_file_add(dwb.files.cookies_allow, domain, true, -1);
        g_string_append_printf(buffer, "%s ", domain);
        count++;
      }
    }
    dwb_soup_save_cookies(dwb.state.last_cookies);
    dwb.state.last_cookies = NULL;
    dwb_set_normal_message(dwb.state.fview, true, "Allowed domain%s: %s", count == 1 ? "" : "s", buffer->str);
    g_string_free(buffer, true);
    dwb_update_status_text(dwb.state.fview, NULL);
    return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_bookmark {{{*/
DwbStatus 
commands_bookmark(KeyMap *km, Arg *arg) {
  gboolean noerror = STATUS_ERROR;
  if ( (noerror = dwb_prepend_navigation(dwb.state.fview, &dwb.fc.bookmarks)) == STATUS_OK) {
    util_file_add_navigation(dwb.files.bookmarks, dwb.fc.bookmarks->data, true, -1);
    dwb.fc.bookmarks = g_list_sort(dwb.fc.bookmarks, (GCompareFunc)util_navigation_compare_first);
    dwb_set_normal_message(dwb.state.fview, true, "Saved bookmark: %s", webkit_web_view_get_uri(CURRENT_WEBVIEW()));
  }
  return noerror;
}/*}}}*/

/* commands_quickmark(KeyMap *km, Arg *arg) {{{*/
DwbStatus
commands_quickmark(KeyMap *km, Arg *arg) {
  if (dwb.state.nv == OPEN_NORMAL)
    dwb.state.nv = arg->i;
  dwb.state.mode = arg->n;
  return STATUS_OK;
}/*}}}*/

/* commands_reload(KeyMap *km, Arg *){{{*/
DwbStatus
commands_reload(KeyMap *km, Arg *arg) {
  WebKitWebView *web = WEBVIEW_FROM_ARG(arg);
  char *path;
  webkit_web_view_get_uri(web);
  if ( (path = (char *)dwb_check_directory(webkit_web_view_get_uri(web), NULL)) ) {
    Arg a = { .p = path, .b = false };
    dwb_load_uri(NULL, &a);
  }
  else {
    webkit_web_view_reload(web);
  }
  return STATUS_OK;
}/*}}}*/

/* commands_reload_bypass_cache {{{*/
DwbStatus
commands_reload_bypass_cache(KeyMap *km, Arg *arg) {
  webkit_web_view_reload_bypass_cache(WEBVIEW_FROM_ARG(arg));
  return STATUS_OK;
}
/*}}}*/

/* commands_stop_loading {{{*/
DwbStatus
commands_stop_loading(KeyMap *km, Arg *arg) {
  webkit_web_view_stop_loading(WEBVIEW_FROM_ARG(arg));
  return STATUS_OK;
}
/*}}}*/

/* commands_view_source(Arg) {{{*/
DwbStatus
commands_view_source(KeyMap *km, Arg *arg) {
  WebKitWebView *web = CURRENT_WEBVIEW();
  webkit_web_view_set_view_source_mode(web, !webkit_web_view_get_view_source_mode(web));
  webkit_web_view_reload(web);
  return STATUS_OK;
}/*}}}*/

/* commands_zoom_in(void *arg) {{{*/
DwbStatus
commands_zoom_in(KeyMap *km, Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  int limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;
  DwbStatus ret = STATUS_OK;

  for (int i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) > 4.0)) {
      ret = STATUS_ERROR;
      break;
    }
    webkit_web_view_zoom_in(web);
  }
  return ret;
}/*}}}*/

/* commands_zoom_out(void *arg) {{{*/
DwbStatus
commands_zoom_out(KeyMap *km, Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  int limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;
  DwbStatus ret = STATUS_OK;

  for (int i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) < 0.25)) {
      ret = STATUS_ERROR;
      break;
    }
    webkit_web_view_zoom_out(web);
  }
  return ret;
}/*}}}*/

/* commands_scroll {{{*/
DwbStatus 
commands_scroll(KeyMap *km, Arg *arg) {
  double scroll;
  GtkAllocation alloc;
  GList *gl = arg->p ? arg->p : dwb.state.fview;

  View *v = gl->data;

  GtkAdjustment *a = arg->n == SCROLL_LEFT || arg->n == SCROLL_RIGHT 
    ? gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(v->scroll)) 
    : gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  int sign = arg->n == SCROLL_UP || arg->n == SCROLL_PAGE_UP || arg->n == SCROLL_HALF_PAGE_UP || arg->n == SCROLL_LEFT ? -1 : 1;

  double value = gtk_adjustment_get_value(a);

  PRINT_DEBUG("gtk_adjustment value: %f", value);

  double inc;
  if (arg->n == SCROLL_PAGE_UP || arg->n == SCROLL_PAGE_DOWN) {
    inc = gtk_adjustment_get_page_increment(a);
    if (inc == 0) {
      gtk_widget_get_allocation(GTK_WIDGET(CURRENT_WEBVIEW()), &alloc);
      inc = alloc.height;
    }
  }
  else if (arg->n == SCROLL_HALF_PAGE_UP || arg->n == SCROLL_HALF_PAGE_DOWN) {
    inc = gtk_adjustment_get_page_increment(a) / 2;
    if (inc == 0) {
      gtk_widget_get_allocation(GTK_WIDGET(CURRENT_WEBVIEW()), &alloc);
      inc = alloc.height / 2;
    }
  }
  else
    inc = dwb.misc.scroll_step > 0 ? dwb.misc.scroll_step : gtk_adjustment_get_step_increment(a);

  PRINT_DEBUG("scroll increment %f", inc);
  /* if gtk_get_step_increment fails and dwb.misc.scroll_step is 0 use a default
   * value */
  if (inc == 0) {
    inc = 40;
  }

  double lower  = gtk_adjustment_get_lower(a);
  double upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;

  PRINT_DEBUG("Scroll lower %f", lower);
  PRINT_DEBUG("Scroll upper %f", upper);

  switch (arg->n) {
    case  SCROLL_TOP:      scroll = lower; break;
    case  SCROLL_BOTTOM:   scroll = upper; break;
    case  SCROLL_PERCENT:  scroll = upper * dwb.state.nummod / 100; break;
    default:        scroll = value + sign * inc * NN(dwb.state.nummod); break;
  }

  scroll = scroll < lower ? lower : scroll > upper ? upper : scroll;
  if (scroll == value) {

    /* Scroll also if  frame-flattening is enabled 
     * this is just a workaround since scrolling is disfunctional if 
     * enable-frame-flattening is set */
    if (value == 0 && arg->n != SCROLL_TOP) {
      int x, y;
      if (arg->n == SCROLL_LEFT || arg->n == SCROLL_RIGHT) {
        x = sign * inc;
        y = 0;
      }
      else {
        x = 0; 
        y = sign * inc;
      }
      char *command = g_strdup_printf("window.scrollBy(%d, %d)", x, y);
      dwb_execute_script(FOCUSED_FRAME(), command, false);
      g_free(command);
    }
  }
  else {
    gtk_adjustment_set_value(a, scroll);
  }
  return STATUS_OK;
}/*}}}*/

/* commands_set_zoom_level(KeyMap *km, Arg *arg) {{{*/
void 
commands_set_zoom_level(KeyMap *km, Arg *arg) {
  GList *gl = arg->p ? arg->p : dwb.state.fview;
  webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(((View*)gl->data)->web), arg->d);
}/*}}}*/

/* commands_set_orientation(KeyMap *km, Arg *arg) {{{*/
DwbStatus 
commands_set_orientation(KeyMap *km, Arg *arg) {
  Layout l;
  if (arg->n) {
    l = arg->n;
  }
  else {
    dwb.state.layout ^= BOTTOM_STACK;
    l = dwb.state.layout;
  }
  gtk_orientable_set_orientation(GTK_ORIENTABLE(dwb.gui.paned), l & BOTTOM_STACK );
  gtk_orientable_set_orientation(GTK_ORIENTABLE(dwb.gui.right), (l & BOTTOM_STACK) ^ 1);
  dwb_resize(dwb.state.size);
  return STATUS_OK;
}/*}}}*/

/* History {{{*/
DwbStatus 
commands_history_back(KeyMap *km, Arg *arg) {
  return dwb_history_back();
}
DwbStatus 
commands_history_forward(KeyMap *km, Arg *arg) {
  return dwb_history_forward();
}/*}}}*/

/* commands_open(KeyMap *km, Arg *arg) {{{*/
DwbStatus  
commands_open(KeyMap *km, Arg *arg) {
  if (dwb.state.nv & OPEN_NORMAL)
    dwb.state.nv = arg->n & ~SET_URL;

  dwb.state.type = arg->i;

  if (arg && arg->p) {
    dwb_load_uri(NULL, arg);
  }
  else {
    dwb_focus_entry();
    if (arg->n & SET_URL)
      dwb_entry_set_text(CURRENT_URL());
  }
  return STATUS_OK;
} /*}}}*/

/* commands_open(KeyMap *km, Arg *arg) {{{*/
DwbStatus  
commands_open_startpage(KeyMap *km, Arg *arg) {
  return dwb_open_startpage(NULL);
} /*}}}*/

/* commands_toggle_maximized {{{*/
void 
commands_maximized_hide(View *v, View *no) {
  if (dwb.misc.factor != 1.0) {
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), 1.0);
  }
  if (v != dwb.state.fview->data) {
    gtk_widget_hide(v->vbox);
  }
}
void 
commands_maximized_show(View *v) {
  if (dwb.misc.factor != 1.0 && v != dwb.state.views->data) {
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), dwb.misc.factor);
  }
  gtk_widget_show(v->vbox);
}

void 
commands_toggle_maximized(KeyMap *km, Arg *arg) {
  dwb.state.layout ^= MAXIMIZED;
  if (dwb.state.layout & MAXIMIZED) {
    g_list_foreach(dwb.state.views,  (GFunc)commands_maximized_hide, NULL);
    if  (dwb.state.views == dwb.state.fview) {
      gtk_widget_hide(dwb.gui.right);
    }
    else if (dwb.state.views->next) {
      gtk_widget_hide(dwb.gui.left);
    }
  }
  else {
    if (dwb.state.views->next) {
      gtk_widget_show(dwb.gui.right);
    }
    gtk_widget_show(dwb.gui.left);
    g_list_foreach(dwb.state.views,  (GFunc)commands_maximized_show, NULL);
  }
  dwb_resize(dwb.state.size);
  dwb_toggle_tabbar();
}/*}}}*/

/* commands_remove_view(KeyMap *km, Arg *arg) {{{*/
void 
commands_remove_view(KeyMap *km, Arg *arg) {
  view_remove(NULL);
}/*}}}*/

/* commands_push_master {{{*/
DwbStatus 
commands_push_master(KeyMap *km, Arg *arg) {
  return view_push_master(arg);
}/*}}}*/

/* commands_focus(KeyMap *km, Arg *arg) {{{*/
DwbStatus 
commands_focus(KeyMap *km, Arg *arg) {
  if (dwb.state.views->next) {
    int pos = modulo(g_list_position(dwb.state.views, dwb.state.fview) + NUMMOD * arg->n, g_list_length(dwb.state.views));
    GList *g = g_list_nth(dwb.state.views, pos);
    dwb_focus_view(g);
    return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_focus_view{{{*/
DwbStatus
commands_focus_nth_view(KeyMap *km, Arg *arg) {
  if (!dwb.state.views->next) 
    return STATUS_ERROR;
  GList *l = g_list_nth(dwb.state.views, dwb.state.nummod);
  if (!l) 
    return STATUS_ERROR;
  dwb_focus_view(l);
  return STATUS_OK;
}/*}}}*/

/* commands_yank {{{*/
DwbStatus
commands_yank(KeyMap *km, Arg *arg) {
  GdkAtom atom = GDK_POINTER_TO_ATOM(arg->p);
  GtkClipboard *clipboard = gtk_clipboard_get(atom);
  gboolean ret = STATUS_ERROR;
  const char *uri = webkit_web_view_get_uri(CURRENT_WEBVIEW());

  gtk_clipboard_set_text(clipboard, uri, -1);
  if (*uri) {
    dwb_set_normal_message(dwb.state.fview, true, "Yanked: %s", uri);
    ret = STATUS_OK;
  }
  return ret;
}/*}}}*/

/* commands_paste {{{*/
DwbStatus
commands_paste(KeyMap *km, Arg *arg) {
  GdkAtom atom = GDK_POINTER_TO_ATOM(arg->p);
  GtkClipboard *clipboard = gtk_clipboard_get(atom);
  char *text = NULL;

  if ( (text = gtk_clipboard_wait_for_text(clipboard)) ) {
    if (dwb.state.nv == OPEN_NORMAL)
      dwb.state.nv = arg->n;
    Arg a = { .p = text, .b = false };
    dwb_load_uri(NULL, &a);
    g_free(text);
    return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_entry_delete_word {{{*/
DwbStatus 
commands_entry_delete_word(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));
  char *text = g_strdup(GET_TEXT());

  if (position > 0) {
    int new = dwb_entry_position_word_back(position);
    util_cut_text(text, new,  position);
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
    gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), new);
    FREE(text);
  }
  return STATUS_OK;
}/*}}}*/

/* commands_entry_delete_letter {{{*/
DwbStatus 
commands_entry_delete_letter(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));
  char *text = g_strdup(GET_TEXT());

  if (position > 0) {
    util_cut_text(text, position-1,  position);
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
    gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), position-1);
    FREE(text);
  }
  return STATUS_OK;
}/*}}}*/

/* commands_entry_delete_line {{{*/
DwbStatus 
commands_entry_delete_line(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));
  char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), &text[position]);
  FREE(text);
  return STATUS_OK;
}/*}}}*/

/* commands_entry_word_forward {{{*/
DwbStatus 
commands_entry_word_forward(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));

  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), dwb_entry_position_word_forward(position));
  return STATUS_OK;
}/*}}}*/

/* commands_entry_word_back {{{*/
DwbStatus 
commands_entry_word_back(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));

  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), dwb_entry_position_word_back(position));
  return STATUS_OK;
}/*}}}*/

/* commands_entry_history_forward {{{*/
DwbStatus 
commands_entry_history_forward(KeyMap *km, Arg *a) {
  Navigation *n = NULL;
  GList *l;
  if ( (l = g_list_last(dwb.state.last_com_history)) && dwb.state.last_com_history->prev ) {
      n = dwb.state.last_com_history->prev->data;
      dwb.state.last_com_history = dwb.state.last_com_history->prev;
  }
  if (n) {
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), n->first);
    gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);
  }
  return STATUS_OK;
}/*}}}*/

/* commands_entry_history_back {{{*/
DwbStatus 
commands_entry_history_back(KeyMap *km, Arg *a) {
  Navigation *n = NULL;

  if (!dwb.fc.commands)
    return STATUS_ERROR;

  if (! dwb.state.last_com_history  ) {
    dwb.state.last_com_history = dwb.fc.commands;
    n = dwb.state.last_com_history->data;
    char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);
    dwb_prepend_navigation_with_argument(&dwb.fc.commands, text, NULL);
    FREE(text);
  }
  else if ( dwb.state.last_com_history && dwb.state.last_com_history->next ) {
    n = dwb.state.last_com_history->next->data;
    dwb.state.last_com_history = dwb.state.last_com_history->next;
  }
  if (n) {
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), n->first);
    gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);
  }
  return STATUS_OK;
}/*}}}*/

/* commands_save_session {{{*/
DwbStatus
commands_save_session(KeyMap *km, Arg *arg) {
  if (arg->n == NORMAL_MODE) {
    dwb.state.mode = SAVE_SESSION;
    session_save(NULL);
    dwb_end();
    return STATUS_END;
  }
  else {
    dwb.state.mode = arg->n;
    dwb_focus_entry();
    dwb_set_normal_message(dwb.state.fview, false, "Session name:");
  }
  return STATUS_OK;
}/*}}}*/

/* commands_bookmarks {{{*/
DwbStatus
commands_bookmarks(KeyMap *km, Arg *arg) {
  if (!g_list_length(dwb.fc.bookmarks)) {
    return STATUS_ERROR;
  }
  if (dwb.state.nv == OPEN_NORMAL)
    dwb.state.nv = arg->n;
  dwb_focus_entry();
  completion_complete(COMP_BOOKMARK, 0);

  return STATUS_OK;
}/*}}}*/

/* commands_history{{{*/
DwbStatus  
commands_complete_type(KeyMap *km, Arg *arg) {
  if (!g_list_length(dwb.fc.history)) {
    return STATUS_ERROR;
  }
  completion_complete(arg->n, 0);

  return STATUS_OK;

}/*}}}*/

void
commands_toggle(Arg *arg, const char *filename, GList **tmp, const char *message) {
  char *host = NULL;
  const char *block = NULL;
  gboolean allowed;
  if (arg->n & ALLOW_HOST) {
    host = dwb_get_host(CURRENT_WEBVIEW());
    block = host;
  }
  else if (arg->n & ALLOW_URI) {
    block = webkit_web_view_get_uri(CURRENT_WEBVIEW());
  }
  else if (arg->p) {
    block = arg->p;
  }
  if (block != NULL) {
    if (arg->n & ALLOW_TMP) {
      GList *l;
      if ( (l = g_list_find_custom(*tmp, block, (GCompareFunc)strcmp)) ) {
        free(l->data);
        *tmp = g_list_delete_link(*tmp, l);
        allowed = false;
      }
      else {
        *tmp = g_list_prepend(*tmp, g_strdup(block));
        allowed = true;
      }
      dwb_set_normal_message(dwb.state.fview, true, "%s temporarily %s for %s", message, allowed ? "allowed" : "blocked", block);
    }
    else {
      allowed = dwb_toggle_allowed(filename, block);
      dwb_set_normal_message(dwb.state.fview, true, "%s %s for %s", message, allowed ? "allowed" : "blocked", block);
    }
  }
  else 
    CLEAR_COMMAND_TEXT(dwb.state.fview);
}

DwbStatus 
commands_toggle_plugin_blocker(KeyMap *km, Arg *arg) {
  commands_toggle(arg, dwb.files.plugins_allow, &dwb.fc.tmp_plugins, "Plugins");
  return STATUS_OK;
}

/* commands_toggle_scripts {{{ */
DwbStatus 
commands_toggle_scripts(KeyMap *km, Arg *arg) {
  commands_toggle(arg, dwb.files.scripts_allow, &dwb.fc.tmp_scripts, "Scripts");
  return STATUS_OK;
}/*}}}*/

/* commands_new_window_or_view{{{*/
DwbStatus 
commands_new_window_or_view(KeyMap *km, Arg *arg) {
  dwb.state.nv = arg->n;
  return STATUS_OK;
}/*}}}*/

/* commands_save_files(KeyMap *km, Arg *arg) {{{*/
DwbStatus 
commands_save_files(KeyMap *km, Arg *arg) {
  if (dwb_save_files(false)) {
    dwb_set_normal_message(dwb.state.fview, true, "Configuration files saved");
    return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_undo() {{{*/
DwbStatus
commands_undo(KeyMap *km, Arg *arg) {
  if (dwb.state.undo_list) {
    WebKitWebView *web = WEBVIEW(view_add(NULL, false));
    WebKitWebBackForwardList *bflist = webkit_web_back_forward_list_new_with_web_view(web);
    for (GList *l = dwb.state.undo_list->data; l; l=l->next) {
      Navigation *n = l->data;
      WebKitWebHistoryItem *item = webkit_web_history_item_new_with_data(n->first, n->second);
      webkit_web_back_forward_list_add_item(bflist, item);
      if (!l->next) {
        webkit_web_view_go_to_back_forward_item(web, item);
      }
      dwb_navigation_free(n);
    }
    g_list_free(dwb.state.undo_list->data);
    dwb.state.undo_list = g_list_delete_link(dwb.state.undo_list, dwb.state.undo_list);
    return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_print (Arg *) {{{*/
DwbStatus
commands_print(KeyMap *km, Arg *arg) {
  WebKitWebFrame *frame = webkit_web_view_get_focused_frame(CURRENT_WEBVIEW());

  if (frame) {
    webkit_web_frame_print(frame);
    return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_web_inspector {{{*/
DwbStatus
commands_web_inspector(KeyMap *km, Arg *arg) {
  if (GET_BOOL("enable-developer-extras")) {
    webkit_web_inspector_show(webkit_web_view_get_inspector(CURRENT_WEBVIEW()));
    return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_execute_userscript (Arg *) {{{*/
DwbStatus
commands_execute_userscript(KeyMap *km, Arg *arg) {
  if (!dwb.misc.userscripts) 
    return STATUS_ERROR;

  if (arg->p) {
    char *path = g_build_filename(dwb.files.userscripts, arg->p, NULL);
    Arg a = { .arg = path };
    dwb_execute_user_script(km, &a);
    g_free(path);
  }
  else {
    dwb_focus_entry();
    completion_complete(COMP_USERSCRIPT, 0);
  }

  return STATUS_OK;
}/*}}}*/

/* commands_toggle_hidden_files {{{*/
DwbStatus
commands_toggle_hidden_files(KeyMap *km, Arg *arg) {
  dwb.state.hidden_files = !dwb.state.hidden_files;
  commands_reload(km, arg);
  return STATUS_OK;
}/*}}}*/

/* commands_quit {{{*/
DwbStatus
commands_quit(KeyMap *km, Arg *arg) {
  dwb_end();
  return STATUS_END;
}/*}}}*/

/* commands_reload_scripts {{{*/
DwbStatus
commands_reload_scripts(KeyMap *km, Arg *arg) {
  dwb_init_scripts();
  for (GList *l = dwb.state.views; l; l=l->next) {
    webkit_web_view_reload(WEBVIEW(l));
  }
  return STATUS_OK;
}/*}}}*/

/* commands_reload_scripts {{{*/
DwbStatus
commands_fullscreen(KeyMap *km, Arg *arg) {
  dwb.state.fullscreen = !dwb.state.fullscreen;
  if (dwb.state.fullscreen) 
    gtk_window_fullscreen(GTK_WINDOW(dwb.gui.window));
  else 
    gtk_window_unfullscreen(GTK_WINDOW(dwb.gui.window));
  return STATUS_OK;
}/*}}}*/
/* commands_reload_scripts {{{*/
DwbStatus
commands_pass_through(KeyMap *km, Arg *arg) {
  return dwb_change_mode(PASS_THROUGH);
}/*}}}*/
/* commands_reload_scripts {{{*/
DwbStatus
commands_open_editor(KeyMap *km, Arg *arg) {
  return dwb_open_in_editor();
}/*}}}*/

/* dwb_command_mode {{{*/
DwbStatus
commands_command_mode(KeyMap *km, Arg *arg) {
  return dwb_change_mode(COMMAND_MODE);
}/*}}}*/
/*}}}*/
