all: setitimer-helper squish-pty squish-unix

CC = gcc
CFLAGS = -Wall -W
LDFLAGS = -lm

setitimer-helper: setitimer-helper.o
	$(CC) $(CFLAGS) setitimer-helper.o -o setitimer-helper $(LDFLAGS)

squish-pty: squish-pty.o
	$(CC) $(CFLAGS) squish-pty.o -o squish-pty $(LDFLAGS)

squish-unix: squish-unix.o
	$(CC) $(CFLAGS) squish-unix.o -o squish-unix $(LDFLAGS)

clean: 
	rm -f *.o setitimer-helper squish-pty squish-unix

