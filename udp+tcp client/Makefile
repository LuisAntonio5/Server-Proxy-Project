CC = gcc
  CFLAGS = -lsodium -lpthread -Wall

  SRCS = $(wildcard *.c)

  PROGS = $(patsubst %.c,%,$(SRCS))

  all: $(PROGS)

  %: %.c

	$(CC) $< $(CFLAGS)  -o $@
