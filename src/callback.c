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

#include <string.h>
#include "dwb.h"
#include "completion.h"


/* dwb_entry_keyrelease_cb {{{*/
gboolean 
callback_entry_insert_text(GtkWidget* entry, char *new_text, int length, gpointer position) { 
  const char *text = GET_TEXT();
  int newlen = strlen(text) + length + 1;
  char buffer[newlen];
  snprintf(buffer, newlen, "%s%s", text, new_text);
  if (dwb.state.mode == QUICK_MARK_OPEN) {
    return dwb_update_find_quickmark(buffer);
  }
  return false;
}
gboolean 
callback_entry_key_release(GtkWidget* entry, GdkEventKey *e) { 
  if (dwb.state.mode == HINT_MODE) {
    if (e->keyval == GDK_KEY_BackSpace) {
      return dwb_update_hints(e);
    }
  }
  if (dwb.state.mode == FIND_MODE) {
    dwb_update_search(dwb.state.forward_search);
  }
  return false;
}/*}}}*/

/* dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {{{*/
gboolean 
callback_entry_key_press(GtkWidget* entry, GdkEventKey *e) {
  Mode mode = dwb.state.mode;
  gboolean ret = false;
  gboolean complete = (mode == DOWNLOAD_GET_PATH || (mode & COMPLETE_PATH));
  gboolean set_text = false;
  if (dwb.state.mode & QUICK_MARK_OPEN) 
    set_text = true;
  /*  Handled by activate-callback */
  if (e->keyval == GDK_KEY_Return)
    return dwb_entry_activate(e);
  if (mode == QUICK_MARK_SAVE) 
    return false;
  else if (mode & COMPLETE_BUFFER) {
    completion_buffer_key_press(e);
    return true;
  }
  else if (e->keyval == GDK_KEY_BackSpace && !complete) {
    return false;
  }
  else if (mode == HINT_MODE) {
    return dwb_update_hints(e);
  }
  else if (mode == SEARCH_FIELD_MODE) {
    if (DWB_TAB_KEY(e)) {
      dwb_update_hints(e);
      return true;
    }
    else if (e->keyval == GDK_KEY_Return) {
      return false;
    }
  }
  else if (!e->is_modifier && complete) {
    if (DWB_TAB_KEY(e)) {
      completion_complete_path(e->state & GDK_SHIFT_MASK);
      return true;
    }
    else {
      completion_clean_path_completion();
    }
  }
  else if (mode & COMPLETION_MODE && !DWB_TAB_KEY(e) && !e->is_modifier && !CLEAN_STATE(e)) {
    completion_clean_completion(set_text);
  }
  else if (mode == FIND_MODE) {
    return false;
  }
  else if (DWB_TAB_KEY(e)) {
    completion_complete(dwb_eval_completion_type(), e->state & GDK_SHIFT_MASK);
    return true;
  }
  if (dwb_eval_editing_key(e)) {
    ret = true;
  }
  return ret;
}/*}}}*/

/* callback_tab_size {{{*/
void 
callback_tab_size(GtkWidget *w, GdkRectangle *a, GList *gl) {
  dwb.misc.tab_height = a->height;
  g_signal_handlers_disconnect_by_func(w, callback_tab_size, NULL);
}/*}}}*/

/* callback_tab_size {{{*/
void 
callback_entry_size(GtkWidget *w, GdkRectangle *a) {
  dwb.misc.bar_height = a->height;
  gtk_widget_set_size_request(dwb.gui.entry, -1, a->height);
  g_signal_handlers_disconnect_by_func(w, callback_entry_size, NULL);
}/*}}}*/
