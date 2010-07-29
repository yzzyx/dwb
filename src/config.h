#ifndef CONFIG_H
#define CONFIG_H
//static int LAYOUT = NormalLayout;
//static int SIZE = 30;
//static double FACTOR = 0.3;
//static int MESSAGE_DELAY = 2;
//static int HISTORY_LENGTH = 500;

//static const gchar *FONT = "monospace bold";
//static const gint ACTIVE_FONT_SIZE = 12;
//static const gint NORMAL_FONT_SIZE = 9;

//static const gint DEFAULT_WIDTH = 1280;
//static const gint DEFAULT_HEIGHT = 1024;

static KeyValue KEYS[] = {
{ "add_view",                 {   "ga",         0,                  },  },  
{ "allow_cookie",             {   "CC",         0,                   },  },  
{ "bookmark",                 {   "M",         0,                   },  },  
{ "find_forward",             {   "/",         0,                   },  }, 
{ "find_next",                {   "n",         0,                   },  },
{ "find_previous",            {   "N",         0,                   },  },
{ "find_backward",            {   "?",         0,                   },  },  
{ "focus_next",               {   "J",         0,                   },  },  
{ "focus_prev",               {   "K",         0,                   },  },  
{ "hint_mode",                {   "f",         0,                   },  },  
{ "hint_mode_new_view",       {   "F",         0,                   },  },  
{ "history_back",             {   "H",         0,                   },  },  
{ "history_forward",          {   "L",         0,                   },  },  
{ "insert_mode",              {   "i",         0,                   },  },  
{ "increase_master",          {   "gl",         0,                  },  },  
{ "decrease_master",          {   "gh",         0,                  },  },  
{ "open",                     {   "o",         0,                   },  },  
{ "open_new_view",            {   "O",         0,                   },  },  
{ "open_quickmark",           {   "b",         0,                   },  },  
{ "open_quickmark_nv",        {   "B",         0,                   },  },  
{ "push_master",              {   "pu",         GDK_CONTROL_MASK,    },  },  
{ "reload",                   {   "r",         0,                   },  },  
{ "remove_view",              {   "d",         0,                   },  },  
{ "save_quickmark",           {   "m",         0,                   },  },  
{ "scroll_bottom",            {   "G",         0,                   },  },  
{ "scroll_down",              {   "j",         0,                   },  },  
{ "scroll_left",              {   "h",         0,                   },  },  
{ "scroll_page_down",         {   "f",         GDK_CONTROL_MASK,    },  },  
{ "scroll_page_up",           {   "b",         GDK_CONTROL_MASK,    },  },  
{ "scroll_right",             {   "l",         0,                   },  },  
{ "scroll_top",               {   "gg",         0,                  },  },  
{ "scroll_up",                {   "k",         0,                   },  },  
{ "show_keys",                {   "sk",         0,                  },  },  
{ "show_settings",            {   "ss",         0,                  },  },  
{ "show_global_settings",     {   "Ss",         0,                  },  },  
{ "toggle_bottomstack",       {   "go",         0,                  },  },  
{ "toggle_maximized",         {   "gm",         0,                  },  },  
{ "view_source",              {   "gf",         0,                  },  },  
{ "zoom_in",                  {   "zi",         0,                  },  },  
{ "zoom_normal",              {   "z=",         0,                  },  },  
{ "zoom_out",                 {   "zo",         0,                  },  },  
{ "save_search_field",        {   "gs",         0,                  },  },  
  // settings
{ "autoload_images",   {   NULL,           0,                  },  },
{ "autoresize_window", {   NULL,           0,                  },  },
{ "autoshrink_images", {   NULL,           0,                  },  },
{ "caret_browsing", {   NULL,           0,                  },  },
{ "java_applets", {   NULL,           0,                  },  },
{ "plugins", {   NULL,           0,                  },  },
{ "private_browsing", {   NULL,           0,                  },  },
{ "scripts", {   NULL,           0,                  },  },
{ "spell_checking", {   NULL,           0,                  },  },
{ "proxy",                    {   "p" ,           GDK_CONTROL_MASK,  },  },
{ "toggle_encoding",                    {   "te" ,           0,  },  },

};

#endif
