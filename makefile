CC = gcc
CFLAGS = -Wall -Wextra -O2 -fopenmp -pthread
LDFLAGS = -lm -fopenmp -pthread

TARGET = mandelbrot

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.pgm times.txt

.PHONY: all clean