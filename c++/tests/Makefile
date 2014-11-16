CXX=g++
RM=rm -f
override CXXFLAGS:=-std=c++11 -I.. -fPIC -Wall -Wextra -O0 -g3 $(CFLAGS)
override LIB:=../libjksn++.a -lm $(LIB)

OBJ=test_int test_float test_utf test_object test_array test_swap_array test_delta test_parse

.PHONY: all clean

all: $(OBJ)

clean:
	$(RM) $(OBJ)

%: %.cpp ../libjksn++.a
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $< $(LIB)
