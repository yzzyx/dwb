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
#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum)
//#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete )
#define kJSDefaultProperty  (kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum)

#define SCRIPT_WEBVIEW(o) (WEBVIEW(((GList*)JSObjectGetPrivate(o))))

typedef struct _Sigmap {
  int sig;
  const char *name;
} Sigmap;

static JSValueRef scripts_execute(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_include(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);


static JSValueRef scripts_system_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_system_get_env(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSValueRef scripts_tab_set(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_tab_get(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSValueRef scripts_io_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_io_read(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_io_write(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSValueRef scripts_get_generic(JSContextRef ctx, GObject *o, JSObjectRef this, const JSValueRef arg, JSValueRef* exc);
static Sigmap _sigmap[] = {
  { SCRIPT_SIG_NAVIGATION, "navigation" },
  { SCRIPT_SIG_LOAD_STATUS, "loadStatus" },
  { SCRIPT_SIG_MIME_TYPE, "mimeType" },
  { SCRIPT_SIG_DOWNLOAD, "download" } 
};
static JSObjectRef _sigObjects[SCRIPT_SIG_LAST];
static JSContextRef _global_context;
static JSObjectRef _signals;
static JSClassRef _viewClass;
static GString *_script;

JSClassRef 
scripts_create_class(const char *name, JSStaticFunction staticFunctions[], JSStaticValue staticValues[]) {
  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.className = name;
  cd.staticFunctions = staticFunctions;
  cd.staticValues = staticValues;
  return JSClassCreate(&cd);
}
JSObjectRef 
scripts_create_object(JSContextRef ctx, JSObjectRef obj, const char *name, JSStaticFunction staticFunctions[], JSStaticValue staticValues[], void *private) {
  JSClassRef class = scripts_create_class(name, staticFunctions, staticValues);
  JSObjectRef ret = JSObjectMake(ctx, class, private);
  JSStringRef js_name = JSStringCreateWithUTF8CString(name);
  JSObjectSetProperty(ctx, obj, js_name, ret, kJSPropertyAttributeReadOnly|kJSPropertyAttributeDontDelete, NULL);
  JSStringRelease(js_name);
  return ret;
  /* Global object */
}
static JSValueRef 
scripts_tabs_get_current(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  return VIEW(dwb.state.fview)->script;
}
static JSValueRef 
scripts_tabs_get_number(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  return JSValueMakeNumber(ctx, g_list_position(dwb.state.views, dwb.state.fview));
}
static JSValueRef 
scripts_tabs_get_length(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  return JSValueMakeNumber(ctx, g_list_length(dwb.state.views));
}
static JSValueRef 
scripts_tabs_get_nth(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1)
    return JSValueMakeNull(ctx);
  double n = JSValueToNumber(ctx, argv[0], NULL);
  if (n == NAN)
    return JSValueMakeNull(ctx);
  GList *nth = g_list_nth(dwb.state.views, (int)n);
  if (nth == NULL)
    return JSValueMakeNull(ctx);
  return VIEW(nth)->script;

}

static void 
scripts_create_global_object() {
  JSStaticFunction global_functions[] = { 
    { "execute",         scripts_execute,         kJSDefaultFunction },
    { "include",         scripts_include,         kJSDefaultFunction },
    { 0, 0, 0 }, 
  };

  //JSStaticValue static_values[] = { 
  //  {0, 0, 0, 0},
  //};
  JSClassRef class = scripts_create_class("dwb", global_functions, NULL);
  _global_context = JSGlobalContextCreate(class);

  JSObjectRef global_object = JSContextGetGlobalObject(_global_context);

  _signals = scripts_create_object(_global_context, global_object, "signals", NULL, NULL, NULL);

  JSStaticFunction io_functions[] = { 
    { "print",     scripts_io_print,            kJSDefaultFunction },
    { "read",      scripts_io_read,             kJSDefaultFunction },
    { "write",     scripts_io_write,            kJSDefaultFunction },
    { 0,           0,           0 },
  };
  scripts_create_object(_global_context, global_object, "io", io_functions, NULL, NULL);

  JSStaticFunction system_functions[] = { 
    { "spawn",           scripts_system_spawn,           kJSDefaultFunction },
    { "getEnv",          scripts_system_get_env,           kJSDefaultFunction },
    //{ "print",           scripts_print,           kJSDefaultFunction },
    { 0, 0, 0 }, 
  };
  scripts_create_object(_global_context, global_object, "system", system_functions, NULL, NULL);

  JSStaticFunction tab_functions[] = { 
    { "current",      scripts_tabs_get_current,    kJSDefaultFunction },
    { "number",       scripts_tabs_get_number,     kJSDefaultFunction },
    { "length",       scripts_tabs_get_length,     kJSDefaultFunction },
    { "nth",          scripts_tabs_get_nth,        kJSDefaultFunction },
    { 0, 0, 0 }, 
  };
  scripts_create_object(_global_context, global_object, "tabs", tab_functions, NULL, NULL);
}

static JSValueRef 
scripts_tab_load_uri(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitWebView *wv = SCRIPT_WEBVIEW(this);
  if (wv != NULL && argc >= 1) {
    char *uri = js_value_to_char(ctx, argv[0], -1);
    webkit_web_view_load_uri(wv, uri);
    g_free(uri);
  }
  return JSValueMakeUndefined(ctx);

}
static JSValueRef 
scripts_tab_history(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc >= 1) {
    double steps = JSValueToNumber(ctx, argv[0], NULL);
    if (steps != NAN) {
      webkit_web_view_go_back_or_forward(SCRIPT_WEBVIEW(this), (int)steps);
    }
  }
  return JSValueMakeUndefined(ctx);
}
static JSValueRef 
scripts_tab_reload(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  webkit_web_view_reload(SCRIPT_WEBVIEW(this));
  return JSValueMakeUndefined(ctx);
}
JSValueRef 
scripts_tab_get_property(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  char *name = js_string_to_char(ctx, js_name, -1);
  WebKitWebView *wv = SCRIPT_WEBVIEW(object);
  GValue v = G_VALUE_INIT;
  g_value_init(&v, G_TYPE_STRING);
  g_object_get_property(G_OBJECT(wv), name, &v);
  g_free(name);

  const char *val = g_value_get_string(&v);
  return js_char_to_value(ctx, val);
}
bool 
scripts_tab_set_property(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef value, JSValueRef* exception) {
  WebKitWebView *wv = SCRIPT_WEBVIEW(object);
  char *name = js_string_to_char(ctx, js_name, -1);
  GValue v = G_VALUE_INIT;
  g_value_init(&v, G_TYPE_STRING);
  g_object_set_property(G_OBJECT(wv), name, &v);
  g_free(name);
  return true;
}

static void 
scripts_create_view_class() {
  JSStaticFunction functions[] = { 
    { "set",             scripts_tab_set,             kJSDefaultFunction },
    { "get",             scripts_tab_get,             kJSDefaultFunction },
    { "loadUri",         scripts_tab_load_uri,             kJSDefaultFunction },
    { "history",         scripts_tab_history,             kJSDefaultFunction },
    { "reload",          scripts_tab_reload,             kJSDefaultFunction },
    { 0, 0, 0 }, 
  };
  JSStaticValue values[] = {
    { "uri", scripts_tab_get_property, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly  },
    { "title", scripts_tab_get_property, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly  },
    { 0, 0, 0 }, 
  };
  JSClassDefinition vcd = kJSClassDefinitionEmpty;
  vcd.className = "view";
  vcd.staticFunctions = functions;
  vcd.staticValues = values;
  _viewClass = JSClassCreate(&vcd);
}

static JSValueRef 
scripts_get_generic(JSContextRef ctx, GObject *o, JSObjectRef this, const JSValueRef arg, JSValueRef* exc) {
  char *name = js_value_to_char(ctx, arg, -1);
  GValue v = G_VALUE_INIT;
  g_value_init(&v, G_TYPE_STRING);
  g_object_get_property(o, name, &v);

  const char *val = g_value_get_string(&v);
  return js_char_to_value(ctx, val);
}
static JSValueRef 
scripts_tab_get(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1)
    return JSValueMakeUndefined(ctx);
  GList *gl = JSObjectGetPrivate(this);
  WebKitWebSettings *s = webkit_web_view_get_settings(WEBVIEW(gl));
  return scripts_get_generic(ctx, G_OBJECT(s), function, argv[0], exc);
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
scripts_tab_set(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  GList *gl = JSObjectGetPrivate(this);
  WebKitWebSettings *s = webkit_web_view_get_settings(WEBVIEW(gl));
  return scripts_set_generic(ctx, G_OBJECT(s), function, this, argc, argv, exc);

}

/* GLOBAL {{{*/
static JSValueRef 
scripts_execute(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  DwbStatus status = STATUS_ERROR;
  if (argc < 1)
    return JSValueMakeBoolean(ctx, false);
  char *command = js_value_to_char(ctx, argv[0], -1);
  if (command != NULL) {
    status = dwb_parse_command_line(command);
    g_free(command);
  }
  return JSValueMakeBoolean(ctx, status == STATUS_OK);
}
static JSValueRef 
scripts_include(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gboolean ret = false;
  if (argc < 1)
    return JSValueMakeBoolean(ctx, false);
  char *path = NULL, *content = NULL; 
  if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX)) == NULL) 
    goto error_out;
  if ( (content = util_get_file_content(path)) == NULL) 
    goto error_out;
  JSStringRef script = JSStringCreateWithUTF8CString(content);
  if (JSEvaluateScript(ctx, script, NULL, NULL, 0, NULL)  != NULL) 
    ret = true;
  JSStringRelease(script);

error_out: 
  g_free(content);
  g_free(path);
  return JSValueMakeBoolean(ctx, ret);
}
static JSValueRef 
scripts_system_get_env(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1)
    return JSValueMakeNull(ctx);
  char *name = js_value_to_char(ctx, argv[0], -1);
  if (name == NULL) 
    return JSValueMakeNull(ctx);
  const char *env = g_getenv(name);
  g_free(name);
  if (env == NULL)
    return JSValueMakeNull(ctx);
  return js_char_to_value(ctx, env);
}

static JSValueRef 
scripts_system_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gboolean ret = false; 
  if (argc < 1)
    return JSValueMakeBoolean(ctx, false);
  char *cmdline = js_value_to_char(ctx, argv[0], -1);
  if (cmdline) {
    ret = g_spawn_command_line_async(cmdline, NULL);
    g_free(cmdline);
  }
  return JSValueMakeBoolean(ctx, ret);
}/*}}}*/


/* IO {{{*/
static JSValueRef 
scripts_io_read(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = NULL;
  char *path = NULL, *content = NULL;
  if (argc < 1)
    return JSValueMakeNull(ctx);
  if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX) ) == NULL )
    goto error_out;
  if ( (content = util_get_file_content(path) ) == NULL )
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
scripts_io_write(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gboolean ret = false;
  FILE *f;
  char *path = NULL, *content = NULL, *mode = NULL;
  if (argc < 3)
    return JSValueMakeBoolean(ctx, false);
  if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX)) == NULL )
    goto error_out;
  if ( (mode = js_value_to_char(ctx, argv[1], -1)) == NULL )
    goto error_out;
  if (g_strcmp0(mode, "w") && g_strcmp0(mode, "a"))
    goto error_out;
  if ( (content = js_value_to_char(ctx, argv[2], -1)) == NULL ) 
    goto error_out;
  if ( (f = fopen(path, mode)) != NULL) {
    fprintf(f, content);
    fclose(f);
  }
error_out:
  g_free(path);
  g_free(mode);
  g_free(content);
  return JSValueMakeBoolean(ctx, ret);
}/* }}} */

static JSValueRef 
scripts_io_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc == 0)
    return JSValueMakeUndefined(_global_context);

  FILE *stream = stdout;
  if (argc >= 2) {
    char *fd = js_value_to_char(ctx, argv[1], -1);
    if (!g_strcmp0(fd, "stderr")) 
      stream = stderr;
    g_free(fd);
  }

  char *out;
  double dout;
  int type = JSValueGetType(ctx, argv[0]);
  switch (type) {
    case kJSTypeString : 
      out = js_value_to_char(ctx, argv[0], -1);
      if (out != NULL) { 
        fprintf(stream, "%s\n", out);
        g_free(out);
      }
      break;
    case kJSTypeBoolean : 
      fprintf(stream, "%s\n", JSValueToBoolean(ctx, argv[0]) ? "true" : "false");
      break;
    case kJSTypeNumber : 
      dout = JSValueToNumber(ctx, argv[0], NULL);
      if (dout != NAN) 
        fprintf(stream, "%f\n", dout);
      break;
    default : break;
  }
  return JSValueMakeUndefined(ctx);
}/*}}}*/

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
  if (_script == NULL)
    _script = g_string_new(script);
  else {
    g_string_append(_script, script);
  }
}
void 
scripts_evaluate(const char *script) {
  if (script == NULL)
    return;
  JSStringRef js_script = JSStringCreateWithUTF8CString(script);
  JSEvaluateScript(_global_context, js_script, NULL, NULL, 0, NULL);
  JSStringRelease(js_script);
}
void 
scripts_init() {
  dwb.misc.script_signals = 0;
  if (_global_context == NULL)
    return;

  char *dir = util_get_data_dir(LIBJS_DIR);
  if (dir != NULL) {
    GString *content = g_string_new(NULL);
    util_get_directory_content(content, dir, "js");
    scripts_evaluate(content->str);
    g_string_free(content, true);
    g_free(dir);
    
  }

  scripts_evaluate(_script->str);
  g_string_free(_script, true);
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
