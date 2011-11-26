#include "plugins.h"
#include "util.h"
#include "view.h"

#define ALLOWED(g)   (VIEW(g)->status->allowed_plugins)
#define PLUGIN_IMAGE_SIZE    "48px"

static void 
plugins_onclick_cb(WebKitDOMElement *element, WebKitDOMEvent *event, GList *gl) {
  WebKitDOMElement *e = g_object_get_data((gpointer)element, "dwb-plugin-element");
  ALLOWED(gl) = g_slist_append(ALLOWED(gl), e);
  WebKitDOMNode *el = WEBKIT_DOM_NODE(webkit_dom_event_get_target(event));
  WebKitDOMNode *parent = webkit_dom_node_get_parent_node(el);
  webkit_dom_node_remove_child(parent, WEBKIT_DOM_NODE(element), NULL);
  webkit_dom_node_append_child(parent, WEBKIT_DOM_NODE(e), NULL);
  char *display_value = (char*)g_object_get_data(G_OBJECT(e), "dwb-plugin-display");
  if (display_value != NULL) {
    char *display = g_strdup_printf("display:%s;", display_value);
    webkit_dom_element_set_attribute(e, "style", display, NULL);
    g_free(display);
  }
}

static char *
plugins_create_click_element(WebKitDOMElement *element, GList *gl) {
  WebKitDOMNode *parent = webkit_dom_node_get_parent_node(WEBKIT_DOM_NODE(element));

  if (parent != NULL) {
    WebKitDOMDocument *doc = webkit_dom_node_get_owner_document(WEBKIT_DOM_NODE(element));
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
  
    WebKitDOMCSSStyleDeclaration *style = webkit_dom_dom_window_get_computed_style(win, element, "");
    char *width = webkit_dom_css_style_declaration_get_property_value(style, "width");
    char *height = webkit_dom_css_style_declaration_get_property_value(style, "height");
    char *top = webkit_dom_css_style_declaration_get_property_value(style, "top");
    char *left = webkit_dom_css_style_declaration_get_property_value(style, "left");
    char *display = webkit_dom_css_style_declaration_get_property_value(style, "display");
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
    /* at least hide element if default behaviour cannot be prevented */

    g_object_set_data((gpointer)div, "dwb-plugin-element", element);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(div), "click", G_CALLBACK(plugins_onclick_cb), false, gl);

    g_free(new_style);
    return display;
  }
  return NULL;
}
static gboolean
plugins_before_load_cb(WebKitDOMDOMWindow *win, WebKitDOMEvent *event, GList *gl) {
  WebKitDOMElement *element = (void*)webkit_dom_event_get_src_element(event);
  gchar *tagname = webkit_dom_element_get_tag_name(element);
  char *type = webkit_dom_element_get_attribute(element, "type");

  if ( (!strcmp(type, "application/x-shockwave-flash") 
      && (! g_ascii_strcasecmp(tagname, "object") || ! g_ascii_strcasecmp(tagname, "embed")) ) 
      && ! g_slist_find(ALLOWED(gl), element) ) {
    VIEW(gl)->status->pb_status |= PLUGIN_STATUS_HAS_PLUGIN;
    webkit_dom_event_prevent_default(event);
    webkit_dom_event_stop_propagation(event);

    plugins_create_click_element(element, gl);
  }
  g_free(tagname);
  return true;
}

static void 
plugins_remove_all(GList *gl) {
  if (ALLOWED(gl) != NULL) {
    ALLOWED(gl) = g_slist_remove_all(ALLOWED(gl), ALLOWED(gl)->data);
    ALLOWED(gl) = NULL;
  }
}

void
plugins_load_status_cb(WebKitWebView *wv, GParamSpec *p, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(wv);
  if (status == WEBKIT_LOAD_PROVISIONAL) {
    plugins_remove_all(gl);
  }
  else if (status ==  WEBKIT_LOAD_COMMITTED) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "beforeload", G_CALLBACK(plugins_before_load_cb), true, gl);
  }
}

void
plugins_frame_load_status_cb(WebKitWebFrame *frame, GList *gl) {
  WebKitWebView *wv = webkit_web_frame_get_web_view(frame);
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  const char *src = webkit_web_frame_get_uri(frame);

  if (strcmp(src, "about:blank")) {
    /* We have to find the correct frame, but there is no access from the web_frame
     * to the Htmlelement */
    WebKitDOMNodeList *frames = webkit_dom_document_query_selector_all(doc, "iframe, frame", NULL);
    for (int i=0; i<webkit_dom_node_list_get_length(frames); i++) {
      WebKitDOMHTMLIFrameElement *iframe = (void*)webkit_dom_node_list_item(frames, i);
      char *iframesrc = webkit_dom_html_iframe_element_get_src(iframe);
      if (!strcmp(src, iframesrc)) {
        WebKitDOMDOMWindow *win = webkit_dom_html_iframe_element_get_content_window(iframe);
        webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "beforeload", G_CALLBACK(plugins_before_load_cb), true, gl);
      }
    }
  }
}
void
plugins_frame_created_cb(WebKitWebView *wv, WebKitWebFrame *frame, GList *gl) {
  g_signal_connect(frame, "load-committed", G_CALLBACK(plugins_frame_load_status_cb), gl);
}

WebKitDOMElement *
plugins_find_in_frames(WebKitDOMDocument *doc, char *selector) {
  WebKitDOMElement *element = NULL;
  WebKitDOMDocument *document;
  WebKitDOMHTMLIFrameElement *iframe;
  WebKitDOMNodeList *list;
  char *source;

  /* TODO nodes with duplicate src/data-property */
  list = webkit_dom_document_get_elements_by_tag_name(doc, "object");
  for (int i=0; i<webkit_dom_node_list_get_length(list); i++) {
    element = (void*)webkit_dom_node_list_item(list, i);
    source = webkit_dom_html_object_element_get_data(WEBKIT_DOM_HTML_OBJECT_ELEMENT(element));
    if (!strcmp(selector, source)) 
      return element;
  }
  list = webkit_dom_document_get_elements_by_tag_name(doc, "embed");
  for (int i=0; i<webkit_dom_node_list_get_length(list); i++) {
    element = (void*)webkit_dom_node_list_item(list, i);
    source = webkit_dom_html_embed_element_get_src(WEBKIT_DOM_HTML_EMBED_ELEMENT(element));
    if (!strcmp(selector, source)) 
      return element;
  }
  WebKitDOMNodeList *frames = webkit_dom_document_get_elements_by_tag_name(doc, "iframe");
  if (frames != NULL) {
    for (int i=0; i<webkit_dom_node_list_get_length(frames); i++) {
      iframe = (void*)webkit_dom_node_list_item(frames, i);
      document = webkit_dom_html_iframe_element_get_content_document(iframe);
      if ((element = plugins_find_in_frames(document, selector)) != NULL)
        return element;
    }
  }
  return NULL;
}

GtkWidget * 
plugins_create_plugin_widget_cb(WebKitWebView *wv, char *mimetype, char *uri, GHashTable *param, GList *gl) {
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  WebKitDOMElement *element;
  if ( (element = plugins_find_in_frames(doc, uri)) != NULL && !g_slist_find(ALLOWED(gl), element)) {
    VIEW(gl)->status->pb_status |= PLUGIN_STATUS_HAS_PLUGIN;
    char *display = plugins_create_click_element(element, gl);
    webkit_dom_element_set_attribute(element, "style", "display:none!important", NULL);
    g_object_set_data((gpointer)element, "dwb-plugin-display", display);
  }
  return NULL;
}
void 
plugins_connect(GList *gl) {
  View *v = VIEW(gl);
  if (v->status->pb_status & PLUGIN_STATUS_CONNECTED) 
    return;

  v->status->signals[SIG_PLUGINS_LOAD] = g_signal_connect(WEBVIEW(gl), "notify::load-status", G_CALLBACK(plugins_load_status_cb), gl);
  v->status->signals[SIG_PLUGINS_FRAME_LOAD] = g_signal_connect(WEBVIEW(gl), "frame-created", G_CALLBACK(plugins_frame_created_cb), gl);
  v->status->signals[SIG_PLUGINS_CREATE_WIDGET] = g_signal_connect(WEBVIEW(gl), "create-plugin-widget", G_CALLBACK(plugins_create_plugin_widget_cb), gl);
  v->status->pb_status ^= (v->status->pb_status & PLUGIN_STATUS_DISCONNECTED) | PLUGIN_STATUS_CONNECTED;
}

void 
plugins_disconnect(GList *gl) {
  View *v = VIEW(gl);
  if (v->status->pb_status & PLUGIN_STATUS_DISCONNECTED) 
    return;

  for (int i=SIG_PLUGINS_LOAD; i<SIG_PLUGINS_LAST; i++) {
    if (VIEW(gl)->status->signals[i] > 0)  {
      g_signal_handler_disconnect(WEBVIEW(gl), VIEW(gl)->status->signals[i]);
      v->status->signals[i] = 0;
    }
  }
  v->status->pb_status ^= (v->status->pb_status & PLUGIN_STATUS_CONNECTED) | PLUGIN_STATUS_DISCONNECTED;
}
