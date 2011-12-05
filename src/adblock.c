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

static JSValueRef adblock_js_callback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/* Static variables {{{*/
static GPtrArray *_simple_rules;
static GPtrArray *_simple_exceptions;
static GPtrArray *_rules;
static GPtrArray *_exceptions;
static GHashTable *_hider_rules;
/*  only used to freeing elementhider */
static GSList *_hider_list;
static GString *_css_rules;
static GString *_css_exceptions;
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

  for (int i=0; i<array->len; i++) {
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

  if (!adblock_match(_exceptions, realuri, host, domain, basehost, basedomain, attributes, thirdparty)) {
    if (adblock_match(_rules, realuri, host, domain, basehost, basedomain, attributes, thirdparty)) {
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
  WebKitDOMStyleSheetList *slist = NULL;
  WebKitDOMStyleSheet *ssheet = NULL;
  WebKitWebView *wv = WEBVIEW(gl);

  WebKitWebDataSource *datasource = webkit_web_frame_get_data_source(frame);
  WebKitNetworkRequest *request = webkit_web_data_source_get_request(datasource);

  SoupMessage *msg = webkit_network_request_get_message(request);
  if (msg == NULL)
    return;

  SoupURI *suri = soup_message_get_uri(msg);
  g_return_if_fail(suri != NULL);

  const char *host = soup_uri_get_host(suri);
  const char *base_domain = domain_get_base_for_host(host);
  g_return_if_fail(host != NULL);
  g_return_if_fail(base_domain != NULL);

  AdblockElementHider *hider;
  char *pattern, *escaped, *replaced = NULL;
  char *tmpreplaced = g_strdup(_css_exceptions->str);
  GString *css_rule = g_string_new(NULL);
  GRegex *regex = NULL;

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


  for (int i=0; subdomains[i]; i++) {
    list = g_hash_table_lookup(_hider_rules, subdomains[i]);
    if (list) {
      for (GSList *l = list; l; l=l->next) {
        hider = l->data;
        if (hider->exception) {
          escaped = g_regex_escape_string(hider->selector, -1);
          pattern = g_strdup_printf("(?<=^|,)%s,?", escaped);
          regex = g_regex_new(pattern, 0, 0, NULL);
          replaced = g_regex_replace(regex, tmpreplaced, -1, 0, "", 0, NULL);
          g_free(tmpreplaced);
          tmpreplaced = replaced;
          g_free(escaped);
          g_free(pattern);
          g_regex_unref(regex);
        }
        else if (domain_match(hider->domains, host, base_domain)) {
          g_string_append(css_rule, hider->selector);
          g_string_append_c(css_rule, ',');
        }
      }
      break;
    }
  }
  /* Adding replaced exceptions */
  if (replaced != NULL) {
    g_string_append(css_rule, replaced);
    g_string_append_c(css_rule, ',');
  }
  /* No exception-exceptions found, so we take all exceptions */
  else if (css_rule == NULL || css_rule->len == 0) {
    g_string_append(css_rule, _css_exceptions->str);
  }
  if ((css_rule != NULL && css_rule->len > 1) || (_css_rules != NULL && _css_rules->len > 1)) {
    g_string_append(css_rule, _css_rules->str);
    if (css_rule->str[css_rule->len-1] == ',') 
      g_string_erase(css_rule, css_rule->len-1, 1);
    g_string_append(css_rule, "{display:none!important;}");
    if (frame == webkit_web_view_get_main_frame(wv)) {
      WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
      slist = webkit_dom_document_get_style_sheets(doc);
      if (slist) {
        ssheet = webkit_dom_style_sheet_list_item(slist, 0);
      }
      if (ssheet) {
        webkit_dom_css_style_sheet_insert_rule((void*)ssheet, css_rule->str, 0, NULL);
      }
      else {
        WebKitDOMElement *style = webkit_dom_document_create_element(doc, "style", NULL);
        WebKitDOMHTMLHeadElement *head = webkit_dom_document_get_head(doc);
        webkit_dom_html_element_set_inner_html(WEBKIT_DOM_HTML_ELEMENT(style), css_rule->str, NULL);
        webkit_dom_node_append_child(WEBKIT_DOM_NODE(head), WEBKIT_DOM_NODE(style), NULL);
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
          "var st=document.createElement('style');\
          document.head.appendChild(st);\
          document.styleSheets[document.styleSheets.length-1].insertRule('%s', 0);", 
          css_rule->str);
      dwb_execute_script(frame, script, false);
      g_free(script);
    }
    g_string_free(css_rule, true);
  }
  g_free(tmpreplaced);

}/*}}}*/
/*}}}*/


/* LOAD_CALLBACKS {{{*/
/* adblock_js_callback {{{*/
static JSValueRef 
adblock_js_callback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exception) {
  if (argc != 1 || ! JSValueIsObject(ctx, argv[0]))
    return JSValueMakeBoolean(ctx, false);
  char *tagname = NULL;
  char *baseuri = NULL;
  char *tmpuri = NULL;
  char *rel = NULL; 
  char *type = NULL;
  JSValueRef exc = NULL;
  AdblockAttribute attributes = AA_SUBDOCUMENT;

  JSObjectRef event = JSValueToObject(ctx, argv[0], &exc);
  if (exc != NULL)
    return NULL;

  JSObjectRef srcElement = js_get_object_property(ctx, event, "srcElement"); 
  if (srcElement == NULL)
    return JSValueMakeBoolean(ctx, false);
  baseuri = js_get_string_property(ctx, srcElement, "baseURI");
  if (baseuri == NULL)
    goto error_out;
  tmpuri = js_get_string_property(ctx, event, "url");
  if (tmpuri == NULL)
    goto error_out;


  tagname = js_get_string_property(ctx, srcElement, "tagName");
  if (!g_strcmp0(tagname, "IMG"))
    attributes |= AA_IMAGE;
  else if (!g_strcmp0(tagname, "SCRIPT"))
    attributes |= AA_SCRIPT;
  else if (!g_strcmp0(tagname, "LINK")  ) {
    rel  = js_get_string_property(ctx, srcElement, "rel");
    type  = js_get_string_property(ctx, srcElement, "type");
    if (!g_strcmp0(rel, "stylesheet") || !g_strcmp0(type, "text/css")) {
      attributes |= AA_STYLESHEET;
    }
    FREE(rel);
    FREE(type);
  }
  else if (!g_strcmp0(tagname, "OBJECT") || ! g_strcmp0(tagname, "EMBED")) {
    attributes |= AA_OBJECT;
  }

  if (adblock_prepare_match(tmpuri, baseuri, attributes)) {
    JSObjectRef prevent = js_get_object_property(ctx, event, "preventDefault");
    if (prevent) {
      JSObjectCallAsFunction(ctx, prevent, event, 0, NULL, NULL);
    }
  }
error_out:
  if (tagname) g_free(tagname);
  if (baseuri) g_free(baseuri);
  if (tmpuri) g_free(tmpuri);
  return NULL;
}/*}}}*/

/* js_create_callback {{{*/
static void
adblock_create_js_callback(WebKitWebFrame *frame, JSObjectCallAsFunctionCallback function, int attr) {
  JSContextRef ctx = webkit_web_frame_get_global_context(frame);
  JSObjectRef globalObject = JSContextGetGlobalObject(ctx);
  JSObjectRef newcall = JSObjectMakeFunctionWithCallback(ctx, NULL, function);
  JSValueRef val = js_get_object_property(ctx, globalObject, "addEventListener");
  if (val) {
    JSStringRef beforeLoadString = JSStringCreateWithUTF8CString("beforeload");
    JSValueRef values[3] = { JSValueMakeString(ctx, beforeLoadString), newcall, JSValueMakeBoolean(ctx, true), };
    JSObjectCallAsFunction(ctx, JSValueToObject(ctx, val, NULL), globalObject, 3,  values, NULL);
    JSStringRelease(beforeLoadString);
  }
}/*}}}*/

/* adblock_frame_load_committed_cb {{{*/
static void 
adblock_frame_load_committed_cb(WebKitWebFrame *frame, GList *gl) {
  adblock_create_js_callback(frame, (JSObjectCallAsFunctionCallback)adblock_js_callback, AA_SUBDOCUMENT);
}/*}}}*/

static void 
adblock_frame_load_status_cb(WebKitWebFrame *frame, GParamSpec *p, GList *gl) {
  WebKitLoadStatus status = webkit_web_frame_get_load_status(frame);
  if (status == WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT) {
    adblock_apply_element_hider(frame, gl);
  }
}

/* adblock_frame_created_cb {{{*/
void 
adblock_frame_created_cb(WebKitWebView *wv, WebKitWebFrame *frame, GList *gl) {
  g_signal_connect(frame, "load-committed", G_CALLBACK(adblock_frame_load_committed_cb), gl);
  g_signal_connect(frame, "notify::load-status", G_CALLBACK(adblock_frame_load_status_cb), gl);
}/*}}}*/

/* adblock_before_load_cb  (domcallback) {{{*/
static gboolean
adblock_before_load_cb(WebKitDOMElement *win, WebKitDOMEvent *event, GList *gl) {
  WebKitDOMElement *src = (void*)webkit_dom_event_get_src_element(event);
  char *tagname = webkit_dom_element_get_tag_name(src);
  const char *url = NULL;
  AdblockAttribute attributes = AA_DOCUMENT;

  WebKitDOMDocument *doc = webkit_dom_dom_window_get_document(WEBKIT_DOM_DOM_WINDOW(win));
  char *baseURI = webkit_dom_document_get_document_uri(doc);

  if (webkit_dom_element_has_attribute(src, "src")) 
    url = webkit_dom_element_get_attribute(src, "src");
  else if (webkit_dom_element_has_attribute(src, "href")) 
    url = webkit_dom_element_get_attribute(src, "href");
  else if (webkit_dom_element_has_attribute(src, "data")) 
    url = webkit_dom_element_get_attribute(src, "data");
  if (url == NULL) 
    return false;

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
  }
  else if (!g_strcmp0(tagname, "OBJECT") || ! g_strcmp0(tagname, "EMBED")) {
    attributes |= AA_OBJECT;
  }
  if (adblock_prepare_match(url, baseURI, attributes)) {
    webkit_dom_event_prevent_default(event);
  }
  return true;
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
  gboolean thirdparty = strcmp(domain, firstdomain);

  if (!adblock_match(_simple_exceptions, uri, host, domain, firsthost, firstdomain, attribute, thirdparty)) {
    if (adblock_match(_simple_rules, uri, host, domain, firsthost, firstdomain, attribute, thirdparty)) {
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
    /* Get hostname and base_domain */
  }
}/*}}}*//*}}}*/


/* START AND END {{{*/

gboolean
adblock_running() {
  return _init && GET_BOOL("adblocker");
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
}/*}}}*/

/* adblock_connect() {{{*/
void 
adblock_connect(GList *gl) {
  if (!_init && !adblock_init()) 
      return;
  if (_rules->len > 0 || _css_rules->len > 0) {
    VIEW(gl)->status->signals[SIG_AD_LOAD_STATUS] = g_signal_connect(WEBVIEW(gl), "notify::load-status", G_CALLBACK(adblock_load_status_cb), gl);
    VIEW(gl)->status->signals[SIG_AD_FRAME_CREATED] = g_signal_connect(WEBVIEW(gl), "frame-created", G_CALLBACK(adblock_frame_created_cb), gl);
  }
  if (_simple_rules->len > 0) {
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
  char *content = util_get_file_content(filterlist);
  char **lines = g_strsplit(content, "\n", -1);
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
    if (pattern == NULL)
      break;
    //return STATUS_IGNORE;
    g_strstrip(pattern);
    if (strlen(pattern) == 0) 
      break;
    //return STATUS_IGNORE;
    if (pattern[0] == '!' || pattern[0] == '[') {
      break;
      //return STATUS_IGNORE;
    }
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
          list = g_hash_table_lookup(_hider_rules, domain);
          if (list == NULL) {
            list = g_slist_append(list, hider);
            g_hash_table_insert(_hider_rules, g_strdup(domain), list);
          }
          else {
            list = g_slist_append(list, hider);
            (void) list;
          }
        }
        hider->exception = hider_exc;
        if (hider_exc) {
          g_string_append(_css_exceptions, tmp + 2);
          g_string_append_c(_css_exceptions, ',');
        }
        _hider_list = g_slist_append(_hider_list, hider);
        g_free(domains);
      }
      /* general rules */
      else {
        g_string_append(_css_rules, tmp + 2);
        g_string_append_c(_css_rules, ',');
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
          if (!strcmp(o, "script"))
            attributes |= (AA_SCRIPT << inverse);
          else if (!strcmp(o, "image"))
            attributes |= (AA_IMAGE << inverse);
          else if (!strcmp(o, "stylesheet"))
            attributes |= (AA_STYLESHEET << inverse);
          else if (!strcmp(o, "object")) {
            attributes |= (AA_OBJECT << inverse);
          }
          else if (!strcmp(o, "object-subrequest")) {
            if (! inverse) {
              adblock_warn_ignored("Adblock option 'object-subrequest' isn't supported", pattern);
              goto error_out;
            }
          }
          else if (!strcmp(o, "subdocument")) {
            attributes |= inverse ? AA_DOCUMENT : AA_SUBDOCUMENT;
          }
          else if (!strcmp(o, "document")) {
            if (exception) 
              attributes |= inverse ? AA_DOCUMENT : AA_SUBDOCUMENT;
            else 
              adblock_warn_ignored("Adblock option 'document' can only be applied to exception rules", pattern);
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
            domain_arr = g_strsplit(options_arr[i] + 7, "|", -1);
          }
          else {
            /*  currently unsupported  xbl, ping, xmlhttprequest, dtd, elemhide,
             *  other, collapse, donottrack, object-subrequest, popup */
            snprintf(warning, 255, "Adblock option '%s' isn't supported", o);
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

        FREE(tmp_c);
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
          g_ptr_array_add(_simple_exceptions, adrule);
        else 
          g_ptr_array_add(_simple_rules, adrule);
      }
      else {
        if (exception) 
          g_ptr_array_add(_exceptions, adrule);
        else 
          g_ptr_array_add(_rules, adrule);
      }
    }
error_out:
    FREE(tmp_a);
    FREE(tmp_b);
  }
  g_strfreev(lines);
  g_free(content);
}/*}}}*/

/* adblock_end() {{{*/
void 
adblock_end() {
  if (_css_rules != NULL) 
    g_string_free(_css_rules, true);
  if (_css_exceptions != NULL) 
    g_string_free(_css_exceptions, true);
  if (_rules != NULL) 
    g_ptr_array_free(_rules, true);
  if (_simple_rules != NULL) 
    g_ptr_array_free(_simple_rules, true);
  if (_simple_exceptions != NULL) 
    g_ptr_array_free(_simple_exceptions, true);
  if(_exceptions != NULL) 
    g_ptr_array_free(_exceptions, true);
  if (_hider_rules != NULL) 
    g_hash_table_remove_all(_hider_rules);
  if (_hider_list != NULL) {
    for (GSList *l = _hider_list; l; l=l->next) 
      adblock_element_hider_free((AdblockElementHider*)l->data);
    g_slist_free(_hider_list);
  }
}/*}}}*/

/* adblock_init() {{{*/
gboolean
adblock_init() {
  if (_init)
    return true;
  if (!GET_BOOL("adblocker"))
    return false;
  char *filterlist = GET_CHAR("adblocker-filterlist");
  if (filterlist == NULL)
    return false;
  if (!g_file_test(filterlist, G_FILE_TEST_EXISTS)) {
    fprintf(stderr, "Filterlist not found: %s\n", filterlist);
    return false;
  }
  _rules              = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  _exceptions         = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  _simple_rules       = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  _simple_exceptions  = g_ptr_array_new_with_free_func((GDestroyNotify)adblock_rule_free);
  _hider_rules        = g_hash_table_new_full((GHashFunc)g_str_hash, (GEqualFunc)g_str_equal, (GDestroyNotify)g_free, NULL);
  _css_exceptions     = g_string_new(NULL);
  _css_rules          = g_string_new(NULL);
  domain_init();
  adblock_rule_parse(filterlist);
  _init = true;
  return true;
}/*}}}*//*}}}*/
#endif
