#ifndef BUFL_h
#define BUFL_h 1
#include "buf.h"

struct bufl {
	int skip;
	struct bufl *next;
	struct bufl *prev;
	struct buf value;
};

extern struct bufl *BufL;

int bufl_close (struct bufl *this);
int bufl_disable (struct bufl *this);
int bufl_enable (struct bufl *this);
int bufl_pull (struct bufl **this);
int bufl_push (struct bufl **this, struct buf *b);
int bufl_sprint (struct bufl *this, char *f);
struct buf *bufl_now (struct bufl *this);
#endif
