#include <stdlib.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h>

/* Types */
typedef struct _Misc Misc;
typedef struct _Dwb Dwb;
typedef struct _Gui Gui;
typedef struct _State State;
typedef struct _View View;

/* functions */
void dwb_exit(void);

void dwb_init_gui(void);
void dwb_create_widget(View *);
View * dwb_add_view(void);
gboolean dwb_entry_keypress_cb(GtkWidget*);
gboolean dwb_entry_acivate_cb(GtkWidget*);

struct _State {
  View *views;
  View *fview;
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

struct _Gui {
  GtkWidget *vbox;
  GtkWidget *topbox;
  GtkWidget *main_hbox;
  GtkWidget *main_vbox;
  GtkWidget *entry;
};
struct _Misc {
  const gchar *name;
};
struct _Dwb {
  Gui gui;
  Misc misc;
  State state;
};
GdkNativeWindow embed = 0;
Dwb dwb;


/*  Callbacks */
gboolean 
dwb_entry_keypress_cb(GtkWidget* entry) {
  return false;
}
gboolean 
dwb_entry_acivate_cb(GtkWidget* entry) {
  return false;
}

/* Gui */


void 
dwb_create_widget(View *v) {
  v->vbox = gtk_vbox_new(false, 0);
  v->web = webkit_web_view_new();
  // Statusbox
  GtkWidget *status_hbox;
  v->statusbox = gtk_event_box_new();
  v->lstatus = gtk_label_new("links");
  v->rstatus = gtk_label_new("rechts");
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
  v->scroll = gtk_scrolled_window_new(NULL, NULL);

  gtk_box_pack_start(GTK_BOX(v->vbox), v->web, true, true, 0);
  gtk_box_pack_start(GTK_BOX(v->vbox), v->statusbox, false, false, 0);
  gtk_container_add(GTK_CONTAINER(v->statusbox), status_hbox);

  gtk_box_pack_end(GTK_BOX(dwb.gui.main_hbox), v->vbox, true, true, 0);
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(v->web), "http://www.archlinux.de");
  gtk_widget_show_all(v->vbox);

}

View *
dwb_add_view() {
  View *v = malloc(sizeof(View));
  dwb_create_widget(v);
  if (dwb.state.views) {
    gtk_widget_reparent(dwb.state.views->vbox, dwb.gui.main_vbox);
    v->next = dwb.state.views;
    dwb.state.views = v;
  }
  dwb.state.views = dwb.state.fview = v;
  return v;
}

void 
dwb_init_gui() {
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (embed) {
    window = gtk_plug_new(embed);
  } 
  else {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_wmclass(GTK_WINDOW(window), dwb.misc.name, dwb.misc.name);
  }
  g_signal_connect(window, "delete-event", G_CALLBACK(dwb_exit), NULL);
  

  dwb.gui.vbox = gtk_vbox_new(false, 0);
  dwb.gui.topbox = gtk_hbox_new(true, 0);
  dwb.gui.main_hbox = gtk_hbox_new(false, 0);
  dwb.gui.main_vbox = gtk_vbox_new(true, 0);
  dwb.gui.entry = gtk_entry_new();
  gtk_box_pack_end(GTK_BOX(dwb.gui.main_hbox), dwb.gui.main_vbox, true, true, 0);
  GtkWidget *sep = gtk_vseparator_new();
  gtk_box_pack_end(GTK_BOX(dwb.gui.main_hbox), sep, false, true, 0);

  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.topbox, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.main_hbox, true, true, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.entry, false, false, 0);

  g_signal_connect(dwb.gui.entry, "key-press-event",
      G_CALLBACK(dwb_entry_keypress_cb), NULL);
  g_signal_connect(dwb.gui.entry, "activate", 
      G_CALLBACK(dwb_entry_acivate_cb), NULL);

  dwb_add_view();
  gtk_container_add(GTK_CONTAINER(window), dwb.gui.vbox);

  gtk_widget_show_all(window);
}

void 
dwb_exit() {
  gtk_main_quit();
}

int main(gint argc, gchar **argv) {
  if (!g_thread_supported()) {
    g_thread_init(NULL);
  }
  gtk_init(&argc, &argv);
  dwb.misc.name = "dwb";
  dwb_init_gui();
  // TODO obacht
  dwb_add_view();
  gtk_main();
}
