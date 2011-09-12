/*
 * Copyright (c) 2010-2011 Stefan Bolte <portix@gmx.net>
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

#include "view.h"
#include "html.h"
#include "plugins.h"

static void dwb_view_ssl_state(GList *);
#if WEBKIT_CHECK_VERSION(1, 4, 0)
static const char *dummy_icon[] = { "1 1 1 1 ", "  c black", " ", };
#endif


// CALLBACKS
static gboolean dwb_view_caret_release_cb(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other);
static gboolean dwb_view_caret_motion_cb(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other);
static gboolean dwb_web_view_button_press_cb(WebKitWebView *, GdkEventButton *, GList *);
static gboolean dwb_web_view_close_web_view_cb(WebKitWebView *, GList *);
static gboolean dwb_web_view_console_message_cb(WebKitWebView *, char *, int , char *, GList *);
static WebKitWebView * dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *);
static gboolean dwb_web_view_download_requested_cb(WebKitWebView *, WebKitDownload *, GList *);
static WebKitWebView * dwb_web_view_inspect_web_view_cb(WebKitWebInspector *, WebKitWebView *, GList *);
static void dwb_web_view_hovering_over_link_cb(WebKitWebView *, char *, char *, GList *);
static gboolean dwb_web_view_mime_type_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, char *, WebKitWebPolicyDecision *, GList *);
static gboolean dwb_web_view_navigation_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, WebKitWebNavigationAction *, WebKitWebPolicyDecision *, GList *);
static gboolean dwb_web_view_new_window_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, WebKitWebNavigationAction *, WebKitWebPolicyDecision *, GList *);
static void dwb_web_view_resource_request_cb(WebKitWebView *, WebKitWebFrame *, WebKitWebResource *, WebKitNetworkRequest *, WebKitNetworkResponse *, GList *);
static void dwb_web_view_window_object_cleared_cb(WebKitWebView *, WebKitWebFrame *, JSGlobalContextRef *, JSObjectRef *, GList *);
static gboolean dwb_web_view_scroll_cb(GtkWidget *, GdkEventScroll *, GList *);
static gboolean dwb_web_view_value_changed_cb(GtkAdjustment *, GList *);
static void dwb_web_view_title_cb(WebKitWebView *, GParamSpec *, GList *);
static void dwb_web_view_uri_cb(WebKitWebView *, GParamSpec *, GList *);
static void dwb_web_view_load_status_cb(WebKitWebView *, GParamSpec *, GList *);
static gboolean dwb_view_entry_keyrelease_cb(GtkWidget *, GdkEventKey *);
static gboolean dwb_view_entry_keypress_cb(GtkWidget *, GdkEventKey *);
static gboolean dwb_view_entry_activate_cb(GtkEntry *, GList *);
static gboolean dwb_view_tab_button_press_cb(GtkWidget *, GdkEventButton *, GList *);

/* WEB_VIEW_CALL_BACKS {{{*/

/* dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *button, GList *gl) {{{*/
void 
dwb_view_link_select(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other) {
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
dwb_view_caret_motion_cb(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other) {
  dwb_view_link_select(web, e, other);
  return false;
}
static gboolean
dwb_view_caret_release_cb(WebKitWebView *web, GdkEventButton *e, WebKitDOMRange *other) {
  dwb_view_link_select(web, e, other);
  g_signal_handlers_disconnect_by_func(web, dwb_view_caret_release_cb, other);
  g_signal_handlers_disconnect_by_func(web, dwb_view_caret_motion_cb, other);
  return false;
}
static gboolean
dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  WebKitHitTestResult *result = webkit_web_view_get_hit_test_result(web, e);
  WebKitHitTestResultContext context;
  g_object_get(result, "context", &context, NULL);
  gboolean ret = false;

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) {
    dwb_insert_mode(NULL);
  }
  else if (e->state & GDK_CONTROL_MASK && e->button == 1) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(web);
    WebKitDOMRange *range = webkit_dom_document_caret_range_from_point(doc, e->x, e->y);
    WebKitDOMDOMSelection *selection = webkit_dom_dom_window_get_selection(webkit_dom_document_get_default_view(doc));
    webkit_dom_dom_selection_remove_all_ranges(selection);
    webkit_dom_dom_selection_add_range(selection, range);
    g_signal_connect(web, "button-release-event", G_CALLBACK(dwb_view_caret_release_cb), range);
    g_signal_connect(web, "motion-notify-event", G_CALLBACK(dwb_view_caret_motion_cb), range);
    ret = true; 
  }
  else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION && e->type == GDK_BUTTON_PRESS && e->state & GDK_BUTTON1_MASK) {
    char *clipboard = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
    g_strstrip(clipboard);
    Arg a = { .p = clipboard, .b = true };
    if (e->button == 3) {
      dwb_load_uri(NULL, &a);
      ret = true;
    }
    else if (e->button == 2) {
      dwb_add_view(&a, dwb.state.background_tabs);
      ret = true;
    }
    FREE(clipboard);
  }
  else if (e->button == 1 && e->type == GDK_BUTTON_PRESS && WEBVIEW(gl) != CURRENT_WEBVIEW()) {
    dwb_unfocus();
    dwb_focus(gl);
  }
  else if (e->button == 3 && e->state & GDK_BUTTON1_MASK) {
    // no popup if button 1 is presssed
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
/* dwb_web_view_button_press_cb {{{*/
static gboolean
dwb_web_view_button_release_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  char *uri =  NULL;
  WebKitHitTestResultContext context;

  WebKitHitTestResult *result = webkit_web_view_get_hit_test_result(web, e);
  g_object_get(result, "context", &context, NULL);
  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    g_object_get(result, "link-uri", &uri, NULL);
    if (e->button == 2) {
      Arg a = { .p = uri, .b = true };
      dwb_add_view(&a, dwb.state.background_tabs);
      return true;
    }
  }
  return false;
}/*}}}*/

/* dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {{{*/
static gboolean 
dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {
  dwb_view_remove(gl);
  return true;
}/*}}}*/

/* dwb_web_view_console_message_cb(WebKitWebView *web, char *message, int line, char *sourceid, GList *gl) {{{*/
static gboolean 
dwb_web_view_console_message_cb(WebKitWebView *web, char *message, int line, char *sourceid, GList *gl) {
  if (gl == dwb.state.fview && !(strncmp(message, "_dwb_input_mode_", 16))) {
    dwb_insert_mode(NULL);
  }
  else if (gl == dwb.state.fview && !(strncmp(message, "_dwb_normal_mode_", 17))) {
    dwb_normal_mode(false);
  }
  if (!strncmp(message, "_dwb_no_input_", 14)) {
    dwb_set_error_message(gl, "No input found in current context");
  }
  return true;
}/*}}}*/

GtkWidget * 
dwb_web_view_create_plugin_widget_cb(WebKitWebView *web, char *mimetype, char *uri, GHashTable *param, GList *gl) {
  PRINT_DEBUG("mimetype: %s uri: %s", mimetype, uri);
  return NULL;
}

/* dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *) {{{*/
static WebKitWebView * 
dwb_web_view_create_web_view_cb(WebKitWebView *web, WebKitWebFrame *frame, GList *gl) {
  if (dwb.misc.tabbed_browsing) {
    // TODO background
    GList *gl = dwb_add_view(NULL, false); 
    return WEBVIEW(gl);
  }
  else {
    dwb.state.nv = OPEN_NEW_WINDOW;
    return web;
  }
}/*}}}*/

/* dwb_web_view_download_requested_cb(WebKitWebView *, WebKitDownload *, GList *) {{{*/
static gboolean 
dwb_web_view_download_requested_cb(WebKitWebView *web, WebKitDownload *download, GList *gl) {
  dwb_dl_get_path(gl, download);
  return true;
}/*}}}*/

/* dwb_web_view_inspect_web_view_cb(WebKitWebInspector *, WebKitWebView *, GList * *){{{*/
static WebKitWebView * 
dwb_web_view_inspect_web_view_cb(WebKitWebInspector *inspector, WebKitWebView *wv, GList *gl) {
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWidget *webview = webkit_web_view_new();
  
  gtk_container_add(GTK_CONTAINER(window), webview);
  gtk_widget_show_all(window);

  return WEBKIT_WEB_VIEW(webview);
}/*}}}*/

/* dwb_web_view_hovering_over_link_cb(WebKitWebView *, char *title, char *uri, GList *) {{{*/
static void 
dwb_web_view_hovering_over_link_cb(WebKitWebView *web, char *title, char *uri, GList *gl) {
  View *v = VIEW(gl);
  if (uri) {
    VIEW(gl)->status->hover_uri = g_strdup(uri);
    dwb_set_status_bar_text(v->urilabel, uri, &dwb.color.active_fg, NULL, false);
  }
  else {
    FREE(VIEW(gl)->status->hover_uri);
    VIEW(gl)->status->hover_uri = NULL;
    dwb_update_uri(gl);
  }

}/*}}}*/

/* dwb_web_view_mime_type_policy_cb {{{*/
static gboolean 
dwb_web_view_mime_type_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, char *mimetype, WebKitWebPolicyDecision *policy, GList *gl) {
  View *v = gl->data;

  /* Prevents from segfault if proxy is set */
  if ( !mimetype || strlen(mimetype) == 0 )
    return false;

  v->status->mimetype = g_strdup(mimetype);

  if (!webkit_web_view_can_show_mime_type(web, mimetype) ||  dwb.state.nv == OPEN_DOWNLOAD) {
    dwb.state.mimetype_request = g_strdup(mimetype);
    webkit_web_policy_decision_download(policy);
    return true;
  }
  return  false;
}/*}}}*/

/* dwb_web_view_navigation_policy_cb {{{*/
static gboolean 
dwb_web_view_navigation_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {

  dwb_clean_load_end(gl);
  char *uri = (char *) webkit_network_request_get_uri(request);
  gboolean ret = false;

  if (g_str_has_prefix(uri, "dwb://")) {
    dwb_html_load(gl, uri);
    return true;
  }
  Arg a = { .p = uri, .b = true };
  if (dwb.state.nv == OPEN_NEW_VIEW || dwb.state.nv == OPEN_NEW_WINDOW) {
    if (dwb.state.nv == OPEN_NEW_VIEW) {
      dwb.state.nv = OPEN_NORMAL;
      dwb_add_view(&a, dwb.state.background_tabs); 
    }
    else {
      dwb_new_window(&a);
    }
    dwb.state.nv = OPEN_NORMAL;
    return true;
  }
  WebKitWebNavigationReason reason = webkit_web_navigation_action_get_reason(action);
  const char *path;

  if (reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED && (path = dwb_check_directory(uri))) {
    dwb_load_uri(NULL, &a);
    webkit_web_policy_decision_ignore(policy);
    return true;
  }
  else if (reason == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED) {
    if (dwb.state.mode == INSERT_MODE) {
      dwb_normal_mode(true);
    }
    else if (dwb.state.mode == SEARCH_FIELD_MODE) {
      PRINT_DEBUG("searchfields navigation request: %s", uri);
      webkit_web_policy_decision_ignore(policy);
      dwb.state.search_engine = dwb.state.form_name && !g_strrstr(uri, HINT_SEARCH_SUBMIT) 
        ? g_strdup_printf("%s?%s=%s", uri, dwb.state.form_name, HINT_SEARCH_SUBMIT) 
        : g_strdup(uri);
      dwb_save_searchengine();
      return true;
    }
  }

  /* mailto, ftp */
  char *scheme = g_uri_parse_scheme(uri);
  if (scheme) {
    if (!strcmp(scheme, "mailto")) {
      dwb_spawn(gl, "mail-client", uri);
      webkit_web_policy_decision_ignore(policy);
      ret = true;
    }
    else if (!strcmp(scheme, "ftp")) {
      dwb_spawn(gl, "ftp-client", uri);
      webkit_web_policy_decision_ignore(policy);
      ret = true;
    }
    g_free(scheme);
  }
  return ret;
}/*}}}*/

/* dwb_web_view_new_window_policy_cb {{{*/
static gboolean 
dwb_web_view_new_window_policy_cb(WebKitWebView *web, WebKitWebFrame *frame,
    WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {
  return false;
}/*}}}*/

/* dwb_web_view_resource_request_cb{{{*/
static void 
dwb_web_view_resource_request_cb(WebKitWebView *web, WebKitWebFrame *frame,
    WebKitWebResource *resource, WebKitNetworkRequest *request,
    WebKitNetworkResponse *response, GList *gl) {
  if (dwb_block_ad(gl, webkit_network_request_get_uri(request))) {
    webkit_network_request_set_uri(request, "about:blank");
    return;
  }
}/*}}}*/

/* dwb_web_view_window_object_cleared_cb {{{*/
static void 
dwb_web_view_window_object_cleared_cb(WebKitWebView *web, WebKitWebFrame *frame, 
    JSGlobalContextRef *context, JSObjectRef *object, GList *gl) {
  // TODO possibly not needed
}/*}}}*/

/* dwb_web_view_scroll_cb(GtkWidget *w, GdkEventScroll * GList *) {{{*/
static gboolean
dwb_web_view_scroll_cb(GtkWidget *w, GdkEventScroll *e, GList *gl) {
  dwb_update_status_text(gl, NULL);
  if (GET_BOOL("enable-frame-flattening")) {
    Arg a = { .n = e->direction };
    dwb_com_scroll(NULL, &a);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_web_view_value_changed_cb(GtkAdjustment *a, GList *gl) {{{ */
static gboolean
dwb_web_view_value_changed_cb(GtkAdjustment *a, GList *gl) {
  dwb_update_status_text(gl, a);
  return false;
}/* }}} */

#if WEBKIT_CHECK_VERSION(1, 4, 0)
/* dwb_web_view_icon_loaded(GtkAdjustment *a, GList *gl) {{{ */
void 
dwb_web_view_icon_loaded(WebKitWebView *web, char *icon_uri, GList *gl) {
  View *v = VIEW(gl);
  GdkPixbuf *pb = webkit_web_view_get_icon_pixbuf(web);
  GdkPixbuf *old, *rescale;
  if (pb) {
    rescale = gdk_pixbuf_scale_simple(pb, dwb.misc.tab_height, dwb.misc.tab_height, GDK_INTERP_BILINEAR);
    if ( (old = gtk_image_get_pixbuf(GTK_IMAGE(v->tabicon))) ) 
      gdk_pixbuf_unref(old);
    gtk_image_set_from_pixbuf(GTK_IMAGE(v->tabicon), rescale);
  }
}/* }}} */
#endif

/* dwb_web_view_title_cb {{{*/
static void 
dwb_web_view_title_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  dwb_update_status(gl);
}/*}}}*/

/* dwb_web_view_title_cb {{{*/
static void 
dwb_web_view_uri_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  dwb_update_uri(gl);
}/*}}}*/

/* dwb_web_view_progress_cb {{{*/
static void 
dwb_web_view_progress_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  View *v = gl->data;
  v->status->progress = webkit_web_view_get_progress(web) * 100;
  if (v->status->progress == 100) {
    v->status->progress = 0;
  }
  dwb_update_status(gl);
}/*}}}*/

static void
dwb_view_popup_activate_cb(GtkMenuItem *menu, GList *gl) {
  GtkAction *a = NULL;
  const char *name;
  PRINT_DEBUG("hover_uri: %s", VIEW(gl)->status->hover_uri);
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
  PRINT_DEBUG("action name: %s", name);
  if (!strcmp(name, "context-menu-action-3")) { /* copy link location */
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text(clipboard, VIEW(gl)->status->hover_uri, -1);
  }
}

static void 
dwb_web_view_populate_popup_cb(WebKitWebView *web, GtkMenu *menu, GList *gl) {
  GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
  for (GList *l = items; l; l=l->next) {
    g_signal_connect(l->data, "activate", G_CALLBACK(dwb_view_popup_activate_cb), gl);
  }
}
// window-object-cleared is emmited in receivedFirstData which emits load-status
// commited, so we don't connect to window-object-cleared but to
// load_status_after instead
static void 
dwb_web_view_load_status_after_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(web);
  if (status == WEBKIT_LOAD_COMMITTED) {
    dwb_execute_script(webkit_web_view_get_main_frame(web), dwb.misc.scripts, false);
  }
}
/* dwb_web_view_load_status_cb {{{*/

static void 
dwb_web_view_load_status_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(web);
  View *v = VIEW(gl);
  char *host =  NULL;
  const char *uri = webkit_web_view_get_uri(web);

  switch (status) {
    case WEBKIT_LOAD_PROVISIONAL: 
      if (v->status->scripts & SCRIPTS_ALLOWED_TEMPORARY) {
        g_object_set(webkit_web_view_get_settings(web), "enable-scripts", false, NULL);
        v->status->scripts &= ~SCRIPTS_ALLOWED_TEMPORARY;
      }
      if (v->status->plugin_blocker && v->status->pb_status == PLUGIN_STATUS_DISCONNECTED)
        dwb_plugin_blocker_connect(gl);
      v->status->ssl = SSL_NONE;
      break;
    case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT: 
      break;
    case WEBKIT_LOAD_COMMITTED: 
      dwb_view_ssl_state(gl);
      if (VIEW(gl)->status->scripts & SCRIPTS_BLOCKED 
          && (((host = dwb_get_host(web)) 
          && (dwb_get_allowed(dwb.files.scripts_allow, host) || dwb_get_allowed(dwb.files.scripts_allow, uri) 
              || g_list_find_custom(dwb.fc.tmp_scripts, host, (GCompareFunc)strcmp) || g_list_find_custom(dwb.fc.tmp_scripts, uri, (GCompareFunc)strcmp)))
          || !strcmp(uri, "dwb://") || !strcmp(uri, "Error"))) {
        g_object_set(webkit_web_view_get_settings(web), "enable-scripts", true, NULL);
        v->status->scripts |= SCRIPTS_ALLOWED_TEMPORARY;
      }
      if (VIEW(gl)->status->plugin_blocker 
          && ( (host != NULL || (host = dwb_get_host(web))) 
          && (dwb_get_allowed(dwb.files.plugins_allow, host) || dwb_get_allowed(dwb.files.plugins_allow, uri)
            || g_list_find_custom(dwb.fc.tmp_plugins, host, (GCompareFunc)strcmp) || g_list_find_custom(dwb.fc.tmp_plugins, uri, (GCompareFunc)strcmp) )
            ))
        dwb_plugin_blocker_disconnect(gl);
      FREE(host);
      break;
    case WEBKIT_LOAD_FINISHED:
      dwb_update_status(gl);
      /* TODO sqlite */
      if (!dwb.misc.private_browsing && dwb_prepend_navigation(gl, &dwb.fc.history))
        dwb_util_file_add_navigation(dwb.files.history, dwb.fc.history->data, false, dwb.misc.history_length);
      break;
    case WEBKIT_LOAD_FAILED: 
      dwb_clean_load_end(gl);
      break;
    default:
      break;
  }
}/*}}}*/

/* dwb_web_view_load_error_cb {{{*/
static gboolean 
dwb_web_view_load_error_cb(WebKitWebView *web, WebKitWebFrame *frame, char *uri, GError *weberror, GList *gl) {
  if (weberror->code == WEBKIT_NETWORK_ERROR_CANCELLED 
      || weberror->code == WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD 
      || weberror->code == WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE) 
    return false;


  char *errorfile = dwb_util_get_data_file(ERROR_FILE);
  if (errorfile == NULL) 
    return false;

  char *site, *search;
  char *content = dwb_util_get_file_content(errorfile);
  char *tmp = g_strdup(uri);
  char *res = tmp;
  if ( (tmp = strstr(tmp, "://")) != NULL) 
    tmp += 3;
  else 
    tmp = res;

  if (g_str_has_suffix(tmp, "/")) 
    tmp[strlen(tmp)-1] = '\0';
  
  char *icon = dwb_get_stock_item_base64_encoded(GTK_STOCK_DIALOG_ERROR);
  if ((search = dwb_get_search_engine(tmp, true)) != NULL) 
    site = g_strdup_printf(content, icon != NULL ? icon : "", uri, weberror->message, "visible", search);
  else 
    site = g_strdup_printf(content, icon != NULL ? icon : "", uri, weberror->message, "hidden", "");

  webkit_web_frame_load_alternate_string(webkit_web_view_get_main_frame(web), site, "Error", uri);

  g_free(site);
  g_free(content);
  g_free(res);
  FREE(icon);
  FREE(search);
  return true;
}/*}}}*/

// Entry
/* dwb_entry_keyrelease_cb {{{*/
static gboolean 
dwb_view_entry_keyrelease_cb(GtkWidget* entry, GdkEventKey *e) { 
  if (dwb.state.mode == HINT_MODE) {
    if (e->keyval == GDK_KEY_BackSpace) {
      return dwb_update_hints(e);
    }
  }
  if (dwb.state.mode == FIND_MODE) {
    dwb_update_search(dwb.state.forward_search);
  }
  return false;
}/*}}}*/

/* dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {{{*/
static gboolean 
dwb_view_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {
  Mode mode = dwb.state.mode;
  gboolean ret = false;
  gboolean complete = (mode == DOWNLOAD_GET_PATH || mode & COMPLETE_PATH);
  if (e->keyval == GDK_KEY_BackSpace && !complete) {
    return false;
  }
  else if (mode == HINT_MODE) {
    return dwb_update_hints(e);
  }
  else if (mode == SEARCH_FIELD_MODE) {
    if (DWB_TAB_KEY(e)) {
      dwb_update_hints(e);
      return true;
    }
    else if (e->keyval == GDK_KEY_Return) {
      return false;
    }
  }
  else if (!e->is_modifier && complete) {
    if (DWB_TAB_KEY(e)) {
      dwb_comp_complete_path(e->state & GDK_SHIFT_MASK);
      return true;
    }
    else {
      dwb_comp_clean_path_completion();
    }
  }
  else if (mode & COMPLETION_MODE && !DWB_TAB_KEY(e) && !e->is_modifier && !CLEAN_STATE(e)) {
    dwb_comp_clean_completion();
  }
  else if (mode == FIND_MODE) {
    return false;
  }
  else if (DWB_TAB_KEY(e)) {
    dwb_comp_complete(dwb_eval_completion_type(), e->state & GDK_SHIFT_MASK);
    return true;
  }
  if (dwb_eval_editing_key(e)) {
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_entry_activate_cb (GtkWidget *entry) {{{*/
static gboolean 
dwb_view_entry_activate_cb(GtkEntry* entry, GList *gl) {
  Mode mode = dwb.state.mode;

  if (mode == HINT_MODE) {
    return false;
  }
  else if (mode == FIND_MODE) {
    dwb_focus_scroll(dwb.state.fview);
    dwb_search(NULL, NULL);
    dwb_normal_mode(true);
  }
  else if (mode == SEARCH_FIELD_MODE) {
    dwb_submit_searchengine();
  }
  else if (mode == SETTINGS_MODE || mode == KEY_MODE) {
    char **token = g_strsplit(GET_TEXT(), " ", 2);
    if  (mode == KEY_MODE) 
      dwb_set_key(token[0], token[1]);
    else 
      dwb_set_setting(token[0], token[1]);
    g_strfreev(token);
  }
  else if (mode == COMMAND_MODE) {
    dwb_parse_command_line(GET_TEXT());
  }
  else if (mode == DOWNLOAD_GET_PATH) {
    dwb_dl_start();
  }
  else if (mode == SAVE_SESSION) {
    dwb_session_save(GET_TEXT());
    dwb_end();
  }
  else {
    Arg a = { .n = 0, .p = (char*)GET_TEXT(), .b = true };
    dwb_load_uri(NULL, &a);
    dwb_prepend_navigation_with_argument(&dwb.fc.commands, a.p, NULL);
    dwb_normal_mode(true);
  }

  return true;
}/*}}}*/
/*}}}*/

/* dwb_view_tab_button_press_cb(GtkWidget, GdkEventButton* , GList * ){{{*/
static gboolean
dwb_view_tab_button_press_cb(GtkWidget *tabevent, GdkEventButton *e, GList *gl) {
  if (e->button == 1 && e->type == GDK_BUTTON_PRESS) {
    Arg a = { .p = gl };
    if ((dwb.state.layout & MAXIMIZED) ) {
      dwb_focus_view(gl);
    }
    else {
      dwb_unfocus();
      dwb_focus(gl);
      dwb_view_push_master(&a);
    }
    return true;
  }
  else if (e->button == 3 && e->type == GDK_BUTTON_PRESS) {
    dwb_view_remove(gl);
    return true;
  }
  return false;
}/*}}}*/

/* DWB_WEB_VIEW {{{*/

/* dwb_web_view_add_history_item(GList *gl) {{{*/
void 
dwb_web_view_add_history_item(GList *gl) {
  WebKitWebView *web = WEBVIEW(gl);
  const char *uri = webkit_web_view_get_uri(web);
  const char *title = webkit_web_view_get_title(web);
  WebKitWebBackForwardList *bl = webkit_web_view_get_back_forward_list(web);
  WebKitWebHistoryItem *hitem = webkit_web_history_item_new_with_data(uri,  title);
  webkit_web_back_forward_list_add_item(bl, hitem);
}/*}}}*/

/* dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, int fontsize) {{{*/
void 
dwb_view_modify_style(View *v, DwbColor *fg, DwbColor *bg, DwbColor *tabfg, DwbColor *tabbg, PangoFontDescription *fd) {
  if (bg) {
    DWB_WIDGET_OVERRIDE_BACKGROUND(v->statusbox, GTK_STATE_NORMAL, bg);
    DWB_WIDGET_OVERRIDE_BASE(v->entry, GTK_STATE_NORMAL, bg);
  }
  if (tabbg)
    DWB_WIDGET_OVERRIDE_BACKGROUND(v->tabevent, GTK_STATE_NORMAL, tabbg);
  if (fg) {
    DWB_WIDGET_OVERRIDE_COLOR(v->rstatus, GTK_STATE_NORMAL, fg);
    DWB_WIDGET_OVERRIDE_COLOR(v->lstatus, GTK_STATE_NORMAL, fg);
    DWB_WIDGET_OVERRIDE_TEXT(v->entry, GTK_STATE_NORMAL, fg);
  }
  if (tabfg) 
    DWB_WIDGET_OVERRIDE_COLOR(v->tablabel, GTK_STATE_NORMAL, tabfg);
  if (fd) {
    DWB_WIDGET_OVERRIDE_FONT(v->rstatus, fd);
    DWB_WIDGET_OVERRIDE_FONT(v->urilabel, fd);
    DWB_WIDGET_OVERRIDE_FONT(v->lstatus, fd);
    DWB_WIDGET_OVERRIDE_FONT(v->tablabel, fd);
  }
} /*}}}*/

/* dwb_view_set_active_style (GList *) {{{*/
void 
dwb_view_set_active_style(View *v) {
  dwb_view_modify_style(v, &dwb.color.active_fg, &dwb.color.active_bg, &dwb.color.tab_active_fg, &dwb.color.tab_active_bg, dwb.font.fd_active);
}/*}}}*/

/* dwb_view_set_normal_style {{{*/
void 
dwb_view_set_normal_style(View *v) {
  dwb_view_modify_style(v, &dwb.color.normal_fg, &dwb.color.normal_bg, &dwb.color.tab_normal_fg, &dwb.color.tab_normal_bg, dwb.font.fd_inactive);
}/*}}}*/

static void 
dwb_web_view_init_settings(GList *gl) {
  View *v = gl->data;
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(v->web), webkit_web_settings_copy(dwb.state.web_settings));
  // apply settings
  v->setting = dwb_get_default_settings();
  for (GList *l = g_hash_table_get_values(v->setting); l; l=l->next) {
    WebSettings *s = l->data;
    if (!s->builtin && !s->global) {
      s->func(gl, s);
    }
  }
}

/* dwb_web_view_init_signals(View *v) {{{*/
static void
dwb_web_view_init_signals(GList *gl) {
  View *v = gl->data;
  GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  v->status->signals[SIG_BUTTON_PRESS]          = g_signal_connect(v->web, "button-press-event",                    G_CALLBACK(dwb_web_view_button_press_cb), gl);
  v->status->signals[SIG_BUTTON_RELEASE]        = g_signal_connect(v->web, "button-release-event",                  G_CALLBACK(dwb_web_view_button_release_cb), gl);
  v->status->signals[SIG_CLOSE_WEB_VIEW]        = g_signal_connect(v->web, "close-web-view",                        G_CALLBACK(dwb_web_view_close_web_view_cb), gl);
  v->status->signals[SIG_CONSOLE_MESSAGE]       = g_signal_connect(v->web, "console-message",                       G_CALLBACK(dwb_web_view_console_message_cb), gl);
  v->status->signals[SIG_CREATE_WEB_VIEW]       = g_signal_connect(v->web, "create-web-view",                       G_CALLBACK(dwb_web_view_create_web_view_cb), gl);
  v->status->signals[SIG_DOWNLOAD_REQUESTED]    = g_signal_connect(v->web, "download-requested",                    G_CALLBACK(dwb_web_view_download_requested_cb), gl);
  v->status->signals[SIG_HOVERING_OVER_LINK]    = g_signal_connect(v->web, "hovering-over-link",                    G_CALLBACK(dwb_web_view_hovering_over_link_cb), gl);
  v->status->signals[SIG_MIME_TYPE]             = g_signal_connect(v->web, "mime-type-policy-decision-requested",   G_CALLBACK(dwb_web_view_mime_type_policy_cb), gl);
  v->status->signals[SIG_NAVIGATION]            = g_signal_connect(v->web, "navigation-policy-decision-requested",  G_CALLBACK(dwb_web_view_navigation_policy_cb), gl);
  v->status->signals[SIG_NEW_WINDOW]            = g_signal_connect(v->web, "new-window-policy-decision-requested",  G_CALLBACK(dwb_web_view_new_window_policy_cb), gl);
  v->status->signals[SIG_RESOURCE_REQUEST]      = g_signal_connect(v->web, "resource-request-starting",             G_CALLBACK(dwb_web_view_resource_request_cb), gl);
  v->status->signals[SIG_WINDOW_OBJECT]         = g_signal_connect(v->web, "window-object-cleared",                 G_CALLBACK(dwb_web_view_window_object_cleared_cb), gl);

  v->status->signals[SIG_LOAD_STATUS]           = g_signal_connect(v->web, "notify::load-status",                   G_CALLBACK(dwb_web_view_load_status_cb), gl);
  v->status->signals[SIG_LOAD_ERROR]            = g_signal_connect(v->web, "load-error",                            G_CALLBACK(dwb_web_view_load_error_cb), gl);
  v->status->signals[SIG_LOAD_STATUS_AFTER]     = g_signal_connect_after(v->web, "notify::load-status",             G_CALLBACK(dwb_web_view_load_status_after_cb), gl);
  v->status->signals[SIG_POPULATE_POPUP]        = g_signal_connect(v->web, "populate-popup",                        G_CALLBACK(dwb_web_view_populate_popup_cb), gl);
  v->status->signals[SIG_PROGRESS]              = g_signal_connect(v->web, "notify::progress",                      G_CALLBACK(dwb_web_view_progress_cb), gl);
  v->status->signals[SIG_TITLE]                 = g_signal_connect(v->web, "notify::title",                         G_CALLBACK(dwb_web_view_title_cb), gl);
  v->status->signals[SIG_URI]                   = g_signal_connect(v->web, "notify::uri",                           G_CALLBACK(dwb_web_view_uri_cb), gl);
  v->status->signals[SIG_SCROLL]                = g_signal_connect(v->web, "scroll-event",                          G_CALLBACK(dwb_web_view_scroll_cb), gl);
  v->status->signals[SIG_CREATE_PLUGIN]         = g_signal_connect(v->web,      "create-plugin-widget",                  G_CALLBACK(dwb_web_view_create_plugin_widget_cb), gl);
  v->status->signals[SIG_VALUE_CHANGED]         = g_signal_connect(a,      "value-changed",                         G_CALLBACK(dwb_web_view_value_changed_cb), gl);
#if WEBKIT_CHECK_VERSION(1, 4, 0)
  v->status->signals[SIG_ICON_LOADED]           = g_signal_connect(v->web, "icon-loaded",                           G_CALLBACK(dwb_web_view_icon_loaded), gl);
#endif

  v->status->signals[SIG_ENTRY_KEY_PRESS]       = g_signal_connect(v->entry, "key-press-event",                     G_CALLBACK(dwb_view_entry_keypress_cb), NULL);
  v->status->signals[SIG_ENTRY_KEY_RELEASE]     = g_signal_connect(v->entry, "key-release-event",                   G_CALLBACK(dwb_view_entry_keyrelease_cb), NULL);
  v->status->signals[SIG_ENTRY_ACTIVATE]        = g_signal_connect(v->entry, "activate",                            G_CALLBACK(dwb_view_entry_activate_cb), gl);

  v->status->signals[SIG_TAB_BUTTON_PRESS]      = g_signal_connect(v->tabevent, "button-press-event",               G_CALLBACK(dwb_view_tab_button_press_cb), gl);

  // WebInspector
  WebKitWebInspector *inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(v->web));
  g_signal_connect(inspector, "inspect-web-view", G_CALLBACK(dwb_web_view_inspect_web_view_cb), gl);
} /*}}}*/

void 
dwb_view_entry_size_allocate_cb(GtkWidget *entry, GdkRectangle *rect, View *v) {
  gtk_widget_set_size_request(v->entry, -1, rect->height);
  g_signal_handlers_disconnect_by_func(entry, dwb_view_entry_size_allocate_cb, v);
}


/* dwb_view_create_web_view(View *v)         return: GList * {{{*/
static View * 
dwb_view_create_web_view() {
  View *v = g_malloc(sizeof(View));

  ViewStatus *status = dwb_malloc(sizeof(ViewStatus));
  status->search_string = NULL;
  status->downloads = NULL;
  status->mimetype = NULL;
  status->hover_uri = NULL;
  status->progress = 0;
  status->allowed_plugins = NULL;
  status->pb_status = PLUGIN_STATUS_DISCONNECTED;


  for (int i=0; i<SIG_LAST; i++) 
    status->signals[i] = 0;
  v->status = status;

  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();

  // Entry
  v->entry = gtk_entry_new();
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), NULL);

  DWB_WIDGET_OVERRIDE_FONT(v->entry, dwb.font.fd_entry);
  gtk_entry_set_has_frame(GTK_ENTRY(v->entry), false);
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), false);

  // completion
  v->bottombox = gtk_vbox_new(false, 1);

  // Statusbox
  GtkWidget *status_hbox;
  v->statusbox = gtk_event_box_new();
  v->lstatus = gtk_label_new(NULL);
  v->urilabel = gtk_label_new(NULL);
  v->rstatus = gtk_label_new(NULL);

  gtk_misc_set_alignment(GTK_MISC(v->lstatus), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(v->urilabel), 1.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(v->rstatus), 1.0, 0.5);
  gtk_label_set_use_markup(GTK_LABEL(v->lstatus), true);
  gtk_label_set_use_markup(GTK_LABEL(v->urilabel), true);
  gtk_label_set_use_markup(GTK_LABEL(v->rstatus), true);
  gtk_label_set_ellipsize(GTK_LABEL(v->urilabel), PANGO_ELLIPSIZE_MIDDLE);

  DWB_WIDGET_OVERRIDE_COLOR(v->urilabel, GTK_STATE_NORMAL, &dwb.color.active_fg);

  status_hbox = gtk_hbox_new(false, 2);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->lstatus, false, false, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->entry, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->urilabel, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->rstatus, false, false, 0);
  gtk_container_add(GTK_CONTAINER(v->statusbox), status_hbox);

  if (dwb.misc.top_statusbar)
    gtk_box_pack_start(GTK_BOX(v->bottombox), v->statusbox, false, false, 0);
  else 
    gtk_box_pack_end(GTK_BOX(v->bottombox), v->statusbox, false, false, 0);

  // Srolling
  v->scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(v->scroll), v->web);
#if _HAS_GTK3
  gtk_window_set_has_resize_grip(GTK_WINDOW(dwb.gui.window), false);
  GtkCssProvider *provider = gtk_css_provider_get_default();
  gtk_css_provider_load_from_data(provider, 
      "GtkScrollbar { \
        -GtkRange-slider-width: 0; \
        -GtkRange-trough-border: 0; \
        }\
        GtkScrolledWindow {\
          -GtkScrolledWindow-scrollbar-spacing : 0;\
        }", -1, NULL);
#else
  WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  g_signal_connect(frame, "scrollbars-policy-changed", G_CALLBACK(dwb_true), NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(v->scroll), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
#endif


  // Tabbar
  v->tabevent = gtk_event_box_new();
  v->tabbox = gtk_hbox_new(false, 1);
  v->tablabel = gtk_label_new(NULL);

  gtk_label_set_use_markup(GTK_LABEL(v->tablabel), true);
  gtk_label_set_width_chars(GTK_LABEL(v->tablabel), 1);
  gtk_misc_set_alignment(GTK_MISC(v->tablabel), 0.0, 0.5);
  gtk_label_set_ellipsize(GTK_LABEL(v->tablabel), PANGO_ELLIPSIZE_END);

  gtk_box_pack_end(GTK_BOX(v->tabbox), v->tablabel, true, true, 0);

#if WEBKIT_CHECK_VERSION(1, 4, 0)
  GdkPixbuf *pb = gdk_pixbuf_new_from_xpm_data(dummy_icon);
  v->tabicon = gtk_image_new_from_pixbuf(pb);
  gtk_box_pack_end(GTK_BOX(v->tabbox), v->tabicon, false, false, 0);
#endif

  gtk_container_add(GTK_CONTAINER(v->tabevent), v->tabbox);

  //gtk_container_add(GTK_CONTAINER(v->tabevent), v->tablabel);
  //
  DWB_WIDGET_OVERRIDE_FONT(v->tablabel, dwb.font.fd_inactive);

  if (dwb.misc.top_statusbar) {
    gtk_box_pack_start(GTK_BOX(v->vbox), v->bottombox, false, false, 0);
    gtk_box_pack_start(GTK_BOX(v->vbox), v->scroll, true, true, 0);
  }
  else {
    gtk_box_pack_start(GTK_BOX(v->vbox), v->scroll, true, true, 0);
    gtk_box_pack_start(GTK_BOX(v->vbox), v->bottombox, false, false, 0);
  }

  gtk_widget_show(v->vbox);
  gtk_widget_show(v->lstatus);
  gtk_widget_show(v->entry);
  gtk_widget_show(v->urilabel);
  gtk_widget_show(v->rstatus);
  gtk_widget_show(status_hbox);
  gtk_widget_show(v->statusbox);
  gtk_widget_show(v->bottombox);
  gtk_widget_show_all(v->scroll);
  gtk_widget_show_all(v->tabevent);
  g_signal_connect(v->bottombox, "size-allocate", G_CALLBACK(dwb_view_entry_size_allocate_cb), v);

  return v;
} /*}}}*/

/* dwb_view_push_master (Arg *) {{{*/
gboolean 
dwb_view_push_master(Arg *arg) {
  GList *gl = NULL, *l = NULL;
  View *old = NULL, *new;
  if (!dwb.state.views->next) {
    return false;
  }
  if (arg && arg->p) {
    gl = arg->p;
  }
  else if (dwb.state.nummod) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod);
    if (!gl) {
      return false;
    }
    CLEAR_COMMAND_TEXT(dwb.state.views);
    dwb_view_set_normal_style(CURRENT_VIEW());
  }
  else {
    gl = dwb.state.fview;
  }
  if (gl == dwb.state.views) {
    old = gl->data;
    l = dwb.state.views->next;
    new = l->data;
    gtk_widget_reparent(old->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), old->vbox, 0);
    gtk_widget_reparent(new->vbox, dwb.gui.left);
    dwb.state.views = g_list_remove_link(dwb.state.views, l);
    dwb.state.views = g_list_concat(l, dwb.state.views);
    dwb_unfocus();
    dwb_focus(l);
  }
  else {
    old = dwb.state.views->data;
    new = gl->data;
    gtk_widget_reparent(new->vbox, dwb.gui.left);
    gtk_widget_reparent(old->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), old->vbox, 0);
    dwb.state.views = g_list_remove_link(dwb.state.views, gl);
    dwb.state.views = g_list_concat(gl, dwb.state.views);
    dwb_focus(dwb.state.views);
  }
  if (dwb.state.layout & MAXIMIZED) {
    gtk_widget_show(dwb.gui.left);
    gtk_widget_hide(dwb.gui.right);
    gtk_widget_show(new->vbox);
    gtk_widget_hide(old->vbox);
  }
  gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), new->tabevent, -1);
  dwb_update_layout(false);
  return true;
}/*}}}*/

/* dwb_view_remove (void) {{{*/
void
dwb_view_remove(GList *g) {
  GList *gl = NULL;
  if (!dwb.state.views->next) {
    return;
  }
  if (g || dwb.state.nummod == 0) {
    gl = g != NULL ? g : dwb.state.fview;
    if (gl == dwb.state.fview) {
      if ( !(dwb.state.fview = dwb.state.fview->prev) ) {
        dwb.state.fview = g_list_first(dwb.state.views)->next;
        gtk_widget_show_all(dwb.gui.topbox);
      }
    }
  }
  else if (dwb.state.nummod) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod);
  }
  View *v = gl->data;
  if (gl == dwb.state.views) {
    if (dwb.state.views->next) {
      gtk_widget_reparent(VIEW(dwb.state.views->next)->vbox, dwb.gui.left);
    }
  }

  /* Get History for the undo list */
  WebKitWebBackForwardList *bflist = webkit_web_view_get_back_forward_list(WEBKIT_WEB_VIEW(v->web));
  GList *store = NULL;

  for (int i = -webkit_web_back_forward_list_get_back_length(bflist); i<=0; i++) {
    WebKitWebHistoryItem *item = webkit_web_back_forward_list_get_nth_item(bflist, i);
    Navigation *n = dwb_navigation_from_webkit_history_item(item);
    if (n) 
      store = g_list_append(store, n);
  }
  dwb.state.undo_list = g_list_prepend(dwb.state.undo_list, store);

  /* Favicon */ 
  GdkPixbuf *pb;
  if ( (pb = gtk_image_get_pixbuf(GTK_IMAGE(v->tabicon))) ) 
    gdk_pixbuf_unref(pb);

  /* Destroy widget */
  gtk_widget_destroy(v->web);
  gtk_widget_destroy(v->scroll);
  gtk_widget_destroy(v->vbox);
  dwb.gui.entry = NULL;
  dwb_focus(dwb.state.fview);
  gtk_container_remove(GTK_CONTAINER(dwb.gui.topbox), v->tabevent);

  /*  clean up */ 
  dwb_source_remove(gl);
  FREE(v->status);
  FREE(v);

  dwb.state.views = g_list_delete_link(dwb.state.views, gl);

  /* Update MAXIMIZED layout */ 
  if (dwb.state.layout & MAXIMIZED) {
    gtk_widget_show(CURRENT_VIEW()->vbox);
    if (dwb.state.fview == dwb.state.views) {
      gtk_widget_hide(dwb.gui.right);
      gtk_widget_show(dwb.gui.left);
    }
    else {
      gtk_widget_show(dwb.gui.right);
      gtk_widget_hide(dwb.gui.left);
    }
  }
  dwb_update_layout(false);
}/*}}}*/

/* dwb_view_ssl_state (GList *gl) {{{*/
static void
dwb_view_ssl_state(GList *gl) {
  View *v = VIEW(gl);
  SslState ssl = SSL_NONE;

  const char *uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(v->web));
  if (uri && g_str_has_prefix(uri, "https")) {
    WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
    WebKitWebDataSource *ds = webkit_web_frame_get_data_source(frame);
    WebKitNetworkRequest *request = webkit_web_data_source_get_request(ds);
    SoupMessage *msg = webkit_network_request_get_message(request);
    if (msg) {
      ssl = soup_message_get_flags(msg) & SOUP_MESSAGE_CERTIFICATE_TRUSTED 
        ? SSL_TRUSTED 
        : SSL_UNTRUSTED;
    }
  }
  v->status->ssl = ssl;
  dwb_update_uri(gl);
}/*}}}*/


/* dwb_add_view(Arg *arg)               return: View *{{{*/
GList *  
dwb_add_view(Arg *arg, gboolean background) {
  GList *ret = NULL;

  View *v = dwb_view_create_web_view();
  if ((dwb.state.layout & MAXIMIZED || background) && dwb.state.fview) {
    int p = g_list_position(dwb.state.views, dwb.state.fview) + 1;
    PRINT_DEBUG("position :%d", p);
    gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), v->tabevent, g_list_length(dwb.state.views) - p);
    gtk_box_insert(GTK_BOX(dwb.gui.right), v->vbox, true, true, 0, p-1);
    dwb.state.views = g_list_insert(dwb.state.views, v, p);
    ret = dwb.state.fview->next;

    if (background) {
      dwb_view_set_normal_style(v);
      gtk_widget_hide(v->vbox);
    }
    else {
      gtk_widget_hide(VIEW(dwb.state.fview)->vbox);
      if (dwb.state.views == dwb.state.fview) {
        gtk_widget_hide(dwb.gui.left);
        gtk_widget_show(dwb.gui.right);
      }
      dwb_unfocus();
      dwb_focus(ret);
    }
  }
  else {
    // reorder
    if (dwb.state.views) {
      View *views = dwb.state.views->data;
      CLEAR_COMMAND_TEXT(dwb.state.views);
      gtk_widget_reparent(views->vbox, dwb.gui.right);
      gtk_box_reorder_child(GTK_BOX(dwb.gui.right), views->vbox, 0);
    }
    gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);
    gtk_box_insert(GTK_BOX(dwb.gui.left), v->vbox, true, true, 0, 0);
    dwb.state.views = g_list_prepend(dwb.state.views, v);
    ret = dwb.state.views;
    dwb_unfocus();
    dwb_focus(ret);
  }



  dwb_web_view_init_signals(ret);
  dwb_web_view_init_settings(ret);
  //dwb_hints_init();

  dwb_update_layout(background);
  if (arg && arg->p) {
    arg->b = true;
    dwb_load_uri(ret, arg);
  }
  else if (strcmp("about:blank", dwb.misc.startpage)) {
    char *page = g_strdup(dwb.misc.startpage);
    Arg a = { .p = page, .b = true }; 
    dwb_load_uri(ret, &a);
    g_free(page);
  }
  return ret;
} /*}}}*/

/*}}}*/
