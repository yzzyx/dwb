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

#include "dwb.h"
#include "completion.h"
#include "commands.h"
#include "view.h"
#include "util.h"
#include "download.h"
#include "session.h"
#include "config.h"
#include "icon.xpm"


/* DECLARATIONS {{{*/
static void dwb_webkit_setting(GList *, WebSettings *);
static void dwb_webview_property(GList *, WebSettings *);
static void dwb_set_cookies(GList *l, WebSettings *s);
static void dwb_set_dummy(GList *, WebSettings *);
static void dwb_set_content_block(GList *, WebSettings *);
static void dwb_set_startpage(GList *, WebSettings *);
static void dwb_set_plugin_blocker(GList *, WebSettings *);
static void dwb_set_content_block_regex(GList *, WebSettings *);
static void dwb_set_message_delay(GList *, WebSettings *);
static void dwb_set_history_length(GList *, WebSettings *);
static void dwb_set_adblock(GList *, WebSettings *);
static void dwb_set_hide_tabbar(GList *, WebSettings *);

static void dwb_clean_buffer(GList *);
static TabBarVisible dwb_eval_tabbar_visible(const char *arg);

static gboolean dwb_command_mode(Arg *arg);
static void dwb_reload_scripts(GList *,  WebSettings *);
static void dwb_reload_layout(GList *,  WebSettings *);
static gboolean dwb_test_cookie_allowed(SoupCookie *);
static char * dwb_test_userscript(const char *);
static void dwb_save_cookies(void);

static void dwb_open_channel(const char *);
static void dwb_open_si_channel(void);
static gboolean dwb_handle_channel(GIOChannel *c, GIOCondition condition, void *data);

static gboolean dwb_eval_key(GdkEventKey *);


static void dwb_tab_label_set_text(GList *, const char *);

static void dwb_save_quickmark(const char *);
static void dwb_open_quickmark(const char *);


static void dwb_init_proxy(void);
static void dwb_init_key_map(void);
static void dwb_init_settings(void);
static void dwb_init_style(void);
static void dwb_init_scripts(void);
static void dwb_init_gui(void);
static void dwb_init_vars(void);

static Navigation * dwb_get_search_completion(const char *text);

static void dwb_clean_vars(void);
//static gboolean dwb_end(void);
/*}}}*/

static GdkNativeWindow embed = 0;
static int signals[] = { SIGFPE, SIGILL, SIGINT, SIGQUIT, SIGTERM, SIGALRM, SIGSEGV};
static int MAX_COMPLETIONS = 11;
static char *restore = NULL;

/* FUNCTION_MAP{{{*/
static FunctionMap FMAP [] = {
  { { "add_view",              "Add a new view",                    }, 1, (Func)dwb_add_view,                NULL,                              ALWAYS_SM,     { .p = NULL }, },
  { { "allow_cookie",          "Cookie allowed",                    }, 0, (Func)dwb_com_allow_cookie,        "No cookie in current context",    POST_SM, },
  { { "bookmark",              "Bookmark current page",             }, 1, (Func)dwb_com_bookmark,            NO_URL,                            POST_SM, },
  { { "bookmarks",             "Bookmarks",                         }, 0, (Func)dwb_com_bookmarks,           "No Bookmarks",                    NEVER_SM,     { .n = OPEN_NORMAL }, }, 
  { { "bookmarks_nv",          "Bookmarks new view",                }, 0, (Func)dwb_com_bookmarks,           "No Bookmarks",                    NEVER_SM,     { .n = OPEN_NEW_VIEW }, },
  { { "bookmarks_nw",          "Bookmarks new window",              }, 0, (Func)dwb_com_bookmarks,           "No Bookmarks",                    NEVER_SM,     { .n = OPEN_NEW_WINDOW}, }, 
  { { "new_view",              "New view for next navigation",      }, 0, (Func)dwb_com_new_window_or_view,  NULL,                              NEVER_SM,     { .n = OPEN_NEW_VIEW}, }, 
  { { "new_window",            "New window for next navigation",    }, 0, (Func)dwb_com_new_window_or_view,  NULL,                              NEVER_SM,     { .n = OPEN_NEW_WINDOW}, }, 
  { { "command_mode",          "Enter command mode",                }, 0, (Func)dwb_command_mode,            NULL,                              POST_SM, },
  { { "decrease_master",       "Decrease master area",              }, 1, (Func)dwb_com_resize_master,       "Cannot decrease further",         ALWAYS_SM,    { .n = 5 } },
  { { "download_hint",         "Download via hints",                }, 0, (Func)dwb_com_show_hints,          NO_HINTS,                          NEVER_SM,    { .n = OPEN_DOWNLOAD }, },
  { { "find_backward",         "Find Backward ",                    }, 0, (Func)dwb_com_find,                NO_URL,                            NEVER_SM,     { .b = false }, },
  { { "find_forward",          "Find Forward ",                     }, 0, (Func)dwb_com_find,                NO_URL,                            NEVER_SM,     { .b = true }, },
  { { "find_next",             "Find next",                         }, 0, (Func)dwb_search,                  "No matches",                      ALWAYS_SM,     { .b = true }, },
  { { "find_previous",         "Find previous",                     }, 0, (Func)dwb_search,                  "No matches",                      ALWAYS_SM,     { .b = false }, },
  { { "focus_input",           "Focus input",                       }, 1, (Func)dwb_com_focus_input,        "No input found in current context",      ALWAYS_SM, },
  { { "focus_next",            "Focus next view",                   }, 0, (Func)dwb_com_focus_next,          "No other view",                   ALWAYS_SM, },
  { { "focus_prev",            "Focus previous view",               }, 0, (Func)dwb_com_focus_prev,          "No other view",                   ALWAYS_SM, },
  { { "focus_nth_view",        "Focus nth view",                    }, 0, (Func)dwb_com_focus_nth_view,       "No such view",                   ALWAYS_SM,  { 0 } },
  { { "hint_mode",             "Follow hints",                      }, 0, (Func)dwb_com_show_hints,          NO_HINTS,                          NEVER_SM,    { .n = OPEN_NORMAL }, },
  { { "hint_mode_nv",          "Follow hints (new view)",           }, 0, (Func)dwb_com_show_hints,          NO_HINTS,                          NEVER_SM,    { .n = OPEN_NEW_VIEW }, },
  { { "hint_mode_nw",          "Follow hints (new window)",         }, 0, (Func)dwb_com_show_hints,          NO_HINTS,                          NEVER_SM,    { .n = OPEN_NEW_WINDOW }, },
  { { "history_back",          "Go Back",                           }, 1, (Func)dwb_com_history_back,        "Beginning of History",            ALWAYS_SM, },
  { { "history_forward",       "Go Forward",                        }, 1, (Func)dwb_com_history_forward,     "End of History",                  ALWAYS_SM, },
  { { "increase_master",       "Increase master area",              }, 1, (Func)dwb_com_resize_master,       "Cannot increase further",         ALWAYS_SM,    { .n = -5 } },
  { { "insert_mode",           "Insert Mode",                       }, 0, (Func)dwb_insert_mode,             NULL,                              ALWAYS_SM, },
  { { "open",                  "Open",                              }, 1, (Func)dwb_com_open,                NULL,                              NEVER_SM,   { .n = OPEN_NORMAL,      .p = NULL } },
  { { "open_nv",               "Viewopen",                          }, 1, (Func)dwb_com_open,                NULL,                              NEVER_SM,   { .n = OPEN_NEW_VIEW,     .p = NULL } },
  { { "open_nw",               "Winopen",                           }, 1, (Func)dwb_com_open,                NULL,                              NEVER_SM,   { .n = OPEN_NEW_WINDOW,     .p = NULL } },
  { { "open_quickmark",        "Quickmark",                         }, 0, (Func)dwb_com_quickmark,           NO_URL,                            NEVER_SM,   { .n = QUICK_MARK_OPEN, .i=OPEN_NORMAL }, },
  { { "open_quickmark_nv",     "Quickmark-new-view",                }, 0, (Func)dwb_com_quickmark,           NULL,                              NEVER_SM,    { .n = QUICK_MARK_OPEN, .i=OPEN_NEW_VIEW }, },
  { { "open_quickmark_nw",     "Quickmark-new-window",              }, 0, (Func)dwb_com_quickmark,           NULL,                              NEVER_SM,    { .n = QUICK_MARK_OPEN, .i=OPEN_NEW_WINDOW }, },
  { { "open_start_page",       "Open startpage",                    }, 1, (Func)dwb_com_open_startpage,      "No startpage set",                ALWAYS_SM, },
  { { "push_master",           "Push to master area",               }, 1, (Func)dwb_com_push_master,         "No other view",                   ALWAYS_SM, },
  { { "reload",                "Reload",                            }, 1, (Func)dwb_com_reload,              NULL,                              ALWAYS_SM, },
  { { "remove_view",           "Close view",                        }, 1, (Func)dwb_com_remove_view,         NULL,                              ALWAYS_SM, },
  { { "save_quickmark",        "Save a quickmark for this page",    }, 0, (Func)dwb_com_quickmark,           NO_URL,                            NEVER_SM,    { .n = QUICK_MARK_SAVE }, },
  { { "save_search_field",     "Add a new searchengine",            }, 0, (Func)dwb_com_add_search_field,    "No input in current context",     POST_SM, },
  { { "scroll_bottom",         "Scroll to  bottom of the page",     }, 1, (Func)dwb_com_scroll,              NULL,                              ALWAYS_SM,    { .n = SCROLL_BOTTOM }, },
  { { "scroll_down",           "Scroll down",                       }, 0, (Func)dwb_com_scroll,              "Bottom of the page",              ALWAYS_SM,    { .n = SCROLL_DOWN, }, },
  { { "scroll_left",           "Scroll left",                       }, 0, (Func)dwb_com_scroll,              "Left side of the page",           ALWAYS_SM,    { .n = SCROLL_LEFT }, },
  { { "scroll_halfpage_down",  "Scroll one-half page down",         }, 0, (Func)dwb_com_scroll,              "Bottom of the page",              ALWAYS_SM,    { .n = SCROLL_HALF_PAGE_DOWN, }, },
  { { "scroll_halfpage_up",    "Scroll one-half page up",           }, 0, (Func)dwb_com_scroll,              "Top of the page",                 ALWAYS_SM,    { .n = SCROLL_HALF_PAGE_UP, }, },
  { { "scroll_page_down",      "Scroll one page down",              }, 0, (Func)dwb_com_scroll,              "Bottom of the page",              ALWAYS_SM,    { .n = SCROLL_PAGE_DOWN, }, },
  { { "scroll_page_up",        "Scroll one page up",                }, 0, (Func)dwb_com_scroll,              "Top of the page",                 ALWAYS_SM,    { .n = SCROLL_PAGE_UP, }, },
  { { "scroll_right",          "Scroll left",                       }, 0, (Func)dwb_com_scroll,              "Right side of the page",          ALWAYS_SM,    { .n = SCROLL_RIGHT }, },
  { { "scroll_top",            "Scroll to the top of the page",     }, 1, (Func)dwb_com_scroll,              NULL,                              ALWAYS_SM,    { .n = SCROLL_TOP }, },
  { { "scroll_up",             "Scroll up",                         }, 0, (Func)dwb_com_scroll,              "Top of the page",                 ALWAYS_SM,    { .n = SCROLL_UP, }, },
  { { "set_global_setting",    "Set global property",               }, 0, (Func)dwb_com_set_setting,         NULL,                              NEVER_SM,    { .n = APPLY_GLOBAL } },
  { { "set_key",               "Set keybinding",                    }, 0, (Func)dwb_com_set_key,             NULL,                              NEVER_SM,    { 0 } },
  { { "set_setting",           "Set property",                      }, 0, (Func)dwb_com_set_setting,         NULL,                              NEVER_SM,    { .n = APPLY_PER_VIEW } },
  { { "show_global_settings",  "Show global settings",              }, 1, (Func)dwb_com_show_settings,       NULL,                              ALWAYS_SM,    { .n = APPLY_GLOBAL } },
  { { "show_keys",             "Key configuration",                 }, 1, (Func)dwb_com_show_keys,           NULL,                              ALWAYS_SM, },
  { { "show_settings",         "Settings",                          }, 1, (Func)dwb_com_show_settings,       NULL,                              ALWAYS_SM,    { .n = APPLY_PER_VIEW } },
  { { "toggle_bottomstack",    "Toggle bottomstack",                }, 1, (Func)dwb_com_set_orientation,     NULL,                              ALWAYS_SM, },
  { { "toggle_encoding",       "Toggle Custom encoding",            }, 1, (Func)dwb_com_toggle_custom_encoding,    NULL,                        POST_SM, },
  { { "toggle_maximized",      "Toggle maximized",                  }, 1, (Func)dwb_com_toggle_maximized,    NULL,                              ALWAYS_SM, },
  { { "view_source",           "View source",                       }, 1, (Func)dwb_com_view_source,         NULL,                              ALWAYS_SM, },
  { { "zoom_in",               "Zoom in",                           }, 1, (Func)dwb_com_zoom_in,             "Cannot zoom in further",          ALWAYS_SM, },
  { { "zoom_normal",           "Zoom 100%",                         }, 1, (Func)dwb_com_set_zoom_level,      NULL,                              ALWAYS_SM,    { .d = 1.0,   .p = NULL } },
  { { "zoom_out",              "Zoom out",                          }, 1, (Func)dwb_com_zoom_out,            "Cannot zoom out further",         ALWAYS_SM, },
  // yank and paste
  { { "yank",                  "Yank",                              }, 1, (Func)dwb_com_yank,                 NO_URL,                 POST_SM,  { .p = GDK_NONE } },
  { { "yank_primary",          "Yank to Primary selection",         }, 1, (Func)dwb_com_yank,                 NO_URL,                 POST_SM,  { .p = GDK_SELECTION_PRIMARY } },
  { { "paste",                 "Paste",                             }, 1, (Func)dwb_com_paste,               "Clipboard is empty",    ALWAYS_SM, { .n = OPEN_NORMAL, .p = GDK_NONE } },
  { { "paste_primary",         "Paste primary selection",           }, 1, (Func)dwb_com_paste,               "No primary selection",  ALWAYS_SM, { .n = OPEN_NORMAL, .p = GDK_SELECTION_PRIMARY } },
  { { "paste_nv",              "Paste, new view",                   }, 1, (Func)dwb_com_paste,               "Clipboard is empty",    ALWAYS_SM, { .n = OPEN_NEW_VIEW, .p = GDK_NONE } },
  { { "paste_primary_nv",      "Paste primary selection, new view", }, 1, (Func)dwb_com_paste,               "No primary selection",  ALWAYS_SM, { .n = OPEN_NEW_VIEW, .p = GDK_SELECTION_PRIMARY } },
  { { "paste_nw",              "Paste, new window",                   }, 1, (Func)dwb_com_paste,             "Clipboard is empty",    ALWAYS_SM, { .n = OPEN_NEW_WINDOW, .p = GDK_NONE } },
  { { "paste_primary_nw",      "Paste primary selection, new window", }, 1, (Func)dwb_com_paste,             "No primary selection",  ALWAYS_SM, { .n = OPEN_NEW_WINDOW, .p = GDK_SELECTION_PRIMARY } },

  { { "save_session",          "Save current session", },              1, (Func)dwb_com_save_session,        NULL,                              ALWAYS_SM,  { .n = NORMAL_MODE } },
  { { "save_named_session",    "Save current session with name", },    0, (Func)dwb_com_save_session,        NULL,                              POST_SM,  { .n = SAVE_SESSION } },
  { { "save",                  "Save all configuration files", },      1, (Func)dwb_com_save_files,        NULL,                              POST_SM,  { .n = SAVE_SESSION } },
  { { "undo",                  "Undo closing last tab", },             1, (Func)dwb_com_undo,              "No more closed views",                              POST_SM },

  //Entry editing
  { { "entry_delete_word",      "Delete word", },                      0,  (Func)dwb_com_entry_delete_word,            NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_delete_letter",    "Delete a single letter", },           0,  (Func)dwb_com_entry_delete_letter,          NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_delete_line",      "Delete to beginning of the line", },  0,  (Func)dwb_com_entry_delete_line,            NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_word_forward",     "Move cursor forward on word", },      0,  (Func)dwb_com_entry_word_forward,           NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_word_back",        "Move cursor back on word", },         0,  (Func)dwb_com_entry_word_back,              NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_history_back",     "Command history back", },             0,  (Func)dwb_com_entry_history_back,           NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_history_forward",  "Command history forward", },          0,  (Func)dwb_com_entry_history_forward,        NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "download_set_execute",   "Complete binaries", },                0, (Func)dwb_dl_set_execute,        NULL,       ALWAYS_SM,  { 0 }, true, }, 
  { { "complete_history",       "Complete browsing history", },       0, (Func)dwb_com_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_HISTORY }, true, }, 
  { { "complete_bookmarks",     "Complete bookmarks", },              0, (Func)dwb_com_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_BOOKMARK }, true, }, 
  { { "complete_commands",      "Complete command history", },        0, (Func)dwb_com_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_INPUT }, true, }, 
  { { "complete_searchengines", "Complete searchengines", },          0, (Func)dwb_com_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_SEARCH }, true, }, 
  { { "complete_userscript",    "Complete userscripts", },            0, (Func)dwb_com_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_USERSCRIPT }, true, }, 

  { { "spell_checking",        "Setting: spell checking",         },   0, (Func)dwb_com_toggle_property,     NULL,                              POST_SM,    { .p = "enable-spell-checking" } },
  { { "scripts",               "Setting: scripts",                },   1, (Func)dwb_com_toggle_property,     NULL,                              POST_SM,    { .p = "enable-scripts" } },
  { { "auto_shrink_images",    "Toggle autoshrink images",        },   0, (Func)dwb_com_toggle_property,     NULL,                    POST_SM,    { .p = "auto-shrink-images" } },
  { { "autoload_images",       "Toggle autoload images",          },   0, (Func)dwb_com_toggle_property,     NULL,                    POST_SM,    { .p = "auto-load-images" } },
  { { "autoresize_window",     "Toggle autoresize window",        },   0, (Func)dwb_com_toggle_property,     NULL,                    POST_SM,    { .p = "auto-resize-window" } },
  { { "caret_browsing",        "Toggle caret browsing",           },   0, (Func)dwb_com_toggle_property,     NULL,                    POST_SM,    { .p = "enable-caret-browsing" } },
  { { "default_context_menu",  "Toggle enable default context menu",           }, 0, (Func)dwb_com_toggle_property,     NULL,       POST_SM,    { .p = "enable-default-context-menu" } },
  { { "file_access_from_file_uris",     "Toggle file access from file uris",   }, 0, (Func)dwb_com_toggle_property,     NULL,                  POST_SM, { .p = "enable-file-acces-from-file-uris" } },
  { { "universal file_access_from_file_uris",   "Toggle universal file access from file uris",   }, 0, (Func)dwb_com_toggle_property,  NULL,   POST_SM, { .p = "enable-universal-file-acces-from-file-uris" } },
  { { "java_applets",          "Toggle java applets",             }, 0, (Func)dwb_com_toggle_property,     NULL,                    POST_SM,    { .p = "enable-java-applets" } },
  { { "plugins",               "Toggle plugins",                  }, 1, (Func)dwb_com_toggle_property,     NULL,                    POST_SM,    { .p = "enable-plugins" } },
  { { "private_browsing",      "Toggle private browsing",         }, 0, (Func)dwb_com_toggle_property,     NULL,                    POST_SM,    { .p = "enable-private-browsing" } },
  { { "page_cache",            "Toggle page cache",               }, 0, (Func)dwb_com_toggle_property,     NULL,                    POST_SM,    { .p = "enable-page-cache" } },
  { { "js_can_open_windows",   "Toggle Javascript can open windows automatically", }, 0, (Func)dwb_com_toggle_property,     NULL,   POST_SM,    { .p = "javascript-can-open-windows-automatically" } },
  { { "enforce_96_dpi",        "Toggle enforce a resolution of 96 dpi", },    0, (Func)dwb_com_toggle_property,     NULL,           POST_SM,    { .p = "enforce-96-dpi" } },
  { { "print_backgrounds",     "Toggle print backgrounds", },      0,    (Func)dwb_com_toggle_property,    NULL,                    POST_SM,    { .p = "print-backgrounds" } },
  { { "resizable_text_areas",  "Toggle resizable text areas", },   0,  (Func)dwb_com_toggle_property,      NULL,                    POST_SM,    { .p = "resizable-text-areas" } },
  { { "tab_cycle",             "Toggle tab cycles through elements", },  0,   (Func)dwb_com_toggle_property,     NULL,              POST_SM,    { .p = "tab-key-cycles-through-elements" } },
  { { "proxy",                 "Toggle proxy",                    }, 1, (Func)dwb_com_toggle_proxy,        NULL,                    POST_SM,    { 0 } },
  { { "toggle_block_content",   "Toggle block content for current domain" },  1, (Func) dwb_com_toggle_block_content, NULL,                  POST_SM,    { 0 } }, 
  { { "allow_content",         "Allow scripts for current domain in the current session" },  1, (Func) dwb_com_allow_content, NULL,              POST_SM,    { 0 } }, 
  { { "allow_plugins",         "Allow plugins for this domain" },      1, (Func) dwb_com_allow_plugins, NO_URL,              POST_SM,    { 0 } }, 
  { { "print",                 "Print page" },                         1, (Func) dwb_com_print, NULL,                             POST_SM,    { 0 } }, 
  { { "execute_userscript",    "Execute userscript" },                 1, (Func) dwb_com_execute_userscript, "No userscripts available",     NEVER_SM,    { 0 } }, 
};/*}}}*/

/* DWB_SETTINGS {{{*/
/* SETTINGS_ARRAY {{{*/
  // { name,                                     description,                                                  builtin, global, type,  argument,                  set-function
static WebSettings DWB_SETTINGS[] = {
  { { "auto-load-images",			                   "Autoload images", },                                         true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "auto-resize-window",			                 "Autoresize window", },                                       true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "auto-shrink-images",			                 "Autoshrink images", },                                       true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "cursive-font-family",			               "Cursive font family", },                                     true, false,  CHAR,    { .p = "serif"           }, (S_Func) dwb_webkit_setting, },
  { { "default-encoding",			                   "Default encoding", },                                        true, false,  CHAR,    { .p = NULL      }, (S_Func) dwb_webkit_setting, },
  { { "default-font-family",			               "Default font family", },                                     true, false,  CHAR,    { .p = "sans-serif"      }, (S_Func) dwb_webkit_setting, },
  { { "default-font-size",			                 "Default font size", },                                       true, false,  INTEGER, { .i = 12                }, (S_Func) dwb_webkit_setting, },
  { { "default-monospace-font-size",			       "Default monospace font size", },                             true, false,  INTEGER, { .i = 10                }, (S_Func) dwb_webkit_setting, },
  { { "enable-caret-browsing",			             "Caret Browsing", },                                          true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-default-context-menu",			       "Enable default context menu", },                             true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-developer-extras",			           "Enable developer extras",    },                              true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-dom-paste",			                   "Enable DOM paste", },                                        true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-file-access-from-file-uris",			 "File access from file uris", },                              true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-html5-database",			             "Enable HTML5-database" },                                    true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-html5-local-storage",			         "Enable HTML5 local storage", },                              true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-java-applet",			                 "Java Applets", },                                            true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-offline-web-application-cache",		 "Offline web application cache", },                           true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-page-cache",			                 "Page cache", },                                              true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-plugins",			                     "Plugins", },                                                 true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-private-browsing",			           "Private Browsing", },                                        true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-scripts",			                     "Script", },                                                  true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-site-specific-quirks",			       "Site specific quirks", },                                    true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-spatial-navigation",			         "Spatial navigation", },                                      true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-spell-checking",			             "Spell checking", },                                          true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "enable-universal-access-from-file-uris",	 "Universal access from file  uris", },                        true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enable-xss-auditor",			                 "XSS auditor", },                                             true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "enforce-96-dpi",			                     "Enforce 96 dpi", },                                          true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "fantasy-font-family",			               "Fantasy font family", },                                     true, false,  CHAR,    { .p = "serif"           }, (S_Func) dwb_webkit_setting, },
  { { "javascript-can-access-clipboard",			   "Javascript can access clipboard", },                         true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "javascript-can-open-windows-automatically", "Javascript can open windows automatically", },             true, false,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting, },
  { { "minimum-font-size",			                 "Minimum font size", },                                       true, false,  INTEGER, { .i = 5                 }, (S_Func) dwb_webkit_setting, },
  { { "minimum-logical-font-size",			         "Minimum logical font size", },                               true, false,  INTEGER, { .i = 5                 }, (S_Func) dwb_webkit_setting, },
  { { "monospace-font-family",			             "Monospace font family", },                                   true, false,  CHAR,    { .p = "monospace"       }, (S_Func) dwb_webkit_setting, },
  { { "print-backgrounds",			                 "Print backgrounds", },                                       true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "resizable-text-areas",			               "Resizable text areas", },                                    true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },
  { { "sans-serif-font-family",			             "Sans serif font family", },                                  true, false,  CHAR,    { .p = "sans-serif"      }, (S_Func) dwb_webkit_setting, },
  { { "serif-font-family",			                 "Serif font family", },                                       true, false,  CHAR,    { .p = "serif"           }, (S_Func) dwb_webkit_setting, },
  { { "spell-checking-languages",			           "Spell checking languages", },                                true, false,  CHAR,    { .p = NULL              }, (S_Func) dwb_webkit_setting, },
  { { "tab-key-cycles-through-elements",			   "Tab cycles through elements in insert mode", },              true, false,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting, },

  { { "user-agent",			                         "User agent", },                                              true, false,  CHAR,    { .p = NULL              }, (S_Func) dwb_webkit_setting, },
  { { "user-stylesheet-uri",			               "User stylesheet uri", },                                     true, false,  CHAR,    { .p = NULL              }, (S_Func) dwb_webkit_setting, },
  { { "zoom-step",			                         "Zoom Step", },                                               true, false,  DOUBLE,  { .d = 0.1               }, (S_Func) dwb_webkit_setting, },
  { { "custom-encoding",                         "Custom encoding", },                                         false, false, CHAR,    { .p = NULL           }, (S_Func) dwb_webview_property, },
  { { "editable",                                "Content editable", },                                        false, false, BOOLEAN, { .b = false             }, (S_Func) dwb_webview_property, },
  { { "full-content-zoom",                       "Full content zoom", },                                       false, false, BOOLEAN, { .b = false             }, (S_Func) dwb_webview_property, },
  { { "zoom-level",                              "Zoom level", },                                              false, false, DOUBLE,  { .d = 1.0               }, (S_Func) dwb_webview_property, },
  { { "proxy",                                   "HTTP-proxy", },                                              false, true,  BOOLEAN, { .b = false              },  (S_Func) dwb_set_proxy, },
  { { "proxy-url",                               "HTTP-proxy url", },                                          false, true,  CHAR,    { .p = NULL              },   (S_Func) dwb_init_proxy, },
  { { "cookies",                                  "All Cookies allowed", },                                     false, true,  BOOLEAN, { .b = false             }, (S_Func) dwb_set_cookies, },

  { { "active-fg-color",                         "UI: Active view foreground", },                              false, true,  COLOR_CHAR, { .p = "#ffffff"         },    (S_Func) dwb_reload_layout, },
  { { "active-bg-color",                         "UI: Active view background", },                              false, true,  COLOR_CHAR, { .p = "#000000"         },    (S_Func) dwb_reload_layout, },
  { { "normal-fg-color",                         "UI: Inactive view foreground", },                            false, true,  COLOR_CHAR, { .p = "#cccccc"         },    (S_Func) dwb_reload_layout, },
  { { "normal-bg-color",                         "UI: Inactive view background", },                            false, true,  COLOR_CHAR, { .p = "#505050"         },    (S_Func) dwb_reload_layout, },

  { { "tab-active-fg-color",                     "UI: Active view tabforeground", },                           false, true,  COLOR_CHAR, { .p = "#ffffff"         },    (S_Func) dwb_reload_layout, },
  { { "tab-active-bg-color",                     "UI: Active view tabbackground", },                           false, true,  COLOR_CHAR, { .p = "#000000"         },    (S_Func) dwb_reload_layout, },
  { { "tab-normal-fg-color",                     "UI: Inactive view tabforeground", },                         false, true,  COLOR_CHAR, { .p = "#cccccc"         },    (S_Func) dwb_reload_layout, },
  { { "tab-normal-bg-color",                     "UI: Inactive view tabbackground", },                         false, true,  COLOR_CHAR, { .p = "#505050"         },    (S_Func) dwb_reload_layout, },
  { { "hide-tabbar",                             "Hide tabbar (never, always, tiled)", },                      false, true,  CHAR,      { .p = "never"         },      (S_Func) dwb_set_hide_tabbar, },
  { { "tabbed-browsing",                         "Enable tabbed browsing", },                                  false, true,  BOOLEAN,      { .b = true         },      (S_Func) dwb_set_dummy, },

  { { "active-completion-fg-color",                    "UI: Completion active foreground", },                        false, true,  COLOR_CHAR, { .p = "#53868b"         }, (S_Func) dwb_init_style, },
  { { "active-completion-bg-color",                    "UI: Completion active background", },                        false, true,  COLOR_CHAR, { .p = "#000000"         }, (S_Func) dwb_init_style, },
  { { "normal-completion-fg-color",                    "UI: Completion inactive foreground", },                      false, true,  COLOR_CHAR, { .p = "#eeeeee"         }, (S_Func) dwb_init_style, },
  { { "normal-completion-bg-color",                    "UI: Completion inactive background", },                      false, true,  COLOR_CHAR, { .p = "#151515"         }, (S_Func) dwb_init_style, },

  { { "insertmode-fg-color",                         "UI: Insertmode foreground", },                               false, true,  COLOR_CHAR, { .p = "#000000"         }, (S_Func) dwb_init_style, },
  { { "insertmode-bg-color",                         "UI: Insertmode background", },                               false, true,  COLOR_CHAR, { .p = "#dddddd"         }, (S_Func) dwb_init_style, },
  { { "error-color",                             "UI: Error color", },                                         false, true,  COLOR_CHAR, { .p = "#ff0000"         }, (S_Func) dwb_init_style, },

  { { "settings-fg-color",                       "UI: Settings view foreground", },                            false, true,  COLOR_CHAR, { .p = "#ffffff"         }, (S_Func) dwb_init_style, },
  { { "settings-bg-color",                       "UI: Settings view background", },                            false, true,  COLOR_CHAR, { .p = "#151515"         }, (S_Func) dwb_init_style, },
  { { "settings-border",                         "UI: Settings view border", },                                false, true,  CHAR,      { .p = "1px dotted black"}, (S_Func) dwb_init_style, },
 
  { { "active-font-size",                        "UI: Active view fontsize", },                                false, true,  INTEGER, { .i = 12                },   (S_Func) dwb_reload_layout, },
  { { "normal-font-size",                        "UI: Inactive view fontsize", },                              false, true,  INTEGER, { .i = 10                },   (S_Func) dwb_reload_layout, },
  
  { { "font",                                    "UI: Font", },                                                false, true,  CHAR, { .p = "monospace"          },   (S_Func) dwb_reload_layout, },
   
  { { "hint-letter-seq",                       "Hints: Letter sequence for letter hints", },             false, true,  CHAR, { .p = "FDSARTGBVECWXQYIOPMNHZULKJ"  }, (S_Func) dwb_reload_scripts, },
  { { "hint-style",                              "Hints: Hintstyle (letter or number)", },                     false, true,  CHAR, { .p = "letter"            },     (S_Func) dwb_reload_scripts, },
  { { "hint-font-size",                          "Hints: Font size", },                                        false, true,  CHAR, { .p = "12px"              },     (S_Func) dwb_reload_scripts, },
  { { "hint-font-weight",                        "Hints: Font weight", },                                      false, true,  CHAR, { .p = "normal"            },     (S_Func) dwb_reload_scripts, },
  { { "hint-font-family",                        "Hints: Font family", },                                      false, true,  CHAR, { .p = "monospace"         },     (S_Func) dwb_reload_scripts, },
  { { "hint-fg-color",                           "Hints: Foreground color", },                                 false, true,  COLOR_CHAR, { .p = "#ffffff"      },     (S_Func) dwb_reload_scripts, },
  { { "hint-bg-color",                           "Hints: Background color", },                                 false, true,  COLOR_CHAR, { .p = "#000088"      },     (S_Func) dwb_reload_scripts, },
  { { "hint-active-color",                       "Hints: Active link color", },                                false, true,  COLOR_CHAR, { .p = "#00ff00"      },     (S_Func) dwb_reload_scripts, },
  { { "hint-normal-color",                       "Hints: Inactive link color", },                              false, true,  COLOR_CHAR, { .p = "#ffff99"      },     (S_Func) dwb_reload_scripts, },
  { { "hint-border",                             "Hints: Hint Border", },                                      false, true,  CHAR, { .p = "2px dashed #000000"    }, (S_Func) dwb_reload_scripts, },
  { { "hint-opacity",                            "Hints: Hint Opacity", },                                     false, true,  DOUBLE, { .d = 0.75         },          (S_Func) dwb_reload_scripts, },
  { { "auto-completion",                         "Show possible keystrokes", },                                false, true,  BOOLEAN, { .b = true         },     (S_Func)dwb_comp_set_autcompletion, },
  { { "startpage",                               "Default homepage", },                                        false, true,  CHAR,    { .p = "about:blank" },        (S_Func)dwb_set_startpage, }, 
  { { "single-instance",                         "Single instance", },                                         false, true,  BOOLEAN,    { .b = false },          (S_Func)dwb_set_single_instance, }, 
  { { "save-session",                            "Autosave sessions", },                                       false, true,  BOOLEAN,    { .b = false },          (S_Func)dwb_set_dummy, }, 
  
  { { "content-block-regex",   "Mimetypes that will be blocked", },     false, false,  CHAR,   { .p = "(application|text)/(x-)?(shockwave-flash|javascript)" }, (S_Func) dwb_set_content_block_regex, }, 
  { { "block-content",                        "Block ugly content", },                                        false, false,  BOOLEAN,    { .b = false },        (S_Func) dwb_set_content_block, }, 
  { { "plugin-blocker",                       "Flashblocker", },                                              false, false,  BOOLEAN,    { .b = false },        (S_Func) dwb_set_plugin_blocker, }, 

  // downloads
  { { "download-external-command",                        "Downloads: External download program", },                               false, true,  CHAR, 
              { .p = "xterm -e wget 'dwb_uri' -O 'dwb_output' --load-cookies 'dwb_cookies'"   },     (S_Func)dwb_set_dummy, },
  { { "download-use-external-program",           "Downloads: Use external download program", },                           false, true,  BOOLEAN, { .b = false         },    (S_Func)dwb_set_dummy, },

  { { "complete-history",                        "Complete browsing history", },                              false, true,  BOOLEAN, { .b = true         },     (S_Func)dwb_init_vars, },
  { { "complete-bookmarks",                        "Complete bookmarks", },                                     false, true,  BOOLEAN, { .b = true         },     (S_Func)dwb_init_vars, },
  { { "complete-searchengines",                   "Complete searchengines", },                                     false, true,  BOOLEAN, { .b = true         },     (S_Func)dwb_init_vars, },
  { { "complete-commands",                        "Complete input history", },                                     false, true,  BOOLEAN, { .b = true         },     (S_Func)dwb_init_vars, },
  { { "complete-userscripts",                        "Complete userscripts", },                                     false, true,  BOOLEAN, { .b = true         },     (S_Func)dwb_init_vars, },

  { { "use-fifo",                        "Create a fifo pipe for communication", },                            false, true,  BOOLEAN, { .b = false         },     (S_Func)dwb_set_dummy },
    
  { { "default-width",                           "Default width", },                                           false, true,  INTEGER, { .i = 800          }, (S_Func)dwb_set_dummy, },
  { { "default-height",                          "Default height", },                                           false, true,  INTEGER, { .i = 600          }, (S_Func)dwb_set_dummy, },
  { { "message-delay",                           "Message delay", },                                           false, true,  INTEGER, { .i = 2          }, (S_Func) dwb_set_message_delay, },
  { { "history-length",                          "History length", },                                          false, true,  INTEGER, { .i = 500          }, (S_Func) dwb_set_history_length, },
  { { "size",                                    "UI: Default tiling area size (in %)", },                     false, true,  INTEGER, { .i = 30          }, (S_Func)dwb_set_dummy, },
  { { "factor",                                  "UI: Default Zoom factor of tiling area", },                  false, true,  DOUBLE, { .d = 0.3          }, (S_Func)dwb_set_dummy, },
  { { "layout",                                  "UI: Default layout (Normal, Bottomstack, Maximized)", },     false, true,  CHAR, { .p = "Normal MAXIMIZED" },  (S_Func)dwb_set_dummy, },
  { { "mail-program",                            "Mail program", },                                            false, true,  CHAR, { .p = "xterm -e mutt 'dwb_uri'" }, (S_Func)dwb_set_dummy }, 
  { { "adblocker",                               "Block advertisements via a filterlist", },                   false, false,  BOOLEAN, { .b = false }, (S_Func)dwb_set_adblock }, 
};/*}}}*/

/* SETTINGS_FUNCTIONS{{{*/
/* dwb_set_dummy{{{*/
static void
dwb_set_dummy(GList *gl, WebSettings *s) {
  return;
}/*}}}*/

/* dwb_set_dummy{{{*/
static void
dwb_set_adblock(GList *gl, WebSettings *s) {
  View *v = gl->data;
  v->status->adblocker = s->arg.b;
}/*}}}*/

/* dwb_set_dummy{{{*/
static void
dwb_set_hide_tabbar(GList *gl, WebSettings *s) {
  dwb.state.tabbar_visible = dwb_eval_tabbar_visible(s->arg.p);
  dwb_toggle_tabbar();
}/*}}}*/

/* dwb_set_startpage(GList *l, WebSettings *){{{*/
static void 
dwb_set_startpage(GList *l, WebSettings *s) {
  dwb.misc.startpage = s->arg.p;
}/*}}}*/

/* dwb_set_plugin_blocker(GList *l, WebSettings *){{{*/
static void 
dwb_set_plugin_blocker(GList *l, WebSettings *s) {
  View *v = l->data;
  v->status->plugin_blocker = s->arg.b;
}/*}}}*/

/* dwb_set_content_block_regex(GList *l, WebSettings *){{{*/
static void 
dwb_set_content_block_regex(GList *l, WebSettings *s) {
  dwb.misc.content_block_regex = s->arg.p;
}/*}}}*/

/* dwb_set_message_delay(GList *l, WebSettings *){{{*/
static void 
dwb_set_message_delay(GList *l, WebSettings *s) {
  dwb.misc.message_delay = s->arg.i;
}/*}}}*/

/* dwb_set_history_length(GList *l, WebSettings *){{{*/
static void 
dwb_set_history_length(GList *l, WebSettings *s) {
  dwb.misc.history_length = s->arg.i;
}/*}}}*/

/* dwb_set_cookies (GList *, WebSettings *s) {{{*/
void 
dwb_set_cookies(GList *l, WebSettings *s) {
  dwb.state.cookies_allowed = s->arg.b;
}/*}}}*/

/* dwb_set_single_instance(GList *l, WebSettings *s){{{*/
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
}/*}}}*/

/* dwb_set_proxy{{{*/
void
dwb_set_proxy(GList *l, WebSettings *s) {
  SoupURI *uri = NULL;

  if (s->arg.b) { 
    uri = dwb.misc.proxyuri;
  }
  g_object_set(G_OBJECT(dwb.misc.soupsession), "proxy-uri", uri, NULL);
  dwb_set_normal_message(dwb.state.fview, true, "Set setting proxy: %s", s->arg.b ? "true" : "false");
}/*}}}*/

/* dwb_webkit_setting(GList *gl WebSettings *s) {{{*/
static void
dwb_webkit_setting(GList *gl, WebSettings *s) {
  WebKitWebSettings *settings = gl ? webkit_web_view_get_settings(WEBVIEW(gl)) : dwb.state.web_settings;
  switch (s->type) {
    case DOUBLE:  g_object_set(settings, s->n.first, s->arg.d, NULL); break;
    case INTEGER: g_object_set(settings, s->n.first, s->arg.i, NULL); break;
    case BOOLEAN: g_object_set(settings, s->n.first, s->arg.b, NULL); break;
    case CHAR:    g_object_set(settings, s->n.first, !s->arg.p || !strcmp(s->arg.p, "null") ? NULL : (char*)s->arg.p  , NULL); break;
    default: return;
  }
}/*}}}*/

/* dwb_webview_property(GList, WebSettings){{{*/
static void
dwb_webview_property(GList *gl, WebSettings *s) {
  WebKitWebView *web = gl ? WEBVIEW(gl) : CURRENT_WEBVIEW();
  switch (s->type) {
    case DOUBLE:  g_object_set(web, s->n.first, s->arg.d, NULL); break;
    case INTEGER: g_object_set(web, s->n.first, s->arg.i, NULL); break;
    case BOOLEAN: g_object_set(web, s->n.first, s->arg.b, NULL); break;
    case CHAR:    g_object_set(web, s->n.first, (char*)s->arg.p, NULL); break;
    default: return;
  }
}/*}}}*//*}}}*/

/* dwb_set_content_block{{{*/
static void
dwb_set_content_block(GList *gl, WebSettings *s) {
  View *v = gl->data;

  v->status->block = s->arg.b;
}/*}}}*/
/*}}}*/


/* CALLBACKS {{{*/

/* dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {{{*/
static gboolean 
dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  gboolean ret = false;

  char *key = dwb_util_keyval_to_char(e->keyval);
  if (e->keyval == GDK_Escape) {
    dwb_normal_mode(true);
    ret = false;
  }
  else if (dwb.state.mode == INSERT_MODE) {
    if (CLEAN_STATE(e) & GDK_MODIFIER_MASK) {
      ret = dwb_eval_key(e);
    }
  }
  else if (dwb.state.mode == QUICK_MARK_SAVE) {
    if (key) {
      dwb_save_quickmark(key);
    }
    ret = true;
  }
  else if (dwb.state.mode == QUICK_MARK_OPEN) {
    if (key) {
      dwb_open_quickmark(key);
    }
    ret = true;
  }
  else if (gtk_widget_has_focus(dwb.gui.entry) || dwb.state.mode & COMPLETION_MODE) {
    ret = false;
  }
  else if (DWB_TAB_KEY(e)) {
    dwb_comp_autocomplete(dwb.keymap, e);
    ret = true;
  }
  else {
    if (dwb.state.mode & AUTO_COMPLETE) {
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
  FREE(key);
  return ret;
}/*}}}*/

/* dwb_key_release_cb {{{*/
static gboolean 
dwb_key_release_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  if (DWB_TAB_KEY(e)) {
    return true;
  }
  return false;
}/*}}}*/

/* dwb_cookie_changed_cb {{{*/
static void 
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
static gpointer 
dwb_hide_message(GList *gl) {
  CLEAR_COMMAND_TEXT(gl);
  return NULL;
}/*}}}*/

/* dwb_set_normal_message {{{*/
void 
dwb_set_normal_message(GList *gl, gboolean hide, const char  *text, ...) {
  va_list arg_list; 
  View *v = gl->data;
  
  va_start(arg_list, text);
  char message[STRING_LENGTH];
  vsprintf(message, text, arg_list);
  va_end(arg_list);

  dwb_set_status_bar_text(v->lstatus, message, &dwb.color.active_fg, dwb.font.fd_bold);

  dwb_source_remove(gl);
  if (hide) {
    v->status->message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, gl);
  }
}/*}}}*/

/* dwb_set_error_message {{{*/
void 
dwb_set_error_message(GList *gl, const char *error, ...) {
  va_list arg_list; 

  va_start(arg_list, error);
  char message[STRING_LENGTH];
  vsprintf(message, error, arg_list);
  va_end(arg_list);

  dwb_source_remove(gl);

  dwb_set_status_bar_text(VIEW(gl)->lstatus, message, &dwb.color.error, dwb.font.fd_bold);
  VIEW(gl)->status->message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, gl);
  gtk_widget_hide(dwb.gui.entry);
}/*}}}*/

/* dwb_update_status_text(GList *gl) {{{*/
void 
dwb_update_status_text(GList *gl, GtkAdjustment *a) {
  View *v = gl ? gl->data : dwb.state.fview->data;
    
  if (!a) {
    a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  }

  const char *uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(v->web));
  GString *string = g_string_new(uri);

  if (v->status->block) {
    char *js_items = v->status->block_current ? g_strdup_printf(" [%d]", v->status->items_blocked) : g_strdup(" [a]");
    g_string_append(string, js_items);
    FREE(js_items);
  }

  gboolean back = webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(v->web));
  gboolean forward = webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(v->web));
  const char *bof = back && forward ? " [+-]" : back ? " [+]" : forward  ? " [-]" : "";
  g_string_append(string, bof);

  if (a) {
    double lower = gtk_adjustment_get_lower(a);
    double upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
    double value = gtk_adjustment_get_value(a); 
    char *position = 
      upper == lower ? g_strdup(" [all]") : 
      value == lower ? g_strdup(" [top]") : 
      value == upper ? g_strdup(" [bot]") : 
      g_strdup_printf(" [%02d%%]", (int)(value * 100/upper));
    g_string_append(string, position);
    FREE(position);
  }

  dwb_set_status_bar_text(VIEW(gl)->rstatus, string->str, NULL, NULL);
  g_string_free(string, true);
}/*}}}*/

/*}}}*/

/* FUNCTIONS {{{*/

static TabBarVisible 
dwb_eval_tabbar_visible(const char *arg) {
  if (!strcasecmp(arg, "never")) {
    return HIDE_TB_NEVER;
  }
  else if (!strcasecmp(arg, "always")) {
    return HIDE_TB_ALWAYS;
  }
  else if (!strcasecmp(arg, "tiled")) {
    return HIDE_TB_TILED;
  }
  return 0;
}
void
dwb_toggle_tabbar(void) {
  gboolean visible = gtk_widget_get_visible(dwb.gui.topbox);
  if (visible ) {
    if (dwb.state.tabbar_visible != HIDE_TB_NEVER && 
        (dwb.state.tabbar_visible == HIDE_TB_ALWAYS || (HIDE_TB_TILED && !(dwb.state.layout & MAXIMIZED)))) {
      gtk_widget_hide(dwb.gui.topbox);
    }
  }
  else if (dwb.state.tabbar_visible != HIDE_TB_ALWAYS && 
      (dwb.state.tabbar_visible == HIDE_TB_NEVER || (HIDE_TB_TILED && (dwb.state.layout & MAXIMIZED)))) {
      gtk_widget_show(dwb.gui.topbox);
  }
}
/* dwb_eval_completion_type {{{*/
CompletionType 
dwb_eval_completion_type(void) {
  switch (dwb.state.mode) {
    case SETTINGS_MODE:  return COMP_SETTINGS;
    case KEY_MODE:       return COMP_KEY;
    case COMMAND_MODE:   return COMP_COMMAND;
    default:            return COMP_NONE;
  }
}/*}}}*/

/* dwb_clean_load_end(GList *) {{{*/
void 
dwb_clean_load_end(GList *gl) {
  View *v = gl->data;
  if (v->status->mimetype) {
    g_free(v->status->mimetype);
    v->status->mimetype = NULL;
  }
}/*}}}*/

/* dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *)   return: (alloc) Navigation* {{{*/
Navigation *
dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *item) {
  Navigation *n = NULL;
  const char *uri;
  const char *title;

  if (item) {
    uri = webkit_web_history_item_get_uri(item);
    title = webkit_web_history_item_get_title(item);
    n = dwb_navigation_new(uri, title);
  }
  return n;
}/*}}}*/

/* dwb_open_channel (const char *filename) {{{*/
static void dwb_open_channel(const char *filename) {
  dwb.misc.si_channel = g_io_channel_new_file(filename, "r+", NULL);
  g_io_add_watch(dwb.misc.si_channel, G_IO_IN, (GIOFunc)dwb_handle_channel, NULL);
}/*}}}*/

/* dwb_test_userscript (const char *)         return: char* (alloc) or NULL {{{*/
static char * 
dwb_test_userscript(const char *filename) {
  char *path = g_build_filename(dwb.files.userscripts, filename, NULL); 

  if (g_file_test(path, G_FILE_TEST_IS_REGULAR) || 
      (g_str_has_prefix(filename, dwb.files.userscripts) && g_file_test(filename, G_FILE_TEST_IS_REGULAR) && (path = g_strdup(filename))) ) {
    return path;
  }
  else {
    g_free(path);
  }
  return NULL;
}/*}}}*/

/* dwb_open_si_channel() {{{*/
static void
dwb_open_si_channel() {
  dwb_open_channel(dwb.files.unifile);
}/*}}}*/

/* dwb_js_get_host_blocked (char *)  return: GList * {{{*/
GList *
dwb_get_host_blocked(GList *fc, char *host) {
  for (GList *l = fc; l; l=l->next) {
    if (!strcmp(host, (char *) l->data)) {
      return l;
    }
  }
  return NULL;
}/*}}}*/

/* dwb_get_host(GList *)                  return: char* (alloc) {{{*/
char * 
dwb_get_host(const char *uri) {
  char *host;
  SoupURI *soup_uri = soup_uri_new(uri);
  if (soup_uri) {
    host = g_strdup(soup_uri->host);
    soup_uri_free(soup_uri);
  }
  return host;
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
    char *value = g_strdup(s->n.first);
    g_hash_table_insert(ret, value, new);
  }
  return ret;
}/*}}}*/

/* dwb_entry_set_text(const char *) {{{*/
void 
dwb_entry_set_text(const char *text) {
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

/* dwb_entry_position_word_back(int position)      return position{{{*/
int 
dwb_entry_position_word_back(int position) {
  char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  if (position == 0) {
    return position;
  }
  int old = position;
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

/* dwb_com_entry_history_forward(int) {{{*/
int 
dwb_entry_position_word_forward(int position) {
  char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  int old = position;
  while ( isalnum(text[++position]) );
  while ( isblank(text[++position]) );
  if (old == position) {
    position++;
  }
  return position;
}/*}}}*/

/* dwb_handle_mail(const char *uri)        return: true if it is a mail-address{{{*/
gboolean 
dwb_handle_mail(const char *uri) {
  if (g_str_has_prefix(uri, "mailto:")) {
    const char *program;
    char *command;
    if ( !(program = GET_CHAR("mail-program")) ) {
      return true;
    }
    if ( !(command = dwb_util_string_replace(program, "dwb_uri", uri))) {
      return true;
    }

    g_spawn_command_line_async(command, NULL);
    free(command);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_resize(double size) {{{*/
void
dwb_resize(double size) {
  int fact = dwb.state.layout & BOTTOM_STACK;

  gtk_widget_set_size_request(dwb.gui.left,  (100 - size) * (fact^1),  (100 - size) *  fact);
  gtk_widget_set_size_request(dwb.gui.right, size * (fact^1), size * fact);
  dwb.state.size = size;
}/*}}}*/

/* dwb_clean_buffer{{{*/
static void
dwb_clean_buffer(GList *gl) {
  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
    dwb.state.buffer = NULL;
  }
  CLEAR_COMMAND_TEXT(gl);
}/*}}}*/

/* dwb_reload_scripts(GList *,  WebSettings  *s) {{{*/
static void 
dwb_reload_scripts(GList *gl, WebSettings *s) {
  FREE(dwb.misc.systemscripts);
  dwb_init_scripts();
  dwb_com_reload(NULL);
}/*}}}*/

/* dwb_reload_layout(GList *,  WebSettings  *s) {{{*/
static void 
dwb_reload_layout(GList *gl, WebSettings *s) {
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

/* dwb_get_search_engine_uri(const char *uri) {{{*/
static char *
dwb_get_search_engine_uri(const char *uri, const char *text) {
  char *ret = NULL;
  if (uri) {
    char **token = g_strsplit(uri, HINT_SEARCH_SUBMIT, 2);
    ret = g_strconcat(token[0], text, token[1], NULL);
    g_strfreev(token);
  }
  return ret;
}/* }}} */

/* dwb_get_search_engine(const char *uri) {{{*/
static char *
dwb_get_search_engine(const char *uri) {
  char *ret = NULL;
  if ( (!strstr(uri, ".") || strstr(uri, " ")) && !strstr(uri, "localhost:")) {
    char **token = g_strsplit(uri, " ", 2);
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
  char *com = g_strdup_printf("dwb_submit_searchengine(\"%s\")", HINT_SEARCH_SUBMIT);
  char *value = dwb_execute_script(com, true);
  if (value) {
    dwb.state.form_name = value;
  }
  FREE(com);
}/*}}}*/

/* dwb_save_searchengine {{{*/
void
dwb_save_searchengine(void) {
  char *text = g_strdup(GET_TEXT());
  dwb_normal_mode(false);

  if (!text)
    return;

  g_strstrip(text);
  if (text && strlen(text) > 0) {
    dwb_append_navigation_with_argument(&dwb.fc.searchengines, text, dwb.state.search_engine);
    dwb_set_normal_message(dwb.state.fview, true, "Search saved");
    if (dwb.state.search_engine) {
      if (!dwb.misc.default_search) {
        dwb.misc.default_search = dwb.state.search_engine;
      }
      else  {
        g_free(dwb.state.search_engine);
        dwb.state.search_engine = NULL;
      }
    }
  }
  else {
    dwb_set_error_message(dwb.state.fview, "No keyword specified, aborting.");
  }
  g_free(text);

}/*}}}*/

/* dwb_layout_from_char {{{*/
static Layout
dwb_layout_from_char(const char *desc) {
  char **token = g_strsplit(desc, " ", 0);
  int i=0;
  Layout layout;
  while (token[i]) {
    if (!(layout & BOTTOM_STACK) && !g_ascii_strcasecmp(token[i], "normal")) {
      layout |= NORMAL_LAYOUT;
    }
    else if (!(layout & NORMAL_LAYOUT) && !g_ascii_strcasecmp(token[i], "bottomstack")) {
      layout |= BOTTOM_STACK;
    }
    else if (!g_ascii_strcasecmp(token[i], "maximized")) {
      layout |= MAXIMIZED;
    }
    else {
      layout = NORMAL_LAYOUT;
    }
    i++;
  }
  g_strfreev(token);
  return layout;
}/*}}}*/

/* dwb_test_cookie_allowed(const char *)     return:  gboolean{{{*/
static gboolean 
dwb_test_cookie_allowed(SoupCookie *cookie) {
  for (GList *l = dwb.fc.cookies_allow; l; l=l->next) {
    if (soup_cookie_domain_matches(cookie, l->data)) {
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

/* dwb_web_settings_get_value(const char *id, void *value) {{{*/
static Arg *  
dwb_web_settings_get_value(const char *id) {
  WebSettings *s = g_hash_table_lookup(dwb.settings, id);
  return &s->arg;
}/*}}}*/

/* update_hints {{{*/
gboolean
dwb_update_hints(GdkEventKey *e) {
  char *buffer = NULL;
  char *com = NULL;

  if (e->keyval == GDK_Return) {
    com = g_strdup("dwb_get_active()");
  }
  else if (DIGIT(e)) {
    dwb.state.nummod = MIN(10*dwb.state.nummod + e->keyval - GDK_0, 314159);
    char *text = g_strdup_printf("hint number: %d", dwb.state.nummod);
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, text, &dwb.color.active_fg, dwb.font.fd_normal);

    com = g_strdup_printf("dwb_update_hints(\"%d\")", dwb.state.nummod);
    FREE(text);
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
    buffer = dwb_execute_script(com, true);
    g_free(com);
  }
  if (buffer) { 
    if (!strcmp("_dwb_no_hints_", buffer)) {
      dwb_set_error_message(dwb.state.fview, NO_HINTS);
      dwb_normal_mode(false);
    }
    else if (!strcmp(buffer, "_dwb_input_")) {
      webkit_web_view_execute_script(CURRENT_WEBVIEW(), "dwb_clear()");
      dwb_insert_mode(NULL);
    }
    else if  (!strcmp(buffer, "_dwb_click_")) {
      dwb.state.scriptlock = 1;
      if (dwb.state.nv != OPEN_DOWNLOAD) {
        dwb_normal_mode(true);
      }
    }
    else  if (!strcmp(buffer, "_dwb_check_")) {
      dwb_normal_mode(true);
    }
  }
  FREE(buffer);

  return true;
}/*}}}*/

/* dwb_execute_script {{{*/
char * 
dwb_execute_script(const char *com, gboolean return_char) {
  View *v = dwb.state.fview->data;

  if (!com) {
    com = dwb.misc.scripts;
  }
  if (!return_char) {
    webkit_web_view_execute_script(WEBKIT_WEB_VIEW(v->web), com); 
    return NULL;
  }

  JSValueRef exc, eval_ret;
  size_t length;
  char *ret = NULL;

  WebKitWebFrame *frame =  webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(v->web));
  JSContextRef context = webkit_web_frame_get_global_context(frame);
  JSStringRef text = JSStringCreateWithUTF8CString(com);
  eval_ret = JSEvaluateScript(context, text, JSContextGetGlobalObject(context), NULL, 0, &exc);
  JSStringRelease(text);

  if (eval_ret && return_char) {
    JSStringRef string = JSValueToStringCopy(context, eval_ret, NULL);
    length = JSStringGetMaximumUTF8CStringSize(string);
    ret = g_new(char, length);
    JSStringGetUTF8CString(string, ret, length);
    JSStringRelease(string);
  }
  return ret;
}
/*}}}*/

/*prepend_navigation_with_argument(GList **fc, const char *first, const char *second) {{{*/
void
dwb_prepend_navigation_with_argument(GList **fc, const char *first, const char *second) {
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

/*append_navigation_with_argument(GList **fc, const char *first, const char *second) {{{*/
void
dwb_append_navigation_with_argument(GList **fc, const char *first, const char *second) {
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
  const char *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri) > 0 && strcmp(uri, SETTINGS) && strcmp(uri, KEY_SETTINGS)) {
    const char *title = webkit_web_view_get_title(w);
    dwb_prepend_navigation_with_argument(fc, uri, title);
    return true;
  }
  return false;

}/*}}}*/

/* dwb_save_quickmark(const char *key) {{{*/
static void 
dwb_save_quickmark(const char *key) {
  WebKitWebView *w = WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web);
  const char *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri)) {
    const char *title = webkit_web_view_get_title(w);
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

    dwb_set_normal_message(dwb.state.fview, true, "Added quickmark: %s - %s", key, uri);
  }
  else {
    dwb_set_error_message(dwb.state.fview, NO_URL);
  }
}/*}}}*/

/* dwb_open_quickmark(const char *key){{{*/
static void 
dwb_open_quickmark(const char *key) {
  for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
    Quickmark *q = l->data;
    if (!strcmp(key, q->key)) {
      Arg a = { .p = q->nav->first };
      dwb_load_uri(&a);
      dwb_set_normal_message(dwb.state.fview, true, "Loading quickmark %s: %s", key, q->nav->first);
      dwb_normal_mode(false);
      return;
    }
  }
  dwb_set_error_message(dwb.state.fview, "No such quickmark: %s", key);
  dwb_normal_mode(false);
}/*}}}*/

/* dwb_tab_label_set_text {{{*/
static void
dwb_tab_label_set_text(GList *gl, const char *text) {
  View *v = gl->data;
  const char *uri = text ? text : webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
  char *escaped = g_strdup_printf("%d : %s", g_list_position(dwb.state.views, gl), uri ? uri : "about:blank");
  gtk_label_set_text(GTK_LABEL(v->tablabel), escaped);

  FREE(escaped);
}/*}}}*/


/* dwb_update_status(GList *gl) {{{*/
void 
dwb_update_status(GList *gl) {
  View *v = gl->data;
  char *filename = NULL;
  WebKitWebView *w = WEBKIT_WEB_VIEW(v->web);
  const char *title = webkit_web_view_get_title(w);
  if (!title && v->status->mimetype && strcmp(v->status->mimetype, "text/html")) {
    const char *uri = webkit_web_view_get_uri(w);
    filename = g_path_get_basename(uri);
    title = filename;
  }
  if (!title) {
    title = dwb.misc.name;
  }

  if (gl == dwb.state.fview) {
    gtk_window_set_title(GTK_WINDOW(dwb.gui.window), title);
  }
  dwb_tab_label_set_text(gl, title);

  dwb_update_status_text(gl, NULL);
  FREE(filename);
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
 
  // TODO remove with new webkit version
  webkit_web_view_execute_script(WEBVIEW(gl), dwb.misc.scripts);
}/*}}}*/

/* dwb_new_window(Arg *arg) {{{*/
void 
dwb_new_window(Arg *arg) {
  char *argv[6];

  argv[0] = (char *)dwb.misc.prog_path;
  argv[1] = "-p"; 
  argv[2] = (char *)dwb.misc.profile;
  argv[3] = "-n";
  argv[4] = arg->p;
  argv[5] = NULL;
  g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}/*}}}*/

/* dwb_load_uri(const char *uri) {{{*/
void 
dwb_load_uri(Arg *arg) {
  g_strstrip(arg->p);

  if (!arg || !arg->p || !strlen(arg->p)) {
    return;
  }
  /* new window ? */
  if (dwb.state.nv == OPEN_NEW_WINDOW) {
    dwb.state.nv = OPEN_NORMAL;
    dwb_new_window(arg);
    return;
  }
  /*  new tab ?  */
  else if (dwb.state.nv == OPEN_NEW_VIEW) {
    dwb.state.nv = OPEN_NORMAL;
    dwb_add_view(arg);
    return;
  }
  /*  get resolved uri */
  char *tmp, *uri = NULL; 
  GError *error = NULL;

  /* free cookie of last visited website */
  if (dwb.state.last_cookie) {
    soup_cookie_free(dwb.state.last_cookie); 
    dwb.state.last_cookie = NULL;
  }

  g_strstrip(arg->p);

  if (dwb_handle_mail(arg->p)) {
    return;
  }

  if ( (uri = dwb_test_userscript(arg->p)) ) {
    Arg a = { .p = uri };
    dwb_execute_user_script(&a);
    g_free(uri);
    return;
  }
  if (g_str_has_prefix(arg->p, "javascript:")) {
    webkit_web_view_execute_script(CURRENT_WEBVIEW(), arg->p);
    return;
  }
  if (g_str_has_prefix(arg->p, "file://") || !strcmp(arg->p, "about:blank")) {
    webkit_web_view_load_uri(CURRENT_WEBVIEW(), arg->p);
    return;
  }
  else if ( g_file_test(arg->p, G_FILE_TEST_IS_REGULAR) ) {
    if ( !(uri = g_filename_to_uri(arg->p, NULL, &error)) ) { 
      if (error->code == G_CONVERT_ERROR_NOT_ABSOLUTE_PATH) {
        g_clear_error(&error);
        char *path = g_get_current_dir();
        tmp = g_build_filename(path, arg->p, NULL);
        if ( !(uri = g_filename_to_uri(tmp, NULL, &error))) {
          fprintf(stderr, "Cannot open %s: %s", (char*)arg->p, error->message);
          g_clear_error(&error);
        }
        FREE(tmp);
        FREE(path);
      }
    }
  }
  else if ( !(uri = dwb_get_search_engine(arg->p)) || strstr(arg->p, "localhost:")) {
    uri = g_str_has_prefix(arg->p, "http://") || g_str_has_prefix(arg->p, "https://") 
      ? g_strdup(arg->p)
      : g_strdup_printf("http://%s", (char*)arg->p);
  }
    /* load uri */
  webkit_web_view_load_uri(CURRENT_WEBVIEW(), uri);
  FREE(uri);
}/*}}}*/

/* dwb_update_layout() {{{*/
void
dwb_update_layout() {
  gboolean visible = gtk_widget_get_visible(dwb.gui.right);
  View *v;

  if (! (dwb.state.layout & MAXIMIZED)) {
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
  /* update tab label */
  for (GList *gl = dwb.state.views; gl; gl = gl->next) {
    View *v = gl->data;
    const char *title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
    dwb_tab_label_set_text(gl, title);
  }
}/*}}}*/

/* dwb_eval_editing_key(GdkEventKey *) {{{*/
gboolean 
dwb_eval_editing_key(GdkEventKey *e) {
  if (!(ALPHA(e) || DIGIT(e))) {
    return false;
  }

  char *key = dwb_util_keyval_to_char(e->keyval);
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
  FREE(key);
  return ret;
}/*}}}*/

/* dwb_eval_key(GdkEventKey *e) {{{*/
static gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = false;
  const char *old = dwb.state.buffer ? dwb.state.buffer->str : NULL;
  int keyval = e->keyval;

  if (dwb.state.scriptlock) {
    return true;
  }
  if (e->is_modifier) {
    return false;
  }
  // don't show backspace in the buffer
  if (keyval == GDK_BackSpace ) {
    if (dwb.state.mode & AUTO_COMPLETE) {
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
  char *key = dwb_util_keyval_to_char(keyval);
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

  const char *buf = dwb.state.buffer->str;
  int length = dwb.state.buffer->len;
  int longest = 0;
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
  if (dwb.state.mode & AUTO_COMPLETE) {
    dwb_comp_clean_autocompletion();
  }
  if (coms && g_list_length(coms) > 0) {
    dwb_comp_autocomplete(coms, NULL);
    ret = true;
  }
  if (tmp) {
    dwb_com_simple_command(tmp);
  }
  FREE(key);
  return ret;

}/*}}}*/

/* dwb_command_mode {{{*/
static gboolean
dwb_command_mode(Arg *arg) {
  dwb_set_normal_message(dwb.state.fview, false, ":");
  dwb_focus_entry();
  dwb.state.mode = COMMAND_MODE;
  return true;
}/*}}}*/

/* dwb_insert_mode(Arg *arg) {{{*/
gboolean
dwb_insert_mode(Arg *arg) {
  if (dwb.state.mode == HINT_MODE) {
    dwb_set_normal_message(dwb.state.fview, true, INSERT);
  }
  dwb_view_modify_style(dwb.state.fview, &dwb.color.insert_fg, &dwb.color.insert_bg, NULL, NULL, NULL, 0);

  dwb.state.mode = INSERT_MODE;
  return true;
}/*}}}*/

/* dwb_normal_mode() {{{*/
void 
dwb_normal_mode(gboolean clean) {
  Mode mode = dwb.state.mode;

  if (dwb.state.mode == HINT_MODE || dwb.state.mode == SEARCH_FIELD_MODE) {
    webkit_web_view_execute_script(CURRENT_WEBVIEW(), "dwb_clear()");
  }
  else if (mode  == INSERT_MODE) {
    dwb_view_modify_style(dwb.state.fview, &dwb.color.active_fg, &dwb.color.active_bg, NULL, NULL, NULL, 0);
    gtk_entry_set_visibility(GTK_ENTRY(dwb.gui.entry), true);
  }
  else if (mode == DOWNLOAD_GET_PATH) {
    dwb_comp_clean_path_completion();
  }
  if (mode & COMPLETION_MODE) {
    dwb_comp_clean_completion();
  }
  if (mode & AUTO_COMPLETE) {
    dwb_comp_clean_autocompletion();
  }
  dwb_focus_scroll(dwb.state.fview);

  if (mode & NORMAL_MODE) {
    webkit_web_view_execute_script(CURRENT_WEBVIEW(), "dwb_blur()");
  }

  if (clean) {
    dwb_clean_buffer(dwb.state.fview);
  }

  webkit_web_view_unmark_text_matches(CURRENT_WEBVIEW());

  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  dwb_clean_vars();
}/*}}}*/

/* dwb_update_search(gboolean ) {{{*/
gboolean 
dwb_update_search(gboolean forward) {
  View *v = CURRENT_VIEW();
  WebKitWebView *web = CURRENT_WEBVIEW();
  const char *text = GET_TEXT();
  int matches;
  if (strlen(text) > 0) {
    FREE(v->status->search_string);
    v->status->search_string =  g_strdup(text);
  }
  if (!v->status->search_string) {
    return false;
  }
  webkit_web_view_unmark_text_matches(web);
  if ( (matches = webkit_web_view_mark_text_matches(web, v->status->search_string, false, 0)) ) {
    dwb_set_normal_message(dwb.state.fview, false, "[%d matches]", matches);
    webkit_web_view_set_highlight_text_matches(web, true);
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

/* dwb_user_script_cb(GIOChannel *, GIOCondition *)     return: false {{{*/
static gboolean
dwb_user_script_cb(GIOChannel *channel, GIOCondition condition, char *filename) {
  GError *error = NULL;
  char *line;

  while (g_io_channel_read_line(channel, &line, NULL, NULL, &error) == G_IO_STATUS_NORMAL) {
    dwb_parse_command_line(g_strchomp(line));
    FREE(line);
  }
  if (error) {
    fprintf(stderr, "Cannot read from std_out: %s\n", error->message);
  }

  g_io_channel_shutdown(channel, true, NULL);
  g_clear_error(&error);

  return false;
}/*}}}*/
/* dwb_execute_user_script(Arg *a) {{{*/
void
dwb_execute_user_script(Arg *a) {
  GError *error = NULL;
  char *argv[3] = { a->p, (char*)webkit_web_view_get_uri(CURRENT_WEBVIEW()), NULL } ;
  int std_out;
  if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &std_out, NULL, &error)) {
    GIOChannel *channel = g_io_channel_unix_new(std_out);
    g_io_add_watch(channel, G_IO_IN, (GIOFunc)dwb_user_script_cb, NULL);
    dwb_set_normal_message(dwb.state.fview, true, "Executing script %s", a->p);
  }
  else {
    fprintf(stderr, "Cannot execute %s: %s\n", (char*)a->p, error->message);
  }
  g_clear_error(&error);
}/*}}}*/

/* dwb_get_scripts() {{{*/
static GList * 
dwb_get_scripts() {
  GDir *dir;
  char *filename;
  char *content;
  GList *gl = NULL;
  Navigation *n = NULL;

  if ( (dir = g_dir_open(dwb.files.userscripts, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      char *path = g_build_filename(dwb.files.userscripts, filename, NULL);

      g_file_get_contents(path, &content, NULL, NULL);
      char **lines = g_strsplit(content, "\n", -1);
      int i=0;
      while (lines[i]) {
        if (g_regex_match_simple(".*dwb:", lines[i], 0, 0)) {
          char **line = g_strsplit(lines[i], "dwb:", 2);
          if (line[1]) {
            n = dwb_navigation_new(filename, line[1]);
            KeyMap *map = dwb_malloc(sizeof(KeyMap));
            Key key = dwb_str_to_key(line[1]);
            FunctionMap fm = { { n->first, n->first }, FM_DONT_SAVE, (Func)dwb_execute_user_script, NULL, POST_SM, { .p = path } };
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
      FREE(content);
      if (!n) {
        n = dwb_navigation_new(filename, "");
      }
      dwb.misc.userscripts = g_list_prepend(dwb.misc.userscripts, n);
      n = NULL;

      g_strfreev(lines);
    }
    g_dir_close(dir);
  }
  return gl;
}/*}}}*/

/*}}}*/

/* EXIT {{{*/

/* dwb_clean_vars() {{{*/
static void 
dwb_clean_vars() {
  dwb.state.mode = NORMAL_MODE;
  dwb.state.buffer = NULL;
  dwb.state.nummod = 0;
  dwb.state.nv = 0;
  dwb.state.scriptlock = 0;
  dwb.state.last_com_history = NULL;
  dwb.state.dl_action = DL_ACTION_DOWNLOAD;
}/*}}}*/

static void
dwb_free_list(GList *list, void (*func)(void*)) {
  for (GList *l = list; l; l=l->next) {
    Navigation *n = l->data;
    func(n);
  }
  g_list_free(list);
}

/* dwb_clean_up() {{{*/
gboolean
dwb_clean_up() {
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    FREE(m);
  }
  g_list_free(dwb.keymap);
  g_hash_table_remove_all(dwb.settings);

  dwb_free_list(dwb.fc.bookmarks, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.history, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.searchengines, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.se_completion, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.mimetypes, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.quickmarks, (void_func)dwb_quickmark_free);
  dwb_free_list(dwb.fc.cookies_allow, (void_func)dwb_free);
  dwb_free_list(dwb.fc.content_block_allow, (void_func)dwb_free);
  dwb_free_list(dwb.fc.plugins_allow, (void_func)dwb_free);
  dwb_free_list(dwb.fc.adblock, (void_func)dwb_free);


  if (g_file_test(dwb.files.fifo, G_FILE_TEST_EXISTS)) {
    unlink(dwb.files.fifo);
  }
  gtk_widget_destroy(dwb.gui.window);
  return true;
}/*}}}*/

/* dwb_save_navigation_fc{{{*/
static void 
dwb_save_navigation_fc(GList *fc, const char *filename, int length) {
  GString *string = g_string_new(NULL);
  int i=0;
  for (GList *l = fc; l && (length<0 || i<length) ; l=l->next, i++)  {
    Navigation *n = l->data;
    g_string_append_printf(string, "%s %s\n", n->first, n->second);
  }
  dwb_util_set_file_content(filename, string->str);
  g_string_free(string, true);
}/*}}}*/

/* dwb_save_simple_file(GList *, const char *){{{*/
static void
dwb_save_simple_file(GList *fc, const char *filename) {
  GString *string = g_string_new(NULL);
  for (GList *l = fc; l; l=l->next)  {
    g_string_append_printf(string, "%s\n", (char*)l->data);
  }
  dwb_util_set_file_content(filename, string->str);
  g_string_free(string, true);
}/*}}}*/

/* dwb_save_keys() {{{*/
static void
dwb_save_keys() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  char *content;
  gsize size;

  if (!g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No keysfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *map = l->data;
    if (! (map->map->prop & FM_DONT_SAVE) ) {
      char *mod = dwb_modmask_to_string(map->mod);
      char *sc = g_strdup_printf("%s %s", mod, map->key ? map->key : "");
      g_key_file_set_value(keyfile, dwb.misc.profile, map->map->n.first, sc);
      FREE(sc);
      FREE(mod);
    }
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    g_file_set_contents(dwb.files.keys, content, size, &error);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save keyfile: %s", error->message);
    g_clear_error(&error);
  }
  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_save_settings {{{*/
void
dwb_save_settings() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  char *content;
  gsize size;

  if (!g_key_file_load_from_file(keyfile, dwb.files.settings, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No settingsfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    char *value = dwb_util_arg_to_char(&s->arg, s->type); 
    g_key_file_set_value(keyfile, dwb.misc.profile, s->n.first, value ? value : "" );

    FREE(value);
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    g_file_set_contents(dwb.files.settings, content, size, &error);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save settingsfile: %s\n", error->message);
    g_clear_error(&error);
  }
  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_save_files() {{{*/
gboolean 
dwb_save_files(gboolean end_session) {
  dwb_save_navigation_fc(dwb.fc.bookmarks, dwb.files.bookmarks, -1);
  if (!GET_BOOL("enable-private-browsing")) {
    dwb_save_navigation_fc(dwb.fc.history, dwb.files.history, dwb.misc.history_length);
  }
  dwb_save_navigation_fc(dwb.fc.searchengines, dwb.files.searchengines, -1);
  dwb_save_navigation_fc(dwb.fc.mimetypes, dwb.files.mimetypes, -1);

  // quickmarks
  GString *quickmarks = g_string_new(NULL); 
  for (GList *l = dwb.fc.quickmarks; l; l=l->next)  {
    Quickmark *q = l->data;
    Navigation n = *(q->nav);
    g_string_append_printf(quickmarks, "%s %s %s\n", q->key, n.first, n.second);
  }
  dwb_util_set_file_content(dwb.files.quickmarks, quickmarks->str);
  g_string_free(quickmarks, true);

  // cookie allow
  dwb_save_simple_file(dwb.fc.cookies_allow, dwb.files.cookies_allow);
  dwb_save_simple_file(dwb.fc.content_block_allow, dwb.files.content_block_allow);
  dwb_save_simple_file(dwb.fc.plugins_allow, dwb.files.plugins_allow);

  // save keys
  dwb_save_keys();

  // save settings
  dwb_save_settings();

  // save session
  if (end_session && GET_BOOL("save-session") && dwb.state.mode != SAVE_SESSION) {
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

/* dwb_str_to_key(char *str)      return: Key{{{*/
Key 
dwb_str_to_key(char *str) {
  Key key = { .mod = 0, .str = NULL };
  GString *buffer = g_string_new(NULL);

  char **string = g_strsplit(str, " ", -1);

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
  key.str = buffer->str;

  g_strfreev(string);
  g_string_free(buffer, false);

  return key;
}/*}}}*/

/* dwb_keymap_delete(GList *, KeyValue )     return: GList * {{{*/
static GList * 
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
      fmap->n.first = (char*)key.id;
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
static void
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

  g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error);
  if (error) {
    fprintf(stderr, "No keyfile found: %s\nUsing default values.\n", error->message);
    g_clear_error(&error);
  }
  for (int i=0; i<LENGTH(KEYS); i++) {
    KeyValue kv;
    char *string = g_key_file_get_value(keyfile, dwb.misc.profile, KEYS[i].id, NULL);
    if (string) {
      kv.key = dwb_str_to_key(string);
      g_free(string);
    }
    else if (KEYS[i].key.str) {
      kv.key = KEYS[i].key;
    }
    kv.id = KEYS[i].id;
    dwb.keymap = dwb_keymap_add(dwb.keymap, kv);
  }

  dwb.keymap = g_list_concat(dwb.keymap, dwb_get_scripts());
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)dwb_util_keymap_sort_second);

  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_read_settings() {{{*/
static gboolean
dwb_read_settings() {
  GError *error = NULL;
  gsize length, numkeys = 0;
  char  **keys = NULL;
  char  *content;
  GKeyFile  *keyfile = g_key_file_new();
  Arg *arg;
  setlocale(LC_NUMERIC, "C");

  g_file_get_contents(dwb.files.settings, &content, &length, &error);
  if (error) {
    fprintf(stderr, "No settingsfile found: %s\nUsing default values.\n", error->message);
    g_clear_error(&error);
  }
  else {
    g_key_file_load_from_data(keyfile, content, length, G_KEY_FILE_KEEP_COMMENTS, &error);
    if (error) {
      fprintf(stderr, "Couldn't read settings file: %s\nUsing default values.\n", error->message);
      g_clear_error(&error);
    }
    else {
      keys = g_key_file_get_keys(keyfile, dwb.misc.profile, &numkeys, &error); 
      if (error) {
        fprintf(stderr, "Couldn't read settings for profile %s: %s\nUsing default values.\n", dwb.misc.profile,  error->message);
        g_clear_error(&error);
      }
    }
  }
  FREE(content);
  for (int j=0; j<LENGTH(DWB_SETTINGS); j++) {
    gboolean set = false;
    char *key = g_strdup(DWB_SETTINGS[j].n.first);
    for (int i=0; i<numkeys; i++) {
      char *value = g_key_file_get_string(keyfile, dwb.misc.profile, keys[i], NULL);
      if (!strcmp(keys[i], DWB_SETTINGS[j].n.first)) {
        WebSettings *s = dwb_malloc(sizeof(WebSettings));
        *s = DWB_SETTINGS[j];
        if ( (arg = dwb_util_char_to_arg(value, s->type)) ) {
          s->arg = *arg;
        }
        g_hash_table_insert(dwb.settings, key, s);
        set = true;
      }
      FREE(value);
    }
    if (!set) {
      g_hash_table_insert(dwb.settings, key, &DWB_SETTINGS[j]);
    }
  }
  if (keys)
    g_strfreev(keys);
  return true;
}/*}}}*/

/* dwb_init_settings() {{{*/
static void
dwb_init_settings() {
  dwb.settings = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)dwb_free, NULL);
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
  char *dir = NULL;
  dwb_util_get_directory_content(&buffer, dwb.files.scriptdir);
  dwb.misc.scripts = g_strdup(buffer->str);
  g_string_truncate(buffer, 0);
  if ( (dir = dwb_util_get_data_dir("scripts")) ) {
    dwb_util_get_directory_content(&buffer, dir);
    g_free(dir);
  }
  dwb.misc.systemscripts = buffer->str;
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
  gdk_color_parse(GET_CHAR("normal-completion-bg-color"), &dwb.color.normal_c_bg);
  gdk_color_parse(GET_CHAR("normal-completion-fg-color"), &dwb.color.normal_c_fg);

  gdk_color_parse(GET_CHAR("error-color"), &dwb.color.error);

  dwb.color.settings_fg_color = GET_CHAR("settings-fg-color");
  dwb.color.settings_bg_color = GET_CHAR("settings-bg-color");

  // Fonts
  int active_font_size = GET_INT("active-font-size");
  int normal_font_size = GET_INT("normal-font-size");
  char *font = GET_CHAR("font");

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
  if (embed) {
    dwb.gui.window = gtk_plug_new(embed);
  } 
  else {
    dwb.gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_wmclass(GTK_WINDOW(dwb.gui.window), dwb.misc.name, dwb.misc.name);
  }
  // Icon
  GdkPixbuf *icon_pixbuf = gdk_pixbuf_new_from_xpm_data(icon);
  gtk_window_set_icon(GTK_WINDOW(dwb.gui.window), icon_pixbuf);

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
dwb_init_file_content(GList *gl, const char *filename, Content_Func func) {
  char *content = dwb_util_get_file_content(filename);

  if (content) {
    char **token = g_strsplit(content, "\n", 0);
    int length = MAX(g_strv_length(token) - 1, 0);
    for (int i=0;  i < length; i++) {
      gl = g_list_append(gl, func(token[i]));
    }
    g_free(content);
    g_strfreev(token);
  }
  return gl;
}/*}}}*/

static Navigation * 
dwb_get_search_completion(const char *text) {
  Navigation *n = dwb_navigation_new_from_line(text);

  char *uri = n->second;
  n->second = g_strdup(dwb_util_domain_from_uri(n->second));

  FREE(uri);

  return n;
}

/* dwb_init_files() {{{*/
static void
dwb_init_files() {
  char *path           = dwb_util_build_path();
  char *profile_path   = g_build_filename(path, dwb.misc.profile, NULL);

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
  dwb.files.plugins_allow = g_build_filename(profile_path, "plugins.allow",      NULL);
  dwb.files.adblock       = g_build_filename(path, "adblock",      NULL);

  if (!g_file_test(dwb.files.scriptdir, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(dwb.files.scriptdir, 0755);
  }

  if (!g_file_test(dwb.files.userscripts, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(dwb.files.userscripts, 0755);
  }

  dwb.fc.bookmarks = dwb_init_file_content(dwb.fc.bookmarks, dwb.files.bookmarks, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.history = dwb_init_file_content(dwb.fc.history, dwb.files.history, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.quickmarks = dwb_init_file_content(dwb.fc.quickmarks, dwb.files.quickmarks, (Content_Func)dwb_quickmark_new_from_line); 
  dwb.fc.searchengines = dwb_init_file_content(dwb.fc.searchengines, dwb.files.searchengines, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.se_completion = dwb_init_file_content(dwb.fc.se_completion, dwb.files.searchengines, (Content_Func)dwb_get_search_completion);
  dwb.fc.mimetypes = dwb_init_file_content(dwb.fc.mimetypes, dwb.files.mimetypes, (Content_Func)dwb_navigation_new_from_line);

  if (g_list_last(dwb.fc.searchengines)) 
    dwb.misc.default_search = ((Navigation*)dwb.fc.searchengines->data)->second;
  else 
    dwb.misc.default_search = NULL;
  dwb.fc.cookies_allow = dwb_init_file_content(dwb.fc.cookies_allow, dwb.files.cookies_allow, (Content_Func)dwb_return);
  dwb.fc.content_block_allow = dwb_init_file_content(dwb.fc.content_block_allow, dwb.files.content_block_allow, (Content_Func)dwb_return);
  dwb.fc.plugins_allow = dwb_init_file_content(dwb.fc.plugins_allow, dwb.files.plugins_allow, (Content_Func)dwb_return);
  dwb.fc.adblock = dwb_init_file_content(dwb.fc.adblock, dwb.files.adblock, (Content_Func)dwb_return);

  FREE(path);
  FREE(profile_path);
}/*}}}*/

/* signals {{{*/
static void
dwb_handle_signal(int s) {
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
  const char *proxy;
  static char *newproxy;
  gboolean use_proxy = GET_BOOL("proxy");
  if ( !(proxy =  g_getenv("http_proxy")) && !(proxy =  GET_CHAR("proxy-url")) )
    return;

  if ( (use_proxy && dwb_util_test_connect(proxy)) || !use_proxy ) {
    newproxy = g_strrstr(proxy, "http://") ? g_strdup(proxy) : g_strdup_printf("http://%s", proxy);
    dwb.misc.proxyuri = soup_uri_new(newproxy);
    g_object_set(G_OBJECT(dwb.misc.soupsession), "proxy-uri", use_proxy ? dwb.misc.proxyuri : NULL, NULL); 
    FREE(newproxy);
  }
}/*}}}*/

/* dwb_init_vars{{{*/
static void 
dwb_init_vars() {
  dwb.misc.message_delay = GET_INT("message-delay");
  dwb.misc.history_length = GET_INT("history-length");
  dwb.misc.factor = GET_DOUBLE("factor");
  dwb.misc.startpage = GET_CHAR("startpage");
  dwb.misc.tabbed_browsing = GET_BOOL("tabbed-browsing");
  dwb.misc.content_block_regex = GET_CHAR("content-block-regex");
  dwb.state.tabbar_visible = dwb_eval_tabbar_visible(GET_CHAR("hide-tabbar"));

  dwb.state.complete_history = GET_BOOL("complete-history");
  dwb.state.complete_bookmarks = GET_BOOL("complete-bookmarks");
  dwb.state.complete_searchengines = GET_BOOL("complete-searchengines");
  dwb.state.complete_commands = GET_BOOL("complete-commands");
  dwb.state.complete_userscripts = GET_BOOL("complete-userscripts");

  dwb.state.size = GET_INT("size");
  dwb.state.layout = dwb_layout_from_char(GET_CHAR("layout"));
  dwb.comps.autocompletion = GET_BOOL("auto-completion");
}/*}}}*/

/* dwb_init() {{{*/
static void dwb_init() {
  dwb_clean_vars();
  dwb.state.views = NULL;
  dwb.state.fview = NULL;
  dwb.state.last_cookie = NULL;
  dwb.comps.completions = NULL; 
  dwb.comps.active_comp = NULL;
  dwb.misc.max_c_items = MAX_COMPLETIONS;
  dwb.misc.userscripts = NULL;


  dwb_init_key_map();
  dwb_init_style();
  dwb_init_scripts();
  dwb_init_gui();

  dwb.misc.soupsession = webkit_get_default_session();
  dwb_init_proxy();
  dwb_init_cookies();
  dwb_init_vars();

  if (dwb.state.layout & BOTTOM_STACK) {
    Arg a = { .n = dwb.state.layout };
    dwb_com_set_orientation(&a);
  }
  if (restore && dwb_session_restore(restore));
  else if (dwb.misc.argc > 0) {
    for (int i=0; i<dwb.misc.argc; i++) {
      Arg a = { .p = dwb.misc.argv[i] };
      dwb_add_view(&a);
    }
  }
  else {
    dwb_add_view(NULL);
  }
  dwb_toggle_tabbar();
} /*}}}*/ /*}}}*/

/* FIFO {{{*/
/* dwb_parse_command_line(const char *line) {{{*/
void 
dwb_parse_command_line(const char *line) {
  char **token = g_strsplit(line, " ", 2);

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
static gboolean
dwb_handle_channel(GIOChannel *c, GIOCondition condition, void *data) {
  char *line = NULL;

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
static void 
dwb_init_fifo(int single) {
  FILE *ff;

  /* Files */
  char *path = dwb_util_build_path();
  dwb.files.unifile = g_build_filename(path, "dwb-uni.fifo", NULL);


  dwb.misc.si_channel = NULL;
  if (single == NEW_INSTANCE) {
    return;
  }

  if (GET_BOOL("single-instance")) {
    if (!g_file_test(dwb.files.unifile, G_FILE_TEST_EXISTS)) {
      mkfifo(dwb.files.unifile, 0666);
    }
    int fd = open(dwb.files.unifile, O_WRONLY | O_NONBLOCK);
    if ( (ff = fdopen(fd, "w")) ) {
      if (dwb.misc.argc) {
        for (int i=0; i<dwb.misc.argc; i++) {
          if (g_file_test(dwb.misc.argv[i], G_FILE_TEST_EXISTS) && !g_path_is_absolute(dwb.misc.argv[i])) {
            char *curr_dir = g_get_current_dir();
            path = g_build_filename(curr_dir, dwb.misc.argv[i], NULL);

            fprintf(ff, "add_view %s\n", path);

            FREE(curr_dir);
            FREE(path);
          }
          else {
            fprintf(ff, "add_view %s\n", dwb.misc.argv[i]);
          }
        }
      }
      else {
        fprintf(ff, "add_view\n");
      }
      fclose(ff);
      exit(EXIT_SUCCESS);
    }
    close(fd);
    dwb_open_si_channel();
  }

  /* fifo */
  if (GET_BOOL("use-fifo")) {
    char *filename = g_strdup_printf("%s-%d.fifo", dwb.misc.name, getpid());
    dwb.files.fifo = g_build_filename(path, filename, NULL);
    FREE(filename);

    if (!g_file_test(dwb.files.fifo, G_FILE_TEST_EXISTS)) {
      mkfifo(dwb.files.fifo, 0600);
    }
    dwb_open_channel(dwb.files.fifo);
  }

  FREE(path);
}/*}}}*/
/*}}}*/

int main(int argc, char *argv[]) {
  dwb.misc.name = NAME;
  dwb.misc.profile = "default";
  dwb.misc.argc = 0;
  dwb.misc.prog_path = argv[0];
  int last = 0;
  int single = 0;

  gtk_init(&argc, &argv);

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
        else if (argv[i][1] == 'n') {
          single = NEW_INSTANCE;
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
  dwb_init_files();
  dwb_init_settings();
  if (GET_BOOL("save-session") && argc == 1 && !restore) {
    restore = "default";
  }

  if (last) {
    dwb.misc.argv = &argv[last];
    dwb.misc.argc = g_strv_length(dwb.misc.argv);
  }
  dwb_init_fifo(single);
  dwb_init_signals();
  dwb_init();
  gtk_main();
  return EXIT_SUCCESS;
}
