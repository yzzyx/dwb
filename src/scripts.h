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

#ifndef SCRIPTS_H
#define SCRIPTS_H

void scripts_init_script(const char *);
void scripts_init();
enum SIGNALS {
  SCRIPT_SIG_FIRST        = 0,
  SCRIPT_SIG_NAVIGATION   = 0,
  SCRIPT_SIG_LOAD_STATUS  = 1,
  SCRIPT_SIG_MIME_TYPE    = 2, 
  SCRIPT_SIG_DOWNLOAD     = 3, 
  SCRIPT_SIG_LAST, 
};
gboolean scripts_emit(JSObjectRef , int , const char *);
void scripts_create_tab(GList *gl);

//#define EMIT_SCRIPT(gl, sig)  (VIEW(gl)->script != NULL && (dwb.misc.script_signals & (1<<SCRIPT_SIG_##sig)) 
//    && webkit_web_view_get_uri(WEBVIEW(gl)) != NULL && *webkit_web_view_get_uri(WEBVIEW(gl)))
#define EMIT_SCRIPT(gl, sig)  (VIEW(gl)->script != NULL && (dwb.misc.script_signals & (1<<SCRIPT_SIG_##sig)))
#define SCRIPTS_EMIT(gl, sig, json)  scripts_emit(VIEW(gl)->script, SCRIPT_SIG_##sig, json)
#define JSON_STRING(x)  "\""#x"\":\"%s\""

#endif
