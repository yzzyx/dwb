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

static void dwb_parse_setting(const char *);
static void dwb_parse_key_setting(const char *);
static void dwb_apply_settings(WebSettings *);
static void dwb_view_ssl_state(GList *);


// CALLBACKS
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
static gboolean
dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  WebKitHitTestResult *result = webkit_web_view_get_hit_test_result(web, e);
  WebKitHitTestResultContext context;
  g_object_get(result, "context", &context, NULL);
  gboolean ret = false;

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) {
    dwb_insert_mode(NULL);
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
  else if (e->button == 1 && e->type == GDK_BUTTON_PRESS) {
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
  Arg a = { .p = gl };
  dwb_com_remove_view(NULL, &a);
  return true;
}/*}}}*/

/* dwb_web_view_console_message_cb(WebKitWebView *web, char *message, int line, char *sourceid, GList *gl) {{{*/
static gboolean 
dwb_web_view_console_message_cb(WebKitWebView *web, char *message, int line, char *sourceid, GList *gl) {
  if (!strcmp(sourceid, KEY_SETTINGS)) {
    dwb_parse_key_setting(message);
  }
  else if (!(strcmp(sourceid, SETTINGS))) {
    dwb_parse_setting(message);
  }
  else if (gl == dwb.state.fview && !(strcmp(message, "_dwb_input_mode_"))) {
    dwb_insert_mode(NULL);
  }
  else if (gl == dwb.state.fview && !(strcmp(message, "_dwb_normal_mode_"))) {
    dwb_normal_mode(false);
  }
  if (!strcmp(message, "_dwb_no_input_")) {
    dwb_set_error_message(gl, "No input found in current context");
  }
  return true;
}/*}}}*/

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
    dwb_set_status_bar_text(v->urilabel, uri, &dwb.color.active_fg, NULL, false);
  }
  else {
    dwb_update_uri(gl);
  }
}/*}}}*/

/* dwb_web_view_mime_type_policy_cb {{{*/
static gboolean 
dwb_web_view_mime_type_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, char *mimetype, WebKitWebPolicyDecision *policy, GList *gl) {
  View *v = gl->data;

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

  char *uri = (char *) webkit_network_request_get_uri(request);
  gboolean ret = false;
  
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
  if (reason == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED) {
    if (dwb.state.mode == INSERT_MODE) {
      dwb_normal_mode(true);
    }
    if (dwb.state.mode == SEARCH_FIELD_MODE) {
      webkit_web_policy_decision_ignore(policy);
      dwb.state.search_engine = dwb.state.form_name && !g_strrstr(uri, HINT_SEARCH_SUBMIT) 
        ? g_strdup_printf("%s?%s=%s", uri, dwb.state.form_name, HINT_SEARCH_SUBMIT) 
        : g_strdup(uri);
      dwb_save_searchengine();
      return true;
    }
  }

  char *scheme = g_uri_parse_scheme(uri);
  if (scheme) {
    if (!strcmp(scheme, "plugin")) {
      webkit_web_policy_decision_ignore(policy);
      char *command = g_strdup_printf("DwbPlugin.click('%s')", uri+7);
      dwb_execute_script(web, command, false);
      g_free(command);
      ret = true;
    }
    else if (!strcmp(scheme, "mailto")) {
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
  return  false;
}/*}}}*/

/* dwb_web_view_value_changed_cb(GtkAdjustment *a, GList *gl) {{{ */
static gboolean
dwb_web_view_value_changed_cb(GtkAdjustment *a, GList *gl) {
  dwb_update_status_text(gl, a);
  return false;
}/* }}} */

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
  dwb_view_ssl_state(gl);
}/*}}}*/

// window-object-cleared is emmited in receivedFirstData which emits load-status
// commited, so we don't connect to window-object-cleared but to
// load_status_after instead
static void 
dwb_web_view_load_status_after_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(web);
  if (status == WEBKIT_LOAD_COMMITTED) {
    dwb_execute_script(web, dwb.misc.scripts, false);
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
      break;
    case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT: 
      // TODO use this state for adblocker
      break;
    case WEBKIT_LOAD_COMMITTED: 
      if (VIEW(gl)->status->scripts & SCRIPTS_BLOCKED 
          && (((host = dwb_get_host(web)) 
          && (dwb_get_allowed(dwb.files.scripts_allow, host) || dwb_get_allowed(dwb.files.scripts_allow, uri) 
              || g_list_find_custom(dwb.fc.tmp_scripts, host, (GCompareFunc)strcmp) || g_list_find_custom(dwb.fc.tmp_scripts, uri, (GCompareFunc)strcmp)))
          || !strcmp(uri, "dwb://"))) {
        g_object_set(webkit_web_view_get_settings(web), "enable-scripts", true, NULL);
        v->status->scripts |= SCRIPTS_ALLOWED_TEMPORARY;
      }
      FREE(host);
      break;
    case WEBKIT_LOAD_FINISHED:
      dwb_update_status(gl);
      dwb_execute_script(web, "DwbHintObj.createStyleSheet()", false);
      if (dwb_prepend_navigation(gl, &dwb.fc.history) && !dwb.misc.private_browsing)
        dwb_util_file_add_navigation(dwb.files.history, dwb.fc.history->data, false, dwb.misc.history_length);
      dwb_clean_load_end(gl);
      break;
    case WEBKIT_LOAD_FAILED: 
      dwb_clean_load_end(gl);
      break;
    default:
      break;
  }
}/*}}}*/

// Entry
/* dwb_entry_keyrelease_cb {{{*/
static gboolean 
dwb_view_entry_keyrelease_cb(GtkWidget* entry, GdkEventKey *e) { 
  Mode mode = dwb.state.mode;

  if (mode == HINT_MODE) {
    if (DWB_TAB_KEY(e)) {
      return true;
    }
    else {
      return dwb_update_hints(e);
    }
  }
  else if (mode == FIND_MODE) {
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
    if (DWB_TAB_KEY(e)) {
      return dwb_update_hints(e);
    }
  }
  else if (mode == SEARCH_FIELD_MODE) {
    if (DWB_TAB_KEY(e)) {
      return dwb_update_hints(e);
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
  }
  else if (mode == SEARCH_FIELD_MODE) {
    dwb_submit_searchengine();
  }
  else if (mode == SETTINGS_MODE) {
    dwb_parse_setting(GET_TEXT());
  }
  else if (mode == KEY_MODE) {
    dwb_parse_key_setting(GET_TEXT());
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
      dwb_focus(gl);
      dwb_view_push_master(&a);
    }
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
dwb_view_modify_style(View *v, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd) {
  if (fg) {
    gtk_widget_modify_fg(v->rstatus, GTK_STATE_NORMAL, fg);
    gtk_widget_modify_fg(v->lstatus, GTK_STATE_NORMAL, fg);
    gtk_widget_modify_text(v->entry, GTK_STATE_NORMAL, fg);
  }
  if (bg) {
    gtk_widget_modify_bg(v->statusbox, GTK_STATE_NORMAL, bg);
    gtk_widget_modify_base(v->entry, GTK_STATE_NORMAL, bg);
  }
  if (tabfg) 
    gtk_widget_modify_fg(v->tablabel, GTK_STATE_NORMAL, tabfg);
  if (tabbg)
    gtk_widget_modify_bg(v->tabevent, GTK_STATE_NORMAL, tabbg);
  if (fd) {
    gtk_widget_modify_font(v->rstatus, fd);
    gtk_widget_modify_font(v->urilabel, fd);
    gtk_widget_modify_font(v->lstatus, fd);
    gtk_widget_modify_font(v->tablabel, fd);
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
  v->status->signals[SIG_LOAD_STATUS_AFTER]     = g_signal_connect_after(v->web, "notify::load-status",             G_CALLBACK(dwb_web_view_load_status_after_cb), gl);
  v->status->signals[SIG_PROGRESS]              = g_signal_connect(v->web, "notify::progress",                      G_CALLBACK(dwb_web_view_progress_cb), gl);
  v->status->signals[SIG_TITLE]                 = g_signal_connect(v->web, "notify::title",                         G_CALLBACK(dwb_web_view_title_cb), gl);
  v->status->signals[SIG_URI]                   = g_signal_connect(v->web, "notify::uri",                           G_CALLBACK(dwb_web_view_uri_cb), gl);
  v->status->signals[SIG_SCROLL]                = g_signal_connect(v->web, "scroll-event",                          G_CALLBACK(dwb_web_view_scroll_cb), gl);
  v->status->signals[SIG_VALUE_CHANGED]         = g_signal_connect(a,      "value-changed",                         G_CALLBACK(dwb_web_view_value_changed_cb), gl);

  v->status->signals[SIG_ENTRY_KEY_PRESS]       = g_signal_connect(v->entry, "key-press-event",                     G_CALLBACK(dwb_view_entry_keypress_cb), NULL);
  v->status->signals[SIG_ENTRY_KEY_RELEASE]     = g_signal_connect(v->entry, "key-release-event",                   G_CALLBACK(dwb_view_entry_keyrelease_cb), NULL);
  v->status->signals[SIG_ENTRY_ACTIVATE]        = g_signal_connect(v->entry, "activate",                            G_CALLBACK(dwb_view_entry_activate_cb), gl);

  v->status->signals[SIG_TAB_BUTTON_PRESS]      = g_signal_connect(v->tabevent, "button-press-event",               G_CALLBACK(dwb_view_tab_button_press_cb), gl);
} /*}}}*/


/* dwb_view_create_web_view(View *v)         return: GList * {{{*/
static GList * 
dwb_view_create_web_view(GList *gl, gboolean background) {
  View *v = g_malloc(sizeof(View));
  GList *tmp;

  ViewStatus *status = g_malloc(sizeof(ViewStatus));
  status->search_string = NULL;
  status->downloads = NULL;
  status->mimetype = NULL;
  status->progress = 0;
  for (int i=0; i<SIG_LAST; i++) 
    status->signals[i] = 0;
  v->status = status;

  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();

  // Entry
  v->entry = gtk_entry_new();
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), NULL);

  gtk_widget_modify_font(v->entry, dwb.font.fd_entry);
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

  gtk_widget_modify_fg(v->urilabel, GTK_STATE_NORMAL, &dwb.color.active_fg);

  status_hbox = gtk_hbox_new(false, 2);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->lstatus, false, false, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->entry, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->urilabel, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->rstatus, false, false, 0);
  gtk_container_add(GTK_CONTAINER(v->statusbox), status_hbox);

  gtk_box_pack_end(GTK_BOX(v->bottombox), v->statusbox, false, false, 0);

  // Srolling
  v->scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(v->scroll), v->web);
  WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  g_signal_connect(frame, "scrollbars-policy-changed", G_CALLBACK(dwb_true), NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(v->scroll), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

  // WebInspector
  WebKitWebInspector *inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(v->web));
  g_signal_connect(inspector, "inspect-web-view", G_CALLBACK(dwb_web_view_inspect_web_view_cb), gl);

  // Tabbar
  v->tabevent = gtk_event_box_new();
  v->tablabel = gtk_label_new(NULL);

  gtk_label_set_use_markup(GTK_LABEL(v->tablabel), true);
  gtk_label_set_width_chars(GTK_LABEL(v->tablabel), 1);
  gtk_misc_set_alignment(GTK_MISC(v->tablabel), 0.0, 0.5);
  gtk_label_set_ellipsize(GTK_LABEL(v->tablabel), PANGO_ELLIPSIZE_END);

  gtk_container_add(GTK_CONTAINER(v->tabevent), v->tablabel);
  gtk_widget_modify_font(v->tablabel, dwb.font.fd_inactive);

  gtk_box_pack_start(GTK_BOX(v->vbox), v->scroll, true, true, 0);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->bottombox, false, false, 0);

  gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);
  
  // Show
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

  int position;
  if (background 
      && dwb.state.views
      && (position = g_list_position(dwb.state.views, dwb.state.fview) > -1) ) {
    gl = g_list_append(gl, v);
    gtk_box_pack_start(GTK_BOX(dwb.gui.right), v->vbox, true, true, 0);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), v->vbox, -1);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), v->tabevent, 0);
    dwb_view_set_normal_style(v);
    dwb_view_set_active_style(CURRENT_VIEW());
    tmp = g_list_last(gl);
    if (dwb.state.layout & MAXIMIZED) 
      gtk_widget_hide(v->vbox);
  }
  else {
    gtk_box_pack_start(GTK_BOX(dwb.gui.left), v->vbox, true, true, 0);
    gl = tmp = g_list_prepend(gl, v);
    dwb_focus(gl);
  }

  dwb_web_view_init_signals(tmp);
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(v->web), webkit_web_settings_copy(dwb.state.web_settings));
  // apply settings
  v->setting = dwb_get_default_settings();
  for (GList *l = g_hash_table_get_values(v->setting); l; l=l->next) {
    WebSettings *s = l->data;
    if (!s->builtin && !s->global) {
      s->func(tmp, s);
    }
  }
  return gl;
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
    dwb_grab_focus(dwb.state.views);
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
dwb_view_remove() {
  GList *gl = NULL;
  if (!dwb.state.views->next) {
    return;
  }
  if (dwb.state.nummod) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod);
  }
  else {
    gl = dwb.state.fview;
    if ( !(dwb.state.fview = dwb.state.fview->next) ) {
      dwb.state.fview = dwb.state.views;
      gtk_widget_show_all(dwb.gui.topbox);
    }
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

  /* Destroy widget */
  gtk_widget_destroy(v->scroll);
  gtk_widget_destroy(v->vbox);
  dwb.gui.entry = NULL;
  dwb_grab_focus(dwb.state.fview);
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

static void 
dwb_view_new_reorder(gboolean background) {
  if (background)
    return;
  if (dwb.state.views) {
    View *views = dwb.state.views->data;
    CLEAR_COMMAND_TEXT(dwb.state.views);
    gtk_widget_reparent(views->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), views->vbox, 0);
    if (dwb.state.layout & MAXIMIZED) {
      gtk_widget_hide(((View *)dwb.state.fview->data)->vbox);
      if (dwb.state.fview != dwb.state.views) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
    }
  }
}
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

  dwb_view_new_reorder(background);
  dwb.state.views = ret = dwb_view_create_web_view(dwb.state.views, background);
  if (background) {
    ret = g_list_last(dwb.state.views);
  }
  else {
    dwb_focus(dwb.state.views);
  }

  dwb_update_layout(background);
  if (arg && arg->p) {
    arg->b = true;
    dwb_load_uri(ret, arg);
  }
  else if (strcmp("about:blank", dwb.misc.startpage)) {
    Arg a = { .p = dwb.misc.startpage, .b = true }; 
    dwb_load_uri(ret, &a);
  }
  return ret;
} /*}}}*/


/* dwb_parse_setting(const char *){{{*/
void
dwb_parse_setting(const char *text) {
  WebSettings *s;
  Arg *a = NULL;
  char **token = g_strsplit(text, " ", 2);

  GHashTable *t = dwb.state.setting_apply == APPLY_GLOBAL ? dwb.settings : ((View*)dwb.state.fview->data)->setting;
  if (token[0]) {
    if  ( (s = g_hash_table_lookup(t, token[0])) ) {
      if ( (a = dwb_util_char_to_arg(token[1], s->type)) || (s->type == CHAR && a->p == NULL)) {
        s->arg = *a;
        dwb_apply_settings(s);
        dwb_set_normal_message(dwb.state.fview, true, "Saved setting %s: %s", s->n.first, s->type == BOOLEAN ? ( s->arg.b ? "true" : "false") : token[1]);
        dwb_save_settings();
      }
      else {
        dwb_set_error_message(dwb.state.fview, "No valid value.");
      }
    }
    else {
      dwb_set_error_message(dwb.state.fview, "No such setting: %s", token[0]);
    }
  }
  dwb_normal_mode(false);

  g_strfreev(token);

}/*}}}*/

/* dwb_parse_key_setting(const char  *text) {{{*/
void
dwb_parse_key_setting(const char *text) {
  KeyValue value;
  char **token = g_strsplit(text, " ", 2);

  value.id = g_strdup(token[0]);

  if (token[1]) {
    value.key = dwb_str_to_key(token[1]); 
  }
  else {
    Key key = { NULL, 0 };
    value.key = key;
  }

  dwb_set_normal_message(dwb.state.fview, true, "Saved key for command %s: %s", token[0], token[1] ? token[1] : "");

  dwb.keymap = dwb_keymap_add(dwb.keymap, value);
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)dwb_util_keymap_sort_second);

  g_strfreev(token);
  dwb_normal_mode(false);
}/*}}}*/

/* dwb_apply_settings(WebSettings *s) {{{*/
static void
dwb_apply_settings(WebSettings *s) {
  WebSettings *new;
  if (dwb.state.setting_apply == APPLY_GLOBAL) {
    new = g_hash_table_lookup(dwb.settings, s->n.first);
    new->arg = s->arg;
    for (GList *l = dwb.state.views; l; l=l->next)  {
      WebSettings *new =  g_hash_table_lookup(VIEW(l)->setting, s->n.first);
      new->arg = s->arg;
      if (s->func) {
        s->func(l, s);
      }
    }
  }
  else {
    s->func(dwb.state.fview, s);
  }
  dwb_normal_mode(false);

}/*}}}*/

/*}}}*/
