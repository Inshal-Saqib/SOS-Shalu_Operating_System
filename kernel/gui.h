#ifndef GUI_H
#define GUI_H

typedef enum {
    GUI_CLOCK,
    GUI_CALENDAR,
    GUI_MEMINFO,
    GUI_MEMTEST,
    GUI_ABOUT,
    GUI_CLEAR,
    GUI_HISTORY,
    GUI_SWITCH_CLI,
    GUI_HALT,
} gui_action_t;

gui_action_t gui_run(void);

#endif
