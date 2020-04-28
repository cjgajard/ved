#ifndef SCREEN_h
#define SCREEN_h 1

struct screen {
	int lsiz;
	char **lines;
};

extern struct screen *Screen;

struct screen *screen_new (void);
void screen_destroy (struct screen *this);
#endif
