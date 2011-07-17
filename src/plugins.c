#include "plugins.h"
#include "util.h"

#define ALLOWED(g)   (VIEW(g)->plugins->allowed)
#define PLUGINS(g)   (VIEW(g)->plugins->elements)
#define PLUGIN_IMAGE_SIZE    "48px"

static gboolean
dwb_plugins_before_load_cb(WebKitDOMDOMWindow *win, WebKitDOMEvent *event, GList *gl) {
  WebKitDOMNode *element = WEBKIT_DOM_NODE(webkit_dom_event_get_target(event));
  gchar *tagname = webkit_dom_node_get_node_name(element);
  char *type = webkit_dom_element_get_attribute(element, "type");

  if ( (!strcmp(type, "application/x-shockwave-flash") 
      && (! g_ascii_strcasecmp(tagname, "object") || ! g_ascii_strcasecmp(tagname, "embed")) ) 
      && ! g_slist_find(VIEW(gl)->plugins->allowed, element) ) {
    PLUGINS(gl) = g_slist_append(PLUGINS(gl), element);
    webkit_dom_event_prevent_default(event);
  }
  return true;
}
static void 
dwb_onclick_cb(WebKitDOMElement *element, WebKitDOMEvent *event, GList *gl) {
  WebKitDOMElement *e = g_object_get_data((gpointer)element, "dwb-plugin-element");
  ALLOWED(gl) = g_slist_append(ALLOWED(gl), e);
  WebKitDOMNode *el = WEBKIT_DOM_NODE(webkit_dom_event_get_target(event));
  WebKitDOMNode *parent = webkit_dom_node_get_parent_node(el);
  webkit_dom_node_remove_child(parent, WEBKIT_DOM_NODE(element), NULL);
  webkit_dom_node_append_child(parent, WEBKIT_DOM_NODE(e), NULL);
}

static void
dwb_plugins_create_click_element(WebKitDOMElement *element, GList *gl) {
  WebKitDOMNode *parent = webkit_dom_node_get_parent_node(WEBKIT_DOM_NODE(element));

  if (parent != NULL) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(WEBVIEW(gl));
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
  
    WebKitDOMCSSStyleDeclaration *style = webkit_dom_dom_window_get_computed_style(win, element, "");
    char *width = webkit_dom_css_style_declaration_get_property_value(style, "width");
    char *height = webkit_dom_css_style_declaration_get_property_value(style, "height");
    char *top = webkit_dom_css_style_declaration_get_property_value(style, "top");
    char *left = webkit_dom_css_style_declaration_get_property_value(style, "left");
    int w, h;
    if (sscanf(width, "%dpx", &w) == 1 && w<48) 
      width = PLUGIN_IMAGE_SIZE;
    if (sscanf(height, "%dpx", &h) == 1 && h<48) 
      height = PLUGIN_IMAGE_SIZE;
      
    WebKitDOMElement *div = webkit_dom_document_create_element(doc, "div", NULL);

    char *new_style = g_strdup_printf("width:%s; height:%s; top: %s; left: %s;%s", width, height, top, left, dwb.misc.pbbackground);
    webkit_dom_element_set_attribute(div, "style", new_style, NULL);
    webkit_dom_element_set_attribute(div, "onclick", "return", NULL);

    webkit_dom_node_remove_child(parent, WEBKIT_DOM_NODE(element), NULL);
    webkit_dom_node_append_child(parent, WEBKIT_DOM_NODE(div), NULL);

    g_object_set_data((gpointer)div, "dwb-plugin-element", element);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(div), "click", G_CALLBACK(dwb_onclick_cb), false, gl);

    g_free(new_style);
  }
}

static void 
dwb_plugins_remove_all(GList *gl) {
  if (VIEW(gl)->plugins) {
    if (PLUGINS(gl) != NULL) {
      PLUGINS(gl) = g_slist_remove_all(PLUGINS(gl), PLUGINS(gl)->data);
      PLUGINS(gl) = NULL;
    }
    if (ALLOWED(gl) != NULL) {
      ALLOWED(gl) = g_slist_remove_all(ALLOWED(gl), ALLOWED(gl)->data);
      ALLOWED(gl) = NULL;
    }
  }
}

void
dwb_plugins_load_status_cb(WebKitWebView *wv, GParamSpec *p, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(wv);
  if (status == WEBKIT_LOAD_PROVISIONAL) {
    dwb_plugins_remove_all(gl);
  }
  else if (status ==  WEBKIT_LOAD_COMMITTED) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "beforeload", G_CALLBACK(dwb_plugins_before_load_cb), true, gl);
  }
  else if (status == WEBKIT_LOAD_FINISHED) {
    g_slist_foreach(PLUGINS(gl), (GFunc) dwb_plugins_create_click_element, gl);
  }
}

void 
dwb_plugin_blocker_connect(GList *gl) {
  VIEW(gl)->status->signals[SIG_PLUGIN_BLOCKER] = g_signal_connect(WEBVIEW(gl), "notify::load-status", G_CALLBACK(dwb_plugins_load_status_cb), gl);
  Plugins *p = malloc(sizeof(Plugins));
  p->allowed = NULL;
  p->elements = NULL;
  VIEW(gl)->plugins = p;
}

void
dwb_plugin_blocker_init() {
  for (GList *l = dwb.state.views; l; l=l->next) {
    dwb_plugin_blocker_connect(l);
  }
}


void
dwb_plugin_blocker_uninit() {
  for (GList *l = dwb.state.views; l; l=l->next) {
    if (VIEW(l)->status->signals[SIG_PLUGIN_BLOCKER] > 0) {
      g_signal_handler_disconnect(WEBVIEW(l), VIEW(l)->status->signals[SIG_PLUGIN_BLOCKER]);
    }
    dwb_plugins_remove_all(l);
    g_free(VIEW(l)->plugins);
    VIEW(l)->plugins = NULL;
  }
}
