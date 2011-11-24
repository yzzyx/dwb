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

#ifndef DWB_H
#define DWB_H
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <libgen.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libsoup/soup.h>
#include <pwd.h>
#include <grp.h>
#include <locale.h>
#include <stdarg.h>
#include <dirent.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h> 

#ifndef NAME
#define NAME "dwb"
#endif
#ifndef REAL_NAME
#define REAL_NAME NAME
#endif
#ifndef VERSION
#define VERSION "0.0.18"
#endif
#ifndef COPYRIGHT
#define COPYRIGHT "© 2010-2011 Stefan Bolte"
#endif
#define SINGLE_INSTANCE 1
#define NEW_INSTANCE 2

#define PBAR_LENGTH   20
#define STRING_LENGTH 1024
#define BUFFER_LENGTH 256

#define INSERT "Insert Mode"

#define NO_URL                      "No URL in current context"
#define NO_HINTS                    "No Hints in current context"

#define HINT_SEARCH_SUBMIT "_dwb_search_submit_"

#define _HAS_GTK3  GTK_CHECK_VERSION(3, 0, 0)

#ifndef true
#define true 1
#endif
#ifndef false 
#define false 0
#endif

#if _HAS_GTK3 
#define DwbColor          GdkRGBA
#define DWB_COLOR_PARSE(color, string)   (gdk_rgba_parse(color, string))
#define DWB_WIDGET_OVERRIDE_BACKGROUND   gtk_widget_override_background_color
#define DWB_WIDGET_OVERRIDE_BASE         gtk_widget_override_background_color
#define DWB_WIDGET_OVERRIDE_COLOR        gtk_widget_override_color
#define DWB_WIDGET_OVERRIDE_TEXT         gtk_widget_override_color
#define DWB_WIDGET_OVERRIDE_FONT         gtk_widget_override_font
#else 
#define DwbColor          GdkColor
#define DWB_COLOR_PARSE(color, string)   (gdk_color_parse(string, color))
#define DWB_WIDGET_OVERRIDE_BACKGROUND   gtk_widget_modify_bg
#define DWB_WIDGET_OVERRIDE_BASE         gtk_widget_modify_base
#define DWB_WIDGET_OVERRIDE_COLOR        gtk_widget_modify_fg
#define DWB_WIDGET_OVERRIDE_TEXT         gtk_widget_modify_text
#define DWB_WIDGET_OVERRIDE_FONT         gtk_widget_modify_font
#endif

/* MAKROS {{{*/ 
#define LENGTH(X)   (sizeof(X)/sizeof(X[0]))
#define GLENGTH(X)  (sizeof(X)/g_array_get_element_size(X)) 
#define NN(X)       ( ((X) == 0) ? 1 : (X) )

//#define CLEAN_STATE(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_BUTTON1_MASK) & ~(GDK_BUTTON2_MASK) & ~(GDK_BUTTON3_MASK) & ~(GDK_BUTTON4_MASK) & ~(GDK_BUTTON5_MASK) & ~(GDK_LOCK_MASK) & ~(GDK_MOD2_MASK) &~(GDK_MOD3_MASK) & ~(GDK_MOD5_MASK))
#define CLEAN_STATE(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_LOCK_MASK) & ~(GDK_MOD2_MASK) &~(GDK_MOD3_MASK) & ~(GDK_MOD5_MASK))
#define CLEAN_STATE_WITH_SHIFT(X) (X->state & ~(GDK_LOCK_MASK) & ~(GDK_MOD2_MASK) &~(GDK_MOD3_MASK) & ~(GDK_MOD5_MASK))
#define CLEAN_SHIFT(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_LOCK_MASK))
#define CLEAN_COMP_MODE(X)          (X & ~(COMPLETION_MODE) & ~(AUTO_COMPLETE))
#define CLEAN_MODE(mode)            ((mode) & ~(COMPLETION_MODE))
/* Maybe this has to be changed in future releases */
#define DWB_NUMMOD_MASK                 (1<<15)

#define GET_TEXT()                  (gtk_entry_get_text(GTK_ENTRY(dwb.gui.entry)))
#define CURRENT_VIEW()              ((View*)dwb.state.fview->data)
#define VIEW(X)                     ((View*)X->data)
#define WEBVIEW(X)                  (WEBKIT_WEB_VIEW(((View*)X->data)->web))
#define CURRENT_WEBVIEW()           (WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web))
#define MAIN_FRAME()                (webkit_web_view_get_main_frame(CURRENT_WEBVIEW()))  
#define FOCUSED_FRAME()             (webkit_web_view_get_focused_frame(CURRENT_WEBVIEW()))  
#define VIEW_FROM_ARG(X)            (X && X->p ? ((GSList*)X->p)->data : dwb.state.fview->data)
#define WEBVIEW_FROM_ARG(arg)       (WEBKIT_WEB_VIEW(((View*)(arg && arg->p ? ((GSList*)arg->p)->data : dwb.state.fview->data))->web))
#define CLEAR_COMMAND_TEXT(X)       dwb_set_status_bar_text(VIEW(X)->lstatus, NULL, NULL, NULL, false)

#define CURRENT_URL()               webkit_web_view_get_uri(CURRENT_WEBVIEW())

#define FOO                         puts("bar")

#define IS_WORD_CHAR(c)           (isalnum(c) || ((c) == '_')) 
// compare string a and b, without newline in string b

#define STRCMP_SKIP_NEWLINE(a, b)   (strncmp((a), (b), strstr((b), "\n") - (b)))
#define STRCMP_FIRST_WORD(a, b)     (strncmp((a), (b), MAX(strstr((a), " ") - a, strstr((b), " ") - b)))

#define FREE(X)                     if ((X)) g_free((X))

#define ALPHA(X)    ((X->keyval >= GDK_KEY_A && X->keyval <= GDK_KEY_Z) ||  (X->keyval >= GDK_KEY_a && X->keyval <= GDK_KEY_z) || X->keyval == GDK_KEY_space)
#define DIGIT(X)   (X->keyval >= GDK_KEY_0 && X->keyval <= GDK_KEY_9)
#define DWB_TAB_KEY(e)              (e->keyval == GDK_KEY_Tab || e->keyval == GDK_KEY_ISO_Left_Tab)

// Settings
#define GET_CHAR(prop)              ((char*)(((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.p))
#define GET_BOOL(prop)              (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.b)
#define GET_INT(prop)               (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.i)
#define GET_DOUBLE(prop)            (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.d)
#define NUMMOD                      (dwb.state.nummod < 1 ? 1 : dwb.state.nummod)

#ifdef DWB_DEBUG
#define PRINT_DEBUG(...) do { \
    fprintf(stderr, "\n\033[31;1mDEBUG:\033[0m %s:%d:%s()\t", __FILE__, __LINE__, __func__); \
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n"); \
  } while(0);
#define DEBUG_TIMED(limit, code) do { \
  GTimer *__debug_timer = g_timer_new(); \
  for (int i=0; i<limit; i++) { (code); }\
  gulong __debug_micro = 0;\
  gdouble __debug_elapsed = g_timer_elapsed(__debug_timer, &__debug_micro);\
  PRINT_DEBUG("timer: \033[32m%s\033[0m: elapsed: %f, micro: %lu", #code, __debug_elapsed, __debug_micro);\
  g_timer_destroy(__debug_timer); \
} while(0);
GTimer *__timer;
#define TIMER_START do {__timer = g_timer_new();g_timer_start(__timer);puts("hallo timer");}while(0)
#define TIMER_END do{ gulong __debug_micro = 0; gdouble __debug_elapsed = g_timer_elapsed(__timer, &__debug_micro);\
  PRINT_DEBUG("\033[33mtimer:\033[0m elapsed: %f, micro: %lu", __debug_elapsed, __debug_micro);\
  g_timer_destroy(__timer); \
} while(0)

#else 
#define PRINT_DEBUG(message, ...) 
#define DEBUG_TIMED(limit, code)
#define TIMER_START
#define TIMER_END

#endif
#define BPKB 1024
#define BPMB 1048576
#define BPGB 1073741824



/*}}}*/


/* TYPES {{{*/

typedef struct _Arg Arg;
typedef struct _Color Color;
typedef struct _Completions Completions;
typedef struct _Dwb Dwb;
typedef struct _FileContent FileContent;
typedef struct _Files Files;
typedef struct _Font DwbFont;
typedef struct _FunctionMap FunctionMap;
typedef struct _Gui Gui;
typedef struct _Key Key;
typedef struct _KeyMap KeyMap;
typedef struct _KeyValue KeyValue;
typedef struct _Misc Misc;
typedef struct _Navigation Navigation;
typedef struct _Plugins Plugins;
typedef struct _Quickmark Quickmark;
typedef struct _Settings Settings;
typedef struct _State State;
typedef struct _View View;
typedef struct _ViewStatus ViewStatus;
typedef struct _WebSettings WebSettings;
/*}}}*/
typedef gboolean (*Command_f)(void*);
typedef gboolean (*Func)(void *, void*);
typedef void (*void_func)(void*);
typedef void (*S_Func)(void *, WebSettings *);
typedef void *(*Content_Func)(const char *);

typedef enum  {
  COMP_NONE         = 0x00,
  COMP_BOOKMARK     = 0x01,
  COMP_HISTORY      = 0x02,
  COMP_SETTINGS     = 0x03,
  COMP_KEY          = 0x04,
  COMP_COMMAND      = 0x05,
  COMP_USERSCRIPT   = 0x06,
  COMP_INPUT        = 0x07,
  COMP_SEARCH       = 0x08,
  COMP_PATH         = 0x09,
  COMP_CUR_HISTORY  = 0x0a,
  COMP_BUFFER       = 0x0b,
} CompletionType;

typedef enum {
  PLUGIN_STATUS_DISABLED      = 1<<0,
  PLUGIN_STATUS_ENABLED       = 1<<1,
  PLUGIN_STATUS_CONNECTED     = 1<<2,
  PLUGIN_STATUS_DISCONNECTED  = 1<<3,
  PLUGIN_STATUS_HAS_PLUGIN    = 1<<4,
} PluginBlockerStatus;

typedef enum {
  HINT_T_ALL = 0,
  HINT_T_LINKS = 1,
  HINT_T_IMAGES = 2,
  HINT_T_EDITABLE = 3,
  HINT_T_URL = 4,
  HINT_T_CLIPBOARD = 5,
  HINT_T_PRIMARY = 6,
  HINT_T_BOOKMARK = 7,
} HintType;
typedef enum {
  HIDE_TB_NEVER     = 0x02,
  HIDE_TB_ALWAYS    = 0x03,
  HIDE_TB_TILED     = 0x05,
} TabBarVisible;

typedef enum {
  NORMAL_MODE           = 1<<0,
  INSERT_MODE           = 1<<1,
  QUICK_MARK_SAVE       = 1<<3,
  QUICK_MARK_OPEN       = 1<<4 ,
  HINT_MODE             = 1<<5,
  FIND_MODE             = 1<<6,
  COMPLETION_MODE       = 1<<7,
  AUTO_COMPLETE         = 1<<8,
  COMMAND_MODE          = 1<<9,
  SEARCH_FIELD_MODE     = 1<<10,
  SETTINGS_MODE         = 1<<12,
  KEY_MODE              = 1<<13,
  DOWNLOAD_GET_PATH     = 1<<14,
  SAVE_SESSION          = 1<<15,
  COMPLETE_PATH         = 1<<16,
  COMPLETE_BUFFER       = 1<<17,
  PASS_THROUGH          = 1<<18,
} Mode;


typedef enum {
  NEVER_SM      = 0x00,
  ALWAYS_SM     = 0x01,
  POST_SM       = 0x02,
} ShowMessage;


typedef enum { 
  NORMAL_LAYOUT    = 0,
  BOTTOM_STACK     = 1<<0 ,
  MAXIMIZED        = 1<<1 ,
} Layout;

typedef enum { 
  CHAR        = 0x01,
  INTEGER     = 0x02,
  DOUBLE      = 0x03,
  BOOLEAN     = 0x04,
  COLOR_CHAR  = 0x05,
  HTML_STRING = 0x06,
} DwbType;

typedef enum { 
  DL_ACTION_DOWNLOAD  = 0x01,
  DL_ACTION_EXECUTE   = 0x02,
} DownloadAction;

typedef enum _DwbStatus {
  STATUS_OK, 
  STATUS_ERROR, 
  STATUS_END,
  STATUS_IGNORE, 
} DwbStatus;

#define APPEND  0x01
#define PREPEND  0x02

#define NO_ERROR 0 
#define ERROR -1

typedef enum {
  ALLOW_HOST  = 1<<0,
  ALLOW_URI   = 1<<1,
  ALLOW_TMP   = 1<<2,
} ScriptAllow;

typedef enum {
  SCRIPTS_ALLOWED           = 1<<0,
  SCRIPTS_BLOCKED           = 1<<1,
  SCRIPTS_ALLOWED_TEMPORARY = 1<<2,
} ScriptState;

typedef enum {
  OPEN_NORMAL      = 1<<0, 
  OPEN_NEW_VIEW    = 1<<1, 
  OPEN_NEW_WINDOW  = 1<<2, 
  OPEN_DOWNLOAD    = 1<<3, 
  SET_URL          = 1<<4, 
} Open;

enum Signal {
  SIG_FIRST = 0, 
  SIG_BUTTON_PRESS,
  SIG_BUTTON_RELEASE,
  SIG_CLOSE_WEB_VIEW, 
  SIG_CONSOLE_MESSAGE,
  SIG_CREATE_WEB_VIEW,
  SIG_DOWNLOAD_REQUESTED,
  SIG_HOVERING_OVER_LINK, 
  SIG_ICON_LOADED, 
  SIG_MIME_TYPE,
  SIG_NAVIGATION,
  SIG_NEW_WINDOW,
  SIG_CREATE_PLUGIN_WIDGET,
  SIG_RESOURCE_REQUEST,
  SIG_WINDOW_OBJECT,
  SIG_LOAD_STATUS,
  SIG_LOAD_ERROR,
  SIG_LOAD_STATUS_AFTER,
  SIG_PROGRESS,
  SIG_TITLE,
  SIG_URI,
  SIG_SCROLL,
  SIG_VALUE_CHANGED,
  SIG_ENTRY_KEY_PRESS,
  SIG_ENTRY_KEY_RELEASE,
  SIG_TAB_BUTTON_PRESS, 
  SIG_POPULATE_POPUP, 
#ifdef DWB_ADBLOCKER
  SIG_AD_LOAD_STATUS,
  SIG_AD_RESOURCE_REQUEST,
#endif

  SIG_PLUGINS_LOAD,
  SIG_PLUGINS_FRAME_LOAD,
  SIG_PLUGINS_CREATE_WIDGET,
  SIG_PLUGINS_LAST,

  SIG_KEY_PRESS,
  SIG_KEY_RELEASE,
  SIG_LAST,
};

enum _Direction {
  SCROLL_UP             = GDK_SCROLL_UP,
  SCROLL_DOWN           = GDK_SCROLL_DOWN,
  SCROLL_LEFT           = GDK_SCROLL_LEFT, 
  SCROLL_RIGHT          = GDK_SCROLL_RIGHT, 
  SCROLL_HALF_PAGE_UP,
  SCROLL_HALF_PAGE_DOWN,
  SCROLL_PAGE_UP,
  SCROLL_PAGE_DOWN, 
  SCROLL_TOP,
  SCROLL_BOTTOM,
  SCROLL_PERCENT,
  SCROLL_PIXEL,
} Direction;

typedef enum {
  SSL_NONE,
  SSL_TRUSTED, 
  SSL_UNTRUSTED,
} SslState;

typedef enum {
  CP_COMMANDLINE   = 1<<0,
  CP_DONT_SAVE     = 1<<1,
  CP_HAS_MODE      = 1<<2,
} CommandProperty;

/*}}}*/


/* STRUCTS {{{*/
struct _Navigation {
  char *first;
  char *second;
};
struct _Arg {
  guint n;
  int i;
  double d;
  gpointer p;
  gpointer arg;
  gboolean b;
  char *e;
};
struct _Key {
  char *str;
  guint mod;
};
struct _KeyValue {
  const char *id;
  Key key;
};


struct _FunctionMap {
  Navigation n;
  int prop; 
  Func func;
  const char *error; 
  ShowMessage hide;
  Arg arg;
  gboolean entry;
};
struct _KeyMap {
  const char *key;
  guint mod;
  FunctionMap *map;
};
struct _Quickmark {
  char *key; 
  Navigation *nav;
};
struct _Completions {
  GList *active_comp;
  GList *completions;
  GList *auto_c;
  GList *active_auto_c;
  gboolean autocompletion;
  GList *path_completion;
  GList *active_path;
};
struct _State {
  GList *views;
  GList *fview;
  WebKitWebSettings *web_settings;
  Mode mode;
  GString *buffer;
  int nummod;
  Open nv;
  DwbType type;
  HintType hint_type;
  guint scriptlock;
  int size;
  GHashTable *settings_hash;
  gboolean forward_search;
  gboolean background_tabs;

  SoupCookieJar *cookiejar;
  SoupCookie *last_cookie;
  GSList *last_cookies;
  gboolean cookies_allowed;

  gboolean complete_history;
  gboolean complete_bookmarks;
  gboolean complete_searchengines;
  gboolean complete_commands;
  gboolean complete_userscripts;

  gboolean hidden_files;
  gboolean view_in_background;

  Layout layout;
  GList *last_com_history;

  GList *undo_list;

  char *search_engine;
  char *form_name;

  WebKitDownload *download;
  DownloadAction dl_action;
  char *download_command;
  char *mimetype_request;

  TabBarVisible tabbar_visible;
  gboolean fullscreen;
};

typedef enum _SettingsApply {
  SETTING_BUILTIN   = 1<<0,
  SETTING_GLOBAL    = 1<<1,
  SETTING_ONINIT    = 1<<2,
  SETTING_PER_VIEW  = 1<<3,
} SettingsApply;
struct _WebSettings {
  Navigation n;
  SettingsApply apply;
  DwbType type;
  Arg arg;
  S_Func func;
};
struct _ViewStatus {
  guint message_id;
  gboolean add_history;
  char *search_string;
  GList *downloads;
  char *mimetype;
  gulong signals[SIG_LAST];
  int progress;
  SslState ssl;
  ScriptState scripts;
  int tab_height;
  char *hover_uri;
  GSList *allowed_plugins;
  PluginBlockerStatus pb_status;
#ifdef DWB_ADBLOCKER
  char *current_stylesheet;
#endif
};

struct _View {
  GtkWidget *vbox;
  GtkWidget *web;
  GtkWidget *tabevent;
  GtkWidget *tabbox;
  GtkWidget *tabicon;
  GtkWidget *tablabel;
  GtkWidget *statusbox;
  GtkWidget *urilabel;
  GtkWidget *rstatus;
  GtkWidget *lstatus;
  GtkWidget *scroll; 
  GtkWidget *entry;
  GtkWidget *autocompletion;
  GtkWidget *compbox;
  GtkWidget *bottombox;
  ViewStatus *status;
  GHashTable *setting;
};
struct _Color {
  DwbColor active_fg;
  DwbColor active_bg;
  DwbColor normal_fg;
  DwbColor normal_bg;
  DwbColor ssl_trusted;
  DwbColor ssl_untrusted;
  DwbColor tab_active_fg;
  DwbColor tab_active_bg;
  DwbColor tab_normal_fg;
  DwbColor tab_normal_bg;
  DwbColor error;
  DwbColor active_c_fg;
  DwbColor active_c_bg;
  DwbColor normal_c_fg;
  DwbColor normal_c_bg;
  DwbColor download_fg;
  DwbColor download_bg;
  char *settings_bg_color;
  char *settings_fg_color;
  char *tab_number_color;
  char *allow_color;
  char *block_color;
};
struct _Font {
  PangoFontDescription *fd_active;
  PangoFontDescription *fd_inactive;
  PangoFontDescription *fd_entry;
  PangoFontDescription *fd_completion;
};
struct _Setting {
  gboolean inc_search;
  gboolean wrap_search;
};

struct _Gui {
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *topbox;
  GtkWidget *paned;
  GtkWidget *right;
  GtkWidget *left;
  GtkWidget *entry;
  GtkWidget *downloadbar;
  int width;
  int height;
};
struct _Misc {
  const char *name;
  const char *prog_path;
  char *scripts;
  const char *profile;
  const char *default_search;
  SoupSession *soupsession;
  char *proxyuri;

  GIOChannel *si_channel;
  GList *userscripts;

  double factor;
  int max_c_items;
  int message_delay;
  int history_length;

  char *settings_border;
  int argc;
  char **argv;

  gboolean tabbed_browsing;
  gboolean private_browsing;

  double scroll_step;

  char *startpage;
  char *download_com;
  JSContextRef global_ctx;
  int tab_height;

  char *pbbackground;
  gboolean top_statusbar;
  int synctimer;
};
struct _Files {
  const char *bookmarks;
  const char *cookies;
  const char *cookies_allow;
  const char *download_path;
  const char *fifo;
  const char *history;
  const char *keys;
  const char *mimetypes;
  const char *quickmarks;
  const char *scriptdir;
  const char *searchengines;
  const char *session;
  const char *settings;
  const char *stylesheet;
  const char *unifile;
  const char *userscripts;
  const char *adblock;
#if 0
  const char *dir_icon;
  const char *file_icon;
  const char *exec_icon;
#endif
  const char *scripts_allow;
  const char *plugins_allow;
  const char *cachedir;
};
// TODO implement plugins blocker, script blocker with File struct
typedef struct _File {
  unsigned long int mod;
  char *filename; 
  GList *content;
} File;
struct _FileContent {
  GList *bookmarks;
  GList *history;
  GList *quickmarks;
  GList *searchengines;
  GList *se_completion;
  GList *keys;
  GList *settings;
  GList *cookies_allow;
  GList *commands;
  GList *mimetypes;
  GList *adblock;
  GList *tmp_scripts;
  GList *tmp_plugins;
  GList *scripts_allow;
};

struct _Dwb {
  Gui gui;
  Color color;
  DwbFont font;
  Misc misc;
  State state;
  Completions comps;
  GList *keymap;
  GHashTable *settings;
  Files files;
  FileContent fc;
  gpointer *instance;
};

/*}}}*/

/* VARIABLES {{{*/
Dwb dwb;
/*}}}*/

DwbStatus dwb_change_mode(Mode, ...);
void dwb_load_uri(GList *gl, const char *);
void dwb_execute_user_script(KeyMap *km, Arg *a);

void dwb_focus_entry(void);
void dwb_focus_scroll(GList *);

gboolean dwb_update_search(gboolean forward);

void dwb_set_normal_message(GList *, gboolean, const char *, ...);
void dwb_set_error_message(GList *, const char *, ...);
void dwb_set_status_text(GList *, const char *, DwbColor *,  PangoFontDescription *);
void dwb_set_status_bar_text(GtkWidget *, const char *, DwbColor *,  PangoFontDescription *, gboolean);
void dwb_update_status_text(GList *gl, GtkAdjustment *);
void dwb_update_status(GList *gl);
void dwb_update_layout(gboolean);
void dwb_unfocus(void);

DwbStatus dwb_prepend_navigation(GList *, GList **);
void dwb_prepend_navigation_with_argument(GList **, const char *, const char *);

Navigation * dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *);
gboolean dwb_update_hints(GdkEventKey *);
gboolean dwb_search(KeyMap *, Arg *);
void dwb_submit_searchengine(void);
void dwb_save_searchengine(void);
char * dwb_execute_script(WebKitWebFrame *, const char *, gboolean);
void dwb_resize(double );
void dwb_toggle_tabbar(void);
DwbStatus dwb_history_back(void);
DwbStatus dwb_history_forward(void);

void dwb_focus(GList *);
void dwb_source_remove(GList *);
gboolean dwb_spawn(GList *, const char *, const char *uri);

int dwb_entry_position_word_back(int position);
int dwb_entry_position_word_forward(int position);
void dwb_entry_set_text(const char *text);

void dwb_set_proxy(GList *, WebSettings *);

void dwb_set_single_instance(GList *, WebSettings *);
void dwb_new_window(const char *uri);

gboolean dwb_eval_editing_key(GdkEventKey *);
void dwb_parse_command_line(const char *);
GHashTable * dwb_get_default_settings(void);

int dwb_end(void);
Key dwb_str_to_key(char *);

GList * dwb_keymap_add(GList *, KeyValue );

void dwb_save_settings(void);
gboolean dwb_save_files(gboolean);
CompletionType dwb_eval_completion_type(void);

void dwb_append_navigation_with_argument(GList **, const char *, const char *);
void dwb_clean_load_end(GList *);
void dwb_update_uri(GList *);
gboolean dwb_get_allowed(const char *, const char *);
gboolean dwb_toggle_allowed(const char *, const char *);
char * dwb_get_host(WebKitWebView *);
void dwb_focus_view(GList *);
void dwb_clean_key_buffer(void);
void dwb_set_key(const char *, char *);
void dwb_set_setting(const char *, char *value);
DwbStatus dwb_open_startpage(GList *);
void dwb_init_scripts(void);
char * dwb_get_search_engine(const char *uri, gboolean);
char * dwb_get_stock_item_base64_encoded(const char *);
void dwb_remove_bookmark(const char *);
void dwb_remove_history(const char *);
void dwb_remove_quickmark(const char *);
DwbStatus dwb_evaluate_hints(const char *);

DwbStatus dwb_set_clipboard(const char *text, GdkAtom atom);
DwbStatus dwb_open_in_editor(void);

#endif
