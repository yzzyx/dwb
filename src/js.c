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

void
js_make_exception(JSContextRef ctx, JSValueRef *exception, const gchar *format, ...) {
  va_list arg_list; 
  
  va_start(arg_list, format);
  gchar message[STRING_LENGTH];
  vsnprintf(message, STRING_LENGTH, format, arg_list);
  va_end(arg_list);
  *exception = js_char_to_value(ctx, message);
}

void 
js_set_object_property(JSContextRef ctx, JSObjectRef arg, const char *name, const char *value, JSValueRef *exc) {
  JSStringRef js_key = JSStringCreateWithUTF8CString(name);
  JSValueRef js_value = js_char_to_value(ctx, value);
  JSObjectSetProperty(ctx, arg, js_key, js_value, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly, exc);
  JSStringRelease(js_key);
}
void 
js_set_object_number_property(JSContextRef ctx, JSObjectRef arg, const char *name, gdouble value, JSValueRef *exc) {
  JSStringRef js_key = JSStringCreateWithUTF8CString(name);
  JSValueRef js_value = JSValueMakeNumber(ctx, value);
  JSObjectSetProperty(ctx, arg, js_key, js_value, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly, exc);
  JSStringRelease(js_key);
}
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

JSValueRef 
js_char_to_value(JSContextRef ctx, const char *text) {
  JSStringRef string = JSStringCreateWithUTF8CString(text);
  JSValueRef ret = JSValueMakeString(ctx, string);
  JSStringRelease(string);
  return ret;
}
/* js_get_string_property {{{*/
char * 
js_get_string_property(JSContextRef ctx, JSObjectRef arg, const char *name) {
  JSValueRef exc = NULL;
  JSStringRef buffer = JSStringCreateWithUTF8CString(name);
  JSValueRef val = JSObjectGetProperty(ctx, arg, buffer, &exc);
  JSStringRelease(buffer);
  if (exc != NULL || !JSValueIsString(ctx, val) )
    return NULL;
  return js_value_to_char(ctx, val, JS_STRING_MAX, NULL);
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
js_string_to_char(JSContextRef ctx, JSStringRef jsstring, size_t size) {
  size_t length;
  if (size > 0) 
    length = MIN(JSStringGetMaximumUTF8CStringSize(jsstring), size);
  else 
    length = JSStringGetMaximumUTF8CStringSize(jsstring);

  char *ret = g_malloc(sizeof(gchar) * length);
  JSStringGetUTF8CString(jsstring, ret, length);
  return ret;
}/*}}}*/

/* js_create_object(WebKitWebFrame *frame, const char *) 
 *
 * Executes a script in a function scope, should return an object with
 * function-properties
 * {{{*/
JSObjectRef 
js_create_object(WebKitWebFrame *frame, const char *script) {
  if (script == NULL)
    return NULL;

  JSStringRef js_script;
  JSValueRef ret, exc = NULL;
  JSObjectRef return_object;

  JSContextRef ctx = webkit_web_frame_get_global_context(frame);
  js_script = JSStringCreateWithUTF8CString(script);
  ret = JSEvaluateScript(ctx, js_script, NULL, NULL, 0, &exc);
  JSStringRelease(js_script);
  if (exc != NULL)
    return NULL;

  return_object = JSValueToObject(ctx, ret, &exc);
  if (exc != NULL)
    return NULL;
  JSValueProtect(ctx, ret);
  return return_object;
}/*}}}*/

/* js_call_as_function(WebKitWebFrame, JSObjectRef, char *string, char *json, * char **ret) {{{*/
char *  
js_call_as_function(WebKitWebFrame *frame, JSObjectRef obj, const char *string, const char *json, char **char_ret) {
  char *ret = NULL;
  JSValueRef js_ret, function, v = NULL;
  JSObjectRef function_object;
  JSStringRef js_json, js_name = NULL;
  JSContextRef ctx;

  if (obj == NULL)  {
    goto error_out;
  }

  ctx = webkit_web_frame_get_global_context(frame);
  js_name = JSStringCreateWithUTF8CString(string);

  if (!JSObjectHasProperty(ctx, obj, js_name)) {
    goto error_out;
  }
  function = JSObjectGetProperty(ctx, obj, js_name, NULL);
  function_object = JSValueToObject(ctx, function, NULL);
  if (json != NULL) {
    js_json = JSStringCreateWithUTF8CString(json);
    v = JSValueMakeFromJSONString(ctx, js_json);
    JSStringRelease(js_json);
  }
  if (v) {
    JSValueRef vals[] = { v };
    js_ret = JSObjectCallAsFunction(ctx, function_object, NULL, 1, vals, NULL);
  }
  else {
    js_ret = JSObjectCallAsFunction(ctx, function_object, NULL, 0, NULL, NULL);
  }
  if (char_ret != NULL) {
    ret = js_value_to_char(ctx, js_ret, JS_STRING_MAX, NULL);
  }
error_out: 
  if (js_name)
    JSStringRelease(js_name);
  if (char_ret != NULL)
    *char_ret = ret;
  return ret;
}/*}}}*/

/*{{{*/
char *
js_value_to_char(JSContextRef ctx, JSValueRef value, size_t limit, JSValueRef *exc) {
  if (value == NULL)
    return NULL;
  if (! JSValueIsString(ctx, value)) 
    return NULL;
  JSStringRef jsstring = JSValueToStringCopy(ctx, value, exc);
  if (jsstring == NULL) 
    return NULL;

  char *ret = js_string_to_char(ctx, jsstring, limit);
  JSStringRelease(jsstring);
  return ret;
}/*}}}*/

/* print_exception {{{*/
gboolean
js_print_exception(JSContextRef ctx, JSValueRef exception) {
  if (exception == NULL) 
    return false;
  if (!JSValueIsObject(ctx, exception))
    return false;
  JSObjectRef o = JSValueToObject(ctx, exception, NULL);
  if (o == NULL) 
    return false;

  gint line = (int)js_get_double_property(ctx, o, "line");
  gchar *message = js_get_string_property(ctx, o, "message");
  fprintf(stderr, "DWB SCRIPT EXCEPTION: in line %d: %s\n", line, message == NULL ? "unknown" : message);
  g_free(message);
  return true;
}
/*}}}*/


JSObjectRef  
js_make_function(JSContextRef ctx, const char *script) {
  JSValueRef exc;
  JSObjectRef ret = NULL;
  JSStringRef body = JSStringCreateWithUTF8CString(script);
  JSObjectRef function = JSObjectMakeFunction(ctx, NULL, 0, NULL, body, NULL, 0, &exc);
  if (function != NULL) {
    ret = function;
  }
  else {
    js_print_exception(ctx, exc);
  }
  JSStringRelease(body);
  return ret;
}

char *
js_value_to_json(JSContextRef ctx, JSValueRef value, size_t limit, JSValueRef *exc) {
  if (value == NULL)
    return NULL;
  JSStringRef js_json = JSValueCreateJSONString(ctx, value, 2, exc);
  if (js_json == NULL)
    return NULL;
  char *json = js_string_to_char(ctx, js_json, limit);
  JSStringRelease(js_json);
  return json;
}
JSValueRef 
js_json_to_value(JSContextRef ctx, const char *text) {
  JSStringRef json = JSStringCreateWithUTF8CString(text == NULL || *text == 0 ? "{}" : text);
  JSValueRef ret = JSValueMakeFromJSONString(ctx, json);
  JSStringRelease(json);
  return ret;
}
JSValueRef
js_execute(JSContextRef ctx, const char *script, JSValueRef *exc) {
  JSObjectRef function = js_make_function(ctx, script);
  if (function != NULL) {
    return JSObjectCallAsFunction(ctx, function, NULL, 0, NULL, exc); 
  }
  return NULL;
}
void 
js_array_iterator_init(JSContextRef ctx, js_array_iterator *iter, JSObjectRef object) {
  iter->ctx = ctx;
  iter->array = object;
  iter->current_index = 0;
  iter->length = js_get_double_property(ctx, object, "length");
}
JSValueRef 
js_array_iterator_next(js_array_iterator *iter, JSValueRef *exc) {
  if (iter->current_index == iter->length)
    return NULL;
  return JSObjectGetPropertyAtIndex(iter->ctx, iter->array, iter->current_index++, exc);
}

