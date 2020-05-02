#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "ascii.h"
#include "buf.h"
#include "cmd.h"
#include "term.h"

char cmd[256] = {0};
char cmdmsg[BUFSIZ];
size_t cmdri = 0;
size_t cmdwi = 0;
unsigned int cmduf = 0;

static int buf_scroll (struct buf *this, int addr)
{
	int last;
	this->scroll = addr;

	if (this->scroll < 0) {
		this->scroll = 0;
		return 0;
	}

	last = buf_lastline(this) - T.lines + 2;
	if (this->scroll > last) {
		this->scroll = last;
	}
	return 0;
}

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

static int cmd_process_fs (void)
{
	struct buf *b;
	cmduf |= UPDATE_BUF;

	while (cmdri < sizeof(cmd) && cmd[cmdri]) {
		switch (cmd[cmdri++]) {
		case 'e':
			if ((b = buf_create(cmd + cmdri)))
				bufl_push(&BufL, b);
			break;
		case 'f':
			bufl_sprint(BufL, cmdmsg);
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
			if (T.x > 0)
				T.x--;
			break;
		case 'j':
			if (T.y < T.lines - 1)
				T.y++;
			break;
		case 'k':
			if (T.y > 0)
				T.y--;
			break;
		case 'l':
			if (T.x < T.cols - 1)
				T.x++;
			break;
		}
	}
	return 0;
}

static int cmd_process_insert (int addr, int match, int append)
{
	struct buf *b;
	if (bufl_read(BufL, &b))
		return 1;

	char *cmdstr = cmd + cmdri;
	size_t cmdlen = strlen(cmdstr);
	if (b->len + cmdlen + 2 >= b->siz) {
		fprintf(stderr, "txt should be resized\n"); // FIXME
		return 2;
	}

	int x = T.x;
	int y = (match ? addr : b->scroll + T.y) + append;
	int pos = buf_pos(b, x, y);

	memmove(b->txt + pos + cmdlen + 1, b->txt + pos, b->len - pos);
	memcpy(b->txt + pos, cmdstr, cmdlen);
	b->txt[pos + cmdlen] = '\n';
	b->len += cmdlen + 1;
	b->txt[b->len + 1] = 0;

	cmduf |= UPDATE_BUF;
	return 0;
}

static int cmd_process_scroll (int back)
{
	struct buf *b;
	if (bufl_read(BufL, &b))
		return 1;
	buf_scroll(b, b->scroll + (back ? -1 : 1) * (T.lines - 2));
	cmduf |= UPDATE_BUF;
	return 0;
}

static int cmd_parseaddr (int *addr) {
	char byte;
	int rel = 0;
	int match = 0;
	*addr = 0;
	
	while ((byte = cmd[cmdri++])) {
		if (byte == '$') {
			struct buf *buf;
			if (bufl_read(BufL, &buf))
				return 0;
			*addr = buf_lastline(buf);
			match += 1;
			continue;
		}
		if (byte == '.') {
			*addr = T.y;
			match += 1;
			continue;
		}
		if (byte == '-' || byte == '+') {
			struct buf *buf;
			if (bufl_read(BufL, &buf))
				return 0;
			*addr = buf->scroll + T.y;
			match += 1;
			rel = 1;
		}
		if (rel || isdigit(byte)) {
			char memo[16];
			int i = 0;
			do {
				memo[i++] = byte;
				byte = cmd[cmdri++];
			}
			while (isdigit(byte));
			memo[i] = 0;

			if (i == 1 && memo[0] == '+') {
				*addr += 1;
			}
			else if (i == 1 && memo[0] == '-') {
				*addr -= 1;
			}
			else if (rel) {
				*addr += atoi(memo);
			}
			else {
				int n = atoi(memo);
				*addr += n >= 0 ? n - 1 : n;
			}
			match += 1;
		}
		cmdri--;
		break;
	}
	return match;
}

int cmd_moveto (int addr, int match)
{
	if (match) {
		struct buf *b;
		if (bufl_read(BufL, &b))
			return 1;
		if (addr < b->scroll || addr >= b->scroll + T.lines - 2) {
			buf_scroll(b, addr);
			cmduf |= UPDATE_BUF;
		}
		T.x = 0;
		T.y = addr - b->scroll;
	}
	return 0;
}

int cmd_process (void)
{
	int ra = 0;
	int ma = cmd_parseaddr(&ra);
	// int rb = 0;
	// int mb = 0;

	// if (cmd[cmdri] == ',') {
	//	cmdri++;
	//	mb = cmd_parseaddr(&rb);
	// }

	switch (cmd[cmdri++]) {
	case 'F':
		cmd_process_fs();
		break;
	case 'm':
		cmd_process_mv();
		break;
	case 'z':
		cmd_process_scroll(0);
		break;
	case 'Z':
		cmd_process_scroll(1);
		break;
	case 'a':
		cmd_process_insert(ra, ma, 1);
		cmd_moveto(ra + 2, ma);
		break;
	case 'i':
		cmd_process_insert(ra, ma, 0);
		cmd_moveto(ra + 1, ma);
		break;
	case '\0':
		cmd_moveto(ra, ma);
		break;
	}

	cmdri = 0;
	cmduf |= UPDATE_CMD;
	return 0;
}
