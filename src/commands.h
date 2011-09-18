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
#include "html.h"

void commands_simple_command(KeyMap *km);
gboolean commands_add_search_field(KeyMap *, Arg *);
void commands_add_view(KeyMap *, Arg *);
gboolean commands_allow_cookie(KeyMap *, Arg *);
gboolean commands_bookmark(KeyMap *, Arg *);
gboolean commands_bookmarks(KeyMap *, Arg *);
gboolean commands_entry_delete_letter(KeyMap *, Arg *);
gboolean commands_entry_delete_line(KeyMap *, Arg *);
gboolean commands_entry_delete_word(KeyMap *, Arg *);
gboolean commands_entry_history_back(KeyMap *, Arg *);
gboolean commands_entry_history_forward(KeyMap *, Arg *);
gboolean commands_entry_word_back(KeyMap *, Arg *);
gboolean commands_entry_word_forward(KeyMap *, Arg *);
gboolean commands_execute_userscript(KeyMap *, Arg *);
gboolean commands_find(KeyMap *, Arg *);
gboolean commands_focus_input(KeyMap *, Arg *);
gboolean commands_focus(KeyMap *, Arg *);
gboolean commands_focus_nth_view(KeyMap *, Arg *);
gboolean commands_complete_type(KeyMap *, Arg *);
gboolean commands_history_back(KeyMap *, Arg *);
gboolean commands_history_forward(KeyMap *, Arg *);
gboolean commands_new_window_or_view(KeyMap *, Arg *);
gboolean commands_open(KeyMap *, Arg *);
gboolean commands_open_startpage(KeyMap *, Arg *);
gboolean commands_paste(KeyMap *, Arg *);
gboolean commands_print(KeyMap *, Arg *);
gboolean commands_push_master(KeyMap *, Arg *);
gboolean commands_quickmark(KeyMap *, Arg *);
gboolean commands_reload(KeyMap *, Arg *);
gboolean commands_reload_bypass_cache(KeyMap *, Arg *);
gboolean commands_resize_master(KeyMap *, Arg *);
gboolean commands_save_files(KeyMap *, Arg *);
gboolean commands_save_session(KeyMap *, Arg *);
gboolean commands_scroll(KeyMap *, Arg *);
gboolean commands_set_key(KeyMap *, Arg *);
gboolean commands_set_orientation(KeyMap *, Arg *);
gboolean commands_set_setting(KeyMap *, Arg *);
gboolean commands_show_hints(KeyMap *, Arg *);
gboolean commands_show_keys(KeyMap *, Arg *);
gboolean commands_show_settings(KeyMap *, Arg *);
gboolean commands_toggle_scripts(KeyMap *, Arg *a);
gboolean commands_toggle_plugin_blocker(KeyMap *, Arg *a);
gboolean commands_toggle_property(KeyMap *, Arg *); 
gboolean commands_toggle_proxy(KeyMap *, Arg *); 
gboolean commands_undo(KeyMap *, Arg *);
gboolean commands_view_source(KeyMap *, Arg *);
gboolean commands_yank(KeyMap *, Arg *);
gboolean commands_zoom_in(KeyMap *, Arg *);
gboolean commands_zoom_out(KeyMap *, Arg *);
gboolean dwb_create_hints(Arg *);
void commands_remove_view(KeyMap *, Arg *);
void commands_set_zoom_level(KeyMap *, Arg *);
void commands_toggle_maximized(KeyMap *, Arg *);
gboolean commands_toggle_hidden_files(KeyMap *, Arg *);
gboolean commands_web_inspector(KeyMap *, Arg *);
gboolean commands_quit(KeyMap *, Arg *);
gboolean commands_reload_scripts(KeyMap *, Arg *);
gboolean commands_fullscreen(KeyMap *, Arg *);
gboolean commands_pass_through(KeyMap *, Arg *);
#endif
