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

#include <string.h>
#include <libgen.h>
#include <gdk/gdkkeysyms.h> 
#include "dwb.h"
#include "commands.h"
#include "util.h"
#include "entry.h"
#include "completion.h"

static GList * completion_update_completion(GtkWidget *box, GList *comps, GList *active, int max, int back);
static GList * completion_get_simple_completion(GList *gl);

typedef gboolean (*Match_Func)(char*, const char*);
static char *_typed;
static int _last_buf;
static gboolean _leading0 = false;
static char *_current_command;
static int _command_len;

/* GUI_FUNCTIONS {{{*/
/* completion_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {{{*/
static void 
completion_modify_completion_item(Completion *c, DwbColor *fg, DwbColor *bg, PangoFontDescription  *fd) {
  DWB_WIDGET_OVERRIDE_COLOR(c->llabel, GTK_STATE_NORMAL, fg);
  DWB_WIDGET_OVERRIDE_COLOR(c->rlabel, GTK_STATE_NORMAL, fg);
  DWB_WIDGET_OVERRIDE_COLOR(c->mlabel, GTK_STATE_NORMAL, fg);

  DWB_WIDGET_OVERRIDE_BACKGROUND(c->event, GTK_STATE_NORMAL, bg);

  DWB_WIDGET_OVERRIDE_FONT(c->llabel, dwb.font.fd_completion);
  DWB_WIDGET_OVERRIDE_FONT(c->mlabel, dwb.font.fd_completion);
  DWB_WIDGET_OVERRIDE_FONT(c->rlabel, dwb.font.fd_completion);
}/*}}}*/

/* completion_get_completion_item(Navigation *)      return: Completion * {{{*/
static Completion * 
completion_get_completion_item(const char  *left, const char *right, const char *middle, void *data) {
  Completion *c = g_malloc(sizeof(Completion));

  c->llabel = gtk_label_new(left);
  c->rlabel = gtk_label_new(right);
  c->mlabel = gtk_label_new(middle);
  c->event = gtk_event_box_new();
  c->data = data;
  gboolean expand = middle != NULL && right != NULL;
#if _HAS_GTK3
  GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous(GTK_BOX(hbox), expand);
#else 
  GtkWidget *hbox = gtk_hbox_new(expand, 0);
#endif

  gtk_box_pack_start(GTK_BOX(hbox), c->llabel, true, true, 5);
  gtk_box_pack_start(GTK_BOX(hbox), c->mlabel, middle != NULL, true, 5);
  gtk_box_pack_start(GTK_BOX(hbox), c->rlabel, right != NULL, true, 5);

  gtk_label_set_ellipsize(GTK_LABEL(c->llabel), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_ellipsize(GTK_LABEL(c->rlabel), PANGO_ELLIPSIZE_MIDDLE);

  gtk_misc_set_alignment(GTK_MISC(c->llabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(c->mlabel), 1.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(c->rlabel), 1.0, 0.5);

  completion_modify_completion_item(c, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_inactive);

  GtkWidget *alignment = gtk_alignment_new(0.5, 0.5, 1, 1);
  int padding = GET_INT("bars-padding");
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), padding, padding, padding, padding);
  gtk_container_add(GTK_CONTAINER(alignment), hbox);
  gtk_container_add(GTK_CONTAINER(c->event), alignment);

  return c;
}/*}}}*/

/* completion_init_completion {{{*/
static GList * 
completion_init_completion(GList *store, GList *gl, gboolean word_beginnings, void *data, const char *value) {
  Navigation *n;
  const char *input = GET_TEXT();
  gboolean match;
  char **token = NULL;
  _typed = g_strdup(input);
  if (dwb.state.mode & COMMAND_MODE) 
    input = strchr(input, ' ');
  if (input == NULL) 
    input = "";
  else 
    token = g_strsplit(input, " ", -1);
  Match_Func func = word_beginnings ? (Match_Func)g_str_has_prefix : (Match_Func)util_strcasestr;


  for (GList *l = gl; l; l=l->next) {
    n = l->data;
    match = false;
    if (*input == 0)  {
      match = true;
    }
    else {
      for (int i=0; token[i]; i++) {
        if (func(n->first, token[i]) || (!word_beginnings && n->second && func(n->second, token[i]))) {
          match = true;
        }
        else {
          match = false; 
          break;
        }
      }
    }
    if (match) {
      Completion *c = completion_get_completion_item(n->first, n->second, value, data);
      gtk_box_pack_start(GTK_BOX(dwb.gui.compbox), c->event, false, false, 0);
      store = g_list_append(store, c);
    }
  }
  g_strfreev(token);
  return store;
}/*}}}*/

/* dwb_completion_set_text(Completion *) {{{*/
void
completion_set_entry_text(Completion *c) {
  const char *text; 
  int l;
  CompletionType type = dwb_eval_completion_type();
  switch (type) {
    case COMP_QUICKMARK: text = c->data; 
                         break;
    default: text = gtk_label_get_text(GTK_LABEL(c->llabel));
             break;
  }
  
  if (dwb.state.mode & COMMAND_MODE && _current_command) {
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), _current_command);
    l = strlen(_current_command);
    gtk_editable_insert_text(GTK_EDITABLE(dwb.gui.entry), " ", -1, &l);
    gtk_editable_insert_text(GTK_EDITABLE(dwb.gui.entry), text, -1, &l);
  }
  else {
    char buf[7];
    gtk_editable_delete_text(GTK_EDITABLE(dwb.gui.entry), 0, -1);
    if (dwb.state.nummod > -1) {
      l =  snprintf(buf, 7, "%d", dwb.state.nummod);
      gtk_editable_insert_text(GTK_EDITABLE(dwb.gui.entry), buf, -1, &l);
    }
    gtk_editable_insert_text(GTK_EDITABLE(dwb.gui.entry), text, -1, &l);
  }
  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);

}/*}}}*/

/* completion_update_completion(GtkWidget *box, GList *comps, GList *active, int max, int back)    Return *GList (Completions*){{{*/
static GList *
completion_update_completion(GtkWidget *box, GList *comps, GList *active, int max, int back) {
  GList *old, *new;
  Completion *c;

  int length = g_list_length(comps);
  int items = MAX(length, max);
  int r = (max) % 2;
  int offset = max / 2 - 1 + r;

  old = active;
  int position = g_list_position(comps, active) + 1;
  if (!back) {
    if (! (new = old->next) ) {
      new = g_list_first(comps);
    }
    if (position > offset &&  position < items - offset - 1 + r) {
      c = g_list_nth(comps, position - offset - 1)->data;
      gtk_widget_hide(c->event);
      c = g_list_nth(comps, position + offset + 1 - r)->data;
      gtk_widget_show_all(c->event);
    }
    else {
      if (position == items || position  == 1) {
        int i = 0;
        for (GList *l = g_list_last(comps); l && i<max; l=l->prev) {
          gtk_widget_hide(COMP_EVENT_BOX(l));
        }
        gtk_widget_show(box);
        i = 0;
        for (GList *l = g_list_first(comps); l && i<max ;l=l->next, i++) {
          gtk_widget_show_all(((Completion*)l->data)->event);
        }
      }
    }
  }
  else {
    if (! (new = old->prev) ) {
      new = g_list_last(comps);
    }
    if (position -1> offset   &&  position < items - offset + r) {
      c = g_list_nth(comps, position - offset - 2)->data;
      gtk_widget_show_all(c->event);
      c = g_list_nth(comps, position + offset - r)->data;
      gtk_widget_hide(c->event);
    }
    else {
      if (position == 1) {
        int i = 0;
        for (GList *l = g_list_first(comps); l && i<max; l=l->next, i++) {
          gtk_widget_hide(COMP_EVENT_BOX(l));
        }
        gtk_widget_show(box);
        i = 0;
        for (GList *l = g_list_last(comps); l && i<max ;l=l->prev, i++) {
          c = l->data;
          gtk_widget_show_all(c->event);
        }
      }
    }
  }
  completion_modify_completion_item(old->data, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_inactive);
  completion_modify_completion_item(new->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_active);
  active = new;
  completion_set_entry_text(active->data);
  return active;
}/*}}}*/
/*}}}*/

/* STANDARD_COMPLETION {{{*/
/* dwb_clean_completion() {{{*/
void 
completion_clean_completion(gboolean set_text) {
  for (GList *l = dwb.comps.completions; l; l=l->next) {
    g_free(l->data);
  }
  g_list_free(dwb.comps.completions);
  if (dwb.comps.view != NULL) 
    gtk_widget_destroy(dwb.gui.compbox);
  dwb.comps.view = NULL;
  dwb.comps.completions = NULL;
  dwb.comps.active_comp = NULL;
  if (set_text && _typed != NULL)
    entry_set_text(_typed);

  FREE0(_current_command);
  _command_len = 0;

  FREE0(_typed);

  if (dwb.state.mode & COMPLETE_BUFFER) {
    _last_buf = 0;
    _leading0 = false;
    dwb.state.mode &= ~COMPLETE_BUFFER;
    dwb_change_mode(NORMAL_MODE, true);
  }
  else 
    dwb.state.mode &= ~(COMPLETION_MODE|COMPLETE_PATH);
}/*}}}*/

/* completion_show_completion(int back) {{{*/
static void 
completion_show_completion(int back) {
  int i=0;
  if (back) {
    dwb.comps.active_comp = g_list_last(dwb.comps.completions);
    for (GList *l = dwb.comps.active_comp; l && i<dwb.misc.max_c_items; l=l->prev, i++) {
      gtk_widget_show_all(((Completion*)l->data)->event);
    }
  }
  else {
    dwb.comps.active_comp = g_list_first(dwb.comps.completions);
    for (GList *l = dwb.comps.active_comp; l && i<dwb.misc.max_c_items; l=l->next, i++) {
      gtk_widget_show_all(((Completion*)l->data)->event);
    }
  }
  if (dwb.comps.active_comp != NULL) {
    completion_modify_completion_item(dwb.comps.active_comp->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_active);
    completion_set_entry_text(dwb.comps.active_comp->data);
    gtk_widget_show(dwb.gui.compbox);
  }

}/*}}}*/

/* dwb_completion_get_normal      return: GList *Completions{{{*/
static GList *
completion_get_normal_completion() {
  GList *list = NULL;

  if (!(dwb.state.mode & COMMAND_MODE) ) {
    if (GET_BOOL("complete-userscripts")) 
      list = completion_init_completion(list, dwb.misc.userscripts, false, NULL, "Userscript");
    if (GET_BOOL("complete-searchengines")) 
      list = completion_init_completion(list, dwb.fc.se_completion, false, NULL, "Searchengine");
  }
  if (GET_BOOL("complete-bookmarks")) 
    list = completion_init_completion(list, dwb.fc.bookmarks, false, NULL, "Bookmark");
  if (GET_BOOL("complete-history")) 
    list = completion_init_completion(list, dwb.fc.history, false, NULL, "History");

  return  list;
}/*}}}*/

/* completion_get_simple_completion      return: GList *Completions{{{*/
static GList *
completion_get_simple_completion(GList *gl) {
  GList *list = NULL;
  list = completion_init_completion(list, gl, false, NULL, NULL);
  return  list;
}/*}}}*/

/* dwb_completion_get_settings      return: GList *Completions{{{*/
static GList *
completion_get_settings_completion() {
  GList *l = g_hash_table_get_values(dwb.settings);
  l = g_list_sort(l, (GCompareFunc)util_web_settings_sort_first);
  const char *input = GET_TEXT();
  GList *list = NULL;

  for (; l; l=l->next) {
    WebSettings *s = l->data;
    Navigation n = s->n;
    if (g_strrstr(n.first, input)) {
      char *value = util_arg_to_char(&s->arg, s->type);
      Completion *c = completion_get_completion_item(s->n.first, s->n.second, value, s);
      gtk_box_pack_start(GTK_BOX(dwb.gui.compbox), c->event, false, false, 0);
      list = g_list_append(list, c);
    }
  }
  if (l != NULL)
    g_list_free(l);
  return list;
}/*}}}*/

/* completion_create_key_completion(GList *l, const char *first, KeyMap *m) {{{*/
static GList * 
completion_create_key_completion(GList *l, const char *first, KeyMap *m) {
  char *mod = dwb_modmask_to_string(m->mod);
  char *value = g_strdup_printf("%s %s", mod, m->key);
  Completion *c = completion_get_completion_item(first, m->map->n.second, value, m);
  gtk_box_pack_start(GTK_BOX(dwb.gui.compbox), c->event, false, false, 0);
  l = g_list_append(l, c);
  g_free(value);
  g_free(mod);
  return l;
}/*}}}*/

static GList *
completion_complete_scripts() {
  GList *list = NULL;
  for (GList *l = dwb.state.script_completion; l; l=l->next) {
    Completion *c = completion_get_completion_item(((Navigation*)l->data)->first, ((Navigation*)l->data)->second, NULL, NULL);
    gtk_box_pack_start(GTK_BOX(dwb.gui.compbox), c->event, false, false, 0);
    list = g_list_append(list, c);
  }
  dwb.state.mode = COMPLETE_SCRIPTS;
  return list;
}
/*dwb_completion_get_keys()         return  GList *Completions{{{*/
static GList * 
completion_get_key_completion(gboolean entry) {
  GList *list = NULL;
  const char *input = GET_TEXT();

  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)util_keymap_sort_first);
  input = dwb_parse_nummod(input);

  /* check for aliases first */
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    if (!entry && ((m->map->prop & CP_OVERRIDE_ENTRY) || !(m->map->prop & CP_COMMANDLINE))) {
      continue;
    }
    if (!entry) {
      for (int i=0; m->map->alias[i] != NULL; i++) {
        if (g_str_has_prefix(m->map->alias[i], input) || !g_strcmp0(input, m->map->alias[i]) ) {
          list = completion_create_key_completion(list, m->map->alias[i], m);
          break;
        }
      }
    }
  }

  /* check for long commands */
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    if (!entry && ((m->map->prop & CP_OVERRIDE_ENTRY) || !(m->map->prop & CP_COMMANDLINE))) {
      continue;
    }
    Navigation n = m->map->n;
    if (g_str_has_prefix(n.first, input)) {
      list = completion_create_key_completion(list, n.first, m);
    }
  }
  return list;
}/*}}}*/

/* completion_path {{{*/
static void 
completion_path(void) {
  if (! (dwb.state.mode & DOWNLOAD_GET_PATH) )
    dwb.state.mode = COMPLETE_PATH;
  completion_complete_path(false);
}/*}}}*/

/* completion_get_quickmarks {{{*/
static  GList *
completion_get_quickmarks(int back) {
  GList *list = NULL;
  Quickmark *q;
  char *escaped = NULL;
  const char *input = GET_TEXT();
  _typed = g_strdup(input);
  for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
    q = l->data;
    if (g_str_has_prefix(q->key, input)) {
      Completion *c = completion_get_completion_item(NULL, q->nav->first, NULL, q->key);
      escaped = g_markup_printf_escaped("%s\t\t<span style='italic'>%s</span>", q->key, q->nav->second);
      if (escaped != NULL) {
        gtk_label_set_markup(GTK_LABEL(c->llabel), escaped);
        g_free(escaped);
      }
      else 
        gtk_label_set_text(GTK_LABEL(c->llabel), q->key);
      gtk_box_pack_start(GTK_BOX(dwb.gui.compbox), c->event, false, false, 0);
      list = g_list_append(list, c);
    }
  }
  if (back)
    list = g_list_reverse(list);
  return list;
}/*}}}*/

static void
completion_buffer_exec(GList *gl) {
  completion_clean_completion(false);
  dwb_focus_view(gl);
  dwb_change_mode(NORMAL_MODE, true);
}
#define COMPLETION_BUFFER_GET_PRIVATE(c)  (((Completion*)((c)->data))->data)
void
completion_buffer_key_press(GdkEventKey *e) {
  if (DWB_TAB_KEY(e)) 
    completion_complete(COMP_BUFFER, e->state & GDK_SHIFT_MASK);
  else if (DIGIT(e)) {
    int value = e->keyval - GDK_KEY_0;
    int length = g_list_length(dwb.state.views);
    if (length < 10) {
      if (value != 0 && value <= length) 
        completion_buffer_exec(g_list_nth(dwb.state.views, value-1));
    }
    else {
      _last_buf = 10*_last_buf + value;
      if (_last_buf > length) {
        completion_clean_completion(false);
        dwb_change_mode(NORMAL_MODE, true);
        return;
      }
      if (_last_buf != 0) {
        if ((_last_buf < 10 && _leading0 == true) || _last_buf >= 10) 
          completion_buffer_exec(g_list_nth(dwb.state.views, _last_buf-1));
      }
      else 
        _leading0 = true;
    }
  }
}
void
completion_eval_buffer_completion(void) {
  /* TODO Wrong View is saved  */
  GList *l = COMPLETION_BUFFER_GET_PRIVATE(dwb.comps.active_comp);
  completion_buffer_exec(l);
}
#undef COMPLETION_BUFFER_GET_PRIVATE
/* completion_complete_buffer {{{*/
static GList *
completion_complete_buffer() {
  GList *list = NULL;
  int i=1;
  const char *format = g_list_length(dwb.state.views) > 10 ? "%02d : %s" : "%d : %s";
  for (GList *l = dwb.state.views;l; l=l->next) {
    WebKitWebView *wv = WEBVIEW(l);
    const char *title = webkit_web_view_get_title(wv);
    const char *uri = webkit_web_view_get_uri(wv);
    char *text = g_strdup_printf(format, i, title != NULL ? title : uri);
    Completion *c = completion_get_completion_item(text, uri, NULL, l);
    g_free(text);
    gtk_box_pack_start(GTK_BOX(dwb.gui.compbox), c->event, false, false, 0);
    list = g_list_append(list, c);
    i++;
  }
  if (list != NULL) {
    dwb.state.mode = COMPLETE_BUFFER;
    entry_focus();
  }
  return list;
}/*}}}*/

static gboolean
completion_command_line() {
  KeyMap *km = NULL;
  gboolean ret = false;

  const char *text = GET_TEXT();
  while (g_ascii_isspace(*text))
    text++;
  char **token = g_strsplit(text, " ", 2);
  const char *bak;
  if (dwb.state.mode & COMMAND_MODE) {
    for (GList *l = dwb.keymap; l; l=l->next) {
      bak = token[0];
      km = l->data;
      while (bak && (g_ascii_isspace(*bak) || g_ascii_isdigit(*bak))) 
        bak++;
      if (! g_strcmp0(bak, km->map->n.first) && km->map->entry & EP_COMP_DEFAULT) {
        ret = true;
        break;
      }
      else {
        for (int i=0; km->map->alias[i] != NULL; i++) {
          if (! g_strcmp0(bak, km->map->alias[i]) && km->map->entry & EP_COMP_DEFAULT) {
            ret = true;
            break;
          }
        }
        if (ret) break;
      }
    }
  }
  if (ret) {
    _command_len = util_strlen_trailing_space(text);
    if ((_command_len > 0 && g_ascii_isspace(text[_command_len-1])) || token[1] != NULL)  {
      FREE0(_current_command);
      _current_command = g_strdup(token[0]);
      dwb.state.mode |= COMPLETE_COMMAND_MODE;
      _command_len++;
    }
    else 
      ret = false;
  }
  g_strfreev(token);
  return ret;
}
/* completion_complete {{{*/
DwbStatus 
completion_complete(CompletionType type, int back) {
  DwbStatus ret = STATUS_OK;
  if (dwb.state.mode & COMMAND_MODE) {
    if (completion_command_line()) 
      type = COMP_NONE;
  }

  dwb.state.mode &= ~(COMPLETE_PATH | AUTO_COMPLETE | COMPLETE_COMMAND_MODE);
  if ( !(dwb.state.mode & COMPLETION_MODE) ) {
#if _HAS_GTK3
    dwb.gui.compbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(dwb.gui.compbox), true);
#else 
    dwb.gui.compbox = gtk_vbox_new(true, 0);
#endif
    gtk_box_pack_start(GTK_BOX(dwb.gui.bottombox), dwb.gui.compbox, false, false, 0);
    switch (type) {
      case COMP_SETTINGS:    dwb.comps.completions = completion_get_settings_completion(); break;
      case COMP_KEY:         dwb.comps.completions = completion_get_key_completion(true); break;
      case COMP_COMMAND:     dwb.comps.completions = completion_get_key_completion(false); break;
      case COMP_BOOKMARK:    dwb.comps.completions = completion_get_simple_completion(dwb.fc.bookmarks); break;
      case COMP_HISTORY:     dwb.comps.completions = completion_get_simple_completion(dwb.fc.history); break;
      case COMP_USERSCRIPT:  dwb.comps.completions = completion_get_simple_completion(dwb.misc.userscripts); break;
      case COMP_SEARCH:      dwb.comps.completions = completion_get_simple_completion(dwb.fc.se_completion); break;
      case COMP_QUICKMARK:   dwb.comps.completions = completion_get_quickmarks(back); break;
      case COMP_PATH:        completion_path(); return STATUS_OK;
      case COMP_BUFFER:      dwb.comps.completions = completion_complete_buffer(); break;
      case COMP_SCRIPT:      dwb.comps.completions = completion_complete_scripts(); break;
      default:               dwb.comps.completions = completion_get_normal_completion(); break;
    }
    if (!dwb.comps.completions) {
      return STATUS_ERROR;
    }
    dwb.state.mode |= COMPLETION_MODE;
    completion_show_completion(back);
    dwb.comps.view = dwb.state.fview;
  }
  else if (dwb.comps.completions && dwb.comps.active_comp) {
    dwb.comps.active_comp = completion_update_completion(dwb.gui.compbox, dwb.comps.completions, dwb.comps.active_comp, dwb.misc.max_c_items, back);
  }
  return ret;
}/*}}}*/
/*}}}*/

/* AUTOCOMPLETION {{{*/
/*completion_eval_autocompletion{{{*/
void 
completion_eval_autocompletion() {
  Completion *c = dwb.comps.active_auto_c->data;
  KeyMap *m = c->data;
  dwb_change_mode(NORMAL_MODE, true);
  commands_simple_command(m);
}/*}}}*/

/* dwb_set_autcompletion{{{*/
DwbStatus 
completion_set_autcompletion(GList *l, WebSettings *s) {
  dwb.comps.autocompletion = s->arg_local.b;
  return STATUS_OK;
}/*}}}*/

/* completion_cleanautocompletion{{{*/
void 
completion_clean_autocompletion() {
  for (GList *l = dwb.comps.auto_c; l; l=l->next) {
    g_free(l->data);
  }
  g_list_free(dwb.comps.auto_c);
  gtk_widget_destroy(dwb.gui.autocompletion);
  dwb.comps.auto_c = NULL;
  dwb.comps.active_auto_c = NULL;
  dwb.state.mode &= ~AUTO_COMPLETE;

  gtk_widget_show(dwb.gui.entry);
  gtk_widget_show(dwb.gui.rstatus);
  gtk_widget_show(dwb.gui.urilabel);
  if (! (dwb.state.bar_visible & BAR_VIS_STATUS) ) 
    gtk_widget_hide(dwb.gui.bottombox);

}/*}}}*/

/* completion_init_autocompletion (GList *gl)      return: GList * (Completion*){{{*/
static GList * 
completion_init_autocompletion(GList *gl) {
  GList *ret = NULL;
  char buffer[128];

#if _HAS_GTK3
  dwb.gui.autocompletion = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_set_homogeneous(GTK_BOX(dwb.gui.autocompletion), true);
#else 
  dwb.gui.autocompletion = gtk_hbox_new(true, 2);
#endif
  int i=0;
  for (GList *l=gl; l; l=l->next, i++) {
    KeyMap *m = l->data;
    if (! (m->map->prop & CP_OVERRIDE_ENTRY) ) {
      snprintf(buffer, 128, "%s  <span style='italic'>%s</span>", m->key, m->map->n.second);
      Completion *c = completion_get_completion_item(NULL, NULL, NULL, m);
      gtk_label_set_use_markup(GTK_LABEL(c->llabel), true);
      gtk_label_set_markup(GTK_LABEL(c->llabel), buffer);
      ret = g_list_append(ret, c);
      if (i<5) {
        gtk_widget_show_all(c->event);
      }
      gtk_box_pack_start(GTK_BOX(dwb.gui.autocompletion), c->event, true,  true, 1);
    }
  }
  gtk_box_pack_start(GTK_BOX(dwb.gui.status_hbox), dwb.gui.autocompletion, true,  true, 10);
  gtk_widget_hide(dwb.gui.entry);
  gtk_widget_hide(dwb.gui.rstatus);
  gtk_widget_hide(dwb.gui.urilabel);
  gtk_widget_show(dwb.gui.autocompletion);
  return ret;
}/*}}}*/

/* dwb_autocomplete GList *gl(KeyMap*)  GdkEventKey *{{{*/
void
completion_autocomplete(GList *gl, GdkEventKey *e) {
  if (!dwb.comps.autocompletion) {
    return;
  }
  if (! (dwb.state.mode & AUTO_COMPLETE) && gl) {
    if (! gtk_widget_get_visible(dwb.gui.bottombox))
      gtk_widget_show(dwb.gui.bottombox);
    dwb.state.mode |= AUTO_COMPLETE;
    dwb.comps.auto_c = completion_init_autocompletion(gl);
    dwb.comps.active_auto_c = g_list_first(dwb.comps.auto_c);
    completion_modify_completion_item(dwb.comps.active_auto_c->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_active);
  }
  else if (e) {
    dwb.comps.active_auto_c = completion_update_completion(dwb.gui.autocompletion, dwb.comps.auto_c, dwb.comps.active_auto_c, 5, e->state & GDK_SHIFT_MASK);
  }
}/*}}}*/
/*}}}*/

/* PATHCOMPLETION {{{*/
/* completion_clean_path_completion {{{*/
void 
completion_clean_path_completion() {
  if (dwb.comps.path_completion) {
    for (GList *l = g_list_first(dwb.comps.path_completion); l; l=l->next) {
      char *data = l->data;
      g_free(data);
    }
    g_list_free(dwb.comps.path_completion);
    dwb.comps.path_completion = NULL;
    dwb.comps.active_path = NULL;
  }
}/*}}}*/

/* completion_get_binaries(GList *list, char *text)      return GList *{{{*/
static GList *
completion_get_binaries(GList *list, char *text) {
  GDir *dir;
  char **paths = g_strsplit(g_getenv("PATH"), ":", -1);
  int i=0;
  char *path;
  const char *filename;

  while ( (path = paths[i++]) ) {
    if ( (dir = g_dir_open(path, 'r', NULL)) ) {
      while ( (filename = g_dir_read_name(dir))) {
        if (g_str_has_prefix(filename, text)) {
          list = g_list_prepend(list, g_strdup(filename));
        }
      }
      g_dir_close(dir);
    }
  }
  g_strfreev(paths);
  return list;
}/* }}} */

static GList *
completion_get_path(GList *list, char *text) {
  GDir *dir;
  char d_tmp[PATH_MAX];
  const char *filename;
  char path[PATH_MAX+1];
  char *store;
  char *newpath;
  gboolean prefix;

  if ( ( prefix = g_str_has_prefix(text, "file://")) ) 
    text += 7;

  g_strlcpy(d_tmp, text, PATH_MAX - 1);
  char *d_name = dirname(d_tmp);
  char *b_name = util_basename(text);
  char *d_current = g_get_current_dir();
  if (! *text ) {
    if (prefix) 
      list = g_list_prepend(list, g_strconcat("file://", d_current, "/", NULL));
    else
      list = g_list_prepend(list, g_strconcat(d_current, "/", NULL));
    g_free(d_current);
    return list;
  }
  else 
    g_free(d_current);

  if (!g_strcmp0(d_name, ".")) {
    completion_complete(0, 0);
    return NULL;
  }
  if (g_file_test(text, G_FILE_TEST_IS_DIR)) {
    g_strlcpy(path, text, BUFFER_LENGTH - 1);
    char path_last = path[strlen(path) - 1];
    if (path_last != '/' && path_last != '.') {
      if (prefix) 
        return g_list_prepend(list, g_strconcat("file://", path, "/", NULL));
      else 
        return g_list_prepend(list, g_strconcat(path, "/", NULL));
    }
  }
  else if (g_file_test(d_name, G_FILE_TEST_IS_DIR)) {
    g_strlcpy(path, d_name, BUFFER_LENGTH - 1);
  }
  if ( (dir = g_dir_open(path, 'r', NULL)) ) {
    while ( (filename = g_dir_read_name(dir)) ) {
      if ( ( !b_name && filename[0] != '.') || (b_name && g_str_has_prefix(filename, b_name))) {
        newpath = g_build_filename(path, filename, NULL);
        if (prefix) 
          store = g_strconcat("file://", newpath, "/" , NULL);
        else 
          store = g_strconcat(newpath, "/" , NULL);
        if (g_file_test(newpath, G_FILE_TEST_IS_DIR)) {
          list = g_list_prepend(list, store);
        }
        else {
          g_free(store);
        }
        g_free(newpath);
      }
    }
    g_dir_close(dir);
  }
  return list;
}/*}}}*/


/* completion_init_path_completion {{{*/
static void
completion_init_path_completion(int back) { 
  char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);
  char expanded[PATH_MAX];
  text = util_expand_home(expanded, text, PATH_MAX);

  dwb.comps.path_completion = dwb.comps.active_path = g_list_append(NULL, g_strdup(text));
  if (dwb.state.dl_action == DL_ACTION_EXECUTE) {
    GList *list = completion_get_binaries(NULL, text);
    list = g_list_sort(list, (GCompareFunc)g_strcmp0);
    dwb.comps.path_completion = g_list_concat(dwb.comps.path_completion, list);
  }
  else  {
    dwb.comps.path_completion = completion_get_path(dwb.comps.path_completion, text);
    dwb.comps.path_completion = g_list_sort(dwb.comps.path_completion, (GCompareFunc)g_strcmp0);
  }
  if (g_list_length(dwb.comps.path_completion) == 1) {
    completion_clean_path_completion();
    return;
  }
  if (dwb.comps.path_completion) {
    if (back) {
      dwb.comps.active_path = g_list_last(dwb.comps.path_completion);
    }
    else if (dwb.comps.path_completion->next) {
      dwb.comps.active_path = dwb.comps.path_completion->next;
    }
  }
}/*}}}*/

/* completion_complete_download{{{*/
void
completion_complete_path(int back) {
  if (! dwb.comps.path_completion ) {
    completion_init_path_completion(0);
  }
  else if (back) {
    if (dwb.comps.path_completion && dwb.comps.active_path && !(dwb.comps.active_path = dwb.comps.active_path->prev) ) {
      dwb.comps.active_path = g_list_last(dwb.comps.path_completion);
    }
  }
  else if (dwb.comps.active_path && !(dwb.comps.active_path = dwb.comps.active_path->next) ) {
    dwb.comps.active_path = g_list_first(dwb.comps.path_completion);
  }
  if (dwb.comps.active_path && dwb.comps.active_path->data) {
    entry_set_text(dwb.comps.active_path->data);
  }
}/*}}}*/
/*}}}*/
