CC = gcc
CFLAGS = -std=c17 -Werror -Wall -Wextra
CLIB =

OBJ = main.o term.o buf.o
OUT = ved

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $(OUT) $^ $(CLIB)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	-rm *.o
	-rm $(OUT)

.PHONY: run
run: $(OUT)
	./$(OUT)
