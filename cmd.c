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

int buf_scroll (struct buf *this, int y)
{
	int last;
	if (!this)
		return 1;
	if (y < 0) {
		this->scroll = 0;
		return 0;
	}
	last = buf_lastline(this) - T.lines;
	if (y > last)
		this->scroll = last;
	this->scroll = y;
	return 0;
}

int buf_y (struct buf *this)
{
	return (this ? this->scroll : 0) + T.y;
}

enum addr {
	ADDR_CURRENT = '.',
	ADDR_START = '0',
	ADDR_END = '$',
	ADDR_NEXT = '+',
	ADDR_PREV = '-'
};

int buf_at (struct buf *this, enum addr key)
{
	switch (key) {
	case ADDR_CURRENT:
		return buf_y(this);
	case ADDR_END:
		return buf_lastline(this);
	case ADDR_NEXT:
		return buf_y(this) + 1;
	case ADDR_START:
	default:
		return 0;
	}
}

static int addrdefault (int match, int *y, enum addr def)
{
	if (match)
		return 1;
	if (y)
		*y = buf_at(Buf, def);
	return 0;
}

static int parseaddr (int *ref, int *y_ptr) {
	char byte;
	int y = 0;
	int match = 0;
	int is_rel = 0;
	char memo[16];
	int i = 0;

	if (!Buf)
		return 0;
	if (!(byte = cmdline[clri]))
		return 0;
	
	if (byte == ADDR_CURRENT || byte == ADDR_END) {
		y = buf_at(Buf, byte);
		match += 1;
		byte = cmdline[++clri];
	}

	if (byte == ADDR_NEXT || byte == ADDR_PREV) {
		if (!match)
			y = ref ? *ref : buf_at(Buf, ADDR_CURRENT);
		match += 1;
		is_rel = 1;
		memo[i++] = byte;
		byte = cmdline[++clri];
	}

	while (isdigit(byte)) {
		memo[i++] = byte;
		byte = cmdline[++clri];
	}
	if (is_rel && i == 1)
		memo[i++] = '1';
	memo[i] = 0;

	if (i) {
		y += atoi(memo) - !is_rel;
		match += 1;
	}

	if (y_ptr)
		*y_ptr = y;
	return match;
}

static int parsecomma (int *aa)
{
	switch (cmdline[clri]) {
	case ',':
		clri++;
		if (aa)
			*aa = buf_at(Buf, ADDR_START);
		return 1;
	case ';':
		clri++;
		if (aa)
			*aa = buf_at(Buf, ADDR_CURRENT);
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
	size_t aa, ab, len;
	char *target;

	if (!Buf)
		return 1;

	aa = buf_pos(Buf, ya);
	ab = buf_pos(Buf, yb);
	len = ab - aa;

	target = Buf->txt + aa;
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

static int insert (int y)
{
	char *cmdstr = cmdline + clri;
	size_t cmdlen = strlen(cmdstr);
	int pos;

	if (!Buf)
		return 1;

	cmdstr = cmdline + clri;
	cmdlen = strlen(cmdstr);

	if (Buf->len + cmdlen + 2 >= Buf->siz) {
		fprintf(stderr, "txt should be resized\n"); /* FIXME */
		return 2;
	}

	pos = buf_pos(Buf, y);

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
	int y = (back ? -1 : 1) * T.lines;
	if (!Buf)
		return 1;
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
	int ya = this->ya;
	int yb = this->yb + 1;
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
			sprintf(cmdmsg, "%lu", buf_save(Buf, cmdline + clri));
			break;
		default:
			return 0;
		}
	}
	return 0;
}

static int cmd_do_append (struct command *this)
{
	return insert(this->ya + 1);
}

static int cmd_do_insert (struct command *this)
{
	return insert(this->ya);
}

static int cmd_do_jump (struct command *this)
{
	if (!Buf)
		return 1;
	buf_scroll(Buf, this->ya - T.lines / 2);
	term_set_x(0);
	term_set_y(this->ya - Buf->scroll);
	return 0;
}

static int cmd_do_move (struct command *this)
{
	if (cmd_do_delete(this))
		return 1;
	if (this->yc >= this->yb)
		this->yc -= cmdclip_lines();
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
	int count, y, pos;
	size_t len = strlen(cmdclip);

	if (!Buf)
		return 1;

	if (Buf->len + len + 1 >= Buf->siz) {
		fprintf(stderr, "txt should be resized\n"); /* FIXME */
		return 2;
	}

	y = this->yc + 1;
	pos = buf_pos(Buf, y);

	memmove(Buf->txt + pos + len, Buf->txt + pos, Buf->len - pos);
	memcpy(Buf->txt + pos, cmdclip, len);
	Buf->len += len;
	Buf->txt[Buf->len] = 0;

	count = cmdclip_lines();
	moveto(y + count - 1);

	cluf |= UPDATE_BUF;
	return 0;
}

static int cmd_do_scrollto (struct command *this)
{
	return scroll_to(this->ya);
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
	int ya = this->ya;
	int yb = this->yb + 1;
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
	clri = 0;
	this->edit = 0;
	return 0;
}

int cmd_update (struct command *cmd)
{
	int ma = 0, mb = 0, mc = 0;
	cmd_reset(cmd);
	ma = parseaddr(NULL, &cmd->ya);
	if (parsecomma(ma ? NULL : &cmd->ya)) {
		ma++;
		mb = parseaddr(&cmd->ya, &cmd->yb);
		addrdefault(mb, &cmd->yb, ADDR_END);
	}
	else {
		cmd->yb = cmd->ya;
	}

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
		addrdefault(ma, &cmd->ya, ADDR_CURRENT);
		cmd->Do = &cmd_do_append;
		break;
	case 'd':
		addrdefault(ma, &cmd->ya, ADDR_CURRENT);
		addrdefault(mb, &cmd->yb, ADDR_CURRENT);
		cmd->edit = EDIT_KIL | EDIT_SRC;
		cmd->Do = &cmd_do_delete;
		break;
	case 'i':
		addrdefault(ma, &cmd->ya, ADDR_CURRENT);
		cmd->Do = &cmd_do_insert;
		break;
	case 'm':
		mc = parseaddr(NULL, &cmd->yc);
		addrdefault(ma, &cmd->ya, ADDR_CURRENT);
		addrdefault(mb, &cmd->yb, ADDR_CURRENT);
		addrdefault(mc, &cmd->yc, ADDR_CURRENT);
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
		mc = parseaddr(NULL, &cmd->yc);
		addrdefault(ma, &cmd->ya, ADDR_CURRENT);
		addrdefault(mb, &cmd->yb, ADDR_CURRENT);
		addrdefault(mc, &cmd->yc, ADDR_CURRENT);
		cmd->edit = EDIT_SRC | EDIT_DST;
		cmd->Do = &cmd_do_transfer;
		break;
	case 'x':
		mc = ma;
		cmd->yc = cmd->ya;
		ma = 0;
		cmd->ya = 0;
		addrdefault(mc, &cmd->yc, ADDR_CURRENT);
		cmd->edit = EDIT_DST;
		cmd->Do = &cmd_do_paste;
		break;
	case 'y':
		addrdefault(ma, &cmd->ya, ADDR_CURRENT);
		addrdefault(mb, &cmd->yb, ADDR_CURRENT);
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
