.SUFFIXES : .c .o

CC = gcc

INC = 
LIBS = -lcgic
CFLAGS = -Wall $(INC)

OBJS = graph_am2315.o
SRCS = graph_am2315.c

TARGET = graph_am2315.cgi

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

dep :
	gccmakedep $(INC) $(SRCS)

install :
	cp $(TARGET) /usr/lib/cgi-bin/

clean:
	rm -fr $(OBJS) $(TARGET) core

new :
	$(MAKE) clean
	$(MAKE)
