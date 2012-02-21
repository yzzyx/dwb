#include <gtk/gtk.h>
#include "dwb.h"
#include "init.h"
#include "view.h"
#include "session.h"

static gboolean application_parse_option(const gchar *, const gchar *, gpointer , GError **);
static void application_execute_args(char **);
static GOptionContext * application_get_option_context(void);
static void application_start(GApplication *, char **);



static gboolean opt_list_sessions = false;
static gboolean opt_single = false;
static gboolean opt_override_restore = false;
static gboolean opt_version = false;
static gboolean opt_force = false;
static gchar *opt_restore = NULL;
static gchar **opt_exe = NULL;
static GOptionEntry options[] = {
  { "embed", 'e', 0, G_OPTION_ARG_INT64, &dwb.gui.wid, "Embed into window with window id wid", "wid"},
  { "force", 'f', 0, G_OPTION_ARG_NONE, &opt_force, "Force restoring a saved session, even if another process has restored the session", NULL },
  { "list-sessions", 'l', 0, G_OPTION_ARG_NONE, &opt_list_sessions, "List saved sessions and exit", NULL },
  { "new-instance", 'n', 0, G_OPTION_ARG_NONE, &opt_single, "Open a new instance, overrides 'single-instance'", NULL},
  { "restore", 'r', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, &application_parse_option, "Restore session with name 'sessionname' or default if name is omitted", "sessionname"},
  { "override-restore", 'R', 0, G_OPTION_ARG_NONE, &opt_override_restore, "Don't restore last session even if 'save-session' is set", NULL},
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &dwb.misc.profile, "Load configuration for 'profile'", "profile" },
  { "execute", 'x', 0, G_OPTION_ARG_STRING_ARRAY, &opt_exe, "Execute commands", NULL},
  { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, "Show version information and exit", NULL},
  { NULL }
};
static GOptionContext *option_context;

/* DwbApplication derived from GApplication {{{*/
#define DWB_TYPE_APPLICATION            (dwb_application_get_type ())
#define DWB_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), DWB_TYPE_APPLICATION, DwbApplication))
#define DWB_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), DWB_TYPE_APPLICATION, DwbApplicationClass))
#define DWB_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DWB_TYPE_APPLICATION))
#define DWB_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DWB_TYPE_APPLICATION))
#define DWB_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), DWB_TYPE_APPLICATION, DwbApplicationClass))

typedef struct _DwbApplication        DwbApplication;
typedef struct _DwbApplicationClass   DwbApplicationClass;
typedef struct _DwbApplicationPrivate DwbApplicationPrivate;

struct _DwbApplication
{
  GApplication parent;
};

struct _DwbApplicationClass
{
  GApplicationClass parent_class;
};
G_DEFINE_TYPE(DwbApplication, dwb_application, G_TYPE_APPLICATION);

static DwbApplication *_app;

static void
dwb_application_main(GApplication *app) {
  gtk_main();
}
static void
dwb_application_main_quit(GApplication *app) {
  gtk_main_quit();
}
static gint 
dwb_application_command_line(GApplication *app, GApplicationCommandLine *cl) {
  gint argc;
  gchar **argv = g_application_command_line_get_arguments(cl, &argc);
  opt_exe = NULL;

  GOptionContext *c = application_get_option_context();
  if (!g_option_context_parse(c, &argc, &argv, NULL)) {
    return 1;
  }
  application_execute_args(argv);
  return 0;
}
static gboolean
dwb_application_local_command_line(GApplication *app, gchar ***argv, gint *exit_status) {
  GError *error = NULL;
  *exit_status = 0;
  gint argc = g_strv_length(*argv);
  GOptionContext *c = application_get_option_context();
  if (!g_option_context_parse(c, &argc, argv, &error)) {
    fprintf(stderr, "Error parsing command line options: %s\n", error->message);
    *exit_status = 1;
    return true;
  }
  dwb_init_files();
  dwb_init_settings();
  if (opt_list_sessions) {
    session_list();
    return true;
  }
  if (opt_version) {
    dwb_version();
    return true;
  }
  if (opt_single || !GET_BOOL("single-instance")) {
    g_application_set_flags(app, G_APPLICATION_NON_UNIQUE);
  }
  if (!g_application_register(app, NULL, &error)) { 
    perror(error->message);
    *exit_status = 1;
    return true;
  }
  gboolean remote = g_application_get_is_remote(app);
  /* Remaining number of args */
  gint argc_remain = g_strv_length(*argv);
  if (remote) {
    if (opt_exe != NULL) {
      gchar **restore_args = g_malloc0_n(g_strv_length(opt_exe)*2 + argc_remain + 1, sizeof(char*));
      if (restore_args == NULL)
        return false;
      int count = 0;
      restore_args[count++] = g_strdup((*argv)[0]);
      gint i=0;
      for (; opt_exe[i] != NULL; i++) {
        restore_args[2*i + count] = g_strdup("-x");
        restore_args[2*i + 1 + count] = opt_exe[i];
      }
      for (; (*argv)[count]; count++) {
        restore_args[2*i+count] = g_strdup((*argv)[count]);
      }
      restore_args[2*i+count] = NULL;
      g_strfreev(*argv);
      *argv = restore_args;
    }
    else if (argc_remain == 1) {
      application_start(app, *argv);
      return true;
    }
    return false;
  }
  application_start(app, *argv);
  return true;
}

static void
dwb_application_class_init (DwbApplicationClass *class)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (class);

  app_class->run_mainloop = dwb_application_main;
  app_class->quit_mainloop = dwb_application_main_quit;
  app_class->local_command_line = dwb_application_local_command_line;
  app_class->command_line = dwb_application_command_line;
}
static void
dwb_application_init(DwbApplication *app) {
  (void)app;
}

static DwbApplication *
dwb_application_new (const gchar *id, GApplicationFlags flags)
{
  g_type_init ();
  return g_object_new (DWB_TYPE_APPLICATION, "application-id", id, "flags", flags, NULL);
}/*}}}*/

static void
application_execute_args(char **argv) {
  if (argv != NULL && argv[1] != NULL) {
    for (int i=1; argv[i] != NULL; i++) {
      view_add(argv[i], false);
    }
  }
  if (opt_exe != NULL) {
    for (int i=0; opt_exe[i] != NULL; i++) {
      dwb_parse_commands(opt_exe[i]);
    }
  }
}
static void 
application_start(GApplication *app, char **argv) {
  gboolean restored = false;
  if (argv == NULL)
    return;
  gtk_init(NULL, NULL);
  dwb_init();

  // restore session
  if (! opt_override_restore) {
    if (GET_BOOL("save-session") || opt_restore != NULL) {
      if (opt_restore == NULL)
        opt_restore = g_strdup("default");
      restored = session_restore(opt_restore, opt_force);
    }
  }
  if (! restored && g_strv_length(argv) == 1 ) {
    view_add(NULL, false);
    dwb_open_startpage(dwb.state.fview);
  }
  else {
    application_execute_args(argv);
  }
  /*  Compute bar height */
  gint w = 0, h = 0;
  PangoContext *pctx = gtk_widget_get_pango_context(VIEW(dwb.state.fview)->tablabel);
  PangoLayout *layout = pango_layout_new(pctx);
  pango_layout_set_text(layout, "a", -1);
  pango_layout_set_font_description(layout, dwb.font.fd_active);
  pango_layout_get_size(layout, &w, &h);
  dwb.misc.bar_height = h/PANGO_SCALE;

  dwb_pack(GET_CHAR("widget-packing"), false);

  gtk_widget_set_size_request(dwb.gui.entry, -1, dwb.misc.bar_height);
  g_object_unref(layout);

  dwb_init_signals();
  g_application_hold(app);
}
static GOptionContext * 
application_get_option_context() {
  if (option_context == NULL) {
    option_context = g_option_context_new("[url]");
    g_option_context_add_main_entries(option_context, options, NULL);
  }
  return option_context;
}

static gboolean 
application_parse_option(const gchar *key, const gchar *value, gpointer data, GError **error) {
  if (!g_strcmp0(key, "-r") || !g_strcmp0(key, "--restore")) {
    if (value != NULL) 
      opt_restore = g_strdup(value);
    else 
      opt_restore = g_strdup("default");
    return true;
  }
  else {
    g_set_error(error, 0, 1, "Invalid option : %s", key);
  }
  return false;
}

void 
application_stop(void) {
  g_application_release(G_APPLICATION(_app));
}

gint 
application_run(gint argc, gchar **argv) {
  _app = dwb_application_new("org.bitbucket.dwb", 0);
  gint ret = g_application_run(G_APPLICATION(_app), argc, argv);
  return ret;
}
