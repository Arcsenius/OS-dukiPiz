CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

.PHONY: all clean
all: lab1/sum_parent lab1/sum_child lab2/minmax

lab1/sum_parent: lab1/sum_parent.c
	$(CC) $(CFLAGS) $< -o $@

lab1/sum_child: lab1/sum_child.c
	$(CC) $(CFLAGS) $< -o $@

lab2/minmax: lab2/minmax.c
	$(CC) $(CFLAGS) -pthread $< -o $@

clean:
	rm -f lab1/sum_parent lab1/sum_child lab2/minmax
