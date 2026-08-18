CXX = g++
CC = gcc

TARGET_BASE = vynec
BUILD_DIR = build

RAYLIB_INCLUDE = -I./vendor/raylib/include
RAYLIB_LIB_PATH = -L./vendor/raylib/lib

URAGE_INCLUDES = -I./vendor/urage/core/include -I./vendor/urage/core/src
URAGE_SRCS = ./vendor/urage/core/src/database_api.c \
             ./vendor/urage/core/src/database.c \
             ./vendor/urage/core/src/btree.c \
             ./vendor/urage/core/src/storage.c \
             ./vendor/urage/core/src/pager.c \
             ./vendor/urage/core/src/type.c

CXXFLAGS = -std=c++23 -O3 -I. \
            $(RAYLIB_INCLUDE) \
           -I./lsp/backend/src -I./lsp/backend/include \
           -DCPPHTTPLIB_OPENSSL_SUPPORT -D_WIN32 -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER

ifeq ($(OS),Windows_NT)
    TARGET = $(TARGET_BASE).exe
    URAGE_LIB = urage.dll
    URAGE_CFLAGS = -shared -DURAGE_BUILD_SHARED
    RAYLIB_LIB_PATH = -L./vendor/raylib/lib
    LDFLAGS = -mconsole -pthread $(RAYLIB_LIB_PATH) -static -static-libgcc -static-libstdc++
    LDFLAGS += -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lwinpthread -lssl -lcrypto -lcrypt32 -lbcrypt -lws2_32
    MKDIR_P = mkdir -p $(1)
    RM = rm -rf $(BUILD_DIR)
    DEL = rm -f $(TARGET)
    DEL_URAGE = rm -f $(URAGE_LIB)
    
else

    TARGET = $(TARGET_BASE)
    URAGE_LIB = liburage.so
    URAGE_CFLAGS = -shared -fPIC
    LDFLAGS = -lssl -lcrypto -ldl -pthread
    LDFLAGS += -lraylib -lGL -lm -lrt -lX11
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

check-copies:
	@echo "Scanning for basic compiler copy/move warnings..."
	@rm -f warning.txt
	@$(foreach src,$(SRCS), \
		echo "Checking $(src)..."; \
		$(CXX) $(CXXFLAGS) -Wpessimizing-move -Wredundant-move -fsyntax-only -fsanitize=leak $(src) >> warning.txt 2>&1 || true; \
	)
	@echo "Scan complete. Results written to warning.txt"

SAN_FLAGS = -fsanitize=address,leak -g -O1

check-leaks:
	@echo "Building Vyne with LeakSanitizer..."
	$(CXX) $(CXXFLAGS) $(SAN_FLAGS) main.cpp $(filter-out main.cpp, $(SRCS)) -o vyne_leak_test $(LDFLAGS)
	@echo "Build complete. Running executable (close the game/CLI normally to view leak report)..."
	./vyne_leak_test tests/network/socket.vy
	@echo "Leak check execution finished."


.PHONY: all clean check-copies check-leaks