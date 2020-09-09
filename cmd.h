#ifndef CMD_h
#define CMD_h 1
#include <stddef.h>

enum cmd_uflags {
	UPDATE_ECHO = 1 << 0,
	UPDATE_BUF = 1 << 1,
	UPDATE_UI = 1 << 2,
	UPDATE_CMD = 1 << 3,
	UPDATE_RUN = 1 << 4,
};

extern char cmdline[256];
extern char cmdmsg[256];
extern size_t clwi; /* command line write index */
extern size_t clri; /* command line read index */
extern unsigned int cluf; /* command line update flags */
extern int clma, claa;
extern int clmb, clab;

int cmdline_process (void);
int cmdline_exec (void);
int cmdline_reset (void);
int cmdline_update (char c);
#endif
