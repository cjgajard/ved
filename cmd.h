#ifndef CMD_h
#define CMD_h 1
#include <stdio.h>

enum cmd_uflags {
	UPDATE_ECHO = 1 << 0,
	UPDATE_BUF = 1 << 1,
	UPDATE_UI = 1 << 2,
	UPDATE_CMD = 1 << 3,
	UPDATE_RUN = 1 << 4,
};

extern char cmd[256];
extern char cmdmsg[BUFSIZ];
extern size_t cmdwi; /* command write index */
extern size_t cmdri; /* command read index */
extern unsigned char cmdmo; /* command mode */
extern unsigned int cmduf; /* command update flags */

int cmd_process (void);
int cmd_reset (void);
int cmd_update (char c);
#endif
