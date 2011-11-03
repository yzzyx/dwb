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

#ifndef COMPLETION_H
#define COMPLETION_H

#include "dwb.h"
#include "commands.h"
#include "util.h"

#define COMP_EVENT_BOX(X)    (((Completion*)((X)->data))->event)

typedef struct _Completion Completion;

struct _Completion {
  GtkWidget *event;
  GtkWidget *rlabel;
  GtkWidget *llabel;
  GtkWidget *mlabel;
  void *data;
};

void completion_clean_completion(void);
void completion_clean_autocompletion(void);
void completion_clean_path_completion(void);
void completion_set_entry_text(Completion *);

void completion_set_autcompletion(GList *, WebSettings *);
void completion_autocomplete(GList *, GdkEventKey *e);
void completion_eval_autocompletion(void);
void completion_eval_buffer_completion(void);
void completion_buffer_key_press(GdkEventKey *);

DwbStatus completion_complete(CompletionType, int);
void completion_complete_path(int back);

#endif
