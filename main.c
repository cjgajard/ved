#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ascii.h"
#include "buf.h"
#include "term.h"

struct termcfg T;
struct bufl *BufL;

struct screen {
	int lsiz;
	char **lines;
};

struct screen *Screen;

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

static int editor_draw_ui (void)
{
	for (int y = 0; y < T.lines; y++) {
		write(STDOUT_FILENO, "~\x1b[K", 4);
		if (y < T.lines - 1)
			write(STDOUT_FILENO, "\r\n", 2);
	}
	fflush(stdout);
	return 0;
}

// static int editor_draw_buffer (void)
// {
//         char *buf = NULL;
//         if (!BufL) {
//                 fprintf(stderr, "not BufL\n");
//                 return 1;
//         }
//         if (!BufL->value.dst) {
//                 fprintf(stderr, "not BufL->value->dst\n");
//                 return 1;
//         }
//         buf = BufL->value.dst;
//         for (int y = 0; y < T.lines - 1; y++) {
//                 char *li = Screen->lines[y];
//                 int i = 0;
//                 char b;
//                 while ((b = *buf++)) {
//                         if (b == '\n')
//                                 break;
//                         if (i >= Screen->lsiz - 3)
//                                 break;
//                         li[i++] = b;
//                 }
//                 li[i++] = '\r';
//                 li[i++] = '\n';
//                 li[i++] = '\0';
//                 write(STDOUT_FILENO, li, strlen(li));
//         }
//         return 0;
// }

static int args_read (int argc, char *argv[])
{
	for (int i = 1; i < argc; i++) {
		struct buf *b = buf_create(argv[i]);
		if (!b)
			return 1;
		bufl_push(&BufL, b);
	}
	return 0;
}

int main (int argc, char *argv[])
{
	unsigned char c = 0;
	int err = 0;

	args_read(argc, argv);

	if (termcfg_init())
		return 1;
	// if (!(Screen = screen_create()))
	// 	return 2;

	editor_draw_ui();
	term_move_topleft();

	// editor_draw_buffer();
	// term_move_topleft();

	int initial_echox = 6;
	int echox = initial_echox;
main_loop:
	if (read(STDIN_FILENO, &c, 1) == -1) {
		perror("read");
		err = errno;
		goto shutdown;
	}
	switch (c) {
	case '\x11':
		goto shutdown;
		break;
	case 'h':
		if (T.x > 0) {
			T.x--;
			term_move_cursor();
		}
		break;
	case 'j':
		if (T.y < T.lines - 1) {
			T.y++;
			term_move_cursor();
		}
		break;
	case 'k':
		if (T.y > 0) {
			T.y--;
			term_move_cursor();
		}
		break;
	case 'l':
		if (T.x < T.cols - 1) {
			T.x++;
			term_move_cursor();
		}
		break;
	}

	printf("\x1b[%dH0x%02x\x1b[%dG", T.lines, c, echox);
	if (c == 0xd) {
		echox = initial_echox;
		printf("\x1b[%dG\x1b[K", echox);
	}
	else if (isprint(c)) {
		printf("%c", c);
		echox += 1;
	}
	else if (iscntrl(c)) {
		struct ascii a = ascii_ctrl[c == 0x7f ? 0x20 : c];
		printf("<%s>", a.name);
		echox += 2 + strlen(a.name);
	}
	else {
		printf("<0x%02x>", c);
		echox += 4;
	}
	fflush(stdout);

	term_move_cursor();
	goto main_loop;
shutdown:
	bufl_close(BufL);
	if ((err = termcfg_close()))
		return err;
	return err;
}
