AR=ar
CC=gcc
RM=rm -f
override CFLAGS+=-fPIC -Wall -O3
override LDFLAGS+=-lm

.PHONY: all clean

all: libjksn.a libjksn.so

clean:
	$(RM) libjksn.so libjksn.a jksn.o

libjksn.a: jksn.o
	$(AR) crs $@ $^

libjksn.so: jksn.o
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)
