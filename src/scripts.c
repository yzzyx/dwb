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
#include <glib.h>
#include "dwb.h"
#include "scripts.h" 
#include "util.h" 
#include "js.h" 
#include "soup.h" 
#include "domain.h" 
#include "application.h" 
#include "completion.h" 
#include "entry.h" 
#include "scratchpad.h" 
//#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete )
#define kJSDefaultProperty  (kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly )
#define kJSDefaultAttributes  (kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly )

#define SCRIPT_WEBVIEW(o) (WEBVIEW(((GList*)JSObjectGetPrivate(o))))
#define EXCEPTION(X)   "DWB EXCEPTION : "X
#define PROP_LENGTH 128
#define G_FILE_TEST_VALID (G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK | G_FILE_TEST_IS_DIR | G_FILE_TEST_IS_EXECUTABLE | G_FILE_TEST_EXISTS)

typedef struct m_Sigmap {
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

static Sigmap m_sigmap[] = {
  { SCRIPTS_SIG_NAVIGATION, "navigation" },
  { SCRIPTS_SIG_LOAD_STATUS, "loadStatus" },
  { SCRIPTS_SIG_MIME_TYPE, "mimeType" },
  { SCRIPTS_SIG_DOWNLOAD, "download" }, 
  { SCRIPTS_SIG_DOWNLOAD_START, "downloadStart" }, 
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
  { SCRIPTS_SIG_HOVERING_OVER_LINK, "hoveringOverLink" },
  { SCRIPTS_SIG_CLOSE_TAB, "closeTab" },
  { SCRIPTS_SIG_CREATE_TAB, "createTab" },
  { SCRIPTS_SIG_FRAME_CREATED, "frameCreated" },
  { SCRIPTS_SIG_CLOSE, "close" },
  { SCRIPTS_SIG_DOCUMENT_LOADED, "documentLoaded" },
  { SCRIPTS_SIG_MOUSE_MOVE, "mouseMove" },
  { SCRIPTS_SIG_STATUS_BAR, "statusBarChange" },
  { SCRIPTS_SIG_CHANGE_MODE, "changeMode" },
  { 0, NULL },
};


static JSValueRef connect_object(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef disconnect_object(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSValueRef wv_load_uri(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_set_title(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_history(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_reload(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSStaticFunction default_functions[] = { 
    { "connect",         connect_object,             kJSDefaultAttributes },
    { "disconnect",      disconnect_object,             kJSDefaultAttributes },
    { 0, 0, 0 }, 
};
static JSStaticFunction wv_functions[] = { 
    { "loadUri",         wv_load_uri,             kJSDefaultAttributes },
    { "setTitle",        wv_set_title,             kJSDefaultAttributes },
    { "history",         wv_history,             kJSDefaultAttributes },
    { "reload",          wv_reload,             kJSDefaultAttributes },
    { "inject",          wv_inject,             kJSDefaultAttributes },
    { 0, 0, 0 }, 
};
static JSValueRef wv_get_main_frame(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_focused_frame(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_all_frames(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_number(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSStaticValue wv_values[] = {
  { "mainFrame", wv_get_main_frame, NULL, kJSDefaultAttributes }, 
  { "focusedFrame", wv_get_focused_frame, NULL, kJSDefaultAttributes }, 
  { "allFrames",  wv_get_all_frames, NULL, kJSDefaultAttributes }, 
  { "number",     wv_get_number, NULL, kJSDefaultAttributes }, 
  { 0, 0, 0, 0 }, 
};

static JSValueRef message_get_uri(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef message_get_first_party(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSStaticValue message_values[] = {
  { "uri",     message_get_uri, NULL, kJSDefaultAttributes }, 
  { "firstParty",     message_get_first_party, NULL, kJSDefaultAttributes }, 
  { 0, 0, 0, 0 }, 
};
static JSValueRef sp_show(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef sp_hide(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef sp_load(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef sp_get(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef sp_send(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSValueRef frame_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSStaticFunction frame_functions[] = { 
  { "inject",          frame_inject,             kJSDefaultAttributes },
  { 0, 0, 0 }, 
};
static JSValueRef frame_get_domain(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef frame_get_host(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSStaticValue frame_values[] = {
  { "host", frame_get_host, NULL, kJSDefaultAttributes }, 
  { "domain", frame_get_domain, NULL, kJSDefaultAttributes }, 
  { 0, 0, 0, 0 }, 
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
static JSObjectRef make_object(JSContextRef ctx, GObject *o);

static JSObjectRef m_sig_objects[SCRIPTS_SIG_LAST];
static JSGlobalContextRef m_global_context;
static GSList *m_script_list;
static JSClassRef m_webview_class, m_frame_class, m_download_class, m_default_class, m_download_class, m_message_class;
static gboolean m_commandline = false;
static JSObjectRef m_array_contructor;
static JSObjectRef m_completion_callback;
static JSObjectRef m_sp_scripts_cb;
static JSObjectRef m_sp_scratchpad_cb;
static GQuark ref_quark;
static JSObjectRef m_global_init;

/* MISC {{{*/
/* uncamelize {{{*/
static char *
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
  while (!(*uncamel = 0) && written++<length-1)
    uncamel++;
  return ret;
}/*}}}*/

/* inject {{{*/
static JSValueRef
inject(JSContextRef ctx, JSContextRef wctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = NULL;
  gboolean global = false;
  JSValueRef args[1];
  int count = 0;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("webview.inject: missing argument"));
    return JSValueMakeBoolean(ctx, false);
  }
  if (argc > 1 && !JSValueIsNull(ctx, argv[1])) {
    args[0] = js_context_change(ctx, wctx, argv[1], exc);
    count = 1;
  }
  if (argc > 2 && JSValueIsBoolean(ctx, argv[2])) 
    global = JSValueToBoolean(ctx, argv[2]);
    
  JSStringRef script = JSValueToStringCopy(ctx, argv[0], exc);
  if (script == NULL) {
    return JSValueMakeNull(ctx);
  }
  if (global) {
    JSEvaluateScript(wctx, script, NULL, NULL, 0, NULL);
    ret = JSValueMakeNull(ctx);
  }
  else {
    JSObjectRef func = JSObjectMakeFunction(wctx, NULL, 0, NULL, script, NULL, 0, NULL);
    if (func != NULL && JSObjectIsFunction(ctx, func)) {
      JSValueRef wret = JSObjectCallAsFunction(wctx, func, NULL, count, count == 1 ? args : NULL, NULL) ;
      char *retx = js_value_to_json(wctx, wret, -1, NULL);
      if (retx) {
        ret = js_char_to_value(ctx, retx);
        g_free(retx);
      }
      else {
        ret = JSValueMakeUndefined(ctx);
      }
    }
  }
  JSStringRelease(script);
  return ret;
}/*}}}*/
/*}}}*/

/* CALLBACK {{{*/
/* callback_data_new {{{*/
static CallbackData * 
callback_data_new(GObject *gobject, JSObjectRef object, JSObjectRef callback, StopCallbackNotify notify)  {
  CallbackData *c = g_malloc(sizeof(CallbackData));
  c->gobject = gobject != NULL ? g_object_ref(gobject) : NULL;
  if (object != NULL) {
    JSValueProtect(m_global_context, object);
    c->object = object;
  }
  if (object != NULL) {
    JSValueProtect(m_global_context, callback);
    c->callback = callback;
  }
  c->notify = notify;
  return c;
}/*}}}*/

/* callback_data_free {{{*/
static void
callback_data_free(CallbackData *c) {
  if (c != NULL) {
    if (c->gobject != NULL) 
      g_object_unref(c->gobject);
    if (c->object != NULL) 
      JSValueUnprotect(m_global_context, c->object);
    JSValueUnprotect(m_global_context, c->object);
    if (c->object != NULL) 
      JSValueUnprotect(m_global_context, c->callback);
    g_free(c);
  }
}/*}}}*/

/* make_callback {{{*/
static void 
make_callback(JSContextRef ctx, JSObjectRef this, GObject *gobject, const char *signalname, JSValueRef value, StopCallbackNotify notify, JSValueRef *exception) {
  JSObjectRef func = JSValueToObject(ctx, value, exception);
  if (func != NULL && JSObjectIsFunction(ctx, func)) {
    CallbackData *c = callback_data_new(gobject, this, func, notify);
    g_signal_connect_swapped(gobject, signalname, G_CALLBACK(callback), c);
  }
}/*}}}*/

/* callback {{{*/
static void 
callback(CallbackData *c) {
  gboolean ret = false;
  JSValueRef val[] = { c->object != NULL ? c->object : JSValueMakeNull(m_global_context) };
  JSValueRef jsret = JSObjectCallAsFunction(m_global_context, c->callback, NULL, 1, val, NULL);
  if (JSValueIsBoolean(m_global_context, jsret))
    ret = JSValueToBoolean(m_global_context, jsret);
  if (ret || (c != NULL && c->gobject != NULL && c->notify != NULL && c->notify(c))) {
    g_signal_handlers_disconnect_by_func(c->gobject, callback, c);
    callback_data_free(c);
  }
}/*}}}*/
/*}}}*/

/* TABS {{{*/
/* tabs_current {{{*/
static JSValueRef 
tabs_current(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) {
  if (dwb.state.fview && CURRENT_VIEW()->script_wv) 
    return CURRENT_VIEW()->script_wv;
  else 
    return JSValueMakeNull(ctx);
}/*}}}*/

/* tabs_number {{{*/
static JSValueRef 
tabs_number(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) {
  return JSValueMakeNumber(ctx, g_list_position(dwb.state.views, dwb.state.fview));
}/*}}}*/

/* tabs_length {{{*/
static JSValueRef 
tabs_length(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) {
  return JSValueMakeNumber(ctx, g_list_length(dwb.state.views));
}/*}}}*/

/* tabs_get_nth {{{*/
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
  return VIEW(nth)->script_wv;
}/*}}}*/
/*}}}*/

/* WEBVIEW {{{*/

GList *
find_webview(JSObjectRef o) {
  GList *r = NULL;
  for (r = dwb.state.views; r && VIEW(r)->script_wv != o; r=r->next);
  return r;
}
/* wv_status_cb {{{*/
static gboolean 
wv_status_cb(CallbackData *c) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(WEBKIT_WEB_VIEW(c->gobject));
  if (status == WEBKIT_LOAD_FINISHED || status == WEBKIT_LOAD_FAILED) {
    return true;
  }
  return false;
}/*}}}*/

/* wv_load_uri {{{*/
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
}/*}}}*/

/* wv_history {{{*/
static JSValueRef 
wv_history(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = JSValueMakeUndefined(ctx);
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("webview.history: missing argument."));
    return ret;
  }
  double steps = JSValueToNumber(ctx, argv[0], exc);
  if (steps != NAN) {
    WebKitWebView *wv = JSObjectGetPrivate(this);
    if (wv != NULL)
      webkit_web_view_go_back_or_forward(wv, (int)steps);
  }
  return ret;
}/*}}}*/

/* wv_reload {{{*/
static JSValueRef 
wv_reload(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitWebView *wv = JSObjectGetPrivate(this);
  if (wv != NULL)
    webkit_web_view_reload(wv);
  return JSValueMakeUndefined(ctx);
}/*}}}*/

/* wv_inject {{{*/
static JSValueRef 
wv_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitWebView *wv = JSObjectGetPrivate(this);
  if (wv != NULL) {
    JSContextRef wctx = webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(wv));
    return inject(ctx, wctx, function, this, argc, argv, exc);
  }
  return JSValueMakeNull(ctx);
}/*}}}*/
/* wv_get_main_frame {{{*/
static JSValueRef 
wv_get_main_frame(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  WebKitWebView *wv = JSObjectGetPrivate(object);
  if (wv != NULL) {
    WebKitWebFrame *frame = webkit_web_view_get_main_frame(wv);
    return make_object(ctx, G_OBJECT(frame));
  }
  return JSValueMakeNull(ctx);
}/*}}}*/

/* wv_get_focused_frame {{{*/
static JSValueRef 
wv_get_focused_frame(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  WebKitWebView *wv = JSObjectGetPrivate(object);
  if (wv != NULL) {
    WebKitWebFrame *frame = webkit_web_view_get_focused_frame(wv);
    return make_object(ctx, G_OBJECT(frame));
  }
  return JSValueMakeNull(ctx);
}/*}}}*/

/* wv_get_all_frames {{{*/
static JSValueRef 
wv_get_all_frames(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  GList *gl = find_webview(object);
  if (gl == NULL)
    return JSValueMakeNull(ctx);
  int argc = g_slist_length(VIEW(gl)->status->frames);
  JSValueRef argv[argc];
  int n=0;
  for (GSList *sl = VIEW(gl)->status->frames; sl; sl=sl->next) {
    argv[n++] = make_object(ctx, G_OBJECT(sl->data));
  }
  return JSObjectMakeArray(ctx, argc, argv, exception);
}/*}}}*/

/* wv_get_number {{{*/
static JSValueRef 
wv_get_number(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  GList *gl = dwb.state.views;
  for (int i=0; gl; i++, gl=gl->next) {
    if (object == VIEW(gl)->script_wv) 
      return JSValueMakeNumber(ctx, i); 
  }
  return JSValueMakeNumber(ctx, -1); 
}/*}}}*/


static JSValueRef
wv_set_title(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret = JSValueMakeUndefined(ctx);
  if (argc == 0) 
    return ret;
  GList *gl = find_webview(this);
  if (gl == NULL)
    return ret;
  char *title = js_value_to_char(ctx, argv[0], -1, exc);
  if (title != NULL) {
    dwb_update_status(gl, title); 
    g_free(title);
  }
  return ret;
}

/*}}}*/

static JSValueRef 
sp_show(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  scratchpad_show();
  return JSValueMakeUndefined(ctx);
}
static JSValueRef 
sp_hide(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  scratchpad_hide();
  return JSValueMakeUndefined(ctx);
}
static JSValueRef 
sp_load(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  char *text;
  if (argc > 0 && (text = js_value_to_char(ctx, argv[0], -1, exc)) != NULL) {
    scratchpad_load(text);
    g_free(text);
  }
  return JSValueMakeUndefined(ctx);
}
static JSObjectRef 
sp_callback_create(JSContextRef ctx, size_t argc, const JSValueRef argv[], JSValueRef *exc) {
  JSObjectRef ret = NULL;
  if (argc > 0) {
    ret = JSValueToObject(ctx, argv[0], exc);
    if (ret == NULL || !JSObjectIsFunction(ctx, ret))
      ret = NULL;
  }
  return ret;
}
static JSValueRef 
sp_get(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  m_sp_scripts_cb = sp_callback_create(ctx, argc, argv, exc);
  return JSValueMakeUndefined(ctx);
}
void 
scripts_scratchpad_get(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  m_sp_scratchpad_cb = sp_callback_create(ctx, argc, argv, exc);
}
void
sp_context_change(JSContextRef src_ctx, JSContextRef dest_ctx, JSObjectRef func, JSValueRef val) {
  if (func != NULL) {
    JSValueRef val_changed = js_context_change(src_ctx, dest_ctx, val, NULL);
    JSValueRef argv[] = { val_changed == 0 ? JSValueMakeNull(dest_ctx) : val_changed };
    JSObjectCallAsFunction(dest_ctx, func, NULL, 1, argv, NULL);
  }
}
// send from scripts context to scratchpad context
static JSValueRef 
sp_send(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc > 0) {
    sp_context_change(m_global_context, webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(scratchpad_get()))), m_sp_scratchpad_cb, argv[0]);
  }
  return JSValueMakeUndefined(ctx);
}
// send from scratchpad context to script context
void 
scripts_scratchpad_send(JSContextRef ctx, JSValueRef val) {
  sp_context_change(ctx, m_global_context, m_sp_scripts_cb, val);
}

/* SOUP_MESSAGE {{{*/
/* soup_uri_to_js_object {{{*/
JSObjectRef 
soup_uri_to_js_object(JSContextRef ctx, SoupURI *uri, JSValueRef *exception) {
  JSObjectRef o = JSObjectMake(ctx, NULL, NULL);
  js_set_object_property(ctx, o, "scheme", uri->scheme, exception);
  js_set_object_property(ctx, o, "user", uri->user, exception);
  js_set_object_property(ctx, o, "password", uri->password, exception);
  js_set_object_property(ctx, o, "host", uri->host, exception);
  js_set_object_number_property(ctx, o, "port", uri->port, exception);
  js_set_object_property(ctx, o, "path", uri->path, exception);
  js_set_object_property(ctx, o, "query", uri->query, exception);
  js_set_object_property(ctx, o, "fragment", uri->fragment, exception);
  return o;
}/*}}}*/

/* message_get_uri {{{*/
static JSValueRef 
message_get_uri(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  SoupMessage *msg = JSObjectGetPrivate(object);
  if (msg == NULL)
    return JSValueMakeNull(ctx);
  SoupURI *uri = soup_message_get_uri(msg);
  if (uri == NULL)
    return JSValueMakeNull(ctx);
  return soup_uri_to_js_object(ctx, uri, exception);
}/*}}}*/

/* message_get_first_party {{{*/
static JSValueRef 
message_get_first_party(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  SoupMessage *msg = JSObjectGetPrivate(object);
  if (msg == NULL)
    return JSValueMakeNull(ctx);
  SoupURI *uri = soup_message_get_first_party(msg);
  if (uri == NULL)
    return JSValueMakeNull(ctx);
  return soup_uri_to_js_object(ctx, uri, exception);
}/*}}}*/
/*}}}*/

/* FRAMES {{{*/
/* frame_get_domain {{{*/
static JSValueRef 
frame_get_domain(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  WebKitWebFrame *frame = JSObjectGetPrivate(object);
  if (frame == NULL)
    return JSValueMakeNull(ctx);
  SoupMessage *msg = dwb_soup_get_message(frame);
  if (msg == NULL)
    return JSValueMakeNull(ctx);
  SoupURI *uri = soup_message_get_uri(msg);
  const char *host = soup_uri_get_host(uri);
  return js_char_to_value(ctx, domain_get_base_for_host(host));
}/*}}}*/

/* frame_get_host {{{*/
static JSValueRef 
frame_get_host(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  WebKitWebFrame *frame = JSObjectGetPrivate(object);
  if (frame == NULL)
    return JSValueMakeNull(ctx);
  SoupMessage *msg = dwb_soup_get_message(frame);
  if (msg == NULL)
    return JSValueMakeNull(ctx);
  SoupURI *uri = soup_message_get_uri(msg);
  return js_char_to_value(ctx, soup_uri_get_host(uri));
}/*}}}*/

/* frame_inject {{{*/
static JSValueRef 
frame_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitWebFrame *frame = JSObjectGetPrivate(this);
  if (frame != NULL) {
    JSContextRef wctx = webkit_web_frame_get_global_context(frame);
    return inject(ctx, wctx, function, this, argc, argv, exc);
  }
  return JSValueMakeNull(ctx);
}/*}}}*/
/*}}}*/

/* GLOBAL {{{*/
/* global_checksum {{{*/
static JSValueRef 
global_checksum(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("checksum: missing argument."));
    return JSValueMakeNull(ctx);
  }
  guchar *original = (guchar*)js_value_to_char(ctx, argv[0], -1, exc);
  if (original == NULL)
    return JSValueMakeNull(ctx);
  GChecksumType type = G_CHECKSUM_SHA256;
  if (argc > 1) {
    type = JSValueToNumber(ctx, argv[1], exc);
    if (type == NAN) {
      ret = JSValueMakeNull(ctx);
      goto error_out;
    }
    type = MIN(MAX(type, G_CHECKSUM_MD5), G_CHECKSUM_SHA256);
  }
  char *checksum = g_compute_checksum_for_data(type, original, -1);

  ret = js_char_to_value(ctx, checksum);
  
error_out:
  g_free(original);
  g_free(checksum);
  return ret;
}/*}}}*/

/* scripts_eval_key {{{*/
DwbStatus
scripts_eval_key(KeyMap *m, Arg *arg) {
  char *json = NULL;
  CLEAR_COMMAND_TEXT();
  if (arg->p == NULL) 
    json = util_create_json(1, INTEGER, "nummod", dwb.state.nummod);
  else 
    json = util_create_json(2, INTEGER, "nummod", dwb.state.nummod, CHAR, "arg", arg->p);
  JSValueRef argv[] = { js_json_to_value(m_global_context, json) };
  JSObjectCallAsFunction(m_global_context, arg->arg, NULL, 1, argv, NULL);
  g_free(json);
  return STATUS_OK;
}/*}}}*/

/* global_unbind{{{*/
static JSValueRef 
global_unbind(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("unbind: missing argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  GList *l = NULL;
  KeyMap *m;
  if (JSValueIsString(ctx, argv[0])) {
    char *name = js_value_to_char(ctx, argv[0], JS_STRING_MAX, exc);
    for (l = dwb.keymap; l && g_strcmp0(((KeyMap*)l->data)->map->n.first, name); l=l->next);
    g_free(name);
  }
  else if (JSValueIsObject(ctx, argv[0])) {
    for (l = dwb.keymap; l && !JSValueIsEqual(ctx, argv[0], ((KeyMap*)l->data)->map->arg.arg, exc); l=l->next);
  }
  if (l != NULL) {
    m = l->data;
    JSValueUnprotect(ctx, m->map->arg.p);
    g_free(m->map->n.first);
    g_free(m->map->n.second);
    g_free(m->map);
    g_free(m);
    dwb.keymap = g_list_delete_link(dwb.keymap, l);
    return JSValueMakeBoolean(ctx, true);
  }
  return JSValueMakeBoolean(ctx, false);
}/*}}}*/
/* global_bind {{{*/
static JSValueRef 
global_bind(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gboolean ret = false;
  char *name = NULL, *callback = NULL;
  guint option = CP_DONT_SAVE | CP_SCRIPT;
  if (argc < 2) {
    js_make_exception(ctx, exc, EXCEPTION("bind: missing argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  gchar *keystr = js_value_to_char(ctx, argv[0], JS_STRING_MAX, exc);
  JSObjectRef func = JSValueToObject(ctx, argv[1], exc);
  if (func == NULL)
    goto error_out;
  if (!JSObjectIsFunction(ctx, func)) {
    js_make_exception(ctx, exc, EXCEPTION("bind: not a function."));
    goto error_out;
  }
  if (argc > 2) {
    name = js_value_to_char(ctx, argv[2], JS_STRING_MAX, exc);
    if (name != NULL) { 
      option |= CP_COMMANDLINE;
      char *callback_name = js_get_string_property(ctx, func, "name");
      callback = g_strdup_printf("JavaScript: %s", callback_name == NULL || *callback_name == 0 ? "[anonymous]" : callback_name);
      g_free(callback_name);
    }
  }
  if (keystr == NULL && name == NULL) 
    goto error_out;
  JSValueProtect(ctx, func);
  KeyMap *map = dwb_malloc(sizeof(KeyMap));
  FunctionMap *fmap = dwb_malloc(sizeof(FunctionMap));
  Key key = dwb_str_to_key(keystr);
  map->key = key.str;
  map->mod = key.mod;
  FunctionMap fm = { { name, callback }, option, (Func)scripts_eval_key, NULL, NEVER_SM, { .arg = func }, EP_NONE,  {NULL} };
  *fmap = fm;
  map->map = fmap;
  dwb.keymap = g_list_prepend(dwb.keymap, map);

  ret = true;
error_out:
  g_free(keystr);
  return JSValueMakeBoolean(ctx, ret);
}/*}}}*/

/* global_execute {{{*/
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
}/*}}}*/

/* global_exit {{{*/
static JSValueRef 
global_exit(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (m_commandline)
    application_stop();
  else 
    dwb_end();
  return JSValueMakeUndefined(ctx);
}/*}}}*/

/* global_include {{{*/
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
  if ( (content = util_get_file_content(path, NULL)) == NULL) {
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
}/*}}}*/


/* global_send_request {{{*/
JSObjectRef 
get_message_data(SoupMessage *msg) {
  JSObjectRef o = JSObjectMake(m_global_context, NULL, NULL);
  js_set_object_property(m_global_context, o, "body", msg->response_body->data, NULL);
  JSObjectRef ho = JSObjectMake(m_global_context, NULL, NULL);
  const char *name, *value;
  SoupMessageHeadersIter iter;
  soup_message_headers_iter_init(&iter, msg->response_headers);
  while (soup_message_headers_iter_next(&iter, &name, &value)) {
    js_set_object_property(m_global_context, ho, name, value, NULL);
  }
  JSStringRef s = JSStringCreateWithUTF8CString("headers");
  JSObjectSetProperty(m_global_context, o, s, ho, kJSDefaultProperty, NULL);
  JSStringRelease(s);
  return o;
}
void
request_callback(SoupSession *session, SoupMessage *message, JSObjectRef function) {
  if (message->response_body->data != NULL) {
    JSObjectRef o = get_message_data(message);
    JSValueRef vals[] = { o, make_object(m_global_context, G_OBJECT(message))  };
    JSObjectCallAsFunction(m_global_context, function, NULL, 2, vals, NULL);
  }
  JSValueUnprotect(m_global_context, function);
}
static JSValueRef 
global_send_request(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gint ret = -1;
  char *method = NULL;
  if (argc < 2) {
    js_make_exception(ctx, exc, EXCEPTION("sendRequest: missing argument."));
    return JSValueMakeNumber(ctx, -1);
  }
  char *uri = js_value_to_char(ctx, argv[0], -1, exc);
  if (uri == NULL) 
    return JSValueMakeNumber(ctx, -1);
  JSObjectRef function = JSValueToObject(ctx, argv[1], exc);
  if (function == NULL || !JSObjectIsFunction(ctx, function)) 
    goto error_out;
  if (argc > 2) 
    method = js_value_to_char(ctx, argv[2], -1, exc);
  SoupMessage *msg = soup_message_new(method == NULL ? "GET" : method, uri);
  if (msg == NULL)
    goto error_out;
  JSValueProtect(ctx, function);
  soup_session_queue_message(webkit_get_default_session(), msg, (SoupSessionCallback)request_callback, function);
  ret = 0;
error_out: 
  g_free(uri);
  g_free(method);
  return JSValueMakeNumber(ctx, ret);
}/*}}}*/

static JSValueRef 
global_send_request_sync(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  char *method = NULL;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("sendRequestSync: missing argument."));
    return JSValueMakeNull(ctx);
  }
  char *uri = js_value_to_char(ctx, argv[0], -1, exc);
  if (uri == NULL) 
    return JSValueMakeNull(ctx);
  if (argc > 1) 
    method = js_value_to_char(ctx, argv[1], -1, exc);
  SoupMessage *msg = soup_message_new(method == NULL ? "GET" : method, uri);
  guint status = soup_session_send_message(webkit_get_default_session(), msg);
  JSObjectRef o = get_message_data(msg);
  JSStringRef js_key = JSStringCreateWithUTF8CString("status");
  JSValueRef js_value = JSValueMakeNumber(ctx, status);
  JSObjectSetProperty(ctx, o, js_key, js_value, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly, exc);
  JSStringRelease(js_key);
  return o;
}
void 
scripts_completion_activate(void) {
  const char *text = GET_TEXT();
  JSValueRef val[] = { js_char_to_value(m_global_context, text) };
  JSObjectCallAsFunction(m_global_context, m_completion_callback, NULL, 1, val, NULL);
  completion_clean_completion(false);
  dwb_change_mode(NORMAL_MODE, true);
}
static JSValueRef 
global_tab_complete(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 3 || !JSValueIsInstanceOfConstructor(ctx, argv[1], m_array_contructor, exc)) {
    js_make_exception(ctx, exc, EXCEPTION("tabComplete: invalid argument."));
    return JSValueMakeUndefined(ctx);
  }
  m_completion_callback = JSValueToObject(ctx, argv[2], exc);
  if (m_completion_callback == NULL)
    return JSValueMakeUndefined(ctx);
  if (!JSObjectIsFunction(ctx, m_completion_callback)) {
    js_make_exception(ctx, exc, EXCEPTION("tabComplete: arguments[2] is not a function."));
    return JSValueMakeUndefined(ctx);
  }
  dwb.state.script_comp_readonly = false;
  if (argc > 3 && JSValueIsBoolean(ctx, argv[3])) {
    dwb.state.script_comp_readonly = JSValueToBoolean(ctx, argv[3]);
  }
  char *left, *right, *label;
  js_array_iterator iter;
  JSValueRef val;
  JSObjectRef cur = NULL;
  Navigation *n;

  label = js_value_to_char(ctx, argv[0], JS_STRING_MAX, exc);
  JSObjectRef o = JSValueToObject(ctx, argv[1], exc);
  js_array_iterator_init(ctx, &iter, o);
  while((val = js_array_iterator_next(&iter, exc))) {
    cur = JSValueToObject(ctx, val, exc);
    if (cur == NULL)
      goto error_out;
    left = js_get_string_property(ctx, cur, "left");
    right = js_get_string_property(ctx, cur, "right");
    n = g_malloc(sizeof(Navigation));
    n->first = left; 
    n->second = right;
    dwb.state.script_completion = g_list_prepend(dwb.state.script_completion, n);
  }
  dwb.state.script_completion = g_list_reverse(dwb.state.script_completion);
  dwb_set_status_bar_text(dwb.gui.lstatus, label, NULL, NULL, true);

  entry_focus();
  completion_complete(COMP_SCRIPT, false);
error_out:
  for (GList *l = dwb.state.script_completion; l; l=l->next) {
    n = l->data;
    g_free(n->first); 
    g_free(n->second);
    g_free(n);
  }
  g_free(label);
  g_list_free(dwb.state.script_completion);
  dwb.state.script_completion = NULL;
  return JSValueMakeUndefined(ctx);
}
/* timeout_callback {{{*/
static gboolean
timeout_callback(JSObjectRef obj) {
  gboolean ret;
  JSValueRef val = JSObjectCallAsFunction(m_global_context, obj, NULL, 0, NULL, NULL);
  if (val == NULL)
    ret = false;
  else {
    ret = !JSValueIsBoolean(m_global_context, val) || JSValueToBoolean(m_global_context, val);
  }
  if (! ret )
    JSValueUnprotect(m_global_context, obj);
  return ret;
  
}/*}}}*/

/* global_timer_stop {{{*/
static JSValueRef 
global_timer_stop(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gdouble sigid;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("timerStop: missing argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  if ((sigid = JSValueToNumber(ctx, argv[0], exc)) != NAN) {
    gboolean ret = g_source_remove((int)sigid);
    return JSValueMakeBoolean(ctx, ret);
  }
  return JSValueMakeBoolean(ctx, false);
}/*}}}*/

/* global_timer_start {{{*/
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
  JSValueProtect(ctx, func);
  int ret = g_timeout_add((int)msec, (GSourceFunc)timeout_callback, func);
  return JSValueMakeNumber(ctx, ret);
}/*}}}*/
/*}}}*/

/* UTIL {{{*/
/* util_domain_from_host {{{*/
static JSValueRef 
util_domain_from_host(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
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
}/*}}}*//*}}}*/
static JSValueRef 
util_markup_escape(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  char *string = NULL, *escaped = NULL;
  if (argc > 0) {
    string = js_value_to_char(ctx, argv[0], -1, exc);
    if (string != NULL) {
      escaped = g_markup_escape_text(string, -1);
      g_free(string);
      if (escaped != NULL) {
        JSValueRef ret = js_char_to_value(ctx, escaped);
        g_free(escaped);
        return ret;
      }
    }
  }
  return JSValueMakeNull(ctx);
}
static JSValueRef 
util_get_mode(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  return JSValueMakeNumber(ctx, BASIC_MODES(dwb.state.mode));
}
/* DATA {{{*/
/* data_get_profile {{{*/
static JSValueRef 
data_get_profile(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  return js_char_to_value(ctx, dwb.misc.profile);
}/*}}}*/

/* data_get_cache_dir {{{*/
static JSValueRef 
data_get_cache_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  return js_char_to_value(ctx, dwb.files[FILES_CACHEDIR]);
}/*}}}*/

/* data_get_config_dir {{{*/
static JSValueRef 
data_get_config_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  char *dir = util_build_path();
  if (dir == NULL) {
    return JSValueMakeNull(ctx);
  }
  JSValueRef ret = js_char_to_value(ctx, dir);
  g_free(dir);
  return ret;
}/*}}}*/

/* data_get_system_data_dir {{{*/
static JSValueRef 
data_get_system_data_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  char *dir = util_get_system_data_dir(NULL);
  if (dir == NULL) {
    return JSValueMakeNull(ctx);
  }
  JSValueRef ret = js_char_to_value(ctx, dir);
  g_free(dir);
  return ret;
}/*}}}*/

/* data_get_user_data_dir {{{*/
static JSValueRef 
data_get_user_data_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) {
  char *dir = util_get_user_data_dir(NULL);
  if (dir == NULL) {
    return JSValueMakeNull(ctx);
  }
  JSValueRef ret = js_char_to_value(ctx, dir);
  g_free(dir);
  return ret;
}/*}}}*/
/*}}}*/

/* SYSTEM {{{*/
/* system_get_env {{{*/
static JSValueRef 
system_get_env(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) {
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
}/*}}}*/

/* spawn_output {{{*/
static gboolean
spawn_output(GIOChannel *channel, GIOCondition condition, JSObjectRef callback) {
  char *content; 
  gsize length;
  if (condition == G_IO_HUP || condition == G_IO_ERR || condition == G_IO_NVAL) {
    g_io_channel_unref(channel);
    return false;
  }
  else if (g_io_channel_read_to_end(channel, &content, &length, NULL) == G_IO_STATUS_NORMAL && content != NULL)  {
      JSValueRef arg = js_char_to_value(m_global_context, content);
      if (arg != NULL) {
        JSValueRef argv[] = { arg };
        JSObjectCallAsFunction(m_global_context, callback, NULL, 1,  argv , NULL);
      }
      g_free(content);
      return true;
  }
  return false;
}/*}}}*/

/* {{{*/
static JSValueRef 
system_spawn_sync(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc<1) {
    js_make_exception(ctx, exc, EXCEPTION("system.spawnSync needs an argument."));
    return JSValueMakeBoolean(ctx, SPAWN_FAILED);
  }
  JSObjectRef ret = NULL;
  int srgc, status;
  char **srgv = NULL, *command = NULL, *out, *err;
  command = js_value_to_char(ctx, argv[0], -1, exc);
  if (command == NULL) 
    return JSValueMakeNull(ctx);
  if (g_shell_parse_argv(command, &srgc, &srgv, NULL) && 
      g_spawn_sync(NULL, srgv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &out, &err, &status, NULL)) {
    ret = JSObjectMake(ctx, NULL, NULL);
    js_set_object_property(ctx, ret, "stdout", out, exc);
    js_set_object_property(ctx, ret, "stderr", err, exc);
    js_set_object_number_property(ctx, ret, "status", status, exc);
  }
  g_free(command);
  g_strfreev(srgv);
  if (ret == NULL)
    return JSValueMakeNull(ctx);
  return ret;
}/*}}}*/

/* system_spawn {{{*/
static JSValueRef 
system_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  int ret = 0; 
  int outfd, errfd;
  char **srgv = NULL;
  int srgc;
  GIOChannel *out_channel, *err_channel;
  JSObjectRef oc = NULL, ec = NULL;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("system.spawn needs an argument."));
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
  g_strfreev(srgv);
  return JSValueMakeNumber(ctx, ret);
}/*}}}*/

/* system_file_test {{{*/
static JSValueRef 
system_file_test(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 2) {
    js_make_exception(ctx, exc, EXCEPTION("system.fileTest needs an argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  char *path = js_value_to_char(ctx, argv[0], PATH_MAX, exc);
  if (path == NULL) 
    return JSValueMakeBoolean(ctx, false);
  double test = JSValueToNumber(ctx, argv[1], exc);
  if (test == NAN || ! ( (((guint)test) & G_FILE_TEST_VALID) == (guint)test) ) 
    return JSValueMakeBoolean(ctx, false);
  gboolean ret = g_file_test(path, (GFileTest) test);
  g_free(path);
  return JSValueMakeBoolean(ctx, ret);
}/*}}}*/

/* system_mkdir {{{*/
static JSValueRef 
system_mkdir(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gboolean ret = false;
  if (argc < 2) {
    js_make_exception(ctx, exc, EXCEPTION("system.mkdir needs an argument."));
    return JSValueMakeBoolean(ctx, false);
  }
  char *path = js_value_to_char(ctx, argv[0], PATH_MAX, exc);
  double mode = JSValueToNumber(ctx, argv[1], exc);
  if (path != NULL && mode != NAN) {
    ret = g_mkdir_with_parents(path, (gint)mode) == 0;
  }
  g_free(path);
  return JSValueMakeBoolean(ctx, ret);

}/*}}}*/

/*}}}*/

/* IO {{{*/
/* io_prompt {{{*/
static JSValueRef 
io_prompt(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  char *prompt = NULL;
  gboolean visibility = true;
  if (argc > 0) {
    prompt = js_value_to_char(ctx, argv[0], JS_STRING_MAX, exc);
  }
  if (argc > 1 && JSValueIsBoolean(ctx, argv[1])) 
    visibility = JSValueToBoolean(ctx, argv[1]);

  const char *response = dwb_prompt(visibility, prompt);
  g_free(prompt);
  if (response == NULL)
    return JSValueMakeNull(ctx);

  return js_char_to_value(ctx, response);
}/*}}}*/

/* io_read {{{*/
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
  if ( (content = util_get_file_content(path, NULL) ) == NULL ) {
    goto error_out;
  }
  ret = js_char_to_value(ctx, content);

error_out:
  g_free(path);
  g_free(content);
  if (ret == NULL)
    return JSValueMakeNull(ctx);
  return ret;

}/*}}}*/

/* io_notify {{{*/
static JSValueRef 
io_notify(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) 
    return JSValueMakeUndefined(ctx);
  char *message = js_value_to_char(ctx, argv[0], -1, exc);
  if (message != NULL) {
    dwb_set_normal_message(dwb.state.fview, true, message);
    g_free(message);
  }
  return JSValueMakeUndefined(ctx);
}/*}}}*/

/* io_error {{{*/
static JSValueRef 
io_error(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) 
    return JSValueMakeUndefined(ctx);
  char *message = js_value_to_char(ctx, argv[0], -1, exc);
  if (message != NULL) {
    dwb_set_error_message(dwb.state.fview, message);
    g_free(message);
  }
  return JSValueMakeUndefined(ctx);
}/*}}}*/
static JSValueRef 
io_status_bar(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc < 1) 
    return JSValueMakeUndefined(ctx);
  JSObjectRef o = JSValueToObject(ctx, argv[0], exc);
  if (o == NULL) 
    return JSValueMakeUndefined(ctx);
  char *middle = js_get_string_property(ctx, o, "middle");
  char *right = js_get_string_property(ctx, o, "right");
  if (middle != NULL) {
    gtk_label_set_markup(GTK_LABEL(dwb.gui.urilabel), middle);
  }
  if (right != NULL) {
    gtk_label_set_markup(GTK_LABEL(dwb.gui.rstatus), right);
  }

  g_free(middle);
  g_free(right);
  return JSValueMakeUndefined(ctx);
}

static JSValueRef 
io_dir_names(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  JSValueRef ret;
  if (argc < 1) {
    js_make_exception(ctx, exc, EXCEPTION("io.dirNames: missing argument."));
    return JSValueMakeNull(ctx);
  }
  GDir *dir;
  char *dir_name = js_value_to_char(ctx, argv[0], PATH_MAX, exc);
  const char *name;
  if (dir_name == NULL)
    return JSValueMakeNull(ctx);
  if ((dir = g_dir_open(dir_name, 0, NULL)) != NULL) {
    GSList *list = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
      list = g_slist_prepend(list, (gpointer)js_char_to_value(ctx, name));
    }
    g_dir_close(dir);
    JSValueRef args[g_slist_length(list)];
    int i=0;
    for (GSList *l = list; l; l=l->next, i++) 
      args[i] = l->data;
    ret = JSObjectMakeArray(ctx, i, args, exc);
    g_slist_free(list);
  }
  else 
    ret = JSValueMakeNull(ctx);
  g_free(dir_name);
  return ret;
}
/* io_write {{{*/
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
    fputs(content, f);
    fclose(f);
    ret = true;
  }
  else {
    js_make_exception(ctx, exc, EXCEPTION("io.write: cannot open %s for writing."), path);
  }
error_out:
  g_free(path);
  g_free(mode);
  g_free(content);
  return JSValueMakeBoolean(ctx, ret);
}/*}}}*/

/* io_print {{{*/
static JSValueRef 
io_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  if (argc == 0) {
    js_make_exception(ctx, exc, EXCEPTION("io.print needs an argument."));
    return JSValueMakeUndefined(m_global_context);
  }

  FILE *stream = stdout;
  if (argc >= 2) {
    JSStringRef js_string = JSValueToStringCopy(ctx, argv[1], exc);
    if (JSStringIsEqualToUTF8CString(js_string, "stderr")) 
      stream = stderr;
    JSStringRelease(js_string);
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
      else 
        fprintf(stream, "NAN\n");
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
/*}}}*/

/* DOWNLOAD {{{*/
/* download_constructor_cb {{{*/
static JSObjectRef 
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
  return JSObjectMake(ctx, m_download_class, download);
}/*}}}*/

/* stop_download_notify {{{*/
static gboolean 
stop_download_notify(CallbackData *c) {
  WebKitDownloadStatus status = webkit_download_get_status(WEBKIT_DOWNLOAD(c->gobject));
  if (status == WEBKIT_DOWNLOAD_STATUS_ERROR || status == WEBKIT_DOWNLOAD_STATUS_CANCELLED || status == WEBKIT_DOWNLOAD_STATUS_FINISHED) {
    return true;
  }
  return false;
}/*}}}*/

/* download_start {{{*/
static JSValueRef 
download_start(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitDownload *download = JSObjectGetPrivate(this);
  if (download == NULL)
    return JSValueMakeBoolean(ctx, false);
  if (webkit_download_get_destination_uri(download) == NULL) {
    js_make_exception(ctx, exc, EXCEPTION("Download.start: destination == null"));
    return JSValueMakeBoolean(ctx, false);
  }
  if (argc > 0) {
    make_callback(ctx, this, G_OBJECT(download), "notify::status", argv[0], stop_download_notify, exc);
  }
  webkit_download_start(download);
  return JSValueMakeBoolean(ctx, true);

}/*}}}*/

/* download_cancel {{{*/
static JSValueRef 
download_cancel(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  WebKitDownload *download = JSObjectGetPrivate(this);
  webkit_download_cancel(download);
  return JSValueMakeUndefined(ctx);
}/*}}}*/
/*}}}*/

/* SIGNALS {{{*/
/* signal_set {{{*/
static bool
signal_set(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef value, JSValueRef* exception) {
  char *name = js_string_to_char(ctx, js_name, -1);
  JSObjectRef o;
  if (name == NULL)
    return false;

  for (int i = SCRIPTS_SIG_FIRST; i<SCRIPTS_SIG_LAST; i++) {
    if (strcmp(name, m_sigmap[i].name)) 
      continue;
    if (JSValueIsNull(ctx, value)) {
      dwb.misc.script_signals &= ~(1<<i);
    }
    else if ( (o = JSValueToObject(ctx, value, exception)) != NULL && JSObjectIsFunction(ctx, o)) {
      m_sig_objects[i] = o;
      dwb.misc.script_signals |= (1<<i);
    }
    break;
  }
  return false;
}/*}}}*/

/* scripts_emit {{{*/
gboolean
scripts_emit(ScriptSignal *sig) {
  JSObjectRef function = m_sig_objects[sig->signal];
  if (function == NULL)
    return false;

  int additional = sig->jsobj != NULL ? 2 : 1;
  int numargs = MIN(sig->numobj, SCRIPT_MAX_SIG_OBJECTS)+additional;
  JSValueRef val[numargs];
  int i = 0;
  
  if (sig->jsobj != NULL) {
    val[i++] = sig->jsobj;
  }
  for (int j=0; j<sig->numobj; j++) {
    if (sig->objects[j] != NULL) 
      val[i++] = make_object(m_global_context, G_OBJECT(sig->objects[j]));
    else 
      val[i++] = JSValueMakeNull(m_global_context);
  }
  JSValueRef vson = js_json_to_value(m_global_context, sig->json);
  val[i++] = vson == NULL ? JSValueMakeNull(m_global_context) : vson;
  JSValueRef js_ret = JSObjectCallAsFunction(m_global_context, function, NULL, numargs, val, NULL);
  if (JSValueIsBoolean(m_global_context, js_ret)) {
    return JSValueToBoolean(m_global_context, js_ret);
  }
  return false;
}/*}}}*/
/*}}}*/

/* OBJECTS {{{*/
// TODO : creating 1000000 objects leaks ~ 4MB  
/* make_object {{{*/
void 
object_destroy_cb(JSObjectRef o) {
  JSObjectSetPrivate(o, NULL);
  JSValueUnprotect(m_global_context, o);
}

static JSObjectRef 
make_object_for_class(JSContextRef ctx, JSClassRef class, GObject *o) {
  JSObjectRef retobj = g_object_get_qdata(o, ref_quark);
  if (retobj != NULL)
    return retobj;

  retobj = JSObjectMake(ctx, class, o);
  g_object_set_qdata_full(o, ref_quark, retobj, (GDestroyNotify)object_destroy_cb);
  JSValueProtect(m_global_context, retobj);
  return retobj;
}


static JSObjectRef 
make_object(JSContextRef ctx, GObject *o) {
  if (o == NULL) {
    JSValueRef v = JSValueMakeNull(ctx);
    return JSValueToObject(ctx, v, NULL);
  }
  JSClassRef class;
  if (WEBKIT_IS_WEB_VIEW(o)) 
     class = m_webview_class;
  else if (WEBKIT_IS_WEB_FRAME(o))
     class = m_frame_class;
  else if (WEBKIT_IS_DOWNLOAD(o)) 
    class = m_download_class;
  else if (SOUP_IS_MESSAGE(o)) 
    class = m_message_class;
  else 
    class = m_default_class;
  return make_object_for_class(ctx, class, o);

}/*}}}*/

static gboolean 
connect_callback(JSObjectRef func) {
  JSValueRef ret = JSObjectCallAsFunction(m_global_context, func, NULL, 0, NULL, NULL);
  if (JSValueIsBoolean(m_global_context, ret))
    return JSValueToBoolean(m_global_context, ret);
  return false;
}
static JSValueRef 
connect_object(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  gulong id = 0;
  if (argc < 2) {
    return JSValueMakeNumber(ctx, -1);
  }
  char *name = js_value_to_char(ctx, argv[0], PROP_LENGTH, exc);
  if (name == NULL) 
    goto error_out;
  JSObjectRef func = JSValueToObject(ctx, argv[1], exc);
  if (func == NULL || !JSObjectIsFunction(ctx, func)) 
    goto error_out;
  GObject *o = JSObjectGetPrivate(this);
  if (o == NULL)
    return JSValueMakeNumber(ctx, -1);
  id = g_signal_connect_swapped(o, name, G_CALLBACK(connect_callback), func);
error_out: 
  g_free(name);
  return JSValueMakeNumber(ctx, id);
}
static JSValueRef 
disconnect_object(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) {
  int id;
  if (argc > 0 && (id = JSValueToNumber(ctx, argv[0], exc)) != NAN) {
    GObject *o = JSObjectGetPrivate(this);
    if (o != NULL) {
      g_signal_handler_disconnect(o, id);
      return JSValueMakeBoolean(ctx, true);
    }
  }
  return JSValueMakeBoolean(ctx, false);
}

/* set_property_cb {{{*/
static bool
set_property_cb(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
  return true;
  //return true;
}/*}}}*/

/* create_class {{{*/
static JSClassRef 
create_class(const char *name, JSStaticFunction staticFunctions[], JSStaticValue staticValues[]) {
  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.className = name;
  cd.staticFunctions = staticFunctions;
  cd.staticValues = staticValues;
  cd.setProperty = set_property_cb;
  return JSClassCreate(&cd);
}/*}}}*/

/* create_object {{{*/
static JSObjectRef 
create_object(JSContextRef ctx, JSClassRef class, JSObjectRef obj, JSClassAttributes attr, const char *name, void *private) {
  JSObjectRef ret = JSObjectMake(ctx, class, private);
  js_set_property(ctx, obj, name, ret, attr, NULL);
  return ret;
}/*}}}*/

/* set_property {{{*/
static bool
set_property(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef jsvalue, JSValueRef* exception) {
  char buf[PROP_LENGTH];
  char *name = js_string_to_char(ctx, js_name, -1);
  if (name == NULL)
    return false;
  uncamelize(buf, name, '-', PROP_LENGTH);
  g_free(name);

  GObject *o = JSObjectGetPrivate(object);
  if (o == NULL)
    return false;
  GObjectClass *class = G_OBJECT_GET_CLASS(o);
  if (class == NULL || !G_IS_OBJECT_CLASS(class))
    return false;
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
      gtype == G_TYPE_UINT64 || gtype == G_TYPE_FLAGS))  {
    double value = JSValueToNumber(ctx, jsvalue, exception);
    if (value != NAN) {
      switch (gtype) {
        case G_TYPE_ENUM :
        case G_TYPE_FLAGS :
        case G_TYPE_INT : g_object_set(o, buf, (gint)value, NULL); break;
        case G_TYPE_UINT : g_object_set(o, buf, (guint)value, NULL); break;
        case G_TYPE_LONG : g_object_set(o, buf, (long)value, NULL); break;
        case G_TYPE_ULONG : g_object_set(o, buf, (gulong)value, NULL); break;
        case G_TYPE_FLOAT : g_object_set(o, buf, (gfloat)value, NULL); break;
        case G_TYPE_DOUBLE : g_object_set(o, buf, (gdouble)value, NULL); break;
        case G_TYPE_INT64 : g_object_set(o, buf, (gint64)value, NULL); break;
        case G_TYPE_UINT64 : g_object_set(o, buf, (guint64)value, NULL); break;

      }
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
}/*}}}*/

/* get_property {{{*/
static JSValueRef
get_property(JSContextRef ctx, JSObjectRef jsobj, JSStringRef js_name, JSValueRef *exception) {
  char buf[PROP_LENGTH];
  JSValueRef ret = NULL;
  char *name = js_string_to_char(ctx, js_name, -1);
  if (name == NULL)
    return NULL;
  uncamelize(buf, name, '-', PROP_LENGTH);
  g_free(name);

  GObject *o = JSObjectGetPrivate(jsobj);
  if (o == NULL)
    return NULL;
  GObjectClass *class = G_OBJECT_GET_CLASS(o);
  if (class == NULL || !G_IS_OBJECT_CLASS(class))
    return NULL;
  GParamSpec *pspec = g_object_class_find_property(class, buf);
  if (pspec == NULL)
    return NULL;
  if (! (pspec->flags & G_PARAM_READABLE))
    return NULL;

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
    JSObjectRef retobj = make_object(ctx, object);
    g_object_unref(object);
    ret = retobj;
  }
 return ret;
}/*}}}*/

/* create_global_object {{{*/
static void 
create_global_object() {
  ref_quark = g_quark_from_static_string("dwb_js_ref");

  JSStaticFunction global_functions[] = { 
    { "execute",          global_execute,         kJSDefaultAttributes },
    { "exit",             global_exit,         kJSDefaultAttributes },
    { "bind",             global_bind,         kJSDefaultAttributes },
    { "unbind",           global_unbind,         kJSDefaultAttributes },
    { "checksum",         global_checksum,         kJSDefaultAttributes },
    { "include",          global_include,         kJSDefaultAttributes },
    { "timerStart",       global_timer_start,         kJSDefaultAttributes },
    { "timerStop",        global_timer_stop,         kJSDefaultAttributes },
    { "sendRequest",      global_send_request,         kJSDefaultAttributes },
    { "sendRequestSync",  global_send_request_sync,         kJSDefaultAttributes },
    { "tabComplete",      global_tab_complete,         kJSDefaultAttributes },
    { 0, 0, 0 }, 
  };

  JSClassRef class = create_class("dwb", global_functions, NULL);
  m_global_context = JSGlobalContextCreate(class);
  JSClassRelease(class);


  JSObjectRef global_object = JSContextGetGlobalObject(m_global_context);

  JSStaticValue data_values[] = {
    { "profile",        data_get_profile, NULL, kJSDefaultAttributes },
    { "cacheDir",       data_get_cache_dir, NULL, kJSDefaultAttributes },
    { "configDir",      data_get_config_dir, NULL, kJSDefaultAttributes },
    { "systemDataDir",  data_get_system_data_dir, NULL, kJSDefaultAttributes },
    { "userDataDir",    data_get_user_data_dir, NULL, kJSDefaultAttributes },
    { 0, 0, 0,  0 }, 
  };
  class = create_class("data", NULL, data_values);
  create_object(m_global_context, class, global_object, kJSDefaultAttributes, "data", NULL);
  JSClassRelease(class);

  JSStaticFunction io_functions[] = { 
    { "print",     io_print,            kJSDefaultAttributes },
    { "prompt",    io_prompt,           kJSDefaultAttributes },
    { "read",      io_read,             kJSDefaultAttributes },
    { "write",     io_write,            kJSDefaultAttributes },
    { "dirNames",  io_dir_names,        kJSDefaultAttributes },
    { "notify",    io_notify,           kJSDefaultAttributes },
    { "error",     io_error,            kJSDefaultAttributes },
    { "statusBar", io_status_bar,      kJSDefaultAttributes },
    { 0,           0,           0 },
  };
  class = create_class("io", io_functions, NULL);
  create_object(m_global_context, class, global_object, kJSPropertyAttributeDontDelete, "io", NULL);
  JSClassRelease(class);

  JSStaticFunction system_functions[] = { 
    { "spawn",           system_spawn,           kJSDefaultAttributes },
    { "spawnSync",       system_spawn_sync,        kJSDefaultAttributes },
    { "getEnv",          system_get_env,           kJSDefaultAttributes },
    { "fileTest",        system_file_test,            kJSDefaultAttributes },
    { "mkdir",           system_mkdir,            kJSDefaultAttributes },
    { 0, 0, 0 }, 
  };
  class = create_class("system", system_functions, NULL);
  create_object(m_global_context, class, global_object, kJSDefaultAttributes, "system", NULL);
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
  create_object(m_global_context, class, global_object, kJSDefaultAttributes, "tabs", NULL);
  JSClassRelease(class);

  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.className = "signals";
  cd.setProperty = signal_set;
  class = JSClassCreate(&cd);
  
  create_object(m_global_context, class, global_object, kJSDefaultAttributes, "signals", NULL);
  JSClassRelease(class);

  class = create_class("extensions", NULL, NULL);
  create_object(m_global_context, class, global_object, kJSDefaultAttributes, "extensions", NULL);
  JSClassRelease(class);

  JSStaticFunction util_functions[] = { 
    { "domainFromHost",   util_domain_from_host,         kJSDefaultAttributes },
    { "markupEscape",     util_markup_escape,         kJSDefaultAttributes },
    { "getMode",          util_get_mode,         kJSDefaultAttributes },
    { 0, 0, 0 }, 
  };
  class = create_class("util", util_functions, NULL);
  create_object(m_global_context, class, global_object, kJSDefaultAttributes, "util", NULL);
  JSClassRelease(class);

  /* Default gobject class */
  cd = kJSClassDefinitionEmpty;
  cd.staticFunctions = default_functions;
  cd.getProperty = get_property;
  cd.setProperty = set_property;

  m_default_class = JSClassCreate(&cd);

  /* Webview */
  cd.staticFunctions = wv_functions;
  cd.staticValues = wv_values;
  cd.parentClass = m_default_class;
  m_webview_class = JSClassCreate(&cd);

  /* Frame */
  cd.staticFunctions = frame_functions;
  cd.staticValues = frame_values;
  cd.parentClass = m_default_class;
  m_frame_class = JSClassCreate(&cd);

  /* SoupMessage */ 
  cd.staticFunctions = default_functions;
  cd.staticValues = message_values;
  cd.parentClass = m_default_class;
  m_message_class = JSClassCreate(&cd);

  /* download */
  cd.className = "Download";
  cd.staticFunctions = download_functions;
  cd.staticValues = NULL;
  cd.parentClass = m_default_class;
  m_download_class = JSClassCreate(&cd);

  JSStaticFunction scratchpad_functions[] = { 
    { "show",         sp_show,             kJSDefaultAttributes },
    { "hide",         sp_hide,             kJSDefaultAttributes },
    { "load",         sp_load,             kJSDefaultAttributes },
    { "get",          sp_get,              kJSDefaultAttributes },
    { "send",         sp_send,              kJSDefaultAttributes },
    { 0, 0, 0 }, 
  };
  cd.className = "Scratchpad";
  cd.staticFunctions = scratchpad_functions;
  cd.staticValues = NULL;
  cd.parentClass = m_default_class;
  class = JSClassCreate(&cd);
  JSObjectRef o = make_object_for_class(m_global_context, class, G_OBJECT(scratchpad_get()));
  js_set_property(m_global_context, global_object, "scratchpad", o, kJSDefaultAttributes, NULL);

  JSObjectRef constructor = JSObjectMakeConstructor(m_global_context, m_download_class, download_constructor_cb);
  JSStringRef name = JSStringCreateWithUTF8CString("Download");
  JSObjectSetProperty(m_global_context, JSContextGetGlobalObject(m_global_context), name, constructor, kJSDefaultProperty, NULL);
  JSStringRelease(name);
}/*}}}*/
/*}}}*/

/* INIT AND END {{{*/
/* apply_scripts {{{*/
static void 
apply_scripts() {
  for (GSList *l = m_script_list; l; l=l->next) {
    JSObjectRef o = JSObjectMake(m_global_context, NULL, NULL);
    JSObjectRef apply = js_get_object_property(m_global_context, l->data, "apply");
    JSValueRef argv[] = {o};
    JSObjectCallAsFunction(m_global_context, apply, l->data, 1, argv, NULL);
  }
  g_slist_free(m_script_list);
  m_script_list = NULL;
  if (m_global_init != NULL) {
    JSObjectCallAsFunction(m_global_context, m_global_init, NULL, 0, NULL, NULL);
    JSValueUnprotect(m_global_context, m_global_init);
  }
}/*}}}*/

/* scripts_create_tab {{{*/
void 
scripts_create_tab(GList *gl) {
  static gboolean applied = false;
  if (m_global_context == NULL )  {
    VIEW(gl)->script_wv = NULL;
    return;
  }
  if (!applied) {
    apply_scripts();
    applied = true;
  }
  JSObjectRef o = make_object(m_global_context, G_OBJECT(VIEW(gl)->web));


  if (EMIT_SCRIPT(CREATE_TAB)) {
    ScriptSignal signal = { o, SCRIPTS_SIG_META(NULL, CREATE_TAB, 0) };
    scripts_emit(&signal);
  }

  JSValueProtect(m_global_context, o);
  VIEW(gl)->script_wv = o;
}/*}}}*/

/* scripts_remove_tab {{{*/
void 
scripts_remove_tab(JSObjectRef obj) {
  if (obj != NULL) {
    if (EMIT_SCRIPT(CLOSE_TAB)) {
      ScriptSignal signal = { obj, SCRIPTS_SIG_META(NULL, CLOSE_TAB, 0) };
      scripts_emit(&signal);
    }
    JSValueUnprotect(m_global_context, obj);
  }
}/*}}}*/

/* scripts_init_script {{{*/
void
scripts_init_script(const char *path, const char *script) {
  if (m_global_context == NULL) 
    create_global_object();
  char *debug = g_strdup_printf("\ntry{/*<dwb*/%s/*dwb>*/}catch(e) { io.debug({message : \"In file %s\", error : e}); }", script, path);
  JSObjectRef function = js_make_function(m_global_context, debug);
  if (function != NULL) {
    m_script_list = g_slist_prepend(m_script_list, function);
  }
  g_free(debug);
}/*}}}*/

void
evaluate(const char *script) {
  JSStringRef js_script = JSStringCreateWithUTF8CString(script);
  JSEvaluateScript(m_global_context, js_script, NULL, NULL, 0, NULL);
  JSStringRelease(js_script);
}

/* scripts_init {{{*/
void 
scripts_init(gboolean force) {
  dwb.misc.script_signals = 0;
  if (m_global_context == NULL) {
    if (force) 
      create_global_object();
    else 
      return;
  }
  dwb.state.script_completion = NULL;

  char *dir = util_get_data_dir(LIBJS_DIR);
  if (dir != NULL) {
    GString *content = g_string_new(NULL);
    util_get_directory_content(content, dir, "js");
    if (content != NULL)  {
      JSStringRef js_script = JSStringCreateWithUTF8CString(content->str);
      JSEvaluateScript(m_global_context, js_script, NULL, NULL, 0, NULL);
      JSStringRelease(js_script);
    }
    g_string_free(content, true);
    g_free(dir);
  }

  JSStringRef global_init = JSStringCreateWithUTF8CString("_init");
  JSObjectRef global_object = JSContextGetGlobalObject(m_global_context);
  m_global_init = js_get_object_property(m_global_context, global_object,
      "_init");
  JSValueProtect(m_global_context, m_global_init);
  JSObjectDeleteProperty(m_global_context, global_object, global_init, NULL);
  JSStringRelease(global_init);

  JSObjectRef o = JSObjectMakeArray(m_global_context, 0, NULL, NULL);
  m_array_contructor = js_get_object_property(m_global_context, o, "constructor");
  JSValueProtect(m_global_context, m_array_contructor);
}/*}}}*/

gboolean 
scripts_execute_one(const char *script) {
  if (m_global_context != NULL)
    return js_execute(m_global_context, script, NULL) != NULL;
  return false;
}
void
scripts_unbind(JSObjectRef obj) {
  if (obj != NULL) {
    JSValueUnprotect(m_global_context, obj);
  }
}

/* scripts_end {{{*/
void
scripts_end() {
  if (m_global_context != NULL) {
    JSValueUnprotect(m_global_context, m_array_contructor);
    JSClassRelease(m_default_class);
    JSClassRelease(m_webview_class);
    JSClassRelease(m_frame_class);
    JSClassRelease(m_download_class);
    JSClassRelease(m_message_class);
    JSGlobalContextRelease(m_global_context);
    m_global_context = NULL;
  }
}/*}}}*//*}}}*/
