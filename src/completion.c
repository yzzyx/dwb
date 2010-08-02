#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "dwb.h"
#include "completion.h"
#include "commands.h"
/* COMPLETION {{{*/
GList * dwb_update_completion(GtkWidget *box, GList *comps, GList *active, gint max, gint back);


/* dwb_clean_completion() {{{*/
void 
dwb_clean_completion() {
  for (GList *l = dwb.comps.completions; l; l=l->next) {
    g_free(l->data);
  }
  g_list_free(dwb.comps.completions);
  gtk_widget_destroy(CURRENT_VIEW()->compbox);
  dwb.comps.completions = NULL;
  dwb.comps.active_comp = NULL;
  dwb.state.mode &= ~CompletionMode;
}/*}}}*/

/* dwb_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {{{*/
void 
dwb_modify_completion_item(Completion *c, GdkColor *fg, GdkColor *bg, PangoFontDescription  *fd) {
  gtk_widget_modify_fg(c->llabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_fg(c->rlabel, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_bg(c->event, GTK_STATE_NORMAL, bg);
  gtk_widget_modify_font(c->llabel, fd);
  gtk_widget_modify_font(c->rlabel, dwb.font.fd_oblique);
}/*}}}*/

/* dwb_get_completion_item(Navigation *)      return: Completion * {{{*/
Completion * 
dwb_get_completion_item(Navigation *n, void *data) {
  Completion *c = g_malloc(sizeof(Completion));

  c->rlabel = gtk_label_new(n->second);
  c->llabel = gtk_label_new(n->first);
  c->event = gtk_event_box_new();
  c->data = data;
  GtkWidget *hbox = gtk_hbox_new(false, 0);
  gtk_box_pack_start(GTK_BOX(hbox), c->llabel, true, true, 5);
  gtk_box_pack_start(GTK_BOX(hbox), c->rlabel, true, true, 5);

  gtk_misc_set_alignment(GTK_MISC(c->llabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(c->rlabel), 1.0, 0.5);

  dwb_modify_completion_item(c, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_normal);

  gtk_container_add(GTK_CONTAINER(c->event), hbox);
  return c;
}/*}}}*/

/* dwb_init_completion {{{*/
GList * 
dwb_init_completion(GList *store, GList *gl) {
  Navigation *n;
  const gchar *input = GET_TEXT();
  for (GList *l = gl; l; l=l->next) {
    n = l->data;
    if (g_strrstr(n->first, input)) {
      Completion *c = dwb_get_completion_item(n, NULL);
      gtk_box_pack_start(GTK_BOX(CURRENT_VIEW()->compbox), c->event, false, false, 0);
      store = g_list_prepend(store, c);
    }
  }
  return store;
}/*}}}*/

/* dwb_completion_set_text(Completion *) {{{*/
void
dwb_completion_set_entry_text(Completion *c) {
  const gchar *text = gtk_label_get_text(GTK_LABEL(c->llabel));
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);

}/*}}}*/

/*dwb_eval_autocompletion{{{*/
void 
dwb_eval_autocompletion() {
  Completion *c = dwb.comps.active_auto_c->data;
  KeyMap *m = c->data;
  dwb_normal_mode(true);
  dwb_simple_command(m);
}/*}}}*/

/* dwb_set_autcompletion{{{*/
void 
dwb_set_autcompletion(GList *l, WebSettings *s) {
  dwb.comps.autocompletion = s->arg.b;
}/*}}}*/

/* dwb_clean_autocompletion{{{*/
void 
dwb_clean_autocompletion() {
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

/* dwb_init_autocompletion (GList *gl)      return: GList * (Completion*){{{*/
GList * 
dwb_init_autocompletion(GList *gl) {
  View *v =  CURRENT_VIEW();
  GList *ret = NULL;

  v->autocompletion = gtk_hbox_new(true, 2);
  gint i=0;
  for (GList *l=gl; l; l=l->next, i++) {
    KeyMap *m = l->data;
    Navigation *n = dwb_navigation_new(m->key, m->map->n.second);
    Completion *c = dwb_get_completion_item(n, m);
    ret = g_list_append(ret, c);
    if (i<5) {
      gtk_widget_show_all(c->event);
    }
    dwb_navigation_free(n);

    gtk_box_pack_start(GTK_BOX(v->autocompletion), c->event, true,  true, 1);
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
dwb_autocomplete(GList *gl, GdkEventKey *e) {
  if (!dwb.comps.autocompletion) {
    return;
  }
  View *v = CURRENT_VIEW();

  if (! (dwb.state.mode & AutoComplete) && gl) {
    dwb.state.mode |= AutoComplete;
    dwb.comps.auto_c = dwb_init_autocompletion(gl);
    dwb.comps.active_auto_c = g_list_first(dwb.comps.auto_c);
    dwb_modify_completion_item(dwb.comps.active_auto_c->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_bold);
  }
  else if (e) {
    dwb.comps.active_auto_c = dwb_update_completion(v->autocompletion, dwb.comps.auto_c, dwb.comps.active_auto_c, 5, e->state & GDK_CONTROL_MASK);
  }
}/*}}}*/

/* dwb_update_completion(GtkWidget *box, GList *comps, GList *active, gint max, gint back)    Return *GList (Completions*){{{*/
GList *
dwb_update_completion(GtkWidget *box, GList *comps, GList *active, gint max, gint back) {
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
    if (position > offset   &&  position < items - offset - 1 + r) {
      c = g_list_nth(comps, position - offset - 1)->data;
      gtk_widget_show_all(c->event);
      c = g_list_nth(comps, position + offset + 1 - r)->data;
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
  dwb_modify_completion_item(old->data, &dwb.color.normal_c_fg, &dwb.color.normal_c_bg, dwb.font.fd_normal);
  dwb_modify_completion_item(new->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_bold);
  active = new;
  dwb_completion_set_entry_text(active->data);
  return active;
}/*}}}*/

void 
dwb_show_completion(gint back) {
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
  dwb_modify_completion_item(dwb.comps.active_comp->data, &dwb.color.active_c_fg, &dwb.color.active_c_bg, dwb.font.fd_bold);
  dwb_completion_set_entry_text(dwb.comps.active_comp->data);
  gtk_widget_show(CURRENT_VIEW()->compbox);

}

/* dwb_completion_get_normal      return: GList *Completions{{{*/
GList *
dwb_completion_get_normal() {
  GList *list = NULL;
  list = dwb_init_completion(list, dwb.fc.history);
  list = dwb_init_completion(list, dwb.fc.bookmarks);
  list = dwb_init_completion(list, dwb.fc.commands);
  return  list;
}/*}}}*/

/* dwb_completion_get_settings      return: GList *Completions{{{*/
GList *
dwb_completion_get_settings() {
  GList *l = g_hash_table_get_values(dwb.settings);
  l = g_list_sort(l, (GCompareFunc)dwb_web_settings_sort_first);
  GList *data = NULL;
  GList *list = NULL;

  for (; l; l=l->next) {
    WebSettings *s = l->data;
    if (dwb.state.setting_apply == Global || !s->global) {
      data = g_list_prepend(data, &s->n);
    }
  }
  list = dwb_init_completion(list, data);
  g_list_free(data);
  return list;
}/*}}}*/

/*dwb_completion_get_keys()         return  GList *Completions{{{*/
GList * 
dwb_completion_get_keys() {
  GList *data = NULL;
  GList *list = NULL;
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    data  = g_list_prepend(data,  &m->map->n);
  }
  //list = g_list_sort(list (GCompareFunc))<++>
  list = dwb_init_completion(list, data);
  g_list_free(data);
  return list;
}/*}}}*/


void 
dwb_complete(gint back) {
  View *v = CURRENT_VIEW();
  if ( !(dwb.state.mode & CompletionMode) ) {
    v->compbox = gtk_vbox_new(true, 0);
    gtk_box_pack_end(GTK_BOX(v->bottombox), v->compbox, false, false, 0);
    switch (dwb.state.mode) {
      case OpenMode:      dwb.comps.completions = dwb_completion_get_normal(); break;
      case SettingsMode:  dwb.comps.completions = dwb_completion_get_settings(); break;
      case KeyMode:       dwb.comps.completions = dwb_completion_get_keys(); break;
      default: break;
    }
    if (!dwb.comps.completions) {
      return;
    }
    dwb_show_completion(back);
    dwb.state.mode |= CompletionMode;
  }
  else if (dwb.comps.completions && dwb.comps.active_comp) {
    dwb.comps.active_comp = dwb_update_completion(v->compbox, dwb.comps.completions, dwb.comps.active_comp, dwb.misc.max_c_items, back);
  }
}
