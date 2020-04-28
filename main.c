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

enum update_flags {
	UPDATE_ECHO = 1 << 0,
	UPDATE_BUF = 1 << 1,
	UPDATE_UI = 1 << 2,
	UPDATE_CMD = 1 << 3,
};

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

int ascii_fprintc (FILE *f, char byte)
{
	int w = 0;
	unsigned char c = byte;
	if (isprint(c)) {
		w += fprintf(f, "%c", c);
	}
	else if (iscntrl(c)) {
		struct ascii a = ascii_ctrl[c == 0x7f ? 0x20 : c];
		w += fprintf(f, "<%s>", a.name);
	}
	else {
		w += fprintf(f, "<0x%02x>", c);
	}
	return w;
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

static char cmd[256] = {0};
static size_t cmdwi = 0; /* command write index */
static size_t cmdri = 0; /* command read index */
static unsigned int cmduf = 0 /* command update flags */;

static int cmd_reset (void)
{
	memset(cmd, 0, sizeof(cmd));
	cmdwi = 0;
	return 0;
}

static int cmd_update (char c)
{
	if (c == ASCII_DEL) {
		if (cmdwi > 0)
			cmd[--cmdwi] = 0;
		return 0;
	}
	if (cmdwi < sizeof(cmd) - 1) {
		cmd[cmdwi++] = c;
		cmd[cmdwi] = 0;
	}
	return 0;
}

static int cmd_process_fs (void)
{
	struct buf *b;
	cmduf |= UPDATE_BUF | UPDATE_UI;

	while (cmdri < sizeof(cmd) && cmd[cmdri]) {
		switch (cmd[cmdri++]) {
		case 'e':
			if ((b = buf_create(cmd + cmdri)))
				bufl_push(&BufL, b);
			break;
		case 'n':
			bufl_enable(BufL);
			break;
		case 'p':
			bufl_disable(BufL);
			break;
		default:
			return 0;
		}
	}
	return 0;
}

static int cmd_process_mv (void)
{
	while (cmdri < sizeof(cmd) && cmd[cmdri]) {
		switch (cmd[cmdri++]) {
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
	}
	return 0;
}

static int cmd_process (void)
{
	switch (cmd[cmdri++]) {
	case ':':
		cmd_process_fs();
		break;
	case 'm':
		cmd_process_mv();
		break;
	}
	cmdri = 0;
	cmduf |= UPDATE_CMD;
	return 0;
}

static int editor_echo_draw (unsigned char c)
{
	printf("\x1b[%dH\x1b[K0x%02x", T.lines, c);
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
	if (cmduf & (UPDATE_UI | UPDATE_BUF)) {
		editor_uibg_draw();
	}
	if (cmduf & UPDATE_BUF) {
		editor_buf_draw();
	}
	if (cmduf & UPDATE_UI) {
		editor_uifg_draw();
	}
	if (cmduf & UPDATE_ECHO)
		editor_echo_draw(c);
	if (cmduf & UPDATE_CMD)
		cmd_reset();
	cmduf = (UPDATE_ECHO);
	term_move_cursor();

	if (read(STDIN_FILENO, &c, 1) == -1) {
		perror("read");
		error = 1;
		goto shutdown;
	}
	switch (c) {
	case CTRL('q'):
		goto shutdown;
		break;
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
