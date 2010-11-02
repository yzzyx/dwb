#define _POSIX_SOURCE || _BSD_SOURCE 
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libsoup/soup.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h> 
#include "dwb.h"
#include "completion.h"
#include "commands.h"
#include "view.h"
#include "util.h"
#include "download.h"
#include "config.h"
#include "session.h"
#define NAME "dwb"


/* DECLARATIONS {{{*/
void dwb_set_cookies(GList *l, WebSettings *s);
static void dwb_set_dummy(GList *, WebSettings *);
static void dwb_set_content_block(GList *, WebSettings *);
void dwb_clean_buffer(GList *);
static void dwb_init_proxy(void);

gboolean dwb_command_mode(Arg *arg);
static void dwb_com_reload_scripts(GList *,  WebSettings *);
static void dwb_com_reload_layout(GList *,  WebSettings *);
static gboolean dwb_test_cookie_allowed(SoupCookie *);
static void dwb_save_cookies(void);

void dwb_open_si_channel(void);
gboolean dwb_handle_channel(GIOChannel *c, GIOCondition condition, void *data);

static gboolean dwb_eval_key(GdkEventKey *);

static void dwb_webkit_setting(GList *, WebSettings *);
static void dwb_webview_property(GList *, WebSettings *);


static void dwb_tab_label_set_text(GList *, const gchar *);

static void dwb_save_quickmark(const gchar *);
static void dwb_open_quickmark(const gchar *);


static void dwb_update_tab_label(void);
static gchar * dwb_get_resolved_uri(const gchar *);


static void dwb_init_key_map(void);
static void dwb_init_settings(void);
static void dwb_init_style(void);
static void dwb_init_scripts(void);
static void dwb_init_gui(void);
static void dwb_init_vars(void);

static void dwb_clean_vars(void);
//static gboolean dwb_end(void);
/*}}}*/

static GdkNativeWindow embed = 0;
static gint signals[] = { SIGFPE, SIGILL, SIGINT, SIGQUIT, SIGTERM, SIGALRM, SIGSEGV};
static gint MAX_COMPLETIONS = 11;
static gchar *restore = NULL;

/* FUNCTION_MAP{{{*/
#define NO_URL                      "No URL in current context"
#define NO_HINTS                    "No Hints in current context"
static FunctionMap FMAP [] = {
  { { "add_view",              "Add a new view",                    }, 1, (Func)dwb_add_view,                NULL,                              AlwaysSM,     { .p = NULL }, },
  { { "allow_cookie",          "Cookie allowed",                    }, 0, (Func)dwb_com_allow_cookie,        "No cookie in current context",    PostSM, },
  { { "bookmark",              "Bookmark current page",             }, 1, (Func)dwb_com_bookmark,            NO_URL,                            PostSM, },
  { { "bookmarks",             "Bookmarks",                         }, 0, (Func)dwb_com_bookmarks,           "No Bookmarks",                    NeverSM,     { .n = OpenNormal }, }, 
  { { "bookmarks_nv",          "Bookmarks new view",                }, 0, (Func)dwb_com_bookmarks,           "No Bookmarks",                    NeverSM,     { .n = OpenNewView }, },
  { { "bookmarks_nw",          "Bookmarks new window",              }, 0, (Func)dwb_com_bookmarks,           "No Bookmarks",                    NeverSM,     { .n = OpenNewWindow}, }, 
  { { "new_view",              "New view for next navigation",      }, 0, (Func)dwb_com_new_window_or_view,  NULL,                              NeverSM,     { .n = OpenNewView}, }, 
  { { "new_window",            "New window for next navigation",    }, 0, (Func)dwb_com_new_window_or_view,  NULL,                              NeverSM,     { .n = OpenNewWindow}, }, 
  { { "command_mode",          "Enter command mode",                }, 0, (Func)dwb_command_mode,            NULL,                              PostSM, },
  { { "decrease_master",       "Decrease master area",              }, 1, (Func)dwb_com_resize_master,       "Cannot decrease further",         AlwaysSM,    { .n = 5 } },
  { { "download_hint",         "Download via hints",                }, 0, (Func)dwb_com_show_hints,          NO_HINTS,                          NeverSM,    { .n = OpenDownload }, },
  { { "find_backward",         "Find Backward ",                    }, 0, (Func)dwb_com_find,                NO_URL,                            NeverSM,     { .b = false }, },
  { { "find_forward",          "Find Forward ",                     }, 0, (Func)dwb_com_find,                NO_URL,                            NeverSM,     { .b = true }, },
  { { "find_next",             "Find next",                         }, 0, (Func)dwb_search,                  "No matches",                      AlwaysSM,     { .b = true }, },
  { { "find_previous",         "Find previous",                     }, 0, (Func)dwb_search,                  "No matches",                      AlwaysSM,     { .b = false }, },
  { { "focus_input",           "Focus input",                       }, 1, (Func)dwb_com_focus_input,        "No input found in current context",      AlwaysSM, },
  { { "focus_next",            "Focus next view",                   }, 0, (Func)dwb_com_focus_next,          "No other view",                   AlwaysSM, },
  { { "focus_prev",            "Focus previous view",               }, 0, (Func)dwb_com_focus_prev,          "No other view",                   AlwaysSM, },
  { { "focus_nth_view",        "Focus nth view",                    }, 0, (Func)dwb_com_focus_nth_view,       "No such view",                    AlwaysSM,  { 0 } },
  { { "hint_mode",             "Follow hints",                      }, 0, (Func)dwb_com_show_hints,          NO_HINTS,                          NeverSM,    { .n = OpenNormal }, },
  { { "hint_mode_nv",          "Follow hints (new view)",           }, 0, (Func)dwb_com_show_hints,          NO_HINTS,                          NeverSM,    { .n = OpenNewView }, },
  { { "hint_mode_nw",          "Follow hints (new window)",         }, 0, (Func)dwb_com_show_hints,          NO_HINTS,                          NeverSM,    { .n = OpenNewWindow }, },
  { { "history_back",          "Go Back",                           }, 1, (Func)dwb_com_history_back,        "Beginning of History",            AlwaysSM, },
  { { "history_forward",       "Go Forward",                        }, 1, (Func)dwb_com_history_forward,     "End of History",                  AlwaysSM, },
  { { "increase_master",       "Increase master area",              }, 1, (Func)dwb_com_resize_master,       "Cannot increase further",         AlwaysSM,    { .n = -5 } },
  { { "insert_mode",           "Insert Mode",                       }, 0, (Func)dwb_insert_mode,             NULL,                              AlwaysSM, },
  { { "open",                  "Open",                              }, 1, (Func)dwb_com_open,                NULL,                              NeverSM,   { .n = OpenNormal,      .p = NULL } },
  { { "open_nv",               "Viewopen",                          }, 1, (Func)dwb_com_open,                NULL,                              NeverSM,   { .n = OpenNewView,     .p = NULL } },
  { { "open_nw",               "Winopen",                           }, 1, (Func)dwb_com_open,                NULL,                              NeverSM,   { .n = OpenNewWindow,     .p = NULL } },
  { { "open_quickmark",        "Quickmark",                         }, 0, (Func)dwb_com_quickmark,           NO_URL,                            NeverSM,   { .n = QuickmarkOpen, .i=OpenNormal }, },
  { { "open_quickmark_nv",     "Quickmark-new-view",                }, 0, (Func)dwb_com_quickmark,           NULL,                              NeverSM,    { .n = QuickmarkOpen, .i=OpenNewView }, },
  { { "open_quickmark_nw",     "Quickmark-new-window",              }, 0, (Func)dwb_com_quickmark,           NULL,                              NeverSM,    { .n = QuickmarkOpen, .i=OpenNewWindow }, },
  { { "push_master",           "Push to master area",               }, 1, (Func)dwb_com_push_master,         "No other view",                   AlwaysSM, },
  { { "reload",                "Reload",                            }, 1, (Func)dwb_com_reload,              NULL,                              AlwaysSM, },
  { { "remove_view",           "Close view",                        }, 1, (Func)dwb_com_remove_view,         NULL,                              AlwaysSM, },
  { { "save_quickmark",        "Save a quickmark for this page",    }, 0, (Func)dwb_com_quickmark,           NO_URL,                            NeverSM,    { .n = QuickmarkSave }, },
  { { "save_search_field",     "Add a new searchengine",            }, 0, (Func)dwb_com_add_search_field,    "No input in current context",     PostSM, },
  { { "scroll_bottom",         "Scroll to  bottom of the page",     }, 1, (Func)dwb_com_scroll,              NULL,                              AlwaysSM,    { .n = Bottom }, },
  { { "scroll_down",           "Scroll down",                       }, 0, (Func)dwb_com_scroll,              "Bottom of the page",              AlwaysSM,    { .n = Down, }, },
  { { "scroll_left",           "Scroll left",                       }, 0, (Func)dwb_com_scroll,              "Left side of the page",           AlwaysSM,    { .n = Left }, },
  { { "scroll_page_down",      "Scroll one page down",              }, 0, (Func)dwb_com_scroll,              "Bottom of the page",              AlwaysSM,    { .n = PageDown, }, },
  { { "scroll_page_up",        "Scroll one page up",                }, 0, (Func)dwb_com_scroll,              "Top of the page",                 AlwaysSM,    { .n = PageUp, }, },
  { { "scroll_right",          "Scroll left",                       }, 0, (Func)dwb_com_scroll,              "Right side of the page",          AlwaysSM,    { .n = Right }, },
  { { "scroll_top",            "Scroll to the top of the page",     }, 1, (Func)dwb_com_scroll,              NULL,                              AlwaysSM,    { .n = Top }, },
  { { "scroll_up",             "Scroll up",                         }, 0, (Func)dwb_com_scroll,              "Top of the page",                 AlwaysSM,    { .n = Up, }, },
  { { "set_global_setting",    "Set global property",               }, 0, (Func)dwb_com_set_setting,         NULL,                              NeverSM,    { .n = Global } },
  { { "set_key",               "Set keybinding",                    }, 0, (Func)dwb_com_set_key,             NULL,                              NeverSM,    { 0 } },
  { { "set_setting",           "Set property",                      }, 0, (Func)dwb_com_set_setting,         NULL,                              NeverSM,    { .n = PerView } },
  { { "show_global_settings",  "Show global settings",              }, 1, (Func)dwb_com_show_settings,       NULL,                              AlwaysSM,    { .n = Global } },
  { { "show_keys",             "Key configuration",                 }, 1, (Func)dwb_com_show_keys,           NULL,                              AlwaysSM, },
  { { "show_settings",         "Settings",                          }, 1, (Func)dwb_com_show_settings,       NULL,                              AlwaysSM,    { .n = PerView } },
  { { "toggle_bottomstack",    "Toggle bottomstack",                }, 1, (Func)dwb_com_set_orientation,     NULL,                              AlwaysSM, },
  { { "toggle_encoding",       "Toggle Custom encoding",            }, 1, (Func)dwb_com_toggle_custom_encoding,    NULL,                        PostSM, },
  { { "toggle_maximized",      "Toggle maximized",                  }, 1, (Func)dwb_com_toggle_maximized,    NULL,                              AlwaysSM, },
  { { "view_source",           "View source",                       }, 1, (Func)dwb_com_view_source,         NULL,                              AlwaysSM, },
  { { "zoom_in",               "Zoom in",                           }, 1, (Func)dwb_com_zoom_in,             "Cannot zoom in further",          AlwaysSM, },
  { { "zoom_normal",           "Zoom 100%",                         }, 1, (Func)dwb_com_set_zoom_level,      NULL,                              AlwaysSM,    { .d = 1.0,   .p = NULL } },
  { { "zoom_out",              "Zoom out",                          }, 1, (Func)dwb_com_zoom_out,            "Cannot zoom out further",         AlwaysSM, },
  // yank and paste
  { { "yank",                  "Yank",                              }, 1, (Func)dwb_com_yank,                 NO_URL,                 PostSM,  { .p = GDK_NONE } },
  { { "yank_primary",          "Yank to Primary selection",         }, 1, (Func)dwb_com_yank,                 NO_URL,                 PostSM,  { .p = GDK_SELECTION_PRIMARY } },
  { { "paste",                 "Paste",                             }, 1, (Func)dwb_com_paste,               "Clipboard is empty",    AlwaysSM, { .n = OpenNormal, .p = GDK_NONE } },
  { { "paste_primary",         "Paste primary selection",           }, 1, (Func)dwb_com_paste,               "No primary selection",  AlwaysSM, { .n = OpenNormal, .p = GDK_SELECTION_PRIMARY } },
  { { "paste_nv",              "Paste, new view",                   }, 1, (Func)dwb_com_paste,               "Clipboard is empty",    AlwaysSM, { .n = OpenNewView, .p = GDK_NONE } },
  { { "paste_primary_nv",      "Paste primary selection, new view", }, 1, (Func)dwb_com_paste,               "No primary selection",  AlwaysSM, { .n = OpenNewView, .p = GDK_SELECTION_PRIMARY } },
  { { "paste_nw",              "Paste, new window",                   }, 1, (Func)dwb_com_paste,             "Clipboard is empty",    AlwaysSM, { .n = OpenNewWindow, .p = GDK_NONE } },
  { { "paste_primary_nw",      "Paste primary selection, new window", }, 1, (Func)dwb_com_paste,             "No primary selection",  AlwaysSM, { .n = OpenNewWindow, .p = GDK_SELECTION_PRIMARY } },

  { { "save_session",          "Save current session", },              1, (Func)dwb_com_save_session,        NULL,                              AlwaysSM,  { .n = NormalMode } },
  { { "save_named_session",    "Save current session with name", },    0, (Func)dwb_com_save_session,        NULL,                              PostSM,  { .n = SaveSession } },
  { { "save",                  "Save all configuration files", },      1, (Func)dwb_com_save_files,        NULL,                              PostSM,  { .n = SaveSession } },

  //Entry editing
  { { "entry_delete_word",      "Delete word", },                      0,  (Func)dwb_com_entry_delete_word,            NULL,        AlwaysSM,  { 0 }, true, }, 
  { { "entry_delete_letter",    "Delete a single letter", },           0,  (Func)dwb_com_entry_delete_letter,          NULL,        AlwaysSM,  { 0 }, true, }, 
  { { "entry_delete_line",      "Delete to beginning of the line", },  0,  (Func)dwb_com_entry_delete_line,            NULL,        AlwaysSM,  { 0 }, true, }, 
  { { "entry_word_forward",     "Move cursor forward on word", },      0,  (Func)dwb_com_entry_word_forward,           NULL,        AlwaysSM,  { 0 }, true, }, 
  { { "entry_word_back",        "Move cursor back on word", },         0,  (Func)dwb_com_entry_word_back,              NULL,        AlwaysSM,  { 0 }, true, }, 
  { { "entry_history_back",     "Command history back", },             0,  (Func)dwb_com_entry_history_back,           NULL,        AlwaysSM,  { 0 }, true, }, 
  { { "entry_history_forward",  "Command history forward", },          0,  (Func)dwb_com_entry_history_forward,        NULL,        AlwaysSM,  { 0 }, true, }, 
  { { "download_set_execute",   "Complete binaries", },             0, (Func)dwb_dl_set_execute,        NULL,        AlwaysSM,  { 0 }, true, }, 

  { { "spell_checking",        "Setting: spell checking",           }, 0, (Func)dwb_com_toggle_property,     NULL,                              PostSM,    { .p = "enable-spell-checking" } },
  { { "scripts",               "Setting: scripts",                  }, 1, (Func)dwb_com_toggle_property,     NULL,                              PostSM,    { .p = "enable-scripts" } },
  { { "auto_shrink_images",    "Toggle autoshrink images",        }, 0, (Func)dwb_com_toggle_property,     NULL,                    PostSM,    { .p = "auto-shrink-images" } },
  { { "autoload_images",       "Toggle autoload images",          }, 0, (Func)dwb_com_toggle_property,     NULL,                    PostSM,    { .p = "auto-load-images" } },
  { { "autoresize_window",     "Toggle autoresize window",        }, 0, (Func)dwb_com_toggle_property,     NULL,                    PostSM,    { .p = "auto-resize-window" } },
  { { "caret_browsing",        "Toggle caret browsing",           }, 0, (Func)dwb_com_toggle_property,     NULL,                    PostSM,    { .p = "enable-caret-browsing" } },
  { { "default_context_menu",  "Toggle enable default context menu",           }, 0, (Func)dwb_com_toggle_property,     NULL,       PostSM,    { .p = "enable-default-context-menu" } },
  { { "file_access_from_file_uris",     "Toggle file access from file uris",   }, 0, (Func)dwb_com_toggle_property,     NULL,                  PostSM, { .p = "enable-file-acces-from-file-uris" } },
  { { "universal file_access_from_file_uris",   "Toggle universal file access from file uris",   }, 0, (Func)dwb_com_toggle_property,  NULL,   PostSM, { .p = "enable-universal-file-acces-from-file-uris" } },
  { { "java_applets",          "Toggle java applets",             }, 0, (Func)dwb_com_toggle_property,     NULL,                    PostSM,    { .p = "enable-java-applets" } },
  { { "plugins",               "Toggle plugins",                  }, 1, (Func)dwb_com_toggle_property,     NULL,                    PostSM,    { .p = "enable-plugins" } },
  { { "private_browsing",      "Toggle private browsing",         }, 0, (Func)dwb_com_toggle_property,     NULL,                    PostSM,    { .p = "enable-private-browsing" } },
  { { "page_cache",            "Toggle page cache",               }, 0, (Func)dwb_com_toggle_property,     NULL,                    PostSM,    { .p = "enable-page-cache" } },
  { { "js_can_open_windows",   "Toggle Javascript can open windows automatically", }, 0, (Func)dwb_com_toggle_property,     NULL,   PostSM,    { .p = "javascript-can-open-windows-automatically" } },
  { { "enforce_96_dpi",        "Toggle enforce a resolution of 96 dpi", },    0, (Func)dwb_com_toggle_property,     NULL,           PostSM,    { .p = "enforce-96-dpi" } },
  { { "print_backgrounds",     "Toggle print backgrounds", },      0,    (Func)dwb_com_toggle_property,    NULL,                    PostSM,    { .p = "print-backgrounds" } },
  { { "resizable_text_areas",  "Toggle resizable text areas", },   0,  (Func)dwb_com_toggle_property,      NULL,                    PostSM,    { .p = "resizable-text-areas" } },
  { { "tab_cycle",             "Toggle tab cycles through elements", },  0,   (Func)dwb_com_toggle_property,     NULL,              PostSM,    { .p = "tab-key-cycles-through-elements" } },
  { { "proxy",                 "Toggle proxy",                    }, 1, (Func)dwb_com_toggle_proxy,        NULL,                    PostSM,    { 0 } },
  { { "toggle_block_content",   "Toggle block content for current domain" },  1, (Func) dwb_com_toggle_block_content, NULL,                  PostSM,    { 0 } }, 
  { { "allow_content",         "Allow scripts for current domain in the current session" },  1, (Func) dwb_com_allow_content, NULL,              PostSM,    { 0 } }, 
};/*}}}*/

/* DWB_SETTINGS {{{*/
static WebSettings DWB_SETTINGS[] = {
  { { "auto-load-images",			                   "Autoload images", },                                         true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "auto-resize-window",			                 "Autoresize images", },                                       true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "auto-shrink-images",			                 "Autoshrink images", },                                       true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "cursive-font-family",			               "Cursive font family", },                                     true, false,  Char,    { .p = "serif"           }, (S_Func) dwb_webkit_setting, },
  { { "default-encoding",			                   "Default encoding", },                                        true, false,  Char,    { .p = NULL      }, (S_Func) dwb_webkit_setting, },
  { { "default-font-family",			               "Default font family", },                                     true, false,  Char,    { .p = "sans-serif"      }, (S_Func) dwb_webkit_setting, },
  { { "default-font-size",			                 "Default font size", },                                       true, false,  Integer, { .i = 12                }, (S_Func) dwb_webkit_setting, },
  { { "default-monospace-font-size",			       "Default monospace font size", },                             true, false,  Integer, { .i = 10                }, (S_Func) dwb_webkit_setting, },
  { { "enable-caret-browsing",			             "Caret Browsing", },                                          true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-default-context-menu",			       "Enable default context menu", },                             true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-developer-extras",			           "Enable developer extras",    },                              true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-dom-paste",			                   "Enable DOM paste", },                                        true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-file-access-from-file-uris",			 "File access from file uris", },                              true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-html5-database",			             "Enable HTML5-database" },                                    true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-html5-local-storage",			         "Enable HTML5 local storage", },                              true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-java-applet",			                 "Java Applets", },                                            true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-offline-web-application-cache",		 "Offline web application cache", },                           true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-page-cache",			                 "Page cache", },                                              true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-plugins",			                     "Plugins", },                                                 true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-private-browsing",			           "Private Browsing", },                                        true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-scripts",			                     "Script", },                                                  true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-site-specific-quirks",			       "Site specific quirks", },                                    true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-spatial-navigation",			         "Spatial navigation", },                                      true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-spell-checking",			             "Spell checking", },                                          true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-universal-access-from-file-uris",	 "Universal access from file  uris", },                        true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-xss-auditor",			                 "XSS auditor", },                                             true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enforce-96-dpi",			                     "Enforce 96 dpi", },                                          true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "fantasy-font-family",			               "Fantasy font family", },                                     true, false,  Char,    { .p = "serif"           }, (S_Func) dwb_webkit_setting, },
  { { "javascript-can-access-clipboard",			   "Javascript can access clipboard", },                         true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "javascript-can-open-windows-automatically", "Javascript can open windows automatically", },             true, false,  Boolean, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "minimum-font-size",			                 "Minimum font size", },                                       true, false,  Integer, { .i = 5                 }, (S_Func) dwb_webkit_setting, },
  { { "minimum-logical-font-size",			         "Minimum logical font size", },                               true, false,  Integer, { .i = 5                 }, (S_Func) dwb_webkit_setting, },
  { { "monospace-font-family",			             "Monospace font family", },                                   true, false,  Char,    { .p = "monospace"       }, (S_Func) dwb_webkit_setting, },
  { { "print-backgrounds",			                 "Print backgrounds", },                                       true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "resizable-text-areas",			               "Resizable text areas", },                                    true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "sans-serif-font-family",			             "Sans serif font family", },                                  true, false,  Char,    { .p = "sans-serif"      }, (S_Func) dwb_webkit_setting, },
  { { "serif-font-family",			                 "Serif font family", },                                       true, false,  Char,    { .p = "serif"           }, (S_Func) dwb_webkit_setting, },
  { { "spell-checking-languages",			           "Spell checking languages", },                                true, false,  Char,    { .p = NULL              }, (S_Func) dwb_webkit_setting, },
  { { "tab-key-cycles-through-elements",			   "Tab cycles through elements in insert mode", },              true, false,  Boolean, { .b = true              }, (S_Func) dwb_webkit_setting, },

  { { "user-agent",			                         "User agent", },                                              true, false,  Char,    { .p = NULL              }, (S_Func) dwb_webkit_setting, },
  { { "user-stylesheet-uri",			               "User stylesheet uri", },                                     true, false,  Char,    { .p = NULL              }, (S_Func) dwb_webkit_setting, },
  { { "zoom-step",			                         "Zoom Step", },                                               true, false,  Double,  { .d = 0.1               }, (S_Func) dwb_webkit_setting, },
  { { "custom-encoding",                         "Custom encoding", },                                         false, false, Char,    { .p = NULL           }, (S_Func) dwb_webview_property, },
  { { "editable",                                "Content editable", },                                        false, false, Boolean, { .b = false             }, (S_Func) dwb_webview_property, },
  { { "full-content-zoom",                       "Full content zoom", },                                       false, false, Boolean, { .b = false             }, (S_Func) dwb_webview_property, },
  { { "zoom-level",                              "Zoom level", },                                              false, false, Double,  { .d = 1.0               }, (S_Func) dwb_webview_property, },
  { { "proxy",                                   "HTTP-proxy", },                                              false, true,  Boolean, { .b = false              },  (S_Func) dwb_set_proxy, },
  { { "proxy-url",                               "HTTP-proxy url", },                                          false, true,  Char,    { .p = NULL              },   (S_Func) dwb_init_proxy, },
  { { "cookies",                                  "All Cookies allowed", },                                     false, true,  Boolean, { .b = false             }, (S_Func) dwb_set_cookies, },

  { { "active-fg-color",                         "UI: Active view foreground", },                              false, true,  ColorChar, { .p = "#ffffff"         },    (S_Func) dwb_com_reload_layout, },
  { { "active-bg-color",                         "UI: Active view background", },                              false, true,  ColorChar, { .p = "#000000"         },    (S_Func) dwb_com_reload_layout, },
  { { "normal-fg-color",                         "UI: Inactive view foreground", },                            false, true,  ColorChar, { .p = "#cccccc"         },    (S_Func) dwb_com_reload_layout, },
  { { "normal-bg-color",                         "UI: Inactive view background", },                            false, true,  ColorChar, { .p = "#505050"         },    (S_Func) dwb_com_reload_layout, },

  { { "tab-active-fg-color",                     "UI: Active view tabforeground", },                           false, true,  ColorChar, { .p = "#ffffff"         },    (S_Func) dwb_com_reload_layout, },
  { { "tab-active-bg-color",                     "UI: Active view tabbackground", },                           false, true,  ColorChar, { .p = "#000000"         },    (S_Func) dwb_com_reload_layout, },
  { { "tab-normal-fg-color",                     "UI: Inactive view tabforeground", },                         false, true,  ColorChar, { .p = "#cccccc"         },    (S_Func) dwb_com_reload_layout, },
  { { "tab-normal-bg-color",                     "UI: Inactive view tabbackground", },                         false, true,  ColorChar, { .p = "#505050"         },    (S_Func) dwb_com_reload_layout, },

  { { "active-completion-fg-color",                    "UI: Completion active foreground", },                        false, true,  ColorChar, { .p = "#53868b"         }, (S_Func) dwb_init_style, },
  { { "active-completion-bg-color",                    "UI: Completion active background", },                        false, true,  ColorChar, { .p = "#000000"         }, (S_Func) dwb_init_style, },
  { { "normal-completion-fg-color",                    "UI: Completion inactive foreground", },                      false, true,  ColorChar, { .p = "#eeeeee"         }, (S_Func) dwb_init_style, },
  { { "normal-comp-bg-color",                    "UI: Completion inactive background", },                      false, true,  ColorChar, { .p = "#151515"         }, (S_Func) dwb_init_style, },

  { { "insertmode-fg-color",                         "UI: Insertmode foreground", },                               false, true,  ColorChar, { .p = "#000000"         }, (S_Func) dwb_init_style, },
  { { "insertmode-bg-color",                         "UI: Insertmode background", },                               false, true,  ColorChar, { .p = "#dddddd"         }, (S_Func) dwb_init_style, },
  { { "error-color",                             "UI: Error color", },                                         false, true,  ColorChar, { .p = "#ff0000"         }, (S_Func) dwb_init_style, },

  { { "settings-fg-color",                       "UI: Settings view foreground", },                            false, true,  ColorChar, { .p = "#ffffff"         }, (S_Func) dwb_init_style, },
  { { "settings-bg-color",                       "UI: Settings view background", },                            false, true,  ColorChar, { .p = "#151515"         }, (S_Func) dwb_init_style, },
  { { "settings-border",                         "UI: Settings view border", },                                false, true,  Char,      { .p = "1px dotted black"}, (S_Func) dwb_init_style, },
 
  { { "active-font-size",                        "UI: Active view fontsize", },                                false, true,  Integer, { .i = 12                },   (S_Func) dwb_com_reload_layout, },
  { { "normal-font-size",                        "UI: Inactive view fontsize", },                              false, true,  Integer, { .i = 10                },   (S_Func) dwb_com_reload_layout, },
  
  { { "font",                                    "UI: Font", },                                                false, true,  Char, { .p = "monospace"          },   (S_Func) dwb_com_reload_layout, },
   
  { { "hint-letter-seq",                       "Hints: Letter sequence for letter hints", },             false, true,  Char, { .p = "FDSARTGBVECWXQYIOPMNHZULKJ"  }, (S_Func) dwb_com_reload_scripts, },
  { { "hint-style",                              "Hints: Hintstyle (letter or number)", },                     false, true,  Char, { .p = "letter"            },     (S_Func) dwb_com_reload_scripts, },
  { { "hint-font-size",                          "Hints: Font size", },                                        false, true,  Char, { .p = "12px"              },     (S_Func) dwb_com_reload_scripts, },
  { { "hint-font-weight",                        "Hints: Font weight", },                                      false, true,  Char, { .p = "normal"            },     (S_Func) dwb_com_reload_scripts, },
  { { "hint-font-family",                        "Hints: Font family", },                                      false, true,  Char, { .p = "monospace"         },     (S_Func) dwb_com_reload_scripts, },
  { { "hint-fg-color",                           "Hints: Foreground color", },                                 false, true,  ColorChar, { .p = "#ffffff"      },     (S_Func) dwb_com_reload_scripts, },
  { { "hint-bg-color",                           "Hints: Background color", },                                 false, true,  ColorChar, { .p = "#000088"      },     (S_Func) dwb_com_reload_scripts, },
  { { "hint-active-color",                       "Hints: Active link color", },                                false, true,  ColorChar, { .p = "#00ff00"      },     (S_Func) dwb_com_reload_scripts, },
  { { "hint-normal-color",                       "Hints: Inactive link color", },                              false, true,  ColorChar, { .p = "#ffff99"      },     (S_Func) dwb_com_reload_scripts, },
  { { "hint-border",                             "Hints: Hint Border", },                                      false, true,  Char, { .p = "2px dashed #000000"    }, (S_Func) dwb_com_reload_scripts, },
  { { "hint-opacity",                            "Hints: Hint Opacity", },                                     false, true,  Double, { .d = 0.75         },          (S_Func) dwb_com_reload_scripts, },
  { { "auto-completion",                         "Show possible keystrokes", },                                false, true,  Boolean, { .b = true         },     (S_Func)dwb_comp_set_autcompletion, },
  { { "startpage",                               "Default homepage", },                                        false, true,  Char,    { .p = "about:blank" },        (S_Func) dwb_init_vars, }, 
  { { "single-instance",                         "Single instance", },                                         false, true,  Boolean,    { .b = false },          (S_Func)dwb_set_single_instance, }, 
  { { "save-session",                            "Autosave sessions", },                                       false, true,  Boolean,    { .b = false },          (S_Func)dwb_set_dummy, }, 

  
  { { "content-block-regex",   "Mimetypes that will be blocked", },       false, false,  Char,    { .p = "(application|text)/(x-)?(shockwave-flash|javascript)" },  (S_Func) dwb_set_dummy, }, 
  { { "block-content",                        "Block ugly content", },                                        false, false,  Boolean,    { .b = false },        (S_Func) dwb_set_content_block, }, 

  // downloads
  { { "download-external-command",                        "Downloads: External download program", },                               false, true,  Char, 
              { .p = "xterm -e wget 'dwb_uri' -O 'dwb_output' --load-cookies 'dwb_cookies'"   },     (S_Func)dwb_set_dummy, },
  { { "download-use-external-program",           "Downloads: Use external download program", },                           false, true,  Boolean, { .b = false         },    (S_Func)dwb_set_dummy, },

  { { "complete-history",                        "Complete browsing history", },                              false, true,  Boolean, { .b = true         },     (S_Func)dwb_set_dummy, },
    
  { { "default-width",                           "Default width", },                                           false, true,  Integer, { .i = 800          }, (S_Func)dwb_set_dummy, },
  { { "default-height",                          "Default height", },                                           false, true,  Integer, { .i = 600          }, (S_Func)dwb_set_dummy, },
  { { "message-delay",                           "Message delay", },                                           false, true,  Integer, { .i = 2          }, (S_Func) dwb_init_vars, },
  { { "history-length",                          "History length", },                                          false, true,  Integer, { .i = 500          }, (S_Func) dwb_init_vars, },
  { { "size",                                    "UI: Default tiling area size (in %)", },                     false, true,  Integer, { .i = 30          }, (S_Func)dwb_set_dummy, },
  { { "factor",                                  "UI: Default Zoom factor of tiling area", },                  false, true,  Double, { .d = 0.3          }, (S_Func)dwb_set_dummy, },
  { { "layout",                                  "UI: Default layout (Normal, Bottomstack, Maximized)", },     false, true,  Char, { .p = "Normal Maximized" },  (S_Func)dwb_set_dummy, },
};/*}}}*/

/* CALLBACKS {{{*/

/* dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {{{*/
gboolean 
dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  gboolean ret = false;

  gchar *key = dwb_util_keyval_to_char(e->keyval);
  if (e->keyval == GDK_Escape) {
    dwb_normal_mode(true);
    ret = false;
  }
  else if (dwb.state.mode == InsertMode) {
    ret = false;
  }
  else if (dwb.state.mode == QuickmarkSave) {
    if (key) {
      dwb_save_quickmark(key);
    }
    ret = true;
  }
  else if (dwb.state.mode == QuickmarkOpen) {
    if (key) {
      dwb_open_quickmark(key);
    }
    ret = true;
  }
  else if (gtk_widget_has_focus(dwb.gui.entry) || dwb.state.mode & CompletionMode) {
    ret = false;
  }
  else if (DWB_TAB_KEY(e)) {
    dwb_comp_autocomplete(dwb.keymap, e);
    ret = true;
  }
  else {
    if (dwb.state.mode & AutoComplete) {
      if (DWB_TAB_KEY(e)) {
        dwb_comp_autocomplete(NULL, e);
      }
      else if (e->keyval == GDK_Return) {
        dwb_comp_eval_autocompletion();
        return true;
      }
      ret = true;
    }
    ret = dwb_eval_key(e);
  }
  g_free(key);
  return ret;
}/*}}}*/

/* dwb_key_release_cb {{{*/
gboolean 
dwb_key_release_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  if (DWB_TAB_KEY(e)) {
    return true;
  }
  return false;
}/*}}}*/

/* dwb_cookie_changed_cb {{{*/
void 
dwb_cookie_changed_cb(SoupCookieJar *cookiejar, SoupCookie *old, SoupCookie *new) {
  if (new) {
    dwb.state.last_cookie = soup_cookie_copy(new);
    dwb_save_cookies();
  }
}/*}}}*/

/*}}}*/

/* COMMAND_TEXT {{{*/

/* dwb_set_status_bar_text(GList *gl, const char *text, GdkColor *fg,  PangoFontDescription *fd) {{{*/
void
dwb_set_status_bar_text(GtkWidget *label, const char *text, GdkColor *fg,  PangoFontDescription *fd) {
  gchar *escaped = g_markup_escape_text(text ? text: "", -1);
  gtk_label_set_markup(GTK_LABEL(label), escaped);
  g_free(escaped);

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

/* dwb_set_normal_message {{{*/
void 
dwb_set_normal_message(GList *gl, const gchar  *text, gboolean hide) {
  View *v = gl->data;
  dwb_set_status_bar_text(v->lstatus, text, &dwb.color.active_fg, dwb.font.fd_bold);
  dwb_source_remove(gl);
  if (hide) {
    v->status->message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, gl);
  }
}/*}}}*/

/* dwb_set_error_message {{{*/
void 
dwb_set_error_message(GList *gl, const gchar *error) {
  dwb_source_remove(gl);
  dwb_set_status_bar_text(VIEW(gl)->lstatus, error, &dwb.color.error, dwb.font.fd_bold);
  VIEW(gl)->status->message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, gl);
  gtk_widget_hide(dwb.gui.entry);
}/*}}}*/

/* dwb_update_status_text(GList *gl) {{{*/
void 
dwb_update_status_text(GList *gl) {
  View *v = gl ? gl->data : dwb.state.fview->data;
  GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  const gchar *uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(v->web));
  GString *string = g_string_new(uri);

  if (v->status->block) {
    gchar *js_items = v->status->block_current ? g_strdup_printf(" [%d]", v->status->items_blocked) : g_strdup(" [a]");
    g_string_append(string, js_items);
    g_free(js_items);
  }

  gdouble lower = gtk_adjustment_get_lower(a);
  gdouble upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
  gdouble value = gtk_adjustment_get_value(a); 
  gboolean back = webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(v->web));
  gboolean forward = webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(v->web));
  const gchar *bof = back && forward ? " [+-]" : back ? " [+]" : forward  ? " [-]" : "";
  g_string_append(string, bof);

  gchar *position = 
    upper == lower ? g_strdup(" [all]") : 
    value == lower ? g_strdup(" [top]") : 
    value == upper ? g_strdup(" [bot]") : 
    g_strdup_printf(" [%02d%%]", (gint)(value * 100/upper));
  g_string_append(string, position);

  dwb_set_status_bar_text(VIEW(gl)->rstatus, string->str, NULL, NULL);
  g_string_free(string, true);
  g_free(position);
}/*}}}*/

/*}}}*/

/* FUNCTIONS {{{*/

/* dwb_open_si_channel() {{{*/
void
dwb_open_si_channel() {
  dwb.misc.si_channel = g_io_channel_new_file(dwb.files.unifile, "r+", NULL);
  g_io_add_watch(dwb.misc.si_channel, G_IO_IN, (GIOFunc)dwb_handle_channel, NULL);
}/*}}}*/

/* dwb_js_get_host_blocked (gchar *) {{{*/
GList *
dwb_get_host_blocked(GList *fc, gchar *host) {
  for (GList *l = fc; l; l=l->next) {
    if (!strcmp(host, (gchar *) l->data)) {
      return l;
    }
  }
  return NULL;
}/*}}}*/

/* dwb_get_host(GList *)                  return: gchar (alloc) {{{*/
gchar * 
dwb_get_host(const gchar *uri) {
  gchar *host;
  SoupURI *soup_uri = soup_uri_new(uri);
  if (soup_uri) {
    host = g_strdup(soup_uri->host);
    soup_uri_free(soup_uri);
  }
  return host;
}/*}}}*/

/* dwb_set_js_block{{{*/
static void
dwb_set_content_block(GList *gl, WebSettings *s) {
  View *v = gl->data;

  v->status->block = s->arg.b;
}/*}}}*/

/* dwb_set_dummy{{{*/
static void
dwb_set_dummy(GList *gl, WebSettings *s) {
  return;
}/*}}}*/

/* dwb_com_focus(GList *gl) {{{*/
void 
dwb_focus(GList *gl) {
  GList *tmp = NULL;

  if (dwb.state.fview) {
    tmp = dwb.state.fview;
  }
  if (tmp) {
    dwb_view_set_normal_style(tmp);
    dwb_source_remove(tmp);
    CLEAR_COMMAND_TEXT(tmp);
  }
  dwb_grab_focus(gl);
} /*}}}*/

/* dwb_get_default_settings()         return: GHashTable {{{*/
GHashTable * 
dwb_get_default_settings() {
  GHashTable *ret = g_hash_table_new(g_str_hash, g_str_equal);
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    WebSettings *new = dwb_malloc(sizeof(WebSettings));
    *new = *s;
    gchar *value = g_strdup(s->n.first);
    g_hash_table_insert(ret, value, new);
  }
  return ret;
}/*}}}*/

void 
dwb_set_cookies(GList *l, WebSettings *s) {
  dwb.state.cookies_allowed = s->arg.b;
}

void
dwb_set_single_instance(GList *l, WebSettings *s) {
  if (!s->arg.b) {
    if (dwb.misc.si_channel) {
      g_io_channel_shutdown(dwb.misc.si_channel, true, NULL);
      g_io_channel_unref(dwb.misc.si_channel);
      dwb.misc.si_channel = NULL;
    }
  }
  else if (!dwb.misc.si_channel) {
    dwb_open_si_channel();
  }
}

/* dwb_set_proxy{{{*/
void
dwb_set_proxy(GList *l, WebSettings *s) {
  SoupURI *uri = NULL;
  gchar *message;

  if (s->arg.b) { 
    uri = dwb.misc.proxyuri;
  }
  g_object_set(G_OBJECT(dwb.misc.soupsession), "proxy-uri", uri, NULL);
  message = g_strdup_printf("Set setting proxy: %s", s->arg.b ? "true" : "false");
  dwb_set_normal_message(dwb.state.fview, message, true);
  g_free(message);
}/*}}}*/

/* dwb_entry_set_text(const gchar *) {{{*/
void 
dwb_entry_set_text(const gchar *text) {
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);
}/*}}}*/

/* dwb_focus_entry() {{{*/
void 
dwb_focus_entry() {
  gtk_widget_grab_focus(dwb.gui.entry);
  gtk_widget_show(dwb.gui.entry);
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
}/*}}}*/

/* dwb_focus_scroll (GList *){{{*/
void
dwb_focus_scroll(GList *gl) {
  View *v = gl->data;
  gtk_widget_grab_focus(v->web);
  gtk_widget_hide(dwb.gui.entry);
}/*}}}*/

/* dwb_entry_position_word_back(gint position)      return position{{{*/
gint 
dwb_entry_position_word_back(gint position) {
  gchar *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  if (position == 0) {
    return position;
  }
  gint old = position;
  while (isalnum(text[position-1]) ) {
    --position;
  }
  while (isblank(text[position-1])) {
    --position;
  }
  if (old == position) {
    --position;
  }
  return position;
}/*}}}*/

/* dwb_com_entry_history_forward(gint) {{{*/
gint 
dwb_entry_position_word_forward(gint position) {
  gchar *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  gint old = position;
  while ( isalnum(text[++position]) );
  while ( isblank(text[++position]) );
  if (old == position) {
    position++;
  }
  return position;
}/*}}}*/

/* dwb_resize(gdouble size) {{{*/
void
dwb_resize(gdouble size) {
  gint fact = dwb.state.layout & BottomStack;

  gtk_widget_set_size_request(dwb.gui.left,  (100 - size) * (fact^1),  (100 - size) *  fact);
  gtk_widget_set_size_request(dwb.gui.right, size * (fact^1), size * fact);
  dwb.state.size = size;
}/*}}}*/

/* dwb_clean_buffer{{{*/
void
dwb_clean_buffer(GList *gl) {
  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
    dwb.state.buffer = NULL;
  }
  CLEAR_COMMAND_TEXT(gl);
}/*}}}*/

/* dwb_com_reload_scripts(GList *,  WebSettings  *s) {{{*/
static void 
dwb_com_reload_scripts(GList *gl, WebSettings *s) {
  g_free(dwb.misc.scripts);
  dwb_init_scripts();
  dwb_com_reload(NULL);
}/*}}}*/

/* dwb_com_reload_layout(GList *,  WebSettings  *s) {{{*/
static void 
dwb_com_reload_layout(GList *gl, WebSettings *s) {
  dwb_init_style();
  for (GList *l = dwb.state.views; l; l=l->next) {
    if (l == dwb.state.fview) {
      dwb_view_set_active_style(l);
    }
    else {
      dwb_view_set_normal_style(l);
    }
  }
}/*}}}*/

/* dwb_get_search_engine_uri(const gchar *uri) {{{*/
gchar *
dwb_get_search_engine_uri(const gchar *uri, const gchar *text) {
  gchar *ret = NULL;
  if (uri) {
    gchar **token = g_strsplit(uri, HINT_SEARCH_SUBMIT, 2);
    ret = g_strconcat(token[0], text, token[1], NULL);
    g_strfreev(token);
  }
  return ret;
}/* }}} */

/* dwb_get_search_engine(const gchar *uri) {{{*/
gchar *
dwb_get_search_engine(const gchar *uri) {
  gchar *ret = NULL;
  if ( (!strstr(uri, ".") || strstr(uri, " ")) && !strstr(uri, "localhost:")) {
    gchar **token = g_strsplit(uri, " ", 2);
    for (GList *l = dwb.fc.searchengines; l; l=l->next) {
      Navigation *n = l->data;
      if (!strcmp(token[0], n->first)) {
        ret = dwb_get_search_engine_uri(n->second, token[1]);
        break;
      }
    }
    if (!ret) {
      ret = dwb_get_search_engine_uri(dwb.misc.default_search, uri);
    }
  }
  return ret;
}/*}}}*/

/* dwb_submit_searchengine {{{*/
void 
dwb_submit_searchengine(void) {
  gchar *com = g_strdup_printf("dwb_submit_searchengine(\"%s\")", HINT_SEARCH_SUBMIT);
  gchar *value = dwb_execute_script(com);
  if (value) {
    dwb.state.form_name = value;
  }
  g_free(com);
}/*}}}*/

/* dwb_save_searchengine {{{*/
void
dwb_save_searchengine(void) {
  gchar *text = g_strdup(GET_TEXT());
  if (text) {
    g_strstrip(text);
    if (text && strlen(text) > 0) {
      dwb_append_navigation_with_argument(&dwb.fc.searchengines, text, dwb.state.search_engine);
      dwb_set_normal_message(dwb.state.fview, "Search saved", true);
      g_free(dwb.state.search_engine);
    }
    else {
      dwb_set_error_message(dwb.state.fview, "No keyword specified, aborting.");
    }
    g_free(text);
  }
  dwb_normal_mode(false);

}/*}}}*/

/* dwb_layout_from_char {{{*/
Layout
dwb_layout_from_char(const gchar *desc) {
  gchar **token = g_strsplit(desc, " ", 0);
  gint i=0;
  Layout layout;
  while (token[i]) {
    if (!(layout & BottomStack) && !g_ascii_strcasecmp(token[i], "normal")) {
      layout |= NormalLayout;
    }
    else if (!(layout & NormalLayout) && !g_ascii_strcasecmp(token[i], "bottomstack")) {
      layout |= BottomStack;
    }
    else if (!g_ascii_strcasecmp(token[i], "maximized")) {
      layout |= Maximized;
    }
    else {
      layout = NormalLayout;
    }
    i++;
  }
  return layout;
}/*}}}*/

/* dwb_test_cookie_allowed(const gchar *)     return:  gboolean{{{*/
static gboolean 
dwb_test_cookie_allowed(SoupCookie *cookie) {
  const gchar *domain = cookie->domain;
  for (GList *l = dwb.fc.cookies_allow; l; l=l->next) {
    if (!strcmp(domain, l->data)) {
      return true;
    }
  }
  return false;
}/*}}}*/

/* dwb_save_cookies {{{*/
static void 
dwb_save_cookies() {
  SoupCookieJar *jar; 

  jar = soup_cookie_jar_text_new(dwb.files.cookies, false);
  for (GSList *l = soup_cookie_jar_all_cookies(dwb.state.cookiejar); l; l=l->next) {
    if (dwb.state.cookies_allowed || dwb_test_cookie_allowed(l->data) ) {
      soup_cookie_jar_add_cookie(jar, l->data);
    }
  }
  g_object_unref(jar);
}/*}}}*/

/* dwb_web_settings_get_value(const gchar *id, void *value) {{{*/
Arg *  
dwb_web_settings_get_value(const gchar *id) {
  WebSettings *s = g_hash_table_lookup(dwb.settings, id);
  return &s->arg;
}/*}}}*/

/* dwb_webkit_setting(GList *gl WebSettings *s) {{{*/
static void
dwb_webkit_setting(GList *gl, WebSettings *s) {
  WebKitWebSettings *settings = gl ? webkit_web_view_get_settings(WEBVIEW(gl)) : dwb.state.web_settings;
  switch (s->type) {
    case Double:  g_object_set(settings, s->n.first, s->arg.d, NULL); break;
    case Integer: g_object_set(settings, s->n.first, s->arg.i, NULL); break;
    case Boolean: g_object_set(settings, s->n.first, s->arg.b, NULL); break;
    case Char:    g_object_set(settings, s->n.first, !s->arg.p || !strcmp(s->arg.p, "null") ? NULL : (gchar*)s->arg.p  , NULL); break;
    default: return;
  }
}/*}}}*/

/* dwb_webview_property(GList, WebSettings){{{*/
static void
dwb_webview_property(GList *gl, WebSettings *s) {
  WebKitWebView *web = gl ? WEBVIEW(gl) : CURRENT_WEBVIEW();
  switch (s->type) {
    case Double:  g_object_set(web, s->n.first, s->arg.d, NULL); break;
    case Integer: g_object_set(web, s->n.first, s->arg.i, NULL); break;
    case Boolean: g_object_set(web, s->n.first, s->arg.b, NULL); break;
    case Char:    g_object_set(web, s->n.first, (gchar*)s->arg.p, NULL); break;
    default: return;
  }
}/*}}}*/

/* update_hints {{{*/
gboolean
dwb_update_hints(GdkEventKey *e) {
  gchar *buffer = NULL;
  gchar *com = NULL;

  if (e->keyval == GDK_Return) {
    com = g_strdup("dwb_get_active()");
  }
  else if (DIGIT(e)) {
    dwb.state.nummod = MIN(10*dwb.state.nummod + e->keyval - GDK_0, 314159);
    gchar *text = g_strdup_printf("hint number: %d", dwb.state.nummod);
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, text, &dwb.color.active_fg, dwb.font.fd_normal);

    com = g_strdup_printf("dwb_update_hints(\"%d\")", dwb.state.nummod);
    g_free(text);
  }
  else if (DWB_TAB_KEY(e)) {
    if (e->state & GDK_SHIFT_MASK) {
      com = g_strdup("dwb_focus_prev()");
    }
    else {
      com = g_strdup("dwb_focus_next()");
    }
  }
  else {
    com = g_strdup_printf("dwb_update_hints(\"%s\")", GET_TEXT());
  }
  if (com) {
    buffer = dwb_execute_script(com);
    g_free(com);
  }
  if (buffer) { 
    if (!strcmp("_dwb_no_hints_", buffer)) {
      dwb_set_error_message(dwb.state.fview, NO_HINTS);
      dwb_normal_mode(false);
    }
    else if (!strcmp(buffer, "_dwb_input_")) {
      dwb_execute_script("dwb_clear()");
      dwb_insert_mode(NULL);
    }
    else if  (!strcmp(buffer, "_dwb_click_")) {
      dwb.state.scriptlock = 1;
      if (dwb.state.nv != OpenDownload) {
        dwb_normal_mode(true);
      }
    }
    else  if (!strcmp(buffer, "_dwb_check_")) {
      dwb_normal_mode(true);
    }
  }
  g_free(buffer);

  return true;
}/*}}}*/

/* dwb_execute_script {{{*/
gchar * 
dwb_execute_script(const char *com) {
  View *v = dwb.state.fview->data;

  if (!com) {
    com = dwb.misc.scripts;
  }
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

/*prepend_navigation_with_argument(GList **fc, const gchar *first, const gchar *second) {{{*/
void
dwb_prepend_navigation_with_argument(GList **fc, const gchar *first, const gchar *second) {
  for (GList *l = (*fc); l; l=l->next) {
    Navigation *n = l->data;
    if (!strcmp(first, n->first)) {
      dwb_navigation_free(n);
      (*fc) = g_list_delete_link((*fc), l);
      break;
    }
  }
  Navigation *n = dwb_navigation_new(first, second);

  (*fc) = g_list_prepend((*fc), n);
}/*}}}*/

/*append_navigation_with_argument(GList **fc, const gchar *first, const gchar *second) {{{*/
void
dwb_append_navigation_with_argument(GList **fc, const gchar *first, const gchar *second) {
  for (GList *l = (*fc); l; l=l->next) {
    Navigation *n = l->data;
    if (!strcmp(first, n->first)) {
      dwb_navigation_free(n);
      (*fc) = g_list_delete_link((*fc), l);
      break;
    }
  }
  Navigation *n = dwb_navigation_new(first, second);

  (*fc) = g_list_append((*fc), n);
}/*}}}*/

/* dwb_prepend_navigation(GList *gl, GList *view) {{{*/
gboolean 
dwb_prepend_navigation(GList *gl, GList **fc) {
  WebKitWebView *w = WEBVIEW(gl);
  const gchar *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri) > 0 && strcmp(uri, SETTINGS) && strcmp(uri, KEY_SETTINGS)) {
    const gchar *title = webkit_web_view_get_title(w);
    dwb_prepend_navigation_with_argument(fc, uri, title);
    return true;
  }
  return false;

}/*}}}*/

/* dwb_save_quickmark(const gchar *key) {{{*/
void 
dwb_save_quickmark(const gchar *key) {
  WebKitWebView *w = WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web);
  const gchar *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri)) {
    const gchar *title = webkit_web_view_get_title(w);
    for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
      Quickmark *q = l->data;
      if (!strcmp(key, q->key)) {
        dwb_com_quickmark_free(q);
        dwb.fc.quickmarks = g_list_delete_link(dwb.fc.quickmarks, l);
        break;
      }
    }
    dwb.fc.quickmarks = g_list_prepend(dwb.fc.quickmarks, dwb_com_quickmark_new(uri, title, key));
    dwb_normal_mode(false);

    gchar *message = g_strdup_printf("Added quickmark: %s - %s", key, uri);
    dwb_set_normal_message(dwb.state.fview, message, true);

    g_free(message);
  }
  else {
    dwb_set_error_message(dwb.state.fview, NO_URL);
  }
}/*}}}*/

/* dwb_open_quickmark(const gchar *key){{{*/
void 
dwb_open_quickmark(const gchar *key) {
  gchar *message = NULL;
  for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
    Quickmark *q = l->data;
    if (!strcmp(key, q->key)) {
      Arg a = { .p = q->nav->first };
      dwb_load_uri(&a);

      message = g_strdup_printf("Loading quickmark %s: %s", key, q->nav->first);
      dwb_set_normal_message(dwb.state.fview, message, true);
    }
  }
  if (!message) {
    message = g_strdup_printf("No such quickmark: %s", key);
    dwb_set_error_message(dwb.state.fview, message);
  }
  dwb_normal_mode(false);
  g_free(message);
}/*}}}*/

/* dwb_tab_label_set_text {{{*/
static void
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

  if (gl == dwb.state.fview) {
    gtk_window_set_title(GTK_WINDOW(dwb.gui.window), title ? title :  dwb.misc.name);
  }
  dwb_tab_label_set_text(gl, title);

  dwb_update_status_text(gl);
}/*}}}*/

/* dwb_update_tab_label {{{*/
static void
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

  if (dwb.gui.entry) {
    gtk_widget_hide(dwb.gui.entry);
  }
  dwb.state.fview = gl;
  dwb.gui.entry = v->entry;
  dwb_view_set_active_style(gl);
  dwb_focus_scroll(gl);
  dwb_update_status(gl);
}/*}}}*/

/* dwb_new_window(Arg *arg) {{{*/
void 
dwb_new_window(Arg *arg) {
  gchar *argv[5];

  argv[0] = (gchar *)dwb.misc.prog_path;
  argv[1] = "-p"; 
  argv[2] = (gchar *)dwb.misc.profile;
  argv[3] = arg->p;
  argv[4] = NULL;
  g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}/*}}}*/

/* dwb_load_uri(const char *uri) {{{*/
void 
dwb_load_uri(Arg *arg) {
  if (dwb.state.nv == OpenNewWindow) {
    dwb_new_window(arg);
    return;
  }
  if (arg && arg->p && strlen(arg->p)) {
    gchar *uri = dwb_get_resolved_uri(arg->p);

    if (dwb.state.last_cookie) {
      soup_cookie_free(dwb.state.last_cookie); 
      dwb.state.last_cookie = NULL;
    }
    View *fview = dwb.state.fview->data;
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(fview->web), uri);
    g_free(uri);
  }
}/*}}}*/

/* dwb_update_layout() {{{*/
void
dwb_update_layout() {
  gboolean visible = gtk_widget_get_visible(dwb.gui.right);
  View *v;

  if (! (dwb.state.layout & Maximized)) {
    if (dwb.state.views->next) {
      if (!visible) {
        gtk_widget_show_all(dwb.gui.right);
        gtk_widget_hide(((View*)dwb.state.views->next->data)->entry);
      }
      v = dwb.state.views->next->data;
      if (dwb.misc.factor != 1.0) {
        webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), dwb.misc.factor);
      }
      Arg *a = dwb_web_settings_get_value("full-content-zoom");
      webkit_web_view_set_full_content_zoom(WEBKIT_WEB_VIEW(v->web), a->b);
    }
    else if (visible) {
      gtk_widget_hide(dwb.gui.right);
    }
    v = dwb.state.views->data;
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(v->web), 1.0);
    dwb_resize(dwb.state.size);
  }
  dwb_update_tab_label();
}/*}}}*/

/* dwb_eval_editing_key(GdkEventKey *) {{{*/
gboolean 
dwb_eval_editing_key(GdkEventKey *e) {
  if (!(ALPHA(e) || DIGIT(e))) {
    return false;
  }

  gchar *key = dwb_util_keyval_to_char(e->keyval);
  gboolean ret = false;

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (km->map->entry) {
      if (!strcmp(key, km->key) && CLEAN_STATE(e) == km->mod) {
        km->map->func(&km->map->arg);
        ret = true;
        break;
      }
    }
  }
  g_free(key);
  return ret;
}/*}}}*/

/* dwb_eval_key(GdkEventKey *e) {{{*/
static gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = false;
  const gchar *old = dwb.state.buffer ? dwb.state.buffer->str : NULL;
  gint keyval = e->keyval;

  if (dwb.state.scriptlock) {
    return true;
  }
  if (e->is_modifier) {
    return false;
  }
  // don't show backspace in the buffer
  if (keyval == GDK_BackSpace ) {
    if (dwb.state.mode & AutoComplete) {
      dwb_comp_clean_autocompletion();
    }
    if (dwb.state.buffer && dwb.state.buffer->str ) {
      if (dwb.state.buffer->len) {
        g_string_erase(dwb.state.buffer, dwb.state.buffer->len - 1, 1);
        dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
      }
      ret = false;
    }
    else {
      ret = true;
    }
    return ret;
  }
  gchar *key = dwb_util_keyval_to_char(keyval);
  if (!key) {
    return false;
  }


  if (!old) {
    dwb.state.buffer = g_string_new(NULL);
    old = dwb.state.buffer->str;
  }
  // nummod 
  if (DIGIT(e)) {
    if (isdigit(old[strlen(old)-1])) {
      dwb.state.nummod = MIN(10*dwb.state.nummod + e->keyval - GDK_0, 314159);
    }
    else {
      dwb.state.nummod = e->keyval - GDK_0;
    }
  }
  g_string_append(dwb.state.buffer, key);
  if (ALPHA(e) || DIGIT(e)) {
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
  }

  const gchar *buf = dwb.state.buffer->str;
  gint length = dwb.state.buffer->len;
  gint longest = 0;
  KeyMap *tmp = NULL;
  GList *coms = NULL;

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (km->map->entry) {
      continue;
    }
    gsize l = strlen(km->key);
    if (!km->key || !l) {
      continue;
    }
    if (dwb.comps.autocompletion && g_str_has_prefix(km->key, buf) && CLEAN_STATE(e) == km->mod) {
      coms = g_list_append(coms, km);
    }
    if (!strcmp(&buf[length - l], km->key) && (CLEAN_STATE(e) == km->mod)) {
      if  (!longest || l > longest) {
        longest = l;
        tmp = km;
      }
      ret = true;
    }
  }
  // autocompletion
  if (dwb.state.mode & AutoComplete) {
    dwb_comp_clean_autocompletion();
  }
  if (coms && g_list_length(coms) > 0) {
    dwb_comp_autocomplete(coms, NULL);
    ret = true;
  }
  if (tmp) {
    dwb_com_simple_command(tmp);
  }
  g_free(key);
  return ret;

}/*}}}*/

/* dwb_command_mode {{{*/
gboolean
dwb_command_mode(Arg *arg) {
  dwb_set_normal_message(dwb.state.fview, ":", false);
  dwb_focus_entry();
  dwb.state.mode = CommandMode;
  return true;
}/*}}}*/

/* dwb_insert_mode(Arg *arg) {{{*/
gboolean
dwb_insert_mode(Arg *arg) {
  if (dwb.state.mode == HintMode) {
    dwb_set_normal_message(dwb.state.fview, INSERT_MODE, true);
  }
  dwb_view_modify_style(dwb.state.fview, &dwb.color.insert_fg, &dwb.color.insert_bg, NULL, NULL, NULL, 0);

  dwb.state.mode = InsertMode;
  return true;
}/*}}}*/

/* dwb_normal_mode() {{{*/
void 
dwb_normal_mode(gboolean clean) {
  Mode mode = dwb.state.mode;

  if (dwb.state.mode == HintMode || dwb.state.mode == SearchFieldMode) {
    dwb_execute_script("dwb_clear()");
  }
  if (mode  == InsertMode) {
    dwb_view_modify_style(dwb.state.fview, &dwb.color.active_fg, &dwb.color.active_bg, NULL, NULL, NULL, 0);
    gtk_entry_set_visibility(GTK_ENTRY(dwb.gui.entry), true);
  }
  else if (mode == DownloadGetPath) {
    dwb_comp_clean_path_completion();
  }
  if (mode & CompletionMode) {
    dwb_comp_clean_completion();
  }
  if (mode & AutoComplete) {
    dwb_comp_clean_autocompletion();
  }

  dwb_focus_scroll(dwb.state.fview);

  if (clean) {
    dwb_clean_buffer(dwb.state.fview);
  }
  if (mode & NormalMode) {
    dwb_execute_script("dwb_blur()");
  }

  webkit_web_view_unmark_text_matches(CURRENT_WEBVIEW());

  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  dwb_clean_vars();
}/*}}}*/

/* dwb_get_resolved_uri(const gchar *uri) {{{*/
static gchar * 
dwb_get_resolved_uri(const gchar *uri) {
  char *tmp = NULL;
  gboolean is_file = false;
  // check if uri is a file
  if ( g_file_test(uri, G_FILE_TEST_IS_REGULAR) || (is_file = g_str_has_prefix(uri, "file://")) ) {
    tmp = is_file ? g_strdup(uri) : g_strdup_printf("file://%s", uri);
  }
  else if ( !(tmp = dwb_get_search_engine(uri)) || strstr(uri, "localhost:")) {
    tmp = g_str_has_prefix(uri, "http://") || g_str_has_prefix(uri, "https://")
      ? g_strdup(uri)
      : g_strdup_printf("http://%s", uri);
  }
  return tmp;
}
/*}}}*/

/* dwb_update_search(gboolean ) {{{*/
gboolean 
dwb_update_search(gboolean forward) {
  View *v = CURRENT_VIEW();
  WebKitWebView *web = CURRENT_WEBVIEW();
  const gchar *text = GET_TEXT();
  gint matches;
  if (strlen(text) > 0) {
    if (v->status->search_string) {
      g_free(v->status->search_string);
    }
    v->status->search_string =  g_strdup(GET_TEXT());
  }
  if (!v->status->search_string) {
    return false;
  }
  webkit_web_view_unmark_text_matches(web);
  if ( (matches = webkit_web_view_mark_text_matches(web, v->status->search_string, false, 0)) ) {
    gchar *message = g_strdup_printf("[%d matches]", matches);
    dwb_set_normal_message(dwb.state.fview, message, false);
    webkit_web_view_set_highlight_text_matches(web, true);
    g_free(message);
  }
  else {
    dwb_set_error_message(dwb.state.fview, "[0 matches]");
    return false;
  }
  return true;
}/*}}}*/

/* dwb_search {{{*/
gboolean
dwb_search(Arg *arg) {
  View *v = CURRENT_VIEW();
  gboolean forward = dwb.state.forward_search;
  if (arg) {
    dwb_update_search(arg->b);
    if (!arg->b) {
      forward = !dwb.state.forward_search;
    }
  }
  if (v->status->search_string) {
    webkit_web_view_search_text(WEBKIT_WEB_VIEW(v->web), v->status->search_string, false, forward, true);
  }
  dwb_normal_mode(false);
  return true;
}/*}}}*/
/*}}}*/

/* EXIT {{{*/

/* dwb_clean_vars() {{{*/
static void 
dwb_clean_vars() {
  dwb.state.mode = NormalMode;
  dwb.state.buffer = NULL;
  dwb.state.nummod = 0;
  dwb.state.nv = 0;
  dwb.state.scriptlock = 0;
  dwb.state.last_com_history = NULL;
  dwb.state.dl_action = Download;
}/*}}}*/

/* dwb_clean_up() {{{*/
gboolean
dwb_clean_up() {
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    g_free(m);
  }
  g_list_free(dwb.keymap);
  return true;
}/*}}}*/

/* dwb_save_navigation_fc{{{*/
void 
dwb_save_navigation_fc(GList *fc, const gchar *filename, gint length, gboolean free) {
  GString *string = g_string_new(NULL);
  gint i=0;
  for (GList *l = fc; l && (length<0 || i<length) ; l=l->next, i++)  {
    Navigation *n = l->data;
    g_string_append_printf(string, "%s %s\n", n->first, n->second);
    if (free) 
      g_free(n);
  }
  dwb_util_set_file_content(filename, string->str);
  g_string_free(string, true);
}/*}}}*/

/* dwb_save_simple_file(GList *, const gchar *){{{*/
void
dwb_save_simple_file(GList *fc, const gchar *filename, gboolean free) {
  GString *string = g_string_new(NULL);
  for (GList *l = fc; l; l=l->next)  {
    g_string_append_printf(string, "%s\n", (gchar*)l->data);
    if (free) {
      g_free(l->data);
    }
  }
  dwb_util_set_file_content(filename, string->str);
  g_string_free(string, true);
}/*}}}*/

/* dwb_save_keys() {{{*/
void
dwb_save_keys() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  gchar *content;
  gsize size;

  if (!g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No keysfile found, creating a new file.\n");
    error = NULL;
  }
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *map = l->data;
    gchar *sc = g_strdup_printf("%s %s", dwb_modmask_to_string(map->mod), map->key ? map->key : "");
    g_key_file_set_value(keyfile, dwb.misc.profile, map->map->n.first, sc);
    g_free(sc);
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    g_file_set_contents(dwb.files.keys, content, size, &error);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save keyfile: %s", error->message);
  }
  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_save_settings {{{*/
void
dwb_save_settings() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  gchar *content;
  gsize size;

  if (!g_key_file_load_from_file(keyfile, dwb.files.settings, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No settingsfile found, creating a new file.\n");
    error = NULL;
  }
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    gchar *value = dwb_util_arg_to_char(&s->arg, s->type); 
    g_key_file_set_value(keyfile, dwb.misc.profile, s->n.first, value ? value : "" );

    g_free(value);
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    g_file_set_contents(dwb.files.settings, content, size, &error);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save settingsfile: %s\n", error->message);
  }
  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_save_files() {{{*/
gboolean 
dwb_save_files(gboolean end_session) {
  dwb_save_navigation_fc(dwb.fc.bookmarks, dwb.files.bookmarks, -1, end_session);
  dwb_save_navigation_fc(dwb.fc.history, dwb.files.history, dwb.misc.history_length, end_session);
  dwb_save_navigation_fc(dwb.fc.searchengines, dwb.files.searchengines, -1, end_session);
  dwb_save_navigation_fc(dwb.fc.mimetypes, dwb.files.mimetypes, -1, end_session);

  // quickmarks
  GString *quickmarks = g_string_new(NULL); 
  for (GList *l = dwb.fc.quickmarks; l; l=l->next)  {
    Quickmark *q = l->data;
    Navigation n = *(q->nav);
    g_string_append_printf(quickmarks, "%s %s %s\n", q->key, n.first, n.second);
    if (end_session)
      dwb_com_quickmark_free(q);
  }
  dwb_util_set_file_content(dwb.files.quickmarks, quickmarks->str);
  g_string_free(quickmarks, true);

  // cookie allow
  dwb_save_simple_file(dwb.fc.cookies_allow, dwb.files.cookies_allow, end_session);
  dwb_save_simple_file(dwb.fc.content_block_allow, dwb.files.content_block_allow, end_session);

  // save keys
  dwb_save_keys();

  // save settings
  dwb_save_settings();


  // save session
  if (end_session && GET_BOOL("save-session") && dwb.state.mode != SaveSession) {
    dwb_session_save(NULL);
  }

  return true;
}
/* }}} */

/* dwb_end() {{{*/
gboolean
dwb_end() {
  if (dwb_save_files(true)) {
    if (dwb_clean_up()) {
      gtk_main_quit();
      return true;
    }
  }
  return false;
}/*}}}*/
/* }}} */

/* KEYS {{{*/

/* dwb_str_to_key(gchar *str)      return: Key{{{*/
Key 
dwb_str_to_key(gchar *str) {
  Key key = { .mod = 0, .str = NULL };
  GString *buffer = g_string_new(NULL);

  gchar **string = g_strsplit(str, " ", -1);

  for (int i=0; i<g_strv_length(string); i++)  {
    if (!g_ascii_strcasecmp(string[i], "Control")) {
      key.mod |= GDK_CONTROL_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod1")) {
      key.mod |= GDK_MOD1_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod4")) {
      key.mod |= GDK_MOD4_MASK;
    }
    else {
      g_string_append(buffer, string[i]);
    }
  }
  key.str = g_strdup(buffer->str);

  g_strfreev(string);
  g_string_free(buffer, true);

  return key;
}/*}}}*/

/* dwb_generate_keyfile {{{*/
void 
dwb_generate_keyfile() {
  dwb.files.keys = g_build_filename(g_get_user_config_dir(), dwb.misc.name, "keys", NULL);
  GKeyFile *keyfile = g_key_file_new();
  gsize l;
  GError *error = NULL;

  for (gint i=0; i<LENGTH(KEYS); i++) {
    KeyValue k = KEYS[i];
    gchar *value = k.key.mod ? g_strdup_printf("%s %s", dwb_modmask_to_string(k.key.mod), k.key.str) : g_strdup(k.key.str);
    g_key_file_set_value(keyfile, dwb.misc.profile, k.id, value);
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

/* dwb_keymap_delete(GList *, KeyValue )     return: GList * {{{*/
GList * 
dwb_keymap_delete(GList *gl, KeyValue key) {
  for (GList *l = gl; l; l=l->next) {
    KeyMap *km = l->data;
    if (!strcmp(km->map->n.first, key.id)) {
      gl = g_list_delete_link(gl, l);
      break;
    }
  }
  gl = g_list_sort(gl, (GCompareFunc)dwb_util_keymap_sort_second);
  return gl;
}/*}}}*/

void
dwb_execute_user_script(Arg *a) {
  GError *e = NULL;
  gchar *argv[3] = { a->p, (gchar*)webkit_web_view_get_uri(CURRENT_WEBVIEW()), NULL } ;
  if (!g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, &e )) {
    fprintf(stderr, "Couldn't execute %s: %s\n", (gchar*)a->p, e->message);
  }
}

/* dwb_get_scripts() {{{*/
GList * 
dwb_get_scripts() {
  GDir *dir;
  gchar *filename;
  gchar *content;
  GList *gl = NULL;

  if ( (dir = g_dir_open(dwb.files.userscripts, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      gchar *path = g_build_filename(dwb.files.userscripts, filename, NULL);

      g_file_get_contents(path, &content, NULL, NULL);
      gchar **lines = g_strsplit(content, "\n", -1);
      gint i=0;
      while (lines[i]) {
        if (g_regex_match_simple(".*dwb:", lines[i], 0, 0)) {
          gchar **line = g_strsplit(lines[i], "dwb:", 2);
          if (line[1]) {
            KeyMap *map = dwb_malloc(sizeof(KeyMap));
            Key key = dwb_str_to_key(line[1]);
            FunctionMap fm = { { filename, filename }, 0, (Func)dwb_execute_user_script, NULL, AlwaysSM, { .p = path } };
            FunctionMap *fmap = dwb_malloc(sizeof(FunctionMap));
            *fmap = fm;
            map->map = fmap;
            map->key = key.str;
            map->mod = key.mod;
            gl = g_list_prepend(gl, map);
          }
          g_strfreev(line);
          break;
        }
        i++;
      }
      g_free(content);

    }
  }
  return gl;
}/*}}}*/

/* dwb_keymap_add(GList *, KeyValue)     return: GList* {{{*/
GList *
dwb_keymap_add(GList *gl, KeyValue key) {
  gl = dwb_keymap_delete(gl, key);
  for (int i=0; i<LENGTH(FMAP); i++) {
    if (!strcmp(FMAP[i].n.first, key.id)) {
      KeyMap *keymap = dwb_malloc(sizeof(KeyMap));
      FunctionMap *fmap = &FMAP[i];
      keymap->key = key.key.str ? key.key.str : "";
      keymap->mod = key.key.mod;
      fmap->n.first = (gchar*)key.id;
      keymap->map = fmap;
      gl = g_list_prepend(gl, keymap);
      break;
    }
  }
  return gl;
}/*}}}*/
/*}}}*/

/* INIT {{{*/

/* dwb_setup_cookies() {{{*/
void
dwb_init_cookies() {
  SoupCookieJar *jar;

  dwb.state.cookiejar = soup_cookie_jar_new();
  soup_session_add_feature(dwb.misc.soupsession, SOUP_SESSION_FEATURE(dwb.state.cookiejar));
  jar = soup_cookie_jar_text_new(dwb.files.cookies, false);
  for (GSList *c = soup_cookie_jar_all_cookies(jar); c; c = c->next) {
    soup_cookie_jar_add_cookie(dwb.state.cookiejar, c->data);
  }
  g_signal_connect(dwb.state.cookiejar, "changed", G_CALLBACK(dwb_cookie_changed_cb), NULL);
  g_object_unref(jar);
}/*}}}*/

/* dwb_init_key_map() {{{*/
static void 
dwb_init_key_map() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  dwb.keymap = NULL;
  gchar **keys;

  g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error);
  if (error) {
    fprintf(stderr, "No keyfile found: %s\nUsing default values.\n", error->message);
    error = NULL;
  }
  else {
    keys = g_key_file_get_keys(keyfile, dwb.misc.profile, NULL, &error);
    if (error) {
        fprintf(stderr, "Couldn't read keyfile for profile %s: %s\nUsing default values.\n", dwb.misc.profile,  error->message);
    }
  }
  for (gint i=0; i<LENGTH(KEYS); i++) {
    KeyValue kv;
    gchar *string = g_key_file_get_value(keyfile, dwb.misc.profile, KEYS[i].id, NULL);
    if (string) {
      kv.key = dwb_str_to_key(string);
    }
    else if (KEYS[i].key.str) {
      kv.key = KEYS[i].key;
    }
    kv.id = KEYS[i].id;
    dwb.keymap = dwb_keymap_add(dwb.keymap, kv);
  }
  dwb.keymap = g_list_concat(dwb.keymap, dwb_get_scripts());
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)dwb_util_keymap_sort_second);
}/*}}}*/

/* dwb_read_settings() {{{*/
gboolean
dwb_read_settings() {
  GError *error = NULL;
  gsize length, numkeys = 0;
  gchar  **keys;
  gchar  *content;
  GKeyFile  *keyfile = g_key_file_new();
  Arg *arg;
  setlocale(LC_NUMERIC, "C");

  g_file_get_contents(dwb.files.settings, &content, &length, &error);
  if (error) {
    fprintf(stderr, "No settingsfile found: %s\nUsing default values.\n", error->message);
    error = NULL;
  }
  else {
    g_key_file_load_from_data(keyfile, content, length, G_KEY_FILE_KEEP_COMMENTS, &error);
    if (error) {
      fprintf(stderr, "Couldn't read settings file: %s\nUsing default values.\n", error->message);
      error = NULL;
    }
    else {
      keys = g_key_file_get_keys(keyfile, dwb.misc.profile, &numkeys, &error); 
      if (error) {
        fprintf(stderr, "Couldn't read settings for profile %s: %s\nUsing default values.\n", dwb.misc.profile,  error->message);
      }
    }
  }
  for (int j=0; j<LENGTH(DWB_SETTINGS); j++) {
    gboolean set = false;
    gchar *key = g_strdup(DWB_SETTINGS[j].n.first);
    for (int i=0; i<numkeys; i++) {
      gchar *value = g_key_file_get_string(keyfile, dwb.misc.profile, keys[i], NULL);
      if (!strcmp(keys[i], DWB_SETTINGS[j].n.first)) {
        WebSettings *s = dwb_malloc(sizeof(WebSettings));
        *s = DWB_SETTINGS[j];
        if ( (arg = dwb_util_char_to_arg(value, s->type)) ) {
          s->arg = *arg;
        }
        g_hash_table_insert(dwb.settings, key, s);
        set = true;
      }
      g_free(value);
    }
    if (!set) {
      g_hash_table_insert(dwb.settings, key, &DWB_SETTINGS[j]);
    }
  }
  return true;
}/*}}}*/

/* dwb_init_settings() {{{*/
static void
dwb_init_settings() {
  dwb.settings = g_hash_table_new(g_str_hash, g_str_equal);
  dwb.state.web_settings = webkit_web_settings_new();
  dwb_read_settings();
  for (GList *l =  g_hash_table_get_values(dwb.settings); l; l = l->next) {
    WebSettings *s = l->data;
    if (s->builtin) {
      s->func(NULL, s);
    }
  }
}/*}}}*/

/* dwb_init_scripts{{{*/
static void 
dwb_init_scripts() {
  GString *buffer = g_string_new(NULL);

  setlocale(LC_NUMERIC, "C");
  g_string_append_printf(buffer, "hint_letter_seq = '%s';\n",       GET_CHAR("hint-letter-seq"));
  g_string_append_printf(buffer, "hint_font_size = '%s';\n",        GET_CHAR("hint-font-size"));
  g_string_append_printf(buffer, "hint_font_weight = '%s';\n",      GET_CHAR("hint-font-weight"));
  g_string_append_printf(buffer, "hint_font_family = '%s';\n",      GET_CHAR("hint-font-family"));
  g_string_append_printf(buffer, "hint_style = '%s';\n",            GET_CHAR("hint-style"));
  g_string_append_printf(buffer, "hint_fg_color = '%s';\n",         GET_CHAR("hint-fg-color"));
  g_string_append_printf(buffer, "hint_bg_color = '%s';\n",         GET_CHAR("hint-bg-color"));
  g_string_append_printf(buffer, "hint_active_color = '%s';\n",     GET_CHAR("hint-active-color"));
  g_string_append_printf(buffer, "hint_normal_color = '%s';\n",     GET_CHAR("hint-normal-color"));
  g_string_append_printf(buffer, "hint_border = '%s';\n",           GET_CHAR("hint-border"));
  g_string_append_printf(buffer, "hint_opacity = %f;\n",            GET_DOUBLE("hint-opacity"));

  // init system scripts
  gchar *dir;
  dwb_util_get_directory_content(&buffer, dwb.files.scriptdir);
  if ( (dir = dwb_util_get_data_dir("scripts")) ) {
    dwb_util_get_directory_content(&buffer, dir);
  }
  dwb.misc.scripts = buffer->str;
  g_string_free(buffer, false);
}/*}}}*/

/* dwb_init_style() {{{*/
static void
dwb_init_style() {
  // Colors 
  // Statusbar
  gdk_color_parse(GET_CHAR("active-fg-color"), &dwb.color.active_fg);
  gdk_color_parse(GET_CHAR("active-bg-color"), &dwb.color.active_bg);
  gdk_color_parse(GET_CHAR("normal-fg-color"), &dwb.color.normal_fg);
  gdk_color_parse(GET_CHAR("normal-bg-color"), &dwb.color.normal_bg);

  // Tabs
  gdk_color_parse(GET_CHAR("tab-active-fg-color"), &dwb.color.tab_active_fg);
  gdk_color_parse(GET_CHAR("tab-active-bg-color"), &dwb.color.tab_active_bg);
  gdk_color_parse(GET_CHAR("tab-normal-fg-color"), &dwb.color.tab_normal_fg);
  gdk_color_parse(GET_CHAR("tab-normal-bg-color"), &dwb.color.tab_normal_bg);

  //InsertMode 
  gdk_color_parse(GET_CHAR("insertmode-fg-color"), &dwb.color.insert_fg);
  gdk_color_parse(GET_CHAR("insertmode-bg-color"), &dwb.color.insert_bg);

  //Downloads
  gdk_color_parse("#ffffff", &dwb.color.download_fg);
  gdk_color_parse("#000000", &dwb.color.download_bg);

  gdk_color_parse(GET_CHAR("active-completion-bg-color"), &dwb.color.active_c_bg);
  gdk_color_parse(GET_CHAR("active-completion-fg-color"), &dwb.color.active_c_fg);
  gdk_color_parse(GET_CHAR("normal-comp-bg-color"), &dwb.color.normal_c_bg);
  gdk_color_parse(GET_CHAR("normal-completion-fg-color"), &dwb.color.normal_c_fg);

  gdk_color_parse(GET_CHAR("error-color"), &dwb.color.error);

  dwb.color.settings_fg_color = GET_CHAR("settings-fg-color");
  dwb.color.settings_bg_color = GET_CHAR("settings-bg-color");

  // Fonts
  gint active_font_size = GET_INT("active-font-size");
  gint normal_font_size = GET_INT("normal-font-size");
  gchar *font = GET_CHAR("font");

  dwb.font.fd_normal = pango_font_description_from_string(font);
  dwb.font.fd_bold = pango_font_description_from_string(font);
  dwb.font.fd_oblique = pango_font_description_from_string(font);

  pango_font_description_set_absolute_size(dwb.font.fd_normal, active_font_size * PANGO_SCALE);
  pango_font_description_set_absolute_size(dwb.font.fd_bold, active_font_size * PANGO_SCALE);
  pango_font_description_set_absolute_size(dwb.font.fd_oblique, active_font_size * PANGO_SCALE);

  pango_font_description_set_weight(dwb.font.fd_normal, PANGO_WEIGHT_NORMAL);
  pango_font_description_set_weight(dwb.font.fd_bold, PANGO_WEIGHT_BOLD);
  pango_font_description_set_style(dwb.font.fd_oblique, PANGO_STYLE_OBLIQUE);

  // Fontsizes
  dwb.font.active_size = active_font_size;
  dwb.font.normal_size = normal_font_size;
  dwb.misc.settings_border = GET_CHAR("settings-border");
} /*}}}*/

/* dwb_init_gui() {{{*/
static void 
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
  gtk_window_set_default_size(GTK_WINDOW(dwb.gui.window), GET_INT("default-width"), GET_INT("default-height"));
  gtk_window_set_geometry_hints(GTK_WINDOW(dwb.gui.window), NULL, NULL, GDK_HINT_MIN_SIZE);
  g_signal_connect(dwb.gui.window, "delete-event", G_CALLBACK(dwb_end), NULL);
  g_signal_connect(dwb.gui.window, "key-press-event", G_CALLBACK(dwb_key_press_cb), NULL);
  g_signal_connect(dwb.gui.window, "key-release-event", G_CALLBACK(dwb_key_release_cb), NULL);
  gtk_widget_modify_bg(dwb.gui.window, GTK_STATE_NORMAL, &dwb.color.active_bg);

  // Main
  dwb.gui.vbox = gtk_vbox_new(false, 1);
  dwb.gui.topbox = gtk_hbox_new(true, 1);
  dwb.gui.paned = gtk_hpaned_new();
  dwb.gui.left = gtk_vbox_new(true, 0);
  dwb.gui.right = gtk_vbox_new(true, 1);

  // Paned
  GtkWidget *paned_event = gtk_event_box_new(); 
  gtk_widget_modify_bg(paned_event, GTK_STATE_NORMAL, &dwb.color.normal_bg);
  gtk_widget_modify_bg(dwb.gui.paned, GTK_STATE_NORMAL, &dwb.color.normal_bg);
  gtk_widget_modify_bg(dwb.gui.paned, GTK_STATE_PRELIGHT, &dwb.color.active_bg);
  gtk_container_add(GTK_CONTAINER(paned_event), dwb.gui.paned);
  //
  // Downloadbar 
  dwb.gui.downloadbar = gtk_hbox_new(false, 3);

  // Pack
  gtk_paned_pack1(GTK_PANED(dwb.gui.paned), dwb.gui.left, true, true);
  gtk_paned_pack2(GTK_PANED(dwb.gui.paned), dwb.gui.right, true, true);

  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.downloadbar, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.topbox, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), paned_event, true, true, 0);

  //dwb_add_view(NULL);
  gtk_container_add(GTK_CONTAINER(dwb.gui.window), dwb.gui.vbox);

  gtk_widget_show(dwb.gui.left);
  gtk_widget_show(dwb.gui.paned);
  gtk_widget_show(paned_event);
  gtk_widget_show_all(dwb.gui.topbox);

  gtk_widget_show(dwb.gui.vbox);
  gtk_widget_show(dwb.gui.window);
} /*}}}*/

/* dwb_init_file_content {{{*/
GList *
dwb_init_file_content(GList *gl, const gchar *filename, Content_Func func) {
  gchar *content = dwb_util_get_file_content(filename);
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
static void
dwb_init_files() {
  gchar *path = dwb_util_build_path();
  gchar *profile_path = g_build_filename(path, dwb.misc.profile, NULL);
  if (!g_file_test(profile_path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(profile_path, 0755);
  }
  dwb.files.bookmarks     = g_build_filename(profile_path, "bookmarks",     NULL);
  dwb.files.history       = g_build_filename(profile_path, "history",       NULL);
  dwb.files.stylesheet    = g_build_filename(profile_path, "stylesheet",    NULL);
  dwb.files.quickmarks    = g_build_filename(profile_path, "quickmarks",    NULL);
  dwb.files.session       = g_build_filename(path, "session",       NULL);
  dwb.files.searchengines = g_build_filename(path, "searchengines", NULL);
  dwb.files.keys          = g_build_filename(path, "keys",          NULL);
  dwb.files.scriptdir     = g_build_filename(path, "scripts",      NULL);
  dwb.files.userscripts   = g_build_filename(path, "userscripts",   NULL);
  dwb.files.settings      = g_build_filename(path, "settings",      NULL);
  dwb.files.mimetypes     = g_build_filename(path, "mimetypes",      NULL);
  dwb.files.cookies       = g_build_filename(profile_path, "cookies",       NULL);
  dwb.files.cookies_allow = g_build_filename(profile_path, "cookies.allow", NULL);
  dwb.files.content_block_allow      = g_build_filename(profile_path, "scripts.allow",      NULL);

  if (!g_file_test(dwb.files.scriptdir, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(dwb.files.scriptdir, 0755);
  }
  if (!g_file_test(dwb.files.userscripts, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(dwb.files.userscripts, 0755);
  }

  dwb.fc.bookmarks = dwb_init_file_content(dwb.fc.bookmarks, dwb.files.bookmarks, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.history = dwb_init_file_content(dwb.fc.history, dwb.files.history, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.quickmarks = dwb_init_file_content(dwb.fc.quickmarks, dwb.files.quickmarks, (Content_Func)dwb_com_quickmark_new_from_line); 
  dwb.fc.searchengines = dwb_init_file_content(dwb.fc.searchengines, dwb.files.searchengines, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.mimetypes = dwb_init_file_content(dwb.fc.mimetypes, dwb.files.mimetypes, (Content_Func)dwb_navigation_new_from_line);

  if (g_list_last(dwb.fc.searchengines)) {
    dwb.misc.default_search = ((Navigation*)dwb.fc.searchengines->data)->second;
  }
  dwb.fc.cookies_allow = dwb_init_file_content(dwb.fc.cookies_allow, dwb.files.cookies_allow, (Content_Func)dwb_return);
  dwb.fc.content_block_allow = dwb_init_file_content(dwb.fc.content_block_allow, dwb.files.content_block_allow, (Content_Func)dwb_return);

  g_free(path);
  g_free(profile_path);
}/*}}}*/

/* signals{{{*/
static void
dwb_handle_signal(gint s) {
  if (s == SIGALRM || s == SIGFPE || s == SIGILL || s == SIGINT || s == SIGQUIT || s == SIGTERM) {
    dwb_end();
    exit(EXIT_SUCCESS);
  }
  else if (s == SIGSEGV) {
    fprintf(stderr, "Received SIGSEGV, try to clean up.\n");
    if (dwb_session_save(NULL)) {
      fprintf(stderr, "Success.\n");
    }
    exit(EXIT_FAILURE);
  }
}

static void 
dwb_init_signals() {
  for (int i=0; i<LENGTH(signals); i++) {
    struct sigaction act, oact;
    act.sa_handler = dwb_handle_signal;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(signals[i], &act, &oact);
  }
}/*}}}*/

/* dwb_init_proxy{{{*/
static void 
dwb_init_proxy() {
  const gchar *proxy;
  gchar *newproxy;
  if ( !(proxy =  g_getenv("http_proxy")) && !(proxy =  GET_CHAR("proxy-url")) )
    return;

  if ( dwb_util_test_connect(proxy) ) {
    newproxy = g_strrstr(proxy, "http://") ? g_strdup(proxy) : g_strdup_printf("http://%s", proxy);
    dwb.misc.proxyuri = soup_uri_new(newproxy);
    //WebSettings *s = g_hash_table_lookup(dwb.settings, "proxy");
    g_object_set(G_OBJECT(dwb.misc.soupsession), "proxy-uri", GET_BOOL("proxy") ? dwb.misc.proxyuri : NULL, NULL); 
    g_free(newproxy);
  }
}/*}}}*/

/* dwb_init_vars{{{*/
static void 
dwb_init_vars() {
  dwb.misc.message_delay = GET_INT("message-delay");
  dwb.misc.history_length = GET_INT("history-length");
  dwb.misc.factor = GET_DOUBLE("factor");
  dwb.misc.startpage = GET_CHAR("startpage");

  dwb.state.size = GET_INT("size");
  dwb.state.layout = dwb_layout_from_char(GET_CHAR("layout"));
  dwb.comps.autocompletion = GET_BOOL("auto-completion");
}/*}}}*/

/* dwb_init() {{{*/
void dwb_init() {

  dwb_clean_vars();
  dwb.state.views = NULL;
  dwb.state.fview = NULL;
  dwb.state.last_cookie = NULL;
  dwb.comps.completions = NULL; 
  dwb.comps.active_comp = NULL;
  dwb.misc.max_c_items = MAX_COMPLETIONS;


  dwb_init_key_map();
  dwb_init_style();
  dwb_init_scripts();
  dwb_init_gui();

  dwb.misc.soupsession = webkit_get_default_session();
  // TODO fix got_headers_cb
#ifdef GOT_HEADERS
  g_signal_connect(dwb.misc.soupsession, "request-started", G_CALLBACK(dwb_soup_request_cb), NULL);
#endif
  dwb_init_proxy();
  dwb_init_cookies();
  dwb_init_vars();

  if (dwb.state.layout & BottomStack) {
    Arg a = { .n = dwb.state.layout };
    dwb_com_set_orientation(&a);
  }
  if (restore && dwb_session_restore(restore));
  else if (dwb.misc.argc > 0) {
    for (gint i=0; i<dwb.misc.argc; i++) {
      Arg a = { .p = dwb.misc.argv[i] };
      dwb_add_view(&a);
    }
  }
  else {
    dwb_add_view(NULL);
  }
} /*}}}*/ /*}}}*/

/* FIFO {{{*/
/* dwb_parse_command_line(const gchar *line) {{{*/
void 
dwb_parse_command_line(const gchar *line) {
  gchar **token = g_strsplit(line, " ", 2);

  if (!token[0]) 
    return;

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    if (!strcmp(m->map->n.first, token[0])) {
      Arg a = m->map->arg;
      if (token[1]) {
        g_strstrip(token[1]);
        m->map->arg.p = token[1];
      }
      if (gtk_widget_has_focus(dwb.gui.entry) && m->map->entry) {
        m->map->func(&m->map->arg);
      }
      else {
        dwb_com_simple_command(m);
      }
      m->map->arg = a;
      break;
    }
  }
  g_strfreev(token);
  dwb_normal_mode(true);
}/*}}}*/

/* dwb_handle_channel {{{*/
gboolean
dwb_handle_channel(GIOChannel *c, GIOCondition condition, void *data) {
  gchar *line = NULL;

  g_io_channel_read_line(c, &line, NULL, NULL, NULL);
  if (line) {
    g_strstrip(line);
    dwb_parse_command_line(line);
    g_io_channel_flush(c, NULL);
    g_free(line);
  }
  return true;
}/*}}}*/

/* dwb_init_fifo{{{*/
void 
dwb_init_fifo() {
  FILE *ff;
  gchar *path = dwb_util_build_path();
  dwb.files.unifile = g_build_filename(path, "dwb-uni.fifo", NULL);
  g_free(path);

  dwb.misc.si_channel = NULL;

  if (!g_file_test(dwb.files.unifile, G_FILE_TEST_EXISTS)) {
    mkfifo(dwb.files.unifile, 0666);
  }
  gint fd = open(dwb.files.unifile, O_WRONLY | O_NONBLOCK);
  if ( (ff = fdopen(fd, "w")) ) {
    if (dwb.misc.argc) {
      for (int i=0; i<dwb.misc.argc; i++) {
        fprintf(ff, "add_view %s\n", dwb.misc.argv[i]);
      }
    }
    else {
      fprintf(ff, "add_view\n");
    }
    fclose(ff);
    exit(EXIT_SUCCESS);
  }
  close(fd);
  if (GET_BOOL("single-instance")) {
    dwb_open_si_channel();
  }
}/*}}}*/
/*}}}*/

int main(gint argc, gchar *argv[]) {
  dwb.misc.name = NAME;
  dwb.misc.profile = "default";
  dwb.misc.argc = 0;
  dwb.misc.prog_path = argv[0];
  gint last = 0;

  gtk_init(&argc, &argv);

  dwb_init_files();
  dwb_init_settings();
  if (GET_BOOL("save-session") && argc == 1) {
    restore = "default";
  }

  if (!g_thread_supported()) {
    g_thread_init(NULL);
  }
  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-') {
        if (argv[i][1] == 'l') {
          dwb_session_list();
        }
        else if (argv[i][1] == 'p' && argv[i++]) {
          dwb.misc.profile = argv[i];
        }
        else if (argv[i][1] == 'r' ) {
          if (!argv[i+1] || argv[i+1][0] == '-') {
            restore = "default";
          }
          else {
            restore = argv[++i];
          }
        }
      }
      else {
        last = i;
        break;
      }
    }
  }
  if (last) {
    dwb.misc.argv = &argv[last];
    dwb.misc.argc = g_strv_length(dwb.misc.argv);
  }
  dwb_init_fifo();
  dwb_init_signals();
  dwb_init();
  gtk_main();
  return EXIT_SUCCESS;
}
