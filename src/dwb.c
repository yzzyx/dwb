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

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <locale.h>
#include <JavaScriptCore/JavaScript.h>
#ifdef HAS_EXECINFO
#include <execinfo.h>
#endif
#include "dwb.h"
#include "soup.h"
#include "completion.h"
#include "commands.h"
#include "view.h"
#include "util.h"
#include "download.h"
#include "session.h"
#include "icon.xpm"
#include "html.h"
#include "plugins.h"
#include "local.h"
#include "js.h"
#include "callback.h"
#include "entry.h"
#include "adblock.h"
#include "domain.h"
#include "application.h"
#include "scripts.h"
#include "scratchpad.h"

/* DECLARATIONS {{{*/
static DwbStatus dwb_webkit_setting(GList *, WebSettings *);
static DwbStatus dwb_webview_property(GList *, WebSettings *);
static DwbStatus dwb_set_background_tab(GList *, WebSettings *);
static DwbStatus dwb_set_scripts(GList *, WebSettings *);
static DwbStatus dwb_set_user_agent(GList *, WebSettings *);
static DwbStatus dwb_set_startpage(GList *, WebSettings *);
static DwbStatus dwb_set_message_delay(GList *, WebSettings *);
static DwbStatus dwb_set_history_length(GList *, WebSettings *);
static DwbStatus dwb_set_plugin_blocker(GList *, WebSettings *);
static DwbStatus dwb_set_sync_interval(GList *, WebSettings *);
static DwbStatus dwb_set_sync_files(GList *, WebSettings *);
static DwbStatus dwb_set_scroll_step(GList *, WebSettings *);
static DwbStatus dwb_set_private_browsing(GList *, WebSettings *);
static DwbStatus dwb_set_new_tab_position_policy(GList *, WebSettings *);
static DwbStatus dwb_set_progress_bar_style(GList *, WebSettings *);
static DwbStatus dwb_set_close_tab_position_policy(GList *, WebSettings *);
static DwbStatus dwb_set_cookies(GList *, WebSettings *);
static DwbStatus dwb_set_widget_packing(GList *, WebSettings *);
static DwbStatus dwb_set_cookie_accept_policy(GList *, WebSettings *);
static DwbStatus dwb_set_favicon(GList *, WebSettings *);
static DwbStatus dwb_set_auto_insert_mode(GList *, WebSettings *);
static DwbStatus dwb_set_tabbar_delay(GList *, WebSettings *);
static DwbStatus dwb_set_close_last_tab_policy(GList *, WebSettings *);
static DwbStatus dwb_set_ntlm(GList *gl, WebSettings *s);
static DwbStatus dwb_set_find_delay(GList *gl, WebSettings *s);
static DwbStatus dwb_set_do_not_track(GList *gl, WebSettings *s);
static DwbStatus dwb_set_show_single_tab(GList *gl, WebSettings *s);
#ifdef WITH_LIBSOUP_2_38
static DwbStatus dwb_set_dns_lookup(GList *gl, WebSettings *s);
#endif
static DwbStatus dwb_init_hints(GList *gl, WebSettings *s);

static Navigation * dwb_get_search_completion_from_navigation(Navigation *);
static gboolean dwb_sync_files(gpointer);
static void dwb_save_key_value(const char *file, const char *key, const char *value);

static gboolean dwb_editable_focus_cb(WebKitDOMElement *element, WebKitDOMEvent *event, GList *gl);

static void dwb_reload_layout(GList *,  WebSettings *);

static void dwb_init_key_map(void);
static void dwb_init_style(void);
static void dwb_apply_style(void);
static void dwb_init_gui(void);

static Navigation * dwb_get_search_completion(const char *text);

static void dwb_clean_vars(void);
typedef struct _EditorInfo {
  char *filename;
  char *id;
  GList *gl;
  WebKitDOMElement *element;
  char *tagname;
} EditorInfo;

typedef struct _UserScriptEnv {
  GIOChannel *channel;
  char *fifo;
  gint fd;
  guint source;
} UserScripEnv;


typedef struct HintMap {
  int type;
  int arg;
} _HintMap;

struct HintMap hint_map[] = {
  { HINT_T_ALL,   HINT_T_ALL }, 
  { HINT_T_LINKS, HINT_T_LINKS }, 
  { HINT_T_IMAGES, HINT_T_IMAGES }, 
  { HINT_T_EDITABLE, HINT_T_EDITABLE }, 
  { HINT_T_URL, HINT_T_URL }, 
  { HINT_T_CLIPBOARD, HINT_T_URL }, 
  { HINT_T_PRIMARY, HINT_T_URL }, 
  { HINT_T_RAPID, HINT_T_URL }, 
  { HINT_T_RAPID_NW, HINT_T_URL }, 
};


static gboolean dwb_user_script_cb(GIOChannel *channel, GIOCondition condition, UserScripEnv *env);


static int signals[] = { SIGFPE, SIGILL, SIGINT, SIGQUIT, SIGTERM, SIGALRM, SIGSEGV};
static int MAX_COMPLETIONS = 11;
/*}}}*/

#include "config.h"

/* SETTINGS_FUNCTIONS{{{*/
/* dwb_set_plugin_blocker {{{*/
static DwbStatus
dwb_set_plugin_blocker(GList *gl, WebSettings *s) {
  View *v = gl->data;
  if (s->arg_local.b) {
    plugins_connect(gl);
    v->plugins->status ^= (v->plugins->status & PLUGIN_STATUS_DISABLED) | PLUGIN_STATUS_ENABLED;
  }
  else {
    plugins_disconnect(gl);
    v->plugins->status ^= (v->plugins->status & PLUGIN_STATUS_ENABLED) | PLUGIN_STATUS_DISABLED;
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_set_adblock {{{*/
void
dwb_set_adblock(GList *gl, WebSettings *s) {
  if (s->arg_local.b) {
    for (GList *l = dwb.state.views; l; l=l->next) 
      adblock_connect(l);
  }
  else {
    for (GList *l = dwb.state.views; l; l=l->next) 
      adblock_disconnect(l);
  }
}/*}}}*/

/* dwb_set_cookies {{{ */
static DwbStatus
dwb_set_cookies(GList *gl, WebSettings *s) {
  dwb.state.cookie_store_policy = dwb_soup_get_cookie_store_policy(s->arg_local.p);
  return STATUS_OK;
}/*}}}*/

/* dwb_set_cookies {{{ */
static DwbStatus
dwb_set_do_not_track(GList *gl, WebSettings *s) {
  dwb.state.do_not_track = s->arg_local.b;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_cookies {{{ */
static DwbStatus
dwb_set_show_single_tab(GList *gl, WebSettings *s) {
  dwb.misc.show_single_tab = s->arg_local.b;
  if (dwb.state.views) {
    if (!dwb.state.views->next) {
      if (!dwb.misc.show_single_tab)
        gtk_widget_hide(dwb.gui.topbox);
      else if (dwb.state.bar_visible & BAR_VIS_TOP)
        gtk_widget_show(dwb.gui.topbox);
    }
  }
  return STATUS_OK;
}/*}}}*/

static DwbStatus 
dwb_set_close_last_tab_policy(GList *gl, WebSettings *s) {
  if (!g_strcmp0("clear", s->arg_local.p)) 
    dwb.misc.clt_policy = CLT_POLICY_CLEAR;
  else if (!g_strcmp0("close", s->arg_local.p)) 
    dwb.misc.clt_policy = CLT_POLICY_CLOSE;
  else if (!g_strcmp0("ignore", s->arg_local.p)) 
    dwb.misc.clt_policy = CLT_POLICY_INGORE;
  else 
    return STATUS_ERROR;
  return STATUS_OK;
}

static DwbStatus 
dwb_set_progress_bar_style(GList *gl, WebSettings *s) {
  if (!g_strcmp0("default", s->arg_local.p)) 
    dwb.misc.progress_bar_style = PROGRESS_BAR_DEFAULT;
  else if (!g_strcmp0("simple", s->arg_local.p)) 
    dwb.misc.progress_bar_style = PROGRESS_BAR_SIMPLE;
  else 
    return STATUS_ERROR;
  return STATUS_OK;
}

static DwbStatus 
dwb_set_ntlm(GList *gl, WebSettings *s) {
  dwb_soup_set_ntlm(s->arg_local.b);
  return STATUS_OK;
}

static DwbStatus 
dwb_set_find_delay(GList *gl, WebSettings *s) {
  dwb.misc.find_delay = s->arg_local.i;
  return STATUS_OK;
}
#ifdef WITH_LIBSOUP_2_38
static DwbStatus 
dwb_set_dns_lookup(GList *gl, WebSettings *s) {
  dwb.misc.dns_lookup = s->arg_local.b;
  return STATUS_OK;
}
#endif

/* dwb_set_cookies {{{  */
static DwbStatus
dwb_set_widget_packing(GList *gl, WebSettings *s) {
  DwbStatus ret = STATUS_OK;
  if (dwb_pack(s->arg_local.p, true) != STATUS_OK) {
    g_free(s->arg_local.p);
    s->arg_local.p = g_strdup(DEFAULT_WIDGET_PACKING);
    ret = STATUS_ERROR;
  }
  return ret;
}/*}}}*/

/* dwb_set_private_browsing  {{{ */
static DwbStatus
dwb_set_private_browsing(GList *gl, WebSettings *s) {
  dwb.misc.private_browsing = s->arg_local.b;
  dwb_webkit_setting(gl, s);
  return STATUS_OK;
}/*}}}*/

static DwbStatus
dwb_tab_position_policy(WebSettings *s, unsigned int shift, unsigned int mask) {
  if (!g_strcmp0(s->arg_local.p, "right"))
    dwb.misc.tab_position = ( TAB_POSITION_RIGHT << shift ) | (dwb.misc.tab_position & mask);
  else if (!g_strcmp0(s->arg_local.p, "left"))
    dwb.misc.tab_position = (TAB_POSITION_LEFT << shift ) | (dwb.misc.tab_position & mask);
  else if (!g_strcmp0(s->arg_local.p, "rightmost"))
    dwb.misc.tab_position = (TAB_POSITION_RIGHTMOST << shift ) | (dwb.misc.tab_position & mask);
  else if (!g_strcmp0(s->arg_local.p, "leftmost"))
    dwb.misc.tab_position = (TAB_POSITION_LEFTMOST << shift ) | (dwb.misc.tab_position & mask);
  else 
    return STATUS_ERROR;
  return STATUS_OK;

}
/* dwb_set_new_tab_position_policy {{{ */
static DwbStatus
dwb_set_new_tab_position_policy(GList *gl, WebSettings *s) {
  return dwb_tab_position_policy(s, 0, CLOSE_TAB_POSITION_MASK);
}/*}}}*/
static DwbStatus
dwb_set_close_tab_position_policy(GList *gl, WebSettings *s) {
  return dwb_tab_position_policy(s, 4, NEW_TAB_POSITION_MASK);
}

/* dwb_set_cookie_accept_policy {{{ */
static DwbStatus
dwb_set_cookie_accept_policy(GList *gl, WebSettings *s) {
  if (dwb_soup_set_cookie_accept_policy(s->arg_local.p) == STATUS_ERROR) {
    s->arg_local.p = g_strdup("always");
    return STATUS_ERROR;
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_set_sync_interval{{{*/
static DwbStatus
dwb_set_sync_interval(GList *gl, WebSettings *s) {
  if (dwb.misc.synctimer > 0) {
    g_source_remove(dwb.misc.synctimer);
    dwb.misc.synctimer = 0;
  }
  dwb.misc.sync_interval = s->arg_local.i;

  if (s->arg_local.i > 0) {
    dwb.misc.synctimer = g_timeout_add_seconds(s->arg_local.i, dwb_sync_files, NULL);
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_set_sync_interval{{{*/
static DwbStatus
dwb_set_sync_files(GList *gl, WebSettings *s) {
  DwbStatus ret = STATUS_OK;
  if (s->arg_local.p == NULL) 
    return STATUS_ERROR;
  int flags = 0;
  char **token = g_strsplit(s->arg_local.p, " ", -1);

  for (int i=0; token[i] != NULL && ret == STATUS_OK; i++) {
    if (!strcmp("all", token[i])) 
      flags = SYNC_ALL;
    else if (!strcmp("history", token[i])) 
      flags |= SYNC_HISTORY;
    else if (!strcmp("cookies", token[i])) 
      flags |= SYNC_COOKIES;
    else if (!strcmp("session", token[i])) 
      flags |= SYNC_SESSION;
    else 
      ret = STATUS_ERROR;
  }
  if (ret != STATUS_ERROR)
    dwb.misc.sync_files = flags;

  g_strfreev(token);
  return ret;
}/*}}}*/

/* dwb_set_scroll_step {{{*/
static DwbStatus
dwb_set_scroll_step(GList *gl, WebSettings *s) {
  dwb.misc.scroll_step = s->arg_local.d;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_startpage(GList *l, WebSettings *){{{*/
static DwbStatus 
dwb_set_startpage(GList *l, WebSettings *s) {
  dwb.misc.startpage = s->arg_local.p;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_message_delay(GList *l, WebSettings *){{{*/
static DwbStatus 
dwb_set_message_delay(GList *l, WebSettings *s) {
  dwb.misc.message_delay = s->arg_local.i;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_history_length(GList *l, WebSettings *){{{*/
static DwbStatus 
dwb_set_history_length(GList *l, WebSettings *s) {
  dwb.misc.history_length = s->arg_local.i;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_background_tab (GList *, WebSettings *s) {{{*/
static DwbStatus 
dwb_set_background_tab(GList *l, WebSettings *s) {
  dwb.state.background_tabs = s->arg_local.b;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_auto_insert_mode {{{*/
static DwbStatus 
dwb_set_auto_insert_mode(GList *l, WebSettings *s) {
  dwb.state.auto_insert_mode = s->arg_local.b;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_tabbar_delay {{{*/
static DwbStatus 
dwb_set_tabbar_delay(GList *l, WebSettings *s) {
  dwb.misc.tabbar_delay = s->arg_local.i;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_favicon(GList *l, WebSettings *s){{{*/
static DwbStatus
dwb_set_favicon(GList *l, WebSettings *s) {
  if (!s->arg_local.b) {
    for (GList *l = dwb.state.views; l; l=l->next) {
      g_signal_handler_disconnect(WEBVIEW(l), VIEW(l)->status->signals[SIG_ICON_LOADED]);
      view_set_favicon(l, false);
    }
  }
  else {
    for (GList *l = dwb.state.views; l; l=l->next) 
      VIEW(l)->status->signals[SIG_ICON_LOADED] = g_signal_connect(VIEW(l)->web, "icon-loaded", G_CALLBACK(view_icon_loaded), l);
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_set_proxy{{{*/
DwbStatus
dwb_set_proxy(GList *l, WebSettings *s) {
  if (s->arg_local.b) {
    SoupURI *uri = soup_uri_new(dwb.misc.proxyuri);
    g_object_set(dwb.misc.soupsession, "proxy-uri", uri, NULL);
    soup_uri_free(uri);
  }
  else  {
    g_object_set(dwb.misc.soupsession, "proxy-uri", NULL, NULL);
  }
  dwb_set_normal_message(dwb.state.fview, true, "Set setting proxy: %s", s->arg_local.b ? "true" : "false");
  return STATUS_OK;
}/*}}}*/

/* dwb_set_scripts {{{*/
static DwbStatus
dwb_set_scripts(GList *gl, WebSettings *s) {
  dwb_webkit_setting(gl, s);
  View *v = VIEW(gl);
  if (s->arg_local.b) 
    v->status->scripts = SCRIPTS_ALLOWED;
  else 
    v->status->scripts = SCRIPTS_BLOCKED;
  return STATUS_OK;
}/*}}}*/

/* dwb_set_user_agent {{{*/
static DwbStatus
dwb_set_user_agent(GList *gl, WebSettings *s) {
  char *ua = s->arg_local.p;
  if (! ua) {
    char *current_ua;
    g_object_get(dwb.state.web_settings, "user-agent", &current_ua, NULL);
    s->arg_local.p = g_strdup_printf("%s %s/%s", current_ua, NAME, VERSION);
  }
  dwb_webkit_setting(gl, s);
  g_hash_table_insert(dwb.settings, g_strdup("user-agent"), s);
  return STATUS_OK;
}/*}}}*/


/* dwb_webkit_setting(GList *gl WebSettings *s) {{{*/
static DwbStatus
dwb_webkit_setting(GList *gl, WebSettings *s) {
  WebKitWebSettings *settings = gl ? webkit_web_view_get_settings(WEBVIEW(gl)) : dwb.state.web_settings;
  switch (s->type) {
    case DOUBLE:  g_object_set(settings, s->n.first, s->arg_local.d, NULL); break;
    case INTEGER: g_object_set(settings, s->n.first, s->arg_local.i, NULL); break;
    case BOOLEAN: g_object_set(settings, s->n.first, s->arg_local.b, NULL); break;
    case CHAR:    g_object_set(settings, s->n.first, !s->arg_local.p || !g_strcmp0(s->arg_local.p, "null") ? NULL : (char*)s->arg_local.p  , NULL); break;
    default: return STATUS_OK;
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_webview_property(GList, WebSettings){{{*/
static DwbStatus
dwb_webview_property(GList *gl, WebSettings *s) {
  WebKitWebView *web = gl ? WEBVIEW(gl) : CURRENT_WEBVIEW();
  switch (s->type) {
    case DOUBLE:  g_object_set(web, s->n.first, s->arg_local.d, NULL); break;
    case INTEGER: g_object_set(web, s->n.first, s->arg_local.i, NULL); break;
    case BOOLEAN: g_object_set(web, s->n.first, s->arg_local.b, NULL); break;
    case CHAR:    g_object_set(web, s->n.first, (char*)s->arg_local.p, NULL); break;
    default: return STATUS_OK;
  }
  return STATUS_OK;
}/*}}}*/

/*}}}*/

/* COMMAND_TEXT {{{*/

/* dwb_set_status_bar_text(GList *gl, const char *text, GdkColor *fg,  PangoFontDescription *fd) {{{*/
void
dwb_set_status_bar_text(GtkWidget *label, const char *text, DwbColor *fg,  PangoFontDescription *fd, gboolean markup) {
  if (markup) {
    gtk_label_set_markup(GTK_LABEL(label), text);
  }
  else {
    gtk_label_set_text(GTK_LABEL(label), text);
  }
  if (fg) {
    DWB_WIDGET_OVERRIDE_COLOR(label, GTK_STATE_NORMAL, fg);
  }
  if (fd) {
    DWB_WIDGET_OVERRIDE_FONT(label, fd);
  }
}/*}}}*/

/* hide command text {{{*/
void 
dwb_source_remove() {
  if ( dwb.state.message_id != 0 ) {
    g_source_remove(dwb.state.message_id);
    dwb.state.message_id = 0;
  }
}
static gpointer 
dwb_hide_message() {
  if (dwb.state.mode & INSERT_MODE) {
    dwb_set_normal_message(dwb.state.fview, false, INSERT_MODE_STRING);
  }
  else {
    if (gtk_widget_get_visible(dwb.gui.bottombox)) 
      CLEAR_COMMAND_TEXT();
    else 
      dwb_dom_remove_from_parent(WEBKIT_DOM_NODE(CURRENT_VIEW()->status_element), NULL);
  }
  return NULL;
}/*}}}*/

/* dwb_set_normal_message {{{*/
void 
dwb_set_normal_message(GList *gl, gboolean hide, const char  *text, ...) {
  if (gl != dwb.state.fview)
    return;
  va_list arg_list; 
  
  va_start(arg_list, text);
  char message[STRING_LENGTH];
  vsnprintf(message, sizeof(message), text, arg_list);
  va_end(arg_list);

  if (gtk_widget_get_visible(dwb.gui.bottombox)) {
    dwb_set_status_bar_text(dwb.gui.lstatus, message, &dwb.color.active_fg, dwb.font.fd_active, false);
  }
  else {
      WebKitDOMDocument *doc = webkit_web_view_get_dom_document(WEBVIEW(gl));
      WebKitDOMElement *docelement = webkit_dom_document_get_document_element(doc);
      webkit_dom_node_append_child(WEBKIT_DOM_NODE(docelement), WEBKIT_DOM_NODE(VIEW(gl)->status_element), NULL);
      webkit_dom_html_element_set_inner_text(WEBKIT_DOM_HTML_ELEMENT(VIEW(gl)->status_element), message, NULL);
  }

  dwb_source_remove();
  if (hide) {
    dwb.state.message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, NULL);
  }
}/*}}}*/

/* dwb_set_error_message {{{*/
void 
dwb_set_error_message(GList *gl, const char *error, ...) {
  if (gl != dwb.state.fview)
    return;
  va_list arg_list; 

  va_start(arg_list, error);
  char message[STRING_LENGTH];
  vsnprintf(message, sizeof(message), error, arg_list);
  va_end(arg_list);

  dwb_source_remove();

  if (gtk_widget_get_visible(dwb.gui.bottombox)) {
    dwb_set_status_bar_text(dwb.gui.lstatus, message, &dwb.color.error, dwb.font.fd_active, false);
  }
  else {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(WEBVIEW(gl));
    WebKitDOMElement *docelement = webkit_dom_document_get_document_element(doc);
    webkit_dom_node_append_child(WEBKIT_DOM_NODE(docelement), WEBKIT_DOM_NODE(VIEW(gl)->status_element), NULL);
    webkit_dom_html_element_set_inner_text(WEBKIT_DOM_HTML_ELEMENT(VIEW(gl)->status_element), message, NULL);
  }
  dwb.state.message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, NULL);
  gtk_widget_hide(dwb.gui.entry);
}/*}}}*/

void 
dwb_update_uri(GList *gl) {
  if (EMIT_SCRIPT(STATUS_BAR))
    return;
  if (gl != dwb.state.fview)
    return;
  View *v = VIEW(gl);

  const char *uri = webkit_web_view_get_uri(CURRENT_WEBVIEW());
  char *decoded = g_uri_unescape_string(uri, "\n\r\f");
  DwbColor *uricolor;
  switch(v->status->ssl) {
    case SSL_TRUSTED:   uricolor = &dwb.color.ssl_trusted; break;
    case SSL_UNTRUSTED: uricolor = &dwb.color.ssl_untrusted; break;
    default:            uricolor = &dwb.color.active_fg; break;
  }
  dwb_set_status_bar_text(dwb.gui.urilabel, decoded ? decoded : uri, uricolor, NULL, false);
  g_free(decoded);
}

/* dwb_update_status_text(GList *gl) {{{*/
void 
dwb_update_status_text(GList *gl, GtkAdjustment *a) {
  g_return_if_fail(gl == dwb.state.fview);
  View *v = gl->data;

  gboolean back = webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(v->web));
  gboolean forward = webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(v->web));

  if (EMIT_SCRIPT(STATUS_BAR)) {
    char *json = util_create_json(5, 
      CHAR, "ssl", v->status->ssl == SSL_TRUSTED 
            ? "trusted" : v->status->ssl == SSL_UNTRUSTED 
            ? "untrusted" : "none",
      BOOLEAN, "canGoBack", back,
      BOOLEAN, "canGoForward", forward, 
      BOOLEAN, "scriptsBlocked", (v->status->scripts & SCRIPTS_BLOCKED) != 0, 
      BOOLEAN, "pluginBlocked", (v->plugins->status & PLUGIN_STATUS_ENABLED) != 0 && 
          (v->plugins->status & PLUGIN_STATUS_HAS_PLUGIN) != 0);
    ScriptSignal signal = { SCRIPTS_WV(gl), SCRIPTS_SIG_META(json, STATUS_BAR, 0) };
    SCRIPTS_EMIT(signal, json);
  }
    
  if (!a) {
    a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  }
  dwb_update_uri(gl);
  GString *string = g_string_new(NULL);

  const char *bof = back && forward ? " [+-]" : back ? " [+]" : forward  ? " [-]" : " ";
  g_string_append(string, bof);

  g_string_append_printf(string, "[%d/%d]", g_list_position(dwb.state.views, dwb.state.fview) + 1, g_list_length(dwb.state.views));

  if (a) {
    double lower = gtk_adjustment_get_lower(a);
    double upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
    double value = gtk_adjustment_get_value(a); 
    char position[7];
    if (upper == lower) 
      strcpy(position, "[all]");
    else if (value == lower)
      strcpy(position, "[top]");
    else if (value == upper) 
      strcpy(position, "[bot]");
    else 
      snprintf(position, sizeof(position), "[%02d%%]", (int)(value * 100/upper + 0.5));
    g_string_append(string, position);
  }
  if (v->status->scripts & SCRIPTS_BLOCKED) {
    const char *format = v->status->scripts & SCRIPTS_ALLOWED_TEMPORARY 
      ? "[<span foreground='%s'>S</span>]"
      : "[<span foreground='%s'><s>S</s></span>]";
    g_string_append_printf(string, format,  v->status->scripts & SCRIPTS_ALLOWED_TEMPORARY ? dwb.color.allow_color : dwb.color.block_color);
  }
  if ((v->plugins->status & PLUGIN_STATUS_ENABLED) &&  (v->plugins->status & PLUGIN_STATUS_HAS_PLUGIN)) {
    if (v->plugins->status & PLUGIN_STATUS_DISCONNECTED) 
      g_string_append_printf(string, "[<span foreground='%s'>P</span>]",  dwb.color.allow_color);
    else 
      g_string_append_printf(string, "[<span foreground='%s'><s>P</s></span>]",  dwb.color.block_color);
  }
  if (LP_STATUS(v)) {
    g_string_append_printf(string, "[<span foreground='%s'>", dwb.color.tab_protected_color);
    if (LP_LOCKED_DOMAIN(v)) 
      g_string_append_c(string, 'd');
    if (LP_LOCKED_URI(v)) 
      g_string_append_c(string, 'u');
    g_string_append(string, "</span>]");
  }
  if (webkit_web_view_get_load_status(WEBVIEW(gl)) == WEBKIT_LOAD_FINISHED) {
    const char *uri = webkit_web_view_get_uri(WEBVIEW(gl));
    gboolean has_quickmark = g_list_find_custom(dwb.fc.quickmarks, uri, (GCompareFunc)util_quickmark_compare_uri) != NULL;
    gboolean has_bookmark = g_list_find_custom(dwb.fc.bookmarks, uri, (GCompareFunc)util_navigation_compare_uri) != NULL;
    if (has_quickmark || has_bookmark) {
      g_string_append_c(string, '[');
      if (has_quickmark) 
        g_string_append_c(string, 'Q');
      if (has_bookmark) 
        g_string_append_c(string, 'B');
      g_string_append_c(string, ']');
    }
  }
  if (v->status->progress != 0) {
    wchar_t *bar_blocks = PROGRESS_DEFAULT;
    wchar_t buffer[PBAR_LENGTH + 1] = { 0 };
    wchar_t cbuffer[PBAR_LENGTH] = { 0 };
    int length = PBAR_LENGTH * v->status->progress / 100;
    if (dwb.misc.progress_bar_style == PROGRESS_BAR_SIMPLE)
        bar_blocks = PROGRESS_SIMPLE;
    wmemset(buffer, bar_blocks[1], length - 1);
    if (dwb.misc.progress_bar_style == PROGRESS_BAR_DEFAULT)
        buffer[length] = bar_blocks[3] - (int)((v->status->progress % 10) / 10.0*8);
    wmemset(cbuffer, bar_blocks[2], PBAR_LENGTH-length-1);
    cbuffer[PBAR_LENGTH - length] = '\0';
    g_string_append_printf(string, "%lc<span foreground='%s'>%ls</span><span foreground='%s'>%ls</span>%lc", bar_blocks[0], dwb.color.progress_full, buffer, dwb.color.progress_empty, cbuffer, bar_blocks[3]);
  }
  if (string->len > 0) 
    dwb_set_status_bar_text(dwb.gui.rstatus, string->str, NULL, NULL, true);
  g_string_free(string, true);
}/*}}}*/

/*}}}*/

/* FUNCTIONS {{{*/

/* dwb_get_raw_data(GList *){{{*/
char *
dwb_get_raw_data(GList *gl) {
  char *ret = NULL;
  WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBVIEW(gl));
  WebKitWebDataSource *data_source = webkit_web_frame_get_data_source(frame);
  GString *data = webkit_web_data_source_get_data(data_source);
  if (data != NULL) {
    ret = data->str;
  }
  return ret;
}/*}}}*/

DwbStatus/*{{{*/
dwb_scheme_handler(GList *gl, WebKitNetworkRequest *request) {
  const char *handler = GET_CHAR("scheme-handler");
  DwbStatus ret = STATUS_OK;
  if (handler == NULL) {
    dwb_set_error_message(gl, "No scheme handler defined");
    return STATUS_ERROR;
  }
  GError *error = NULL;
  char **scheme_handler = g_strsplit(handler, " ", -1);
  int l = g_strv_length(scheme_handler);
  char **argv = g_malloc0_n(l + 2, sizeof(char*));
  GSList *list = NULL;
  const char *uri = webkit_network_request_get_uri(request);
  int i=0;
  for (; i<l; i++) {
    argv[i] = scheme_handler[i];
  }
  argv[i++] = (char*)uri;
  argv[i++] = NULL;
  list = g_slist_append(list, dwb_navigation_new("DWB_URI", uri));
  list = g_slist_prepend(list, dwb_navigation_new("DWB_COOKIES", dwb.files[FILES_COOKIES]));

  char *scheme = g_uri_parse_scheme(uri);
  if (scheme) {
    list = g_slist_append(list, dwb_navigation_new("DWB_SCHEME", scheme));
    g_free(scheme);
  }
  const char *referer = soup_get_header_from_request(request, "Referer");
  if (referer)
    list = g_slist_append(list, dwb_navigation_new("DWB_REFERER", uri));
  const char *user_agent = soup_get_header_from_request(request, "User-Agent");
  if (user_agent)
    list = g_slist_append(list, dwb_navigation_new("DWB_USER_AGENT", uri));
  if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, (GSpawnChildSetupFunc)dwb_setup_environment, list, NULL, &error)) {
    dwb_set_error_message(gl, "Spawning scheme handler failed");
    fprintf(stderr, "Scheme handler failed: %s", error->message);
    g_clear_error(&error);
    ret = STATUS_ERROR;
  }
  for (int i=0; i<l; i++) 
    g_free(argv[i]);
  g_free(scheme_handler);
  g_free(argv);
  return ret;
}/*}}}*/

/* dwb_glist_prepend_unique(GList **list, char *text) {{{*/
void
dwb_glist_prepend_unique(GList **list, char *text) {
  for (GList *l = (*list); l; l=l->next) {
    if (!g_strcmp0(text, l->data)) {
      g_free(l->data);
      (*list) = g_list_delete_link((*list), l);
      break;
    }
  }
  (*list) = g_list_prepend((*list), text);
}/*}}}*/

/* dwb_set_open_mode(Open) {{{*/
void
dwb_set_open_mode(Open mode) {
  if (mode & OPEN_NEW_VIEW && !dwb.misc.tabbed_browsing) 
      dwb.state.nv = (mode & ~OPEN_NEW_VIEW) | OPEN_NEW_WINDOW;
  else 
    dwb.state.nv = mode;
}/*}}}*/

/* dwb_clipboard_get_text(GdkAtom)GdkAtom {{{*/
char *
dwb_clipboard_get_text(GdkAtom atom) {
  GtkClipboard *clipboard = gtk_clipboard_get(atom);

  if (clipboard != NULL)
    return gtk_clipboard_wait_for_text(clipboard);
  return NULL;
}/*}}}*/

/* dwb_set_clipboard (const char *, GdkAtom) {{{*/
DwbStatus
dwb_set_clipboard(const char *text, GdkAtom atom) {
  GtkClipboard *clipboard = gtk_clipboard_get(atom);
  gboolean ret = STATUS_ERROR;
  if (text == NULL) 
    return STATUS_ERROR;

  gtk_clipboard_set_text(clipboard, text, -1);
  if (*text) {
    dwb_set_normal_message(dwb.state.fview, true, "Yanked: %s", text);
    ret = STATUS_OK;
  }
  return ret;
}/*}}}*/

void
dwb_paste_primary() {
  GtkClipboard *p_clip = gtk_widget_get_clipboard(CURRENT_WEBVIEW_WIDGET(), GDK_SELECTION_PRIMARY);
  if (p_clip == NULL)
    return;
  char *p_text = gtk_clipboard_wait_for_text(p_clip);
  if (p_text != NULL) {
    js_call_as_function(FOCUSED_FRAME(), CURRENT_VIEW()->js_base, "pastePrimary", p_text, kJSTypeString, NULL);
  }
  g_free(p_text);
}

/* dwb_scroll (Glist *gl, double step, ScrollDirection dir) {{{*/
void 
dwb_scroll(GList *gl, double step, ScrollDirection dir) {
  double scroll;
  GtkAllocation alloc;
  View *v = gl->data;

  GtkAdjustment *a = dir == SCROLL_LEFT || dir == SCROLL_RIGHT 
    ? gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(v->scroll)) 
    : gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  int sign = dir == SCROLL_UP || dir == SCROLL_PAGE_UP || dir == SCROLL_HALF_PAGE_UP || dir == SCROLL_LEFT ? -1 : 1;

  double value = gtk_adjustment_get_value(a);

  double inc;
  if (dir == SCROLL_PAGE_UP || dir == SCROLL_PAGE_DOWN) {
    inc = gtk_adjustment_get_page_increment(a);
    if (inc == 0) {
      gtk_widget_get_allocation(GTK_WIDGET(CURRENT_WEBVIEW()), &alloc);
      inc = alloc.height;
    }
  }
  else if (dir == SCROLL_HALF_PAGE_UP || dir == SCROLL_HALF_PAGE_DOWN) {
    inc = gtk_adjustment_get_page_increment(a) / 2;
    if (inc == 0) {
      gtk_widget_get_allocation(GTK_WIDGET(CURRENT_WEBVIEW()), &alloc);
      inc = alloc.height / 2;
    }
  }
  else
    inc = step > 0 ? step : gtk_adjustment_get_step_increment(a);

  /* if gtk_get_step_increment fails and dwb.misc.scroll_step is 0 use a default
   * value */
  if (inc == 0) {
    inc = 40;
  }

  double lower  = gtk_adjustment_get_lower(a);
  double upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;

  switch (dir) {
    case  SCROLL_TOP:      scroll = dwb.state.nummod < 0 ? lower : (upper * dwb.state.nummod)/100;
                           break;
    case  SCROLL_BOTTOM:   scroll = dwb.state.nummod < 0 ? upper : (upper * dwb.state.nummod)/100;
                           break;
    default:               scroll = value + sign * inc * NUMMOD; break;
  }

  scroll = scroll < lower ? lower : scroll > upper ? upper : scroll;
  if (scroll == value) {

    /* Scroll also if  frame-flattening is enabled 
     * this is just a workaround since scrolling is disfunctional if 
     * enable-frame-flattening is set */
    if (value == 0 && dir != SCROLL_TOP) {
      int x, y;
      if (dir == SCROLL_LEFT || dir == SCROLL_RIGHT) {
        x = sign * inc;
        y = 0;
      }
      else {
        x = 0; 
        y = sign * inc;
      }
      char *command = g_strdup_printf("window.scrollBy(%d, %d)", x, y);
      dwb_execute_script(FOCUSED_FRAME(), command, false);
      g_free(command);
    }
  }
  else {
    gtk_adjustment_set_value(a, scroll);
  }
}/*}}}*/

/* dwb_editor_watch (GChildWatchFunc) {{{*/
static void
dwb_editor_watch(GPid pid, int status, EditorInfo *info) {
  gsize length;
  char *content = util_get_file_content(info->filename, &length);
  WebKitDOMElement *e = NULL;
  WebKitWebView *wv;
  if (content == NULL) 
    goto clean;
  if (!info->gl || !g_list_find(dwb.state.views, info->gl->data)) {
    if (info->id == NULL) 
      goto clean;
    else 
      wv = CURRENT_WEBVIEW();
  }
  else 
    wv = WEBVIEW(info->gl);
  if (info->id != NULL) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    e = webkit_dom_document_get_element_by_id(doc, info->id);
    if (e == NULL && (e = info->element) == NULL ) {
      goto clean;
    }
  }
  else 
    e = info->element;

  /*  g_file_get_contents adds an additional newline */
  if (length > 0 && content[length-1] == '\n')
    content[length-1] = 0;
  if (!g_strcmp0(info->tagname, "INPUT")) 
    webkit_dom_html_input_element_set_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), content);
  else if (!g_strcmp0(info->tagname, "TEXTAREA")) 
    webkit_dom_html_text_area_element_set_value(WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(e), content);

clean:
  unlink(info->filename);
  g_free(info->filename);
  g_free(info->id);
  g_free(info);
}/*}}}*/

/* dwb_dom_remove_from_parent(WebKitDOMNode *node, GError **error) {{{*/
gboolean 
dwb_dom_remove_from_parent(WebKitDOMNode *node, GError **error) {
  WebKitDOMNode *parent = webkit_dom_node_get_parent_node(node);
  if (parent != NULL) {
    webkit_dom_node_remove_child(parent, node, error);
    return true;
  }
  return false;
}/*}}}*/


gboolean
dwb_dom_add_frame_listener(WebKitWebFrame *frame, const char *signal, GCallback callback, gboolean bubble, GList *gl) {
  WebKitWebView *wv = webkit_web_frame_get_web_view(frame);
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  const char *src = webkit_web_frame_get_uri(frame);
  char *framesrc;
  gboolean ret = false;
  WebKitDOMDOMWindow *win;
  if (g_strcmp0(src, "about:blank")) {
    /* We have to find the correct frame, but there is no access from the web_frame
     * to the Htmlelement */
    WebKitDOMNodeList *frames = webkit_dom_document_query_selector_all(doc, "iframe, frame", NULL);
    for (guint i=0; i<webkit_dom_node_list_get_length(frames) && ret == false; i++) {
      WebKitDOMNode *node = webkit_dom_node_list_item(frames, i);
      char *tagname = webkit_dom_node_get_node_name(node);
      if (!g_ascii_strcasecmp(tagname, "iframe")) {
        framesrc = webkit_dom_html_iframe_element_get_src(WEBKIT_DOM_HTML_IFRAME_ELEMENT(node));
        win = webkit_dom_html_iframe_element_get_content_window(WEBKIT_DOM_HTML_IFRAME_ELEMENT(node));
      }
      else {
        framesrc = webkit_dom_html_frame_element_get_src(WEBKIT_DOM_HTML_FRAME_ELEMENT(node));
        win = webkit_dom_html_frame_element_get_content_window(WEBKIT_DOM_HTML_FRAME_ELEMENT(node));
      }
      if (!g_strcmp0(src, framesrc)) {
        ret = webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), signal, callback, true, gl);
      }
      g_free(framesrc);
    }
    g_object_unref(frames);
  }
  return ret;
}

/* dwb_get_editable(WebKitDOMElement *) {{{*/
static gboolean
dwb_get_editable(WebKitDOMElement *element) {
  if (element == NULL)
    return false;

  char *tagname = webkit_dom_node_get_node_name(WEBKIT_DOM_NODE(element));
  if (!g_strcmp0(tagname, "INPUT")) {
    char *type = webkit_dom_element_get_attribute((void*)element, "type");
    if (!g_strcmp0(type, "text") || !g_strcmp0(type, "search")|| !g_strcmp0(type, "password")) 
      return true;
  }
  else if (!g_strcmp0(tagname, "TEXTAREA")) {
    return true;
  }
  return false;
}/*}}}*/

/* dwb_get_active_input(WebKitDOMDocument )  {{{*/
static WebKitDOMElement *
dwb_get_active_element(WebKitDOMDocument *doc) {
  WebKitDOMElement *ret = NULL;
  WebKitDOMDocument *d = NULL;
  WebKitDOMElement *active = webkit_dom_html_document_get_active_element((void*)doc);
  char *tagname = webkit_dom_element_get_tag_name(active);
  if (! g_strcmp0(tagname, "FRAME")) {
    d = webkit_dom_html_frame_element_get_content_document(WEBKIT_DOM_HTML_FRAME_ELEMENT(active));
    ret = dwb_get_active_element(d);
  }
  else if (! g_strcmp0(tagname, "IFRAME")) {
    d = webkit_dom_html_iframe_element_get_content_document(WEBKIT_DOM_HTML_IFRAME_ELEMENT(active));
    ret = dwb_get_active_element(d);
  }
  else {
    ret = active;
  }
  return ret;
}/*}}}*/

/* dwb_open_in_editor(void) ret: gboolean success {{{*/
DwbStatus
dwb_open_in_editor(void) {
  DwbStatus ret = STATUS_OK;
  char *editor = GET_CHAR("editor");
  char **commands = NULL;
  char *commandstring = NULL;
  char *value = NULL;
  GPid pid;

  if (editor == NULL) 
    return STATUS_ERROR;
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(CURRENT_WEBVIEW());
  WebKitDOMElement *active = dwb_get_active_element(doc);
  
  if (active == NULL) 
    return STATUS_ERROR;

  if (!dwb_get_editable(active)) 
    return STATUS_ERROR;

  char *tagname = webkit_dom_element_get_tag_name(active);
  if (tagname == NULL) {
    ret = STATUS_ERROR;
    goto clean;
  }
  if (! g_strcmp0(tagname, "INPUT")) 
    value = webkit_dom_html_input_element_get_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(active));
  else if (! g_strcmp0(tagname, "TEXTAREA"))
    value = webkit_dom_html_text_area_element_get_value(WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(active));
  if (value == NULL) {
    ret = STATUS_ERROR;
    goto clean;
  }

  char *path = util_get_temp_filename("edit");
  
  commandstring = util_string_replace(editor, "dwb_uri", path);
  if (commandstring == NULL)  {
    ret = STATUS_ERROR;
    goto clean;
  }

  g_file_set_contents(path, value, -1, NULL);
  commands = g_strsplit(commandstring, " ", -1);
  g_free(commandstring);
  gboolean success = g_spawn_async(NULL, commands, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL);
  g_strfreev(commands);
  if (!success) {
    ret = STATUS_ERROR;
    goto clean;
  }

  EditorInfo *info = dwb_malloc(sizeof(EditorInfo));
  char *id = webkit_dom_html_element_get_id(WEBKIT_DOM_HTML_ELEMENT(active));
  if (id != NULL && strlen(id) > 0) {
    info->id = id;
  }
  else  {
    info->id = NULL;
  }
  info->tagname = tagname;
  info->element = active;
  info->filename = path;
  info->gl = dwb.state.fview;
  g_child_watch_add(pid, (GChildWatchFunc)dwb_editor_watch, info);

clean:
  g_free(value);

  return ret;
}/*}}}*/

/* Auto insert mode {{{*/
static gboolean
dwb_auto_insert(WebKitDOMElement *element) {
  if (dwb_get_editable(element)) {
    dwb_change_mode(INSERT_MODE);
    return true;
  }
  return false;
}

static gboolean
dwb_editable_focus_cb(WebKitDOMElement *element, WebKitDOMEvent *event, GList *gl) {
  webkit_dom_event_target_remove_event_listener(WEBKIT_DOM_EVENT_TARGET(element), "focus", G_CALLBACK(dwb_editable_focus_cb), true);
  if (gl != dwb.state.fview) 
    return false;
  if (!(dwb.state.mode & INSERT_MODE)) {
    WebKitDOMEventTarget *target = webkit_dom_event_get_target(event);
    dwb_auto_insert((void*)target);
  }
  return false;
}
void
dwb_check_auto_insert(GList *gl) {
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(WEBVIEW(gl));
  WebKitDOMElement *active = dwb_get_active_element(doc);
  if (!dwb_auto_insert(active)) {
    WebKitDOMHTMLElement *element = webkit_dom_document_get_body(doc);
    if (element == NULL) 
      element = WEBKIT_DOM_HTML_ELEMENT(webkit_dom_document_get_document_element(doc));
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(element), "focus", G_CALLBACK(dwb_editable_focus_cb), true, gl);
  }
}/*}}}*/

/* remove history, bookmark, quickmark {{{*/
static int
dwb_remove_navigation_item(GList **content, const char *line, const char *filename) {
  Navigation *n = dwb_navigation_new_from_line(line);
  GList *item = g_list_find_custom(*content, n, (GCompareFunc)util_navigation_compare_first);
  dwb_navigation_free(n);
  if (item) {
    if (filename != NULL) 
      util_file_remove_line(filename, line);
    *content = g_list_delete_link(*content, item);
    return 1;
  }
  return 0;
}
void
dwb_remove_bookmark(const char *line) {
  dwb_remove_navigation_item(&dwb.fc.bookmarks, line, dwb.files[FILES_BOOKMARKS]);
}
void
dwb_remove_download(const char *line) {
  dwb_remove_navigation_item(&dwb.fc.downloads, line, dwb.files[FILES_BOOKMARKS]);
}
void
dwb_remove_history(const char *line) {
  dwb_remove_navigation_item(&dwb.fc.history, line, dwb.misc.synctimer <= 0 ? dwb.files[FILES_HISTORY] : NULL);
}
void
dwb_remove_search_engine(const char *line) {
  Navigation *n = dwb_navigation_new_from_line(line);
  GList *item = g_list_find_custom(dwb.fc.searchengines, n, (GCompareFunc)util_navigation_compare_first);

  if (item != NULL) {
    if (item == dwb.fc.searchengines) 
      dwb.misc.default_search = dwb.fc.searchengines->next != NULL ? NAVIGATION(dwb.fc.searchengines->next)->second : NULL;
    util_file_remove_line(dwb.files[FILES_SEARCHENGINES], line);
    dwb_navigation_free(item->data);
    dwb.fc.searchengines = g_list_delete_link(dwb.fc.searchengines, item);
  }
  item = g_list_find_custom(dwb.fc.se_completion, n, (GCompareFunc)util_navigation_compare_first);
  if (item != NULL)  {
    dwb_navigation_free(item->data);
    dwb.fc.se_completion = g_list_delete_link(dwb.fc.se_completion, item);
  }
  dwb_navigation_free(n);
}
void 
dwb_remove_quickmark(const char *line) {
  Quickmark *q = dwb_quickmark_new_from_line(line);
  GList *item = g_list_find_custom(dwb.fc.quickmarks, q, (GCompareFunc)util_quickmark_compare);
  dwb_quickmark_free(q);
  if (item) {
    util_file_remove_line(dwb.files[FILES_QUICKMARKS], line);
    dwb.fc.quickmarks = g_list_delete_link(dwb.fc.quickmarks, item);
  }
}/*}}}*/

/* dwb_sync_history {{{*/
static gboolean
dwb_sync_files(gpointer data) {
  if (dwb.misc.sync_files & SYNC_HISTORY) {
    GString *buffer = g_string_new(NULL);
    int i=0;
    for (GList *gl = dwb.fc.history; gl && (dwb.misc.history_length < 0 || i < dwb.misc.history_length); gl=gl->next, i++) {
      Navigation *n = gl->data;
      g_string_append_printf(buffer, "%s %s\n", n->first, n->second);
    }
    g_file_set_contents(dwb.files[FILES_HISTORY], buffer->str, -1, NULL);
    g_string_free(buffer, true);
  }
  if (dwb.misc.sync_files & SYNC_COOKIES) {
    dwb_soup_sync_cookies();
  }
  if ((dwb.misc.sync_files & SYNC_SESSION) && GET_BOOL("save-session")) {
    session_save(NULL, SESSION_SYNC | SESSION_FORCE);
  }
  return true;
}/*}}}*/

/* dwb_follow_selection() {{{*/
void 
dwb_follow_selection() {
  char *href = NULL;
  WebKitDOMNode *n = NULL, *tmp;
  WebKitDOMRange *range = NULL;
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(CURRENT_WEBVIEW());
  WebKitDOMDOMWindow *window = webkit_dom_document_get_default_view(doc);
  WebKitDOMDOMSelection *selection = webkit_dom_dom_window_get_selection(window);
  if (selection == NULL)  
    return;
  range = webkit_dom_dom_selection_get_range_at(selection, 0, NULL);
  if (range == NULL) 
    return;

  WebKitDOMNode *document_element = WEBKIT_DOM_NODE(webkit_dom_document_get_document_element(doc));
  n = webkit_dom_range_get_start_container(range, NULL); 
  while( n && n != document_element && href == NULL) {
    if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(n)) {
      href = webkit_dom_html_anchor_element_get_href(WEBKIT_DOM_HTML_ANCHOR_ELEMENT(n));
      dwb_load_uri(dwb.state.fview, href);
    }
    tmp = n;
    n = webkit_dom_node_get_parent_node(tmp);
  }
}/*}}}*/

/* dwb_open_startpage(GList *) {{{*/
DwbStatus
dwb_open_startpage(GList *gl) {
  if (dwb.misc.startpage == NULL) 
    return STATUS_ERROR;
  if (gl == NULL) 
    gl = dwb.state.fview;

  dwb_load_uri(gl, dwb.misc.startpage);
  return STATUS_OK;
}/*}}}*/

/* dwb_apply_settings(WebSettings *s) {{{*/
static DwbStatus
dwb_apply_settings(WebSettings *s, const char *key, const char *value, int scope) {
  DwbStatus ret = STATUS_OK;
  if (s->apply & SETTING_GLOBAL) {
    if (s->func) 
      ret = s->func(NULL, s);
  }
  else {
    for (GList *l = dwb.state.views; l; l=l->next) {
      if (s->func) 
        s->func(l, s);
    }
  }
  if (ret != STATUS_ERROR) {
    if (s->type == BOOLEAN) 
      value = s->arg_local.b ? "true" : "false";
    if (scope == SET_GLOBAL) {
      s->arg = s->arg_local;
      dwb_set_normal_message(dwb.state.fview, true, "Saved setting %s: %s", s->n.first, value);
      dwb_save_key_value(dwb.files[FILES_SETTINGS], key, value);
    }
    else {
      dwb_set_normal_message(dwb.state.fview, true, "Changed %s: %s", s->n.first, s->type == BOOLEAN ? ( s->arg_local.b ? "true" : "false") : value);
    }
  }
  else {
    dwb_set_error_message(dwb.state.fview, "Error setting value.");
  }
  return ret;
}/*}}}*/

/* dwb_toggle_setting {{{*/
DwbStatus
dwb_toggle_setting(const char *key, int scope) {
  WebSettings *s;
  DwbStatus ret = STATUS_ERROR;
  Arg oldarg;

  if (key == NULL) 
    return STATUS_ERROR;
  s = g_hash_table_lookup(dwb.settings, key);
  if (s == NULL) {
    dwb_set_error_message(dwb.state.fview, "No such setting: %s", key);
    return STATUS_ERROR;
  }
  if (s->type != BOOLEAN) {
    dwb_set_error_message(dwb.state.fview, "Not a boolean value.");
    return STATUS_ERROR;
  }
  oldarg = s->arg_local;
  s->arg_local.b = !s->arg_local.b;
  ret = dwb_apply_settings(s, key, NULL, scope);
  if (ret == STATUS_ERROR) {
    s->arg = oldarg;
  }
  return ret;
}/*}}}*//*}}}*/

/* dwb_set_setting(const char *){{{*/
DwbStatus
dwb_set_setting(const char *key, char *value, int scope) {
  WebSettings *s;
  Arg *a = NULL, oldarg = { .p = NULL };

  DwbStatus ret = STATUS_ERROR;

  if (key == NULL)
    return ret;

  s = g_hash_table_lookup(dwb.settings, key);
  if (s == NULL) {
    dwb_set_error_message(dwb.state.fview, "No such setting: %s", key);
    return STATUS_ERROR;
  }
  a = util_char_to_arg(value, s->type);
  if (a == NULL) {
    dwb_set_error_message(dwb.state.fview, "No valid value.");
    return STATUS_ERROR;
  }
  oldarg = s->arg;
  s->arg_local = *a;
  ret = dwb_apply_settings(s, key, value, scope);
  if (ret == STATUS_ERROR) {
    g_free(a->p);
    g_free(a);
    s->arg = oldarg;
  }
  return ret;
}/*}}}*/

/* dwb_set_key(const char *prop, char *val) {{{*/
DwbStatus
dwb_set_key(const char *prop, char *val) {
  KeyValue value;
  if (prop == NULL || val == NULL)
    return STATUS_ERROR;

  value.id = g_strdup(prop);
  if (val)
    value.key = dwb_str_to_key(val); 
  else {
    Key key = { NULL, 0, 0 };
    value.key = key;
  }

  dwb_set_normal_message(dwb.state.fview, true, "Saved key for command %s: %s", prop, val ? val : "");

  dwb.keymap = dwb_keymap_add(dwb.keymap, value);
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)util_keymap_sort_second);
  dwb_save_key_value(dwb.files[FILES_KEYS], prop, val);
  return STATUS_OK;
}/*}}}*/

/* dwb_get_host(WebKitWebView *) {{{*/
char *
dwb_get_host(WebKitWebView *web) {
  char *host = NULL;
  SoupURI *uri = soup_uri_new(webkit_web_view_get_uri(web));
  if (uri) {
    host = g_strdup(uri->host);
    soup_uri_free(uri);
  }
  return host;
}/*}}}*/

/* dwb_hide_tabbar {{{*/
static gboolean
dwb_hide_tabbar(int *running) {
  if (! (dwb.state.bar_visible & BAR_VIS_TOP)) {
    gtk_widget_hide(dwb.gui.topbox);
  }
  *running = 0;
  return false;
}/*}}}*/

#if 0
void 
dwb_hide_tab(GList *gl) {
  puts("hide tab");
}
void 
dwb_show_tab(GList *gl) {
  puts("show tab");
}
#endif

/* dwb_focus_view(GList *gl){{{*/
gboolean
dwb_focus_view(GList *gl) {
  static int running;
  if (gl != dwb.state.fview) {
    if (EMIT_SCRIPT(TAB_FOCUS)) {
      //ScriptSignal signal = { SCRIPTS_WV(gl), .objects = { SCRIPTS_WV(dwb.state.fview) }, SCRIPTS_SIG_META(NULL, TAB_FOCUS, 1) };
    ScriptSignal signal = {
      SCRIPTS_WV(gl), .objects = { G_OBJECT(VIEW(dwb.state.fview)->web)  }, SCRIPTS_SIG_META(NULL, TAB_FOCUS, 1) };
      SCRIPTS_EMIT_RETURN(signal, NULL, true);
    }
    gtk_widget_show(VIEW(gl)->scroll);
    dwb_soup_clean();
    if (! (CURRENT_VIEW()->status->lockprotect & LP_VISIBLE) )
      gtk_widget_hide(VIEW(dwb.state.fview)->scroll);
    dwb_change_mode(NORMAL_MODE, true);
    dwb_unfocus();
    dwb_focus(gl);
    if (! (dwb.state.bar_visible & BAR_VIS_TOP) && dwb.misc.tabbar_delay > 0) {
      gtk_widget_show(dwb.gui.topbox);
      if (running != 0) 
        g_source_remove(running);
      running = g_timeout_add(dwb.misc.tabbar_delay * 1000, (GSourceFunc)dwb_hide_tabbar, &running);
    }
    return false;
  }
  return true;
}/*}}}*/

/* dwb_toggle_allowed(const char *filename, const char *data) {{{*/
gboolean 
dwb_toggle_allowed(const char *filename, const char *data, GList **pers) {
  if (!data)
    return false;
  char *content = util_get_file_content(filename, NULL);
  char **lines = NULL;
  gboolean allowed = false;
  if (content != NULL) {
    lines = util_get_lines(filename);
    if (lines != NULL)  {
      for (int i=0; lines[i] != NULL; i++) {
        if (!g_strcmp0(lines[i], data)) {
          allowed = true;
          break;
        }
      } 
    }
  }
  GString *buffer = g_string_new(NULL);
  if (!allowed) {
    if (content) {
      g_string_append(buffer, content);
    }
    g_string_append_printf(buffer, "%s\n", data);
    *pers = g_list_prepend(*pers, g_strdup(data));
    g_strfreev(lines);
  }
  else if (content) {
    if (pers != NULL) {
      dwb_free_list(*pers, (void_func)g_free);
      *pers = NULL;
      if (lines != NULL) {
        for (int i=0; lines[i] != NULL; i++) {
          if (strlen(lines[i]) && g_strcmp0(lines[i], data)) {
            g_string_append_printf(buffer, "%s\n", lines[i]);
            *pers = g_list_prepend(*pers, lines[i]);
          }
        }
      }
    }
  }
  g_file_set_contents(filename, buffer->str, -1, NULL);

  g_free(content);
  g_string_free(buffer, true);

  return !allowed;
}/*}}}*/

/* dwb_reload(GList *){{{*/
void
dwb_reload(GList *gl) {
  const char *path = webkit_web_view_get_uri(WEBVIEW(gl));
  if ( !local_check_directory(dwb.state.fview, path, false, NULL) ) {
    webkit_web_view_reload(WEBVIEW(gl));
  }
}/*}}}*/

/* dwb_history{{{*/
DwbStatus
dwb_history(Arg *a) {
  WebKitWebView *w = CURRENT_WEBVIEW();

  if ( (a->i == -1 && !webkit_web_view_can_go_back(w)) || (a->i == 1 && !webkit_web_view_can_go_forward(w))) 
    return STATUS_ERROR;

  WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(w);

  if (bf_list == NULL) 
    return STATUS_ERROR;

  int n = a->i == -1 ? MIN(webkit_web_back_forward_list_get_back_length(bf_list), NUMMOD) : MIN(webkit_web_back_forward_list_get_forward_length(bf_list), NUMMOD);
  WebKitWebHistoryItem *item = webkit_web_back_forward_list_get_nth_item(bf_list, a->i * n);
  if (a->n == OPEN_NORMAL) {
    webkit_web_view_go_to_back_forward_item(w, item);
  }
  else {
    const char *uri = webkit_web_history_item_get_uri(item);
    if (a->n == OPEN_NEW_VIEW) {
      view_add(uri, dwb.state.background_tabs);
    }
    if (a->n == OPEN_NEW_WINDOW) {
      dwb_new_window(uri);
    }
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_history_back {{{*/
DwbStatus
dwb_history_back() {
  Arg a = { .n = OPEN_NORMAL, .i = -1 };
  return dwb_history(&a);
}/*}}}*/

/* dwb_history_forward{{{*/
DwbStatus
dwb_history_forward() {
  Arg a = { .n = OPEN_NORMAL, .i = 1 };
  return dwb_history(&a);
}/*}}}*/

/* dwb_eval_completion_type {{{*/
CompletionType 
dwb_eval_completion_type(void) {
  switch (CLEAN_MODE(dwb.state.mode)) {
    case SETTINGS_MODE_LOCAL:        
    case SETTINGS_MODE:         return COMP_SETTINGS;
    case KEY_MODE:              return COMP_KEY;
    case COMMAND_MODE:          return COMP_COMMAND;
    case COMPLETE_BUFFER:       return COMP_BUFFER;
    case QUICK_MARK_OPEN:       return COMP_QUICKMARK;
    default:                    return COMP_NONE;
  }
}/*}}}*/

/* dwb_clean_load_begin {{{*/
void 
dwb_clean_load_begin(GList *gl) {
  View *v = gl->data;
  v->status->ssl = SSL_NONE;
  v->plugins->status &= ~PLUGIN_STATUS_HAS_PLUGIN; 
  if (gl == dwb.state.fview && (dwb.state.mode == INSERT_MODE || dwb.state.mode == FIND_MODE)) {  
    dwb_change_mode(NORMAL_MODE, true);
  }
  view_set_favicon(gl, false);
}/*}}}*/

/* dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *)   return: (alloc) Navigation* {{{*/
Navigation *
dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *item) {
  Navigation *n = NULL;
  const char *uri;
  const char *title;

  if (item) {
    uri = webkit_web_history_item_get_uri(item);
    title = webkit_web_history_item_get_title(item);
    n = dwb_navigation_new(uri, title);
  }
  return n;
}/*}}}*/

/* dwb_focus(GList *gl) {{{*/
void 
dwb_unfocus() {
  if (dwb.state.fview) {
    dwb.state.last_tab = g_list_position(dwb.state.views, dwb.state.fview);
    view_set_normal_style(dwb.state.fview);
    dwb_source_remove();
    CLEAR_COMMAND_TEXT();
    dwb_dom_remove_from_parent(WEBKIT_DOM_NODE(CURRENT_VIEW()->status_element), NULL);
    dwb.state.fview = NULL;
  }
} /*}}}*/

/* dwb_focus_scroll (GList *){{{*/
void
dwb_focus_scroll(GList *gl) {
  if (gl == NULL)
    return;

  View *v = gl->data;
  if (! (dwb.state.bar_visible & BAR_VIS_STATUS))
    gtk_widget_hide(dwb.gui.bottombox);
  gtk_widget_set_can_focus(v->web, true);
  gtk_widget_grab_focus(v->web);
  gtk_widget_hide(dwb.gui.entry);
}/*}}}*/

/* dwb_handle_mail(const char *uri)        return: true if it is a mail-address{{{*/
gboolean 
dwb_spawn(GList *gl, const char *prop, const char *uri) {
  const char *program;
  char *command;
  if ( (program = GET_CHAR(prop)) && (command = util_string_replace(program, "dwb_uri", uri)) ) {
    g_spawn_command_line_async(command, NULL);
    g_free(command);
    return true;
  }
  else {
    dwb_set_error_message(dwb.state.fview, "Cannot open %s", uri);
    return false;
  }
}/*}}}*/

/* dwb_update_tabs(){{{*/
void
dwb_update_tabs(void) {
  for (GList *l = dwb.state.views; l; l=l->next) {
    if (l == dwb.state.fview) {
      view_set_active_style(l);
    }
    else {
      view_set_normal_style(l);
    }
  }
}/*}}}*/

/* dwb_reload_layout(GList *,  WebSettings  *s) {{{*/
static void 
dwb_reload_layout(GList *gl, WebSettings *s) {
  dwb_init_style();
  dwb_update_tabs();
  dwb_apply_style();
}/*}}}*/

/* dwb_get_search_engine_uri(const char *uri) {{{*/
static char *
dwb_get_search_engine_uri(const char *uri, const char *text) {
  char *ret = NULL;
  if (uri != NULL && text != NULL) {
    char *hint_search_submit = GET_CHAR("searchengine-submit-pattern");
    if (hint_search_submit == NULL) {
        hint_search_submit = HINT_SEARCH_SUBMIT;
    }
    GRegex *regex = g_regex_new(hint_search_submit, 0, 0, NULL);
    char *escaped = g_uri_escape_string(text, NULL, true);
    ret = g_regex_replace(regex, uri, -1, 0, escaped, 0, NULL);
    g_free(escaped);
    g_regex_unref(regex);
  }
  return ret;
}/* }}} */

/* dwb_get_search_engine_uri {{{*/
char *
dwb_get_searchengine(const char *uri) {
  char *ret = NULL;
  char **token = g_strsplit(uri, " ", 2);
  for (GList *l = dwb.fc.searchengines; l; l=l->next) {
    Navigation *n = l->data;
    if (!g_strcmp0(token[0], n->first)) {
      ret = dwb_get_search_engine_uri(n->second, token[1]);
      break;
    }
  }
  if (!ret) {
    ret = dwb_get_search_engine_uri(dwb.misc.default_search, uri);
  }
  g_strfreev(token);
  return ret;
}/*}}}*/

static gboolean
dwb_check_localhost(const char *uri) {
  if ((!strncmp("localhost", uri, 9) || !strncmp("127.0.0.1", uri, 9)) && (uri[9] == ':' || uri[9] == '\0')) 
    return true;
  return false;
}
/* dwb_get_search_engine(const char *uri) {{{*/
char *
dwb_check_searchengine(const char *uri, gboolean force) {
  char *ret = NULL;
  if (dwb_check_localhost(uri))
    return NULL;
  if ( force || !strchr(uri, '.') ) {
    ret = dwb_get_searchengine(uri);
  }
  return ret;
}/*}}}*/

/* dwb_submit_searchengine {{{*/
void 
dwb_submit_searchengine(void) {
  char buffer[64];
  char *value;
  char *hint_search_submit = GET_CHAR("searchengine-submit-pattern");
  if (hint_search_submit == NULL) {
      hint_search_submit = HINT_SEARCH_SUBMIT;
  }
  snprintf(buffer, sizeof(buffer), "{ \"searchString\" : \"%s\" }", hint_search_submit);
  if ( (value = js_call_as_function(MAIN_FRAME(), CURRENT_VIEW()->js_base, "submitSearchEngine", buffer, kJSTypeObject, &value)) ) {
    dwb.state.form_name = value;
  }
}/*}}}*/

/* dwb_save_searchengine {{{*/
void
dwb_save_searchengine(char *search_engine) {
  char *text = g_strdup(GET_TEXT());
  dwb_change_mode(NORMAL_MODE, false);
  char *uri = NULL;
  gboolean confirmed = true;

  if (!text)
    return;

  g_strstrip(text);
  if (text && strlen(text) > 0) {
    Navigation compn = { .first = text };
    GList *existing = g_list_find_custom(dwb.fc.searchengines, &compn, (GCompareFunc)util_navigation_compare_first);
    if (existing != NULL) {
      uri = util_domain_from_uri(((Navigation*)existing->data)->second);
      if (uri) {
        confirmed = dwb_confirm(dwb.state.fview, "Overwrite searchengine %s : %s [y/n]?", ((Navigation*)existing->data)->first, uri);
        g_free(uri);
      }
      if (!confirmed) {
        dwb_set_error_message(dwb.state.fview, "Aborted");
        return;
      }
    }
    dwb_append_navigation_with_argument(&dwb.fc.searchengines, text, search_engine);
    Navigation *n = g_list_last(dwb.fc.searchengines)->data;
    Navigation *cn = dwb_get_search_completion_from_navigation(dwb_navigation_dup(n));

    dwb.fc.se_completion = g_list_append(dwb.fc.se_completion, cn);
    util_file_add_navigation(dwb.files[FILES_SEARCHENGINES], n, true, -1);

    dwb_set_normal_message(dwb.state.fview, true, "Searchengine saved");
    if (search_engine) {
      if (!dwb.misc.default_search) {
        dwb.misc.default_search = search_engine;
      }
      else  {
        g_free(search_engine);
      }
    }
  }
  else {
    dwb_set_error_message(dwb.state.fview, "No keyword specified, aborting.");
  }
  g_free(text);
}/*}}}*/

gboolean
dwb_hints_unlock() {
  dwb.state.scriptlock = 0;
  return false;
}

/* dwb_evaluate_hints(const char *buffer)  return DwbStatus {{{*/
DwbStatus 
dwb_evaluate_hints(const char *buffer) {
  DwbStatus ret = STATUS_OK;
  if (!g_strcmp0(buffer, "undefined")) 
    return ret;
  else if (!g_strcmp0("_dwb_no_hints_", buffer)) {
    ret = STATUS_ERROR;
  }
  else if (!g_strcmp0(buffer, "_dwb_input_")) {
    dwb_change_mode(INSERT_MODE);
    ret = STATUS_END;
  }
  else if  (!g_strcmp0(buffer, "_dwb_click_") && HINT_NOT_RAPID ) {
    int timeout = GET_INT("hints-key-lock");
    if (timeout > 0) {
      dwb.state.scriptlock = 1;
      g_timeout_add(timeout, dwb_hints_unlock, NULL);
    }
    if ( !(dwb.state.nv & OPEN_DOWNLOAD) ) {
      dwb_change_mode(NORMAL_MODE, dwb.state.message_id == 0);
      ret = STATUS_END;
    }
  }
  else  if (!g_strcmp0(buffer, "_dwb_check_")) {
    dwb_change_mode(NORMAL_MODE, true);
    ret = STATUS_END;
  }
  else  {
    dwb.state.mode = NORMAL_MODE;
    Arg *a = NULL;
    ret = STATUS_END;
    switch (dwb.state.hint_type) {
      case HINT_T_ALL:     break;
      case HINT_T_IMAGES : dwb_load_uri(NULL, buffer); 
                           dwb_change_mode(NORMAL_MODE, true);
                           break;
      case HINT_T_URL    : a = util_arg_new();
                           a->n = dwb.state.nv | SET_URL;
                           a->p = (char *)buffer;
                           commands_open(NULL, a);
                           break;
      case HINT_T_CLIPBOARD : dwb_change_mode(NORMAL_MODE, true);
                              ret = dwb_set_clipboard(buffer, GDK_NONE);
                              break;
      case HINT_T_PRIMARY   : dwb_change_mode(NORMAL_MODE, true);
                              ret = dwb_set_clipboard(buffer, GDK_SELECTION_PRIMARY);
                              break;
      case HINT_T_RAPID     : a = util_arg_new();
                              view_add((char *) buffer, true);
                              a->n = OPEN_NORMAL;
                              a->i = HINT_T_RAPID;
                              dwb_show_hints(a);
                              break;
      case HINT_T_RAPID_NW     : a = util_arg_new();
                                 dwb_new_window((char*)buffer);
                              a->n = OPEN_NORMAL;;
                              a->i = HINT_T_RAPID_NW;
                              dwb_show_hints(a);
                              break;
      default : return ret;
    }
    g_free(a);
  }
  return ret;
}/*}}}*/

/* update_hints {{{*/
gboolean
dwb_update_hints(GdkEventKey *e) {
  char *buffer;
  char *com = NULL;
  char *val;
  gboolean ret = false;
  char json[BUFFER_LENGTH] = {0};

  if (e->keyval == GDK_KEY_Return) {
    com = "followActive";
    snprintf(json, sizeof(json), "{ \"type\" : \"%d\" }", hint_map[dwb.state.hint_type].arg);
  }
  else if (DWB_COMPLETE_KEY(e)) {
    if ((DWB_TAB_KEY(e) && e->state & GDK_SHIFT_MASK) || e->keyval == GDK_KEY_Up) {
      com = "focusPrev";
    }
    else {
      com = "focusNext";
    }
    ret = true;
  }
  else if (e->is_modifier) {
    return false;
  }
  else {
    val = util_keyval_to_char(e->keyval, true);
    snprintf(json, sizeof(json), "{ \"input\" : \"%s%s\", \"type\" : %d }", GET_TEXT(), val ? val : "", hint_map[dwb.state.hint_type].arg);
    com = "updateHints";
    g_free(val);
  }
  if (com) {
    buffer = js_call_as_function(MAIN_FRAME(), CURRENT_VIEW()->js_base, com, *json ? json : NULL, *json ? kJSTypeObject : kJSTypeUndefined, &buffer);
  }
  if (buffer != NULL) { 
    if (dwb_evaluate_hints(buffer) == STATUS_END) 
      ret = true;
    g_free(buffer);
  }
  return ret;
}/*}}}*/

/* dwb_show_hints(Arg *) {{{*/
DwbStatus 
dwb_show_hints(Arg *arg) {
  DwbStatus ret = STATUS_OK;
  if (dwb.state.nv == OPEN_NORMAL) {
    dwb_set_open_mode(arg->n | OPEN_VIA_HINTS);
  }
  if (dwb.state.mode != HINT_MODE) {
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
    char json[64];
    snprintf(json, sizeof(json), "{ \"newTab\" : \"%d\", \"type\" : \"%d\" }",
        (dwb.state.nv & (OPEN_NEW_WINDOW|OPEN_NEW_VIEW)), 
        hint_map[arg->i].arg);
    char *jsret; 
    js_call_as_function(MAIN_FRAME(), CURRENT_VIEW()->js_base, "showHints", json, kJSTypeObject, &jsret);
    if (jsret != NULL) {
      ret = dwb_evaluate_hints(jsret);
      g_free(jsret);
      if (ret == STATUS_END) {
        return ret;
      }
    }
    dwb.state.mode = HINT_MODE;
    dwb.state.hint_type = arg->i;
    entry_focus();
  }
  return ret;
}/*}}}*/

/* dwb_execute_script {{{*/
char *
dwb_execute_script(WebKitWebFrame *frame, const char *com, gboolean ret) {
  JSValueRef eval_ret;

  JSContextRef context = webkit_web_frame_get_global_context(frame);
  g_return_val_if_fail(context != NULL, NULL);

  JSObjectRef global_object = JSContextGetGlobalObject(context);
  g_return_val_if_fail(global_object != NULL, NULL);

  JSStringRef text = JSStringCreateWithUTF8CString(com);
  JSValueRef exc = NULL;
  eval_ret = JSEvaluateScript(context, text, global_object, NULL, 0, &exc);
  JSStringRelease(text);
  if (exc != NULL)
    return NULL;

  if (eval_ret && ret) {
    return js_value_to_char(context, eval_ret, JS_STRING_MAX, NULL);
  }
  return NULL;
}
/*}}}*/

/*prepend_navigation_with_argument(GList **fc, const char *first, const char *second) {{{*/
void
dwb_prepend_navigation_with_argument(GList **fc, const char *first, const char *second) {
  for (GList *l = (*fc); l; l=l->next) {
    Navigation *n = l->data;
    if (!g_strcmp0(first, n->first)) {
      dwb_navigation_free(n);
      (*fc) = g_list_delete_link((*fc), l);
      break;
    }
  }
  Navigation *n = dwb_navigation_new(first, second);

  (*fc) = g_list_prepend((*fc), n);
}/*}}}*/

/*append_navigation_with_argument(GList **fc, const char *first, const char *second) {{{*/
void
dwb_append_navigation_with_argument(GList **fc, const char *first, const char *second) {
  for (GList *l = (*fc); l; l=l->next) {
    Navigation *n = l->data;
    if (!g_strcmp0(first, n->first)) {
      dwb_navigation_free(n);
      (*fc) = g_list_delete_link((*fc), l);
      break;
    }
  }
  Navigation *n = dwb_navigation_new(first, second);

  (*fc) = g_list_append((*fc), n);
}/*}}}*/

/* dwb_prepend_navigation(GList *gl, GList *view) {{{*/
DwbStatus 
dwb_prepend_navigation(GList *gl, GList **fc) {
  WebKitWebView *w = WEBVIEW(gl);
  const char *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri) > 0) {
    const char *title = webkit_web_view_get_title(w);
    dwb_prepend_navigation_with_argument(fc, uri, title);
    return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* dwb_confirm_snooper {{{*/
static gboolean
dwb_confirm_snooper_cb(GtkWidget *w, GdkEventKey *e, int *state) {
  /*  only handle keypress */
  if (e->type == GDK_KEY_RELEASE) 
    return false;
  switch (e->keyval) {
    case GDK_KEY_y:       *state = 1; break;
    case GDK_KEY_n:       *state = 0; break;
    case GDK_KEY_Escape:  break;
    default:              return true;
  }
  dwb.state.mode &= ~CONFIRM;
  return true;
}/*}}}*/

/* dwb_prompt_snooper_cb {{{*/
static gboolean
dwb_prompt_snooper_cb(GtkWidget *w, GdkEventKey *e, int *state) {
  gboolean ret = false;
  if (e->type == GDK_KEY_RELEASE) 
    return false;
  switch (e->keyval) {
    case GDK_KEY_Return:       *state = 0; ret = true; break;
    case GDK_KEY_Escape:  *state = -1; ret = true; break;
    default:              return false;
  }
  dwb.state.mode &= ~CONFIRM;
  return ret;
}/*}}}*/

/* dwb_confirm()  return confirmed (gboolean) {{{
 * yes / no confirmation
 * */
gboolean
dwb_confirm(GList *gl, char *prompt, ...) {
  dwb.state.mode |= CONFIRM;

  va_list arg_list; 

  va_start(arg_list, prompt);
  char message[STRING_LENGTH];
  vsnprintf(message, sizeof(message), prompt, arg_list);
  va_end(arg_list);
  dwb_source_remove();
  dwb_set_status_bar_text(dwb.gui.lstatus, message, &dwb.color.prompt, dwb.font.fd_active, false);
  if (! (dwb.state.bar_visible & BAR_VIS_STATUS) ) 
    gtk_widget_show(dwb.gui.bottombox);


  int state = -1;
  int id = gtk_key_snooper_install((GtkKeySnoopFunc)dwb_confirm_snooper_cb, &state);
  while ((dwb.state.mode & CONFIRM) && state == -1) {
    gtk_main_iteration();
  }
  if (! (dwb.state.bar_visible & BAR_VIS_STATUS) ) 
    gtk_widget_hide(dwb.gui.bottombox);
  gtk_key_snooper_remove(id);
  return state > 0;
}/*}}}*/

/* dwb_prompt {{{*/
const char *
dwb_prompt(gboolean visibility, char *prompt, ...) {
  dwb.state.mode |= CONFIRM;

  dwb_source_remove();
  va_list arg_list; 
  va_start(arg_list, prompt);
  char message[STRING_LENGTH];
  vsnprintf(message, sizeof(message), prompt, arg_list);
  va_end(arg_list);
  dwb_set_status_bar_text(dwb.gui.lstatus, message, &dwb.color.active_fg, dwb.font.fd_active, false);
  if (! (dwb.state.bar_visible & BAR_VIS_STATUS) ) 
    gtk_widget_show(dwb.gui.bottombox);
  gtk_entry_set_visibility(GTK_ENTRY(dwb.gui.entry), visibility);
  int state = -1;
  entry_focus();
  int id = gtk_key_snooper_install((GtkKeySnoopFunc)dwb_prompt_snooper_cb, &state);
  while ((dwb.state.mode & CONFIRM) && state == -1) {
    gtk_main_iteration();
  }
  if (! (dwb.state.bar_visible & BAR_VIS_STATUS) ) 
    gtk_widget_hide(dwb.gui.bottombox);
  gtk_key_snooper_remove(id);
  dwb_focus_scroll(dwb.state.fview);
  CLEAR_COMMAND_TEXT();
  
  gtk_entry_set_visibility(GTK_ENTRY(dwb.gui.entry), true);
  return state == 0 ? GET_TEXT() : NULL;
}/*}}}*/

/* dwb_save_quickmark(const char *key) {{{*/
void 
dwb_save_quickmark(const char *key) {
  dwb_focus_scroll(dwb.state.fview);
  WebKitWebView *w = WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web);
  const char *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri)) {
    const char *title = webkit_web_view_get_title(w);
    for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
      Quickmark *q = l->data;
      if (!g_strcmp0(key, q->key)) {
        if (g_strcmp0(uri, q->nav->first)) {
          if (!dwb_confirm(dwb.state.fview, "Overwrite quickmark %s : %s [y/n]?", q->key, q->nav->first)) {
            dwb_set_error_message(dwb.state.fview, "Aborted saving quickmark %s : %s", key, uri);
            dwb_change_mode(NORMAL_MODE, false);
            return;
          }
        }
        dwb_quickmark_free(q);
        dwb.fc.quickmarks = g_list_delete_link(dwb.fc.quickmarks, l);
        break;
      }
    }
    dwb.fc.quickmarks = g_list_prepend(dwb.fc.quickmarks, dwb_quickmark_new(uri, title, key));
    char *text = g_strdup_printf("%s %s %s", key, uri, title);
    util_file_add(dwb.files[FILES_QUICKMARKS], text, true, -1);
    g_free(text);

    dwb_set_normal_message(dwb.state.fview, true, "Added quickmark: %s - %s", key, uri);
  }
  else {
    dwb_set_error_message(dwb.state.fview, NO_URL);
  }
  dwb_change_mode(NORMAL_MODE, false);
}/*}}}*/

/* dwb_open_quickmark(const char *key){{{*/
void 
dwb_open_quickmark(const char *key) {
  gboolean found = false;
  for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
    Quickmark *q = l->data;
    if (!g_strcmp0(key, q->key)) {
      dwb_set_normal_message(dwb.state.fview, true, "Loading quickmark %s: %s", key, q->nav->first);
      dwb_load_uri(NULL, q->nav->first);
      found = true;
      break;
    }
  }
  if (!found) {
    dwb_set_error_message(dwb.state.fview, "No such quickmark: %s", key);
  }
  dwb_change_mode(NORMAL_MODE, false);
}/*}}}*/

/* dwb_update_find_quickmark (const char *) {{{*/
gboolean
dwb_update_find_quickmark(const char *text) {
  int found = 0;
  const Quickmark *lastfound = NULL;
  for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
    Quickmark *q = l->data;
    if (g_str_has_prefix(q->key, text)) {
      lastfound = q;
      found++;
    }
  }
  if (found == 0) {
    dwb_set_error_message(dwb.state.fview, "No such quickmark: %s", text);
    dwb_change_mode(NORMAL_MODE, true);
  }
  if (lastfound != NULL && found == 1 && !g_strcmp0(text, lastfound->key)) {
    dwb_set_normal_message(dwb.state.fview, true, "Loading quickmark %s: %s", lastfound->key, lastfound->nav->first);
    dwb_load_uri(NULL, lastfound->nav->first);
    dwb_change_mode(NORMAL_MODE, false);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_tab_label_set_text {{{*/
void
dwb_tab_label_set_text(GList *gl, const char *text) {
  View *v = gl->data;
  const char *title = NULL;
  if (text == NULL) 
    title = webkit_web_view_get_title(WEBVIEW(gl));
  else 
    title = text;
  char progress[11] = { 0 };
  if (v->status->progress != 0) {
    snprintf(progress, sizeof(progress), "[%2d%%] ", v->status->progress);
  }
  char *escaped = g_markup_printf_escaped("<span foreground='%s'>%d%s</span> %s%s", 
      LP_PROTECTED(v) ? dwb.color.tab_protected_color : dwb.color.tab_number_color,
      g_list_position(dwb.state.views, gl) + 1, 
      LP_VISIBLE(v) ? "*" : "",
      progress,
      title ? title : "---");
  gtk_label_set_markup(GTK_LABEL(v->tablabel), escaped);

  g_free(escaped);
}/*}}}*/

/* dwb_update_status(GList *gl) {{{*/
void 
dwb_update_status(GList *gl, const char *title) {
  View *v = gl->data;
  char *filename = NULL;
  WebKitWebView *w = WEBKIT_WEB_VIEW(v->web);
  if (title == NULL)
    title = webkit_web_view_get_title(w);
  if (!title) 
    title = "---";

  if (gl == dwb.state.fview) {
    if (v->status->progress != 0) {
      char *text = g_strdup_printf("[%d%%] %s", v->status->progress, title);
      gtk_window_set_title(GTK_WINDOW(dwb.gui.window), text);
      g_free(text);
    }
    else {
      gtk_window_set_title(GTK_WINDOW(dwb.gui.window), title);
    }
    dwb_update_status_text(gl, NULL);
  }
  dwb_tab_label_set_text(gl, title);

  g_free(filename);
}/*}}}*/

/* dwb_update_layout(GList *gl) {{{*/
void 
dwb_update_layout() {
  for (GList *gl = dwb.state.views; gl; gl = gl->next) {
    View *v = gl->data;
    const char *title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
    dwb_tab_label_set_text(gl, title);
  }
  dwb_update_tabs();
}/*}}}*/

/* dwb_focus(GList *gl) {{{*/
void 
dwb_focus(GList *gl) {
  if (dwb.gui.entry) {
    gtk_widget_hide(dwb.gui.entry);
  }
  dwb.state.fview = gl;
  view_set_active_style(gl);
  dwb_focus_scroll(gl);
  dwb_update_status(gl, NULL);
}/*}}}*/

/* dwb_new_window(const char *arg) {{{*/
void 
dwb_new_window(const char  *uri) {
  char *argv[7];

  argv[0] = (char *)dwb.misc.prog_path;
  argv[1] = "-p"; 
  argv[2] = (char *)dwb.misc.profile;
  argv[3] = "-n";
  argv[4] = "-R";
  argv[5] = (char *)uri;
  argv[6] = NULL;
  g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}/*}}}*/

/* dwb_test_userscript (const char *)         return: char* (alloc) or NULL {{{*/
static char * 
dwb_test_userscript(const char *filename) {
  char *path = g_build_filename(dwb.files[FILES_USERSCRIPTS], filename, NULL); 

  if (g_file_test(path, G_FILE_TEST_IS_REGULAR) || 
      (g_str_has_prefix(filename, dwb.files[FILES_USERSCRIPTS]) && g_file_test(filename, G_FILE_TEST_IS_REGULAR) && (path = g_strdup(filename))) ) {
    return path;
  }
  else {
    g_free(path);
  }
  return NULL;
}/*}}}*/

/* dwb_load_uri(const char *uri) {{{*/
void 
dwb_load_uri(GList *gl, const char *arg) {
  /* TODO parse scheme */
  if (arg == NULL)
    return;
  const char *tmpuri;
  char *script;
  char *uri = NULL; 
  char *argback = g_strdup(arg);
  char *backuri = argback;
  if (backuri != NULL && *backuri != '\0')
    g_strstrip(backuri);

  tmpuri = backuri;

  if (tmpuri == NULL || *tmpuri == '\0') {
    goto clean;
  }
  if (gl == NULL)
    gl = dwb.state.fview;

  WebKitWebView *web = WEBVIEW(gl);

  if (!g_strcmp0(tmpuri, "$URI"))
    tmpuri = webkit_web_view_get_uri(WEBVIEW(gl));

  /* new window ? */
  if (dwb.state.nv & OPEN_NEW_WINDOW) {
    dwb.state.nv = OPEN_NORMAL;
    dwb_new_window(tmpuri);
    goto clean;
  }
  /*  new tab ?  */
  else if (dwb.state.nv & OPEN_NEW_VIEW) {
    gboolean background = dwb.state.nv & OPEN_BACKGROUND;
    dwb.state.nv = OPEN_NORMAL;
    view_add(tmpuri, background);
    goto clean;
  }
  /*  get resolved uri */

  dwb_soup_clean();
  /* Check if uri is a html-string */
  if (dwb.state.type == HTML_STRING) {
    webkit_web_view_load_string(web, tmpuri, "text/html", NULL, NULL);
    dwb.state.type = 0;
    goto clean;
  }
  /* Check if uri is a javascript snippet */
  if (g_str_has_prefix(tmpuri, "javascript:")) {
    if (GET_BOOL("javascript-schemes"))
    {
      char *unescaped = g_uri_unescape_string(tmpuri, NULL);
      dwb_execute_script(webkit_web_view_get_main_frame(web), unescaped, false);
      g_free(unescaped);
    }
    else 
      dwb_set_error_message(dwb.state.fview, "Loading of javascript schemes permitted");
    goto clean;
  }
  /* Check if uri is a directory */
  if ( (local_check_directory(gl, tmpuri, true, NULL)) ) {
    goto clean;
  }
  if ( (script = dwb_test_userscript(tmpuri)) ) {
    Arg a = { .arg = script };
    dwb_execute_user_script(NULL, &a);
    g_free(script);
    goto clean;
  }
  /* Check if uri is a regular file */
  if (g_str_has_prefix(tmpuri, "file://") || !g_strcmp0(tmpuri, "about:blank")) {
    webkit_web_view_load_uri(web, tmpuri);
    goto clean;
  }
  else if ( g_file_test(tmpuri, G_FILE_TEST_IS_REGULAR) ) {
    GError *error = NULL;
    if ( !(uri = g_filename_to_uri(tmpuri, NULL, &error)) ) { 
      if (error->code == G_CONVERT_ERROR_NOT_ABSOLUTE_PATH) {
        g_clear_error(&error);
        char *path = g_get_current_dir();
        char *tmp = g_build_filename(path, tmpuri, NULL);
        if ( !(uri = g_filename_to_uri(tmp, NULL, &error))) {
          fprintf(stderr, "Cannot open %s: %s", tmpuri, error->message);
          g_clear_error(&error);
        }
        g_free(tmp);
        g_free(path);
      }
    }
  }
  else if (g_str_has_prefix(tmpuri, "dwb:")) {
    webkit_web_view_load_uri(web, tmpuri);
    goto clean;
  }
  /* Check if searchengine is needed and load uri */
  else {
    if ( g_str_has_prefix(tmpuri, "http://") || g_str_has_prefix(tmpuri, "https://")) {
      uri = g_strdup(tmpuri);
    }
    else if (strchr(tmpuri, ' ')) {
      uri = dwb_get_searchengine(tmpuri);
    }
#ifdef WITH_LIBSOUP_2_38
    else if (dwb.misc.dns_lookup) {
      uri = g_strdup_printf("http://%s", tmpuri);
      
      if (!dwb_check_localhost(tmpuri)) {
        SoupURI *suri = soup_uri_new(uri);
        const char *host = suri ? soup_uri_get_host(suri) : " ";
        VIEW(gl)->status->request_uri = g_strdup(tmpuri);
        soup_session_prefetch_dns(dwb.misc.soupsession, host, NULL, (SoupAddressCallback)callback_dns_resolve, gl);
        if (suri != NULL) soup_uri_free(suri); 
        goto clean;
      }
    }
#endif
    else if (!(uri = dwb_check_searchengine(tmpuri, false))) {
      uri = g_strdup_printf("http://%s", tmpuri);
    }
  }
  webkit_web_view_load_uri(web, uri);
clean: 
  g_free(uri);
  g_free(argback);
}/*}}}*/

/* dwb_clean_key_buffer() {{{*/
void 
dwb_clean_key_buffer() {
  dwb.state.nummod = -1;
  g_string_truncate(dwb.state.buffer, 0);
}/*}}}*/

const char * /* dwb_parse_nummod {{{*/
dwb_parse_nummod(const char *text) {
  char num[6];
  int i=0;
  while (g_ascii_isspace(*text))
    text++;
  for (i=0; i<5 && g_ascii_isdigit(*text); i++, text++) {
    num[i] = *text;
  }
  num[i] = '\0';
  if (*num != '\0')
    dwb.state.nummod = (int)strtol(num, NULL, 10);
  while (g_ascii_isspace(*text)) text++; 
  return text;
}/*}}}*/


gboolean /* dwb_entry_activate (GdkEventKey *e) {{{*/
dwb_entry_activate(GdkEventKey *e) {
  char **token = NULL;
  gboolean status;
  switch (CLEAN_MODE(dwb.state.mode))  {
    case HINT_MODE:           dwb_update_hints(e); return false;
    case FIND_MODE:           dwb_focus_scroll(dwb.state.fview);
                              dwb_update_search();
                              dwb_search(NULL);
                              dwb_change_mode(NORMAL_MODE, true);
                              return true;
    case SEARCH_FIELD_MODE:   dwb_submit_searchengine();
                              return true;
    case SETTINGS_MODE_LOCAL: 
    case SETTINGS_MODE:       token = g_strsplit(GET_TEXT(), " ", 2);
                              dwb_set_setting(token[0], token[1], dwb.state.mode == SETTINGS_MODE ? SET_GLOBAL : SET_LOCAL);
                              //dwb_set_setting(token[0], token[1], 0);
                              dwb_change_mode(NORMAL_MODE, false);
                              g_strfreev(token);
                              return true;
    case KEY_MODE:            token = g_strsplit(GET_TEXT(), " ", 2);
                              status = dwb_set_key(token[0], token[1]);
                              dwb_change_mode(NORMAL_MODE, status == STATUS_ERROR);
                              g_strfreev(token);
                              return true;
    case COMMAND_MODE:        if (dwb.state.mode & COMPLETION_MODE) 
                                completion_clean_completion(false);
                              dwb_parse_command_line(GET_TEXT());
                              return true;
    case DOWNLOAD_GET_PATH:   download_start(NULL); 
                              return true;
    case SAVE_SESSION:        session_save(GET_TEXT(), SESSION_FORCE);
                              dwb_end();
                              return true;
    case COMPLETE_BUFFER:     completion_eval_buffer_completion();
                              return true;
    case QUICK_MARK_SAVE:     dwb_save_quickmark(GET_TEXT());
                              return true;
    case QUICK_MARK_OPEN:     dwb_open_quickmark(GET_TEXT());
                              return true;
    case COMPLETE_PATH:       completion_clean_path_completion();
                              break;
    case COMPLETE_SCRIPTS:    scripts_completion_activate();
                              return true;
    default : break;
  }
  CLEAR_COMMAND_TEXT();
  const char *text = GET_TEXT();
  dwb_load_uri(NULL, text);
  if (text != NULL && *text)
    dwb_glist_prepend_unique(&dwb.fc.navigations, g_strdup(text));
  dwb_change_mode(NORMAL_MODE, false);
  return true;
}/*}}}*/

/* dwb_get_key(GdkEventKey *e, unsigned gint *mod_mask, gboolean *isprint) {{{*/
char *
dwb_get_key(GdkEventKey *e, unsigned int *mod_mask, gboolean *isprint) {
  char *key = util_keyval_to_char(e->keyval, true);
  *isprint = false;
  if (key != NULL) {
    *mod_mask = CLEAN_STATE(e);
    *isprint = true;
  }
  else if ( (key = gdk_keyval_name(e->keyval))) {
    key = g_strdup_printf("@%s@", key);
    *mod_mask = CLEAN_STATE_WITH_SHIFT(e);
  }
  return key;
}/*}}}*/

/* dwb_eval_key(GdkEventKey *e) {{{*/
gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = true, isprint = false;
  int keyval = e->keyval;
  unsigned int mod_mask = 0;
  int keynum = -1;

  if (dwb.state.scriptlock) {
    return false;
  }
  if (e->is_modifier) {
    return false;
  }
  /* don't show backspace in the buffer */
  if (keyval == GDK_KEY_BackSpace && CLEAN_STATE(e) == 0) {
    if (dwb.state.mode & AUTO_COMPLETE) {
      completion_clean_autocompletion();
    }
    if (dwb.state.buffer->len > 0) {
      g_string_erase(dwb.state.buffer, dwb.state.buffer->len - 1, 1);
      dwb_set_status_bar_text(dwb.gui.lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_active, false);
    }
    return false;
  }
  /* Multimedia keys */
  Arg a = { .i = 1 };
  switch (keyval) {
    case GDK_KEY_Back : dwb_history_back(); return true;
    case GDK_KEY_Forward : dwb_history_forward(); return true;
    case GDK_KEY_Cancel : commands_stop_loading(NULL, NULL); return true;
    case GDK_KEY_Reload : commands_reload(NULL, NULL); return true;
    case GDK_KEY_ZoomIn : commands_zoom(NULL, &a); return true;
    case GDK_KEY_ZoomOut : a.i = -1; commands_zoom(NULL, &a); return true;
  }
  char *key = dwb_get_key(e, &mod_mask, &isprint);
  if (key == NULL)
    return false;
  if (DIGIT(e)) {
    keynum = e->keyval - GDK_KEY_0;
    if (dwb.state.nummod >= 0) {
      dwb.state.nummod = MIN(10*dwb.state.nummod + keynum, 314159);
    }
    else {
      dwb.state.nummod = e->keyval - GDK_KEY_0;
    }
    if (mod_mask) {
#define IS_NUMMOD(X)  (((X) & DWB_NUMMOD_MASK) && ((X) & ~DWB_NUMMOD_MASK) == mod_mask)
      for (GSList *l = dwb.custom_commands; l; l=l->next) {
        CustomCommand *c = l->data;
        if (IS_NUMMOD(c->key->mod) || (c->key->mod == mod_mask && c->key->num == dwb.state.nummod)) {
          for (int i=0; c->commands[i]; i++) {
            if (dwb_parse_command_line(c->commands[i]) == STATUS_END)
              return true;
          }
          break;
        }
      }
      for (GList *l = dwb.keymap; l; l=l->next) {
        KeyMap *km = l->data;
        if (IS_NUMMOD(km->mod)) {
        //if ((km->mod & DWB_NUMMOD_MASK) && (km->mod & ~DWB_NUMMOD_MASK) == mod_mask) {
          commands_simple_command(km);
          break;
        }
      }
#undef IS_NUMMOD
    }
    g_free(key);
    return true;
  }
  g_string_append(dwb.state.buffer, key);
  if (ALPHA(e) || DIGIT(e)) {
    dwb_set_status_bar_text(dwb.gui.lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_active, false);
  }

  const char *buf = dwb.state.buffer->str;
  guint longest = 0;
  KeyMap *tmp = NULL;
  GList *coms = NULL;
  for (GSList *l = dwb.custom_commands; l; l=l->next) {
    CustomCommand *c = l->data;
    if (c->key->num == dwb.state.nummod  && c->key->mod == mod_mask) {
      if (!g_strcmp0(c->key->str, buf)) {
        for (int i=0; c->commands[i]; i++) {
          if (dwb_parse_command_line(c->commands[i]) == STATUS_END) {
            return true;
          }
        }
        return true;
      }
      else if (g_str_has_prefix(c->key->str, buf)) 
        longest = 1;
    }
  }

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (km->map->prop & CP_OVERRIDE_ENTRY || km->key == NULL) {
      continue;
    }
    gsize kl = strlen(km->key);
    if (!km->key || !kl ) {
      continue;
    }
    if (g_str_has_prefix(km->key, buf) && (mod_mask == km->mod) ) {
      if  (!longest || kl > longest) {
        longest = kl;
        tmp = km;
      }
      if (dwb.comps.autocompletion) {
        coms = g_list_append(coms, km);
      }
    }
  }
  /* autocompletion */
  if (dwb.state.mode & AUTO_COMPLETE) {
    completion_clean_autocompletion();
  }
  if (coms && g_list_length(coms) > 0) {
    completion_autocomplete(coms, NULL);
  }
  if (tmp && dwb.state.buffer->len == longest) {
    commands_simple_command(tmp);
    ret = true;
  }
  else if (e->state & GDK_CONTROL_MASK || !isprint) {
    ret = false;
  }
  if (longest == 0) {
    dwb_clean_key_buffer();
    CLEAR_COMMAND_TEXT();
  }
  g_free(key);
  return ret;
}/*}}}*/

/* dwb_eval_override_key {{{*/
gboolean 
dwb_eval_override_key(GdkEventKey *e, CommandProperty prop) {
  if (DIGIT(e))
    return false;
  char *key = NULL;
  unsigned int mod; 
  gboolean isprint;
  gboolean ret = false;
  if (CLEAN_STATE(e) && (key = dwb_get_key(e, &mod, &isprint)) != NULL)  {
    for (GList *l = dwb.override_keys; l; l=l->next) {
      KeyMap *m = l->data;
      if (m->map->prop & prop && !g_strcmp0(m->key, key) && m->mod == mod) {
        m->map->func(m, &m->map->arg);
        ret = true; 
        break;
      }
    }
  }
  g_free(key);
  return ret;
}/*}}}*/

/* dwb_insert_mode(Arg *arg) {{{*/
static DwbStatus
dwb_insert_mode(void) {
  dwb_focus_scroll(dwb.state.fview);
  dwb_set_normal_message(dwb.state.fview, false, INSERT_MODE_STRING);

  dwb.state.mode = INSERT_MODE;
  return STATUS_OK;
}/*}}}*/

/* dwb_command_mode(void) {{{*/
static DwbStatus 
dwb_command_mode(void) {
  dwb_set_normal_message(dwb.state.fview, false, ":");
  entry_focus();
  dwb.state.mode = COMMAND_MODE;
  return STATUS_OK;
}/*}}}*/

/* dwb_normal_mode() {{{*/
static DwbStatus 
dwb_normal_mode(gboolean clean) {
  Mode mode = dwb.state.mode;
  if (dwb.state.fview == NULL)
    return STATUS_OK;

  if (mode == HINT_MODE || mode == SEARCH_FIELD_MODE) {
    js_call_as_function(MAIN_FRAME(), CURRENT_VIEW()->js_base, "clear", NULL, kJSTypeUndefined, NULL);
  }
  else if (mode == DOWNLOAD_GET_PATH) {
    completion_clean_path_completion();
  }
  if (mode & COMPLETION_MODE) {
    completion_clean_completion(false);
  }
  dwb_focus_scroll(dwb.state.fview);

  if (clean) {
    dwb_clean_key_buffer();
    CLEAR_COMMAND_TEXT();
  }
  dwb_dom_remove_from_parent(WEBKIT_DOM_NODE(CURRENT_VIEW()->status_element), NULL);
  if (mode == NORMAL_MODE) {
      webkit_web_view_set_highlight_text_matches(CURRENT_WEBVIEW(), false);
  }

  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  dwb_clean_vars();
  return STATUS_OK;
}/*}}}*/

/* dwb_change_mode (Mode mode, ...)   return DwbStatus {{{*/
DwbStatus 
dwb_change_mode(Mode mode, ...) {
  DwbStatus ret = STATUS_OK;
  gboolean clean;
  va_list vl;
  if (dwb.state.mode & AUTO_COMPLETE) 
    completion_clean_autocompletion();
  if (EMIT_SCRIPT(CHANGE_MODE)) {
    char buffer[] = { BASIC_MODES(mode) + 48, 0 };
    ScriptSignal sig = { SCRIPTS_WV(dwb.state.fview), SCRIPTS_SIG_META(buffer, CHANGE_MODE, 0) };
    if (scripts_emit(&sig))
      return STATUS_OK;
  }
  switch(mode) {
    case NORMAL_MODE: 
      va_start(vl, mode);
      clean = va_arg(vl, gboolean);
      ret = dwb_normal_mode(clean);
      va_end(vl);
      break;
    case INSERT_MODE:   ret = dwb_insert_mode(); break;
    case COMMAND_MODE:  ret = dwb_command_mode(); break;
    default: PRINT_DEBUG("Unknown mode: %d", mode); break;
  }
  return ret;
}/*}}}*/

/* gboolean dwb_highlight_search(void) {{{*/
gboolean
dwb_highlight_search() {
  View *v = CURRENT_VIEW();
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  int matches;
  webkit_web_view_unmark_text_matches(web);

  if ( v->status->search_string != NULL && (matches = webkit_web_view_mark_text_matches(web, v->status->search_string, dwb.state.search_flags & FIND_CASE_SENSITIVE, 0)) ) {
    dwb_set_normal_message(dwb.state.fview, false, "[%3d hits] ", matches);
    webkit_web_view_set_highlight_text_matches(web, true);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_update_search(gboolean ) {{{*/
gboolean 
dwb_update_search(void) {
  View *v = CURRENT_VIEW();
  const char *text = GET_TEXT();
  if (strlen(text) > 0) {
    FREE0(v->status->search_string);
    v->status->search_string =  g_strdup(text);
  }
  if (!v->status->search_string) {
    return false;
  }
  if (! dwb_highlight_search()) {
    dwb_set_status_bar_text(dwb.gui.lstatus, "[  0 hits] ", &dwb.color.error, dwb.font.fd_active, false);
    return false;
  }
  return true;
}/*}}}*/

/* dwb_search {{{*/
gboolean
dwb_search(Arg *arg) {
  gboolean ret = false;
  View *v = CURRENT_VIEW();
  gboolean forward = dwb.state.search_flags & FIND_FORWARD;
  if (arg) {
    if (!arg->b) {
      forward = !forward;
    }
    dwb_highlight_search();
  }
  if (v->status->search_string) {
    ret = webkit_web_view_search_text(WEBKIT_WEB_VIEW(v->web), v->status->search_string, dwb.state.search_flags & FIND_CASE_SENSITIVE, forward, true);
  }
  return ret;
}/*}}}*/

static void
dwb_user_script_watch(GPid pid, gint status, UserScripEnv *env) {
  if (WIFEXITED(status) || WIFSIGNALED(status)) {
    g_source_remove(env->source);
    unlink(env->fifo);
    g_free(env->fifo);
    g_io_channel_shutdown(env->channel, true, NULL);
    g_io_channel_unref(env->channel);
    env->channel = NULL;
    g_free(env);
  }
}

/* dwb_user_script_cb(GIOChannel *, GIOCondition *)     return: false {{{*/
static gboolean
dwb_user_script_cb(GIOChannel *channel, GIOCondition condition, UserScripEnv *env) {
  GError *error = NULL;
  char *line;

  while ( g_io_channel_read_line(channel, &line, NULL, NULL, &error) == G_IO_STATUS_NORMAL ) {
    dwb_parse_command_line(g_strchomp(line));
    g_free(line);
  }
  if (error) {
    fprintf(stderr, "Cannot read from std_out: %s\n", error->message);
  }
  g_clear_error(&error);
  return true;
}/*}}}*/

/* dwb_setup_environment(GSList *list) {{{*/
void
dwb_setup_environment(GSList *list) {
  Navigation *n;
  for (GSList *l = list; l; l=l->next) {
    n = l->data;
    g_setenv(n->first, n->second, false);
    dwb_navigation_free(n);
  }
  g_slist_free(list);

}/*}}}*/

/* dwb_execute_user_script(Arg *a) {{{*/
void
dwb_execute_user_script(KeyMap *km, Arg *a) {
  GError *error = NULL;
  char nummod[64];
  snprintf(nummod, sizeof(nummod), "%d", NUMMOD);
  char *argv[] = { a->arg, (char*)webkit_web_view_get_uri(CURRENT_WEBVIEW()), (char *)webkit_web_view_get_title(CURRENT_WEBVIEW()), (char *)dwb.misc.profile, nummod, a->p, NULL } ;
  GPid pid;
  GSList *list = NULL;
  char *fifo;

  const char *uri = webkit_web_view_get_uri(CURRENT_WEBVIEW());
  if (uri != NULL)
    list = g_slist_append(list, dwb_navigation_new("DWB_URI",     uri));

  const char *title = webkit_web_view_get_title(CURRENT_WEBVIEW());
  if (title != NULL)
    list = g_slist_append(list, dwb_navigation_new("DWB_TITLE",   webkit_web_view_get_title(CURRENT_WEBVIEW())));

  list = g_slist_append(list, dwb_navigation_new("DWB_PROFILE", dwb.misc.profile));
  list = g_slist_append(list, dwb_navigation_new("DWB_NUMMOD",  nummod));

  if (a->p != NULL)
    list = g_slist_append(list, dwb_navigation_new("DWB_ARGUMENT",  a->p));

  const char *referer = soup_get_header(dwb.state.fview, "Referer");
  if (referer != NULL)
    list = g_slist_append(list, dwb_navigation_new("DWB_REFERER",  referer));

  const char *user_agent = soup_get_header(dwb.state.fview, "User-Agent");
  if (user_agent != NULL)
    list = g_slist_append(list, dwb_navigation_new("DWB_USER_AGENT",  user_agent));

  fifo = util_get_temp_filename("fifo_");
  list = g_slist_append(list, dwb_navigation_new("DWB_FIFO",  fifo));
  mkfifo(fifo, 0600);
  
  if (g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, (GSpawnChildSetupFunc)dwb_setup_environment, list, &pid, &error)) {
    UserScripEnv *env = g_malloc(sizeof(UserScripEnv));
    env->fifo = fifo;
    env->fd = open(env->fifo, O_RDONLY | O_NONBLOCK); 
    env->channel = g_io_channel_unix_new(env->fd);
    if (env->channel != NULL) {
      dwb_set_normal_message(dwb.state.fview, true, "Executing script %s", a->arg);
      env->source = g_io_add_watch(env->channel, G_IO_IN, (GIOFunc)dwb_user_script_cb, env);
      g_child_watch_add(pid, (GChildWatchFunc)dwb_user_script_watch, env);
    }
    else  {
      close(env->fd);
      unlink(fifo);
      g_free(fifo);
      g_free(env);
    }

  }
  else {
    fprintf(stderr, "Cannot execute %s: %s\n", (char*)a->p, error->message);
    for (GSList *l = list; l; l=l->next) {
      dwb_navigation_free(l->data);
    }
    g_slist_free(list);
  }
  g_clear_error(&error);
}/*}}}*/

/* dwb_get_scripts() {{{*/
static GList * 
dwb_get_scripts() {
  GDir *dir;
  char *filename = NULL;
  char *content = NULL;
  GList *gl = NULL;
  Navigation *n;
  GError *error = NULL;
  FILE *f;
  int l1, l2;

  if ( (dir = g_dir_open(dwb.files[FILES_USERSCRIPTS], 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      gboolean javascript = false;
      char buf[11] = {0};
      char *path = g_build_filename(dwb.files[FILES_USERSCRIPTS], filename, NULL);
      char *realpath;
      /* ignore subdirectories */
      if (g_file_test(path, G_FILE_TEST_IS_DIR))
        continue;
      else if (g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
        realpath = g_file_read_link(path, &error);
        if (realpath != NULL) {
          g_free(path);
          fprintf(stderr, "Cannot read %s : %s\n", path, error->message);
          goto loop_end;
        }
        else {
          g_free(path);
          path = realpath;
        }
      }
      if ( (f = fopen(path, "r")) != NULL && (l1 = fgetc(f)) && (l2 = fgetc(f)) ) {
        if ( (l1 == '#' && l2 == '!') || (l1 == '/' && l2 == '/' && fgetc(f) == '!')  ) {
          if (fgets(buf, sizeof(buf), f) != NULL && !g_strcmp0(buf, "javascript")) {
            int next = fgetc(f);
            if (g_ascii_isspace(next)) {
              javascript = true;
            }
          }
        }
        fclose(f);

      }

      if (!javascript && !g_file_test(path, G_FILE_TEST_IS_EXECUTABLE)) {
        fprintf(stderr, "Warning: userscript %s isn't executable and will be ignored.\n", path);
        FREE0(path);
        goto loop_end;
      }

      g_file_get_contents(path, &content, NULL, NULL);
      if (content == NULL) {
        goto loop_end;
      }
      if (javascript) {
        char *script = strchr(content, '\n');
        if (script && *(script+1)) {
          scripts_init_script(path, script+1);
          goto loop_end;
        }
      }

      char **lines = g_strsplit(content, "\n", -1);


      int i=0;
      n = NULL;
      KeyMap *map = dwb_malloc(sizeof(KeyMap));
      FunctionMap *fmap = dwb_malloc(sizeof(FunctionMap));
      char *tmp;
      while (lines[i]) {
        if (g_regex_match_simple(".*dwb:", lines[i], 0, 0)) {
          char **line = g_strsplit(lines[i], "dwb:", 2);
          if (line[1]) {
            tmp = line[1];
            while (g_ascii_isspace(*tmp))
              tmp++;
            if (*tmp != '\0') {
              n = dwb_navigation_new(filename, tmp);
              Key key = dwb_str_to_key(tmp);
              map->key = key.str;
              map->mod = key.mod;
            }
          }
          g_strfreev(line);
          break;
        }
        i++;
      }
      if (!n) {
        n = dwb_navigation_new(filename, "");
        map->key = "";
        map->mod = 0;
      }
      FunctionMap fm = { { n->first, n->first }, CP_DONT_SAVE | CP_COMMANDLINE | CP_USERSCRIPT, (Func)dwb_execute_user_script, NULL, POST_SM, { .arg = path }, EP_NONE, {NULL} };
      *fmap = fm;
      map->map = fmap;
      dwb.misc.userscripts = g_list_prepend(dwb.misc.userscripts, n);
      gl = g_list_prepend(gl, map);

      g_strfreev(lines);
loop_end: 
      FREE0(content);
    }
    g_dir_close(dir);
  }
  return gl;
}/*}}}*/

/* dwb_reload_userscripts()  {{{*/
void 
dwb_reload_userscripts(void) {
  dwb_free_list(dwb.misc.userscripts, (void_func) dwb_navigation_free);
  dwb.misc.userscripts = NULL;
  g_list_foreach(dwb.misc.userscripts, (GFunc)dwb_navigation_free, NULL);
  g_list_free(dwb.misc.userscripts);
  dwb.misc.userscripts = NULL;
  KeyMap *m;
  GSList *delete = NULL;
  for (GList *l = dwb.keymap; l; l=l->next) {
    m = l->data;
    if (m->map->prop & CP_USERSCRIPT) {
      FREE0(m->map);
      FREE0(m);
      delete = g_slist_prepend(delete, l);
    }
  }
  for (GSList *sl = delete; sl; sl=sl->next) {
    dwb.keymap = g_list_delete_link(dwb.keymap, sl->data);
  }
  g_slist_free(delete);
  dwb.keymap = g_list_concat(dwb.keymap, dwb_get_scripts());
  dwb_set_normal_message(dwb.state.fview, true, "Userscripts reloaded");
}/*}}}*/

/*}}}*/

/* EXIT {{{*/

/* dwb_clean_vars() {{{*/
static void 
dwb_clean_vars() {
  dwb.state.mode = NORMAL_MODE;
  dwb.state.nummod = -1;
  dwb.state.nv = OPEN_NORMAL;
  dwb.state.type = 0;
  dwb.state.dl_action = DL_ACTION_DOWNLOAD;
  if (dwb.state.mimetype_request) {
    FREE0(dwb.state.mimetype_request);
  }
}/*}}}*/

/* dwb_free_list(GList *list, void (*func)(void*)) {{{*/
void
dwb_free_list(GList *list, void (*func)(void*)) {
  for (GList *l = list; l; l=l->next) {
    Navigation *n = l->data;
    func(n);
  }
  g_list_free(list);
}/*}}}*/

static void
dwb_free_custom_keys() {
  for (GSList *l = dwb.custom_commands; l; l=l->next) {
    CustomCommand *c = l->data;
    FREE0(c->key);
    g_strfreev(c->commands);
    c->commands = NULL;
  }
  g_slist_free(dwb.custom_commands);
  dwb.custom_commands = NULL;
}

/* dwb_clean_up() {{{*/
gboolean
dwb_clean_up() {
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    if (m->map->prop & CP_SCRIPT) 
      scripts_unbind(m->map->arg.p);
    if (m->map->prop & CP_USERSCRIPT)
      g_free(m->map);
    g_free(m);
    m = NULL;
  }
  g_list_free(dwb.keymap);
  g_list_free(dwb.override_keys);
  dwb.keymap = NULL;
  g_hash_table_remove_all(dwb.settings);
  g_string_free(dwb.state.buffer, true);
  g_free(dwb.misc.hints);
  g_free(dwb.misc.hint_style);
  g_free(dwb.misc.scripts);

  dwb_free_list(dwb.fc.bookmarks, (void_func)dwb_navigation_free);
  /*  TODO sqlite */
  dwb_free_list(dwb.fc.history, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.searchengines, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.se_completion, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.mimetypes, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.quickmarks, (void_func)dwb_quickmark_free);
  dwb_free_list(dwb.fc.cookies_allow, (void_func)g_free);
  dwb_free_list(dwb.fc.cookies_session_allow, (void_func)g_free);
  dwb_free_list(dwb.fc.navigations, (void_func)g_free);
  dwb_free_list(dwb.fc.commands, (void_func)g_free);
  dwb_free_list(dwb.misc.userscripts, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.pers_plugins, (void_func)g_free);
  dwb_free_list(dwb.fc.pers_scripts, (void_func)g_free);
  dwb_free_custom_keys();

  for (GList *gl = dwb.state.views; gl; gl=gl->next) {
    view_clean(gl);
  }

  dwb_soup_end();
  adblock_end();
  domain_end();

  util_rmdir(dwb.files[FILES_CACHEDIR], true, true);

  for (int i=FILES_FIRST; i<FILES_LAST; i++) {
    g_free(dwb.files[i]);
  }

  gtk_widget_destroy(dwb.gui.window);
  scripts_end();
  return true;
}/*}}}*/

static void 
dwb_save_key_value(const char *file, const char *key, const char *value) {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  char *content;

  if (!g_key_file_load_from_file(keyfile, file, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No keysfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  g_key_file_set_value(keyfile, dwb.misc.profile, key, value);
  if ( (content = g_key_file_to_data(keyfile, NULL, &error)) ) {
    util_set_file_content(file, content);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save keyfile: %s", error->message);
    g_clear_error(&error);
  }
  g_key_file_free(keyfile);
}

/* dwb_save_keys() {{{*/
static void
dwb_save_keys() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  char *content;
  gsize size;

  if (!g_key_file_load_from_file(keyfile, dwb.files[FILES_KEYS], G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No keysfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *map = l->data;
    if (! (map->map->prop & CP_DONT_SAVE) ) {
      char *mod = dwb_modmask_to_string(map->mod);
      char *sc = g_strdup_printf("%s %s", mod, map->key ? map->key : "");
      g_key_file_set_value(keyfile, dwb.misc.profile, map->map->n.first, sc);
      g_free(sc);
      g_free(mod);
    }
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    util_set_file_content(dwb.files[FILES_KEYS], content);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save keyfile: %s", error->message);
    g_clear_error(&error);
  }
  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_save_settings {{{*/
void
dwb_save_settings() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  GList *l;
  char *content;
  gsize size;
  setlocale(LC_NUMERIC, "C");

  if (!g_key_file_load_from_file(keyfile, dwb.files[FILES_SETTINGS], G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No settingsfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  for (l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    char *value = util_arg_to_char(&s->arg, s->type); 
    g_key_file_set_value(keyfile, dwb.misc.profile, s->n.first, value ? value : "" );

    g_free(value);
  }
  if (l != NULL)
    g_list_free(l);
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    util_set_file_content(dwb.files[FILES_SETTINGS], content);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save settingsfile: %s\n", error->message);
    g_clear_error(&error);
  }
  g_key_file_free(keyfile);
}/*}}}*/

/*{{{*/
static void
dwb_save_list(GList *list, const char *filename, int limit) {
  if (g_list_length(list) > 0) {
    GString *buffer = g_string_new(NULL);
    int i = 0;
    for (GList *l = list; l; l=l->next, i++) {
      g_string_append_printf(buffer, "%s\n", (char*)l->data);
    }
    if (buffer->len > 0) 
      util_set_file_content(filename, buffer->str);
    g_string_free(buffer, true);
  }
}/*}}}*/

/* dwb_save_files() {{{*/
gboolean 
dwb_save_files(gboolean end_session) {
  dwb_save_keys();
  dwb_save_settings();
  dwb_sync_files(NULL);
  /* Save command history */
  if (! dwb.misc.private_browsing) {
    dwb_save_list(dwb.fc.navigations, dwb.files[FILES_NAVIGATION_HISTORY], GET_INT("navigation-history-max"));
    dwb_save_list(dwb.fc.commands, dwb.files[FILES_COMMAND_HISTORY], GET_INT("navigation-history-max"));
  }
  /* save session */
  if (end_session && GET_BOOL("save-session") && dwb.state.mode != SAVE_SESSION) {
    session_save(NULL, 0);
  }
  return true;
}
/* }}} */

/* dwb_end() {{{*/
gboolean
dwb_end() {
  if (dwb.state.mode & CONFIRM) 
    return false;
  for (GList *l = dwb.state.views; l; l=l->next) {
    if (LP_PROTECTED(VIEW(l))) {
      if (!dwb_confirm(dwb.state.fview, "There are protected tabs, really close [y/n]?")) {
        CLEAR_COMMAND_TEXT();
        return false;
      }
      break;
    }
  }
  if (dwb.state.download_ref_count > 0) {
    if (!dwb_confirm(dwb.state.fview, "There are unfinished downloads, really close [y/n]?")) {
      CLEAR_COMMAND_TEXT();
      return false;
    }
  }
  if (EMIT_SCRIPT(CLOSE)) {
    ScriptSignal s = { .jsobj = NULL, SCRIPTS_SIG_META(NULL, CLOSE, 0) };
    scripts_emit(&s);
  }

  if (dwb_save_files(true)) {
    if (dwb_clean_up()) {
      application_stop();
      //gtk_main_quit();
      return true;
    }
  }
  return false;
}/*}}}*/

/* }}} */

/* KEYS {{{*/

/* dwb_str_to_key(char *str)      return: Key{{{*/
Key 
dwb_str_to_key(char *str) {
  Key key = { .mod = 0, .str = NULL };
  if (str == NULL || *str == '\0')
    return key;
  g_strstrip(str);
  GString *buffer = g_string_new(NULL);
  GString *keybuffer;
  char *end;

  char **string = g_strsplit(str, " ", -1);

  for (guint i=0; i<g_strv_length(string); i++)  {
    if (!g_ascii_strcasecmp(string[i], "Control")) {
      key.mod |= GDK_CONTROL_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod1")) {
      key.mod |= GDK_MOD1_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod4")) {
      key.mod |= GDK_MOD4_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Shift")) {
      key.mod |= GDK_SHIFT_MASK;
    }
    else if (!g_strcmp0(string[i], "[n]")) {
      key.mod |= DWB_NUMMOD_MASK;
    }
    else {
      g_string_append(buffer, string[i]);
    }
  }
  const char *escape, *start = buffer->str;
  if ((escape = strchr(start, '\\'))) {
    keybuffer = g_string_new(NULL);
    do {
      if (*(escape + 1) == '\\')
        g_string_append_c(keybuffer, '\\');
      else 
        g_string_append_len(keybuffer, start, escape - start);
      start = escape + 1;
    } while ((escape = strchr(start, '\\')));
    g_string_append_len(keybuffer, start, escape - start);
    key.str = keybuffer->str;
    g_string_free(keybuffer, false);
  }
  else 
    key.str = buffer->str;
  key.num = strtol(buffer->str, &end, 10);
  if (end == buffer->str) 
    key.num = -1;

  g_strfreev(string);
  g_string_free(buffer, false);

  return key;
}/*}}}*/

/* dwb_keymap_delete(GList *, KeyValue )     return: GList * {{{*/
static GList * 
dwb_keymap_delete(GList *gl, KeyValue key) {
  for (GList *l = gl; l; l=l->next) {
    KeyMap *km = l->data;
    if (!g_strcmp0(km->map->n.first, key.id)) {
      gl = g_list_delete_link(gl, l);
      break;
    }
  }
  gl = g_list_sort(gl, (GCompareFunc)util_keymap_sort_second);
  return gl;
}/*}}}*/

/* dwb_keymap_add(GList *, KeyValue)     return: GList* {{{*/
GList *
dwb_keymap_add(GList *gl, KeyValue key) {
  gl = dwb_keymap_delete(gl, key);
  for (guint i=0; i<LENGTH(FMAP); i++) {
    if (!g_strcmp0(FMAP[i].n.first, key.id)) {
      KeyMap *keymap = dwb_malloc(sizeof(KeyMap));
      FunctionMap *fmap = &FMAP[i];
      keymap->key = key.key.str ? key.key.str : NULL;
      keymap->mod = key.key.mod;
      fmap->n.first = (char*)key.id;
      keymap->map = fmap;
      gl = g_list_prepend(gl, keymap);
      if (FMAP[i].prop & CP_OVERRIDE)
        dwb.override_keys = g_list_append(dwb.override_keys, keymap);
      break;
    }
  }
  return gl;
}/*}}}*/
/*}}}*/

/* INIT {{{*/

/* dwb_init_key_map() {{{*/
static void 
dwb_init_key_map() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  dwb.keymap = NULL;
  dwb.override_keys = NULL;

  g_key_file_load_from_file(keyfile, dwb.files[FILES_KEYS], G_KEY_FILE_KEEP_COMMENTS, &error);
  if (error) {
    fprintf(stderr, "No keyfile found: %s\nUsing default values.\n", error->message);
    g_clear_error(&error);
  }
  for (guint i=0; i<LENGTH(KEYS); i++) {
    KeyValue kv;
    char *string = g_key_file_get_value(keyfile, dwb.misc.profile, KEYS[i].id, NULL);
    if (string) {
      kv.key = dwb_str_to_key(string);
      g_free(string);
    }
    else if (KEYS[i].key.str) {
      kv.key = KEYS[i].key;
      kv.key.num = 0;
    }
    else {
       kv.key.str = NULL;
       kv.key.mod = 0;
       kv.key.num = 0;
    }

    kv.id = KEYS[i].id;
    dwb.keymap = dwb_keymap_add(dwb.keymap, kv);
  }

  dwb.keymap = g_list_concat(dwb.keymap, dwb_get_scripts());
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)util_keymap_sort_second);

  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_init_settings() {{{*/
void
dwb_init_settings() {
  GError *error = NULL;
  gsize length, numkeys = 0;
  char  **keys = NULL;
  char  *content, *key, *value;
  Arg *arg;
  WebSettings *s;
  GKeyFile  *keyfile = g_key_file_new();
  dwb.settings = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, NULL);
  dwb.state.web_settings = webkit_web_settings_new();
  setlocale(LC_NUMERIC, "C");

  g_file_get_contents(dwb.files[FILES_SETTINGS], &content, &length, &error);
  if (error) {
    fprintf(stderr, "No settingsfile found: %s\nUsing default values.\n", error->message);
    g_clear_error(&error);
  }
  else {
    g_key_file_load_from_data(keyfile, content, length, G_KEY_FILE_KEEP_COMMENTS, &error);
    if (error) {
      fprintf(stderr, "Couldn't read settings file: %s\nUsing default values.\n", error->message);
      g_clear_error(&error);
    }
    else {
      keys = g_key_file_get_keys(keyfile, dwb.misc.profile, &numkeys, &error); 
      if (error) {
        fprintf(stderr, "Couldn't read settings for profile %s: %s\nUsing default values.\n", dwb.misc.profile,  error->message);
        g_clear_error(&error);
      }
    }
  }
  g_free(content);
  for (guint j=0; j<LENGTH(DWB_SETTINGS); j++) {
    s = NULL;
    value = NULL;
    key = g_strdup(DWB_SETTINGS[j].n.first);
    for (guint i=0; i<numkeys; i++) {
      value = g_key_file_get_string(keyfile, dwb.misc.profile, keys[i], NULL);
      if (!g_strcmp0(keys[i], DWB_SETTINGS[j].n.first)) {
        s = dwb_malloc(sizeof(WebSettings));
        *s = DWB_SETTINGS[j];
        if ( (arg = util_char_to_arg(value, s->type)) ) {
          s->arg = *arg;
        }
        break;
      }
    }
    if (s == NULL) {
      s = &DWB_SETTINGS[j];
    }
    s->arg_local = s->arg;
    g_hash_table_insert(dwb.settings, key, s);
    if (s->apply & SETTING_BUILTIN || s->apply & SETTING_ONINIT) {
      s->func(NULL, s);
    }
    g_free(value);
  }
  if (keys)
    g_strfreev(keys);
}/*}}}*/

static DwbStatus 
dwb_init_hints(GList *gl, WebSettings *s) {
  setlocale(LC_NUMERIC, "C");
  g_free(dwb.misc.hints);
  char *scriptpath = util_get_data_file(BASE_SCRIPT, "scripts");
  dwb.misc.hints = util_get_file_content(scriptpath, NULL);
  g_free(scriptpath);

  g_free(dwb.misc.hint_style);
  dwb.misc.hint_style = g_strdup_printf(
      "{ \"hintLetterSeq\" : \"%s\", \"hintFont\" : \"%s\", \"hintStyle\" : \"%s\", \"hintFgColor\" : \"%s\",\
      \"hintBgColor\" : \"%s\", \"hintActiveColor\" : \"%s\", \"hintNormalColor\" : \"%s\", \"hintBorder\" : \"%s\",\
      \"hintOpacity\" : \"%f\", \"hintHighlighLinks\" : %s, \"hintAutoFollow\" : %s }", 
      GET_CHAR("hint-letter-seq"),
      GET_CHAR("hint-font"),
      GET_CHAR("hint-style"), 
      GET_CHAR("hint-fg-color"), 
      GET_CHAR("hint-bg-color"), 
      GET_CHAR("hint-active-color"), 
      GET_CHAR("hint-normal-color"), 
      GET_CHAR("hint-border"), 
      GET_DOUBLE("hint-opacity"),
      GET_BOOL("hint-highlight-links") ? "true" : "false",
      GET_BOOL("hint-autofollow") ? "true" : "false");
  return STATUS_OK;
}

/* dwb_init_style() {{{*/
static void
dwb_init_style() {
  /* Colors  */
  /* Statusbar */
  DWB_COLOR_PARSE(&dwb.color.active_fg, GET_CHAR("foreground-color"));
  DWB_COLOR_PARSE(&dwb.color.active_bg, GET_CHAR("background-color"));

  /* Tabs */
  DWB_COLOR_PARSE(&dwb.color.tab_active_fg, GET_CHAR("tab-active-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.tab_active_bg, GET_CHAR("tab-active-bg-color"));
  DWB_COLOR_PARSE(&dwb.color.tab_normal_fg1, GET_CHAR("tab-normal-fg-color-1"));
  DWB_COLOR_PARSE(&dwb.color.tab_normal_bg1, GET_CHAR("tab-normal-bg-color-1"));
  DWB_COLOR_PARSE(&dwb.color.tab_normal_fg2, GET_CHAR("tab-normal-fg-color-2"));
  DWB_COLOR_PARSE(&dwb.color.tab_normal_bg2, GET_CHAR("tab-normal-bg-color-2"));

  /* Downloads */
  DWB_COLOR_PARSE(&dwb.color.download_fg, GET_CHAR("download-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.download_bg, GET_CHAR("download-bg-color"));
  DWB_COLOR_PARSE(&dwb.color.download_start, GET_CHAR("download-gradient-start"));
  DWB_COLOR_PARSE(&dwb.color.download_end, GET_CHAR("download-gradient-end"));

  /* SSL */
  DWB_COLOR_PARSE(&dwb.color.ssl_trusted, GET_CHAR("ssl-trusted-color"));
  DWB_COLOR_PARSE(&dwb.color.ssl_untrusted, GET_CHAR("ssl-untrusted-color"));

  DWB_COLOR_PARSE(&dwb.color.active_c_bg, GET_CHAR("active-completion-bg-color"));
  DWB_COLOR_PARSE(&dwb.color.active_c_fg, GET_CHAR("active-completion-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.normal_c_bg, GET_CHAR("normal-completion-bg-color"));
  DWB_COLOR_PARSE(&dwb.color.normal_c_fg, GET_CHAR("normal-completion-fg-color"));

  DWB_COLOR_PARSE(&dwb.color.error, GET_CHAR("error-color"));
  DWB_COLOR_PARSE(&dwb.color.prompt, GET_CHAR("prompt-color"));

  dwb.color.tab_number_color = GET_CHAR("tab-number-color");
  dwb.color.tab_protected_color = GET_CHAR("tab-protected-color");
  dwb.color.allow_color = GET_CHAR("status-allowed-color");
  dwb.color.block_color = GET_CHAR("status-blocked-color");

  dwb.color.progress_full = GET_CHAR("progress-bar-full-color");
  dwb.color.progress_empty = GET_CHAR("progress-bar-empty-color");

  char *font = GET_CHAR("font");
  if (font) 
    dwb.font.fd_active = pango_font_description_from_string(font);
  char *f;
#define SET_FONT(var, prop) do { \
  if ((f = GET_CHAR(prop)) != NULL) \
    var = pango_font_description_from_string(f); \
  else if (dwb.font.fd_active) \
    var = dwb.font.fd_active; \
  } while(0)

  SET_FONT(dwb.font.fd_inactive, "font-nofocus");
  SET_FONT(dwb.font.fd_entry, "font-entry");
  SET_FONT(dwb.font.fd_completion, "font-completion");
#undef SET_FONT
} /*}}}*/

static void
dwb_apply_style() {
  DWB_WIDGET_OVERRIDE_FONT(dwb.gui.entry, dwb.font.fd_entry);
  DWB_WIDGET_OVERRIDE_BASE(dwb.gui.entry, GTK_STATE_NORMAL, &dwb.color.active_bg);
  DWB_WIDGET_OVERRIDE_TEXT(dwb.gui.entry, GTK_STATE_NORMAL, &dwb.color.active_fg);

  DWB_WIDGET_OVERRIDE_BACKGROUND(dwb.gui.statusbox, GTK_STATE_NORMAL, &dwb.color.active_bg);
  DWB_WIDGET_OVERRIDE_COLOR(dwb.gui.rstatus, GTK_STATE_NORMAL, &dwb.color.active_fg);
  DWB_WIDGET_OVERRIDE_COLOR(dwb.gui.lstatus, GTK_STATE_NORMAL, &dwb.color.active_fg);
  DWB_WIDGET_OVERRIDE_FONT(dwb.gui.rstatus, dwb.font.fd_active);
  DWB_WIDGET_OVERRIDE_FONT(dwb.gui.urilabel, dwb.font.fd_active);
  DWB_WIDGET_OVERRIDE_FONT(dwb.gui.lstatus, dwb.font.fd_active);

  DWB_WIDGET_OVERRIDE_BACKGROUND(dwb.gui.window, GTK_STATE_NORMAL, &dwb.color.active_bg);
}

DwbStatus
dwb_pack(const char *layout, gboolean rebuild) {
  DwbStatus ret = STATUS_OK;
  const char *default_layout = DEFAULT_WIDGET_PACKING;
  if (layout == NULL) {
    layout = default_layout;
    ret = STATUS_ERROR;
  }
  while (g_ascii_isspace(*layout))
    layout++;
  if (strlen(layout) != WIDGET_PACK_LENGTH) {
    layout = default_layout;
    ret = STATUS_ERROR;
  }

  const char *valid_chars = default_layout;
  char *buf = g_ascii_strdown(layout, WIDGET_PACK_LENGTH);
  char *matched;
  while (*valid_chars) {
    if ((matched = strchr(buf, *valid_chars)) == NULL) {
      ret = STATUS_ERROR;
      layout = default_layout;
      break;
    }
    *matched = 'x';
    valid_chars++;
  }
  g_free(buf);
  const char *bak = layout;

  if (rebuild) {
    gtk_widget_remove_from_parent(dwb.gui.topbox);
    gtk_widget_remove_from_parent(dwb.gui.downloadbar);
    gtk_widget_remove_from_parent(dwb.gui.mainbox);
    gtk_widget_remove_from_parent(dwb.gui.statusbox);
    gtk_widget_remove_from_parent(dwb.gui.bottombox);
  }
  gboolean wv = false;
  while (*bak) {
    switch (*bak) {
      case 't': 
        gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.topbox, false, false, 0);
        dwb.state.bar_visible |= BAR_VIS_TOP;
        break;
      case 'T': 
        gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.topbox, false, false, 0);
        dwb.state.bar_visible &= ~BAR_VIS_TOP;
        break;
      case 'd': 
        gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.downloadbar, false, false, 0);
        break;
      case 'w': 
        gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.mainbox, true, true, 0);
        wv = true;
        break;
      case 's': 
        if (! wv) {
          gtk_box_pack_start(GTK_BOX(dwb.gui.bottombox), dwb.gui.statusbox, false, false, 0);
        }
        else {
          gtk_box_pack_end(GTK_BOX(dwb.gui.bottombox), dwb.gui.statusbox, false, false, 0);
        }
        gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.bottombox, false, false, 0);
        dwb.state.bar_visible |= BAR_VIS_STATUS;
        break;
      case 'S': 
        if (! wv) {
          gtk_box_pack_start(GTK_BOX(dwb.gui.bottombox), dwb.gui.statusbox, false, false, 0);
        }
        else {
          gtk_box_pack_end(GTK_BOX(dwb.gui.bottombox), dwb.gui.statusbox, false, false, 0);
        }
        gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.bottombox, false, false, 0);
        dwb.state.bar_visible &= ~BAR_VIS_STATUS;
        break;
      default: break;
    }
    bak++;

  }
  gtk_widget_show_all(dwb.gui.statusbox);
  gtk_widget_set_visible(dwb.gui.bottombox, dwb.state.bar_visible & BAR_VIS_STATUS);
  gtk_widget_set_visible(dwb.gui.topbox, dwb.state.bar_visible & BAR_VIS_TOP);
  return ret;
}

/* dwb_init_gui() {{{*/
static void 
dwb_init_gui() {
  /* Window */
  if (dwb.gui.wid == 0) 
    dwb.gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  else 
    dwb.gui.window = gtk_plug_new(dwb.gui.wid);
  gtk_widget_set_name(dwb.gui.window, dwb.misc.name);
  /* Icon */
  GdkPixbuf *icon_pixbuf = gdk_pixbuf_new_from_xpm_data(icon);
  gtk_window_set_icon(GTK_WINDOW(dwb.gui.window), icon_pixbuf);

#if _HAS_GTK3
  gtk_window_set_has_resize_grip(GTK_WINDOW(dwb.gui.window), false);
  GtkCssProvider *provider = gtk_css_provider_get_default();
  GString *buffer = g_string_new("GtkEntry {background-image: none; }");
  if (! GET_BOOL("scrollbars")) {
    g_string_append(buffer, "GtkScrollbar { \
        -GtkRange-slider-width: 0; \
        -GtkRange-trough-border: 0; \
        }\
        GtkScrolledWindow {\
          -GtkScrolledWindow-scrollbar-spacing : 0;\
        }");
  }
  gtk_css_provider_load_from_data(provider, buffer->str, -1, NULL);
  g_string_free(buffer, true);
  GdkScreen *screen = gdk_screen_get_default();
  gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#endif

  gtk_window_set_default_size(GTK_WINDOW(dwb.gui.window), GET_INT("default-width"), GET_INT("default-height"));
  gtk_window_set_geometry_hints(GTK_WINDOW(dwb.gui.window), NULL, NULL, GDK_HINT_MIN_SIZE);
  g_signal_connect(dwb.gui.window, "delete-event", G_CALLBACK(callback_delete_event), NULL);
  g_signal_connect(dwb.gui.window, "key-press-event", G_CALLBACK(callback_key_press), NULL);
  g_signal_connect(dwb.gui.window, "key-release-event", G_CALLBACK(callback_key_release), NULL);


  /* Main */
#if _HAS_GTK3 
  dwb.gui.vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  dwb.gui.topbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_box_set_homogeneous(GTK_BOX(dwb.gui.topbox), true);
  dwb.gui.mainbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_box_set_homogeneous(GTK_BOX(dwb.gui.mainbox), true);
#else
  dwb.gui.vbox = gtk_vbox_new(false, 0);
  dwb.gui.topbox = gtk_hbox_new(true, 1);
  dwb.gui.mainbox = gtk_hbox_new(true, 1);
#endif

  /* Downloadbar */
#if _HAS_GTK3 
  dwb.gui.downloadbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
#else
  dwb.gui.downloadbar = gtk_hbox_new(false, 3);
#endif


  /* entry */
  dwb.gui.entry = gtk_entry_new();
  gtk_entry_set_inner_border(GTK_ENTRY(dwb.gui.entry), NULL);

  gtk_entry_set_has_frame(GTK_ENTRY(dwb.gui.entry), false);
  gtk_entry_set_inner_border(GTK_ENTRY(dwb.gui.entry), false);

#if _HAS_GTK3 
  dwb.gui.bottombox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
  dwb.gui.bottombox = gtk_vbox_new(false, 0);
#endif
  dwb.gui.statusbox = gtk_event_box_new();
  dwb.gui.lstatus = gtk_label_new(NULL);
  dwb.gui.urilabel = gtk_label_new(NULL);
  dwb.gui.rstatus = gtk_label_new(NULL);

  gtk_misc_set_alignment(GTK_MISC(dwb.gui.lstatus), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(dwb.gui.urilabel), 1.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(dwb.gui.rstatus), 1.0, 0.5);
  gtk_label_set_use_markup(GTK_LABEL(dwb.gui.lstatus), true);
  gtk_label_set_use_markup(GTK_LABEL(dwb.gui.urilabel), true);
  gtk_label_set_use_markup(GTK_LABEL(dwb.gui.rstatus), true);
  gtk_label_set_ellipsize(GTK_LABEL(dwb.gui.urilabel), PANGO_ELLIPSIZE_MIDDLE);

  DWB_WIDGET_OVERRIDE_COLOR(dwb.gui.urilabel, GTK_STATE_NORMAL, &dwb.color.active_fg);

#if _HAS_GTK3
  dwb.gui.status_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
#else 
  dwb.gui.status_hbox = gtk_hbox_new(false, 2);
#endif
  GtkWidget *alignment = gtk_alignment_new(0.5, 0.5, 1, 1);
  int padding = GET_INT("bars-padding");
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), padding, padding, padding, padding);
  gtk_container_add(GTK_CONTAINER(alignment), dwb.gui.status_hbox);

  gtk_box_pack_start(GTK_BOX(dwb.gui.status_hbox), dwb.gui.lstatus, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.status_hbox), dwb.gui.entry, true, true, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.status_hbox), dwb.gui.urilabel, true, true, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.status_hbox), dwb.gui.rstatus, false, false, 0);
  gtk_container_add(GTK_CONTAINER(dwb.gui.statusbox), alignment);
  gtk_box_pack_end(GTK_BOX(dwb.gui.vbox), scratchpad_get(), false, false, 0);

  gtk_container_add(GTK_CONTAINER(dwb.gui.window), dwb.gui.vbox);

  gtk_widget_show(dwb.gui.mainbox);
  gtk_widget_show(dwb.gui.vbox);
  gtk_widget_show(dwb.gui.window);

  g_signal_connect(dwb.gui.entry, "key-press-event",                     G_CALLBACK(callback_entry_key_press), NULL);
  g_signal_connect(dwb.gui.entry, "key-release-event",                   G_CALLBACK(callback_entry_key_release), NULL);
  g_signal_connect(dwb.gui.entry, "insert-text",                         G_CALLBACK(callback_entry_insert_text), NULL);

  dwb_apply_style();
} /*}}}*/

/* dwb_init_file_content {{{*/
GList *
dwb_init_file_content(GList *gl, const char *filename, Content_Func func) {
  char **lines = util_get_lines(filename);
  char *line;
  void *value;

  if (lines) {
    int length = MAX(g_strv_length(lines) - 1, 0);
    for (int i=0;  i < length; i++) {
      line = lines[i];
      while (g_ascii_isspace(*line))
        line++;
      if (*line == '\0' || *line == '#')
        continue;
      value = func(line);
      if (value != NULL)
        gl = g_list_append(gl, value);
    }
    g_strfreev(lines);
  }
  return gl;
}/*}}}*/

static Navigation * 
dwb_get_search_completion_from_navigation(Navigation *n) {
  char *uri = n->second;
  n->second = util_domain_from_uri(n->second);

  g_free(uri);
  return n;
}
static Navigation * 
dwb_get_search_completion(const char *text) {
  Navigation *n = dwb_navigation_new_from_line(text);
  return dwb_get_search_completion_from_navigation(n);
}

static inline void
dwb_check_create(const char *filename) {
  if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
    int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    close(fd);
  }
}
GList *
dwb_get_simple_list(GList *gl, const char *filename) {
  if (gl != NULL) {
    for (GList *l = gl; l; l=l->next) {
      g_free(l->data);
    }
    g_list_free(gl);
    gl = NULL;
  }
  char **lines = util_get_lines(filename);
  if (lines == NULL)
    return NULL;
  for (int i=0; lines[i]; i++) {
    gl = g_list_prepend(gl, lines[i]);
  }
  return gl;
}

/* dwb_init_files() {{{*/
void
dwb_init_files() {
  char *path           = util_build_path();
  char *profile_path   = util_check_directory(g_build_filename(path, dwb.misc.profile, NULL));
  char *userscripts, *cachedir;

  dwb.fc.bookmarks = NULL;
  dwb.fc.history = NULL;
  dwb.fc.quickmarks = NULL;
  dwb.fc.searchengines = NULL;
  dwb.fc.se_completion = NULL;
  dwb.fc.mimetypes = NULL;
  dwb.fc.navigations = NULL;
  dwb.fc.commands = NULL;


  cachedir = g_build_filename(g_get_user_cache_dir(), dwb.misc.name, NULL);
  dwb.files[FILES_CACHEDIR] = util_check_directory(cachedir);

  dwb.files[FILES_BOOKMARKS]       = g_build_filename(profile_path, "bookmarks",     NULL);
  dwb_check_create(dwb.files[FILES_BOOKMARKS]);
  dwb.files[FILES_HISTORY]         = g_build_filename(profile_path, "history",       NULL);
  dwb_check_create(dwb.files[FILES_HISTORY]);
  dwb.files[FILES_QUICKMARKS]      = g_build_filename(profile_path, "quickmarks",    NULL);
  dwb_check_create(dwb.files[FILES_QUICKMARKS]);
  dwb.files[FILES_SESSION]         = g_build_filename(profile_path, "session",       NULL);
  dwb_check_create(dwb.files[FILES_SESSION]);
  dwb.files[FILES_NAVIGATION_HISTORY] = g_build_filename(profile_path, "navigate.history",       NULL);
  dwb_check_create(dwb.files[FILES_NAVIGATION_HISTORY]);
  dwb.files[FILES_COMMAND_HISTORY] = g_build_filename(profile_path, "commands.history",       NULL);
  dwb_check_create(dwb.files[FILES_COMMAND_HISTORY]);
  dwb.files[FILES_SEARCHENGINES]   = g_build_filename(path, "searchengines", NULL);
  dwb_check_create(dwb.files[FILES_SEARCHENGINES]);
  dwb.files[FILES_KEYS]            = g_build_filename(path, "keys",          NULL);
  dwb.files[FILES_SETTINGS]        = g_build_filename(path, "settings",      NULL);
  dwb.files[FILES_MIMETYPES]       = g_build_filename(path, "mimetypes",      NULL);
  dwb_check_create(dwb.files[FILES_MIMETYPES]);
  dwb.files[FILES_COOKIES]         = g_build_filename(profile_path, "cookies",       NULL);
  dwb_check_create(dwb.files[FILES_COOKIES]);
  dwb.files[FILES_COOKIES_ALLOW]   = g_build_filename(profile_path, "cookies.allow", NULL);
  dwb_check_create(dwb.files[FILES_COOKIES_ALLOW]);
  dwb.files[FILES_COOKIES_SESSION_ALLOW]   = g_build_filename(profile_path, "cookies_session.allow", NULL);
  dwb_check_create(dwb.files[FILES_COOKIES_SESSION_ALLOW]);
  dwb.files[FILES_SCRIPTS_ALLOW]   = g_build_filename(profile_path, "scripts.allow",      NULL);
  dwb_check_create(dwb.files[FILES_SCRIPTS_ALLOW]);
  dwb.files[FILES_PLUGINS_ALLOW]   = g_build_filename(profile_path, "plugins.allow",      NULL);
  dwb_check_create(dwb.files[FILES_PLUGINS_ALLOW]);
  dwb.files[FILES_CUSTOM_KEYS]     = g_build_filename(profile_path, "custom_keys",      NULL);
  dwb_check_create(dwb.files[FILES_CUSTOM_KEYS]);

  userscripts               = g_build_filename(path, "userscripts",   NULL);
  dwb.files[FILES_USERSCRIPTS]     = util_check_directory(userscripts);

  dwb.fc.bookmarks = dwb_init_file_content(dwb.fc.bookmarks, dwb.files[FILES_BOOKMARKS], (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.history = dwb_init_file_content(dwb.fc.history, dwb.files[FILES_HISTORY], (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.quickmarks = dwb_init_file_content(dwb.fc.quickmarks, dwb.files[FILES_QUICKMARKS], (Content_Func)dwb_quickmark_new_from_line); 
  dwb.fc.searchengines = dwb_init_file_content(dwb.fc.searchengines, dwb.files[FILES_SEARCHENGINES], (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.se_completion = dwb_init_file_content(dwb.fc.se_completion, dwb.files[FILES_SEARCHENGINES], (Content_Func)dwb_get_search_completion);
  dwb.fc.mimetypes = dwb_init_file_content(dwb.fc.mimetypes, dwb.files[FILES_MIMETYPES], (Content_Func)dwb_navigation_new_from_line);
  dwb.fc.navigations = dwb_init_file_content(dwb.fc.navigations, dwb.files[FILES_NAVIGATION_HISTORY], (Content_Func)dwb_return);
  dwb.fc.commands = dwb_init_file_content(dwb.fc.commands, dwb.files[FILES_COMMAND_HISTORY], (Content_Func)dwb_return);
  dwb.fc.tmp_scripts = NULL;
  dwb.fc.tmp_plugins = NULL;
  dwb.fc.downloads   = NULL;
  dwb.fc.pers_scripts = dwb_get_simple_list(NULL, dwb.files[FILES_SCRIPTS_ALLOW]);
  dwb.fc.pers_plugins = dwb_get_simple_list(NULL, dwb.files[FILES_PLUGINS_ALLOW]);

  if (g_list_last(dwb.fc.searchengines)) 
    dwb.misc.default_search = ((Navigation*)dwb.fc.searchengines->data)->second;
  else 
    dwb.misc.default_search = NULL;
  dwb.fc.cookies_allow = dwb_init_file_content(dwb.fc.cookies_allow, dwb.files[FILES_COOKIES_ALLOW], (Content_Func)dwb_return);
  dwb.fc.cookies_session_allow = dwb_init_file_content(dwb.fc.cookies_session_allow, dwb.files[FILES_COOKIES_SESSION_ALLOW], (Content_Func)dwb_return);

  g_free(path);
  g_free(profile_path);
}/*}}}*/

/* signals {{{*/
static void
dwb_handle_signal(int s) {
  if (((s == SIGTERM || s == SIGINT) && dwb_end()) || s == SIGFPE || s == SIGILL || s == SIGQUIT) 
    exit(EXIT_SUCCESS);
  else if (s == SIGSEGV) {
    fprintf(stderr, "Received SIGSEGV, trying to clean up.\n");
#ifdef HAS_EXECINFO
    void  *buffer[100];
    char **symbols = NULL;
    int trace_size = backtrace(buffer, 100);
    symbols = backtrace_symbols(buffer, trace_size);
    fprintf(stderr, "\nLast %d stack frames: \n\n", trace_size);
    for (int i=0; trace_size; i++)
      fprintf(stderr, "%3d: %s\n", trace_size-i, symbols[i]);
    g_free(symbols);
#endif
    dwb_clean_up();
    exit(EXIT_FAILURE);
  }
}

void 
dwb_init_signals() {
  for (guint i=0; i<LENGTH(signals); i++) {
    struct sigaction act, oact;
    act.sa_handler = dwb_handle_signal;

    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(signals[i], &act, &oact);
  }
}/*}}}*/

char *
dwb_get_stock_item_base64_encoded(const char *name) {
  GdkPixbuf *pb;
  char *ret = NULL;
#if _HAS_GTK3
  pb = gtk_widget_render_icon_pixbuf(dwb.gui.window, name, -1);
#else 
  pb = gtk_widget_render_icon(dwb.gui.window, name, -1, NULL);
#endif
  if (pb) {
    char *buffer;
    size_t buffer_size;
    gboolean success = gdk_pixbuf_save_to_buffer(pb, &buffer, &buffer_size, "png", NULL, NULL);
    if (success) {
      char *encoded = g_base64_encode((unsigned char*)buffer, buffer_size);
      ret = g_strdup_printf("data:image/png;base64,%s", encoded);
      g_free(encoded);
      g_free(buffer);
    }
    g_object_unref(pb);
  }
  return ret;
}

void
dwb_init_custom_keys(gboolean reload) {
  if (reload)
    dwb_free_custom_keys();
  char **lines = util_get_lines(dwb.files[FILES_CUSTOM_KEYS]);
  if (lines == NULL)
    return;
  const char *current_line;
  GString *keybuf;
  CustomCommand *command;

  for (int i=0; lines[i]; i++) {
    if (! *lines[i]) 
      continue;
    keybuf = g_string_new(NULL);
    
    current_line = lines[i];
    while (*current_line && *current_line != ':') {
      if (*current_line == '\\') {
        current_line++;
        if (!*current_line) {
          continue;
        }
      }
      g_string_append_c(keybuf, *current_line);
      current_line++;
    }
    if (*current_line != ':') {
      g_string_free(keybuf, true);
      continue;
    }
    current_line++;
    if (!*current_line) {
      g_string_free(keybuf, true);
      continue;
    }
    
    command = dwb_malloc(sizeof(CustomCommand));
    command->key = dwb_malloc(sizeof(Key));

    *(command->key) = dwb_str_to_key(keybuf->str);
    command->commands = g_strsplit(current_line, ";;", -1);
    dwb.custom_commands = g_slist_append(dwb.custom_commands, command);
    g_string_free(keybuf, true);
  }
  g_strfreev(lines);
}

void
dwb_init_vars(void) {
  dwb.state.views = NULL;
  dwb.state.fview = NULL;
  dwb.state.fullscreen = false;
  dwb.state.download_ref_count = 0;
  dwb.state.message_id = 0;

  dwb.state.bar_visible = BAR_VIS_TOP | BAR_VIS_STATUS;

  dwb.comps.completions = NULL; 
  dwb.comps.active_comp = NULL;
  dwb.comps.view = NULL;

  dwb.misc.max_c_items = MAX_COMPLETIONS;
  dwb.misc.userscripts = NULL;
  dwb.misc.proxyuri = NULL;
  dwb.misc.scripts = NULL;

  dwb.misc.hints = NULL;
  dwb.misc.hint_style = NULL;

  dwb.misc.sync_interval = 0;
  dwb.misc.synctimer = 0;
  dwb.misc.sync_files = SYNC_ALL;

  dwb.misc.bar_height = 0;
  dwb.state.last_tab = 0;
}


/* dwb_init() {{{*/
void 
dwb_init() {
  dwb_init_signals();
  dwb_clean_vars();
  dwb.state.buffer = g_string_new(NULL);

  dwb.misc.tabbed_browsing = GET_BOOL("tabbed-browsing");

  char *cache_model = GET_CHAR("cache-model");

  if (cache_model != NULL && !g_ascii_strcasecmp(cache_model, "documentviewer"))
    webkit_set_cache_model(WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

  dwb_init_key_map();
  dwb_init_style();
  dwb_init_gui();
  dwb_init_custom_keys(false);
  domain_init();
  adblock_init();
  dwb_init_hints(NULL, NULL);

  dwb_soup_init();
} /*}}}*/ /*}}}*/

/* FIFO {{{*/
/* dwb_parse_command_line(const char *line) {{{*/
DwbStatus 
dwb_parse_command_line(const char *line) {
  DwbStatus ret = STATUS_OK;
  if (line == NULL || *line == '\0')
    return STATUS_OK;
  const char *bak;
  int nummod;
  line = util_str_chug(line);
  bak = dwb_parse_nummod(line);
  nummod = dwb.state.nummod;
  char **token = g_strsplit(bak, " ", 2);
  KeyMap *m = NULL;
  gboolean found;
  gboolean has_arg = false;

  if (!token[0]) 
    return STATUS_OK;

  if (token[1])
    has_arg = true;

  for (GList *l = dwb.keymap; l; l=l->next) {
    bak = token[0];
    found = false;
    m = l->data;
    if (!g_strcmp0(m->map->n.first, bak)) 
      found = true;
    else {
      for (int i=0; m->map->alias[i]; i++) {
        if (!g_strcmp0(m->map->alias[i], bak)) {
          found = true;
          break;
        }
      }
    }
    if (found) {
      if (m->map->prop & CP_HAS_MODE) 
        dwb_change_mode(NORMAL_MODE, true);
      dwb.state.nummod = nummod;
      if (token[1] && ! m->map->arg.ro) {
        g_strstrip(token[1]);
        m->map->arg.p = token[1];
      }
      if (gtk_widget_has_focus(dwb.gui.entry) && (m->map->prop & CP_OVERRIDE_ENTRY)) {
        m->map->func(&m, &m->map->arg);
      }
      else {
        ret = commands_simple_command(m);
      }
      break;
    }
  }
  g_strfreev(token);
  if (ret == STATUS_END)
    return ret;
  dwb_glist_prepend_unique(&dwb.fc.commands, g_strdup(line));
  dwb.state.nummod = -1;
  /* Check for dwb.keymap is necessary for commands that quit dwb. */
  if (dwb.keymap == NULL || m == NULL)
    return ret;
  if (m->map->prop & CP_HAS_MODE)
    return STATUS_OK;
  if (!(m->map->prop & CP_DONT_CLEAN) || (m->map->prop & CP_NEEDS_ARG && has_arg) ) {
    dwb_change_mode(NORMAL_MODE, dwb.state.message_id == 0);
  }
  return ret;
}/*}}}*/
void 
dwb_parse_commands(const char *line) {
  char **commands = g_strsplit(util_str_chug(line), ";;", -1);
  for (int i=0; commands[i]; i++) {
    dwb_parse_command_line(commands[i]);
  }
  g_strfreev(commands);
}
/*}}}*/

void
dwb_version() {
  fprintf(stderr, "    This is : "NAME"\n"
                  "    Version : "VERSION"\n"
                  "      Built : "__DATE__" "__TIME__"\n"
                  "  Copyright : "COPYRIGHT"\n"
                  "    License : "LICENSE"\n");
}
/* MAIN {{{*/
int 
main(int argc, char *argv[]) {
  dwb.misc.name = REAL_NAME;
  dwb.misc.profile = "default";
  dwb.misc.prog_path = argv[0];
  dwb.gui.wid = 0;
  gint ret = application_run(argc, argv); 
  return ret;
}/*}}}*/
