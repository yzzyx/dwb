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
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <JavaScriptCore/JavaScript.h>
#include "dwb.h"
#include "scripts.h" 
#include "util.h" 
#include "js.h" 
#include "soup.h" 
#include "domain.h" 
//#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete )
#define kJSDefaultProperty  (kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly )
#define kJSDefaultAttributes  (kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly )

#define SCRIPT_WEBVIEW(o) (WEBVIEW(((GList*)JSObjectGetPrivate(o))))
#define EXCEPTION(X)   "DWB EXCEPTION : "X
#define PROP_LENGTH 128

typedef struct _Sigmap {
  int sig;
  const char *name;
} Sigmap;

typedef struct _CallbackData CallbackData;
typedef gboolean (*StopCallbackNotify)(CallbackData *);

struct _CallbackData {
  GObject *gobject;
  JSObjectRef object;
  JSObjectRef callback;
  StopCallbackNotify notify;
};

static Sigmap _sigmap[] = {
  { SCRIPTS_SIG_NAVIGATION, "navigation" },
  { SCRIPTS_SIG_LOAD_STATUS, "loadStatus" },
  { SCRIPTS_SIG_MIME_TYPE, "mimeType" },
  { SCRIPTS_SIG_DOWNLOAD, "download" }, 
  { SCRIPTS_SIG_DOWNLOAD_STATUS, "downloadStatus" }, 
  { SCRIPTS_SIG_RESOURCE, "resource" },
  { SCRIPTS_SIG_KEY_PRESS, "keyPress" },
  { SCRIPTS_SIG_KEY_RELEASE, "keyRelease" },
  { SCRIPTS_SIG_BUTTON_PRESS, "buttonPress" },
  { SCRIPTS_SIG_BUTTON_RELEASE, "buttonRelease" },
  { SCRIPTS_SIG_TAB_FOCUS, "tabFocus" },
  { SCRIPTS_SIG_FRAME_STATUS, "frameStatus" },
  { SCRIPTS_SIG_LOAD_FINISHED, "loadFinished" },
  { SCRIPTS_SIG_LOAD_COMMITTED, "loadCommitted" },
};


static JSValueRef wv_load_uri(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_history(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_reload(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSStaticFunction wv_functions[] = { 
    { "loadUri",         wv_load_uri,             kJSDefaultAttributes },
    { "history",         wv_history,             kJSDefaultAttributes },
    { "reload",          wv_reload,             kJSDefaultAttributes },
    { "inject",          wv_inject,             kJSDefaultAttributes },
    { 0, 0, 0 }, 
};

static JSValueRef frame_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSStaticFunction frame_functions[] = { 
  { "inject",          frame_inject,             kJSDefaultAttributes },
  { 0, 0, 0 }, 
};

static JSValueRef download_start(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef download_cancel(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSStaticFunction download_functions[] = { 
  { "start",          download_start,        kJSDefaultAttributes },
  { "cancel",         download_cancel,        kJSDefaultAttributes },
  { 0, 0, 0 }, 
};
enum {
  SPAWN_SUCCESS = 0, 
  SPAWN_FAILED = 1<<0, 
  SPAWN_STDOUT_FAILED = 1<<1, 
  SPAWN_STDERR_FAILED = 1<<2, 
};


static void callback(CallbackData *c);
static void make_callback(JSContextRef ctx, JSObjectRef this, GObject *gobject, const char *signalname, JSValueRef value, StopCallbackNotify notify, JSValueRef *exception);

static JSValueRef get_property(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSObjectRef _sigObjects[SCRIPTS_SIG_LAST];
static JSGlobalContextRef _global_context;
static GSList *_script_list;
static JSClassRef _webview_class, _frame_class, _download_class, _default_class, _download_class;

static CallbackData * 
callback_data_new(GObject *gobject, JSObjectRef object, JSObjectRef callback, StopCallbackNotify notify)  {
  CallbackData *c = g_malloc(sizeof(CallbackData));
  c->gobject = gobject != NULL ? g_object_ref(gobject) : NULL;
  if (object != NULL) {
    JSValueProtect(_global_context, object);
    c->object = object;
  }
  if (object != NULL) {
    JSValueProtect(_global_context, callback);
    c->callback = callback;
  }
  c->notify = notify;
  return c;
}
static void
callback_data_free(CallbackData *c) {
  if (c != NULL) {
    if (c->gobject != NULL) 
      g_object_unref(c->gobject);
    if (c->object != NULL) 
      JSValueUnprotect(_global_context, c->object);
    JSValueUnprotect(_global_context, c->object);
    if (c->object != NULL) 
      JSValueUnprotect(_global_context, c->callback);
    g_free(c);
  }
}
static void 
callback(CallbackData *c) {
  gboolean ret = false;
  JSValueRef val[] = { c->object != NULL ? c->object : JSValueMakeNull(_global_context) };
  JSValueRef jsret = JSObjectCallAsFunction(_global_context, c->callback, NULL, 1, val, NULL);
  if (JSValueIsBoolean(_global_context, jsret))
    ret = JSValueToBoolean(_global_context, jsret);
  if (ret || (c != NULL && c->gobject != NULL && c->notify != NULL && c->notify(c))) {
    g_signal_handlers_disconnect_by_func(c->gobject, callback, c);
    callback_data_free(c);
  }
}
static void 
make_callback(JSContextRef ctx, JSObjectRef this, GObject *gobject, const char *signalname, JSValueRef value, StopCallbackNotify notify, JSValueRef *exception) {
  JSObjectRef func = JSValueToObject(ctx, value, exception);
  if (func != NULL && JSObjectIsFunction(ctx, func)) {
    CallbackData *c = callback_data_new(gobject, this, func, notify);
    g_signal_connect_swapped(gobject, signalname, G_CALLBACK(callback), c);
  }
}

char *
uncamelize(char *uncamel, const char *camel, char rep, size_t length) {
  char *ret = uncamel;
  size_t written = 0;
  if (isupper(*camel) && length > 1) {
    *uncamel++ = tolower(*camel++);
    written++;
  }
  while (written++<length-1 && *camel != 0) {
    if (isupper(*camel)) {
      *uncamel++ = rep;
      if (++written >= length-1)
        break;
      *uncamel++ = tolower(*camel++);
    }
    else {
      *uncamel++ = *camel++;
    }
  }
  while (!(*uncamel = 0) && written++<length-1);
  return ret;
}

void 
evaluate(const char *script) {
  if (script == NULL)
    return;
  JSStringRef js_script = JSStringCreateWithUTF8CString(script);
  JSEvaluateScript(_global_context, js_script, NULL, NULL, 0, NULL);
  JSStringRelease(js_script);
}

bool
set_property_cb(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
  return true;
}

static JSClassRef 
create_class(const char *name, JSStaticFunction staticFunctions[], JSStaticValue staticValues[]) {
  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.className = name;
  cd.staticFunctions = staticFunctions;
  cd.staticValues = staticValues;
  cd.setProperty = set_property_cb;
  return JSClassCreate(&cd);
}
static JSObjectRef 
create_object(JSContextRef ctx, JSClassRef class, JSObjectRef obj, JSClassAttributes attr, const char *name, void *private) {
  JSObjectRef ret = JSObjectMake(ctx, class, private);
  JSStringRef js_name = JSStringCreateWithUTF8CString(name);
  JSObjectSetProperty(ctx, obj, js_name, ret, attr, NULL);
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

/* TABS {{{*/
static JSValueRef 
tabs_current(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) {
  return scripts_make_object(_global_context, NULL, G_OBJECT(CURRENT_WEBVIEW()));
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

static void make_callback(JSContextRef ctx, JSObjectRef this, GObject *gobject, const char *signalname, JSValueRef value, StopCallbackNotify notify, JSValueRef *exception);
static gboolean 
wv_status_cb(CallbackData *c) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(WEBKIT_WEB_VIEW(c->gobject));
  if (status == WEBKIT_LOAD_FINISHED || status == WEBKIT_LOAD_FAILED) {
    return true;
  }
  return false;
}

static JSValueRef 
wv_load_uri(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc == 0) {
    js_make_exception(ctx, exc, EXCEPTION("webview.loadUri: missing argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  WebKitWebView *wv = JSObjectGetPrivate(this);

  if (wv != NULL) {
    char *uri = js_value_to_char(ctx, argv[0], -1, exc);
    if (uri == NULL)
      return false;
    webkit_web_view_load_uri(wv, uri);
    g_free(uri);
    if (argc > 1)  {
      make_callback(ctx, this, G_OBJECT(wv), "notify::load-status", argv[1], wv_status_cb, exc);
    }
    return JSValueMakeBoolean(ctx, true);
  }
  return false;
}
static JSValueRef 
wv_history(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = JSValueMakeUndefined(ctx);
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("webview.history: missing argument."));
    return ret;
  }
  double steps = JSValueToNumber(ctx, argv[0], exc);
  if (steps != NAN) {
    webkit_web_view_go_back_or_forward(JSObjectGetPrivate(this), (int)steps);
  }
  return ret;
}
static JSValueRef 
wv_reload(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  webkit_web_view_reload(JSObjectGetPrivate(this));
  return JSValueMakeUndefined(ctx);
}
static JSValueRef 
wv_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitWebView *wv = JSObjectGetPrivate(this);
  JSContextRef wctx = webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(wv));
  return inject(ctx, wctx, function, this, argc, argv, exc);
}
bool
set_gobject_property(JSContextRef ctx, GObject *o, JSStringRef js_name, JSValueRef jsvalue, JSValueRef* exception) {
  char buf[PROP_LENGTH];
  char *name = js_string_to_char(ctx, js_name, -1);
  if (name == NULL)
    return false;
  uncamelize(buf, name, '-', PROP_LENGTH);
  g_free(name);

  GObjectClass *class = G_OBJECT_GET_CLASS(o);
  GParamSpec *pspec = g_object_class_find_property(class, buf);

  if (pspec == NULL)
    return false;
  if (! (pspec->flags & G_PARAM_WRITABLE))
    return false;
  int jstype = JSValueGetType(ctx, jsvalue);
  GType gtype = G_TYPE_IS_FUNDAMENTAL(pspec->value_type) ? pspec->value_type : g_type_parent(pspec->value_type);

  if (jstype == kJSTypeNumber && 
      (gtype == G_TYPE_INT || gtype == G_TYPE_UINT || gtype == G_TYPE_LONG || gtype == G_TYPE_ULONG ||
      gtype == G_TYPE_FLOAT || gtype == G_TYPE_DOUBLE || gtype == G_TYPE_ENUM || gtype == G_TYPE_INT64 ||
      gtype == G_TYPE_UINT64))  {
    double value = JSValueToNumber(ctx, jsvalue, exception);
    if (value != NAN) {
      g_object_set(o, buf, value, NULL);
      return true;
    }
    return false;
  }
  else if (jstype == kJSTypeBoolean && gtype == G_TYPE_BOOLEAN) {
    bool value = JSValueToBoolean(ctx, jsvalue);
    g_object_set(o, buf, value, NULL);
    return true;
  }
  else if (jstype == kJSTypeString && gtype == G_TYPE_STRING) {
    char *value = js_value_to_char(ctx, jsvalue, -1, exception);
    g_object_set(o, buf, value, NULL);
    g_free(value);
    return true;
  }
  return false;
  // TODO object
}
bool
set_property(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
  return set_gobject_property(ctx, G_OBJECT(JSObjectGetPrivate(object)), propertyName, value, exception);
}
JSValueRef
get_gobject_property(JSContextRef ctx, JSObjectRef jsobj, GObject *o, JSStringRef js_name, JSValueRef *exception) {
  char buf[PROP_LENGTH];
  char keyint[PROP_LENGTH + 8];
  JSValueRef ret = NULL;
  char *name = js_string_to_char(ctx, js_name, -1);
  if (name == NULL)
    return NULL;
  uncamelize(buf, name, '-', PROP_LENGTH);
  g_free(name);

  GObjectClass *class = G_OBJECT_GET_CLASS(o);
  GParamSpec *pspec = g_object_class_find_property(class, buf);
  if (pspec == NULL)
    goto error_out;
  if (! (pspec->flags & G_PARAM_READABLE))
    goto error_out;

  GType gtype = pspec->value_type, act; 
  while ((act = g_type_parent(gtype))) {
    gtype = act;
  }
#define CHECK_NUMBER(GTYPE, TYPE) G_STMT_START if (gtype == G_TYPE_##GTYPE) { \
  TYPE value; g_object_get(o, buf, &value, NULL); return JSValueMakeNumber(ctx, (double)value); \
}    G_STMT_END
  CHECK_NUMBER(INT, gint);
  CHECK_NUMBER(UINT, guint);
  CHECK_NUMBER(LONG, glong);
  CHECK_NUMBER(ULONG, gulong);
  CHECK_NUMBER(FLOAT, gfloat);
  CHECK_NUMBER(DOUBLE, gdouble);
  CHECK_NUMBER(ENUM, gint);
  CHECK_NUMBER(INT64, gint64);
  CHECK_NUMBER(UINT64, guint64);
  CHECK_NUMBER(FLAGS, guint);
#undef CHECK_NUMBER
  if (pspec->value_type == G_TYPE_BOOLEAN) {
    gboolean bval;
    g_object_get(o, buf, &bval, NULL);
    ret = JSValueMakeBoolean(ctx, bval);
  }
  else if (pspec->value_type == G_TYPE_STRING) {
    char *value;
    g_object_get(o, buf, &value, NULL);
    ret = js_char_to_value(ctx, value);
    g_free(value);
  }
  else if (G_TYPE_IS_CLASSED(gtype)) {
    GObject *object;
    g_object_get(o, buf, &object, NULL);
    if (object == NULL)
      return NULL;
    strcpy(keyint, "DWB_INT_");
    strcat(keyint, buf);
    JSObjectRef retobj = g_object_get_data(object, keyint);
    if (retobj == NULL) {
      retobj = scripts_make_object(ctx, NULL, object);
    }
    g_object_unref(object);
    ret = retobj;
  }
error_out:
 return ret;
}
void
object_finalize(JSObjectRef o) {
}
// TODO : creating 1000000 objects leaks ~ 4MB  
JSObjectRef 
scripts_make_object(JSContextRef ctx, const char *key, GObject *o) {
  static int counter;
  printf("%d\n", counter++);
  if (o == NULL) {
    JSValueRef v = JSValueMakeNull(ctx);
    return JSValueToObject(ctx, v, NULL);
  }
  JSClassRef class;
  if (WEBKIT_IS_WEB_VIEW(o)) 
     class = _webview_class;
  else if (WEBKIT_IS_WEB_FRAME(o))
     class = _frame_class;
  else if (WEBKIT_IS_DOWNLOAD(o)) 
    class = _download_class;
  else 
    class = _default_class;

  JSObjectRef retobj = JSObjectMake(ctx, class, o);
  //if (key != NULL) {
  //  GQuark quark = g_quark_from_string(key);
  //  g_object_set_qdata(o, quark, (gpointer*)retobj);
  //}
  //JSClassRelease(jsclass);
  return retobj;
}
JSValueRef 
get_property(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  return get_gobject_property(ctx, object, G_OBJECT(JSObjectGetPrivate(object)), js_name, exception);
}

#if 0
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
#endif

static JSValueRef 
frame_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitWebFrame *frame = JSObjectGetPrivate(this);
  JSContextRef wctx = webkit_web_frame_get_global_context(frame);
  return inject(ctx, wctx, function, this, argc, argv, exc);
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
      ret = JSObjectCallAsFunction(ctx, function, thisObject, 0, NULL, exc);
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
static JSValueRef 
global_domain_from_host(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("domainFromHost: missing argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  char *host = js_value_to_char(ctx, argv[0], -1, exc);
  const char *domain = domain_get_base_for_host(host);
  if (domain == NULL)
    return JSValueMakeNull(ctx);
  JSValueRef ret = js_char_to_value(ctx, domain);
  g_free(host);
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
      return true;
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
  char *json = NULL;
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
    case kJSTypeUndefined : 
      fprintf(stream, "undefined\n");
      break;
    case kJSTypeNull : 
      fprintf(stream, "null\n");
      break;
    case kJSTypeObject : 
      json = js_value_to_json(ctx, argv[0], -1, NULL);
      if (json != NULL) {
        fprintf(stream, "%s\n", json);
        g_free(json);
      }
      break;
    default : break;
  }
  return JSValueMakeUndefined(ctx);
}/*}}}*/

JSObjectRef 
download_constructor_cb(JSContextRef ctx, JSObjectRef constructor, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
  if (argc<1) {
    js_make_exception(ctx, exception, EXCEPTION("Download constructor: missing argument"));
    return JSValueToObject(ctx, JSValueMakeNull(ctx), NULL);
  }
  char *uri = js_value_to_char(ctx, argv[0], -1, exception);
  if (uri == NULL) {
    js_make_exception(ctx, exception, EXCEPTION("Download constructor: invalid argument"));
    return JSValueToObject(ctx, JSValueMakeNull(ctx), NULL);
  }
  WebKitNetworkRequest *request = webkit_network_request_new(uri);
  g_free(uri);
  if (request == NULL) {
    js_make_exception(ctx, exception, EXCEPTION("Download constructor: invalid uri"));
    return JSValueToObject(ctx, JSValueMakeNull(ctx), NULL);
  }
  WebKitDownload *download = webkit_download_new(request);
  return JSObjectMake(ctx, _download_class, download);
}

static gboolean 
stop_download_notify(CallbackData *c) {
  WebKitDownloadStatus status = webkit_download_get_status(WEBKIT_DOWNLOAD(c->gobject));
  if (status == WEBKIT_DOWNLOAD_STATUS_ERROR || status == WEBKIT_DOWNLOAD_STATUS_CANCELLED || status == WEBKIT_DOWNLOAD_STATUS_FINISHED) {
    return true;
  }
  return false;
}
static JSValueRef 
download_start(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitDownload *download = JSObjectGetPrivate(this);
  if (webkit_download_get_destination_uri(download) == NULL) {
    js_make_exception(ctx, exc, EXCEPTION("Download.start: destination == null"));
    return JSValueMakeBoolean(ctx, false);
  }
  if (argc > 0) {
    make_callback(ctx, this, G_OBJECT(download), "notify::status", argv[0], stop_download_notify, exc);
  }
  webkit_download_start(download);
  return JSValueMakeBoolean(ctx, true);

}
static JSValueRef 
download_cancel(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  webkit_download_cancel(JSObjectGetPrivate(this));
  return JSValueMakeUndefined(ctx);
}
/* SIGNAL {{{*/
static bool
signal_set(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef value, JSValueRef* exception) {
  char *name = js_string_to_char(ctx, js_name, -1);
  JSObjectRef o;
  if (name == NULL)
    return false;

  for (int i = SCRIPTS_SIG_FIRST; i<SCRIPTS_SIG_LAST; i++) {
    if (strcmp(name, _sigmap[i].name)) 
      continue;
    if (JSValueIsNull(ctx, value)) {
      dwb.misc.script_signals &= ~(1<<i);
    }
    else if ( (o = JSValueToObject(ctx, value, exception)) != NULL && JSObjectIsFunction(ctx, o)) {
      _sigObjects[i] = o;
      dwb.misc.script_signals |= (1<<i);
    }
    break;
  }
  return false;
}

gboolean
scripts_emit(ScriptSignal *sig) {
  JSObjectRef function = _sigObjects[sig->signal];
  if (function == NULL)
    return false;
  int numargs = MIN(sig->numobj, SCRIPT_MAX_SIG_OBJECTS)+2;
  JSValueRef val[numargs];
  int i = 0;
  val[i++] = sig->jsobj;
  for (int j=0; j<sig->numobj; j++) {
    val[i++] = scripts_make_object(_global_context, NULL, G_OBJECT(sig->objects[j]));
  }
  JSValueRef vson = NULL;
  JSStringRef js_json = JSStringCreateWithUTF8CString(sig->json == NULL ? "{}" : sig->json);
  vson = JSValueMakeFromJSONString(_global_context, js_json);
  JSStringRelease(js_json);
  val[i++] = vson == NULL ? JSValueMakeNull(_global_context) : vson;
  JSValueRef js_ret = JSObjectCallAsFunction(_global_context, function, NULL, numargs, val, NULL);
  if (JSValueIsBoolean(_global_context, js_ret)) {
    return JSValueToBoolean(_global_context, js_ret);
  }
  return false;
}

static void 
create_global_object() {
  JSStaticFunction global_functions[] = { 
    { "execute",         global_execute,         kJSDefaultAttributes },
    { "include",         global_include,         kJSDefaultAttributes },
    { "timerStart",    global_timer_start,         kJSDefaultAttributes },
    { "timerStop",     global_timer_stop,         kJSDefaultAttributes },
    { "domainFromHost",     global_domain_from_host,         kJSDefaultAttributes },
    { 0, 0, 0 }, 
  };

  JSClassRef class = create_class("dwb", global_functions, NULL);
  _global_context = JSGlobalContextCreate(class);
  JSClassRelease(class);


  JSObjectRef global_object = JSContextGetGlobalObject(_global_context);

  JSStaticValue data_values[] = {
    { "profile",        data_get_profile, NULL, kJSDefaultAttributes },
    { "cacheDir",       data_get_cache_dir, NULL, kJSDefaultAttributes },
    { "configDir",      data_get_config_dir, NULL, kJSDefaultAttributes },
    { "systemDataDir",  data_get_system_data_dir, NULL, kJSDefaultAttributes },
    { "userDataDir",    data_get_user_data_dir, NULL, kJSDefaultAttributes },
    { 0, 0, 0,  0 }, 
  };
  class = create_class("data", NULL, data_values);
  create_object(_global_context, class, global_object, kJSDefaultAttributes, "data", NULL);
  JSClassRelease(class);

  JSStaticFunction io_functions[] = { 
    { "print",     io_print,            kJSDefaultAttributes },
    { "read",      io_read,             kJSDefaultAttributes },
    { "write",     io_write,            kJSDefaultAttributes },
    { 0,           0,           0 },
  };
  class = create_class("io", io_functions, NULL);
  create_object(_global_context, class, global_object, kJSDefaultAttributes, "io", NULL);
  JSClassRelease(class);

  JSStaticFunction system_functions[] = { 
    { "spawn",           system_spawn,           kJSDefaultAttributes },
    { "getEnv",          system_get_env,           kJSDefaultAttributes },
    { 0, 0, 0 }, 
  };
  class = create_class("system", system_functions, NULL);
  create_object(_global_context, class, global_object, kJSDefaultAttributes, "system", NULL);
  JSClassRelease(class);

  JSStaticFunction tab_functions[] = { 
    { "nth",          tabs_get_nth,        kJSDefaultAttributes },
    { 0, 0, 0 }, 
  };
  JSStaticValue tab_values[] = { 
    { "current",      tabs_current, NULL,   kJSDefaultAttributes },
    { "number",       tabs_number,  NULL,   kJSDefaultAttributes },
    { "length",       tabs_length,  NULL,   kJSDefaultAttributes },
    { 0, 0, 0, 0 }, 
  };
  class = create_class("tabs", tab_functions, tab_values);
  create_object(_global_context, class, global_object, kJSDefaultAttributes, "tabs", NULL);
  JSClassRelease(class);

  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.className = "signals";
  cd.setProperty = signal_set;
  class = JSClassCreate(&cd);
  
  create_object(_global_context, class, global_object, kJSDefaultAttributes, "signals", NULL);
  JSClassRelease(class);

  class = create_class("extensions", NULL, NULL);
  create_object(_global_context, class, global_object, kJSDefaultAttributes, "extensions", NULL);
  JSClassRelease(class);

  cd = kJSClassDefinitionEmpty;
  cd.className = "globals";
  class = JSClassCreate(&cd);
  create_object(_global_context, class, global_object, kJSPropertyAttributeDontDelete, "globals", NULL);
  class = JSClassCreate(&cd);

  /* Default gobject class */
  cd = kJSClassDefinitionEmpty;
  cd.getProperty = get_property;
  cd.setProperty = set_property;
  cd.finalize = object_finalize;

  _default_class = JSClassCreate(&cd);

  /* Webview */
  cd.staticFunctions = wv_functions;
  _webview_class = JSClassCreate(&cd);

  /* Frame */
  cd.staticFunctions = frame_functions;
  _frame_class = JSClassCreate(&cd);

  /* download */
  cd.className = "Download";
  cd.staticFunctions = download_functions;
  _download_class = JSClassCreate(&cd);
  JSObjectRef constructor = JSObjectMakeConstructor(_global_context, _download_class, download_constructor_cb);
  JSStringRef name = JSStringCreateWithUTF8CString("Download");
  JSObjectSetProperty(_global_context, JSContextGetGlobalObject(_global_context), name, constructor, kJSDefaultProperty, NULL);
  JSStringRelease(name);
}
void 
scripts_create_tab(GList *gl) {
  if (_global_context == NULL )  {
    VIEW(gl)->script = NULL;
    return;
  }
  JSObjectRef o = scripts_make_object(_global_context, NULL, G_OBJECT(VIEW(gl)->web));
  JSValueProtect(_global_context, o);
  VIEW(gl)->script_wv = o;
}
void 
scripts_remove_tab(JSObjectRef obj) {
  if (obj != NULL) {
    JSValueUnprotect(_global_context, obj);
  }
}
gboolean
print_exception(JSContextRef ctx, JSValueRef exception) {
  JSValueRef exc = NULL;
  if (exception == NULL) 
    return false;
  if (!JSValueIsObject(ctx, exception))
    return false;
  JSObjectRef o = JSValueToObject(ctx, exception, NULL);
  if (o == NULL) 
    return false;

  gint line = (int)js_get_double_property(ctx, o, "line");
  if (exc != NULL)
    return false;
  gchar *message = js_get_string_property(ctx, o, "message");
  if (exc != NULL)
    return false;
  fprintf(stderr, "DWB SCRIPT EXCEPTION: in line %d: %s\n", line, message);
  return true;
}

void
scripts_init_script(const char *script) {
  if (_global_context == NULL) 
    create_global_object();
  JSStringRef body = JSStringCreateWithUTF8CString(script);
  JSValueRef exc = NULL;
  JSObjectRef function = JSObjectMakeFunction(_global_context, NULL, 0, NULL, body, NULL, 0, &exc);
  if (function != NULL) {
    _script_list = g_slist_prepend(_script_list, function);
  }
  else {
    print_exception(_global_context, exc);
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

  JSStringRef on_init = JSStringCreateWithUTF8CString("init");
  JSValueRef ret, init;
  JSObjectRef retobj, initobj;
  GSList *scripts = NULL;
  for (GSList *l = _script_list; l; l=l->next) {
    JSObjectRef o = JSObjectMake(_global_context, NULL, NULL);
    if ( (ret = JSObjectCallAsFunction(_global_context, l->data, o, 0, NULL, NULL)) == NULL)
      continue;
    if ( (retobj = JSValueToObject(_global_context, ret, NULL)) == NULL)
      continue;
    if ( (init = JSObjectGetProperty(_global_context, retobj, on_init, NULL)) == NULL)
      continue;
    if ((initobj = JSValueToObject(_global_context, init, NULL)) != NULL && JSObjectIsFunction(_global_context, initobj)) {
      scripts = g_slist_prepend(scripts, initobj);
    }
  }
  g_slist_free(_script_list);
  for (GSList *l = scripts; l; l=l->next) {
    JSObjectCallAsFunction(_global_context, l->data, NULL, 0, NULL, NULL);
  }
  g_slist_free(scripts);
  _script_list = NULL;
  JSStringRelease(on_init);
}
void
scripts_end() {
  if (_global_context != NULL) {
    JSClassRelease(_default_class);
    JSClassRelease(_webview_class);
    JSClassRelease(_frame_class);
    JSClassRelease(_download_class);
    JSGlobalContextRelease(_global_context);
    _global_context = NULL;
  }
}
