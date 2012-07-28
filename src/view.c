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

#include <string.h>
#include <gdk/gdkkeysyms.h> 
#include <JavaScriptCore/JavaScript.h> 
#include "dwb.h"
#include "commands.h"
#include "util.h"
#include "download.h"
#include "session.h"
#include "view.h"
#include "html.h"
#include "plugins.h"
#include "local.h"
#include "soup.h"
#include "adblock.h"
#include "js.h"
#include "scripts.h"

static void view_ssl_state(GList *);
static unsigned long _click_time;
static guint _sig_caret_button_release;
static guint _sig_caret_motion;
static const char const* dummy_icon[] = { "1 1 1 1 ", "- c NONE", "", };


/* CALLBACKS */
static gboolean view_caret_release_cb(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other);
static gboolean view_caret_motion_cb(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other);
static gboolean view_button_press_cb(WebKitWebView *, GdkEventButton *, GList *);
static gboolean view_close_web_view_cb(WebKitWebView *, GList *);
static gboolean view_console_message_cb(WebKitWebView *, char *, int , char *, GList *);
static WebKitWebView * view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *);
static gboolean view_download_requested_cb(WebKitWebView *, WebKitDownload *, GList *);
static WebKitWebView * view_inspect_web_view_cb(WebKitWebInspector *, WebKitWebView *, GList *);
static void view_hovering_over_link_cb(WebKitWebView *, char *, char *, GList *);
static gboolean view_mime_type_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, char *, WebKitWebPolicyDecision *, GList *);
static gboolean view_navigation_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, WebKitWebNavigationAction *, WebKitWebPolicyDecision *, GList *);
static gboolean view_new_window_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, WebKitWebNavigationAction *, WebKitWebPolicyDecision *, GList *);
//static void view_resource_request_cb(WebKitWebView *, WebKitWebFrame *, WebKitWebResource *, WebKitNetworkRequest *, WebKitNetworkResponse *, GList *);
static gboolean view_scroll_cb(GtkWidget *, GdkEventScroll *, GList *);
static gboolean view_value_changed_cb(GtkAdjustment *, GList *);
static void view_title_cb(WebKitWebView *, GParamSpec *, GList *);
static void view_uri_cb(WebKitWebView *, GParamSpec *, GList *);
static void view_load_status_cb(WebKitWebView *, GParamSpec *, GList *);
static gboolean view_tab_button_press_cb(GtkWidget *, GdkEventButton *, GList *);
static void view_frame_created_cb(WebKitWebView *wv, WebKitWebFrame *, GList *);

/* WEB_VIEW_CALL_BACKS {{{*/

/* view_main_frame_committed_cb {{{*/
#if 0
void
view_main_frame_committed_cb(WebKitWebFrame *frame, GList *gl) {
//  if (gl == dwb.state.fview && (dwb.state.mode == INSERT_MODE || dwb.state.mode == FIND_MODE)) {
//    dwb_change_mode(NORMAL_MODE, true);
//  }
}/*}}}*/
#endif

static void 
view_resource_request_cb(WebKitWebView *wv, WebKitWebFrame *frame, WebKitWebResource *resource, WebKitNetworkRequest *request, WebKitNetworkResponse *response, GList *gl) {
  if (EMIT_SCRIPT(RESOURCE))  {
    ScriptSignal signal = {
      SCRIPTS_WV(gl), .objects = { G_OBJECT(frame), G_OBJECT(request), G_OBJECT(response) }, SCRIPTS_SIG_META(NULL, RESOURCE, 3) };
    scripts_emit(&signal);
  }
  if (dwb.state.do_not_track) {
    SoupMessage *msg = webkit_network_request_get_message(request);
    if (msg != NULL) {
      soup_message_headers_append(msg->request_headers, "DNT", "1");
    }
  }
}
#if WEBKIT_CHECK_VERSION(1, 8, 0) 
static void 
view_document_finished(WebKitWebView *wv, WebKitWebFrame *frame, GList *gl) {
  if (EMIT_SCRIPT(DOCUMENT_LOADED)) {
    ScriptSignal signal = { SCRIPTS_WV(gl), { G_OBJECT(frame) }, SCRIPTS_SIG_META(NULL, DOCUMENT_LOADED, 1) };
    scripts_emit(&signal);
  }
}
#endif

/* view_button_press_cb(WebKitWebView *web, GdkEventButton *button, GList *gl) {{{*/
static void 
view_disconnect_caret(WebKitWebView *web) {
  if (_sig_caret_button_release > 0) {
    g_signal_handler_disconnect(web, _sig_caret_button_release);
    _sig_caret_button_release = 0;
  }
  if (_sig_caret_motion > 0) {
    g_signal_handler_disconnect(web, _sig_caret_motion);
    _sig_caret_motion = 0;
  }
}
void 
view_link_select(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other) {
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(web);
  WebKitDOMRange *range = webkit_dom_document_caret_range_from_point(doc, e->x, e->y);
  WebKitDOMDOMSelection *selection = webkit_dom_dom_window_get_selection(webkit_dom_document_get_default_view(doc));

  int start, end;
  start = webkit_dom_range_get_start_offset(other, NULL);
  end = webkit_dom_range_get_end_offset(range, NULL);
  WebKitDOMNode *node = webkit_dom_range_get_start_container(other, NULL);
  WebKitDOMNode *thisnode = webkit_dom_range_get_start_container(range, NULL);
  if (start < end) 
    webkit_dom_dom_selection_set_base_and_extent(selection, node, start, thisnode, end, NULL);
  else 
    webkit_dom_dom_selection_set_base_and_extent(selection, thisnode, end, node, start, NULL);
}
static gboolean
view_caret_motion_cb(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other) {
  view_link_select(web, e, other);
  return false;
}
static gboolean
view_caret_release_cb(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other) {
  view_link_select(web, e, other);
  view_disconnect_caret(web);
  return false;
}
static gboolean
view_button_press_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  WebKitHitTestResult *result = webkit_web_view_get_hit_test_result(web, e);
  WebKitHitTestResultContext context;
  g_object_get(result, "context", &context, NULL);
  gboolean ret = false;
  _click_time = e->time;
  if (EMIT_SCRIPT(BUTTON_PRESS)) {
    char *json = util_create_json(8, 
      UINTEGER, "time", e->time, UINTEGER,    "type", e->type, 
      DOUBLE,   "x", e->x, DOUBLE,            "y", e->y, 
      UINTEGER, "state", e->state, UINTEGER,  "button", e->button, 
      DOUBLE,   "xRoot", e->x_root, DOUBLE,   "yRoot", e->y_root);
    ScriptSignal signal = { SCRIPTS_WV(gl), { G_OBJECT(result) }, SCRIPTS_SIG_META(json, BUTTON_PRESS, 1) };
    SCRIPTS_EMIT_RETURN(signal, json);
  }

  if (gtk_widget_has_focus(dwb.gui.entry)) {
    dwb_focus_scroll(gl);
    CLEAR_COMMAND_TEXT();
  }
  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) {
    dwb_change_mode(INSERT_MODE);
    return false;
  }
  else if (e->state & GDK_CONTROL_MASK && e->button == 1) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(web);
    WebKitDOMRange *range = webkit_dom_document_caret_range_from_point(doc, e->x, e->y);
    WebKitDOMDOMSelection *selection = webkit_dom_dom_window_get_selection(webkit_dom_document_get_default_view(doc));
    webkit_dom_dom_selection_remove_all_ranges(selection);
    webkit_dom_dom_selection_add_range(selection, range);
    _sig_caret_button_release = g_signal_connect(web, "button-release-event", G_CALLBACK(view_caret_release_cb), range);
    _sig_caret_motion = g_signal_connect(web, "motion-notify-event", G_CALLBACK(view_caret_motion_cb), range);
    ret = true; 
  }
  else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION && e->type == GDK_BUTTON_PRESS && e->state & GDK_BUTTON1_MASK) {
    char *clipboard = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
    g_strstrip(clipboard);
    if (e->button == 3) {
      dwb_load_uri(NULL, clipboard);
      ret = true;
    }
    else if (e->button == 2) {
      view_add(clipboard, dwb.state.background_tabs);
      ret = true;
    }
    g_free(clipboard);
  }
  else if (e->button == 1 && e->type == GDK_BUTTON_PRESS && WEBVIEW(gl) != CURRENT_WEBVIEW()) {
    dwb_unfocus();
    dwb_focus(gl);
  }
  else if (e->button == 3 && e->state & GDK_BUTTON1_MASK) {
    /* no popup if button 1 is presssed */
    ret = true;
  }
  else if (e->button == 8) {
    dwb_history_back();
  }
  else if (e->button == 9) {
    dwb_history_forward();
  }
  return ret;
}/*}}}*/

/* view_button_press_cb {{{*/
static gboolean
view_button_release_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  char *uri =  NULL;
  WebKitHitTestResultContext context;

  WebKitHitTestResult *result = webkit_web_view_get_hit_test_result(web, e);
  g_object_get(result, "context", &context, NULL);

  if (EMIT_SCRIPT(BUTTON_RELEASE)) {
    char *json = util_create_json(7, 
      UINTEGER, "time", e->time, 
      DOUBLE,   "x", e->x, DOUBLE,            "y", e->y, 
      UINTEGER, "state", e->state, UINTEGER,  "button", e->button, 
      DOUBLE,   "xRoot", e->x_root, DOUBLE,   "yRoot", e->y_root);
    ScriptSignal signal = { SCRIPTS_WV(gl), { G_OBJECT(result) }, SCRIPTS_SIG_META(json, BUTTON_RELEASE, 1) };
    SCRIPTS_EMIT_RETURN(signal, json);
  }

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    if (e->button == 2 || (e->time - _click_time < 200 && (e->button == 1 && e->state & GDK_CONTROL_MASK))) {
      g_object_get(result, "link-uri", &uri, NULL);
      view_add(uri, dwb.state.background_tabs);
      view_disconnect_caret(web);
      return true;
    }
  }
  return false;
}/*}}}*/

/* view_close_web_view_cb(WebKitWebView *web, GList *gl) {{{*/
static gboolean 
view_close_web_view_cb(WebKitWebView *web, GList *gl) {
  view_remove(gl);
  return true;
}/*}}}*/

/* view_frame_committed_cb {{{*/
static void 
view_frame_committed_cb(WebKitWebFrame *frame, GList *gl) {
  if (EMIT_SCRIPT(FRAME_STATUS)) {
    ScriptSignal signal = { SCRIPTS_WV(gl), 
      .objects = { G_OBJECT(frame) }, SCRIPTS_SIG_META(NULL, FRAME_STATUS, 1) };
    scripts_emit(&signal);
  }
}/*}}}*/

/* view_frame_created_cb {{{*/
static void 
view_frame_created_cb(WebKitWebView *wv, WebKitWebFrame *frame, GList *gl) {
  if (EMIT_SCRIPT(FRAME_CREATED)) {
    ScriptSignal signal = { SCRIPTS_WV(gl), .objects = { G_OBJECT(frame) }, SCRIPTS_SIG_META(NULL, FRAME_CREATED, 1) };
    scripts_emit(&signal);
  }
  g_signal_connect(frame, "notify::load-status", G_CALLBACK(view_frame_committed_cb), gl);
  VIEW(gl)->status->frames = g_slist_prepend(VIEW(gl)->status->frames, frame);
}/*}}}*/

/* view_console_message_cb(WebKitWebView *web, char *message, int line, char *sourceid, GList *gl) {{{*/
static gboolean 
view_console_message_cb(WebKitWebView *web, char *message, int line, char *sourceid, GList *gl) {
  return true;
}/*}}}*/

/* view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *) {{{*/
static WebKitWebView * 
view_create_web_view_cb(WebKitWebView *web, WebKitWebFrame *frame, GList *gl) {
  if (dwb.misc.tabbed_browsing) {
    GList *gl = view_add(NULL, dwb.state.background_tabs); 
    return WEBVIEW(gl);
  }
  else {
    dwb.state.nv = OPEN_NEW_WINDOW;
    return web;
  }
}/*}}}*/

/* view_download_requested_cb(WebKitWebView *, WebKitDownload *, GList *) {{{*/
static gboolean 
view_download_requested_cb(WebKitWebView *web, WebKitDownload *download, GList *gl) {
  if (EMIT_SCRIPT(DOWNLOAD)) {
    char *json = util_create_json(2, 
        CHAR, "referer", soup_get_header_from_request(webkit_download_get_network_request(download), "Referer"), 
        CHAR, "mimeType", dwb.state.mimetype_request);
    ScriptSignal signal = { SCRIPTS_WV(gl), .objects = { G_OBJECT(download) }, SCRIPTS_SIG_META(json, DOWNLOAD, 1) };
    SCRIPTS_EMIT_RETURN(signal, json);
  }
  download_get_path(gl, download);
  return true;
}/*}}}*/

gboolean 
view_delete_web_inspector(GtkWidget *widget, GdkEvent *event, WebKitWebInspector *inspector) {
  webkit_web_inspector_close(inspector);
  for (GList *gl = dwb.state.views; gl; gl=gl->next) {
    if (VIEW(gl)->inspector_window == widget) {
      VIEW(gl)->inspector_window = NULL; 
      break;
    }
  }
  gtk_widget_destroy(widget);

  return true;
}

/* view_inspect_web_view_cb(WebKitWebInspector *, WebKitWebView *, GList * *){{{*/
static WebKitWebView * 
view_inspect_web_view_cb(WebKitWebInspector *inspector, WebKitWebView *wv, GList *gl) {
  GtkWidget *window;
  if (dwb.gui.wid == 0) {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  }
  else
    window = gtk_plug_new(dwb.gui.wid);
  GtkWidget *webview = webkit_web_view_new();
  WebKitWebSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));
  g_object_set(settings, "user-stylesheet-uri", GET_CHAR("user-stylesheet-uri"), NULL);
  gtk_window_set_title(GTK_WINDOW(window), "dwb-web-inspector");

  gtk_window_set_default_size(GTK_WINDOW(window), GET_INT("default-width"), GET_INT("default-height"));
  gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, NULL, GDK_HINT_MIN_SIZE);
  
  gtk_container_add(GTK_CONTAINER(window), webview);
  gtk_widget_show_all(window);
  VIEW(gl)->inspector_window = window;

  g_signal_connect(window, "delete-event", G_CALLBACK(view_delete_web_inspector), inspector);
  return WEBKIT_WEB_VIEW(webview);
}/*}}}*/

/* view_hovering_over_link_cb(WebKitWebView *, char *title, char *uri, GList *) {{{*/
static void 
view_hovering_over_link_cb(WebKitWebView *web, char *title, char *uri, GList *gl) {
  if (EMIT_SCRIPT(HOVERING_OVER_LINK)) {
    char *json = util_create_json(2, CHAR, "uri", uri, CHAR, "title", title);
    ScriptSignal signal = { SCRIPTS_WV(gl), SCRIPTS_SIG_META(json, HOVERING_OVER_LINK, 0) };
    scripts_emit(&signal);
    g_free(json);
  }
  if (uri) {
    VIEW(gl)->status->hover_uri = g_strdup(uri);
    dwb_set_status_bar_text(dwb.gui.urilabel, uri, &dwb.color.active_fg, NULL, false);
    if (! (dwb.state.bar_visible & BAR_VIS_STATUS)) {
      WebKitDOMDocument *doc = webkit_web_view_get_dom_document(web);
      WebKitDOMElement *docelement = webkit_dom_document_get_document_element(doc);
      webkit_dom_node_append_child(WEBKIT_DOM_NODE(docelement), WEBKIT_DOM_NODE(VIEW(gl)->hover.element), NULL);
      webkit_dom_html_anchor_element_set_href(WEBKIT_DOM_HTML_ANCHOR_ELEMENT(VIEW(gl)->hover.anchor), uri);
      webkit_dom_html_element_set_inner_text(WEBKIT_DOM_HTML_ELEMENT(VIEW(gl)->hover.anchor), uri, NULL);
    }
  }
  else {
    g_free(VIEW(gl)->status->hover_uri);
    VIEW(gl)->status->hover_uri = NULL;
    dwb_update_uri(gl);
    if (! (dwb.state.bar_visible & BAR_VIS_STATUS)) 
      dwb_dom_remove_from_parent(WEBKIT_DOM_NODE(VIEW(gl)->hover.element), NULL);
  }
}/*}}}*/

/* view_mime_type_policy_cb {{{*/
static gboolean 
view_mime_type_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, char *mimetype, WebKitWebPolicyDecision *policy, GList *gl) {
  /* Prevents from segfault if proxy is set */
  if ( !mimetype || strlen(mimetype) == 0 )
    return false;

  if (EMIT_SCRIPT(MIME_TYPE)) {
    char *json = util_create_json(1, CHAR, "mimeType", mimetype);
    ScriptSignal signal = { SCRIPTS_WV(gl), { G_OBJECT(frame), G_OBJECT(request) }, SCRIPTS_SIG_META(json, MIME_TYPE, 2) };
    if (scripts_emit(&signal)) {
      webkit_web_policy_decision_ignore(policy);
      g_free(json);
      return true;
    }
    g_free(json);
  }

  if (!webkit_web_view_can_show_mime_type(web, mimetype) ||  dwb.state.nv & OPEN_DOWNLOAD) {
    dwb.state.mimetype_request = g_strdup(mimetype);
    webkit_web_policy_decision_download(policy);
    return true;
  }
  return  false;
}/*}}}*/



/* view_navigation_policy_cb {{{*/
static gboolean 
view_navigation_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {

  char *uri = (char *) webkit_network_request_get_uri(request);
  gboolean ret = false;
  WebKitWebNavigationReason reason = webkit_web_navigation_action_get_reason(action);

  if (EMIT_SCRIPT(NAVIGATION)) {
    ScriptSignal signal = { SCRIPTS_WV(gl), { G_OBJECT(frame), G_OBJECT(request), G_OBJECT(action) }, SCRIPTS_SIG_META(NULL, NAVIGATION, 3) };
    if (scripts_emit(&signal)) {
      webkit_web_policy_decision_ignore(policy);
      return true;
    }
  }

  if (!g_str_has_prefix(uri, "http:") && !g_str_has_prefix(uri, "https:") &&
      !g_str_has_prefix(uri, "about:") && !g_str_has_prefix(uri, "dwb:") &&
      !g_str_has_prefix(uri, "file:")) {
    if (dwb_scheme_handler(gl, request) == STATUS_OK)  {
      webkit_web_policy_decision_ignore(policy);
      return true;
    }
  }

  /* Check if tab is locked */
  if (LP_LOCKED_URI(VIEW(gl))) {
    const char *initial_uri = webkit_web_view_get_uri(web);
    if (g_strcmp0(uri, initial_uri)) {
      dwb_set_error_message(dwb.state.fview, "Locked to domain %s, request aborted.", initial_uri);
      webkit_web_policy_decision_ignore(policy);
      return true;
    }

  }
  else if (LP_LOCKED_DOMAIN(VIEW(gl))) {
    const char *host, *initial_host;
    WebKitWebFrame *mainframe = webkit_web_view_get_main_frame(web);
    WebKitWebDataSource *datasource = webkit_web_frame_get_data_source(mainframe);
    WebKitNetworkRequest *initial_request = webkit_web_data_source_get_request(datasource);

    initial_host = dwb_soup_get_host_from_request(initial_request);
    host         = dwb_soup_get_host_from_request(request);

    if (g_strcmp0(initial_host, host)) {
      dwb_set_error_message(dwb.state.fview, "Locked to domain %s, request aborted.", initial_host);
      webkit_web_policy_decision_ignore(policy);
      return true;
    }
  }
  if (g_str_has_prefix(uri, "dwb:")) {
    if (!html_load(gl, uri)) {
      fprintf(stderr, "Error loadings %s, maybe some files are missing.\n", uri);
    }
    webkit_web_policy_decision_ignore(policy);
    return true;
  }
  if (webkit_web_navigation_action_get_button(action) == 2) {
    gboolean background = dwb.state.nv & OPEN_BACKGROUND;
    dwb.state.nv = OPEN_NORMAL;
    webkit_web_policy_decision_ignore(policy);
    view_add(uri, background);
    return true;
  }
  /* In single mode (without tabs), this seems to be the only way of creating a
   * new window
   */
  if (dwb.state.nv & OPEN_NEW_WINDOW) {
    if ((dwb.state.nv & OPEN_VIA_HINTS) || (dwb.state.nv & OPEN_EXPLICIT && reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED)) {
      dwb_change_mode(NORMAL_MODE, true);
      dwb_new_window(uri);
      webkit_web_policy_decision_ignore(policy);
      return true;
    }
  }
  GError *error = NULL;

  switch (reason) {
    /* special handler for filesystem browsing */
    case WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED:
    case WEBKIT_WEB_NAVIGATION_REASON_BACK_FORWARD:
    case WEBKIT_WEB_NAVIGATION_REASON_RELOAD:
        if (local_check_directory(gl, uri, reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED,  &error)) {
          if (error != NULL) {
            dwb_set_error_message(gl, error->message);
            g_clear_error(&error);
          }
          webkit_web_policy_decision_ignore(policy);
          return true;
        }
        dwb_soup_clean();
    case WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED: 
        if (dwb.state.mode == SEARCH_FIELD_MODE) {
          webkit_web_policy_decision_ignore(policy);
          char *hint_search_submit = GET_CHAR("searchengine-submit-pattern");
          if (hint_search_submit == NULL) {
              hint_search_submit = HINT_SEARCH_SUBMIT;
          }
          char *tmp_uri = g_uri_unescape_string(uri, NULL);
          char *search_engine = dwb.state.form_name && !g_strrstr(tmp_uri, hint_search_submit) 
            ? g_strdup_printf("%s?%s=%s", tmp_uri, dwb.state.form_name, hint_search_submit) 
            : g_strdup(tmp_uri);
          dwb_save_searchengine(search_engine);
          webkit_web_policy_decision_ignore(policy);
          return true;
        }
    default: break;

  }
  return ret;
}/*}}}*/

/* view_new_window_policy_cb {{{*/
static gboolean 
view_new_window_policy_cb(WebKitWebView *web, WebKitWebFrame *frame,
    WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *decision, GList *gl) {
  return false;
}/*}}}*/

/* view_create_plugin_widget_cb {{{*/
static GtkWidget * 
view_create_plugin_widget_cb(WebKitWebView *web, char *mime_type, char *uri, GHashTable *param, GList *gl) {
  VIEW(gl)->plugins->status |= PLUGIN_STATUS_HAS_PLUGIN;
#if 0
  GHashTableIter iter;
  g_hash_table_iter_init(&iter, param);
  char *key, *value;
  while(g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer*)&value)) {
    printf("%s %s\n", key, value);
  }
#endif
  return NULL;
}/*}}}*/

/* view_scroll_cb(GtkWidget *w, GdkEventScroll * GList *) {{{*/
static gboolean
view_scroll_cb(GtkWidget *w, GdkEventScroll *e, GList *gl) {
  dwb_update_status_text(gl, NULL);
  if (GET_BOOL("enable-frame-flattening")) {
    Arg a = { .n = e->direction };
    commands_scroll(NULL, &a);
    return true;
  }
  return false;
}/*}}}*/

/* view_value_changed_cb(GtkAdjustment *a, GList *gl) {{{ */
static gboolean
view_value_changed_cb(GtkAdjustment *a, GList *gl) {
  dwb_update_status_text(gl, a);
  return false;
}/* }}} */

/* view_icon_loaded(GtkAdjustment *a, GList *gl) {{{ */
void 
view_icon_loaded(WebKitWebView *web, char *icon_uri, GList *gl) {
  view_set_favicon(gl, true);
}/* }}} */

/* view_title_cb {{{*/
static void 
view_title_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  dwb_update_status(gl);
}/*}}}*/

/* view_title_cb {{{*/
static void 
view_uri_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  dwb_update_uri(gl);
}/*}}}*/

/* view_progress_cb {{{*/
static void 
view_progress_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  View *v = gl->data;
  v->status->progress = webkit_web_view_get_progress(web) * 100;
  if (v->status->progress == 100) {
    v->status->progress = 0;
  }
  dwb_update_status(gl);
}/*}}}*/

/* view_popup_activate_cb {{{*/
static void
view_popup_activate_cb(GtkMenuItem *menu, GList *gl) {
  GtkAction *a = NULL;
  const char *name;
  /* 
   * context-menu-action-2000       open link
   * context-menu-action-1          open link in window
   * context-menu-action-2          download linked file
   * context-menu-action-3          copy link location
   * context-menu-action-13         reload
   * context-menu-action-10         back
   * context-menu-action-11         forward
   * context-menu-action-12         stop
   * 
   * */

  a = gtk_activatable_get_related_action(GTK_ACTIVATABLE(menu));
  if (a == NULL) 
    return;
  name  = gtk_action_get_name(a);
  if (!g_strcmp0(name, "context-menu-action-3")) { /* copy link location */
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text(clipboard, VIEW(gl)->status->hover_uri, -1);
  }
}/*}}}*/

/* view_populate_popup_cb {{{*/
static void 
view_populate_popup_cb(WebKitWebView *web, GtkMenu *menu, GList *gl) {
  GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
  for (GList *l = items; l; l=l->next) {
    g_signal_connect(l->data, "activate", G_CALLBACK(view_popup_activate_cb), gl);
  }
  g_list_free(items);
}/*}}}*/

/* view_load_status_cb {{{*/
/* window-object-cleared is emmited in receivedFirstData which emits load-status
 * commited, so we don't connect to window-object-cleared but to
 * load_status_after instead */
static void 
view_load_status_after_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(web);
  if (status == WEBKIT_LOAD_COMMITTED) {
    dwb_execute_script(webkit_web_view_get_main_frame(web), dwb.misc.scripts, false);
    if (VIEW(gl)->hint_object != NULL) {
      JSValueUnprotect(JS_CONTEXT_REF(gl), VIEW(gl)->hint_object);
      VIEW(gl)->hint_object = NULL;
    }
    VIEW(gl)->hint_object = js_create_object(webkit_web_view_get_main_frame(web), dwb.misc.hints);
    js_call_as_function(webkit_web_view_get_main_frame(web), VIEW(gl)->hint_object, "init", dwb.misc.hint_style, NULL);
  }
}/*}}}*/
/* view_load_status_cb {{{*/

static void 
view_load_status_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(web);
  View *v = VIEW(gl);
  char *host =  NULL;
  const char *uri = webkit_web_view_get_uri(web);


  switch (status) {
    case WEBKIT_LOAD_PROVISIONAL: 
      dwb_clean_load_begin(gl);
      g_slist_free(v->status->frames);
      v->status->frames = NULL;
      v->status->frames = g_slist_prepend(v->status->frames, webkit_web_view_get_main_frame(WEBVIEW(gl)));
      break;
    case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT: 
      /* This is more or less a dummy call, to compile the script and speed up
       * execution time 
       * */
      js_call_as_function(webkit_web_view_get_main_frame(web), v->hint_object, "createStyleSheet", NULL, NULL);
      break;
    case WEBKIT_LOAD_COMMITTED: 
      if (v->status->scripts & SCRIPTS_ALLOWED_TEMPORARY) {
        g_object_set(webkit_web_view_get_settings(web), "enable-scripts", false, NULL);
        v->status->scripts &= ~SCRIPTS_ALLOWED_TEMPORARY;
      }
      if (v->plugins->status & PLUGIN_STATUS_ENABLED) 
        plugins_connect(gl);
      if (VIEW(gl)->status->scripts & SCRIPTS_BLOCKED 
          && (((host = dwb_get_host(web)) 
          && (g_list_find_custom(dwb.fc.pers_scripts, host, (GCompareFunc)g_strcmp0) || g_list_find_custom(dwb.fc.pers_scripts, uri, (GCompareFunc)g_strcmp0) 
              || g_list_find_custom(dwb.fc.tmp_scripts, host, (GCompareFunc)g_strcmp0) || g_list_find_custom(dwb.fc.tmp_scripts, uri, (GCompareFunc)g_strcmp0)))
          ||  g_str_has_prefix(uri, "dwb:") || !g_strcmp0(uri, "Error"))) {
        g_object_set(webkit_web_view_get_settings(web), "enable-scripts", true, NULL);
        v->status->scripts |= SCRIPTS_ALLOWED_TEMPORARY;
      }
      if (v->plugins->status & PLUGIN_STATUS_ENABLED 
          && ( (host != NULL || (host = dwb_get_host(web))) 
          && (g_list_find_custom(dwb.fc.pers_plugins, host, (GCompareFunc)g_strcmp0) || g_list_find_custom(dwb.fc.pers_plugins, uri, (GCompareFunc)g_strcmp0)
            || g_list_find_custom(dwb.fc.tmp_plugins, host, (GCompareFunc)g_strcmp0) || g_list_find_custom(dwb.fc.tmp_plugins, uri, (GCompareFunc)g_strcmp0) )
            )) {
        plugins_disconnect(gl);
      }
      if (EMIT_SCRIPT(LOAD_COMMITTED)) {
        ScriptSignal signal = { SCRIPTS_WV(gl), SCRIPTS_SIG_META(NULL, LOAD_COMMITTED, 0) };
        scripts_emit(&signal);
      }
      view_ssl_state(gl);
      g_free(host);
      break;
    case WEBKIT_LOAD_FINISHED:
      dwb_update_status(gl);
      /* TODO sqlite */
      if (!dwb.misc.private_browsing 
          && g_strcmp0(uri, "about:blank")
          && !g_str_has_prefix(uri, "dwb:") 
          && (dwb_prepend_navigation(gl, &dwb.fc.history) == STATUS_OK)
          && dwb.misc.sync_interval <= 0) {
        util_file_add_navigation(dwb.files[FILES_HISTORY], dwb.fc.history->data, false, dwb.misc.history_length);
      }
      dwb_execute_script(webkit_web_view_get_main_frame(web), dwb.misc.scripts_onload, false);
      if (dwb.state.auto_insert_mode) 
        dwb_check_auto_insert(gl);
      if (EMIT_SCRIPT(LOAD_FINISHED)) {
        ScriptSignal signal = { SCRIPTS_WV(gl), SCRIPTS_SIG_META(NULL, LOAD_FINISHED, 0) };
        scripts_emit(&signal);
      }
      break;
    case WEBKIT_LOAD_FAILED: 
      break;
    default:
      break;
  }
  if (EMIT_SCRIPT(LOAD_STATUS)) {
      ScriptSignal signal = { SCRIPTS_WV(gl), SCRIPTS_SIG_META(NULL, LOAD_STATUS, 0) };
      scripts_emit(&signal);
  }
}/*}}}*/

/* view_load_error_cb {{{*/
static gboolean 
view_load_error_cb(WebKitWebView *web, WebKitWebFrame *frame, char *uri, GError *weberror, GList *gl) {
  if (weberror->code == WEBKIT_NETWORK_ERROR_CANCELLED 
      || weberror->code == WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD 
      || weberror->code == WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE) 
    return false;

  char *errorfile = util_get_data_file(ERROR_FILE, "lib");
  if (errorfile == NULL) 
    return false;

  char *site, *search;
  char *content = util_get_file_content(errorfile, NULL);
  char *tmp = g_strdup(uri);
  char *res = tmp;
  if ( (tmp = strstr(tmp, "://")) != NULL) 
    tmp += 3;
  else 
    tmp = res;

  if (g_str_has_suffix(tmp, "/")) 
    tmp[strlen(tmp)-1] = '\0';
  
  char *icon = dwb_get_stock_item_base64_encoded(GTK_STOCK_DIALOG_ERROR);
  if ((search = dwb_get_searchengine(tmp)) != NULL)  
    site = g_strdup_printf(content, icon != NULL ? icon : "", uri, weberror->message, "visible", search);
  else 
    site = g_strdup_printf(content, icon != NULL ? icon : "", uri, weberror->message, "hidden", "");

  webkit_web_frame_load_alternate_string(frame, site, uri, uri);

  g_free(site);
  g_free(content);
  g_free(res);
  g_free(icon);
  g_free(search);
  return true;
}/*}}}*/

/* Entry */

/* dwb_entry_activate_cb (GtkWidget *entry) {{{*/
/*}}}*/

/* view_tab_button_press_cb(GtkWidget, GdkEventButton* , GList * ){{{*/
static gboolean
view_tab_button_press_cb(GtkWidget *tabevent, GdkEventButton *e, GList *gl) {
  if (e->button == 1 && e->type == GDK_BUTTON_PRESS) {
    dwb_focus_view(gl);
    return true;
  }
  else if (e->button == 3 && e->type == GDK_BUTTON_PRESS) {
    view_remove(gl);
    return true;
  }
  return false;
}/*}}}*/

/* DWB_WEB_VIEW {{{*/

void
view_set_favicon(GList *gl, gboolean web) {
  GdkPixbuf *pb = NULL, *old;
  GdkPixbuf *new = NULL;
  if ( (old = gtk_image_get_pixbuf(GTK_IMAGE(VIEW(gl)->tabicon))) ) {
    g_object_unref(old);
  }
  if (web) {
    pb = webkit_web_view_get_icon_pixbuf(WEBVIEW(gl));
    if (pb) {
      new = gdk_pixbuf_scale_simple(pb, dwb.misc.bar_height, dwb.misc.bar_height, GDK_INTERP_BILINEAR);
      g_object_unref(pb);
    }
  }
  gtk_image_set_from_pixbuf(GTK_IMAGE(VIEW(gl)->tabicon), new);
}

/* view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, int fontsize) {{{*/
void 
view_modify_style(GList  *gl, DwbColor *tabfg, DwbColor *tabbg, PangoFontDescription *fd) {
  View *v = VIEW(gl);
  DWB_WIDGET_OVERRIDE_BACKGROUND(v->tabevent, GTK_STATE_NORMAL, tabbg);
  DWB_WIDGET_OVERRIDE_COLOR(v->tablabel, GTK_STATE_NORMAL, tabfg);
  DWB_WIDGET_OVERRIDE_FONT(v->tablabel, fd);
} /*}}}*/

/* view_set_active_style (GList *) {{{*/
void 
view_set_active_style(GList *gl) {
  view_modify_style(gl, &dwb.color.tab_active_fg, &dwb.color.tab_active_bg, dwb.font.fd_active);
}/*}}}*/

/* view_set_normal_style {{{*/
void 
view_set_normal_style(GList *gl) {
  if (g_list_position(dwb.state.views, gl) % 2 == 0)
    view_modify_style(gl, &dwb.color.tab_normal_fg1, &dwb.color.tab_normal_bg1, dwb.font.fd_inactive);
  else
    view_modify_style(gl, &dwb.color.tab_normal_fg2, &dwb.color.tab_normal_bg2, dwb.font.fd_inactive);
}/*}}}*/

/* view_init_settings {{{*/
static void 
view_init_settings(GList *gl) {
  View *v = gl->data;
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(v->web), webkit_web_settings_copy(dwb.state.web_settings));
  GList *l;
  for (l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    if (s->apply & SETTING_PER_VIEW && s->func != NULL) {
      s->func(gl, s);
    }
  }
  if (l != NULL) 
    g_list_free(l);
}/*}}}*/

/* view_init_signals(View *v) {{{*/
static void
view_init_signals(GList *gl) {
  View *v = gl->data;
  GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  v->status->signals[SIG_BUTTON_PRESS]          = g_signal_connect(v->web, "button-press-event",                    G_CALLBACK(view_button_press_cb), gl);
  v->status->signals[SIG_BUTTON_RELEASE]        = g_signal_connect(v->web, "button-release-event",                  G_CALLBACK(view_button_release_cb), gl);
  v->status->signals[SIG_CLOSE_WEB_VIEW]        = g_signal_connect(v->web, "close-web-view",                        G_CALLBACK(view_close_web_view_cb), gl);
  v->status->signals[SIG_CONSOLE_MESSAGE]       = g_signal_connect(v->web, "console-message",                       G_CALLBACK(view_console_message_cb), gl);
  v->status->signals[SIG_CREATE_WEB_VIEW]       = g_signal_connect(v->web, "create-web-view",                       G_CALLBACK(view_create_web_view_cb), gl);
  v->status->signals[SIG_DOWNLOAD_REQUESTED]    = g_signal_connect(v->web, "download-requested",                    G_CALLBACK(view_download_requested_cb), gl);
  v->status->signals[SIG_HOVERING_OVER_LINK]    = g_signal_connect(v->web, "hovering-over-link",                    G_CALLBACK(view_hovering_over_link_cb), gl);
  v->status->signals[SIG_MIME_TYPE]             = g_signal_connect(v->web, "mime-type-policy-decision-requested",   G_CALLBACK(view_mime_type_policy_cb), gl);
  v->status->signals[SIG_NAVIGATION]            = g_signal_connect(v->web, "navigation-policy-decision-requested",  G_CALLBACK(view_navigation_policy_cb), gl);
  v->status->signals[SIG_NEW_WINDOW]            = g_signal_connect(v->web, "new-window-policy-decision-requested",  G_CALLBACK(view_new_window_policy_cb), gl);
  v->status->signals[SIG_RESOURCE_REQUEST]      = g_signal_connect(v->web, "resource-request-starting",             G_CALLBACK(view_resource_request_cb), gl);
#if WEBKIT_CHECK_VERSION(1, 8, 0)
  v->status->signals[SIG_DOCUMENT_FINISHED]     = g_signal_connect(v->web, "document-load-finished",             G_CALLBACK(view_document_finished), gl);
#endif
  v->status->signals[SIG_CREATE_PLUGIN_WIDGET]  = g_signal_connect(v->web, "create-plugin-widget",                  G_CALLBACK(view_create_plugin_widget_cb), gl);
  v->status->signals[SIG_FRAME_CREATED]         = g_signal_connect(v->web, "frame-created",                         G_CALLBACK(view_frame_created_cb), gl);

  v->status->signals[SIG_LOAD_STATUS]           = g_signal_connect(v->web, "notify::load-status",                   G_CALLBACK(view_load_status_cb), gl);
  v->status->signals[SIG_LOAD_ERROR]            = g_signal_connect(v->web, "load-error",                            G_CALLBACK(view_load_error_cb), gl);
  v->status->signals[SIG_LOAD_STATUS_AFTER]     = g_signal_connect_after(v->web, "notify::load-status",             G_CALLBACK(view_load_status_after_cb), gl);
  v->status->signals[SIG_POPULATE_POPUP]        = g_signal_connect(v->web, "populate-popup",                        G_CALLBACK(view_populate_popup_cb), gl);
  v->status->signals[SIG_PROGRESS]              = g_signal_connect(v->web, "notify::progress",                      G_CALLBACK(view_progress_cb), gl);
  v->status->signals[SIG_TITLE]                 = g_signal_connect(v->web, "notify::title",                         G_CALLBACK(view_title_cb), gl);
  v->status->signals[SIG_URI]                   = g_signal_connect(v->web, "notify::uri",                           G_CALLBACK(view_uri_cb), gl);
  v->status->signals[SIG_SCROLL]                = g_signal_connect(v->web, "scroll-event",                          G_CALLBACK(view_scroll_cb), gl);
  v->status->signals[SIG_VALUE_CHANGED]         = g_signal_connect(a,      "value-changed",                         G_CALLBACK(view_value_changed_cb), gl);
  if (GET_BOOL("enable-favicon")) 
    v->status->signals[SIG_ICON_LOADED]           = g_signal_connect(v->web, "icon-loaded",                           G_CALLBACK(view_icon_loaded), gl);

  /* v->status->signals[SIG_ENTRY_ACTIVATE]        = g_signal_connect(v->entry, "activate",                            G_CALLBACK(view_entry_activate_cb), gl); */

  v->status->signals[SIG_TAB_BUTTON_PRESS]      = g_signal_connect(v->tabevent, "button-press-event",               G_CALLBACK(view_tab_button_press_cb), gl);
  //WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  //v->status->signals[SIG_MAIN_FRAME_COMMITTED]  = g_signal_connect(frame, "load-committed", G_CALLBACK(view_main_frame_committed_cb), gl);

  /* WebInspector */
  WebKitWebInspector *inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(v->web));
  g_signal_connect(inspector, "inspect-web-view", G_CALLBACK(view_inspect_web_view_cb), gl);
} /*}}}*/

/* view_create_web_view(View *v)         return: GList * {{{*/
static View * 
view_create_web_view() {
  View *v = g_malloc(sizeof(View));

  ViewStatus *status = dwb_malloc(sizeof(ViewStatus));
  status->search_string = NULL;
  status->downloads = NULL;
  status->hover_uri = NULL;
#ifdef WITH_LIBSOUP_2_38
  status->request_uri = NULL;
#endif
  status->progress = 0;
  status->allowed_plugins = NULL;
  status->style = NULL;
  status->lockprotect = 0;
  status->frames = NULL;

  v->hint_object = NULL;
  v->inspector_window = NULL;
  v->plugins = plugins_new();
  for (int i=0; i<SIG_LAST; i++) 
    status->signals[i] = 0;
  v->status = status;

  v->web = webkit_web_view_new();

  /* Srolling */
  v->scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(v->scroll), v->web);

#if !_HAS_GTK3
  if (! GET_BOOL("scrollbars")) {
    WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
    g_signal_connect(frame, "scrollbars-policy-changed", G_CALLBACK(gtk_true), NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(v->scroll), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  }
#endif


  /* Tabbar */
  v->tabevent = gtk_event_box_new();
#if _HAS_GTK3
  v->tabbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
#else 
  v->tabbox = gtk_hbox_new(false, 1);
#endif

  v->tablabel = gtk_label_new(NULL);

  gtk_label_set_use_markup(GTK_LABEL(v->tablabel), true);
  gtk_label_set_width_chars(GTK_LABEL(v->tablabel), 1);
  gtk_misc_set_alignment(GTK_MISC(v->tablabel), 0.0, 0.5);
  gtk_label_set_ellipsize(GTK_LABEL(v->tablabel), PANGO_ELLIPSIZE_END);

  gtk_box_pack_end(GTK_BOX(v->tabbox), v->tablabel, true, true, 3);

  GdkPixbuf *pb = gdk_pixbuf_new_from_xpm_data(dummy_icon);
  v->tabicon = gtk_image_new_from_pixbuf(pb);
  gtk_box_pack_end(GTK_BOX(v->tabbox), v->tabicon, false, false, 0);

  GtkWidget *alignment = gtk_alignment_new(0.5, 0.5, 1, 1);
  int padding = GET_INT("bars-padding");
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), padding, padding, padding, padding);
  gtk_container_add(GTK_CONTAINER(alignment), v->tabbox);
  gtk_container_add(GTK_CONTAINER(v->tabevent), alignment);

  DWB_WIDGET_OVERRIDE_FONT(v->tablabel, dwb.font.fd_inactive);

  gtk_widget_set_can_focus(v->scroll, false);
  gtk_widget_show_all(v->scroll);
  gtk_widget_show_all(v->tabevent);

  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(v->web));
  v->hover.element = webkit_dom_document_create_element(doc, "div", NULL);
  v->hover.anchor = webkit_dom_document_create_element(doc, "a", NULL);
  char *font = GET_CHAR("font-hidden-statusbar");
  char *bgcolor = GET_CHAR("background-color");
  char *fgcolor = GET_CHAR("foreground-color");
  gchar *style = g_strdup_printf(
      "bottom:0px;right:0px;position:fixed;z-index:1000;\
      text-overflow:ellipsis;white-space:nowrap;overflow:hidden;max-width:100%%;\
      border-left:1px solid #555;\
      border-top:1px solid #555;\
      padding-left:2px;\
      border-radius:5px 0px 0px 0px;letter-spacing:0px;background:%s;color:%s;font:%s", 
      bgcolor, 
      fgcolor, 
      font);
  webkit_dom_element_set_attribute(v->hover.element, "style", style, NULL);
  g_free(style);
  webkit_dom_element_set_attribute(v->hover.anchor, "style", "text-decoration:none;color:inherit;", NULL);
  webkit_dom_node_append_child(WEBKIT_DOM_NODE(v->hover.element), WEBKIT_DOM_NODE(v->hover.anchor), NULL);
  webkit_dom_html_element_set_id(WEBKIT_DOM_HTML_ELEMENT(v->hover.element), "dwb_hover_element");

  v->status_element = webkit_dom_document_create_element(doc, "div", NULL);
  style = g_strdup_printf(
      "bottom:0px;left:0px;position:fixed;z-index:1000;\
      border-right:1px solid #555;\
      border-top:1px solid #555;\
      padding-right:2px;\
      border-radius:0px 5px 0px 0px;letter-spacing:0px;background:%s;color:%s;font:%s;", 
      bgcolor, 
      fgcolor, 
      font);
  webkit_dom_element_set_attribute(v->status_element, "style", style, NULL);
  g_free(style);
  return v;
} /*}}}*/

/* view_clear_tab {{{*/
void 
view_clear_tab(GList *gl) {
  WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(WEBVIEW(gl));
  webkit_web_back_forward_list_clear(bf_list);
  dwb_load_uri(gl, "about:blank");
}/*}}}*/

void
view_clean(GList *gl) {
  View *v = VIEW(gl);
  /* Inspector */
  if (v->inspector_window != NULL) {
    gtk_widget_destroy(v->inspector_window);
    v->inspector_window = NULL;
  }

  /* Favicon */ 
  GdkPixbuf *pb;
  if ( (pb = gtk_image_get_pixbuf(GTK_IMAGE(v->tabicon))) ) 
    g_object_unref(pb);

  gtk_widget_destroy(v->tabicon);
  gtk_widget_destroy(v->tablabel);
  gtk_widget_destroy(v->tabbox);
  gtk_widget_destroy(v->tabevent);

  plugins_free(v->plugins);

  scripts_remove_tab(v->script_wv);

  if (v->status->style) {
    g_object_unref(v->status->style);
  }

  g_object_unref(v->hover.anchor);
  g_object_unref(v->hover.element);
  g_object_unref(v->status_element);


  /* Destroy widget */
  gtk_widget_destroy(v->web);
  gtk_widget_destroy(v->scroll);

  FREE0(v->status->hover_uri);
#ifdef WITH_LIBSOUP_2_38
  FREE0(v->status->request_uri);
#endif
  FREE0(v->status);

  FREE0(v);
}
/* view_remove (void) {{{*/
DwbStatus 
view_remove(GList *gl) {
  if (!dwb.state.views->next) {
    if (dwb.misc.clt_policy == CLT_POLICY_CLOSE) {
      view_clear_tab(dwb.state.fview);
      dwb_end();
      return STATUS_END;
    }
    if (dwb.misc.clt_policy == CLT_POLICY_CLEAR) 
      view_clear_tab(dwb.state.fview);
    return STATUS_OK;
  }
  if (dwb.state.nummod >= 0) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod - 1);
    if (gl == NULL)
      return STATUS_OK;
  }
  else if (gl == NULL) 
    gl = dwb.state.fview;
  View *v = gl->data;
  int position = g_list_position(dwb.state.views, gl);
  /* Check for protected tab */
  if (LP_PROTECTED(v) && !dwb_confirm(dwb.state.fview, "Really close tab %d [y/n]?", position + 1) ) {
    CLEAR_COMMAND_TEXT();
    return STATUS_OK;
  }
  /* Get new focused tab */
  GList *new_fview = dwb.state.fview;
  if (gl == dwb.state.fview) {
    if (dwb.state.last_tab > position) 
      dwb.state.last_tab--;
    if (dwb.misc.tab_position & TAB_CLOSE_POSITION_RIGHTMOST) {
      if (dwb.state.fview->next) {
        new_fview = g_list_last(dwb.state.views);
      }
      else {
        new_fview = g_list_last(dwb.state.views)->prev;
      }
    }
    else if (dwb.misc.tab_position & TAB_CLOSE_POSITION_RIGHT) {
      if ( !(new_fview = dwb.state.fview->next) ) {
        new_fview = g_list_last(dwb.state.views)->prev;
      }
    }
    else if (dwb.misc.tab_position & TAB_CLOSE_POSITION_LEFTMOST) {
      if (dwb.state.fview == dwb.state.views) 
        new_fview = dwb.state.views->next;
      else 
        new_fview = dwb.state.views;
    }
    else { // tab.misc.tab_position & TAB_CLOSE_POSITION_LEFT
      if ( !(new_fview = dwb.state.fview->prev) ) {
        new_fview = g_list_first(dwb.state.views)->next;
      }
    }
    if (dwb.state.bar_visible & BAR_VIS_TOP && !gtk_widget_get_visible(dwb.gui.topbox)) 
      gtk_widget_show_all(dwb.gui.topbox);
  }
  else if (dwb.state.nummod - 1 == position) {
    dwb.state.last_tab = -1;
  }
  /* Get History for the undo list */
  WebKitWebBackForwardList *bflist = webkit_web_view_get_back_forward_list(WEBKIT_WEB_VIEW(v->web));
  if ( bflist != NULL ) {
    GList *store = NULL;

    for (int i = -webkit_web_back_forward_list_get_back_length(bflist); i<=0; i++) {
      WebKitWebHistoryItem *item = webkit_web_back_forward_list_get_nth_item(bflist, i);
      Navigation *n = dwb_navigation_from_webkit_history_item(item);
      if (n) 
        store = g_list_append(store, n);
    }
    dwb.state.undo_list = g_list_prepend(dwb.state.undo_list, store);
  }

  dwb_focus(new_fview);
  view_clean(gl);

  dwb_source_remove();

  dwb.state.views = g_list_delete_link(dwb.state.views, gl);

  gtk_widget_show(CURRENT_VIEW()->scroll);
  dwb_update_layout();
  CLEAR_COMMAND_TEXT();
  return STATUS_OK;
}/*}}}*/

/* view_ssl_state (GList *gl) {{{*/
static void
view_ssl_state(GList *gl) {
  View *v = VIEW(gl);
  SslState ssl = SSL_NONE;

  const char *uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(v->web));
  if (uri && g_str_has_prefix(uri, "https")) {
    SoupMessage *msg = dwb_soup_get_message(MAIN_FRAME_CAST(v->web));
    if (msg) {
      ssl = soup_message_get_flags(msg) & SOUP_MESSAGE_CERTIFICATE_TRUSTED 
        ? SSL_TRUSTED 
        : SSL_UNTRUSTED;
    }
  }
  v->status->ssl = ssl;
  dwb_update_uri(gl);
}/*}}}*/


/* view_add(Arg *arg)               return: View *{{{*/
GList *  
view_add(const char *uri, gboolean background) {
  GList *ret = NULL;

  if (!dwb.misc.tabbed_browsing && dwb.state.views) {
    dwb_new_window(uri);
    return NULL;
  }
  View *v = view_create_web_view();
  gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);
  if (dwb.state.fview) {
    int p;
    if (dwb.misc.tab_position & TAB_POSITION_RIGHTMOST) 
      p = g_list_length(dwb.state.views);
    else if (dwb.misc.tab_position & TAB_POSITION_LEFT) 
      p = g_list_position(dwb.state.views, dwb.state.fview);
    else if (dwb.misc.tab_position & TAB_POSITION_LEFTMOST) 
      p = 0;
    else 
      p = g_list_position(dwb.state.views, dwb.state.fview) + 1;

    gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), v->tabevent, g_list_length(dwb.state.views) - p);
    gtk_box_insert(GTK_BOX(dwb.gui.mainbox), v->scroll, true, true, 0, p, GTK_PACK_START);
    dwb.state.views = g_list_insert(dwb.state.views, v, p);
    ret = g_list_nth(dwb.state.views, p);

    if (background) {
      view_set_normal_style(ret);
      gtk_widget_hide(v->scroll);
    }
    else {
      if (! (CURRENT_VIEW()->status->lockprotect & LP_VISIBLE) )
        gtk_widget_hide(VIEW(dwb.state.fview)->scroll);
      dwb_unfocus();
      dwb_focus(ret);
    }
  }
  else {
    gtk_box_pack_start(GTK_BOX(dwb.gui.mainbox), v->scroll, true, true, 0);
    dwb.state.views = g_list_prepend(dwb.state.views, v);
    ret = dwb.state.views;
    dwb_focus(ret);
  }

  scripts_create_tab(ret);
  view_init_signals(ret);
  view_init_settings(ret);
  if (GET_BOOL("adblocker"))
    adblock_connect(ret);

  dwb_update_layout();

  if (uri != NULL) {
    dwb_load_uri(ret, uri);
  }
  return ret;
} /*}}}*/

/*}}}*/
