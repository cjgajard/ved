#ifndef CMD_h
#define CMD_h 1
#include <stddef.h>

enum cmd_uflags {
	UPDATE_ECHO = 1 << 0,
	UPDATE_BUF = 1 << 1,
	UPDATE_UI = 1 << 2,
	UPDATE_CMD = 1 << 3,
};

extern char cmd[256];
extern size_t cmdwi; /* command write index */
extern size_t cmdri; /* command read index */
extern unsigned int cmduf; /* command update flags */

int cmd_process (void);
int cmd_process_fs (void);
int cmd_process_mv (void);
int cmd_reset (void);
int cmd_update (char c);
#endif
