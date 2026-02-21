CXX = g++
CXXFLAGS = -std=c++23 -O3 -Wall -I.
LDFLAGS = -mconsole
TARGET_BASE = vyne
BUILD_DIR = build

# 1. OS DETECTION & COMMAND SETUP
ifeq ($(OS),Windows_NT)
    TARGET = $(TARGET_BASE).exe
    # Use the logic you confirmed works on your PowerShell setup
    MKDIR_P = mkdir $(subst /,\,$(1)) 2>nul || (exit 0)
    RM = if exist $(BUILD_DIR) rd /s /q $(BUILD_DIR)
    DEL = if exist $(TARGET) del /f /q $(TARGET)
    # Force CMD to prevent the /usr/bin/bash conflict you had
    SHELL := cmd.exe
else
    TARGET = $(TARGET_BASE)
    MKDIR_P = mkdir -p $(1)
    RM = rm -rf $(BUILD_DIR)
    DEL = rm -f $(TARGET)
endif

# 2. SOURCE SEARCH
SRCS = $(wildcard *.cpp) \
       $(wildcard cli/*.cpp) \
       $(wildcard vyne/*.cpp) \
       $(wildcard vyne/*/*.cpp) \
       $(wildcard vyne/*/*/*.cpp) \
       $(wildcard vyne/*/*/*/*.cpp)

OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# 3. COMPILATION RULE
$(BUILD_DIR)/%.o: %.cpp
	@$(call MKDIR_P,$(dir $@))
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo Cleaning...
	@$(RM)
	@$(DEL)

.PHONY: all clean