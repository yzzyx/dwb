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

Plugins *
plugins_new() 
{
    Plugins *p = dwb_malloc(sizeof(Plugins));
    p->created = 0;
    p->clicks = NULL;
    p->status = 0;
    p->elements = NULL;
    p->max = 0;
    return p;
}
void 
plugins_free(Plugins *p) 
{
    if (p == NULL) 
        return;

    if (p->clicks != NULL) 
    {
        for (GSList *l = p->clicks; l; l=l->next) 
            g_object_unref(l->data);

        g_slist_free(p->clicks);
    }
    if (p->elements != NULL) 
    {
        for (GSList *l = p->elements; l; l=l->next) 
            g_object_unref(l->data);

        g_slist_free(p->elements);
    }
    FREE0(p);
}
static void 
plugins_onclick_cb(WebKitDOMElement *element, WebKitDOMEvent *event, GList *gl) 
{
    WebKitDOMElement *e = g_object_get_data((gpointer)element, "dwb-plugin-element");
    ALLOWED(gl) = g_slist_append(ALLOWED(gl), e);
    WebKitDOMNode *parent = webkit_dom_node_get_parent_node(WEBKIT_DOM_NODE(element));
    WebKitDOMNode *child = webkit_dom_node_get_first_child(WEBKIT_DOM_NODE(element));
    webkit_dom_node_remove_child(WEBKIT_DOM_NODE(parent), child, NULL);
    webkit_dom_node_remove_child(parent, WEBKIT_DOM_NODE(element), NULL);
    webkit_dom_node_append_child(parent, WEBKIT_DOM_NODE(e), NULL);
    g_object_unref(parent);
}

void
plugins_create_click_element(WebKitDOMElement *element, GList *gl) 
{
    WebKitDOMNode *parent = webkit_dom_node_get_parent_node(WEBKIT_DOM_NODE(element));
    View *v = VIEW(gl);
    WebKitDOMElement *div;

    if (parent == NULL) 
        return;

    WebKitDOMDocument *doc = webkit_dom_node_get_owner_document(WEBKIT_DOM_NODE(element));
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);

    WebKitDOMCSSStyleDeclaration *style = webkit_dom_dom_window_get_computed_style(win, element, "");
    char *width = webkit_dom_css_style_declaration_get_property_value(style, "width");
    char *height = webkit_dom_css_style_declaration_get_property_value(style, "height");
    char *top = webkit_dom_css_style_declaration_get_property_value(style, "top");
    char *left = webkit_dom_css_style_declaration_get_property_value(style, "left");
    char *position = webkit_dom_css_style_declaration_get_property_value(style, "position");
    int w, h;
    if (sscanf(width, "%dpx", &w) == 1 && w<72) 
        w = 72;
    if (sscanf(height, "%dpx", &h) == 1 && h<24) 
        h = 24;


    if (v->plugins->max <= v->plugins->created) 
    {
        div = webkit_dom_document_create_element(doc, "div", NULL);
        v->plugins->clicks = g_slist_prepend(v->plugins->clicks, div);
        v->plugins->max++;
    }
    else 
        div = g_slist_nth_data(v->plugins->clicks, v->plugins->created);
    webkit_dom_html_element_set_inner_html(WEBKIT_DOM_HTML_ELEMENT(div), 
            "<div style='display:table-cell;vertical-align:middle;text-align:center;color:#fff;background:#000;border:1px solid #666;font:11px monospace bold'>click to enable flash</div>", NULL);

    char *new_style = g_strdup_printf("position:%s;width:%dpx;height:%dpx;top:%s;left:%s;display:table;z-index:37000;", position, w, h, top, left);
    webkit_dom_element_set_attribute(div, "style", new_style, NULL);
    g_free(new_style);

    webkit_dom_element_set_attribute(div, "onclick", "return", NULL);

    g_object_set_data((gpointer)div, "dwb-plugin-element", element);

    webkit_dom_node_remove_child(parent, WEBKIT_DOM_NODE(element), NULL);
    webkit_dom_node_append_child(parent, WEBKIT_DOM_NODE(div), NULL);
    v->plugins->elements = g_slist_prepend(v->plugins->elements, element);

    webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(div), "click", G_CALLBACK(plugins_onclick_cb), true, gl);
    g_object_unref(style);
    g_object_unref(parent);
    v->plugins->created++;
}
static gboolean
plugins_before_load_cb(WebKitDOMDOMWindow *win, WebKitDOMEvent *event, GList *gl) 
{
    WebKitDOMElement *element = (void*)webkit_dom_event_get_src_element(event);
    char *tagname = webkit_dom_element_get_tag_name(element);
    char *type = webkit_dom_element_get_attribute(element, "type");

    if ( (!g_strcmp0(type, "application/x-shockwave-flash") 
                && (! g_ascii_strcasecmp(tagname, "object") || ! g_ascii_strcasecmp(tagname, "embed")) ) 
            && ! g_slist_find(ALLOWED(gl), element) ) 
    {
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
plugins_remove_all(GList *gl) 
{
    if (ALLOWED(gl) != NULL) 
    {
        ALLOWED(gl) = g_slist_remove_all(ALLOWED(gl), ALLOWED(gl)->data);
        ALLOWED(gl) = NULL;
    }
    View *v = VIEW(gl);
    v->plugins->created = 0;

    if (v->plugins->elements != NULL) 
    {
        for (GSList *l = v->plugins->elements; l; l=l->next) 
            g_object_unref(l->data);

        g_slist_free(v->plugins->elements);
        v->plugins->elements = NULL;
    }
}

static gboolean
plugins_before_unload_cb(WebKitDOMDOMWindow *win, WebKitDOMEvent *event, GList *gl) 
{
    plugins_remove_all(gl);
    return true;
}

void
plugins_load_status_cb(WebKitWebView *wv, GParamSpec *p, GList *gl) 
{
    WebKitLoadStatus status = webkit_web_view_get_load_status(wv);
    if (status == WEBKIT_LOAD_COMMITTED) 
    {
        WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
        WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view(doc);
        webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "beforeload", G_CALLBACK(plugins_before_load_cb), true, gl);
        webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), "beforeunload", G_CALLBACK(plugins_before_unload_cb), true, gl);
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

void 
plugins_frame_load_cb(WebKitWebFrame *frame, GParamSpec *p, GList *gl) 
{
    dwb_dom_add_frame_listener(frame, "beforeload", G_CALLBACK(plugins_before_load_cb), true, gl);
}

void
plugins_frame_created_cb(WebKitWebView *wv, WebKitWebFrame *frame, GList *gl) 
{
    g_signal_connect(frame, "notify::load-status", G_CALLBACK(plugins_frame_load_cb), gl);
}

void 
plugins_connect(GList *gl) 
{
    View *v = VIEW(gl);
    if (v->plugins->status & PLUGIN_STATUS_CONNECTED) 
        return;

    v->status->signals[SIG_PLUGINS_LOAD] = g_signal_connect(WEBVIEW(gl), "notify::load-status", G_CALLBACK(plugins_load_status_cb), gl);
    v->status->signals[SIG_PLUGINS_FRAME_CREATED] = g_signal_connect(WEBVIEW(gl), "frame-created", G_CALLBACK(plugins_frame_created_cb), gl);
    v->plugins->status ^= (v->plugins->status & PLUGIN_STATUS_DISCONNECTED) | PLUGIN_STATUS_CONNECTED;
}

void 
plugins_disconnect(GList *gl) 
{
    View *v = VIEW(gl);
    if (v->plugins->status & PLUGIN_STATUS_DISCONNECTED) 
        return;

    for (int i=SIG_PLUGINS_FIRST+1; i<SIG_PLUGINS_LAST; i++) 
    {
        if (VIEW(gl)->status->signals[i] > 0)  
        {
            g_signal_handler_disconnect(WEBVIEW(gl), VIEW(gl)->status->signals[i]);
            v->status->signals[i] = 0;
        }
    }
    plugins_remove_all(gl);
    v->plugins->status ^= (v->plugins->status & PLUGIN_STATUS_CONNECTED) | PLUGIN_STATUS_DISCONNECTED;
}
