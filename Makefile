CC = gcc
CFLAGS = -std=c90 -Werror -Wall -Wextra
CLIB =

OBJ = main.o term.o buf.o bufl.o cmd.o ascii.o
OUT = ved
ERR = tmp/error.log
DUMMY := main.c
TMPDUMMY = tmp/$(DUMMY)

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $(OUT) $^ $(CLIB)

tmp:
	-mkdir -p tmp 2>/dev/null

$(TMPDUMMY): tmp $(DUMMY)
	cp $(DUMMY) $(TMPDUMMY)

.PHONY: clean
clean:
	-rm *.o 2>/dev/null
	-rm $(OUT)

.PHONY: log
log: tmp
	echo >$(ERR) && tail -f $(ERR)

.PHONY: run
run: $(OUT) $(TMPDUMMY)
	./$(OUT) $(TMPDUMMY) 2>>$(ERR)
