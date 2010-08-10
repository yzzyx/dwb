#ifndef COMMANDS_H
#define COMMANDS_H

void dwb_com_simple_command(KeyMap *km);
gboolean dwb_com_set_setting(Arg *);
gboolean dwb_com_set_key(Arg *);
gboolean dwb_com_allow_cookie(Arg *);
gboolean dwb_create_hints(Arg *);
gboolean dwb_com_find(Arg *);
gboolean dwb_com_focus_input(Arg *);
gboolean dwb_com_resize_master(Arg *);
gboolean dwb_com_show_hints(Arg *);
gboolean dwb_com_show_keys(Arg *);
gboolean dwb_com_show_settings(Arg *);
gboolean dwb_com_quickmark(Arg *);
gboolean dwb_com_bookmark(Arg *);
gboolean dwb_com_open(Arg *);
gboolean dwb_com_focus_next(Arg *);
gboolean dwb_com_focus_prev(Arg *);
gboolean dwb_com_reload(Arg *);
gboolean dwb_com_set_orientation(Arg *);
void dwb_com_toggle_maximized(Arg *);
gboolean dwb_com_view_source(Arg *);
gboolean dwb_com_zoom_in(Arg *);
gboolean dwb_com_zoom_out(Arg *);
void dwb_com_set_zoom_level(Arg *);
gboolean dwb_com_scroll(Arg *);
gboolean dwb_com_history_back(Arg *);
gboolean dwb_com_history_forward(Arg *);
gboolean dwb_com_toggle_property(Arg *); 
gboolean dwb_com_toggle_proxy(Arg *); 
gboolean dwb_com_add_search_field(Arg *);
gboolean dwb_com_toggle_custom_encoding(Arg *);
gboolean dwb_com_yank(Arg *);
gboolean dwb_com_paste(Arg *);
gboolean dwb_com_entry_delete_word(Arg *);
gboolean dwb_com_entry_delete_letter(Arg *);
gboolean dwb_com_entry_delete_line(Arg *);
gboolean dwb_com_entry_word_forward(Arg *);
gboolean dwb_com_entry_word_back(Arg *);
gboolean dwb_com_entry_history_forward(Arg *);
gboolean dwb_com_entry_history_back(Arg *);
gboolean dwb_com_save_session(Arg *);
gboolean dwb_com_bookmarks(Arg *);
void dwb_com_remove_view(Arg *arg);
gboolean dwb_com_push_master(Arg *arg);
void dwb_com_focus(GList *gl);
#endif
