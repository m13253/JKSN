AR=ar
CC=gcc
override CFLAGS+=-fPIC -Wall -O3

.PHONY: all clean

all: libjksn.a libjksn.so

clean:
	rm libjksn.so libjksn.a jksn.o

libjksn.a: jksn.o
	$(AR) crs $@ $^

libjksn.so: jksn.o
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)
