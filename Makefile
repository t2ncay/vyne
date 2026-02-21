CXX = g++
CXXFLAGS = -std=c++23 -O3 -Wall -I.
LDFLAGS = -mconsole
TARGET = vyne.exe

SRCS = $(wildcard *.cpp) \
	   $(wildcard cli/*.cpp) \
	   $(wildcard vyne/*.cpp) \
       $(wildcard vyne/*/*.cpp) \
       $(wildcard vyne/*/*/*.cpp) \
       $(wildcard vyne/*/*/*/*.cpp)

OBJS = $(SRCS:.cpp=.o)

$(info >>> FILES FOUND: $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

check:
	@echo "Checking root:"
	@dir /b vyne
	@echo "Checking modules:"
	@dir /b vyne\modules

clean:
	@if exist vyne\*.o del /S /Q vyne\*.o
	@if exist $(TARGET) del $(TARGET)

.PHONY: all clean check