#include <stdlib.h>
#include <string.h>
#include "ascii.h"
#include "buf.h"
#include "cmd.h"
#include "term.h"

#define CTRL(x) ((x) & 0x1f)

struct termcfg T;

static char *range_color = "44";
static char *cursor_color = "105";

static int args_read (int argc, char *argv[])
{
	for (int i = 1; i < argc; i++) {
		struct buf *b = buf_create(argv[i]);
		if (!b)
			continue;
		bufl_push(&BufL, b);
		Buf = b;
	}
	return 0;
}

static int editor_uibg_draw (void)
{
	term_move_topleft();
	for (int y = 0; y < T.lines; y++) {
		printf("\x1b[K~");
		if (y < T.lines - 1)
			printf("\r\n");
	}
	term_commit();
	return 0;
}

static int editor_buf_draw (void)
{
	if (!Buf)
		return 1;
	if (!Buf->txt)
		return 2;
	size_t fpos = buf_scroll_pos(Buf);

	term_move_topleft();
	for (int y = 0; y < T.lines; y++) {
		printf("\x1b[K");

		int is_range = y == T.y;
		if (is_range)
			printf("\x1b[%sm", range_color);
		printf("%d\t", Buf->scroll + y + 1);
		if (is_range)
			printf("\x1b[0m");

		int nextline = 0;
		for (int x = 0; x < T.cols; x++) {
			char byte = ' ';
			int is_cursor = y == T.y && x == T.x;
			int is_tab = 0;
			if (!nextline) {
				if (fpos >= Buf->siz)
					return 0;
				if (!(byte = Buf->txt[fpos++]))
					return 0;
				switch (byte) {
				case EOF:
					return 0;
				case '\n':
					byte = ' ';
					nextline = 1;
					break;
				case '\t':
					byte = ' ';
					is_tab = 1;
					x += 7;
					break;
				}
			}
			if (is_cursor)
				printf("\x1b[%sm", cursor_color);
			printf("%c", byte);
			if (is_cursor)
				printf("\x1b[0m");
			if (is_tab)
				printf("%-7c", ' ');

		}

		printf("\r\n");
	}
	return 0;
}

static int editor_uifg_draw (void)
{
	printf("\x1b[%dH\x1b[K", T.lines + 1);
	printf("%s", cmdmsg);
	term_commit();
	cmdmsg[0] = 0;
	return 0;
}

static int editor_echo_draw (void)
{
	printf("\x1b[%dH\x1b[K", T.lines + 2);
	printf("(%d,%d)", (Buf ? Buf->scroll : 0) + T.y + 1, T.x + 1);
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
	cmduf = UPDATE_ECHO | UPDATE_BUF;

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
