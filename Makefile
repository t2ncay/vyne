CXX = g++
CC = gcc

TARGET_BASE = vynec
BUILD_DIR = build
URAGE_INCLUDES = -I./vendor/urage/core/include -I./vendor/urage/core/src
URAGE_SRCS = ./vendor/urage/core/src/database_api.c \
             ./vendor/urage/core/src/database.c \
             ./vendor/urage/core/src/btree.c \
             ./vendor/urage/core/src/storage.c \
             ./vendor/urage/core/src/pager.c \
             ./vendor/urage/core/src/type.c

CXXFLAGS = -std=c++23 -O3 -Wall -I. \
           -I./lsp/backend/src -I./lsp/backend/include \
           -DCPPHTTPLIB_OPENSSL_SUPPORT

ifeq ($(OS),Windows_NT)
    TARGET = $(TARGET_BASE).exe
    URAGE_LIB = urage.dll
    URAGE_CFLAGS = -shared -DURAGE_BUILD_SHARED
    LDFLAGS = -mconsole -pthread
    LDFLAGS += -lshell32
    MKDIR_P = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
    RM = if exist $(BUILD_DIR) rd /s /q $(BUILD_DIR)
    DEL = if exist $(TARGET) del /f /q $(TARGET)
    DEL_URAGE = if exist $(URAGE_LIB) del /f /q $(URAGE_LIB)
    SHELL := cmd.exe
    
else

    TARGET = $(TARGET_BASE)
    URAGE_LIB = liburage.so
    URAGE_CFLAGS = -shared -fPIC
    LDFLAGS = -lssl -lcrypto -ldl -pthread
    MKDIR_P = mkdir -p $(1)
    RM = rm -rf $(BUILD_DIR)
    DEL = rm -f $(TARGET)
    DEL_URAGE = rm -f $(URAGE_LIB)
    
endif

SRCS = $(wildcard *.cpp) \
       $(wildcard cli/*.cpp) \
       $(wildcard vyne/*.cpp) \
       $(wildcard vyne/*/*.cpp) \
       $(wildcard vyne/*/*/*.cpp) \
       $(wildcard vyne/*/*/*/*.cpp) \
       $(wildcard editors/vscode/lsp/backend/src/*.cpp) 

OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))

all: $(URAGE_LIB) $(TARGET)

$(URAGE_LIB): $(URAGE_SRCS)
	$(CC) $(URAGE_CFLAGS) $(URAGE_INCLUDES) $(URAGE_SRCS) -o $(URAGE_LIB)

$(TARGET): $(OBJS) $(URAGE_LIB)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@$(call MKDIR_P,$(dir $@))
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo Cleaning...
	@$(RM)
	@$(DEL)
	@$(DEL_URAGE)

.PHONY: all clean