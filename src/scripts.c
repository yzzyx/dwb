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

#include <stdio.h>
#include <JavaScriptCore/JavaScript.h>
#include "dwb.h"
#define kJSDefaultFunction  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum)
#define kJSDefaultProperty  (kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum)
static JSValueRef scripts_set(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_local_Set(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_execute(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_connect(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSStaticFunction static_functions[] = { 
  { "set",             scripts_set,             kJSDefaultFunction }, 
  { "localSet",        scripts_local_Set,       kJSDefaultFunction }, 
  { "execute",         scripts_execute,         kJSDefaultFunction },
  { "spawn",           scripts_spawn,           kJSDefaultFunction },
  { "connect",         scripts_connect,         kJSDefaultFunction },
  { NULL }, 
};

static gboolean _init = false;
static void 
scripts_init() {
  JSClassDefinition cd = kJSClassDefinitionEmpty;
  cd.className = "dwb";
  cd.staticFunctions = static_functions;
}
static JSValueRef scripts_set(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_local_Set(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_execute(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_spawn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
static JSValueRef scripts_connect(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef* exc);
void
scripts_init_script(const char *script) {
  if (!_init) {
  }
  puts(script);

}
