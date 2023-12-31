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
	/* attr.c_cc[VMIN] = 0; */
	/* attr.c_cc[VTIME] = 1; */
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &attr) == -1)
		return 1;
	return 0;
}

int term_commit (void)
{
	return fflush(stdout);
}

int term_move_cursor (void)
{
	char buf[32];
	sprintf(buf, "\x1b[%d;%dH", T.y + 1, T.x + 1 + 8);
	buf[31] = 0;
	return write(STDOUT_FILENO, buf, strlen(buf));
}

int term_move_topleft (void)
{
	return printf("\x1b[H");
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

int term_read (unsigned char *c)
{
	if (read(STDIN_FILENO, c, 1) == -1) {
		perror("read");
		return 1;
	}
	return 0;
}

int term_set_x (int x)
{
	if (x < 0)
		x = 0;
	if (x > T.cols - 1)
		x = T.cols - 1;
	T.x = x;
	return 0;
}

int term_set_y (int y)
{
	if (y < 0)
		y = 0;
	if (y > T.lines - 1)
		y = T.lines - 1;
	T.y = y;
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

	T.lines -= 2;
	T.cols -= 8;

	printf("\x1b[?25l");
	printf("\x1b[%dS", T.init_row);
	term_commit();
	return 0;
}

int termcfg_close (void)
{
	int err;
	int y;
	term_move_topleft();
	for (y = 0; y < T.lines + 2; y++) {
		printf("\x1b[K\x1b[B");
	}
	printf("\x1b[?34h\x1b[?25h");
	term_move_topleft();
	term_commit();

	if ((err = tcattr_restore())) {
		perror("tcattr_restore");
		return err;
	}
	return 0;
}
