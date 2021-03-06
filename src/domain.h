/*
 * Copyright (c) 2010-2013 Stefan Bolte <portix@gmx.net>
 * Copyright (c) 2013 Elias Norberg <xyzzy@kudzu.se>
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


#ifndef DOMAIN_H
#define DOMAIN_H

#define SUBDOMAIN_MAX 32

void domain_init(void);
void domain_end(void);

GSList * domain_get_cookie_domains(WebKitWebView *wv);
gboolean domain_match(char **, const char *, const char *);
const char * domain_get_base_for_host(const char *host);
const char * domain_get_tld(const char *domain);
#endif
