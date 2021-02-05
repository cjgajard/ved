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

extern struct buf *Buf;

int buf_lastline (struct buf *this);
int buf_scroll (struct buf *this, int addr);
int buf_addr (struct buf *this);
size_t buf_cliplen (int ya, int yb, size_t *start);
size_t buf_pos (struct buf *this, int x, int y);
size_t buf_save (struct buf *this, char *path);
size_t buf_scroll_pos (struct buf *this);
struct buf *buf_create (char *path);
void buf_destroy (struct buf *this);
#endif
