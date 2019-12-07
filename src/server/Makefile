.SUFFIXES : .c .o

CC = gcc

INC = 
LIBS = -ljson-c -lpaho-mqtt3cs -lrt
CFLAGS = -Wall $(INC)

OBJS = ninano.o
SRCS = ninano.c

TARGET = ninano

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

dep :
	gccmakedep $(INC) $(SRCS)

install :
	cp $(TARGET) /usr/local/bin
	cp alarm.sh /var/ninano/

clean:
	rm -fr $(OBJS) $(TARGET) core

new :
	$(MAKE) clean
	$(MAKE)
