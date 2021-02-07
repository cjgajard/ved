#ifndef ASCII_h
#define ASCII_h 1
#include <bits/types/FILE.h>
#define ASCII_BS  0x08
#define ASCII_DEL 0x7F
#define ASCII_LF  0x0A
#define ASCII_CR  0x0D

struct ascii {
	unsigned char code;
	char name[4];
	char help[27];
};

extern struct ascii ascii_ctrl[0x21];

int ascii_fprintc (FILE *f, char byte);
#endif
