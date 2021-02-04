#include <stdlib.h>
#include <string.h>
#include "buf.h"

#define BUFADDITIONAL 512

struct buf *Buf = NULL;

int buf_lastline (struct buf *this)
{
	int addr = 0;
	size_t fpos = 0;
	char byte;
	while (fpos < this->len && (byte = this->txt[fpos++])) {
		if (byte == '\n')
			addr++;
	}
	return addr;
}

size_t buf_pos (struct buf *this, int x, int y)
{
	size_t fpos = 0;
	for (int i = 0; i < y; i++) {
		while (fpos < this->len && this->txt[fpos++] != '\n');
	}
	for (int i = 0; i < x; i++) {
		if (fpos >= this->siz)
			break;
		char c = this->txt[fpos++];
		if (c == '\n')
			break;
		if (c == '\t')
			i += 7; // FIXME
	}
	return fpos;
}

size_t buf_cliplen (int ya, int yb, size_t *start)
{
	size_t a = buf_pos(Buf, 0, ya);
	size_t b = buf_pos(Buf, 0, yb);
	if (start)
		*start = a;
	return b - a;
}

size_t buf_save (struct buf *this, char *path)
{
	char *fpath = path[0] ? path : this->path;
	FILE *f;
	if (!(f = fopen(fpath, "w"))) {
		return 0;
	}
	size_t w = fwrite(this->txt, 1, this->len, f);
	fclose(f);
	return w;
}

size_t buf_scroll_pos (struct buf *this)
{
	size_t fpos = 0;
	for (int i = 0; i < this->scroll; i++) {
		while (fpos < this->siz && this->txt[fpos++] != '\n');
	}
	return fpos;
}

struct buf *buf_create (char *path)
{
	struct buf *this;
	if (!(this = malloc(sizeof(*this))))
		return NULL;

	FILE *src;
	if (!(src = fopen(path, "r")))
		return NULL;

	fseek(src, 0, SEEK_END);
	long len = ftell(src);
	long size = 0;
	while (size < len + BUFADDITIONAL)
		size += BUFSIZ;

	if (!(this->txt = malloc(size)))
		return NULL;

	memset(this->txt, 0, size);
	fseek(src, 0, SEEK_SET);
	fread(this->txt, 1, size, src);
	this->txt[size - 1] = 0;
	fclose(src);

	this->siz = size;
	this->len = len;
	this->scroll = 0;
	strncpy(this->path, path, sizeof(this->path));

	return this;
}

void buf_destroy (struct buf *this)
{
	if (this->txt)
		free(this->txt);
	free(this);
}
