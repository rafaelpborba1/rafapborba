CC = gcc
CFLAGS = -Wall -Wextra -O3 -fopenmp -std=c99 -D_POSIX_C_SOURCE=199309L
LDFLAGS = -fopenmp -lpthread -lm

TARGET = mandelbrot
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.pgm times.txt evidencias.log

.PHONY: all clean
