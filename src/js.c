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
#include "dwb.h"
#include "js.h"
#define JS_STRING_MAX 1024

/* js_get_object_property {{{*/
JSObjectRef 
js_get_object_property(JSContextRef ctx, JSObjectRef arg, const char *name) {
  JSValueRef exc = NULL;
  JSObjectRef ret;
  JSStringRef buffer = JSStringCreateWithUTF8CString(name);
  JSValueRef val = JSObjectGetProperty(ctx, arg, buffer, &exc);
  JSStringRelease(buffer);
  if (exc != NULL || !JSValueIsObject(ctx, val)) 
    return NULL;

  ret = JSValueToObject(ctx, val, &exc);
  if (exc != NULL)
    return NULL;
  return ret;

}/*}}}*/

/* js_get_string_property {{{*/
char * 
js_get_string_property(JSContextRef ctx, JSObjectRef arg, const char *name) {
  JSValueRef exc = NULL;
  JSStringRef buffer = JSStringCreateWithUTF8CString(name);
  JSValueRef val = JSObjectGetProperty(ctx, arg, buffer, &exc);
  JSStringRelease(buffer);
  if (exc != NULL || !JSValueIsString(ctx, val) )
    return NULL;
  return js_value_to_char(ctx, val);
}/*}}}*/

/* js_get_double_property {{{*/
double  
js_get_double_property(JSContextRef ctx, JSObjectRef arg, const char *name) {
  double ret;
  JSValueRef exc = NULL;
  JSStringRef buffer = JSStringCreateWithUTF8CString(name);
  JSValueRef val = JSObjectGetProperty(ctx, arg, buffer, &exc);
  JSStringRelease(buffer);
  if (exc != NULL || !JSValueIsNumber(ctx, val) )
    return 0;
  ret = JSValueToNumber(ctx, val, &exc);
  if (exc != NULL)
    return 0;
  return ret;
}/*}}}*/

/* js_string_to_char 
 * Converts a JSStringRef, return a newly allocated char.
 * {{{*/
char *
js_string_to_char(JSContextRef ctx, JSStringRef jsstring) {
  size_t length = MIN(JSStringGetLength(jsstring), JS_STRING_MAX) + 1;

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
