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
	cmduf |= UPDATE_BUF;

	while (cmdri < sizeof(cmd) && cmd[cmdri]) {
		switch (cmd[cmdri++]) {
		case 'e':
			{
				struct buf *b;
				if (!(b = buf_create(cmd + cmdri))) {
					return 1;
				}
				bufl_push(&BufL, b);
				Buf = b;
				break;
			}
		case 'f':
			bufl_sprint(BufL, cmdmsg);
			break;
		case 'n':
			bufl_enable(BufL);
			Buf = bufl_now(BufL);
			break;
		case 'p':
			bufl_disable(BufL);
			Buf = bufl_now(BufL);
			break;
		case 'w':
			if (!Buf) {
				sprintf(cmdmsg, "No file");
				break;
			}
			sprintf(cmdmsg, "%zu", buf_save(Buf, cmd + cmdri));
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
			term_set_y(T.y + 1);
			break;
		case 'k':
			term_set_y(T.y - 1);
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
	if (!Buf)
		return 1;
	char *cmdstr = cmd + cmdri;
	size_t cmdlen = strlen(cmdstr);
	if (Buf->len + cmdlen + 2 >= Buf->siz) {
		fprintf(stderr, "txt should be resized\n"); // FIXME
		return 2;
	}

	int x = T.x;
	int y = (match ? addr : Buf->scroll + T.y) + append;
	int pos = buf_pos(Buf, x, y);

	memmove(Buf->txt + pos + cmdlen + 1, Buf->txt + pos, Buf->len - pos);
	memcpy(Buf->txt + pos, cmdstr, cmdlen);
	Buf->txt[pos + cmdlen] = '\n';
	Buf->len += cmdlen + 1;
	Buf->txt[Buf->len + 1] = 0;

	term_set_y(y + 1 - append);

	cmduf |= UPDATE_BUF;
	return 0;
}

static int cmd_process_scroll (int back)
{
	if (!Buf)
		return 1;
	buf_scroll(Buf, Buf->scroll + (back ? -1 : 1) * (T.lines - 2));
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
			if (!Buf)
				return 0;
			*addr = buf_lastline(Buf);
			match += 1;
			continue;
		}
		if (byte == '.') {
			*addr = T.y;
			match += 1;
			continue;
		}
		if (byte == '-' || byte == '+') {
			if (!Buf)
				return 0;
			*addr = Buf->scroll + T.y;
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
		if (!Buf)
			return 1;
		if (addr < Buf->scroll || addr >= Buf->scroll + T.lines - 2) {
			buf_scroll(Buf, addr);
			cmduf |= UPDATE_BUF;
		}
		T.x = 0;
		term_set_y(addr - Buf->scroll);
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
