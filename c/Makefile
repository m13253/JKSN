AR=ar
CC=gcc
RM=rm -f
override CFLAGS:=-fPIC -Wall -Wextra -Wsign-compare -Wsign-conversion -O3 $(CFLAGS)
override LIB:=-lm $(LIB)

.PHONY: all clean tests

all: libjksn.a libjksn.so

clean:
	$(RM) libjksn.so libjksn.a jksn.o

tests: libjksn.a
	$(MAKE) -C tests

libjksn.a: jksn.o
	$(AR) crs $@ $^

libjksn.so: jksn.o
	$(CC) $(CFLAGS) $(LDLAGS) -shared -o $@ $^ $(LIB)
