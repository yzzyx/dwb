#ifndef CALLBACKS_H
#define CALLBACKS_H

gboolean callback_entry_key_release(GtkWidget *, GdkEventKey *);
gboolean callback_entry_key_press(GtkWidget *, GdkEventKey *);
gboolean callback_entry_insert_text(GtkWidget *, GdkEventKey *);
void callback_tab_size(GtkWidget *w, GtkAllocation *a);
void callback_entry_size(GtkWidget *w, GtkAllocation *a);
#endif
