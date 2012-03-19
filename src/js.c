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

#include <JavaScriptCore/JavaScript.h>
#include "dwb.h"
#include "util.h"
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

JSObjectRef 
js_create_object(WebKitWebFrame *frame, const char *script) {
  if (script == NULL)
    return NULL;
  JSContextRef ctx = webkit_web_frame_get_global_context(frame);
  JSStringRef jsscript = JSStringCreateWithUTF8CString(script);
  JSValueRef exc = NULL;
  JSValueRef ret = JSEvaluateScript(ctx, jsscript, NULL, NULL, 0, &exc);
  if (exc != NULL)
    return NULL;
  JSStringRelease(jsscript);
  JSValueProtect(ctx, ret);
  JSObjectRef return_object = JSValueToObject(ctx, ret, &exc);
  if (exc != NULL)
    return NULL;
  JSPropertyNameArrayRef array = JSObjectCopyPropertyNames(ctx, return_object);
  for (int i=0; i<JSPropertyNameArrayGetCount(array); i++) {
    JSStringRef prop_name = JSPropertyNameArrayGetNameAtIndex(array, i);
    JSValueRef prop = JSObjectGetProperty(ctx, return_object, prop_name, NULL);
    JSObjectDeleteProperty(ctx, return_object, prop_name, NULL);
    JSObjectSetProperty(ctx, return_object, prop_name, prop, 
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, NULL);
  }
  JSPropertyNameArrayRelease(array);
  return return_object;
}
char *  
js_call_as_function(WebKitWebFrame *frame, JSObjectRef obj, char *string, char *json, char **char_ret) {
  if (obj == NULL) {
    goto error_out;
  }
  JSContextRef ctx = webkit_web_frame_get_global_context(frame);
  JSStringRef name = JSStringCreateWithUTF8CString(string);
  if (!JSObjectHasProperty(ctx, obj, name)) {
    goto error_out;
  }
  JSValueRef function = JSObjectGetProperty(ctx, obj, name, NULL);
  JSObjectRef function_object = JSValueToObject(ctx, function, NULL);
  JSValueRef ret;
  JSValueRef v = NULL;
  if (json != NULL) {
    JSStringRef js_json = JSStringCreateWithUTF8CString(json);
    v = JSValueMakeFromJSONString(ctx, js_json);
  }
  if (v) {
    JSValueRef vals[] = { v };
    ret = JSObjectCallAsFunction(ctx, function_object, NULL, 1, vals, NULL);
  }
  else {
    ret = JSObjectCallAsFunction(ctx, function_object, NULL, 0, NULL, NULL);
  }
  if (char_ret != NULL) {
    *char_ret = js_value_to_char(ctx, ret);
    return *char_ret;
  }
error_out: 
  if (char_ret != NULL)
    *char_ret = NULL;
  return NULL;
}

/*{{{*/
char *
js_value_to_char(JSContextRef ctx, JSValueRef value) {
  JSValueRef exc = NULL;
  if (value == NULL)
    return NULL;
  if (! JSValueIsString(ctx, value)) 
    return NULL;
  JSStringRef jsstring = JSValueToStringCopy(ctx, value, &exc);
  if (exc != NULL) 
    return NULL;

  char *ret = js_string_to_char(ctx, jsstring);
  JSStringRelease(jsstring);
  return ret;
}/*}}}*/
