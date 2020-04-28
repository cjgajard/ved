#include <stdlib.h>
#include <string.h>
#include "screen.h"
#include "term.h"

struct screen *screen_new (void)
{
	struct screen *this;
	char **lines;
	if (!(this = malloc(sizeof(*this))))
		return NULL;
	this->lsiz = T.cols * 4 * sizeof(char *);
	if (!(lines = malloc(T.lines * sizeof(char *))))
		return NULL;
	for (int i = 0; i < T.lines; i++) {
		if (!(lines[i] = malloc(this->lsiz)))
			return NULL;
		memset(lines[i], 0, this->lsiz);
	}
	return this;
}

void screen_destroy (struct screen *this)
{
	if (!this)
		return;
	for (int i = 0; i < T.lines; i++)
		free(this->lines[i]);
	free(this->lines);
	free(this);
}
