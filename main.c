#include <stdlib.h>
#include <string.h>
#include "ascii.h"
#include "buf.h"
#include "cmd.h"
#include "term.h"

#define CTRL(x) ((x) & 0x1f)

struct termcfg T;

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
	int bottom = T.lines - 1;
	for (int y = 0; y < bottom; y++) {
		printf("\x1b[K~");
		if (y < bottom - 1)
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
	size_t fpos = buf_scroll_pos(b);
	term_move_topleft();
	for (int i = 0; i < T.lines - 1; i++) {
		printf("\x1b[K");
		printf("%d\t", b->scroll + i + 1);
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

static int editor_uifg_draw (void)
{
	printf("\x1b[%dH\x1b[K", T.lines - 1);
	printf("%s", cmdmsg);
	term_commit();
	cmdmsg[0] = 0;
	return 0;
}

static int editor_echo_draw (void)
{
	struct buf *b = NULL;
	bufl_read(BufL, &b);

	printf("\x1b[%dH\x1b[K", T.lines);
	if (b)
		printf("(%d,%d)", b->scroll + T.y + 1, T.x + 1);

	printf(" ");
	for (int i = 0, len = strlen(cmd); i < len; i++)
		ascii_fprintc(stdout, cmd[i]);
	term_commit();
	return 0;
}

int main (int argc, char *argv[])
{
	int error = 0;
	unsigned char c = 0;

	cmduf = UPDATE_BUF | UPDATE_ECHO;

	args_read(argc, argv);

	if (termcfg_init())
		return 1;
main_loop:
	if (cmduf & UPDATE_BUF)
		editor_uibg_draw();
	if (cmduf & UPDATE_BUF)
		editor_buf_draw();
	if (cmduf & UPDATE_CMD)
		editor_uifg_draw();
	if (cmduf & UPDATE_ECHO)
		editor_echo_draw();
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
