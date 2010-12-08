#ifndef DWB_H
#define DWB_H
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define NAME "dwb"
/* SETTINGS MAKROS {{{*/
#define KEY_SETTINGS "Dwb Key Settings"
#define SETTINGS "Dwb Settings"

#define SINGLE_INSTANCE 1
#define NEW_INSTANCE 2

#define STRING_LENGTH 256

// SETTTINGS_VIEW %s: bg-color  %s: fg-color %s: border
#define SETTINGS_VIEW "<head>\n<style type=\"text/css\">\n \
  body { background-color: %s; color: %s; font: fantasy; font-size:16; font-weight: bold; text-align:center; }\n\
.line { border: %s; vertical-align: middle; }\n \
.text { float: left; font-variant: normal; font-size: 14;}\n \
.key { text-align: right;  font-size: 12; }\n \
.active { background-color: #660000; }\n \
h2 { font-variant: small-caps; }\n \
.alignCenter { margin-left: 25%%; width: 50%%; }\n \
</style>\n \
<script type=\"text/javascript\">\n  \
function get_value(e) { value = e.value ? e.id + \" \" + e.value : e.id; console.log(value); e.blur(); } \
</script>\n<noscript>Enable scripts to add settings!</noscript>\n</head>\n"
#define HTML_H2  "<h2>%s -- Profile: %s</h2>"

#define HTML_BODY_START "<body>\n"
#define HTML_BODY_END "</body>\n"
#define HTML_FORM_START "<div class=\"alignCenter\">\n <form onsubmit=\"return false\">\n"
#define HTML_FORM_END "</form>\n</div>\n"
#define HTML_DIV_START "<div class=\"line\">\n"
#define HTML_DIV_KEYS_TEXT "<div class=\"text\">%s</div>\n "
#define HTML_DIV_KEYS_VALUE "<div class=\"key\">\n <input onchange=\"get_value(this)\" id=\"%s\" value=\"%s %s\"/>\n</div>\n"
#define HTML_DIV_SETTINGS_VALUE "<div class=\"key\">\n <input onchange=\"get_value(this);\" id=\"%s\" value=\"%s\"/>\n</div>\n"
#define HTML_DIV_SETTINGS_CHECKBOX "<div class=\"key\"\n <input id=\"%s\" type=\"checkbox\" onchange=\"get_value(this);\" %s>\n</div>\n"
#define HTML_DIV_END "</div>\n"
/*}}}*/
#define INSERT_MODE "Insert Mode"

#define NO_URL                      "No URL in current context"
#define NO_HINTS                    "No Hints in current context"

#define HINT_SEARCH_SUBMIT "_dwb_search_submit_"

/* MAKROS {{{*/ 
#define LENGTH(X)   (sizeof(X)/sizeof(X[0]))
#define GLENGTH(X)  (sizeof(X)/g_array_get_element_size(X)) 
#define NN(X)       ( ((X) == 0) ? 1 : (X) )

#define CLEAN_STATE(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_BUTTON1_MASK) & ~(GDK_BUTTON2_MASK) & ~(GDK_BUTTON3_MASK) & ~(GDK_BUTTON4_MASK) & ~(GDK_BUTTON5_MASK) & ~(GDK_LOCK_MASK) & ~(GDK_MOD2_MASK) &~(GDK_MOD3_MASK) & ~(GDK_MOD5_MASK))
#define CLEAN_SHIFT(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_LOCK_MASK))
#define CLEAN_COMP_MODE(X)          (X & ~(CompletionMode) & ~(AutoComplete))

#define GET_TEXT()                  (gtk_entry_get_text(GTK_ENTRY(dwb.gui.entry)))
#define CURRENT_VIEW()              ((View*)dwb.state.fview->data)
#define VIEW(X)                     ((View*)X->data)
#define WEBVIEW(X)                  (WEBKIT_WEB_VIEW(((View*)X->data)->web))
#define CURRENT_WEBVIEW()           (WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web))
#define VIEW_FROM_ARG(X)            (X && X->p ? ((GSList*)X->p)->data : dwb.state.fview->data)
#define WEBVIEW_FROM_ARG(arg)       (WEBKIT_WEB_VIEW(((View*)(arg && arg->p ? ((GSList*)arg->p)->data : dwb.state.fview->data))->web))
#define CLEAR_COMMAND_TEXT(X)       dwb_set_status_bar_text(VIEW(X)->lstatus, NULL, NULL, NULL)

#define DIGIT(X)   (X->keyval >= GDK_0 && X->keyval <= GDK_9)
#define ALPHA(X)    ((X->keyval >= GDK_A && X->keyval <= GDK_Z) ||  (X->keyval >= GDK_a && X->keyval <= GDK_z) || X->keyval == GDK_space)

#define DWB_TAB_KEY(e)              (e->keyval == GDK_Tab || e->keyval == GDK_ISO_Left_Tab)

// Settings
#define GET_CHAR(prop)              ((char*)(((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.p))
#define GET_BOOL(prop)              (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.b)
#define GET_INT(prop)               (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.i)
#define GET_DOUBLE(prop)            (((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.d)
/*}}}*/

/* TYPES {{{*/

typedef struct _Arg Arg;
typedef struct _Misc Misc;
typedef struct _Dwb Dwb;
typedef struct _Gui Gui;
typedef struct _State State;
typedef struct _Completions Completions;
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
/*}}}*/
typedef gboolean (*Command_f)(void*);
typedef gboolean (*Func)(void*);
typedef void (*S_Func)(void *, WebSettings *);
typedef void *(*Content_Func)(const char *);

/* ENUMS {{{*/
enum _Mode {
  NormalMode          = 1<<0,
  InsertMode          = 1<<1,
  OpenMode            = 1<<2,
  QuickmarkSave       = 1<<3,
  QuickmarkOpen       = 1<<4, 
  HintMode            = 1<<5,
  FindMode            = 1<<6,
  CompletionMode      = 1<<7,
  AutoComplete        = 1<<8,
  CommandMode         = 1<<9,
  SearchFieldMode     = 1<<10,
  SearchKeywordMode   = 1<<11,
  SettingsMode        = 1<<12,
  KeyMode             = 1<<13,
  DownloadGetPath     = 1<<14,
  SaveSession         = 1<<15,
  BookmarksMode       = 1<<16,
};

enum _ShowMessage {
  NeverSM = 0, 
  AlwaysSM, 
  PostSM,
};

enum _Open {
  OpenNormal = 0, 
  OpenNewView,
  OpenNewWindow,
  OpenDownload,
};

enum _Layout {
  NormalLayout = 0,
  BottomStack = 1<<0, 
  Maximized = 1<<1, 
};

enum _Direction {
  Up = GDK_SCROLL_UP,
  Down = GDK_SCROLL_DOWN,
  Left = GDK_SCROLL_LEFT, 
  Right = GDK_SCROLL_RIGHT, 
  PageUp,
  PageDown, 
  Top,
  Bottom,
};
enum _DwbType {
  Char, 
  Integer,
  Double,
  Boolean, 
  Pointer, 
  ColorChar,
};
enum _SettingsScope {
  Global,
  PerView,
};
enum _DownloadAction  {
  Download,
  Execute,
};
typedef enum _DownloadAction DownloadAction;
/*}}}*/

typedef enum _Mode Mode;
typedef enum _Open Open;
typedef enum _Layout Layout;
typedef enum _Direction Direction;
typedef enum _DwbType DwbType;
typedef enum _SettingsScope SettingsScope;
typedef enum _ShowMessage ShowMessage;

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
  gboolean b;
  char *e;
};
struct _Key {
  const char *str;
  guint mod;
};
struct _KeyValue {
  const char *id;
  Key key;
};
struct _FunctionMap {
  Navigation n;
  int command_line; // command line function ? 
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
  guint scriptlock;
  int size;
  GHashTable *settings_hash;
  SettingsScope setting_apply;
  gboolean forward_search;

  SoupCookieJar *cookiejar;
  SoupCookie *last_cookie;
  gboolean cookies_allowed;

  gboolean complete_history;
  gboolean complete_bookmarks;
  gboolean complete_searchengines;
  gboolean complete_commands;

  Layout layout;
  GList *last_com_history;

  GList *undo_list;

  char *search_engine;
  char *form_name;

  WebKitDownload *download;
  DownloadAction dl_action;
  char *download_command;
  char *mimetype_request;
};

struct _WebSettings {
  Navigation n;
  gboolean builtin;
  gboolean global;
  DwbType type;
  Arg arg;
  S_Func func;
};
struct _ViewStatus {
  guint message_id;
  gboolean add_history;
  char *search_string;
  GList *downloads;
  char *current_host;
  int items_blocked;
  gboolean block;
  gboolean block_current;
  gboolean custom_encoding;
  char *mimetype;
  gboolean plugin_blocker;
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
  GtkWidget *entry;
  GtkWidget *autocompletion;
  GtkWidget *compbox;
  GtkWidget *bottombox;
  View *next;
  ViewStatus *status;
  GHashTable *setting;
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
  GdkColor insert_bg;
  GdkColor insert_fg;
  GdkColor error;
  GdkColor active_c_fg;
  GdkColor active_c_bg;
  GdkColor normal_c_fg;
  GdkColor normal_c_bg;
  GdkColor download_fg;
  GdkColor download_bg;
  char *settings_bg_color;
  char *settings_fg_color;
};
struct _Font {
  PangoFontDescription *fd_normal;
  PangoFontDescription *fd_bold;
  PangoFontDescription *fd_oblique;
  int active_size;
  int normal_size;
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
  SoupURI *proxyuri;

  GIOChannel *si_channel;

  double factor;
  int max_c_items;
  int message_delay;
  int history_length;

  char *settings_border;
  int argc;
  char **argv;
  gboolean single;

  char *startpage;
  char *download_com;

  char *content_block_regex;
};
struct _Files {
  const char *bookmarks;
  const char *history;
  const char *mimetypes;
  const char *quickmarks;
  const char *session;
  const char *searchengines;
  const char *stylesheet;
  const char *keys;
  const char *scriptdir;
  const char *userscripts;
  const char *settings;
  const char *cookies;
  const char *cookies_allow;
  const char *download_path;
  const char *content_block_allow;
  const char *unifile;
  const char *fifo;
};
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
  GList *content_block_allow;
  GList *content_allow;
};

struct _Dwb {
  Gui gui;
  Color color;
  Font font;
  Misc misc;
  State state;
  Completions comps;
  GList *keymap;
  GHashTable *settings;
  Files files;
  FileContent fc;
};

/*}}}*/

/* VARIABLES {{{*/
Dwb dwb;
/*}}}*/

gboolean dwb_insert_mode(Arg *);
void dwb_normal_mode(gboolean);

void dwb_load_uri(Arg *);
void dwb_focus_entry(void);
void dwb_focus_scroll(GList *);

gboolean dwb_update_search(gboolean forward);

void dwb_set_normal_message(GList *, gboolean, const char *, ...);
void dwb_set_error_message(GList *, const char *, ...);
void dwb_set_status_text(GList *, const char *, GdkColor *,  PangoFontDescription *);
void dwb_set_status_bar_text(GtkWidget *, const char *, GdkColor *,  PangoFontDescription *);
void dwb_update_status_text(GList *gl, GtkAdjustment *);
void dwb_update_status(GList *gl);
void dwb_update_layout(void);
void dwb_focus(GList *gl);

gboolean dwb_prepend_navigation(GList *, GList **);
void dwb_prepend_navigation_with_argument(GList **, const char *, const char *);

Navigation * dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *);
gboolean dwb_update_hints(GdkEventKey *);
gboolean dwb_search(Arg *);
void dwb_submit_searchengine(void);
void dwb_save_searchengine(void);
char * dwb_execute_script(const char *, gboolean);
void dwb_resize(double );
void dwb_grab_focus(GList *);
void dwb_source_remove(GList *);

int dwb_entry_position_word_back(int position);
int dwb_entry_position_word_forward(int position);
void dwb_entry_set_text(const char *text);

void dwb_set_proxy(GList *, WebSettings *);

void dwb_set_single_instance(GList *, WebSettings *);
void dwb_new_window(Arg *arg);

gboolean dwb_eval_editing_key(GdkEventKey *);
void dwb_parse_command_line(const char *);
GHashTable * dwb_get_default_settings(void);

char * dwb_get_host(const char *uri);
GList * dwb_get_host_blocked(GList *, char *host);

gboolean dwb_end(void);
Key dwb_str_to_key(char *);

GList * dwb_keymap_add(GList *, KeyValue );

void dwb_save_settings(void);
gboolean dwb_save_files(gboolean);

void dwb_append_navigation_with_argument(GList **, const char *, const char *);
void dwb_clean_load_end(GList *);
#endif
