CC=gcc
override CFLAGS+=-fPIC -Wall -O3

.PHONY: all clean

all: libjksn.so

clean:
	rm libjksn.so jksn.o

libjksn.so: jksn.o
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)
