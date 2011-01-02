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

typedef struct _Completion Completion;

struct _Completion {
  GtkWidget *event;
  GtkWidget *rlabel;
  GtkWidget *llabel;
  GtkWidget *mlabel;
  void *data;
};

void dwb_comp_clean_completion(void);
void dwb_comp_clean_autocompletion(void);
void dwb_comp_clean_path_completion(void);

void dwb_comp_set_autcompletion(GList *, WebSettings *);
void dwb_comp_autocomplete(GList *, GdkEventKey *e);
void dwb_comp_eval_autocompletion(void);

void dwb_comp_complete(CompletionType, int);
void dwb_comp_complete_download(int back);

#endif
