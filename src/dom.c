/*
 * Copyright (c) 2010-2013 Stefan Bolte <portix@gmx.net>
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

#include <string.h>
#include "dwb.h"

gboolean
dom_add_frame_listener(WebKitWebFrame *frame, const char *signal, GCallback callback, gboolean bubble, GList *gl) 
{
    char *framesrc, *tagname;
    gboolean ret = false;
    WebKitDOMDOMWindow *win;
    WebKitDOMNode *node;
    WebKitWebView *wv = webkit_web_frame_get_web_view(frame);
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    const char *src = webkit_web_frame_get_uri(frame);
    if (g_strcmp0(src, "about:blank")) 
    {
        /* We have to find the correct frame, but there is no access from the web_frame
         * to the Htmlelement */
        WebKitDOMNodeList *frames = webkit_dom_document_query_selector_all(doc, "iframe, frame", NULL);
        for (guint i=0; i<webkit_dom_node_list_get_length(frames) && ret == false; i++) 
        {
            node = webkit_dom_node_list_item(frames, i);
            tagname = webkit_dom_node_get_node_name(node);
            if (!g_ascii_strcasecmp(tagname, "iframe")) 
            {
                framesrc = webkit_dom_html_iframe_element_get_src(WEBKIT_DOM_HTML_IFRAME_ELEMENT(node));
                win = webkit_dom_html_iframe_element_get_content_window(WEBKIT_DOM_HTML_IFRAME_ELEMENT(node));
            }
            else 
            {
                framesrc = webkit_dom_html_frame_element_get_src(WEBKIT_DOM_HTML_FRAME_ELEMENT(node));
                win = webkit_dom_html_frame_element_get_content_window(WEBKIT_DOM_HTML_FRAME_ELEMENT(node));
            }
            if (!g_strcmp0(src, framesrc)) 
                ret = webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(win), signal, callback, true, gl);

            g_free(framesrc);
        }
        g_object_unref(frames);
    }
    return ret;
}

/* dwb_get_editable(WebKitDOMElement *) {{{*/
gboolean
dom_get_editable(WebKitDOMElement *element) 
{
    if (element == NULL)
        return false;

    char *tagname = webkit_dom_node_get_node_name(WEBKIT_DOM_NODE(element));
    if (tagname == NULL)
        return false;
    if (!strcasecmp(tagname, "INPUT")) 
    {
        char *type = webkit_dom_element_get_attribute((void*)element, "type");
        if (!g_strcmp0(type, "text") || !g_strcmp0(type, "search")|| !g_strcmp0(type, "password")) 
            return true;
    }
    else if (!strcasecmp(tagname, "TEXTAREA")) 
        return true;

    return false;
}/*}}}*/

/* dom_get_active_element(WebKitDOMDocument )  {{{*/
WebKitDOMElement *
dom_get_active_element(WebKitDOMDocument *doc) 
{
    WebKitDOMElement *ret = NULL;
    WebKitDOMDocument *d = NULL;

    WebKitDOMElement *active = webkit_dom_html_document_get_active_element((void*)doc);
    char *tagname = webkit_dom_element_get_tag_name(active);

    if (! g_strcmp0(tagname, "FRAME")) 
    {
        d = webkit_dom_html_frame_element_get_content_document(WEBKIT_DOM_HTML_FRAME_ELEMENT(active));
        ret = dom_get_active_element(d);
    }
    else if (! g_strcmp0(tagname, "IFRAME")) 
    {
        d = webkit_dom_html_iframe_element_get_content_document(WEBKIT_DOM_HTML_IFRAME_ELEMENT(active));
        ret = dom_get_active_element(d);
    }
    else 
        ret = active;

    return ret;
}/*}}}*/


/* dwb_dom_remove_from_parent(WebKitDOMNode *node, GError **error) {{{*/
gboolean 
dom_remove_from_parent(WebKitDOMNode *node, GError **error) 
{
    WebKitDOMNode *parent = webkit_dom_node_get_parent_node(node);
    if (parent != NULL) 
    {
        webkit_dom_node_remove_child(parent, node, error);
        return true;
    }
    return false;
}/*}}}*/
