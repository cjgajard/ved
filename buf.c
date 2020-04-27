#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "buf.h"

struct buf *buf_create (char *path)
{
	struct buf *this;
	if (!(this = malloc(sizeof(*this))))
		return NULL;

	if (!(this->txt = malloc(BUFSIZ)))
		return NULL;
	memset(this->txt, 0, BUFSIZ);
	this->txtsiz = BUFSIZ;

	strncpy(this->path, path, sizeof(this->path));

	FILE *src;
	if (!(src = fopen(path, "r")))
		return NULL;
	fread(this->txt, 1, BUFSIZ, src);
	fclose(src);
	this->txt[BUFSIZ - 1] = 0;

	return this;
}

void buf_destroy (struct buf *this)
{
	if (this->txt)
		free(this->txt);
	free(this);
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

int bufl_read (struct bufl *this, struct buf *b)
{
	struct bufl *node = this;
	while (node) {
		if (!node->skip) {
			*b = node->value;
			return 0;
		}
		node = node->next;
	}
	return 1;
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


int bufl_close (struct bufl *this)
{
	while (!bufl_pull(&this));
	return 0;
}

int bufl_print (struct bufl *this)
{
	struct bufl *node = this;
	if (!node)
		return 0;

	while (node->next)
		node = node->next;

	int w = 0;
	while (node) {
		if (w)
			printf(" ");
		if (node->skip)
			w += printf("\x1b[90m");
		w += printf("%s", node->value.path);
		if (node->skip)
			w += printf("\x1b[0m");
		node = node->prev;
	}
	if (w)
		w += printf("\n");
	return w;
}
