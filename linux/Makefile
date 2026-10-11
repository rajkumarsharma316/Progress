CXX ?= g++
CXXFLAGS = -O3 -std=c++17 -Wall
LDFLAGS = -lX11 -lcairo

TARGET = streakbar
SRCS = main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
