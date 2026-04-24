#include "history.h"

/* ── Storage ───────────────────────────────────────────────────────────── */
#define MAX_ENTRIES   32
#define MAX_LINE_LEN  80
#define MAX_CLIP_LEN  80

/* Command history ring buffer */
static char history[MAX_ENTRIES][MAX_LINE_LEN];
static int  history_count = 0;

/* Clipboard buffer */
static char clipboard[MAX_CLIP_LEN];
static int  clipboard_len = 0;

/* ── History ───────────────────────────────────────────────────────────── */
void history_add(const char* line) {
    if (!line || line[0] == '\0') return;

    /* Don't store duplicate of last entry */
    if (history_count > 0) {
        int last = (history_count - 1) % MAX_ENTRIES;
        int same = 1;
        for (int i = 0; line[i] || history[last][i]; i++) {
            if (line[i] != history[last][i]) { same = 0; break; }
        }
        if (same) return;
    }

    int idx = history_count % MAX_ENTRIES;
    int i;
    for (i = 0; i < MAX_LINE_LEN - 1 && line[i]; i++)
        history[idx][i] = line[i];
    history[idx][i] = '\0';
    history_count++;
}

int history_get(int offset, char* buf) {
    /* offset 1 = last command, 2 = second last, etc. */
    if (offset <= 0 || offset > history_count) return 0;
    if (offset > MAX_ENTRIES) return 0;
    int idx = (history_count - offset) % MAX_ENTRIES;
    int i;
    for (i = 0; i < MAX_LINE_LEN - 1 && history[idx][i]; i++)
        buf[i] = history[idx][i];
    buf[i] = '\0';
    return 1;
}

int history_count_get(void) {
    return history_count;
}

/* ── Clipboard ─────────────────────────────────────────────────────────── */
void clipboard_copy(const char* text, int len) {
    if (len > MAX_CLIP_LEN - 1) len = MAX_CLIP_LEN - 1;
    for (int i = 0; i < len; i++)
        clipboard[i] = text[i];
    clipboard[len] = '\0';
    clipboard_len = len;
}

int clipboard_paste(char* buf) {
    if (clipboard_len == 0) return 0;
    for (int i = 0; i < clipboard_len; i++)
        buf[i] = clipboard[i];
    buf[clipboard_len] = '\0';
    return clipboard_len;
}

int clipboard_has_data(void) {
    return clipboard_len > 0;
}
