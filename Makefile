CC = gcc
CFLAGS = -I. -std=gnu99 -Wall -O2 -g
LFLAGS = -lm  -L. -ltiepie

ifeq ($(OS),Windows_NT)
  TARGET := blockspeedtest.exe
  RM := del
else
  TARGET := blockspeedtest
  RM := rm -f
endif

all : $(TARGET)

clean :
	$(RM) $(TARGET)

$(TARGET) : blockspeedtest.c
	$(CC) $(CFLAGS) -o $@ $< $(LFLAGS)
