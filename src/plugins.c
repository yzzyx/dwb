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
#include "util.h"
#include "view.h"

#define ALLOWED(g)   (VIEW(g)->status->allowed_plugins)
#define PLUGIN_IMAGE_SIZE    "48px"

Plugins *
plugins_new() {
  Plugins *p = dwb_malloc(sizeof(Plugins));
  p->created = 0;
  p->clicks = NULL;
  p->status = 0;
  p->elements = NULL;
  p->max = 0;
  return p;
}
void 
plugins_free(Plugins *p) {
  if (p == NULL) 
    return;
  if (p->clicks != NULL) {
    for (GSList *l = p->clicks; l; l=l->next) 
      g_object_unref(l->data);
    g_slist_free(p->clicks);
  }
  if (p->elements != NULL) {
    for (GSList *l = p->elements; l; l=l->next) 
      g_object_unref(l->data);
    g_slist_free(p->elements);
  }
  FREE0(p);
}
static void 
plugins_onclick_cb(WebKitDOMElement *element, WebKitDOMEvent *event, GList *gl) {
  WebKitDOMElement *e = g_object_get_data((gpointer)element, "dwb-plugin-element");
  ALLOWED(gl) = g_slist_append(ALLOWED(gl), e);
  WebKitDOMNode *el = WEBKIT_DOM_NODE(webkit_dom_event_get_target(event));
  WebKitDOMNode *parent = webkit_dom_node_get_parent_node(el);
  WebKitDOMNode *child = webkit_dom_node_get_first_child(WEBKIT_DOM_NODE(element));
  webkit_dom_node_remove_child(WEBKIT_DOM_NODE(parent), child, NULL);
  webkit_dom_node_remove_child(parent, WEBKIT_DOM_NODE(element), NULL);
  webkit_dom_node_append_child(parent, WEBKIT_DOM_NODE(e), NULL);
  char *display_value = (char*)g_object_get_data(G_OBJECT(e), "dwb-plugin-display");
  if (display_value != NULL) {
    char *display = g_strdup_printf("display:%s;", display_value);
    webkit_dom_element_set_attribute(e, "style", display, NULL);
    g_free(display);
    g_free(display_value);
  }
  g_object_unref(el);
  g_object_unref(parent);
}

static char *
plugins_create_click_element(WebKitDOMElement *element, GList *gl) {
  WebKitDOMNode *parent = webkit_dom_node_get_parent_node(WEBKIT_DOM_NODE(element));
  View *v = VIEW(gl);
  WebKitDOMElement *div;

  if (parent != NULL) {
    WebKitDOMDocument *doc = webkit_dom_node_get_owner_document(WEBKIT_DOM_NODE(element));
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
  
    WebKitDOMCSSStyleDeclaration *style = webkit_dom_dom_window_get_computed_style(win, element, "");
    char *width = webkit_dom_css_style_declaration_get_property_value(style, "width");
    char *height = webkit_dom_css_style_declaration_get_property_value(style, "height");
    char *top = webkit_dom_css_style_declaration_get_property_value(style, "top");
    char *left = webkit_dom_css_style_declaration_get_property_value(style, "left");
    char *position = webkit_dom_css_style_declaration_get_property_value(style, "position");
    char *display = webkit_dom_css_style_declaration_get_property_value(style, "display");
    int w, h;
    if (sscanf(width, "%dpx", &w) == 1 && w<48) 
      width = PLUGIN_IMAGE_SIZE;
    if (sscanf(height, "%dpx", &h) == 1 && h<48) 
      height = PLUGIN_IMAGE_SIZE;
      

    if (v->plugins->max <= v->plugins->created) {
      div = webkit_dom_document_create_element(doc, "div", NULL);
      v->plugins->clicks = g_slist_prepend(v->plugins->clicks, div);
      v->plugins->max++;
    }
    else 
      div = g_slist_nth_data(v->plugins->clicks, v->plugins->created);
    webkit_dom_html_element_set_inner_html(WEBKIT_DOM_HTML_ELEMENT(div), 
        "<div style='display:table-cell;vertical-align:middle;text-align:center;color:#fff;background-color:#000;font:11px monospace bold'>click to play</div>", NULL);

    char *new_style = g_strdup_printf("position:%s;width:%s; height:%s; top: %s; left: %s;display:table;", position, width, height, top, left);
    webkit_dom_element_set_attribute(div, "style", new_style, NULL);
    g_free(new_style);

    webkit_dom_element_set_attribute(div, "onclick", "return", NULL);

    webkit_dom_node_remove_child(parent, WEBKIT_DOM_NODE(element), NULL);
    webkit_dom_node_append_child(parent, WEBKIT_DOM_NODE(div), NULL);
    v->plugins->elements = g_slist_prepend(v->plugins->elements, element);

    //* at least hide element if default behaviour cannot be prevented */
    g_object_set_data((gpointer)div, "dwb-plugin-element", element);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(div), "click", G_CALLBACK(plugins_onclick_cb), true, gl);
    g_object_unref(style);
    g_object_unref(parent);
    v->plugins->created++;
    return display;
  }
  return NULL;
}
static gboolean
plugins_before_load_cb(WebKitDOMDOMWindow *win, WebKitDOMEvent *event, GList *gl) {
  WebKitDOMElement *element = (void*)webkit_dom_event_get_src_element(event);
  char *tagname = webkit_dom_element_get_tag_name(element);
  char *type = webkit_dom_element_get_attribute(element, "type");

  if ( (!g_strcmp0(type, "application/x-shockwave-flash") 
        && (! g_ascii_strcasecmp(tagname, "object") || ! g_ascii_strcasecmp(tagname, "embed")) ) 
      && ! g_slist_find(ALLOWED(gl), element) ) {
    VIEW(gl)->plugins->status |= PLUGIN_STATUS_HAS_PLUGIN;
    webkit_dom_event_prevent_default(event);
    webkit_dom_event_stop_propagation(event);

    plugins_create_click_element(element, gl);
  }
  g_object_unref(element);
  g_free(tagname);
  g_free(type);
  return true;
}

static void 
plugins_remove_all(GList *gl) {
  if (ALLOWED(gl) != NULL) {
    ALLOWED(gl) = g_slist_remove_all(ALLOWED(gl), ALLOWED(gl)->data);
    ALLOWED(gl) = NULL;
  }
  View *v = VIEW(gl);
  v->plugins->created = 0;
  if (v->plugins->elements != NULL) {
    for (GSList *l = v->plugins->elements; l; l=l->next) {
      g_object_unref(l->data);
    }
    g_slist_free(v->plugins->elements);
    v->plugins->elements = NULL;
  }
}

void
plugins_window_object_cleared(WebKitWebView *wv, WebKitWebFrame *frame, gpointer *context, gpointer *window_object, GList *gl) {
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  if (frame != webkit_web_view_get_main_frame(wv)) {
    const char *src = webkit_web_frame_get_uri(frame);
    if (g_strcmp0(src, "about:blank")) {
      /* We have to find the correct frame, but there is no access from the web_frame
       * to the Htmlelement */
      WebKitDOMNodeList *frames = webkit_dom_document_query_selector_all(doc, "iframe, frame", NULL);
      for (guint i=0; i<webkit_dom_node_list_get_length(frames); i++) {
        WebKitDOMHTMLIFrameElement *iframe = (void*)webkit_dom_node_list_item(frames, i);
        char *iframesrc = webkit_dom_html_iframe_element_get_src(iframe);
        if (!g_strcmp0(src, iframesrc)) {
          WebKitDOMDOMWindow *win = webkit_dom_html_iframe_element_get_content_window(iframe);
          webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "beforeload", G_CALLBACK(plugins_before_load_cb), true, gl);
        }
        g_object_unref(iframe);
        g_free(iframesrc);
      }
      g_object_unref(frames);
    }
  }
  else {
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "beforeload", G_CALLBACK(plugins_before_load_cb), true, gl);
  }
}
void
plugins_load_status_cb(WebKitWebView *wv, GParamSpec *p, GList *gl) {
  WebKitLoadStatus status = webkit_web_view_get_load_status(wv);
  if (status == WEBKIT_LOAD_PROVISIONAL) {
    plugins_remove_all(gl);
  }
}

#if 0
void
plugins_frame_load_status_cb(WebKitWebFrame *frame, GList *gl) {
  WebKitWebView *wv = webkit_web_frame_get_web_view(frame);
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
}
void
plugins_frame_created_cb(WebKitWebView *wv, WebKitWebFrame *frame, GList *gl) {
  g_signal_connect(frame, "load-committed", G_CALLBACK(plugins_frame_load_status_cb), gl);
}
#endif

WebKitDOMElement *
plugins_find_in_frames(WebKitDOMDocument *doc, char *selector) {
  WebKitDOMElement *element = NULL;
  WebKitDOMDocument *document;
  WebKitDOMHTMLIFrameElement *iframe;
  WebKitDOMNodeList *list;
  char *source = NULL;

  /* TODO nodes with duplicate src/data-property */
  list = webkit_dom_document_get_elements_by_tag_name(doc, "object");
  for (guint i=0; i<webkit_dom_node_list_get_length(list); i++) {
    element = (void*)webkit_dom_node_list_item(list, i);
    source = webkit_dom_html_object_element_get_data(WEBKIT_DOM_HTML_OBJECT_ELEMENT(element));
    if (!g_strcmp0(selector, source)) {
      g_free(source);
      return element;
    }
    g_object_unref(element);
    g_free(source);
  }
  list = webkit_dom_document_get_elements_by_tag_name(doc, "embed");
  for (guint i=0; i<webkit_dom_node_list_get_length(list); i++) {
    element = (void*)webkit_dom_node_list_item(list, i);
    source = webkit_dom_html_embed_element_get_src(WEBKIT_DOM_HTML_EMBED_ELEMENT(element));
    if (!g_strcmp0(selector, source)) {
      g_free(source);
      return element;
    }
    g_object_unref(element);
    g_free(source);
  }
  list = webkit_dom_document_get_elements_by_tag_name(doc, "iframe");
  if (list != NULL) {
    for (guint i=0; i<webkit_dom_node_list_get_length(list); i++) {
      iframe = (void*)webkit_dom_node_list_item(list, i);
      document = webkit_dom_html_iframe_element_get_content_document(iframe);
      element = plugins_find_in_frames(document, selector);
      g_object_unref(iframe);
      g_object_unref(document);
      if (element != NULL)
        return element;
    }
  }
  return NULL;
}

GtkWidget * 
plugins_create_plugin_widget_cb(WebKitWebView *wv, char *mimetype, char *uri, GHashTable *param, GList *gl) {
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  WebKitDOMElement *element = plugins_find_in_frames(doc, uri);
  if (element  != NULL) {
    if ( !g_slist_find(ALLOWED(gl), element)) {
      VIEW(gl)->plugins->status |= PLUGIN_STATUS_HAS_PLUGIN;
      char *display = plugins_create_click_element(element, gl);
      webkit_dom_element_set_attribute(element, "style", "display:none!important", NULL);
      g_object_set_data((gpointer)element, "dwb-plugin-display", display);
    }
  }
  return NULL;
}
void 
plugins_connect(GList *gl) {
  View *v = VIEW(gl);
  if (v->plugins->status & PLUGIN_STATUS_CONNECTED) 
    return;

  v->status->signals[SIG_PLUGINS_LOAD] = g_signal_connect(WEBVIEW(gl), "notify::load-status", G_CALLBACK(plugins_load_status_cb), gl);
  v->status->signals[SIG_PLUGINS_WINDOW_OBJECT_CLEARED] = g_signal_connect(WEBVIEW(gl), "window-object-cleared", G_CALLBACK(plugins_window_object_cleared), gl);
  v->status->signals[SIG_PLUGINS_CREATE_WIDGET] = g_signal_connect(WEBVIEW(gl), "create-plugin-widget", G_CALLBACK(plugins_create_plugin_widget_cb), gl);
  v->plugins->status ^= (v->plugins->status & PLUGIN_STATUS_DISCONNECTED) | PLUGIN_STATUS_CONNECTED;
}

void 
plugins_disconnect(GList *gl) {
  View *v = VIEW(gl);
  if (v->plugins->status & PLUGIN_STATUS_DISCONNECTED) 
    return;

  for (int i=SIG_PLUGINS_FIRST+1; i<SIG_PLUGINS_LAST; i++) {
    if (VIEW(gl)->status->signals[i] > 0)  {
      g_signal_handler_disconnect(WEBVIEW(gl), VIEW(gl)->status->signals[i]);
      v->status->signals[i] = 0;
    }
  }
  v->plugins->status ^= (v->plugins->status & PLUGIN_STATUS_CONNECTED) | PLUGIN_STATUS_DISCONNECTED;
}
