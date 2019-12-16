CC = gcc
  CFLAGS = -lsodium -lpthread

  SRCS = $(wildcard *.c)

  PROGS = $(patsubst %.c,%,$(SRCS))

  all: $(PROGS)

  %: %.c

	$(CC) $< $(CFLAGS)  -o $@
