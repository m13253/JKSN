CC=clang
RM=rm -f
CFLAGS=-fPIC -Wall -O1 -g3 -I.. -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins -g3
LDFLAGS=../libjksn.a

OBJ=test_int test_utf test_object test_array test_swap_array test_delta test_parse

.PHONY: all clean

all: $(OBJ)

clean:
	$(RM) $(OBJ)

%: %.c ../libjksn.a
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS)
