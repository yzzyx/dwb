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

#include "soup.h"

/* dwb_soup_save_cookies(cookies) {{{*/
void 
dwb_soup_save_cookies(GSList *cookies) {
  int fd = open(dwb.files.cookies, 0);
  flock(fd, LOCK_EX);
  SoupCookieJar *jar = soup_cookie_jar_text_new(dwb.files.cookies, false);
  for (GSList *l=cookies; l; l=l->next) {
    soup_cookie_jar_add_cookie(jar, l->data);
  }
  g_object_unref(jar);
  flock(fd, LOCK_UN);
  close(fd);
}/*}}}*/

/* dwb_test_cookie_allowed(const char *)     return:  gboolean{{{*/
static gboolean 
dwb_soup_test_cookie_allowed(SoupCookie *cookie) {
  for (GList *l = dwb.fc.cookies_allow; l; l=l->next) {
    if (l->data && soup_cookie_domain_matches(cookie, l->data)) {
      return true;
    }
  }
  return false;
}/*}}}*/

/* dwb_soup_cookie_compare(SoupCookie *, SoupCookie *) {{{*/
int 
dwb_soup_cookie_compare(SoupCookie *a, SoupCookie *b) {
  return ! soup_cookie_equal(a, b);
}/*}}}*/

static void 
dwb_soup_cookie_changed_cb(SoupCookieJar *jar, SoupCookie *old, SoupCookie *new, gpointer *p) {
  int fd = open(dwb.files.cookies, 0);
  flock(fd, LOCK_EX);
  SoupCookieJar *j = soup_cookie_jar_text_new(dwb.files.cookies, false);
  if (old) {
    soup_cookie_jar_delete_cookie(j, old);
  }
  if (new) {
    if (dwb.state.cookies_allowed || dwb_soup_test_cookie_allowed(new)) {
      soup_cookie_jar_add_cookie(j, soup_cookie_copy(new));
    }
#if 0
    else if (! g_slist_find_custom(dwb.state.last_cookies, new, (GCompareFunc)dwb_soup_cookie_compare ) && ! dwb_block_ad(dwb.state.fview, soup_cookie_get_domain(new))){
#endif
    else if (! g_slist_find_custom(dwb.state.last_cookies, new, (GCompareFunc)dwb_soup_cookie_compare )) {
      dwb.state.last_cookies = g_slist_append(dwb.state.last_cookies, soup_cookie_copy(new));
    }
  }
  g_object_unref(j);
  flock(fd, LOCK_UN);
  close(fd);
}

/* dwb_soup_init_cookies {{{*/
void
dwb_soup_init_cookies(SoupSession *s) {
  SoupCookieJar *jar = soup_cookie_jar_new(); 
  SoupCookieJar *old_cookies = soup_cookie_jar_text_new(dwb.files.cookies, true);
  GSList *l = soup_cookie_jar_all_cookies(old_cookies);
  for (; l; l=l->next ) {
    soup_cookie_jar_add_cookie(jar, soup_cookie_copy(l->data)); 
  }
  soup_cookies_free(l);
  g_object_unref(old_cookies);
  soup_session_add_feature(s, SOUP_SESSION_FEATURE(jar));
  g_signal_connect(jar, "changed", G_CALLBACK(dwb_soup_cookie_changed_cb), NULL);
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
}
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
}
