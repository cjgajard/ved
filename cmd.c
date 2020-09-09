#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "ascii.h"
#include "buf.h"
#include "cmd.h"
#include "term.h"

char cmdline[256] = {0};
char cmdmsg[256] = {0};
char cmdclip[BUFSIZ];
size_t clri = 0;
size_t clwi = 0;
unsigned int cluf = 0;

static int cmd_do_buf (void);
static int cmd_do_delete (int ma, int aa, int mb, int ab);
static int cmd_do_fs (void);
static int cmd_do_insert (int ma, int aa, int append);
static int cmd_do_jump (int ma, int aa);
static int cmd_do_move (int ma, int aa, int mb, int ab);
static int cmd_do_movement (void);
static int cmd_do_paste (int ma, int aa);
static int cmd_do_scroll (int ma, int aa, int back);
static int cmd_do_transfer (int ma, int aa, int mb, int ab);
static int cmd_do_yank (int ma, int aa, int mb, int ab);

static int currentaddr (void)
{
	return (!Buf ? 0 : Buf->scroll) + T.y;
}

static int parseaddr (int *addr) {
	char byte;
	int rel = 0;
	int match = 0;
	*addr = 0;

	while ((byte = cmdline[clri++])) {
		if (byte == '$') {
			if (!Buf)
				return 0;
			*addr = buf_lastline(Buf);
			match += 1;
			continue;
		}
		if (byte == '.') {
			*addr = currentaddr();
			match += 1;
			continue;
		}
		if (byte == '-' || byte == '+') {
			if (!Buf)
				return 0;
			if (!match)
				*addr = currentaddr();
			match += 1;
			rel = 1;
		}
		if (rel || isdigit(byte)) {
			char memo[16];
			int i = 0;
			do {
				memo[i++] = byte;
				byte = cmdline[clri++];
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
		clri--;
		break;
	}
	return match;
}

static int parsecomma (int *aa)
{
	switch (cmdline[clri]) {
	case ',':
		clri++;
		if (!aa)
			return 0;
		*aa = 0; /* address 0 is line 1 */
		return 1;
	case ';':
		clri++;
		if (!aa)
			return 0;
		*aa = currentaddr();
		return 1;
	default:
		return 0;
	}
}

static int moveto (int y)
{
	if (!Buf) {
		return 1;
	}
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

static int cliptext (int delete, int ma, int aa, int mb, int ab)
{
	if (!Buf)
		return 1;

	int ya = ma ? aa : currentaddr();
	int yb = (mb ? ab : ya) + 1;
	if (ya >= yb)
		return 2;

	int start = buf_pos(Buf, 0, ya);
	int end = buf_pos(Buf, 0, yb);
	int len = end - start;
	memcpy(cmdclip, Buf->txt + start, len);
	cmdclip[len + 1] = 0;

	if (delete) {
		memmove(Buf->txt + start, Buf->txt + end, Buf->len - end);
		Buf->len -= len;
		Buf->txt[Buf->len] = 0;
	}

	moveto(ya);
	return 0;
}

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

static int cmd_do_buf (void)
{
	cluf |= UPDATE_BUF;

	while (clri < sizeof(cmdline) && cmdline[clri]) {
		switch (cmdline[clri++]) {
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
			clri--;
			return 0;
		}
	}
	return 0;
}

static int cmd_do_delete (int ma, int aa, int mb, int ab)
{
	return cliptext(1, ma, aa, mb, ab);
}

static int cmd_do_fs (void)
{
	cluf |= UPDATE_BUF;

	while (clri < sizeof(cmdline) && cmdline[clri]) {
		switch (cmdline[clri++]) {
		case 'e':
			{
				struct buf *b;
				if (!(b = buf_create(cmdline + clri))) {
					return 1;
				}
				bufl_push(&BufL, b);
				Buf = b;
			}
			break;
		case 'w':
			if (!Buf)
				return 2;
			sprintf(cmdmsg, "%zu", buf_save(Buf, cmdline + clri));
			break;
		default:
			return 0;
		}
	}
	return 0;
}

static int cmd_do_insert (int ma, int aa, int append)
{
	if (!Buf)
		return 1;

	char *cmdstr = cmdline + clri;
	size_t cmdlen = strlen(cmdstr);
	
	if (Buf->len + cmdlen + 2 >= Buf->siz) {
		fprintf(stderr, "txt should be resized\n"); // FIXME
		return 2;
	}

	int x = 0;
	int y = (ma ? aa : currentaddr()) + append;
	int pos = buf_pos(Buf, x, y);

	memmove(Buf->txt + pos + cmdlen + 1, Buf->txt + pos, Buf->len - pos);
	memcpy(Buf->txt + pos, cmdstr, cmdlen);
	Buf->txt[pos + cmdlen] = '\n';
	Buf->len += cmdlen + 1;
	Buf->txt[Buf->len] = 0;

	moveto(y);

	cluf |= UPDATE_BUF;
	return 0;
}

static int cmd_do_jump (int ma, int aa)
{
	if (!ma)
		return 1;
	buf_scroll(Buf, aa - T.lines / 2);
	term_set_x(0);
	term_set_y(aa - Buf->scroll);
	return 0;
}

static int cmd_do_move (int ma, int aa, int mb, int ab)
{
	int ac = 0;
	int mc = parseaddr(&ac);
	if (!mc)
		return 1;
	if (cmd_do_delete(ma, aa, mb, ab))
		return 2;
	if (cmd_do_paste(mc, ac))
		return 3;
	return 0;
}

static int cmd_do_movement (void)
{
	while (clri < sizeof(cmdline) && cmdline[clri]) {
		switch (cmdline[clri++]) {
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
		}
	}
	return 0;
}

static int cmd_do_paste (int ma, int aa)
{
	if (!Buf)
		return 1;

	size_t len = strlen(cmdclip);

	if (Buf->len + len + 1 >= Buf->siz) {
		fprintf(stderr, "txt should be resized\n"); // FIXME
		return 2;
	}

	int x = 0;
	int y = (ma ? aa : currentaddr()) + 1;
	int pos = buf_pos(Buf, x, y);

	memmove(Buf->txt + pos + len, Buf->txt + pos, Buf->len - pos);
	memcpy(Buf->txt + pos, cmdclip, len);
	Buf->len += len;
	Buf->txt[Buf->len] = 0;

	char *str;
	int count = 0;
	for (str = cmdclip; *str; str++)
		count += ((*str) == '\n');
	moveto(y + count - 1);

	cluf |= UPDATE_BUF;
	return 0;
}

static int cmd_do_scroll (int ma, int aa, int back)
{
	if (!Buf)
		return 1;
	if (ma) {
		buf_scroll(Buf, aa);
		moveto(aa);
	}
	else {
		int y = (back ? -1 : 1) * T.lines;
		buf_scroll(Buf, Buf->scroll + y);
	}
	cluf |= UPDATE_BUF;
	return 0;
}

static int cmd_do_transfer (int ma, int aa, int mb, int ab)
{
	int ac = 0;
	int mc = parseaddr(&ac);
	if (!mc)
		return 1;
	if (cmd_do_yank(ma, aa, mb, ab))
		return 2;
	if (cmd_do_paste(mc, ac))
		return 3;
	return 0;
}

static int cmd_do_yank (int ma, int aa, int mb, int ab)
{
	return cliptext(0, ma, aa, mb, ab);
}

int cmdline_reset (void)
{
	memset(cmdline, 0, sizeof(cmdline));
	clwi = 0;
	return 0;
}

int cmdline_update (char c)
{
	if (c == ASCII_DEL) {
		if (clwi > 0)
			cmdline[--clwi] = 0;
		return 0;
	}
	if (clwi < sizeof(cmdline) - 1) {
		cmdline[clwi++] = c;
		cmdline[clwi] = 0;
	}
	return 0;
}

int cmdline_process (void)
{
	int aa = 0;
	int ma = parseaddr(&aa);
	ma += parsecomma(ma ? NULL : &aa);
	int ab = 0;
	int mb = parseaddr(&ab);

	switch (cmdline[clri++]) {
	case 'F':
		cmd_do_fs();
		break;
	case 'B':
		cmd_do_buf();
		break;
	case 'Z':
		cmd_do_scroll(ma, aa, 1);
		break;
	case 'a':
		cmd_do_insert(ma, aa, 1);
		break;
	case 'i':
		cmd_do_insert(ma, aa, 0);
		break;
	case 'd':
		cmd_do_delete(ma, aa, mb, ab);
		break;
	case 'm':
		cmd_do_move(ma, aa, mb, ab);
		break;
	case 'p':
		cmd_do_movement();
		break;
	case 'q':
		/* check dirty */
		return 1;
	case 't':
		cmd_do_transfer(ma, aa, mb, ab);
		break;
	case 'x':
		cmd_do_paste(ma, aa);
		break;
	case 'y':
		cmd_do_yank(ma, aa, mb, ab);
		break;
	case 'z':
		cmd_do_scroll(ma, aa, 0);
		break;
	case '\0':
		cmd_do_jump(ma, aa);
		break;
	}

	clri = 0;
	cluf |= UPDATE_CMD;
	return 0;
}
