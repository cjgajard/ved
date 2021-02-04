#include <stdlib.h>
#include <string.h>
#include "buf.h"

#define BUFADDITIONAL 512

struct buf *Buf = NULL;
struct bufl *BufL = NULL;

int bufl_active_color_on = 0;
int bufl_active_color_off = 90;

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

static int bufl_isactive (struct bufl *this)
{
	return !this->skip && (!this->prev || this->prev->skip);
}

int bufl_close (struct bufl *this)
{
	while (!bufl_pull(&this));
	return 0;
}

int bufl_disable (struct bufl *this)
{
	struct bufl *node = this;
	while (node) {
		if (!node->skip) {
			node->skip = 1;
			return 0;
		}
		node = node->next;
	}
	return 1;
}

int bufl_enable (struct bufl *this)
{
	struct bufl *node = this;
	while (node) {
		if (!node->skip)
			return 2;
		if (!node->next || !node->next->skip) {
			node->skip = 0;
			return 0;
		}
		node = node->next;
	}
	return 1;
}

int bufl_pull (struct bufl **this)
{
	struct bufl *node = *this;
	if (!node)
		return 1;
	*this = node->next;
	if (*this)
		(*this)->prev = NULL;
	free(node);
	return 0;
}

int bufl_push (struct bufl **this, struct buf *b)
{
	struct bufl **node = this;
	struct bufl *new;
	new = malloc(sizeof(*new));
	new->skip = 0;
	new->next = *node;
	new->prev = NULL;
	new->value = *b;
	*node = new;
	if (new->next)
		new->next->prev = new;
	return 0;
}

int bufl_sprint (struct bufl *this, char *ptr)
{
	struct bufl *node = this;
	if (!node)
		return 0;

	while (node->next)
		node = node->next;

	int w = 0;
	while (node) {
		if (w)
			w += sprintf(ptr + w, " ");
		int color = bufl_isactive(node) ?
			bufl_active_color_on : bufl_active_color_off;
		if (color)
			w += sprintf(ptr + w, "\x1b[%dm", color);
		w += sprintf(ptr + w, "%s", node->value.path);
		if (color)
			w += sprintf(ptr + w, "\x1b[0m");
		node = node->prev;
	}
	if (w)
		w += sprintf(ptr + w, "\n");
	return w;
}

struct buf *bufl_now (struct bufl *this)
{
	struct bufl *node = this;
	while (node) {
		if (!node->skip) {
			return &node->value;
		}
		node = node->next;
	}
	return NULL;
}
