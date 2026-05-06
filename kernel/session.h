#ifndef SESSION_H
#define SESSION_H

#define SESSION_MAGIC 0xC0FFEE42

typedef struct {
    unsigned int magic;
    char         last_cmd[80];
    unsigned int cmd_count;
    char         username[32];
    unsigned int logged_in;
} session_t;

void session_save(const char* last_cmd, unsigned int count, const char* user);
void session_load(session_t* out);
int  session_exists(void);
void session_clear(void);

#endif
