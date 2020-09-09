#ifndef CMD_h
#define CMD_h 1
#include <stddef.h>

#define CMD_QUIT -1
#define CTRL(x) ((x) & 0x1f)

enum cmd_uflags {
	UPDATE_ECHO = 1 << 0,
	UPDATE_BUF = 1 << 1,
	UPDATE_UI = 1 << 2,
	UPDATE_CMD = 1 << 3,
	UPDATE_RUN = 1 << 4,
};

struct command {
	int ma, aa;
	int mb, ab;
	int (*Do)(struct command *this);
};

extern char cmdline[256];
extern char cmdmsg[256];
extern size_t clwi; /* command line write index */
extern size_t clri; /* command line read index */
extern unsigned int cluf; /* command line update flags */

int cmdline_read (struct command *cmd);
int cmdline_reset (void);
int cmdline_update (char c);
#endif
