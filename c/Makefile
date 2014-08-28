AR=ar
CC=gcc
RM=rm -f
override CFLAGS:=-fPIC -Wall -Wextra -O3 $(CFLAGS)
override LDFLAGS:=-lm $(LDFLAGS)

.PHONY: all clean tests

all: libjksn.a libjksn.so

clean:
	$(RM) libjksn.so libjksn.a jksn.o

tests: libjksn.a
	$(MAKE) -C tests

libjksn.a: jksn.o
	$(AR) crs $@ $^

libjksn.so: jksn.o
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)
