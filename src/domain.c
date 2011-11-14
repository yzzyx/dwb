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
#ifdef DWB_DOMAIN_SERVICE
#include <glib-2.0/glib.h>
#include <string.h>
#include "util.h"
#define SUBDOMAIN_MAX 32

static GHashTable *_tld_table;
static char **_effective_tlds;

char **
domain_get_subdomains_for_host(const char *host) {
  int n=0;
  GSList *list = NULL;
  list = g_slist_append(list, g_strdup(host));
  n++;
  const char *cur_domain = host;
  const char *prev_domain = host;
  const char *pprev_domain = host;
  const char *ret = NULL;
  char *nextdot = strchr(cur_domain, '.');
  char *entry = NULL;
  while (1) {
    entry = g_hash_table_lookup(_tld_table, cur_domain);
    if (entry != NULL) {
      if (*entry == '*') {
        ret = pprev_domain;
        break;
      }
      else if (*entry == '!' && nextdot) {
        ret = nextdot + 1;
        break;
      }
      else {
        ret = prev_domain;
        break;
      }
    }
    if (nextdot == NULL)
      break;
    pprev_domain = prev_domain;
    prev_domain = cur_domain;
    cur_domain = nextdot + 1;
    list = g_slist_append(list, g_strdup(cur_domain));
    n++;
    nextdot = strchr(cur_domain, '.');
  }
  char **rets = calloc(n+1, sizeof(char *));
  rets[n] = NULL;
  int i=0;
  for (GSList *l = list; l; l = l->next, i++)
    rets[i] = l->data;
  g_slist_free(list);
  return rets;
}
const char *
domain_get_base_for_host(const char *host) {
  const char *cur_domain = host;
  const char *prev_domain = host;
  const char *pprev_domain = host;
  const char *ret = NULL;
  char *nextdot = strchr(cur_domain, '.');
  char *entry = NULL;
  while (1) {
    entry = g_hash_table_lookup(_tld_table, cur_domain);
    if (entry != NULL) {
      if (*entry == '*') {
        ret = pprev_domain;
        break;
      }
      else if (*entry == '!' && nextdot) {
        ret = nextdot + 1;
        break;
      }
      else {
        ret = prev_domain;
        break;
      }
    }
    if (nextdot == NULL)
      break;
    pprev_domain = prev_domain;
    prev_domain = cur_domain;
    cur_domain = nextdot + 1;
    nextdot = strchr(cur_domain, '.');
  }
  return ret;
}
void
domain_end() {
  if (_tld_table)
    g_hash_table_unref(_tld_table);
  if (_effective_tlds) {
    g_strfreev(_effective_tlds);
  }
}

void 
domain_init() {
  _tld_table = g_hash_table_new((GHashFunc)g_str_hash, (GEqualFunc)g_str_equal);
  char *eff_tld;

  char *tld_file = util_get_data_file("tlds.txt");
  char *content = util_get_file_content(tld_file);
  _effective_tlds = g_strsplit(content, "\n", -1);
  for (int i=0; (eff_tld = _effective_tlds[i]); i++) {
    if (*eff_tld == '*' || *eff_tld == '!') 
      eff_tld++;
    if (*eff_tld == '.')
      eff_tld++;
    g_hash_table_insert(_tld_table, eff_tld, _effective_tlds[i]);
  }
  g_free(tld_file);
}
#endif
