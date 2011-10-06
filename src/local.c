#include "local.h"
#include "commands.h"


/* local_toggle_hidden_cb {{{*/
gboolean
local_toggle_hidden_cb(WebKitDOMElement *el, WebKitDOMEvent *ev, GList *gl) {
  dwb.state.hidden_files = webkit_dom_html_input_element_get_checked(WEBKIT_DOM_HTML_INPUT_ELEMENT(el));
  commands_reload(NULL, NULL);
  return true;
}/*}}}*/

/* local_load_directory_cb {{{*/
void
local_load_directory_cb(WebKitWebView *wv) {
  if (webkit_web_view_get_load_status(wv) != WEBKIT_LOAD_FINISHED) 
    return;
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  WebKitDOMElement *e = webkit_dom_document_get_element_by_id(doc, "dwb_local_checkbox");
  webkit_dom_html_input_element_set_checked(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), dwb.state.hidden_files);
  webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(e), "change", G_CALLBACK(local_toggle_hidden_cb), false, dwb.state.fview);
  g_signal_handlers_disconnect_by_func(wv, local_load_directory_cb, NULL);
}/*}}}*/

/* local_show_directory(WebKitWebView *, const char *path, gboolean add_to_history) 
 * 
 * Param: 
 * WebKitWebView: 
 * path: 
 * gboolean add_to_history
 * {{{*/
static void 
local_show_directory(WebKitWebView *web, const char *path, gboolean add_to_history) {
  /* TODO needs fix: when opening local files close commandline  */
  char *fullpath; 
  const char *filename;
  char *newpath = NULL;
  const char *tmp;
  const char *orig_path;
  GSList *content = NULL;
  GDir *dir = NULL;
  GString *buffer;
  GError *error = NULL;
  if (g_path_is_absolute(path)) {
    orig_path = path;
  }
  else {
    /*  resolve absolute path */
    char *current_dir = g_get_current_dir();
    char *full = g_build_filename(current_dir, path, NULL);
    char **components = g_strsplit(full, "/", -1);

    g_free(current_dir);
    g_free(full);

    int up = 0;
    char *tmppath = NULL;
    for (int i = g_strv_length(components)-1; i>=0; i--) {
      if (!strcmp(components[i], "..")) 
        up++;
      else if (! strcmp(components[i], "."))
        continue;
      else if (up > 0) 
        up--;
      else {
        newpath = g_build_filename("/", components[i], tmppath, NULL); 
        if (tmppath != NULL)
          g_free(tmppath);
        tmppath = newpath;
      }
    }
    orig_path = newpath;
    g_strfreev(components);
  }

  if (! (dir = g_dir_open(orig_path, 0, &error))) {
    fprintf(stderr, "dwb error: %s\n", error->message);
    g_clear_error(&error);
    return;
  }

  fullpath = g_build_filename(orig_path, "..", NULL);
  content = g_slist_prepend(content, fullpath);
  while ( (filename = g_dir_read_name(dir)) ) {
    fullpath = g_build_filename(orig_path, filename, NULL);
    content = g_slist_prepend(content, fullpath);
  }
  content = g_slist_sort(content, (GCompareFunc)util_compare_path);
  g_dir_close(dir);
  buffer = g_string_new(NULL);
  for (GSList *l = content; l; l=l->next) {
    fullpath = l->data;
    filename = g_strrstr(fullpath, "/") + 1;
    if (!dwb.state.hidden_files && filename[0] == '.' && filename[1] != '.')
      continue;
    struct stat st;
    char time[100];
    char size[50];
    char class[30] = { 0 };
    char *link = NULL;
    char *printname = NULL;
    if (lstat(fullpath, &st) != 0) {
      fprintf(stderr, "stat failed for %s\n", fullpath);
      continue;
    }
    strftime(time, 99, "%x %X", localtime(&st.st_mtime));
    if (st.st_size > BPGB) 
      snprintf(size, 49, "%.1fG", (double)st.st_size / BPGB);
    else if (st.st_size > BPMB) 
      snprintf(size, 49, "%.1fM", (double)st.st_size / BPMB);
    else if (st.st_size > BPKB) 
      snprintf(size, 49, "%.1fK", (double)st.st_size / BPKB);
    else 
      snprintf(size, 49, "%lu", st.st_size);

    char perm[11];
    int bits = 0;
    if (S_ISREG(st.st_mode))
      perm[bits++] = '-';
    else if (S_ISCHR(st.st_mode)) {
      perm[bits++] = 'c';
      strcpy(class, "dwb_local_character_device");
    }
    else if (S_ISDIR(st.st_mode)) {
      perm[bits++] = 'd';
      strcpy(class, "dwb_local_directory");
    }
    else if (S_ISBLK(st.st_mode)) {
      perm[bits++] = 'b';
      strcpy(class, "dwb_local_blockdevice");
    }
    else if (S_ISFIFO(st.st_mode)) {
      perm[bits++] = 'f';
      strcpy(class, "dwb_local_fifo");
    }
    else if (S_ISLNK(st.st_mode)) {
      perm[bits++] = 'l';
      strcpy(class, "dwb_local_link");
      link = g_file_read_link(fullpath, NULL);
      if (link != NULL) {
        char *tmp_path = fullpath;
        printname = g_strdup_printf("%s -> %s", filename, link);
        l->data = fullpath = g_build_filename(orig_path, link, NULL);
        g_free(tmp_path);
        g_free(link);
      }
    }
    /* user permissions */
    perm[bits++] = st.st_mode & S_IRUSR ? 'r' : '-';
    perm[bits++] = st.st_mode & S_IWUSR ? 'w' : '-';
    if (st.st_mode & S_ISUID) {
      perm[bits++] = st.st_mode & S_IXUSR ? 's' : 'S';
      strcpy(class, "dwb_local_setuid");
    }
    else
      perm[bits++] = st.st_mode & S_IXUSR ? 'x' : '-';
    if (st.st_mode & S_IXUSR && *class == 0) 
      strcpy(class, "dwb_local_executable");
    /*  group permissons */
    perm[bits++] = st.st_mode & S_IRGRP ? 'r' : '-';
    perm[bits++] = st.st_mode & S_IWGRP ? 'w' : '-';
    if (st.st_mode & S_ISGID) {
      perm[bits++] = st.st_mode & S_IXGRP ? 's' : 'S';
      strcpy(class, "dwb_local_setuid");
    }
    else
      perm[bits++] = st.st_mode & S_IXGRP ? 'x' : '-';
    /*  other */
    perm[bits++] = st.st_mode & S_IRGRP ? 'r' : '-';
    perm[bits++] = st.st_mode & S_IWGRP ? 'w' : '-';
    if (st.st_mode & S_ISVTX) {
      perm[bits++] = st.st_mode & S_IXGRP ? 't' : 'T';
      strcpy(class, "dwb_local_sticky");
    }
    else
      perm[bits++] = st.st_mode & S_IXGRP ? 'x' : '-';
    perm[bits] = '\0';
    struct passwd *pwd = getpwuid(st.st_uid);
    char *user = pwd && pwd->pw_name ? pwd->pw_name : "";
    struct group *grp = getgrgid(st.st_gid);
    char *group = pwd && grp->gr_name ? grp->gr_name : "";
    if (*class == 0)
      strcpy(class, "dwb_local_regular");

    g_string_append_printf(buffer, "<div class='dwb_local_table_row'><div>%s</div><div>%lu</div><div>%s</div><div>%s</div>", perm, st.st_nlink, user, group);
    g_string_append_printf(buffer, "<div>%s</div>", size);
    g_string_append_printf(buffer, "<div>%s</div>", time);
    g_string_append_printf(buffer, "<div class='%s'><a href='%s'>%s</a></div></div>", class, fullpath, printname == NULL ? filename: printname);
    FREE(printname);

  }
  tmp = orig_path+1;
  char *match;
  GString *path_buffer = g_string_new("/<a class='dwb_local_headline' href='");
  while ((match = strchr(tmp, '/'))) {
    g_string_append_len(path_buffer, orig_path, match-orig_path);
    g_string_append(path_buffer, "'>");
    g_string_append_len(path_buffer, tmp, match-tmp);
    tmp = match+1;
    g_string_append(path_buffer, "</a>/<a href='");
  }
  g_string_append_len(path_buffer, orig_path, match-orig_path);
  g_string_append(path_buffer, "'>");
  g_string_append_len(path_buffer, tmp, match-tmp);
  g_string_append(path_buffer, "</a>");

  char *local_file = util_get_data_file(LOCAL_FILE);
  char *filecontent = util_get_file_content(local_file);

  char *favicon = dwb_get_stock_item_base64_encoded("gtk-harddisk");
  /*  title, favicon, toppath, content */
  char *page = g_strdup_printf(filecontent, orig_path, favicon, path_buffer->str, buffer->str);

  g_free(favicon);
  g_free(local_file);
  g_free(filecontent);
  g_string_free(buffer, true);
  g_string_free(path_buffer, true);

  fullpath = g_strdup_printf("file:///%s", orig_path);
  /* add a history item */
  /* TODO sqlite */
  if (add_to_history) {
    WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(web);
    WebKitWebHistoryItem *item = webkit_web_history_item_new_with_data(fullpath, fullpath);
    webkit_web_back_forward_list_add_item(bf_list, item);
  }

  g_signal_connect(web, "notify::load-status", G_CALLBACK(local_load_directory_cb), NULL);
  webkit_web_view_load_string(web, page, NULL, NULL, fullpath);

  FREE(fullpath);
  g_free(page);
  FREE(newpath);

  for (GSList *l = content; l; l=l->next) {
    g_free(l->data);
  }
  g_slist_free(content);
}/*}}}*/

/* dwb_check_directory(const char *) {{{*/
gboolean
local_check_directory(WebKitWebView *wv, const char *path, gboolean add_to_history, GError **error) {
  char *unescaped = g_uri_unescape_string(path, NULL);
  char *local = unescaped;
  gboolean ret = true;
  if (g_str_has_prefix(local, "file://")) 
    local += *(local + 8) == '/' ? 8 : 7;
  if (!g_file_test(local, G_FILE_TEST_IS_DIR)) {
    ret = false;
    goto out;
  }
  if (access(local, R_OK)) 
    g_set_error(error, 0, 1, "Cannot access %s", local);
  int len = strlen (local);
  if (len>1 && local[len-1] == '/')
    local[len-1] = '\0';

  local_show_directory(wv, local, add_to_history);
out:
  g_free(unescaped);
  return ret;
}/*}}}*/