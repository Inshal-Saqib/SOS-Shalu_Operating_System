#include "session.h"

static session_t saved = { 0, {0}, 0, {0}, 0 };

static void scopy(char* dst, const char* src, int max) {
    int i=0;
    while(src[i] && i<max-1){ dst[i]=src[i]; i++; }
    dst[i]=0;
}

void session_save(const char* last_cmd, unsigned int count, const char* user) {
    saved.magic     = SESSION_MAGIC;
    saved.cmd_count = count;
    saved.logged_in = 1;
    scopy(saved.last_cmd, last_cmd, 80);
    scopy(saved.username, user,     32);
}

void session_load(session_t* out) {
    out->magic     = saved.magic;
    out->cmd_count = saved.cmd_count;
    out->logged_in = saved.logged_in;
    scopy(out->last_cmd, saved.last_cmd, 80);
    scopy(out->username, saved.username, 32);
}

int session_exists(void) {
    return saved.magic == SESSION_MAGIC && saved.logged_in == 1;
}

void session_clear(void) {
    saved.magic     = 0;
    saved.logged_in = 0;
    saved.cmd_count = 0;
    saved.last_cmd[0] = 0;
    saved.username[0] = 0;
}
