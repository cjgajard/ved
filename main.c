#include <stdlib.h>
#include <string.h>
#include "ascii.h"
#include "buf.h"
#include "cmd.h"
#include "term.h"

#define CTRL(x) ((x) & 0x1f)

struct termcfg T;
struct bufl *BufL;

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
	struct buf *b;
	if (bufl_read(BufL, &b))
		return 1;
	if (!b->txt)
		return 2;
	size_t fpos = 0;
	term_move_topleft();
	for (int i = 1; i < T.lines; i++) {
		printf("\x1b[K");
		for (int j = 0; j < T.cols; j++) {
			char byte;
			if (fpos >= b->siz)
				return 0;
			if (!(byte = b->txt[fpos++]))
				return 0;
			if (byte == '\n')
				break;
			printf("%c", byte);
		}
		printf("\r\n");
	}
	return 0;
}

static int editor_echo_draw (unsigned char c)
{
	printf("\x1b[%dH\x1b[K", T.lines);
	printf("(0x%02x)", c);
	printf(" ");
	for (int i = 0, len = strlen(cmd); i < len; i++)
		ascii_fprintc(stdout, cmd[i]);
	term_commit();
	return 0;
}

static int editor_uifg_draw (void)
{
	printf("\x1b[%dH\x1b[K", T.lines - 1);
	bufl_fprint(BufL, stdout);
	return 0;
}

int main (int argc, char *argv[])
{
	int error = 0;
	unsigned char c = 0;

	cmduf = UPDATE_BUF | UPDATE_UI;

	args_read(argc, argv);

	if (termcfg_init())
		return 1;
	// if (!(Screen = screen_create()))
	// 	return 2;
main_loop:
	if (cmduf & UPDATE_BUF)
		editor_uibg_draw();
	if (cmduf & UPDATE_BUF)
		editor_buf_draw();
	if (cmduf & UPDATE_UI)
		editor_uifg_draw();
	if (cmduf & UPDATE_ECHO)
		editor_echo_draw(c);
	if (cmduf & UPDATE_CMD)
		cmd_reset();
	cmduf = UPDATE_ECHO;

	term_move_cursor();

	if (term_read(&c)) {
		error = 1;
		goto shutdown;
	}

	switch (c) {
	case CTRL('q'):
		goto shutdown;
	case CTRL('m'):
		cmd_process();
		break;
	}

	cmd_update(c);
	goto main_loop;
shutdown:
	bufl_close(BufL);
	if (termcfg_close())
		error = 2;
	return error;
}
