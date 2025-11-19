# Makefile for QQ Monitor

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -luser32 -lgdi32
TARGET = qq_monitor.exe
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	del /f $(TARGET)

run: $(TARGET)
	$(TARGET)

.PHONY: all clean run

