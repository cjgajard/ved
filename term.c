#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "term.h"

static struct termios tcattr;

static int tcattr_save (void)
{
	if (tcgetattr(STDIN_FILENO, &tcattr) == -1)
		return errno;
	return 0;
}

static int tcattr_restore (void)
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tcattr) == -1)
		return errno;
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
		return errno;
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

int termcfg_init (void)
{
	T.x = 0;
	T.y = 0;
	T.ol = 0;
	T.oc = 0;
	T.cols = 80;
	T.lines = 24;

	int err = 0;
	if ((err = tcattr_save())) {
		perror("tcattr_save");
		return err;
	}
	if ((err = tcattr_raw())) {
		perror("tcattr_raw");
		return err;
	}

	char str[32] = {0};
	char c;
	int i = 0;
	T.init_row = 0;
	T.init_col = 0;
	write(STDOUT_FILENO, "\x1b[6n", 4);
	while (read(STDIN_FILENO, &c, 1) != -1) {
		if ((size_t)i >= sizeof(str))
			break;
		str[i++] = c;
		if (c == 'R')
			break;
	}
	sscanf(str, "\x1b[%d;%dR", &T.init_row, &T.init_col);

	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
		perror("ioctl");
		return 1;
	}
	if (!ws.ws_col) {
		fprintf(stderr, "ioctl: No columns\n");
		return 1;
	}
	T.cols = ws.ws_col;
	T.lines = ws.ws_row;

	return 0;
}

int termcfg_close (void)
{
	int err;
	if ((err = tcattr_restore())) {
		perror("tcattr_restore");
		return err;
	}
	return 0;
}
