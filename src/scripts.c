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

#include <stdio.h>
#include <math.h>
#include <JavaScriptCore/JavaScript.h>
#include "dwb.h"
#include "scripts.h" 
#include "util.h" 
#include "js.h" 
//#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum)
#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete)
#define kJSDefaultProperty  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum)

typedef struct _Sigmap {
  int sig;
  const char *name;
} Sigmap;

static JSValueRef scripts_execute(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_set(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_get(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSValueRef scripts_io_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_io_get_file_content(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_io_set_file_content(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSStaticFunction static_functions[] = { 
  { "execute",         scripts_execute,         kJSDefaultFunction },
  { "spawn",           scripts_spawn,           kJSDefaultFunction },
  //{ "print",           scripts_print,           kJSDefaultFunction },
  { 0, 0, 0 }, 
};
static JSStaticValue static_values[] = { 
  {0, 0, 0, 0},
};
static Sigmap _sigmap[] = {
  { SCRIPT_SIG_NAVIGATION, "onNavigation" },
  { SCRIPT_SIG_LOAD_STATUS, "onLoadStatus" },
  { SCRIPT_SIG_MIME_TYPE, "onMimeType" },
  { SCRIPT_SIG_DOWNLOAD, "onDownload" } 
};
static JSObjectRef _sigObjects[SCRIPT_SIG_LAST];
static JSContextRef _global_context;
static JSObjectRef _signals;
static JSClassRef _viewClass;

static void 
scripts_create_global_object() {
  JSClassDefinition cd;
  JSStringRef name;
  JSClassRef class;
  /* Global object */
  cd = kJSClassDefinitionEmpty;
  cd.className = "dwb";
  cd.staticFunctions = static_functions;
  cd.staticValues = static_values;
  class = JSClassCreate(&cd);
  _global_context = JSGlobalContextCreate(class);

  name = JSStringCreateWithUTF8CString("signals");
  cd = kJSClassDefinitionEmpty;
  cd.className = "signals";
  class = JSClassCreate(&cd);
  _signals = JSObjectMake(_global_context, class, NULL);
  JSObjectSetProperty(_global_context, JSContextGetGlobalObject(_global_context), name, _signals, kJSPropertyAttributeReadOnly|kJSPropertyAttributeDontDelete, NULL);
  JSStringRelease(name);

  name = JSStringCreateWithUTF8CString("io");
  cd = kJSClassDefinitionEmpty;
  cd.className = "io";
  JSStaticFunction io_functions[] = { 
    { "print",     scripts_io_print,            kJSDefaultFunction },
    { "getFile",   scripts_io_get_file_content, kJSDefaultFunction },
    { "setFile",   scripts_io_set_file_content, kJSDefaultFunction },
    { 0,           0,           0 },
  };
  cd.staticFunctions = io_functions;
  class = JSClassCreate(&cd);
  JSObjectRef io = JSObjectMake(_global_context, class, NULL);
  JSObjectSetProperty(_global_context, JSContextGetGlobalObject(_global_context), name, io, kJSPropertyAttributeReadOnly|kJSPropertyAttributeDontDelete, NULL);
  JSStringRelease(name);
  
}

static void 
scripts_create_view_class() {
  static JSStaticFunction static_functions[] = { 
    { "set",             scripts_set,             kJSDefaultFunction },
    { "get",             scripts_get,             kJSDefaultFunction },
    { 0, 0, 0 }, 
  };
  JSClassDefinition vcd = kJSClassDefinitionEmpty;
  vcd.className = "view";
  vcd.staticFunctions = static_functions;
  _viewClass = JSClassCreate(&vcd);
}

static JSValueRef 
scripts_execute(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  return NULL;
}
static JSValueRef 
scripts_get_generic(JSContextRef ctx, GObject *o, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1)
    return JSValueMakeBoolean(ctx, false);
  char *name = js_value_to_char(ctx, argv[0], -1);
  GValue v = G_VALUE_INIT;
  g_value_init(&v, G_TYPE_STRING);
  g_object_get_property(o, name, &v);

  const char *val = g_value_get_string(&v);
  return js_char_to_value(ctx, val);
}
static JSValueRef 
scripts_get(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  GList *gl = JSObjectGetPrivate(this);
  WebKitWebSettings *s = webkit_web_view_get_settings(WEBVIEW(gl));
  return scripts_get_generic(ctx, G_OBJECT(s), function, argc, argv, exc);
}
static JSValueRef 
scripts_set_generic(JSContextRef ctx, GObject *o, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  char *sval = NULL;
  char dval;
  char *propname = NULL;
  if (argc < 1)
    return JSValueMakeBoolean(ctx, false);
  if (!JSValueIsObject(ctx, argv[0])) 
    return JSValueMakeBoolean(ctx, false);
  JSObjectRef argobj = JSValueToObject(ctx, argv[0], NULL);
  if (argobj == NULL)
    return JSValueMakeBoolean(ctx, false);
  JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(ctx, argobj);
  for (int i=0; i<JSPropertyNameArrayGetCount(names); i++)  {
    GValue v = G_VALUE_INIT;
    JSStringRef js_propname = JSPropertyNameArrayGetNameAtIndex(names, i); 
    JSValueRef value = JSObjectGetProperty(ctx, argobj, js_propname, NULL);

    int type = JSValueGetType(ctx, value);
    switch (type) {
      case  kJSTypeString : 
        sval = js_value_to_char(ctx, value, -1);
        if (sval == NULL) 
          return JSValueMakeBoolean(ctx, false);
        g_value_init(&v, G_TYPE_STRING);
        g_value_set_static_string(&v, sval);
        g_free(sval);
        break;
      case kJSTypeBoolean : 
        g_value_init(&v, G_TYPE_BOOLEAN);
        g_value_set_boolean(&v, JSValueToBoolean(ctx, value));
        break;
      case kJSTypeNumber : 
        dval = JSValueToNumber(ctx, value, NULL);
        if (dval == NAN) 
          return JSValueMakeBoolean(ctx, false);
        g_value_init(&v, G_TYPE_DOUBLE);
        g_value_set_double(&v, dval);
        break;
      default : return JSValueMakeBoolean(ctx, false);
    }
    propname = js_string_to_char(ctx, js_propname, JS_STRING_MAX);
    g_object_set_property(o, propname, &v);
    g_free(propname);
  }
  JSPropertyNameArrayRelease(names);
  return JSValueMakeBoolean(ctx, true);
}
static JSValueRef 
scripts_set(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  GList *gl = JSObjectGetPrivate(this);
  WebKitWebSettings *s = webkit_web_view_get_settings(WEBVIEW(gl));
  return scripts_set_generic(ctx, G_OBJECT(s), function, this, argc, argv, exc);

}
static JSValueRef scripts_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  return NULL;
}
static JSValueRef 
scripts_io_get_file_content(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = NULL;
  char *path = NULL, *content = NULL;
  if (argc < 1)
    return JSValueMakeNull(ctx);
  path = js_value_to_char(ctx, argv[0], PATH_MAX);
  if (path == NULL)
    goto error_out;
  content = util_get_file_content(path);
  if (content == NULL)
    goto error_out;
  ret = js_char_to_value(ctx, content);

error_out:
  g_free(path);
  g_free(content);
  if (ret == NULL)
    return JSValueMakeNull(ctx);
  return ret;

}
static JSValueRef 
scripts_io_set_file_content(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  char *path = NULL, *content = NULL;
  gboolean ret = false;
  if (argc < 2)
    return JSValueMakeBoolean(ctx, false);
  path = js_value_to_char(ctx, argv[0], PATH_MAX);
  content = js_value_to_char(ctx, argv[1], -1);
  ret = util_set_file_content(path, content);
  g_free(path);
  g_free(content);
  return JSValueMakeBoolean(ctx, ret);
}
static JSValueRef 
scripts_io_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc == 0)
    return JSValueMakeUndefined(_global_context);

  char *out;
  double dout;
  int type = JSValueGetType(ctx, argv[0]);
  switch (type) {
    case kJSTypeString : 
      out = js_value_to_char(ctx, argv[0], -1);
      if (out != NULL) { 
        puts(out);
        g_free(out);
      }
      break;
    case kJSTypeBoolean : 
      printf("%s\n", JSValueToBoolean(ctx, argv[0]) ? "true" : "false");
      break;
    case kJSTypeNumber : 
      dout = JSValueToNumber(ctx, argv[0], NULL);
      if (dout != NAN) 
        printf("%f\n", dout);
      break;
    default : break;
  }
  return JSValueMakeUndefined(ctx);
}

gboolean
scripts_emit(JSObjectRef obj, int signal, const char *json) {
  JSObjectRef function = _sigObjects[signal];
  if (function == NULL)
    return false;

  JSStringRef js_json = JSStringCreateWithUTF8CString(json);
  JSValueRef val[] = { obj, JSValueMakeFromJSONString(_global_context, js_json) };
  JSStringRelease(js_json);
  JSValueRef js_ret = JSObjectCallAsFunction(_global_context, function, NULL, 2, val, NULL);
  if (JSValueIsBoolean(_global_context, js_ret))
    return JSValueToBoolean(_global_context, js_ret);
  return false;
}

void 
scripts_create_tab(GList *gl) {
  if (_global_context == NULL )  {
    VIEW(gl)->script = NULL;
    return;
  }
  if (_viewClass == NULL ) 
    scripts_create_view_class();
  if (_global_context == NULL ) 
    scripts_create_global_object();
  VIEW(gl)->script = JSObjectMake(_global_context, _viewClass, gl);
}

void
scripts_init_script(const char *script) {
  if (_global_context == NULL) 
    scripts_create_global_object();

  JSStringRef js_script = JSStringCreateWithUTF8CString(script);
  JSEvaluateScript(_global_context, js_script, NULL, NULL, 0, NULL);
  JSStringRelease(js_script);
}
void 
scripts_init() {
  dwb.misc.script_signals = 0;
  if (_global_context == NULL)
    return;
  //JSObjectRef global_object = JSContextGetGlobalObject(_global_context);

  for (int i=SCRIPT_SIG_FIRST; i<SCRIPT_SIG_LAST; i++) {
    JSStringRef name = JSStringCreateWithUTF8CString(_sigmap[i].name);
    if (!JSObjectHasProperty(_global_context, _signals, name)) 
      goto release;
    JSValueRef val = JSObjectGetProperty(_global_context, _signals, name, NULL);
    if (!JSValueIsObject(_global_context, val)) 
      goto release;
    JSValueProtect(_global_context, val);
    JSObjectRef o = JSValueToObject(_global_context, val, NULL);
    if (o != NULL) {
      _sigObjects[i] = o;
      dwb.misc.script_signals |= (1<<i);
    }
release:
    JSStringRelease(name);
  }
}

