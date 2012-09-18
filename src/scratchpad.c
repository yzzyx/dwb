#include "dwb.h"

GtkWidget *g_scratchpad;
static gboolean
navigation_cb(WebKitWebView *wv, WebKitWebFrame *frame, WebKitNetworkRequest *request, 
    WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision) {
  const char *uri = webkit_network_request_get_uri(request);
  if (g_strcmp0("scratchpad", uri)) {
    webkit_web_policy_decision_ignore(decision);
    return true;
  }
  return false;
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


