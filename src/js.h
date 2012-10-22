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

#ifndef JS_H
#define JS_H
#include <JavaScriptCore/JavaScript.h>

void js_make_exception(JSContextRef ctx, JSValueRef *exception, const gchar *format, ...);
char * js_string_to_char(JSContextRef ctx, JSStringRef jsstring, size_t );
char * js_value_to_char(JSContextRef ctx, JSValueRef value, size_t limit, JSValueRef *);
JSObjectRef js_get_object_property(JSContextRef ctx, JSObjectRef arg, const char *name);
void js_set_property(JSContextRef ctx, JSObjectRef arg, const char *name, JSValueRef value, JSClassAttributes attr, JSValueRef *);
void js_set_object_property(JSContextRef ctx, JSObjectRef arg, const char *name, const char *value, JSValueRef *);
void js_set_object_number_property(JSContextRef ctx, JSObjectRef arg, const char *name, gdouble value, JSValueRef *exc);
char * js_get_string_property(JSContextRef ctx, JSObjectRef arg, const char *name);
double  js_get_double_property(JSContextRef ctx, JSObjectRef arg, const char *name);
JSObjectRef js_create_object(WebKitWebFrame *, const char *);
char * js_call_as_function(WebKitWebFrame *, JSObjectRef, const char *string, const char *args, JSType, char **char_ret);
JSValueRef js_char_to_value(JSContextRef ctx, const char *text);
char * js_value_to_json(JSContextRef ctx, JSValueRef value, size_t limit, JSValueRef *exc);
JSValueRef js_execute(JSContextRef ctx, const char *, JSValueRef *exc);
gboolean js_print_exception(JSContextRef ctx, JSValueRef exception);
JSObjectRef js_make_function(JSContextRef ctx, const char *script);
JSValueRef js_json_to_value(JSContextRef ctx, const char *text);
JSValueRef js_context_change(JSContextRef, JSContextRef, JSValueRef, JSValueRef *);

typedef struct _js_array_iterator {
  JSContextRef ctx;
  JSObjectRef array;
  int current_index;
  int length;
} js_array_iterator;
void js_array_iterator_init(JSContextRef ctx, js_array_iterator *iter, JSObjectRef object);
JSValueRef js_array_iterator_next(js_array_iterator *iter, JSValueRef *exc);

#define  JS_STRING_MAX 1024

#endif
