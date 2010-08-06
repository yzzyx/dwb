#ifndef UTIL_H
#define UTIL_H

// strings
gchar * dwb_util_string_replace(const gchar *haystack, const gchar *needle, const gchar *replacemant);
void dwb_util_cut_text(gchar *, gint, gint);

gboolean dwb_util_is_hex(const gchar *string);
int dwb_util_test_connect(const char *uri);

// keys
gchar * dwb_modmask_to_string(guint );
gchar * dwb_util_keyval_to_char(guint );

// arg
gchar * dwb_util_arg_to_char(Arg *, DwbType );
Arg * dwb_util_char_to_arg(gchar *, DwbType );

// sort 
gint dwb_util_keymap_sort_first(KeyMap *, KeyMap *);
gint dwb_util_keymap_sort_second(KeyMap *, KeyMap *);
gint dwb_util_web_settings_sort_second(WebSettings *, WebSettings *);
gint dwb_util_web_settings_sort_first(WebSettings *, WebSettings *);

// files
void dwb_util_get_directory_content(GString **, const gchar *);
GList * dwb_util_get_directory_entries(const gchar *path, const gchar *);
gchar * dwb_util_get_file_content(const gchar *);
void dwb_util_set_file_content(const gchar *, const gchar *);
gchar * dwb_util_build_path(void);
gchar * dwb_util_get_data_dir(const gchar *);

// navigation
Navigation * dwb_navigation_new_from_line(const gchar *);
Navigation * dwb_navigation_new(const gchar *, const gchar *);
void dwb_navigation_free(Navigation *);

// quickmarks
void dwb_com_quickmark_free(Quickmark *);
Quickmark * dwb_com_quickmark_new(const gchar *, const gchar *, const gchar *);
Quickmark * dwb_com_quickmark_new_from_line(const gchar *);

// useless
gboolean dwb_true(void);
gboolean dwb_false(void);
gchar * dwb_return(const gchar *);

#endif
