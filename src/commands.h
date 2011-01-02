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

void dwb_com_simple_command(KeyMap *km);
gboolean dwb_com_add_search_field(Arg *);
gboolean dwb_com_allow_content(Arg *a);
gboolean dwb_com_allow_cookie(Arg *);
gboolean dwb_com_allow_plugins(Arg *a);
gboolean dwb_com_bookmark(Arg *);
gboolean dwb_com_bookmarks(Arg *);
gboolean dwb_com_entry_delete_letter(Arg *);
gboolean dwb_com_entry_delete_line(Arg *);
gboolean dwb_com_entry_delete_word(Arg *);
gboolean dwb_com_entry_history_back(Arg *);
gboolean dwb_com_entry_history_forward(Arg *);
gboolean dwb_com_entry_word_back(Arg *);
gboolean dwb_com_entry_word_forward(Arg *);
gboolean dwb_com_execute_userscript(Arg *);
gboolean dwb_com_find(Arg *);
gboolean dwb_com_focus_input(Arg *);
gboolean dwb_com_focus_next(Arg *);
gboolean dwb_com_focus_nth_view(Arg *);
gboolean dwb_com_focus_prev(Arg *);
gboolean dwb_com_complete_type(Arg *);
gboolean dwb_com_history_back(Arg *);
gboolean dwb_com_history_forward(Arg *);
gboolean dwb_com_new_window_or_view(Arg *);
gboolean dwb_com_open(Arg *);
gboolean dwb_com_open_startpage(Arg *);
gboolean dwb_com_paste(Arg *);
gboolean dwb_com_print(Arg *);
gboolean dwb_com_push_master(Arg *);
gboolean dwb_com_quickmark(Arg *);
gboolean dwb_com_reload(Arg *);
gboolean dwb_com_resize_master(Arg *);
gboolean dwb_com_save_files(Arg *);
gboolean dwb_com_save_session(Arg *);
gboolean dwb_com_scroll(Arg *);
gboolean dwb_com_set_key(Arg *);
gboolean dwb_com_set_orientation(Arg *);
gboolean dwb_com_set_setting(Arg *);
gboolean dwb_com_show_hints(Arg *);
gboolean dwb_com_show_keys(Arg *);
gboolean dwb_com_show_settings(Arg *);
gboolean dwb_com_toggle_block_content(Arg *a);
gboolean dwb_com_toggle_custom_encoding(Arg *);
gboolean dwb_com_toggle_property(Arg *); 
gboolean dwb_com_toggle_proxy(Arg *); 
gboolean dwb_com_undo(Arg *);
gboolean dwb_com_view_source(Arg *);
gboolean dwb_com_yank(Arg *);
gboolean dwb_com_zoom_in(Arg *);
gboolean dwb_com_zoom_out(Arg *);
gboolean dwb_create_hints(Arg *);
void dwb_com_remove_view(Arg *);
void dwb_com_set_zoom_level(Arg *);
void dwb_com_toggle_maximized(Arg *);
#endif
