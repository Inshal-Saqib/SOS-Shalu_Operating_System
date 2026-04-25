#ifndef UPTIME_H
#define UPTIME_H

void     uptime_init(void);
void     uptime_tick(void);
unsigned uptime_seconds(void);
void     uptime_print(void);

#endif
