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

/* dwb_com_simple_command(keyMap *km) {{{*/
void 
dwb_com_simple_command(KeyMap *km) {
  gboolean (*func)(void *, void *) = km->map->func;
  Arg *arg = &km->map->arg;
  arg->e = NULL;

  if (dwb.state.mode & AUTO_COMPLETE) {
    dwb_comp_clean_autocompletion();
  }

  if (func(km, arg)) {
    if (!km->map->hide) {
      dwb_set_normal_message(dwb.state.fview, false, "%s:", km->map->n.second);
    }
    else if (km->map->hide == ALWAYS_SM) {
      CLEAR_COMMAND_TEXT(dwb.state.fview);
      gtk_widget_hide(dwb.gui.entry);
    }
  }
  else {
    dwb_set_error_message(dwb.state.fview, arg->e ? arg->e : km->map->error);
  }
  dwb.state.nummod = 0;
  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
    dwb.state.buffer = NULL;
  }
}/*}}}*/

/* COMMANDS {{{*/
/* dwb_com_add_view(KeyMap *, Arg *) {{{*/
void 
dwb_com_add_view(KeyMap *km, Arg *arg) {
  dwb_add_view(arg);
}/*}}}*/

/* dwb_com_set_setting {{{*/
gboolean 
dwb_com_set_setting(KeyMap *km, Arg *arg) {
  dwb.state.mode = SETTINGS_MODE;
  dwb.state.setting_apply = arg->n;
  dwb_focus_entry();
  return true;
}/*}}}*/

/* dwb_com_set_key {{{*/
gboolean 
dwb_com_set_key(KeyMap *km, Arg *arg) {
  dwb.state.mode = KEY_MODE;
  dwb_focus_entry();
  return true;
}/*}}}*/

/* dwb_com_focus_input {{{*/
gboolean
dwb_com_focus_input(KeyMap *km, Arg *a) {
  char *value;
  gboolean ret = true;

  value = dwb_execute_script("dwb_focus_input()", true);
  if (value && !strcmp(value, "_dwb_no_input_")) {
    ret = false;
  }
  FREE(value);
  
  return ret;
}/*}}}*/

/* dwb_com_add_search_field(KeyMap *km, Arg *) {{{*/
gboolean
dwb_com_add_search_field(KeyMap *km, Arg *a) {
  char *value;
  gboolean ret = true;
  value = dwb_execute_script("dwb_add_searchengine()", true);
  if (value) {
    if (!strcmp(value, "_dwb_no_hints_")) {
      return false;
    }
  }
  dwb.state.mode = SEARCH_FIELD_MODE;
  dwb_set_normal_message(dwb.state.fview, false, "Enter a Keyword for marked search:");
  dwb_focus_entry();
  FREE(value);
  return ret;

}/*}}}*/

/* dwb_com_toggle_property {{{*/
gboolean 
dwb_com_toggle_property(KeyMap *km, Arg *a) {
  char *prop = a->p;
  gboolean value;
  WebKitWebSettings *settings = webkit_web_view_get_settings(CURRENT_WEBVIEW());
  g_object_get(settings, prop, &value, NULL);
  g_object_set(settings, prop, !value, NULL);
  dwb_set_normal_message(dwb.state.fview, true, "Property %s set to %s", prop, !value ? "true" : "false");
  return true;
}/*}}}*/

/* dwb_com_toggle_proxy {{{*/
gboolean
dwb_com_toggle_proxy(KeyMap *km, Arg *a) {
  WebSettings *s = g_hash_table_lookup(dwb.settings, "proxy");
  s->arg.b = !s->arg.b;

  dwb_set_proxy(NULL, s);

  return true;
}/*}}}*/

/*dwb_com_find {{{*/
gboolean  
dwb_com_find(KeyMap *km, Arg *arg) { 
  View *v = CURRENT_VIEW();
  dwb.state.mode = FIND_MODE;
  dwb.state.forward_search = arg->b;
  if (v->status->search_string) {
    g_free(v->status->search_string);
    v->status->search_string = NULL;
  }
  dwb_focus_entry();
  return true;
}/*}}}*/

/*dwb_com_resize_master {{{*/
gboolean  
dwb_com_resize_master(KeyMap *km, Arg *arg) { 
  gboolean ret = true;
  int inc = dwb.state.nummod == 0 ? arg->n : dwb.state.nummod * arg->n;
  int size = dwb.state.size + inc;
  if (size > 90 || size < 10) {
    size = size > 90 ? 90 : 10;
    ret = false;
  }
  dwb_resize(size);
  return ret;
}/*}}}*/

/* dwb_com_show_hints {{{*/
gboolean
dwb_com_show_hints(KeyMap *km, Arg *arg) {
  if (dwb.state.nv == OPEN_NORMAL)
    dwb.state.nv = arg->n;
  if (dwb.state.mode != HINT_MODE) {
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
    webkit_web_view_execute_script(CURRENT_WEBVIEW(), "dwb_show_hints()");
    dwb.state.mode = HINT_MODE;
    dwb_focus_entry();
  }
  return true;
}/*}}}*/

/* dwb_com_show_keys(KeyMap *km, Arg *arg){{{*/
gboolean 
dwb_com_show_keys(KeyMap *km, Arg *arg) {
  View *v = dwb.state.fview->data;
  GString *buffer = g_string_new(NULL);
  g_string_append_printf(buffer, SETTINGS_VIEW, dwb.color.settings_bg_color, dwb.color.settings_fg_color, dwb.misc.settings_border);
  g_string_append_printf(buffer, HTML_H2, "Keyboard Configuration", dwb.misc.profile);

  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)dwb_util_keymap_sort_first);
  g_string_append(buffer, HTML_BODY_START);
  g_string_append(buffer, HTML_FORM_START);
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    Navigation n = m->map->n;
    g_string_append(buffer, HTML_DIV_START);
    g_string_append_printf(buffer, HTML_DIV_KEYS_TEXT, n.first);
    g_string_append_printf(buffer, HTML_DIV_KEYS_VALUE, n.first, dwb_modmask_to_string(m->mod), m->key ? m->key : "");
    g_string_append(buffer, HTML_DIV_END);
  }
  g_string_append(buffer, HTML_FORM_END);
  g_string_append(buffer, HTML_BODY_END);
  dwb_web_view_add_history_item(dwb.state.fview);

  webkit_web_view_load_string(WEBKIT_WEB_VIEW(v->web), buffer->str, "text/html", NULL, KEY_SETTINGS);
  g_string_free(buffer, true);
  return true;
}/*}}}*/

/* dwb_com_show_settings(KeyMap *km, Arg *a) {{{*/
gboolean
dwb_com_show_settings(KeyMap *km, Arg *arg) {
  View *v = dwb.state.fview->data;
  GString *buffer = g_string_new(NULL);
  GHashTable *t;
  const char *setting_string;

  dwb.state.setting_apply = arg->n;
  if ( dwb.state.setting_apply == APPLY_GLOBAL ) {
    t = dwb.settings;
    setting_string = "Global Settings";
  }
  else {
    t = v->setting;
    setting_string = "Settings";
  }

  GList *l = g_hash_table_get_values(t);
  l = g_list_sort(l, (GCompareFunc)dwb_util_web_settings_sort_first);

  g_string_append_printf(buffer, SETTINGS_VIEW, dwb.color.settings_bg_color, dwb.color.settings_fg_color, dwb.misc.settings_border);
  g_string_append_printf(buffer, HTML_H2, setting_string, dwb.misc.profile);

  g_string_append(buffer, HTML_BODY_START);
  g_string_append(buffer, HTML_FORM_START);
  for (; l; l=l->next) {
    WebSettings *m = l->data;
    if (!m->global || (m->global && dwb.state.setting_apply == APPLY_GLOBAL)) {
      g_string_append(buffer, HTML_DIV_START);
      g_string_append_printf(buffer, HTML_DIV_KEYS_TEXT, m->n.first);
      if (m->type == BOOLEAN) {
        const char *value = m->arg.b ? "checked" : "";
        g_string_append_printf(buffer, HTML_DIV_SETTINGS_CHECKBOX, m->n.first, value);
      }
      else {
        char *value = dwb_util_arg_to_char(&m->arg, m->type);
        g_string_append_printf(buffer, HTML_DIV_SETTINGS_VALUE, m->n.first, value ? value : "");
      }
      g_string_append(buffer, HTML_DIV_END);
    }
  }
  g_list_free(l);
  g_string_append(buffer, HTML_FORM_END);
  g_string_append(buffer, HTML_BODY_END);
  dwb_web_view_add_history_item(dwb.state.fview);

  webkit_web_view_load_string(WEBKIT_WEB_VIEW(v->web), buffer->str, "text/html", NULL, SETTINGS);
  g_string_free(buffer, true);
  return true;
}/*}}}*/

/* dwb_com_allow_cookie {{{*/
gboolean
dwb_com_allow_cookie(KeyMap *km, Arg *arg) {
  if (dwb.state.last_cookies) {
    int count = 0;
    GString *buffer = g_string_new(NULL);
    for (GSList *l = dwb.state.last_cookies; l; l=l->next) {
      SoupCookie *c = l->data;
      const char *domain = soup_cookie_get_domain(c);
      if ( ! dwb.fc.cookies_allow || ! g_list_find_custom(dwb.fc.cookies_allow, domain, (GCompareFunc) strcmp) ) {
        dwb.fc.cookies_allow = g_list_append(dwb.fc.cookies_allow, g_strdup(domain));
        g_string_append_printf(buffer, "%s ", domain);
        count++;
      }
    }
    dwb_soup_save_cookies(dwb.state.last_cookies);
    dwb.state.last_cookies = NULL;
    dwb_set_normal_message(dwb.state.fview, true, "Allowed domain%s: %s", count == 1 ? "" : "s", buffer->str);
    g_string_free(buffer, true);
    dwb_update_status_text(dwb.state.fview, NULL);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_com_bookmark {{{*/
gboolean 
dwb_com_bookmark(KeyMap *km, Arg *arg) {
  gboolean noerror;
  if ( (noerror = dwb_prepend_navigation(dwb.state.fview, &dwb.fc.bookmarks)) ) {
    dwb.fc.bookmarks = g_list_sort(dwb.fc.bookmarks, (GCompareFunc)dwb_util_navigation_sort_first);
    dwb_set_normal_message(dwb.state.fview, true, "Saved bookmark: %s", webkit_web_view_get_uri(CURRENT_WEBVIEW()));
  }
    
  return dwb_prepend_navigation(dwb.state.fview, &dwb.fc.bookmarks);
}/*}}}*/

/* dwb_com_quickmark(KeyMap *km, Arg *arg) {{{*/
gboolean
dwb_com_quickmark(KeyMap *km, Arg *arg) {
  if (dwb.state.nv == OPEN_NORMAL)
    dwb.state.nv = arg->i;
  dwb.state.mode = arg->n;
  return true;
}/*}}}*/

/* dwb_com_reload(KeyMap *km, Arg *){{{*/
gboolean
dwb_com_reload(KeyMap *km, Arg *arg) {
  WebKitWebView *web = WEBVIEW_FROM_ARG(arg);
  webkit_web_view_reload(web);
  return true;
}/*}}}*/

/* dwb_com_view_source(Arg) {{{*/
gboolean
dwb_com_view_source(KeyMap *km, Arg *arg) {
  //WebKitWebView *web = WEBVIEW_FROM_ARG(arg);
  WebKitWebView *web = CURRENT_WEBVIEW();
  webkit_web_view_set_view_source_mode(web, !webkit_web_view_get_view_source_mode(web));
  webkit_web_view_reload(web);
  return true;
}/*}}}*/

/* dwb_com_zoom_in(void *arg) {{{*/
gboolean
dwb_com_zoom_in(KeyMap *km, Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  int limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;
  gboolean ret;

  for (int i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) > 4.0)) {
      ret = false;
      break;
    }
    webkit_web_view_zoom_in(web);
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_com_zoom_out(void *arg) {{{*/
gboolean
dwb_com_zoom_out(KeyMap *km, Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  int limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;
  gboolean ret;

  for (int i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) < 0.25)) {
      ret = false;
      break;
    }
    webkit_web_view_zoom_out(web);
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_com_scroll {{{*/
gboolean 
dwb_com_scroll(KeyMap *km, Arg *arg) {
  gboolean ret = true;
  double scroll;
  GList *gl = arg->p ? arg->p : dwb.state.fview;

  View *v = gl->data;

  GtkAdjustment *a = arg->n == SCROLL_LEFT || arg->n == SCROLL_RIGHT 
    ? gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(v->scroll)) 
    : gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  int sign = arg->n == SCROLL_UP || arg->n == SCROLL_PAGE_UP || arg->n == SCROLL_HALF_PAGE_UP || arg->n == SCROLL_LEFT ? -1 : 1;

  double value = gtk_adjustment_get_value(a);

  double inc;
  if (arg->n == SCROLL_PAGE_UP || arg->n == SCROLL_PAGE_DOWN)
    inc = gtk_adjustment_get_page_increment(a);
  else if (arg->n == SCROLL_HALF_PAGE_UP || arg->n == SCROLL_HALF_PAGE_DOWN)
    inc = gtk_adjustment_get_page_increment(a) / 2;
  else
    inc = gtk_adjustment_get_step_increment(a);

  double lower  = gtk_adjustment_get_lower(a);
  double upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
  switch (arg->n) {
    case  SCROLL_TOP:      scroll = lower; break;
    case  SCROLL_BOTTOM:   scroll = upper; break;
    default:        scroll = value + sign * inc * NN(dwb.state.nummod); break;
  }

  scroll = scroll < lower ? lower : scroll > upper ? upper : scroll;
  if (scroll == value) 
    ret = false;
  else {
    gtk_adjustment_set_value(a, scroll);
  }
  return ret;
}/*}}}*/

/* dwb_com_set_zoom_level(KeyMap *km, Arg *arg) {{{*/
void 
dwb_com_set_zoom_level(KeyMap *km, Arg *arg) {
  GList *gl = arg->p ? arg->p : dwb.state.fview;
  webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(((View*)gl->data)->web), arg->d);

}/*}}}*/

/* dwb_com_set_orientation(KeyMap *km, Arg *arg) {{{*/
gboolean 
dwb_com_set_orientation(KeyMap *km, Arg *arg) {
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
  return true;
}/*}}}*/

/* History {{{*/
gboolean 
dwb_com_history_back(KeyMap *km, Arg *arg) {
  gboolean ret = false;
  WebKitWebView *w = CURRENT_WEBVIEW();
  if (webkit_web_view_can_go_back(w)) {
    webkit_web_view_go_back(w);
    ret = true;
  }
  return ret;
}
gboolean 
dwb_com_history_forward(KeyMap *km, Arg *arg) {
  gboolean ret = false;
  WebKitWebView *w = CURRENT_WEBVIEW();
  if (webkit_web_view_can_go_forward(w)) {
    webkit_web_view_go_forward(w);
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_com_open(KeyMap *km, Arg *arg) {{{*/
gboolean  
dwb_com_open(KeyMap *km, Arg *arg) {
  if (dwb.state.nv == OPEN_NORMAL)
    dwb.state.nv = arg->n;

  dwb.state.type = arg->i;

  if (arg && arg->p) {
    dwb_load_uri(arg);
  }
  else {
    dwb_focus_entry();
  }
  return true;
} /*}}}*/

/* dwb_com_open(KeyMap *km, Arg *arg) {{{*/
gboolean  
dwb_com_open_startpage(KeyMap *km, Arg *arg) {
  if (!dwb.misc.startpage) 
    return false;

  Arg a = { .p = dwb.misc.startpage };
  dwb_load_uri(&a);
  return true;
} /*}}}*/

/* dwb_com_toggle_maximized {{{*/
void 
dwb_com_maximized_hide(View *v, View *no) {
  if (dwb.misc.factor != 1.0) {
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), 1.0);
  }
  if (v != dwb.state.fview->data) {
    gtk_widget_hide(v->vbox);
  }
}
void 
dwb_com_maximized_show(View *v) {
  if (dwb.misc.factor != 1.0 && v != dwb.state.views->data) {
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), dwb.misc.factor);
  }
  gtk_widget_show(v->vbox);
}

void 
dwb_com_toggle_maximized(KeyMap *km, Arg *arg) {
  dwb.state.layout ^= MAXIMIZED;
  if (dwb.state.layout & MAXIMIZED) {
    g_list_foreach(dwb.state.views,  (GFunc)dwb_com_maximized_hide, NULL);
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
    g_list_foreach(dwb.state.views,  (GFunc)dwb_com_maximized_show, NULL);
  }
  dwb_resize(dwb.state.size);
  dwb_toggle_tabbar();
}/*}}}*/


/* dwb_com_remove_view(KeyMap *km, Arg *arg) {{{*/
void 
dwb_com_remove_view(KeyMap *km, Arg *arg) {
  dwb_view_remove();
}/*}}}*/

/* dwb_com_push_master {{{*/
gboolean 
dwb_com_push_master(KeyMap *km, Arg *arg) {
  return dwb_view_push_master(arg);
}/*}}}*/

/* dwb_com_focus_next(KeyMap *km, Arg *arg) {{{*/
gboolean 
dwb_com_focus_next(KeyMap *km, Arg *arg) {
  GList *gl = dwb.state.fview;
  if (!dwb.state.views->next) {
    return false;
  }
  if (gl->next) {
    if (dwb.state.layout & MAXIMIZED) {
      if (gl == dwb.state.views) {
        gtk_widget_hide(dwb.gui.left);
        gtk_widget_show(dwb.gui.right);
      }
      gtk_widget_show(((View *)gl->next->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(gl->next);
  }
  else {
    if (dwb.state.layout & MAXIMIZED) {
      gtk_widget_hide(dwb.gui.right);
      gtk_widget_show(dwb.gui.left);
      gtk_widget_show(((View *)dwb.state.views->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(g_list_first(dwb.state.views));
  }
  return true;
}/*}}}*/

/* dwb_com_focus_prev(KeyMap *km, Arg *arg) {{{*/
gboolean 
dwb_com_focus_prev(KeyMap *km, Arg *arg) {
  GList *gl = dwb.state.fview;
  if (!dwb.state.views->next) {
    return false;
  }
  if (gl == dwb.state.views) {
    GList *last = g_list_last(dwb.state.views);
    if (dwb.state.layout & MAXIMIZED) {
      gtk_widget_hide(dwb.gui.left);
      gtk_widget_show(dwb.gui.right);
      gtk_widget_show(((View *)last->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(last);
  }
  else {
    if (dwb.state.layout & MAXIMIZED) {
      if (gl == dwb.state.views->next) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
      gtk_widget_show(((View *)gl->prev->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(gl->prev);
  }
  return true;
}/*}}}*/

/* dwb_com_focus_view{{{*/
gboolean
dwb_com_focus_nth_view(KeyMap *km, Arg *arg) {
  if (!dwb.state.views->next) 
    return true;
  GList *l = g_list_nth(dwb.state.views, dwb.state.nummod);
  if (!l) 
    return false;
  if (l != dwb.state.fview) {
    if (dwb.state.layout & MAXIMIZED) { 
      if (l == dwb.state.views) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
      else {
        gtk_widget_hide(dwb.gui.left);
        gtk_widget_show(dwb.gui.right);
      }
      gtk_widget_show(((View *)l->data)->vbox);
      gtk_widget_hide(((View *)dwb.state.fview->data)->vbox);
    }
    dwb_focus(l);
  }
  return true;
}/*}}}*/


/* dwb_com_yank {{{*/
gboolean
dwb_com_yank(KeyMap *km, Arg *arg) {
  WebKitWebView *w = CURRENT_WEBVIEW();

  GdkAtom atom = GDK_POINTER_TO_ATOM(arg->p);
  GtkClipboard *old = gtk_clipboard_get(atom);

  char *text = NULL;
  char buffer[STRING_LENGTH];
  char *message = buffer;

  gboolean ret = false;

  if (webkit_web_view_has_selection(w)) {
    webkit_web_view_copy_clipboard(w);
    GtkClipboard *new = gtk_clipboard_get(GDK_NONE);
    text = gtk_clipboard_wait_for_text(new);
    message = "selection";
  }
  else {
    text = g_strdup(webkit_web_view_get_uri(w));
    message = text;
  }
  if (text) {
    if (strlen(text)) {
      gtk_clipboard_set_text(old, text, -1);
      dwb_set_normal_message(dwb.state.fview, true, "Yanked: %s", message);
      ret = true;
    }
    g_free(text);
  }
  return ret;
}/*}}}*/

/* dwb_com_paste {{{*/
gboolean
dwb_com_paste(KeyMap *km, Arg *arg) {
  GdkAtom atom = GDK_POINTER_TO_ATOM(arg->p);
  GtkClipboard *clipboard = gtk_clipboard_get(atom);
  char *text = NULL;

  if ( (text = gtk_clipboard_wait_for_text(clipboard)) ) {
    if (dwb.state.nv == OPEN_NORMAL)
      dwb.state.nv = arg->n;
    Arg a = { .p = text };
    dwb_load_uri(&a);
    g_free(text);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_com_entry_delete_word {{{*/
gboolean 
dwb_com_entry_delete_word(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));
  char *text = g_strdup(GET_TEXT());

  if (position > 0) {
    int new = dwb_entry_position_word_back(position);
    dwb_util_cut_text(text, new,  position);
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
    gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), new);
    FREE(text);
  }
  return true;
}/*}}}*/

/* dwb_com_entry_delete_letter {{{*/
gboolean 
dwb_com_entry_delete_letter(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));
  char *text = g_strdup(GET_TEXT());

  if (position > 0) {
    dwb_util_cut_text(text, position-1,  position);
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
    gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), position-1);
    FREE(text);
  }
  return true;
}/*}}}*/

/* dwb_com_entry_delete_line {{{*/
gboolean 
dwb_com_entry_delete_line(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));
  char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), &text[position]);
  FREE(text);
  return true;
}/*}}}*/

/* dwb_com_entry_word_forward {{{*/
gboolean 
dwb_com_entry_word_forward(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));

  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), dwb_entry_position_word_forward(position));
  return  true;
}/*}}}*/

/* dwb_com_entry_word_back {{{*/
gboolean 
dwb_com_entry_word_back(KeyMap *km, Arg *a) {
  int position = gtk_editable_get_position(GTK_EDITABLE(dwb.gui.entry));

  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), dwb_entry_position_word_back(position));
  return  true;
}/*}}}*/

/* dwb_com_entry_history_forward {{{*/
gboolean 
dwb_com_entry_history_forward(KeyMap *km, Arg *a) {
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
  return  true;
}/*}}}*/

/* dwb_com_entry_history_back {{{*/
gboolean 
dwb_com_entry_history_back(KeyMap *km, Arg *a) {
  Navigation *n = NULL;

  if (!dwb.fc.commands)
    return false;

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
  return  true;
}/*}}}*/

gboolean
dwb_com_save_session(KeyMap *km, Arg *arg) {
  if (arg->n == NORMAL_MODE) {
    dwb.state.mode = SAVE_SESSION;
    dwb_session_save(NULL);
    dwb_end();
  }
  else {
    dwb.state.mode = arg->n;
    dwb_focus_entry();
    dwb_set_normal_message(dwb.state.fview, false, "Session name:");
  }
  return true;
}
/*}}}*/

/* dwb_com_bookmarks {{{*/
gboolean
dwb_com_bookmarks(KeyMap *km, Arg *arg) {
  if (!g_list_length(dwb.fc.bookmarks)) {
    return false;
  }
  if (dwb.state.nv == OPEN_NORMAL)
    dwb.state.nv = arg->n;
  dwb_focus_entry();
  dwb_comp_complete(COMP_BOOKMARK, 0);

  return true;
}/*}}}*/

/* dwb_com_history{{{*/
gboolean  
dwb_com_complete_type(KeyMap *km, Arg *arg) {
  if (!g_list_length(dwb.fc.history)) {
    return false;
  }
  dwb_comp_complete(arg->n, 0);

  return true;

}/*}}}*/

/* dwb_com_toggle_ugly {{{*/
void
dwb_com_toggle_ugly(GList **fc, const char *desc) {
  WebKitWebView *web = CURRENT_WEBVIEW();
  const char *uri = webkit_web_view_get_uri(web);
  char *host = dwb_get_host(uri);
  GList *block;
  if (host) {
    if ( (block = dwb_get_host_blocked(*fc, host)) ) {
      *fc = g_list_delete_link(*fc, block);
      dwb_set_normal_message(dwb.state.fview, true, "%s blocked for domain %s", desc, host);
    }
    else {
      *fc = g_list_prepend(*fc, host);
      dwb_set_normal_message(dwb.state.fview, true, "%s allowed for domain %s", desc, host);
    }
    webkit_web_view_reload(web);
  }
}/*}}}*/

/* dwb_com_toggle_block_content{{{*/
gboolean
dwb_com_toggle_block_content(KeyMap *km, Arg *arg) {
  dwb_com_toggle_ugly(&dwb.fc.content_block_allow, "Content");
  return true;
}/*}}}*/

/* dwb_com_allow_content (){{{*/
gboolean 
dwb_com_allow_content(KeyMap *km, Arg *arg) {
  dwb_com_toggle_ugly(&dwb.fc.content_allow, "Content");
  return true;
}/*}}}*/

/* dwb_com_allow_plugins (Arg *){{{*/
gboolean 
dwb_com_allow_plugins(KeyMap *km, Arg *arg) {
  const char *host = CURRENT_HOST();
  GList *list = NULL;

  if (!host || !strlen(host)) {
    return false;
  }
  if ( (list = g_list_find_custom(dwb.fc.plugins_allow, host, (GCompareFunc)strcmp)) ) {
    g_free(list->data);
    dwb.fc.plugins_allow = g_list_remove_link(dwb.fc.plugins_allow, list);
    dwb_set_normal_message(dwb.state.fview, true, "Plugins blocked on %s", host);
  }
  else {
    dwb.fc.plugins_allow = g_list_prepend(dwb.fc.plugins_allow, g_strdup(host));
    dwb_set_normal_message(dwb.state.fview, true, "Plugins allowed on %s", host);
  }
  webkit_web_view_reload(CURRENT_WEBVIEW());
  return true;
}/*}}}*/

/* dwb_com_new_window_or_view{{{*/
gboolean 
dwb_com_new_window_or_view(KeyMap *km, Arg *arg) {
  dwb.state.nv = arg->n;
  return true;
}/*}}}*/

/* dwb_com_save_files(KeyMap *km, Arg *arg) {{{*/
gboolean 
dwb_com_save_files(KeyMap *km, Arg *arg) {
  if (dwb_save_files(false)) {
    dwb_set_normal_message(dwb.state.fview, true, "Configuration files saved");
    return true;
  }
  return false;
}/*}}}*/

/* dwb_com_undo() {{{*/
gboolean
dwb_com_undo(KeyMap *km, Arg *arg) {
  if (dwb.state.undo_list) {
    WebKitWebView *web = WEBVIEW(dwb_add_view(NULL));
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
    return true;
  }
  return false;
}/*}}}*/

/* dwb_com_print (Arg *) {{{*/
gboolean
dwb_com_print(KeyMap *km, Arg *arg) {
  WebKitWebFrame *frame = webkit_web_view_get_focused_frame(CURRENT_WEBVIEW());

  if (frame) {
    webkit_web_frame_print(frame);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_com_execute_userscript (Arg *) {{{*/
gboolean
dwb_com_execute_userscript(KeyMap *km, Arg *arg) {
  if (!dwb.misc.userscripts) 
    return false;

  if (arg->p) {
    char *path = g_build_filename(dwb.files.userscripts, arg->p, NULL);
    Arg a = { .p = path };
    dwb_execute_user_script(km, &a);
    g_free(path);
  }
  else {
    dwb_focus_entry();
    dwb_comp_complete(COMP_USERSCRIPT, 0);
  }

  return true;
}/*}}}*/
