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
#include <string.h>
#include <math.h>
#include <JavaScriptCore/JavaScript.h>
#include "dwb.h"
#include "scripts.h" 
#include "util.h" 
#include "js.h" 
#include "soup.h" 
#include "domain.h" 
#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete )
//#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete )
#define kJSDefaultProperty  (kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly )

#define SCRIPT_WEBVIEW(o) (WEBVIEW(((GList*)JSObjectGetPrivate(o))))
#define EXCEPTION(X)   "DWB EXCEPTION : "X

typedef struct _Sigmap {
  int sig;
  const char *name;
} Sigmap;

static Sigmap _sigmap[] = {
  { SCRIPT_SIG_NAVIGATION, "navigation" },
  { SCRIPT_SIG_LOAD_STATUS, "loadStatus" },
  { SCRIPT_SIG_MIME_TYPE, "mimeType" },
  { SCRIPT_SIG_DOWNLOAD, "download" }, 
  { SCRIPT_SIG_DOWNLOAD_STATUS, "downloadStatus" }, 
  { SCRIPT_SIG_RESOURCE, "resource" },
  { SCRIPT_SIG_KEY_PRESS, "keyPress" },
  { SCRIPT_SIG_KEY_RELEASE, "keyRelease" },
  { SCRIPT_SIG_BUTTON_PRESS, "buttonPress" },
  { SCRIPT_SIG_BUTTON_RELEASE, "buttonRelease" },
  { SCRIPT_SIG_TAB_FOCUS, "tabFocus" },
  { SCRIPT_SIG_FRAME_STATUS, "frameStatus" },
  { SCRIPT_SIG_LOAD_FINISHED, "loadFinished" },
  { SCRIPT_SIG_LOAD_COMMITTED, "loadCommitted" },
};
enum {
  SPAWN_SUCCESS = 0, 
  SPAWN_FAILED = 1<<0, 
  SPAWN_STDOUT_FAILED = 1<<1, 
  SPAWN_STDERR_FAILED = 1<<2, 
};

static JSObjectRef _sigObjects[SCRIPT_SIG_LAST];
static JSGlobalContextRef _global_context;
static JSClassRef _view_class;
static GSList *_script_list;

void 
evaluate(const char *script) {
  if (script == NULL)
    return;
  JSStringRef js_script = JSStringCreateWithUTF8CString(script);
  JSEvaluateScript(_global_context, js_script, NULL, NULL, 0, NULL);
  JSStringRelease(js_script);
}

static JSClassRef 
create_class(const char *name, JSStaticFunction staticFunctions[], JSStaticValue staticValues[]) {
  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.className = name;
  cd.staticFunctions = staticFunctions;
  cd.staticValues = staticValues;
  return JSClassCreate(&cd);
}
static JSObjectRef 
create_object(JSContextRef ctx, JSClassRef class, JSObjectRef obj, const char *name, void *private) {
  JSObjectRef ret = JSObjectMake(ctx, class, private);
  JSClassRelease(class);
  JSStringRef js_name = JSStringCreateWithUTF8CString(name);
  JSObjectSetProperty(ctx, obj, js_name, ret, kJSPropertyAttributeReadOnly|kJSPropertyAttributeDontDelete, NULL);
  JSStringRelease(js_name);
  return ret;
}

static JSValueRef
inject(JSContextRef ctx, JSContextRef wctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gboolean ret = false, global = false;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("webview.inject: missing argument"));
    return JSValueMakeBoolean(ctx, false);
  }
  if (argc > 1 && JSValueIsBoolean(ctx, argv[1])) 
    global = JSValueToBoolean(ctx, argv[1]);
    
  JSStringRef script = JSValueToStringCopy(ctx, argv[0], exc);
  if (script == NULL)
    return JSValueMakeBoolean(ctx, false);

  if (global) {
    ret = JSEvaluateScript(wctx, script, NULL, NULL, 0, exc) != NULL;
  }
  else {
    JSObjectRef function = JSObjectMakeFunction(wctx, NULL, 0, NULL, script, NULL, 0, exc);
    if (function != NULL) {
      ret = JSObjectCallAsFunction(wctx, function, NULL, 0, NULL, exc) != NULL;
    }
  }
  JSStringRelease(script);
  return JSValueMakeBoolean(ctx, ret);
}

static JSValueRef 
set_generic(JSContextRef ctx, GObject *o, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  char *sval = NULL;
  char dval;
  char *propname = NULL;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("webview.set: missing argument"));
    return JSValueMakeBoolean(ctx, false);
  }
  if (!JSValueIsObject(ctx, argv[0])) 
    return JSValueMakeBoolean(ctx, false);
  JSObjectRef argobj = JSValueToObject(ctx, argv[0], exc);
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
        sval = js_value_to_char(ctx, value, -1, exc);
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
get_generic(JSContextRef ctx, GObject *o, JSObjectRef this, const JSValueRef arg, JSValueRef* exc) {
  char *name = js_value_to_char(ctx, arg, -1, exc);
  GValue v = G_VALUE_INIT;
  g_value_init(&v, G_TYPE_STRING);
  g_object_get_property(o, name, &v);

  const char *val = g_value_get_string(&v);
  return js_char_to_value(ctx, val);
}

/* TABS {{{*/
static JSValueRef 
tabs_current(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) {
  return VIEW(dwb.state.fview)->script;
}
static JSValueRef 
tabs_number(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) {
  return JSValueMakeNumber(ctx, g_list_position(dwb.state.views, dwb.state.fview));
}
static JSValueRef 
tabs_length(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) {
  return JSValueMakeNumber(ctx, g_list_length(dwb.state.views));
}
static JSValueRef 
tabs_get_nth(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("tabs.nth: missing argument"));
    return JSValueMakeNull(ctx);
  }
  double n = JSValueToNumber(ctx, argv[0], exc);
  if (n == NAN)
    return JSValueMakeNull(ctx);
  GList *nth = g_list_nth(dwb.state.views, (int)n);
  if (nth == NULL)
    return JSValueMakeNull(ctx);
  return VIEW(nth)->script;
}


static bool 
tab_set_uri(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef arg, JSValueRef* exc) {
  WebKitWebView *wv = SCRIPT_WEBVIEW(this);
  if (wv != NULL) {
    char *uri = js_value_to_char(ctx, arg, -1, exc);
    if (uri == NULL)
      return false;
    webkit_web_view_load_uri(wv, uri);
    g_free(uri);
    return true;
  }
  return false;
}
static JSValueRef 
tab_history(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = JSValueMakeUndefined(ctx);
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("webview.history: missing argument."));
    return ret;
  }
  double steps = JSValueToNumber(ctx, argv[0], NULL);
  if (steps != NAN) {
    webkit_web_view_go_back_or_forward(SCRIPT_WEBVIEW(this), (int)steps);
  }
  return ret;
}
static JSValueRef 
tab_reload(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  webkit_web_view_reload(SCRIPT_WEBVIEW(this));
  return JSValueMakeUndefined(ctx);
}
static JSValueRef 
tab_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  GList *gl = JSObjectGetPrivate(this);
  JSContextRef wctx = JS_CONTEXT_REF(gl);
  return inject(ctx, wctx, function, this, argc, argv, exc);
}
JSValueRef
get_property(JSContextRef ctx, GObject *o, JSStringRef js_name, JSValueRef *exception) {
  char *name = js_string_to_char(ctx, js_name, -1);
  GValue v = G_VALUE_INIT;
  g_value_init(&v, G_TYPE_STRING);
  g_object_get_property(o, name, &v);
  g_free(name);

  const char *val = g_value_get_string(&v);
  return js_char_to_value(ctx, val);
}
JSValueRef 
tab_get_property(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  return get_property(ctx, G_OBJECT(SCRIPT_WEBVIEW(object)), js_name, exception);
}
JSValueRef 
tab_get_domain(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  SoupMessage *msg = dwb_soup_get_message(SCRIPT_WEBVIEW(object));
  if (msg == NULL)
    return JSValueMakeNull(ctx);
  SoupURI *uri = soup_message_get_uri(msg);
  const char *host = soup_uri_get_host(uri);
  return js_char_to_value(ctx, domain_get_base_for_host(host));
}

JSValueRef 
tab_get_host(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  SoupMessage *msg = dwb_soup_get_message(SCRIPT_WEBVIEW(object));
  if (msg == NULL)
    return JSValueMakeNull(ctx);
  SoupURI *uri = soup_message_get_uri(msg);
  return js_char_to_value(ctx, soup_uri_get_host(uri));
}

static JSValueRef 
tab_get(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("webview.get: missing argument."));
    return JSValueMakeUndefined(ctx);
  }
  GList *gl = JSObjectGetPrivate(this);
  WebKitWebSettings *s = webkit_web_view_get_settings(WEBVIEW(gl));
  return get_generic(ctx, G_OBJECT(s), function, argv[0], exc);
}
static JSValueRef 
tab_set(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  GList *gl = JSObjectGetPrivate(this);
  WebKitWebSettings *s = webkit_web_view_get_settings(WEBVIEW(gl));
  return set_generic(ctx, G_OBJECT(s), function, this, argc, argv, exc);

}/*}}}*/

static JSValueRef 
frame_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitWebFrame *frame = JSObjectGetPrivate(this);
  JSContextRef wctx = webkit_web_frame_get_global_context(frame);
  return inject(ctx, wctx, function, this, argc, argv, exc);
}

static JSValueRef 
object_get_property(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  return get_property(ctx, JSObjectGetPrivate(object), js_name, exception);
}
/* GLOBAL {{{*/
static JSValueRef 
global_execute(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  DwbStatus status = STATUS_ERROR;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("execute: missing argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  char *command = js_value_to_char(ctx, argv[0], -1, exc);
  if (command != NULL) {
    status = dwb_parse_command_line(command);
    g_free(command);
  }
  return JSValueMakeBoolean(ctx, status == STATUS_OK);
}
static JSValueRef 
global_include(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = NULL;
  gboolean global = false;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("include: missing argument."));
    return JSValueMakeNull(ctx);
  }
  if (argc > 1 && JSValueIsBoolean(ctx, argv[1])) 
    global = JSValueToBoolean(ctx, argv[1]);

  char *path = NULL, *content = NULL; 
  if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX, exc)) == NULL) 
    goto error_out;
  if ( (content = util_get_file_content(path)) == NULL) {
    js_make_exception(ctx, exc, EXCEPTION("include: reading %s failed."), path);
    goto error_out;
  }
  JSStringRef script = JSStringCreateWithUTF8CString(content);

  if (global) {
    ret = JSEvaluateScript(ctx, script, NULL, NULL, 0, exc);
  }
  else {
    JSObjectRef function = JSObjectMakeFunction(ctx, NULL, 0, NULL, script, NULL, 0, exc);
    if (function != NULL) {
      ret = JSObjectCallAsFunction(ctx, function, NULL, 0, NULL, exc);
    }
  }

  JSStringRelease(script);

error_out: 
  g_free(content);
  g_free(path);
  if (ret == NULL)
    return JSValueMakeNull(ctx);
  return ret;
}

static gboolean
timeout_callback(JSObjectRef obj) {
  JSValueRef val = JSObjectCallAsFunction(_global_context, obj, NULL, 0, NULL, NULL);
  if (val == NULL)
    return false;
  return !JSValueIsBoolean(_global_context, val) || JSValueToBoolean(_global_context, val);
}
static JSValueRef 
global_timer_stop(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gdouble sigid;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("timerStop: missing argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  if ((sigid = JSValueToNumber(ctx, argv[0], exc)) != NAN) {
    g_source_remove((int)sigid);
    return JSValueMakeBoolean(ctx, true);
  }
  return JSValueMakeBoolean(ctx, false);
}
static JSValueRef 
global_timer_start(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 2) {
    js_make_exception(ctx, exc, EXCEPTION("timerStart: missing argument."));
    return JSValueMakeNumber(ctx, -1);
  }
  double msec = 10;
  if ((msec = JSValueToNumber(ctx, argv[0], exc)) == NAN )
    return JSValueMakeNumber(ctx, -1);
  JSObjectRef func = JSValueToObject(ctx, argv[1], exc);
  if (func == NULL || !JSObjectIsFunction(ctx, func)) {
    js_make_exception(ctx, exc, EXCEPTION("timerStart: argument 2 is not a function."));
    return JSValueMakeNumber(ctx, -1);
  }
  int ret = g_timeout_add((int)msec, (GSourceFunc)timeout_callback, func);
  return JSValueMakeNumber(ctx, ret);
}
static JSValueRef 
data_get_profile(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  return js_char_to_value(ctx, dwb.misc.profile);
}
static JSValueRef 
data_get_cache_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  return js_char_to_value(ctx, dwb.files.cachedir);
}
static JSValueRef 
data_get_config_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  char *dir = util_build_path();
  if (dir == NULL) {
    js_make_exception(ctx, exception, EXCEPTION("configDir: cannot get config path."));
    return JSValueMakeNull(ctx);
  }
  JSValueRef ret = js_char_to_value(ctx, dir);
  g_free(dir);
  return ret;
}
static JSValueRef 
data_get_system_data_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  char *dir = util_get_system_data_dir(NULL);
  if (dir == NULL) {
    js_make_exception(ctx, exception, EXCEPTION("systemDataDir: cannot get system data directory."));
    return JSValueMakeNull(ctx);
  }
  JSValueRef ret = js_char_to_value(ctx, dir);
  g_free(dir);
  return ret;
}
static JSValueRef 
data_get_user_data_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  char *dir = util_get_user_data_dir(NULL);
  if (dir == NULL) {
    js_make_exception(ctx, exception, EXCEPTION("userDataDir: cannot get user data directory."));
    return JSValueMakeNull(ctx);
  }
  JSValueRef ret = js_char_to_value(ctx, dir);
  g_free(dir);
  return ret;
}
static JSValueRef 
system_get_env(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("system.getEnv: missing argument"));
    return JSValueMakeNull(ctx);
  }
  char *name = js_value_to_char(ctx, argv[0], -1, exc);
  if (name == NULL) 
    return JSValueMakeNull(ctx);
  const char *env = g_getenv(name);
  g_free(name);
  if (env == NULL)
    return JSValueMakeNull(ctx);
  return js_char_to_value(ctx, env);
}
gboolean
spawn_output(GIOChannel *channel, GIOCondition condition, JSObjectRef callback) {
  char *content; 
  gsize length;
  if (condition == G_IO_HUP || condition == G_IO_ERR || condition == G_IO_NVAL) {
    g_io_channel_unref(channel);
    return false;
  }
  else if (g_io_channel_read_to_end(channel, &content, &length, NULL) == G_IO_STATUS_NORMAL && content != NULL)  {
      JSValueRef arg = js_char_to_value(_global_context, content);
      if (arg != NULL) {
        JSValueRef argv[] = { arg };
        JSObjectCallAsFunction(_global_context, callback, NULL, 1,  argv , NULL);
      }
      g_free(content);
  }
  return false;
}

static JSValueRef 
system_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  int ret = 0; 
  int outfd, errfd;
  char **srgv = NULL;
  int srgc;
  GIOChannel *out_channel, *err_channel;
  JSObjectRef oc = NULL, ec = NULL;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("spawn needs an argument."));
    return JSValueMakeBoolean(ctx, SPAWN_FAILED);
  }
  if (argc > 1) {
    oc = JSValueToObject(ctx, argv[1], NULL);
    if ( oc == NULL || !JSObjectIsFunction(ctx, oc) )  {
      if (!JSValueIsNull(ctx, argv[1])) 
        ret |= SPAWN_STDOUT_FAILED;
      oc = NULL;
    }
  }
  if (argc > 2) {
    ec = JSValueToObject(ctx, argv[2], NULL);
    if ( ec == NULL || !JSObjectIsFunction(ctx, ec) ) {
      if (!JSValueIsNull(ctx, argv[2])) 
        ret |= SPAWN_STDERR_FAILED;
      ec = NULL;
    }
  }
  char *cmdline = js_value_to_char(ctx, argv[0], -1, exc);
  if (cmdline == NULL) {
    ret |= SPAWN_FAILED;
    goto error_out;
  }

  if (!g_shell_parse_argv(cmdline, &srgc, &srgv, NULL) || 
     !g_spawn_async_with_pipes(NULL, srgv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, oc != NULL ? &outfd : NULL, ec != NULL ? &errfd : NULL, NULL)) {
    js_make_exception(ctx, exc, EXCEPTION("spawning %s failed."), cmdline);
    ret |= SPAWN_FAILED;
    goto error_out;
  }
  if (oc != NULL) {
    out_channel = g_io_channel_unix_new(outfd);
    g_io_add_watch(out_channel, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL, (GIOFunc)spawn_output, oc);
    g_io_channel_set_close_on_unref(out_channel, true);
  }
  if (ec != NULL) {
    err_channel = g_io_channel_unix_new(errfd);
    g_io_add_watch(err_channel, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL, (GIOFunc)spawn_output, ec);
    g_io_channel_set_close_on_unref(err_channel, true);
  }
error_out:
  g_free(cmdline);
  return JSValueMakeNumber(ctx, ret);
}/*}}}*/

/* IO {{{*/
static JSValueRef 
io_read(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = NULL;
  char *path = NULL, *content = NULL;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("io.read needs an argument."));
    return JSValueMakeNull(ctx);
  }
  if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX, exc) ) == NULL )
    goto error_out;
  if ( (content = util_get_file_content(path) ) == NULL ) {
    goto error_out;
  }
  ret = js_char_to_value(ctx, content);

error_out:
  g_free(path);
  g_free(content);
  if (ret == NULL)
    return JSValueMakeNull(ctx);
  return ret;

}

static JSValueRef 
io_write(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gboolean ret = false;
  FILE *f;
  char *path = NULL, *content = NULL, *mode = NULL;
  if (argc < 3) {
    js_make_exception(ctx, exc, EXCEPTION("io.write needs 3 arguments."));
    return JSValueMakeBoolean(ctx, false);
  }
  if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX, exc)) == NULL )
    goto error_out;
  if ( (mode = js_value_to_char(ctx, argv[1], -1, exc)) == NULL )
    goto error_out;
  if (g_strcmp0(mode, "w") && g_strcmp0(mode, "a")) {
    js_make_exception(ctx, exc, EXCEPTION("io.write: invalid mode."));
    goto error_out;
  }
  if ( (content = js_value_to_char(ctx, argv[2], -1, exc)) == NULL ) 
    goto error_out;
  if ( (f = fopen(path, mode)) != NULL) {
    fprintf(f, content);
    fclose(f);
  }
  else {
    js_make_exception(ctx, exc, EXCEPTION("io.write: cannot open %s for writing."), path);
  }
error_out:
  g_free(path);
  g_free(mode);
  g_free(content);
  return JSValueMakeBoolean(ctx, ret);
}

static JSValueRef 
io_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc == 0) {
    js_make_exception(ctx, exc, EXCEPTION("io.print needs an argument."));
    return JSValueMakeUndefined(_global_context);
  }

  FILE *stream = stdout;
  if (argc >= 2) {
    char *fd = js_value_to_char(ctx, argv[1], -1, exc);
    if (!g_strcmp0(fd, "stderr")) 
      stream = stderr;
    g_free(fd);
  }

  char *out;
  double dout;
  int type = JSValueGetType(ctx, argv[0]);
  switch (type) {
    case kJSTypeString : 
      out = js_value_to_char(ctx, argv[0], -1, exc);
      if (out != NULL) { 
        fprintf(stream, "%s\n", out);
        g_free(out);
      }
      break;
    case kJSTypeBoolean : 
      fprintf(stream, "%s\n", JSValueToBoolean(ctx, argv[0]) ? "true" : "false");
      break;
    case kJSTypeNumber : 
      dout = JSValueToNumber(ctx, argv[0], exc);
      if (dout != NAN) 
        fprintf(stream, "%f\n", dout);
      break;
    default : break;
  }
  return JSValueMakeUndefined(ctx);
}/*}}}*/

/* SIGNAL {{{*/
static bool
signal_set(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef value, JSValueRef* exception) {
  char *name = js_string_to_char(ctx, js_name, -1);
  JSObjectRef o;
  if (name == NULL)
    return false;

  for (int i = SCRIPT_SIG_FIRST; i<SCRIPT_SIG_LAST; i++) {
    if (strcmp(name, _sigmap[i].name)) 
      continue;
    if (JSValueIsNull(ctx, value)) 
      dwb.misc.script_signals &= ~(1<<i);
    else if ( (o = JSValueToObject(ctx, value, exception)) != NULL && JSObjectIsFunction(ctx, o)) {
      _sigObjects[i] = o;
      dwb.misc.script_signals |= (1<<i);
    }
    return true;
  }
  return false;
}

gboolean
scripts_emit(JSObjectRef obj, int signal, const char *json) {
  JSObjectRef function = _sigObjects[signal];
  if (function == NULL)
    return false;

  JSStringRef js_json = JSStringCreateWithUTF8CString(json);
  JSValueRef vson = JSValueMakeFromJSONString(_global_context, js_json);
  JSValueRef val[] = { obj, vson ? vson : JSValueMakeNull(_global_context) };
  JSStringRelease(js_json);
  
  JSValueRef js_ret = JSObjectCallAsFunction(_global_context, function, NULL, 2, val, NULL);
  if (JSValueIsBoolean(_global_context, js_ret))
    return JSValueToBoolean(_global_context, js_ret);
  return false;
}/*}}}*/

static void 
create_global_object() {
  JSStaticFunction global_functions[] = { 
    { "execute",         global_execute,         kJSDefaultFunction },
    { "include",         global_include,         kJSDefaultFunction },
    { "timerStart",    global_timer_start,         kJSDefaultFunction },
    { "timerStop",     global_timer_stop,         kJSDefaultFunction },
    { 0, 0, 0 }, 
  };

  JSClassRef class = create_class("dwb", global_functions, NULL);
  _global_context = JSGlobalContextCreate(class);
  JSClassRelease(class);


  JSObjectRef global_object = JSContextGetGlobalObject(_global_context);

  JSStaticValue data_values[] = {
    { "profile",        data_get_profile, NULL, kJSDefaultFunction },
    { "cacheDir",       data_get_cache_dir, NULL, kJSDefaultFunction },
    { "configDir",      data_get_config_dir, NULL, kJSDefaultFunction },
    { "systemDataDir",  data_get_system_data_dir, NULL, kJSDefaultFunction },
    { "userDataDir",    data_get_user_data_dir, NULL, kJSDefaultFunction },
    { 0, 0, 0,  0 }, 
  };
  class = create_class("data", NULL, data_values);
  create_object(_global_context, class, global_object, "data", NULL);

  JSStaticFunction wv_functions[] = { 
    { "set",             tab_set,             kJSDefaultFunction },
    { "get",             tab_get,             kJSDefaultFunction },
    { "history",         tab_history,             kJSDefaultFunction },
    { "reload",          tab_reload,             kJSDefaultFunction },
    { "inject",          tab_inject,             kJSDefaultFunction },
    { 0, 0, 0 }, 
  };
  JSStaticValue wv_values[] = {
    { "uri",    tab_get_property, tab_set_uri, kJSPropertyAttributeDontDelete },
    { "title",  tab_get_property, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly  },
    { "domain", tab_get_domain, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly  },
    { "host",   tab_get_host, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly  },
    { 0, 0, 0 }, 
  };
  _view_class = create_class("webview", wv_functions, wv_values);


  JSStaticFunction io_functions[] = { 
    { "print",     io_print,            kJSDefaultFunction },
    { "read",      io_read,             kJSDefaultFunction },
    { "write",     io_write,            kJSDefaultFunction },
    { 0,           0,           0 },
  };
  class = create_class("io", io_functions, NULL);
  create_object(_global_context, class, global_object, "io", NULL);

  JSStaticFunction system_functions[] = { 
    { "spawn",           system_spawn,           kJSDefaultFunction },
    { "getEnv",          system_get_env,           kJSDefaultFunction },
    { 0, 0, 0 }, 
  };
  class = create_class("system", system_functions, NULL);
  create_object(_global_context, class, global_object, "system", NULL);

  JSStaticFunction tab_functions[] = { 
    { "nth",          tabs_get_nth,        kJSDefaultFunction },
    { 0, 0, 0 }, 
  };
  JSStaticValue tab_values[] = { 
    { "current",      tabs_current, NULL,   kJSDefaultFunction },
    { "number",       tabs_number,  NULL,   kJSDefaultFunction },
    { "length",       tabs_length,  NULL,   kJSDefaultFunction },
    { 0, 0, 0, 0 }, 
  };
  class = create_class("tabs", tab_functions, tab_values);
  create_object(_global_context, class, global_object, "tabs", NULL);

  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.setProperty = signal_set;
  class = JSClassCreate(&cd);
  
  create_object(_global_context, class, global_object, "signals", NULL);

  class = create_class("extensions", NULL, NULL);
  create_object(_global_context, class, global_object, "extensions", NULL);
}
void 
scripts_create_tab(GList *gl) {
  if (_global_context == NULL )  {
    VIEW(gl)->script = NULL;
    return;
  }
  if (_global_context == NULL ) 
    create_global_object();
  VIEW(gl)->script = JSObjectMake(_global_context, _view_class, gl);
}
JSObjectRef 
scripts_create_frame(WebKitWebFrame *frame) {
  JSStaticFunction functions[] = { 
    { "inject",          frame_inject,             kJSDefaultFunction },
    { 0, 0, 0 }, 
  };
  JSStaticValue values[] = { 
    { "uri",      object_get_property, NULL,   kJSDefaultFunction },
    { "title",    object_get_property, NULL,   kJSDefaultFunction },
    { "name",    object_get_property, NULL,   kJSDefaultFunction },
    { 0, 0, 0, 0 }, 
  };
  JSClassRef class = create_class("frame", functions, values);
  return JSObjectMake(_global_context, class, frame);
}

void
scripts_init_script(const char *script) {
  puts(script);
  if (_global_context == NULL) 
    create_global_object();
  JSStringRef body = JSStringCreateWithUTF8CString(script);
  JSObjectRef function = JSObjectMakeFunction(_global_context, NULL, 0, NULL, body, NULL, 0, NULL);
  if (function != NULL) {
    _script_list = g_slist_append(_script_list, function);
  }
  else {
    fprintf(stderr, "An error occured creating script : \n%s\n", script);
  }
  JSStringRelease(body);
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
    evaluate(content->str);
    g_string_free(content, true);
    g_free(dir);
  }

  for (GSList *l = _script_list; l; l=l->next) {
    JSObjectCallAsFunction(_global_context, l->data, NULL, 0, NULL, NULL);
  }
  g_slist_free(_script_list);
  _script_list = NULL;
}
void
scripts_end() {
  if (_global_context != NULL) {
    JSClassRelease(_view_class);
    JSGlobalContextRelease(_global_context);
    _global_context = NULL;
  }
}
