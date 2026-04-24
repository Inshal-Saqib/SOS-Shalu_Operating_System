#ifndef HISTORY_H
#define HISTORY_H

/* Command history */
void history_add(const char* line);
int  history_get(int offset, char* buf);
int  history_count_get(void);

/* Clipboard */
void clipboard_copy(const char* text, int len);
int  clipboard_paste(char* buf);
int  clipboard_has_data(void);

#endif
