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

#include <fcntl.h>
#include <sys/file.h>
#include "dwb.h"
#include "util.h"
#include "soup.h"
static SoupCookieJar *_jar;
static guint _changed_id;
static SoupCookieJar *_tmpJar;

/* dwb_soup_allow_cookie(GList *, const char, CookieStorePolicy) {{{*/
void
dwb_soup_allow_cookie_tmp() {
  GSList *last_cookies = soup_cookie_jar_all_cookies(_tmpJar);
  const char *domain;
  SoupCookie *c;
  for (GSList *l = last_cookies; l; l=l->next) {
    c = l->data;
    domain = soup_cookie_get_domain(c);
    if ( g_list_find_custom(dwb.fc.cookies_session_allow, domain, (GCompareFunc)g_strcmp0)  == NULL ) {
      dwb.fc.cookies_session_allow = g_list_append(dwb.fc.cookies_session_allow, g_strdup(domain));
      soup_cookie_jar_add_cookie(_jar, soup_cookie_copy(c));
    }
  }
  soup_cookies_free(last_cookies);
  dwb_reload();
}
DwbStatus
dwb_soup_allow_cookie(GList **whitelist, const char *filename, CookieStorePolicy policy) {
  DwbStatus ret = STATUS_ERROR;

  GSList *last_cookies = soup_cookie_jar_all_cookies(_tmpJar);
  if (last_cookies == NULL)
    return ret;

  int length;
  const char *domain;
  SoupCookie *c;
  GString *buffer = g_string_new(NULL);
  GSList *asked = NULL, *allowed = NULL;

  for (GSList *l = last_cookies; l; l=l->next) {
    c = l->data;
    domain = soup_cookie_get_domain(c);
    if ( *whitelist == NULL || g_list_find_custom(*whitelist, domain, (GCompareFunc) g_strcmp0) == NULL ) {
      /* only ask once, if it was already prompted for this domain and allowed it will be handled
       * in the else clause */
      if (g_slist_find_custom(asked, domain, (GCompareFunc)g_strcmp0))
        continue;
      if (dwb_confirm(dwb.state.fview, "Allow %s cookies for domain %s [y/n]", policy == COOKIE_STORE_PERSISTENT ? "persistent" : "session", domain)) {
        *whitelist = g_list_append(*whitelist, g_strdup(domain));
        util_file_add(filename, domain, true, -1);
        g_string_append_printf(buffer, "%s ", domain);
        allowed = g_slist_prepend(allowed, soup_cookie_copy(c));
      }
      asked = g_slist_prepend(asked, (char*)domain);
      CLEAR_COMMAND_TEXT();
    }
    else {
      allowed = g_slist_prepend(allowed, soup_cookie_copy(c));
    }
  }
  length = g_slist_length(allowed);
  if (length > 0) {
    dwb_set_normal_message(dwb.state.fview, true, "Allowed domain%s: %s", length == 1 ? "" : "s", buffer->str);
    ret = STATUS_OK;
  }
  if (policy == COOKIE_STORE_PERSISTENT) 
    dwb_soup_save_cookies(allowed);
  /* Save all cookies to the jar */
  for (GSList *l = allowed; l; l=l->next)
    soup_cookie_jar_add_cookie(_jar, l->data);
  /* soup_cookie_jar_add_cookie steals the cookie, it must no be freed */
  g_slist_free(allowed);

  dwb_reload();
  g_string_free(buffer, true);
  g_slist_free(asked);
  soup_cookies_free(last_cookies);
  return ret;
}/*}}}*/

/* dwb_soup_clean() {{{*/
void 
dwb_soup_clean() {
  GSList *all_cookies = soup_cookie_jar_all_cookies(_tmpJar);
  for (GSList *l = all_cookies; l; l=l->next) {
    soup_cookie_jar_delete_cookie(_tmpJar, l->data);
  }
  soup_cookies_free(all_cookies);
}/*}}}*/

/* dwb_soup_get_host_from_request(WebKitNetworkRequest ) {{{*/
const char *
dwb_soup_get_host_from_request(WebKitNetworkRequest *request) {
  const char *host = NULL;
  SoupMessage *msg = webkit_network_request_get_message(request);
  if (msg != NULL) {
    SoupURI *suri = soup_message_get_uri(msg);
    if (suri != NULL)
      host = soup_uri_get_host(suri);
  }
  return host;
}/*}}}*/

/* dwb_soup_save_cookies(cookies) {{{*/
void 
dwb_soup_save_cookies(GSList *cookies) {
  int fd = open(dwb.files.cookies, 0);
  flock(fd, LOCK_EX);
  SoupCookieJar *jar = soup_cookie_jar_text_new(dwb.files.cookies, false);
  for (GSList *l=cookies; l; l=l->next) {
    soup_cookie_jar_add_cookie(jar, soup_cookie_copy(l->data));
  }
  g_object_unref(jar);
  flock(fd, LOCK_UN);
  close(fd);
}/*}}}*/

/* dwb_test_cookie_allowed(const char *)     return:  gboolean{{{*/
static gboolean 
dwb_soup_test_cookie_allowed(GList *list, SoupCookie *cookie) {
  g_return_val_if_fail(cookie != NULL, false);
  g_return_val_if_fail(cookie->domain != NULL, false);
  for (GList *l = list; l; l=l->next) {
    if (l->data && soup_cookie_domain_matches(cookie, l->data)) {
      return true;
    }
  }
  return false;
}/*}}}*/

/* dwb_soup_set_cookie_accept_policy {{{*/
DwbStatus 
dwb_soup_set_cookie_accept_policy(const char *policy) {
  SoupCookieJarAcceptPolicy apo = 37;
  DwbStatus ret = STATUS_OK;
  if (! g_ascii_strcasecmp(policy, "nothirdparty"))
    apo = SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
  else if (! g_ascii_strcasecmp(policy, "never"))
    apo = SOUP_COOKIE_JAR_ACCEPT_NEVER;
  else if (! g_ascii_strcasecmp(policy, "always"))
    apo = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;

  if (apo == 37) {
    dwb_set_error_message(dwb.state.fview, "Invalid value for cookies-accept-policy: %d, using 0", policy);
    apo = 0;
    ret = STATUS_ERROR;
  }
  soup_cookie_jar_set_accept_policy(_jar, apo);
  return ret;
}/*}}}*/

/* dwb_soup_get_cookie_store_policy(const char *policy) {{{*/
CookieStorePolicy 
dwb_soup_get_cookie_store_policy(const char *policy) {
  if (! g_ascii_strcasecmp(policy, "persistent")) 
    return COOKIE_STORE_PERSISTENT;
  else if (! g_ascii_strcasecmp(policy, "never")) 
    return COOKIE_STORE_NEVER;
  else
    return COOKIE_STORE_SESSION;
}/*}}}*/

/*dwb_soup_cookie_changed_cb {{{*/
static void 
dwb_soup_cookie_changed_cb(SoupCookieJar *jar, SoupCookie *old, SoupCookie *new, gpointer *p) {
  int fd = open(dwb.files.cookies, 0);
  flock(fd, LOCK_EX);
  SoupCookieJar *j = soup_cookie_jar_text_new(dwb.files.cookies, false);
  if (old) {
    soup_cookie_jar_delete_cookie(j, old);
  }
  if (new) {
    if (dwb.state.cookie_store_policy == COOKIE_STORE_PERSISTENT || dwb_soup_test_cookie_allowed(dwb.fc.cookies_allow, new)) {
      soup_cookie_jar_add_cookie(j, soup_cookie_copy(new));
    }
    else { 
      soup_cookie_jar_add_cookie(_tmpJar, soup_cookie_copy(new));
      if (dwb.state.cookie_store_policy == COOKIE_STORE_NEVER && !dwb_soup_test_cookie_allowed(dwb.fc.cookies_session_allow, new) ) {
        g_signal_handler_block(jar, _changed_id);
        soup_cookie_jar_delete_cookie(jar, new);
        g_signal_handler_unblock(jar, _changed_id);
      }
    }
  }
  g_object_unref(j);
  flock(fd, LOCK_UN);
  close(fd);
}/*}}}*/

/* dwb_soup_init_cookies {{{*/
void
dwb_soup_init_cookies(SoupSession *s) {
  _jar = soup_cookie_jar_new(); 
  _tmpJar = soup_cookie_jar_new();
  dwb_soup_set_cookie_accept_policy(GET_CHAR("cookies-accept-policy"));
  SoupCookieJar *old_cookies = soup_cookie_jar_text_new(dwb.files.cookies, true);
  GSList *l = soup_cookie_jar_all_cookies(old_cookies);
  for (; l; l=l->next ) {
    soup_cookie_jar_add_cookie(_jar, soup_cookie_copy(l->data)); 
  }
  soup_cookies_free(l);
  g_object_unref(old_cookies);
  soup_session_add_feature(s, SOUP_SESSION_FEATURE(_jar));
  _changed_id = g_signal_connect(_jar, "changed", G_CALLBACK(dwb_soup_cookie_changed_cb), NULL);
}/*}}}*/

/* dwb_init_proxy{{{*/
void 
dwb_soup_init_proxy() {
  const char *proxy;
  gboolean use_proxy = GET_BOOL("proxy");
  if ( !(proxy =  g_getenv("http_proxy")) && !(proxy =  GET_CHAR("proxy-url")) )
    return;

  if (dwb.misc.proxyuri)
    g_free(dwb.misc.proxyuri);

  dwb.misc.proxyuri = g_strrstr(proxy, "://") ? g_strdup(proxy) : g_strdup_printf("http://%s", proxy);
  SoupURI *uri = soup_uri_new(dwb.misc.proxyuri);
  g_object_set(dwb.misc.soupsession, "proxy-uri", use_proxy ? uri : NULL, NULL); 
  soup_uri_free(uri);
}/*}}}*/

/* dwb_soup_init_session_features() {{{*/
void 
dwb_soup_init_session_features() {
  char *cert = GET_CHAR("ssl-ca-cert");
  if (cert != NULL && g_file_test(cert, G_FILE_TEST_EXISTS)) {
    g_object_set(dwb.misc.soupsession, 
        SOUP_SESSION_SSL_CA_FILE, cert, NULL);
  }
  g_object_set(dwb.misc.soupsession, SOUP_SESSION_SSL_STRICT, GET_BOOL("ssl-strict"), NULL);
  //soup_session_add_feature(dwb.misc.soupsession, SOUP_SESSION_FEATURE(soup_content_sniffer_new()));
  //soup_session_add_feature_by_type(webkit_get_default_session(), SOUP_TYPE_CONTENT_SNIFFER);
}/*}}}*/

/* dwb_soup_end(() {{{*/
void
dwb_soup_end() {
  g_object_unref(_tmpJar);
  g_object_unref(_jar);
  FREE(dwb.misc.proxyuri);
}/*}}}*/

/* dwb_soup_init() {{{*/
void 
dwb_soup_init() {
  dwb.misc.soupsession = webkit_get_default_session();
  dwb_soup_init_proxy();
  dwb_soup_init_cookies(dwb.misc.soupsession);
  dwb_soup_init_session_features();
#if 0
  SoupLogger *sl = soup_logger_new(SOUP_LOGGER_LOG_HEADERS, -1);
  soup_session_add_feature(dwb.misc.soupsession, sl);
#endif
}/*}}}*/
