CC = gcc
CFLAGS = -O0 -Wno-unused-result
LDFLAGS =
SRC = $(wildcard *.c)
EXE = client server


all: $(EXE)

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f $(EXE)
