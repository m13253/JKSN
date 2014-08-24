CC=clang
RM=rm -f
CFLAGS=-fPIC -Wall -O3 -g3 -I.. -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins -g3
LDFLAGS=../libjksn.a

OBJ=test_int test_utf test_object

.PHONY: all clean

all: $(OBJ)

clean:
	$(RM) $(OBJ)

%: %.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)
