#ifndef COMMANDS_H
#define COMMANDS_H

void dwb_simple_command(KeyMap *km);
gboolean dwb_set_setting(Arg *);
gboolean dwb_set_key(Arg *);
gboolean dwb_allow_cookie(Arg *);
gboolean dwb_create_hints(Arg *);
gboolean dwb_find(Arg *);
gboolean dwb_focus_input(Arg *);
gboolean dwb_resize_master(Arg *);
gboolean dwb_show_hints(Arg *);
gboolean dwb_show_keys(Arg *);
gboolean dwb_show_settings(Arg *);
gboolean dwb_quickmark(Arg *);
gboolean dwb_bookmark(Arg *);
gboolean dwb_open(Arg *);
gboolean dwb_focus_next(Arg *);
gboolean dwb_focus_prev(Arg *);
gboolean dwb_reload(Arg *);
gboolean dwb_set_orientation(Arg *);
void dwb_toggle_maximized(Arg *);
gboolean dwb_view_source(Arg *);
gboolean dwb_zoom_in(Arg *);
gboolean dwb_zoom_out(Arg *);
void dwb_set_zoom_level(Arg *);
gboolean dwb_scroll(Arg *);
gboolean dwb_history_back(Arg *);
gboolean dwb_history_forward(Arg *);
gboolean dwb_toggle_property(Arg *); 
gboolean dwb_toggle_proxy(Arg *); 
gboolean dwb_add_search_field(Arg *);
gboolean dwb_toggle_custom_encoding(Arg *);
gboolean dwb_yank(Arg *);
gboolean dwb_paste(Arg *);
gchar * dwb_get_search_engine(const gchar *uri);
#endif
