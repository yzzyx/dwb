/*
 * Copyright (c) 2012 Stefan Bolte <portix@gmx.net>
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

#ifndef _BSD_SOURCE
#define _BSD_SOURCE 
#endif
#ifndef _BSD_SOURCE
#define _POSIX_SOURCE 
#endif

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
//#include <readline/readline.h>
#include <termios.h>
#include <sys/stat.h>

#include <glib-2.0/glib.h>
#include <libsoup-2.4/libsoup/soup.h>

#define API_BASE "https://api.bitbucket.org/1.0/repositories/portix/dwb_extensions/src/tip/src/?format=yaml"
#define REPO_BASE "https://bitbucket.org/portix/dwb_extensions" 
#define REPO_TREE "/raw/tip/src" 

#define SKIP(line, c) do{ \
  while(*line && *line != c)line++; line++; } while(0)

#define SKIP_SPACE(X) do { while(g_ascii_isspace(*(X))) (X)++; } while(0)

#ifndef false
#define false 0
#endif
#ifndef true
#define true (!false)
#endif

#define TMPL_CONFIG     "___CONFIG"
#define TMPL_SCRIPT     "___SCRIPT"
#define TMPL_DISABLED   "___DISABLED"

#define REGEX_REPLACE(X, name) (g_strdup_printf("(?<=^|\n)//<%s"TMPL_##X".*//>%s"TMPL_##X"\\s*(?=\n|$|/*)\\s*", name, name))
#define EXT(name) "\033[1m"#name"\033[0m"
#define FREE0(X) (X == NULL ? NULL : (X = (g_free(X), NULL)))

enum 
{
  F_NO_CONFIG       = 1<<0,
  F_BIND            = 1<<1,
  F_UPDATE          = 1<<2,
  F_NO_CONFIRM      = 1<<3,

  F_MATCH_MULTILINE = 1<<10,
  F_MATCH_CONTENT   = 1<<11
};

static char *m_installed;
static char *m_loader;
static char *m_meta_data;
static char *m_user_dir;
static const char *m_system_dir;
static const char *m_editor;
static const char *m_diff;
static SoupSession *session;

static void
vprint_error(const char *format, va_list args) 
{
  fputs("\033[31m!!!\033[0m ", stderr);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
}

static void
print_error(const char *format, ...) 
{
  va_list args;
  va_start(args, format);
  vprint_error(format, args);
  va_end(args);
}

static void
notify(const char *format, ...) 
{
  va_list args;
  va_start(args, format);
  fputs("\033[32m==>\033[0m ", stdout);
  vfprintf(stdout, format, args);
  fputc('\n', stdout);
  va_end(args);
}

static void 
clean_up() 
{
  g_free(m_installed);
  g_free(m_loader);
  g_free(m_meta_data);
  g_free(m_user_dir);
  g_object_unref(session);
}

static void 
die(int status, const char *format, ...) 
{
  va_list args;
  va_start(args, format);
  vprint_error(format, args);
  print_error("Aborting");
  va_end(args);
  clean_up();
  exit(status);
}

static void
for_each(char **argv, int flags, int (*func)(const char *, int)) 
{
  if (argv != NULL) 
  {
    for (int i=0; argv[i]; i++) 
      func(argv[i], flags);
  }
}

static void 
check_dir(const char *dir) {
  if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents(dir, 0700);
}

static int
regex_replace_file(const char *path, const char *regex, const char *replacement) 
{
  int ret = -1;
  char *content = NULL; 
  if (g_file_get_contents(path, &content, NULL, NULL)) 
  {
    char **matches = g_regex_split_simple(regex, content, G_REGEX_DOTALL, 0);
    if (matches[0]) 
    {
      g_free(content);
      content = g_strdup_printf("%s%s%s", matches[0], replacement ? replacement : "", matches[1] ? matches[1] : "");
      if (g_file_set_contents(path, content, -1, NULL))
        ret = 0;
    }
    else 
      g_file_set_contents(path, replacement, -1, NULL);

    g_strfreev(matches);
  }
  else 
    g_file_set_contents(path, replacement, -1, NULL);

  g_free(content);
  return ret;
}

static int 
set_data(const char *name, const char *data, const char *template, int flags) 
{
  char *regex, *content = NULL;
  char *format;
  int ret = -1;
  char **matches = NULL;
  if (flags & F_MATCH_MULTILINE) 
    format = "(?<=^|\n)/\\*<%s%s.*%s%s>\\*/\\s*(?=\n|$)";
  else 
    format = "(?<=^|\n)//<%s%s.*//>%s%s\\s*(?=\n|$)";

  regex = g_strdup_printf(format, name, template, name, template);
  if (g_file_get_contents(m_loader, &content, NULL, NULL)) 
  {
    matches = g_regex_split_simple(regex, content, G_REGEX_DOTALL, 0);
    GString *buffer = g_string_new(matches[0]);

    if (flags & F_MATCH_MULTILINE) 
      g_string_append_printf(buffer, "/*<%s%s%s%s%s>*/\n", 
          name, template, data ? data : "\n", name, template);
    else 
      g_string_append_printf(buffer, "//<%s%s%s//>%s%s\n", 
          name, template, data ? data : "\n", name, template);
    
    if (matches[1]) 
      g_string_append(buffer, matches[1]);

    if (g_file_set_contents(m_loader, buffer->str, -1, NULL))
     ret = 0;

    g_string_free(buffer, true);
  }
  g_free(regex);
  g_free(content);
  return ret;
}


static char *
get_data(const char *name, const char *data, const char *template, int flags) 
{
  char *ret = NULL;
  char *content = NULL, *regex = NULL;
  const char *new_data = NULL;
  const char *format;
  const char *nname = name == NULL ? "" : name;

  if (flags & F_MATCH_MULTILINE)
    format = "(?<=^|\n)/\\*<%s%s|%s%s>\\*/\\s*(?=\n|$)";
  else
    format = "(?<=^|\n)//<%s%s|//>%s%s\\s*(?=\n|$)";

  regex = g_strdup_printf(format, nname, template, nname, template);
  if (flags & F_MATCH_CONTENT) 
    new_data = data;
  else 
  {
    g_file_get_contents(data, &content, NULL, NULL);
    new_data = content;
  }
  if (new_data != NULL) 
  {
    char **matches = g_regex_split_simple(regex, new_data, G_REGEX_DOTALL, 0);
    if (matches[1] != NULL)
      ret = g_strdup(matches[1]);
    g_strfreev(matches);
  }
  g_free(content);
  g_free(regex);
  return ret;
}

static int
grep(const char *filename, const char *name, char *buffer, size_t length) 
{
  FILE *f = fopen(filename, "r");
  char line[128];
  int i = -1;
  char *first_space;
  if (f == NULL)
    return -1;
  while(fgets(line, sizeof(line), f) != NULL) 
  {
    first_space = strchr(line, ' ');
    if (first_space == NULL)
      continue;
    if (!strncmp(name, line, first_space - line)) 
    {
      unsigned int count = 0;
      while (line[count] && line[count] != '\n' && count<length-1) 
      {
        buffer[count] = line[count];
        count++;
      }
      buffer[count++] = '\0';
      i = count;
      break;
    }
  }
  fclose(f);
  return i;
}

static int
update_installed(const char *name, const char *meta) 
{
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%s\n", meta);
  char *regex = g_strdup_printf("(?<=^|\n)%s\\s\\w+\n", name);
  regex_replace_file(m_installed, regex, buffer);
  g_free(regex);
  return 0;

}
char *
xreadline(char *buffer, size_t length, const char *prompt) {
  for (;;) {
    fputs(prompt, stderr);
    if (isatty(STDIN_FILENO))
      tcflush(STDIN_FILENO, TCIFLUSH);
    if (fgets(buffer, length, stdin)) {
      if (buffer)
        break;
    }
  }
  char *backup = buffer;
  while(*buffer) {
    if (*buffer == '\n') {
      *buffer = '\0';
      break;
    }
    buffer++;
  }
  return backup;
}

static int 
yes_no(int preset, const char *format, ...) 
{
  va_list args; 
  char *response;
  int ret = -1;
  char prompt[512];
  char cformat[512];
  char buffer[128];

  snprintf(cformat, sizeof(cformat), "\033[34m:::\033[0m %s %s? ", format, preset ? "(Y/n)" : "(y/N)");

  va_start(args, format);
  vsnprintf(prompt, sizeof(prompt), cformat, args);
  va_end(args);

  while (ret == -1) 
  {
    response = xreadline(buffer, sizeof(buffer), prompt);
    if (response != NULL) 
    {
      g_strstrip(response);
      if (*response == '\0')
        ret = preset;
      else if (! strcasecmp("y", response))
        ret = 1;
      else if (! strcasecmp("n", response))
        ret = 0;
      else 
        print_error("try again");
    }
    else 
      ret = preset;
  }
  return ret;
}

static char *
get_response(char *buffer, size_t length, const char *format, ...) 
{
  char prompt[512];
  char cformat[512];
  va_list args; 

  snprintf(cformat, sizeof(cformat), "\033[34m:::\033[0m %s ", format);
  va_start(args, format);
  vsnprintf(prompt, sizeof(prompt), cformat, args);
  va_end(args);

  return xreadline(buffer, length, prompt);
}

int 
create_tmp(char *buffer, int size, const char *template, int suffix_length) {
  strncpy(buffer, template, size);
  int fd = mkstemps(buffer, suffix_length);
  if (fd == -1) 
    die(1, "Cannot create temporary file");
  return fd;
}

static int
diff(const char *text1, const char *text2, char **ret) 
{
  gboolean spawn_success = true;
  GError *e = NULL;
  char *new_text = NULL;
  char file1[32], file2[32];

  if (g_strcmp0(text1, text2) == 0 || text1 == NULL || text2 == NULL) 
  {
    *ret = NULL;
    return 0;
  }

  notify("The default configuration differs from configuration in use");
  if (! yes_no(1, "Edit configuration")) 
  {
    *ret = g_strdup(text1);
    return 1;
  }

  int fd1 = create_tmp(file1, sizeof(file1), "/tmp/tmpXXXXXXorig.js", 7);
  int fd2 = create_tmp(file2, sizeof(file2), "/tmp/tmpXXXXXXnew.js", 6);

  char *text2_new = g_strdup_printf("// THIS FILE WILL BE DISCARDED\n%s// THIS FILE WILL BE DISCARDED", text2);
  if (g_file_set_contents(file1, text1, -1, &e) && g_file_set_contents(file2, text2_new, -1, &e)) 
  {
    char *args[4];
    args[0] = (char *)m_diff;
    args[1] = file1;
    args[2] = file2;
    args[3] = NULL;

    spawn_success = g_spawn_sync(NULL, args, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_CHILD_INHERITS_STDIN, NULL, NULL, NULL, NULL, NULL, NULL);
    if (spawn_success) 
      g_file_get_contents(file1, &new_text, NULL, NULL);

    unlink(file1);
    unlink(file2);
  }
  else 
    print_error("Cannot diff : %s", e->message);

  g_free(text2_new);
  close(fd1);
  close(fd2);

  if (!spawn_success)
    die(1, "Cannot spawn "EXT(%s)", please set "EXT(EDITOR)" to an appropriate value", m_editor);
  *ret = new_text;
  return 1;
}

static char *
edit(const char *text) 
{
  gboolean spawn_success = true;
  char *new_config = NULL;
  char file[32];

  int fd = create_tmp(file, sizeof(file), "/tmp/tmpXXXXXX.js", 3);

  if (g_file_set_contents(file, text, -1, NULL)) 
  {
    char *args[3]; 
    args[0] = (char*)m_editor;
    args[1] = file;
    args[2] = NULL;

    spawn_success = g_spawn_sync(NULL, args, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_CHILD_INHERITS_STDIN, NULL, NULL, NULL, NULL, NULL, NULL);
    if (spawn_success)
      g_file_get_contents(file, &new_config, NULL, NULL);

    unlink(file);
  }
  close(fd);
  if (!spawn_success)
    die(1, "Cannot spawn "EXT(%s)", please set "EXT(EDITOR)" to an appropriate value", m_editor);
  return new_config;
}

static void 
set_loader(const char *name, const char *config, int flags) 
{
  char *script = NULL;
  char shortcut[64], command[128];
  gboolean load = true;
  gboolean has_cmd;


  notify("Updating extension loader", name, m_loader);

  if (flags & F_BIND) 
  {
    while (get_response(shortcut, sizeof(shortcut), "Shortcut for toggling "EXT(%s)"?", name) == NULL || *shortcut == '\0');
    get_response(command, sizeof(command), "Command for toggling "EXT(%s)"?", name);
    load = yes_no(1, "Load "EXT(%s)" on startup", name);
  }
  has_cmd = command != NULL && *command != '\0';

  if (config == NULL || flags & F_NO_CONFIG) 
  {
    if (flags & F_BIND)
      script = g_strdup_printf("//<%s"TMPL_SCRIPT"\nextensions.bind(\"%s\", \"%s\", {\n"
              "  load : %s%s%s%s\n});\n//>%s"TMPL_SCRIPT"\n", 
          name, name, shortcut,
          load    ? "true" : "false",
          has_cmd ? ",\n  command : \"" : "",
          has_cmd ? command : "",
          has_cmd ? "\"" : "",
          name);
    else 
      script = g_strdup_printf("//<%s"TMPL_SCRIPT"\nextensions.load(\"%s\");\n//>%s"TMPL_SCRIPT"\n", name, name, name);
  }
  else 
  {
    if (flags & F_BIND) 
      script = g_strdup_printf("//<%s"TMPL_SCRIPT"\n"
              "var config_%s = {\n"
              "//<%s"TMPL_CONFIG"%s//>%s"TMPL_CONFIG"\n};\n"
              "extensions.bind(\"%s\", \"%s\", {\n"
              "  config : config_%s,\n"
              "  load : %s%s%s%s\n});\n//>%s"TMPL_SCRIPT"\n", 
          name, name, name, config, name, name, shortcut, name, 
          load    ? "true" : "false",
          has_cmd ? ",\n  command : \"" : "",
          has_cmd ? command : "",
          has_cmd ? "\"" : "",
          name);
    else
      script = g_strdup_printf("//<%s"TMPL_SCRIPT"\nextensions.load(\"%s\", {\n"
              "//<%s"TMPL_CONFIG"%s//>%s"TMPL_CONFIG"\n});\n//>%s"TMPL_SCRIPT"\n", 
          name, name, name, config, name, name);
  }
  if (! g_file_test(m_loader, G_FILE_TEST_EXISTS) ) 
  {
    char *file_content = g_strdup_printf("//!javascript\n%s", script);
    g_file_set_contents(m_loader, file_content, -1, NULL);
    g_free(file_content);
  }
  else 
  {
    char *regex = REGEX_REPLACE(SCRIPT, name);
    regex_replace_file(m_loader, regex, script);
    g_free(regex);
  }
  g_free(script);
}

static int 
add_to_loader(const char *name, const char *content, int flags) 
{
  g_return_val_if_fail(content != NULL, -1);
  char *config = NULL;
  const char *new_config = NULL;
  char *data = NULL;
  gchar **matches = g_regex_split_simple("//<DEFAULT_CONFIG|//>DEFAULT_CONFIG", content, G_REGEX_DOTALL, 0);

  if (matches[1] == NULL) 
    notify("No default configuration found");
  else if (flags & F_NO_CONFIRM) {
    notify("Skipping configuration check");
    if (! (flags & F_UPDATE) ) 
      new_config = matches[1];
  }
  else if ((flags & F_UPDATE)) 
  {
    data = get_data(name, m_loader, TMPL_CONFIG, 0);
    if (diff(data, matches[1], &config) == 0) {
      notify("Config is up to date");
      goto unwind;
    }
    else 
      new_config = config;
  }
  else if (!(flags & F_NO_CONFIG) && yes_no(1, "Edit configuration") == 1) 
    new_config = config = edit(matches[1]);
  else 
    new_config = matches[1];

  if ( (flags & F_UPDATE) != 0 ) {
    notify("Updating extension-loader");
    set_data(name, new_config, TMPL_CONFIG, 0);
  }
  else if ( (flags & F_NO_CONFIRM) == 0 && new_config != NULL ) 
    set_loader(name, new_config, flags);

unwind:
  g_strfreev(matches);
  g_free(config);
  g_free(data);
  return 0;
}

static gboolean 
check_installed(const char *name) 
{
  char meta[128];
  if (grep(m_installed, name, meta, sizeof(meta)) == -1) 
    return false;
  return true;
}

static int 
install_extension(const char *name, int flags) 
{
  int ret = -1, status;
  char buffer[512];
  char meta[128];
  char *content = NULL;
  GError *e = NULL;

  if (grep(m_meta_data, name, meta, sizeof(meta)) == -1) 
    die(1, "extension %s not found", name);

  snprintf(buffer, sizeof(buffer), "%s/%s", m_system_dir, name);
  if (g_file_test(buffer, G_FILE_TEST_EXISTS)) 
  {
    notify("Using %s", buffer);
    if (g_file_get_contents(buffer, &content, NULL, NULL) ) 
    {
      if (add_to_loader(name, content, flags) == 0) 
      {
        update_installed(name, meta);
        ret = 0;
      }
      g_free(content);
    }
    else 
      print_error("Opening %s failed", buffer);
  }
  else 
  {
    snprintf(buffer, sizeof(buffer), "%s/%s", REPO_BASE REPO_TREE, name);
    notify("Downloading %s", buffer);
    SoupMessage *msg = soup_message_new("GET", buffer);

    status = soup_session_send_message(session, msg);
    if (status == 200 && msg->response_body->data) 
    {
      snprintf(buffer, sizeof(buffer), "%s/%s", m_user_dir, name);
      if (g_file_set_contents(buffer, msg->response_body->data, -1, &e)) 
      {
        if (add_to_loader(name, msg->response_body->data, flags) == 0) 
        {
          update_installed(name, meta);
          ret = 0;
        }
      }
      else 
        print_error("Saving %s failed: %s", name, e->message);
    }
    else 
      print_error("Download failed with status %d", status);
    g_object_unref(msg);
  }
  return ret;
}

static int
sync_meta(const char *output) 
{
  int ret = 0, status;
  FILE *file;
  const char *response, *name;
  struct stat st;
  char buffer[1024];

  static gboolean sync = false;

  check_dir(m_user_dir);

  if (sync)
    return ret;
  sync = true;

  notify("Syncing with " REPO_BASE);
  SoupMessage *msg = soup_message_new("GET", API_BASE);
  status = soup_session_send_message(session, msg);

  if (status == 200) 
  {
    if (msg->response_body && msg->response_body->data) 
    {
      file = fopen(output, "w");
      if (file == NULL) 
      {
        ret = -1;
        print_error("Cannot open %s for writing", m_meta_data);
        goto unwind;
      }
      response = msg->response_body->data;
      while(*response) 
      {
        SKIP_SPACE(response);
        while (*response && *response != '-') 
          SKIP(response, '\n');

        if (*response == '\n')
          continue;

        if (*response == '\0')
          break;

        SKIP(response, '/');

        while (*response && *response != ',') 
          fputc(*(response++), file);

        fputc(' ', file);

        SKIP(response, ':');

        response += 2;
        while (*response && *response != ',') 
          fputc(*(response++), file);

        fputc('\n', file);
        SKIP(response, '\n');
      }

      GDir *d = g_dir_open(m_system_dir, 0, NULL);
      if (d == NULL) 
        die(1, "Cannot open %s", m_system_dir);
      while((name = g_dir_read_name(d)) != NULL) 
      {
        snprintf(buffer, sizeof(buffer), "%s/%s", m_system_dir, name);
        if (stat(buffer, &st) != -1) 
          fprintf(file, "%s %lu\n", name, st.st_mtime);
      }
      g_dir_close(d);
      fclose(file);
    }
  }
  else 
  {
    ret = -1;
    print_error("Syncing failed, request returned with status %d\n", status);
  }
unwind:
  g_object_unref(msg);
  return ret;
}

static int
cl_change_config(const char *name, int flags) 
{
  char *config, *new_config = NULL;

  if (!check_installed(name))
    die(1, "Extension "EXT(%s)" is not installed", name);

  config = get_data(name, m_loader, TMPL_CONFIG, 0);

  if (config) 
  {
    if (flags & (F_NO_CONFIG | F_NO_CONFIRM) ) 
      new_config = config;
    else 
    {
      new_config = edit(config);
      g_free(config);
    }

    set_loader(name, new_config, flags);
    g_free(new_config);
  }
  else 
  {
    notify("No config found, you can use extensionrc instead");
    set_loader(name, NULL, flags);
  }
  return 0;
}

static int 
cl_install(const char *name, int flags) 
{
  if ( !(flags & F_UPDATE) 
      && check_installed(name) 
      && !(flags & F_NO_CONFIRM)
      && !yes_no(0, EXT(%s)" is already installed, continue anyway", name))
    return -1;

  notify("%s "EXT(%s), (flags & F_UPDATE) ? "Updating" : "Installing", name);
  if (sync_meta(m_meta_data) == 0) 
    return install_extension(name, flags);
  return -1;
}

static int
cl_uninstall(const char *name, int flags) 
{
  (void) flags;
  char *path, *regex; 
  if (!check_installed(name)) 
    die(1, "extension %s is not installed", name);

  path = g_build_filename(m_user_dir, name, NULL);
  if (g_file_test(path, G_FILE_TEST_EXISTS)) {
    notify("Removing %s", name);
    unlink(path);
  }
  g_free(path);

  regex = REGEX_REPLACE(SCRIPT, name);
  if (regex_replace_file(m_loader, regex, NULL) != -1) 
    notify("Updating extension loader");
  g_free(regex);

  regex = g_strdup_printf("(?<=^|\n)%s\\s\\w+\n", name);
  if (regex_replace_file(m_installed, regex, NULL) != -1) 
    notify("Updating metadata");
  g_free(regex);
  return 0;
}

static int
cl_disable(const char *name, int flags) 
{
  (void) flags;
  char *regex, *data, *new_data;
  if (!check_installed(name))
    die(1, "Extension "EXT(%s)" is not installed", name);

  regex = g_strdup_printf("(?<=^|\n)/\\*<%s"TMPL_DISABLED".*%s"TMPL_DISABLED">\\*/\\s*(?=$|\n)", name, name);
  data = get_data(name, m_loader, TMPL_SCRIPT, 0);

  if (!g_regex_match_simple(regex, data, G_REGEX_DOTALL, 0)) 
  {
    new_data = g_strdup_printf("\n/*<%s"TMPL_DISABLED"%s%s"TMPL_DISABLED">*/\n", name, data ? data : "\n", name);
    if (set_data(name, new_data, TMPL_SCRIPT, 0) != -1) 
      notify(EXT(%s)" disabled", name);
    else
      print_error("Unable to disable "EXT(%s), name);
    g_free(new_data);
  }
  else 
    print_error(EXT(%s)" is already disabled", name);
  g_free(data);
  g_free(regex);
  return 0;
}

static int
cl_enable(const char *name, int flags) 
{
  (void)flags;
  if (!check_installed(name))
    die(1, "Extension "EXT(%s)" is not installed", name);
  char *data = get_data(name, m_loader, TMPL_DISABLED, F_MATCH_MULTILINE);
  if (data != NULL) 
  {
    if (set_data(name, data, TMPL_SCRIPT, 0) != -1) 
      notify(EXT(%s)" enabled", name);
    else
      print_error("Unable to enable "EXT(%s), name);
    g_free(data);
  }
  else 
    print_error(EXT(%s)" is already enabled", name);
  return 0;
}

static void
do_update(const char *meta, int flags) 
{
  char buffer[128];
  char *space = strchr(meta, ' ');
  if (space != NULL) 
  {
    strncpy(buffer, meta, MIN(sizeof(buffer), (unsigned int)(space - meta + 1)));
    if ((flags & F_NO_CONFIRM) || yes_no(1, "Update "EXT(%s), buffer))
      if (cl_install(buffer, flags | F_UPDATE)) 
        notify(EXT(%s)" successfully updated", buffer);
  }
}

static int
cl_update(int flags) {
  sync_meta(m_meta_data);
  char *meta = NULL;
  char *installed = NULL;
  size_t l_inst = 0, l_meta = 0;
  int updates = 0;
  char **lines_inst;

  if (!g_file_get_contents(m_installed, &installed, &l_inst, NULL) || l_inst == 0) 
    die(1, "No installed extensions found");
  if (!g_file_get_contents(m_meta_data, &meta, &l_meta, NULL) || l_meta == 0) 
    die(1, "Cannot open metadata");

  lines_inst = g_strsplit(installed, "\n", -1);
  g_free(installed);

  for (int i=0; lines_inst[i]; i++) 
  {
    if (*(lines_inst[i]) == '\0')
      continue;
    char *regex = g_strdup_printf("(?<=^|\n)%s(?=$|\n)", lines_inst[i]);
    if (!g_regex_match_simple(regex, meta, G_REGEX_DOTALL, 0)) 
    {
      do_update(lines_inst[i], flags);
      updates++;
    }
  }

  if (updates == 0)
    notify("Up to date");
  else 
    notify("Done");

  g_strfreev(lines_inst);
  return 0;
}

static char **
get_list(const char *path) 
{
  int installed = 0, i;
  char *content = NULL;
  GSList *list = NULL;
  char **ret = NULL, **matches = NULL;
  char *tmp;
  if (g_file_get_contents(path, &content, NULL, NULL)) 
  {
    matches = g_regex_split_simple("\\s\\w*(\n|$)", content, G_REGEX_DOTALL, 0);
    for (int i=0; matches[i]; i++) 
    {
      tmp = matches[i];
      SKIP_SPACE(tmp);
      if (*tmp) 
      {
        list = g_slist_prepend(list, tmp);
        installed++;
      }
    }
  }
  if (list != NULL) {
    list = g_slist_sort(list, (GCompareFunc)g_strcmp0);

    ret = g_new(char*, installed+1);

    i=0;
    for (GSList *l = list; l; l=l->next)
      ret[i++] = g_strdup(l->data);
    ret[i] = NULL;
    g_slist_free(list);
  }
  g_strfreev(matches);
  g_free(content);
  return ret;
}

static void 
list(const char *file, const char *message, gboolean sync) {
  if (sync)
    sync_meta(file);
  char **list = get_list(file);
  if (list != NULL) 
  {
    notify("%s:", message);
    for (int i=0; list[i]; i++) 
      printf("  * %s\n", list[i]);

    g_strfreev(list);
  }
  else 
    notify("No extensions installed");

}

static int
cl_info(const char *name, int flags)  
{
  (void) flags;

  SoupMessage *msg = NULL;
  char *data = NULL, *path;
  const char *tmp;

  path = g_build_filename(m_system_dir, name, NULL);
  if ((data = get_data(NULL, path, "INFO", F_MATCH_MULTILINE)) != NULL) 
    goto unwind;
  FREE0(data);
  g_free(path);

  path = g_build_filename(m_user_dir, name, NULL);
  if ((data = get_data(NULL, path, "INFO", F_MATCH_MULTILINE)) != NULL) 
    goto unwind;
  FREE0(data);
  g_free(path);

  path = g_strconcat(REPO_BASE REPO_TREE, "/", name, NULL);
  msg = soup_message_new("GET", path);
  int status = soup_session_send_message(session, msg);
  if (status == 200) 
    data = get_data(NULL, msg->response_body->data, "INFO", F_MATCH_MULTILINE | F_MATCH_CONTENT);
unwind: 
  if (data != NULL) 
  {
    tmp = data;
    SKIP_SPACE(tmp);
    printf("\033[1m%s\033[0m - %s", name, tmp);
  }
  else 
    print_error("Extension "EXT(%s)" not found", name);

  g_free(path);
  g_free(data);
  if (msg != NULL)
    g_object_unref(msg);
  return 0;
}

static int
cl_update_ext(const char *name, int flags) 
{
  if (cl_install(name, flags | F_UPDATE)) 
    notify(EXT(%s)" successfully updated", name);
  return 0;
}

char *
get_config(const char *name) {
  if (!check_installed(name)) {
    print_error(EXT(%s)" is not installed", name);
    return NULL;
  }
  char *config = get_data(name, m_loader, TMPL_CONFIG, 0);
  if (config == NULL) 
    print_error("No configuration found for "EXT(%s), name);
  return config;
}

static int 
cl_show_config(const char *name, int flags) {
  (void) flags;
  char *config = get_config(name);
  if (config) {
    notify("Configuration for "EXT(%s)"\n%s", name, config);
    g_free(config);
    return 0;
  }
  return -1;
}

static int 
cl_edit(const char *name, int flags) {
  (void) flags;
  char *config = get_config(name);
  if (config != NULL) {
    char *new_config = edit(config);
    if (new_config != NULL) {
      set_data(name, new_config, TMPL_CONFIG, 0);
      g_free(new_config);
    }
    g_free(config);
    return 0;
  }
  return -1;
}

int 
main(int argc, char **argv) 
{
  g_type_init();
  GError *e = NULL;
  GOptionContext *ctx;
  char *config_dir;
  gboolean missing = argc == 1;
  char **o_install = NULL;
  char **o_remove = NULL;
  gboolean o_bind = false;
  char **o_setbind = NULL;
  char **o_setload = NULL;
  char **o_disable = NULL;
  char **o_enable = NULL;
  char **o_info = NULL;
  char **o_update_ext = NULL;
  char **o_show_config = NULL;
  char **o_edit = NULL;
  char *o_proxy = NULL;
  gboolean o_noconfig = false;
  gboolean o_update = false;
  gboolean o_list_installed = false;
  gboolean o_list_all = false;
  gboolean o_no_confirm = false;
  int flags = 0;
  const char *proxy;
  GOptionEntry options[] = {
    { "list-all",  'a', 0, G_OPTION_ARG_NONE, &o_list_all, "List all installed extensions",  NULL},
    { "bind",     'b', 0, G_OPTION_ARG_NONE, &o_bind, "When installing an extension use extensions.bind instead of extensions.load", NULL },
    { "setbind",  'B', 0, G_OPTION_ARG_STRING_ARRAY, &o_setbind, "Edit configuration, use extensions.bind", "<extension>" },
    { "config",   'c', 0, G_OPTION_ARG_STRING_ARRAY, &o_show_config, "Show configuration for <extension>", "<extension>" },
    { "disable",  'd', 0, G_OPTION_ARG_STRING_ARRAY, &o_disable, "Disable <extension>", "<extension>" },
    { "enable",   'e', 0, G_OPTION_ARG_STRING_ARRAY, &o_enable,  "Enable <extension>", "<extension>" },
    { "edit",     'E', 0, G_OPTION_ARG_STRING_ARRAY, &o_edit,  "Edit configuration with EDITOR", "<extension>" },
    { "install",  'i', 0, G_OPTION_ARG_STRING_ARRAY, &o_install, "Install <extension>",  "<extension>" },
    { "info",     'I', 0, G_OPTION_ARG_STRING_ARRAY, &o_info, "Show info about <extension>",  "<extension>" },
    { "list-installed",  'l', 0, G_OPTION_ARG_NONE, &o_list_installed, "List installed extensions",  NULL},
    { "setload",  'L', 0, G_OPTION_ARG_STRING_ARRAY, &o_setload, "Edit configuration for <extension>, use exensions.load", "<extension>" },
    { "no-config", 'n', 0, G_OPTION_ARG_NONE, &o_noconfig, "Don't use config in loader script, use extensionrc instead", NULL },
    { "no-confirm",   'N', 0, G_OPTION_ARG_NONE, &o_no_confirm,  "Update extensions", NULL },
    { "remove",   'r', 0, G_OPTION_ARG_STRING_ARRAY, &o_remove, "Remove <extension>", "<extension>" },
    { "proxy",   'p', 0, G_OPTION_ARG_STRING, &o_proxy, "HTTP-proxy to use", NULL },
    { "upgrade",   'u', 0, G_OPTION_ARG_NONE, &o_update,  "Update all extensions", NULL },
    { "update",   'U', 0, G_OPTION_ARG_STRING_ARRAY, &o_update_ext,  "Update <extension>", "<extension>" },
    { NULL, 0, 0, 0, NULL, NULL, NULL },
  };

  ctx = g_option_context_new(NULL);
  g_option_context_add_main_entries(ctx, options, NULL);
  if (!g_option_context_parse(ctx, &argc, &argv, &e) || missing) {
    if (missing) 
      fprintf(stderr, "Missing argument\n");
    else if (argc > 1) 
      fprintf(stderr, "Unknown option %s\n", argv[1]);
    else 
      fprintf(stderr, "%s\n", missing ? "" : e->message);
    fputs(g_option_context_get_help(ctx, false, NULL), stderr);
    return 1;
  }

  session = soup_session_sync_new();
  if (o_proxy != NULL)
      proxy = o_proxy;
  else if ( (proxy = g_getenv("https_proxy" )) == NULL )
      proxy = g_getenv("http_proxy");
  if (proxy) 
  {
      SoupURI *uri = soup_uri_new(proxy);
      g_object_set(session, "proxy-uri", uri, NULL);
      soup_uri_free(uri);
  }

  m_user_dir = g_build_filename(g_get_user_data_dir(), "dwb", "extensions", NULL);
  check_dir(m_user_dir);

  config_dir = g_build_filename(g_get_user_config_dir(), "dwb", "userscripts", NULL);
  check_dir(config_dir);

  m_loader = g_build_filename(config_dir, "extension_loader.js", NULL);
  g_free(config_dir);

  m_meta_data = g_build_filename(m_user_dir, ".metadata", NULL);
  m_installed = g_build_filename(m_user_dir, ".installed", NULL);

  m_system_dir = SYSTEM_EXTENSION_DIR;
  if (!g_file_test(m_system_dir, G_FILE_TEST_EXISTS))
    die(1, "FATAL: %s not found, check your installation", m_system_dir);

  if ((m_editor = g_getenv("EDITOR")) == NULL)
    m_editor = "vim";
  if ((m_diff = g_getenv("DIFF_VIEWER"))== NULL)
    m_diff = "vimdiff";

  if (o_bind)
    flags |= F_BIND;
  if (o_noconfig)
    flags |= F_NO_CONFIG;
  if (o_no_confirm)
    flags |= F_NO_CONFIRM;

  if (o_list_all) 
    list(m_meta_data, "Available extensions", true);
  if (o_list_installed) 
    list(m_installed, "Installed extensions", false);
  if (o_update) 
    cl_update(flags);

  for_each(o_update_ext, flags, cl_update_ext);
  for_each(o_info, flags, cl_info);
  for_each(o_setbind, flags | F_BIND, cl_change_config);
  for_each(o_disable, flags, cl_disable);
  for_each(o_enable, flags, cl_enable);
  for_each(o_setload, flags & ~F_BIND, cl_change_config);
  for_each(o_remove, flags, cl_uninstall);
  for_each(o_install, flags, cl_install);
  for_each(o_show_config, flags, cl_show_config);
  for_each(o_edit, flags, cl_edit);
  clean_up();
  return 0;
}
