#
# To compile, type "make" or make "all"
# To remove files, type "make clean"
#
OBJS = server.o request.o segel.o queue.o
TARGET = server

CC = gcc
CFLAGS = -g -Wall 

LIBS = -lpthread -lm

.SUFFIXES: .c .o 

all: server

server: server.o request.o segel.o queue.o
	$(CC) $(CFLAGS) -o server server.o request.o segel.o queue.o $(LIBS)


.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) server 
