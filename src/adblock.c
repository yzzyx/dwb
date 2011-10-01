#include "dwb.h"
#include "util.h"

#define BUFFER_SIZE 4096
#define LINE_SIZE 1024
typedef enum _AdblockOption {
  AO_WILDCARD           = 1<<0,
  AO_SEPERATOR          = 1<<1,
  AO_BEGIN              = 1<<2,
  AO_BEGIN_DOMAIN       = 1<<3,
  AO_END                = 1<<4,
  AO_REGEXP             = 1<<6,
  AO_MATCH_CASE          = 1<<7,
  /*  Attributes */
} AdblockOption;
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
  AA_THIRDPARTY         = 1<<14,
  /* inverse */
} AdblockAttribute;


typedef struct _AdblockRule {
  void *pattern;
  AdblockOption options;
  char **domains;
} AdblockRule;

typedef struct _AdblockElementHider {
  const char *css_rule;
  char **domains;
} AdblockElementHider;

static GPtrArray *_hider;
static GPtrArray *_rules;
static GPtrArray *_exceptions;
static GString *_css_rules;


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
DwbStatus
adblock_rule_parse(char *pattern) {
  DwbStatus ret = STATUS_OK;
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
      tmp_a = g_strndup(pattern, options - pattern);
      char **options_arr = g_strsplit(options+1, ",", -1);
      int inverse;
      for (int i=0; options_arr[i] != NULL; i++) {
        inverse = 0;
        /*  attributes */
        if (*options_arr[i] == '~') 
          inverse = 16;
        if (!strcmp(options_arr[i], "script"))
          attributes |= (AA_SCRIPT << inverse);
        else if (!strcmp(options_arr[i], "image"))
          attributes |= (AA_IMAGE << inverse);
        else if (!strcmp(options_arr[i], "background"))
          fprintf(stderr, "Not supported: adblock option 'background'\n");
        else if (!strcmp(options_arr[i], "stylesheet"))
          attributes |= (AA_STYLESHEET << inverse);
        else if (!strcmp(options_arr[i], "object"))
          attributes |= (AA_OBJECT << inverse);
        else if (!strcmp(options_arr[i], "xbl"))
          fprintf(stderr, "Not supported: adblock option 'xbl'\n");
        else if (!strcmp(options_arr[i], "ping"))
          fprintf(stderr, "Not supported: adblock option 'xbl'\n");
        else if (!strcmp(options_arr[i], "xmlhttprequest"))
          fprintf(stderr, "Not supported: adblock option 'xmlhttprequest'\n");
        else if (!strcmp(options_arr[i], "object-subrequest"))
          fprintf(stderr, "Not supported: adblock option 'object-subrequest'\n");
        else if (!strcmp(options_arr[i], "dtd"))
          fprintf(stderr, "Not supported: adblock option 'dtd'\n");
        else if (!strcmp(options_arr[i], "subdocument"))
          attributes |= (AA_SUBDOCUMENT << inverse);
        else if (!strcmp(options_arr[i], "document"))
          attributes |= (AA_DOCUMENT << inverse);
        else if (!strcmp(options_arr[i], "elemhide"))
          attributes |= (AA_ELEMHIDE << inverse);
        else if (!strcmp(options_arr[i], "other"))
          fprintf(stderr, "Not supported: adblock option 'other'\n");
        else if (!strcmp(options_arr[i], "collapse"))
          fprintf(stderr, "Not supported: adblock option 'collapse'\n");
        else if (!strcmp(options_arr[i], "donottrack"))
          fprintf(stderr, "Not supported: adblock option 'donottrack'\n");
        else if (!strcmp(options_arr[i], "match-case"))
          option |= AO_MATCH_CASE;
        else if (g_str_has_prefix(options_arr[i], "domain=")) {
          domains = g_strsplit(options_arr[i] + 7, "|", -1);
        }
      }
      tmp = tmp_a;
      g_strfreev(options_arr);
    }
    else {
      tmp = pattern;
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
      GError *error = NULL;
      tmp_c = g_strndup(tmp+1, length-2);
      rule = g_regex_new(tmp_c, 0, 0, &error);
      if (error != NULL) {
        fprintf(stderr, "dwb warning: ignoring adblock rule %s: %s\n", pattern, error->message);
        ret = STATUS_ERROR;
        g_clear_error(&error);
        goto error_out;
      }
      option |= AO_REGEXP;
    }
    else {
      if (strchr(tmp, '*')) {
        if (tmp[length-1] == '*') 
          tmp[length-1] = '\0';
        else 
          option |= AO_WILDCARD;
      }
      if (strchr(tmp, '^')) {
        option |= AO_SEPERATOR;
      }
      //printf("%d: %s%s%s\n", option & (AO_WILDCARD | AO_SEPERATOR), option & AO_WILDCARD ? "wildcard " : "", option & AO_SEPERATOR & AO_SEPERATOR ? "seperator " : "", tmp);


      rule = g_strdup(tmp);
    }

    AdblockRule *adrule = adblock_rule_new();
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
}

void
adblock_init() {
  _css_rules = g_string_new(NULL);
  _hider = g_ptr_array_new();
  _rules = g_ptr_array_new();
  _exceptions = g_ptr_array_new();

  char *content = util_get_file_content("easylist.txt");
  char **lines = g_strsplit(content, "\n", -1);
  for (int i=0; lines[i] != NULL; i++) {
    adblock_rule_parse(lines[i]);
  }
  g_strfreev(lines);
  g_free(content);
}
