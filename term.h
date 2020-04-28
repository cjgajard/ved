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

ssize_t term_commit (void);
ssize_t term_move_cursor (void);
ssize_t term_move_topleft (void);
ssize_t term_read (unsigned char *c);
#endif
