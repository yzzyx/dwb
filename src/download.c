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
#include "dwb.h"
#include "entry.h"
#include "util.h"
#include "soup.h"

typedef struct _DwbDownloadStatus {
#if _HAS_GTK3 
  gdouble blue, red, green, alpha;
#else
  guint blue, red, green;
#endif
  gint64 time;
} DwbDownloadStatus;

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
static DownloadAction lastaction;

/*  dwb_get_command_from_mimetype(char *mimetype){{{*/
static char *
download_get_command_from_mimetype(char *mimetype) {
  char *command = NULL;
  if (mimetype == NULL)
    return NULL;
  for (GList *l = dwb.fc.mimetypes; l; l=l->next) {
    Navigation *n = l->data;
    if (!g_strcmp0(n->first, mimetype)) {
      command = n->second;
      break;
    }
  }
  return command;
}/*}}}*/

/* dwb_get_download_command {{{*/
static char *
download_get_command(const char *uri, const char *output) {
  char *command = g_strdup(GET_CHAR("download-external-command"));
  char *newcommand = NULL;
  WebKitNetworkRequest *request = webkit_download_get_network_request(dwb.state.download);
  const char *referer = soup_get_header_from_request(request, "Referer");

  if ( (newcommand = util_string_replace(command, "dwb_uri", uri)) ) {
    g_free(command);
    command = newcommand;
  }
  if ( (newcommand = util_string_replace(command, "dwb_cookies", dwb.files.cookies)) ) {
    g_free(command);
    command = newcommand;
  }
  if ( (newcommand = util_string_replace(command, "dwb_output", output)) ) {
    g_free(command);
    command = newcommand;
  }
  if (referer != NULL) {
    if ( (newcommand = util_string_replace(command, "dwb_referer", referer)) ) {
      g_free(command);
      command = newcommand;
    }
  }
  return command;
}/*}}}*/

/* download_get_download_label(WebKitDownload *) {{{*/
static GList * 
download_get_download_label(WebKitDownload *download) {
  for (GList *l = downloads; l; l=l->next) {
    DwbDownload *label = l->data;
    if (label->download == download) {
      return l;
    }
  }
  return NULL;
}/*}}}*/

/* download_progress_cb(WebKitDownload *) {{{*/
static void
download_progress_cb(WebKitDownload *download, GParamSpec *p, DwbDownloadStatus *status) {
  GList *l = download_get_download_label(download); 
  DwbDownload *label = l->data;
  /* Update at most four times a second */
  gint64 time = g_get_monotonic_time()/250000;
  if (time != status->time) {
    double elapsed = webkit_download_get_elapsed_time(download);
    double progress = webkit_download_get_progress(download);
    double total_size = (double)webkit_download_get_total_size(download) / 0x100000;


    double current_size = (double)webkit_download_get_current_size(download) / 0x100000;
    guint remaining = (guint)(elapsed / progress - elapsed);
    char *message = g_strdup_printf("[%d:%02d|%2d%%|%.3f/%.3f]", remaining/60, remaining%60,  (int)(progress*100), current_size,  total_size);
    gtk_label_set_text(GTK_LABEL(label->rlabel), message);
    g_free(message);

#if _HAS_GTK3
    gdouble red, green, blue, alpha;
#else 
    guint red, green, blue;
#endif
    red = ((progress) * dwb.color.download_end.red + (1-progress) * dwb.color.download_start.red);
    green = ((progress) * dwb.color.download_end.green + (1-progress) * dwb.color.download_start.green);
    blue = ((progress) * dwb.color.download_end.blue + (1-progress) * dwb.color.download_start.blue);
#if _HAS_GTK3 
    alpha = ((progress) * dwb.color.download_end.alpha + (1-progress) * dwb.color.download_start.alpha);

    if (blue != status->blue || red != status->red || green != status->green || alpha != status->alpha) {
#else 
    if (blue != status->blue || red != status->red || green != status->green) {
#endif
        DwbColor gradient = { 
          .red = red, 
          .green = green, 
          .blue = blue,
#if _HAS_GTK3 
          .alpha = alpha
#endif
        };
        DWB_WIDGET_OVERRIDE_BACKGROUND(label->event, GTK_STATE_NORMAL, &gradient);
    }
    status->blue  = blue;
    status->red   = red;
    status->green = green;
#if _HAS_GTK3 
    status->alpha = alpha;
#endif
  }
  status->time = time;
}/*}}}*/

/* download_set_mimetype(const char *) {{{*/
static void
download_set_mimetype(const char *command) {
  GList *list = NULL;
  if (dwb.state.mimetype_request) {
    Navigation *n = dwb_navigation_new(dwb.state.mimetype_request, command);
    if ( (list = g_list_find_custom(dwb.fc.mimetypes, n, (GCompareFunc)util_navigation_compare_first))) {
      g_free(list->data);
      dwb.fc.mimetypes = g_list_delete_link(dwb.fc.mimetypes, list);
    }
    dwb.fc.mimetypes = g_list_prepend(dwb.fc.mimetypes, n);
    util_file_add_navigation(dwb.files.mimetypes, n, true, -1);
  }
}/*}}}*/

static void 
download_do_spawn(const char *command, const char *path) {
  GError *error = NULL;
  char *argv[64];

  if (g_str_has_prefix(path, "file://")) 
    path += 7;

  char **argcom = g_strsplit(command, " ", -1);

  int argc=0;
  for (; argc<g_strv_length(argcom) && argc<62; argc++) {
    argv[argc] = argcom[argc];
  }
  argv[argc++] = (char *)path;
  argv[argc++] = NULL;

  if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error) ) {
    fprintf(stderr, "Couldn't open %s with %s : %s\n", path, command, error->message);
    g_clear_error(&error);
  }
  download_set_mimetype(command);
  g_strfreev(argcom);
}
/* download_spawn(DwbDownload *) {{{*/
static void 
download_spawn(DwbDownload *dl) {
  const char *filename = webkit_download_get_destination_uri(dl->download);
  download_do_spawn(dl->path, filename);
}/*}}}*/

gboolean
download_delay(DwbDownload *download) {
  gtk_widget_destroy(download->event);
  g_free(download->path);
  g_free(download);
  if (!downloads) {
    gtk_widget_hide(dwb.gui.downloadbar);
  }
  return false;
}

/* download_status_cb(WebKitDownload *) {{{*/
static void
download_status_cb(WebKitDownload *download, GParamSpec *p, DwbDownloadStatus *dstatus) {
  WebKitDownloadStatus status = webkit_download_get_status(download);

  if (status == WEBKIT_DOWNLOAD_STATUS_FINISHED || status == WEBKIT_DOWNLOAD_STATUS_CANCELLED || status == WEBKIT_DOWNLOAD_STATUS_ERROR) {
    GList *list = download_get_download_label(download);
    if (list) {
      DwbDownload *label = list->data;
      if (label->action == DL_ACTION_EXECUTE && status == WEBKIT_DOWNLOAD_STATUS_FINISHED) {
        download_spawn(label);
      }
      /* Setting time to 0 will force recomputing size */
      dstatus->time = 0;
      switch (status) {
        case WEBKIT_DOWNLOAD_STATUS_FINISHED: 
          download_progress_cb(download, NULL, dstatus);
          break;
        case WEBKIT_DOWNLOAD_STATUS_CANCELLED: 
          gtk_label_set_text(GTK_LABEL(label->rlabel), "cancelled");
          break;
        case WEBKIT_DOWNLOAD_STATUS_ERROR: 
          gtk_label_set_text(GTK_LABEL(label->rlabel), "failed");
          break;
        default: 
          break;
      }
      Navigation *n = dwb_navigation_new(webkit_download_get_uri(download), webkit_download_get_destination_uri(download));
      dwb.fc.downloads = g_list_append(dwb.fc.downloads, n);
      g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)download_delay, label);
      downloads = g_list_delete_link(downloads, list);
    }
    if (dwb.state.mimetype_request) {
      g_free(dwb.state.mimetype_request);
      dwb.state.mimetype_request = NULL;
    }
    g_free(dstatus);
    dwb.state.download_ref_count--;
  }
}/*}}}*/

/* download_button_press_cb(GtkWidget *w, GdkEventButton *e, GList *) {{{*/
static gboolean 
download_button_press_cb(GtkWidget *w, GdkEventButton *e, GList *gl) {
  if (e->button == 3) {
    DwbDownload *label = gl->data;
    webkit_download_cancel(label->download);
  }
  return false;
}/*}}}*/

/* download_add_progress_label (GList *gl, const char *filename) {{{*/
static DwbDownload *
download_add_progress_label(GList *gl, const char *filename) {
  DwbDownload *l = g_malloc(sizeof(DwbDownload));

  GtkWidget *hbox = gtk_hbox_new(false, 3);
  l->event = gtk_event_box_new();
  l->rlabel = gtk_label_new("???");
  l->llabel = gtk_label_new(filename);

  gtk_box_pack_start(GTK_BOX(hbox), l->llabel, false, false, 1);
  gtk_box_pack_start(GTK_BOX(hbox), l->rlabel, false, false, 1);
  gtk_container_add(GTK_CONTAINER(l->event), hbox);
  gtk_box_pack_start(GTK_BOX(dwb.gui.downloadbar), l->event, false, false, 2);

  gtk_label_set_use_markup(GTK_LABEL(l->llabel), true);
  gtk_label_set_use_markup(GTK_LABEL(l->rlabel), true);
  gtk_misc_set_alignment(GTK_MISC(l->llabel), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(l->rlabel), 1.0, 0.5);

  DWB_WIDGET_OVERRIDE_COLOR(l->llabel, GTK_STATE_NORMAL, &dwb.color.download_fg);
  DWB_WIDGET_OVERRIDE_COLOR(l->rlabel, GTK_STATE_NORMAL, &dwb.color.download_fg);

  DWB_WIDGET_OVERRIDE_BACKGROUND(l->event, GTK_STATE_NORMAL, &dwb.color.download_bg);

  DWB_WIDGET_OVERRIDE_FONT(l->llabel, dwb.font.fd_active);
  DWB_WIDGET_OVERRIDE_FONT(l->rlabel, dwb.font.fd_active);

  l->download = dwb.state.download;

  gtk_widget_show_all(dwb.gui.downloadbar);
  return l;
}/*}}}*/

/* download_start {{{*/
void 
download_start(const char *path) {
  if (path == NULL) 
    path = GET_TEXT();
  char *fullpath = NULL;
  const char *filename = webkit_download_get_suggested_filename(dwb.state.download);
  const char *uri = webkit_download_get_uri(dwb.state.download);
  char *command = NULL;
  char *tmppath = NULL;
  const char *last_slash;
  char path_buffer[PATH_MAX+1];
  gboolean external = GET_BOOL("download-use-external-program");
  gboolean clean = true;
  
  if (!filename || !strlen(filename)) {
    filename = "dwb_download";
  }

  /* Handle local files */
  if (g_str_has_prefix(uri, "file://") && g_file_test(uri+7, G_FILE_TEST_EXISTS)) {
    if (dwb.state.dl_action == DL_ACTION_EXECUTE) {
      download_do_spawn(path, uri+7);
    }
    else {
      GFile *source = g_file_new_for_uri(uri);
      fullpath = g_build_filename(path, filename, NULL);
      GFile *dest = g_file_new_for_path(fullpath);
      g_file_copy_async(source, dest, G_FILE_COPY_OVERWRITE, G_PRIORITY_DEFAULT, NULL, NULL, NULL, NULL, NULL);
      g_object_unref(source);
      g_object_unref(dest);
    }
  }
  /* Remote download; */
  else {
    if (dwb.state.dl_action == DL_ACTION_EXECUTE) {
      char *cache_name = g_build_filename(dwb.files.cachedir, filename, NULL);
      fullpath = g_strconcat("file://", cache_name, NULL);
      g_free(cache_name);
      lastaction = DL_ACTION_EXECUTE;
    }
    else {
      if (!path || *path == '\0') {
        path = g_get_current_dir();
      }
      if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        fullpath = g_build_filename(path, filename, NULL);
        if (!external) {
          tmppath = fullpath;
          fullpath = g_strconcat("file://", tmppath, NULL);
          g_free(tmppath);
        }
      }
      else {
        filename = strrchr(path, '/');
        if (filename != NULL) {
          filename++;
        }
        else {
          dwb_set_error_message(dwb.state.fview, "Invalid path: %s", path);
          clean = false;
          goto error_out;
        }
        fullpath = external ? g_strdup(path) : g_strconcat("file://", path, NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR) && (last_slash = strrchr(path, '/'))) {
          g_strlcpy(path_buffer, path, last_slash - path);
          path = path_buffer;
        }
      }
      lastaction = DL_ACTION_DOWNLOAD;
    }

    if (external && dwb.state.dl_action == DL_ACTION_DOWNLOAD) {
      command = download_get_command(uri, fullpath);
      if (!g_spawn_command_line_async(command, NULL)) {
        dwb_set_error_message(dwb.state.fview, "Cannot spawn download program");
      }
    }
    else {
      webkit_download_set_destination_uri(dwb.state.download, fullpath);
      DwbDownload *active = download_add_progress_label(dwb.state.fview, filename);
      active->action = dwb.state.dl_action;
      active->path = g_strdup(path);
      gtk_widget_show_all(dwb.gui.downloadbar);
      downloads = g_list_prepend(downloads, active);
      DwbDownloadStatus *s = dwb_malloc(sizeof(DwbDownloadStatus));
      s->blue = s->time = 0;
      g_signal_connect(active->event, "button-press-event", G_CALLBACK(download_button_press_cb), downloads);
      g_signal_connect(dwb.state.download, "notify::current-size", G_CALLBACK(download_progress_cb), s);
      g_signal_connect(dwb.state.download, "notify::status", G_CALLBACK(download_status_cb), s);
      webkit_download_start(dwb.state.download);
      dwb.state.download_ref_count++;
    }
    g_free(lastdir);
    if (dwb.state.dl_action != DL_ACTION_EXECUTE) {
      lastdir = g_strdup(path);
    }
  }

error_out:
  dwb_change_mode(NORMAL_MODE, clean);
  dwb.state.download = NULL;
  g_free(fullpath);
}/*}}}*/

/* download_entry_set_directory() {{{*/
static void
download_entry_set_directory() {
  dwb_set_normal_message(dwb.state.fview, false, "Downloadpath:");
  char *default_dir = GET_CHAR("download-directory");
  char *current_dir = NULL, *new_dir = NULL;
  if (default_dir != NULL) {
    entry_set_text(default_dir);
    return;
  }
  else if (lastdir != NULL) {
    entry_set_text(lastdir);
    return;
  }
  else 
    current_dir = g_get_current_dir();
    
  if (g_file_test(current_dir, G_FILE_TEST_IS_DIR) && current_dir[strlen(current_dir) - 1] != '/') {
    new_dir =  g_strdup_printf("%s/", current_dir);
    entry_set_text(new_dir);
    g_free(new_dir);
  }
  else 
    entry_set_text(current_dir);
  g_free(current_dir);
}/*}}}*/

/* download_entry_set_spawn_command{{{*/
void
download_entry_set_spawn_command(const char *command) {
  if (!command && dwb.state.mimetype_request) {
    command = download_get_command_from_mimetype(dwb.state.mimetype_request);
  }
  dwb_set_normal_message(dwb.state.fview, false, "Spawn (%s):", dwb.state.mimetype_request ? dwb.state.mimetype_request : "???");
  entry_set_text(command ? command : "");
}/*}}}*/

/* download_get_path {{{*/
void 
download_get_path(GList *gl, WebKitDownload *d) {
  const char *path, *command;
  const char *uri = webkit_download_get_uri(d) + 7;

  if (g_file_test(uri,  G_FILE_TEST_IS_EXECUTABLE)) {
    g_spawn_command_line_async(uri, NULL);
    return;
  }
  dwb.state.download = d;
  path = GET_CHAR("download-directory");
  if (path != NULL && g_file_test(path, G_FILE_TEST_IS_DIR) && GET_BOOL("download-no-confirm")) {
    download_start(path);
  }
  else {
    command = download_get_command_from_mimetype(dwb.state.mimetype_request);
    entry_focus();
    dwb.state.mode = DOWNLOAD_GET_PATH;
    dwb.state.download = d;
    if ( lastaction != DL_ACTION_DOWNLOAD && 
        ( command != NULL ||  g_file_test(uri, G_FILE_TEST_EXISTS)) ) {
      dwb.state.dl_action = DL_ACTION_EXECUTE;
      download_entry_set_spawn_command(command);
    }
    else {
      download_entry_set_directory();
    }
  }
}/*}}}*/

/* download_set_execute {{{*/
/* TODO complete path should not be in download.c */
void 
download_set_execute(Arg *arg) {
  if (dwb.state.mode == DOWNLOAD_GET_PATH) {
    if (dwb.state.dl_action == DL_ACTION_DOWNLOAD) {
      dwb.state.dl_action = DL_ACTION_EXECUTE;
      download_entry_set_spawn_command(NULL);
    }
    else {
      dwb.state.dl_action = DL_ACTION_DOWNLOAD;
      download_entry_set_directory();
    }
  }
}/*}}}*/
