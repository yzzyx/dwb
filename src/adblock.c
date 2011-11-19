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

#ifdef DWB_ADBLOCKER
#include "dwb.h"
#include "util.h"
#include "domain.h"

#define BUFFER_SIZE 4096
#define LINE_SIZE 1024

#define ADBLOCK_INVERSE 15
/* clear upper bits, last attribute may be 1<<14 */
#define ADBLOCK_CLEAR_UPPER 0x7fff
#define ADBLOCK_CLEAR_LOWER 0x3fff8000

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

typedef struct _AdblockRule {
  GRegex *pattern;
  AdblockOption options;
  AdblockAttribute attributes;
  char **domains;
} AdblockRule;

typedef struct _AdblockElementHider {
  char *selector;
  char **domains;
} AdblockElementHider;
/*}}}*/

/* Static variables {{{*/
static GPtrArray *_hider;
static GPtrArray *_rules;
static GPtrArray *_exceptions;
//static GHashTable *_tld_table;
static GString *_css_rules;
static int _sig_resource;
static gboolean _init = false;
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
  if (hider->selector)
    g_free(hider->selector);
  if (hider->domains)
    g_strfreev(hider->domains);
  g_free(hider);
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

/* adblock_match(GPtrArray *, SoupURI *, const char *base_domain, * AdblockAttribute, gboolean thirdparty) {{{*/
gboolean                
adblock_match(GPtrArray *array, SoupURI *soupuri, const char *base_domain, AdblockAttribute attributes, gboolean thirdparty) {
  gboolean match = false;

  char *uri = soup_uri_to_string(soupuri, false);
  const char *base_start = strstr(uri, base_domain);
  const char *uri_start = strstr(uri, soupuri->host);
  const char *suburis[SUBDOMAIN_MAX];
  int uc = 0;
  const char *host = soup_uri_get_host(soupuri);
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

  for (int i=0; i<array->len; i++) {
    rule = g_ptr_array_index(array, i);
    if (attributes) {
      /* If exception attributes exists, check if exception is matched */
      if (rule->attributes & ADBLOCK_CLEAR_LOWER && rule->attributes & (attributes<<ADBLOCK_INVERSE)) 
        continue;
      /* If attribute restriction exists, check if attribute is matched */
      if (rule->attributes & ADBLOCK_CLEAR_UPPER && ! (rule->attributes & attributes) ) 
        continue;
    }
    if (rule->domains && !domain_match(rule->domains, host, base_domain)) {
      continue;
    }
    if    ( (rule->options & AO_THIRDPARTY && !thirdparty) 
        ||  (rule->options & AO_NOTHIRDPARTY && thirdparty) )
      continue;
    if (rule->options & AO_BEGIN_DOMAIN) {
      for (int i=0; suburis[i]; i++) {
        if ( (match = adblock_do_match(rule, suburis[i])) ) 
          goto done;
      }
    }
    else if ((match = adblock_do_match(rule, uri))) {
      break;
    }

  }
done:
  g_free(uri);
  return match;
}/*}}}*//*}}}*/


/* CALLBACKS {{{*/
/* adblock_content_sniffed_cb(SoupMessage *, char *, GHashTable *, SoupSession *) {{{*/
void 
#ifdef DWB_DEBUG
adblock_content_sniffed_cb_real(SoupMessage *msg, char *type, GHashTable *table, SoupSession *session) {
#else
adblock_content_sniffed_cb(SoupMessage *msg, char *type, GHashTable *table, SoupSession *session) {
#endif
  AdblockAttribute attribute = 0;
  SoupURI *uri = soup_message_get_uri(msg);
  SoupURI *first_party_uri = soup_message_get_first_party(msg);
  const char *host = soup_uri_get_host(uri);
  const char *first_party_host = soup_uri_get_host(first_party_uri);
  const char *base_domain = domain_get_base_for_host(host);
  const char *base_first_party = domain_get_base_for_host(first_party_host);

  g_return_if_fail(base_domain != NULL);
  g_return_if_fail(base_first_party != NULL);

  gboolean third_party = strcmp(base_first_party, base_domain);

  if (!strncmp(type, "image/", 6)) {
    attribute = AA_IMAGE;
  }
  else if (!strstr("javascript", type)) {
    attribute = AA_SCRIPT;
  }
  else if (!strcmp(type, "application/x-shockwave-flash") || !strcmp(type, "application/x-flv")) {
    attribute = AA_OBJECT_SUBREQUEST;
  }
  else if (!strcmp(type, "text/css")) {
    attribute = AA_STYLESHEET;
  }
  PRINT_DEBUG("%s %s %d %s\n", host, base_domain, attribute, soup_uri_to_string(uri, false));
  if (adblock_match(_exceptions, uri, base_domain, attribute, third_party)) {
    return;
  }
  if (adblock_match(_rules, uri, base_domain, attribute, third_party)) {
    soup_session_cancel_message(session, msg, 204);
  }
}/*}}}*/
#ifdef DWB_DEBUG
void 
adblock_content_sniffed_cb(SoupMessage *msg, char *type, GHashTable *table, SoupSession *session) {
  DEBUG_TIMED(1, adblock_content_sniffed_cb_real(msg, type, table, session));
}
#endif

/* adblock_frame_load_committed_cb {{{*/
static void 
adblock_frame_load_committed_cb(WebKitWebFrame *frame, GList *gl) {

}/*}}}*/

/* adblock_frame_created_cb {{{*/
static void 
adblock_frame_created_cb(WebKitWebView *wv, WebKitWebFrame *frame, GList *gl) {
  g_signal_connect(frame, "load-committed", G_CALLBACK(adblock_frame_load_committed_cb), gl);
}/*}}}*/

/* adblock_load_status_cb(WebKitWebView *, GParamSpec *, GList *) {{{*/
static void
adblock_load_status_cb(WebKitWebView *wv, GParamSpec *p, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(wv);
  if (status == WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT) {
    /* Get hostname and base_domain */
    AdblockElementHider *hider;
    WebKitWebFrame *frame = webkit_web_view_get_main_frame(wv);
    WebKitWebDataSource *datasource = webkit_web_frame_get_data_source(frame);
    WebKitNetworkRequest *request = webkit_web_data_source_get_request(datasource);
    SoupMessage *msg = webkit_network_request_get_message(request);
    SoupURI *suri = soup_message_get_uri(msg);
    const char *host = soup_uri_get_host(suri);
    const char *base_domain = domain_get_base_for_host(host);

    g_return_if_fail(base_domain != NULL);
    
    GString *css_rule = g_string_new(_css_rules->str);
    for (int i=0; i<_hider->len; i++) {
      hider = g_ptr_array_index(_hider, i);
      if (domain_match(hider->domains, host, base_domain)) {
        g_string_append(css_rule, hider->selector);
        g_string_append_c(css_rule, ',');
      }
    }
    /* remove trailing comma */
    g_string_erase(css_rule, css_rule->len-1, 1);

    g_string_append(css_rule, "{display:none!important;}");

    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    WebKitDOMElement *style = webkit_dom_document_create_element(doc, "style", NULL);
    webkit_dom_element_set_attribute(style, "type", "text/css", NULL);
    WebKitDOMHTMLElement *head = WEBKIT_DOM_HTML_ELEMENT(webkit_dom_document_get_head(doc));
    webkit_dom_html_element_set_inner_html(WEBKIT_DOM_HTML_ELEMENT(style), css_rule->str, NULL);
    webkit_dom_node_append_child(WEBKIT_DOM_NODE(head), WEBKIT_DOM_NODE(style), NULL);

    g_string_free(css_rule, true);
  }
}/*}}}*/

/* adblock_request_started_cb(SoupSession *, SoupMessage *, SoupSocket *) {{{*/
static void
adblock_request_started_cb(SoupSession *session, SoupMessage *msg, SoupSocket *socket) {
  g_signal_connect(msg, "content-sniffed", G_CALLBACK(adblock_content_sniffed_cb), session);
}/*}}}*//*}}}*/


/* START AND END {{{*/
/* adblock_disconnect(GList *) {{{*/
void 
adblock_disconnect(GList *gl) {
  if ((VIEW(gl)->status->signals[SIG_AD_LOAD_STATUS]) > 0) 
    g_signal_handler_disconnect(WEBVIEW(gl), (VIEW(gl)->status->signals[SIG_AD_LOAD_STATUS]));
  if ((VIEW(gl)->status->signals[SIG_AD_FRAME_CREATED]) > 0) 
    g_signal_handler_disconnect(WEBVIEW(gl), (VIEW(gl)->status->signals[SIG_AD_FRAME_CREATED]));
  
  if (_sig_resource != 0) {
    g_signal_handler_disconnect(webkit_get_default_session(), _sig_resource);
    _sig_resource = 0;
  }
}/*}}}*/

/* adblock_connect() {{{*/
void 
adblock_connect(GList *gl) {
  if (_init) {
    if (_hider->len > 0 || _css_rules->len > 0) {
      VIEW(gl)->status->signals[SIG_AD_LOAD_STATUS] = g_signal_connect(WEBVIEW(gl), "notify::load-status", G_CALLBACK(adblock_load_status_cb), gl);
      VIEW(gl)->status->signals[SIG_AD_FRAME_CREATED] = g_signal_connect(WEBVIEW(gl), "frame-created", G_CALLBACK(adblock_frame_created_cb), gl);
    }
    if (_sig_resource == 0) {
      _sig_resource = g_signal_connect(webkit_get_default_session(), "request-started", G_CALLBACK(adblock_request_started_cb), NULL);
    }
  }
}/*}}}*/

/* adblock_rule_parse(char *pattern) return: DwbStatus {{{*/
DwbStatus
adblock_rule_parse(char *pattern) {
  DwbStatus ret = STATUS_OK;
  GError *error = NULL;
  GRegexCompileFlags regex_flags = G_REGEX_OPTIMIZE | G_REGEX_CASELESS;
  if (pattern == NULL)
    return STATUS_IGNORE;
  g_strstrip(pattern);
  if (strlen(pattern) == 0) 
    return STATUS_IGNORE;
  if (pattern[0] == '!' || pattern[0] == '[') {
    return STATUS_IGNORE;
  }
  //AdblockRule *rule = adblock_rule_new();
  char *tmp = NULL;
  char *tmp_a = NULL, *tmp_b = NULL, *tmp_c = NULL;
  /* Element hiding rules */
  if ( (tmp = strstr(pattern, "##")) != NULL) {
    /* Match domains */
    if (pattern[0] != '#') {
      // TODO domain hashtable
      char *domains = g_strndup(pattern, tmp-pattern);
      AdblockElementHider *hider = adblock_element_hider_new();
      hider->domains = g_strsplit(domains, ",", -1);
      hider->selector = g_strdup(tmp+2);
      g_free(domains);
      g_ptr_array_add(_hider, hider);
    }
    /* general rules */
    else {
      g_string_append(_css_rules, tmp + 2);
      g_string_append_c(_css_rules, ',');
    }
  }
  /*  Request patterns */
  else { 
    gboolean exception = false;
    char *options = NULL;
    int option = 0;
    int attributes = 0;
    void *rule = NULL;
    char **domains = NULL;
    /* TODO parse options */ 
    /* Exception */
    tmp = pattern;
    if (tmp[0] == '@' && tmp[1] == '@') {
      exception = true;
      tmp +=2;
    }
    options = strstr(tmp, "$");
    if (options != NULL) {
      tmp_a = g_strndup(tmp, options - tmp);
      char **options_arr = g_strsplit(options+1, ",", -1);
      int inverse = 0;
      char *o;
      for (int i=0; options_arr[i] != NULL; i++) {
        inverse = 0;
        o = options_arr[i];
        /*  attributes */
        if (*o == '~') {
          inverse = ADBLOCK_INVERSE;
          o++;
        }
        if (!strcmp(o, "script"))
          attributes |= (AA_SCRIPT << inverse);
        else if (!strcmp(o, "image"))
          attributes |= (AA_IMAGE << inverse);
        else if (!strcmp(o, "stylesheet"))
          attributes |= (AA_STYLESHEET << inverse);
        else if (!strcmp(o, "object")) {
          fprintf(stderr, "Not supported: adblock option 'object'\n");
        }
        else if (!strcmp(o, "xbl")) {
          fprintf(stderr, "Not supported: adblock option 'xbl'\n");
          goto error_out;
        }
        else if (!strcmp(o, "ping")) {
          fprintf(stderr, "Not supported: adblock option 'xbl'\n");
          goto error_out;
        }
        else if (!strcmp(o, "xmlhttprequest")) {
          fprintf(stderr, "Not supported: adblock option 'xmlhttprequest'\n");
          goto error_out;
        }
        else if (!strcmp(o, "object-subrequest")) {
          attributes |= (AA_OBJECT_SUBREQUEST << inverse);
        }
        else if (!strcmp(o, "dtd")) {
          fprintf(stderr, "Not supported: adblock option 'dtd'\n");
          goto error_out;
        }
        else if (!strcmp(o, "subdocument")) {
          attributes |= (AA_SUBDOCUMENT << inverse);
          fprintf(stderr, "Not supported: adblock option 'subdocument'\n");
        }
        else if (!strcmp(o, "document")) {
          attributes |= (AA_DOCUMENT << inverse);
          fprintf(stderr, "Not supported: adblock option 'document'\n");
        }
        else if (!strcmp(o, "elemhide")) {
          attributes |= (AA_ELEMHIDE << inverse);
        }
        else if (!strcmp(o, "other")) {
          fprintf(stderr, "Not supported: adblock option 'other'\n");
          goto error_out;
        }
        else if (!strcmp(o, "collapse")) {
          fprintf(stderr, "Not supported: adblock option 'collapse'\n");
          goto error_out;
        }
        else if (!strcmp(o, "donottrack")) {
          fprintf(stderr, "Not supported: adblock option 'donottrack'\n");
          goto error_out;
        }
        else if (!strcmp(o, "match-case"))
          option |= AO_MATCH_CASE;
        else if (!strcmp(o, "third-party")) {
          if (inverse) {
            option |= AO_NOTHIRDPARTY;
          }
          else {
            option |= AO_THIRDPARTY;
          }
        }
        else if (g_str_has_prefix(o, "domain=")) {
          domains = g_strsplit(options_arr[i] + 7, "|", -1);
        }
      }
      tmp = tmp_a;
      g_strfreev(options_arr);
    }
    int length = strlen(tmp);
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

      FREE(tmp_c);
      if (error != NULL) {
        fprintf(stderr, "dwb warning: ignoring adblock rule %s: %s\n", pattern, error->message);
        ret = STATUS_ERROR;
        g_clear_error(&error);
        goto error_out;
      }
    }
    else {
      GString *buffer = g_string_new(NULL);
      if (option & AO_BEGIN || option & AO_BEGIN_DOMAIN) {
        g_string_append_c(buffer, '^');
      }
      for (char *regexp_tmp = tmp; *regexp_tmp; regexp_tmp++ ) {
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
      if (error != NULL) {
        fprintf(stderr, "dwb warning: ignoring adblock rule %s: %s\n", pattern, error->message);
        ret = STATUS_ERROR;
        g_clear_error(&error);
      }
      g_string_free(buffer, true);
    }

    AdblockRule *adrule = adblock_rule_new();
    adrule->attributes = attributes;
    adrule->pattern = rule;
    adrule->options = option;
    adrule->domains = domains;
    if (exception) 
      g_ptr_array_add(_exceptions, adrule);
    else 
      g_ptr_array_add(_rules, adrule);
  }
error_out:
  FREE(tmp_a);
  FREE(tmp_b);
  return ret;
}/*}}}*/

/* adblock_end() {{{*/
void 
adblock_end() {
  if (_css_rules != NULL) 
    g_string_free(_css_rules, true);
  if (_rules != NULL) 
    g_ptr_array_free(_rules, true);
  if(_exceptions != NULL) 
    g_ptr_array_free(_exceptions, true);
}/*}}}*/

/* adblock_init() {{{*/
void
adblock_init() {
  char *filterlist = GET_CHAR("adblocker-filterlist");
  if (filterlist == NULL)
    return;
  if (!g_file_test(filterlist, G_FILE_TEST_EXISTS)) {
    fprintf(stderr, "Filterlist not found: %s\n", filterlist);
    return;
  }
  _css_rules = g_string_new(NULL);
  _hider              = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_element_hider_free);
  _rules              = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  _exceptions         = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  //_simple_rules       = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  //_simple_exceptions  = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  domain_init();

  char *content = util_get_file_content(filterlist);
  char **lines = g_strsplit(content, "\n", -1);
  for (int i=0; lines[i] != NULL; i++) {
    adblock_rule_parse(lines[i]);
  }
  g_strfreev(lines);
  g_free(content);
  PRINT_DEBUG("rules: %d exc: %d simple_rules: %d simple_exceptions: %d\n", _rules->len, _exceptions->len, _simple_rules->len, _simple_exceptions->len);
  _init = true;
}/*}}}*//*}}}*/
#endif
