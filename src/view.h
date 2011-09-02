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

#ifndef VIEW_H
#define VIEW_H

#include "dwb.h"
#include "commands.h"
#include "completion.h"
#include "util.h"
#include "download.h"
#include "session.h"

GList * dwb_add_view(Arg *, gboolean background);
void dwb_view_remove(GList *gl);
gboolean dwb_view_push_master(Arg *);
GList * dwb_add_view_new_with_webview(void);
void dwb_view_set_active_style(View *);
void dwb_view_set_normal_style(View *);
#if _HAS_GTK3
void dwb_view_modify_style(View *, DwbColor *fg, DwbColor *bg, DwbColor *tabfg, DwbColor *tabbg, PangoFontDescription *fd);
#else
void dwb_view_modify_style(View *, DwbColor *fg, DwbColor *bg, DwbColor *tabfg, DwbColor *tabbg, PangoFontDescription *fd);
#endif /* _HAS_GTK3 */
void dwb_web_view_add_history_item(GList *gl);

GtkWidget * dwb_web_view_create_plugin_widget_cb(WebKitWebView *, char *, char *, GHashTable *, GList *);
#endif
