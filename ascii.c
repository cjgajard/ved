#include <ctype.h>
#include <stdio.h>
#include "ascii.h"

struct ascii ascii_ctrl[0x21] = {
	{ 0, "NUL", "null"},
	{ 1, "SOH", "start of heading"},
	{ 2, "STX", "start of text"},
	{ 3, "ETX", "end of text"},
	{ 4, "EOT", "end of transmission"},
	{ 5, "ENQ", "enquiry"},
	{ 6, "ACK", "acknowledge"},
	{ 7, "BEL", "bell"},
	{ 8,  "BS", "backspace"},
	{ 9, "TAB", "tab"},
	{10,  "LF", "line feed"},
	{11,  "VT", "vertical tab"},
	{12,  "FF", "form feed"},
	{13,  "CR", "carriage return"},
	{14,  "SO", "shift out"},
	{15,  "SI", "shift in"},
	{16, "DLE", "data link escape"},
	{17, "DC1", "device control 1"},
	{18, "DC2", "device control 2"},
	{19, "DC3", "device control 3"},
	{20, "DC4", "device control 4"},
	{21, "NAK", "negative acknowledge"},
	{22, "SYN", "synchronous idle"},
	{23, "ETB", "end of trans. block"},
	{24, "CAN", "cancel"},
	{25,  "EM", "end of medium"},
	{26, "SUB", "substitute"},
	{27, "ESC", "escape"},
	{28,  "FS", "file separator"},
	{29,  "GS", "group separator"},
	{30,  "RS", "record separator"},
	{31,  "US", "unit separator"},
	{127, "DEL", "delete"},
};

int ascii_fprintc (FILE *f, char byte)
{
	int w = 0;
	unsigned char c = byte;
	if (isprint(c)) {
		w += fprintf(f, "%c", c);
	}
	else if (iscntrl(c)) {
		struct ascii a = ascii_ctrl[c == 0x7f ? 0x20 : c];
		w += fprintf(f, "<%s>", a.name);
	}
	else {
		w += fprintf(f, "<0x%02x>", c);
	}
	return w;
}
