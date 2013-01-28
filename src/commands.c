/*
 * Copyright (c) 2010-2013 Stefan Bolte <portix@gmx.net>
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

#include "dwb.h"
#include "completion.h"
#include "util.h"
#include "view.h"
#include "session.h"
#include "soup.h"
#include "html.h"
#include "commands.h"
#include "local.h"
#include "entry.h"
#include "adblock.h"
#include "download.h"
#include "js.h"
#include "scripts.h"

inline static int 
dwb_floor(double x) { 
  return x >= 0 ? (int) x : (int) x - 1;
}
inline static int 
modulo(int x, int y) {
  return x - dwb_floor((double)x/y)  * y;
}
/* commands.h {{{*/
/* commands_simple_command(keyMap *km) {{{*/
DwbStatus 
commands_simple_command(KeyMap *km) 
{
    int ret;
    gboolean (*func)(void *, void *) = km->map->func;
    Arg *arg = &km->map->arg;
    arg->e = NULL;

    if (dwb.state.mode & AUTO_COMPLETE) 
    {
        completion_clean_autocompletion();
    }

    if (EMIT_SCRIPT(EXECUTE_COMMAND))
    {
        char *json = util_create_json(3, 
                CHAR, "command", km->map->n.first, 
                CHAR, "argument", arg->p, 
                INTEGER, "nummod", dwb.state.nummod);
        ScriptSignal sig = { NULL, SCRIPTS_SIG_META(json, EXECUTE_COMMAND, 0) } ;

        gboolean prevent = scripts_emit(&sig);
        g_free(json);

        if (prevent) 
            return STATUS_OK;
    }

    ret = func(km, arg);
    switch (ret) 
    {
        case STATUS_OK:
            if (km->map->hide == NEVER_SM) 
                dwb_set_normal_message(dwb.state.fview, false, "%s:", km->map->n.second);
            else if (km->map->hide == ALWAYS_SM) 
            {
                gtk_widget_hide(dwb.gui.entry);
                CLEAR_COMMAND_TEXT();
            }
            break;
        case STATUS_ERROR: 
            dwb_set_error_message(dwb.state.fview, arg->e ? arg->e : km->map->error);
            break;
        case STATUS_END: 
            return ret;
        default: break;
    }
    if (! km->map->arg.ro)
        km->map->arg.p = NULL;
    dwb_clean_key_buffer();
    return ret;
}/*}}}*/
static WebKitWebView * 
commands_get_webview_with_nummod() 
{
    if (dwb.state.nummod > 0 && dwb.state.nummod <= (gint)g_list_length(dwb.state.views)) 
        return WEBVIEW(g_list_nth(dwb.state.views, NUMMOD - 1));
    else 
        return CURRENT_WEBVIEW();
}
static GList * 
commands_get_view_from_nummod() 
{
    if (dwb.state.nummod > 0 && dwb.state.nummod <= (gint)g_list_length(dwb.state.views)) 
        return g_list_nth(dwb.state.views, NUMMOD - 1);
    return dwb.state.fview;
}

/* commands_add_view(KeyMap *, Arg *) {{{*/
DwbStatus 
commands_add_view(KeyMap *km, Arg *arg) 
{
    view_add(arg->p, false);
    if (arg->p == NULL)
        dwb_open_startpage(dwb.state.fview);
    return STATUS_OK;
}/*}}}*/

/* commands_set_setting {{{*/
DwbStatus 
commands_set_setting(KeyMap *km, Arg *arg) 
{
    dwb.state.mode = arg->n;
    entry_focus();
    return STATUS_OK;
}/*}}}*/

/* commands_set_key {{{*/
DwbStatus 
commands_set_key(KeyMap *km, Arg *arg) 
{
    dwb.state.mode = KEY_MODE;
    entry_focus();
    return STATUS_OK;
}/*}}}*/

/* commands_focus_input {{{*/
DwbStatus
commands_focus_input(KeyMap *km, Arg *a) 
{
    char *value;
    DwbStatus ret = STATUS_OK;

    if ( (value = js_call_as_function(MAIN_FRAME(), CURRENT_VIEW()->js_base, "focusInput", NULL, kJSTypeUndefined, &value)) ) 
    {
        if (!g_strcmp0(value, "_dwb_no_input_")) 
            ret = STATUS_ERROR;
        g_free(value);
    }

    return ret;
}/*}}}*/

/* commands_add_search_field(KeyMap *km, Arg *) {{{*/
DwbStatus
commands_add_search_field(KeyMap *km, Arg *a) 
{
    char *value;
    if ( (value = js_call_as_function(MAIN_FRAME(), CURRENT_VIEW()->js_base, "addSearchEngine", NULL, kJSTypeUndefined, &value)) ) {
        if (!g_strcmp0(value, "_dwb_no_hints_")) {
            return STATUS_ERROR;
        }
    }
    dwb.state.mode = SEARCH_FIELD_MODE;
    dwb_set_normal_message(dwb.state.fview, false, "Keyword:");
    entry_focus();
    g_free(value);
    return STATUS_OK;

}/*}}}*/

DwbStatus 
commands_insert_mode(KeyMap *km, Arg *a) 
{
    return dwb_change_mode(INSERT_MODE);
}
DwbStatus 
commands_normal_mode(KeyMap *km, Arg *a) 
{
    dwb_change_mode(NORMAL_MODE, true);
    return STATUS_OK;
}

/* commands_toggle_proxy {{{*/
DwbStatus
commands_toggle_proxy(KeyMap *km, Arg *a) 
{
    WebSettings *s = g_hash_table_lookup(dwb.settings, "proxy");
    s->arg_local.b = !s->arg_local.b;

    dwb_set_proxy(NULL, s);

    return STATUS_OK;
}/*}}}*/

/*commands_find {{{*/
DwbStatus  
commands_find(KeyMap *km, Arg *arg) 
{ 
    View *v = CURRENT_VIEW();
    dwb.state.mode = FIND_MODE;
    dwb.state.search_flags = arg->n;

    if (v->status->search_string) {
        g_free(v->status->search_string);
        v->status->search_string = NULL;
    }

    entry_focus();
    return STATUS_OK;
}/*}}}*/

DwbStatus  
commands_search(KeyMap *km, Arg *arg) 
{ 
    if (!dwb_search(arg)) 
        return STATUS_ERROR;
    return STATUS_OK;
}

/* commands_show_hints {{{*/
DwbStatus
commands_show_hints(KeyMap *km, Arg *arg) 
{
  return dwb_show_hints(arg);
}/*}}}*/

DwbStatus 
commands_show(KeyMap *km, Arg *arg) 
{
  html_load(dwb.state.fview, arg->p);
  return STATUS_OK;
}

/* commands_allow_cookie {{{*/
DwbStatus
commands_allow_cookie(KeyMap *km, Arg *arg) 
{
    switch (arg->n) 
    {
        case COOKIE_ALLOW_PERSISTENT: 
            return dwb_soup_allow_cookie(&dwb.fc.cookies_allow, dwb.files[FILES_COOKIES_ALLOW], arg->n);
        case COOKIE_ALLOW_SESSION:
            return dwb_soup_allow_cookie(&dwb.fc.cookies_session_allow, dwb.files[FILES_COOKIES_SESSION_ALLOW], arg->n);
        case COOKIE_ALLOW_SESSION_TMP:
            dwb_soup_allow_cookie_tmp();
            break;
        default: 
            break;

    }
    return STATUS_OK;
}/*}}}*/

/* commands_bookmark {{{*/
DwbStatus 
commands_bookmark(KeyMap *km, Arg *arg) 
{
    gboolean noerror = STATUS_ERROR;
    if ( (noerror = dwb_prepend_navigation(dwb.state.fview, &dwb.fc.bookmarks)) == STATUS_OK) 
    {
        util_file_add_navigation(dwb.files[FILES_BOOKMARKS], dwb.fc.bookmarks->data, true, -1);
        dwb.fc.bookmarks = g_list_sort(dwb.fc.bookmarks, (GCompareFunc)util_navigation_compare_first);
        dwb_set_normal_message(dwb.state.fview, true, "Saved bookmark: %s", webkit_web_view_get_uri(CURRENT_WEBVIEW()));
    }
    return noerror;
}/*}}}*/

/* commands_quickmark(KeyMap *km, Arg *arg) {{{*/
DwbStatus
commands_quickmark(KeyMap *km, Arg *arg) 
{
    if (dwb.state.nv == OPEN_NORMAL)
        dwb_set_open_mode(arg->i);
    entry_focus();
    dwb.state.mode = arg->n;
    return STATUS_OK;
}/*}}}*/

/* commands_reload(KeyMap *km, Arg *){{{*/
DwbStatus
commands_reload(KeyMap *km, Arg *arg) 
{
    GList *gl = dwb.state.nummod > 0 && dwb.state.nummod <= (gint)g_list_length(dwb.state.views)
        ? g_list_nth(dwb.state.views, dwb.state.nummod-1) 
        : dwb.state.fview;
    dwb_reload(gl);
    return STATUS_OK;
}/*}}}*/

/* commands_reload_bypass_cache {{{*/
DwbStatus
commands_reload_bypass_cache(KeyMap *km, Arg *arg) 
{
    webkit_web_view_reload_bypass_cache(commands_get_webview_with_nummod());
    return STATUS_OK;
}
/*}}}*/

/* commands_stop_loading {{{*/
DwbStatus
commands_stop_loading(KeyMap *km, Arg *arg) 
{
    webkit_web_view_stop_loading(commands_get_webview_with_nummod());
    return STATUS_OK;
}
/*}}}*/

/* commands_view_source(Arg) {{{*/
DwbStatus
commands_view_source(KeyMap *km, Arg *arg) 
{
    WebKitWebView *web = CURRENT_WEBVIEW();
    webkit_web_view_set_view_source_mode(web, !webkit_web_view_get_view_source_mode(web));
    webkit_web_view_reload(web);
    return STATUS_OK;
}/*}}}*/

/* commands_zoom_in(void *arg) {{{*/
DwbStatus
commands_zoom(KeyMap *km, Arg *arg) 
{
    View *v = dwb.state.fview->data;
    WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);

    gfloat zoomlevel = MAX(webkit_web_view_get_zoom_level(web) + arg->i * NUMMOD * GET_DOUBLE("zoom-step"), 0);
    webkit_web_view_set_zoom_level(web, zoomlevel);
    dwb_set_normal_message(dwb.state.fview, true, "Zoomlevel: %d%%", (int)(zoomlevel * 100));
    return STATUS_OK;
}/*}}}*/

/* commands_scroll {{{*/
DwbStatus 
commands_scroll(KeyMap *km, Arg *arg) 
{
    GList *gl = arg->p ? arg->p : dwb.state.fview;
    dwb_scroll(gl, dwb.misc.scroll_step, arg->n);
    return STATUS_OK;
}/*}}}*/

/* commands_set_zoom_level(KeyMap *km, Arg *arg) {{{*/
DwbStatus 
commands_set_zoom_level(KeyMap *km, Arg *arg) 
{
    GList *gl = arg->p ? arg->p : dwb.state.fview;
    double zoomlevel = dwb.state.nummod < 0 ? arg->d : (double)dwb.state.nummod  / 100;

    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(((View*)gl->data)->web), zoomlevel);
    dwb_set_normal_message(dwb.state.fview, true, "Zoomlevel: %d%%", (int)(zoomlevel * 100));
    return STATUS_OK;
}/*}}}*/

/* History {{{*/
DwbStatus 
commands_history(KeyMap *km, Arg *arg) 
{
    return dwb_history(arg);
}/*}}}*/

/* commands_open(KeyMap *km, Arg *arg) {{{*/
DwbStatus  
commands_open(KeyMap *km, Arg *arg) 
{
    if (dwb.state.nv & OPEN_NORMAL)
        dwb_set_open_mode(arg->n & ~SET_URL);

    if (arg)
        dwb.state.type = arg->i;


    if (arg && arg->p && ! (arg->n & SET_URL)) 
    {
        dwb_load_uri(NULL, arg->p);
        CLEAR_COMMAND_TEXT();
        return STATUS_OK;
    }
    else 
    {
        entry_focus();
        if (arg && (arg->n & SET_URL))
            entry_set_text(arg->p ? arg->p : CURRENT_URL());
    }
    return STATUS_OK;
} /*}}}*/

/* commands_open(KeyMap *km, Arg *arg) {{{*/
DwbStatus  
commands_open_startpage(KeyMap *km, Arg *arg) 
{
    return dwb_open_startpage(NULL);
} /*}}}*/

/* commands_remove_view(KeyMap *km, Arg *arg) {{{*/
DwbStatus 
commands_remove_view(KeyMap *km, Arg *arg) 
{
    return view_remove(NULL);
}/*}}}*/

/* commands_focus(KeyMap *km, Arg *arg) {{{*/
DwbStatus 
commands_focus(KeyMap *km, Arg *arg) 
{
    if (dwb.state.views->next) 
    {
        int pos = modulo(g_list_position(dwb.state.views, dwb.state.fview) + NUMMOD * arg->n, g_list_length(dwb.state.views));
        GList *g = g_list_nth(dwb.state.views, pos);
        dwb_focus_view(g, km->map->n.first);
        return STATUS_OK;
    }
    return STATUS_ERROR;
}/*}}}*/

/* commands_focus_view{{{*/
DwbStatus
commands_focus_nth_view(KeyMap *km, Arg *arg) 
{
    GList *l = NULL;
    if (!dwb.state.views->next) 
        return STATUS_OK;

    switch (dwb.state.nummod) 
    {
        case 0  : l = g_list_last(dwb.state.views); break;
        case -1 : l = g_list_first(dwb.state.views); break;
        default : l = g_list_nth(dwb.state.views, dwb.state.nummod - 1); 
    }

    if (l == NULL) 
        return STATUS_ERROR;

    dwb_focus_view(l, km->map->n.first);
    return STATUS_OK;
}/*}}}*/

/* commands_yank {{{*/
DwbStatus
commands_yank(KeyMap *km, Arg *arg) 
{
    const char *text = NULL;
    GdkAtom atom = GDK_POINTER_TO_ATOM(arg->p);
    WebKitWebView *wv = commands_get_webview_with_nummod();

    if (arg->n == CA_URI) 
        text = webkit_web_view_get_uri(wv);
    else if (arg->n == CA_TITLE)
        text = webkit_web_view_get_title(wv);

    return dwb_set_clipboard(text, atom);
}/*}}}*/

/* commands_paste {{{*/
DwbStatus
commands_paste(KeyMap *km, Arg *arg) 
{
    GdkAtom atom = GDK_POINTER_TO_ATOM(arg->p);
    char *text = NULL;

    if ( (text = dwb_clipboard_get_text(atom)) ) 
    {
        if (dwb.state.nv == OPEN_NORMAL)
            dwb_set_open_mode(arg->n);
        dwb_load_uri(NULL, text);
        g_free(text);
        return STATUS_OK;
    }
    return STATUS_ERROR;
}/*}}}*/

/* dwb_entry_movement() {{{*/
DwbStatus
commands_entry_movement(KeyMap *m, Arg *a) 
{
    entry_move_cursor_step(a->n, a->i, a->b);
    return STATUS_OK;
}/*}}}*/

/* commands_entry_history_forward {{{*/
DwbStatus 
commands_entry_history_forward(KeyMap *km, Arg *a) 
{
    if (dwb.state.mode == COMMAND_MODE) 
        return entry_history_forward(&dwb.state.last_com_history);
    else 
        return entry_history_forward(&dwb.state.last_nav_history);
}/*}}}*/

/* commands_entry_history_back {{{*/
DwbStatus 
commands_entry_history_back(KeyMap *km, Arg *a) 
{
    if (dwb.state.mode == COMMAND_MODE) 
        return entry_history_back(&dwb.fc.commands, &dwb.state.last_com_history);
    else 
        return entry_history_back(&dwb.fc.navigations, &dwb.state.last_nav_history);
}/*}}}*/

/* commands_entry_confirm {{{*/
DwbStatus 
commands_entry_confirm(KeyMap *km, Arg *a) 
{
    gboolean ret;
    GdkEventKey e = { .state = 0, .keyval = GDK_KEY_Return };
    g_signal_emit_by_name(dwb.gui.entry, "key-press-event", &e, &ret);

    return STATUS_OK;
}/*}}}*/
/* commands_entry_escape {{{*/
DwbStatus 
commands_entry_escape(KeyMap *km, Arg *a) 
{
    dwb_change_mode(NORMAL_MODE, true);
    return STATUS_OK;
}/*}}}*/

/* commands_save_session {{{*/
DwbStatus
commands_save_session(KeyMap *km, Arg *arg) 
{
    if (arg->n == NORMAL_MODE) 
    {
        dwb.state.mode = SAVE_SESSION;
        session_save(NULL, SESSION_FORCE);
        dwb_end();
        return STATUS_END;
    }
    else 
    {
        dwb.state.mode = arg->n;
        entry_focus();
        dwb_set_normal_message(dwb.state.fview, false, "Session name:");
    }
    return STATUS_OK;
}/*}}}*/

/* commands_bookmarks {{{*/
DwbStatus
commands_bookmarks(KeyMap *km, Arg *arg) 
{
    if (!g_list_length(dwb.fc.bookmarks)) 
        return STATUS_ERROR;

    if (dwb.state.nv == OPEN_NORMAL)
        dwb_set_open_mode(arg->n);

    completion_complete(COMP_BOOKMARK, 0);
    entry_focus();

    if (dwb.comps.active_comp != NULL) 
        completion_set_entry_text((Completion*)dwb.comps.active_comp->data);

    return STATUS_OK;
}/*}}}*/

/* commands_history{{{*/
DwbStatus  
commands_complete_type(KeyMap *km, Arg *arg) 
{
    return completion_complete(arg->n, 0);
}/*}}}*/

void
commands_toggle(Arg *arg, const char *filename, GList **tmp, GList **pers, const char *message) 
{
    char *host = NULL;
    const char *block = NULL;
    gboolean allowed;

    if (arg->n & ALLOW_HOST) 
    {
        host = dwb_get_host(CURRENT_WEBVIEW());
        block = host;
    }
    else if (arg->n & ALLOW_URI) 
        block = webkit_web_view_get_uri(CURRENT_WEBVIEW());
    else if (arg->p) 
        block = arg->p;
    if (block != NULL) 
    {
        if (arg->n & ALLOW_TMP) 
        {
            GList *l;
            if ( (l = g_list_find_custom(*tmp, block, (GCompareFunc)g_strcmp0)) ) 
            {
                g_free(l->data);
                *tmp = g_list_delete_link(*tmp, l);
                allowed = false;
            }
            else 
            {
                *tmp = g_list_prepend(*tmp, g_strdup(block));
                allowed = true;
            }
            dwb_set_normal_message(dwb.state.fview, true, "%s temporarily %s for %s", message, allowed ? "allowed" : "blocked", block);
        }
        else 
        {
            allowed = dwb_toggle_allowed(filename, block, pers);
            dwb_set_normal_message(dwb.state.fview, true, "%s %s for %s", message, allowed ? "allowed" : "blocked", block);
        }
    }
    else 
        CLEAR_COMMAND_TEXT();
}

DwbStatus 
commands_toggle_plugin_blocker(KeyMap *km, Arg *arg) 
{
    commands_toggle(arg, dwb.files[FILES_PLUGINS_ALLOW], &dwb.fc.tmp_plugins, &dwb.fc.pers_plugins, "Plugins");
    return STATUS_OK;
}

/* commands_toggle_scripts {{{ */
DwbStatus 
commands_toggle_scripts(KeyMap *km, Arg *arg) 
{
    commands_toggle(arg, dwb.files[FILES_SCRIPTS_ALLOW], &dwb.fc.tmp_scripts,  &dwb.fc.pers_scripts, "Scripts");
    return STATUS_OK;
}/*}}}*/

/* commands_new_window_or_view{{{*/
DwbStatus 
commands_new_window_or_view(KeyMap *km, Arg *arg) 
{
    dwb_set_open_mode(arg->n | OPEN_EXPLICIT);
    return STATUS_OK;
}/*}}}*/

/* commands_save_files(KeyMap *km, Arg *arg) {{{*/
DwbStatus 
commands_save_files(KeyMap *km, Arg *arg) 
{
  if (dwb_save_files(false)) 
  {
      dwb_set_normal_message(dwb.state.fview, true, "Configuration files saved");
      return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_undo() {{{*/
DwbStatus
commands_undo(KeyMap *km, Arg *arg) 
{
  if (dwb.state.undo_list) 
  {
      WebKitWebView *web = WEBVIEW(view_add(NULL, false));
      WebKitWebBackForwardList *bflist = webkit_web_back_forward_list_new_with_web_view(web);

      for (GList *l = dwb.state.undo_list->data; l; l=l->next) 
      {
          Navigation *n = l->data;
          WebKitWebHistoryItem *item = webkit_web_history_item_new_with_data(n->first, n->second);

          webkit_web_back_forward_list_add_item(bflist, item);
          if (!l->next) 
              webkit_web_view_go_to_back_forward_item(web, item);

          dwb_navigation_free(n);
      }
      g_list_free(dwb.state.undo_list->data);
      dwb.state.undo_list = g_list_delete_link(dwb.state.undo_list, dwb.state.undo_list);
      return STATUS_OK;
  }
  return STATUS_ERROR;
}/*}}}*/

/* commands_print (Arg *) {{{*/
DwbStatus
commands_print(KeyMap *km, Arg *arg) 
{
    WebKitWebView *wv = commands_get_webview_with_nummod();
    WebKitWebFrame *frame = webkit_web_view_get_focused_frame(wv);
    if (frame) 
    {
        char *print_command = GET_CHAR("print-previewer");
        if (print_command) 
            g_object_set(gtk_settings_get_default(), "gtk-print-preview-command", print_command, NULL);

        webkit_web_frame_print(frame);
        return STATUS_OK;
    }
    return STATUS_ERROR;
}/*}}}*/

DwbStatus
commands_print_preview(KeyMap *km, Arg *arg) 
{
    WebKitWebView *wv = commands_get_webview_with_nummod();
    WebKitWebFrame *frame = webkit_web_view_get_focused_frame(wv);
    if (frame) 
    {
        char *print_command = GET_CHAR("print-previewer");
        if (print_command) 
            g_object_set(gtk_settings_get_default(), "gtk-print-preview-command", print_command, NULL);

        GtkPrintOperation *operation = gtk_print_operation_new();
        webkit_web_frame_print_full(frame, operation, GTK_PRINT_OPERATION_ACTION_PREVIEW, NULL);
        g_object_unref(operation);
        return STATUS_OK;
    }
    return STATUS_ERROR;
}/*}}}*/


/* commands_web_inspector {{{*/
DwbStatus
commands_web_inspector(KeyMap *km, Arg *arg) 
{
    if (GET_BOOL("enable-developer-extras")) 
    {
        WebKitWebView *wv = commands_get_webview_with_nummod();
        webkit_web_inspector_show(webkit_web_view_get_inspector(wv));
        return STATUS_OK;
    }
    return STATUS_ERROR;
}/*}}}*/

/* commands_execute_userscript (Arg *) {{{*/
DwbStatus
commands_execute_userscript(KeyMap *km, Arg *arg) 
{
    if (!dwb.misc.userscripts) 
        return STATUS_ERROR;

    if (arg->p) 
    {
        char *path = g_build_filename(dwb.files[FILES_USERSCRIPTS], arg->p, NULL);
        Arg a = { .arg = path };
        dwb_execute_user_script(km, &a);
        g_free(path);
    }
    else 
    {
        entry_focus();
        completion_complete(COMP_USERSCRIPT, 0);
    }

    return STATUS_OK;
}/*}}}*/

/* commands_toggle_hidden_files {{{*/
DwbStatus
commands_toggle_hidden_files(KeyMap *km, Arg *arg) 
{
    dwb.state.hidden_files = !dwb.state.hidden_files;
    dwb_reload(dwb.state.fview);
    return STATUS_OK;
}/*}}}*/

/* commands_quit {{{*/
DwbStatus
commands_quit(KeyMap *km, Arg *arg) 
{
    dwb_end();
    return STATUS_END;
}/*}}}*/

DwbStatus
commands_reload_user_scripts(KeyMap *km, Arg *arg) 
{
    dwb_reload_userscripts();
    return STATUS_OK;
}

/* commands_reload_scripts {{{*/
DwbStatus
commands_fullscreen(KeyMap *km, Arg *arg) 
{
    if (dwb.state.bar_visible & BAR_PRESENTATION)
        return STATUS_ERROR;
    dwb.state.fullscreen = !dwb.state.fullscreen;

    if (dwb.state.fullscreen) 
        gtk_window_fullscreen(GTK_WINDOW(dwb.gui.window));
    else 
        gtk_window_unfullscreen(GTK_WINDOW(dwb.gui.window));

    return STATUS_OK;
}/*}}}*/
/* commands_reload_scripts {{{*/
DwbStatus
commands_open_editor(KeyMap *km, Arg *arg) 
{
    return dwb_open_in_editor();
}/*}}}*/

/* dwb_command_mode {{{*/
DwbStatus
commands_command_mode(KeyMap *km, Arg *arg) 
{
    return dwb_change_mode(COMMAND_MODE);
}/*}}}*/
/* dwb_command_mode {{{*/
DwbStatus
commands_only(KeyMap *km, Arg *arg) 
{
    DwbStatus ret = STATUS_ERROR;
    GList *l = dwb.state.views, *next;
    while (l) 
    {
        next = l->next;
        if (l != dwb.state.fview) 
        {
            view_remove(l);
            ret = STATUS_OK;
        }
        l = next;
    }
    return ret;
}/*}}}*/
static void 
commands_set_bars(int status) 
{
    gtk_widget_set_visible(dwb.gui.topbox, (status & BAR_VIS_TOP) && (GET_BOOL("show-single-tab") || dwb.state.views->next));
    gtk_widget_set_visible(dwb.gui.bottombox, status & BAR_VIS_STATUS);
    if ((status & BAR_VIS_STATUS) ) 
        dwb_dom_remove_from_parent(WEBKIT_DOM_NODE(CURRENT_VIEW()->hover.element), NULL);
}
/* commands_toggle_bars {{{*/
DwbStatus
commands_toggle_bars(KeyMap *km, Arg *arg) 
{
    dwb.state.bar_visible ^= arg->n;
    commands_set_bars(dwb.state.bar_visible);
    return STATUS_OK;
}/*}}}*/
/* commands_presentation_mode {{{*/
DwbStatus
commands_presentation_mode(KeyMap *km, Arg *arg) 
{
    static int last;
    if (dwb.state.bar_visible & BAR_PRESENTATION) 
    {
        if (! dwb.state.fullscreen)
            gtk_window_unfullscreen(GTK_WINDOW(dwb.gui.window));

        commands_set_bars(last);
        dwb.state.bar_visible = last & ~BAR_PRESENTATION;
    }
    else 
    {
        dwb.state.bar_visible |= BAR_PRESENTATION;
        if (! dwb.state.fullscreen)
            gtk_window_fullscreen(GTK_WINDOW(dwb.gui.window));
        commands_set_bars(0);
        last = dwb.state.bar_visible;
        dwb.state.bar_visible = BAR_PRESENTATION;
    }
    return STATUS_OK;
}/*}}}*/
/* commands_toggle_lock_protect {{{*/
DwbStatus
commands_toggle_lock_protect(KeyMap *km, Arg *arg) 
{
  GList *gl = dwb.state.nummod < 0 ? dwb.state.fview : g_list_nth(dwb.state.views, dwb.state.nummod-1);
  if (gl == NULL)
    return STATUS_ERROR;

  View *v = VIEW(gl);
  v->status->lockprotect ^= arg->n;
  dwb_update_status(gl, NULL);

  if (arg->n & LP_VISIBLE && gl != dwb.state.fview)
    gtk_widget_set_visible(v->scroll, LP_VISIBLE(v));

  return STATUS_OK;
}/*}}}*/
/* commands_execute_javascript {{{*/
DwbStatus
commands_execute_javascript(KeyMap *km, Arg *arg) 
{
    static char *script;
    if (arg->p == NULL && script == NULL)
        return STATUS_ERROR;
    if (arg->p) {
        FREE0(script);
        script = g_strdup(arg->p);
    }
    WebKitWebView *wv = commands_get_webview_with_nummod();
    dwb_execute_script(webkit_web_view_get_focused_frame(wv), script, false);
    return STATUS_OK;
}/*}}}*/
/* commands_set {{{*/
DwbStatus
commands_set(KeyMap *km, Arg *arg) 
{
    DwbStatus ret = STATUS_ERROR;
    const char *command = util_str_chug(arg->p);
    if (command && *command) 
    {
        char **args = g_strsplit(command, " ", 2);
        if (g_strv_length(args) >= 2)
            ret = dwb_set_setting(args[0], args[1], arg->n);
        g_strfreev(args);
    }
    return ret;
}/*}}}*/
/* commands_set {{{*/
DwbStatus
commands_toggle_setting(KeyMap *km, Arg *arg) 
{
    const char *command = util_str_chug(arg->p);
    return dwb_toggle_setting(command, arg->n);
}/*}}}*/
DwbStatus
commands_tab_move(KeyMap *km, Arg *arg) 
{
    GList *sibling;
    int newpos;
    int l = g_list_length(dwb.state.views);
    if (dwb.state.views->next == NULL) 
        return STATUS_ERROR;
    switch (arg->n) 
    {
        case TAB_MOVE_LEFT   : newpos = MAX(MIN(l-1, g_list_position(dwb.state.views, dwb.state.fview)-NUMMOD), 0); break;
        case TAB_MOVE_RIGHT  : newpos = MAX(MIN(l-1, g_list_position(dwb.state.views, dwb.state.fview)+NUMMOD), 0); break;
        default :  newpos = MAX(MIN(l, NUMMOD)-1, 0); break;
    }
    gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), CURRENT_VIEW()->tabevent, l-(newpos+1));
    gtk_box_reorder_child(GTK_BOX(dwb.gui.mainbox), CURRENT_VIEW()->scroll, newpos);

    dwb.state.views = g_list_remove_link(dwb.state.views, dwb.state.fview);

    sibling = g_list_nth(dwb.state.views, newpos);
    g_list_position(dwb.state.fview, sibling);

    if (sibling == NULL) 
    {
        GList *last = g_list_last(dwb.state.views);
        last->next = dwb.state.fview;
        dwb.state.fview->prev = last;
    }
    else if (sibling->prev == NULL) {
        dwb.state.views->prev = dwb.state.fview;
        dwb.state.fview->next = dwb.state.views;
        dwb.state.views = dwb.state.fview;
    }
    else 
    {
        sibling->prev->next = dwb.state.fview;
        dwb.state.fview->prev = sibling->prev;
        sibling->prev = dwb.state.fview;
        dwb.state.fview->next = sibling;
    }
    dwb_focus(dwb.state.fview);
    dwb_update_layout();
    return STATUS_OK;
}
DwbStatus 
commands_clear_tab(KeyMap *km, Arg *arg) 
{
    GList *gl = commands_get_view_from_nummod();
    view_clear_tab(gl);
    return STATUS_OK;
}
DwbStatus 
commands_cancel_download(KeyMap *km, Arg *arg) 
{
    return download_cancel(dwb.state.nummod);
}
DwbStatus 
commands_dump(KeyMap *km, Arg *arg) 
{
    char *data = dwb_get_raw_data(dwb.state.fview);
    if (data == NULL) 
        return STATUS_ERROR;

    if (arg->p == NULL) 
        puts(data);
    else 
    {
        util_set_file_content(arg->p, data);
        arg->p = NULL;
    }
    return STATUS_OK;
}
/*{{{*/
DwbStatus 
commands_sanitize(KeyMap *km, Arg *arg) 
{
    Sanitize s = util_string_to_sanitize(arg->p);
    if (s == SANITIZE_ERROR) 
        return STATUS_ERROR;

    if (s & SANITIZE_HISTORY) 
    {
        dwb_free_list(dwb.fc.history, (void_func)dwb_navigation_free);
        dwb.fc.history = NULL;
        remove(dwb.files[FILES_HISTORY]);
    }
    if (s & (SANITIZE_HISTORY | SANITIZE_CACHE)) 
    {
        for (GList *gl = dwb.state.views; gl; gl=gl->next) 
        {
            WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(WEBVIEW(gl));
            webkit_web_back_forward_list_clear(bf_list);
        }
    }
    if (s & SANITIZE_COOKIES) 
        remove(dwb.files[FILES_COOKIES]);

    if (s & (SANITIZE_CACHE | SANITIZE_COOKIES)) 
        dwb_soup_clear_cookies();

    if (s & (SANITIZE_SESSION)) 
        session_clear_session();

    if (s & (SANITIZE_ALLSESSIONS)) 
        remove(dwb.files[FILES_SESSION]);

    dwb_set_normal_message(dwb.state.fview, true, "Sanitized %s", arg->p ? arg->p : "all");
    return STATUS_OK;
}/*}}}*/
DwbStatus 
commands_eval(KeyMap *km, Arg *arg) 
{
  if (arg->p != NULL && scripts_execute_one(arg->p)) 
  {
      return STATUS_OK;
  }
  return STATUS_ERROR;
}
/*}}}*/

DwbStatus 
commands_download(KeyMap *km, Arg *arg) 
{
    WebKitNetworkRequest *request = webkit_network_request_new(CURRENT_URL());
    if (request != NULL) 
    {
        dwb.state.mimetype_request = NULL;
        WebKitDownload *download = webkit_download_new(request);
        download_get_path(dwb.state.fview, download);
    }
    return STATUS_OK;
}
DwbStatus 
commands_toggle_tab(KeyMap *km, Arg *arg) 
{
    GList *last = g_list_nth(dwb.state.views, dwb.state.last_tab);
    if (last) 
    {
        dwb_focus_view(last, km->map->n.first);
        return STATUS_OK;
    }
    return STATUS_ERROR;
}
DwbStatus
commands_reload_bookmarks(KeyMap *km, Arg *arg)
{
    dwb_reload_bookmarks();
    return STATUS_OK;
}
DwbStatus
commands_reload_quickmarks(KeyMap *km, Arg *arg)
{
    dwb_reload_quickmarks();
    return STATUS_OK;
}
