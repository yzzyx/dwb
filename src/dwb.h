#ifndef DWB_H
#define DWB_H
#include <gtk/gtk.h>
#include <webkit/webkit.h>

/* SETTINGS MAKROS {{{*/
#define KEY_SETTINGS "Dwb Key Settings"
#define SETTINGS "Dwb Settings"

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
function setting_submit() { e = document.activeElement; value = e.value ? e.id + \" \" + e.value : e.id; console.log(value); e.blur(); return false; } \
function checkbox_click(id) { e = document.activeElement; value = e.value ? e.id + \" \" + e.value : e.id; console.log(value); e.blur(); } \
</script>\n<noscript>Enable scripts to add settings!</noscript>\n</head>\n"
#define HTML_H2  "<h2>%s -- Profile: %s</h2>"

#define HTML_BODY_START "<body>\n"
#define HTML_BODY_END "</body>\n"
#define HTML_FORM_START "<div class=\"alignCenter\">\n <form onsubmit=\"return setting_submit()\">\n"
#define HTML_FORM_END "<input type=\"submit\" value=\"save\"/></form>\n</div>\n"
#define HTML_DIV_START "<div class=\"line\">\n"
#define HTML_DIV_KEYS_TEXT "<div class=\"text\">%s</div>\n "
#define HTML_DIV_KEYS_VALUE "<div class=\"key\">\n <input id=\"%s\" value=\"%s %s\"/>\n</div>\n"
#define HTML_DIV_SETTINGS_VALUE "<div class=\"key\">\n <input id=\"%s\" value=\"%s\"/>\n</div>\n"
#define HTML_DIV_SETTINGS_CHECKBOX "<div class=\"key\"\n <input id=\"%s\" type=\"checkbox\" onchange=\"checkbox_click();\" %s>\n</div>\n"
#define HTML_DIV_END "</div>\n"
/*}}}*/
#define INSERT_MODE "Insert Mode"

#define HINT_SEARCH_SUBMIT "_dwb_search_submit_"

/* MAKROS {{{*/ 
#define LENGTH(X)   (sizeof(X)/sizeof(X[0]))
#define GLENGTH(X)  (sizeof(X)/g_array_get_element_size(X)) 
#define NN(X)       ( ((X) == 0) ? 1 : (X) )
#define NUMMOD      (dwb.state.nummod == 0 ? 1 : dwb.state.nummod)

#define CLEAN_STATE(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_BUTTON1_MASK) & ~(GDK_BUTTON2_MASK) & ~(GDK_BUTTON3_MASK) & ~(GDK_BUTTON4_MASK) & ~(GDK_BUTTON5_MASK))
#define CLEAN_COMP_MODE(X)          (X & ~(CompletionMode) & ~(AutoComplete))

#define GET_TEXT()                  (gtk_entry_get_text(GTK_ENTRY(dwb.gui.entry)))
#define CURRENT_VIEW()              ((View*)dwb.state.fview->data)
#define VIEW(X)                     ((View*)X->data)
#define WEBVIEW(X)                  (WEBKIT_WEB_VIEW(((View*)X->data)->web))
#define CURRENT_WEBVIEW()           (WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web))
#define VIEW_FROM_ARG(X)            (X && X->p ? ((GSList*)X->p)->data : dwb.state.fview->data)
#define WEBVIEW_FROM_ARG(arg)       (WEBKIT_WEB_VIEW(((View*)(arg && arg->p ? ((GSList*)arg->p)->data : dwb.state.fview->data))->web))
#define CLEAR_COMMAND_TEXT(X)       dwb_set_status_bar_text(((View *)X->data)->lstatus, NULL, NULL, NULL)

#define DIGIT(X)   (X->keyval >= GDK_0 && X->keyval <= GDK_9)
#define ALPHA(X)    ((X->keyval >= GDK_A && X->keyval <= GDK_Z) ||  (X->keyval >= GDK_a && X->keyval <= GDK_z) || X->keyval == GDK_space)
#define True (void*) true
#define False (void*) false

#define DWB_TAB_KEY(e)              (e->keyval == GDK_Tab || e->keyval == GDK_ISO_Left_Tab)

// Settings
#define GET_CHAR(prop)              ((gchar*)(((WebSettings*)g_hash_table_lookup(dwb.settings, prop))->arg.p))
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
typedef union _Type Type;
/*}}}*/
typedef gboolean (*Command_f)(void*);
typedef gboolean (*Func)(void*);
typedef void (*S_Func)(void *, WebSettings *);
typedef void *(*Content_Func)(const gchar *);

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
  gchar *first;
  gchar *second;
};
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
  const gchar *id;
  Key key;
};
struct _FunctionMap {
  Navigation n;
  Func func;
  const gchar *error; 
  ShowMessage hide;
  Arg arg;
  gboolean entry;
};
struct _KeyMap {
  const gchar *key;
  guint mod;
  FunctionMap *map;
};
struct _Quickmark {
  gchar *key; 
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
  gint nummod;
  Open nv;
  guint scriptlock;
  gint size;
  GHashTable *settings_hash;
  SettingsScope setting_apply;
  gboolean forward_search;
  SoupCookieJar *cookiejar;
  SoupCookie *last_cookie;
  Layout layout;
  GList *last_com_history;

  gchar *input_id;

  gchar *search_engine;
  gchar *form_name;

  WebKitDownload *download;
  DownloadAction dl_action;
  gchar *download_command;
  gchar *mimetype_request;
};

union _Type {
  gboolean b;
  gdouble f;
  guint i; 
  gpointer p;
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
  gchar *hover_uri;
  gboolean add_history;
  gchar *search_string;
  GList *downloads;
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
  gchar *settings_bg_color;
  gchar *settings_fg_color;
};
struct _Font {
  PangoFontDescription *fd_normal;
  PangoFontDescription *fd_bold;
  PangoFontDescription *fd_oblique;
  gint active_size;
  gint normal_size;
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
  gint width;
  gint height;
};
struct _Misc {
  const gchar *name;
  const gchar *prog_path;
  gchar *scripts;
  const gchar *profile;
  const gchar *default_search;
  SoupSession *soupsession;
  SoupURI *proxyuri;

  gdouble factor;
  gint max_c_items;
  gint message_delay;
  gint history_length;

  gchar *settings_border;
  gint argc;
  gchar **argv;
  gchar *fifo;
  gboolean single;

  gchar *startpage;
  gchar *download_com;
};
struct _Files {
  const gchar *bookmarks;
  const gchar *history;
  const gchar *mimetypes;
  const gchar *quickmarks;
  const gchar *session;
  const gchar *searchengines;
  const gchar *stylesheet;
  const gchar *keys;
  const gchar *scriptdir;
  const gchar *settings;
  const gchar *cookies;
  const gchar *cookies_allow;
  const gchar *download_path;
};
struct _FileContent {
  GList *bookmarks;
  GList *history;
  GList *quickmarks;
  GList *searchengines;
  GList *keys;
  GList *settings;
  GList *cookies_allow;
  GList *commands;
  GList *mimetypes;
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

gboolean dwb_update_search(gboolean forward);

void dwb_set_normal_message(GList *, const gchar *, gboolean);
void dwb_set_error_message(GList *, const gchar *);
void dwb_set_status_text(GList *, const gchar *, GdkColor *,  PangoFontDescription *);
void dwb_set_status_bar_text(GtkWidget *, const gchar *, GdkColor *,  PangoFontDescription *);
void dwb_update_status_text(GList *gl);
void dwb_update_status(GList *gl);
void dwb_update_layout(void);

gboolean dwb_prepend_navigation(GList *, GList **);
void dwb_prepend_navigation_with_argument(GList **, const gchar *, const gchar *);

gboolean dwb_update_hints(GdkEventKey *);
gboolean dwb_search(Arg *);
void dwb_submit_searchengine(void);
void dwb_save_searchengine(void);
gchar * dwb_execute_script(const gchar *);
void dwb_resize(gdouble );
void dwb_grab_focus(GList *);
void dwb_source_remove(GList *);

gint dwb_entry_position_word_back(gint position);
gint dwb_entry_position_word_forward(gint position);
void dwb_entry_set_text(const gchar *text);

void dwb_set_proxy(GList *, WebSettings *);
void dwb_new_window(Arg *arg);

gboolean dwb_eval_editing_key(GdkEventKey *);
void dwb_parse_command_line(const gchar *);
GHashTable * dwb_get_default_settings(void);

void dwb_exit(void);
Key dwb_strv_to_key(gchar **, gsize );

GList * dwb_keymap_delete(GList *, KeyValue );
GList * dwb_keymap_add(GList *, KeyValue );
#endif
