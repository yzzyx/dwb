#ifndef COMPLETION_H
#define COMPLETION_H

typedef struct _Completion Completion;

struct _Completion {
  GtkWidget *event;
  GtkWidget *rlabel;
  GtkWidget *llabel;
  void *data;
};

void dwb_comp_clean_completion(void);
void dwb_comp_clean_autocompletion(void);
void dwb_autocomplete(GList *, GdkEventKey *e);
void dwb_set_autcompletion(GList *, WebSettings *);
void dwb_eval_autocompletion(void);
void dwb_normal_completion(gint);
void dwb_settings_completion(gint back);
void dwb_complete(gint);
void dwb_init_path_completion(void);
void dwb_complete_download(gint back);
void dwb_clean_path_completion(void);

#endif
