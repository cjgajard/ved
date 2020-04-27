#ifndef BUF_h
#define BUF_h 1
#include <stdio.h>

struct buf {
	char *txt;
	char path[256];
};

struct buf *buf_create(char *path);
void buf_destroy(struct buf *this);

struct bufl {
	struct buf value;
	struct bufl *next;
};

extern struct bufl *BufL;

int bufl_close(struct bufl *this);
int bufl_pull(struct bufl **this);
int bufl_push(struct bufl **this, struct buf *b);
int bufl_read(struct bufl *this, struct buf *b);
#endif
