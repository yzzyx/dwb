#ifndef UTIL_H
#define UTIL_H

// strings
char * dwb_util_string_replace(const char *haystack, const char *needle, const char *replacemant);
void dwb_util_cut_text(char *, int, int);

gboolean dwb_util_is_hex(const char *string);
int dwb_util_test_connect(const char *uri);

// keys
char * dwb_modmask_to_string(guint );
char * dwb_util_keyval_to_char(guint );

// arg
char * dwb_util_arg_to_char(Arg *, DwbType );
Arg * dwb_util_char_to_arg(char *, DwbType );

// sort 
int dwb_util_navigation_sort_first(Navigation *, Navigation *);
int dwb_util_keymap_sort_first(KeyMap *, KeyMap *);
int dwb_util_keymap_sort_second(KeyMap *, KeyMap *);
int dwb_util_web_settings_sort_second(WebSettings *, WebSettings *);
int dwb_util_web_settings_sort_first(WebSettings *, WebSettings *);

// files
void dwb_util_get_directory_content(GString **, const char *);
GList * dwb_util_get_directory_entries(const char *path, const char *);
char * dwb_util_get_file_content(const char *);
void dwb_util_set_file_content(const char *, const char *);
char * dwb_util_build_path(void);
char * dwb_util_get_data_dir(const char *);

// navigation
Navigation * dwb_navigation_new_from_line(const char *);
Navigation * dwb_navigation_new(const char *, const char *);
void dwb_navigation_free(Navigation *);

void dwb_web_settings_free(WebSettings *);

// quickmarks
void dwb_quickmark_free(Quickmark *);
Quickmark * dwb_quickmark_new(const char *, const char *, const char *);
Quickmark * dwb_quickmark_new_from_line(const char *);

// useless
gboolean dwb_true(void);
gboolean dwb_false(void);
char * dwb_return(const char *);

void * dwb_malloc(size_t);
void dwb_free(void *);

char * dwb_util_domain_from_uri(const char *);
#endif
