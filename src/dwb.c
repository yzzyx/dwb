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

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>
#include <JavaScriptCore/JavaScript.h>
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
#ifdef DWB_ADBLOCKER
#include "adblock.h"
#include "domain.h"
#endif

/* DECLARATIONS {{{*/
static void dwb_webkit_setting(GList *, WebSettings *);
static void dwb_webview_property(GList *, WebSettings *);
static void dwb_set_background_tab(GList *, WebSettings *);
static void dwb_set_scripts(GList *, WebSettings *);
static void dwb_set_user_agent(GList *, WebSettings *);
static void dwb_set_startpage(GList *, WebSettings *);
static void dwb_set_message_delay(GList *, WebSettings *);
static void dwb_set_history_length(GList *, WebSettings *);
static void dwb_set_plugin_blocker(GList *, WebSettings *);
static void dwb_set_sync_interval(GList *, WebSettings *);
static void dwb_set_private_browsing(GList *, WebSettings *);
static void dwb_set_cookies(GList *, WebSettings *);
static DwbStatus dwb_set_widget_packing(GList *, WebSettings *);
static DwbStatus dwb_set_cookie_accept_policy(GList *, WebSettings *);
static void dwb_reload_scripts(GList *, WebSettings *);
static void dwb_set_single_instance(GList *, WebSettings *);
static Navigation * dwb_get_search_completion_from_navigation(Navigation *);
static gboolean dwb_sync_history(gpointer);
static void dwb_save_key_value(const char *file, const char *key, const char *value);
static DwbStatus dwb_pack(const char *layout, gboolean rebuild);

static void dwb_reload_layout(GList *,  WebSettings *);
static char * dwb_test_userscript(const char *);

static void dwb_open_channel(const char *);
static void dwb_open_si_channel(void);
static gboolean dwb_handle_channel(GIOChannel *c, GIOCondition condition, void *data);


static void dwb_init_key_map(void);
static void dwb_init_settings(void);
static void dwb_init_style(void);
static void dwb_apply_style(void);
static void dwb_init_gui(void);
static void dwb_init_vars(void);

static Navigation * dwb_get_search_completion(const char *text);

static void dwb_clean_vars(void);
typedef struct _EditorInfo {
  char *filename;
  char *id;
  GList *gl;
  WebKitDOMElement *element;
  char *tagname;
} EditorInfo;

static int signals[] = { SIGFPE, SIGILL, SIGINT, SIGQUIT, SIGTERM, SIGALRM, SIGSEGV};
static int MAX_COMPLETIONS = 11;
static char *restore = NULL;
/*}}}*/

#include "config.h"

/* SETTINGS_FUNCTIONS{{{*/
/* dwb_set_plugin_blocker {{{*/
static void
dwb_set_plugin_blocker(GList *gl, WebSettings *s) {
  View *v = gl->data;
  if (s->arg.b) {
    plugins_connect(gl);
    v->plugins->status ^= (v->plugins->status & PLUGIN_STATUS_DISABLED) | PLUGIN_STATUS_ENABLED;
  }
  else {
    plugins_disconnect(gl);
    v->plugins->status ^= (v->plugins->status & PLUGIN_STATUS_ENABLED) | PLUGIN_STATUS_DISABLED;
  }
}/*}}}*/

#ifdef DWB_ADBLOCKER
/* dwb_set_adblock {{{*/
void
dwb_set_adblock(GList *gl, WebSettings *s) {
  if (s->arg.b) {
    for (GList *l = dwb.state.views; l; l=l->next) 
      adblock_connect(l);
  }
  else {
    for (GList *l = dwb.state.views; l; l=l->next) 
      adblock_disconnect(l);
  }
}/*}}}*/
#endif

/* dwb_set_cookies */
static void
dwb_set_cookies(GList *gl, WebSettings *s) {
  dwb.state.cookie_store_policy = dwb_soup_get_cookie_store_policy(s->arg.p);
}/*}}}*/

/* dwb_set_cookies */
static DwbStatus
dwb_set_widget_packing(GList *gl, WebSettings *s) {
  DwbStatus ret = STATUS_OK;
  if (dwb_pack(s->arg.p, true) != STATUS_OK) {
    g_free(s->arg.p);
    s->arg.p = g_strdup(DEFAULT_WIDGET_PACKING);
    ret = STATUS_ERROR;
  }
  return ret;
}/*}}}*/

/* dwb_set_private_browsing  {{{ */
static void
dwb_set_private_browsing(GList *gl, WebSettings *s) {
  dwb.misc.private_browsing = s->arg.b;
  dwb_webkit_setting(gl, s);
}/*}}}*/

/* dwb_set_cookie_accept_policy {{{ */
static DwbStatus
dwb_set_cookie_accept_policy(GList *gl, WebSettings *s) {
  if (dwb_soup_set_cookie_accept_policy(s->arg.p) == STATUS_ERROR) {
    s->arg.p = g_strdup("always");
    return STATUS_ERROR;
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_set_sync_interval{{{*/
static void
dwb_set_sync_interval(GList *gl, WebSettings *s) {
  if (dwb.misc.synctimer > 0) {
    g_source_remove(dwb.misc.synctimer);
    dwb.misc.synctimer = 0;
  }

  if (s->arg.i > 0) {
    dwb.misc.synctimer = g_timeout_add_seconds(s->arg.i, dwb_sync_history, NULL);
  }
}/*}}}*/

/* dwb_set_startpage(GList *l, WebSettings *){{{*/
static void 
dwb_set_startpage(GList *l, WebSettings *s) {
  dwb.misc.startpage = s->arg.p;
}/*}}}*/

/* dwb_set_message_delay(GList *l, WebSettings *){{{*/
static void 
dwb_set_message_delay(GList *l, WebSettings *s) {
  dwb.misc.message_delay = s->arg.i;
}/*}}}*/

/* dwb_set_history_length(GList *l, WebSettings *){{{*/
static void 
dwb_set_history_length(GList *l, WebSettings *s) {
  dwb.misc.history_length = s->arg.i;
}/*}}}*/

/* dwb_set_background_tab (GList *, WebSettings *s) {{{*/
static void 
dwb_set_background_tab(GList *l, WebSettings *s) {
  dwb.state.background_tabs = s->arg.b;
}/*}}}*/

/* dwb_set_single_instance(GList *l, WebSettings *s){{{*/
static void
dwb_set_single_instance(GList *l, WebSettings *s) {
  if (!s->arg.b) {
    if (dwb.misc.si_channel) {
      g_io_channel_shutdown(dwb.misc.si_channel, true, NULL);
      g_io_channel_unref(dwb.misc.si_channel);
      dwb.misc.si_channel = NULL;
    }
  }
  else if (!dwb.misc.si_channel) {
    dwb_open_si_channel();
  }
}/*}}}*/

/* dwb_set_proxy{{{*/
void
dwb_set_proxy(GList *l, WebSettings *s) {
  if (s->arg.b) {
    SoupURI *uri = soup_uri_new(dwb.misc.proxyuri);
    g_object_set(dwb.misc.soupsession, "proxy-uri", uri, NULL);
    soup_uri_free(uri);
  }
  else  {
    g_object_set(dwb.misc.soupsession, "proxy-uri", NULL, NULL);
  }
  dwb_set_normal_message(dwb.state.fview, true, "Set setting proxy: %s", s->arg.b ? "true" : "false");
}/*}}}*/

/* dwb_set_scripts {{{*/
void
dwb_set_scripts(GList *gl, WebSettings *s) {
  dwb_webkit_setting(gl, s);
  View *v = VIEW(gl);
  if (s->arg.b) 
    v->status->scripts = SCRIPTS_ALLOWED;
  else 
    v->status->scripts = SCRIPTS_BLOCKED;
}/*}}}*/

/* dwb_set_user_agent {{{*/
void
dwb_set_user_agent(GList *gl, WebSettings *s) {
  char *ua = s->arg.p;
  if (! ua) {
    char *current_ua;
    g_object_get(dwb.state.web_settings, "user-agent", &current_ua, NULL);
    s->arg.p = g_strdup_printf("%s %s/%s", current_ua, NAME, VERSION);
  }
  dwb_webkit_setting(gl, s);
  g_hash_table_insert(dwb.settings, g_strdup("user-agent"), s);
}/*}}}*/


/* dwb_webkit_setting(GList *gl WebSettings *s) {{{*/
static void
dwb_webkit_setting(GList *gl, WebSettings *s) {
  WebKitWebSettings *settings = gl ? webkit_web_view_get_settings(WEBVIEW(gl)) : dwb.state.web_settings;
  switch (s->type) {
    case DOUBLE:  g_object_set(settings, s->n.first, s->arg.d, NULL); break;
    case INTEGER: g_object_set(settings, s->n.first, s->arg.i, NULL); break;
    case BOOLEAN: g_object_set(settings, s->n.first, s->arg.b, NULL); break;
    case CHAR:    g_object_set(settings, s->n.first, !s->arg.p || !g_strcmp0(s->arg.p, "null") ? NULL : (char*)s->arg.p  , NULL); break;
    default: return;
  }
}/*}}}*/

/* dwb_webview_property(GList, WebSettings){{{*/
static void
dwb_webview_property(GList *gl, WebSettings *s) {
  WebKitWebView *web = gl ? WEBVIEW(gl) : CURRENT_WEBVIEW();
  switch (s->type) {
    case DOUBLE:  g_object_set(web, s->n.first, s->arg.d, NULL); break;
    case INTEGER: g_object_set(web, s->n.first, s->arg.i, NULL); break;
    case BOOLEAN: g_object_set(web, s->n.first, s->arg.b, NULL); break;
    case CHAR:    g_object_set(web, s->n.first, (char*)s->arg.p, NULL); break;
    default: return;
  }
}/*}}}*/

/*dwb_reload_scripts {{{  */
static void 
dwb_reload_scripts(GList *gl, WebSettings *s) {
  dwb_init_scripts();
} /*}}}*/
/*}}}*/

/* COMMAND_TEXT {{{*/

/* dwb_set_status_bar_text(GList *gl, const char *text, GdkColor *fg,  PangoFontDescription *fd) {{{*/
void
dwb_set_status_bar_text(GtkWidget *label, const char *text, DwbColor *fg,  PangoFontDescription *fd, gboolean markup) {
  if (markup) {
    char *escaped =  g_markup_escape_text(text, -1);
    gtk_label_set_markup(GTK_LABEL(label), text);
    g_free(escaped);
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
  CLEAR_COMMAND_TEXT();
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
  vsnprintf(message, STRING_LENGTH, text, arg_list);
  va_end(arg_list);

  dwb_set_status_bar_text(dwb.gui.lstatus, message, &dwb.color.active_fg, dwb.font.fd_active, false);

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
  vsnprintf(message, STRING_LENGTH, error, arg_list);
  va_end(arg_list);

  dwb_source_remove(dwb.state.fview);

  dwb_set_status_bar_text(dwb.gui.lstatus, message, &dwb.color.error, dwb.font.fd_active, false);
  dwb.state.message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, NULL);
  gtk_widget_hide(dwb.gui.entry);
}/*}}}*/

void 
dwb_update_uri(GList *gl) {
  if (gl != dwb.state.fview)
    return;
  View *v = VIEW(gl);

  const char *uri = webkit_web_view_get_uri(CURRENT_WEBVIEW());
  DwbColor *uricolor;
  switch(v->status->ssl) {
    case SSL_TRUSTED:   uricolor = &dwb.color.ssl_trusted; break;
    case SSL_UNTRUSTED: uricolor = &dwb.color.ssl_untrusted; break;
    default:            uricolor = &dwb.color.active_fg; break;
  }
  dwb_set_status_bar_text(dwb.gui.urilabel, uri, uricolor, NULL, false);
}

/* dwb_update_status_text(GList *gl) {{{*/
void 
dwb_update_status_text(GList *gl, GtkAdjustment *a) {
  View *v = gl ? gl->data : dwb.state.fview->data;
    
  if (!a) {
    a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  }
  dwb_update_uri(gl);
  GString *string = g_string_new(NULL);

  gboolean back = webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(v->web));
  gboolean forward = webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(v->web));
  const char *bof = back && forward ? " [+-]" : back ? " [+]" : forward  ? " [-]" : " ";
  g_string_append(string, bof);

  if (a) {
    double lower = gtk_adjustment_get_lower(a);
    double upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
    double value = gtk_adjustment_get_value(a); 
    char *position = 
      upper == lower ? g_strdup("[all]") : 
      value == lower ? g_strdup("[top]") : 
      value == upper ? g_strdup("[bot]") : 
      g_strdup_printf("[%02d%%]", (int)(value * 100/upper + 0.5));
    g_string_append(string, position);
    FREE(position);
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
  if (v->status->progress != 0) {
    wchar_t buffer[PBAR_LENGTH + 1] = { 0 };
    wchar_t cbuffer[PBAR_LENGTH] = { 0 };
    int length = PBAR_LENGTH * v->status->progress / 100;
    wmemset(buffer, 0x2588, length - 1);
    buffer[length] = 0x258f - (int)((v->status->progress % 10) / 10.0*8);
    wmemset(cbuffer, 0x2581, PBAR_LENGTH-length-1);
    cbuffer[PBAR_LENGTH - length] = '\0';
    g_string_append_printf(string, "\u2595%ls%ls\u258f", buffer, cbuffer);
  }
  if (string->len > 0) 
    dwb_set_status_bar_text(dwb.gui.rstatus, string->str, NULL, NULL, true);
  g_string_free(string, true);
}/*}}}*/

/*}}}*/

/* FUNCTIONS {{{*/

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

/* dwb_set_clipboard (const char *, GdkAtom) {{{*/
DwbStatus
dwb_set_clipboard(const char *text, GdkAtom atom) {
  GtkClipboard *clipboard = gtk_clipboard_get(atom);
  gboolean ret = STATUS_ERROR;

  gtk_clipboard_set_text(clipboard, text, -1);
  if (*text) {
    dwb_set_normal_message(dwb.state.fview, true, "Yanked: %s", text);
    ret = STATUS_OK;
  }
  return ret;
}/*}}}*/

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
  char *content = util_get_file_content(info->filename);
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

  if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(e)) 
    webkit_dom_html_input_element_set_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), content);
  if (WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(e)) 
    webkit_dom_html_text_area_element_set_value(WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(e), content);

clean:
  unlink(info->filename);
  g_free(info->filename);
  FREE(info->id);
  g_free(info);
}/*}}}*/

/* dwb_get_active_input(WebKitDOMDocument )  {{{*/
static WebKitDOMElement *
dwb_get_active_input(WebKitDOMDocument *doc) {
  WebKitDOMElement *ret = NULL;
  WebKitDOMDocument *d = NULL;
  WebKitDOMElement *active = webkit_dom_html_document_get_active_element(WEBKIT_DOM_HTML_DOCUMENT(doc));
  char *tagname = webkit_dom_element_get_tag_name(active);
  if (! g_strcmp0(tagname, "FRAME")) {
    d = webkit_dom_html_frame_element_get_content_document(WEBKIT_DOM_HTML_FRAME_ELEMENT(active));
    ret = dwb_get_active_input(d);
  }
  else if (! g_strcmp0(tagname, "IFRAME")) {
    d = webkit_dom_html_iframe_element_get_content_document(WEBKIT_DOM_HTML_IFRAME_ELEMENT(active));
    ret = dwb_get_active_input(d);
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
  WebKitDOMElement *active = dwb_get_active_input(doc);
  
  if (active == NULL) 
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
  FREE(value);

  return ret;
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
  dwb_remove_navigation_item(&dwb.fc.bookmarks, line, dwb.files.bookmarks);
}
void
dwb_remove_download(const char *line) {
  dwb_remove_navigation_item(&dwb.fc.downloads, line, dwb.files.bookmarks);
}
void
dwb_remove_history(const char *line) {
  dwb_remove_navigation_item(&dwb.fc.history, line, dwb.misc.synctimer <= 0 ? dwb.files.history : NULL);
}
void 
dwb_remove_quickmark(const char *line) {
  Quickmark *q = dwb_quickmark_new_from_line(line);
  GList *item = g_list_find_custom(dwb.fc.quickmarks, q, (GCompareFunc)util_quickmark_compare);
  dwb_quickmark_free(q);
  if (item) {
    util_file_remove_line(dwb.files.quickmarks, line);
    dwb.fc.quickmarks = g_list_delete_link(dwb.fc.quickmarks, item);
  }
}/*}}}*/

/* dwb_sync_history {{{*/
static gboolean
dwb_sync_history(gpointer data) {
  GString *buffer = g_string_new(NULL);
  for (GList *gl = dwb.fc.history; gl; gl=gl->next) {
    Navigation *n = gl->data;
    g_string_append_printf(buffer, "%s %s\n", n->first, n->second);
  }
  g_file_set_contents(dwb.files.history, buffer->str, -1, NULL);
  g_string_free(buffer, true);
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
  if (!dwb.misc.startpage) 
    return STATUS_ERROR;
  if (gl == NULL) 
    gl = dwb.state.fview;

  dwb_load_uri(gl, dwb.misc.startpage);
  return STATUS_OK;
}/*}}}*/

/* dwb_apply_settings(WebSettings *s) {{{*/
static DwbStatus
dwb_apply_settings(WebSettings *s) {
  DwbStatus ret = STATUS_OK;
  if (s->apply & SETTING_ONINIT) 
    return ret;
  else if (s->apply & SETTING_GLOBAL) {
    if (s->func) 
      ret = s->func(NULL, s);
  }
  else {
    for (GList *l = dwb.state.views; l; l=l->next) {
      if (s->func) 
        s->func(l, s);
    }
  }
  dwb_change_mode(NORMAL_MODE, false);
  return ret;
}/*}}}*/

/* dwb_set_setting(const char *){{{*/
void
dwb_set_setting(const char *key, char *value) {
  WebSettings *s;
  Arg *a = NULL;

  GHashTable *t = dwb.settings;
  if (key) {
    if  ( (s = g_hash_table_lookup(t, key)) ) {
      if ( (a = util_char_to_arg(value, s->type))) {
        s->arg = *a;
        if (dwb_apply_settings(s) != STATUS_ERROR) {
          dwb_set_normal_message(dwb.state.fview, true, "Saved setting %s: %s", s->n.first, s->type == BOOLEAN ? ( s->arg.b ? "true" : "false") : value);
          dwb_save_key_value(dwb.files.settings, key, value);
        }
        else {
          dwb_set_error_message(dwb.state.fview, "Error setting value.");
        }
      }
      else {
        dwb_set_error_message(dwb.state.fview, "No valid value.");
      }
    }
    else {
      dwb_set_error_message(dwb.state.fview, "No such setting: %s", key);
    }
  }
  dwb_change_mode(NORMAL_MODE, false);


}/*}}}*/

/* dwb_set_key(const char *prop, char *val) {{{*/
void
dwb_set_key(const char *prop, char *val) {
  KeyValue value;

  value.id = g_strdup(prop);
  if (val)
    value.key = dwb_str_to_key(val); 
  else {
    Key key = { NULL, 0 };
    value.key = key;
  }

  dwb_set_normal_message(dwb.state.fview, true, "Saved key for command %s: %s", prop, val ? val : "");

  dwb.keymap = dwb_keymap_add(dwb.keymap, value);
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)util_keymap_sort_second);
  dwb_save_key_value(dwb.files.keys, prop, val);

  dwb_change_mode(NORMAL_MODE, false);
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

#if 0 
  /*  this sometimes segfaults */
  const char *host = NULL;
  WebKitSecurityOrigin *origin = webkit_web_frame_get_security_origin(webkit_web_view_get_main_frame(web));
  if (origin) {
    host = webkit_security_origin_get_host(origin);
  }
  return host;
#endif
}/*}}}*/

/* dwb_focus_view(GList *gl){{{*/
void
dwb_focus_view(GList *gl) {
  if (gl != dwb.state.fview) {
    gtk_widget_show(VIEW(gl)->scroll);
    if (! (CURRENT_VIEW()->status->lockprotect & LP_VISIBLE) )
      gtk_widget_hide(VIEW(dwb.state.fview)->scroll);
    dwb_unfocus();
    dwb_focus(gl);
  }
}/*}}}*/

/* dwb_get_allowed(const char *filename, const char *data) {{{*/
gboolean 
dwb_get_allowed(const char *filename, const char *data) {
  char *content;
  gboolean ret = false;
  g_file_get_contents(filename, &content, NULL, NULL);
  if (content) {
    char **lines = g_strsplit(content, "\n", -1);
    for (int i=0; i<g_strv_length(lines); i++) {
      if (!g_strcmp0(lines[i], data)) {
        ret = true; 
        break;
      }
    }
    g_strfreev(lines);
    FREE(content);
  }
  return ret;
}/*}}}*/

/* dwb_toggle_allowed(const char *filename, const char *data) {{{*/
gboolean 
dwb_toggle_allowed(const char *filename, const char *data) {
  char *content = NULL;
  if (!data)
    return false;
  gboolean allowed = dwb_get_allowed(filename, data);
  g_file_get_contents(filename, &content, NULL, NULL);
  GString *buffer = g_string_new(NULL);
  if (!allowed) {
    if (content) {
      g_string_append(buffer, content);
    }
    g_string_append_printf(buffer, "%s\n", data);
  }
  else if (content) {
    char **lines = g_strsplit(content, "\n", -1);
    for (int i=0; i<g_strv_length(lines); i++) {
      if (strlen(lines[i]) && g_strcmp0(lines[i], data)) {
        g_string_append_printf(buffer, "%s\n", lines[i]);
      }
    }
  }
  g_file_set_contents(filename, buffer->str, -1, NULL);

  FREE(content);
  g_string_free(buffer, true);

  return !allowed;
}/*}}}*/

void
dwb_reload(void) {
  const char *path = webkit_web_view_get_uri(CURRENT_WEBVIEW());
  if ( !local_check_directory(dwb.state.fview, path, false, NULL) ) {
    webkit_web_view_reload(CURRENT_WEBVIEW());
  }
}

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
  if (dwb.state.mode == INSERT_MODE || dwb.state.mode == FIND_MODE) {  
    dwb_change_mode(NORMAL_MODE, true);
  }
}/*}}}*/

/* dwb_clean_load_end(GList *) {{{*/
void 
dwb_clean_load_end(GList *gl) {
  View *v = gl->data;
  if (v->status->mimetype) {
    g_free(v->status->mimetype);
    v->status->mimetype = NULL;
  }
#if 0
  if (dwb.state.mode == INSERT_MODE || dwb.state.mode == FIND_MODE) {  
    dwb_change_mode(NORMAL_MODE, true);
  }
#endif
}/*}}}*/

/* dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *)   return: (alloc) Navigation* {{{*/
/* TODO sqlite */
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

/* dwb_open_channel (const char *filename) {{{*/
static void dwb_open_channel(const char *filename) {
  dwb.misc.si_channel = g_io_channel_new_file(filename, "r+", NULL);
  g_io_add_watch(dwb.misc.si_channel, G_IO_IN, (GIOFunc)dwb_handle_channel, NULL);
}/*}}}*/

/* dwb_test_userscript (const char *)         return: char* (alloc) or NULL {{{*/
static char * 
dwb_test_userscript(const char *filename) {
  char *path = g_build_filename(dwb.files.userscripts, filename, NULL); 

  if (g_file_test(path, G_FILE_TEST_IS_REGULAR) || 
      (g_str_has_prefix(filename, dwb.files.userscripts) && g_file_test(filename, G_FILE_TEST_IS_REGULAR) && (path = g_strdup(filename))) ) {
    return path;
  }
  else {
    g_free(path);
  }
  return NULL;
}/*}}}*/

/* dwb_open_si_channel() {{{*/
static void
dwb_open_si_channel() {
  dwb_open_channel(dwb.files.unifile);
}/*}}}*/

/* dwb_focus(GList *gl) {{{*/
void 
dwb_unfocus() {
  if (dwb.state.fview) {
    view_set_normal_style(VIEW(dwb.state.fview));
    dwb_source_remove(dwb.state.fview);
    CLEAR_COMMAND_TEXT();
    dwb.state.fview = NULL;
  }
} /*}}}*/

/* dwb_get_default_settings()         return: GHashTable {{{*/
GHashTable * 
dwb_get_default_settings() {
  GHashTable *ret = g_hash_table_new(g_str_hash, g_str_equal);
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    WebSettings *new = dwb_malloc(sizeof(WebSettings));
    *new = *s;
    char *value = g_strdup(s->n.first);
    g_hash_table_insert(ret, value, new);
  }
  return ret;
}/*}}}*/

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

/* dwb_reload_layout(GList *,  WebSettings  *s) {{{*/
static void 
dwb_reload_layout(GList *gl, WebSettings *s) {
  dwb_init_style();
  View *v;
  for (GList *l = dwb.state.views; l; l=l->next) {
    v = VIEW(l);
    if (l == dwb.state.fview) {
      view_set_active_style(v);
    }
    else {
      view_set_normal_style(v);
    }
  }
  dwb_init_style();
  dwb_apply_style();
}/*}}}*/

/* dwb_get_search_engine_uri(const char *uri) {{{*/
static char *
dwb_get_search_engine_uri(const char *uri, const char *text) {
  char *ret = NULL;
  if (uri != NULL && text != NULL) {
    GRegex *regex = g_regex_new(HINT_SEARCH_SUBMIT, 0, 0, NULL);
    char *escaped = g_uri_escape_string(text, NULL, true);
    ret = g_regex_replace(regex, uri, -1, 0, escaped, 0, NULL);
    g_free(escaped);
    g_regex_unref(regex);
  }
  return ret;
}/* }}} */

/* dwb_get_search_engine(const char *uri) {{{*/
char *
dwb_get_search_engine(const char *uri, gboolean force) {
  char *ret = NULL;
  if ( force || ((!strstr(uri, ".") || strstr(uri, " ")) && !strstr(uri, "localhost:"))) {
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
  }
  return ret;
}/*}}}*/

/* dwb_submit_searchengine {{{*/
void 
dwb_submit_searchengine(void) {
  char *com = g_strdup_printf("DwbHintObj.submitSearchEngine(\"%s\")", HINT_SEARCH_SUBMIT);
  char *value;
  if ( (value = dwb_execute_script(MAIN_FRAME(), com, true))) {
    dwb.state.form_name = value;
  }
  FREE(com);
}/*}}}*/

/* dwb_save_searchengine {{{*/
void
dwb_save_searchengine(void) {
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
    dwb_append_navigation_with_argument(&dwb.fc.searchengines, text, dwb.state.search_engine);
    Navigation *n = g_list_last(dwb.fc.searchengines)->data;
    Navigation *cn = dwb_get_search_completion_from_navigation(dwb_navigation_dup(n));

    dwb.fc.se_completion = g_list_append(dwb.fc.se_completion, cn);
    util_file_add_navigation(dwb.files.searchengines, n, true, -1);

    dwb_set_normal_message(dwb.state.fview, true, "Searchengine saved");
    if (dwb.state.search_engine) {
      if (!dwb.misc.default_search) {
        dwb.misc.default_search = dwb.state.search_engine;
      }
      else  {
        g_free(dwb.state.search_engine);
        dwb.state.search_engine = NULL;
      }
    }
  }
  else {
    dwb_set_error_message(dwb.state.fview, "No keyword specified, aborting.");
  }
  g_free(text);
}/*}}}*/

/* dwb_evaluate_hints(const char *buffer)  return DwbStatus {{{*/
DwbStatus 
dwb_evaluate_hints(const char *buffer) {
  DwbStatus ret = STATUS_OK;
  if (!g_strcmp0(buffer, "undefined")) 
    return ret;
  else if (!g_strcmp0("_dwb_no_hints_", buffer)) {
    dwb_set_error_message(dwb.state.fview, NO_HINTS);
    dwb_change_mode(NORMAL_MODE, false);
    ret = STATUS_ERROR;
  }
  else if (!g_strcmp0(buffer, "_dwb_input_")) {
    dwb_change_mode(INSERT_MODE);
    ret = STATUS_END;
  }
  else if  (!g_strcmp0(buffer, "_dwb_click_")) {
    dwb.state.scriptlock = 1;
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
      default : return ret;
    }
    FREE(a);
  }
  return ret;
}/*}}}*/

/* update_hints {{{*/
gboolean
dwb_update_hints(GdkEventKey *e) {
  char *buffer = NULL;
  char *com = NULL;
  char input[BUFFER_LENGTH] = { 0 }, *val;
  gboolean ret = false;

  if (e->keyval == GDK_KEY_Return) {
    com = g_strdup_printf("DwbHintObj.followActive(%d)", MIN(dwb.state.hint_type, HINT_T_URL));
  }
  else if (DWB_TAB_KEY(e)) {
    if (e->state & GDK_SHIFT_MASK) {
      com = g_strdup("DwbHintObj.focusPrev()");
    }
    else {
      com = g_strdup("DwbHintObj.focusNext()");
    }
    ret = true;
  }
  else if (e->is_modifier) {
    return false;
  }
  else {
    val = util_keyval_to_char(e->keyval, true);
    snprintf(input, BUFFER_LENGTH, "%s%s", GET_TEXT(), val ? val : "");
    com = g_strdup_printf("DwbHintObj.updateHints(\"%s\", %d)", input, MIN(dwb.state.hint_type, HINT_T_URL));
    FREE(val);
  }
  if (com) {
    buffer = dwb_execute_script(MAIN_FRAME(), com, true);
    g_free(com);
  }
  if (buffer != NULL) { 
    if (dwb_evaluate_hints(buffer) == STATUS_END) 
      ret = true;
    g_free(buffer);
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
  eval_ret = JSEvaluateScript(context, text, global_object, NULL, 0, NULL);
  JSStringRelease(text);

  if (eval_ret && ret) {
    return js_value_to_char(context, eval_ret);
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

/* dwb_confirm()  return confirmed (gboolean) {{{
 * yes / no confirmation
 * */
gboolean
dwb_confirm(GList *gl, char *prompt, ...) {
  dwb.state.mode |= CONFIRM;

  va_list arg_list; 

  va_start(arg_list, prompt);
  char message[STRING_LENGTH];
  vsnprintf(message, STRING_LENGTH, prompt, arg_list);
  va_end(arg_list);
  dwb_source_remove(gl);
  dwb_set_status_bar_text(dwb.gui.lstatus, message, &dwb.color.prompt, dwb.font.fd_active, false);

  int state = -1;
  int id = gtk_key_snooper_install((GtkKeySnoopFunc)dwb_confirm_snooper_cb, &state);
  while ((dwb.state.mode & CONFIRM) && state == -1) {
    gtk_main_iteration();
  }
  gtk_key_snooper_remove(id);
  return state > 0;
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
    util_file_add(dwb.files.quickmarks, text, true, -1);
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
  const char *uri = text ? text : webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
  char progress[11] = { 0 };
  char buf[5] = { 0 };
  int i=0;
  char sep1 = 0, sep2 = 0;
  if (v->status->lockprotect != 0) {
    sep1 = '[';
    sep2 = ']';
    if (LP_PROTECTED(v)) 
      buf[i++] = 'p';
    if (LP_LOCKED_DOMAIN(v)) 
      buf[i++] = 'd';
    if (LP_LOCKED_URI(v)) 
      buf[i++] = 'u';
    if (LP_VISIBLE(v)) 
      buf[i++] = 'v';
    buf[i++] = '\0';
  }
  if (v->status->progress != 0) {
    snprintf(progress, 11, "[%2d%%] ", v->status->progress);
  }

  char *escaped = g_markup_printf_escaped("[<span foreground='%s'>%d</span>]%c<span foreground='%s'>%s</span>%c %s%s", 
      dwb.color.tab_number_color,
      g_list_position(dwb.state.views, gl) + 1, 
      sep1,
      dwb.color.tab_protected_color,
      buf, 
      sep2,
      progress,
      uri ? uri : "about:blank");
  gtk_label_set_markup(GTK_LABEL(v->tablabel), escaped);

  g_free(escaped);
}/*}}}*/

/* dwb_update_status(GList *gl) {{{*/
void 
dwb_update_status(GList *gl) {
  View *v = gl->data;
  char *filename = NULL;
  WebKitWebView *w = WEBKIT_WEB_VIEW(v->web);
  const char *title = webkit_web_view_get_title(w);
  if (!title && v->status->mimetype && g_strcmp0(v->status->mimetype, "text/html")) {
    const char *uri = webkit_web_view_get_uri(w);
    filename = g_path_get_basename(uri);
    title = filename;
  }
  if (!title) {
    title = dwb.misc.name;
  }

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

  FREE(filename);
}/*}}}*/
/* dwb_update_layout(GList *gl) {{{*/
void 
dwb_update_layout() {
  for (GList *gl = dwb.state.views; gl; gl = gl->next) {
    View *v = gl->data;
    const char *title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
    dwb_tab_label_set_text(gl, title);
  }
}/*}}}*/

/* dwb_focus(GList *gl) {{{*/
void 
dwb_focus(GList *gl) {
  if (dwb.gui.entry) {
    gtk_widget_hide(dwb.gui.entry);
  }
  dwb.state.fview = gl;
  view_set_active_style(VIEW(gl));
  dwb_focus_scroll(gl);
  dwb_update_status(gl);
}/*}}}*/

/* dwb_new_window(const char *arg) {{{*/
void 
dwb_new_window(const char  *uri) {
  char *argv[6];

  argv[0] = (char *)dwb.misc.prog_path;
  argv[1] = "-p"; 
  argv[2] = (char *)dwb.misc.profile;
  argv[3] = "-n";
  argv[4] = (char *)uri;
  argv[5] = NULL;
  g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}/*}}}*/

/* dwb_load_uri(const char *uri) {{{*/
void 
dwb_load_uri(GList *gl, const char *arg) {
  /* TODO parse scheme */
  if (arg != NULL && strlen(arg) > 0)
    g_strstrip((char*)arg);
  gl = gl == NULL ? dwb.state.fview : gl;
  WebKitWebView *web = WEBVIEW(gl);

  if (!arg || !arg || !strlen(arg)) {
    return;
  }

  /* new window ? */
  if (dwb.state.nv & OPEN_NEW_WINDOW) {
    dwb.state.nv = OPEN_NORMAL;
    dwb_new_window(arg);
    return;
  }
  /*  new tab ?  */
  else if (dwb.state.nv == OPEN_NEW_VIEW) {
    dwb.state.nv = OPEN_NORMAL;
    view_add(arg, false);
    return;
  }
  /*  get resolved uri */
  char *uri = NULL; 

  dwb_soup_clean();
  /* Check if uri is a html-string */
  if (dwb.state.type == HTML_STRING) {
    webkit_web_view_load_string(web, arg, "text/html", NULL, NULL);
    dwb.state.type = 0;
    return;
  }
  /* Check if uri is a userscript */
  if ( (uri = dwb_test_userscript(arg)) ) {
    Arg a = { .arg = uri };
    dwb_execute_user_script(NULL, &a);
    g_free(uri);
    return;
  }
  /* Check if uri is a javascript snippet */
  if (g_str_has_prefix(arg, "javascript:")) {
    dwb_execute_script(webkit_web_view_get_main_frame(web), arg, false);
    return;
  }
  /* Check if uri is a directory */
  if ( (local_check_directory(gl, arg, true, NULL)) ) {
    return;
  }
  /* Check if uri is a regular file */
  if (g_str_has_prefix(arg, "file://") || !g_strcmp0(arg, "about:blank")) {
    webkit_web_view_load_uri(web, arg);
    return;
  }
  else if ( g_file_test(arg, G_FILE_TEST_IS_REGULAR) ) {
    GError *error = NULL;
    if ( !(uri = g_filename_to_uri(arg, NULL, &error)) ) { 
      if (error->code == G_CONVERT_ERROR_NOT_ABSOLUTE_PATH) {
        g_clear_error(&error);
        char *path = g_get_current_dir();
        char *tmp = g_build_filename(path, arg, NULL);
        if ( !(uri = g_filename_to_uri(tmp, NULL, &error))) {
          fprintf(stderr, "Cannot open %s: %s", (char*)arg, error->message);
          g_clear_error(&error);
        }
        FREE(tmp);
        FREE(path);
      }
    }
  }
  else if (g_str_has_prefix(arg, "dwb://")) {
    webkit_web_view_load_uri(web, arg);
    return;
  }
  /* Check if searchengine is needed and load uri */

  else if (!(uri = dwb_get_search_engine(arg, false)) || strstr(arg, "localhost:")) {
    uri = g_str_has_prefix(arg, "http://") || g_str_has_prefix(arg, "https://") 
      ? g_strdup(arg)
      : g_strdup_printf("http://%s", (char*)arg);
  }
  webkit_web_view_load_uri(web, uri);
  FREE(uri);
}/*}}}*/

/* dwb_eval_editing_key(GdkEventKey *) {{{*/
gboolean 
dwb_eval_editing_key(GdkEventKey *e) {
  if (DIGIT(e)) {
    return false;
  }

  char *key = util_keyval_to_char(e->keyval, false);
  if (key == NULL) {
    key = g_strdup(gdk_keyval_name(e->keyval));
  }
  gboolean ret = false;

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (km->map->entry & EP_ENTRY) {
      if (!g_strcmp0(key, km->key) && CLEAN_STATE(e) == km->mod) {
        km->map->func(&km, &km->map->arg);
        ret = true;
        break;
      }
    }
  }
  FREE(key);
  return ret;
}/*}}}*/

/* dwb_clean_key_buffer() {{{*/
void 
dwb_clean_key_buffer() {
  dwb.state.nummod = -1;
  g_string_truncate(dwb.state.buffer, 0);
}/*}}}*/

const char *
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

}

gboolean 
dwb_entry_activate(GdkEventKey *e) {
  char **token = NULL;
  switch (CLEAN_MODE(dwb.state.mode))  {
    case HINT_MODE:           dwb_update_hints(e); return false;
    case FIND_MODE:           dwb_focus_scroll(dwb.state.fview);
                              dwb_search(NULL);
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
    case QUICK_MARK_SAVE:     dwb_save_quickmark(GET_TEXT());
                              return true;
    case QUICK_MARK_OPEN:     dwb_open_quickmark(GET_TEXT());
                              return true;
    case COMPLETE_PATH:       completion_clean_path_completion();
                              break;
    default : break;
  }
  CLEAR_COMMAND_TEXT();
  const char *text = GET_TEXT();
  dwb_load_uri(NULL, text);
  if (text != NULL && *text)
    dwb_glist_prepend_unique(&dwb.fc.navigations, g_strdup(text));
  dwb_change_mode(NORMAL_MODE, false);
  return true;
}
/* dwb_eval_key(GdkEventKey *e) {{{*/
gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = false;
  int keyval = e->keyval;
  unsigned int mod_mask;
  int keynum = -1;

  if (dwb.state.scriptlock) {
    return true;
  }
  if (e->is_modifier) {
    return false;
  }
  /* don't show backspace in the buffer */
  if (keyval == GDK_KEY_BackSpace ) {
    if (dwb.state.mode & AUTO_COMPLETE) {
      completion_clean_autocompletion();
    }
    if (dwb.state.buffer->len > 0) {
      g_string_erase(dwb.state.buffer, dwb.state.buffer->len - 1, 1);
      dwb_set_status_bar_text(dwb.gui.lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_active, false);
      ret = false;
    }
    else {
      ret = false;
    }
    return ret;
  }
  /* Multimedia keys */
  switch (keyval) {
    case GDK_KEY_Back : dwb_history_back(); return true;
    case GDK_KEY_Forward : dwb_history_forward(); return true;
    case GDK_KEY_Cancel : commands_stop_loading(NULL, NULL); return true;
    case GDK_KEY_Reload : commands_reload(NULL, NULL); return true;
    case GDK_KEY_ZoomIn : commands_zoom_in(NULL, NULL); return true;
    case GDK_KEY_ZoomOut : commands_zoom_out(NULL, NULL); return true;
  }
  char *key = util_keyval_to_char(keyval, true);
  if (key) {
    mod_mask = CLEAN_STATE(e);
  }
  else if ( (key = g_strdup(gdk_keyval_name(keyval)))) {
    mod_mask = CLEAN_STATE_WITH_SHIFT(e);
  }
  else {
    return false;
  }
  /* nummod */
  if (DIGIT(e)) {
    keynum = e->keyval - GDK_KEY_0;
    if (dwb.state.nummod >= 0) {
      dwb.state.nummod = MIN(10*dwb.state.nummod + keynum, 314159);
    }
    else {
      dwb.state.nummod = e->keyval - GDK_KEY_0;
    }
    if (mod_mask) {
      for (GList *l = dwb.keymap; l; l=l->next) {
        KeyMap *km = l->data;
        if ((km->mod & DWB_NUMMOD_MASK) && (km->mod & ~DWB_NUMMOD_MASK) == mod_mask) {
          commands_simple_command(km);
          break;
        }
      }
    }
    FREE(key);
    return false;
  }
  g_string_append(dwb.state.buffer, key);
  if (ALPHA(e) || DIGIT(e)) {
    dwb_set_status_bar_text(dwb.gui.lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_active, false);
  }

  const char *buf = dwb.state.buffer->str;
  int longest = 0;
  KeyMap *tmp = NULL;
  GList *coms = NULL;

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (km->map->entry & EP_ENTRY) {
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
      ret = true;
    }
  }
  /* autocompletion */
  if (dwb.state.mode & AUTO_COMPLETE) {
    completion_clean_autocompletion();
  }
  if (coms && g_list_length(coms) > 0) {
    completion_autocomplete(coms, NULL);
    ret = true;
  }
  if (tmp && dwb.state.buffer->len == longest) {
    commands_simple_command(tmp);
  }
  if (longest == 0) {
    dwb_clean_key_buffer();
    CLEAR_COMMAND_TEXT();
  }
  FREE(key);
  return ret;

}/*}}}*/

/* dwb_insert_mode(Arg *arg) {{{*/
static DwbStatus
dwb_insert_mode(void) {
  if (dwb.state.mode & PASS_THROUGH)
    return STATUS_ERROR;
  if (dwb.state.mode == HINT_MODE) {
    dwb_set_normal_message(dwb.state.fview, true, INSERT);
  }
  dwb_set_normal_message(dwb.state.fview, false, "-- INSERT MODE --");
  dwb_focus_scroll(dwb.state.fview);

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

/* dwb_passthrough_mode () {{{*/
static DwbStatus
dwb_passthrough_mode(void) {
  dwb.state.mode |= PASS_THROUGH;
  dwb_set_normal_message(dwb.state.fview, false, "-- PASS THROUGH --");
  return STATUS_OK;
}/*}}}*/

/* dwb_normal_mode() {{{*/
static DwbStatus 
dwb_normal_mode(gboolean clean) {
  Mode mode = dwb.state.mode;

  if (mode == HINT_MODE || mode == SEARCH_FIELD_MODE) {
    dwb_execute_script(MAIN_FRAME(), "DwbHintObj.clear()", false);
  }
  else if (mode == DOWNLOAD_GET_PATH) {
    completion_clean_path_completion();
  }
  if (mode & COMPLETION_MODE) {
    completion_clean_completion(false);
  }
  if (mode & AUTO_COMPLETE) {
    completion_clean_autocompletion();
  }
  dwb_focus_scroll(dwb.state.fview);

  if (clean) {
    dwb_clean_key_buffer();
    CLEAR_COMMAND_TEXT();
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
  switch(mode) {
    case NORMAL_MODE: 
      va_start(vl, mode);
      clean = va_arg(vl, gboolean);
      ret = dwb_normal_mode(clean);
      va_end(vl);
      break;
    case INSERT_MODE:   ret = dwb_insert_mode(); break;
    case COMMAND_MODE:  ret = dwb_command_mode(); break;
    case PASS_THROUGH:  ret = dwb_passthrough_mode(); break;
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

  if ( v->status->search_string != NULL && (matches = webkit_web_view_mark_text_matches(web, v->status->search_string, false, 0)) ) {
    dwb_set_normal_message(dwb.state.fview, false, "[%3d hits] ", matches);
    webkit_web_view_set_highlight_text_matches(web, true);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_update_search(gboolean ) {{{*/
gboolean 
dwb_update_search(gboolean forward) {
  View *v = CURRENT_VIEW();
  const char *text = GET_TEXT();
  if (strlen(text) > 0) {
    FREE(v->status->search_string);
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
  gboolean forward = dwb.state.forward_search;
  if (arg) {
    if (!arg->b) {
      forward = !dwb.state.forward_search;
    }
    dwb_highlight_search();
  }
  if (v->status->search_string) {
    ret = webkit_web_view_search_text(WEBKIT_WEB_VIEW(v->web), v->status->search_string, false, forward, true);
  }
  return ret;
}/*}}}*/

/* dwb_user_script_cb(GIOChannel *, GIOCondition *)     return: false {{{*/
static gboolean
dwb_user_script_cb(GIOChannel *channel, GIOCondition condition, GIOChannel *out_channel) {
  GError *error = NULL;
  char *line;

  while (g_io_channel_read_line(channel, &line, NULL, NULL, &error) == G_IO_STATUS_NORMAL) {
    if (g_str_has_prefix(line, "javascript:")) {
      char *value; 
      if ( ( value = dwb_execute_script(MAIN_FRAME(), line + 11, true))) {
        g_io_channel_write_chars(out_channel, value, -1, NULL, NULL);
        g_io_channel_write_chars(out_channel, "\n", 1, NULL, NULL);
        g_free(value);
      }
    }
    else if (!g_strcmp0(line, "close\n")) {
      g_io_channel_shutdown(channel, true, NULL);
      FREE(line);
      break;
    }
    else {
      dwb_parse_command_line(g_strchomp(line));
    }
    g_io_channel_flush(out_channel, NULL);
    FREE(line);
  }
  if (error) {
    fprintf(stderr, "Cannot read from std_out: %s\n", error->message);
  }
  g_clear_error(&error);

  return false;
}/*}}}*/

/* dwb_execute_user_script(Arg *a) {{{*/
void
dwb_execute_user_script(KeyMap *km, Arg *a) {
  GError *error = NULL;
  char nummod[64];
  snprintf(nummod, 64, "%d", NUMMOD);
  char *argv[6] = { a->arg, (char*)webkit_web_view_get_uri(CURRENT_WEBVIEW()), (char *)dwb.misc.profile, nummod, a->p, NULL } ;
  int std_out;
  int std_in;
  if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &std_in, &std_out, NULL, &error)) {
    GIOChannel *channel = g_io_channel_unix_new(std_out);
    GIOChannel *out_channel = g_io_channel_unix_new(std_in);
    g_io_add_watch(channel, G_IO_IN, (GIOFunc)dwb_user_script_cb, out_channel);
    dwb_set_normal_message(dwb.state.fview, true, "Executing script %s", a->arg);
  }
  else {
    fprintf(stderr, "Cannot execute %s: %s\n", (char*)a->p, error->message);
  }
  g_clear_error(&error);
}/*}}}*/

/* dwb_get_scripts() {{{*/
static GList * 
dwb_get_scripts() {
  GDir *dir;
  char *filename;
  char *content;
  GList *gl = NULL;
  Navigation *n = NULL;
  GError *error = NULL;

  if ( (dir = g_dir_open(dwb.files.userscripts, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      char *path = g_build_filename(dwb.files.userscripts, filename, NULL);
      char *realpath;
      /* ignore subdirectories */
      if (g_file_test(path, G_FILE_TEST_IS_DIR))
        continue;
      else if (g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
        realpath = g_file_read_link(path, &error);
        if (error != NULL) {
          fprintf(stderr, "Cannot read %s : %s\n", path, error->message);
          continue;
        }
        g_free(path);
        path = realpath;
      }

      g_file_get_contents(path, &content, NULL, NULL);
      if (content == NULL) 
        continue;

      char **lines = g_strsplit(content, "\n", -1);

      g_free(content);
      content = NULL;

      int i=0;
      KeyMap *map = dwb_malloc(sizeof(KeyMap));
      FunctionMap *fmap = dwb_malloc(sizeof(FunctionMap));
      while (lines[i]) {
        if (g_regex_match_simple(".*dwb:", lines[i], 0, 0)) {
          char **line = g_strsplit(lines[i], "dwb:", 2);
          if (line[1]) {
            n = dwb_navigation_new(filename, line[1]);
            Key key = dwb_str_to_key(line[1]);
            map->key = key.str;
            map->mod = key.mod;
            gl = g_list_prepend(gl, map);
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
        gl = g_list_prepend(gl, map);
      }
      FunctionMap fm = { { n->first, n->first }, CP_DONT_SAVE | CP_COMMANDLINE, (Func)dwb_execute_user_script, NULL, POST_SM, { .arg = path } };
      *fmap = fm;
      map->map = fmap;
      dwb.misc.userscripts = g_list_prepend(dwb.misc.userscripts, n);
      n = NULL;

      g_strfreev(lines);
    }
    g_dir_close(dir);
  }
  return gl;
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
  dwb.state.scriptlock = 0;
  dwb.state.dl_action = DL_ACTION_DOWNLOAD;
  if (dwb.state.mimetype_request) {
    g_free(dwb.state.mimetype_request);
    dwb.state.mimetype_request = NULL;
  }
}/*}}}*/

/* dwb_free_list(GList *list, void (*func)(void*)) {{{*/
static void
dwb_free_list(GList *list, void (*func)(void*)) {
  for (GList *l = list; l; l=l->next) {
    Navigation *n = l->data;
    func(n);
  }
  g_list_free(list);
}/*}}}*/

/* dwb_clean_up() {{{*/
gboolean
dwb_clean_up() {
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    FREE(m);
    m = NULL;
  }
  g_list_free(dwb.keymap);
  dwb.keymap = NULL;
  g_hash_table_remove_all(dwb.settings);

  dwb_free_list(dwb.fc.bookmarks, (void_func)dwb_navigation_free);
  /*  TODO sqlite */
  dwb_free_list(dwb.fc.history, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.searchengines, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.se_completion, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.mimetypes, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.quickmarks, (void_func)dwb_quickmark_free);
  dwb_free_list(dwb.fc.cookies_allow, (void_func)dwb_free);
  dwb_free_list(dwb.fc.cookies_session_allow, (void_func)dwb_free);
  dwb_free_list(dwb.fc.navigations, (void_func)dwb_free);
  dwb_free_list(dwb.fc.commands, (void_func)dwb_free);

  dwb_soup_end();
#ifdef DWB_ADBLOCKER
  adblock_end();
#endif
#ifdef DWB_DOMAIN_SERVICE
  domain_end();
#endif

  if (g_file_test(dwb.files.fifo, G_FILE_TEST_EXISTS)) {
    unlink(dwb.files.fifo);
  }
  util_rmdir(dwb.files.cachedir, true);
  gtk_widget_destroy(dwb.gui.window);
  return true;
}/*}}}*/

static void dwb_save_key_value(const char *file, const char *key, const char *value) {
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

  if (!g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No keysfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *map = l->data;
    if (! (map->map->prop & CP_DONT_SAVE) ) {
      char *mod = dwb_modmask_to_string(map->mod);
      char *sc = g_strdup_printf("%s %s", mod, map->key ? map->key : "");
      g_key_file_set_value(keyfile, dwb.misc.profile, map->map->n.first, sc);
      FREE(sc);
      FREE(mod);
    }
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    util_set_file_content(dwb.files.keys, content);
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
  char *content;
  gsize size;
  setlocale(LC_NUMERIC, "C");

  if (!g_key_file_load_from_file(keyfile, dwb.files.settings, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No settingsfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    char *value = util_arg_to_char(&s->arg, s->type); 
    g_key_file_set_value(keyfile, dwb.misc.profile, s->n.first, value ? value : "" );

    FREE(value);
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    util_set_file_content(dwb.files.settings, content);
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
  if (dwb.misc.synctimer > 0) {
    dwb_sync_history(NULL);
  }
  /* Save command history */
  if (! dwb.misc.private_browsing) {
    dwb_save_list(dwb.fc.navigations, dwb.files.navigation_history, GET_INT("navigation-history-max"));
    dwb_save_list(dwb.fc.commands, dwb.files.command_history, GET_INT("navigation-history-max"));
  }
  /* save session */
  if (end_session && GET_BOOL("save-session") && dwb.state.mode != SAVE_SESSION) {
    session_save(NULL);
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
      
  if (dwb_save_files(true)) {
    if (dwb_clean_up()) {
      gtk_main_quit();
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
  GString *buffer = g_string_new(NULL);

  char **string = g_strsplit(str, " ", -1);

  for (int i=0; i<g_strv_length(string); i++)  {
    if (!g_ascii_strcasecmp(string[i], "Control")) {
      key.mod |= GDK_CONTROL_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod1")) {
      key.mod |= GDK_MOD1_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod4")) {
      key.mod |= GDK_MOD4_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button1")) {
      key.mod |= GDK_BUTTON1_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button2")) {
      key.mod |= GDK_BUTTON2_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button3")) {
      key.mod |= GDK_BUTTON3_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button4")) {
      key.mod |= GDK_BUTTON4_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button5")) {
      key.mod |= GDK_BUTTON5_MASK;
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
  key.str = buffer->str;

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
  for (int i=0; i<LENGTH(FMAP); i++) {
    if (!g_strcmp0(FMAP[i].n.first, key.id)) {
      KeyMap *keymap = dwb_malloc(sizeof(KeyMap));
      FunctionMap *fmap = &FMAP[i];
      keymap->key = key.key.str ? key.key.str : "";
      keymap->mod = key.key.mod;
      fmap->n.first = (char*)key.id;
      keymap->map = fmap;
      gl = g_list_prepend(gl, keymap);
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

  g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error);
  if (error) {
    fprintf(stderr, "No keyfile found: %s\nUsing default values.\n", error->message);
    g_clear_error(&error);
  }
  for (int i=0; i<LENGTH(KEYS); i++) {
    KeyValue kv;
    char *string = g_key_file_get_value(keyfile, dwb.misc.profile, KEYS[i].id, NULL);
    if (string) {
      kv.key = dwb_str_to_key(string);
      g_free(string);
    }
    else if (KEYS[i].key.str) {
      kv.key = KEYS[i].key;
    }
    else {
       kv.key.str = NULL;
       kv.key.mod = 0;
    }

    kv.id = KEYS[i].id;
    dwb.keymap = dwb_keymap_add(dwb.keymap, kv);
  }

  dwb.keymap = g_list_concat(dwb.keymap, dwb_get_scripts());
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)util_keymap_sort_second);

  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_read_settings() {{{*/
static gboolean
dwb_read_settings() {
  GError *error = NULL;
  gsize length, numkeys = 0;
  char  **keys = NULL;
  char  *content;
  GKeyFile  *keyfile = g_key_file_new();
  Arg *arg;
  setlocale(LC_NUMERIC, "C");

  g_file_get_contents(dwb.files.settings, &content, &length, &error);
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
  FREE(content);
  for (int j=0; j<LENGTH(DWB_SETTINGS); j++) {
    gboolean set = false;
    char *key = g_strdup(DWB_SETTINGS[j].n.first);
    for (int i=0; i<numkeys; i++) {
      char *value = g_key_file_get_string(keyfile, dwb.misc.profile, keys[i], NULL);
      if (!g_strcmp0(keys[i], DWB_SETTINGS[j].n.first)) {
        WebSettings *s = dwb_malloc(sizeof(WebSettings));
        *s = DWB_SETTINGS[j];
        if ( (arg = util_char_to_arg(value, s->type)) ) {
          s->arg = *arg;
        }
        g_hash_table_insert(dwb.settings, key, s);
        set = true;
      }
      FREE(value);
    }
    if (!set) {
      g_hash_table_insert(dwb.settings, key, &DWB_SETTINGS[j]);
    }
  }
  if (keys)
    g_strfreev(keys);
  return true;
}/*}}}*/

/* dwb_init_settings() {{{*/
static void
dwb_init_settings() {
  dwb.settings = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)dwb_free, NULL);
  dwb.state.web_settings = webkit_web_settings_new();
  dwb_read_settings();
  for (GList *l =  g_hash_table_get_values(dwb.settings); l; l = l->next) {
    WebSettings *s = l->data;
    if (s->apply & SETTING_BUILTIN || s->apply & SETTING_ONINIT) {
      s->func(NULL, s);
    }
  }
}/*}}}*/

/* dwb_init_scripts{{{*/
void 
dwb_init_scripts() {
  FREE(dwb.misc.scripts);
  GString *normalbuffer = g_string_new(NULL);
  GString *allbuffer    = g_string_new(NULL);

  setlocale(LC_NUMERIC, "C");
  /* user scripts */
  util_get_directory_content(&normalbuffer, dwb.files.scriptdir, "js");
  util_get_directory_content(&allbuffer, dwb.files.scriptdir, "all.js");

  /* systemscripts */
  char *dir = NULL;
  if ( (dir = util_get_system_data_dir("scripts")) ) {
    util_get_directory_content(&normalbuffer, dir, "js");
    util_get_directory_content(&allbuffer, dir, "all.js");
    g_free(dir);
  }
  g_string_append_printf(normalbuffer, "DwbHintObj.init(\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%f\", %s);", 
      GET_CHAR("hint-letter-seq"),
      GET_CHAR("hint-font"),
      GET_CHAR("hint-style"), 
      GET_CHAR("hint-fg-color"), 
      GET_CHAR("hint-bg-color"), 
      GET_CHAR("hint-active-color"), 
      GET_CHAR("hint-normal-color"), 
      GET_CHAR("hint-border"), 
      GET_DOUBLE("hint-opacity"),
      GET_BOOL("hint-highlight-links") ? "true" : "false");
  g_string_append(normalbuffer, allbuffer->str);
  dwb.misc.scripts = normalbuffer->str;
  dwb.misc.allscripts = allbuffer->str;
  g_string_free(normalbuffer, false);
  g_string_free(allbuffer, false);
}/*}}}*/

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
  DWB_COLOR_PARSE(&dwb.color.tab_normal_fg, GET_CHAR("tab-normal-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.tab_normal_bg, GET_CHAR("tab-normal-bg-color"));

  /* Downloads */
  DWB_COLOR_PARSE(&dwb.color.download_fg, "#ffffff");
  DWB_COLOR_PARSE(&dwb.color.download_bg, "#000000");

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


  char *font = GET_CHAR("font");
  dwb.font.fd_active = pango_font_description_from_string(font);
  char *f;
#define SET_FONT(var, prop) f = GET_CHAR(prop); var = pango_font_description_from_string(f ? f : font)
  SET_FONT(dwb.font.fd_inactive, "font-inactive");
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

static DwbStatus
dwb_pack(const char *layout, gboolean rebuild) {
  DwbStatus ret = STATUS_OK;
  const char *default_layout = DEFAULT_WIDGET_PACKING;
  if (layout == NULL) {
    layout = default_layout;
    ret = STATUS_ERROR;
  }
  while (g_ascii_isspace(*layout))
    layout++;
  if (strlen(layout) != 4) {
    layout = default_layout;
    ret = STATUS_ERROR;
  }

  const char *valid_chars = default_layout;
  char *buf = g_ascii_strdown(layout, 4);
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
  gtk_window_set_wmclass(GTK_WINDOW(dwb.gui.window), dwb.misc.name, dwb.misc.name);
  gtk_widget_set_name(dwb.gui.window, dwb.misc.name);
  /* Icon */
  GdkPixbuf *icon_pixbuf = gdk_pixbuf_new_from_xpm_data(icon);
  gtk_window_set_icon(GTK_WINDOW(dwb.gui.window), icon_pixbuf);

#if _HAS_GTK3
  gtk_window_set_has_resize_grip(GTK_WINDOW(dwb.gui.window), false);
  GtkCssProvider *provider = gtk_css_provider_get_default();
  GString *buffer = g_string_new("GtkEntry {background-image: none; }");
  if (! dwb.misc.scrollbars) {
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
  dwb.gui.vbox = gtk_vbox_new(false, 1);
  dwb.gui.topbox = gtk_hbox_new(true, 1);
  dwb.gui.mainbox = gtk_hbox_new(true, 1);

  /* Downloadbar */
  dwb.gui.downloadbar = gtk_hbox_new(false, 3);

  /* entry */
  dwb.gui.entry = gtk_entry_new();
  gtk_entry_set_inner_border(GTK_ENTRY(dwb.gui.entry), NULL);

  gtk_entry_set_has_frame(GTK_ENTRY(dwb.gui.entry), false);
  gtk_entry_set_inner_border(GTK_ENTRY(dwb.gui.entry), false);

  GtkWidget *status_hbox;
  dwb.gui.bottombox = gtk_vbox_new(false, 0);
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

  status_hbox = gtk_hbox_new(false, 2);
  gtk_box_pack_start(GTK_BOX(status_hbox), dwb.gui.lstatus, false, false, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), dwb.gui.entry, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), dwb.gui.urilabel, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), dwb.gui.rstatus, false, false, 0);
  gtk_container_add(GTK_CONTAINER(dwb.gui.statusbox), status_hbox);

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
  char *content = util_get_file_content(filename);

  if (content) {
    char **token = g_strsplit(content, "\n", 0);
    int length = MAX(g_strv_length(token) - 1, 0);
    for (int i=0;  i < length; i++) {
      void *value = func(token[i]);
      if (value != NULL)
        gl = g_list_append(gl, value);
    }
    g_free(content);
    g_strfreev(token);
  }
  return gl;
}/*}}}*/

static Navigation * 
dwb_get_search_completion_from_navigation(Navigation *n) {
  char *uri = n->second;
  n->second = util_domain_from_uri(n->second);

  FREE(uri);
  return n;
}
static Navigation * 
dwb_get_search_completion(const char *text) {
  Navigation *n = dwb_navigation_new_from_line(text);
  return dwb_get_search_completion_from_navigation(n);
}

/* dwb_init_files() {{{*/
static void
dwb_init_files() {
  char *path           = util_build_path();
  char *profile_path = util_check_directory(g_build_filename(path, dwb.misc.profile, NULL));
  char *userscripts, *scripts, *cachedir;

  dwb.fc.bookmarks = NULL;
  dwb.fc.history = NULL;
  dwb.fc.quickmarks = NULL;
  dwb.fc.searchengines = NULL;
  dwb.fc.se_completion = NULL;
  dwb.fc.mimetypes = NULL;
  dwb.fc.navigations = NULL;
  dwb.fc.commands = NULL;


  cachedir = g_build_filename(g_get_user_cache_dir(), dwb.misc.name, NULL);
  dwb.files.cachedir = util_check_directory(cachedir);

  dwb.files.bookmarks       = g_build_filename(profile_path, "bookmarks",     NULL);
  dwb.files.history         = g_build_filename(profile_path, "history",       NULL);
  dwb.files.stylesheet      = g_build_filename(profile_path, "stylesheet",    NULL);
  dwb.files.quickmarks      = g_build_filename(profile_path, "quickmarks",    NULL);
  dwb.files.session         = g_build_filename(profile_path, "session",       NULL);
  dwb.files.navigation_history = g_build_filename(profile_path, "navigate.history",       NULL);
  dwb.files.command_history = g_build_filename(profile_path, "commands.history",       NULL);
  dwb.files.searchengines   = g_build_filename(path, "searchengines", NULL);
  dwb.files.keys            = g_build_filename(path, "keys",          NULL);
  dwb.files.settings        = g_build_filename(path, "settings",      NULL);
  dwb.files.mimetypes       = g_build_filename(path, "mimetypes",      NULL);
  dwb.files.cookies         = g_build_filename(profile_path, "cookies",       NULL);
  dwb.files.cookies_allow   = g_build_filename(profile_path, "cookies.allow", NULL);
  dwb.files.cookies_session_allow   = g_build_filename(profile_path, "cookies_session.allow", NULL);
  dwb.files.adblock         = g_build_filename(path, "adblock",      NULL);
  dwb.files.scripts_allow   = g_build_filename(profile_path, "scripts.allow",      NULL);
  dwb.files.plugins_allow   = g_build_filename(profile_path, "plugins.allow",      NULL);

  scripts                   = g_build_filename(path, "scripts",      NULL);
  dwb.files.scriptdir       = util_check_directory(scripts);
  userscripts               = g_build_filename(path, "userscripts",   NULL);
  dwb.files.userscripts     = util_check_directory(userscripts);

  dwb.fc.bookmarks = dwb_init_file_content(dwb.fc.bookmarks, dwb.files.bookmarks, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.history = dwb_init_file_content(dwb.fc.history, dwb.files.history, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.quickmarks = dwb_init_file_content(dwb.fc.quickmarks, dwb.files.quickmarks, (Content_Func)dwb_quickmark_new_from_line); 
  dwb.fc.searchengines = dwb_init_file_content(dwb.fc.searchengines, dwb.files.searchengines, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.se_completion = dwb_init_file_content(dwb.fc.se_completion, dwb.files.searchengines, (Content_Func)dwb_get_search_completion);
  dwb.fc.mimetypes = dwb_init_file_content(dwb.fc.mimetypes, dwb.files.mimetypes, (Content_Func)dwb_navigation_new_from_line);
  dwb.fc.navigations = dwb_init_file_content(dwb.fc.navigations, dwb.files.navigation_history, (Content_Func)dwb_return);
  dwb.fc.commands = dwb_init_file_content(dwb.fc.commands, dwb.files.command_history, (Content_Func)dwb_return);
  dwb.fc.tmp_scripts = NULL;
  dwb.fc.tmp_plugins = NULL;
  dwb.fc.downloads   = NULL;

  if (g_list_last(dwb.fc.searchengines)) 
    dwb.misc.default_search = ((Navigation*)dwb.fc.searchengines->data)->second;
  else 
    dwb.misc.default_search = NULL;
  dwb.fc.cookies_allow = dwb_init_file_content(dwb.fc.cookies_allow, dwb.files.cookies_allow, (Content_Func)dwb_return);
  dwb.fc.cookies_session_allow = dwb_init_file_content(dwb.fc.cookies_session_allow, dwb.files.cookies_session_allow, (Content_Func)dwb_return);

  FREE(path);
  FREE(profile_path);
}/*}}}*/

/* signals {{{*/
static void
dwb_handle_signal(int s) {
  if (((s == SIGTERM || s == SIGINT) && dwb_end()) || s == SIGFPE || s == SIGILL || s == SIGQUIT) 
    exit(EXIT_SUCCESS);
  else if (s == SIGSEGV) {
    fprintf(stderr, "Received SIGSEGV, trying to clean up.\n");
    dwb_clean_up();
    exit(EXIT_FAILURE);
  }
}

static void 
dwb_init_signals() {
  for (int i=0; i<LENGTH(signals); i++) {
    struct sigaction act, oact;
    act.sa_handler = dwb_handle_signal;

    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(signals[i], &act, &oact);
  }
}/*}}}*/

/* dwb_init_vars{{{*/
static void 
dwb_init_vars() {
  dwb.misc.message_delay = GET_INT("message-delay");
  dwb.misc.history_length = GET_INT("history-length");
  dwb.misc.startpage = GET_CHAR("startpage");
  dwb.misc.tabbed_browsing = GET_BOOL("tabbed-browsing");
  dwb.misc.private_browsing = GET_BOOL("enable-private-browsing");
  dwb.misc.scroll_step = GET_DOUBLE("scroll-step");
  dwb.misc.scrollbars = GET_BOOL("scrollbars");

  dwb.state.cookie_store_policy = dwb_soup_get_cookie_store_policy(GET_CHAR("cookies-store-policy"));

  dwb.state.complete_history = GET_BOOL("complete-history");
  dwb.state.complete_bookmarks = GET_BOOL("complete-bookmarks");
  dwb.state.complete_searchengines = GET_BOOL("complete-searchengines");
  dwb.state.complete_userscripts = GET_BOOL("complete-userscripts");
  dwb.state.background_tabs = GET_BOOL("background-tabs");

  dwb.state.buffer = g_string_new(NULL);
  dwb.comps.autocompletion = GET_BOOL("auto-completion");
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


/* dwb_init() {{{*/
static void 
dwb_init() {
  dwb_clean_vars();
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
  dwb.misc.synctimer = 0;
  dwb.misc.bar_height = 0;

  char *path = util_get_data_file(PLUGIN_FILE);
  if (path) {
    dwb.misc.pbbackground = util_get_file_content(path);
    g_free(path);
  }
  char *cache_model = GET_CHAR("cache-model");
  if (!g_ascii_strcasecmp(cache_model, "documentviewer"))
    webkit_set_cache_model(WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

  dwb_init_key_map();
  dwb_init_style();
  dwb_init_vars();
  dwb_init_gui();
  dwb_init_scripts();
#ifdef DWB_ADBLOCKER
  adblock_init();
#endif

  dwb_soup_init();

  gboolean restore_success = false;
  if (restore) 
    restore_success = session_restore(restore);
  if (dwb.misc.argc > 0) {
    for (int i=0; i<dwb.misc.argc; i++) {
      view_add(dwb.misc.argv[i], false);
    }
  }
  else if (!restore || !restore_success) {
    view_add(NULL, false);
  }
  PangoContext *pctx = gtk_widget_get_pango_context(VIEW(dwb.state.fview)->tablabel);
  PangoLayout *layout = pango_layout_new(pctx);
  int w = 0, h = 0;
  pango_layout_set_text(layout, "a", -1);
  pango_layout_set_font_description(layout, dwb.font.fd_active);
  pango_layout_get_size(layout, &w, &h);
  dwb.misc.bar_height = h/PANGO_SCALE;
  dwb_pack(GET_CHAR("widget-packing"), false);
  gtk_widget_set_size_request(dwb.gui.entry, -1, dwb.misc.bar_height);
  g_object_unref(layout);
} /*}}}*/ /*}}}*/

/* FIFO {{{*/
/* dwb_parse_command_line(const char *line) {{{*/
void 
dwb_parse_command_line(const char *line) {
  char **token = g_strsplit(line, " ", 2);
  KeyMap *m = NULL;
  gboolean found;
  int nummod;

  if (!token[0]) 
    return;
  const char *bak;

  for (GList *l = dwb.keymap; l; l=l->next) {
    bak = token[0];
    found = false;
    m = l->data;
    bak = dwb_parse_nummod(bak);
    nummod = dwb.state.nummod;
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
      if (gtk_widget_has_focus(dwb.gui.entry) && (m->map->entry & EP_ENTRY)) {
        m->map->func(&m, &m->map->arg);
      }
      else {
        commands_simple_command(m);
      }
      break;
    }
  }
  g_strfreev(token);
  dwb_glist_prepend_unique(&dwb.fc.commands, g_strdup(line));
  /* Check for dwb.keymap is necessary for commands that quit dwb. */
  if (dwb.keymap != NULL && m != NULL && !(m->map->prop & CP_HAS_MODE)) {
    dwb_change_mode(NORMAL_MODE, true);
  }
}/*}}}*/

/* dwb_handle_channel {{{*/
static gboolean
dwb_handle_channel(GIOChannel *c, GIOCondition condition, void *data) {
  char *line = NULL;

  g_io_channel_read_line(c, &line, NULL, NULL, NULL);
  if (line) {
    g_strstrip(line);
    dwb_parse_command_line(line);
    g_io_channel_flush(c, NULL);
    g_free(line);
  }
  return true;
}/*}}}*/

/* dwb_init_fifo{{{*/
static void 
dwb_init_fifo(gboolean single) {
  FILE *ff;

  /* Files */
  char *path = util_build_path();
  dwb.files.unifile = g_build_filename(path, "dwb-uni.fifo", NULL);

  dwb.misc.si_channel = NULL;
  if (single) {
    FREE(path);
    return;
  }

  if (GET_BOOL("single-instance")) {
    if (!g_file_test(dwb.files.unifile, G_FILE_TEST_EXISTS)) {
      mkfifo(dwb.files.unifile, 0666);
    }
    int fd = open(dwb.files.unifile, O_WRONLY | O_NONBLOCK);
    if ( (ff = fdopen(fd, "w")) ) {
      if (dwb.misc.argc) {
        for (int i=0; i<dwb.misc.argc; i++) {
          if (g_file_test(dwb.misc.argv[i], G_FILE_TEST_EXISTS) && !g_path_is_absolute(dwb.misc.argv[i])) {
            char *curr_dir = g_get_current_dir();
            path = g_build_filename(curr_dir, dwb.misc.argv[i], NULL);

            fprintf(ff, "add_view %s\n", path);

            FREE(curr_dir);
            FREE(path);
          }
          else {
            fprintf(ff, "add_view %s\n", dwb.misc.argv[i]);
          }
        }
      }
      else {
        fprintf(ff, "add_view\n");
      }
      fclose(ff);
      exit(EXIT_SUCCESS);
    }
    close(fd);
    dwb_open_si_channel();
  }

  /* fifo */
  if (GET_BOOL("use-fifo")) {
    char *filename = g_strdup_printf("%s-%d.fifo", dwb.misc.name, getpid());
    dwb.files.fifo = g_build_filename(path, filename, NULL);
    FREE(filename);

    if (!g_file_test(dwb.files.fifo, G_FILE_TEST_EXISTS)) {
      mkfifo(dwb.files.fifo, 0600);
    }
    dwb_open_channel(dwb.files.fifo);
  }

  FREE(path);
}/*}}}*/
/*}}}*/

/* MAIN {{{*/
int 
main(int argc, char *argv[]) {
  dwb.misc.name = REAL_NAME;
  dwb.misc.profile = "default";
  dwb.misc.argc = 0;
  dwb.misc.prog_path = argv[0];
  dwb.gui.wid = 0;
  int last = 0;
  gboolean single = false;
  int argr = argc;

  gtk_init(&argc, &argv);

  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (! argv[i] )
        continue;
      if (argv[i][0] == '-') {
        if (argv[i][1] == 'l') {
          session_list();
          argr--;
        }
        else if (argv[i][1] == 'p' && argv[i+1]) {
          dwb.misc.profile = argv[++i];
          argr -= 2;
        }
        else if (argv[i][1] == 'n') {
          single = true;
          argr -=1;
        }
        else if (argv[i][1] == 'e' && argv[i+1]) {
          dwb.gui.wid = strtol(argv[++i], NULL, 10);
        }
        else if (argv[i][1] == 'r' ) {
          if (!argv[i+1] || argv[i+1][0] == '-') {
            restore = "default";
            argr--;
          }
          else {
            restore = argv[++i];
            argr -=2;
          }
        }
        else if (argv[i][1] == 'v') {
          printf("%s %s, %s\n", NAME, VERSION, COPYRIGHT);
          return 0;
        }
      }
      else {
        last = i;
        break;
      }
    }
  }
  dwb_init_files();
  dwb_init_settings();
  if (GET_BOOL("save-session") && argr == 1 && !restore && !single) {
    restore = "default";
  }

  if (last) {
    dwb.misc.argv = &argv[last];
    dwb.misc.argc = g_strv_length(dwb.misc.argv);
  }
  dwb_init_fifo(single);
  dwb_init_signals();
  dwb_init();
  gtk_main();
  return EXIT_SUCCESS;
}/*}}}*/
