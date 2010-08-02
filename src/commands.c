#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "dwb.h"
#include "commands.h"
#include "completion.h"

/* dwb_simple_command(keyMap *km) {{{*/
void 
dwb_simple_command(KeyMap *km) {
  gboolean (*func)(void *) = km->map->func;
  Arg *arg = &km->map->arg;
  arg->e = NULL;

  if (dwb.state.mode & AutoComplete) {
    dwb_clean_autocompletion();
  }

  if (func(arg)) {
    if (!km->map->hide) {
      gchar *message = g_strconcat(km->map->n.second, ":", NULL);
      dwb_set_normal_message(dwb.state.fview, message, false);
      g_free(message);
    }
    else if (km->map->hide == AlwaysSM) {
      CLEAR_COMMAND_TEXT(dwb.state.fview);
    }
  }
  else {
    dwb_set_error_message(dwb.state.fview, arg->e ? arg->e : km->map->error);
  }
  dwb.state.nummod = 0;
  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
    dwb.state.buffer = NULL;
  }
}/*}}}*/

