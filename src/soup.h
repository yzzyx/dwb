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

#ifndef COOKIES_H
#define COOKIES_H

#include "dwb.h"
#include "util.h"

DwbStatus dwb_soup_set_cookie_accept_policy(const char *);
void dwb_soup_cookies_set_accept_policy(Arg *);
void dwb_soup_save_cookies(GSList *);
void dwb_soup_init_cookies(SoupSession *);
void dwb_soup_init_proxy();
void dwb_soup_init_session_features();
void dwb_soup_init();

#endif
