CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread

SRCS = main.cpp memory.cpp interconnect.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = sim

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
