#include <stdlib.h>
#include "bufl.h"

struct bufl *BufL = NULL;

int bufl_active_color_on = 0;
int bufl_active_color_off = 90;

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
	int w = 0;
	if (!node)
		return 0;

	while (node->next)
		node = node->next;

	while (node) {
		int color = bufl_isactive(node) ?
			bufl_active_color_on : bufl_active_color_off;
		if (w)
			w += sprintf(ptr + w, " ");
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
