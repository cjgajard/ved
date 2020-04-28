#include <string.h>
#include "ascii.h"
#include "buf.h"
#include "cmd.h"
#include "term.h"

char cmd[256] = {0};
size_t cmdri = 0;
size_t cmdwi = 0;
unsigned int cmduf = 0;

int cmd_reset (void)
{
	memset(cmd, 0, sizeof(cmd));
	cmdwi = 0;
	return 0;
}

int cmd_update (char c)
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

int cmd_process_fs (void)
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

int cmd_process_mv (void)
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

int cmd_process (void)
{
	switch (cmd[cmdri++]) {
	case 'F':
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