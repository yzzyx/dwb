#ifndef SESSION_H
#define SESSION_H

gboolean dwb_session_save(const gchar *);
gboolean dwb_session_restore(const gchar *);
void dwb_session_list(void);

#endif
