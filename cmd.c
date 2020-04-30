#include <ctype.h>
#include <string.h>
#include <stdlib.h>
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

static int cmd_process_fs (void)
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

static int cmd_process_mv (void)
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

static int cmd_process_insert (int append)
{
	struct buf *b;
	if (bufl_read(BufL, &b))
		return 1;

	char *cmdstr = cmd + cmdri;
	size_t cmdlen = strlen(cmdstr);
	if (b->len + cmdlen + 1 >= b->siz) {
		fprintf(stderr, "txt should be resized\n"); // FIXME
		return 2;
	}

	int pos = buf_pos(b, T.x, T.y);
	if (append)
		pos++;
	memmove(b->txt + pos + cmdlen, b->txt + pos, b->len - pos);
	b->len += cmdlen;
	b->txt[b->len + 1] = 0;
	memcpy(b->txt + pos, cmdstr, cmdlen);

	cmduf |= UPDATE_BUF;
	return 0;
}

static int cmd_process_scroll (int a, int ma)
{
	struct buf *b;
	if (bufl_read(BufL, &b))
		return 1;
	int z = ma ? a : T.y;
	b->scroll = z >= 0 ? z : 0;
	cmduf |= UPDATE_BUF;
	return 0;
}

int buf_lastline (struct buf *this)
{
	int addr = 0;
	size_t fpos = 0;
	char byte;
	while (fpos < this->siz && (byte = this->txt[fpos++])) {
		if (byte == '\n')
			addr++;
	}
	return addr;
}

static int cmd_parseaddr (int *addr, int second) {
	char byte;
	int rel = 0;
	int match = 0;
	*addr = 0;
	
	while ((byte = cmd[cmdri++])) {
		if (second && !match && byte == ',') {
			continue;
		}
		if (byte == '$') {
			struct buf *buf;
			if (bufl_read(BufL, &buf))
				return 2;
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
			*addr = T.y;
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

int cmd_process (void)
{
	fprintf(stderr, "\n"); // DEV
	int ra = 0; /* range A */
	int ma = cmd_parseaddr(&ra, 0); /* match A */
	fprintf(stderr, "ra=%d\n", ra); // DEV

	// int rb = 0; /* range B */
	// int mb = cmd_parseaddr(&rangeB, 0); /* match B */
	// fprintf(stderr, "rb=%d\n", rb); // DEV
	fprintf(stderr, "cmd=%s cmdri=%zu\n", cmd, cmdri); // DEV

	switch (cmd[cmdri++]) {
	case 'F':
		cmd_process_fs();
		break;
	case 'm':
		cmd_process_mv();
		break;
	case 'z':
		cmd_process_scroll(ra, ma);
		break;
	case 'a':
		cmd_process_insert(1);
		break;
	case 'i':
		cmd_process_insert(0);
		break;
	case '\0':
		{
			struct buf *b;
			if (bufl_read(BufL, &b))
				break;
			if (ra >= b->scroll && ra < b->scroll + T.lines) {
				T.y = ra - b->scroll;
				if (T.y < 0)
					T.y = 0;
			}
			else {
				b->scroll = ra - T.y;
				if (b->scroll < 0) {
					b->scroll = 0;
					T.y = ra;
				}
				cmduf |= UPDATE_BUF;
			}
		}
		break;
	}

	cmdri = 0;
	cmduf |= UPDATE_CMD;
	return 0;
}
