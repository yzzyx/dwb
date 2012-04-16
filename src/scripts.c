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
static JSValueRef scripts_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSStaticFunction static_functions[] = { 
  { "execute",         scripts_execute,         kJSDefaultFunction },
  { "spawn",           scripts_spawn,           kJSDefaultFunction },
  { "print",           scripts_print,           kJSDefaultFunction },
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
static JSContextRef _globalContext;
static JSClassRef _viewClass;

static void 
scripts_create_global_object() {
  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.className = "dwb";
  cd.staticFunctions = static_functions;
  cd.staticValues = static_values;
  JSClassRef class = JSClassCreate(&cd);
  _globalContext = JSGlobalContextCreate(class);
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
  char *name = js_value_to_char(ctx, argv[0]);
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
scripts_set_generic(JSContextRef ctx, GObject *o, JSObjectRef function, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
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
        sval = js_value_to_char(ctx, value);
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
    propname = js_string_to_char(ctx, js_propname);
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
  return scripts_set_generic(ctx, G_OBJECT(s), function, argc, argv, exc);

}
static JSValueRef scripts_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  return NULL;
}
static JSValueRef 
scripts_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc == 0)
    return JSValueMakeUndefined(_globalContext);

  char *out;
  double dout;
  int type = JSValueGetType(ctx, argv[0]);
  switch (type) {
    case kJSTypeString : 
      out = js_value_to_char(ctx, argv[0]);
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
  return JSValueMakeUndefined(_globalContext);
}

gboolean
scripts_emit(JSObjectRef obj, int signal, const char *json) {
  JSObjectRef function = _sigObjects[signal];
  if (function == NULL)
    return false;

  JSStringRef js_json = JSStringCreateWithUTF8CString(json);
  JSValueRef val[] = { obj, JSValueMakeFromJSONString(_globalContext, js_json) };
  JSStringRelease(js_json);
  JSValueRef js_ret = JSObjectCallAsFunction(_globalContext, function, NULL, 2, val, NULL);
  if (JSValueIsBoolean(_globalContext, js_ret))
    return JSValueToBoolean(_globalContext, js_ret);
  return false;
}

void 
scripts_create_tab(GList *gl) {
  if (_globalContext == NULL )  {
    VIEW(gl)->script = NULL;
    return;
  }
  if (_viewClass == NULL ) 
    scripts_create_view_class();
  if (_globalContext == NULL ) 
    scripts_create_global_object();
  VIEW(gl)->script = JSObjectMake(_globalContext, _viewClass, gl);
}

void
scripts_init_script(const char *script) {
  if (_globalContext == NULL) 
    scripts_create_global_object();

  JSStringRef js_script = JSStringCreateWithUTF8CString(script);
  JSEvaluateScript(_globalContext, js_script, NULL, NULL, 0, NULL);
  JSStringRelease(js_script);
}
void 
scripts_init() {
  dwb.misc.script_signals = 0;
  if (_globalContext == NULL)
    return;
  JSObjectRef globalObject = JSContextGetGlobalObject(_globalContext);
  for (int i=SCRIPT_SIG_FIRST; i<SCRIPT_SIG_LAST; i++) {
    JSStringRef name = JSStringCreateWithUTF8CString(_sigmap[i].name);
    if (!JSObjectHasProperty(_globalContext, globalObject, name)) 
      goto release;
    JSValueRef val = JSObjectGetProperty(_globalContext, globalObject, name, NULL);
    if (!JSValueIsObject(_globalContext, val)) 
      goto release;
    JSValueProtect(_globalContext, val);
    JSObjectRef o = JSValueToObject(_globalContext, val, NULL);
    if (o != NULL) {
      _sigObjects[i] = o;
      dwb.misc.script_signals |= (1<<i);
    }
release:
    JSStringRelease(name);
  }
}

