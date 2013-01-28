/*
 * Copyright (c) 2010-2013 Stefan Bolte <portix@gmx.net>
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

#ifndef DOM_H
#define DOM_H

gboolean dom_add_frame_listener(WebKitWebFrame *frame, const char *signal, GCallback callback, gboolean bubble, GList *gl);
gboolean dom_get_editable(WebKitDOMElement *element);
WebKitDOMElement * dom_get_active_element(WebKitDOMDocument *doc);
gboolean dom_remove_from_parent(WebKitDOMNode *node, GError **error);
#endif
