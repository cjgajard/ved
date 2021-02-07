#ifndef CMD_h
#define CMD_h 1
#include <stddef.h>

#define CMD_QUIT -1

enum cmd_uflags {
	UPDATE_ECHO = 1 << 0,
	UPDATE_BUF = 1 << 1,
	UPDATE_UI = 1 << 2,
	UPDATE_CMD = 1 << 3,
	UPDATE_RUN = 1 << 4,
};

/*
	y	t	d	m	x	z	null
del	0	0	1	1	0	0	0
src	1	1	1	1	0	0	0
->mc	0	1	0	1	1	0	0
mov	0	0	0	0	0	1	1
	b_	bg	r_	rg	_g	b	b
*/

enum cmd_editflags {
	EDIT_KIL = 1 << 0,
	EDIT_SRC = 1 << 1,
	EDIT_DST = 1 << 2,
	EDIT_MOV = 1 << 3,
};

struct command {
	int ma, aa;
	int mb, ab;
	int mc, ac;
	enum cmd_editflags edit;
	int (*Do)(struct command *this);
};

int cmd_reset (struct command *this);
int cmd_update (struct command *this);

extern char cmdline[256];
extern char cmdmsg[256];
extern unsigned int cluf; /* command line update flags */

int cmdline_reset (void);
int cmdline_update (char c);
#endif
