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

#include "dwb.h"
#include "tabs.h"

#define TAB_VISIBLE(gl) (VIEW((gl))->status->group & dwb.state.current_groups)

static gint 
count_visible() {
  int n=0;
  for (GList *gl = dwb.state.views; gl; gl=gl->next) {
    if (TAB_VISIBLE(gl))
      n++;
  }
  return n;
}
GList *
tab_next(GList *gl, gint steps, gboolean skip)
{
    g_return_val_if_fail(gl != NULL, dwb.state.fview);

    GList *n, *last = gl;
    gint l, i;

    if (!dwb.state.views->next)
        return gl;

    l = count_visible(dwb.state.views);
    steps %= l;

    for (i=0, n=gl->next; i<steps && n; n=n->prev)
    {
        if (n == gl)
            return gl;
        if (n == NULL) 
        {
            if (skip)
                n = dwb.state.views;
            else 
                return last;
        }
        if (TAB_VISIBLE(n)) {
            i++;
            last = n;
        }
    }
    return n;
}
GList *
tab_prev(GList *gl, gint steps, gboolean skip)
{
    g_return_val_if_fail(gl != NULL, dwb.state.fview);

    GList *p, *last = gl;
    gint l, i;

    if (!dwb.state.views->next)
        return gl;

    l = count_visible(dwb.state.views);
    steps %= l;

    for (i=0, p=gl->prev; i<steps && p; p=p->prev)
    {
        if (p == gl)
            return gl;
        if (p == NULL) 
        {
            if (skip) 
                p = g_list_last(dwb.state.views);
            else 
                return last;
        }
        if (TAB_VISIBLE(p)) 
        {
            i++;
            last = p;
        }
    }

    return p;
}
int 
tab_position(GList *gl)
{
    int n=0;

    if (dwb.state.current_groups == 0)
        return g_list_position(dwb.state.views, gl);

    for (GList *l = dwb.state.views; l; l=l->next) 
    {
        if (TAB_VISIBLE(l)) 
        {
            if (l == gl)
                return n;
            n++;
        }
    }
    return -1;
}
void 
tab_update(GList *gl)
{
    if (TAB_VISIBLE(gl))
        tab_show(gl);
    else 
        tab_hide(gl);
}
void 
tab_show(GList *gl)
{

}
void 
tab_hide(GList *gl)
{
}
