CC = gcc
CFLAGS = -Wall -Wextra -pthread
LIBS = -pthread

# Assuming libunp.a is in the parent unpv13e-master directory
LIBUNP = ../unpv13e-master/libunp.a

all: lab4 client_test

lab4: lab4.c
	$(CC) $(CFLAGS) -o lab4 lab4.c $(LIBS)

# If you need to link with libunp.a, use this instead:
# lab4: lab4.c
# 	$(CC) $(CFLAGS) -o lab4 lab4.c $(LIBUNP) $(LIBS)

client_test: client_test.c
	$(CC) $(CFLAGS) -o client_test client_test.c

clean:
	rm -f lab4 client_test

.PHONY: all clean

