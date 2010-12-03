#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "dwb.h"
#include "util.h"
#include "completion.h"

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
    g_free(command);
    command = newcommand;
  }
  if ( (newcommand = dwb_util_string_replace(command, "dwb_cookies", dwb.files.cookies)) ) {
    g_free(command);
    command = newcommand;
  }
  if ( (newcommand = dwb_util_string_replace(command, "dwb_output", output)) ) {
    g_free(command);
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
  g_free(message);

  guint blue = ((1 - progress) * 0xaa);
  guint green = progress * 0xaa;
  char *colorstring = g_strdup_printf("#%02x%02x%02x", 0x00, green, blue);

  GdkColor color; 
  gdk_color_parse(colorstring, &color);
  gtk_widget_modify_bg(label->event, GTK_STATE_NORMAL, &color);
  g_free(colorstring);
}/*}}}*/

/* dwb_dl_set_mimetype(const char *) {{{*/
static void
dwb_dl_set_mimetype(const char *command) {
  if (dwb.state.mimetype_request) {
    for (GList *l = dwb.fc.mimetypes; l; l=l->next) {
      Navigation *n = l->data;
      if (!strcmp(dwb.state.mimetype_request, n->first)) {
        g_free(n->second);
        n->second = g_strdup(command);
        return;
      }
    }
    Navigation *n = dwb_navigation_new(dwb.state.mimetype_request, command);
    dwb.fc.mimetypes = g_list_prepend(dwb.fc.mimetypes, n);
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
  g_free(command);
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
      if (label->action == Execute && status == WEBKIT_DOWNLOAD_STATUS_FINISHED) {
        dwb_dl_spawn(label);
      }
      gtk_widget_destroy(label->event);
      g_free(label->path);
      downloads = g_list_delete_link(downloads, list);
    }
    if (!downloads) {
      gtk_widget_hide(dwb.gui.downloadbar);
    }
    g_free(dwb.state.mimetype_request);
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
  char *command = NULL;
  gboolean external = GET_BOOL("download-use-external-program");
  
  if (path[0] != '/') {
    dwb_set_error_message(dwb.state.fview, "A full path must be specified");
    dwb_normal_mode(false);
    return;
  }

  if (!filename || !strlen(filename)) {
    filename = "dwb_download";
  }

  if (dwb.state.dl_action == Execute) {
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

  if (external && dwb.state.dl_action == Download) {
    const char *uri = webkit_download_get_uri(dwb.state.download);
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
  if (lastdir) {
    g_free(lastdir);
  }
  if (dwb.state.dl_action != Execute) {
    lastdir = g_strdup(path);
  }

  dwb_normal_mode(true);
  dwb.state.download = NULL;
  g_free(fullpath);
}/*}}}*/

/* dwb_dl_entry_set_directory() {{{*/
static void
dwb_dl_entry_set_directory() {
  dwb_set_normal_message(dwb.state.fview, false, "Downloadpath:");
  char *current_dir = lastdir ? g_strdup(lastdir) : g_get_current_dir();
  char *newdir = current_dir[strlen(current_dir) - 1] != '/' ? g_strdup_printf("%s/", current_dir) : g_strdup(current_dir);

  dwb_entry_set_text(newdir);
  if (current_dir) 
    g_free(current_dir);
  if (newdir) 
    g_free(newdir);
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
  char *command;
  dwb_focus_entry();
  dwb.state.mode = DownloadGetPath;
  dwb.state.download = d;

  if ( dwb.state.mimetype_request && (command = dwb_get_command_from_mimetype(dwb.state.mimetype_request)) ) {
    dwb.state.dl_action = Execute;
    dwb_dl_entry_set_spawn_command(command);
  }
  else {
    dwb_dl_entry_set_directory();
  }
}/*}}}*/

/* dwb_dl_set_execute {{{*/
void 
dwb_dl_set_execute(Arg *arg) {
  if (dwb.state.mode == DownloadGetPath) {
    if (dwb.state.dl_action == Download) {
      dwb.state.dl_action = Execute;
      dwb_dl_entry_set_spawn_command(NULL);
    }
    else {
      dwb.state.dl_action = Download;
      dwb_dl_entry_set_directory();
    }
  }
}/*}}}*/
