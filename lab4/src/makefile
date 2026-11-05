CC = gcc
AR = ar
CFLAGS = -std=c17 -Wall

LAB3_DIR = ../../lab3/src
PROGRAMS = parallel_min_max process_memory psum
PARALLEL_SOURCES = parallel_min_max.c $(LAB3_DIR)/find_min_max.c $(LAB3_DIR)/utils.c

.PHONY: all clean
all: $(PROGRAMS)

parallel_min_max: $(PARALLEL_SOURCES) $(LAB3_DIR)/find_min_max.h $(LAB3_DIR)/utils.h
	$(CC) $(CFLAGS) -I$(LAB3_DIR) -o $@ $(PARALLEL_SOURCES)

process_memory: process_memory.c
	$(CC) $(CFLAGS) -o $@ $<

psum: parallel_sum.c sum.h $(LAB3_DIR)/utils.c $(LAB3_DIR)/utils.h libsum.a
	$(CC) $(CFLAGS) -pthread -I$(LAB3_DIR) -o $@ $< $(LAB3_DIR)/utils.c libsum.a

libsum.a: sum.o
	$(AR) rcs $@ $<

sum.o: sum.c sum.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROGRAMS) sum.o libsum.a