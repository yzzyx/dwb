#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h>


#define LENGTH(X)   (sizeof(X)/sizeof(X[0]))
#define GLENGTH(X)  (sizeof(X)/g_array_get_element_size(X))
#define NN(X)       ((X) == 0) ? 1 : (X) 
/* Types */
typedef enum _Mode Mode;
typedef struct _Arg Arg;
typedef struct _Misc Misc;
typedef struct _Dwb Dwb;
typedef struct _Gui Gui;
typedef struct _State State;
typedef struct _View View;
typedef struct _Color Color;
typedef struct _Font Font;
typedef struct  _FunctionMap FunctionMap;
typedef struct  _KeyMap KeyMap;
typedef struct  _Key Key;

struct _Key {
  const gchar *desc;
  const gchar *key;
  guint mod;
};
#include "config.h"

/* functions */
void dwb_exit(void);
gboolean dwb_eval_key(GdkEventKey *);

void dwb_resize(gdouble);
void dwb_normal_mode(void);

void dwb_set_command_text(View *, const char *, GdkColor *,  PangoFontDescription *);
void dwb_view_modify_style(View *, GdkColor *, GdkColor *, GdkColor *, GdkColor *, PangoFontDescription *, gint);
void dwb_create_web_view(View *);
View * dwb_add_view(void *arg);
gboolean dwb_entry_keypress_cb(GtkWidget *, GdkEventKey *e);
gboolean dwb_entry_acivate_cb(GtkWidget *);

void dwb_open(void *);
void dwb_focus(View *);
void dwb_zoom_in(void *arg);
void dwb_zoom_out(void *arg);

void dwb_keymap_free(KeyMap *);
KeyMap * dwb_keymap_prepend(KeyMap *, const gchar *, const gchar *, guint, void (*func)(void*), void *);
KeyMap * dwb_keymap_new();

void dwb_init_key_map(void);
void dwb_init_style(void);
void dwb_init_gui(void);

enum _Mode {
  NormalMode,
  OpenMode,
};

struct _Arg {
  gint n;
  gpointer p;
};
struct _State {
  View *views;
  View *fview;
  Mode mode;
  GString *buffer;
  gint nummod;
};
struct _FunctionMap {
  const gchar *desc;
  void (*func)(void*);
  void *arg;
};
struct _KeyMap {
  const gchar  *desc;
  const gchar *key;
  guint mod;
  void (*func)(void*);
  void *arg;
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
};
struct _Dwb {
  Mode mode;
  Gui gui;
  Color color;
  Font font;
  Misc misc;
  State state;
  GSList *keymap;
};
GdkNativeWindow embed = 0;
Dwb dwb;

FunctionMap FMAP [] = {
  { "open",         (void*)dwb_open,      NULL },
  { "add_view",     (void*)dwb_add_view,  NULL },
  { "zoom_in",      (void*)dwb_zoom_in,   NULL },
  { "zoom_out",     (void*)dwb_zoom_out,   NULL },
};

/* CALLBACKS {{{*/
/* dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {{{*/
gboolean 
dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {
  // TODO implement
  if (e->keyval  == GDK_Escape) {
    dwb_normal_mode();
  }
  return false;
}/*}}}*/
/* dwb_entry_acivate_cb (GtkWidget *entry) {{{*/
gboolean 
dwb_entry_acivate_cb(GtkWidget* entry) {
  // TODO implement
  return false;
}/*}}}*/
/* dwb_web_view_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {{{*/
gboolean 
dwb_web_view_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  gboolean ret = false;

  if (gtk_widget_has_focus(dwb.gui.entry)) {
    return false;
  }
  ret = dwb_eval_key(e);
  
  return ret;
}/*}}}*//*}}}*/
/* dwb_zoom_in(void *arg) {{{*/
void 
dwb_simple_command(KeyMap *km) {
  void (*func)(void *) = km->func;

  func(km->arg);
  dwb.state.nummod = 0;
  g_string_free(dwb.state.buffer, true);
  dwb.state.buffer = NULL;
  dwb_set_command_text(dwb.state.fview, km->desc, &dwb.color.active_fg, dwb.font.fd_bold);
}
void
dwb_zoom_in(void *arg) {
  WebKitWebView *web = WEBKIT_WEB_VIEW(dwb.state.fview->web);
  gint limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;

  for (gint i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) > 4.0)) {
      break;
    }
    printf("%d\n", NN(dwb.state.nummod));
    webkit_web_view_zoom_in(web);
  }
}/*}}}*/
void
dwb_zoom_out(void *arg) {
  WebKitWebView *web = WEBKIT_WEB_VIEW(dwb.state.fview->web);
  gint limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;

  for (int i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) < 0.25)) {
      break;
    }
    webkit_web_view_zoom_out(web);
    printf("%f\n", webkit_web_view_get_zoom_level(web));
  }
}/*}}}*/
/* dwb_resize(gdouble size) {{{*/
void
dwb_resize(gdouble size) {
  // TODO
  gtk_widget_set_size_request(dwb.gui.left,  100 - size,  0);
  gtk_widget_set_size_request(dwb.gui.right, size, 0);
}/*}}}*/
/* dwb_focus(View *v) {{{*/
void 
dwb_focus(View *v) {
  View *tmp = NULL;

  if (dwb.state.fview) {
    tmp = dwb.state.fview;
  }
  dwb.state.fview = v;
  if (tmp) {
    dwb_view_modify_style(tmp, &dwb.color.normal_fg, &dwb.color.normal_bg, &dwb.color.tab_normal_fg, &dwb.color.tab_normal_bg, dwb.font.fd_oblique, dwb.font.normal_size);
  }
  dwb_view_modify_style(v, &dwb.color.active_fg, &dwb.color.active_bg, &dwb.color.tab_active_fg, &dwb.color.tab_active_bg, dwb.font.fd_bold, dwb.font.active_size);
  gtk_widget_grab_focus(v->web);
} /*}}}*/

void
dwb_set_command_text(View *v, const char *text, GdkColor *fg,  PangoFontDescription *fd) {
  gtk_label_set_text(GTK_LABEL(v->lstatus), text);
  if (fg) {
    gtk_widget_modify_fg(v->lstatus, GTK_STATE_NORMAL, fg);
  }
  if (fd) {
    gtk_widget_modify_font(v->lstatus, fd);
  }
}
/* command */
/* open(void *arg) {{{*/
void 
dwb_open(void *arg) {
  gtk_widget_grab_focus(dwb.gui.entry);
  dwb.mode = OpenMode;
  dwb_set_command_text(dwb.state.fview, "open", &dwb.color.active_fg, dwb.font.fd_normal);
}
/*}}}*/
/* DWB_WEB_VIEW {{{*/
/* dwb_view_modify_style(View *v, GdkColor *fg, GdkColor *bg, PangoFontDescription *fd, gint fontsize){{{*/
void dwb_view_modify_style(View *v, GdkColor *fg, GdkColor *bg, GdkColor *tabfg, GdkColor *tabbg, PangoFontDescription *fd, gint fontsize) {
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
dwb_web_view_init_signals(View *v) {
  //g_signal_connect(v->vbox, "key-press-event", G_CALLBACK(dwb_web_view_key_press_cb), v);
} /*}}}*/
/* dwb_create_web_view(View *v) {{{*/
void 
dwb_create_web_view(View *v) {
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

  // Srolling
  GtkWidget *vscroll = gtk_vscrollbar_new(NULL);
  GtkWidget *hscroll = gtk_hscrollbar_new(NULL);
  v->vadj = GTK_ADJUSTMENT(gtk_range_get_adjustment(GTK_RANGE(vscroll)));
  v->hadj = GTK_ADJUSTMENT(gtk_range_get_adjustment(GTK_RANGE(hscroll)));
  gtk_widget_set_scroll_adjustments(v->web, v->hadj, v->vadj);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->web, true, true, 0);

  // Tabbar
  v->tabevent = gtk_event_box_new();
  v->tablabel = gtk_label_new("hallo");
  gtk_misc_set_alignment(GTK_MISC(v->tablabel), 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(v->tabevent), v->tablabel);
  gtk_widget_modify_font(v->tablabel, dwb.font.fd_normal);

  gtk_box_pack_end(GTK_BOX(dwb.gui.topbox), v->tabevent, true, true, 0);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->statusbox, false, false, 0);
  gtk_container_add(GTK_CONTAINER(v->statusbox), status_hbox);

  gtk_box_pack_start(GTK_BOX(dwb.gui.left), v->vbox, true, true, 0);
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(v->web), "http://www.google.de");
  dwb_web_view_init_signals(v);
  gtk_widget_show(v->vbox);
  gtk_widget_show_all(v->statusbox);
  gtk_widget_show(v->web);
  gtk_widget_show(v->tabevent);
} /*}}}*/
/* dwb_add_view() ret: View *{{{*/
View *
dwb_add_view(void *arg) {
  View *v = malloc(sizeof(View));
  dwb_create_web_view(v);
  if (dwb.state.views) {
    gtk_widget_reparent(dwb.state.views->vbox, dwb.gui.right);
    v->next = dwb.state.views;
    dwb.state.views = v;
    dwb_resize(size);
  }
  dwb_focus(v);
  dwb.state.views = v;
  return v;
} /*}}}*//*}}}*/

void 
dwb_normal_mode() {
  dwb.mode = NormalMode;
  gtk_widget_grab_focus(dwb.state.fview->web);
  dwb_set_command_text(dwb.state.fview, NULL, NULL, NULL);
  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
    dwb.state.buffer = NULL;
  }
}
/* clean_up {{{*/
void
clean_up() {
  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    g_free(m);
  }
  g_slist_free(dwb.keymap);
}/*}}}*/
/* dwb_exit() {{{*/
void 
dwb_exit() {
  clean_up();
  gtk_main_quit();
}
/*}}}*/
/* dwb_eval_key(GdkEventKey *e) {{{*/
gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = false;
  gchar *pre = "buffer: ";
  const gchar *old = dwb.state.buffer ? dwb.state.buffer->str : NULL;

  gchar *key = gdk_keyval_name(e->keyval);
  if (e->keyval == GDK_Escape) {
    dwb_normal_mode();
    return false;
  }

  if (e->is_modifier) {
    return false;
  }

  if (!old) {
    dwb.state.buffer = g_string_new(pre);
    old = dwb.state.buffer->str;
  }
  if (isdigit(key[0])) {
    if (isdigit(old[strlen(old)-1])) {
      dwb.state.nummod = 10 * dwb.state.nummod + atoi(key);
    }
    else {
      dwb.state.nummod = atoi(key);
    }
  }
  g_string_append(dwb.state.buffer, key);
   printf("%d\n", dwb.state.nummod);

  dwb_set_command_text(dwb.state.fview, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);

  const gchar *buf = dwb.state.buffer->str;
  gint length = strlen(buf);

  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (!strcmp(&buf[length - strlen(km->key)], km->key) && !(e->state ^ km->mod)) {
      printf("%s, %s: %s\n", &buf[length - strlen(km->key)], km->key, km->desc);
      dwb_simple_command(km);
      break;
    }
  }
  return ret;

}/*}}}*/
/* INIT {{{*/
/* dwb_init_key_map() {{{*/
void 
dwb_init_key_map() {
  dwb.keymap = NULL;
  for (int j=0; j<LENGTH(KEYS); j++) {
    for (int i=0; i<LENGTH(FMAP); i++) {
      if (!strcmp(FMAP[i].desc, KEYS[j].desc)) {
        KeyMap *keymap = malloc(sizeof(KeyMap));
        Key key = KEYS[j];
        FunctionMap fmap = FMAP[i];
        keymap->desc = key.desc;
        keymap->key = key.key;
        keymap->mod = key.mod;
        keymap->func = fmap.func; 
        keymap->arg = fmap.arg;
        dwb.keymap = g_slist_prepend(dwb.keymap, keymap);
        printf("%s: %s\n", key.key, key.desc);
        break;
      }
    }
  }
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
  g_signal_connect(dwb.gui.window, "key-press-event", G_CALLBACK(dwb_web_view_key_press_cb), NULL);

  // Main
  dwb.gui.vbox = gtk_vbox_new(false, 0);
  dwb.gui.topbox = gtk_hbox_new(true, 3);
  dwb.gui.paned = gtk_hpaned_new();
  dwb.gui.left = gtk_vbox_new(true, 0);
  dwb.gui.right = gtk_vbox_new(true, 0);

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
  g_signal_connect(dwb.gui.entry, "activate", G_CALLBACK(dwb_entry_acivate_cb), NULL);

  // Pack
  gtk_paned_pack1(GTK_PANED(dwb.gui.paned), dwb.gui.left, true, true);
  gtk_paned_pack2(GTK_PANED(dwb.gui.paned), dwb.gui.right, true, true);

  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.topbox, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), paned_event, true, true, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.entry, false, false, 0);


  dwb_add_view(NULL);
  gtk_container_add(GTK_CONTAINER(dwb.gui.window), dwb.gui.vbox);

  gtk_widget_show_all(dwb.gui.window);
} /*}}}*/
/* dwb_init() {{{*/
void dwb_init() {
  dwb.mode = NormalMode;
  dwb.state.buffer = NULL;

  dwb_init_style();
  dwb_init_key_map();
  dwb_init_gui();
  //dwb_add_view(NULL);
  //dwb_add_view(NULL);
} /*}}}*/ /*}}}*/

int main(gint argc, gchar **argv) {
  if (!g_thread_supported()) {
    g_thread_init(NULL);
  }
  gtk_init(&argc, &argv);
  dwb.misc.name = "dwb";
  // TODO obacht
  dwb_init();
  //dwb_add_view();
  //dwb_resize(size);
  gtk_main();
}
