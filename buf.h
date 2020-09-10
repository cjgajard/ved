#ifndef BUF_h
#define BUF_h 1
#include <stdio.h>

struct buf {
	char *txt;
	size_t siz;
	size_t len;
	int scroll;
	char path[256];
};

int buf_lastline (struct buf *this);
int buf_scroll (struct buf *this, int addr);
size_t buf_cliplen (int ya, int yb, size_t *start);
size_t buf_pos (struct buf *this, int x, int y);
size_t buf_save (struct buf *this, char *path);
size_t buf_scroll_pos (struct buf *this);
struct buf *buf_create (char *path);
void buf_destroy (struct buf *this);

struct bufl {
	int skip;
	struct bufl *next;
	struct bufl *prev;
	struct buf value;
};

extern struct buf *Buf;
extern struct bufl *BufL;

int bufl_close (struct bufl *this);
int bufl_disable (struct bufl *this);
int bufl_enable (struct bufl *this);
int bufl_pull (struct bufl **this);
int bufl_push (struct bufl **this, struct buf *b);
int bufl_sprint (struct bufl *this, char *f);
struct buf *bufl_now (struct bufl *this);
#endif
