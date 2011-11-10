#include "dwb.h"
#include "util.h"
#include <fnmatch.h>

#define BUFFER_SIZE 4096
#define LINE_SIZE 1024
typedef enum _AdblockOption {
  AO_WILDCARD           = 1<<0,
  AO_SEPERATOR          = 1<<1,
  AO_BEGIN              = 1<<2,
  AO_BEGIN_DOMAIN       = 1<<3,
  AO_END                = 1<<4,
  AO_REGEXP             = 1<<6,
  AO_MATCH_CASE         = 1<<7,
  AO_THIRDPARTY         = 1<<8,
  AO_NOTHIRDPARTY       = 1<<9,
} AdblockOption;
/*  Attributes */
typedef enum _AdblockAttribute {
  AA_SCRIPT             = 1<<0,
  AA_IMAGE              = 1<<1,
  AA_BACKGROUND         = 1<<2,
  AA_STYLESHEET         = 1<<3,
  AA_OBJECT             = 1<<4,
  AA_XBL                = 1<<5,
  AA_PING               = 1<<6,
  AA_XMLHTTPREQUEST     = 1<<7,
  AA_OBJECT_SUBREQUEST  = 1<<8,
  AA_DTD                = 1<<9,
  AA_SUBDOCUMENT        = 1<<10,
  AA_DOCUMENT           = 1<<11,
  AA_ELEMHIDE           = 1<<12,
  AA_OTHER              = 1<<13,
  /* inverse */
} AdblockAttribute;


typedef struct _AdblockRule {
  void *pattern;
  AdblockOption options;
  AdblockAttribute attributes;
  char **domains;
} AdblockRule;

typedef struct _AdblockElementHider {
  const char *css_rule;
  char **domains;
} AdblockElementHider;

static GPtrArray *_hider;
static GPtrArray *_simple_rules;
static GPtrArray *_simple_exceptions;
static GPtrArray *_rules;
static GPtrArray *_exceptions;
static GString *_css_rules;
static SoupContentSniffer *content_sniffer;

static AdblockRule *
adblock_rule_new() {
  AdblockRule *rule = dwb_malloc(sizeof(AdblockRule));
  rule->pattern = NULL;
  rule->options = 0;
  rule->domains = NULL;
  return rule;
}
#if 0
static void 
adblock_rule_free(AdblockRule *rule) {
  if (rule->pattern != NULL) {
    g_free(rule->pattern);
  }
  if (rule->domains != NULL) {
    g_strfreev(rule->domains);
  }
  g_free(rule);
}
#endif
//gboolean
//adblock_domain_matches(const char *host, const char *domain) {
//  char *match;
//  if (!strcmp(host, domain))
//    return true;
//  if ( (match = g_strrstr(host, domain)) == NULL) {
//    return false;
//  }
//  if (match == host)
//    return true;
//  while (*host)  {
//    if (*host == '.' && ((host == domain) || (*(host+1) && host+1 == match))) 
//      return true;
//    host++;
//  }
//  return false;
//}
gboolean 
adblock_domain_matches_uri(char *uri, char *domain) {
  if (uri == NULL || domain == NULL) 
    return false;
  while (*uri == '/') {
    uri++;
  }
  char *res = NULL, *fres = NULL;

  char *tmp = strstr(uri, "://");
  if (tmp) 
    tmp += 3;
  else 
    tmp = uri;
  char *slash = strchr(tmp, '/');
  if (slash) {
    res = g_strndup(tmp, slash - tmp);
    fres = res;
  }
  else 
    res = tmp;
  char *colon = strchr(res, ':');
  if (colon) {
    res = g_strndup(res, colon - res);
    FREE(fres);
    fres = res;
  }
  return !strcmp(domain, res);

}
gboolean
adblock_check_thirdparty(SoupURI *first_party, SoupURI *uri) {
  return false;
}
gboolean
adblock_domain_matches(const char *host, const char *domain) {
  const char *tmp = host;
  if (!strcmp(host, domain)) 
    return true;
  while ((tmp = strchr(tmp, '.'))) {
    if (!strcmp(tmp, domain))
      return true;
    tmp++;
  }
  return false;
}

gboolean                
adblock_match_simple(GPtrArray *array,
                            WebKitWebView         *wv,
                            WebKitWebFrame        *frame,
                            WebKitWebResource     *resource,
                            WebKitNetworkRequest  *request,
                            WebKitNetworkResponse *response,
                            GList                 *gl) {
  puts("resource");
  g_return_val_if_fail(request != NULL, false);

  gboolean ret      = false;
  const char *uri   = webkit_network_request_get_uri(request);
  SoupMessage *msg  = webkit_network_request_get_message(request);

  gboolean is_third_party = false;
  if (msg == NULL) {
    msg = webkit_network_response_get_message(response);
    if (msg == NULL)
      puts("immer noch");
  }

  g_return_val_if_fail(msg != NULL, false);
  SoupURI *first_party = soup_message_get_first_party(msg);
  SoupURI *suri        = soup_message_get_uri(msg);

  is_third_party = adblock_check_thirdparty(first_party, suri);
  for (int i=0; i<array->len; i++) {
    AdblockRule *rule = g_ptr_array_index(array, i);

    if    ( (rule->options & AO_THIRDPARTY && !is_third_party) 
        ||  (rule->options & AO_NOTHIRDPARTY && is_third_party) )
      continue;
    if (rule->options & AO_SEPERATOR || rule->options & AO_REGEXP) {
      if (g_regex_match(rule->pattern, uri, 0, NULL)) {
        printf("blocked %s %s", uri, g_regex_get_pattern(rule->pattern));
        ret = true;
        break;
      }
    }
    else {
      int flags = FNM_NOESCAPE;
      if ((rule->options & AO_MATCH_CASE) == 0 ) {
        flags |= FNM_CASEFOLD;
      }
      if (fnmatch(rule->pattern, uri, flags) == 0) {
        printf("blocked %s %s", uri, (rule->pattern));
        ret = true;
        break;
      }
    }

  }
  return ret;
}
void                
adblock_resource_request_cb(WebKitWebView         *wv,
                            WebKitWebFrame        *frame,
                            WebKitWebResource     *resource,
                            WebKitNetworkRequest  *request,
                            WebKitNetworkResponse *response,
                            GList                 *gl) {
  /* Don't block the main request */
  WebKitWebFrame *main_frame = webkit_web_view_get_main_frame(wv);
  if (frame == main_frame && webkit_web_frame_get_load_status(frame) == WEBKIT_LOAD_PROVISIONAL)
    return;
  /* First process exceptions */
  if (adblock_match_simple(_simple_exceptions, wv, frame, resource, request, response, gl)) 
    return;
  if (adblock_match_simple(_simple_rules, wv, frame, resource, request, response, gl)) {
    webkit_network_request_set_uri(request, "about:blank");
    return;
  }
    
  if (request == NULL) 
    return;
  SoupMessage *message;
  if (request) {
    message = webkit_network_request_get_message(request);
    //req_uri = webkit_network_request_get_uri(request);
  }
  else if (response) {
    message = webkit_network_response_get_message(response);
    //req_uri = webkit_network_response_get_uri(response);
  }
  else 
    return;
  if (message == NULL)
    return;
  //SoupAddress *address = soup_message_get_address(message);
  //const char *host = soup_address_get_name(address);
  
  //const char *url = webkit_network(frame);
  //if (! adblock_domain_matches_uri("http://blub.bla/fjkl", host)) 
  //  puts("no");

  //DEBUG_TIMED(50000, adblock_domain_matches("blub.bli.bla", "bli.bla"))
  //DEBUG_TIMED(50000, adblock_domain_matches_uri("http://blub.bla/fjkl", host));

  SoupURI *suri = soup_message_get_uri(message);
  SoupURI *sfirst_party = soup_message_get_first_party(message);
  printf("%p %p\n", suri, sfirst_party);
  gboolean thirdparty = !soup_uri_host_equal(suri, sfirst_party);
  printf("%d\n", thirdparty);

  char *uri = soup_uri_to_string(suri, false);


  gboolean block = false;
  AdblockRule *r;
  for (int i=0; i<_rules->len; i++) {
    r = g_ptr_array_index(_rules, i);
    if (r->options & AO_REGEXP) {
      if (g_regex_match(r->pattern, uri, 0, NULL)) {
        block = true;
        break;
      }
    }
    else if (r->options & AO_SEPERATOR) {
    }
    else if (r->options & AO_WILDCARD) {
      if (!fnmatch(r->pattern, uri, 0)) {
        block = true;
        break;
      }
    }
    else {
      if (strstr(uri, r->pattern)) {
        block = true;
        break;
      }
    }
  }
  if (block) {
    webkit_network_request_set_uri(request, "about:blank");
  }
  //soup_message_set_uri(message, soup_uri_new("about:blank"));
  //  //char *content = soup_content_sniffer_sniff(content_sniffer, message, soup_buffer, &ht);
  //  //printf("request: %s\n", content);

  //}
  //else if (response) {
  //  //char *content = soup_content_sniffer_sniff(content_sniffer, message, soup_buffer, &ht);
  //  //printf("response: %s\n", content);

  //}

}
DwbStatus
adblock_rule_parse(char *pattern) {
  DwbStatus ret = STATUS_OK;
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
      char *domains = g_strndup(pattern, tmp-pattern);
      AdblockElementHider *hider = dwb_malloc(sizeof(AdblockElementHider));
      hider->domains = g_strsplit(domains, ",", -1);
      hider->css_rule = g_strdup(tmp+2);
      g_free(domains);
      g_ptr_array_add(_hider, hider);
    }
    /* general rules */
    else {
      g_string_append_printf(_css_rules, "%s{display:none!important;}", tmp + 2);
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
      int inverse;
      char *o;
      for (int i=0; options_arr[i] != NULL; i++) {
        inverse = 0;
        o = options_arr[i];
        /*  attributes */
        if (*o == '~') {
          inverse = 15;
          o++;
        }
        if (!strcmp(o, "script"))
          attributes |= (AA_SCRIPT << inverse);
        else if (!strcmp(o, "image"))
          attributes |= (AA_IMAGE << inverse);
        else if (!strcmp(o, "background")) {
          fprintf(stderr, "Not supported: adblock option 'background'\n");
          goto error_out;
        }
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
          attributes |= AA_OBJECT_SUBREQUEST;
        }
        else if (!strcmp(o, "dtd")) {
          fprintf(stderr, "Not supported: adblock option 'dtd'\n");
          goto error_out;
        }
        else if (!strcmp(o, "subdocument"))
          attributes |= (AA_SUBDOCUMENT << inverse);
        else if (!strcmp(o, "document")) 
          attributes |= (AA_DOCUMENT << inverse);
        else if (!strcmp(o, "elemhide"))
          attributes |= (AA_ELEMHIDE << inverse);
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
    //else {
    //  tmp = pattern;
    //}
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
      GError *error = NULL;
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
      option |= AO_REGEXP;
    }
    else {
      if (strchr(tmp, '^')) {
        option |= AO_SEPERATOR;
        GString *buffer = g_string_new(NULL);
        if (option & AO_BEGIN) {
          g_string_append_c(buffer, '^');
        }
        for (char *regexp_tmp = tmp; *regexp_tmp; regexp_tmp++ ) {
          switch (*regexp_tmp) {
            case '^' : g_string_append(buffer, "([\\x00-\\x24\\x26-\\x2C\\x2F\\x3A-\\x40\\x5B-\\x5E\\x60\\x7B-\\x80]|$)");
                      /*  skip anchor at end of pattern */
                       if (*(regexp_tmp+1) && *(regexp_tmp+1) == '|') { 
                         continue;
                       }
                       break;
            case '*' : g_string_append(buffer, ".*");
                       break;
            default: g_string_append_c(buffer, *regexp_tmp);
          }
        }
        if (option & AO_END) {
          g_string_append_c(buffer, '$');
        }
        if ( (option & AO_MATCH_CASE) != 0) 
          regex_flags &= ~G_REGEX_CASELESS;
        rule = g_regex_new(buffer->str, regex_flags, 0, NULL);
        g_string_free(buffer, true);
      }
      else {
        /* Skip leading and trailing wildcard */
        for (; *tmp == '*'; tmp++, length--);
        for (; tmp[length-1] == '*'; length--) 
          tmp[length-1] = '\0';
        rule = g_strdup_printf("%s%s%s", option & AO_BEGIN ? "" : "*", tmp, option & AO_END ? "" : "*");
      }
    }

    AdblockRule *adrule = adblock_rule_new();
    adrule->attributes = attributes;
    adrule->pattern = rule;
    adrule->options = option;
    adrule->domains = domains;
    if (attributes) {
      if (exception) 
        g_ptr_array_add(_exceptions, adrule);
      else 
        g_ptr_array_add(_rules, adrule);
    }
    else {
      if (exception) 
        g_ptr_array_add(_simple_exceptions, adrule);
      else 
        g_ptr_array_add(_simple_rules, adrule);
    }
  }
error_out:
  FREE(tmp_a);
  FREE(tmp_b);
  return ret;
}
void 
adblock_content_sniffed_cb(SoupMessage *msg, char *type, GHashTable *table, SoupSession *session) {
  AdblockAttribute attribute = 0;
  puts("sniffed");
  if (!strncmp(type, "image/", 6)) {
    attribute = AA_IMAGE;
  }
  else if (!fnmatch("*javascript*", type, FNM_NOESCAPE)) {
    attribute = AA_SCRIPT;
  }
  else if (!strcmp(type, "application/x-shockwave-flash") || !strcmp(type, "application/x-flv")) {
    attribute = AA_OBJECT_SUBREQUEST;
  }
  else if (!strcmp(type, "text/css")) {
    attribute = AA_STYLESHEET;
  }
  if (attribute)
    printf("%d\n", attribute);

}
void
adblock_request_started_cb(SoupSession *session, SoupMessage *msg, SoupSocket *socket) {
  g_signal_connect(msg, "content-sniffed", G_CALLBACK(adblock_content_sniffed_cb), session);
}

void
adblock_init() {
  _css_rules = g_string_new(NULL);
  _hider = g_ptr_array_new();
  _rules = g_ptr_array_new();
  _exceptions = g_ptr_array_new();
  _simple_rules = g_ptr_array_new();
  _simple_exceptions = g_ptr_array_new();

  char *content = util_get_file_content("easylist.txt");
  char **lines = g_strsplit(content, "\n", -1);
  for (int i=0; lines[i] != NULL; i++) {
    adblock_rule_parse(lines[i]);
  }
  g_strfreev(lines);
  g_free(content);
  g_signal_connect(webkit_get_default_session(), "request-started", G_CALLBACK(adblock_request_started_cb), NULL);
  content_sniffer = soup_content_sniffer_new();
}
