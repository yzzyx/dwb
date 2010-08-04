#ifndef UTIL_H
#define UTIL_H

void dwb_cut_text(gchar *, gint, gint);
gchar * dwb_build_path(void);
gboolean dwb_ishex(const gchar *string);
gchar * dwb_modmask_to_string(guint );
gchar * dwb_arg_to_char(Arg *, DwbType );
gchar * dwb_get_data_dir(const gchar *);
gboolean dwb_true(void);
gboolean dwb_false(void);
gchar * dwb_return(const gchar *);
GHashTable * dwb_get_default_settings(void);
Arg * dwb_char_to_arg(gchar *, DwbType );
gchar * dwb_keyval_to_char(guint );
gint dwb_keymap_sort_first(KeyMap *, KeyMap *);
gint dwb_keymap_sort_second(KeyMap *, KeyMap *);
gint dwb_web_settings_sort_second(WebSettings *, WebSettings *);
gint dwb_web_settings_sort_first(WebSettings *, WebSettings *);

void dwb_get_directory_content(GString **, const gchar *);
GList * dwb_get_directory_entries(const gchar *path, const gchar *);

gchar * dwb_get_file_content(const gchar *);
void dwb_set_file_content(const gchar *, const gchar *);

Navigation * dwb_navigation_new_from_line(const gchar *);
Navigation * dwb_navigation_new(const gchar *, const gchar *);
void dwb_navigation_free(Navigation *);

void dwb_quickmark_free(Quickmark *);
Quickmark * dwb_quickmark_new(const gchar *, const gchar *, const gchar *);
Quickmark * dwb_quickmark_new_from_line(const gchar *);

#endif
