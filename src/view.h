/*
 * Copyright (c) 2010-2012 Stefan Bolte <portix@gmx.net>
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

#ifndef VIEW_H
#define VIEW_H

GList * view_add(const char *uri, gboolean background);
DwbStatus view_remove(GList *gl);
DwbStatus view_push_master(Arg *);
void view_set_active_style(GList *);
void view_set_normal_style(GList *);
void view_modify_style(GList *, DwbColor *tabfg, DwbColor *tabbg, PangoFontDescription *fd);
void view_icon_loaded(WebKitWebView *web, char *icon_uri, GList *gl);
void view_set_favicon(GList *gl, gboolean);
void view_clear_tab(GList *gl);

GtkWidget * dwb_web_view_create_plugin_widget_cb(WebKitWebView *, char *, char *, GHashTable *, GList *);
#endif
