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

#ifdef DWB_ADBLOCKER
#ifndef ADBLOCK_H
#define ADBLOCK_H

#include "dwb.h"

gboolean adblock_init();
gboolean adblock_running();
void adblock_end();
void adblock_connect(GList *gl);
void adblock_disconnect(GList *gl);
void adblock_set_user_stylesheet(const char *file);
JSValueRef adblock_js_callback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

#endif // ADBLOCK_H
#endif // DWB_ADBLOCKER
