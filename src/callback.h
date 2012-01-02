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

#ifndef CALLBACKS_H
#define CALLBACKS_H

gboolean callback_entry_key_release(GtkWidget *, GdkEventKey *);
gboolean callback_entry_key_press(GtkWidget *, GdkEventKey *);
gboolean callback_entry_insert_text(GtkWidget *, GdkEventKey *);
gboolean callback_delete_event(GtkWidget *w);
gboolean callback_key_press(GtkWidget *w, GdkEventKey *e);
gboolean callback_key_release(GtkWidget *w, GdkEventKey *e);
#endif
