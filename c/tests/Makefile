CC=gcc
RM=rm -f
override CFLAGS:=-I.. -fPIC -Wall -Wextra -O3 -g3 $(CFLAGS)
override LIB:=../libjksn.a -lm $(LIB)

OBJ=test_int test_float test_utf test_object test_array test_swap_array test_delta test_parse

.PHONY: all clean

all: $(OBJ)

clean:
	$(RM) $(OBJ)

%: %.c ../libjksn.a
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $< $(LIB)
