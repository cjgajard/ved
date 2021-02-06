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
size_t clri = 0;
size_t clwi = 0;
unsigned int cluf = 0;

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

int buf_addr (struct buf *this)
{
	return (this ? this->scroll : 0) + T.y;
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
			*addr = buf_addr(Buf);
			match += 1;
			continue;
		}
		if (byte == '-' || byte == '+') {
			if (!Buf)
				return 0;
			if (!match)
				*addr = buf_addr(Buf);
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

	size_t start = 0;
	size_t len = buf_cliplen(ya, yb, &start);

	char *target = Buf->txt + start;
	memcpy(cmdclip, target, len);
	cmdclip[len] = 0;

	if (delete) {
		memmove(target, target + len, Buf->len - start -len);
		Buf->len -= len;
		Buf->txt[Buf->len] = 0;
	}

	moveto(ya);
	return 0;
}

static int insert (int ma, int aa, int append)
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
	int y = (ma ? aa : buf_addr(Buf)) + append;
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

static int scroll (int ma, int aa, int back)
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
	int ya = this->ma ? this->aa : buf_addr(Buf);
	int yb = (this->mb ? this->ab : ya) + 1;
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
	return insert(this->ma, this->aa, 1);
}

static int cmd_do_insert (struct command *this)
{
	return insert(this->ma, this->aa, 0);
}

static int cmd_do_jump (struct command *this)
{
	if (!this->ma)
		return 1;
	if (!Buf)
		return 2;
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

	int y = (this->mc ? this->ac : buf_addr(Buf)) + 1;
	int pos = buf_pos(Buf, 0, y);

	memmove(Buf->txt + pos + len, Buf->txt + pos, Buf->len - pos);
	memcpy(Buf->txt + pos, cmdclip, len);
	Buf->len += len;
	Buf->txt[Buf->len] = 0;

	int count = cmdclip_lines();
	moveto(y + count - 1);

	cluf |= UPDATE_BUF;
	return 0;
}

static int cmd_do_scroll (struct command *this)
{
	return scroll(this->ma, this->aa, 0);
}

static int cmd_do_scrollback (struct command *this)
{
	return scroll(this->ma, this->aa, 1);
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
	int ya = this->ma ? this->aa : buf_addr(Buf);
	int yb = (this->mb ? this->ab : ya) + 1;
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
	if (c == ASCII_DEL || c == ASCII_BS) {
		if (clwi > 0)
			cmdline[--clwi] = 0;
		return 0;
	}
	if (clwi >= sizeof(cmdline) - 1)
		return 1;
	cmdline[clwi++] = c == CTRL('m') ? 0 : c;
	cmdline[clwi] = 0;
	return 0;
}

int cmdline_read (struct command *cmd)
{
	clri = 0;
	cmd->ma = parseaddr(&cmd->aa);
	cmd->ma += parsecomma(cmd->ma ? NULL : &cmd->aa);
	cmd->mb = parseaddr(&cmd->ab);
	cmd->mc = 0;
	cmd->ac = 0;
	cmd->edit = 0;

	switch (cmdline[clri++]) {
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
		cmd->Do = &cmd_do_append;
		break;
	case 'i':
		cmd->Do = &cmd_do_insert;
		break;
	case 'd':
		cmd->edit = EDIT_KIL | EDIT_SRC;
		cmd->Do = &cmd_do_delete;
		break;
	case 'm':
		cmd->edit = EDIT_KIL | EDIT_SRC;
		cmd->mc = parseaddr(&cmd->ac);
		cmd->Do = &cmd_do_move;
		break;
	case 'p':
		cmd->Do = &cmd_do_movement;
		break;
	case 'q':
		cmd->Do = &cmd_do_quit;
		break;
	case 't':
		cmd->edit = EDIT_SRC;
		cmd->mc = parseaddr(&cmd->ac);
		cmd->Do = &cmd_do_transfer;
		break;
	case 'x':
		cmd->mc = cmd->ma;
		cmd->ac = cmd->aa;
		cmd->Do = &cmd_do_paste;
		break;
	case 'y':
		cmd->edit = EDIT_SRC;
		cmd->Do = &cmd_do_yank;
		break;
	case 'z':
		cmd->edit = EDIT_MOV;
		cmd->Do = &cmd_do_scroll;
		break;
	case '\0':
		cmd->edit = cmd->ma ? EDIT_MOV : 0;
		cmd->Do = &cmd_do_jump;
		break;
	default:
		cmd->Do = NULL;
		break;
	}
	return 0;
}
