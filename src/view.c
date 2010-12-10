#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h>
#include "dwb.h"
#include "view.h"
#include "commands.h"
#include "completion.h"
#include "util.h"
#include "download.h"
#include "session.h"

static void dwb_parse_setting(const char *);
static void dwb_parse_key_setting(const char *);
static void dwb_apply_settings(WebSettings *);

static char *lasturi;
static GList *allowed_plugins;

typedef struct _Plugin Plugin;

struct _Plugin {
  char *uri;
  Plugin *next;
};
static Plugin *plugins;


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
    Arg a = { .p = clipboard };
    if (e->button == 3) {
      dwb_load_uri(&a);
      ret = true;
    }
    else if (e->button == 2) {
      dwb_add_view(&a);
      ret = true;
    }
    if (clipboard) 
      g_free(clipboard);
  }
  else if (e->button == 1 && e->type == GDK_BUTTON_PRESS) {
    dwb_focus(gl);
  }
  return ret;
}/*}}}*/

/* dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {{{*/
gboolean 
dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {
  Arg a = { .p = gl };
  dwb_com_remove_view(&a);
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
  else if (!(strcmp(sourceid, "_dwb_input_"))) {
    dwb_insert_mode(NULL);
  }
  return true;
}/*}}}*/

/* dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *) {{{*/
static GtkWidget * 
dwb_web_view_create_web_view_cb(WebKitWebView *web, WebKitWebFrame *frame, GList *gl) {
  dwb_add_view(NULL); 
  return ((View*)dwb.state.fview->data)->web;
}/*}}}*/

gboolean 
dwb_view_plugin_blocker_button_cb(GtkWidget *widget, GdkEventButton *e, char *uri) {
  allowed_plugins = g_list_prepend(allowed_plugins, uri);
  GtkWidget *parent = gtk_widget_get_parent(widget);
  gtk_container_remove(GTK_CONTAINER(parent), widget);
  gtk_widget_destroy(widget);
  dwb_com_reload(NULL);
  return true;
}

/* dwb_web_view_create_plugin_widget_cb(WebKitWebView *, WebKitWebFrame *, GList *) {{{*/
static GtkWidget * 
dwb_web_view_create_plugin_widget_cb(WebKitWebView *web, char *mime_type, char *uri, GHashTable *t, GList *gl) {
  GtkWidget *event_box = NULL;
  View *v = gl->data;

  for (GList *l = allowed_plugins; l; l=l->next) {
    if (!strcmp(uri, l->data)) {
      return NULL;
    }
  }
  if (v->status->plugin_blocker) {
    lasturi = g_strdup(uri);
    event_box = gtk_event_box_new();

    Plugin *p = malloc(sizeof(Plugin));
    p->uri = lasturi;
    p->next = plugins;
    plugins = p;

    GtkWidget *label = gtk_label_new("Plugin blocked");
    gtk_container_add(GTK_CONTAINER(event_box), label);
    g_signal_connect(G_OBJECT(event_box), "button-press-event", G_CALLBACK(dwb_view_plugin_blocker_button_cb), lasturi);

    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &dwb.color.active_fg);
    gtk_widget_modify_bg(event_box, GTK_STATE_NORMAL, &dwb.color.active_bg);
    PangoFontDescription *fd = dwb.font.fd_bold;
    pango_font_description_set_absolute_size(fd, dwb.font.active_size * PANGO_SCALE);
    gtk_widget_modify_font(label, fd);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);

    gtk_widget_show_all(event_box);
  }
  return event_box;
}/*}}}*/

/* dwb_web_view_download_requested_cb(WebKitWebView *, WebKitDownload *, GList *) {{{*/
static gboolean 
dwb_web_view_download_requested_cb(WebKitWebView *web, WebKitDownload *download, GList *gl) {
  dwb_dl_get_path(gl, download);
  return true;
}/*}}}*/

/* dwb_web_view_inspect_web_view_cb(WebKitWebInspector *, WebKitWebView *, GList * *){{{*/
WebKitWebView * 
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
    dwb_set_status_bar_text(v->rstatus, uri, NULL, NULL);
  }
  else {
    dwb_update_status_text(gl, NULL);
  }
}/*}}}*/

/* dwb_web_view_mime_type_policy_cb {{{*/
static gboolean 
dwb_web_view_mime_type_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, char *mimetype, WebKitWebPolicyDecision *policy, GList *gl) {
  View *v = gl->data;

  v->status->mimetype = g_strdup(mimetype);

  if (!webkit_web_view_can_show_mime_type(web, mimetype) ||  dwb.state.nv == OpenDownload) {
    dwb.state.mimetype_request = g_strdup(mimetype);
    webkit_web_policy_decision_download(policy);
    return true;
  }
  return  false;
}/*}}}*/

/* dwb_web_view_enter_notify_cb(GtkWidget *, GdkEventCrossing *, GList *){{{*/
static gboolean 
dwb_web_view_enter_notify_cb(GtkWidget *web, GdkEventCrossing *e, GList *gl) {
  dwb_focus(gl);
  return false;
}/*}}}*/

/* dwb_web_view_navigation_policy_cb {{{*/
static gboolean 
dwb_web_view_navigation_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {

  int button = webkit_web_navigation_action_get_button(action);
  Arg a = { .p = (char*)webkit_network_request_get_uri(request) };
  if (button != -1) {
    if (button == 2) {
        dwb_add_view(&a);
        webkit_web_policy_decision_ignore(policy);
        return true;
    }
  }

  if (dwb.state.nv == OpenNewView || dwb.state.nv == OpenNewWindow) {
    char *uri = (char *)webkit_network_request_get_uri(request);
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
  const char *request_uri = webkit_network_request_get_uri(request);
  WebKitWebNavigationReason reason = webkit_web_navigation_action_get_reason(action);

  if (reason == WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED) {
    if (dwb.state.mode == InsertMode) {
      dwb_normal_mode(true);
    }
    if (dwb.state.mode == SearchFieldMode) {
      webkit_web_policy_decision_ignore(policy);
      dwb.state.search_engine = dwb.state.form_name && !g_strrstr(request_uri, HINT_SEARCH_SUBMIT) 
        ? g_strdup_printf("%s?%s=%s", request_uri, dwb.state.form_name, HINT_SEARCH_SUBMIT) 
        : g_strdup(request_uri);
      dwb_save_searchengine();
      return true;
    }
  }
  return false;
}/*}}}*/

/* dwb_web_view_new_window_policy_cb {{{*/
static gboolean 
dwb_web_view_new_window_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action, WebKitWebPolicyDecision *policy, GList *gl) {
  return false;
}/*}}}*/

/* dwb_web_view_resource_request_cb{{{*/
static void 
dwb_web_view_resource_request_cb(WebKitWebView *web, WebKitWebFrame *frame,
    WebKitWebResource *resource, WebKitNetworkRequest *request,
    WebKitNetworkResponse *response, GList *gl) {
  SoupMessage *msg = webkit_network_request_get_message(request);

  if (!msg) 
    return;

  static SoupBuffer buffer;
  SoupContentSniffer *sniffer = soup_content_sniffer_new();
  View *v = gl->data;

  if (v && v->status) {
    const char *content_type = soup_content_sniffer_sniff(sniffer, msg, &buffer, NULL);
    if (!v->status->current_host) {
      SoupURI *uri = soup_message_get_uri(msg);
      v->status->current_host = g_strdup(uri->host);
      v->status->block_current = 
        !dwb_get_host_blocked(dwb.fc.content_block_allow, v->status->current_host) && !dwb_get_host_blocked(dwb.fc.content_allow, v->status->current_host) 
        ? true : false;
    }
    if (v->status->block && v->status->block_current && g_regex_match_simple(dwb.misc.content_block_regex, content_type, 0, 0)) {
      webkit_network_request_set_uri(request, "about:blank");
      v->status->items_blocked++;
    }
  }
}/*}}}*/

/* dwb_web_view_script_alert_cb {{{*/
static gboolean 
dwb_web_view_script_alert_cb(WebKitWebView *web, WebKitWebFrame *frame, char *message, GList *gl) {
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_window_object_cleared_cb {{{*/
static void 
dwb_web_view_window_object_cleared_cb(WebKitWebView *web, WebKitWebFrame *frame, 
    JSGlobalContextRef *context, JSObjectRef *object, GList *gl) {
  dwb_execute_script(NULL, false);
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

/* dwb_web_view_load_status_cb {{{*/
static void 
dwb_web_view_load_status_cb(WebKitWebView *web, GParamSpec *pspec, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(web);
  double progress = webkit_web_view_get_progress(web);
  char *text = NULL;

  switch (status) {
    case WEBKIT_LOAD_PROVISIONAL: 
      dwb_view_clean_vars(gl);
      break;
    case WEBKIT_LOAD_COMMITTED: 
      break;
    case WEBKIT_LOAD_FINISHED:
      if (dwb.state.fview)
        dwb_update_status(gl);
      dwb_prepend_navigation(gl, &dwb.fc.history);
      dwb_clean_load_end(gl);
      break;
    case WEBKIT_LOAD_FAILED: 
      dwb_clean_load_end(gl);
      break;
    default:
      text = g_strdup_printf("loading [%d%%]", (int)(progress * 100));
      dwb_set_status_bar_text(VIEW(gl)->rstatus, text, NULL, NULL); 
      gtk_window_set_title(GTK_WINDOW(dwb.gui.window), text);
      g_free(text);
      break;
  }
}/*}}}*/

/* dwb_web_view_realize_cb {{{*/
void
dwb_web_view_realize_cb(GtkWidget *widget, GList *gl) {
  GdkWindow *window = gtk_widget_get_window(widget);
  GdkEventMask events = gdk_window_get_events(window);
  gdk_window_set_events(window, events | GDK_ENTER_NOTIFY_MASK);
}/*}}}*/

// Entry
/* dwb_entry_keyrelease_cb {{{*/
static gboolean 
dwb_view_entry_keyrelease_cb(GtkWidget* entry, GdkEventKey *e) { 
  Mode mode = dwb.state.mode;
  if (mode == HintMode) {
    if (DIGIT(e) || DWB_TAB_KEY(e)) {
      return true;
    }
    else {
      return dwb_update_hints(e);
    }
  }
  else if (mode == FindMode) {
    dwb_update_search(dwb.state.forward_search);
    return false;
  }
  return false;
}/*}}}*/

/* dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {{{*/
static gboolean 
dwb_view_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {
  Mode mode = dwb.state.mode;
  gboolean ret = false;
  if (e->keyval == GDK_BackSpace) {
    return false;
  }
  if (mode == HintMode) {
    if (DIGIT(e) || DWB_TAB_KEY(e)) {
      return dwb_update_hints(e);
    }
    else if  (e->keyval == GDK_Return) {
      return true;
    }
  }
  else if (mode == SearchFieldMode) {
    if (DWB_TAB_KEY(e)) {
      return dwb_update_hints(e);
    }
    else if (e->keyval == GDK_Return) {
      return false;
    }
  }
  else if (mode == DownloadGetPath) {
    if (DWB_TAB_KEY(e)) {
      dwb_comp_complete_download(e->state & GDK_SHIFT_MASK);
      return true;
    }
    else {
      dwb_comp_clean_path_completion();
    }
  }
  else if (mode & CompletionMode && e->keyval != GDK_Tab && e->keyval != GDK_ISO_Left_Tab && !e->is_modifier) {
    dwb_comp_clean_completion();
  }
  else if (mode == FindMode) {
    return false;
  }
  else if (DWB_TAB_KEY(e)) {
    dwb_comp_complete(e->state & GDK_SHIFT_MASK);
    return true;
  }
  if (dwb_eval_editing_key(e)) {
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_entry_activate_cb (GtkWidget *entry) {{{*/
static gboolean 
dwb_view_entry_activate_cb(GtkEntry* entry) {
  char *text = g_strdup(gtk_entry_get_text(entry));
  gboolean ret = false;
  Mode mode = dwb.state.mode;

  if (mode == HintMode) {
    ret = false;
  }
  else if (mode == FindMode) {
    dwb_focus_scroll(dwb.state.fview);
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
    dwb_end();
  }
  else {
    Arg a = { .n = 0, .p = text };
    dwb_load_uri(&a);
    dwb_prepend_navigation_with_argument(&dwb.fc.commands, text, NULL);
    dwb_normal_mode(true);
  }
  g_free(text);

  return true;
}/*}}}*/
/*}}}*/

/* dwb_view_tab_button_press_cb(GtkWidget, GdkEventButton* , GList * ){{{*/
gboolean
dwb_view_tab_button_press_cb(GtkWidget *tabevent, GdkEventButton *e, GList *gl) {
  if (e->button == 1 && e->type == GDK_BUTTON_PRESS) {
    Arg a = { .p = gl };
    dwb_focus(gl);
    dwb_com_push_master(&a);
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
dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, int fontsize) {
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
static void
dwb_web_view_init_signals(GList *gl) {
  View *v = gl->data;
  GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  g_signal_connect(v->web, "button-press-event",                    G_CALLBACK(dwb_web_view_button_press_cb), gl);
  g_signal_connect(v->web, "close-web-view",                        G_CALLBACK(dwb_web_view_close_web_view_cb), gl);
  g_signal_connect(v->web, "console-message",                       G_CALLBACK(dwb_web_view_console_message_cb), gl);
  g_signal_connect(v->web, "create-web-view",                       G_CALLBACK(dwb_web_view_create_web_view_cb), gl);
  g_signal_connect(v->web, "create-plugin-widget",                  G_CALLBACK(dwb_web_view_create_plugin_widget_cb), gl);
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
  g_signal_connect(v->web, "scroll-event",                          G_CALLBACK(dwb_web_view_scroll_cb), gl);
  g_signal_connect(a,      "value-changed",                         G_CALLBACK(dwb_web_view_value_changed_cb), gl);

  g_signal_connect(v->entry, "key-press-event",                     G_CALLBACK(dwb_view_entry_keypress_cb), NULL);
  g_signal_connect(v->entry, "key-release-event",                   G_CALLBACK(dwb_view_entry_keyrelease_cb), NULL);
  g_signal_connect(v->entry, "activate",                            G_CALLBACK(dwb_view_entry_activate_cb), NULL);

  g_signal_connect(v->tabevent, "button-press-event",               G_CALLBACK(dwb_view_tab_button_press_cb), gl);
  g_signal_connect(v->web,    "enter-notify-event",                   G_CALLBACK(dwb_web_view_enter_notify_cb), gl);
} /*}}}*/

/* dwb_view_clean_vars(GList *){{{*/
void
dwb_view_clean_vars(GList *gl) {
  View *v = gl->data;

  if (v->status->plugin_blocker) {
    for (Plugin *p = plugins; p; p = p->next) {
      g_free(p->uri);
      g_free(p);
    }
    plugins = NULL;
  }

  v->status->items_blocked = 0;
  if (v->status->current_host) {
    g_free(v->status->current_host); 
    v->status->current_host = NULL;
  }
}/*}}}*/

/* dwb_view_create_web_view(View *v)         return: GList * {{{*/
static GList * 
dwb_view_create_web_view(GList *gl) {
  View *v = g_malloc(sizeof(View));

  ViewStatus *status = g_malloc(sizeof(ViewStatus));
  status->search_string = NULL;
  status->downloads = NULL;
  status->current_host = NULL;
  status->custom_encoding = false;
  v->status = status;


  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();
  g_signal_connect(v->web, "realize", G_CALLBACK(dwb_web_view_realize_cb), gl);

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

  status_hbox = gtk_hbox_new(false, 2);
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
  for (GList *l = g_hash_table_get_values(v->setting); l; l=l->next) {
    WebSettings *s = l->data;
    if (!s->builtin && !s->global) {
      s->func(gl, s);
    }
  }
  dwb_view_clean_vars(gl);
  return gl;
} /*}}}*/


void 
dwb_view_new_reorder() {
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
}

/* dwb_add_view(Arg *arg)               return: View *{{{*/
GList *  
dwb_add_view(Arg *arg) {
  dwb_view_new_reorder();
  dwb.state.views = dwb_view_create_web_view(dwb.state.views);
  dwb_focus(dwb.state.views);
  dwb_execute_script(NULL, false);

  dwb_update_layout();
  if (arg && arg->p) {
    dwb_load_uri(arg);
  }
  else if (strcmp("about:blank", dwb.misc.startpage)) {
    Arg a = { .p = dwb.misc.startpage }; 
    dwb_load_uri(&a);
  }
  return dwb.state.views;
} /*}}}*/

GList *
dwb_add_view_new_with_webview(void) {
  dwb_view_new_reorder();
  return NULL;
}

/* dwb_parse_setting(const char *){{{*/
void
dwb_parse_setting(const char *text) {
  WebSettings *s;
  Arg *a = NULL;
  char **token = g_strsplit(text, " ", 2);

  GHashTable *t = dwb.state.setting_apply == Global ? dwb.settings : ((View*)dwb.state.fview->data)->setting;
  if (token[0]) {
    if  ( (s = g_hash_table_lookup(t, token[0])) ) {
      if ( (a = dwb_util_char_to_arg(token[1], s->type)) || (s->type == Char && a->p == NULL)) {
        s->arg = *a;
        dwb_apply_settings(s);
        dwb_set_normal_message(dwb.state.fview, true, "Saved setting %s: %s", s->n.first, s->type == Boolean ? ( s->arg.b ? "true" : "false") : token[1]);
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
  if (dwb.state.setting_apply == Global) {
    new = g_hash_table_lookup(dwb.settings, s->n.first);
    new->arg = s->arg;
    for (GList *l = dwb.state.views; l; l=l->next)  {
      WebSettings *new =  g_hash_table_lookup((((View*)l->data)->setting), s->n.first);
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
