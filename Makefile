CXX = g++
CXXFLAGS = -std=c++23 -O3 -Wall -I. -I./lsp/backend/src -I./lsp/backend/include
TARGET_BASE = vyne_bin
BUILD_DIR = build

ifeq ($(OS),Windows_NT)
    TARGET = $(TARGET_BASE).exe
    # Use mconsole for only windows
    LDFLAGS = -mconsole
    # Use the logic you confirmed works on your PowerShell setup
    MKDIR_P = mkdir $(subst /,\,$(1)) 2>nul || (exit 0)
    RM = if exist $(BUILD_DIR) rd /s /q $(BUILD_DIR)
    DEL = if exist $(TARGET) del /f /q $(TARGET)
    SHELL := cmd.exe
    
else

    TARGET = $(TARGET_BASE)
    LDFLAGS = 
    MKDIR_P = mkdir -p $(1)
    RM = rm -rf $(BUILD_DIR)
    DEL = rm -f $(TARGET)
    
endif

SRCS = $(wildcard *.cpp) \
       $(wildcard cli/*.cpp) \
       $(wildcard vyne/*.cpp) \
       $(wildcard vyne/*/*.cpp) \
       $(wildcard vyne/*/*/*.cpp) \
       $(wildcard vyne/*/*/*/*.cpp) \
       $(wildcard lsp/backend/src/*.cpp) 

OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@$(call MKDIR_P,$(dir $@))
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo Cleaning...
	@$(RM)
	@$(DEL)

.PHONY: all clean