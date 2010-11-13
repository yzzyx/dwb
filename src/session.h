#ifndef SESSION_H
#define SESSION_H

gboolean dwb_session_save(const char *);
gboolean dwb_session_restore(const char *);
void dwb_session_list(void);

#endif
