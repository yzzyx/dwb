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

#include "util.h"

/* util_string_replace(const char *haystack, const char *needle, const char  *replace)      return: char * (alloc){{{*/

char *
util_get_temp_filename(const char *prefix) {
  struct timeval t;
  gettimeofday(&t, NULL);
  const char *path = g_get_user_cache_dir();
  char *filename = g_strdup_printf("%s%lu", prefix, t.tv_usec + t.tv_sec*1000000);
  char *cache_path = g_build_filename(path, dwb.misc.name, filename, NULL);
  g_free(filename);

  return cache_path;
}
char *
util_string_replace(const char *haystack, const char *needle, const char *replacemant) {
  char **token;
  char *ret = NULL;
  if ( haystack && needle && (token = g_regex_split_simple(needle, haystack, 0, 0)) && strcmp(token[0], haystack)) {
    ret = g_strconcat(token[0], replacemant, token[1], NULL);
    g_strfreev(token);
  }
  return ret;
}/*}}}*/
/* util_cut_text(char *text, int start, int end) {{{*/
void
util_cut_text(char *text, int start, int end) {
  int length = strlen(text);
  int del = end - start;

  memmove(&text[start], &text[end], length - end);
  memset(&text[length-del], '\0', del);
}/*}}}*/

/* util_is_hex(const char *string) {{{*/
gboolean
util_is_hex(const char *string) {
  char *dup, *loc; 
  dup = loc = g_strdup(string);
  gboolean ret = !strtok(dup, "1234567890abcdefABCDEF");

  FREE(loc);
  return ret;
}/*}}}*/

/* util_modmask_to_char(guint modmask)      return char*{{{*/
char *
dwb_modmask_to_string(guint modmask) {
  char *mod[7];
  int i=0;
  for (; i<7 && modmask; i++) {
    if (modmask & GDK_CONTROL_MASK) {
      mod[i] = "Control";
      modmask ^= GDK_CONTROL_MASK;
    }
    else if (modmask & GDK_MOD1_MASK) {
      mod[i] = "Mod1";
      modmask ^= GDK_MOD1_MASK;
    }
    else if (modmask & GDK_MOD4_MASK) {
      mod[i] = "Mod4";
      modmask ^= GDK_MOD4_MASK;
    }
    else if (modmask & GDK_SHIFT_MASK) {
      mod[i] = "Shift";
      modmask ^= GDK_SHIFT_MASK;
    }
    else if (modmask & DWB_NUMMOD_MASK) {
      mod[i] = "[n]";
      modmask ^= DWB_NUMMOD_MASK;
    }
  }
  mod[i] = NULL; 
  char *line = g_strjoinv(" ", mod);
  return line;
}/*}}}*/
/* util_keyval_to_char (guint keyval)      return: char * (alloc) {{{*/
char *
util_keyval_to_char(guint keyval) {
  char *key = dwb_malloc(6);
  guint32 unichar;
  int length;
  if ( (unichar = gdk_keyval_to_unicode(keyval)) ) {
    if ( (length = g_unichar_to_utf8(unichar, key)) ) {
      memset(&key[length], '\0', 6-length); 
      return key;
    }
  }
  FREE(key);
  return NULL;
}/*}}}*/

Arg *
util_arg_new() {
  Arg *ret = dwb_malloc(sizeof(Arg));
  ret->n = 0;
  ret->i = 0;
  ret->d = 0;
  ret->p = NULL;
  ret->arg = NULL;
  ret->b = false;
  ret->e = NULL;
  return ret;
}
/* util_char_to_arg(char *value, DwbType type)    return: Arg*{{{*/
Arg *
util_char_to_arg(char *value, DwbType type) {
  errno = 0;
  Arg *ret = util_arg_new();
  if (type == BOOLEAN && !value)  {
    ret->b = false;
  }
  else if (value || type == CHAR) {
    if (value) {
      g_strstrip(value);
      if (strlen(value) == 0) {
        if (type == CHAR) {
          return ret;
        }
        return NULL;
      }
    }
    if (type == BOOLEAN) {
      if(!g_ascii_strcasecmp(value, "false") || !strcmp(value, "0")) {
        ret->b = false;
      }
      else {
        ret->b = true;
      }
    }
    else if (type == INTEGER) {
      int n = strtol(value, NULL, 10);
      if (n != LONG_MAX &&  n != LONG_MIN && !errno ) {
        ret->i = n;
      }
    }
    else if (type == DOUBLE) {
      char *end = NULL;
      double d = g_strtod(value, &end);
      if (! *end) {
        ret->d = d;
      }
    }
    else if (type == CHAR) {
      ret->p = !value || (value && !strcmp(value, "null")) ? NULL : g_strdup(value);
    }
    else if (type == COLOR_CHAR) {
      int length = strlen(value);
      if (value[0] == '#' && (length == 4 || length == 7) && util_is_hex(&value[1])) {
        ret->p = g_strdup(value);
      }
    }
  }
  return ret;
}/*}}}*/
/* util_arg_to_char(Arg *arg, DwbType type) {{{*/
char *
util_arg_to_char(Arg *arg, DwbType type) {
  char *value = NULL;
  if (type == BOOLEAN) {
    if (arg->b) 
      value = g_strdup("true");
    else
      value = g_strdup("false");
  }
  else if (type == DOUBLE) {
    value = g_strdup_printf("%.2f", arg->d);
  }
  else if (type == INTEGER) {
    value = g_strdup_printf("%d", arg->i);
  }
  else if (type == CHAR || type == COLOR_CHAR) {
    if (arg->p) {
      char *tmp = (char*) arg->p;
      value = g_strdup(tmp);
    }
  }
  return value;
}/*}}}*/

/* util_navigation_sort_first {{{*/
int
util_navigation_compare_first(Navigation *a, Navigation *b) {
  return (strcmp(a->first, b->first));
}/*}}}*/
/* util_navigation_sort_first {{{*/
int
util_navigation_compare_second(Navigation *a, Navigation *b) {
  return (strcmp(a->second, b->second));
}/*}}}*/
int 
util_quickmark_compare(Quickmark *a, Quickmark *b) {
  return strcmp(a->key, b->key);
}
/* util_keymap_sort_first(KeyMap *, KeyMap *) {{{*/
int
util_keymap_sort_first(KeyMap *a, KeyMap *b) {
  return strcmp(a->map->n.first, b->map->n.first);
}/*}}}*/
/* util_keymap_sort_second(KeyMap *, KeyMap *) {{{*/
int
util_keymap_sort_second(KeyMap *a, KeyMap *b) {
  return strcmp(a->map->n.second, b->map->n.second);
}/*}}}*/
/* util_web_settings_sort_first(WebSettings *a, WebSettings *b) {{{*/
int
util_web_settings_sort_first(WebSettings *a, WebSettings *b) {
  return strcmp(a->n.first, b->n.first);
}/*}}}*/
/* util_web_settings_sort_second (WebSettings *a, WebSettings *b) {{{*/
int
util_web_settings_sort_second(WebSettings *a, WebSettings *b) {
  return strcmp(a->n.second, b->n.second);
}/*}}}*/

/* util_get_directory_entries(const char *)   return: GList * {{{*/
GList *
util_get_directory_entries(const char *path, const char *text) {
  GList *list = NULL;
  GDir *dir;
  char *filename;
  char *store;

  if ( (dir = g_dir_open(path, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      if (strlen(text) && g_str_has_prefix(filename, text)) {
        char *newpath = g_build_filename(path, filename, NULL);
        if (g_file_test(newpath, G_FILE_TEST_IS_DIR)) {
          store = g_strconcat(newpath, "/", NULL);
          FREE(newpath);
        }
        else {
          store = newpath;
        }
        list = g_list_prepend(list,  store);
      }
    }
    g_dir_close(dir);
  }
  list = g_list_sort(list, (GCompareFunc)strcmp);
  return list;
}/*}}}*/
/*util_get_directory_content(GString **, const char *filename) {{{*/
void 
util_get_directory_content(GString **buffer, const char *dirname) {
  GDir *dir;
  char *content;
  GError *error = NULL;
  char *filename, *filepath;

  if ( (dir = g_dir_open(dirname, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      if (filename[0] != '.') {
        filepath = g_build_filename(dirname, filename, NULL);
        if (g_file_get_contents(filepath, &content, NULL, &error)) {
          g_string_append((*buffer), content);
        }
        else {
          fprintf(stderr, "Cannot read %s: %s\n", filename, error->message);
          g_clear_error(&error);
        }
        FREE(filepath);
        FREE(content);
      }
    }
    g_dir_close (dir);
  }

}/*}}}*/
void 
util_rmdir(const char *path, gboolean recursive) {
  GDir *dir = g_dir_open(path, 0, NULL);
  if (dir == NULL) 
    return;
  const char *filename = NULL;
  char *fullpath;
  while ( (filename = g_dir_read_name(dir)) )  {
    fullpath = g_build_filename(path, filename, NULL);
    if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
      unlink(fullpath);
    }
    else if (recursive) {
      util_rmdir(fullpath, true);
      rmdir(fullpath);
    }
    g_free(fullpath);
  }
}
/* util_get_file_content(const char *filename)    return: char * (alloc) {{{*/
char *
util_get_file_content(const char *filename) {
  GError *error = NULL;
  char *content = NULL;
  if (!(g_file_test(filename, G_FILE_TEST_IS_REGULAR) &&  g_file_get_contents(filename, &content, NULL, &error))) {
    fprintf(stderr, "Cannot open %s: %s\n", filename, error ? error->message : "file not found");
    g_clear_error(&error);
  }
  return content;
}/*}}}*/
/* util_set_file_content(const char *filename, const char *content) {{{*/
gboolean
util_set_file_content(const char *filename, const char *content) {
  GError *error = NULL;
  if (!g_file_set_contents(filename, content, -1, &error)) {
    fprintf(stderr, "Cannot save %s : %s", filename, error->message);
    g_clear_error(&error);
    return false;
  }
  return true;
}/*}}}*/
/* util_build_path()       return: char * (alloc) {{{*/
char *
util_build_path() {
  char *path = g_build_filename(g_get_user_config_dir(), dwb.misc.name, NULL);
  if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(path, 0755);
  }
  return path;
}/*}}}*/
/* util_get_data_dir(const char *)      return  char * (alloc) {{{*/
char *
util_get_data_dir(const char *dir) {
  char *path = NULL;
  const char *dirs;

  const char *const *config = g_get_system_data_dirs();
  int i = 0;

  while ( (dirs = config[i++]) ) {
    path = g_build_filename(dirs, dwb.misc.name, dir, NULL);
    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
      return path;
    }
    FREE(path);
  }
  return NULL;
}/*}}}*/
char *
util_get_data_file(const char *filename) {
  char *path = util_get_data_dir("lib");
  char *ret = g_build_filename(path, filename, NULL);
  g_free(path);
  if (g_file_test(ret, G_FILE_TEST_EXISTS)) {
    return ret;
  }
  return NULL;
}
static inline int
util_strcmp_skip_newline(const char *s1, const char *s2) {
  char *nl = strstr(s2, "\n");
  if (nl != NULL) 
    return strncmp(s1, s2, nl - s1);
  else 
    return strcmp(s1, s2);
}
int
util_file_remove_line(const char *filename, const char *line) {
  int ret = 1;
  char *content = util_get_file_content(filename);
  char **lines = g_strsplit(content, "\n", -1);
  GString *buffer = g_string_new(NULL);
  for (int i=0; lines[i]; i++) {
    if (strlen(lines[i]) > 0 && STRCMP_FIRST_WORD(lines[i], line)) {
      g_string_append_printf(buffer, "%s\n", lines[i]);
    }
  }
  g_file_set_contents(filename, buffer->str, -1, NULL);

  g_string_free(buffer, true);
  g_free(content);
  g_strfreev(lines);

  return ret;
}

/* NAVIGATION {{{*/
/* dwb_navigation_new(const char *uri, const char *title) {{{*/
Navigation *
dwb_navigation_new(const char *uri, const char *title) {
  Navigation *nv = dwb_malloc(sizeof(Navigation)); 
  nv->first = uri ? g_strdup(uri) : NULL;
  nv->second = title ? g_strdup(title) : NULL;
  return nv;
}/*}}}*/
Navigation * 
dwb_navigation_dup(Navigation *n) {
  return dwb_navigation_new(n->first, n->second);
}

/* dwb_navigation_new_from_line(const char *text){{{*/
Navigation * 
dwb_navigation_new_from_line(const char *text) {
  char **line;
  Navigation *nv = NULL;

  if (text) {
    line = g_strsplit(text, " ", 2);
    nv = dwb_navigation_new(line[0], line[1]);
    g_strfreev(line);
  }
  return nv;
}/*}}}*/

/* dwb_navigation_free(Navigation *n){{{*/
void
dwb_navigation_free(Navigation *n) {
  FREE(n->first);
  FREE(n->second);
  FREE(n);
}/*}}}*/
/*}}}*/
/* QUICKMARK {{{*/
/* dwb_quickmark_new(const char *uri, const char *title,  const char *key)  {{{*/
Quickmark *
dwb_quickmark_new(const char *uri, const char *title, const char *key) {
  Quickmark *q = dwb_malloc(sizeof(Quickmark));
  q->key = key ? g_strdup(key) : NULL;
  q->nav = dwb_navigation_new(uri, title);
  return q;
}/* }}} */

/* dwb_quickmark_new_from_line(const char *line) {{{*/
Quickmark * 
dwb_quickmark_new_from_line(const char *line) {
  Quickmark *q = NULL;
  char **token;
  if (line) {
    token = g_strsplit(line, " ", 3);
    q = dwb_quickmark_new(token[1], token[2], token[0]);
    g_strfreev(token);
  }
  return q;

}/*}}}*/

/* dwb_quickmark_free(Quickmark *q) {{{*/
void
dwb_quickmark_free(Quickmark *q) {
  FREE(q->key);
  dwb_navigation_free(q->nav);
  FREE(q);

}/*}}}*/
/*}}}*/

/* dwb_true, dwb_false {{{*/
gboolean
dwb_false() {
  return false;
}

gboolean
dwb_true() {
  return true;
}/*}}}*/
/* dwb_return(const char *)     return char * (alloc) {{{*/
char *
dwb_return(const char *ret) {
  return ret && strlen(ret) > 0 ? g_strdup(ret) : NULL;
}/*}}}*/
/* dwb_malloc(size_t size)         return: void* {{{*/
void *
dwb_malloc(size_t size) {
  void *r;
  if ( !(r = malloc(size)) ) {
    fprintf(stderr, "Cannot malloc %d bytes of memory", (int)size);
    dwb_end();
    exit(EXIT_SUCCESS);
  }
  return r;
}/*}}}*/
/*dwb_free(void *p) {{{*/
void 
dwb_free(void *p) {
  if (p) 
    g_free(p);
}/*}}}*/

/* util_domain_from_uri (char *uri)      return: char* {{{*/
char *
util_domain_from_uri(const char *uri) {
  if (!uri) 
    return NULL;

  char *uri_p = (char*)uri;
  char *p = NULL;
  char domain[STRING_LENGTH] = { 0 };

  if ( (p = strstr(uri, "://")) ) {
    uri_p = p + 3;
  }
  if ( (p = strchr(uri_p, '/')) ) {
    strncpy(domain, uri_p, p - uri_p);
  }
  char *ret = domain[0] ? domain : uri_p;
  return ret;
}/*}}}*/

/* util_compare_path(char *uri)      return: char* {{{*/
int
util_compare_path(const char *a, const char *b) {
  return strcmp(g_strrstr(a, "/"), g_strrstr(b, "/"));
}/*}}}*/

/* util_basename(const char *path)       return: char * {{{*/
/* Sligtly different implementation of basename, doesn't modify the pointer,
 * returns pointer to the basename or NULL, if there is no basename
 */
/* dwb_comp_get_path(GList *, char *)        return GList* */
char *
util_basename(const char *path) {
  int l = strlen(path);
  if (path[l-1] == '/')
    return NULL;

  char *ret = strrchr(path, '/');
  if (ret && l > 1) {
    ret++;
  }
  return ret;
}/*}}}*/

/* util_file_add(const char *filename, const char *text, int append, int max){{{*/
gboolean
util_file_add(const char *filename, const char *text, int append, int max) {
  if (!text)
    return false;

  FILE *file;
  char buffer[STRING_LENGTH];
  GString *content = g_string_new(NULL);

  if (!append) 
    g_string_append_printf(content, "%s\n", text);

  gboolean ret;
  if ( (file = fopen(filename, "r")) ) {
    for (int i=0; fgets(buffer, sizeof buffer, file) &&  (max < 0 || i < max-1); i++ ) {
      if (STRCMP_FIRST_WORD(text, buffer) && STRCMP_SKIP_NEWLINE(text, buffer) ) {
        g_string_append(content, buffer);
      }
    }
    fclose(file);
  }
  else {
    fprintf(stderr, "Cannot open file %s\n", filename);
    ret = false;
  }
  if (append)
    g_string_append_printf(content, "%s\n", text);
  ret = util_set_file_content(filename, content->str);
  g_string_free(content, true);
  return ret;
}

gboolean 
util_file_add_navigation(const char *filename, const Navigation *n, int append, int max) {
  gboolean  ret;
  char *text = g_strdup_printf("%s %s", n->first, n->second);
  ret = util_file_add(filename, text, append, max);
  g_free(text);
  return ret;
}/*}}}*/

void
gtk_box_insert(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, gint padding, int position) {
  gtk_box_pack_start(box, child, expand, fill, padding);
  gtk_box_reorder_child(box, child, position);
}
char * 
util_strcasestr(const char *haystack, const char *needle) {
  if (needle == NULL || ! *needle )
    return (char *) haystack;
  for (; *haystack; haystack++) {
    if (tolower(*haystack) == tolower(*needle)) {
      const char *h = haystack, *n = needle;
      for (; *h && *n && tolower(*h) == tolower(*n); ++h, ++n);
      if (! *n ) 
        return (char*)haystack;
    }
  }
  return NULL;
}
