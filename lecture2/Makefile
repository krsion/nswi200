CC = gcc
CFLAGS = -Wall

TARGETS = recursion recursion4 recursion5

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
