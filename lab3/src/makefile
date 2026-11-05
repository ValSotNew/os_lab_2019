CC = gcc
CFLAGS = -I. -Wall

.PHONY: all clean
all: sequential_min_max parallel_min_max run_sequential_min_max

sequential_min_max: sequential_min_max.c find_min_max.o utils.o
	$(CC) $(CFLAGS) -o $@ $^

parallel_min_max: parallel_min_max.c find_min_max.o utils.o
	$(CC) $(CFLAGS) -o $@ $^

find_min_max.o: find_min_max.c find_min_max.h utils.h
	$(CC) $(CFLAGS) -c -o $@ $<

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c -o $@ $<

run_sequential_min_max: run_sequential_min_max.c | sequential_min_max
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f sequential_min_max parallel_min_max find_min_max.o utils.o run_sequential_min_max