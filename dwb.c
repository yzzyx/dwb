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
#define CLEAN_STATE(X) (X->state & ~(GDK_SHIFT_MASK) & ~(GDK_BUTTON1_MASK) & ~(GDK_BUTTON2_MASK) & ~(GDK_BUTTON3_MASK) & ~(GDK_BUTTON4_MASK) & ~(GDK_BUTTON5_MASK))
/* Types */
typedef enum _Mode Mode;
typedef enum _Open Open;
typedef enum _Layout Layout;
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

/* functions */
void dwb_exit(void);
gboolean dwb_eval_key(GdkEventKey *);

void dwb_resize(gdouble);
void dwb_normal_mode(void);
void dwb_update_layout(void);

void dwb_set_command_text(GList *, const char *, GdkColor *,  PangoFontDescription *);
void dwb_view_modify_style(GList *, GdkColor *, GdkColor *, GdkColor *, GdkColor *, PangoFontDescription *, gint);
GList * dwb_create_web_view(GList *);
void dwb_add_view(Arg *);
void dwb_remove_view(Arg *);

gboolean dwb_web_view_button_press_cb(WebKitWebView *, GdkEventButton *, GList *);
gboolean dwb_entry_keypress_cb(GtkWidget *, GdkEventKey *e);
gboolean dwb_entry_activate_cb(GtkEntry *);

void dwb_focus(GList *);
void dwb_load_uri(Arg *);
void dwb_grab_focus(GList *);
gchar * dwb_get_resolved_uri(const gchar *);

void dwb_open(Arg *);
void dwb_focus_next(Arg *);
void dwb_focus_prev(Arg *);
void dwb_push_master(Arg *);
void dwb_toggle_maximized(Arg *);
void dwb_zoom_in(Arg *);
void dwb_zoom_out(Arg *);

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
enum _Open {
  OpenNormal, 
  OpenNewView,
  OpenNewWindow,
};
enum _Layout {
  NormalLayout = 1<<0, 
  BottomStack = 1<<1, 
  Maximized = 1<<2, 
};

struct _Arg {
  gint n;
  gpointer p;
};
struct _State {
  GList *views;
  GList *fview;
  Mode mode;
  GString *buffer;
  gint nummod;
  Open nv;
};
struct _FunctionMap {
  const gchar *desc;
  void (*func)(void*);
  gchar *text;
  Arg arg;
};
struct _KeyMap {
  const gchar  *desc;
  const gchar *key;
  guint mod;
  void (*func)(void*);
  gchar *text;
  Arg arg;
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
#include "config.h"

FunctionMap FMAP [] = {
  { "open",                  (void*)dwb_open,             "open",           { .n = OpenNormal,      .p = NULL } },
  { "open_new_view",         (void*)dwb_open,             "open new view",  { .n = OpenNewView,     .p = NULL } },
  { "add_view",              (void*)dwb_add_view,         "add view",       { .n = 0,               .p = NULL } },
  { "remove_view",           (void*)dwb_remove_view,      "remove view",    { .n = 0,               .p = NULL } },
  { "zoom_in",               (void*)dwb_zoom_in,          "zoom in",        { .n = 0,               .p = NULL } },
  { "zoom_out",              (void*)dwb_zoom_out,         "zoom out",       { .n = 0,               .p = NULL } },
  { "push_master",           (void*)dwb_push_master,      "push master",    { .n = 0,               .p = NULL } },
  { "focus_next",            (void*)dwb_focus_next,       "focus next",     { .n = 0,               .p = NULL } },
  { "focus_prev",            (void*)dwb_focus_prev,       "focus prev",     { .n = 0,               .p = NULL} }, 
  { "toggle_maximized",      (void*)dwb_toggle_maximized, "toggle maximized" },  
};

/* CALLBACKS {{{*/
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
/* dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {{{*/
gboolean 
dwb_entry_keypress_cb(GtkWidget* entry, GdkEventKey *e) {
  // TODO implement
  if (e->keyval  == GDK_Escape) {
    dwb_normal_mode();
  }
  return false;
}/*}}}*/
/* dwb_entry_activate_cb (GtkWidget *entry) {{{*/
gboolean 
dwb_entry_activate_cb(GtkEntry* entry) {
  gchar *text = (gchar*)gtk_entry_get_text(entry);
  Arg a = { .n = 0, .p = text };

  dwb_load_uri(&a);
  dwb_normal_mode();
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

/* COMMANDS {{{*/
/* dwb_push_master {{{*/
void
dwb_push_master(Arg *arg) {
  GList *gl, *l;
  View *old, *new;
  if (!dwb.state.views->next) {
    return;
  }
  if (arg->p) {
    gl = arg->p;
  }
  else if (dwb.state.nummod) {
    gl = g_list_nth(dwb.state.views, dwb.state.nummod);
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
  gtk_box_reorder_child(GTK_BOX(dwb.gui.topbox), new->tabevent, -1);
  dwb_update_layout();
}/*}}}*/
/* dwb_open(Arg *arg) {{{*/
void 
dwb_open(Arg *arg) {
  dwb.state.nv = arg->n;
  gtk_widget_grab_focus(dwb.gui.entry);
  dwb.mode = OpenMode;
  //dwb_set_command_text(dwb.state.fview, "open", &dwb.color.active_fg, dwb.font.fd_normal);
} /*}}}*/
/* dwb_remove_view(Arg *arg) {{{*/
void 
dwb_toggle_maximized(Arg *arg) {
  LAYOUT ^= Maximized;
  dwb_update_layout();
}
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
  dwb.state.views = g_list_delete_link(dwb.state.views, gl);
  dwb_update_layout();
}/*}}}*/
/* dwb_zoom_in(void *arg) {{{*/
void 
dwb_simple_command(KeyMap *km) {
  void (*func)(void *) = km->func;

  func(&km->arg);
  dwb.state.nummod = 0;
  g_string_free(dwb.state.buffer, true);
  dwb.state.buffer = NULL;
  dwb_set_command_text(dwb.state.fview, km->text, &dwb.color.active_fg, dwb.font.fd_bold);
}
void
dwb_zoom_in(Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  gint limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;

  for (gint i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) > 4.0)) {
      break;
    }
    webkit_web_view_zoom_in(web);
  }
}/*}}}*/
/* dwb_zoom_out(void *arg) {{{*/
void
dwb_zoom_out(Arg *arg) {
  View *v = dwb.state.fview->data;
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  gint limit = dwb.state.nummod < 1 ? 1 : dwb.state.nummod;

  for (int i=0; i<limit; i++) {
    if ((webkit_web_view_get_zoom_level(web) < 0.25)) {
      break;
    }
    webkit_web_view_zoom_out(web);
  }
}/*}}}*//*}}}*/
/* dwb_resize(gdouble size) {{{*/
void
dwb_resize(gdouble size) {
  // TODO save new size
  gtk_widget_set_size_request(dwb.gui.left,  100 - size,  0);
  gtk_widget_set_size_request(dwb.gui.right, size, 0);
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
    dwb_set_command_text(tmp, NULL, NULL, NULL);
  }
  dwb_grab_focus(gl);
} /*}}}*/
/* dwb_focus_next(Arg *arg) {{{*/
void 
dwb_focus_next(Arg *arg) {
  GList *gl = dwb.state.fview;
  if (gl->next) {
    dwb_focus(gl->next);
  }
  else {
    dwb_focus(dwb.state.views);
  }
}/*}}}*/
/* dwb_focus_prev(Arg *arg) {{{*/
void 
dwb_focus_prev(Arg *arg) {
  GList *gl = dwb.state.fview;
  if (gl->prev) {
    dwb_focus(gl->prev);
  }
  else {
    dwb_focus(g_list_last(dwb.state.views));
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
/* dwb_set_command_text(GList *gl, const char *text, GdkColor *fg,  PangoFontDescription *fd) {{{*/
void
dwb_set_command_text(GList *gl, const char *text, GdkColor *fg,  PangoFontDescription *fd) {
  View *v = gl->data;
  gtk_label_set_text(GTK_LABEL(v->lstatus), text);
  if (fg) {
    gtk_widget_modify_fg(v->lstatus, GTK_STATE_NORMAL, fg);
  }
  if (fd) {
    gtk_widget_modify_font(v->lstatus, fd);
  }
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
  //TODO Maximized
  gboolean visible = gtk_widget_get_visible(dwb.gui.right);
  View *v;

  if (LAYOUT & Maximized) {
    if (visible) {
      gtk_widget_hide(dwb.gui.right);
    }
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
  dwb_resize(SIZE);
}/*}}}*/
/* DWB_WEB_VIEW {{{*/
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
  //g_signal_connect(v->vbox, "key-press-event", G_CALLBACK(dwb_web_view_key_press_cb), v);
  g_signal_connect(v->web, "button-press-event", G_CALLBACK(dwb_web_view_button_press_cb), gl);
} /*}}}*/
/* dwb_create_web_view(View *v) {{{*/
GList * 
dwb_create_web_view(GList *gl) {
  View *v = malloc(sizeof(View));
  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();
  // Statusbox
  GtkWidget *status_hbox;
  v->statusbox = gtk_event_box_new();
  v->lstatus = gtk_label_new(NULL);
  v->rstatus = gtk_label_new("right");
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
  gtk_widget_show(v->web);
  gtk_widget_show(v->tabevent);

  gl = g_list_prepend(gl, v);
  dwb_web_view_init_signals(gl);
  return gl;
} /*}}}*/
/* dwb_add_view(Arg *arg) ret: View *{{{*/
void 
dwb_add_view(Arg *arg) {
  //View *v = dwb_create_web_view();

  if (dwb.state.views) {
    View *views = dwb.state.views->data;
    dwb_set_command_text(dwb.state.views, NULL, NULL, NULL);
    gtk_widget_reparent(views->vbox, dwb.gui.right);
    gtk_box_reorder_child(GTK_BOX(dwb.gui.right), views->vbox, 0);
  }
  dwb.state.views = dwb_create_web_view(dwb.state.views);
  dwb_focus(dwb.state.views);
  dwb_update_layout();
  return ;
} /*}}}*//*}}}*/

void 
dwb_clean_vars() {
  dwb.mode = NormalMode;
  dwb.state.buffer = NULL;
  dwb.state.nv = 0;
}

// TODO
/* dwb_get_search_engine(const gchar *uri) {{{*/
gchar *
dwb_get_search_engine(const gchar *uri) {
  gchar *ret = NULL;
  return ret;
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
}/*}}}*/
/* dwb_normal_mode() {{{*/
void 
dwb_normal_mode() {
  View *v = dwb.state.fview->data;

  gtk_widget_grab_focus(v->web);
  dwb_set_command_text(dwb.state.fview, NULL, NULL, NULL);

  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
  }
  dwb_clean_vars();
}/*}}}*/
/* clean_up() {{{*/
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
  gboolean ret = true;
  gchar *pre = "buffer: ";
  const gchar *old = dwb.state.buffer ? dwb.state.buffer->str : NULL;
  gint keyval = e->keyval;
  gboolean digit = keyval >= GDK_0 && keyval <= GDK_9;
  gboolean alpha =    (keyval >= GDK_A && keyval <= GDK_Z) 
                  ||  (keyval >= GDK_a && keyval <= GDK_z);

  gchar *key = gdk_keyval_name(keyval);
  if (keyval == GDK_Escape) {
    dwb_normal_mode();
    return false;
  }
  else if (keyval == GDK_BackSpace && dwb.state.buffer->str ) {
    if (dwb.state.buffer->len > strlen(pre)) {
      g_string_erase(dwb.state.buffer, dwb.state.buffer->len - 1, 1);
      dwb_set_command_text(dwb.state.fview, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
    }
    return false;
  }

  if (e->is_modifier) {
    return false;
  }

  if (!old) {
    dwb.state.buffer = g_string_new(pre);
    old = dwb.state.buffer->str;
  }
  if (digit) {
    if (isdigit(old[strlen(old)-1])) {
      dwb.state.nummod = 10 * dwb.state.nummod + atoi(key);
    }
    else {
      dwb.state.nummod = atoi(key);
    }
  }
  g_string_append(dwb.state.buffer, key);
  if (digit || alpha) {
    dwb_set_command_text(dwb.state.fview, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_normal);
  }

  const gchar *buf = dwb.state.buffer->str;
  gint length = strlen(buf);

  for (GSList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (!strcmp(&buf[length - strlen(km->key)], km->key) && !(CLEAN_STATE(e) ^ km->mod)) {
      dwb_simple_command(km);
      ret = true;
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
        keymap->text = fmap.text;
        dwb.keymap = g_slist_prepend(dwb.keymap, keymap);
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
/* dwb_init() {{{*/
void dwb_init() {

  dwb_clean_vars();
  dwb.state.views = NULL;
  dwb.state.fview = NULL;

  dwb_init_style();
  dwb_init_key_map();
  dwb_init_gui();
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
  return EXIT_SUCCESS;
}
