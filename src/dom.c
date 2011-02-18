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

#include "dom.h"
// webview
#define get_document(wv)                              webkit_web_view_get_dom_document(wv)
#define DOCUMENT()                                    get_document(CURRENT_WEBVIEW())

// Window 
#define get_computed_style(win, element)      webkit_dom_dom_window_get_computed_style(win, WEBKIT_DOM_ELEMENT(element), "null")

// document
#define get_window(doc)                                     webkit_dom_document_get_default_view(doc)
#define WINDOW()                                            get_window(DOCUMENT())
#define document_query_selector_all(selector, gerror)  webkit_dom_document_query_selector_all(document, selector, gerror)
#define document_create_element(tagname, gerror)       webkit_dom_document_create_element(document, tagname, gerror)
// nodelist
#define node_list_item(list, i)                       webkit_dom_node_list_item(list, i)
#define node_list_get_length(list)                    webkit_dom_node_list_get_length(list, i)
// node 
#define node_append_child(node, child, error)         webkit_dom_node_append_child(WEBKIT_DOM_NODE(node), WEBKIT_DOM_NODE(child), error)

// element 
#define html_element_get_hidden(element)              webkit_dom_html_element_get_hidden(WEBKIT_DOM_HTML_ELEMENT(element))
// style 
#define style_get_property_value(style, property)     webkit_dom_css_style_declaration_get_property_value(style, property)
#define style_compare_property(style, name, value)    (!strcmp(style_get_property_value(style, name), value))
#define style_set_property_full(s, name, value, p, e)       webkit_dom_css_style_declaration_set_property(s, name, value, p, e)
#define style_set_property(s, name, value)       webkit_dom_css_style_declaration_set_property(s, name, value, "", NULL)

#define _(X) webkit_dom_##X

// Types
//
#define Document    WebKitDOMDocument
#define Body        WebKitDOMHTMLBodyElement
#define Head        WebKitDOMHTMLHeadElement
#define Window      WebKitDOMDOMWindow
#define NodeList    WebKitDOMNodeList
#define Node        WebKitDOMNode
#define Style       WebKitDOMCSSStyleDeclaration
#define HtmlElement WebKitDOMHTMLElement
#define Element     WebKitDOMElement

#define NODE(X)    WebKitDOMNode(X)
static Document *document;
static Window *window;
static Node *hints;
static Body *body;
//static Head *head;

gboolean 
dwb_dom_get_visible(Node *node) {
  if (html_element_get_hidden(node)) {
    return false;
  }
  Style *style = get_computed_style(window, node);
  if (style_compare_property(style, "display", "none"))
    return  false;
  if (style_compare_property(style, "visibility", "hidden"))
    return  false;

  return true;
}
void
dwb_dom_style_set_property(Style *style, const char *property, const char *format, ...) {
  va_list args;
  va_start(args, format);
  char *value = g_strdup_vprintf(format, args);
  style_set_property(style, property, value);
  g_free(value);
}

void 
dwb_dom_create_hint(Node *node) {
  Element *span = document_create_element("span", NULL);
  Element *content = webkit_dom_document_create_text_node(document, "blub");
  node_append_child(span, content, NULL);
  node_append_child(hints, span, NULL);
  node_append_child(body, hints, NULL);
  int leftpos = webkit_dom_element_get_client_left(node);
  int toppos = webkit_dom_element_get_client_left(node);
  Style *s = webkit_dom_element_get_style(span);

  style_set_property(s, "position", "absolute");
  dwb_dom_style_set_property(s, "left", "%dpx", leftpos);
  dwb_dom_style_set_property(s, "top",  "%dpx", toppos);
  style_set_property(s, "background", "#00ff00");
  

}

void 
dwb_dom_show_hints() {
  document = DOCUMENT();
  body = webkit_dom_document_get_body(document);
  window     = get_window(document);
  hints = document_create_element("div", NULL);
  NodeList *nodes    = document_query_selector_all("a, input", NULL);
  for (int i=0; i<webkit_dom_node_list_get_length(nodes); i++) {
    Node *node = node_list_item(nodes, i);
    if (dwb_dom_get_visible(node) ) {
      dwb_dom_create_hint(node);
    }
  }


}
