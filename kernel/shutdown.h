#ifndef SHUTDOWN_H
#define SHUTDOWN_H

/* Return values for confirmation prompts */
#define POWER_SHUTDOWN  1
#define POWER_RESTART   2
#define POWER_CANCEL    0

void shutdown(void);
void shutdown_do(void);
void restart(void);
int  shutdown_confirm_cli(void);
int  shutdown_confirm_gui(void);

#endif
