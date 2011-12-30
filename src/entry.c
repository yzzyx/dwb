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

#include "dwb.h"
#include "entry.h"
static char *_store;
/* dwb_entry_history_forward {{{*/
DwbStatus
entry_history_forward(GList **last) {
  const char *text = NULL;
  GList *prev = NULL;
  if (*last != NULL) {
    if ((*last)->prev == NULL) {
      text = _store;
    }
    else {
      prev = (*last)->prev;
      text = prev->data;
    }
  }
  *last = prev;
  if (text != NULL) {
    entry_set_text(text);
  }
  return STATUS_OK;
}/*}}}*/

/* entry_history_back(GList **list, GList **last) {{{ */
DwbStatus
entry_history_back(GList **list, GList **last) {
  char *text = NULL;
  if (*list == NULL)
    return STATUS_ERROR;

  GList *next;
  if (*last == NULL) {
    next = *list;
    FREE(_store);
    _store = g_strdup(GET_TEXT());
  }
  else if ((*last)->next != NULL)
    next = (*last)->next;
  else 
    return STATUS_OK;

  *last = next;
  if (next) 
    text = next->data;
  if (text && *text) {
    entry_set_text(text);
  }
  return STATUS_OK;
} /* }}} */

/* entry_focus() {{{*/
void 
entry_focus() {
  if (! (dwb.state.bar_visible & BAR_VIS_STATUS)) {
    gtk_widget_show(dwb.gui.statusbox);
  }
  gtk_widget_show(dwb.gui.entry);
  gtk_widget_grab_focus(dwb.gui.entry);
  gtk_widget_set_can_focus(CURRENT_WEBVIEW_WIDGET(), false);
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  dwb.state.last_com_history = NULL;
  dwb.state.last_nav_history = NULL;
}/*}}}*/

/* entry_set_text(const char *) {{{*/
void 
entry_set_text(const char *text) {
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);
}/*}}}*/

/* entry_move_cursor_step(GtkMovementStep, int step, gboolean delete)  {{{*/
void 
entry_move_cursor_step(GtkMovementStep step, int stepcount, gboolean del) {
  g_signal_emit_by_name(dwb.gui.entry, "move-cursor", step, stepcount, del);
  if (del)
    gtk_editable_delete_selection(GTK_EDITABLE(dwb.gui.entry));
}/*}}}*/
