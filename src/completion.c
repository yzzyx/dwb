#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "dwb.h"
#include "completion.h"
#include "commands.h"
#include "util.h"

static GList * dwb_comp_update_completion(GtkWidget *box, GList *comps, GList *active, gint max, gint back);

typedef gboolean (*Match_Func)(gchar*, const gchar*);

/* GUI_FUNCTIONS {{{*/
/* dwb_comp_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {{{*/
static void 
dwb_comp_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {
  gtk_widget_modify_fg(c->llabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_fg(c->rlabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_fg(c->mlabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_bg(c->event, GTK_STATE_NORMAL, bg);
  gtk_widget_modify_font(c->llabel, fd);
  gtk_widget_modify_font(c->mlabel, fd);
  gtk_widget_modify_font(c->rlabel, dwb.font.fd_oblique);
}/*}}}*/

/* dwb_comp_get_completion_item(Navigation *)      return: Completion * {{{*/
static Completion * 
dwb_comp_get_completion_item(Navigation *n, void *data, const gchar *value) {
  Completion *c = g_malloc(sizeof(Completion));

  c->rlabel = gtk_label_new(n->second);
  c->llabel = gtk_label_new(n->first);
  c->mlabel = gtk_label_new(value);
  c->event = gtk_event_box_new();
  c->data = data;
  GtkWidget *hbox = gtk_hbox_new(value ? true :false, 0);



  gtk_box_pack_start(GTK_BOX(hbox), c->llabel, true, true, 5);
  gtk_box_pack_start(GTK_BOX(hbox), c->mlabel, value ? true : false , true, 5);
  gtk_box_pack_start(GTK_BOX(hbox), c->rlabel, true, true, 5);

  gtk_label_set_ellipsize(GTK_LABEL(c->llabel), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_ellipsize(GTK_LABEL(c->rlabel), PANGO_ELLIPSIZE_MIDDLE);

  gtk_misc_set_alignment(GTK_MISC(c->llabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(c->mlabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(c->rlabel), 1.0, 0.5);

  dwb_comp_modify_completion_item(c, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_normal);

  gtk_container_add(GTK_CONTAINER(c->event), hbox);
  return c;
}/*}}}*/

/* dwb_comp_init_completion {{{*/
static GList * 
dwb_comp_init_completion(GList *store, GList *gl, gboolean word_beginnings) {
  Navigation *n;
  const gchar *input = GET_TEXT();
  Match_Func func = word_beginnings ? (Match_Func)g_str_has_prefix : (Match_Func)g_strrstr;

  for (GList *l = gl; l; l=l->next) {
    n = l->data;
    if (func(n->first, input)) {
      Completion *c = dwb_comp_get_completion_item(n, NULL, NULL);
      gtk_box_pack_start(GTK_BOX(CURRENT_VIEW()->compbox), c->event, false, false, 0);
      store = g_list_append(store, c);
    }
  }
  return store;
}/*}}}*/

/* dwb_completion_set_text(Completion *) {{{*/
static void
dwb_comp_set_entry_text(Completion *c) {
  const gchar *text = gtk_label_get_text(GTK_LABEL(c->llabel));
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);

}/*}}}*/

/* dwb_comp_update_completion(GtkWidget *box, GList *comps, GList *active, gint max, gint back)    Return *GList (Completions*){{{*/
static GList *
dwb_comp_update_completion(GtkWidget *box, GList *comps, GList *active, gint max, gint back) {
  GList *old, *new;
  Completion *c;

  gint length = g_list_length(comps);
  gint items = MAX(length, max);
  gint r = (max) % 2;
  gint offset = max / 2 - 1 + r;

  old = active;
  gint position = g_list_position(comps, active) + 1;
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
        gtk_widget_hide_all(box);
        gtk_widget_show(box);
        int i = 0;
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
        gtk_widget_hide_all(box);
        gtk_widget_show(box);
        int i = 0;
        for (GList *l = g_list_last(comps); l && i<max ;l=l->prev, i++) {
          c = l->data;
          gtk_widget_show_all(c->event);
        }
      }
    }
  }
  dwb_comp_modify_completion_item(old->data, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_normal);
  dwb_comp_modify_completion_item(new->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_bold);
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
    g_free(l->data);
  }
  g_list_free(dwb.comps.completions);
  gtk_widget_destroy(CURRENT_VIEW()->compbox);
  dwb.comps.completions = NULL;
  dwb.comps.active_comp = NULL;
  dwb.state.mode &= ~CompletionMode;
}/*}}}*/

/* dwb_comp_show_completion(gint back) {{{*/
static void 
dwb_comp_show_completion(gint back) {
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
  dwb_comp_modify_completion_item(dwb.comps.active_comp->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_bold);
  dwb_comp_set_entry_text(dwb.comps.active_comp->data);
  gtk_widget_show(CURRENT_VIEW()->compbox);

}/*}}}*/

/* dwb_completion_get_normal      return: GList *Completions{{{*/
static GList *
dwb_comp_get_normal_completion() {
  GList *list = NULL;
  list = dwb_comp_init_completion(list, dwb.fc.commands, true);
  list = dwb_comp_init_completion(list, dwb.fc.bookmarks, false);
  if (GET_BOOL("complete-history")) {
    list = dwb_comp_init_completion(list, dwb.fc.history, false);
  }
  return  list;
}/*}}}*/

/* dwb_comp_get_bookmark_completion      return: GList *Completions{{{*/
static GList *
dwb_comp_get_bookmark_completion() {
  GList *list = NULL;
  list = dwb_comp_init_completion(list, dwb.fc.bookmarks, false);
  return  list;
}/*}}}*/

/* dwb_completion_get_settings      return: GList *Completions{{{*/
static GList *
dwb_comp_get_settings_completion() {
  GList *l = g_hash_table_get_values(dwb.state.setting_apply == Global ? dwb.settings : CURRENT_VIEW()->setting);
  l = g_list_sort(l, (GCompareFunc)dwb_util_web_settings_sort_first);
  const gchar *input = GET_TEXT();
  GList *list = NULL;

  for (; l; l=l->next) {
    WebSettings *s = l->data;
    if (dwb.state.setting_apply == Global || !s->global) {
      Navigation n = s->n;
      if (g_str_has_prefix(n.first, input)) {
        gchar *value = dwb_util_arg_to_char(&s->arg, s->type);
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
  const gchar *input = GET_TEXT();

  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)dwb_util_keymap_sort_first);

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    if (!entry && m->map->entry) {
      continue;
    }
    Navigation n = m->map->n;
    if (g_str_has_prefix(n.first, input)) {
      gchar *value = g_strdup_printf("%s %s", dwb_modmask_to_string(m->mod), m->key);
      Completion *c = dwb_comp_get_completion_item(&n, m, value);
      gtk_box_pack_start(GTK_BOX(CURRENT_VIEW()->compbox), c->event, false, false, 0);
      list = g_list_append(list, c);
      g_free(value);
    }
  }
  return list;
}/*}}}*/

/* dwb_comp_complete {{{*/
void 
dwb_comp_complete(gint back) {
  View *v = CURRENT_VIEW();
  if ( !(dwb.state.mode & CompletionMode) ) {
    v->compbox = gtk_vbox_new(true, 0);
    gtk_box_pack_end(GTK_BOX(v->bottombox), v->compbox, false, false, 0);
    switch (dwb.state.mode) {
      case SettingsMode:  dwb.comps.completions = dwb_comp_get_settings_completion(); break;
      case KeyMode:       dwb.comps.completions = dwb_comp_get_key_completion(true); break;
      case CommandMode:   dwb.comps.completions = dwb_comp_get_key_completion(false); break;
      case BookmarksMode: dwb.comps.completions = dwb_comp_get_bookmark_completion(); break;
      default:            dwb.comps.completions = dwb_comp_get_normal_completion(); break;
    }
    if (!dwb.comps.completions) {
      return;
    }
    dwb_comp_show_completion(back);
    dwb.state.mode |= CompletionMode;
  }
  else if (dwb.comps.completions && dwb.comps.active_comp) {
    dwb.comps.active_comp = dwb_comp_update_completion(v->compbox, dwb.comps.completions, dwb.comps.active_comp, dwb.misc.max_c_items, back);
  }
  if (dwb.state.mode == SettingsMode || dwb.state.mode == KeyMode) {

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
    g_free(l->data);
  }
  g_list_free(dwb.comps.auto_c);
  gtk_widget_destroy(CURRENT_VIEW()->autocompletion);
  dwb.comps.auto_c = NULL;
  dwb.comps.active_auto_c = NULL;
  dwb.state.mode &= ~AutoComplete;

  View *v = CURRENT_VIEW();
  gtk_widget_show(v->entry);
  gtk_widget_show(v->rstatus);

}/*}}}*/

/* dwb_comp_init_autocompletion (GList *gl)      return: GList * (Completion*){{{*/
static GList * 
dwb_comp_init_autocompletion(GList *gl) {
  View *v =  CURRENT_VIEW();
  GList *ret = NULL;

  v->autocompletion = gtk_hbox_new(true, 2);
  gint i=0;
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

  if (! (dwb.state.mode & AutoComplete) && gl) {
    dwb.state.mode |= AutoComplete;
    dwb.comps.auto_c = dwb_comp_init_autocompletion(gl);
    dwb.comps.active_auto_c = g_list_first(dwb.comps.auto_c);
    dwb_comp_modify_completion_item(dwb.comps.active_auto_c->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_bold);
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
    for (GList *l = dwb.comps.path_completion; l; l=l->next) {
      g_free(l->data);
    }
    g_list_free(dwb.comps.path_completion);
    dwb.comps.path_completion = dwb.comps.active_path = NULL;
  }
}/*}}}*/

/* dwb_comp_get_binaries(GList *list, gchar *text)      return GList *{{{*/
static GList *
dwb_comp_get_binaries(GList *list, gchar *text) {
  GDir *dir;
  gchar **paths = g_strsplit(g_getenv("PATH"), ":", -1);
  gint i=0;
  gchar *path;
  const gchar *filename;

  while ( (path = paths[i++]) ) {
    if ( (dir = g_dir_open(path, 'r', NULL)) ) {
      while ( (filename = g_dir_read_name(dir))) {
        if (g_str_has_prefix(filename, text)) {
          gchar *store = g_build_filename(path, filename, NULL);
          list = g_list_prepend(list, store);
        }
      }
      g_dir_close(dir);
    }
  }
  return list;
}/* }}} */
/* dwb_comp_get_path(GList *, gchar *)        return GList* */
static GList *
dwb_comp_get_path(GList *list, gchar *text) {
  gchar *path = "/";
  gchar *last = "";
  if (text && strlen(text)) {
    gchar *tmp = strrchr(text, '/'); 
    tmp++;
    last = g_strdup(tmp);
    memset(tmp, '\0', sizeof(tmp));
    path = text;
  }

  GDir *dir;
  const gchar *filename;

  if ( (dir = g_dir_open(path, 'r', NULL)) ) {
    while ( (filename = g_dir_read_name(dir)) ) {
      // ignore hidden files
      if (!strlen(last) && filename[0] == '.') 
        continue;
      if (g_str_has_prefix(filename, last)) {
        gchar *newpath = g_build_filename(path, filename, NULL);
        gchar *store = g_strconcat(newpath, g_file_test(newpath, G_FILE_TEST_IS_DIR) ? "/" : "", NULL);
        list = g_list_prepend(list, store);
        g_free(newpath);
      }
    }
    g_dir_close(dir);
  }
  return list;
}/*}}}*/

/* dwb_comp_init_path_completion {{{*/
static void
dwb_comp_init_path_completion(gint back) { 
  gchar *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  dwb.comps.path_completion = g_list_append(NULL, g_strdup(text));
  if (dwb.state.dl_action == Execute && text[0] != '/') {
    GList *list = dwb_comp_get_binaries(NULL, text);
    list = g_list_sort(list, (GCompareFunc)strcmp);
    dwb.comps.path_completion = g_list_concat(dwb.comps.path_completion, list);
  }
  else  {
    if (text[0] == '/') {
      dwb.comps.path_completion = dwb_comp_get_path(dwb.comps.path_completion, text);
      dwb.comps.path_completion = g_list_sort(dwb.comps.path_completion, (GCompareFunc)strcmp);
    }
  }
  if (dwb.comps.path_completion) {
    if (back) {
      dwb.comps.active_path = g_list_last(dwb.comps.path_completion);
    }
    else {
      dwb.comps.active_path = dwb.comps.path_completion->next;
    }
  }
}/*}}}*/

/* dwb_comp_complete_download{{{*/
void
dwb_comp_complete_download(gint back) {
  if (! dwb.comps.path_completion ) {
    dwb_comp_init_path_completion(back);
  }
  else if (back) {
    if (dwb.comps.active_path && !(dwb.comps.active_path = dwb.comps.active_path->prev) ) {
      dwb.comps.active_path = g_list_last(dwb.comps.path_completion);
    }
    if (dwb.comps.active_path) {
      dwb_entry_set_text(dwb.comps.active_path->data);
    }
  }
  else if (dwb.comps.active_path && !(dwb.comps.active_path = dwb.comps.active_path->next) ) {
    dwb.comps.active_path = dwb.comps.path_completion;
  }
  if (dwb.comps.active_path) {
    dwb_entry_set_text(dwb.comps.active_path->data);
  }
}/*}}}*/
/*}}}*/
