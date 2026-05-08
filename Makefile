CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
TARGET := chaosnet
SRC := /home/runner/work/Chaos-Net/Chaos-Net/src/chaosnet.cpp

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

test: $(TARGET)
	/home/runner/work/Chaos-Net/Chaos-Net/tests/test_cli.sh

clean:
	rm -f $(TARGET)
