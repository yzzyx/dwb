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
#include "local.h"

static void view_ssl_state(GList *);
static const char *dummy_icon[] = { "1 1 1 1 ", "  c black", " ", };


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
static void view_resource_request_cb(WebKitWebView *, WebKitWebFrame *, WebKitWebResource *, WebKitNetworkRequest *, WebKitNetworkResponse *, GList *);
/* static void view_window_object_cleared_cb(WebKitWebView *, WebKitWebFrame *, JSGlobalContextRef *, JSObjectRef *, GList *); */
static gboolean view_scroll_cb(GtkWidget *, GdkEventScroll *, GList *);
static gboolean view_value_changed_cb(GtkAdjustment *, GList *);
static void view_title_cb(WebKitWebView *, GParamSpec *, GList *);
static void view_uri_cb(WebKitWebView *, GParamSpec *, GList *);
static void view_load_status_cb(WebKitWebView *, GParamSpec *, GList *);
static gboolean view_entry_keyrelease_cb(GtkWidget *, GdkEventKey *);
static gboolean view_entry_keypress_cb(GtkWidget *, GdkEventKey *);
static gboolean view_entry_activate_cb(GtkEntry *, GList *);
static gboolean view_tab_button_press_cb(GtkWidget *, GdkEventButton *, GList *);

/* WEB_VIEW_CALL_BACKS {{{*/

/* view_button_press_cb(WebKitWebView *web, GdkEventButton *button, GList *gl) {{{*/
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
  g_signal_handlers_disconnect_by_func(web, view_caret_release_cb, other);
  g_signal_handlers_disconnect_by_func(web, view_caret_motion_cb, other);
  return false;
}
static gboolean
view_button_press_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  WebKitHitTestResult *result = webkit_web_view_get_hit_test_result(web, e);
  WebKitHitTestResultContext context;
  g_object_get(result, "context", &context, NULL);
  gboolean ret = false;

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) {
    dwb_change_mode(INSERT_MODE);
  }
  else if (e->state & GDK_CONTROL_MASK && e->button == 1) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(web);
    WebKitDOMRange *range = webkit_dom_document_caret_range_from_point(doc, e->x, e->y);
    WebKitDOMDOMSelection *selection = webkit_dom_dom_window_get_selection(webkit_dom_document_get_default_view(doc));
    webkit_dom_dom_selection_remove_all_ranges(selection);
    webkit_dom_dom_selection_add_range(selection, range);
    g_signal_connect(web, "button-release-event", G_CALLBACK(view_caret_release_cb), range);
    g_signal_connect(web, "motion-notify-event", G_CALLBACK(view_caret_motion_cb), range);
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
    FREE(clipboard);
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
  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    g_object_get(result, "link-uri", &uri, NULL);
    if (e->button == 2) {
      view_add(uri, dwb.state.background_tabs);
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

/* view_console_message_cb(WebKitWebView *web, char *message, int line, char *sourceid, GList *gl) {{{*/
static gboolean 
view_console_message_cb(WebKitWebView *web, char *message, int line, char *sourceid, GList *gl) {
#if 0
  if (gl == dwb.state.fview && !(strncmp(message, "_dwb_input_mode_", 16))) {
    dwb_insert_mode(NULL);
  }
  else if (gl == dwb.state.fview && !(strncmp(message, "_dwb_normal_mode_", 17))) {
    dwb_change_mode(NORMAL_MODE, true);
  }
  if (!strncmp(message, "_dwb_no_input_", 14)) {
    dwb_set_error_message(gl, "No input found in current context");
  }
#endif
  return true;
}/*}}}*/

/* view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *) {{{*/
static WebKitWebView * 
view_create_web_view_cb(WebKitWebView *web, WebKitWebFrame *frame, GList *gl) {
  if (dwb.misc.tabbed_browsing) {
    GList *gl = view_add(NULL, false); 
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
  download_get_path(gl, download);
  return true;
}/*}}}*/

gboolean 
view_delete_web_inspector(GtkWidget *widget, GdkEvent *event, GtkWidget *wv) {
  /* Remove webview before destroying the window, otherwise closing the
   * webinspector  will result in a segfault 
   * */
  gtk_container_remove(GTK_CONTAINER(widget), wv);
  gtk_widget_destroy(widget);
  return true;
}

/* view_inspect_web_view_cb(WebKitWebInspector *, WebKitWebView *, GList * *){{{*/
static WebKitWebView * 
view_inspect_web_view_cb(WebKitWebInspector *inspector, WebKitWebView *wv, GList *gl) {
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWidget *webview = webkit_web_view_new();
  
  gtk_container_add(GTK_CONTAINER(window), webview);
  gtk_widget_show_all(window);

  g_signal_connect(window, "delete-event", G_CALLBACK(view_delete_web_inspector), webview);
  return WEBKIT_WEB_VIEW(webview);
}/*}}}*/

/* view_hovering_over_link_cb(WebKitWebView *, char *title, char *uri, GList *) {{{*/
static void 
view_hovering_over_link_cb(WebKitWebView *web, char *title, char *uri, GList *gl) {
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

/* view_mime_type_policy_cb {{{*/
static gboolean 
view_mime_type_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, char *mimetype, WebKitWebPolicyDecision *policy, GList *gl) {
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

/* view_navigation_policy_cb {{{*/
static gboolean 
view_navigation_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {

  char *uri = (char *) webkit_network_request_get_uri(request);
  gboolean ret = false;

  if (dwb.state.mode == INSERT_MODE) {
    dwb_change_mode(NORMAL_MODE, true);
  }
  if (g_str_has_prefix(uri, "dwb://")) {
    html_load(gl, uri);
    return true;
  }
  if (dwb.state.nv == OPEN_NEW_VIEW || dwb.state.nv == OPEN_NEW_WINDOW) {
    if (dwb.state.nv == OPEN_NEW_VIEW) {
      dwb.state.nv = OPEN_NORMAL;
      view_add(uri, dwb.state.background_tabs); 
    }
    else {
      dwb_new_window(uri);
    }
    dwb.state.nv = OPEN_NORMAL;
    return true;
  }
  WebKitWebNavigationReason reason = webkit_web_navigation_action_get_reason(action);
  GError *error = NULL;

  if ((reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED || reason == WEBKIT_WEB_NAVIGATION_REASON_BACK_FORWARD || reason == WEBKIT_WEB_NAVIGATION_REASON_RELOAD) 
      && (local_check_directory(gl, uri, reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED,  &error))) {
    if (error != NULL) {
      dwb_set_error_message(gl, error->message);
      g_clear_error(&error);
    }
    webkit_web_policy_decision_ignore(policy);
    return false;
  }
  else if (reason == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED) {
    if (dwb.state.mode == SEARCH_FIELD_MODE) {
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

/* view_new_window_policy_cb {{{*/
static gboolean 
view_new_window_policy_cb(WebKitWebView *web, WebKitWebFrame *frame,
    WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {
  return false;
}/*}}}*/

/* view_resource_request_cb{{{*/
static void 
view_resource_request_cb(WebKitWebView *web, WebKitWebFrame *frame,
    WebKitWebResource *resource, WebKitNetworkRequest *request,
    WebKitNetworkResponse *response, GList *gl) {
  if (dwb_block_ad(gl, webkit_network_request_get_uri(request))) {
    webkit_network_request_set_uri(request, "about:blank");
    return;
  }
}/*}}}*/

/* view_create_plugin_widget_cb {{{*/
static GtkWidget * 
view_create_plugin_widget_cb(WebKitWebView *web, char *mime_type, char *uri, GHashTable *param, GList *gl) {
  VIEW(gl)->status->pb_status |= PLUGIN_STATUS_HAS_PLUGIN;
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
      if (v->status->scripts & SCRIPTS_ALLOWED_TEMPORARY) {
        g_object_set(webkit_web_view_get_settings(web), "enable-scripts", false, NULL);
        v->status->scripts &= ~SCRIPTS_ALLOWED_TEMPORARY;
      }
      if (v->status->pb_status & PLUGIN_STATUS_ENABLED) 
        plugins_connect(gl);
      v->status->ssl = SSL_NONE;
      v->status->pb_status &= ~PLUGIN_STATUS_HAS_PLUGIN; 
      break;
    case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT: 
      /* This is more or less a dummy call, to compile the script and speed up
       * execution time 
       * */
      dwb_execute_script(webkit_web_view_get_main_frame(web), "DwbHintObj.createStylesheet();", false);
      break;
    case WEBKIT_LOAD_COMMITTED: 
      view_ssl_state(gl);
      if (VIEW(gl)->status->scripts & SCRIPTS_BLOCKED 
          && (((host = dwb_get_host(web)) 
          && (dwb_get_allowed(dwb.files.scripts_allow, host) || dwb_get_allowed(dwb.files.scripts_allow, uri) 
              || g_list_find_custom(dwb.fc.tmp_scripts, host, (GCompareFunc)strcmp) || g_list_find_custom(dwb.fc.tmp_scripts, uri, (GCompareFunc)strcmp)))
          || !strcmp(uri, "dwb://") || !strcmp(uri, "Error"))) {
        g_object_set(webkit_web_view_get_settings(web), "enable-scripts", true, NULL);
        v->status->scripts |= SCRIPTS_ALLOWED_TEMPORARY;
      }
      if (v->status->pb_status & PLUGIN_STATUS_ENABLED 
          && ( (host != NULL || (host = dwb_get_host(web))) 
          && (dwb_get_allowed(dwb.files.plugins_allow, host) || dwb_get_allowed(dwb.files.plugins_allow, uri)
            || g_list_find_custom(dwb.fc.tmp_plugins, host, (GCompareFunc)strcmp) || g_list_find_custom(dwb.fc.tmp_plugins, uri, (GCompareFunc)strcmp) )
            )) {
        plugins_disconnect(gl);
      }
      FREE(host);
      dwb_clean_load_end(gl);
      break;
    case WEBKIT_LOAD_FINISHED:
      dwb_update_status(gl);
      /* TODO sqlite */
      if (!dwb.misc.private_browsing 
          && strcmp(uri, "about:blank")
          && !g_str_has_prefix(uri, "dwb://") 
          && (dwb_prepend_navigation(gl, &dwb.fc.history) == STATUS_OK)
          && dwb.misc.synctimer <= 0) {
        util_file_add_navigation(dwb.files.history, dwb.fc.history->data, false, dwb.misc.history_length);
      }
      break;
    case WEBKIT_LOAD_FAILED: 
      dwb_clean_load_end(gl);
      break;
    default:
      break;
  }
}/*}}}*/

/* view_load_error_cb {{{*/
static gboolean 
view_load_error_cb(WebKitWebView *web, WebKitWebFrame *frame, char *uri, GError *weberror, GList *gl) {
  if (weberror->code == WEBKIT_NETWORK_ERROR_CANCELLED 
      || weberror->code == WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD 
      || weberror->code == WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE) 
    return false;


  char *errorfile = util_get_data_file(ERROR_FILE);
  if (errorfile == NULL) 
    return false;

  char *site, *search;
  char *content = util_get_file_content(errorfile);
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

  webkit_web_frame_load_alternate_string(frame, site, uri, uri);

  g_free(site);
  g_free(content);
  g_free(res);
  FREE(icon);
  FREE(search);
  return true;
}/*}}}*/

/* view_entry_size_allocate_cb {{{*/
void 
view_entry_size_allocate_cb(GtkWidget *entry, GdkRectangle *rect, View *v) {
  gtk_widget_set_size_request(v->entry, -1, rect->height);
  g_signal_handlers_disconnect_by_func(entry, view_entry_size_allocate_cb, v);
}/*}}}*/

/* Entry */
/* dwb_entry_keyrelease_cb {{{*/
static gboolean 
view_entry_keyrelease_cb(GtkWidget* entry, GdkEventKey *e) { 
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
view_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {
  Mode mode = dwb.state.mode;
  gboolean ret = false;
  gboolean complete = (mode == DOWNLOAD_GET_PATH || (mode & COMPLETE_PATH));
  /*  Handled by activate-callback */
  if (e->keyval == GDK_KEY_Return)
    return false;
  if (mode & COMPLETE_BUFFER) {
    completion_buffer_key_press(e);
    return true;
  }
  else if (e->keyval == GDK_KEY_BackSpace && !complete) {
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
      completion_complete_path(e->state & GDK_SHIFT_MASK);
      return true;
    }
    else {
      completion_clean_path_completion();
    }
  }
  else if (mode & COMPLETION_MODE && !DWB_TAB_KEY(e) && !e->is_modifier && !CLEAN_STATE(e)) {
    completion_clean_completion();
  }
  else if (mode == FIND_MODE) {
    return false;
  }
  else if (DWB_TAB_KEY(e)) {
    completion_complete(dwb_eval_completion_type(), e->state & GDK_SHIFT_MASK);
    return true;
  }
  if (dwb_eval_editing_key(e)) {
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_entry_activate_cb (GtkWidget *entry) {{{*/
static gboolean 
view_entry_activate_cb(GtkEntry* entry, GList *gl) {
  char **token = NULL;
  switch (CLEAN_MODE(dwb.state.mode))  {
    case HINT_MODE:           return false;
    case FIND_MODE:           dwb_focus_scroll(dwb.state.fview);
                              dwb_search(NULL, NULL);
                              dwb_change_mode(NORMAL_MODE, true);
                              return true;
    case SEARCH_FIELD_MODE:   dwb_submit_searchengine();
                              return true;
    case SETTINGS_MODE:       token = g_strsplit(GET_TEXT(), " ", 2);
                              dwb_set_setting(token[0], token[1]);
                              g_strfreev(token);
                              return true;
    case KEY_MODE:            token = g_strsplit(GET_TEXT(), " ", 2);
                              dwb_set_key(token[0], token[1]);
                              g_strfreev(token);
                              return true;
    case COMMAND_MODE:        dwb_parse_command_line(GET_TEXT());
                              return true;
    case DOWNLOAD_GET_PATH:   download_start(); 
                              return true;
    case SAVE_SESSION:        session_save(GET_TEXT());
                              dwb_end();
                              return true;
    case COMPLETE_BUFFER:     completion_eval_buffer_completion();
                              return true;
    case COMPLETE_PATH:       completion_clean_path_completion();
                              break;
    default : break;
  }
  Arg a = { .n = 0, .p = (char*)GET_TEXT(), .b = true };
  dwb_load_uri(NULL, GET_TEXT());
  dwb_prepend_navigation_with_argument(&dwb.fc.commands, a.p, NULL);
  dwb_change_mode(NORMAL_MODE, true);
  return true;

}/*}}}*/
/*}}}*/

/* view_tab_button_press_cb(GtkWidget, GdkEventButton* , GList * ){{{*/
static gboolean
view_tab_button_press_cb(GtkWidget *tabevent, GdkEventButton *e, GList *gl) {
  if (e->button == 1 && e->type == GDK_BUTTON_PRESS) {
    Arg a = { .p = gl };
    if ((dwb.state.layout & MAXIMIZED) ) {
      dwb_focus_view(gl);
    }
    else {
      dwb_unfocus();
      dwb_focus(gl);
      view_push_master(&a);
    }
    return true;
  }
  else if (e->button == 3 && e->type == GDK_BUTTON_PRESS) {
    view_remove(gl);
    return true;
  }
  return false;
}/*}}}*/

/* DWB_WEB_VIEW {{{*/

/* view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, int fontsize) {{{*/
void 
view_modify_style(View *v, DwbColor *fg, DwbColor *bg, DwbColor *tabfg, DwbColor *tabbg, PangoFontDescription *fd) {
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

/* view_set_active_style (GList *) {{{*/
void 
view_set_active_style(View *v) {
  view_modify_style(v, &dwb.color.active_fg, &dwb.color.active_bg, &dwb.color.tab_active_fg, &dwb.color.tab_active_bg, dwb.font.fd_active);
}/*}}}*/

/* view_set_normal_style {{{*/
void 
view_set_normal_style(View *v) {
  view_modify_style(v, &dwb.color.normal_fg, &dwb.color.normal_bg, &dwb.color.tab_normal_fg, &dwb.color.tab_normal_bg, dwb.font.fd_inactive);
}/*}}}*/

/* view_init_settings {{{*/
static void 
view_init_settings(GList *gl) {
  View *v = gl->data;
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(v->web), webkit_web_settings_copy(dwb.state.web_settings));
  /* apply settings */
  v->setting = dwb_get_default_settings();
  for (GList *l = g_hash_table_get_values(v->setting); l; l=l->next) {
    WebSettings *s = l->data;
    if (s->apply & SETTING_PER_VIEW) {
      s->func(gl, s);
    }
  }
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
  v->status->signals[SIG_CREATE_PLUGIN_WIDGET]  = g_signal_connect(v->web, "create-plugin-widget",                  G_CALLBACK(view_create_plugin_widget_cb), gl);

  v->status->signals[SIG_LOAD_STATUS]           = g_signal_connect(v->web, "notify::load-status",                   G_CALLBACK(view_load_status_cb), gl);
  v->status->signals[SIG_LOAD_ERROR]            = g_signal_connect(v->web, "load-error",                            G_CALLBACK(view_load_error_cb), gl);
  v->status->signals[SIG_LOAD_STATUS_AFTER]     = g_signal_connect_after(v->web, "notify::load-status",             G_CALLBACK(view_load_status_after_cb), gl);
  v->status->signals[SIG_POPULATE_POPUP]        = g_signal_connect(v->web, "populate-popup",                        G_CALLBACK(view_populate_popup_cb), gl);
  v->status->signals[SIG_PROGRESS]              = g_signal_connect(v->web, "notify::progress",                      G_CALLBACK(view_progress_cb), gl);
  v->status->signals[SIG_TITLE]                 = g_signal_connect(v->web, "notify::title",                         G_CALLBACK(view_title_cb), gl);
  v->status->signals[SIG_URI]                   = g_signal_connect(v->web, "notify::uri",                           G_CALLBACK(view_uri_cb), gl);
  v->status->signals[SIG_SCROLL]                = g_signal_connect(v->web, "scroll-event",                          G_CALLBACK(view_scroll_cb), gl);
  v->status->signals[SIG_VALUE_CHANGED]         = g_signal_connect(a,      "value-changed",                         G_CALLBACK(view_value_changed_cb), gl);
  v->status->signals[SIG_ICON_LOADED]           = g_signal_connect(v->web, "icon-loaded",                           G_CALLBACK(view_icon_loaded), gl);

  v->status->signals[SIG_ENTRY_KEY_PRESS]       = g_signal_connect(v->entry, "key-press-event",                     G_CALLBACK(view_entry_keypress_cb), NULL);
  v->status->signals[SIG_ENTRY_KEY_RELEASE]     = g_signal_connect(v->entry, "key-release-event",                   G_CALLBACK(view_entry_keyrelease_cb), NULL);
  v->status->signals[SIG_ENTRY_ACTIVATE]        = g_signal_connect(v->entry, "activate",                            G_CALLBACK(view_entry_activate_cb), gl);

  v->status->signals[SIG_TAB_BUTTON_PRESS]      = g_signal_connect(v->tabevent, "button-press-event",               G_CALLBACK(view_tab_button_press_cb), gl);

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
  status->mimetype = NULL;
  status->hover_uri = NULL;
  status->progress = 0;
  status->allowed_plugins = NULL;
  status->pb_status = 0;


  for (int i=0; i<SIG_LAST; i++) 
    status->signals[i] = 0;
  v->status = status;

  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();

  /* Entry */
  v->entry = gtk_entry_new();
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), NULL);

  DWB_WIDGET_OVERRIDE_FONT(v->entry, dwb.font.fd_entry);
  gtk_entry_set_has_frame(GTK_ENTRY(v->entry), false);
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), false);

  /* completion */
  v->bottombox = gtk_vbox_new(false, 1);

  /* Statusbox */
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

  /* Srolling */
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
  GtkStyleContext *ctx = gtk_widget_get_style_context(v->scroll);
  gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  ctx = gtk_widget_get_style_context(gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(v->scroll)));
  gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  ctx = gtk_widget_get_style_context(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(v->scroll)));
  gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#else
  WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  g_signal_connect(frame, "scrollbars-policy-changed", G_CALLBACK(dwb_true), NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(v->scroll), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
#endif


  /* Tabbar */
  v->tabevent = gtk_event_box_new();
  v->tabbox = gtk_hbox_new(false, 1);
  v->tablabel = gtk_label_new(NULL);

  gtk_label_set_use_markup(GTK_LABEL(v->tablabel), true);
  gtk_label_set_width_chars(GTK_LABEL(v->tablabel), 1);
  gtk_misc_set_alignment(GTK_MISC(v->tablabel), 0.0, 0.5);
  gtk_label_set_ellipsize(GTK_LABEL(v->tablabel), PANGO_ELLIPSIZE_END);

  gtk_box_pack_end(GTK_BOX(v->tabbox), v->tablabel, true, true, 0);

  GdkPixbuf *pb = gdk_pixbuf_new_from_xpm_data(dummy_icon);
  v->tabicon = gtk_image_new_from_pixbuf(pb);
  gtk_box_pack_end(GTK_BOX(v->tabbox), v->tabicon, false, false, 0);

  gtk_container_add(GTK_CONTAINER(v->tabevent), v->tabbox);

  /* gtk_container_add(GTK_CONTAINER(v->tabevent), v->tablabel); */

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
  g_signal_connect(v->bottombox, "size-allocate", G_CALLBACK(view_entry_size_allocate_cb), v);

  return v;
} /*}}}*/

/* view_push_master (Arg *) {{{*/
DwbStatus 
view_push_master(Arg *arg) {
  GList *gl = NULL, *l = NULL;
  View *old = NULL, *new;
  if (!dwb.state.views->next) {
    return STATUS_ERROR;
  }
  if (arg && arg->p) {
    gl = arg->p;
  }
  else if (dwb.state.nummod) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod);
    if (!gl) {
      return STATUS_ERROR;
    }
    CLEAR_COMMAND_TEXT(dwb.state.views);
    view_set_normal_style(CURRENT_VIEW());
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
  return STATUS_OK;
}/*}}}*/

/* view_remove (void) {{{*/
void
view_remove(GList *g) {
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
  gl = NULL;

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

/* view_ssl_state (GList *gl) {{{*/
static void
view_ssl_state(GList *gl) {
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


/* view_add(Arg *arg)               return: View *{{{*/
GList *  
view_add(const char *uri, gboolean background) {
  GList *ret = NULL;

  View *v = view_create_web_view();
  if ((dwb.state.layout & MAXIMIZED || background) && dwb.state.fview) {
    int p = g_list_position(dwb.state.views, dwb.state.fview) + 1;
    PRINT_DEBUG("position :%d", p);
    gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), v->tabevent, g_list_length(dwb.state.views) - p);
    gtk_box_insert(GTK_BOX(dwb.gui.right), v->vbox, true, true, 0, p-1);
    dwb.state.views = g_list_insert(dwb.state.views, v, p);
    ret = dwb.state.fview->next;

    if (background) {
      view_set_normal_style(v);
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
    /* reorder */
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



  view_init_signals(ret);
  view_init_settings(ret);

  dwb_update_layout(background);
  if (uri != NULL) {
    dwb_load_uri(ret, uri);
  }
  else if (strcmp("about:blank", dwb.misc.startpage)) {
    char *page = g_strdup(dwb.misc.startpage);
    dwb_load_uri(ret, page);
    g_free(page);
  }
  return ret;
} /*}}}*/

/*}}}*/
