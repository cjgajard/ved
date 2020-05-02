#ifndef TERMCFG_h
#define TERMCFG_h 1
#include <sys/types.h>

struct termcfg {
	int cols, lines;
	int init_row, init_col;
	int ol, oc;
	int x, y;
};

int termcfg_init (void);
int termcfg_close (void);

extern struct termcfg T;

int term_commit (void);
int term_move_cursor (void);
int term_move_topleft (void);
int term_read (unsigned char *c);
int term_set_y (int y);

#endif
