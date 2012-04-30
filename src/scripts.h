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

#ifndef SCRIPTS_H
#define SCRIPTS_H

#include <JavaScriptCore/JavaScript.h>

enum SIGNALS {
  SCRIPTS_SIG_FIRST        = 0,
  SCRIPTS_SIG_NAVIGATION   = 0,
  SCRIPTS_SIG_LOAD_STATUS,
  SCRIPTS_SIG_MIME_TYPE,
  SCRIPTS_SIG_DOWNLOAD,
  SCRIPTS_SIG_DOWNLOAD_STATUS,
  SCRIPTS_SIG_RESOURCE,
  SCRIPTS_SIG_KEY_PRESS,
  SCRIPTS_SIG_KEY_RELEASE,
  SCRIPTS_SIG_BUTTON_PRESS,
  SCRIPTS_SIG_BUTTON_RELEASE,
  SCRIPTS_SIG_TAB_FOCUS,
  SCRIPTS_SIG_FRAME_STATUS,
  SCRIPTS_SIG_LOAD_FINISHED,
  SCRIPTS_SIG_LOAD_COMMITTED,
  SCRIPTS_SIG_LAST, 
} ;

#define SCRIPT_MAX_SIG_OBJECTS 8
typedef struct _ScriptSignal {
  JSObjectRef jsobj;
  GObject *objects[SCRIPT_MAX_SIG_OBJECTS]; 
  char *json;
  unsigned int signal;
  int numobj;
} ScriptSignal;
//gboolean scripts_emit(JSObjectRef , int , const char *);
//gboolean scripts_emit(GObject , int, int , ...);
//gboolean scripts_emit(int signal, int numargs, ...);
gboolean scripts_emit(ScriptSignal *);
void scripts_create_tab(GList *gl);
void scripts_remove_tab(JSObjectRef );
void scripts_end(void);
void scripts_init_script(const char *);
void scripts_init(void);
JSObjectRef scripts_make_object(JSContextRef ctx, GObject *o);

#define EMIT_SCRIPT(sig)  ((dwb.misc.script_signals & (1<<SCRIPTS_SIG_##sig)))
#define SCRIPTS_EMIT_RETURN(signal, json) G_STMT_START  \
  if (scripts_emit(&signal)) { \
    g_free(json); \
    return true; \
  } else g_free(json); \
G_STMT_END

#define SCRIPTS_WV(gl) .jsobj = VIEW(gl)->script_wv
#define SCRIPTS_SIG_META(js, sig, num)  .json = js, .signal = SCRIPTS_SIG_##sig, .numobj = num
#endif
