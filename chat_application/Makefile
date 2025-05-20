CC = gcc
CFLAGS = -Wall -pthread -Iinclude
TARGET = main
SRCDIR = source
INCDIR = include
DEBUGDIR = debug

all: $(DEBUGDIR)/$(TARGET)

$(DEBUGDIR)/$(TARGET): $(SRCDIR)/main.c
	@mkdir -p $(DEBUGDIR)
	$(CC) $(CFLAGS) -o $(DEBUGDIR)/$(TARGET) $(SRCDIR)/main.c

clean:
	rm -f $(DEBUGDIR)/$(TARGET)