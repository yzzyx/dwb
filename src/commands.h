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


void commands_simple_command(KeyMap *km);
void commands_add_view(KeyMap *, Arg *);
DwbStatus commands_add_search_field(KeyMap *, Arg *);
DwbStatus commands_allow_cookie(KeyMap *, Arg *);
DwbStatus commands_bookmark(KeyMap *, Arg *);
DwbStatus commands_bookmarks(KeyMap *, Arg *);

DwbStatus commands_entry_movement(KeyMap *, Arg *);
DwbStatus commands_entry_history_back(KeyMap *, Arg *);
DwbStatus commands_entry_history_forward(KeyMap *, Arg *);

DwbStatus commands_execute_userscript(KeyMap *, Arg *);
DwbStatus commands_find(KeyMap *, Arg *);
DwbStatus commands_search(KeyMap *, Arg *);
DwbStatus commands_focus_input(KeyMap *, Arg *);
DwbStatus commands_focus(KeyMap *, Arg *);
DwbStatus commands_focus_nth_view(KeyMap *, Arg *);
DwbStatus commands_complete_type(KeyMap *, Arg *);
DwbStatus commands_history(KeyMap *, Arg *);
DwbStatus commands_new_window_or_view(KeyMap *, Arg *);
DwbStatus commands_open(KeyMap *, Arg *);
DwbStatus commands_open_startpage(KeyMap *, Arg *);
DwbStatus commands_paste(KeyMap *, Arg *);
DwbStatus commands_print(KeyMap *, Arg *);
DwbStatus commands_push_master(KeyMap *, Arg *);
DwbStatus commands_quickmark(KeyMap *, Arg *);
DwbStatus commands_reload(KeyMap *, Arg *);
DwbStatus commands_reload_bypass_cache(KeyMap *, Arg *);
DwbStatus commands_stop_loading(KeyMap *, Arg *);
DwbStatus commands_resize_master(KeyMap *, Arg *);
DwbStatus commands_save_files(KeyMap *, Arg *);
DwbStatus commands_save_session(KeyMap *, Arg *);
DwbStatus commands_scroll(KeyMap *, Arg *);
DwbStatus commands_set_key(KeyMap *, Arg *);
DwbStatus commands_set_orientation(KeyMap *, Arg *);
DwbStatus commands_set_setting(KeyMap *, Arg *);
DwbStatus commands_show_hints(KeyMap *, Arg *);
DwbStatus commands_show_keys(KeyMap *, Arg *);
DwbStatus commands_show_settings(KeyMap *, Arg *);
DwbStatus commands_toggle_scripts(KeyMap *, Arg *a);
DwbStatus commands_toggle_plugin_blocker(KeyMap *, Arg *a);
DwbStatus commands_toggle_property(KeyMap *, Arg *); 
DwbStatus commands_toggle_proxy(KeyMap *, Arg *); 
DwbStatus commands_undo(KeyMap *, Arg *);
DwbStatus commands_view_source(KeyMap *, Arg *);
DwbStatus commands_yank(KeyMap *, Arg *);
DwbStatus commands_zoom_in(KeyMap *, Arg *);
DwbStatus commands_zoom_out(KeyMap *, Arg *);
DwbStatus dwb_create_hints(Arg *);
void commands_remove_view(KeyMap *, Arg *);
void commands_set_zoom_level(KeyMap *, Arg *);
void commands_toggle_maximized(KeyMap *, Arg *);
DwbStatus commands_toggle_hidden_files(KeyMap *, Arg *);
DwbStatus commands_web_inspector(KeyMap *, Arg *);
DwbStatus commands_quit(KeyMap *, Arg *);
DwbStatus commands_reload_scripts(KeyMap *, Arg *);
DwbStatus commands_fullscreen(KeyMap *, Arg *);
DwbStatus commands_pass_through(KeyMap *, Arg *);
DwbStatus commands_open_editor(KeyMap *, Arg *);
DwbStatus commands_insert_mode(KeyMap *, Arg *);
DwbStatus commands_command_mode(KeyMap *, Arg *);
DwbStatus commands_only(KeyMap *, Arg *);
DwbStatus commands_toggle_bars(KeyMap *, Arg *);
DwbStatus commands_presentation_mode(KeyMap *, Arg *);
DwbStatus commands_toggle_lock_protect(KeyMap *, Arg *);
//DwbStatus commands_toggle_locked(KeyMap *, Arg *);
#ifdef DWB_ADBLOCKER
DwbStatus commands_toggle_adblocker(KeyMap *, Arg *);
#endif

#endif
