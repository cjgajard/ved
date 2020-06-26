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

int buf_scroll (struct buf *this, int addr)
{
	if (!this)
		return 1;
	int last;
	this->scroll = addr;

	if (this->scroll < 0) {
		this->scroll = 0;
		return 0;
	}

	last = buf_lastline(this) - T.lines;
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
			*addr = Buf->scroll + T.y;
			match += 1;
			continue;
		}
		if (byte == '-' || byte == '+') {
			if (!Buf)
				return 0;
			if (!match)
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

static int cmd_parsecomma (int *addr)
{
	int match = 0;

	if (cmd[cmdri] == ',') {
		cmdri++;
		match = cmd_parseaddr(addr);
	}

	return match;
}

static int cmd_moveto (int y)
{
	if (!Buf)
		return 1;

	if (y < Buf->scroll) {
		buf_scroll(Buf, y);
		term_set_y(0);
	}
	else if (y <= Buf->scroll + T.lines) {
		term_set_y(y - Buf->scroll);
	}
	else {
		buf_scroll(Buf, y - T.lines);
		term_set_y(T.lines);
	}
	return 0;
}

static int cmd_process_buf (void)
{
	cmduf |= UPDATE_BUF;

	while (cmdri < sizeof(cmd) && cmd[cmdri]) {
		switch (cmd[cmdri++]) {
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
		default:
			cmdri--;
			return 0;
		}
	}
	return 0;
}

static int cmd_process_cut (int ma, int aa, int mb, int ab)
{
	if (!Buf)
		return 1;

	int ya = ma ? aa : Buf->scroll + T.y;
	int yb = mb ? ab : ya + 1;

	if (ya >= yb) {
		memcpy(cmdmsg, "?", 2);
		return 1;
	}

	int start = buf_pos(Buf, 0, ya);
	int end = buf_pos(Buf, 0, yb);
	int len = end - start;

	// TODO: memcpy(clipboard, Buf->txt + start, len - 1 /* newline */);
	memmove(Buf->txt + start, Buf->txt + end, Buf->len - end);
	Buf->len -= len;
	Buf->txt[Buf->len + 1] = 0;

	cmd_moveto(ya);
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
			}
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

static int cmd_process_insert (int ma, int aa, int append)
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
	int y = (ma ? aa : Buf->scroll + T.y) + append;
	int pos = buf_pos(Buf, x, y);

	memmove(Buf->txt + pos + cmdlen + 1, Buf->txt + pos, Buf->len - pos);
	memcpy(Buf->txt + pos, cmdstr, cmdlen);
	Buf->txt[pos + cmdlen] = '\n';
	Buf->len += cmdlen + 1;
	Buf->txt[Buf->len + 1] = 0;

	cmd_moveto(y);

	cmduf |= UPDATE_BUF;
	return 0;
}

static int cmd_process_jump (int ma, int aa)
{
	if (!ma)
		return 1;
	buf_scroll(Buf, aa - T.lines / 2);
	term_set_x(0);
	term_set_y(aa - Buf->scroll);
	return 0;
}

static int cmd_process_mv (void)
{
	while (cmdri < sizeof(cmd) && cmd[cmdri]) {
		switch (cmd[cmdri++]) {
		case 'h':
			term_set_x(T.x - 1);
			break;
		case 'j':
			if (T.y < T.lines - 1)
				term_set_y(T.y + 1);
			else
				buf_scroll(Buf, Buf->scroll + 1);
			break;
		case 'k':
			if (!T.y)
				buf_scroll(Buf, Buf->scroll - 1);
			else
				term_set_y(T.y - 1);
			break;
		case 'l':
			term_set_x(T.x + 1);
			break;
		}
	}
	return 0;
}

static int cmd_process_scroll (int ma, int aa, int back)
{
	if (!Buf)
		return 1;
	if (ma) {
		buf_scroll(Buf, aa);
		cmd_moveto(aa);
	}
	else {
		int y = (back ? -1 : 1) * T.lines;
		buf_scroll(Buf, Buf->scroll + y);
	}
	cmduf |= UPDATE_BUF;
	return 0;
}

int cmd_process (void)
{
	int aa = 0;
	int ma = cmd_parseaddr(&aa);

	int ab = 0;
	int mb = cmd_parsecomma(&ab);

	switch (cmd[cmdri++]) {
	case 'F':
		cmd_process_fs();
		break;
	case 'B':
		cmd_process_buf();
		break;
	case 'Z':
		cmd_process_scroll(ma, aa, 1);
		break;
	case 'a':
		cmd_process_insert(ma, aa, 1);
		break;
	case 'i':
		cmd_process_insert(ma, aa, 0);
		break;
	case 'd':
		cmd_process_cut(ma, aa, mb, ab);
		break;
	case 'm':
		cmd_process_mv();
		break;
	case 'z':
		cmd_process_scroll(ma, aa, 0);
		break;
	case '\0':
		cmd_process_jump(ma, aa);
		break;
	}

	cmdri = 0;
	cmduf |= UPDATE_CMD;
	return 0;
}
