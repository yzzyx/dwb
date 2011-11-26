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

#include <JavaScriptCore/JavaScript.h>
#include <webkit/webkit.h>
#include <glib-2.0/glib.h>


void
js_create_callback(WebKitWebFrame *frame, const char *name, JSObjectCallAsFunctionCallback function) {
  JSContextRef ctx = webkit_web_frame_get_global_context(frame);
  JSStringRef jsname = JSStringCreateWithUTF8CString(name);
  JSObjectRef jsfunction = JSObjectMakeFunctionWithCallback(ctx, jsname, function);
  JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), jsname, jsfunction, 
      kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly, NULL);
  JSStringRelease(jsname);
}

/* js_string_to_char 
 * Converts a JSStringRef, return a newly allocated char.
 * {{{*/
char *
js_string_to_char(JSContextRef ctx, JSStringRef jsstring) {
  size_t length = JSStringGetLength(jsstring) + 1;

  char *ret = g_new(char, length);
  size_t written = JSStringGetUTF8CString(jsstring, ret, length);
    /* TODO: handle length error */
  if (written != length)
    return NULL;
  return ret;
}/*}}}*/

/*{{{*/
char *
js_value_to_char(JSContextRef ctx, JSValueRef value) {
  JSValueRef exc = NULL;
  if (! JSValueIsString(ctx, value)) 
    return NULL;
  JSStringRef jsstring = JSValueToStringCopy(ctx, value, &exc);
  if (exc != NULL) 
    return NULL;

  char *ret = js_string_to_char(ctx, jsstring);
  JSStringRelease(jsstring);
  return ret;
}/*}}}*/
