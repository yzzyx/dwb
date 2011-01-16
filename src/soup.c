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
dwb_soup_save_cookie(SoupCookie *cookie) {
  SoupCookieJar *jar = soup_cookie_jar_text_new(dwb.files.cookies, false);
  soup_cookie_jar_add_cookie(jar, cookie);
  g_object_unref(jar);
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

/* dwb_soup_got_headers_cb(SoupMessage *) {{{*/
static void 
dwb_soup_got_headers_cb(SoupMessage *message) {
  int fd = open(dwb.files.cookies, O_RDONLY);
  flock(fd, LOCK_EX);

  SoupCookieJar *jar = soup_cookie_jar_text_new(dwb.files.cookies, false);

  for (GSList *l = soup_cookies_from_response(message); l; l=l->next) {
    SoupCookie *c = l->data;
    if (dwb.state.cookies_allowed || dwb_soup_test_cookie_allowed(c)) {
      dwb_soup_save_cookie(c);
    }
    else {
      dwb.state.last_cookies = g_slist_append(dwb.state.last_cookies, soup_cookie_copy(c));
    }
  }
  g_object_unref(jar);
  flock(fd, LOCK_EX);
  close(fd);
}/*}}}*/

/* dwb_soup_cookies_request_started_after_cb (SoupSession *, SoupMessage *, SoupSocket *) {{{*/
static void 
dwb_soup_cookies_request_started_after_cb(SoupSession *session, SoupMessage *message, SoupSocket *socket) {
  SoupCookieJar *jar = soup_cookie_jar_text_new(dwb.files.cookies, true);
  SoupURI *uri = soup_message_get_uri(message);
  char *c = soup_cookie_jar_get_cookies(jar, uri, true);
  if (c) {
    soup_message_headers_append(message->request_headers, "Cookie", c);
  }
  g_object_unref(jar);
}/*}}}*/

/* dwb_soup_cookies_request_started_cb (SoupSession *, SoupMessage *, SoupSocket *) {{{*/
static void 
dwb_soup_cookies_request_started_cb(SoupSession *session, SoupMessage *message, SoupSocket *socket) {
  g_signal_connect_after(message, "got-headers", G_CALLBACK(dwb_soup_got_headers_cb), NULL);
}/*}}}*/

/* dwb_cookie_changed_cb {{{*/
void
dwb_soup_init_cookies(SoupSession *s) {
  g_signal_connect_after(s, "request-started", G_CALLBACK(dwb_soup_cookies_request_started_after_cb), NULL);
  g_signal_connect(s, "request-started", G_CALLBACK(dwb_soup_cookies_request_started_cb), NULL);
}/*}}}*/

/* dwb_init_proxy{{{*/
void 
dwb_soup_init_proxy(SoupSession *s) {
  const char *proxy;
  static char *newproxy;
  gboolean use_proxy = GET_BOOL("proxy");
  if ( !(proxy =  g_getenv("http_proxy")) && !(proxy =  GET_CHAR("proxy-url")) )
    return;

  if ( (use_proxy && dwb_util_test_connect(proxy)) || !use_proxy ) {
    newproxy = g_strrstr(proxy, "http://") ? g_strdup(proxy) : g_strdup_printf("http://%s", proxy);
    dwb.misc.proxyuri = soup_uri_new(newproxy);
    g_object_set(G_OBJECT(s), "proxy-uri", use_proxy ? dwb.misc.proxyuri : NULL, NULL); 
    FREE(newproxy);
  }
}/*}}}*/
