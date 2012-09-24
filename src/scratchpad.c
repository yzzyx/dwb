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

GtkWidget *g_scratchpad;
static gboolean
navigation_cb(WebKitWebView *wv, WebKitWebFrame *frame, WebKitNetworkRequest *request, 
    WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision) {
  const char *uri = webkit_network_request_get_uri(request);
  if (g_strcmp0("scratchpad", uri) && ! g_str_has_prefix(uri, "file://")) {
    webkit_web_policy_decision_ignore(decision);
    return true;
  }
  return false;
}

void
scratchpad_load(const char *text) {
  if (g_str_has_prefix(text, "file://")) 
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(g_scratchpad), text);
  else 
    webkit_web_view_load_string(WEBKIT_WEB_VIEW(g_scratchpad), text, NULL, NULL, "scratchpad");
}

void 
scratchpad_show(void) {
  gtk_widget_show(g_scratchpad);
  gtk_widget_grab_focus(g_scratchpad);
}
void 
scratchpad_hide(void) {
  gtk_widget_hide(g_scratchpad);
  dwb_focus_scroll(dwb.state.fview);
}
static void
scratchpad_init(void) {
  g_scratchpad = webkit_web_view_new();
  g_signal_connect(g_scratchpad, "navigation-policy-decision-requested", G_CALLBACK(navigation_cb), NULL);
  gtk_widget_set_size_request(g_scratchpad, -1, 200);
}
GtkWidget * 
scratchpad_get(void) {
  if (g_scratchpad == NULL)
    scratchpad_init();
  return g_scratchpad;
}


