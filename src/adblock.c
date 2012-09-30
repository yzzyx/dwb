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
#include <JavaScriptCore/JavaScript.h>
#include "dwb.h"
#include "util.h"
#include "domain.h"
#include "adblock.h"
#include "js.h"



/* DECLARATIONS {{{*/
/* Type definitions {{{*/
typedef enum _AdblockOption {
  AO_BEGIN              = 1<<2,
  AO_BEGIN_DOMAIN       = 1<<3,
  AO_END                = 1<<4,
  AO_MATCH_CASE         = 1<<7,
  AO_THIRDPARTY         = 1<<8,
  AO_NOTHIRDPARTY       = 1<<9,
} AdblockOption;
/*  Attributes */
typedef enum _AdblockAttribute {
  AA_SCRIPT             = 1<<0,
  AA_IMAGE              = 1<<1,
  AA_STYLESHEET         = 1<<2,
  AA_OBJECT             = 1<<3,
  AA_XBL                = 1<<4,
  AA_PING               = 1<<5,
  AA_XMLHTTPREQUEST     = 1<<6,
  AA_OBJECT_SUBREQUEST  = 1<<7,
  AA_DTD                = 1<<8,
  AA_SUBDOCUMENT        = 1<<9,
  AA_DOCUMENT           = 1<<10,
  AA_ELEMHIDE           = 1<<11,
  AA_OTHER              = 1<<12,
  /* inverse */
} AdblockAttribute;
#define AA_CLEAR_FRAME(X) (X & ~(AA_SUBDOCUMENT|AA_DOCUMENT))

#define AB_INVERSE 15
/* clear upper bits, last attribute may be 1<<14 */
#define AB_CLEAR_UPPER 0x7fff
#define AB_CLEAR_LOWER 0x3fff8000

typedef struct _AdblockRule {
  GRegex *pattern;
  AdblockOption options;
  AdblockAttribute attributes;
  char **domains;
} AdblockRule;

typedef struct _AdblockElementHider {
  char *selector;
  char **domains;
  gboolean exception;
} AdblockElementHider;
/*}}}*/

/* Static variables {{{*/
static GPtrArray *m_simple_rules;
static GPtrArray *m_simple_exceptions;
static GPtrArray *m_rules;
static GPtrArray *m_exceptions;
static GHashTable *m_hider_rules;
gboolean m_has_hider_rules;
/*  only used to freeing elementhider */
static GSList *m_hider_list;
static GString *m_css_rules;
static GString *m_css_exceptions;
static gboolean m_init = false;
/*}}}*//*}}}*/


/* NEW AND FREE {{{*/
/* adblock_rule_new {{{*/
static AdblockRule *
adblock_rule_new() {
  AdblockRule *rule = dwb_malloc(sizeof(AdblockRule));
  rule->pattern = NULL;
  rule->options = 0;
  rule->attributes = 0;
  rule->domains = NULL;
  return rule;
}/*}}}*/

/* adblock_rule_free {{{*/
static void 
adblock_rule_free(AdblockRule *rule) {
  if (rule->pattern != NULL) {
    g_regex_unref(rule->pattern);
  }
  if (rule->domains != NULL) {
    g_strfreev(rule->domains);
  }
  g_free(rule);
}/*}}}*/

/* adblock_element_hider_new {{{*/
static AdblockElementHider *
adblock_element_hider_new() {
  AdblockElementHider *hider = dwb_malloc(sizeof(AdblockElementHider));
  hider->selector = NULL;
  hider->domains = NULL;
  return hider;
}/*}}}*/

/* adblock_element_hider_free {{{*/
static void
adblock_element_hider_free(AdblockElementHider *hider) {
  if (hider) {
    if (hider->selector) {
      g_free(hider->selector);
    }
    if (hider->domains) {
      g_strfreev(hider->domains);
    }
    g_free(hider);
  }
}/*}}}*//*}}}*/


/* MATCH {{{*/
/* inline adblock_do_match(AdblockRule *, const char *) {{{*/
static inline gboolean
adblock_do_match(AdblockRule *rule, const char *uri) {
  if (g_regex_match(rule->pattern, uri, 0, NULL)) {
    PRINT_DEBUG("blocked %s %s\n", uri, g_regex_get_pattern(rule->pattern));
    return true;
  }
  return false;
}/*}}}*/

/* adblock_match(GPtrArray *, SoupURI *, const char *base_domain, * AdblockAttribute, gboolean thirdparty)  {{{
 * Params: 
 * array      - the filter array
 * uri        - the uri to check
 * uri_host   - the hostname of the request
 * uri_base   - the domainname of the request
 * host       - the hostname of the page
 * domain     - the domainname of the page
 * thirdparty - thirdparty request ? 
 * */
gboolean                
adblock_match(GPtrArray *array, const char *uri, const char *uri_host, const char *uri_base, const char *host, const char *domain, AdblockAttribute attributes, gboolean thirdparty) {
  if (array->len == 0)
    return false;
  const char *base_start = strstr(uri, uri_base);
  const char *uri_start = strstr(uri, uri_host);
  const char *suburis[SUBDOMAIN_MAX];
  int uc = 0;
  const char *cur = uri_start;
  const char *nextdot;
  AdblockRule *rule;
  /* Get all suburis */
  suburis[uc++] = cur;
  while (cur != base_start) {
    nextdot = strchr(cur, '.');
    cur = nextdot + 1;
    suburis[uc++] = cur;
    if (uc == SUBDOMAIN_MAX-1)
      break;
  }
  suburis[uc++] = NULL;

  for (guint i=0; i<array->len; i++) {
    rule = g_ptr_array_index(array, i);
    if ( (attributes & AA_DOCUMENT && !(rule->attributes & AA_DOCUMENT)) || (attributes & AA_SUBDOCUMENT && !(rule->attributes & AA_SUBDOCUMENT)) )
      continue;
    /* If exception attributes exists, check if exception is matched */
    if (AA_CLEAR_FRAME(rule->attributes) & AB_CLEAR_LOWER && (AA_CLEAR_FRAME(rule->attributes) == (AA_CLEAR_FRAME(attributes)<<AB_INVERSE))) 
      continue;
    /* If attribute restriction exists, check if attribute is matched */
    if (AA_CLEAR_FRAME(rule->attributes) & AB_CLEAR_UPPER && (AA_CLEAR_FRAME(rule->attributes) != AA_CLEAR_FRAME(attributes))) {
      continue;
    }
    if (rule->domains && !domain_match(rule->domains, host, domain)) {
      continue;
    }
    if    ( (rule->options & AO_THIRDPARTY && !thirdparty) 
        ||  (rule->options & AO_NOTHIRDPARTY && thirdparty) )
      continue;
    if (rule->options & AO_BEGIN_DOMAIN) {
      for (int i=0; suburis[i]; i++) {
        if ( adblock_do_match(rule, suburis[i]) ) 
          return true;
      }
    }
    else if (adblock_do_match(rule, uri)) {
      return true;
    }
  }
  return false;
}/*}}}*/

/* adblock_prepare_match (const char *uri, const char *baseURI, AdblockAttribute attributes {{{ */
static gboolean
adblock_prepare_match(const char *uri, const char *baseURI, AdblockAttribute attributes) {
  char *realuri = NULL;
  SoupURI *suri = NULL, *sbaseuri = NULL;
  gboolean ret = false;

  if (! g_regex_match_simple("^https?://", uri, 0, 0)) {
    gboolean last_slash = g_str_has_suffix(baseURI, "/");
    if (*uri == '/' && last_slash) 
      realuri = g_strconcat(baseURI, uri+1, NULL);
    else if (*uri != '/' && !last_slash)
      realuri = g_strconcat(baseURI, "/", uri, NULL);
    else 
      realuri = g_strconcat(baseURI, uri, NULL);
  }
  else 
    realuri = g_strdup(uri);

  /* FIXME: soup_uri_get_host is just used to get parse the uri */
  suri = soup_uri_new(realuri);
  if (suri == NULL) 
    goto error_out;
  const char *host = soup_uri_get_host(suri);
  if (host == NULL)
    goto error_out;

  sbaseuri = soup_uri_new(baseURI);
  if (sbaseuri == NULL) 
    goto error_out;

  const char *basehost = soup_uri_get_host(sbaseuri);
  if (basehost == NULL)
    goto error_out;
  
  const char *domain = domain_get_base_for_host(host);
  const char *basedomain = domain_get_base_for_host(basehost);
  gboolean thirdparty = g_strcmp0(domain, basedomain);

  if (!adblock_match(m_exceptions, realuri, host, domain, basehost, basedomain, attributes, thirdparty)) {
    if (adblock_match(m_rules, realuri, host, domain, basehost, basedomain, attributes, thirdparty)) {
      ret = true;
    }
  }
error_out:
  if (realuri != NULL) g_free(realuri);
  if (sbaseuri != NULL) soup_uri_free(sbaseuri);
  if (suri != NULL) soup_uri_free(suri);
  return ret;
}/*}}}*/

/* adblock_apply_element_hider(WebKitWebFrame *frame, GList *gl) {{{*/
void 
adblock_apply_element_hider(WebKitWebFrame *frame, GList *gl) {
  GSList *list;
  WebKitWebView *wv = WEBVIEW(gl);

  WebKitWebDataSource *datasource = webkit_web_frame_get_data_source(frame);
  WebKitNetworkRequest *request = webkit_web_data_source_get_request(datasource);

  SoupMessage *msg = webkit_network_request_get_message(request);
  if (msg == NULL)
    return;

  SoupURI *suri = soup_message_get_first_party(msg);
  g_return_if_fail(suri != NULL);

  const char *host = soup_uri_get_host(suri);
  const char *base_domain = domain_get_base_for_host(host);
  g_return_if_fail(host != NULL);
  g_return_if_fail(base_domain != NULL);

  AdblockElementHider *hider;
  GString *css_rule = g_string_new(NULL);

  /* get all subdomains */
  const char *subdomains[SUBDOMAIN_MAX];
  char *nextdot = NULL;
  int uc = 0;
  const char *tmphost = host;
  subdomains[uc++] = tmphost;
  while (tmphost != base_domain) {
    nextdot = strchr(tmphost, '.');
    tmphost = nextdot + 1;
    subdomains[uc++] = tmphost;
    if (uc == SUBDOMAIN_MAX-1)
      break;
  }
  subdomains[uc++] = NULL;

  gboolean has_exception = false;
  for (int i=0; subdomains[i]; i++) {
    list = g_hash_table_lookup(m_hider_rules, subdomains[i]);
    if (list) {
      for (GSList *l = list; l; l=l->next) {
        hider = l->data;
        if (hider->exception) 
          has_exception = true;
        else if  (domain_match(hider->domains, host, base_domain)) {
          g_string_append(css_rule, hider->selector);
          g_string_append_c(css_rule, ',');
        }
      }
      break;
    }
  }
  /* Adding replaced exceptions */
  if (! has_exception) {
    g_string_append(css_rule, m_css_exceptions->str);
  }
  if (m_css_rules != NULL && m_css_rules->len > 1) {
    g_string_append(css_rule, m_css_rules->str);
  }
  if (css_rule != NULL && css_rule->len > 0) {
    if (css_rule->str[css_rule->len-1] == ',') 
      g_string_erase(css_rule, css_rule->len-1, 1);
    g_string_append(css_rule, "{display:none!important;}");
    if (frame == webkit_web_view_get_main_frame(wv)) {
      WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
      if (VIEW(gl)->status->style == NULL) {
        WebKitDOMDocument *doc = webkit_web_view_get_dom_document(WEBVIEW(gl));
        VIEW(gl)->status->style = webkit_dom_document_create_element(doc, "style", NULL);
      }
      WebKitDOMHTMLHeadElement *head = webkit_dom_document_get_head(doc);
      if (G_IS_OBJECT(head)) {
        webkit_dom_html_element_set_inner_html(WEBKIT_DOM_HTML_ELEMENT(VIEW(gl)->status->style), css_rule->str, NULL);
        webkit_dom_node_append_child(WEBKIT_DOM_NODE(head), WEBKIT_DOM_NODE(VIEW(gl)->status->style), NULL);
        g_object_unref(head);
      }
    }
    else {
      const char *css = css_rule->str;
      /* Escape css_rule, at least ' and \ must be escaped, maybe some more? */
      for (int pos = 0; *css; pos++, css++) {
        if (*css == '\'' || *css == '\\')  {
          g_string_insert_c(css_rule, pos, '\\');
          pos++;
          css++;
        }
      }
      char *script = g_strdup_printf(
          "(function() {var st=document.createElement('style');\
          document.head.appendChild(st);\
          document.styleSheets[document.styleSheets.length-1].insertRule('%s', 0);})();", 
          css_rule->str);
      dwb_execute_script(frame, script, false);
      g_free(script);
    }
    g_string_free(css_rule, true);
  }
}/*}}}*/
/*}}}*/


/* LOAD_CALLBACKS {{{*/

/* adblock_before_load_cb  (domcallback) {{{*/
static gboolean
adblock_before_load_cb(WebKitDOMDOMWindow *win, WebKitDOMEvent *event, GList *gl) {
  WebKitDOMElement *src = (void*)webkit_dom_event_get_src_element(event);
  char *tagname = webkit_dom_element_get_tag_name(src);
  const char *url = NULL;

  gboolean ret = false;

  WebKitDOMDocument *doc = webkit_dom_dom_window_get_document(win);
  char *baseURI = webkit_dom_document_get_document_uri(doc);

  WebKitDOMDocument *main_doc = webkit_web_view_get_dom_document(WEBVIEW(gl));
  WebKitDOMDOMWindow *main_win = webkit_dom_document_get_default_view(main_doc);
  AdblockAttribute attributes = win == main_win ? AA_DOCUMENT : AA_SUBDOCUMENT;

  if (webkit_dom_element_has_attribute(src, "src")) 
    url = webkit_dom_element_get_attribute(src, "src");
  else if (webkit_dom_element_has_attribute(src, "href")) 
    url = webkit_dom_element_get_attribute(src, "href");
  else if (webkit_dom_element_has_attribute(src, "data")) 
    url = webkit_dom_element_get_attribute(src, "data");
  if (url == NULL) 
    goto error_out;

  if (!g_strcmp0(tagname, "IMG")) {
    attributes |= AA_IMAGE;
  }
  else if (!g_strcmp0(tagname, "SCRIPT"))
    attributes |= AA_SCRIPT;
  else if (!g_strcmp0(tagname, "LINK")  ) {
    char *rel  = webkit_dom_html_link_element_get_rel((void*)src);
    char *type  = webkit_dom_element_get_attribute(src, "type");
    if (!g_strcmp0(rel, "stylesheet") || !g_strcmp0(type, "text/css")) {
      attributes |= AA_STYLESHEET;
    }
    g_free(rel);
    g_free(type);
  }
  else if (!g_strcmp0(tagname, "OBJECT") || ! g_strcmp0(tagname, "EMBED")) {
    attributes |= AA_OBJECT;
  }
  if (adblock_prepare_match(url, baseURI, attributes)) {
    webkit_dom_event_prevent_default(event);
  }
  ret = true;
error_out:
  g_object_unref(src);
  g_free(tagname);
  g_free(baseURI);
  return ret;
}/*}}}*/

static void 
adblock_frame_load_status_cb(WebKitWebFrame *frame, GParamSpec *p, GList *gl) {
  WebKitLoadStatus status = webkit_web_frame_get_load_status(frame);
  if (status == WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT) {
    adblock_apply_element_hider(frame, gl);
  }
  else if (status == WEBKIT_LOAD_COMMITTED) {
    dwb_dom_add_frame_listener(frame, "beforeload", G_CALLBACK(adblock_before_load_cb), true, gl);
  }
}

/* adblock_frame_created_cb {{{*/
void 
adblock_frame_created_cb(WebKitWebView *wv, WebKitWebFrame *frame, GList *gl) {
  g_signal_connect(frame, "notify::load-status", G_CALLBACK(adblock_frame_load_status_cb), gl);
}/*}}}*/
/* adblock_resource_request_cb {{{*/
static void 
adblock_resource_request_cb(WebKitWebView *wv, WebKitWebFrame *frame,
    WebKitWebResource *resource, WebKitNetworkRequest *request,
    WebKitNetworkResponse *response, GList *gl) {
  if (request == NULL) 
    return;
  AdblockAttribute attribute = webkit_web_view_get_main_frame(wv) == frame ? AA_DOCUMENT : AA_SUBDOCUMENT;

  const char *uri = webkit_network_request_get_uri(request);
  if (uri == NULL)
    return;
  SoupMessage *msg = webkit_network_request_get_message(request);
  if (msg == NULL)
    return;
  SoupURI *suri = soup_message_get_uri(msg);
  const char *host = soup_uri_get_host(suri);
  if (host == NULL)
    return;
  const char *domain = domain_get_base_for_host(host);
  if (domain == NULL)
    return;

  SoupURI *sfirst_party = soup_message_get_first_party(msg);
  if (sfirst_party == NULL)
    return;
  const char *firsthost = soup_uri_get_host(sfirst_party);
  if (firsthost == NULL)
    return;
  const char *firstdomain = domain_get_base_for_host(firsthost);
  if (firstdomain == NULL)
    return;
  gboolean thirdparty = g_strcmp0(domain, firstdomain);

  if (!adblock_match(m_simple_exceptions, uri, host, domain, firsthost, firstdomain, attribute, thirdparty)) {
    if (adblock_match(m_simple_rules, uri, host, domain, firsthost, firstdomain, attribute, thirdparty)) {
      webkit_network_request_set_uri(request, "about:blank");
    }
  }
}/*}}}*/
 
/* adblock_load_status_cb(WebKitWebView *, GParamSpec *, GList *) {{{*/
static void
adblock_load_status_cb(WebKitWebView *wv, GParamSpec *p, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(wv);
  if (status == WEBKIT_LOAD_COMMITTED) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "beforeload", G_CALLBACK(adblock_before_load_cb), true, gl);
  }
  else if (status == WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT) {
    WebKitWebFrame *frame = webkit_web_view_get_main_frame(wv);
    adblock_apply_element_hider(frame, gl);
  }
}/*}}}*//*}}}*/


/* START AND END {{{*/

gboolean
adblock_running() {
  return m_init && GET_BOOL("adblocker");
}

/* adblock_disconnect(GList *) {{{*/
void 
adblock_disconnect(GList *gl) {
  View *v = VIEW(gl);
  if (v->status->signals[SIG_AD_LOAD_STATUS] > 0)  {
    g_signal_handler_disconnect(WEBVIEW(gl), VIEW(gl)->status->signals[SIG_AD_LOAD_STATUS]);
    v->status->signals[SIG_AD_LOAD_STATUS] = 0;
  }
  if (v->status->signals[SIG_AD_FRAME_CREATED] > 0) {
    g_signal_handler_disconnect(WEBVIEW(gl), (VIEW(gl)->status->signals[SIG_AD_FRAME_CREATED]));
    v->status->signals[SIG_AD_FRAME_CREATED] = 0;
  }
  if (v->status->signals[SIG_AD_RESOURCE_REQUEST] > 0) {
    g_signal_handler_disconnect(WEBVIEW(gl), (VIEW(gl)->status->signals[SIG_AD_RESOURCE_REQUEST]));
    v->status->signals[SIG_AD_RESOURCE_REQUEST] = 0;
  }
  if (VIEW(gl)->status->style != NULL) {
    g_object_unref(VIEW(gl)->status->style);
    VIEW(gl)->status->style = NULL;
  }
}/*}}}*/

/* adblock_connect() {{{*/
void 
adblock_connect(GList *gl) {
  if (!m_init && !adblock_init()) 
      return;
  if (m_rules->len > 0 || m_css_rules->len > 0 || m_has_hider_rules) {
    VIEW(gl)->status->signals[SIG_AD_LOAD_STATUS] = g_signal_connect(WEBVIEW(gl), "notify::load-status", G_CALLBACK(adblock_load_status_cb), gl);
    VIEW(gl)->status->signals[SIG_AD_FRAME_CREATED] = g_signal_connect(WEBVIEW(gl), "frame-created", G_CALLBACK(adblock_frame_created_cb), gl);
  }
  if (m_simple_rules->len > 0) {
    VIEW(gl)->status->signals[SIG_AD_RESOURCE_REQUEST] = g_signal_connect(WEBVIEW(gl), "resource-request-starting", G_CALLBACK(adblock_resource_request_cb), gl);
  }
}/*}}}*/

/* adblock_warn_ignored(const char *message, const char *rule){{{*/
static void 
adblock_warn_ignored(const char *message, const char *rule) {
  fprintf(stderr, "Adblock warning: %s\n", message);
  fprintf(stderr, "Adblock warning: Rule %s will be ignored\n", rule);
}/*}}}*/

/* adblock_rule_parse(char *filterlist)  {{{*/
static void
adblock_rule_parse(char *filterlist) {
  char **lines = NULL;
  if  (g_file_test(filterlist, G_FILE_TEST_IS_DIR)) {
    GString *string = g_string_new(NULL);
    util_get_directory_content(string, filterlist, NULL);
    if (string->str) 
      lines = g_strsplit(string->str, "\n", -1);
    g_string_free(string, true);
  }
  else 
    lines = util_get_lines(filterlist);
  if (lines == NULL)
    return;
  char *pattern;
  GError *error = NULL;
  char **domain_arr = NULL;
  char *domains;
  const char *domain;
  const char *tmp;
  const char *option_string;
  const char *o;
  char *tmp_a, *tmp_b, *tmp_c;
  int length = 0;
  int option, attributes, inverse;
  gboolean exception;
  GRegex *rule;
  char **options_arr;
  char warning[256];
  for (int i=0; lines[i] != NULL; i++) {
    pattern = lines[i];

    //DwbStatus ret = STATUS_OK;
    GRegexCompileFlags regex_flags = G_REGEX_OPTIMIZE | G_REGEX_CASELESS;
    g_strchomp(pattern);
    util_str_chug(pattern);
    if (*pattern == '\0' || *pattern == '!' || *pattern == '[') 
      continue;

    tmp_a = tmp_b = tmp_c = NULL;
    /* Element hiding rules */
    if ( (tmp = strstr(pattern, "##")) != NULL) {
      /* Match domains */
      if (*pattern != '#') {
        domains = g_strndup(pattern, tmp-pattern);
        AdblockElementHider *hider = adblock_element_hider_new();
        domain_arr = g_strsplit(domains, ",", -1);
        hider->domains = domain_arr;

        hider->selector = g_strdup(tmp+2);
        GSList *list;
        gboolean hider_exc = true;
        for (; *domain_arr; domain_arr++) {
          domain = *domain_arr;
          if (*domain == '~')
            domain++;
          else 
            hider_exc = false;
          list = g_hash_table_lookup(m_hider_rules, domain);
          if (list == NULL) {
            list = g_slist_append(list, hider);
            g_hash_table_insert(m_hider_rules, g_strdup(domain), list);
          }
          else {
            list = g_slist_append(list, hider);
            (void) list;
          }
          m_has_hider_rules = true;
        }
        hider->exception = hider_exc;
        if (hider_exc) {
          g_string_append(m_css_exceptions, tmp + 2);
          g_string_append_c(m_css_exceptions, ',');
        }
        m_hider_list = g_slist_append(m_hider_list, hider);
        g_free(domains);
      }
      /* general rules */
      else {
        g_string_append(m_css_rules, tmp + 2);
        g_string_append_c(m_css_rules, ',');
      }
    }
    /*  Request patterns */
    else { 
      exception = false;
      option = 0;
      attributes = 0;
      rule = NULL;
      domain_arr = NULL;
      /* Exception */
      tmp = pattern;
      if (tmp[0] == '@' && tmp[1] == '@') {
        exception = true;
        tmp +=2;
      }
      option_string = strstr(tmp, "$");
      if (option_string != NULL) {
        tmp_a = g_strndup(tmp, option_string - tmp);
        options_arr = g_strsplit(option_string+1, ",", -1);
        for (int i=0; options_arr[i] != NULL; i++) {
          inverse = 0;
          o = options_arr[i];
          /*  attributes */
          if (*o == '~') {
            inverse = AB_INVERSE;
            o++;
          }
          if (!g_strcmp0(o, "script"))
            attributes |= (AA_SCRIPT << inverse);
          else if (!g_strcmp0(o, "image"))
            attributes |= (AA_IMAGE << inverse);
          else if (!g_strcmp0(o, "stylesheet"))
            attributes |= (AA_STYLESHEET << inverse);
          else if (!g_strcmp0(o, "object")) {
            attributes |= (AA_OBJECT << inverse);
          }
          else if (!g_strcmp0(o, "subdocument")) {
            attributes |= inverse ? AA_DOCUMENT : AA_SUBDOCUMENT;
          }
          else if (!g_strcmp0(o, "document")) {
            if (exception) 
              attributes |= inverse ? AA_DOCUMENT : AA_SUBDOCUMENT;
            else 
              adblock_warn_ignored("Adblock option 'document' can only be applied to exception rules", pattern);
          }
          else if (!g_strcmp0(o, "match-case"))
            option |= AO_MATCH_CASE;
          else if (!g_strcmp0(o, "third-party")) {
            if (inverse) {
              option |= AO_NOTHIRDPARTY;
            }
            else {
              option |= AO_THIRDPARTY;
            }
          }
          else if (g_str_has_prefix(o, "domain=")) {
            domain_arr = g_strsplit(options_arr[i] + 7, "|", -1);
          }
          /* Unsupported should only be ignored if they are actually rules, not
           * exceptions */
          else if ((inverse && exception) || (!inverse && !exception)) {
            /*  currently unsupported  xbl, ping, xmlhttprequest, dtd, elemhide,
             *  other, collapse, donottrack, object-subrequest, popup 
             *  */
            snprintf(warning, sizeof(warning), "Adblock option '%s' isn't supported", o);
            adblock_warn_ignored(warning, pattern);
            goto error_out;
          }
        }
        tmp = tmp_a;
        g_strfreev(options_arr);
      }
      length = strlen(tmp);
      /* Beginning of pattern / domain */
      if (length > 0 && tmp[0] == '|') {
        if (length > 1 && tmp[1] == '|') {
          option |= AO_BEGIN_DOMAIN;
          tmp += 2;
          length -= 2;
        }
        else {
          option |= AO_BEGIN;
          tmp++;
          length--;
        }
      }
      /* End of pattern */
      if (length > 0 && tmp[length-1] == '|') {
        tmp_b = g_strndup(tmp, length-1);
        tmp = tmp_b;
        option |= AO_END;
        length--;
      }
      /* Regular Expression */
      if (length > 0 && tmp[0] == '/' && tmp[length-1] == '/') {
        tmp_c = g_strndup(tmp+1, length-2);

        if ( (option & AO_MATCH_CASE) != 0) 
          regex_flags &= ~G_REGEX_CASELESS;
        rule = g_regex_new(tmp_c, regex_flags, 0, &error);

        g_free(tmp_c);
        if (error != NULL) {
          adblock_warn_ignored("Invalid regular expression", pattern);
          //ret = STATUS_ERROR;
          g_clear_error(&error);
          goto error_out;
        }
      }
      else {
        GString *buffer = g_string_new(NULL);
        if (option & AO_BEGIN || option & AO_BEGIN_DOMAIN) {
          g_string_append_c(buffer, '^');
        }
        /*  FIXME: possibly use g_regex_escape_string */
        for (const char *regexp_tmp = tmp; *regexp_tmp; regexp_tmp++ ) {
          switch (*regexp_tmp) {
            case '^' : g_string_append(buffer, "([\\x00-\\x24\\x26-\\x2C\\x2F\\x3A-\\x40\\x5B-\\x5E\\x60\\x7B-\\x80]|$)");
                       break;
            case '*' : g_string_append(buffer, ".*");
                       break;
            case '?' : 
            case '{' : 
            case '}' : 
            case '(' : 
            case ')' : 
            case '[' : 
            case ']' : 
            case '+' : 
            case '.' : 
            case '\\' : 
            case '|' : g_string_append_c(buffer, '\\');
            default  : g_string_append_c(buffer, *regexp_tmp);
          }
        }
        if (option & AO_END) {
          g_string_append_c(buffer, '$');
        }
        if ( (option & AO_MATCH_CASE) != 0) 
          regex_flags &= ~G_REGEX_CASELESS;
        rule = g_regex_new(buffer->str, regex_flags, 0, &error);
        g_string_free(buffer, true);
        if (error != NULL) {
          fprintf(stderr, "dwb warning: ignoring adblock rule %s: %s\n", pattern, error->message);
          g_clear_error(&error);
          goto error_out;
        }
      }
      AdblockRule *adrule = adblock_rule_new();
      adrule->attributes = attributes;
      adrule->pattern = rule;
      adrule->options = option;
      adrule->domains = domain_arr;

      if (! (attributes & (AA_DOCUMENT | AA_SUBDOCUMENT)) )
          adrule->attributes |= AA_SUBDOCUMENT | AA_DOCUMENT;

      if (!(attributes & ~(AA_SUBDOCUMENT | AA_DOCUMENT))) {
        if (exception) 
          g_ptr_array_add(m_simple_exceptions, adrule);
        else 
          g_ptr_array_add(m_simple_rules, adrule);
      }
      else {
        if (exception) 
          g_ptr_array_add(m_exceptions, adrule);
        else 
          g_ptr_array_add(m_rules, adrule);
      }
    }
error_out:
    g_free(tmp_a);
    g_free(tmp_b);
  }
  g_strfreev(lines);
}/*}}}*/

/* adblock_end() {{{*/
void 
adblock_end() {
  if (m_css_rules != NULL) 
    g_string_free(m_css_rules, true);
  if (m_css_exceptions != NULL) 
    g_string_free(m_css_exceptions, true);
  if (m_rules != NULL) 
    g_ptr_array_free(m_rules, true);
  if (m_simple_rules != NULL) 
    g_ptr_array_free(m_simple_rules, true);
  if (m_simple_exceptions != NULL) 
    g_ptr_array_free(m_simple_exceptions, true);
  if(m_exceptions != NULL) 
    g_ptr_array_free(m_exceptions, true);
  if (m_hider_rules != NULL) 
    g_hash_table_remove_all(m_hider_rules);
  if (m_hider_list != NULL) {
    for (GSList *l = m_hider_list; l; l=l->next) 
      adblock_element_hider_free((AdblockElementHider*)l->data);
    g_slist_free(m_hider_list);
  }
}/*}}}*/

/* adblock_init() {{{*/
gboolean
adblock_init() {
  if (m_init)
    return true;
  if (!GET_BOOL("adblocker"))
    return false;
  char *filterlist = GET_CHAR("adblocker-filterlist");
  if (filterlist == NULL)
    return false;
  char buffer[PATH_MAX];
  filterlist = util_expand_home(buffer, filterlist, sizeof(buffer));
  if (!g_file_test(filterlist, G_FILE_TEST_EXISTS)) {
    fprintf(stderr, "Filterlist not found: %s\n", filterlist);
    return false;
  }
  m_rules              = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  m_exceptions         = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  m_simple_rules       = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  m_simple_exceptions  = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  m_hider_rules        = g_hash_table_new_full((GHashFunc)g_str_hash, (GEqualFunc)g_str_equal, (GDestroyNotify)g_free, NULL);
  m_css_exceptions     = g_string_new(NULL);
  m_css_rules          = g_string_new(NULL);
  adblock_rule_parse(filterlist);
  m_init = true;
  return true;
}/*}}}*//*}}}*/
