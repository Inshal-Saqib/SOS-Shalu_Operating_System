#ifndef SHUTDOWN_H
#define SHUTDOWN_H

#define POWER_CANCEL    0
#define POWER_SHUTDOWN  1
#define POWER_RESTART   2
#define POWER_LOGOUT    3

/* Global logout flag checked by kernel main loop */
extern volatile int g_logout_requested;

void shutdown(void);
void shutdown_do(void);
void restart(void);
void logout_sos(void);
int  shutdown_confirm_cli(void);
int  shutdown_confirm_gui(void);

#endif
