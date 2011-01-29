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

/* dwb_util_string_replace(const char *haystack, const char *needle, const char  *replace)      return: char * (alloc){{{*/
char *
dwb_util_string_replace(const char *haystack, const char *needle, const char *replacemant) {
  char **token;
  char *ret = NULL;
  if ( haystack && needle && (token = g_regex_split_simple(needle, haystack, 0, 0)) && strcmp(token[0], haystack)) {
    ret = g_strconcat(token[0], replacemant, token[1], NULL);
    g_strfreev(token);
  }
  return ret;
}/*}}}*/
/* dwb_util_cut_text(char *text, int start, int end) {{{*/
void
dwb_util_cut_text(char *text, int start, int end) {
  int length = strlen(text);
  int del = end - start;

  memmove(&text[start], &text[end], length - end);
  memset(&text[length-del], '\0', del);
}/*}}}*/

/* dwb_util_is_hex(const char *string) {{{*/
gboolean
dwb_util_is_hex(const char *string) {
  char *dup, *loc; 
  dup = loc = g_strdup(string);
  gboolean ret = !strtok(dup, "1234567890abcdefABCDEF");

  FREE(loc);
  return ret;
}/*}}}*/
/* dwb_util_test_connect(const char *uri)      return: int {{{*/
int
dwb_util_test_connect(const char *uri) {
  struct sockaddr_in addr;
  int s; 
  int ret = 1;

  char **token = g_strsplit(uri, ":", 2);
  if (token[0] && token[1]) {
    char *host = !strcmp(token[0], "localhost") ? "127.0.0.1" : token[0];
    int port = strtol(token[1], NULL, 10);

    s = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);
    if ( (ret = connect(s, (struct sockaddr*)&addr, sizeof(addr))) ) {
      fprintf(stderr, "Cannot connect to %s\n",  uri);
    }
  }

  shutdown(s, SHUT_RDWR);
  g_strfreev(token);

  return (!ret);
}/*}}}*/

/* dwb_util_modmask_to_char(guint modmask)      return char*{{{*/
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
  }
  mod[i] = NULL; 
  char *line = g_strjoinv(" ", mod);
  return line;
}/*}}}*/
/* dwb_util_keyval_to_char (guint keyval)      return: char * (alloc) {{{*/
char *
dwb_util_keyval_to_char(guint keyval) {
  char *key = dwb_malloc(6);
  guint32 unichar;
  int length;
  if ( (unichar = gdk_keyval_to_unicode(keyval)) ) {
    if ( (length = g_unichar_to_utf8(unichar, key)) ) {
      memset(&key[length], '\0', 6-length); 
    }
  }
  if (length && isprint(key[0])) {
    return key;
  }
  else {
    FREE(key);
    return NULL;
  }
}/*}}}*/

/* dwb_util_char_to_arg(char *value, DwbType type)    return: Arg*{{{*/
Arg *
dwb_util_char_to_arg(char *value, DwbType type) {
  errno = 0;
  Arg *ret = NULL;
  if (type == BOOLEAN && !value)  {
    Arg a =  { .b = false };
    ret = &a;
  }
  else if (value || type == CHAR) {
    if (value) {
      g_strstrip(value);
      if (strlen(value) == 0) {
        if (type == CHAR) {
          Arg a = { .p = NULL };
          ret = &a;
          return ret;
        }
        return NULL;
      }
    }
    if (type == BOOLEAN) {
      if(!g_ascii_strcasecmp(value, "false") || !strcmp(value, "0")) {
        Arg a = { .b = false };
        ret = &a;
      }
      else {
        Arg a = { .b = true };
        ret = &a;
      }
    }
    else if (type == INTEGER) {
      int n = strtol(value, NULL, 10);
      if (n != LONG_MAX &&  n != LONG_MIN && !errno ) {
        Arg a = { .i = n };
        ret = &a;
      }
    }
    else if (type == DOUBLE) {
      double d;
      if ((d = g_strtod(value, NULL)) ) {
        Arg a = { .d = d };
        ret = &a;
      }
    }
    else if (type == CHAR) {
      Arg a = { .p = !value || (value && !strcmp(value, "null")) ? NULL : g_strdup(value) };
      ret = &a;
    }
    else if (type == COLOR_CHAR) {
      int length = strlen(value);
      if (value[0] == '#' && (length == 4 || length == 7) && dwb_util_is_hex(&value[1])) {
        Arg a = { .p = g_strdup(value) };
        ret = &a;
      }
    }
  }
  return ret;
}/*}}}*/
/* dwb_util_arg_to_char(Arg *arg, DwbType type) {{{*/
char *
dwb_util_arg_to_char(Arg *arg, DwbType type) {
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
      value = g_strdup_printf(tmp);
    }
  }
  return value;
}/*}}}*/

/* dwb_util_navigation_sort_first {{{*/
int
dwb_util_navigation_sort_first(Navigation *a, Navigation *b) {
  return (strcmp(b->first, b->first));
}/*}}}*/
/* dwb_util_keymap_sort_first(KeyMap *, KeyMap *) {{{*/
int
dwb_util_keymap_sort_first(KeyMap *a, KeyMap *b) {
  return strcmp(a->map->n.first, b->map->n.first);
}/*}}}*/
/* dwb_util_keymap_sort_second(KeyMap *, KeyMap *) {{{*/
int
dwb_util_keymap_sort_second(KeyMap *a, KeyMap *b) {
  return strcmp(a->map->n.second, b->map->n.second);
}/*}}}*/
/* dwb_util_web_settings_sort_first(WebSettings *a, WebSettings *b) {{{*/
int
dwb_util_web_settings_sort_first(WebSettings *a, WebSettings *b) {
  return strcmp(a->n.first, b->n.first);
}/*}}}*/
/* dwb_util_web_settings_sort_second (WebSettings *a, WebSettings *b) {{{*/
int
dwb_util_web_settings_sort_second(WebSettings *a, WebSettings *b) {
  return strcmp(a->n.second, b->n.second);
}/*}}}*/

/* dwb_util_get_directory_entries(const char *)   return: GList * {{{*/
GList *
dwb_util_get_directory_entries(const char *path, const char *text) {
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
  }
  list = g_list_sort(list, (GCompareFunc)strcmp);
  return list;
}/*}}}*/
/*dwb_util_get_directory_content(GString **, const char *filename) {{{*/
void 
dwb_util_get_directory_content(GString **buffer, const char *dirname) {
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
/* dwb_util_get_file_content(const char *filename)    return: char * (alloc) {{{*/
char *
dwb_util_get_file_content(const char *filename) {
  GError *error = NULL;
  char *content = NULL;
  if (!(g_file_test(filename, G_FILE_TEST_IS_REGULAR) &&  g_file_get_contents(filename, &content, NULL, &error))) {
    fprintf(stderr, "Cannot open %s: %s\n", filename, error ? error->message : "file not found");
    g_clear_error(&error);
  }
  return content;
}/*}}}*/
/* dwb_util_set_file_content(const char *filename, const char *content) {{{*/
void
dwb_util_set_file_content(const char *filename, const char *content) {
  GError *error = NULL;
  if (!g_file_set_contents(filename, content, -1, &error)) {
    fprintf(stderr, "Cannot save %s : %s", filename, error->message);
    g_clear_error(&error);
  }
}/*}}}*/
/* dwb_util_build_path()       return: char * (alloc) {{{*/
char *
dwb_util_build_path() {
  char *path = g_build_filename(g_get_user_config_dir(), dwb.misc.name, NULL);
  if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(path, 0755);
  }
  return path;
}/*}}}*/
/* dwb_util_get_data_dir(const char *)      return  char * (alloc) {{{*/
char *
dwb_util_get_data_dir(const char *dir) {
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

/* NAVIGATION {{{*/
/* dwb_navigation_new(const char *uri, const char *title) {{{*/
Navigation *
dwb_navigation_new(const char *uri, const char *title) {
  Navigation *nv = dwb_malloc(sizeof(Navigation)); 
  nv->first = uri ? g_strdup(uri) : NULL;
  nv->second = title ? g_strdup(title) : NULL;
  return nv;

}/*}}}*/

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

void 
dwb_free(void *p) {
  if (p) 
    g_free(p);
}

/* dwb_util_domain_from_uri (char *uri)      return: char* {{{*/
char *
dwb_util_domain_from_uri(const char *uri) {
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

/* dwb_util_domain_from_uri (char *uri)      return: char* {{{*/
int
dwb_util_compare_path(const char *a, const char *b) {
  return strcmp(g_strrstr(a, "/"), g_strrstr(b, "/"));
}/*}}}*/

