#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ascii.h"
#include "buf.h"
#include "term.h"

#define CTRL(x) ((x) & 0x1f)

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

static int args_read (int argc, char *argv[])
{
	for (int i = 1; i < argc; i++) {
		struct buf *b = buf_create(argv[i]);
		if (!b)
			continue;
		bufl_push(&BufL, b);
	}
	return 0;
}

static int editor_uibg_draw (void)
{
	term_move_topleft();
	for (int y = 1; y < T.lines; y++) {
		printf("\x1b[K~");
		if (y < T.lines - 1)
			printf("\r\n");
	}
	term_commit();
	return 0;
}

static int editor_buf_draw (void)
{
	struct buf b;
	if (bufl_read(BufL, &b))
		return 1;
	if (!b.txt)
		return 2;
	int fpos = 0;
	term_move_topleft();
	for (int i = 1; i < T.lines; i++) {
		printf("\x1b[K");
		for (int j = 0; j < T.cols; j++) {
			char byte;
			if (fpos++ >= b.txtsiz)
				return 0;
			if (!(byte = *b.txt++))
				return 0;
			if (byte == '\n')
				break;
			printf("%c", byte);
		}
		printf("\r\n");
	}
	return 0;
}

static int initial_echox = 6;
static int echox;

static int editor_echo_draw (unsigned char c)
{
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
	term_commit();
	return 0;
}

static int editor_uifg_draw (void)
{
	printf("\x1b[%dH\x1b[K", T.lines - 1);
	bufl_print(BufL);
	return 0;
}

int main (int argc, char *argv[])
{
	unsigned char c = 0;
	int err = 0;

	int update_buf = 1;
	int update_ui = 1;
	int update_echo = 0;

	args_read(argc, argv);

	if (termcfg_init())
		return 1;
	// if (!(Screen = screen_create()))
	// 	return 2;
	echox = initial_echox;
main_loop:
	if (update_ui || update_buf)
		editor_uibg_draw();
	if (update_buf)
		editor_buf_draw();
	if (update_ui)
		editor_uifg_draw();
	if (update_echo)
		editor_echo_draw(c);
	term_move_cursor();

	update_buf = 0;
	update_ui = 0;
	update_echo = 1;

	if (read(STDIN_FILENO, &c, 1) == -1) {
		perror("read");
		err = errno;
		goto shutdown;
	}
	switch (c) {
	case CTRL('Q'):
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
	case CTRL('N'):
		bufl_enable(BufL);
		update_buf = 1;
		update_ui = 1;
		break;
	case CTRL('P'):
		bufl_disable(BufL);
		update_buf = 1;
		update_ui = 1;
		break;
	}
	goto main_loop;
shutdown:
	bufl_close(BufL);
	if ((err = termcfg_close()))
		return err;
	return err;
}
