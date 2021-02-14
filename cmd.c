#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "ascii.h"
#include "bufl.h"
#include "cmd.h"
#include "term.h"

char cmdline[256] = {0};
char cmdmsg[256] = {0};
char cmdclip[BUFSIZ];
unsigned int cluf = 0;
static size_t clwi = 0; /* command line write index */
static size_t clri = 0; /* command line read index */

static int cmd_do_append (struct command *this);
static int cmd_do_buf (struct command *this);
static int cmd_do_delete (struct command *this);
static int cmd_do_fs (struct command *this);
static int cmd_do_insert (struct command *this);
static int cmd_do_jump (struct command *this);
static int cmd_do_move (struct command *this);
static int cmd_do_movement (struct command *this);
static int cmd_do_paste (struct command *this);
static int cmd_do_scroll (struct command *this);
static int cmd_do_scrollback (struct command *this);
static int cmd_do_transfer (struct command *this);
static int cmd_do_yank (struct command *this);
static int cmd_do_quit (struct command *this);

int buf_scroll (struct buf *this, int addr)
{
	if (!this)
		return 1;
	if (addr < 0) {
		this->scroll = 0;
		return 0;
	}
	int last = buf_lastline(this) - T.lines;
	if (addr > last)
		this->scroll = last;
	this->scroll = addr;
	return 0;
}

int buf_addr (struct buf *this)
{
	return (this ? this->scroll : 0) + T.y;
}

enum addr {
	ADDR_CURRENT = '.',
	ADDR_START = '0',
	ADDR_END = '$',
	ADDR_NEXT = '+',
	ADDR_PREV = '-',
};

int buf_at (struct buf *this, enum addr key)
{
	switch (key) {
	case ADDR_CURRENT:
		return buf_addr(this);
	case ADDR_END:
		return buf_lastline(this);
	case ADDR_NEXT:
		return buf_addr(this) + 1;
	case ADDR_START:
	default:
		return 0;
	}
}

static int addrdefault (int match, int *addr, enum addr def)
{
	if (match)
		return 1;
	if (addr)
		*addr = buf_at(Buf, def);
	return 0;
}

static int parseaddr (int *addr_ptr) {
	char byte;
	int addr = 0;
	int match = 0;
	int rel = 0;

	if (!Buf)
		return 0;

	if (!(byte = cmdline[clri]))
		return match;

	if (byte == ADDR_CURRENT || byte == ADDR_END) {
		addr = buf_at(Buf, byte);
		match += 1;
		byte = cmdline[clri++];
	}

	if (byte == ADDR_NEXT || byte == ADDR_PREV) {
		if (!match)
			addr = buf_at(Buf, ADDR_CURRENT);
		if (byte == ADDR_NEXT)
			rel = 1;
		if (byte == ADDR_PREV)
			rel = -1;
		match += 1;
		byte = cmdline[clri++];
	}

	if (isdigit(byte) || rel) {
		char memo[16];
		int i = 0;
		while (isdigit(byte)) {
			memo[i++] = byte;
			byte = cmdline[++clri];
		}
		memo[i] = 0;
		addr += i ? atoi(memo) - (rel >= 0 ? 1 : 0) : rel;
		match += 1;
	}

	if (addr_ptr)
		*addr_ptr = addr;
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
		*aa = buf_addr(Buf);
		return 1;
	default:
		return 0;
	}
}

/* ACTIONS --------------------------------------------- */

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

static int cmdclip_lines (void)
{
	char *str;
	int count = 0;
	for (str = cmdclip; *str; str++)
		count += ((*str) == '\n');
	return count;
}

static int cliptext (int ya, int yb, int delete)
{
	if (!Buf)
		return 1;

	size_t aa = buf_pos(Buf, ya);
	size_t ab = buf_pos(Buf, yb);
	size_t len = ab - aa;

	char *target = Buf->txt + aa;
	memcpy(cmdclip, target, len);
	cmdclip[len] = 0;

	if (delete) {
		memmove(target, target + len, Buf->len - aa - len);
		Buf->len -= len;
		Buf->txt[Buf->len] = 0;
	}

	moveto(ya);
	return 0;
}

static int insert (int addr, int append)
{
	if (!Buf)
		return 1;

	char *cmdstr = cmdline + clri;
	size_t cmdlen = strlen(cmdstr);

	if (Buf->len + cmdlen + 2 >= Buf->siz) {
		fprintf(stderr, "txt should be resized\n"); // FIXME
		return 2;
	}

	int y = addr + append;
	int pos = buf_pos(Buf, y);

	memmove(Buf->txt + pos + cmdlen + 1, Buf->txt + pos, Buf->len - pos);
	memcpy(Buf->txt + pos, cmdstr, cmdlen);
	Buf->txt[pos + cmdlen] = '\n';
	Buf->len += cmdlen + 1;
	Buf->txt[Buf->len] = 0;

	moveto(y);

	cluf |= UPDATE_BUF;
	return 0;
}

static int scroll_to (int ya)
{
	if (!Buf)
		return 1;
	buf_scroll(Buf, ya);
	moveto(ya);
	cluf |= UPDATE_BUF;
	return 0;
}

static int scroll_page (int back)
{
	if (!Buf)
		return 1;
	int y = (back ? -1 : 1) * T.lines;
	buf_scroll(Buf, Buf->scroll + y);
	cluf |= UPDATE_BUF;
	return 0;
}

/* COMMANDS --------------------------------------------- */

static int cmd_do_buf (struct command *this)
{
	(void)this;
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

static int cmd_do_delete (struct command *this)
{
	int ya = this->aa;
	int yb = this->ab + 1;
	if (ya >= yb)
		return 1;
	if (cliptext(ya, yb, 1))
		return 2;
	return 0;
}

static int cmd_do_fs (struct command *this)
{
	(void)this;
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

static int cmd_do_append (struct command *this)
{
	return insert(this->aa, 1);
}

static int cmd_do_insert (struct command *this)
{
	return insert(this->aa, 0);
}

static int cmd_do_jump (struct command *this)
{
	if (!Buf)
		return 1;
	buf_scroll(Buf, this->aa - T.lines / 2);
	term_set_x(0);
	term_set_y(this->aa - Buf->scroll);
	return 0;
}

static int cmd_do_move (struct command *this)
{
	if (cmd_do_delete(this))
		return 1;
	if (this->ac >= this->ab)
		this->ac -= cmdclip_lines();
	if (cmd_do_paste(this))
		return 2;
	return 0;
}

static int cmd_do_movement (struct command *this)
{
	(void)this;
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

static int cmd_do_paste (struct command *this)
{
	if (!Buf)
		return 1;

	size_t len = strlen(cmdclip);

	if (Buf->len + len + 1 >= Buf->siz) {
		fprintf(stderr, "txt should be resized\n"); // FIXME
		return 2;
	}

	int y = this->ac + 1;
	int pos = buf_pos(Buf, y);

	memmove(Buf->txt + pos + len, Buf->txt + pos, Buf->len - pos);
	memcpy(Buf->txt + pos, cmdclip, len);
	Buf->len += len;
	Buf->txt[Buf->len] = 0;

	int count = cmdclip_lines();
	moveto(y + count - 1);

	cluf |= UPDATE_BUF;
	return 0;
}

static int cmd_do_scrollto (struct command *this)
{
	return scroll_to(this->aa);
}

static int cmd_do_scroll (struct command *this)
{
	(void)this;
	return scroll_page(0);
}

static int cmd_do_scrollback (struct command *this)
{
	(void)this;
	return scroll_page(1);
}

static int cmd_do_transfer (struct command *this)
{
	if (cmd_do_yank(this))
		return 2;
	if (cmd_do_paste(this))
		return 3;
	return 0;
}

static int cmd_do_yank (struct command *this)
{
	int ya = this->aa;
	int yb = this->ab + 1;
	if (ya >= yb)
		return 1;
	if (cliptext(ya, yb, 0))
		return 2;
	return 0;
}

static int cmd_do_quit (struct command *this)
{
	(void)this;
	/* TODO check dirty */
	return CMD_QUIT;
}

/* METHODS --------------------------------------------- */

int cmdline_reset (void)
{
	cmdline[0] = 0;
	clwi = 0;
	return 0;
}

int cmdline_update (char c)
{
	if (c == ASCII_CR || c == ASCII_LF)
		return 1;

	if (c == ASCII_BS || c == ASCII_DEL) {
		if (clwi > 0)
			cmdline[--clwi] = 0;
		return 0;
	}

	if (clwi >= sizeof(cmdline) - 1)
		return 0;
	cmdline[clwi++] = c;
	cmdline[clwi] = 0;
	return 0;
}

int cmd_reset (struct command *this)
{
	this->edit = 0;
	return 0;
}

int cmd_update (struct command *cmd)
{
	clri = 0;
	int ma, mb, mc = 0;
	ma = parseaddr(&cmd->aa);
	ma += parsecomma(ma ? NULL : &cmd->aa);
	mb = parseaddr(&cmd->ab);
	if (ma && !mb) {
		mb = ma;
		cmd->ab = cmd->aa;
	}
	cmd->edit = 0;

	switch (cmdline[clri++]) {
	case '\0':
		cmd->edit = ma ? EDIT_MOV : 0;
		cmd->Do = &cmd_do_jump;
		break;
	case 'F':
		cmd->Do = &cmd_do_fs;
		break;
	case 'B':
		cmd->Do = &cmd_do_buf;
		break;
	case 'Z':
		cmd->Do = &cmd_do_scrollback;
		break;
	case 'a':
		addrdefault(ma, &cmd->aa, ADDR_CURRENT);
		cmd->Do = &cmd_do_append;
		break;
	case 'd':
		addrdefault(ma, &cmd->aa, ADDR_CURRENT);
		addrdefault(mb, &cmd->ab, ADDR_CURRENT);
		cmd->edit = EDIT_KIL | EDIT_SRC;
		cmd->Do = &cmd_do_delete;
		break;
	case 'i':
		addrdefault(ma, &cmd->aa, ADDR_CURRENT);
		cmd->Do = &cmd_do_insert;
		break;
	case 'm':
		mc = parseaddr(&cmd->ac);
		addrdefault(ma, &cmd->aa, ADDR_CURRENT);
		addrdefault(mb, &cmd->ab, ADDR_CURRENT);
		addrdefault(mc, &cmd->ac, ADDR_CURRENT);
		cmd->edit = EDIT_KIL | EDIT_SRC | EDIT_DST;
		cmd->Do = &cmd_do_move;
		break;
	case 'p':
		cmd->Do = &cmd_do_movement;
		break;
	case 'q':
		cmd->Do = &cmd_do_quit;
		break;
	case 't':
		mc = parseaddr(&cmd->ac);
		addrdefault(ma, &cmd->aa, ADDR_CURRENT);
		addrdefault(mb, &cmd->ab, ADDR_CURRENT);
		addrdefault(mc, &cmd->ac, ADDR_CURRENT);
		cmd->edit = EDIT_SRC | EDIT_DST;
		cmd->Do = &cmd_do_transfer;
		break;
	case 'x':
		mc = ma;
		cmd->ac = cmd->aa;
		ma = 0;
		cmd->aa = 0;
		addrdefault(mc, &cmd->ac, ADDR_CURRENT);
		cmd->edit = EDIT_DST;
		cmd->Do = &cmd_do_paste;
		break;
	case 'y':
		addrdefault(ma, &cmd->aa, ADDR_CURRENT);
		addrdefault(mb, &cmd->ab, ADDR_CURRENT);
		cmd->edit = EDIT_SRC;
		cmd->Do = &cmd_do_yank;
		break;
	case 'z':
		cmd->edit = EDIT_MOV;
		cmd->Do = ma ? &cmd_do_scrollto : &cmd_do_scroll;
		break;
	default:
		cmd->Do = NULL;
		break;
	}
	return 0;
}
