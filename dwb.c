#define _POSIX_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h>

#define NAME "dwb2";

#define SETTINGS_VIEW "<style type=\"text/css\"> \
  body { background-color: #303030; color: #dddddd; font: monospace; font-size:14; font-weight: bold; }\
  #line { border: 1px dotted black; vertical-align: middle; }\
  .text { float: left; font-variant: small-caps; font-size: 14;}\
  .key { text-align: right;  font-size: 12; }\
  .active { background-color: #660000; }\
  h2 { font-variant: small-caps; }\
  </style><body>"
#define HTML_H2  "<h2 align=\"center\">%s</h2>"

#define DIV_START "<div id=\"line\">"
#define DIV_KEYS_TEXT "<div class=\"text\">%s</div>"
#define DIV_KEYS_KEY "<div class=\"key\"><input id=\"_dwb_setting_%s\" value=\"%s %s\"/></div>"
#define DIV_END "</div>"

/* PRE {{{*/

/* MAKROS {{{*/ 
#define LENGTH(X)   (sizeof(X)/sizeof(X[0]))
#define GLENGTH(X)  (sizeof(X)/g_array_get_element_size(X)) 
#define NN(X)       ( ((X) == 0) ? 1 : (X) )
#define NUMMOD      (dwb.state.nummod == 0 ? 1 : dwb.state.nummod)

#define CLEAN_STATE(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_BUTTON1_MASK) & ~(GDK_BUTTON2_MASK) & ~(GDK_BUTTON3_MASK) & ~(GDK_BUTTON4_MASK) & ~(GDK_BUTTON5_MASK))

#define GET_TEXT()                  gtk_entry_get_text(GTK_ENTRY(dwb.gui.entry));
#define CURRENT_VIEW()              ((View*)dwb.state.fview->data)
#define VIEW(X)                     ((View*)X->data)
#define WEBVIEW(X)                  (WEBKIT_WEB_VIEW(((View*)X->data)->web))
#define CURRENT_WEBVIEW()           (WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web))
#define VIEW_FROM_ARG(X)            (X && X->p ? ((GList*)X->p)->data : dwb.state.fview->data)
#define WEBVIEW_FROM_ARG(arg)       (WEBKIT_WEB_VIEW(((View*)(arg && arg->p ? ((GList*)arg->p)->data : dwb.state.fview->data))->web))
#define CLEAR_COMMAND_TEXT(X)       dwb_set_status_bar_text(((View *)X->data)->lstatus, NULL, NULL, NULL)

#define DIGIT(X)   (X->keyval >= GDK_0 && X->keyval <= GDK_9)
#define ALPHA(X)    ((X->keyval >= GDK_A && X->keyval <= GDK_Z) ||  (X->keyval >= GDK_a && X->keyval <= GDK_z) || X->keyval == GDK_space)
#define True (void*) true
#define False (void*) false
/*}}}*/

/* TYPES {{{*/
typedef enum _Mode Mode;
typedef enum _Open Open;
typedef enum _Layout Layout;
typedef enum _Direction Direction;

typedef struct _Arg Arg;
typedef struct _Misc Misc;
typedef struct _Dwb Dwb;
typedef struct _Gui Gui;
typedef struct _State State;
typedef struct _View View;
typedef struct _Color Color;
typedef struct _Font Font;
typedef struct _FunctionMap FunctionMap;
typedef struct _KeyMap KeyMap;
typedef struct _Key Key;
typedef struct _KeyValue KeyValue;
typedef struct _ViewStatus ViewStatus;
typedef struct _Files Files;
typedef struct _FileContent FileContent;
typedef struct _Navigation Navigation;
typedef struct _Quickmark Quickmark;
typedef struct _WebSettings WebSettings;
typedef struct _Settings Settings;
typedef union _Type Type;
/*}}}*/

/* DECLARATIONS {{{*/
gchar * dwb_modmask_to_string(guint modmask);

gboolean dwb_update_hints(GdkEventKey *);
gchar * dwb_get_directory_content(const gchar *);
Navigation * dwb_navigation_new_from_line(const gchar *);
Navigation * dwb_navigation_new(const gchar *, const gchar *);
void dwb_navigation_free(Navigation *);
void dwb_quickmark_free(Quickmark *);
Quickmark * dwb_quickmark_new(const gchar *, const gchar *, const gchar *);
Quickmark * dwb_quickmark_new_from_line(const gchar *);
void dwb_web_view_add_history_item(GList *gl);

gboolean dwb_eval_key(GdkEventKey *);

void dwb_resize(gdouble);
void dwb_normal_mode(gboolean);

gboolean dwb_append_navigation(GList *, GList **);
void dwb_set_error_message(GList *, const gchar *);
void dwb_set_normal_message(GList *, const gchar *, gboolean);
void dwb_set_status_bar_text(GtkWidget *, const gchar *, GdkColor *,  PangoFontDescription *);
void dwb_set_status_text(GList *, const gchar *, GdkColor *,  PangoFontDescription *);
void dwb_tab_label_set_text(GList *, const gchar *);
void dwb_update_status(GList *gl);
void dwb_update_status_text(GList *gl);

void dwb_save_quickmark(const gchar *);
void dwb_open_quickmark(const gchar *);
void dwb_get_key_setting(GdkEventKey *e);
Key dwb_strv_to_key(gchar **string, gsize length);

GSList * dwb_keymap_replace(GSList *, KeyValue );
GSList * dwb_create_key_map(GSList *gl, KeyValue key);

void dwb_view_modify_style(GList *, GdkColor *, GdkColor *, GdkColor *, GdkColor *, PangoFontDescription *, gint);
GList * dwb_create_web_view(GList *);
void dwb_add_view(Arg *);
void dwb_remove_view(Arg *);
void dwb_source_remove(GList *);

gboolean dwb_web_view_close_web_view_cb(WebKitWebView *, GList *);
gboolean dwb_web_view_console_message_cb(WebKitWebView *, gchar *, gint , gchar *, GList *);
GtkWidget * dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *);
gboolean dwb_web_view_download_requested_cb(WebKitWebView *, WebKitDownload *, GList *);
void dwb_web_view_hovering_over_link_cb(WebKitWebView *, gchar *, gchar *, GList *);
gboolean dwb_web_view_mime_type_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, gchar *, WebKitWebPolicyDecision *, GList *);
gboolean dwb_web_view_navigation_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, WebKitWebNavigationAction *, WebKitWebPolicyDecision *, GList *);
gboolean dwb_web_view_new_window_policy_cb(WebKitWebView *, WebKitWebFrame *, WebKitNetworkRequest *, WebKitWebNavigationAction *, WebKitWebPolicyDecision *, GList *);
gboolean dwb_web_view_script_alert_cb(WebKitWebView *, WebKitWebFrame *, gchar *, GList *);
void dwb_web_view_window_object_cleared_cb(WebKitWebView *, WebKitWebFrame *, JSGlobalContextRef *, JSObjectRef *, GList *);
void dwb_web_view_load_status_cb(WebKitWebView *, GParamSpec *, GList *);
void dwb_web_view_resource_request_cb(WebKitWebView *, WebKitWebFrame *, WebKitWebResource *, WebKitNetworkRequest *, WebKitNetworkResponse *, GList *);


gboolean dwb_web_view_button_press_cb(WebKitWebView *, GdkEventButton *, GList *);
gboolean dwb_entry_keypress_cb(GtkWidget *, GdkEventKey *e);
gboolean dwb_entry_release_cb(GtkWidget *, GdkEventKey *e);
gboolean dwb_entry_activate_cb(GtkEntry *);

void dwb_update_layout(void);
void dwb_update_tab_label(void);
void dwb_focus(GList *);
void dwb_load_uri(Arg *);
void dwb_grab_focus(GList *);
gchar * dwb_get_resolved_uri(const gchar *);
gchar * dwb_execute_script(const gchar *);

gboolean dwb_show_hints(Arg *);
gboolean dwb_show_keys(Arg *);
gboolean dwb_quickmark(Arg *);
gboolean dwb_bookmark(Arg *);
gboolean dwb_open(Arg *);
gboolean dwb_focus_next(Arg *);
gboolean dwb_focus_prev(Arg *);
gboolean dwb_push_master(Arg *);
gboolean dwb_set_orientation(Arg *arg);
void dwb_toggle_maximized(Arg *);
gboolean dwb_zoom_in(Arg *);
gboolean dwb_zoom_out(Arg *);
void dwb_set_zoom_level(Arg *);
gboolean dwb_scroll(Arg *);
gboolean dwb_history_back(Arg *);
gboolean dwb_history_forward(Arg *);
gboolean dwb_insert_mode(Arg *);

void dwb_init_key_map(void);
void dwb_init_style(void);
void dwb_init_gui(void);

void dwb_clean_vars(void);
void dwb_exit(void);
/*}}}*/

/* ENUMS {{{*/
enum _Mode {
  NormalMode,
  InsertMode,
  OpenMode,
  QuickmarkSave,
  QuickmarkOpen, 
  HintMode,
  KeySettings,
};

enum _Open {
  OpenNormal, 
  OpenNewView,
  OpenNewWindow,
};

enum _Layout {
  NormalLayout = 0,
  BottomStack = 1<<0, 
  Maximized = 1<<2, 
};

enum _Direction {
  Up, 
  Down, 
  Left, 
  Right, 
  PageUp,
  PageDown, 
  Top,
  Bottom,
};
/*}}}*/

/* STRUCTS {{{*/
struct _Arg {
  guint n;
  gint i;
  gdouble d;
  gpointer p;
  gboolean b;
  gchar *e;
};
struct _Key {
  const gchar *str;
  guint mod;
};
struct _KeyValue {
  const gchar *desc;
  Key key;
};
struct _FunctionMap {
  const gchar *desc;
  gboolean (*func)(void*);
  const gchar *text;
  const gchar *error; 
  gboolean hide;
  Arg arg;
};
struct _KeyMap {
  const gchar *key;
  guint mod;
  FunctionMap *map;
};
struct _Navigation {
  gchar *uri;
  gchar *title;
};
struct _Quickmark {
  gchar *key; 
  Navigation *nav;
};

struct _State {
  GList *views;
  GList *fview;
  Mode mode;
  GString *buffer;
  gint nummod;
  Open nv;
  gchar *key_id;
};

union _Type {
  gboolean b;
  gdouble f;
  guint i; 
  gpointer p;
};
struct _WebSettings {
  gchar *prop;
  Type type;
};

struct _View {
  GtkWidget *vbox;
  GtkWidget *web;
  GtkWidget *tabevent;
  GtkWidget *tablabel;
  GtkWidget *statusbox;
  GtkWidget *rstatus;
  GtkWidget *lstatus;
  GtkWidget *scroll; 
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  View *next;
  ViewStatus *status;
};
struct _ViewStatus {
  guint message_id;
  gchar *hover_uri;
  gboolean add_history;
};
struct _Color {
  GdkColor active_fg;
  GdkColor active_bg;
  GdkColor normal_fg;
  GdkColor normal_bg;
  GdkColor tab_active_fg;
  GdkColor tab_active_bg;
  GdkColor tab_normal_fg;
  GdkColor tab_normal_bg;
  GdkColor entry_fg;
  GdkColor entry_bg;
  GdkColor error;
};
struct _Font {
  PangoFontDescription *fd_normal;
  PangoFontDescription *fd_bold;
  PangoFontDescription *fd_oblique;
  gint active_size;
  gint normal_size;
};

struct _Gui {
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *topbox;
  GtkWidget *paned;
  GtkWidget *right;
  GtkWidget *left;
  GtkWidget *entry;
  gint width;
  gint height;
};
struct _Misc {
  const gchar *name;
  const gchar *scripts;
};
struct _Files {
  const gchar *bookmarks;
  const gchar *history;
  const gchar *mimetype;
  const gchar *quickmarks;
  const gchar *session;
  const gchar *searchengines;
  const gchar *stylesheet;
  const gchar *keys;
  const gchar *scriptdir;
};
struct _FileContent {
  GList *bookmarks;
  GList *history;
  GList *mimetype;
  GList *quickmarks;
  GList *searchengines;
  GList *keys;
};

struct _Dwb {
  Mode mode;
  Gui gui;
  Color color;
  Font font;
  Misc misc;
  State state;
  GSList *keymap;
  Files files;
  FileContent fc;
};

/*}}}*/

/* VARIABLES {{{*/
GdkNativeWindow embed = 0;
Dwb dwb;
gint signals[] = { SIGFPE, SIGILL, SIGINT, SIGQUIT, SIGTERM, SIGALRM, SIGSEGV};
/*}}}*/

#include "config.h"

#define NO_URL                      "No URL in current context"
#define NO_HINTS                    "No Hints in current context"
/* FUNCTION_MAP{{{*/
FunctionMap FMAP [] = {
  { "add_view",              (void*)dwb_add_view,           "Add a new view",                 NULL,                       true,                                             },
  { "bookmark",              (void*)dwb_bookmark,           "Bookmark current page",          NO_URL,                     true,                                             },
  { "focus_next",            (void*)dwb_focus_next,         "Focus next view",                "No other view",            true,                                             },
  { "focus_prev",            (void*)dwb_focus_prev,         "Focus prevous view",             "No other view",            true,                                             }, 
  { "history_back",          (void*)dwb_history_back,       "Go Back",                        "Beginning of History",     true,                                             },
  { "history_forward",       (void*)dwb_history_forward,    "Go Forward",                     "End of History",           true,                                             },
  { "insert_mode",           (void*)dwb_insert_mode,        "Insert mode",                    NULL,                       false,                                            },
  { "open",                  (void*)dwb_open,               "Open URL",                       NULL,                       false,   { .n = OpenNormal,      .p = NULL }      },
  { "open_new_view",         (void*)dwb_open,               "Open URL in a new view",         NULL,                       false,   { .n = OpenNewView,     .p = NULL }      },
  { "open_quickmark",        (void*)dwb_quickmark,          "Open quickmark",                 NO_URL,                     false,   { .n = QuickmarkOpen, .i=OpenNormal },   },
  { "open_quickmark_nv",     (void*)dwb_quickmark,          "Open quickmark in a new view",   NULL,                       false,    { .n = QuickmarkOpen, .i=OpenNewView },  },
  { "push_master",           (void*)dwb_push_master,        "Push to master area",            "No other view",            true,                                             },
  { "remove_view",           (void*)dwb_remove_view,        "Close view",                     NULL,                       true,                                             },
  { "save_quickmark",        (void*)dwb_quickmark,          "Save a quickmark for this page", NO_URL,                     false,    { .n = QuickmarkSave },                  },
  { "scroll_bottom",         (void*)dwb_scroll,             "Scroll to  bottom of the page",  NULL,                       true,    { .n = Bottom },                         },
  { "scroll_down",           (void*)dwb_scroll,             "Scroll down",                    "Bottom of the page",       true,    { .n = Down, },                          },
  { "scroll_left",           (void*)dwb_scroll,             "Scroll left",                    "Left side of the  page",   true,    { .n = Left },                           },
  { "scroll_page_down",      (void*)dwb_scroll,             "Scroll one page down",           "Bottom of the page",       true,    { .n = PageDown, },                      },
  { "scroll_page_up",        (void*)dwb_scroll,             "Scroll one page up",             "Top of the page",          true,    { .n = PageUp, },                        },
  { "scroll_right",          (void*)dwb_scroll,             "Scroll left",                    "Right side of the page",   true,    { .n = Right },                          },
  { "scroll_top",            (void*)dwb_scroll,             "Scroll to the top of the page",   NULL,                       true,    { .n = Top },                            },
  { "scroll_up",             (void*)dwb_scroll,             "Scroll up",                      "Top of the page",          true,    { .n = Up, },                            },
  { "show_keys",             (void*)dwb_show_keys,          "Key configuration",              NULL,                       true,                                             },
  { "toggle_bottomstack",    (void*)dwb_set_orientation,    "Toggle bottomstack",             NULL,                       true,                                             }, 
  { "toggle_maximized",      (void*)dwb_toggle_maximized,   "Toggle maximized",               NULL,                       true,                                             },  
  { "zoom_in",               (void*)dwb_zoom_in,            "Zoom in",                        "Cannot zoom in further",   true,                                             },
  { "zoom_normal",           (void*)dwb_set_zoom_level,     "Zoom 100%",                      NULL,                       true,    { .d = 1.0,   .p = NULL }                },
  { "zoom_out",              (void*)dwb_zoom_out,           "Zoom out",                       "Cannot zoom out further",  true,                                             },
  { "hint_mode",             (void*)dwb_show_hints,           "Follow hints",                 NO_HINTS,                   false,    { .n = OpenNormal },                    },

};/*}}}*/

/*}}}*/

/* UTIL {{{*/

/* dwb_keymap_sort_text {{{*/
gint
dwb_keymap_sort_text(KeyMap *a, KeyMap *b) {
  return strcmp(a->map->text, b->map->text);
}/*}}}*/

/*dwb_get_directory_content(const gchar *filename) {{{*/
gchar *
dwb_get_directory_content(const gchar *dirname) {
  GDir *dir;
  GString *buffer = g_string_new(NULL);
  gchar *content;
  GError *error = NULL;
  gchar *filename, *filepath;
  gchar *ret;

  if ( (dir = g_dir_open(dirname, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      if (filename[0] != '.') {
        filepath = g_strconcat(dirname, "/",  filename, NULL);
        if (g_file_get_contents(filepath, &content, NULL, &error)) {
          g_string_append(buffer, content);
        }
        else {
          fprintf(stderr, "Cannot read %s: %s", filename, error->message);
        }
        g_free(filepath);
        g_free(content);
      }
    }
    g_dir_close (dir);
  }
  ret = g_strdup(buffer->str);
  g_string_free(buffer, true);
  return ret;

}/*}}}*/

/* dwb_get_file_content(const gchar *filename){{{*/
gchar *
dwb_get_file_content(const gchar *filename) {
  GError *error = NULL;
  gchar *content = NULL;
  if (!(g_file_test(filename, G_FILE_TEST_IS_REGULAR) &&  g_file_get_contents(filename, &content, NULL, &error))) {
    fprintf(stderr, "Cannot open %s: %s\n", filename, error ? error->message : "file not found");
  }
  return content;
}/*}}}*/

/* dwb_set_file_content(const gchar *filename, const gchar *content){{{*/
void
dwb_set_file_content(const gchar *filename, const gchar *content) {
  GError *error = NULL;
  if (!g_file_set_contents(filename, content, -1, &error)) {
    fprintf(stderr, "Cannot save %s : %s", filename, error->message);
  }
}/*}}}*/

/* dwb_navigation_new(const gchar *uri, const gchar *title) {{{*/
Navigation *
dwb_navigation_new(const gchar *uri, const gchar *title) {
  Navigation *nv = malloc(sizeof(Navigation)); 
  nv->uri = uri ? g_strdup(uri) : NULL;
  nv->title = title ? g_strdup(title) : NULL;
  return nv;

}/*}}}*/

/* dwb_navigation_new_from_line(const gchar *text){{{*/
Navigation * 
dwb_navigation_new_from_line(const gchar *text) {
  gchar **line;
  Navigation *nv = NULL;
  
  if (text) {
    line = g_strsplit(text, " ", 2);
    nv = dwb_navigation_new(line[0], line[1]);
    g_free(line);
  }
  return nv;
}/*}}}*/

/* dwb_navigation_free(Navigation *n){{{*/
void
dwb_navigation_free(Navigation *n) {
  if (n->uri)   
    g_free(n->uri);
  if (n->title)  
    g_free(n->title);
  g_free(n);
}/*}}}*/

/* dwb_quickmark_new_from_line(const gchar *line) {{{*/
Quickmark * 
dwb_quickmark_new_from_line(const gchar *line) {
  Quickmark *q = NULL;
  gchar **token;
  if (line) {
    token = g_strsplit(line, " ", 3);
    q = dwb_quickmark_new(token[1], token[2], token[0]);
    g_strfreev(token);
  }
  return q;

}/*}}}*/

/* dwb_quickmark_new(const gchar *uri, const gchar *title,  const gchar *key)  {{{*/
Quickmark *
dwb_quickmark_new(const gchar *uri, const gchar *title, const gchar *key) {
  Quickmark *q = malloc(sizeof(Quickmark));
  q->key = key ? g_strdup(key) : NULL;
  q->nav = dwb_navigation_new(uri, title);
  return q;
}/* }}} */

/* dwb_quickmark_free(Quickmark *q) {{{*/
void
dwb_quickmark_free(Quickmark *q) {
  if (q->key) 
    g_free(q->key);
  if (q->nav)
    dwb_navigation_free(q->nav);
  g_free(q);

}/*}}}*/

/* }}} */

/* CALLBACKS {{{*/

/* WEB_VIEW_CALL_BACKS {{{*/

/* dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *button, GList *gl) {{{*/
gboolean
dwb_web_view_button_press_cb(WebKitWebView *web, GdkEventButton *e, GList *gl) {
  Arg arg = { .p = gl };
  if (e->button == 1) {
    if (e->type == GDK_BUTTON_PRESS) {
      dwb_focus(gl);
    }
    if (e->type == GDK_2BUTTON_PRESS) {
      dwb_push_master(&arg);
    }
  }
  return false;
}/*}}}*/

/* dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {{{*/
gboolean 
dwb_web_view_close_web_view_cb(WebKitWebView *web, GList *gl) {
  Arg a = { .p = gl };
  dwb_remove_view(&a);
  return true;
}/*}}}*/

/* dwb_web_view_console_message_cb(WebKitWebView *web, gchar *message, gint line, gchar *sourceid, GList *gl) {{{*/
gboolean 
dwb_web_view_console_message_cb(WebKitWebView *web, gchar *message, gint line, gchar *sourceid, GList *gl) {
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitWebFrame *, GList *) {{{*/
GtkWidget * 
dwb_web_view_create_web_view_cb(WebKitWebView *web, WebKitWebFrame *frame, GList *gl) {
  // TODO implement
  return NULL;
}/*}}}*/

/* dwb_web_view_create_web_view_cb(WebKitWebView *, WebKitDownload *, GList *) {{{*/
gboolean 
dwb_web_view_download_requested_cb(WebKitWebView *web, WebKitDownload *download, GList *gl) {
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_hovering_over_link_cb(WebKitWebView *, gchar *title, gchar *uri, GList *) {{{*/
void 
dwb_web_view_hovering_over_link_cb(WebKitWebView *web, gchar *title, gchar *uri, GList *gl) {
  // TODO implement
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
  // TODO implement
  return false;
}/*}}}*/

/* dwb_web_view_navigation_policy_cb {{{*/
gboolean 
dwb_web_view_navigation_policy_cb(WebKitWebView *web, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
    WebKitWebPolicyDecision *policy, GList *gl) {
  // TODO implement
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
  JSEvaluateScript((JSContextRef)context, script, JSContextGetGlobalObject((JSContextRef)context), 
      NULL, 0, &exc);
  JSStringRelease(script);
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
      dwb_append_navigation(gl, &dwb.fc.history);
      if (dwb.state.mode == KeySettings) {
        Arg a = { 0 };
        dwb_show_hints(&a);
      }
      else {
        dwb_normal_mode(true);
      }
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

/*}}}*/

/* dwb_entry_keypress_cb {{{*/
gboolean 
dwb_entry_release_cb(GtkWidget* entry, GdkEventKey *e) { 
  if (dwb.state.mode == HintMode) {
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
dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {
  // TODO implement
  if (dwb.state.mode == HintMode ) {
    if (DIGIT(e) || e->keyval == GDK_Tab) {
      return dwb_update_hints(e);
    }
  }
  return false;
}/*}}}*/

/* dwb_entry_activate_cb (GtkWidget *entry) {{{*/
gboolean 
dwb_entry_activate_cb(GtkEntry* entry) {
  gchar *text = (gchar*)gtk_entry_get_text(entry);
  Arg a = { .n = 0, .p = text };

  dwb_load_uri(&a);
  dwb_normal_mode(true);
  return false;
}/*}}}*/

/* dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {{{*/
gboolean 
dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  gboolean ret = false;

  gchar *key = gdk_keyval_name(e->keyval);
  if (e->keyval == GDK_Escape) {
    dwb_normal_mode(true);
    return false;
  }
  else if (dwb.state.mode == InsertMode) {
    return false;
  }
  else if (dwb.state.mode == QuickmarkSave) {
    dwb_save_quickmark(key);
    return true;
  }
  else if (dwb.state.mode == QuickmarkOpen) {
    dwb_open_quickmark(key);
    return true;
  }
  else if (dwb.state.mode == KeySettings) {
    dwb_get_key_setting(e);
    return false;
  }
  else if (gtk_widget_has_focus(dwb.gui.entry)) {
    return false;
  }
  ret = dwb_eval_key(e);
  
  return ret;
}/*}}}*/
/*}}}*/

/* COMMANDS {{{*/

/* dwb_simple_command(keyMap *km) {{{*/
void 
dwb_simple_command(KeyMap *km) {
  gboolean (*func)(void *) = km->map->func;
  Arg *arg = &km->map->arg;
  arg->e = NULL;

  if (func(arg)) {
    dwb_set_normal_message(dwb.state.fview, km->map->text, km->map->hide);
  }
  else {
    dwb_set_error_message(dwb.state.fview, arg->e ? arg->e : km->map->error);
  }
  dwb.state.nummod = 0;
  g_string_free(dwb.state.buffer, true);
  dwb.state.buffer = NULL;
}/*}}}*/

/* dwb_show_hints {{{*/
gboolean
dwb_show_hints(Arg *arg) {
  dwb.state.nv = arg->n;
  if (dwb.state.mode != HintMode) {
    gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
    dwb_execute_script("show_hints()");
    dwb.state.mode = HintMode;
    gtk_widget_grab_focus(dwb.gui.entry);
  }
  return true;

}/*}}}*/

/* dwb_show_keys(Arg *arg){{{*/
gboolean 
dwb_show_keys(Arg *arg) {
  dwb.state.mode = KeySettings;
  View *v = dwb.state.fview->data;
  GString *buffer = g_string_new(SETTINGS_VIEW);
  g_string_append_printf(buffer, HTML_H2, "Keyboard Configuration");

  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    g_string_append(buffer, DIV_START);
    g_string_append_printf(buffer, DIV_KEYS_TEXT, m->map->text);
    g_string_append_printf(buffer, DIV_KEYS_KEY, m->map->desc, dwb_modmask_to_string(m->mod), m->key);
    g_string_append(buffer, DIV_END);
  }
  dwb_web_view_add_history_item(dwb.state.fview);

  webkit_web_view_load_string(WEBKIT_WEB_VIEW(v->web), buffer->str, "text/html", NULL, "settings");
  g_string_free(buffer, true);
  return true;
}/*}}}*/

/* dwb_insert_mode(Arg *arg) {{{*/
gboolean
dwb_insert_mode(Arg *arg) {
  dwb.state.mode = InsertMode;
  return true;
}/*}}}*/

/* dwb_bookmark {{{*/
gboolean 
dwb_bookmark(Arg *arg) {
  GList *gl = arg && arg->p ? arg->p : dwb.state.fview;

  return dwb_append_navigation(gl, &dwb.fc.bookmarks);
}/*}}}*/

/* dwb_quickmark(Arg *arg) {{{*/
gboolean
dwb_quickmark(Arg *arg) {
  dwb.state.nv = arg->i;
  dwb.state.mode = arg->n;
  return true;
}/*}}}*/

/* dwb_zoom_in(void *arg) {{{*/
gboolean
dwb_zoom_in(Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  gint limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;
  gboolean ret;

  for (gint i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) > 4.0)) {
      ret = false;
      break;
    }
    webkit_web_view_zoom_in(web);
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_zoom_out(void *arg) {{{*/
gboolean
dwb_zoom_out(Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  gint limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;
  gboolean ret;

  for (int i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) < 0.25)) {
      ret = false;
      break;
    }
    webkit_web_view_zoom_out(web);
    ret = true;
  }
  return ret;
}/*}}}*/

/* dwb_scroll {{{*/
gboolean 
dwb_scroll(Arg *arg) {
  gboolean ret = true;
  gdouble scroll;

  GList *gl = arg && arg->p ? arg->p : dwb.state.fview;
  View *v = gl->data;

  GtkAdjustment *a = arg->n == Left || arg->n == Right ? v->hadj : v->vadj;
  gint sign = arg->n == Up || arg->n == PageUp || arg->n == Left ? -1 : 1;

  gdouble value = gtk_adjustment_get_value(a);

  gdouble inc = arg->n == PageUp || arg->n == PageDown 
    ? gtk_adjustment_get_page_increment(a) 
    : gtk_adjustment_get_step_increment(a);

  gdouble lower  = gtk_adjustment_get_lower(a);
  gdouble upper  = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
  switch (arg->n) {
    case  Top:      scroll = lower; break;
    case  Bottom:   scroll = upper; break;
    default:        scroll = value + sign * inc * NN(dwb.state.nummod); break;
  }

  scroll = scroll < lower ? lower : scroll > upper ? upper : scroll;
  if (scroll == value) 
    ret = false;
  else {
    gtk_adjustment_set_value(a, scroll);
    dwb_update_status_text(gl);
  }
  return ret;
}/*}}}*/

/* dwb_set_zoom_level(Arg *arg) {{{*/
void 
dwb_set_zoom_level(Arg *arg) {
  GList *gl = arg->p ? arg->p : dwb.state.fview;
  webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(((View*)gl->data)->web), arg->d);

}/*}}}*/

/* dwb_set_orientation(Arg *arg) {{{*/
gboolean 
dwb_set_orientation(Arg *arg) {
  LAYOUT ^= BottomStack;
  gtk_orientable_set_orientation(GTK_ORIENTABLE(dwb.gui.paned), LAYOUT & BottomStack);
  gtk_orientable_set_orientation(GTK_ORIENTABLE(dwb.gui.right), (LAYOUT & BottomStack) ^ 1);
  dwb_resize(SIZE);
  return true;
}/*}}}*/

/* History {{{*/
gboolean 
dwb_history_back(Arg *arg) {
  gboolean ret = false;
  WebKitWebView *w = WEBVIEW_FROM_ARG(arg);
  if (!dwb.state.nummod) {
    if (webkit_web_view_can_go_back(w)) {
      webkit_web_view_go_back(w);
      ret = true;
    }
  }
  return ret;
}
gboolean 
dwb_history_forward(Arg *arg) {
  gboolean ret = false;
  WebKitWebView *w = WEBVIEW_FROM_ARG(arg);
  if (!dwb.state.nummod) {
    if (webkit_web_view_can_go_forward(w)) {
      webkit_web_view_go_forward(w);
      ret = true;
    }
  }
  return ret;
}/*}}}*/

/* dwb_push_master {{{*/
gboolean 
dwb_push_master(Arg *arg) {
  GList *gl, *l;
  View *old, *new;
  if (!dwb.state.views->next) {
    return false;
  }
  if (arg->p) {
    gl = arg->p;
  }
  else if (dwb.state.nummod) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod);
    if (!gl) {
      return false;
    }
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
  if (LAYOUT & Maximized) {
    gtk_widget_show(dwb.gui.left);
    gtk_widget_hide(dwb.gui.right);
    gtk_widget_show(new->vbox);
    gtk_widget_hide(old->vbox);
  }
  gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), new->tabevent, -1);
  dwb_update_layout();
  return true;
}/*}}}*/

/* dwb_open(Arg *arg) {{{*/
gboolean  
dwb_open(Arg *arg) {
  dwb.state.nv = arg->n;
  gtk_widget_grab_focus(dwb.gui.entry);
  dwb.mode = OpenMode;
  return true;
} /*}}}*/

/* dwb_get_search_engine(const gchar *uri) {{{*/
gchar *
dwb_get_search_engine(const gchar *uri) {
  gchar *ret = NULL;
  return ret;
}/*}}}*/

/* dwb_toggle_maximized {{{*/
void 
dwb_maximized_hide_zoom(View *v, View *no) {
  if (FACTOR != 1.0) {
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), 1.0);
  }
  if (v != dwb.state.fview->data) {
    gtk_widget_hide(v->vbox);
  }
}
void 
dwb_maximized_show_zoom(View *v) {
  if (FACTOR != 1.0 && v != dwb.state.views->data) {
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), FACTOR);
  }
  gtk_widget_show(v->vbox);
}

void 
dwb_toggle_maximized(Arg *arg) {
  LAYOUT ^= Maximized;
  if (LAYOUT & Maximized) {
    g_list_foreach(dwb.state.views,  (GFunc)dwb_maximized_hide_zoom, NULL);
    if  (dwb.state.views == dwb.state.fview) {
      gtk_widget_hide(dwb.gui.right);
    }
    else if (dwb.state.views->next) {
      gtk_widget_hide(dwb.gui.left);
    }
  }
  else {
    if (dwb.state.views->next) {
      gtk_widget_show(dwb.gui.right);
    }
    gtk_widget_show(dwb.gui.left);
    g_list_foreach(dwb.state.views,  (GFunc)dwb_maximized_show_zoom, NULL);
  }
}/*}}}*/

/* dwb_remove_view(Arg *arg) {{{*/
void 
dwb_remove_view(Arg *arg) {
  GList *gl;
  if (!dwb.state.views->next) {
    dwb_exit();
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
      View *newfirst = dwb.state.views->next->data;
      gtk_widget_reparent(newfirst->vbox, dwb.gui.left);
    }
  }
  gtk_widget_destroy(v->vbox);
  dwb_grab_focus(dwb.state.fview);
  gtk_container_remove(GTK_CONTAINER(dwb.gui.topbox), v->tabevent);

  // clean up
  dwb_source_remove(gl);
  g_free(v->status);
  g_free(v);

  dwb.state.views = g_list_delete_link(dwb.state.views, gl);

  if (LAYOUT & Maximized) {
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
  dwb_update_layout();
}/*}}}*/

/* dwb_resize(gdouble size) {{{*/
void
dwb_resize(gdouble size) {
  gint fact = LAYOUT & BottomStack;

  gtk_widget_set_size_request(dwb.gui.left,  (100 - size) * (fact^1),  (100 - size) *  fact);
  gtk_widget_set_size_request(dwb.gui.right, size * (fact^1), size * fact);
}/*}}}*/

/* dwb_focus(GList *gl) {{{*/
void 
dwb_focus(GList *gl) {
  GList *tmp = NULL;

  if (dwb.state.fview) {
    tmp = dwb.state.fview;
  }
  if (tmp) {
    dwb_view_modify_style(tmp, &dwb.color.normal_fg, &dwb.color.normal_bg, &dwb.color.tab_normal_fg, &dwb.color.tab_normal_bg, dwb.font.fd_oblique, dwb.font.normal_size);
    dwb_source_remove(tmp);
    CLEAR_COMMAND_TEXT(tmp);
  }
  dwb_grab_focus(gl);
} /*}}}*/

/* dwb_focus_next(Arg *arg) {{{*/
gboolean 
dwb_focus_next(Arg *arg) {
  GList *gl = dwb.state.fview;
  if (!dwb.state.views->next) {
    return false;
  }
  if (gl->next) {
    if (LAYOUT & Maximized) {
      if (gl == dwb.state.views) {
        gtk_widget_hide(dwb.gui.left);
        gtk_widget_show(dwb.gui.right);
      }
      gtk_widget_show(((View *)gl->next->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(gl->next);
  }
  else {
    if (LAYOUT & Maximized) {
      gtk_widget_hide(dwb.gui.right);
      gtk_widget_show(dwb.gui.left);
      gtk_widget_show(((View *)dwb.state.views->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(g_list_first(dwb.state.views));
  }
  return true;
}/*}}}*/

/* dwb_focus_prev(Arg *arg) {{{*/
gboolean 
dwb_focus_prev(Arg *arg) {
  GList *gl = dwb.state.fview;
  if (!dwb.state.views->next) {
    return false;
  }
  if (gl == dwb.state.views) {
    GList *last = g_list_last(dwb.state.views);
    if (LAYOUT & Maximized) {
      gtk_widget_hide(dwb.gui.left);
      gtk_widget_show(dwb.gui.right);
      gtk_widget_show(((View *)last->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(last);
  }
  else {
    if (LAYOUT & Maximized) {
      if (gl == dwb.state.views->next) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
      gtk_widget_show(((View *)gl->prev->data)->vbox);
      gtk_widget_hide(((View *)gl->data)->vbox);
    }
    dwb_focus(gl->prev);
  }
  return true;
}/*}}}*/
/*}}}*/

/* DWB_WEB_VIEW {{{*/

void 
dwb_web_view_add_history_item(GList *gl) {
  WebKitWebView *web = WEBVIEW(gl);
  const gchar *uri = webkit_web_view_get_uri(web);
  const gchar *title = webkit_web_view_get_title(web);
  WebKitWebBackForwardList *bl = webkit_web_view_get_back_forward_list(web);
  WebKitWebHistoryItem *hitem = webkit_web_history_item_new_with_data(uri,  title);
  webkit_web_back_forward_list_add_item(bl, hitem);
}

void 
dwb_init_setttings(View *v) {
  //WebKitWebSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(v->web));

  //for (int i=0; i<LENGTH(SETTINGS); i++) {
  //  WebSettings s = SETTINGS[i];
  //  if (s.type.f > 0.01) {
  //  printf("%f\n", s.type.f);
  //    g_object_set(settings, s.prop, s.type.f, NULL);
  //  }
  //  else {
  //    g_object_set(settings, s.prop, s.type, NULL);
  //  }

  //}
  ////gint string;
  ////g_object_get(settings, "default-monospace-font-size", &string, NULL);
  ////printf("%d\n", string);
}

/* dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, gint fontsize) {{{*/
void dwb_view_modify_style(GList *gl, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, gint fontsize) {
  View *v = gl->data;
  gtk_widget_modify_fg(v->rstatus, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_fg(v->lstatus, GTK_STATE_NORMAL, fg);
  gtk_widget_modify_bg(v->statusbox, GTK_STATE_NORMAL, bg);
  gtk_widget_modify_fg(v->tablabel, GTK_STATE_NORMAL, tabfg);
  gtk_widget_modify_bg(v->tabevent, GTK_STATE_NORMAL, tabbg);
  pango_font_description_set_absolute_size(fd, fontsize * PANGO_SCALE);
  gtk_widget_modify_font(v->rstatus, fd);
  gtk_widget_modify_font(v->lstatus, fd);

} /*}}}*/

/* dwb_web_view_init_signals(View *v) {{{*/
void
dwb_web_view_init_signals(GList *gl) {
  View *v = gl->data;
  //g_signal_connect(v->vbox, "key-press-event", G_CALLBACK(dwb_key_press_cb), v);
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

} /*}}}*/

/* dwb_create_web_view(View *v) {{{*/
GList * 
dwb_create_web_view(GList *gl) {
  View *v = malloc(sizeof(View));
  ViewStatus *status = malloc(sizeof(ViewStatus));
  v->status = status;
  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();
  // Statusbox
  GtkWidget *status_hbox;
  v->statusbox = gtk_event_box_new();
  v->lstatus = gtk_label_new(NULL);
  v->rstatus = gtk_label_new(NULL);
  gtk_misc_set_alignment(GTK_MISC(v->lstatus), 0.0, 0.0);
  gtk_misc_set_alignment(GTK_MISC(v->rstatus), 1.0, 1.0);
  status_hbox = gtk_hbox_new(false, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->lstatus, true, true, 0);
  gtk_box_pack_start(GTK_BOX(status_hbox), v->rstatus, true, true, 0);
  gtk_label_set_use_markup(GTK_LABEL(v->lstatus), true);
  gtk_label_set_use_markup(GTK_LABEL(v->rstatus), true);

  // Srolling
  GtkWidget *vscroll = gtk_vscrollbar_new(NULL);
  GtkWidget *hscroll = gtk_hscrollbar_new(NULL);
  v->vadj = GTK_ADJUSTMENT(gtk_range_get_adjustment(GTK_RANGE(vscroll)));
  v->hadj = GTK_ADJUSTMENT(gtk_range_get_adjustment(GTK_RANGE(hscroll)));
  gtk_widget_set_scroll_adjustments(v->web, v->hadj, v->vadj);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->web, true, true, 0);

  // Tabbar
  v->tabevent = gtk_event_box_new();
  v->tablabel = gtk_label_new(NULL);
  gtk_label_set_use_markup(GTK_LABEL(v->tablabel), true);
  gtk_label_set_width_chars(GTK_LABEL(v->tablabel), 1);
  gtk_misc_set_alignment(GTK_MISC(v->tablabel), 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(v->tabevent), v->tablabel);
  gtk_widget_modify_font(v->tablabel, dwb.font.fd_normal);

  gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->statusbox, false, false, 0);
  gtk_container_add(GTK_CONTAINER(v->statusbox), status_hbox);

  gtk_box_pack_start(GTK_BOX(dwb.gui.left), v->vbox, true, true, 0);
  gtk_widget_show(v->vbox);
  gtk_widget_show_all(v->statusbox);
  gtk_widget_show_all(v->web);
  gtk_widget_show_all(v->tabevent);

  gl = g_list_prepend(gl, v);
  dwb_web_view_init_signals(gl);
  dwb_init_setttings(v);
  return gl;
} /*}}}*/

/* dwb_add_view(Arg *arg) ret: View *{{{*/
void 
dwb_add_view(Arg *arg) {
  if (dwb.state.views) {
    View *views = dwb.state.views->data;
    CLEAR_COMMAND_TEXT(dwb.state.views);
    gtk_widget_reparent(views->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), views->vbox, 0);
    if (LAYOUT & Maximized) {
      gtk_widget_hide(((View *)dwb.state.fview->data)->vbox);
      if (dwb.state.fview != dwb.state.views) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
    }
  }
  dwb.state.views = dwb_create_web_view(dwb.state.views);
  dwb_focus(dwb.state.views);
  dwb_update_layout();

} /*}}}*//*}}}*/

/* COMMAND_TEXT {{{*/

/* dwb_set_status_bar_text(GList *gl, const char *text, GdkColor *fg,  PangoFontDescription *fd) {{{*/
void
dwb_set_status_bar_text(GtkWidget *label, const char *text, GdkColor *fg,  PangoFontDescription *fd) {
  gtk_label_set_text(GTK_LABEL(label), text);
  if (fg) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, fg);
  }
  if (fd) {
    gtk_widget_modify_font(label, fd);
  }
}/*}}}*/

/* hide command text {{{*/
void 
dwb_source_remove(GList *gl) {
  guint id;
  View *v = gl->data;
  if ( (id = v->status->message_id) ) {
    g_source_remove(id);
  }
}
gpointer 
dwb_hide_message(GList *gl) {
  CLEAR_COMMAND_TEXT(gl);
  return NULL;
}/*}}}*/

void 
dwb_set_normal_message(GList *gl, const gchar  *text, gboolean hide) {
  View *v = gl->data;
  dwb_set_status_bar_text(v->lstatus, text, &dwb.color.active_fg, dwb.font.fd_bold);
  dwb_source_remove(gl);
  if (hide) {
    v->status->message_id = g_timeout_add_seconds(MESSAGE_DELAY, (GSourceFunc)dwb_hide_message, gl);
  }
}

void 
dwb_set_error_message(GList *gl, const gchar *error) {
  dwb_source_remove(gl);
  dwb_set_status_bar_text(VIEW(gl)->lstatus, error, &dwb.color.error, dwb.font.fd_bold);
  VIEW(gl)->status->message_id = g_timeout_add_seconds(MESSAGE_DELAY, (GSourceFunc)dwb_hide_message, gl);
}

/* dwb_update_status_text(GList *gl) {{{*/
void 
dwb_update_status_text(GList *gl) {
  View *v = gl->data;
  GtkAdjustment *a = v->vadj;
  const gchar *uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(v->web));
  GString *string = g_string_new(uri);
  gdouble lower = gtk_adjustment_get_lower(a);
  gdouble upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
  gdouble value = gtk_adjustment_get_value(a); 
  gchar *position = 
          upper == lower ? g_strdup(" -- [all]") : 
          value == lower ? g_strdup(" -- [top]") : 
          value == upper ? g_strdup(" -- [bot]") : 
          g_strdup_printf(" -- [%02d%%]", (gint)(value * 100/upper));
  g_string_append(string, position);

  dwb_set_status_text(gl, string->str, NULL, NULL);
  g_string_free(string, true);
  g_free(position);
}/*}}}*/

/*dwb_set_status_text(GList *gl, const gchar *text, GdkColor *fg, PangoFontDescription *fd) {{{*/
void 
dwb_set_status_text(GList *gl, const gchar *text, GdkColor *fg, PangoFontDescription *fd) {
  gchar *escaped = g_markup_escape_text(text, -1);
  dwb_set_status_bar_text(VIEW(gl)->rstatus, escaped, fg, fd);
  g_free(escaped);
}/*}}}*/
/*}}}*/

/* FUNCTIONS {{{*/

gboolean
dwb_update_hints(GdkEventKey *e) {
  gchar *buffer = NULL;
  gboolean ret = false;
  gchar *com = NULL;

  if (DIGIT(e)) {
    dwb.state.nummod = MIN(10*dwb.state.nummod + e->keyval - GDK_0, 314159);
    gchar *text = g_strdup_printf("hint number: %d", dwb.state.nummod);
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, text, &dwb.color.active_fg, dwb.font.fd_normal);
    com = g_strdup_printf("update_hints(\"%d\")", dwb.state.nummod);
    g_free(text);
    ret = true;
  }
  else if (e->keyval == GDK_Tab) {
    if (e->state & GDK_CONTROL_MASK) {
      com = g_strdup("focus_prev()");
    }
    else {
      com = g_strdup("focus_next()");
    }
    ret = true;
  }
  else {
    com = g_strdup_printf("update_hints(\"%s\")", gtk_entry_get_text(GTK_ENTRY(dwb.gui.entry)));
  }
  buffer = dwb_execute_script(com);
  g_free(com);
  if (buffer && strcmp(buffer, "undefined")) {
    if (!strcmp("_dwb_no_hints_", buffer)) {
      dwb_set_error_message(dwb.state.fview, NO_HINTS);
    }
    else if (g_str_has_prefix(buffer, "_dwb_input_")) {
      gtk_widget_grab_focus(VIEW(dwb.state.fview)->web);
      dwb_insert_mode(NULL);
    }
    else if (g_str_has_prefix(buffer, "_dwb_setting_")) {
      dwb.state.key_id = g_strdup(&buffer[13]);
      dwb.state.mode = KeySettings;
    }
    else {
      Arg a = { .p = buffer };
      dwb_load_uri(&a);
    }
  }
  g_free(buffer);

  return ret;
}

/* dwb_execute_script {{{*/
gchar * 
dwb_execute_script(const char *com) {
  View *v = dwb.state.fview->data;

  JSValueRef exc, eval_ret;
  size_t length;
  gchar *ret = NULL;

  WebKitWebFrame *frame =  webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  JSContextRef context = webkit_web_frame_get_global_context(frame);
  JSStringRef text = JSStringCreateWithUTF8CString(com);
  eval_ret = JSEvaluateScript(context, text, JSContextGetGlobalObject(context), NULL, 0, &exc);
  JSStringRelease(text);

  if (eval_ret) {
    JSStringRef string = JSValueToStringCopy(context, eval_ret, NULL);
    length = JSStringGetMaximumUTF8CStringSize(string);
    ret = g_new(gchar, length);
    JSStringGetUTF8CString(string, ret, length);
    JSStringRelease(string);
  }
  return ret;
}
/*}}}*/

/* dwb_append_navigation(GList *gl, GList *view) {{{*/
gboolean 
dwb_append_navigation(GList *gl, GList **fc) {
  WebKitWebView *w = WEBVIEW(gl);
  const gchar *uri = webkit_web_view_get_uri(w);
  if (uri) {
    const gchar *title = webkit_web_view_get_title(w);
    for (GList *l = (*fc); l; l=l->next) {
      Navigation *n = l->data;
      if (!strcmp(uri, n->uri)) {
        dwb_navigation_free(n);
        (*fc) = g_list_delete_link((*fc), l);
        break;
      }
    }
    Navigation *n = dwb_navigation_new(uri, title);

    (*fc) = g_list_prepend((*fc), n);
    return true;
  }
  return false;
  
}/*}}}*/

/* dwb_save_quickmark(const gchar *key) {{{*/
void 
dwb_save_quickmark(const gchar *key) {
  WebKitWebView *w = WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web);
  const gchar *uri = webkit_web_view_get_uri(w);
  if (uri) {
    const gchar *title = webkit_web_view_get_title(w);
    for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
      Quickmark *q = l->data;
      if (!strcmp(key, q->key)) {
        dwb_quickmark_free(q);
        dwb.fc.quickmarks = g_list_delete_link(dwb.fc.quickmarks, l);
        break;
      }
    }
    dwb.fc.quickmarks = g_list_prepend(dwb.fc.quickmarks, dwb_quickmark_new(uri, title, key));
    dwb_normal_mode(false);
    gchar *message = g_strdup_printf("Added quickmark: %s - %s.", key, uri);
    dwb_set_normal_message(dwb.state.fview, message, true);
    g_free(message);
  }
  else {
    dwb_set_error_message(dwb.state.fview, NO_URL);
  }
}/*}}}*/

/* dwb_get_key_setting {{{*/
void 
dwb_get_key_setting(GdkEventKey *e) {
  KeyValue value;
  if (e->keyval == GDK_Return) {
    gchar *buffer = dwb_execute_script("get_active_element()");
    if (buffer) {
      g_strstrip(buffer);
    }
    if (strlen(buffer) > 0 && dwb.state.key_id) {
      gchar **token = g_strsplit(buffer, " ", -1);

      value.desc = dwb.state.key_id;
      value.key = dwb_strv_to_key(token, g_strv_length(token));

      dwb.keymap = dwb_keymap_replace(dwb.keymap, value);
      dwb_set_normal_message(dwb.state.fview, "Key saved.", true);

      dwb.state.key_id = NULL;
      g_free(dwb.state.key_id);
      g_strfreev(token);
    }
    
    dwb_normal_mode(false);
  }

}/*}}}*/

/* dwb_open_quickmark(const gchar *key){{{*/
void 
dwb_open_quickmark(const gchar *key) {
  for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
    Quickmark *q = l->data;
    if (!strcmp(key, q->key)) {
      Arg a = { .p = q->nav->uri };
      dwb_load_uri(&a);
      dwb_normal_mode(true);
      gchar *message = g_strdup_printf("Loading quickmark %s: %s", key, q->nav->uri);
      dwb_set_normal_message(dwb.state.fview, message, true);
      g_free(message);
      return;
    }
  }
  dwb_normal_mode(true);
  gchar *message = g_strdup_printf("No such quickmark: %s", key);
  dwb_set_error_message(dwb.state.fview, message);
  g_free(message);
}/*}}}*/

/* dwb_tab_label_set_text {{{*/
void
dwb_tab_label_set_text(GList *gl, const gchar *text) {
  View *v = gl->data;
  const gchar *uri = text ? text : webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
  gchar *escaped = g_markup_printf_escaped("%d : %s", g_list_position(dwb.state.views, gl), uri ? uri : "about:blank");
  gtk_label_set_text(GTK_LABEL(v->tablabel), escaped);
  g_free(escaped);
}/*}}}*/

/* dwb_update_status(GList *gl) {{{*/
void 
dwb_update_status(GList *gl) {
  View *v = gl->data;
  WebKitWebView *w = WEBKIT_WEB_VIEW(v->web);
  const gchar *title = webkit_web_view_get_title(w);
  const gchar *uri = webkit_web_view_get_uri(w);

  gtk_window_set_title(GTK_WINDOW(dwb.gui.window), title ? title : uri);
  dwb_tab_label_set_text(gl, title);

  dwb_update_status_text(gl);
}/*}}}*/

/* dwb_update_tab_label {{{*/
void
dwb_update_tab_label() {
  for (GList *gl = dwb.state.views; gl; gl = gl->next) {
    View *v = gl->data;
    const gchar *title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
    dwb_tab_label_set_text(gl, title);
  }
}/*}}}*/

/* dwb_grab_focus(GList *gl) {{{*/
void 
dwb_grab_focus(GList *gl) {
  View *v = gl->data;

  dwb.state.fview = gl;
  dwb_view_modify_style(gl, &dwb.color.active_fg, &dwb.color.active_bg, &dwb.color.tab_active_fg, &dwb.color.tab_active_bg, dwb.font.fd_bold, dwb.font.active_size);
  gtk_widget_grab_focus(v->web);
}/*}}}*/

/* dwb_load_uri(const char *uri) {{{*/
void 
dwb_load_uri(Arg *arg) {
  gchar *uri = dwb_get_resolved_uri(arg->p);

  if (dwb.state.nv == OpenNewView) {
    dwb_add_view(NULL);
  }
  View *fview = dwb.state.fview->data;
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(fview->web), uri);

}/*}}}*/

/* dwb_update_layout() {{{*/
void
dwb_update_layout() {
  gboolean visible = gtk_widget_get_visible(dwb.gui.right);
  View *v;

  if (LAYOUT & Maximized) {
    return; 
  }
  if (dwb.state.views->next) {
    if (!visible) {
      gtk_widget_show_all(dwb.gui.right);
    }
    v = dwb.state.views->next->data;
    if (FACTOR != 1.0) {
      webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), FACTOR);
    }
    webkit_web_view_set_full_content_zoom(WEBKIT_WEB_VIEW(v->web), true);
  }
  else if (visible) {
    gtk_widget_hide(dwb.gui.right);
  }
  v = dwb.state.views->data;
  webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), 1.0);
  dwb_update_tab_label();
  dwb_resize(SIZE);
}/*}}}*/

/* dwb_eval_key(GdkEventKey *e) {{{*/
gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = true;
  gchar *pre = "buffer: ";
  const gchar *old = dwb.state.buffer ? dwb.state.buffer->str : NULL;
  gint keyval = e->keyval;

  gchar *key = gdk_keyval_name(keyval);
  // don't show backspace in the buffer
  if (keyval == GDK_BackSpace ) {
    if (dwb.state.buffer && dwb.state.buffer->str ) {
      if (dwb.state.buffer->len > strlen(pre)) {
        g_string_erase(dwb.state.buffer, dwb.state.buffer->len - 1, 1);
        dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
      }
      return false;
    }
    else  
      return true;
  }

  if (e->is_modifier) {
    return false;
  }

  if (!old) {
    dwb.state.buffer = g_string_new(pre);
    old = dwb.state.buffer->str;
  }
  if (DIGIT(e)) {
    if (isdigit(old[strlen(old)-1])) {
      dwb.state.nummod = MIN(10*dwb.state.nummod + e->keyval - GDK_0, 314159);
    }
    else {
      dwb.state.nummod = e->keyval - GDK_0;
    }
  }
  g_string_append(dwb.state.buffer, key);
  if (DIGIT(e) || ALPHA(e)) {
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
  }

  const gchar *buf = dwb.state.buffer->str;
  gint length = dwb.state.buffer->len;
  gint longest = 0;
  KeyMap *tmp = NULL;

  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (!strcmp(&buf[length - strlen(km->key)], km->key) && !(CLEAN_STATE(e) ^ km->mod)) {
      if  (!longest || strlen(km->key) > longest) {
        longest = strlen(km->key);
        tmp = km;
      }
      ret = true;
    }
  }
  if (tmp) {
    dwb_simple_command(tmp);
  }
  return ret;

}/*}}}*/

/* dwb_normal_mode() {{{*/
void 
dwb_normal_mode(gboolean clean) {
  View *v = dwb.state.fview->data;

  if (dwb.state.mode == HintMode) {
    dwb_execute_script("clear()");
  }

  dwb.state.mode = NormalMode;
  gtk_widget_grab_focus(v->web);
  if (clean) {
    CLEAR_COMMAND_TEXT(dwb.state.fview);
  }

  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
  }
  dwb_clean_vars();
}/*}}}*/

/* dwb_get_resolved_uri(const gchar *uri) {{{*/
gchar * 
dwb_get_resolved_uri(const gchar *uri) {
    char *tmp = NULL;
    // check if uri is a file
    if ( g_file_test(uri, G_FILE_TEST_IS_REGULAR) ) {
        tmp = g_str_has_prefix(uri, "file://") 
            ? g_strdup(uri) 
            : g_strdup_printf("file://%s", uri);
    }
    else if ( !(tmp = dwb_get_search_engine(uri)) || strstr(uri, "localhost:")) {
        tmp = g_str_has_prefix(uri, "http://") || g_str_has_prefix(uri, "https://")
        ? g_strdup(uri)
        : g_strdup_printf("http://%s", uri);
    }
    return tmp;
}
/*}}}*/
/*}}}*/

/* EXIT {{{*/

/* dwb_clean_vars() {{{*/
void 
dwb_clean_vars() {
  dwb.mode = NormalMode;
  dwb.state.buffer = NULL;
  dwb.state.nv = 0;
}/*}}}*/

/* dwb_clean_up() {{{*/
gboolean
dwb_clean_up() {
  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    g_free(m);
  }
  g_slist_free(dwb.keymap);
  return true;
}/*}}}*/

/* dwb_save_files() {{{*/
gboolean 
dwb_save_files() {
  // Bookmarks
  GString *bookmarks = g_string_new(NULL);
  for (GList *l = dwb.fc.bookmarks; l; l=l->next)  {
    Navigation *n = l->data;
    g_string_append_printf(bookmarks, "%s %s\n", n->uri, n->title);
    g_free(n);
  }
  dwb_set_file_content(dwb.files.bookmarks, bookmarks->str);
  g_string_free(bookmarks, true);

  // History
  GString *history = g_string_new(NULL);
  gint n = 0;
  for (GList *l = dwb.fc.history; n<HISTORY_LENGTH && l; l=l->next, n++)  {
    Navigation *n = l->data;
    g_string_append_printf(history, "%s %s\n", n->uri, n->title);
    g_free(n);
  }
  dwb_set_file_content(dwb.files.history, history->str);
  g_string_free(history, true);

  GString *quickmarks = g_string_new(NULL); 
  for (GList *l = dwb.fc.quickmarks; l; l=l->next)  {
    Quickmark *q = l->data;
    Navigation *n = q->nav;
    g_string_append_printf(quickmarks, "%s %s %s\n", q->key, n->uri, n->title);
    g_free(n);
    g_free(q);
  }
  dwb_set_file_content(dwb.files.quickmarks, quickmarks->str);
  g_string_free(quickmarks, true);

  // save keys
  GKeyFile *keyfile = g_key_file_new();
  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *map = l->data;
    gchar *sc = g_strdup_printf("%s %s", dwb_modmask_to_string(map->mod), map->key);
    g_key_file_set_value(keyfile, "default", map->map->desc, sc);
    g_free(sc);
  }
  gsize l;
  GError *error = NULL;
  gchar *content;
  if ( (content = g_key_file_to_data(keyfile, &l, &error)) ) {
    g_file_set_contents(dwb.files.keys, content, l, &error);
  }
  if (error) {
    fprintf(stderr, "Couldn't save keyfile: %s", error->message);
  }
  return true;
}
/* }}} */

/* dwb_end() {{{*/
gboolean
dwb_end() {
  if (dwb_save_files()) {
    if (dwb_clean_up()) {
      return true;
    }
  }
  return false;
}/*}}}*/

/* dwb_exit() {{{ */
void 
dwb_exit() {
  dwb_end();
  gtk_main_quit();
} /*}}}*/
/*}}}*/

/* INIT {{{*/

gchar *
dwb_modmask_to_string(guint modmask) {
  gchar *mod[7];
  int i=0;
  for (; i<7 && modmask; i++) {
    if (modmask & GDK_CONTROL_MASK) {
      mod[i] = "Control";
      modmask ^= GDK_CONTROL_MASK;
    }
    else if (modmask & GDK_MOD1_MASK) {
      mod[i] = "Mod1";
      modmask ^= GDK_MOD1_MASK;
    }
    else if (modmask & GDK_MOD2_MASK) {
      mod[i] = "Mod2";
      modmask ^= GDK_MOD2_MASK;
    }
    else if (modmask & GDK_MOD3_MASK) {
      mod[i] = "Mod3";
      modmask ^= GDK_MOD3_MASK;
    }
    else if (modmask & GDK_MOD4_MASK) {
      mod[i] = "Mod4";
      modmask ^= GDK_MOD4_MASK;
    }
    else if (modmask & GDK_MOD5_MASK) {
      mod[i] = "Mod5";
      modmask ^= GDK_MOD5_MASK;
    }
  }
  mod[i] = NULL; 
  gchar *line = g_strjoinv(" ", mod);
  return line;
}
Key 
dwb_strv_to_key(gchar **string, gsize length) {
  Key key = { .mod = 0, .str = NULL };
  GString *buffer = g_string_new(NULL);

  for (int i=0; i<length; i++)  {
    if (!g_ascii_strcasecmp(string[i], "Control")) {
      key.mod |= GDK_CONTROL_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod1")) {
      key.mod |= GDK_MOD1_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod2")) {
      key.mod |= GDK_MOD2_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod3")) {
      key.mod |= GDK_MOD3_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod4")) {
      key.mod |= GDK_MOD4_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod5")) {
      key.mod |= GDK_MOD5_MASK;
    }
    else {
      g_string_append(buffer, string[i]);
    }
  }
  key.str = g_strdup(buffer->str);
  g_string_free(buffer, true);
  return key;
}

/* dwb_generate_keyfile {{{*/
void 
dwb_generate_keyfile() {
  gchar *path = g_strconcat(g_get_user_config_dir(), "/", dwb.misc.name, "/",  NULL);
  dwb.files.keys = g_strconcat(path, "keys", NULL);
  GKeyFile *keyfile = g_key_file_new();
  gsize l;
  GError *error = NULL;
  
  for (gint i=0; i<LENGTH(KEYS); i++) {
    KeyValue k = KEYS[i];
    gchar *value = k.key.mod ? g_strdup_printf("%s %s", dwb_modmask_to_string(k.key.mod), k.key.str) : g_strdup(k.key.str);
    g_key_file_set_value(keyfile, "default", k.desc, value);
    g_free(value);
  }
  gchar *content;
  if ( (content = g_key_file_to_data(keyfile, &l, &error)) ) {
    g_file_set_contents(dwb.files.keys, content, l, &error);
  }
  if (error) {
    fprintf(stderr, "Couldn't create keyfile: %s\n", error->message);
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "Keyfile created.\n");
}/*}}}*/

GSList * 
dwb_keymap_replace(GSList *gl, KeyValue key) {
  for (GSList *l = gl; l; l=l->next) {
    KeyMap *km = l->data;
    if (!strcmp(km->map->desc, key.desc)) {
      gl = g_slist_delete_link(gl, l);
      break;
    }
  }
  gl = dwb_create_key_map(gl, key);
  gl = g_slist_sort(gl, (GCompareFunc)dwb_keymap_sort_text);
  return gl;
}

GSList *
dwb_create_key_map(GSList *gl, KeyValue key) {
  for (int i=0; i<LENGTH(FMAP); i++) {
    if (!strcmp(FMAP[i].desc, key.desc)) {
      KeyMap *keymap = malloc(sizeof(KeyMap));
      FunctionMap *fmap = &FMAP[i];
      keymap->key = key.key.str;
      keymap->mod = key.key.mod;
      fmap->desc = key.desc;
      keymap->map = fmap;
      gl = g_slist_prepend(gl, keymap);
      break;
    }
  }
  return gl;
}

GSList *
dwb_read_key_config() {
  GKeyFile *keyfile = g_key_file_new();
  gchar *groupname = "default";
  gchar *content;
  gsize l, keylength;
  GError *error = NULL;
  gchar **keys;
  GSList *gl;
  if (g_file_get_contents(dwb.files.keys, &content, &l, &error)) {
    if (g_key_file_load_from_data(keyfile, content, l, G_KEY_FILE_KEEP_COMMENTS, &error)) {
      if ( (keys = g_key_file_get_keys(keyfile, groupname, &keylength, &error)) ) {
        for  (int i=0; i<keylength; i++) {
          gchar **stringlist = g_key_file_get_string_list(keyfile, groupname, keys[i], &l, NULL);
          Key key = dwb_strv_to_key(stringlist, l);
          KeyValue kv;
          kv.desc = keys[i];
          kv.key = key;
          gl = dwb_keymap_replace(gl, kv);
          g_strfreev(stringlist);
        }
      }
    }
  }
  if (error) {
    fprintf(stderr, "Couldn't read config: %s\n", error->message);
  }
  return gl;
}

/* dwb_init_key_map() {{{*/
void 
dwb_init_key_map() {
  dwb.keymap = NULL;
  if (g_file_test(dwb.files.keys, G_FILE_TEST_IS_REGULAR)) {
    dwb.keymap = dwb_read_key_config();
  }
  else {
    for (int j=0; j<LENGTH(KEYS); j++) {
      dwb.keymap = dwb_create_key_map(dwb.keymap, KEYS[j]);
    }
    puts("No key-configuration, using default! A custom keyboard-file can be generated with --generate-keyfile");
  }
  dwb.keymap = g_slist_sort(dwb.keymap, (GCompareFunc)dwb_keymap_sort_text);
}/*}}}*/

/* dwb_init_style() {{{*/
void
dwb_init_style() {
  // Colors 
  // Statusbar
  gdk_color_parse(ACTIVE_FG_COLOR, &dwb.color.active_fg);
  gdk_color_parse(ACTIVE_BG_COLOR, &dwb.color.active_bg);
  gdk_color_parse(NORMAL_FG_COLOR, &dwb.color.normal_fg);
  gdk_color_parse(NORMAL_BG_COLOR, &dwb.color.normal_bg);

  // Tabs
  gdk_color_parse(TAB_ACTIVE_FG_COLOR, &dwb.color.tab_active_fg);
  gdk_color_parse(TAB_ACTIVE_BG_COLOR, &dwb.color.tab_active_bg);
  gdk_color_parse(TAB_NORMAL_FG_COLOR, &dwb.color.tab_normal_fg);
  gdk_color_parse(TAB_NORMAL_BG_COLOR, &dwb.color.tab_normal_bg);

  //Entry
  gdk_color_parse(ENTRY_FG_COLOR, &dwb.color.entry_fg);
  gdk_color_parse(ENTRY_BG_COLOR, &dwb.color.entry_bg);

  gdk_color_parse(ERROR_COLOR, &dwb.color.error);

  // Fonts
  dwb.font.fd_normal = pango_font_description_from_string(FONT);
  pango_font_description_set_absolute_size(dwb.font.fd_normal, ACTIVE_FONT_SIZE * PANGO_SCALE);
  dwb.font.fd_bold = pango_font_description_from_string(FONT);
  pango_font_description_set_absolute_size(dwb.font.fd_bold, ACTIVE_FONT_SIZE * PANGO_SCALE);
  dwb.font.fd_oblique = pango_font_description_from_string(FONT);
  pango_font_description_set_absolute_size(dwb.font.fd_oblique, ACTIVE_FONT_SIZE * PANGO_SCALE);
  pango_font_description_set_weight(dwb.font.fd_normal, PANGO_WEIGHT_NORMAL);
  pango_font_description_set_weight(dwb.font.fd_bold, PANGO_WEIGHT_BOLD);
  pango_font_description_set_style(dwb.font.fd_oblique, PANGO_STYLE_OBLIQUE);
  
  // Fontsizes
  dwb.font.active_size = ACTIVE_FONT_SIZE;
  dwb.font.normal_size = NORMAL_FONT_SIZE;
} /*}}}*/

/* dwb_init_gui() {{{*/
void 
dwb_init_gui() {
  // Window
  dwb.gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (embed) {
    dwb.gui.window = gtk_plug_new(embed);
  } 
  else {
    dwb.gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_wmclass(GTK_WINDOW(dwb.gui.window), dwb.misc.name, dwb.misc.name);
  }
  gtk_window_set_default_size(GTK_WINDOW(dwb.gui.window), DEFAULT_WIDTH, DEFAULT_HEIGHT);
  g_signal_connect(dwb.gui.window, "delete-event", G_CALLBACK(dwb_exit), NULL);
  g_signal_connect(dwb.gui.window, "key-press-event", G_CALLBACK(dwb_key_press_cb), NULL);

  // Main
  dwb.gui.vbox = gtk_vbox_new(false, 1);
  dwb.gui.topbox = gtk_hbox_new(true, 3);
  dwb.gui.paned = gtk_hpaned_new();
  dwb.gui.left = gtk_vbox_new(true, 0);
  dwb.gui.right = gtk_vbox_new(true, 1);

  // Paned
  GtkWidget *paned_event = gtk_event_box_new(); 
  gtk_widget_modify_bg(paned_event, GTK_STATE_NORMAL, &dwb.color.normal_bg);
  gtk_widget_modify_bg(dwb.gui.paned, GTK_STATE_NORMAL, &dwb.color.normal_bg);
  gtk_widget_modify_bg(dwb.gui.paned, GTK_STATE_PRELIGHT, &dwb.color.active_bg);
  gtk_container_add(GTK_CONTAINER(paned_event), dwb.gui.paned);
  // Entry
  dwb.gui.entry = gtk_entry_new();
  gtk_entry_set_inner_border(GTK_ENTRY(dwb.gui.entry), NULL);

  gtk_widget_modify_font(dwb.gui.entry, dwb.font.fd_bold);
  gtk_widget_modify_base(dwb.gui.entry, GTK_STATE_NORMAL, &dwb.color.entry_bg);
  gtk_widget_modify_text(dwb.gui.entry, GTK_STATE_NORMAL, &dwb.color.entry_fg);
  gtk_entry_set_has_frame(GTK_ENTRY(dwb.gui.entry), false);

  g_signal_connect(dwb.gui.entry, "key-press-event", G_CALLBACK(dwb_entry_keypress_cb), NULL);
  g_signal_connect(dwb.gui.entry, "key-release-event", G_CALLBACK(dwb_entry_release_cb), NULL);
  g_signal_connect(dwb.gui.entry, "activate", G_CALLBACK(dwb_entry_activate_cb), NULL);

  // Pack
  gtk_paned_pack1(GTK_PANED(dwb.gui.paned), dwb.gui.left, true, true);
  gtk_paned_pack2(GTK_PANED(dwb.gui.paned), dwb.gui.right, true, true);

  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.topbox, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), paned_event, true, true, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.entry, false, false, 0);


  dwb_add_view(NULL);
  gtk_container_add(GTK_CONTAINER(dwb.gui.window), dwb.gui.vbox);

  gtk_widget_show(dwb.gui.entry);
  gtk_widget_show(dwb.gui.left);
  gtk_widget_show(dwb.gui.paned);
  gtk_widget_show(paned_event);
  gtk_widget_show_all(dwb.gui.topbox);

  gtk_widget_show(dwb.gui.vbox);
  gtk_widget_show(dwb.gui.window);
} /*}}}*/

/* dwb_init_file_content {{{*/
GList *
dwb_init_file_content(GList *gl, const gchar *filename, void *(*func)(const gchar *)) {
  gchar *content = dwb_get_file_content(filename);
  if (content) {
    gchar **token = g_strsplit(content, "\n", 0);
    gint length = MAX(g_strv_length(token) - 1, 0);
    for (int i=0;  i < length; i++) {
      gl = g_list_append(gl, func(token[i]));
    }
    g_free(content);
    g_strfreev(token);
  }
  return gl;
}/*}}}*/

/* dwb_init_files() {{{*/
void
dwb_init_files() {
  gchar *path = g_strconcat(g_get_user_config_dir(), "/", dwb.misc.name, "/",  NULL);
  dwb.files.bookmarks     = g_strconcat(path, "bookmarks", NULL);
  dwb.files.history       = g_strconcat(path, "history", NULL);
  dwb.files.stylesheet    = g_strconcat(path, "stylesheet", NULL);
  dwb.files.quickmarks    = g_strconcat(path, "quickmarks", NULL);
  dwb.files.session       = g_strconcat(path, "session", NULL);
  dwb.files.searchengines = g_strconcat(path, "searchengines", NULL);
  dwb.files.stylesheet    = g_strconcat(path, "stylesheet", NULL);
  dwb.files.keys          = g_strconcat(path, "keys", NULL);
  dwb.files.scriptdir     = g_strconcat(path, "/scripts", NULL);

  dwb.misc.scripts = dwb_get_directory_content(dwb.files.scriptdir);

  dwb.fc.bookmarks = dwb_init_file_content(dwb.fc.bookmarks, dwb.files.bookmarks, (void*)dwb_navigation_new_from_line); 
  dwb.fc.history = dwb_init_file_content(dwb.fc.history, dwb.files.history, (void*)dwb_navigation_new_from_line); 
  dwb.fc.quickmarks = dwb_init_file_content(dwb.fc.quickmarks, dwb.files.quickmarks, (void*)dwb_quickmark_new_from_line); 
  
}/*}}}*/

/* signals{{{*/
void
dwb_handle_signal(gint s) {
  if (s == SIGALRM || s == SIGFPE || s == SIGILL || s == SIGINT || s == SIGQUIT || s == SIGTERM) {
    dwb_end();
    exit(EXIT_SUCCESS);
  }
  else if (s == SIGSEGV) {
    fprintf(stderr, "Received SIGSEGV, try to clean up.\n");
    if (dwb_end()) {
      fprintf(stderr, "Success.\n");
    }
    exit(EXIT_FAILURE);
  }
}

void 
dwb_init_signals() {
  for (int i=0; i<LENGTH(signals); i++) {
    struct sigaction act, oact;
    act.sa_handler = dwb_handle_signal;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(signals[i], &act, &oact);
  }
}/*}}}*/

/* dwb_init() {{{*/
void dwb_init() {

  dwb_clean_vars();
  dwb.state.views = NULL;
  dwb.state.fview = NULL;

  dwb_init_signals();
  dwb_init_files();
  dwb_init_style();
  dwb_init_key_map();
  dwb_init_gui();
} /*}}}*/ /*}}}*/

int main(gint argc, gchar **argv) {
  dwb.misc.name = NAME;
  if (!g_thread_supported()) {
    g_thread_init(NULL);
  }
  if (argc > 1) {
    for (int i=0; i<argc; i++) {
      if (!strcmp(argv[i], "--generate-keyfile")) {
        dwb_generate_keyfile();
        exit(EXIT_SUCCESS);
      }
    }
  }
  gtk_init(&argc, &argv);
  dwb_init();
  gtk_main();
  return EXIT_SUCCESS;
}
