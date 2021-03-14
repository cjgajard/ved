#include <stdlib.h>
#include <string.h>
#include "ascii.h"
#include "bufl.h"
#include "cmd.h"
#include "term.h"

#define COLUMN_MAX 32

struct termcfg T;
static struct command cmd = {0};

static char column_buf[COLUMN_MAX];
static char column_separator[] = "\x1b[37m|\x1b[0m";
static char cur_color[] = "43";
static char dst_color[] = "42";
static char kil_color[] = "41";
static char kil_cur_color[] = "101";
static char mov_color[] = "44";
static char mov_cur_color[] = "46";

static int args_read (int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc; i++) {
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
	int y;
	term_move_topleft();
	for (y = 0; y < T.lines; y++) {
		printf("\x1b[K~");
		if (y < T.lines - 1)
			printf("\r\n");
	}
	term_commit();
	return 0;
}

static char *line_color (int line)
{
	int y = line + (Buf ? Buf->scroll : 0);
	int is_cursor = line == T.y;
	char *range_color = NULL;

	if (cmd.edit & EDIT_DST && y == cmd.yc)
		return dst_color;

	if (cmd.edit & EDIT_KIL)
		range_color = is_cursor ? kil_cur_color : kil_color;
	else if (cmd.edit & EDIT_SRC)
		range_color = is_cursor ? mov_cur_color : mov_color;
	else if (cmd.edit & EDIT_MOV)
		range_color = mov_color;

	if (range_color && (y >= cmd.ya && y <= cmd.yb))
		return range_color;
	if (is_cursor)
		return cur_color;
	return NULL;
}

static int editor_buf_draw (void)
{
	int y;
	int linenr_w;
	size_t fpos;
	if (!Buf)
		return 1;
	if (!Buf->txt)
		return 2;
	fpos = buf_scroll_pos(Buf);

	term_move_topleft();
	linenr_w = sprintf(column_buf, "%d", Buf->scroll + T.lines);
	for (y = 0; y < T.lines; y++) {
		int x;
		int nextline;
		char *color;
		printf("\x1b[K");

		color = line_color(y);
		if (color)
			printf("\x1b[%sm", color);
		printf("%*d", linenr_w, Buf->scroll + y + 1);
		if (color)
			printf("\x1b[0m");
		printf("%s", column_separator);

		nextline = 0;
		for (x = 0; x < T.cols; x++) {
			char byte = ' ';
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
			printf("%c", byte);
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
	int i;
	int len;
	printf("\x1b[%dH\x1b[K:", T.lines + 2);
	for (i = 0, len = strlen(cmdline); i < len; i++)
		ascii_fprintc(stdout, cmdline[i]);
	term_commit();
	return 0;
}

int main (int argc, char *argv[])
{
	int error = 0;
	int result = 0;
	unsigned char c = 0;
	args_read(argc, argv);
	if (termcfg_init())
		return 1;
	cluf = UPDATE_BUF | UPDATE_ECHO;
main_loop:
	if (cluf & UPDATE_BUF)
		editor_uibg_draw();
	if (cluf & UPDATE_BUF)
		editor_buf_draw();
	if (cluf & UPDATE_CMD)
		editor_uifg_draw();
	if (cluf & UPDATE_ECHO)
		editor_echo_draw();
	if (cluf & UPDATE_CMD)
		cmdline_reset();
	cluf = UPDATE_ECHO | UPDATE_BUF;

	if (term_read(&c)) {
		error = 1;
		goto shutdown;
	}
	if (!cmdline_update(c)) {
		cmd_update(&cmd);
		goto main_loop;
	}
	result = cmd.Do ? cmd.Do(&cmd) : 0;
	if (result == CMD_QUIT)
		goto shutdown;
	cmd_reset(&cmd);
	cluf |= UPDATE_CMD;
	goto main_loop;
shutdown:
	bufl_close(BufL);
	if (termcfg_close())
		error = 2;
	return error;
}
