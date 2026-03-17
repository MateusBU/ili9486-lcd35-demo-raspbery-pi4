## Makefile — ILI9486 Demo (Raspberry Pi 4)
##
## Uso:
##   make          → compila
##   make run      → compila e executa com sudo
##   make clean    → remove binários

CC      = gcc
CFLAGS  = -O2 -Wall -Wextra
LDFLAGS = -lwiringPi -lm
TARGET  = demo
SRCS    = main.c ili9486.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) ili9486.h
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)
	@echo "Build OK → sudo ./$(TARGET)"

run: all
	sudo ./$(TARGET)

clean:
	rm -f $(TARGET) *.o