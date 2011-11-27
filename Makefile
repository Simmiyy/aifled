CC = gcc
CFLAGS = -march=i486 -mtune=native #-DDEBUG
OBJFILES = aifled.o
PROGRAM = aifled 
VERSION = 0.1

all: $(OBJFILES)
	$(CC) $(CFLAGS) $(OBJFILES) -o $(PROGRAM)

clean:
	-rm $(OBJFILES)

dist:
	-cd .. ; tar czf aifled-$(VERSION).tar.gz aifled-$(VERSION)
