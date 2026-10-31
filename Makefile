CC = gcc
CFLAGS = -Wall

all: ast recovery

ast: ast.c json_c.c
	$(CC) $(CFLAGS) ast.c -o ast

recovery: all_recovery.c json_c.c
	$(CC) $(CFLAGS) all_recovery.c -o recovery

run: all
	./ast
	./recovery

clean:
	rm -f ast recovery restored.c

.PHONY: all run clean
