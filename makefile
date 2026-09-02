CC = gcc
CFLAGS = -Wall -O3 -fopenmp -pthread

all: mandelbrot

mandelbrot: main.c
	$(CC) $(CFLAGS) main.c -o mandelbrot -lm

clean:
	rm -f mandelbrot *.pgm times.txt