CC = gcc
CFLAGS = -Wall -O2 -fopenmp -pthread -lm

all:
	$(CC) $(CFLAGS) main.c -o mandelbrot

clean:
	rm -f mandelbrot *.pgm times.txt