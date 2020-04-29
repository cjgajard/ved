#ifndef BUF_h
#define BUF_h 1
#include <stdio.h>

struct buf {
	char *txt;
	size_t siz;
	size_t len;
	size_t scroll;
	char path[256];
};

size_t buf_pos (struct buf *this, int x, int y);
size_t buf_scroll_pos (struct buf *this);
struct buf *buf_create (char *path);
void buf_destroy (struct buf *this);

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
int bufl_fprint (struct bufl *this, FILE *f);
int bufl_pull (struct bufl **this);
int bufl_push (struct bufl **this, struct buf *b);
int bufl_read (struct bufl *this, struct buf **b);
#endif
