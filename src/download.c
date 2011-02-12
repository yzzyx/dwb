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

#include "download.h"

typedef struct _DwbDownload {
  GtkWidget *event;
  GtkWidget *rlabel;
  GtkWidget *llabel;
  WebKitDownload *download;
  DownloadAction action;
  char *path;
} DwbDownload;

static GList *downloads = NULL;
static char *lastdir = NULL;

/*  dwb_get_command_from_mimetype(char *mimetype){{{*/
static char *
dwb_get_command_from_mimetype(char *mimetype) {
  char *command = NULL;
  for (GList *l = dwb.fc.mimetypes; l; l=l->next) {
    Navigation *n = l->data;
    if (!strcmp(n->first, mimetype)) {
      command = n->second;
      break;
    }
  }
  return command;
}/*}}}*/

/* dwb_get_download_command {{{*/
static char *
dwb_get_download_command(const char *uri, const char *output) {
  char *command = g_strdup(GET_CHAR("download-external-command"));
  char *newcommand = NULL;

  if ( (newcommand = dwb_util_string_replace(command, "dwb_uri", uri)) ) {
    FREE(command);
    command = newcommand;
  }
  if ( (newcommand = dwb_util_string_replace(command, "dwb_cookies", dwb.files.cookies)) ) {
    FREE(command);
    command = newcommand;
  }
  if ( (newcommand = dwb_util_string_replace(command, "dwb_output", output)) ) {
    FREE(command);
    command = newcommand;
  }
  if ( GET_BOOL("use-fifo") && (newcommand = dwb_util_string_replace(command, "dwb_fifo", dwb.files.fifo)) ) {
    FREE(command);
    command = newcommand;
  }
  return command;
}/*}}}*/

/* dwb_dl_get_download_label(WebKitDownload *) {{{*/
static GList * 
dwb_dl_get_download_label(WebKitDownload *download) {
  for (GList *l = downloads; l; l=l->next) {
    DwbDownload *label = l->data;
    if (label->download == download) {
      return l;
    }
  }
  return NULL;
}/*}}}*/

/* dwb_dl_progress_cb(WebKitDownload *) {{{*/
static void
dwb_dl_progress_cb(WebKitDownload *download) {
  GList *l = dwb_dl_get_download_label(download); 
  DwbDownload *label = l->data;

  double elapsed = webkit_download_get_elapsed_time(download);
  double progress = webkit_download_get_progress(download);


  double current_size = (double)webkit_download_get_current_size(download) / 0x100000;
  double total_size = (double)webkit_download_get_total_size(download) / 0x100000;
  guint remaining = (guint)(elapsed / progress - elapsed);
  char *message = g_strdup_printf("[%d:%02d][%d%%][%.3f/%.3f]", remaining/60, remaining%60,  (int)(progress*100), current_size,  total_size);
  gtk_label_set_text(GTK_LABEL(label->rlabel), message);
  FREE(message);

  guint blue = ((1 - progress) * 0xaa);
  guint green = progress * 0xaa;
  char *colorstring = g_strdup_printf("#%02x%02x%02x", 0x00, green, blue);

  GdkColor color; 
  gdk_color_parse(colorstring, &color);
  gtk_widget_modify_bg(label->event, GTK_STATE_NORMAL, &color);
  FREE(colorstring);
}/*}}}*/

/* dwb_dl_set_mimetype(const char *) {{{*/
static void
dwb_dl_set_mimetype(const char *command) {
  GList *list = NULL;
  if (dwb.state.mimetype_request) {
    Navigation *n = dwb_navigation_new(dwb.state.mimetype_request, command);
    if ( (list = g_list_find_custom(dwb.fc.mimetypes, n, (GCompareFunc)dwb_util_navigation_compare_first))) {
      g_free(list->data);
      dwb.fc.mimetypes = g_list_delete_link(dwb.fc.mimetypes, list);
    }
    dwb.fc.mimetypes = g_list_prepend(dwb.fc.mimetypes, n);
    dwb_util_file_add_navigation(dwb.files.mimetypes, n, true, -1);
  }
}/*}}}*/

/* dwb_dl_spawn(DwbDownload *) {{{*/
static void 
dwb_dl_spawn(DwbDownload *dl) {
  GError *error = NULL;
  const char *filename = webkit_download_get_destination_uri(dl->download);
  char *command = g_strconcat(dl->path, " ", filename + 7, NULL);

  char **argv = g_strsplit(command, " ", -1);
  if (! g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, &error) ) {
    fprintf(stderr, "Couldn't open %s with %s : %s\n", filename + 7, dl->path, error->message);
    g_clear_error(&error);
  }
  dwb_dl_set_mimetype(dl->path);
  FREE(command);
  g_strfreev(argv);
}/*}}}*/

/* dwb_dl_status_cb(WebKitDownload *) {{{*/
static void
dwb_dl_status_cb(WebKitDownload *download) {
  WebKitDownloadStatus status = webkit_download_get_status(download);

  if (status == WEBKIT_DOWNLOAD_STATUS_FINISHED || status == WEBKIT_DOWNLOAD_STATUS_CANCELLED) {
    GList *list = dwb_dl_get_download_label(download);
    if (list) {
      DwbDownload *label = list->data;
      if (label->action == DL_ACTION_EXECUTE && status == WEBKIT_DOWNLOAD_STATUS_FINISHED) {
        dwb_dl_spawn(label);
      }
      gtk_widget_destroy(label->event);
      FREE(label->path);
      downloads = g_list_delete_link(downloads, list);
    }
    if (!downloads) {
      gtk_widget_hide(dwb.gui.downloadbar);
    }
    if (dwb.state.mimetype_request) {
      g_free(dwb.state.mimetype_request);
      dwb.state.mimetype_request = NULL;
    }
  }
}/*}}}*/

/* dwb_dl_button_press_cb(GtkWidget *w, GdkEventButton *e, GList *) {{{*/
static gboolean 
dwb_dl_button_press_cb(GtkWidget *w, GdkEventButton *e, GList *gl) {
  if (e->button == 3) {
    DwbDownload *label = gl->data;
    webkit_download_cancel(label->download);
  }
  return false;
}/*}}}*/

/* dwb_dl_add_progress_label (GList *gl, const char *filename) {{{*/
static DwbDownload *
dwb_dl_add_progress_label(GList *gl, const char *filename) {
  DwbDownload *l = g_malloc(sizeof(DwbDownload));

  GtkWidget *hbox = gtk_hbox_new(false, 5);
  l->event = gtk_event_box_new();
  l->rlabel = gtk_label_new("???");
  char *escaped  = g_markup_escape_text(filename, -1);
  l->llabel = gtk_label_new(escaped);

  gtk_box_pack_start(GTK_BOX(hbox), l->llabel, false, false, 1);
  gtk_box_pack_start(GTK_BOX(hbox), l->rlabel, false, false, 1);
  gtk_container_add(GTK_CONTAINER(l->event), hbox);
  gtk_box_pack_start(GTK_BOX(dwb.gui.downloadbar), l->event, false, false, 2);

  gtk_label_set_use_markup(GTK_LABEL(l->llabel), true);
  gtk_label_set_use_markup(GTK_LABEL(l->rlabel), true);
  gtk_misc_set_alignment(GTK_MISC(l->llabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(l->rlabel), 1.0, 0.5);
  gtk_widget_modify_fg(l->llabel, GTK_STATE_NORMAL, &dwb.color.download_fg);
  gtk_widget_modify_fg(l->rlabel, GTK_STATE_NORMAL, &dwb.color.download_fg);
  gtk_widget_modify_bg(l->event, GTK_STATE_NORMAL, &dwb.color.download_bg);
  gtk_widget_modify_font(l->llabel, dwb.font.fd_bold);
  gtk_widget_modify_font(l->rlabel, dwb.font.fd_bold);

  l->download = dwb.state.download;

  gtk_widget_show_all(dwb.gui.downloadbar);
  return l;
}/*}}}*/

/* dwb_dl_start {{{*/
void 
dwb_dl_start() {
  const char *path = GET_TEXT();
  char *fullpath;
  const char *filename = webkit_download_get_suggested_filename(dwb.state.download);
  const char *uri = webkit_download_get_uri(dwb.state.download);
  char *command = NULL;
  gboolean external = GET_BOOL("download-use-external-program");
  
  if (g_str_has_prefix(uri, "file://") && g_file_test(uri + 7, G_FILE_TEST_EXISTS)) {
    GError *error = NULL;
    char *command = g_strconcat(path, " ", uri + 7, NULL);
    char **argv = g_strsplit(command, " ", -1);
    if (! g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, &error) ) {
      fprintf(stderr, "Couldn't open %s with %s : %s\n", uri + 7, path, error->message);
      g_clear_error(&error);
    }
    dwb_dl_set_mimetype(path);
    g_strfreev(argv);
    g_free(command);

    return;
  }
  if (path[0] != '/') {
    dwb_set_error_message(dwb.state.fview, "A full path must be specified");
    dwb_normal_mode(false);
    return;
  }

  if (!filename || !strlen(filename)) {
    filename = "dwb_download";
  }

  if (dwb.state.dl_action == DL_ACTION_EXECUTE) {
    fullpath = g_build_filename("file:///tmp", filename, NULL);
  }
  else {
    if (!path || !strlen(path)) {
      path = g_get_current_dir();
    }
    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
      fullpath = external ? g_build_filename(path, filename, NULL) : g_build_filename("file://", path, filename, NULL);
    }
    else {
      filename = strrchr(path, '/')+1;
      fullpath = external ? g_strdup(path) : g_build_filename("file://", path, NULL);
    }
  }

  if (external && dwb.state.dl_action == DL_ACTION_DOWNLOAD) {
    command = dwb_get_download_command(uri, fullpath);
    if (!g_spawn_command_line_async(command, NULL)) {
      dwb_set_error_message(dwb.state.fview, "Cannot spawn download program");
    }
  }
  else {
    webkit_download_set_destination_uri(dwb.state.download, fullpath);
    DwbDownload *active = dwb_dl_add_progress_label(dwb.state.fview, filename);
    active->action = dwb.state.dl_action;
    active->path = g_strdup(path);
    gtk_widget_show_all(dwb.gui.downloadbar);
    downloads = g_list_prepend(downloads, active);
    g_signal_connect(active->event, "button-press-event", G_CALLBACK(dwb_dl_button_press_cb), downloads);
    g_signal_connect(dwb.state.download, "notify::progress", G_CALLBACK(dwb_dl_progress_cb), NULL);
    g_signal_connect(dwb.state.download, "notify::status", G_CALLBACK(dwb_dl_status_cb), NULL);
    webkit_download_start(dwb.state.download);
  }
  FREE(lastdir);
  if (dwb.state.dl_action != DL_ACTION_EXECUTE) {
    lastdir = g_strdup(path);
  }

  dwb_normal_mode(true);
  dwb.state.download = NULL;
  FREE(fullpath);
}/*}}}*/

/* dwb_dl_entry_set_directory() {{{*/
static void
dwb_dl_entry_set_directory() {
  dwb_set_normal_message(dwb.state.fview, false, "Downloadpath:");
  char *current_dir = lastdir ? g_strdup(lastdir) : g_get_current_dir();
  char *newdir = current_dir[strlen(current_dir) - 1] != '/' ? g_strdup_printf("%s/", current_dir) : g_strdup(current_dir);

  dwb_entry_set_text(newdir);

  FREE(current_dir);
  FREE(newdir);
}/*}}}*/

/* dwb_dl_entry_set_spawn_command{{{*/
void
dwb_dl_entry_set_spawn_command(const char *command) {
  if (!command && dwb.state.mimetype_request) {
    command = dwb_get_command_from_mimetype(dwb.state.mimetype_request);
  }
  dwb_set_normal_message(dwb.state.fview, false, "Spawn (%s):", dwb.state.mimetype_request ? dwb.state.mimetype_request : "???");
  dwb_entry_set_text(command ? command : "");
}/*}}}*/

/* dwb_dl_get_path {{{*/
void 
dwb_dl_get_path(GList *gl, WebKitDownload *d) {
  const char *uri = webkit_download_get_uri(d) + 7;

  if (g_file_test(uri,  G_FILE_TEST_IS_EXECUTABLE)) {
    g_spawn_command_line_async(uri, NULL);
    return;
  }

  char *command = NULL;
  dwb_focus_entry();
  dwb.state.mode = DOWNLOAD_GET_PATH;
  dwb.state.download = d;
  if ( (dwb.state.mimetype_request && (command = dwb_get_command_from_mimetype(dwb.state.mimetype_request))) ||  g_file_test(uri, G_FILE_TEST_EXISTS)) {
    dwb.state.dl_action = DL_ACTION_EXECUTE;
    dwb_dl_entry_set_spawn_command(command);
  }
  else {
    dwb_dl_entry_set_directory();
  }
}/*}}}*/

/* dwb_dl_set_execute {{{*/
// TODO complete path should not be in download.c
void 
dwb_dl_set_execute(Arg *arg) {
  if (dwb.state.mode == DOWNLOAD_GET_PATH) {
    if (dwb.state.dl_action == DL_ACTION_DOWNLOAD) {
      dwb.state.dl_action = DL_ACTION_EXECUTE;
      dwb_dl_entry_set_spawn_command(NULL);
    }
    else {
      dwb.state.dl_action = DL_ACTION_DOWNLOAD;
      dwb_dl_entry_set_directory();
    }
  }
}/*}}}*/
