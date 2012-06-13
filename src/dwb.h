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

#ifndef DWB_H
#define DWB_H
#include <gtk/gtk.h>
#include <webkit/webkit.h>
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
#define COPYRIGHT "© 2010-2012 portix"
#endif

#define PBAR_LENGTH   20
#define STRING_LENGTH 1024
#define BUFFER_LENGTH 256

#define INSERT "Insert Mode"
#define INSERT_MODE_STRING  "-- INSERT MODE --"

#define NO_URL                      "No URL in current context"
#define NO_HINTS                    "No Hints in current context"

#define HINT_SEARCH_SUBMIT "_dwb_search_submit_"

#if GTK_CHECK_VERSION(3, 0, 0)
#define _HAS_GTK3 1
#endif

#if _HAS_GTK3 
#include <gtk/gtkx.h>
#endif

#ifndef false 
#define false 0
#endif
#ifndef true
#define true !false
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

//#define CLEAN_STATE_WITH_SHIFT(X) (X->state & ~(GDK_LOCK_MASK) & ~(GDK_MOD2_MASK) &~(GDK_MOD3_MASK) & ~(GDK_MOD5_MASK) & ~(GDK_SUPER_MASK) & ~(GDK_HYPER_MASK) & ~(GDK_META_MASK))

#define CLEAN_STATE_WITH_SHIFT(X) ((X)->state & (GDK_MOD1_MASK|GDK_MOD4_MASK|\
      GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK|GDK_BUTTON4_MASK|GDK_BUTTON5_MASK|\
      GDK_SHIFT_MASK|GDK_CONTROL_MASK ))
//#define CLEAN_STATE(X) (CLEAN_STATE_WITH_SHIFT(X) & ~(GDK_SHIFT_MASK))
#define CLEAN_STATE(X) ((X)->state & (GDK_MOD1_MASK|GDK_MOD4_MASK|\
      GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK|GDK_BUTTON4_MASK|GDK_BUTTON5_MASK|\
      GDK_CONTROL_MASK ))
#define CLEAN_SHIFT(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_LOCK_MASK))
#define CLEAN_COMP_MODE(X)          (X & ~(COMPLETION_MODE) & ~(AUTO_COMPLETE))
#define CLEAN_MODE(mode)            ((mode) & ~(COMPLETION_MODE))
/* Maybe this has to be changed in future releases */
#define DWB_NUMMOD_MASK                 (1<<15)

#define GET_TEXT()                  (gtk_entry_get_text(GTK_ENTRY(dwb.gui.entry)))
#define CURRENT_VIEW()              ((View*)dwb.state.fview->data)
#define VIEW(X)                     ((View*)X->data)
#define WEBVIEW(X)                  (WEBKIT_WEB_VIEW(((View*)X->data)->web))
#define CURRENT_WEBVIEW_WIDGET()    (((View*)dwb.state.fview->data)->web)
#define CURRENT_WEBVIEW()           (WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web))
#define MAIN_FRAME()                (webkit_web_view_get_main_frame(CURRENT_WEBVIEW()))  
#define MAIN_FRAME_CAST(X)                (webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(X)))  
#define FOCUSED_FRAME()             (webkit_web_view_get_focused_frame(CURRENT_WEBVIEW()))  
#define VIEW_FROM_ARG(X)            (X && X->p ? ((GSList*)X->p)->data : dwb.state.fview->data)
#define WEBVIEW_FROM_ARG(arg)       (WEBKIT_WEB_VIEW(((View*)(arg && arg->p ? ((GSList*)arg->p)->data : dwb.state.fview->data))->web))
#define CLEAR_COMMAND_TEXT()        dwb_set_status_bar_text(dwb.gui.lstatus, NULL, NULL, NULL, false)
#define BOOLEAN(X)                  (!(!(X)))
#define NAVIGATION(X)               ((Navigation*)((X)->data))
#define JS_CONTEXT_REF(X)            (webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(WEBVIEW(gl))))

#define CURRENT_URL()               webkit_web_view_get_uri(CURRENT_WEBVIEW())

#define FOO                         puts("bar")

#define IS_WORD_CHAR(c)           (isalnum((int)c) || ((c) == '_')) 

/* compare string a and b, without newline in string b */
#define STRCMP_SKIP_NEWLINE(a, b)   (strncmp((a), (b), strstr((b), "\n") - (b)))
#define STRCMP_FIRST_WORD(a, b)     (strncmp((a), (b), MAX(strstr((a), " ") - a, strstr((b), " ") - b)))

#define FREE0(X)                     ((X == NULL) ? NULL : (X = (g_free(X), NULL)))

#define ALPHA(X)    ((X->keyval >= GDK_KEY_A && X->keyval <= GDK_KEY_Z) ||  (X->keyval >= GDK_KEY_a && X->keyval <= GDK_KEY_z) || X->keyval == GDK_KEY_space)
#define DIGIT(X)   (X->keyval >= GDK_KEY_0 && X->keyval <= GDK_KEY_9)
#define DWB_TAB_KEY(e)              (e->keyval == GDK_KEY_Tab || e->keyval == GDK_KEY_ISO_Left_Tab)
#define DWB_COMPLETE_KEY(e)         (DWB_TAB_KEY(e) || e->keyval == GDK_KEY_Down || e->keyval == GDK_KEY_Up)

// Settings
#define GET_CHAR(prop)              ((char*)(((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg_local.p))
#define GET_BOOL(prop)              (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg_local.b)
#define GET_INT(prop)               (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg_local.i)
#define GET_DOUBLE(prop)            (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg_local.d)
#define NUMMOD                      (dwb.state.nummod < 0 ? 1 : dwb.state.nummod)

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
#define TIMER_START do {__timer = g_timer_new();g_timer_start(__timer);}while(0)
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

#define DEFAULT_WIDGET_PACKING "dtws"
#define WIDGET_PACK_LENGTH 4

/*}}}*/

typedef enum _DwbStatus {
  STATUS_OK, 
  STATUS_ERROR, 
  STATUS_END,
  STATUS_IGNORE, 
} DwbStatus;


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
typedef DwbStatus (*S_Func)(void *, WebSettings *);
typedef void *(*Content_Func)(void *);

typedef enum  {
  COMP_NONE         = 1,
  COMP_BOOKMARK,
  COMP_HISTORY,
  COMP_SETTINGS,
  COMP_KEY,
  COMP_COMMAND,
  COMP_USERSCRIPT,
  COMP_SEARCH,
  COMP_PATH,
  COMP_CUR_HISTORY,
  COMP_BUFFER,
  COMP_QUICKMARK,
  COMP_SCRIPT,
} CompletionType;

typedef enum  {
  SANITIZE_ERROR  = -1,
  SANITIZE_HISTORY        = 1<<0, 
  SANITIZE_COOKIES        = 1<<1, 
  SANITIZE_CACHE          = 1<<2, 
  SANITIZE_SESSION        = 1<<3, 
  SANITIZE_ALLSESSIONS    = 1<<4, 
} Sanitize;
#define SANITIZE_ALL (SANITIZE_HISTORY | SANITIZE_CACHE | SANITIZE_COOKIES | SANITIZE_ALLSESSIONS)

enum SetSetting {
  SET_GLOBAL, 
  SET_LOCAL, 
};
enum FindFlags {
  FIND_FORWARD        = 1<<0, 
  FIND_CASE_SENSITIVE = 1<<1,
  FIND_WRAP           = 1<<2,
};
enum {
  EP_NONE = 0,
  EP_COMP_DEFAULT = 1<<1,
  EP_COMP_QUICKMARK = 1<<2,
} EntryProp;
#define EP_COMPLETION (EP_COMP_DEFAULT | EP_COMP_QUICKMARK)

typedef enum _TabMoveDirection {
  TAB_MOVE_NONE,
  TAB_MOVE_LEFT,
  TAB_MOVE_RIGHT,

} TabMoveDirection;
typedef enum _TabPosition {
  TAB_POSITION_LEFT               = 1<<0, 
  TAB_POSITION_RIGHT              = 1<<1, 
  TAB_POSITION_LEFTMOST           = 1<<2,  
  TAB_POSITION_RIGHTMOST          = 1<<3, 
  TAB_CLOSE_POSITION_LEFT         = 1<<4,
  TAB_CLOSE_POSITION_RIGHT        = 1<<5,
  TAB_CLOSE_POSITION_LEFTMOST     = 1<<6,
  TAB_CLOSE_POSITION_RIGHTMOST    = 1<<7,
} TabPosition;
#define NEW_TAB_POSITION_MASK   (TAB_POSITION_LEFT | TAB_POSITION_RIGHT | TAB_POSITION_LEFTMOST | TAB_POSITION_RIGHTMOST) 
#define CLOSE_TAB_POSITION_MASK (TAB_CLOSE_POSITION_LEFT | TAB_CLOSE_POSITION_RIGHT | TAB_CLOSE_POSITION_LEFTMOST | TAB_CLOSE_POSITION_RIGHTMOST) 

typedef enum {
  PLUGIN_STATUS_DISABLED      = 1<<0,
  PLUGIN_STATUS_ENABLED       = 1<<1,
  PLUGIN_STATUS_CONNECTED     = 1<<2,
  PLUGIN_STATUS_DISCONNECTED  = 1<<3,
  PLUGIN_STATUS_HAS_PLUGIN    = 1<<4,
} PluginBlockerStatus;

typedef enum {
  LP_PROTECT          = 1<<0,
  LP_LOCK_DOMAIN      = 1<<1,
  LP_LOCK_URI         = 1<<2,
  LP_VISIBLE          = 1<<3,
} LockProtect;
#define LP_PROTECTED(v) ((v)->status->lockprotect & LP_PROTECT)
#define LP_LOCKED_DOMAIN(v) ((v)->status->lockprotect & LP_LOCK_DOMAIN)
#define LP_LOCKED_URI(v) ((v)->status->lockprotect & LP_LOCK_URI)
#define LP_VISIBLE(v) ((v)->status->lockprotect & LP_VISIBLE)
#define LP_STATUS(v)   ((v)->status->lockprotect & (LP_LOCK_DOMAIN | LP_LOCK_URI))

typedef enum {
  HINT_T_ALL        = 0,
  HINT_T_LINKS      = 1,
  HINT_T_IMAGES     = 2,
  HINT_T_EDITABLE   = 3,
  HINT_T_URL        = 4,
  HINT_T_CLIPBOARD  = 5,
  HINT_T_PRIMARY    = 6,
  HINT_T_RAPID      = 7,
  HINT_T_RAPID_NW   = 8,
} HintType;
#define HINT_NOT_RAPID (dwb.state.hint_type != HINT_T_RAPID && dwb.state.hint_type != HINT_T_RAPID_NW)

typedef enum {
  BAR_VIS_TOP = 1<<0,
  BAR_VIS_STATUS = 1<<1,
  BAR_PRESENTATION = 1<<2,
} BarVisibility;

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
  COMPLETE_QUICKMARKS   = 1<<18,
  COMPLETE_COMMAND_MODE = 1<<19,
  CONFIRM               = 1<<21,
  SETTINGS_MODE_LOCAL   = 1<<22,
  COMPLETE_SCRIPTS      = 1<<23,
} Mode;


typedef enum {
  NEVER_SM      = 0x00,
  ALWAYS_SM     = 0x01,
  POST_SM       = 0x02,
} ShowMessage;

typedef enum { 
  CHAR        = 0x01,
  INTEGER     = 0x02,
  DOUBLE      = 0x03,
  BOOLEAN     = 0x04,
  COLOR_CHAR  = 0x05,
  HTML_STRING = 0x06,
  ULONG       = 0x07,
  LONG        = 0x08,
  UINTEGER    = 0x09,
} DwbType;

typedef enum { 
  DL_ACTION_DOWNLOAD  = 0x01,
  DL_ACTION_EXECUTE   = 0x02,
} DownloadAction;

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
  OPEN_VIA_HINTS   = 1<<5,
  OPEN_EXPLICIT    = 1<<6,
  OPEN_BACKGROUND  = 1<<7,
} Open;

enum {
  CA_TITLE,
  CA_URI,
} ClipboardAction;

typedef enum {
  COOKIE_STORE_SESSION,
  COOKIE_STORE_PERSISTENT,
  COOKIE_STORE_NEVER,
  COOKIE_ALLOW_SESSION,
  COOKIE_ALLOW_SESSION_TMP,
  COOKIE_ALLOW_PERSISTENT,
} CookieStorePolicy;


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
#if WEBKIT_CHECK_VERSION(1, 8, 0) 
  SIG_DOCUMENT_FINISHED,
#endif
  SIG_WINDOW_OBJECT,
  SIG_LOAD_STATUS,
  SIG_LOAD_ERROR,
  SIG_LOAD_STATUS_AFTER,
  SIG_MAIN_FRAME_COMMITTED,
  SIG_PROGRESS,
  SIG_TITLE,
  SIG_URI,
  SIG_SCROLL,
  SIG_VALUE_CHANGED,
  SIG_ENTRY_KEY_PRESS,
  SIG_ENTRY_KEY_RELEASE,
  SIG_ENTRY_INSERT_TEXT,
  SIG_TAB_BUTTON_PRESS, 
  SIG_POPULATE_POPUP, 
  SIG_FRAME_CREATED, 
  SIG_AD_LOAD_STATUS,
  SIG_AD_FRAME_CREATED,
  SIG_AD_RESOURCE_REQUEST,

  SIG_PLUGINS_LOAD,
  SIG_PLUGINS_FRAME_LOAD,
  SIG_PLUGINS_CREATE_WIDGET,
  SIG_PLUGINS_LAST,

  SIG_KEY_PRESS,
  SIG_KEY_RELEASE,
  SIG_LAST,
};

typedef enum {
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
} ScrollDirection;

typedef enum {
  SSL_NONE,
  SSL_TRUSTED, 
  SSL_UNTRUSTED,
} SslState;

typedef enum {
  CP_COMMANDLINE          = 1<<0,
  CP_DONT_SAVE            = 1<<1,
  CP_HAS_MODE             = 1<<2,
  CP_USERSCRIPT           = 1<<3,
  CP_DONT_CLEAN           = 1<<4,
  CP_OVERRIDE_INSERT      = 1<<5,
  CP_OVERRIDE_ENTRY       = 1<<6,
  CP_OVERRIDE_ALL         = 1<<7,
  CP_NEEDS_ARG            = 1<<8,
  CP_SCRIPT               = 1<<9,
} CommandProperty;
#define CP_OVERRIDE  (CP_OVERRIDE_INSERT | CP_OVERRIDE_ENTRY | CP_OVERRIDE_ALL)

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
  gboolean ro;
};
struct _Key {
  char *str;
  guint mod;
  int num;
};
struct _KeyValue {
  const char *id;
  Key key;
};
typedef struct _CustomCommand {
  Key *key;
  char **commands;
} CustomCommand;


struct _FunctionMap {
  Navigation n;
  int prop; 
  Func func;
  const char *error; 
  ShowMessage hide;
  Arg arg;
  unsigned int entry;
  const char *alias[5];
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
  GList *view;
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
  GHashTable *settings_hash;
  int search_flags;
  gboolean background_tabs;

  SoupCookieJar *cookiejar;
  CookieStorePolicy cookie_store_policy;

  gboolean hidden_files;
  gboolean view_in_background;

  GList *last_com_history;
  GList *last_nav_history;

  GList *undo_list;

  char *search_engine;
  char *form_name;

  WebKitDownload *download;
  DownloadAction dl_action;
  char *download_command;
  char *mimetype_request;
  int download_ref_count;

  guint message_id;

  gboolean fullscreen;
  unsigned int bar_visible;
  gboolean auto_insert_mode;
  GList *script_completion;
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
  Arg arg_local;
};
struct _Plugins {
  GSList *elements;
  GSList *clicks;
  int created;
  int max;
  PluginBlockerStatus status;
};
struct _ViewStatus {
  gboolean add_history;
  char *search_string;
  GList *downloads;
  gulong signals[SIG_LAST];
  int progress;
  SslState ssl;
  ScriptState scripts;
  char *hover_uri;
#ifdef WITH_LIBSOUP_2_38
  char *request_uri;
#endif
  GSList *allowed_plugins;
  unsigned int lockprotect;
  WebKitDOMElement *style;
  GSList *frames;
};
struct _View {
  GtkWidget *web;
  GtkWidget *tabevent;
  GtkWidget *tabbox;
  GtkWidget *tabicon;
  GtkWidget *tablabel;
  GtkWidget *scroll; 
  GtkWidget *inspector_window;
  ViewStatus *status;
  Plugins *plugins;
  struct {
    WebKitDOMElement *element;
    WebKitDOMElement *anchor;
  } hover;
  WebKitDOMElement *status_element;
  JSObjectRef hint_object;
  JSObjectRef script_wv;
};
struct _Color {
  DwbColor active_fg;
  DwbColor active_bg;
  DwbColor ssl_trusted;
  DwbColor ssl_untrusted;
  DwbColor tab_active_fg;
  DwbColor tab_active_bg;
  DwbColor tab_normal_fg1;
  DwbColor tab_normal_bg1;
  DwbColor tab_normal_fg2;
  DwbColor tab_normal_bg2;
  DwbColor error;
  DwbColor prompt;
  DwbColor active_c_fg;
  DwbColor active_c_bg;
  DwbColor normal_c_fg;
  DwbColor normal_c_bg;
  DwbColor download_fg;
  DwbColor download_bg;
  DwbColor download_start;
  DwbColor download_end;
  char *tab_number_color;
  char *tab_protected_color;
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
  GtkWidget *mainbox;
  GtkWidget *downloadbar;
  /* Statusbar */
  GtkWidget *statusbox;
  GtkWidget *urilabel;
  GtkWidget *rstatus;
  GtkWidget *lstatus;
  GtkWidget *entry;
  GtkWidget *autocompletion;
  GtkWidget *compbox;
  GtkWidget *bottombox;

  int width;
  int height;
  guint wid;
};
struct _Misc {
  const char *name;
  const char *prog_path;
  /* applied to the mainframe */
  char *scripts;
  char *scripts_onload;
  char *hints;
  /* applied to all frames */
  const char *profile;
  const char *default_search;
  gint find_delay;
  SoupSession *soupsession;
  char *proxyuri;
#ifdef WITH_LIBSOUP_2_38
  gboolean dns_lookup;
#endif

  GIOChannel *si_channel;
  GList *userscripts;

  int max_c_items;
  int message_delay;
  int tabbar_delay;
  int history_length;

  char *settings_border;

  gboolean tabbed_browsing;
  gboolean private_browsing;

  double scroll_step;

  char *startpage;
  char *download_com;
  JSContextRef global_ctx;

  char *pbbackground;
  int synctimer;
  int sync_interval;
  int bar_height;
  TabPosition tab_position;
  char *hint_style;
  int script_signals;

};
struct _Files {
  const char *bookmarks;
  const char *navigation_history;
  const char *command_history;
  const char *cookies;
  const char *cookies_allow;
  const char *cookies_session_allow;
  const char *download_path;
  const char *history;
  const char *keys;
  const char *mimetypes;
  const char *quickmarks;
  const char *searchengines;
  const char *session;
  const char *settings;
  const char *userscripts;
  const char *scripts_allow;
  const char *plugins_allow;
  const char *cachedir;
  const char *custom_keys;
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
  GList *cookies_session_allow;
  GList *navigations;
  GList *commands;
  GList *mimetypes;
  GList *adblock;
  GList *tmp_scripts;
  GList *tmp_plugins;
  GList *pers_scripts; 
  GList *pers_plugins; 
  GList *downloads;
};

struct _Dwb {
  Gui gui;
  Color color;
  DwbFont font;
  Misc misc;
  State state;
  Completions comps;
  GList *keymap;
  GList *override_keys;
  GSList *custom_commands;
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

void dwb_focus_scroll(GList *);

gboolean dwb_update_search(void);

void dwb_set_normal_message(GList *, gboolean, const char *, ...);
void dwb_set_error_message(GList *gl, const char *, ...);
gboolean dwb_confirm(GList *, char *, ...);
void dwb_set_status_text(GList *, const char *, DwbColor *,  PangoFontDescription *);
void dwb_tab_label_set_text(GList *, const char *);
void dwb_set_status_bar_text(GtkWidget *, const char *, DwbColor *,  PangoFontDescription *, gboolean);
void dwb_update_status_text(GList *gl, GtkAdjustment *);
void dwb_update_status(GList *gl);
void dwb_unfocus(void);

DwbStatus dwb_prepend_navigation(GList *, GList **);
void dwb_prepend_navigation_with_argument(GList **, const char *, const char *);
void dwb_glist_prepend_unique(GList **, char *);

Navigation * dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *);
gboolean dwb_update_hints(GdkEventKey *);
DwbStatus dwb_show_hints(Arg *);
gboolean dwb_search(Arg *);
void dwb_submit_searchengine(void);
void dwb_save_searchengine(void);
char * dwb_execute_script(WebKitWebFrame *, const char *, gboolean);
void dwb_toggle_tabbar(void);
DwbStatus dwb_history(Arg *a);
void dwb_reload(GList *gl);
DwbStatus dwb_history_back(void);
DwbStatus dwb_history_forward(void);
void dwb_scroll(GList *, double, ScrollDirection);

void dwb_focus(GList *);
void dwb_source_remove();
gboolean dwb_spawn(GList *, const char *, const char *uri);

DwbStatus dwb_set_proxy(GList *, WebSettings *);

void dwb_new_window(const char *uri);

gboolean dwb_eval_editing_key(GdkEventKey *);
DwbStatus dwb_parse_command_line(const char *);

int dwb_end(void);
Key dwb_str_to_key(char *);

GList * dwb_keymap_add(GList *, KeyValue );

void dwb_save_settings(void);
gboolean dwb_save_files(gboolean);
CompletionType dwb_eval_completion_type(void);

void dwb_append_navigation_with_argument(GList **, const char *, const char *);
void dwb_clean_load_end(GList *);
void dwb_clean_load_begin(GList *);
void dwb_update_uri(GList *);
gboolean dwb_get_allowed(const char *, const char *);
gboolean dwb_toggle_allowed(const char *, const char *, GList **);
char * dwb_get_host(WebKitWebView *);
gboolean dwb_focus_view(GList *);
void dwb_clean_key_buffer(void);
void dwb_set_key(const char *, char *);
DwbStatus dwb_set_setting(const char *, char *value, int);
DwbStatus dwb_toggle_setting(const char *, int );
DwbStatus dwb_open_startpage(GList *);
void dwb_init_scripts(void);
void dwb_reload_userscripts(void);
char * dwb_get_searchengine(const char *uri);
char * dwb_get_stock_item_base64_encoded(const char *);
void dwb_remove_bookmark(const char *);
void dwb_remove_download(const char *);
void dwb_remove_history(const char *);
void dwb_remove_quickmark(const char *);
void dwb_remove_search_engine(const char *);
DwbStatus dwb_evaluate_hints(const char *);
void dwb_set_open_mode(Open);

DwbStatus dwb_set_clipboard(const char *text, GdkAtom atom);
char * dwb_clipboard_get_text(GdkAtom atom);
DwbStatus dwb_open_in_editor(void);
gboolean dwb_confirm(GList *gl, char *prompt, ...);

void dwb_save_quickmark(const char *);
void dwb_open_quickmark(const char *);
gboolean dwb_update_find_quickmark(const char *text);

gboolean dwb_entry_activate(GdkEventKey *e);
void dwb_set_adblock(GList *, WebSettings *);

gboolean dwb_eval_key(GdkEventKey *);
gboolean dwb_eval_override_key(GdkEventKey *e, CommandProperty prop);
char * dwb_get_key(GdkEventKey *, unsigned int *, gboolean *);
void dwb_follow_selection(void);
void dwb_update_layout(void);
const char * dwb_parse_nummod(const char *);
void dwb_init_custom_keys(gboolean);
void dwb_update_tabs(void);
void dwb_setup_environment(GSList *);
void dwb_check_auto_insert(GList *);
void dwb_version();
DwbStatus dwb_pack(const char *layout, gboolean rebuild);
void dwb_init_signals(void);
void dwb_parse_commands(const char *line);
DwbStatus dwb_scheme_handler(GList *gl, WebKitNetworkRequest *request);
GList *dwb_get_simple_list(GList *, const char *filename);
const char * dwb_prompt(gboolean visibility, char *prompt, ...);

gboolean dwb_dom_remove_from_parent(WebKitDOMNode *node, GError **error);
char * dwb_get_raw_data(GList *gl);

void dwb_free_list(GList *list, void (*func)(void*));

#endif
