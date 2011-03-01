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

#ifndef COMMANDS_H
#define COMMANDS_H

#include "dwb.h"
#include "commands.h"
#include "completion.h"
#include "util.h"
#include "view.h"
#include "session.h"
#include "soup.h"

void dwb_com_simple_command(KeyMap *km);
gboolean dwb_com_add_search_field(KeyMap *, Arg *);
void dwb_com_add_view(KeyMap *, Arg *);
gboolean dwb_com_allow_cookie(KeyMap *, Arg *);
gboolean dwb_com_bookmark(KeyMap *, Arg *);
gboolean dwb_com_bookmarks(KeyMap *, Arg *);
gboolean dwb_com_entry_delete_letter(KeyMap *, Arg *);
gboolean dwb_com_entry_delete_line(KeyMap *, Arg *);
gboolean dwb_com_entry_delete_word(KeyMap *, Arg *);
gboolean dwb_com_entry_history_back(KeyMap *, Arg *);
gboolean dwb_com_entry_history_forward(KeyMap *, Arg *);
gboolean dwb_com_entry_word_back(KeyMap *, Arg *);
gboolean dwb_com_entry_word_forward(KeyMap *, Arg *);
gboolean dwb_com_execute_userscript(KeyMap *, Arg *);
gboolean dwb_com_find(KeyMap *, Arg *);
gboolean dwb_com_focus_input(KeyMap *, Arg *);
gboolean dwb_com_focus_next(KeyMap *, Arg *);
gboolean dwb_com_focus_nth_view(KeyMap *, Arg *);
gboolean dwb_com_focus_prev(KeyMap *, Arg *);
gboolean dwb_com_complete_type(KeyMap *, Arg *);
gboolean dwb_com_history_back(KeyMap *, Arg *);
gboolean dwb_com_history_forward(KeyMap *, Arg *);
gboolean dwb_com_new_window_or_view(KeyMap *, Arg *);
gboolean dwb_com_open(KeyMap *, Arg *);
gboolean dwb_com_open_startpage(KeyMap *, Arg *);
gboolean dwb_com_paste(KeyMap *, Arg *);
gboolean dwb_com_print(KeyMap *, Arg *);
gboolean dwb_com_push_master(KeyMap *, Arg *);
gboolean dwb_com_quickmark(KeyMap *, Arg *);
gboolean dwb_com_reload(KeyMap *, Arg *);
gboolean dwb_com_resize_master(KeyMap *, Arg *);
gboolean dwb_com_save_files(KeyMap *, Arg *);
gboolean dwb_com_save_session(KeyMap *, Arg *);
gboolean dwb_com_scroll(KeyMap *, Arg *);
gboolean dwb_com_set_key(KeyMap *, Arg *);
gboolean dwb_com_set_orientation(KeyMap *, Arg *);
gboolean dwb_com_set_setting(KeyMap *, Arg *);
gboolean dwb_com_show_hints(KeyMap *, Arg *);
gboolean dwb_com_show_keys(KeyMap *, Arg *);
gboolean dwb_com_show_settings(KeyMap *, Arg *);
gboolean dwb_com_toggle_scripts(KeyMap *, Arg *a);
gboolean dwb_com_toggle_property(KeyMap *, Arg *); 
gboolean dwb_com_toggle_proxy(KeyMap *, Arg *); 
gboolean dwb_com_undo(KeyMap *, Arg *);
gboolean dwb_com_view_source(KeyMap *, Arg *);
gboolean dwb_com_yank(KeyMap *, Arg *);
gboolean dwb_com_zoom_in(KeyMap *, Arg *);
gboolean dwb_com_zoom_out(KeyMap *, Arg *);
gboolean dwb_create_hints(Arg *);
void dwb_com_remove_view(KeyMap *, Arg *);
void dwb_com_set_zoom_level(KeyMap *, Arg *);
void dwb_com_toggle_maximized(KeyMap *, Arg *);
gboolean dwb_com_toggle_hidden_files(KeyMap *, Arg *);
#endif
