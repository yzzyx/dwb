/*
 * Copyright (c) 2010-2012 Stefan Bolte <portix@gmx.net>
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include "dwb.h"
#include "util.h"

/* util_string_replace(const char *haystack, const char *needle, const char  *replace)      return: char * (alloc){{{*/

char *
util_get_temp_filename(const char *prefix) {
  struct timeval t;
  gettimeofday(&t, NULL);
  char *filename = g_strdup_printf("%s%lu", prefix, t.tv_usec + t.tv_sec*1000000);
  char *cache_path = g_build_filename(dwb.files.cachedir, filename, NULL);
  g_free(filename);

  return cache_path;
}
char *
util_string_replace(const char *haystack, const char *needle, const char *replacemant) {
  char **token;
  char *ret = NULL;
  if ( haystack && needle && (token = g_regex_split_simple(needle, haystack, 0, 0)) && g_strcmp0(token[0], haystack)) {
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

  g_free(loc);
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
util_keyval_to_char(guint keyval, gboolean ignore_whitespace) {
  char *key = NULL;
  guint32 unichar;
  int length;
  if ( (unichar = gdk_keyval_to_unicode(keyval)) ) {
    if (ignore_whitespace && !g_unichar_isgraph(unichar))
      return NULL;
    key = g_malloc0_n(6, sizeof(char));
    if ( key && (length = g_unichar_to_utf8(unichar, key))) {
      memset(&key[length], '\0', 6-length); 
      return key;
    }
    else 
      g_free(key);
  }
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
      if(value == NULL || !g_ascii_strcasecmp(value, "false") || !g_strcmp0(value, "0")) {
        ret->b = false;
      }
      else {
        ret->b = true;
      }
    }
    else if (type == INTEGER) {
      long n = strtol(value, NULL, 10);
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
      ret->p = !value || (value && !g_strcmp0(value, "null")) ? NULL : g_strdup(value);
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
  return (g_strcmp0(a->first, b->first));
}/*}}}*/
/* util_navigation_sort_first {{{*/
int
util_navigation_compare_second(Navigation *a, Navigation *b) {
  return (g_strcmp0(a->second, b->second));
}/*}}}*/
int 
util_quickmark_compare(Quickmark *a, Quickmark *b) {
  return g_strcmp0(a->key, b->key);
}
/* util_keymap_sort_first(KeyMap *, KeyMap *) {{{*/
int
util_keymap_sort_first(KeyMap *a, KeyMap *b) {
  return g_strcmp0(a->map->n.first, b->map->n.first);
}/*}}}*/
/* util_keymap_sort_second(KeyMap *, KeyMap *) {{{*/
int
util_keymap_sort_second(KeyMap *a, KeyMap *b) {
  return g_strcmp0(a->map->n.second, b->map->n.second);
}/*}}}*/
/* util_web_settings_sort_first(WebSettings *a, WebSettings *b) {{{*/
int
util_web_settings_sort_first(WebSettings *a, WebSettings *b) {
  return g_strcmp0(a->n.first, b->n.first);
}/*}}}*/
/* util_web_settings_sort_second (WebSettings *a, WebSettings *b) {{{*/
int
util_web_settings_sort_second(WebSettings *a, WebSettings *b) {
  return g_strcmp0(a->n.second, b->n.second);
}/*}}}*/

/*util_get_directory_content(GString **, const char *filename) {{{*/
void 
util_get_directory_content(GString **buffer, const char *dirname, const char *extension) {
  GDir *dir;
  char *content;
  GError *error = NULL;
  char *filename, *filepath;
  char *firstdot;

  if ( (dir = g_dir_open(dirname, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      if (*filename == '.') 
        continue;
      if (extension) {
        firstdot = strchr(filename, '.');
        if (!firstdot)
          continue;
        if (g_strcmp0(firstdot+1, extension))
          continue;
      }
      filepath = g_build_filename(dirname, filename, NULL);
      if (g_file_get_contents(filepath, &content, NULL, &error)) {
        g_string_append((*buffer), content);
      }
      else {
        fprintf(stderr, "Cannot read %s: %s\n", filename, error->message);
        g_clear_error(&error);
      }
      g_free(filepath);
      g_free(content);
    }
    g_dir_close (dir);
  }

}/*}}}*/
void 
util_rmdir(const char *path, gboolean only_content, gboolean recursive) {
  GDir *dir = g_dir_open(path, 0, NULL);
  if (dir == NULL) 
    return;
  const char *filename = NULL;
  char *fullpath = NULL;
  while ( (filename = g_dir_read_name(dir)) )  {
    fullpath = g_build_filename(path, filename, NULL);
    if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
      unlink(fullpath);
    }
    else if (recursive) {
      util_rmdir(fullpath, false, true);
      rmdir(fullpath);
    }
    g_free(fullpath);
  }
  if (filename == NULL && !only_content) {
    rmdir(path);
  }
  g_dir_close(dir);
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
char **
util_get_lines(const char *filename) {
  char **ret = NULL;
  char *content = util_get_file_content(filename);
  if (content) {
    ret = g_strsplit(content, "\n", -1);
    g_free(content);
  }
  return ret;
}
/* util_set_file_content(const char *filename, const char *content) {{{*/
gboolean
util_set_file_content(const char *filename, const char *content) {
  GError *error = NULL;
  gboolean ret = true;
  char *link = NULL;
  char *dname = NULL;
  char *realpath = NULL;
  if (g_file_test(filename, G_FILE_TEST_IS_SYMLINK)) {
     link = g_file_read_link(filename, &error);
     if (link == NULL) {
       fprintf(stderr, "Cannot save %s : %s\n", filename, error->message);
       g_clear_error(&error);
       return false;
     }
     dname = g_path_get_dirname(filename);
     realpath = g_build_filename(dname, link, NULL);
     g_free(link);
     g_free(dname);
     filename = realpath;
  }
  if (!g_file_set_contents(filename, content, -1, &error)) {
    fprintf(stderr, "Cannot save %s : %s", filename, error->message);
    g_clear_error(&error);
    ret = false;
  }
  g_free(realpath);
  return ret;
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

/* dwb_check_directory(char *filename (alloc) )  return: char * (alloc) {{{*/
char *
util_check_directory(char *filename) {
  GError *error = NULL;
  char *ret = filename;
  g_return_val_if_fail(filename != NULL, NULL);
  if (g_file_test(filename, G_FILE_TEST_IS_SYMLINK)) {
    ret = g_file_read_link(filename, &error);
    if (error != NULL) {
      fprintf(stderr, "Cannot read link %s : %s, creating a new directory\n", filename, error->message);
      g_mkdir_with_parents(filename, 0700);
      ret = filename;
      g_clear_error(&error);
    }
    else {
      g_free(filename);
    }
  }
  else if (! g_file_test(filename, G_FILE_TEST_IS_DIR) ) {
    g_mkdir_with_parents(filename, 0700);
  }
  return ret;
}/*}}}*/

/* util_get_data_dir(const char *)      return  char * (alloc) {{{*/
char *
util_get_system_data_dir(const char *dir) {
  char *path = NULL;
  path = g_build_filename(SYSTEM_DATA_DIR, dwb.misc.name, dir, NULL);
  if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
    return path;
  }
  g_free(path);
  return NULL;
}/*}}}*/

/* util_get_user_data_dir {{{*/
char *
util_get_user_data_dir(const char *dir) {
  const gchar *data_dir = g_get_user_data_dir();

  char *path = g_build_filename(data_dir, dwb.misc.name, dir, NULL);
  if (path != NULL && !g_file_test(path, G_FILE_TEST_IS_DIR)) {
    g_free(path);
    return NULL;
  }
  return path;
}/*}}}*/


/* util_get_data_file(const char *filename)   return: filename (alloc) or NULL {{{*/
char *
util_get_data_file(const char *filename) {
  if (filename == NULL)
    return NULL;
  char *path = NULL;
  char *ret = NULL;
  char *basename = g_path_get_basename(filename);
  path = util_get_user_data_dir("lib");
  if (path != NULL) {
    ret = g_build_filename(path, basename, NULL);
    if (g_file_test(ret, G_FILE_TEST_EXISTS)) 
      goto clean;
  }

  path = util_get_system_data_dir("lib");
  if (path != NULL)  {
    ret = g_build_filename(path, basename, NULL);
    if (g_file_test(ret, G_FILE_TEST_EXISTS)) 
      goto clean;
  }
  ret = g_strdup(filename);
clean: 
  g_free(path);
  g_free(basename);
  return ret;
}/*}}}*/

/* util_file_remove_line const char *filename, const char *line {{{*/
int
util_file_remove_line(const char *filename, const char *line) {
  int ret = 1;
  char *content = util_get_file_content(filename);
  char **lines = g_strsplit(content, "\n", -1);
  const char *tmp;
  GString *buffer = g_string_new(NULL);
  int last_non_empty = g_strv_length(lines)-1;
  for (int i=0; i<last_non_empty; i++) {
    tmp = lines[i];
    while (g_ascii_isspace(*tmp)) 
      tmp++;
    if (*tmp == '\0' || *tmp == '#')
      g_string_append_printf(buffer, "%s\n", lines[i]);
    else if (*tmp != '\0' && STRCMP_FIRST_WORD(tmp, line)) {
      g_string_append_printf(buffer, "%s\n", lines[i]);
    }
  }
  g_file_set_contents(filename, buffer->str, -1, NULL);

  g_string_free(buffer, true);
  g_free(content);
  g_strfreev(lines);

  return ret;
}/*}}}*/

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
  if (text == NULL)
    return NULL;
  while (g_ascii_isspace(*text))
    text++;

  if (*text != '\0') {
    line = g_strsplit(text, " ", 2);
    nv = dwb_navigation_new(line[0], line[1]);
    g_strfreev(line);
  }
  return nv;
}/*}}}*/

/* dwb_navigation_free(Navigation *n){{{*/
void
dwb_navigation_free(Navigation *n) {
  if (n != NULL) {
    g_free(n->first);
    g_free(n->second);
    g_free(n);
  }
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
  if (line == NULL) 
    return NULL;
  while (g_ascii_isspace(*line))
    line++;
  if (*line != '\0') {
    token = g_strsplit(line, " ", 3);
    q = dwb_quickmark_new(token[1], token[2], token[0]);
    g_strfreev(token);
  }
  return q;
}/*}}}*/

/* dwb_quickmark_free(Quickmark *q) {{{*/
void
dwb_quickmark_free(Quickmark *q) {
  if (q != NULL) {
    g_free(q->key);
    dwb_navigation_free(q->nav);
    g_free(q);
  }
}/*}}}*/
/*}}}*/

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
  return g_strdup(ret);
}/*}}}*/

/* util_compare_path(char *uri)      return: char* {{{*/
int
util_compare_path(const char *a, const char *b) {
  return g_strcmp0(g_strrstr(a, "/"), g_strrstr(b, "/"));
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
  char *tmp;

  if (!append) 
    g_string_append_printf(content, "%s\n", text);

  gboolean ret = false;
  if ( (file = fopen(filename, "r"))) {
    for (int i=0; fgets(buffer, sizeof buffer, file) &&  (max < 0 || i < max-1); i++ ) {
      tmp = buffer;
      while (g_ascii_isspace(*tmp) && *tmp != '\n')
        tmp++;
      if (*tmp == '#' || *tmp == '\n')
        g_string_append(content, buffer);
      else if (STRCMP_FIRST_WORD(text, tmp) && STRCMP_SKIP_NEWLINE(text, tmp) ) {
        g_string_append(content, buffer);
      }
    }
    fclose(file);
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
gtk_box_insert(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, gint padding, int position, GtkPackType packtype) {
  if (packtype == GTK_PACK_START) 
    gtk_box_pack_start(box, child, expand, fill, padding);
  else 
    gtk_box_pack_end(box, child, expand, fill, padding);
  gtk_box_reorder_child(box, child, position);
}
void
gtk_widget_remove_from_parent(GtkWidget *widget) {
  g_object_ref(widget);
  GtkWidget *parent = gtk_widget_get_parent(widget);
  gtk_container_remove(GTK_CONTAINER(parent), widget);
}

char * 
util_strcasestr(const char *haystack, const char *needle) {
  if (needle == NULL || ! *needle )
    return (char *) haystack;
  for (; *haystack; haystack++) {
    if (tolower((int)*haystack) == tolower((int)*needle)) {
      const char *h = haystack, *n = needle;
      for (; *h && *n && tolower((int)*h) == tolower((int)*n); ++h, ++n);
      if (! *n ) 
        return (char*)haystack;
    }
  }
  return NULL;
}
int 
util_strlen_trailing_space(const char *str) {
  int len;
  for (len=0; *str != '\0'; str++, len++);
  return len;
}
const char *
util_str_chug(const char *str) {
  while (g_ascii_isspace(*str))
    str++;
  return str;

}
