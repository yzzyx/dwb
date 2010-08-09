#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h>
#include "dwb.h"
#include "view.h"
#include "completion.h"
#include "util.h"
#include "download.h"
#include "session.h"

/* WEB_VIEW_CALL_BACKS {{{*/

/* dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *button, GList *gl) {{{*/
gboolean
dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  Arg arg = { .p = gl };

  WebKitHitTestResult *result = webkit_web_view_get_hit_test_result(web, e);
  WebKitHitTestResultContext context;
  g_object_get(result, "context", &context, NULL);

  View *v = gl->data;

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) {
    dwb_insert_mode(NULL);
  }
  else if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    if (v->status->hover_uri && e->button == 2) {
      Arg a = { .p = v->status->hover_uri };
      dwb.state.nv = OpenNewView;
      dwb_load_uri(&a);
    }
  }
  else if (e->button == 1) {
    if (e->type == GDK_BUTTON_PRESS) {
      dwb_com_focus(gl);
    }
    if (e->type == GDK_2BUTTON_PRESS) {
      dwb_com_push_master(&arg);
    }
  }
  return false;
}/*}}}*/

/* dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {{{*/
gboolean 
dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {
  Arg a = { .p = gl };
  dwb_com_remove_view(&a);
  return true;
}/*}}}*/

/* dwb_web_view_console_message_cb(WebKitWebView *web, gchar *message, gint line, gchar *sourceid, GList *gl) {{{*/
gboolean 
dwb_web_view_console_message_cb(WebKitWebView *web, gchar *message, gint line, gchar *sourceid, GList *gl) {
  if (!strcmp(sourceid, KEY_SETTINGS)) {
    dwb_parse_key_setting(message);
  }
  else if (!(strcmp(sourceid, SETTINGS))) {
    dwb_parse_setting(message);
  }
  return true;
}/*}}}*/

/* dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *) {{{*/
GtkWidget * 
dwb_web_view_create_web_view_cb(WebKitWebView *web, WebKitWebFrame *frame, GList *gl) {
  dwb_add_view(NULL); 
  return ((View*)dwb.state.fview->data)->web;
}/*}}}*/

/* dwb_web_view_download_requested_cb(WebKitWebView *, WebKitDownload *, GList *) {{{*/
gboolean 
dwb_web_view_download_requested_cb(WebKitWebView *web, WebKitDownload *download, GList *gl) {
  dwb_dl_get_path(gl, download);
  return true;
}/*}}}*/

/* dwb_web_view_hovering_over_link_cb(WebKitWebView *, gchar *title, gchar *uri, GList *) {{{*/
void 
dwb_web_view_hovering_over_link_cb(WebKitWebView *web, gchar *title, gchar *uri, GList *gl) {
  VIEW(gl)->status->hover_uri = uri;
  if (uri) {
    dwb_set_status_text(gl, uri, NULL, NULL);
  }
  else {
    dwb_update_status_text(gl);
  }

}/*}}}*/

/* dwb_web_view_mime_type_policy_cb {{{*/
gboolean 
dwb_web_view_mime_type_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, gchar *mimetype, WebKitWebPolicyDecision *policy, GList *gl) {
  if (webkit_web_view_can_show_mime_type(web, mimetype)) {
    return  false;
  }
  else {
    dwb.state.mimetype_request = g_strdup(mimetype);
    webkit_web_policy_decision_download(policy);
    return true;
  }
}/*}}}*/

/* dwb_web_view_navigation_policy_cb {{{*/
gboolean 
dwb_web_view_navigation_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {

  if (dwb.state.nv == OpenNewView || dwb.state.nv == OpenNewWindow) {
    gchar *uri = (gchar *)webkit_network_request_get_uri(request);
    Arg a = { .p = uri };
    if (dwb.state.nv == OpenNewView) {
      dwb.state.nv = OpenNormal;
      dwb_add_view(&a); 
    }
    else {
      dwb_new_window(&a);
    }
    dwb.state.nv = OpenNormal;
    return true;
  }
  const gchar *request_uri = webkit_network_request_get_uri(request);
  WebKitWebNavigationReason reason = webkit_web_navigation_action_get_reason(action);

  if (dwb.state.mode == SearchFieldMode && reason == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED ) {
    webkit_web_policy_decision_ignore(policy);
    dwb.state.search_engine = dwb.state.form_name && !g_strrstr(request_uri, HINT_SEARCH_SUBMIT) 
      ? g_strdup_printf("%s?%s=%s", request_uri, dwb.state.form_name, HINT_SEARCH_SUBMIT) 
      : g_strdup(request_uri);
    dwb_save_searchengine();
    return true;
  }
  if (dwb.state.nv == OpenDownload) {
    webkit_web_policy_decision_download(policy);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_web_view_new_window_policy_cb {{{*/
gboolean 
dwb_web_view_new_window_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action, WebKitWebPolicyDecision *policy, GList *gl) {
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_resource_request_cb{{{*/
void 
dwb_web_view_resource_request_cb(WebKitWebView *web, WebKitWebFrame *frame,
    WebKitWebResource *resource, WebKitNetworkRequest *request,
    WebKitNetworkResponse *response, GList *gl) {
  // TODO implement
}/*}}}*/

/* dwb_web_view_script_alert_cb {{{*/
gboolean 
dwb_web_view_script_alert_cb(WebKitWebView *web, WebKitWebFrame *frame, gchar *message, GList *gl) {
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_window_object_cleared_cb {{{*/
void 
dwb_web_view_window_object_cleared_cb(WebKitWebView *web, WebKitWebFrame *frame, 
    JSGlobalContextRef *context, JSObjectRef *object, GList *gl) {
  JSStringRef script; 
  JSValueRef exc;

  script = JSStringCreateWithUTF8CString(dwb.misc.scripts);
  JSEvaluateScript((JSContextRef)context, script, JSContextGetGlobalObject((JSContextRef)context), NULL, 0, &exc);
  JSStringRelease(script);
}/*}}}*/

/* dwb_web_view_title_cb {{{*/
void 
dwb_web_view_title_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  dwb_update_status(gl);
}/*}}}*/

/* dwb_web_view_focus_cb {{{*/
gboolean 
dwb_web_view_focus_cb(WebKitWebView *web, GtkDirectionType *direction, GList *gl) {
  return false;
}/*}}}*/

/* dwb_web_view_load_status_cb {{{*/
void 
dwb_web_view_load_status_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(web);
  gdouble progress = webkit_web_view_get_progress(web);
  gchar *text = NULL;
  switch (status) {
    case WEBKIT_LOAD_PROVISIONAL: 
      break;
    case WEBKIT_LOAD_COMMITTED: 
      break;
    case WEBKIT_LOAD_FINISHED:
      dwb_update_status(gl);
      dwb_prepend_navigation(gl, &dwb.fc.history);
      break;
    case WEBKIT_LOAD_FAILED: 
      break;
    default:
      text = g_strdup_printf("loading [%d%%]", (gint)(progress * 100));
      dwb_set_status_text(gl, text, NULL, NULL); 
      gtk_window_set_title(GTK_WINDOW(dwb.gui.window), text);
      g_free(text);
      break;
  }
}/*}}}*/

// Entry
/* dwb_entry_keyrelease_cb {{{*/
gboolean 
dwb_view_entry_keyrelease_cb(GtkWidget* entry, GdkEventKey *e) { 
  Mode mode = dwb.state.mode;
  if (mode == HintMode) {
    if (DIGIT(e) || e->keyval == GDK_Tab) {
      return true;
    }
    else {
      return dwb_update_hints(e);
    }
  }
  return false;
}/*}}}*/

/* dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {{{*/
gboolean 
dwb_view_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {
  Mode mode = dwb.state.mode;
  if (e->keyval == GDK_BackSpace) {
    return false;
  }
  if (mode == HintMode) {
    if (DIGIT(e) || e->keyval == GDK_Tab) {
      return dwb_update_hints(e);
    }
    else if  (e->keyval == GDK_Return) {
      return true;
    }
  }
  else if (mode == SearchFieldMode) {
    if (e->keyval == GDK_Tab) {
      return dwb_update_hints(e);
    }
    else if (e->keyval == GDK_Return) {
      return false;
    }
  }
  else if (mode == DownloadGetPath) {
    if (e->keyval == GDK_Tab) {
      dwb_comp_complete_download(e->state & GDK_CONTROL_MASK);
      return true;
    }
    else {
      dwb_comp_clean_path_completion();
    }
  }
  else if (mode & CompletionMode && e->keyval != GDK_Tab && !e->is_modifier) {
    dwb_comp_clean_completion();
    return false;
  }
  else if (e->keyval == GDK_Tab) {
    dwb_comp_complete(e->state & GDK_CONTROL_MASK);
    return true;
  }

  if (dwb_eval_editing_key(e)) {
    return true;
  }
  return false;
}/*}}}*/

/* dwb_entry_activate_cb (GtkWidget *entry) {{{*/
gboolean 
dwb_view_entry_activate_cb(GtkEntry* entry) {
  gchar *text = g_strdup(gtk_entry_get_text(entry));
  gboolean ret = false;
  Mode mode = dwb.state.mode;

  if (mode == HintMode) {
    ret = false;
  }
  else if (mode == FindMode) {
    gtk_widget_grab_focus(CURRENT_VIEW()->scroll);
    dwb_search(NULL);
  }
  else if (mode == SearchFieldMode) {
    dwb_submit_searchengine();
  }
  else if (mode == SettingsMode) {
    dwb_parse_setting(GET_TEXT());
  }
  else if (mode == KeyMode) {
    dwb_parse_key_setting(GET_TEXT());
  }
  else if (mode == CommandMode) {
    dwb_parse_command_line(text);
  }
  else if (mode == DownloadGetPath) {
    dwb_dl_start();
  }
  else if (mode == SaveSession) {
    dwb_session_save(GET_TEXT());
  }
  else {
    Arg a = { .n = 0, .p = text };
    dwb_prepend_navigation_with_argument(&dwb.fc.commands, text, NULL);
    dwb_load_uri(&a);
    dwb_normal_mode(true);
  }
  g_free(text);

  return true;
}/*}}}*/
/*}}}*/

/* DWB_WEB_VIEW {{{*/

/* dwb_web_view_add_history_item(GList *gl) {{{*/
void 
dwb_web_view_add_history_item(GList *gl) {
  WebKitWebView *web = WEBVIEW(gl);
  const gchar *uri = webkit_web_view_get_uri(web);
  const gchar *title = webkit_web_view_get_title(web);
  WebKitWebBackForwardList *bl = webkit_web_view_get_back_forward_list(web);
  WebKitWebHistoryItem *hitem = webkit_web_history_item_new_with_data(uri,  title);
  webkit_web_back_forward_list_add_item(bl, hitem);
}/*}}}*/

/* dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, gint fontsize) {{{*/
void 
dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, gint fontsize) {
  View *v = gl->data;
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
    pango_font_description_set_absolute_size(fd, fontsize * PANGO_SCALE);
    gtk_widget_modify_font(v->rstatus, fd);
    gtk_widget_modify_font(v->lstatus, fd);
    gtk_widget_modify_font(v->tablabel, fd);
  }

} /*}}}*/

/* dwb_view_set_active_style (GList *) {{{*/
void 
dwb_view_set_active_style(GList *gl) {
  dwb_view_modify_style(gl, &dwb.color.active_fg, &dwb.color.active_bg, &dwb.color.tab_active_fg, &dwb.color.tab_active_bg, dwb.font.fd_bold, dwb.font.active_size);
}/*}}}*/

/* dwb_view_set_normal_style {{{*/
void 
dwb_view_set_normal_style(GList *gl) {
  dwb_view_modify_style(gl, &dwb.color.normal_fg, &dwb.color.normal_bg, &dwb.color.tab_normal_fg, &dwb.color.tab_normal_bg, dwb.font.fd_bold, dwb.font.normal_size);
}/*}}}*/

/* dwb_web_view_init_signals(View *v) {{{*/
void
dwb_web_view_init_signals(GList *gl) {
  View *v = gl->data;
  g_signal_connect(v->web, "button-press-event",                    G_CALLBACK(dwb_web_view_button_press_cb), gl);
  g_signal_connect(v->web, "close-web-view",                        G_CALLBACK(dwb_web_view_close_web_view_cb), gl);
  g_signal_connect(v->web, "console-message",                       G_CALLBACK(dwb_web_view_console_message_cb), gl);
  g_signal_connect(v->web, "create-web-view",                       G_CALLBACK(dwb_web_view_create_web_view_cb), gl);
  g_signal_connect(v->web, "download-requested",                    G_CALLBACK(dwb_web_view_download_requested_cb), gl);
  g_signal_connect(v->web, "hovering-over-link",                    G_CALLBACK(dwb_web_view_hovering_over_link_cb), gl);
  g_signal_connect(v->web, "mime-type-policy-decision-requested",   G_CALLBACK(dwb_web_view_mime_type_policy_cb), gl);
  g_signal_connect(v->web, "navigation-policy-decision-requested",  G_CALLBACK(dwb_web_view_navigation_policy_cb), gl);
  g_signal_connect(v->web, "new-window-policy-decision-requested",  G_CALLBACK(dwb_web_view_new_window_policy_cb), gl);
  g_signal_connect(v->web, "resource-request-starting",             G_CALLBACK(dwb_web_view_resource_request_cb), gl);
  g_signal_connect(v->web, "script-alert",                          G_CALLBACK(dwb_web_view_script_alert_cb), gl);
  g_signal_connect(v->web, "window-object-cleared",                 G_CALLBACK(dwb_web_view_window_object_cleared_cb), gl);

  g_signal_connect(v->web, "notify::load-status",                   G_CALLBACK(dwb_web_view_load_status_cb), gl);
  g_signal_connect(v->web, "notify::title",                         G_CALLBACK(dwb_web_view_title_cb), gl);
  g_signal_connect(v->web, "focus",                                 G_CALLBACK(dwb_web_view_focus_cb), gl);

  g_signal_connect(v->entry, "key-press-event",                     G_CALLBACK(dwb_view_entry_keypress_cb), NULL);
  g_signal_connect(v->entry, "key-release-event",                   G_CALLBACK(dwb_view_entry_keyrelease_cb), NULL);
  g_signal_connect(v->entry, "activate",                            G_CALLBACK(dwb_view_entry_activate_cb), NULL);
} /*}}}*/

/* dwb_view_create_web_view(View *v)         return: GList * {{{*/
GList * 
dwb_view_create_web_view(GList *gl) {
  View *v = g_malloc(sizeof(View));

  ViewStatus *status = g_malloc(sizeof(ViewStatus));
  status->search_string = NULL;
  status->downloads = NULL;
  v->status = status;


  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();

  // Entry
  v->entry = gtk_entry_new();
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), NULL);

  gtk_widget_modify_font(v->entry, dwb.font.fd_normal);
  gtk_entry_set_has_frame(GTK_ENTRY(v->entry), false);
  gtk_entry_set_inner_border(GTK_ENTRY(v->entry), false);

  // completion
  v->bottombox = gtk_vbox_new(false, 1);

  // Statusbox
  GtkWidget *status_hbox;
  v->statusbox = gtk_event_box_new();
  v->lstatus = gtk_label_new(NULL);
  v->rstatus = gtk_label_new(NULL);

  gtk_misc_set_alignment(GTK_MISC(v->lstatus), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(v->rstatus), 1.0, 0.5);
  gtk_label_set_use_markup(GTK_LABEL(v->lstatus), true);
  gtk_label_set_use_markup(GTK_LABEL(v->rstatus), true);
  gtk_label_set_ellipsize(GTK_LABEL(v->rstatus), PANGO_ELLIPSIZE_MIDDLE);

  status_hbox = gtk_hbox_new(false, 5);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->lstatus, false, false, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->entry, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->rstatus, true, true, 0);
  gtk_container_add(GTK_CONTAINER(v->statusbox), status_hbox);

  gtk_box_pack_end(GTK_BOX(v->bottombox), v->statusbox, false, false, 0);

  // Srolling
  v->scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(v->scroll), v->web);
  WebKitWebFrame *frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  g_signal_connect(frame, "scrollbars-policy-changed", G_CALLBACK(dwb_true), NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(v->scroll), GTK_POLICY_NEVER, GTK_POLICY_NEVER);


  // Tabbar
  v->tabevent = gtk_event_box_new();
  v->tablabel = gtk_label_new(NULL);

  gtk_label_set_use_markup(GTK_LABEL(v->tablabel), true);
  gtk_label_set_width_chars(GTK_LABEL(v->tablabel), 1);
  gtk_misc_set_alignment(GTK_MISC(v->tablabel), 0.0, 0.5);
  gtk_label_set_ellipsize(GTK_LABEL(v->tablabel), PANGO_ELLIPSIZE_END);

  gtk_container_add(GTK_CONTAINER(v->tabevent), v->tablabel);
  gtk_widget_modify_font(v->tablabel, dwb.font.fd_normal);

  gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);

  gtk_box_pack_start(GTK_BOX(v->vbox), v->scroll, true, true, 0);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->bottombox, false, false, 0);

  gtk_box_pack_start(GTK_BOX(dwb.gui.left), v->vbox, true, true, 0);
  
  // Show
  gtk_widget_show(v->vbox);
  gtk_widget_show(v->lstatus);
  gtk_widget_show(v->entry);
  gtk_widget_show(v->rstatus);
  gtk_widget_show(status_hbox);
  gtk_widget_show(v->statusbox);
  gtk_widget_show(v->bottombox);
  gtk_widget_show_all(v->scroll);
  gtk_widget_show_all(v->tabevent);

  gl = g_list_prepend(gl, v);
  dwb_web_view_init_signals(gl);
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(v->web), webkit_web_settings_copy(dwb.state.web_settings));
  // apply settings
  v->setting = dwb_get_default_settings();
  return gl;
} /*}}}*/

/* dwb_add_view(Arg *arg)               return: View *{{{*/
void 
dwb_add_view(Arg *arg) {
  if (dwb.state.views) {
    View *views = dwb.state.views->data;
    CLEAR_COMMAND_TEXT(dwb.state.views);
    gtk_widget_reparent(views->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), views->vbox, 0);
    if (dwb.state.layout & Maximized) {
      gtk_widget_hide(((View *)dwb.state.fview->data)->vbox);
      if (dwb.state.fview != dwb.state.views) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
    }
  }
  dwb.state.views = dwb_view_create_web_view(dwb.state.views);
  dwb_com_focus(dwb.state.views);

  for (GList *l = g_hash_table_get_values(((View*)dwb.state.views->data)->setting); l; l=l->next) {
    WebSettings *s = l->data;
    if (!s->builtin && !s->global) {
      s->func(dwb.state.views, s);
    }
  }
  dwb_update_layout();
  if (arg && arg->p) {
    dwb_load_uri(arg);
  }
  else if (strcmp("about:blank", dwb.misc.startpage)) {
    Arg a = { .p = dwb.misc.startpage }; 
    dwb_load_uri(&a);
  }
} /*}}}*//*}}}*/
