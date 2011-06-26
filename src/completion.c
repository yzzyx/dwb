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

#include "completion.h"

static GList * dwb_comp_update_completion(GtkWidget *box, GList *comps, GList *active, int max, int back);
static GList * dwb_comp_get_simple_completion(GList *gl);

typedef gboolean (*Match_Func)(char*, const char*);

/* GUI_FUNCTIONS {{{*/
/* dwb_comp_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {{{*/
static void 
dwb_comp_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {
  gtk_widget_modify_fg(c->llabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_fg(c->rlabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_fg(c->mlabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_bg(c->event, GTK_STATE_NORMAL, bg);
  gtk_widget_modify_font(c->llabel, dwb.font.fd_completion);
  gtk_widget_modify_font(c->mlabel, dwb.font.fd_completion);
  gtk_widget_modify_font(c->rlabel, dwb.font.fd_completion);
}/*}}}*/

/* dwb_comp_get_completion_item(Navigation *)      return: Completion * {{{*/
static Completion * 
dwb_comp_get_completion_item(Navigation *n, void *data, const char *value) {
  Completion *c = g_malloc(sizeof(Completion));

  c->rlabel = gtk_label_new(n->second);
  c->llabel = gtk_label_new(n->first);
  c->mlabel = gtk_label_new(value);
  c->event = gtk_event_box_new();
  c->data = data;
  GtkWidget *hbox = gtk_hbox_new(value ? true : false, 0);

  gtk_box_pack_start(GTK_BOX(hbox), c->llabel, true, true, 5);
  gtk_box_pack_start(GTK_BOX(hbox), c->mlabel, value ? true : false , true, 5);
  gtk_box_pack_start(GTK_BOX(hbox), c->rlabel, true, true, 5);

  gtk_label_set_ellipsize(GTK_LABEL(c->llabel), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_ellipsize(GTK_LABEL(c->rlabel), PANGO_ELLIPSIZE_MIDDLE);

  gtk_misc_set_alignment(GTK_MISC(c->llabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(c->mlabel), 1.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(c->rlabel), 1.0, 0.5);

  dwb_comp_modify_completion_item(c, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_inactive);

  gtk_container_add(GTK_CONTAINER(c->event), hbox);
  return c;
}/*}}}*/

/* dwb_comp_init_completion {{{*/
static GList * 
dwb_comp_init_completion(GList *store, GList *gl, gboolean word_beginnings, void *data, const char *value) {
  Navigation *n;
  const char *input = GET_TEXT();
  Match_Func func = word_beginnings ? (Match_Func)g_str_has_prefix : (Match_Func)g_strrstr;
  // TODO groupline
#if 0
  GtkWidget *ev = gtk_event_box_new();
  GtkLabel *label = gtk_label_new(value);
  gtk_container_add(GTK_CONTAINER(ev), label);
  gtk_box_pack_start(GTK_BOX(CURRENT_VIEW()->compbox), ev, false, false, 0);
  gtk_widget_show_all(ev);
#endif
  for (GList *l = gl; l; l=l->next) {
    n = l->data;
    if (func(n->first, input) || (!word_beginnings && n->second && func(n->second, input))) {
      Completion *c = dwb_comp_get_completion_item(n, data, value);
      gtk_box_pack_start(GTK_BOX(CURRENT_VIEW()->compbox), c->event, false, false, 0);
      store = g_list_append(store, c);
    }
  }
  return store;
}/*}}}*/

/* dwb_completion_set_text(Completion *) {{{*/
static void
dwb_comp_set_entry_text(Completion *c) {
  const char *text = gtk_label_get_text(GTK_LABEL(c->llabel));
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);

}/*}}}*/

/* dwb_comp_update_completion(GtkWidget *box, GList *comps, GList *active, int max, int back)    Return *GList (Completions*){{{*/
static GList *
dwb_comp_update_completion(GtkWidget *box, GList *comps, GList *active, int max, int back) {
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
  dwb_comp_modify_completion_item(old->data, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_inactive);
  dwb_comp_modify_completion_item(new->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_active);
  active = new;
  dwb_comp_set_entry_text(active->data);
  return active;
}/*}}}*/
/*}}}*/

/* STANDARD_COMPLETION {{{*/
/* dwb_clean_completion() {{{*/
void 
dwb_comp_clean_completion() {
  for (GList *l = dwb.comps.completions; l; l=l->next) {
    FREE(l->data);
  }
  g_list_free(dwb.comps.completions);
  gtk_widget_destroy(CURRENT_VIEW()->compbox);
  dwb.comps.completions = NULL;
  dwb.comps.active_comp = NULL;
  dwb.state.mode &= ~(COMPLETION_MODE|COMPLETE_PATH);
}/*}}}*/

/* dwb_comp_show_completion(int back) {{{*/
static void 
dwb_comp_show_completion(int back) {
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
  dwb_comp_modify_completion_item(dwb.comps.active_comp->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_active);
  dwb_comp_set_entry_text(dwb.comps.active_comp->data);
  gtk_widget_show(CURRENT_VIEW()->compbox);

}/*}}}*/

/* dwb_completion_get_normal      return: GList *Completions{{{*/
static GList *
dwb_comp_get_normal_completion() {
  GList *list = NULL;

  if (dwb.state.complete_userscripts) 
    list = dwb_comp_init_completion(list, dwb.misc.userscripts, false, NULL, "Userscript");
  if (dwb.state.complete_searchengines) 
    list = dwb_comp_init_completion(list, dwb.fc.se_completion, false, NULL, "Searchengine");
  if (dwb.state.complete_commands) 
    list = dwb_comp_init_completion(list, dwb.fc.commands, true, NULL, "Commandline");
  if (dwb.state.complete_bookmarks) 
    list = dwb_comp_init_completion(list, dwb.fc.bookmarks, false, NULL, "Bookmark");
  if (dwb.state.complete_history) 
    list = dwb_comp_init_completion(list, dwb.fc.history, false, NULL, "History");

  return  list;
}/*}}}*/

/* dwb_comp_get_simple_completion      return: GList *Completions{{{*/
static GList *
dwb_comp_get_simple_completion(GList *gl) {
  GList *list = NULL;
  list = dwb_comp_init_completion(list, gl, false, NULL, NULL);
  return  list;
}/*}}}*/

/* dwb_completion_get_settings      return: GList *Completions{{{*/
static GList *
dwb_comp_get_settings_completion() {
  GList *l = g_hash_table_get_values(dwb.state.setting_apply == APPLY_GLOBAL ? dwb.settings : CURRENT_VIEW()->setting);
  l = g_list_sort(l, (GCompareFunc)dwb_util_web_settings_sort_first);
  const char *input = GET_TEXT();
  GList *list = NULL;

  for (; l; l=l->next) {
    WebSettings *s = l->data;
    if (dwb.state.setting_apply == APPLY_GLOBAL || !s->global) {
      Navigation n = s->n;
      if (g_strrstr(n.first, input)) {
        char *value = dwb_util_arg_to_char(&s->arg, s->type);
        Completion *c = dwb_comp_get_completion_item(&n, s, value);
        gtk_box_pack_start(GTK_BOX(CURRENT_VIEW()->compbox), c->event, false, false, 0);
        list = g_list_append(list, c);
      }
    }
  }
  return list;
}/*}}}*/

/*dwb_completion_get_keys()         return  GList *Completions{{{*/
static GList * 
dwb_comp_get_key_completion(gboolean entry) {
  GList *list = NULL;
  const char *input = GET_TEXT();

  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)dwb_util_keymap_sort_first);

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    if (!entry && (m->map->entry || !(m->map->prop & FM_COMMANDLINE))) {
      continue;
    }
    Navigation n = m->map->n;
    if (g_str_has_prefix(n.first, input)) {
      char *mod = dwb_modmask_to_string(m->mod);
      char *value = g_strdup_printf("%s %s", mod, m->key);
      Completion *c = dwb_comp_get_completion_item(&n, m, value);
      gtk_box_pack_start(GTK_BOX(CURRENT_VIEW()->compbox), c->event, false, false, 0);
      list = g_list_append(list, c);
      FREE(value);
      g_free(mod);
    }
  }
  return list;
}/*}}}*/

static void 
dwb_comp_path(void) {
  dwb.state.mode |= COMPLETE_PATH;
  dwb_comp_complete_path(false);
}

/* dwb_comp_complete {{{*/
void 
dwb_comp_complete(CompletionType type, int back) {
  View *v = CURRENT_VIEW();
  dwb.state.mode &= ~(COMPLETE_PATH | AUTO_COMPLETE);
  if ( !(dwb.state.mode & COMPLETION_MODE) ) {
    v->compbox = gtk_vbox_new(true, 0);
    gtk_box_pack_end(GTK_BOX(v->bottombox), v->compbox, false, false, 0);
    switch (type) {
      case COMP_SETTINGS:    dwb.comps.completions = dwb_comp_get_settings_completion(); break;
      case COMP_KEY:         dwb.comps.completions = dwb_comp_get_key_completion(true); break;
      case COMP_COMMAND:     dwb.comps.completions = dwb_comp_get_key_completion(false); break;
      case COMP_BOOKMARK:    dwb.comps.completions = dwb_comp_get_simple_completion(dwb.fc.bookmarks); break;
      case COMP_HISTORY:     dwb.comps.completions = dwb_comp_get_simple_completion(dwb.fc.history); break;
      case COMP_USERSCRIPT:  dwb.comps.completions = dwb_comp_get_simple_completion(dwb.misc.userscripts); break;
      case COMP_INPUT:       dwb.comps.completions = dwb_comp_get_simple_completion(dwb.fc.commands); break;
      case COMP_SEARCH:      dwb.comps.completions = dwb_comp_get_simple_completion(dwb.fc.se_completion); break;
      case COMP_PATH:        dwb_comp_path(); return;
      default:               dwb.comps.completions = dwb_comp_get_normal_completion(); break;
    }
    if (!dwb.comps.completions) {
      return;
    }
    dwb_comp_show_completion(back);
    dwb.state.mode |= COMPLETION_MODE;
  }
  else if (dwb.comps.completions && dwb.comps.active_comp) {
    dwb.comps.active_comp = dwb_comp_update_completion(v->compbox, dwb.comps.completions, dwb.comps.active_comp, dwb.misc.max_c_items, back);
  }
}/*}}}*/
/*}}}*/

/* AUTOCOMPLETION {{{*/
/*dwb_comp_eval_autocompletion{{{*/
void 
dwb_comp_eval_autocompletion() {
  Completion *c = dwb.comps.active_auto_c->data;
  KeyMap *m = c->data;
  dwb_normal_mode(true);
  dwb_com_simple_command(m);
}/*}}}*/

/* dwb_set_autcompletion{{{*/
void 
dwb_comp_set_autcompletion(GList *l, WebSettings *s) {
  dwb.comps.autocompletion = s->arg.b;
}/*}}}*/

/* dwb_comp_cleanautocompletion{{{*/
void 
dwb_comp_clean_autocompletion() {
  for (GList *l = dwb.comps.auto_c; l; l=l->next) {
    FREE(l->data);
  }
  g_list_free(dwb.comps.auto_c);
  gtk_widget_destroy(CURRENT_VIEW()->autocompletion);
  dwb.comps.auto_c = NULL;
  dwb.comps.active_auto_c = NULL;
  dwb.state.mode &= ~AUTO_COMPLETE;

  View *v = CURRENT_VIEW();
  gtk_widget_show(v->entry);
  gtk_widget_show(v->rstatus);
  gtk_widget_show(v->urilabel);

}/*}}}*/

/* dwb_comp_init_autocompletion (GList *gl)      return: GList * (Completion*){{{*/
static GList * 
dwb_comp_init_autocompletion(GList *gl) {
  View *v =  CURRENT_VIEW();
  GList *ret = NULL;

  v->autocompletion = gtk_hbox_new(true, 2);
  int i=0;
  for (GList *l=gl; l; l=l->next, i++) {
    KeyMap *m = l->data;
    if (!m->map->entry) {
      Navigation *n = dwb_navigation_new(m->key, m->map->n.second);
      Completion *c = dwb_comp_get_completion_item(n, m, NULL);
      ret = g_list_append(ret, c);
      if (i<5) {
        gtk_widget_show_all(c->event);
      }
      dwb_navigation_free(n);

      gtk_box_pack_start(GTK_BOX(v->autocompletion), c->event, true,  true, 1);
    }
  }
  GtkWidget *hbox = gtk_bin_get_child(GTK_BIN(v->statusbox));
  gtk_box_pack_start(GTK_BOX(hbox), v->autocompletion, true,  true, 10);
  gtk_widget_hide(v->entry);
  gtk_widget_hide(v->rstatus);
  gtk_widget_hide(v->urilabel);
  gtk_widget_show(v->autocompletion);
  return ret;
}/*}}}*/

/* dwb_autocomplete GList *gl(KeyMap*)  GdkEventKey *{{{*/
void
dwb_comp_autocomplete(GList *gl, GdkEventKey *e) {
  if (!dwb.comps.autocompletion) {
    return;
  }
  View *v = CURRENT_VIEW();

  if (! (dwb.state.mode & AUTO_COMPLETE) && gl) {
    dwb.state.mode |= AUTO_COMPLETE;
    dwb.comps.auto_c = dwb_comp_init_autocompletion(gl);
    dwb.comps.active_auto_c = g_list_first(dwb.comps.auto_c);
    dwb_comp_modify_completion_item(dwb.comps.active_auto_c->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_active);
  }
  else if (e) {
    dwb.comps.active_auto_c = dwb_comp_update_completion(v->autocompletion, dwb.comps.auto_c, dwb.comps.active_auto_c, 5, e->state & GDK_SHIFT_MASK);
  }
}/*}}}*/
/*}}}*/

/* PATHCOMPLETION {{{*/
/* dwb_comp_clean_path_completion {{{*/
void 
dwb_comp_clean_path_completion() {
  if (dwb.comps.path_completion) {
    for (GList *l = g_list_first(dwb.comps.path_completion); l; l=l->next) {
      char *data = l->data;
      FREE(data);
    }
    g_list_free(dwb.comps.path_completion);
    dwb.comps.path_completion = NULL;
    dwb.comps.active_path = NULL;
  }
}/*}}}*/

/* dwb_comp_get_binaries(GList *list, char *text)      return GList *{{{*/
static GList *
dwb_comp_get_binaries(GList *list, char *text) {
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
dwb_comp_get_path(GList *list, char *text) {
  GDir *dir;
  char d_tmp[BUFFER_LENGTH];
  strncpy(d_tmp, text, BUFFER_LENGTH - 1);
  char *d_name = dirname(d_tmp);
  char *b_name = dwb_util_basename(text);
  char *d_current = g_get_current_dir();
  const char *filename;
  char path[BUFFER_LENGTH];

  if (! *text ) {
    list = g_list_prepend(list, g_strconcat(d_current, "/", NULL));
    return list;
  }
  else if (!strcmp(d_name, ".")) {
    dwb_comp_complete(0, 0);
    return NULL;
  }
  else if (g_file_test(text, G_FILE_TEST_IS_DIR)) {
    strncpy(path, text, BUFFER_LENGTH - 1);
    char path_last = path[strlen(path) - 1];
    if (path_last != '/' && path_last != '.') {
      return g_list_prepend(list, g_strconcat(path, "/", NULL));
    }
  }
  else if (g_file_test(d_name, G_FILE_TEST_IS_DIR)) {
    strncpy(path, d_name, BUFFER_LENGTH - 1);
  }
  if ( (dir = g_dir_open(path, 'r', NULL)) ) {
    while ( (filename = g_dir_read_name(dir)) ) {
      if ( ( !b_name && filename[0] != '.') || (b_name && g_str_has_prefix(filename, b_name))) {
        char *newpath = g_build_filename(path, filename, NULL);
        char *store = g_strconcat(newpath, "/" , NULL);
        if (g_file_test(store, G_FILE_TEST_IS_DIR)) {
          list = g_list_prepend(list, store);
        }
        else {
          free(store);
        }
        FREE(newpath);
      }
    }
    g_dir_close(dir);
  }
  return list;
}/*}}}*/


/* dwb_comp_init_path_completion {{{*/
static void
dwb_comp_init_path_completion(int back) { 
  char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  dwb.comps.path_completion = dwb.comps.active_path = g_list_append(NULL, g_strdup(text));
  if (dwb.state.dl_action == DL_ACTION_EXECUTE) {
    GList *list = dwb_comp_get_binaries(NULL, text);
    list = g_list_sort(list, (GCompareFunc)strcmp);
    dwb.comps.path_completion = g_list_concat(dwb.comps.path_completion, list);
  }
  else  {
    dwb.comps.path_completion = dwb_comp_get_path(dwb.comps.path_completion, text);
    dwb.comps.path_completion = g_list_sort(dwb.comps.path_completion, (GCompareFunc)strcmp);
  }
  if (g_list_length(dwb.comps.path_completion) == 1) {
    dwb_comp_clean_path_completion();
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

/* dwb_comp_complete_download{{{*/
void
dwb_comp_complete_path(int back) {
  if (! dwb.comps.path_completion ) {
    dwb_comp_init_path_completion(0);
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
    dwb_entry_set_text(dwb.comps.active_path->data);
  }
}/*}}}*/
/*}}}*/
