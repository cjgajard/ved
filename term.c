#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "term.h"

static struct termios tcattr;

static int tcattr_save (void)
{
	if (tcgetattr(STDIN_FILENO, &tcattr) == -1)
		return 1;
	return 0;
}

static int tcattr_restore (void)
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tcattr) == -1)
		return 1;
	return 0;
}

static int tcattr_raw (void)
{
	struct termios attr = tcattr;
	attr.c_cflag |= CS8;
	attr.c_iflag &= ~(ICRNL | INPCK | ISTRIP | IXON);
	attr.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	attr.c_oflag &= ~(OPOST);
	// attr.c_cc[VMIN] = 0;
	// attr.c_cc[VTIME] = 1;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &attr) == -1)
		return 1;
	return 0;
}

ssize_t term_commit (void)
{
	return fflush(stdout);
}

ssize_t term_erase_display (void)
{
	return write(STDOUT_FILENO, "\x1b[2J", 4);
}

ssize_t term_move_topleft (void)
{
	// return write(STDOUT_FILENO, "\x1b[H", 3);
	return printf("\x1b[H");
}

ssize_t term_move_cursor (void)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", T.y + 1, T.x + 1);
	return write(STDOUT_FILENO, buf, strlen(buf));
}

static int term_get_cursor (int *row, int *col)
{
	char c;
	char str[32];
	int i = 0;
	write(STDOUT_FILENO, "\x1b[6n", 4);
	while (read(STDIN_FILENO, &c, 1) != -1) {
		if ((size_t)i >= sizeof(str))
			break;
		str[i++] = c;
		if (c == 'R')
			break;
	}
	sscanf(str, "\x1b[%d;%dR", row, col);
	return 0;
}

int termcfg_init (void)
{
	T.x = 0;
	T.y = 0;
	T.ol = 0;
	T.oc = 0;
	T.cols = 80;
	T.lines = 24;
	T.init_row = 0;
	T.init_col = 0;

	if (tcattr_save()) {
		perror("tcattr_save");
		return 1;
	}
	if (tcattr_raw()) {
		perror("tcattr_raw");
		return 2;
	}

	term_get_cursor(&T.init_row, &T.init_col);
	write(STDOUT_FILENO, "\x1b[999B\x1b[999C", 12);
	term_get_cursor(&T.lines, &T.cols);

	// printf("\x1b[?25l");
	printf("\x1b[%dS", T.init_row);
	term_commit();
	return 0;
}

int termcfg_close (void)
{
	term_move_topleft();
	for (int y = 0; y < T.lines; y++) {
		printf("\x1b[K\x1b[B");
	}
	// printf("\x1b[?34h\x1b[?25h");
	term_move_topleft();
	term_commit();

	int err;
	if ((err = tcattr_restore())) {
		perror("tcattr_restore");
		return err;
	}
	return 0;
}
