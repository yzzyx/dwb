#ifndef VIEW_H
#define VIEW_H

GList * dwb_add_view(Arg *);
GList * dwb_add_view_new_with_webview(void);
void dwb_view_set_active_style(GList *);
void dwb_view_set_normal_style(GList *);
void dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, int fontsize);
void dwb_web_view_add_history_item(GList *gl);
void dwb_view_clean_vars(GList *gl);
#endif
