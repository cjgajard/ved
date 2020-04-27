#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "buf.h"

struct buf *buf_create(char *path)
{
	struct buf *this;
	if (!(this = malloc(sizeof(*this))))
		return NULL;

	if (!(this->txt = malloc(BUFSIZ)))
		return NULL;
	memset(this->txt, 0, BUFSIZ);

	strncpy(this->path, path, sizeof(this->path));

	FILE *src;
	if (!(src = fopen(path, "r"))) {
		fprintf(stderr, "ERROR: fopen\r\n");
		return NULL;
	}
	fread(this->txt, 1, BUFSIZ, src);
	fclose(src);
	this->txt[BUFSIZ - 1] = 0;

	return this;
}

void buf_destroy(struct buf *this)
{
	if (this->txt)
		free(this->txt);
	free(this);
}

int bufl_push(struct bufl **this, struct buf *b)
{
	struct bufl **node = this;
	struct bufl *new;
	new = malloc(sizeof(*new));
	new->value = *b;
	new->next = *node;
	*node = new;
	return 0;
}

int bufl_pull(struct bufl **this)
{
	struct bufl *node = *this;
	if (!node)
		return 1;
	*this = node->next;
	free(node);
	return 0;
}

int bufl_read(struct bufl *this, struct buf *b)
{
	if (!this)
		return 1;
	*b = this->value;
	return 0;
}

int bufl_close(struct bufl *this)
{
	while (!bufl_pull(&this));
	return 0;
}

int bufl_print(struct bufl *this)
{
	struct bufl *node = this;
	int w = 0;
	while (node) {
		if (w)
			printf(" -> ");
		w += printf("%s", node->value.path);
		node = node->next;
	}
	if (w)
		w += printf("\n");
	return w;
}
