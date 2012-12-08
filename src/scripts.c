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

typedef struct Sigmap_s {
    int sig;
    const char *name;
} Sigmap;

typedef struct _CallbackData CallbackData;
typedef struct _SSignal SSignal;
typedef gboolean (*StopCallbackNotify)(CallbackData *);

struct _CallbackData {
    GObject *gobject;
    JSObjectRef object;
    JSObjectRef callback;
    StopCallbackNotify notify;
};
struct _SSignal {
    int id;
    GSignalQuery *query;
    GObject *object;
    JSObjectRef func;
};
typedef struct DeferredPriv_s 
{
    JSObjectRef reject;
    JSObjectRef resolve;
    JSObjectRef next;
} DeferredPriv;
//static GSList *s_signals;
#define S_SIGNAL(X) ((SSignal*)X->data)


static Sigmap s_sigmap[] = {
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


static JSObjectRef make_object_for_class(JSContextRef ctx, JSClassRef class, GObject *o, gboolean);

static JSValueRef connect_object(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef disconnect_object(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);

static JSValueRef wv_load_uri(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_history(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_reload(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef wv_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
#if WEBKIT_CHECK_VERSION(1, 10, 0)
static JSValueRef wv_to_png(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc);
#endif
static JSStaticFunction default_functions[] = { 
    { "connect",         connect_object,                kJSDefaultAttributes },
    { "disconnect",      disconnect_object,             kJSDefaultAttributes },
    { 0, 0, 0 }, 
};
static JSStaticFunction wv_functions[] = { 
    { "loadUri",         wv_load_uri,             kJSDefaultAttributes },
    { "history",         wv_history,             kJSDefaultAttributes },
    { "reload",          wv_reload,             kJSDefaultAttributes },
    { "inject",          wv_inject,             kJSDefaultAttributes },
#if WEBKIT_CHECK_VERSION(1, 10, 0)
    { "toPng",           wv_to_png,             kJSDefaultAttributes },
#endif
    { 0, 0, 0 }, 
};
static JSValueRef wv_get_main_frame(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_focused_frame(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_all_frames(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_number(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_tab_widget(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_tab_box(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_tab_label(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_tab_icon(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSValueRef wv_get_scrolled_window(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception);
static JSStaticValue wv_values[] = {
    { "mainFrame",     wv_get_main_frame, NULL, kJSDefaultAttributes }, 
    { "focusedFrame",  wv_get_focused_frame, NULL, kJSDefaultAttributes }, 
    { "allFrames",     wv_get_all_frames, NULL, kJSDefaultAttributes }, 
    { "number",        wv_get_number, NULL, kJSDefaultAttributes }, 
    { "tabWidget",     wv_get_tab_widget, NULL, kJSDefaultAttributes }, 
    { "tabBox",        wv_get_tab_box, NULL, kJSDefaultAttributes }, 
    { "tabLabel",      wv_get_tab_label, NULL, kJSDefaultAttributes }, 
    { "tabIcon",       wv_get_tab_icon, NULL, kJSDefaultAttributes }, 
    { "scrolledWindow",wv_get_scrolled_window, NULL, kJSDefaultAttributes }, 
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
enum {
    CONSTRUCTOR_DEFAULT = 0,
    CONSTRUCTOR_WEBVIEW,
    CONSTRUCTOR_DOWNLOAD,
    CONSTRUCTOR_FRAME,
    CONSTRUCTOR_SOUP_MESSAGE,
    CONSTRUCTOR_DEFERRED,
    CONSTRUCTOR_LAST,
};



static void callback(CallbackData *c);
static void make_callback(JSContextRef ctx, JSObjectRef this, GObject *gobject, const char *signalname, JSValueRef value, StopCallbackNotify notify, JSValueRef *exception);
static JSObjectRef make_object(JSContextRef ctx, GObject *o);

/* Static variables */
static JSObjectRef s_sig_objects[SCRIPTS_SIG_LAST];
static JSGlobalContextRef s_global_context;
static GSList *s_script_list;
static JSClassRef s_gobject_class, s_webview_class, s_frame_class, s_download_class, s_download_class, s_message_class, s_deferred_class;
static gboolean s_commandline = false;
static JSObjectRef s_array_contructor;
static JSObjectRef s_completion_callback;
static JSObjectRef s_sp_scripts_cb;
static JSObjectRef s_sp_scratchpad_cb;
static GQuark s_ref_quark;
static JSObjectRef s_init_before, s_init_after;
static JSObjectRef s_constructors[CONSTRUCTOR_LAST];

/* Only defined once */
static JSValueRef UNDEFINED, NIL;

/* MISC {{{*/
/* uncamelize {{{*/
static char *
uncamelize(char *uncamel, const char *camel, char rep, size_t length) 
{
    char *ret = uncamel;
    size_t written = 0;
    if (isupper(*camel) && length > 1) 
    {
        *uncamel++ = tolower(*camel++);
        written++;
    }
    while (written++<length-1 && *camel != 0) 
    {
        if (isupper(*camel)) 
        {
            *uncamel++ = rep;
            if (++written >= length-1)
                break;
            *uncamel++ = tolower(*camel++);
        }
        else 
            *uncamel++ = *camel++;
    }
    *uncamel = 0;
    return ret;
}/*}}}*/

/* inject {{{*/
static JSValueRef
inject(JSContextRef ctx, JSContextRef wctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    JSValueRef ret = NIL;
    gboolean global = false;
    JSValueRef args[1];
    int count = 0;
    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("inject: missing argument"));
        return NIL;
    }
    if (argc > 1 && !JSValueIsNull(ctx, argv[1])) 
    {
        args[0] = js_context_change(ctx, wctx, argv[1], exc);
        count = 1;
    }
    if (argc > 2 && JSValueIsBoolean(ctx, argv[2])) 
        global = JSValueToBoolean(ctx, argv[2]);

    JSStringRef script = JSValueToStringCopy(ctx, argv[0], exc);
    if (script == NULL) 
        return NIL;

    if (global) 
        JSEvaluateScript(wctx, script, NULL, NULL, 0, NULL);
    else 
    {
        JSObjectRef func = JSObjectMakeFunction(wctx, NULL, 0, NULL, script, NULL, 0, NULL);
        if (func != NULL && JSObjectIsFunction(ctx, func)) 
        {
            JSValueRef wret = JSObjectCallAsFunction(wctx, func, NULL, count, count == 1 ? args : NULL, NULL) ;
            // This could be replaced with js_context_change
            char *retx = js_value_to_json(wctx, wret, -1, NULL);
            if (retx) 
            {
                ret = js_char_to_value(ctx, retx);
                g_free(retx);
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
callback_data_new(GObject *gobject, JSObjectRef object, JSObjectRef callback, StopCallbackNotify notify)  
{
    CallbackData *c = g_malloc(sizeof(CallbackData));
    c->gobject = gobject != NULL ? g_object_ref(gobject) : NULL;
    if (object != NULL) 
    {
        JSValueProtect(s_global_context, object);
        c->object = object;
    }
    if (object != NULL) 
    {
        JSValueProtect(s_global_context, callback);
        c->callback = callback;
    }
    c->notify = notify;
    return c;
}/*}}}*/

/* callback_data_free {{{*/
static void
callback_data_free(CallbackData *c) 
{
    if (c != NULL) 
    {
        if (c->gobject != NULL) 
            g_object_unref(c->gobject);
        if (c->object != NULL) 
            JSValueUnprotect(s_global_context, c->object);
        JSValueUnprotect(s_global_context, c->object);
        if (c->object != NULL) 
            JSValueUnprotect(s_global_context, c->callback);
        g_free(c);
    }
}/*}}}*/

static void 
ssignal_free(SSignal *sig) 
{
    if (sig != NULL) 
    {
        g_free(sig->query);
        g_free(sig);
    }
}

static SSignal * 
ssignal_new()
{
    SSignal *sig = g_malloc(sizeof(SSignal)); 
    if (sig != NULL) 
    {
        sig->query = g_malloc(sizeof(GSignalQuery));
        if (sig->query != NULL) 
            return sig;
        g_free(sig);
    }
    return NULL;
}

/* make_callback {{{*/
static void 
make_callback(JSContextRef ctx, JSObjectRef this, GObject *gobject, const char *signalname, JSValueRef value, StopCallbackNotify notify, JSValueRef *exception) 
{
    JSObjectRef func = js_value_to_function(ctx, value, exception);
    if (func != NULL) 
    {
        CallbackData *c = callback_data_new(gobject, this, func, notify);
        g_signal_connect_swapped(gobject, signalname, G_CALLBACK(callback), c);
    }
}/*}}}*/

/* callback {{{*/
static void 
callback(CallbackData *c) 
{
    gboolean ret = false;
    JSValueRef val[] = { c->object != NULL ? c->object : NIL };
    JSValueRef jsret = JSObjectCallAsFunction(s_global_context, c->callback, NULL, 1, val, NULL);
    if (JSValueIsBoolean(s_global_context, jsret))
        ret = JSValueToBoolean(s_global_context, jsret);
    if (ret || (c != NULL && c->gobject != NULL && c->notify != NULL && c->notify(c))) 
    {
        g_signal_handlers_disconnect_by_func(c->gobject, callback, c);
        callback_data_free(c);
    }
}/*}}}*/
/*}}}*/

/* TABS {{{*/
/* tabs_current {{{*/
static JSValueRef 
tabs_current(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) 
{
    if (dwb.state.fview && CURRENT_VIEW()->script_wv) 
        return CURRENT_VIEW()->script_wv;
    else 
        return NIL;
}/*}}}*/

/* tabs_number {{{*/
static JSValueRef 
tabs_number(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) 
{
    return JSValueMakeNumber(ctx, g_list_position(dwb.state.views, dwb.state.fview));
}/*}}}*/

/* tabs_length {{{*/
static JSValueRef 
tabs_length(JSContextRef ctx, JSObjectRef this, JSStringRef name, JSValueRef* exc) 
{
    return JSValueMakeNumber(ctx, g_list_length(dwb.state.views));
}/*}}}*/

/* tabs_get_nth {{{*/
static JSValueRef 
tabs_get_nth(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("tabs.nth: missing argument"));
        return NIL;
    }
    double n = JSValueToNumber(ctx, argv[0], exc);
    if (n == NAN)
        return NIL;
    GList *nth = g_list_nth(dwb.state.views, (int)n);
    if (nth == NULL)
        return NIL;
    return VIEW(nth)->script_wv;
}/*}}}*/
/*}}}*/

/* WEBVIEW {{{*/

static GList *
find_webview(JSObjectRef o) 
{
    for (GList *r = dwb.state.fview; r; r=r->next)
        if (VIEW(r)->script_wv == o)
            return r;
    for (GList *r = dwb.state.fview->prev; r; r=r->prev)
        if (VIEW(r)->script_wv == o)
            return r;
    return NULL;
}
/* wv_status_cb {{{*/
static gboolean 
wv_status_cb(CallbackData *c) 
{
    WebKitLoadStatus status = webkit_web_view_get_load_status(WEBKIT_WEB_VIEW(c->gobject));
    if (status == WEBKIT_LOAD_FINISHED || status == WEBKIT_LOAD_FAILED) 
        return true;
    return false;
}/*}}}*/

/* wv_load_uri {{{*/
static JSValueRef 
wv_load_uri(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc == 0) 
    {
        js_make_exception(ctx, exc, EXCEPTION("webview.loadUri: missing argument."));
        return JSValueMakeBoolean(ctx, false);
    }
    WebKitWebView *wv = JSObjectGetPrivate(this);

    if (wv != NULL) 
    {
        char *uri = js_value_to_char(ctx, argv[0], -1, exc);
        if (uri == NULL)
            return false;
        webkit_web_view_load_uri(wv, uri);
        g_free(uri);
        if (argc > 1)  
            make_callback(ctx, this, G_OBJECT(wv), "notify::load-status", argv[1], wv_status_cb, exc);

        return JSValueMakeBoolean(ctx, true);
    }
    return false;
}/*}}}*/

/* wv_history {{{*/
static JSValueRef 
wv_history(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("webview.history: missing argument."));
        return UNDEFINED;
    }
    double steps = JSValueToNumber(ctx, argv[0], exc);
    if (steps != NAN) {
        WebKitWebView *wv = JSObjectGetPrivate(this);
        if (wv != NULL)
            webkit_web_view_go_back_or_forward(wv, (int)steps);
    }
    return UNDEFINED;
}/*}}}*/

/* wv_reload {{{*/
static JSValueRef 
wv_reload(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    WebKitWebView *wv = JSObjectGetPrivate(this);
    if (wv != NULL)
        webkit_web_view_reload(wv);
    return UNDEFINED;
}/*}}}*/

/* wv_inject {{{*/
static JSValueRef 
wv_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    WebKitWebView *wv = JSObjectGetPrivate(this);
    if (wv != NULL) 
    {
        JSContextRef wctx = webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(wv));
        return inject(ctx, wctx, function, this, argc, argv, exc);
    }
    return NIL;
}/*}}}*/
#if WEBKIT_CHECK_VERSION(1, 10, 0)
static JSValueRef 
wv_to_png(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    WebKitWebView *wv;
    cairo_status_t status = -1;
    cairo_surface_t *sf;
    char *filename;
    if (argc < 1 || (wv = JSObjectGetPrivate(this)) == NULL || (JSValueIsNull(ctx, argv[0])) || (filename = js_value_to_char(ctx, argv[0], -1, NULL)) == NULL) 
    {
        return JSValueMakeNumber(ctx, status);
    }
    if (argc > 2) 
    {
        gboolean keep_aspect = false;
        double width = JSValueToNumber(ctx, argv[1], exc);
        double height = JSValueToNumber(ctx, argv[2], exc);
        if (width != NAN && height != NAN) 
        {
            sf = webkit_web_view_get_snapshot(wv);

            if (argc > 3 && JSValueIsBoolean(ctx, argv[3])) 
                keep_aspect = JSValueToBoolean(ctx, argv[3]);
            if (keep_aspect && (width <= 0 || height <= 0))
                return JSValueMakeNumber(ctx, status);


            int w = cairo_image_surface_get_width(sf);
            int h = cairo_image_surface_get_height(sf);

            double aspect = (double)w/h;
            double new_width = width;
            double new_height = height;

            if (width <= 0 || keep_aspect)
                new_width = height * aspect;
            if ((width > 0 && height <= 0) || keep_aspect)
                new_height = width / aspect;
            if (keep_aspect) 
            {
                if (new_width > width) 
                {
                    new_width = width;
                    new_height = new_width / aspect;
                }
                else if (new_height > height) 
                {
                    new_height = height;
                    new_width = new_height * aspect;
                }
            }

            double sw, sh;
            if (width <= 0 || height <= 0)
                sw = sh = MIN(width / w, height / h);
            else 
            {
                sw = width / w;
                sh = height / h;
            }

            cairo_surface_t *scaled_surface = cairo_surface_create_similar_image(sf, CAIRO_FORMAT_RGB24, new_width, new_height);
            cairo_t *cr = cairo_create(scaled_surface);

            cairo_save(cr);
            cairo_scale(cr, sw, sh);

            cairo_set_source_surface(cr, sf, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);

            cairo_destroy(cr);

            status = cairo_surface_write_to_png(scaled_surface, filename);
            cairo_surface_destroy(scaled_surface);
        }
        else 
            return JSValueMakeNumber(ctx, status);
    }
    else 
    {
        sf = webkit_web_view_get_snapshot(wv);
        status = cairo_surface_write_to_png(sf, filename);
    }
    cairo_surface_destroy(sf);

    return JSValueMakeNumber(ctx, status);
}
#endif
/* wv_get_main_frame {{{*/
static JSValueRef 
wv_get_main_frame(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    WebKitWebView *wv = JSObjectGetPrivate(object);
    if (wv != NULL) 
    {
        WebKitWebFrame *frame = webkit_web_view_get_main_frame(wv);
        return make_object(ctx, G_OBJECT(frame));
    }
    return NIL;
}/*}}}*/

/* wv_get_focused_frame {{{*/
static JSValueRef 
wv_get_focused_frame(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    WebKitWebView *wv = JSObjectGetPrivate(object);
    if (wv != NULL) 
    {
        WebKitWebFrame *frame = webkit_web_view_get_focused_frame(wv);
        return make_object(ctx, G_OBJECT(frame));
    }
    return NIL;
}/*}}}*/

/* wv_get_all_frames {{{*/
static JSValueRef 
wv_get_all_frames(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    int argc, n = 0;
    GList *gl = find_webview(object);
    if (gl == NULL)
        return NIL;
    argc = g_slist_length(VIEW(gl)->status->frames);

    JSValueRef argv[argc];
    n=0;

    for (GSList *sl = VIEW(gl)->status->frames; sl; sl=sl->next) 
        argv[n++] = make_object(ctx, G_OBJECT(sl->data));

    return JSObjectMakeArray(ctx, argc, argv, exception);
}/*}}}*/

/* wv_get_number {{{*/
static JSValueRef 
wv_get_number(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    GList *gl = dwb.state.views;
    for (int i=0; gl; i++, gl=gl->next) 
    {
        if (object == VIEW(gl)->script_wv) 
            return JSValueMakeNumber(ctx, i); 
    }
    return JSValueMakeNumber(ctx, -1); 
}/*}}}*/

static JSValueRef 
wv_get_tab_widget(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    GList *gl = find_webview(object);
    if (gl == NULL)
        return NIL;
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(VIEW(gl)->tabevent), true);
}
static JSValueRef 
wv_get_tab_box(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    GList *gl = find_webview(object);
    if (gl == NULL)
        return NIL;
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(VIEW(gl)->tabbox), true);
}
static JSValueRef 
wv_get_tab_label(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    GList *gl = find_webview(object);
    if (gl == NULL)
        return NIL;
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(VIEW(gl)->tablabel), true);
}
static JSValueRef 
wv_get_tab_icon(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    GList *gl = find_webview(object);
    if (gl == NULL)
        return NIL;
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(VIEW(gl)->tabicon), true);
}

static JSValueRef 
wv_get_scrolled_window(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    GList *gl = find_webview(object);
    if (gl == NULL)
        return NIL;
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(VIEW(gl)->scroll), true);
}


/*}}}*/

static JSValueRef 
sp_show(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    scratchpad_show();
    return UNDEFINED;
}
static JSValueRef 
sp_hide(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    scratchpad_hide();
    return UNDEFINED;
}
static JSValueRef 
sp_load(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    char *text;
    if (argc > 0 && (text = js_value_to_char(ctx, argv[0], -1, exc)) != NULL) 
    {
        scratchpad_load(text);
        g_free(text);
    }
    return UNDEFINED;
}
static JSObjectRef 
sp_callback_create(JSContextRef ctx, size_t argc, const JSValueRef argv[], JSValueRef *exc) 
{
    JSObjectRef ret = NULL;
    if (argc > 0)
    {
        ret = js_value_to_function(ctx, argv[0], exc);
    }
    return ret;
}
static JSValueRef 
sp_get(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    s_sp_scripts_cb = sp_callback_create(ctx, argc, argv, exc);
    return UNDEFINED;
}
void 
scripts_scratchpad_get(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    s_sp_scratchpad_cb = sp_callback_create(ctx, argc, argv, exc);
}
void
sp_context_change(JSContextRef src_ctx, JSContextRef dest_ctx, JSObjectRef func, JSValueRef val) 
{
    if (func != NULL) 
    {
        JSValueRef val_changed = js_context_change(src_ctx, dest_ctx, val, NULL);
        JSValueRef argv[] = { val_changed == 0 ? NIL : val_changed };
        JSObjectCallAsFunction(dest_ctx, func, NULL, 1, argv, NULL);
    }
}
// send from scripts context to scratchpad context
static JSValueRef 
sp_send(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc > 0) 
        sp_context_change(s_global_context, webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(scratchpad_get()))), s_sp_scratchpad_cb, argv[0]);

    return UNDEFINED;
}
// send from scratchpad context to script context
void 
scripts_scratchpad_send(JSContextRef ctx, JSValueRef val) 
{
    sp_context_change(ctx, s_global_context, s_sp_scripts_cb, val);
}

/* SOUP_MESSAGE {{{*/
static JSValueRef 
get_soup_uri(JSContextRef ctx, JSObjectRef object, SoupURI * (*func)(SoupMessage *), JSValueRef *exception)
{
    SoupMessage *msg = JSObjectGetPrivate(object);
    if (msg == NULL)
        return NIL;

    SoupURI *uri = func(msg);
    if (uri == NULL)
        return NIL;

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
}

/* message_get_uri {{{*/
static JSValueRef 
message_get_uri(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    return get_soup_uri(ctx, object, soup_message_get_uri, exception);
}/*}}}*/

/* message_get_first_party {{{*/
static JSValueRef 
message_get_first_party(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    return get_soup_uri(ctx, object, soup_message_get_first_party, exception);
}/*}}}*/
/*}}}*/

/* FRAMES {{{*/
/* frame_get_domain {{{*/
static JSValueRef 
frame_get_domain(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    WebKitWebFrame *frame = JSObjectGetPrivate(object);
    if (frame == NULL)
        return NIL;

    SoupMessage *msg = dwb_soup_get_message(frame);
    if (msg == NULL)
        return NIL;

    SoupURI *uri = soup_message_get_uri(msg);
    const char *host = soup_uri_get_host(uri);

    return js_char_to_value(ctx, domain_get_base_for_host(host));
}/*}}}*/

/* frame_get_host {{{*/
static JSValueRef 
frame_get_host(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    WebKitWebFrame *frame = JSObjectGetPrivate(object);
    if (frame == NULL)
        return NIL;

    SoupMessage *msg = dwb_soup_get_message(frame);
    if (msg == NULL)
        return NIL;

    SoupURI *uri = soup_message_get_uri(msg);
    return js_char_to_value(ctx, soup_uri_get_host(uri));
}/*}}}*/

/* frame_inject {{{*/
static JSValueRef 
frame_inject(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    WebKitWebFrame *frame = JSObjectGetPrivate(this);
    if (frame != NULL) 
    {
        JSContextRef wctx = webkit_web_frame_get_global_context(frame);
        return inject(ctx, wctx, function, this, argc, argv, exc);
    }
    return NIL;
}/*}}}*/
/*}}}*/

/* GLOBAL {{{*/
/* global_checksum {{{*/
static JSValueRef 
global_checksum(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    char *checksum = NULL;
    guchar *original = NULL;
    JSValueRef ret;

    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("checksum: missing argument."));
        return NIL;
    }
    original = (guchar*)js_value_to_char(ctx, argv[0], -1, exc);
    if (original == NULL)
        return NIL;

    GChecksumType type = G_CHECKSUM_SHA256;
    if (argc > 1) 
    {
        type = JSValueToNumber(ctx, argv[1], exc);
        if (type == NAN) 
        {
            ret = NIL;
            goto error_out;
        }
        type = MIN(MAX(type, G_CHECKSUM_MD5), G_CHECKSUM_SHA256);
    }
    checksum = g_compute_checksum_for_data(type, original, -1);

    ret = js_char_to_value(ctx, checksum);

error_out:
    g_free(original);
    g_free(checksum);
    return ret;
}/*}}}*/

/* scripts_eval_key {{{*/
DwbStatus
scripts_eval_key(KeyMap *m, Arg *arg) 
{
    char *json = NULL;
    CLEAR_COMMAND_TEXT();
    if (arg->p == NULL) 
        json = util_create_json(1, INTEGER, "nummod", dwb.state.nummod);
    else 
        json = util_create_json(2, INTEGER, "nummod", dwb.state.nummod, CHAR, "arg", arg->p);

    JSValueRef argv[] = { js_json_to_value(s_global_context, json) };
    JSObjectCallAsFunction(s_global_context, arg->arg, NULL, 1, argv, NULL);

    g_free(json);
    return STATUS_OK;
}/*}}}*/

/* global_unbind{{{*/
static JSValueRef 
global_unbind(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("unbind: missing argument."));
        return JSValueMakeBoolean(ctx, false);
    }
    GList *l = NULL;
    KeyMap *m;
    if (JSValueIsString(ctx, argv[0])) 
    {
        char *name = js_value_to_char(ctx, argv[0], JS_STRING_MAX, exc);
        for (l = dwb.keymap; l && g_strcmp0(((KeyMap*)l->data)->map->n.first, name); l=l->next)
            ;
        g_free(name);
    }
    else if (JSValueIsObject(ctx, argv[0])) 
    {
        for (l = dwb.keymap; l && !JSValueIsEqual(ctx, argv[0], ((KeyMap*)l->data)->map->arg.arg, exc); l=l->next)
            ;
    }
    if (l != NULL) 
    {
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
global_bind(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    gchar *keystr, *callback_name;
    gboolean ret = false;
    char *name = NULL, *callback = NULL;
    guint option = CP_DONT_SAVE | CP_SCRIPT;

    if (argc < 2) 
    {
        js_make_exception(ctx, exc, EXCEPTION("bind: missing argument."));
        return JSValueMakeBoolean(ctx, false);
    }
    keystr = js_value_to_char(ctx, argv[0], JS_STRING_MAX, exc);

    JSObjectRef func = js_value_to_function(ctx, argv[1], exc);
    if (func == NULL)
        goto error_out;

    if (argc > 2) 
    {
        name = js_value_to_char(ctx, argv[2], JS_STRING_MAX, exc);
        if (name != NULL) 
        { 
            option |= CP_COMMANDLINE;
            callback_name = js_get_string_property(ctx, func, "name");
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

    FunctionMap fm = { { name, callback }, option, (Func)scripts_eval_key, NULL, POST_SM, { .arg = func }, EP_NONE,  {NULL} };
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
global_execute(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    DwbStatus status = STATUS_ERROR;
    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("execute: missing argument."));
        return JSValueMakeBoolean(ctx, false);
    }
    char *command = js_value_to_char(ctx, argv[0], -1, exc);
    if (command != NULL) 
    {
        status = dwb_parse_command_line(command);
        g_free(command);
    }
    return JSValueMakeBoolean(ctx, status == STATUS_OK);
}/*}}}*/

/* global_exit {{{*/
static JSValueRef 
global_exit(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (s_commandline)
        application_stop();
    else 
        dwb_end();
    return UNDEFINED;
}/*}}}*/

/* global_include {{{*/
static JSValueRef 
global_include(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    JSValueRef ret = NULL;
    JSStringRef script;
    gboolean global = false;
    char *path = NULL, *content = NULL; 

    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("include: missing argument."));
        return NIL;
    }

    if (argc > 1 && JSValueIsBoolean(ctx, argv[1])) 
        global = JSValueToBoolean(ctx, argv[1]);

    if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX, exc)) == NULL) 
        goto error_out;

    if ( (content = util_get_file_content(path, NULL)) == NULL) 
    {
        js_make_exception(ctx, exc, EXCEPTION("include: reading %s failed."), path);
        goto error_out;
    }

    script = JSStringCreateWithUTF8CString(content);

    if (global) 
        ret = JSEvaluateScript(ctx, script, NULL, NULL, 0, exc);
    else 
    {
        JSObjectRef function = JSObjectMakeFunction(ctx, NULL, 0, NULL, script, NULL, 0, exc);
        if (function != NULL) 
        {
            JSObjectRef this = JSObjectMake(ctx, NULL, NULL);
            JSValueProtect(ctx, this);
            ret = JSObjectCallAsFunction(ctx, function, this, 0, NULL, exc);
        }
    }
    JSStringRelease(script);

error_out: 
    g_free(content);
    g_free(path);
    if (ret == NULL)
        return NIL;
    return ret;
}/*}}}*/


/* global_send_request {{{*/
static JSObjectRef 
get_message_data(SoupMessage *msg) 
{
    const char *name, *value;
    SoupMessageHeadersIter iter;
    JSObjectRef o, ho;
    JSStringRef s;

    o = JSObjectMake(s_global_context, NULL, NULL);
    js_set_object_property(s_global_context, o, "body", msg->response_body->data, NULL);

    ho = JSObjectMake(s_global_context, NULL, NULL);

    soup_message_headers_iter_init(&iter, msg->response_headers);
    while (soup_message_headers_iter_next(&iter, &name, &value)) 
        js_set_object_property(s_global_context, ho, name, value, NULL);

    s = JSStringCreateWithUTF8CString("headers");
    JSObjectSetProperty(s_global_context, o, s, ho, kJSDefaultProperty, NULL);
    JSStringRelease(s);
    return o;
}
static void
request_callback(SoupSession *session, SoupMessage *message, JSObjectRef function) 
{
    if (message->response_body->data != NULL) 
    {
        JSObjectRef o = get_message_data(message);
        JSValueRef vals[] = { o, make_object(s_global_context, G_OBJECT(message))  };
        JSObjectCallAsFunction(s_global_context, function, NULL, 2, vals, NULL);
    }
    JSValueUnprotect(s_global_context, function);
}
static JSValueRef 
global_send_request(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    gint ret = -1;
    char *method = NULL, *uri = NULL;
    SoupMessage *msg;
    JSObjectRef function;
    if (argc < 2) 
    {
        js_make_exception(ctx, exc, EXCEPTION("sendRequest: missing argument."));
        return JSValueMakeNumber(ctx, -1);
    }

    uri = js_value_to_char(ctx, argv[0], -1, exc);
    if (uri == NULL) 
        return JSValueMakeNumber(ctx, -1);

    function = js_value_to_function(ctx, argv[1], exc);
    if (function == NULL)
        goto error_out;

    if (argc > 2) 
        method = js_value_to_char(ctx, argv[2], -1, exc);

    msg = soup_message_new(method == NULL ? "GET" : method, uri);
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
global_send_request_sync(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    char *method = NULL, *uri = NULL;
    SoupMessage *msg;
    guint status;
    JSObjectRef o;
    JSStringRef js_key;
    JSValueRef js_value;

    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("sendRequestSync: missing argument."));
        return NIL;
    }
    uri = js_value_to_char(ctx, argv[0], -1, exc);
    if (uri == NULL) 
        return NIL;

    if (argc > 1) 
        method = js_value_to_char(ctx, argv[1], -1, exc);

    msg = soup_message_new(method == NULL ? "GET" : method, uri);

    status = soup_session_send_message(webkit_get_default_session(), msg);
    o = get_message_data(msg);

    js_key = JSStringCreateWithUTF8CString("status");
    js_value = JSValueMakeNumber(ctx, status);
    JSObjectSetProperty(ctx, o, js_key, js_value, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly, exc);

    JSStringRelease(js_key);
    return o;
}
void 
scripts_completion_activate(void) 
{
    const char *text = GET_TEXT();
    JSValueRef val[] = { js_char_to_value(s_global_context, text) };
    JSObjectCallAsFunction(s_global_context, s_completion_callback, NULL, 1, val, NULL);
    completion_clean_completion(false);
    dwb_change_mode(NORMAL_MODE, true);
}
static JSValueRef 
global_tab_complete(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 3 || !JSValueIsInstanceOfConstructor(ctx, argv[1], s_array_contructor, exc)) 
    {
        js_make_exception(ctx, exc, EXCEPTION("tabComplete: invalid argument."));
        return UNDEFINED;
    }
    s_completion_callback = js_value_to_function(ctx, argv[2], exc);
    if (s_completion_callback == NULL)
        return UNDEFINED;

    dwb.state.script_comp_readonly = false;
    if (argc > 3 && JSValueIsBoolean(ctx, argv[3])) 
    {
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
    while((val = js_array_iterator_next(&iter, exc))) 
    {
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
    for (GList *l = dwb.state.script_completion; l; l=l->next) 
    {
        n = l->data;
        g_free(n->first); 
        g_free(n->second);
        g_free(n);
    }
    g_free(label);
    g_list_free(dwb.state.script_completion);
    dwb.state.script_completion = NULL;
    return UNDEFINED;
}
/* timeout_callback {{{*/
static gboolean
timeout_callback(JSObjectRef obj) 
{
    gboolean ret;
    JSValueRef val = JSObjectCallAsFunction(s_global_context, obj, NULL, 0, NULL, NULL);
    if (val == NULL)
        ret = false;
    else 
        ret = !JSValueIsBoolean(s_global_context, val) || JSValueToBoolean(s_global_context, val);

    if (! ret )
        JSValueUnprotect(s_global_context, obj);
    return ret;
}/*}}}*/

/* global_timer_stop {{{*/
static JSValueRef 
global_timer_stop(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    gdouble sigid;
    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("timerStop: missing argument."));
        return JSValueMakeBoolean(ctx, false);
    }
    if ((sigid = JSValueToNumber(ctx, argv[0], exc)) != NAN) 
    {
        gboolean ret = g_source_remove((int)sigid);
        return JSValueMakeBoolean(ctx, ret);
    }
    return JSValueMakeBoolean(ctx, false);
}/*}}}*/

/* global_timer_start {{{*/
static JSValueRef 
global_timer_start(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 2) 
    {
        js_make_exception(ctx, exc, EXCEPTION("timerStart: missing argument."));
        return JSValueMakeNumber(ctx, -1);
    }
    double msec = 10;
    if ((msec = JSValueToNumber(ctx, argv[0], exc)) == NAN )
        return JSValueMakeNumber(ctx, -1);

    JSObjectRef func = js_value_to_function(ctx, argv[1], exc);
    if (func == NULL)
        return JSValueMakeNumber(ctx, -1);

    JSValueProtect(ctx, func);

    int ret = g_timeout_add((int)msec, (GSourceFunc)timeout_callback, func);

    return JSValueMakeNumber(ctx, ret);
}/*}}}*/
/*}}}*/

/* UTIL {{{*/
/* util_domain_from_host {{{*/
static JSValueRef 
util_domain_from_host(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("domainFromHost: missing argument."));
        return JSValueMakeBoolean(ctx, false);
    }
    char *host = js_value_to_char(ctx, argv[0], -1, exc);
    const char *domain = domain_get_base_for_host(host);
    if (domain == NULL)
        return NIL;

    JSValueRef ret = js_char_to_value(ctx, domain);
    g_free(host);
    return ret;
}/*}}}*//*}}}*/
static JSValueRef 
util_markup_escape(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    char *string = NULL, *escaped = NULL;
    if (argc > 0) 
    {
        string = js_value_to_char(ctx, argv[0], -1, exc);
        if (string != NULL) 
        {
            escaped = g_markup_escape_text(string, -1);
            g_free(string);
            if (escaped != NULL) 
            {
                JSValueRef ret = js_char_to_value(ctx, escaped);
                g_free(escaped);
                return ret;
            }
        }
    }
    return NIL;
}
static JSValueRef 
util_get_mode(JSContextRef ctx, JSObjectRef f, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    return JSValueMakeNumber(ctx, BASIC_MODES(dwb.state.mode));
}

void 
deferred_destroy(JSContextRef ctx, JSObjectRef this, DeferredPriv *priv) 
{
    g_return_if_fail(this != NULL);

    if (priv == NULL)
        priv = JSObjectGetPrivate(this);
    JSObjectSetPrivate(this, NULL);

    g_free(priv);

    JSValueUnprotect(ctx, this);
}

static JSObjectRef
deferred_new(JSContextRef ctx) 
{
    DeferredPriv *priv = g_try_malloc(sizeof(DeferredPriv));
    priv->resolve = priv->reject = priv->next = NULL;

    JSObjectRef ret = JSObjectMake(ctx, s_deferred_class, priv);
    JSValueProtect(ctx, ret);

    return ret;
}
static JSValueRef 
deferred_then(JSContextRef ctx, JSObjectRef f, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    DeferredPriv *priv = JSObjectGetPrivate(this);
    if (priv == NULL) 
        return NIL;

    if (argc > 0)
        priv->resolve = js_value_to_function(ctx, argv[0], NULL);
    if (argc > 1) 
        priv->reject = js_value_to_function(ctx, argv[1], NULL);

    priv->next = deferred_new(ctx);

    return priv->next;
}
static DeferredPriv * 
deferred_transition(JSContextRef ctx, JSObjectRef old, JSObjectRef new)
{
    DeferredPriv *opriv = JSObjectGetPrivate(old);
    DeferredPriv *npriv = JSObjectGetPrivate(new);

    npriv->resolve = opriv->resolve;
    npriv->reject = opriv->reject;
    npriv->next = opriv->next;

    deferred_destroy(ctx, old, opriv);
    return npriv;
}
static JSValueRef 
deferred_resolve(JSContextRef ctx, JSObjectRef f, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    JSValueRef ret = NULL;

    DeferredPriv *priv = JSObjectGetPrivate(this);
    if (priv == NULL)
        return UNDEFINED;

    if (priv->resolve) 
        ret = JSObjectCallAsFunction(ctx, priv->resolve, NULL, argc, argv, exc);

    JSObjectRef next = priv->next;
    deferred_destroy(ctx, this, priv);

    if (next) 
    {
        if ( ret && JSValueIsObjectOfClass(ctx, ret, s_deferred_class) ) 
        {
            JSObjectRef o = JSValueToObject(ctx, ret, NULL);
            deferred_transition(ctx, next, o)->reject = NULL;
        }
        else 
            deferred_resolve(ctx, f, next, argc, argv, exc);
    }
    return UNDEFINED;
}
static JSValueRef 
deferred_reject(JSContextRef ctx, JSObjectRef f, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    JSValueRef ret = NULL;

    DeferredPriv *priv = JSObjectGetPrivate(this);
    if (priv == NULL)
        return UNDEFINED;

    if (priv->reject) 
        ret = JSObjectCallAsFunction(ctx, priv->reject, NULL, argc, argv, exc);

    JSObjectRef next = priv->next;
    deferred_destroy(ctx, this, priv);

    if (next) 
    {
        if ( ret && JSValueIsObjectOfClass(ctx, ret, s_deferred_class) ) 
        {
            JSObjectRef o = JSValueToObject(ctx, ret, NULL);
            deferred_transition(ctx, next, o)->resolve = NULL;
        }
        else 
            deferred_reject(ctx, f, next, argc, argv, exc);
    }
    return UNDEFINED;
}

/* DATA {{{*/
/* data_get_profile {{{*/
static JSValueRef 
data_get_profile(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    return js_char_to_value(ctx, dwb.misc.profile);
}/*}}}*/

/* data_get_cache_dir {{{*/
static JSValueRef 
data_get_cache_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    return js_char_to_value(ctx, dwb.files[FILES_CACHEDIR]);
}/*}}}*/

/* data_get_config_dir {{{*/
static JSValueRef 
data_get_config_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    char *dir = util_build_path();
    if (dir == NULL) 
        return NIL;

    JSValueRef ret = js_char_to_value(ctx, dir);
    g_free(dir);
    return ret;
}/*}}}*/

/* data_get_system_data_dir {{{*/
static JSValueRef 
data_get_system_data_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    char *dir = util_get_system_data_dir(NULL);
    if (dir == NULL) 
        return NIL;

    JSValueRef ret = js_char_to_value(ctx, dir);
    g_free(dir);
    return ret;
}/*}}}*/

/* data_get_user_data_dir {{{*/
static JSValueRef 
data_get_user_data_dir(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef* exception) 
{
    char *dir = util_get_user_data_dir(NULL);
    if (dir == NULL) 
        return NIL;

    JSValueRef ret = js_char_to_value(ctx, dir);
    g_free(dir);
    return ret;
}/*}}}*/
/*}}}*/

/* SYSTEM {{{*/
/* system_get_env {{{*/
static JSValueRef 
system_get_env(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 1) 
        return NIL;

    char *name = js_value_to_char(ctx, argv[0], -1, exc);
    if (name == NULL) 
        return NIL;

    const char *env = g_getenv(name);
    g_free(name);

    if (env == NULL)
        return NIL;

    return js_char_to_value(ctx, env);
}/*}}}*/

/* spawn_output {{{*/
static gboolean
spawn_output(GIOChannel *channel, GIOCondition condition, JSObjectRef callback) 
{
    char *content; 
    gsize length;
    if (condition == G_IO_HUP || condition == G_IO_ERR || condition == G_IO_NVAL) 
    {
        g_io_channel_unref(channel);
        return false;
    }
    else if (g_io_channel_read_to_end(channel, &content, &length, NULL) == G_IO_STATUS_NORMAL && content != NULL)  
    {
        JSValueRef arg = js_char_to_value(s_global_context, content);
        if (arg != NULL) 
        {
            JSValueRef argv[] = { arg };
            JSObjectCallAsFunction(s_global_context, callback, NULL, 1,  argv , NULL);
        }
        g_free(content);
        return true;
    }
    return false;
}/*}}}*/

/* {{{*/
static JSValueRef 
system_spawn_sync(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc<1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("system.spawnSync needs an argument."));
        return JSValueMakeBoolean(ctx, SPAWN_FAILED);
    }
    JSObjectRef ret = NULL;
    int srgc, status;
    char **srgv = NULL, *command = NULL, *out, *err;

    command = js_value_to_char(ctx, argv[0], -1, exc);
    if (command == NULL) 
        return NIL;

    if (g_shell_parse_argv(command, &srgc, &srgv, NULL) && 
            g_spawn_sync(NULL, srgv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &out, &err, &status, NULL)) 
    {
        ret = JSObjectMake(ctx, NULL, NULL);
        js_set_object_property(ctx, ret, "stdout", out, exc);
        js_set_object_property(ctx, ret, "stderr", err, exc);
        js_set_object_number_property(ctx, ret, "status", status, exc);
    }
    g_free(command);
    g_strfreev(srgv);

    if (ret == NULL)
        return NIL;

    return ret;
}/*}}}*/

/* system_spawn {{{*/
static JSValueRef 
system_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    int ret = 0; 
    int outfd, errfd;
    char **srgv = NULL, *cmdline = NULL;
    int srgc;
    GIOChannel *out_channel, *err_channel;
    JSObjectRef oc = NULL, ec = NULL;

    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("system.spawn needs an argument."));
        return JSValueMakeBoolean(ctx, SPAWN_FAILED);
    }
    if (argc > 1) 
    {
        oc = js_value_to_function(ctx, argv[1], NULL);
        if ( oc == NULL )
        {
            if (!JSValueIsNull(ctx, argv[1])) 
                ret |= SPAWN_STDOUT_FAILED;
            oc = NULL;
        }
    }
    if (argc > 2) {
        ec = js_value_to_function(ctx, argv[2], NULL);
        if ( ec == NULL )
        {
            if (!JSValueIsNull(ctx, argv[2])) 
                ret |= SPAWN_STDERR_FAILED;
            ec = NULL;
        }
    }
    cmdline = js_value_to_char(ctx, argv[0], -1, exc);
    if (cmdline == NULL) 
    {
        ret |= SPAWN_FAILED;
        goto error_out;
    }

    if (!g_shell_parse_argv(cmdline, &srgc, &srgv, NULL) || 
            !g_spawn_async_with_pipes(NULL, srgv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, 
                oc != NULL ? &outfd : NULL, 
                ec != NULL ? &errfd : NULL, NULL)) 
    {
        js_make_exception(ctx, exc, EXCEPTION("spawning %s failed."), cmdline);
        ret |= SPAWN_FAILED;
        goto error_out;
    }

    if (oc != NULL) 
    {
        out_channel = g_io_channel_unix_new(outfd);
        g_io_add_watch(out_channel, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL, (GIOFunc)spawn_output, oc);
        g_io_channel_set_close_on_unref(out_channel, true);
    }
    if (ec != NULL) 
    {
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
system_file_test(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 2) 
    {
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
system_mkdir(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    gboolean ret = false;
    if (argc < 2) 
    {
        js_make_exception(ctx, exc, EXCEPTION("system.mkdir needs an argument."));
        return JSValueMakeBoolean(ctx, false);
    }
    char *path = js_value_to_char(ctx, argv[0], PATH_MAX, exc);
    double mode = JSValueToNumber(ctx, argv[1], exc);
    if (path != NULL && mode != NAN) 
    {
        ret = g_mkdir_with_parents(path, (gint)mode) == 0;
    }
    g_free(path);
    return JSValueMakeBoolean(ctx, ret);

}/*}}}*/

/*}}}*/

/* IO {{{*/
/* io_prompt {{{*/
static JSValueRef 
io_prompt(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    char *prompt = NULL;
    gboolean visibility = true;
    if (argc > 0) 
        prompt = js_value_to_char(ctx, argv[0], JS_STRING_MAX, exc);

    if (argc > 1 && JSValueIsBoolean(ctx, argv[1])) 
        visibility = JSValueToBoolean(ctx, argv[1]);

    const char *response = dwb_prompt(visibility, prompt);
    g_free(prompt);

    if (response == NULL)
        return NIL;

    return js_char_to_value(ctx, response);
}/*}}}*/

/* io_read {{{*/
static JSValueRef 
io_read(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    JSValueRef ret = NULL;
    char *path = NULL, *content = NULL;
    if (argc < 1) 
    {
        js_make_exception(ctx, exc, EXCEPTION("io.read needs an argument."));
        return NIL;
    }
    if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX, exc) ) == NULL )
        goto error_out;

    if ( (content = util_get_file_content(path, NULL) ) == NULL ) 
        goto error_out;

    ret = js_char_to_value(ctx, content);

error_out:
    g_free(path);
    g_free(content);
    if (ret == NULL)
        return NIL;
    return ret;

}/*}}}*/

/* io_notify {{{*/
static JSValueRef 
io_notify(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 1) 
        return UNDEFINED;

    char *message = js_value_to_char(ctx, argv[0], -1, exc);
    if (message != NULL) 
    {
        dwb_set_normal_message(dwb.state.fview, true, message);
        g_free(message);
    }
    return UNDEFINED;
}/*}}}*/

/* io_error {{{*/
static JSValueRef 
io_error(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 1) 
        return UNDEFINED;

    char *message = js_value_to_char(ctx, argv[0], -1, exc);
    if (message != NULL) 
    {
        dwb_set_error_message(dwb.state.fview, message);
        g_free(message);
    }
    return UNDEFINED;
}/*}}}*/

static JSValueRef 
io_dir_names(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc < 1) 
        return NIL;

    JSValueRef ret;
    GDir *dir;
    char *dir_name = js_value_to_char(ctx, argv[0], PATH_MAX, exc);
    const char *name;

    if (dir_name == NULL)
        return NIL;

    if ((dir = g_dir_open(dir_name, 0, NULL)) != NULL) 
    {
        GSList *list = NULL;
        while ((name = g_dir_read_name(dir)) != NULL) 
        {
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
        ret = NIL;

    g_free(dir_name);
    return ret;
}
/* io_write {{{*/
static JSValueRef 
io_write(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    gboolean ret = false;
    FILE *f;
    char *path = NULL, *content = NULL, *mode = NULL;
    if (argc < 3) 
    {
        js_make_exception(ctx, exc, EXCEPTION("io.write needs 3 arguments."));
        return JSValueMakeBoolean(ctx, false);
    }

    if ( (path = js_value_to_char(ctx, argv[0], PATH_MAX, exc)) == NULL )
        goto error_out;

    if ( (mode = js_value_to_char(ctx, argv[1], -1, exc)) == NULL )
        goto error_out;

    if (g_strcmp0(mode, "w") && g_strcmp0(mode, "a")) 
    {
        js_make_exception(ctx, exc, EXCEPTION("io.write: invalid mode."));
        goto error_out;
    }
    if ( (content = js_value_to_char(ctx, argv[2], -1, exc)) == NULL ) 
        goto error_out;

    if ( (f = fopen(path, mode)) != NULL) 
    {
        fputs(content, f);
        fclose(f);
        ret = true;
    }
    else 
        js_make_exception(ctx, exc, EXCEPTION("io.write: cannot open %s for writing."), path);

error_out:
    g_free(path);
    g_free(mode);
    g_free(content);
    return JSValueMakeBoolean(ctx, ret);
}/*}}}*/

/* io_print {{{*/
static JSValueRef 
io_print(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    if (argc == 0) 
        return UNDEFINED;

    FILE *stream = stdout;
    if (argc >= 2) 
    {
        JSStringRef js_string = JSValueToStringCopy(ctx, argv[1], exc);
        if (JSStringIsEqualToUTF8CString(js_string, "stderr")) 
            stream = stderr;
        JSStringRelease(js_string);
    }

    char *out;
    double dout;
    char *json = NULL;
    int type = JSValueGetType(ctx, argv[0]);
    switch (type) 
    {
        case kJSTypeString : 
            out = js_value_to_char(ctx, argv[0], -1, exc);
            if (out != NULL) 
            { 
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
            if (json != NULL) 
            {
                fprintf(stream, "%s\n", json);
                g_free(json);
            }
            break;
        default : break;
    }
    return UNDEFINED;
}/*}}}*/
/*}}}*/

/* DOWNLOAD {{{*/
/* download_constructor_cb {{{*/
static JSObjectRef 
download_constructor_cb(JSContextRef ctx, JSObjectRef constructor, size_t argc, const JSValueRef argv[], JSValueRef* exception) 
{
    if (argc<1) 
        return JSValueToObject(ctx, NIL, NULL);

    char *uri = js_value_to_char(ctx, argv[0], -1, exception);
    if (uri == NULL) 
    {
        js_make_exception(ctx, exception, EXCEPTION("Download constructor: invalid argument"));
        return JSValueToObject(ctx, NIL, NULL);
    }

    WebKitNetworkRequest *request = webkit_network_request_new(uri);
    g_free(uri);
    if (request == NULL) 
    {
        js_make_exception(ctx, exception, EXCEPTION("Download constructor: invalid uri"));
        return JSValueToObject(ctx, NIL, NULL);
    }

    WebKitDownload *download = webkit_download_new(request);
    return JSObjectMake(ctx, s_download_class, download);
}/*}}}*/
static JSObjectRef 
deferred_constructor_cb(JSContextRef ctx, JSObjectRef constructor, size_t argc, const JSValueRef argv[], JSValueRef* exception) 
{
    return deferred_new(ctx);
}

/* stop_download_notify {{{*/
static gboolean 
stop_download_notify(CallbackData *c) 
{
    WebKitDownloadStatus status = webkit_download_get_status(WEBKIT_DOWNLOAD(c->gobject));
    if (status == WEBKIT_DOWNLOAD_STATUS_ERROR || status == WEBKIT_DOWNLOAD_STATUS_CANCELLED || status == WEBKIT_DOWNLOAD_STATUS_FINISHED) 
        return true;

    return false;
}/*}}}*/

/* download_start {{{*/
static JSValueRef 
download_start(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    WebKitDownload *download = JSObjectGetPrivate(this);
    if (download == NULL)
        return JSValueMakeBoolean(ctx, false);

    if (webkit_download_get_destination_uri(download) == NULL) 
    {
        js_make_exception(ctx, exc, EXCEPTION("Download.start: destination == null"));
        return JSValueMakeBoolean(ctx, false);
    }

    if (argc > 0) 
        make_callback(ctx, this, G_OBJECT(download), "notify::status", argv[0], stop_download_notify, exc);

    webkit_download_start(download);
    return JSValueMakeBoolean(ctx, true);

}/*}}}*/

/* download_cancel {{{*/
static JSValueRef 
download_cancel(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    WebKitDownload *download = JSObjectGetPrivate(this);
    webkit_download_cancel(download);
    return UNDEFINED;
}/*}}}*/
/*}}}*/

/* gui {{{*/
static JSValueRef
gui_get_window(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.window), true);
}
static JSValueRef
gui_get_main_box(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.vbox), true);
}
static JSValueRef
gui_get_tab_box(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.topbox), true);
}
static JSValueRef
gui_get_content_box(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.mainbox), true);
}
static JSValueRef
gui_get_status_widget(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.statusbox), true);
}
static JSValueRef
gui_get_status_alignment(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.alignment), true);
}
static JSValueRef
gui_get_status_box(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.status_hbox), true);
}
    static JSValueRef
gui_get_message_label(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.lstatus), true);
}
static JSValueRef
gui_get_entry(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.entry), true);
}
static JSValueRef
gui_get_uri_label(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.urilabel), true);
}
static JSValueRef
gui_get_status_label(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* exception) 
{
    return make_object_for_class(ctx, s_gobject_class, G_OBJECT(dwb.gui.rstatus), true);
}
/*}}}*/

/* SIGNALS {{{*/
/* signal_set {{{*/
static bool
signal_set(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef value, JSValueRef* exception) 
{
    char *name = js_string_to_char(ctx, js_name, -1);
    JSObjectRef o;

    if (name == NULL)
        return false;

    for (int i = SCRIPTS_SIG_FIRST; i<SCRIPTS_SIG_LAST; i++) 
    {
        if (strcmp(name, s_sigmap[i].name)) 
            continue;

        if (JSValueIsNull(ctx, value)) 
        {
            s_sig_objects[i] = NULL;
            dwb.misc.script_signals &= ~(1<<i);
        }
        else if ( (o = js_value_to_function(ctx, value, exception)) != NULL) 
        {
            s_sig_objects[i] = o;
            dwb.misc.script_signals |= (1<<i);
        }
        break;
    }

    g_free(name);
    return false;
}/*}}}*/

/* scripts_emit {{{*/
gboolean
scripts_emit(ScriptSignal *sig) 
{
    JSObjectRef function = s_sig_objects[sig->signal];
    if (function == NULL)
        return false;

    int additional = sig->jsobj != NULL ? 2 : 1;
    int numargs = MIN(sig->numobj, SCRIPT_MAX_SIG_OBJECTS)+additional;
    JSValueRef val[numargs];
    int i = 0;

    if (sig->jsobj != NULL) 
        val[i++] = sig->jsobj;

    for (int j=0; j<sig->numobj; j++) 
    {
        if (sig->objects[j] != NULL) 
            val[i++] = make_object(s_global_context, G_OBJECT(sig->objects[j]));
        else 
            val[i++] = NIL;
    }

    JSValueRef vson = js_json_to_value(s_global_context, sig->json);
    val[i++] = vson == NULL ? NIL : vson;

    JSValueRef js_ret = JSObjectCallAsFunction(s_global_context, function, NULL, numargs, val, NULL);

    if (JSValueIsBoolean(s_global_context, js_ret)) 
        return JSValueToBoolean(s_global_context, js_ret);

    return false;
}/*}}}*/
/*}}}*/

/* OBJECTS {{{*/
/* make_object {{{*/
static void 
object_destroy_cb(JSObjectRef o) 
{
    JSObjectSetPrivate(o, NULL);
    JSValueUnprotect(s_global_context, o);
}

static JSObjectRef 
make_object_for_class(JSContextRef ctx, JSClassRef class, GObject *o, gboolean protect) 
{
    JSObjectRef retobj = g_object_get_qdata(o, s_ref_quark);
    if (retobj != NULL)
        return retobj;

    retobj = JSObjectMake(ctx, class, o);
    if (protect) 
    {
        g_object_set_qdata_full(o, s_ref_quark, retobj, (GDestroyNotify)object_destroy_cb);
        JSValueProtect(s_global_context, retobj);
    }
    else 
        g_object_set_qdata_full(o, s_ref_quark, retobj, NULL);

    return retobj;
}


static JSObjectRef 
make_object(JSContextRef ctx, GObject *o) 
{
    if (o == NULL) 
    {
        JSValueRef v = NIL;
        return JSValueToObject(ctx, v, NULL);
    }
    JSClassRef class;
    if (WEBKIT_IS_WEB_VIEW(o)) 
        class = s_webview_class;
    else if (WEBKIT_IS_WEB_FRAME(o))
        class = s_frame_class;
    else if (WEBKIT_IS_DOWNLOAD(o)) 
        class = s_download_class;
    else if (SOUP_IS_MESSAGE(o)) 
        class = s_message_class;
    else 
        class = s_gobject_class;
    return make_object_for_class(ctx, class, o, true);
}/*}}}*/

static gboolean 
connect_callback(SSignal *sig, ...) 
{
    va_list args;
    JSValueRef cur;
    JSValueRef argv[sig->query->n_params + 1];
    va_start(args, sig);
#define CHECK_NUMBER(GTYPE, TYPE) G_STMT_START if (gtype == G_TYPE_##GTYPE) { \
    TYPE MM_value = va_arg(args, TYPE); \
    cur = JSValueMakeNumber(s_global_context, MM_value); goto apply;} G_STMT_END
    for (guint i=0; i<sig->query->n_params; i++) 
    {
        GType gtype = sig->query->param_types[i], act;
        while ((act = g_type_parent(gtype))) 
            gtype = act;
        CHECK_NUMBER(INT, gint);
        CHECK_NUMBER(UINT, guint);
        CHECK_NUMBER(LONG, glong);
        CHECK_NUMBER(ULONG, gulong);
        CHECK_NUMBER(FLOAT, gdouble);
        CHECK_NUMBER(DOUBLE, gdouble);
        CHECK_NUMBER(ENUM, gint);
        CHECK_NUMBER(INT64, gint64);
        CHECK_NUMBER(UINT64, guint64);
        CHECK_NUMBER(FLAGS, guint);
        if (sig->query->param_types[i] == G_TYPE_BOOLEAN) 
        {
            gboolean value = va_arg(args, gboolean);
            cur = JSValueMakeBoolean(s_global_context, value);
        }
        else if (sig->query->param_types[i] == G_TYPE_STRING) 
        {
            char *value = va_arg(args, char *);
            cur = js_char_to_value(s_global_context, value);
        }
        else if (G_TYPE_IS_CLASSED(gtype)) 
        {
            GObject *value = va_arg(args, gpointer);
            if (value != NULL) // avoid conversion to JSObjectRef
                cur = make_object(s_global_context, value);
            else 
                cur = NIL;
        }
        else 
        {
            cur = UNDEFINED;
        }

apply:
        argv[i+1] = cur;
    }
#undef CHECK_NUMBER
    argv[0] = make_object(s_global_context, va_arg(args, gpointer));
    JSValueRef ret = JSObjectCallAsFunction(s_global_context, sig->func, NULL, sig->query->n_params+1, argv, NULL);
    if (JSValueIsBoolean(s_global_context, ret)) 
    {
        return JSValueToBoolean(s_global_context, ret);
    }
    return false;
}
static void
on_disconnect_object(SSignal *sig, GClosure *closure)
{
    ssignal_free(sig);
}
static void
notify_callback(GObject *o, GParamSpec *param, JSObjectRef func)
{
    JSValueRef argv[] = { make_object(s_global_context, o) };
    JSObjectCallAsFunction(s_global_context, func, NULL, 1, argv, NULL);
}
static JSValueRef 
connect_object(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    GConnectFlags flags = 0;
    gulong id = 0;
    SSignal *sig;
    char *name = NULL;
    guint signal_id;

    if (argc < 2) 
        return JSValueMakeNumber(ctx, 0);

    name = js_value_to_char(ctx, argv[0], PROP_LENGTH, exc);
    if (name == NULL) 
        goto error_out;

    JSObjectRef func = js_value_to_function(ctx, argv[1], exc);
    if (func == NULL) 
        goto error_out;

    GObject *o = JSObjectGetPrivate(this);
    if (o == NULL)
        goto error_out;

    if (argc > 2 && JSValueIsBoolean(ctx, argv[2]) && JSValueToBoolean(ctx, argv[2]))
        flags |= G_CONNECT_AFTER;

    if (strncmp(name, "notify::", 8) == 0) 
    {
        g_signal_connect_data(o, name, G_CALLBACK(notify_callback), func, NULL, flags);
    }
    else
    {
        signal_id = g_signal_lookup(name, G_TYPE_FROM_INSTANCE(o));

        flags |= G_CONNECT_SWAPPED;

        if (signal_id == 0)
            goto error_out;

        sig = ssignal_new();
        if (sig == NULL) 
            goto error_out;

        g_signal_query(signal_id, sig->query);
        if (sig->query->signal_id == 0) 
        {
            ssignal_free(sig);
            goto error_out;
        }

        sig->func = func;
        id = g_signal_connect_data(o, name, G_CALLBACK(connect_callback), sig, (GClosureNotify)on_disconnect_object, flags);
        if (id > 0) 
        {
            sig->id = id;
            sig->object = o;
        }
        else 
            ssignal_free(sig);
    }

error_out: 
    g_free(name);
    return JSValueMakeNumber(ctx, id);
}
static JSValueRef 
disconnect_object(JSContextRef ctx, JSObjectRef function, JSObjectRef this, size_t argc, const JSValueRef argv[], JSValueRef* exc) 
{
    int id;
    if (argc > 0 && JSValueIsNumber(ctx, argv[0]) && (id = JSValueToNumber(ctx, argv[0], exc)) != NAN) 
    {
        GObject *o = JSObjectGetPrivate(this);
        if (o != NULL && g_signal_handler_is_connected(o, id)) 
        {
            g_signal_handler_disconnect(o, id);
            return JSValueMakeBoolean(ctx, true);
        }
    }
    return JSValueMakeBoolean(ctx, false);
}

/* set_property_cb {{{*/
static bool
set_property_cb(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) 
{
    return true;
}/*}}}*/

/* create_class {{{*/
static JSClassRef 
create_class(const char *name, JSStaticFunction staticFunctions[], JSStaticValue staticValues[]) 
{
    JSClassDefinition cd = kJSClassDefinitionEmpty;
    cd.className = name;
    cd.staticFunctions = staticFunctions;
    cd.staticValues = staticValues;
    cd.setProperty = set_property_cb;
    return JSClassCreate(&cd);
}/*}}}*/

/* create_object {{{*/
static JSObjectRef 
create_object(JSContextRef ctx, JSClassRef class, JSObjectRef obj, JSClassAttributes attr, const char *name, void *private) 
{
    JSObjectRef ret = JSObjectMake(ctx, class, private);
    js_set_property(ctx, obj, name, ret, attr, NULL);
    return ret;
}/*}}}*/

/* set_property {{{*/
static bool
set_property(JSContextRef ctx, JSObjectRef object, JSStringRef js_name, JSValueRef jsvalue, JSValueRef* exception) 
{
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
             gtype == G_TYPE_UINT64 || gtype == G_TYPE_FLAGS))  
    {
        double value = JSValueToNumber(ctx, jsvalue, exception);
        if (value != NAN) 
        {
            switch (gtype) 
            {
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
    else if (jstype == kJSTypeBoolean && gtype == G_TYPE_BOOLEAN) 
    {
        bool value = JSValueToBoolean(ctx, jsvalue);
        g_object_set(o, buf, value, NULL);
        return true;
    }
    else if (jstype == kJSTypeString && gtype == G_TYPE_STRING) 
    {
        char *value = js_value_to_char(ctx, jsvalue, -1, exception);
        g_object_set(o, buf, value, NULL);
        g_free(value);
        return true;
    }
    return false;
}/*}}}*/

/* get_property {{{*/
static JSValueRef
get_property(JSContextRef ctx, JSObjectRef jsobj, JSStringRef js_name, JSValueRef *exception) 
{
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
    while ((act = g_type_parent(gtype))) 
        gtype = act;

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
    if (pspec->value_type == G_TYPE_BOOLEAN) 
    {
        gboolean bval;
        g_object_get(o, buf, &bval, NULL);
        ret = JSValueMakeBoolean(ctx, bval);
    }
    else if (pspec->value_type == G_TYPE_STRING) 
    {
        char *value;
        g_object_get(o, buf, &value, NULL);
        ret = js_char_to_value(ctx, value);
        g_free(value);
    }
    else if (G_TYPE_IS_CLASSED(gtype)) 
    {
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

static JSObjectRef
create_constructor(JSContextRef ctx, char *name, JSClassRef class, JSObjectCallAsConstructorCallback cb, JSValueRef *exc)
{
    JSObjectRef constructor = JSObjectMakeConstructor(ctx, class, cb);
    JSStringRef js_name = JSStringCreateWithUTF8CString(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), js_name, constructor, kJSPropertyAttributeDontDelete, exc);
    JSStringRelease(js_name);
    JSValueProtect(ctx, constructor);
    return constructor;

}
/* create_global_object {{{*/
static void 
create_global_object() 
{
    s_ref_quark = g_quark_from_static_string("dwb_js_ref");

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
    s_global_context = JSGlobalContextCreate(class);
    JSClassRelease(class);


    JSObjectRef global_object = JSContextGetGlobalObject(s_global_context);

    JSStaticValue data_values[] = {
        { "profile",        data_get_profile, NULL, kJSDefaultAttributes },
        { "cacheDir",       data_get_cache_dir, NULL, kJSDefaultAttributes },
        { "configDir",      data_get_config_dir, NULL, kJSDefaultAttributes },
        { "systemDataDir",  data_get_system_data_dir, NULL, kJSDefaultAttributes },
        { "userDataDir",    data_get_user_data_dir, NULL, kJSDefaultAttributes },
        { 0, 0, 0,  0 }, 
    };
    class = create_class("data", NULL, data_values);
    create_object(s_global_context, class, global_object, kJSDefaultAttributes, "data", NULL);
    JSClassRelease(class);

    JSStaticFunction io_functions[] = { 
        { "print",     io_print,            kJSDefaultAttributes },
        { "prompt",    io_prompt,           kJSDefaultAttributes },
        { "read",      io_read,             kJSDefaultAttributes },
        { "write",     io_write,            kJSDefaultAttributes },
        { "dirNames",  io_dir_names,        kJSDefaultAttributes },
        { "notify",    io_notify,           kJSDefaultAttributes },
        { "error",     io_error,            kJSDefaultAttributes },
        { 0,           0,           0 },
    };
    class = create_class("io", io_functions, NULL);
    create_object(s_global_context, class, global_object, kJSPropertyAttributeDontDelete, "io", NULL);
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
    create_object(s_global_context, class, global_object, kJSDefaultAttributes, "system", NULL);
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
    create_object(s_global_context, class, global_object, kJSDefaultAttributes, "tabs", NULL);
    JSClassRelease(class);

    JSClassDefinition cd = kJSClassDefinitionEmpty;
    cd.className = "signals";
    cd.setProperty = signal_set;
    class = JSClassCreate(&cd);

    create_object(s_global_context, class, global_object, kJSDefaultAttributes, "signals", NULL);
    JSClassRelease(class);

    class = create_class("extensions", NULL, NULL);
    create_object(s_global_context, class, global_object, kJSDefaultAttributes, "extensions", NULL);
    JSClassRelease(class);

    JSStaticFunction util_functions[] = { 
        { "domainFromHost",   util_domain_from_host,         kJSDefaultAttributes },
        { "markupEscape",     util_markup_escape,         kJSDefaultAttributes },
        { "getMode",          util_get_mode,         kJSDefaultAttributes },
        { 0, 0, 0 }, 
    };
    class = create_class("util", util_functions, NULL);
    create_object(s_global_context, class, global_object, kJSDefaultAttributes, "util", NULL);
    JSClassRelease(class);

    /* Default gobject class */
    cd = kJSClassDefinitionEmpty;
    cd.className = "GObject";
    cd.staticFunctions = default_functions;
    cd.getProperty = get_property;
    cd.setProperty = set_property;
    s_gobject_class = JSClassCreate(&cd);

    s_constructors[CONSTRUCTOR_DEFAULT] = create_constructor(s_global_context, "GObject", s_gobject_class, NULL, NULL);

    /* Webview */
    cd.className = "WebKitWebView";
    cd.staticFunctions = wv_functions;
    cd.staticValues = wv_values;
    cd.parentClass = s_gobject_class;
    s_webview_class = JSClassCreate(&cd);

    s_constructors[CONSTRUCTOR_WEBVIEW] = create_constructor(s_global_context, "WebKitWebView", s_webview_class, NULL, NULL);

    /* Frame */
    cd.className = "WebKitWebFrame";
    cd.staticFunctions = frame_functions;
    cd.staticValues = frame_values;
    cd.parentClass = s_gobject_class;
    s_frame_class = JSClassCreate(&cd);

    s_constructors[CONSTRUCTOR_FRAME] = create_constructor(s_global_context, "WebKitWebFrame", s_frame_class, NULL, NULL);

    /* SoupMessage */ 
    cd.className = "SoupMessage";
    cd.staticFunctions = default_functions;
    cd.staticValues = message_values;
    cd.parentClass = s_gobject_class;
    s_message_class = JSClassCreate(&cd);

    s_constructors[CONSTRUCTOR_SOUP_MESSAGE] = create_constructor(s_global_context, "SoupMessage", s_message_class, NULL, NULL);

    JSStaticFunction deferred_functions[] = { 
        { "then",             deferred_then,         kJSDefaultAttributes },
        { "resolve",          deferred_resolve,         kJSDefaultAttributes },
        { "reject",           deferred_reject,         kJSDefaultAttributes },
        { 0, 0, 0 }, 
    };
    cd = kJSClassDefinitionEmpty;
    cd.className = "Deferred"; 
    cd.staticFunctions = deferred_functions;
    s_deferred_class = JSClassCreate(&cd);
    s_constructors[CONSTRUCTOR_DEFERRED] = create_constructor(s_global_context, "Deferred", s_deferred_class, deferred_constructor_cb, NULL);

    JSStaticValue gui_values[] = {
        { "window",           gui_get_window, NULL, kJSDefaultAttributes }, 
        { "mainBox",          gui_get_main_box, NULL, kJSDefaultAttributes }, 
        { "tabBox",           gui_get_tab_box, NULL, kJSDefaultAttributes }, 
        { "contentBox",       gui_get_content_box, NULL, kJSDefaultAttributes }, 
        { "statusWidget",     gui_get_status_widget, NULL, kJSDefaultAttributes }, 
        { "statusAlignment",  gui_get_status_alignment, NULL, kJSDefaultAttributes }, 
        { "statusBox",        gui_get_status_box, NULL, kJSDefaultAttributes }, 
        { "messageLabel",     gui_get_message_label, NULL, kJSDefaultAttributes }, 
        { "entry",            gui_get_entry, NULL, kJSDefaultAttributes }, 
        { "uriLabel",         gui_get_uri_label, NULL, kJSDefaultAttributes }, 
        { "statusLabel",      gui_get_status_label, NULL, kJSDefaultAttributes }, 
        { 0, 0, 0, 0 }, 
    };
    cd.className = "gui";
    cd = kJSClassDefinitionEmpty;
    cd.staticValues = gui_values;
    class = JSClassCreate(&cd);
    create_object(s_global_context, class, global_object, kJSDefaultAttributes, "gui", NULL);
    JSClassRelease(class);

    /* download */
    cd.className = "Download";
    cd.staticFunctions = download_functions;
    cd.staticValues = NULL;
    cd.parentClass = s_gobject_class;
    s_download_class = JSClassCreate(&cd);

    s_constructors[CONSTRUCTOR_DOWNLOAD] = create_constructor(s_global_context, "Download", s_download_class, download_constructor_cb, NULL);

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
    cd.parentClass = s_gobject_class;
    class = JSClassCreate(&cd);


    JSObjectRef o = make_object_for_class(s_global_context, class, G_OBJECT(scratchpad_get()), true);
    js_set_property(s_global_context, global_object, "scratchpad", o, kJSDefaultAttributes, NULL);
}/*}}}*/
/*}}}*/

/* INIT AND END {{{*/
/* apply_scripts {{{*/
    static void 
apply_scripts() 
{
    int length = g_slist_length(s_script_list); 
    int i=0;

    // XXX Not needed?
    JSValueRef *scripts = g_try_malloc(length * sizeof(JSValueRef));
    JSObjectRef *objects = g_try_malloc(length * sizeof(JSObjectRef));
    for (GSList *l=s_script_list; l; l=l->next, i++) 
    {
        scripts[i] = JSObjectMake(s_global_context, NULL, NULL);
        objects[i] = JSObjectMake(s_global_context, NULL, NULL);
        js_set_property(s_global_context, objects[i], "self", scripts[i], 0, NULL);
        js_set_property(s_global_context, objects[i], "func", l->data, 0, NULL);
    }
    if (s_init_before != NULL) 
    {
        JSValueRef argv[] = {  JSObjectMakeArray(s_global_context, length, (JSValueRef*)objects, NULL) };
        JSObjectCallAsFunction(s_global_context, s_init_before, NULL, 1, argv, NULL);
        JSValueUnprotect(s_global_context, s_init_before);
    }

    i=0;
    for (GSList *l = s_script_list; l; l=l->next, i++) 
    {

        JSObjectRef apply = js_get_object_property(s_global_context, l->data, "apply");
        JSValueRef argv[] = { scripts[i] };
        JSObjectCallAsFunction(s_global_context, apply, l->data, 1, argv, NULL);
    }
    g_slist_free(s_script_list);
    s_script_list = NULL;

    if (s_init_after != NULL) 
    {
        JSObjectCallAsFunction(s_global_context, s_init_after, NULL, 0, NULL, NULL);
        JSValueUnprotect(s_global_context, s_init_after);
    }
    g_free(scripts);
    g_free(objects);
}/*}}}*/

/* scripts_create_tab {{{*/
void 
scripts_create_tab(GList *gl) 
{
    static gboolean applied = false;
    if (s_global_context == NULL )  
    {
        VIEW(gl)->script_wv = NULL;
        return;
    }
    if (!applied) 
    {
        apply_scripts();
        applied = true;
    }
    JSObjectRef o = make_object(s_global_context, G_OBJECT(VIEW(gl)->web));

    JSValueProtect(s_global_context, o);
    VIEW(gl)->script_wv = o;
}/*}}}*/

/* scripts_remove_tab {{{*/
void 
scripts_remove_tab(JSObjectRef obj) 
{
    if (obj != NULL) 
    {
        if (EMIT_SCRIPT(CLOSE_TAB)) 
        {
            ScriptSignal signal = { obj, SCRIPTS_SIG_META(NULL, CLOSE_TAB, 0) };
            scripts_emit(&signal);
        }
        JSValueUnprotect(s_global_context, obj);
    }
}/*}}}*/

/* scripts_init_script {{{*/
void
scripts_init_script(const char *path, const char *script) 
{
    char *debug = NULL;
    if (s_global_context == NULL) 
        create_global_object();

    debug = g_strdup_printf("\ntry{/*<dwb*/%s/*dwb>*/}catch(e){io.debug({message:\"In file %s\",error:e});};", script, path);
    JSObjectRef function = js_make_function(s_global_context, debug);

    if (function != NULL) 
        s_script_list = g_slist_prepend(s_script_list, function);

    g_free(debug);
}/*}}}*/

void
evaluate(const char *script) 
{
    JSStringRef js_script = JSStringCreateWithUTF8CString(script);
    JSEvaluateScript(s_global_context, js_script, NULL, NULL, 0, NULL);
    JSStringRelease(js_script);
}

JSObjectRef 
get_private(JSContextRef ctx, char *name) 
{
    JSStringRef js_name = JSStringCreateWithUTF8CString(name);
    JSObjectRef global_object = JSContextGetGlobalObject(s_global_context);

    JSObjectRef ret = js_get_object_property(s_global_context, global_object, name);
    JSValueProtect(s_global_context, ret);
    JSObjectDeleteProperty(s_global_context, global_object, js_name, NULL);

    JSStringRelease(js_name);
    return ret;
}

/* scripts_init {{{*/
void 
scripts_init(gboolean force) 
{
    dwb.misc.script_signals = 0;
    if (s_global_context == NULL) 
    {
        if (force) 
            create_global_object();
        else 
            return;
    }
    dwb.state.script_completion = NULL;

    char *dir = util_get_data_dir(LIBJS_DIR);
    if (dir != NULL) 
    {
        GString *content = g_string_new(NULL);
        util_get_directory_content(content, dir, "js");
        if (content != NULL)  
        {
            JSStringRef js_script = JSStringCreateWithUTF8CString(content->str);
            JSEvaluateScript(s_global_context, js_script, NULL, NULL, 0, NULL);
            JSStringRelease(js_script);
        }
        g_string_free(content, true);
        g_free(dir);
    }
    UNDEFINED = JSValueMakeUndefined(s_global_context);
    JSValueProtect(s_global_context, UNDEFINED);
    NIL = JSValueMakeNull(s_global_context);
    JSValueProtect(s_global_context, NIL);

    s_init_before = get_private(s_global_context, "_initBefore");
    s_init_after = get_private(s_global_context, "_initAfter");

    JSObjectRef o = JSObjectMakeArray(s_global_context, 0, NULL, NULL);
    s_array_contructor = js_get_object_property(s_global_context, o, "constructor");
    JSValueProtect(s_global_context, s_array_contructor); 
}/*}}}*/

gboolean 
scripts_execute_one(const char *script) 
{
    if (s_global_context != NULL)
        return js_execute(s_global_context, script, NULL) != NULL;

    return false;
}
void
scripts_unbind(JSObjectRef obj) 
{
    if (obj != NULL) 
        JSValueUnprotect(s_global_context, obj);
}

/* scripts_end {{{*/
void
scripts_end() 
{
    if (s_global_context != NULL) 
    {
        for (int i=0; i<CONSTRUCTOR_LAST; i++) 
            JSValueUnprotect(s_global_context, s_constructors[i]);
        JSValueUnprotect(s_global_context, s_array_contructor);
        JSValueUnprotect(s_global_context, UNDEFINED);
        JSValueUnprotect(s_global_context, NIL);
        JSClassRelease(s_gobject_class);
        JSClassRelease(s_webview_class);
        JSClassRelease(s_frame_class);
        JSClassRelease(s_download_class);
        JSClassRelease(s_message_class);
        JSGlobalContextRelease(s_global_context);
        s_global_context = NULL;
    }
}/*}}}*//*}}}*/
