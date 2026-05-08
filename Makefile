CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
TARGET := chaosnet
SRC := src/chaosnet.cpp

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

test: $(TARGET)
	tests/test_cli.sh

clean:
	rm -f $(TARGET)
