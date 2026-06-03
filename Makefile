CC = gcc
CFLAGS = -Wall

TARGETS = recursion recursion4 recursion5 malloc \
          readers_writers readers_writers_starve

all: $(TARGETS)

readers_writers: readers_writers.c
	$(CC) $(CFLAGS) -o $@ $< -lpthread

readers_writers_starve: readers_writers_starve.c
	$(CC) $(CFLAGS) -o $@ $< -lpthread

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
