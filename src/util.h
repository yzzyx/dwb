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

#ifndef UTIL_H
#define UTIL_H

#include "dwb.h"

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
int dwb_util_navigation_compare_first(Navigation *, Navigation *);
int dwb_util_navigation_compare_second(Navigation *, Navigation *);

int dwb_util_keymap_sort_first(KeyMap *, KeyMap *);
int dwb_util_keymap_sort_second(KeyMap *, KeyMap *);
int dwb_util_web_settings_sort_second(WebSettings *, WebSettings *);
int dwb_util_web_settings_sort_first(WebSettings *, WebSettings *);

// files
void dwb_util_get_directory_content(GString **, const char *);
GList * dwb_util_get_directory_entries(const char *path, const char *);
char * dwb_util_get_file_content(const char *);
gboolean dwb_util_set_file_content(const char *, const char *);
char * dwb_util_build_path(void);
char * dwb_util_get_data_dir(const char *);
char * dwb_util_get_data_file(const char *);

// navigation
Navigation * dwb_navigation_new_from_line(const char *);
Navigation * dwb_navigation_new(const char *, const char *);
Navigation * dwb_navigation_dup(Navigation *n);
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
int dwb_util_compare_path(const char *, const char *);
char * dwb_util_basename(const char *);

gboolean dwb_util_file_add(const char *filename, const char *text, int, int);
gboolean dwb_util_file_add_navigation(const char *, const Navigation *, int, int);
void gtk_box_insert(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, gint padding, int position);
char * dwb_util_strcasestr(const char *haystack, const char *needle);

#endif
