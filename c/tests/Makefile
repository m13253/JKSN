CC=clang
CFLAGS=-fPIC -Wall -O3 -g3 -I.. -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins -g3
LDFLAGS=../libjksn.a

all: test_int test_utf

%: %.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)
