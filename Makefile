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
           -DCPPHTTPLIB_OPENSSL_SUPPORT -D_WIN32 -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER \
           -MMD -MP

# Test flags (no optimization for better error messages)
TEST_CXXFLAGS = -std=c++23 -O0 -g -I. \
                 $(RAYLIB_INCLUDE) \
                 -I./lsp/backend/src -I./lsp/backend/include \
                 -DCPPHTTPLIB_OPENSSL_SUPPORT -D_WIN32 -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER \
                 -MMD -MP

ifeq ($(OS),Windows_NT)
    TARGET = $(TARGET_BASE).exe
    TEST_TARGET = vyne_test.exe
    URAGE_LIB = urage.dll
    URAGE_CFLAGS = -shared -DURAGE_BUILD_SHARED
    RAYLIB_LIB_PATH = -L./vendor/raylib/lib
    LDFLAGS = -mconsole -pthread $(RAYLIB_LIB_PATH) -static -static-libgcc -static-libstdc++
    LDFLAGS += -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lwinpthread -lssl -lcrypto -lcrypt32 -lbcrypt -lws2_32
    MKDIR_P = mkdir -p $(1)
    RM = rm -rf $(BUILD_DIR)
    DEL = rm -f $(TARGET)
    DEL_URAGE = rm -f $(URAGE_LIB)
    DEL_TEST = rm -f $(TEST_TARGET)
    PYTHON = python
    SEP = \\
    
else
    TARGET = $(TARGET_BASE)
    TEST_TARGET = vyne_test
    URAGE_LIB = liburage.so
    URAGE_CFLAGS = -shared -fPIC
    LDFLAGS = -lssl -lcrypto -ldl -pthread
    LDFLAGS += -lraylib -lGL -lm -lrt -lX11
    MKDIR_P = mkdir -p $(1)
    RM = rm -rf $(BUILD_DIR)
    DEL = rm -f $(TARGET)
    DEL_URAGE = rm -f $(URAGE_LIB)
    DEL_TEST = rm -f $(TEST_TARGET)
    PYTHON = python3
    SEP = /
endif

SRCS = $(wildcard *.cpp) \
       $(wildcard cli/*.cpp) \
       $(wildcard vyne/*.cpp) \
       $(wildcard vyne/*/*.cpp) \
       $(wildcard vyne/*/*/*.cpp) \
       $(wildcard vyne/*/*/*/*.cpp) \
       $(wildcard editors/vscode/lsp/backend/src/*.cpp)

OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))
DEPS = $(OBJS:.o=.d)

# ---- Build Targets ----

all: $(URAGE_LIB) $(TARGET)

$(URAGE_LIB): $(URAGE_SRCS)
	$(CC) $(URAGE_CFLAGS) $(URAGE_INCLUDES) $(URAGE_SRCS) -o $(URAGE_LIB)

$(TARGET): $(OBJS) $(URAGE_LIB)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@$(call MKDIR_P,$(dir $@))
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- Test Build ----

$(TEST_TARGET): $(OBJS) $(URAGE_LIB)
	$(CXX) $(OBJS) -o $(TEST_TARGET) $(LDFLAGS)

# ---- Test Runner ----

TEST_DIR = tests
TEST_RESULTS = test_results.txt

# Color codes for terminal output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
CYAN = \033[0;36m
RESET = \033[0m
BOLD = \033[1m

# Test categories
TEST_COMPILER = $(wildcard $(TEST_DIR)/compiler/*.vy) \
                $(wildcard $(TEST_DIR)/*.vy)
TEST_DSP = $(wildcard $(TEST_DIR)/dsp/*.vy)
TEST_GRAPHICS = $(wildcard $(TEST_DIR)/graphics/*.vy)
TEST_NETWORK = $(wildcard $(TEST_DIR)/network/*.vy)
TEST_TRAINING = $(wildcard $(TEST_DIR)/training/*.vy)
TEST_VSERV = $(wildcard $(TEST_DIR)/network/vserv/*.vy)

# Skip these tests (they require user interaction or specific environments)
SKIP_TESTS = $(TEST_DIR)/graphics/game_test.vy \
             $(TEST_DIR)/graphics/drone_fpv.vy \
             $(TEST_DIR)/graphics/sector_shift.vy \
             $(TEST_DIR)/graphics/vcas.vy \
             $(TEST_DIR)/graphics/vdec_minigame.vy \
             $(TEST_DIR)/graphics/vterm.vy \
             $(TEST_DIR)/network/vserv/server.vy \
             $(TEST_DIR)/dsp/room_spatializer.vy \
             $(TEST_DIR)/dsp/v_studio.vy

TEST_RUNNER = ./$(TARGET)

.PHONY: test test-compiler test-dsp test-graphics test-network test-training test-vserv test-all

# ---- Main Test Target ----

test: test-compiler test-dsp test-network test-training
	@echo ""
	@echo "$(BOLD)=== Test Summary ===$(RESET)"
	@if [ -f $(TEST_RESULTS) ]; then \
		cat $(TEST_RESULTS); \
	else \
		echo "$(GREEN)All tests passed!$(RESET)"; \
	fi
	@$(RM) $(TEST_RESULTS) 2>/dev/null || true

# ---- Compiler Tests ----

test-compiler: $(TARGET)
	@echo "$(CYAN)Running compiler tests...$(RESET)"
	@echo "=== Compiler Tests ===" > $(TEST_RESULTS)
	@$(call run_tests,$(TEST_COMPILER),Compiler)

# ---- DSP Tests ----

test-dsp: $(TARGET)
	@echo "$(CYAN)Running DSP tests...$(RESET)"
	@echo "=== DSP Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_DSP),DSP)

# ---- Graphics Tests ----

test-graphics: $(TARGET)
	@echo "$(CYAN)Running graphics tests...$(RESET)"
	@echo "=== Graphics Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_GRAPHICS),Graphics)

# ---- Network Tests ----

test-network: $(TARGET)
	@echo "$(CYAN)Running network tests...$(RESET)"
	@echo "=== Network Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_NETWORK),Network)

# ---- Training Tests ----

test-training: $(TARGET)
	@echo "$(CYAN)Running training tests...$(RESET)"
	@echo "=== Training Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_TRAINING),Training)

# ---- VServ Tests ----

test-vserv: $(TARGET)
	@echo "$(CYAN)Running VServ tests...$(RESET)"
	@echo "=== VServ Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_VSERV),VServ)

# ---- All Tests ----

test-all: test-compiler test-dsp test-graphics test-network test-training test-vserv
	@echo ""
	@echo "$(BOLD)=== Full Test Summary ===$(RESET)"
	@if [ -f $(TEST_RESULTS) ]; then \
		cat $(TEST_RESULTS); \
	else \
		echo "$(GREEN)All tests passed!$(RESET)"; \
	fi
	@$(RM) $(TEST_RESULTS) 2>/dev/null || true

# ---- Helper Functions ----

# Test runner function
define run_tests
	total=0; passed=0; failed=0; skipped=0; \
	for test in $(1); do \
		total=$$((total + 1)); \
		skip=0; \
		for skip_test in $(SKIP_TESTS); do \
			if [ "$$test" = "$$skip_test" ]; then \
				skip=1; \
				break; \
			fi; \
		done; \
		if [ $$skip -eq 1 ]; then \
			skipped=$$((skipped + 1)); \
			echo "$(YELLOW)  SKIP: $$(basename $$test)$(RESET)"; \
			continue; \
		fi; \
		echo "  Running: $$(basename $$test)"; \
		$(TEST_RUNNER) "$$test" > /dev/null 2>&1; \
		if [ $$? -eq 0 ]; then \
			passed=$$((passed + 1)); \
			echo "$(GREEN)  PASS: $$(basename $$test)$(RESET)"; \
		else \
			failed=$$((failed + 1)); \
			echo "$(RED)  FAIL: $$(basename $$test)$(RESET)"; \
			echo "  FAIL: $$(basename $$test)" >> $(TEST_RESULTS); \
		fi; \
	done; \
	echo ""; \
	echo "$(2) Tests: $$total total, $$passed passed, $$failed failed, $$skipped skipped" | tee -a $(TEST_RESULTS); \
	if [ $$failed -gt 0 ]; then \
		echo "$(RED)  Some $(2) tests failed!$(RESET)"; \
	fi
endef

# ---- Benchmark Tests ----

benchmark: $(TARGET)
	@echo "$(CYAN)Running benchmarks...$(RESET)"
	@echo "=== Benchmarks ==="
	@./$(TARGET) --benchmark $(TEST_DIR)/fib_test.vy
	@./$(TARGET) --benchmark $(TEST_DIR)/factorial_test.vy
	@./$(TARGET) --benchmark $(TEST_DIR)/bubble_sort_test.vy

# ---- Quick Test (only fast tests) ----

quick-test: $(TARGET)
	@echo "$(CYAN)Running quick tests...$(RESET)"
	@./$(TARGET) $(TEST_DIR)/array_test.vy > /dev/null 2>&1 && echo "$(GREEN)✓ array_test$(RESET)" || echo "$(RED)✗ array_test$(RESET)"
	@./$(TARGET) $(TEST_DIR)/for_test.vy > /dev/null 2>&1 && echo "$(GREEN)✓ for_test$(RESET)" || echo "$(RED)✗ for_test$(RESET)"
	@./$(TARGET) $(TEST_DIR)/function_test.vy > /dev/null 2>&1 && echo "$(GREEN)✓ function_test$(RESET)" || echo "$(RED)✗ function_test$(RESET)"
	@./$(TARGET) $(TEST_DIR)/enum_test.vy > /dev/null 2>&1 && echo "$(GREEN)✓ enum_test$(RESET)" || echo "$(RED)✗ enum_test$(RESET)"
	@./$(TARGET) $(TEST_DIR)/interface_test.vy > /dev/null 2>&1 && echo "$(GREEN)✓ interface_test$(RESET)" || echo "$(RED)✗ interface_test$(RESET)"
	@./$(TARGET) $(TEST_DIR)/map_test.vy > /dev/null 2>&1 && echo "$(GREEN)✓ map_test$(RESET)" || echo "$(RED)✗ map_test$(RESET)"
	@./$(TARGET) $(TEST_DIR)/string_test.vy > /dev/null 2>&1 && echo "$(GREEN)✓ string_test$(RESET)" || echo "$(RED)✗ string_test$(RESET)"
	@./$(TARGET) $(TEST_DIR)/pipeline_test.vy > /dev/null 2>&1 && echo "$(GREEN)✓ pipeline_test$(RESET)" || echo "$(RED)✗ pipeline_test$(RESET)"

# ---- Clean ----

clean:
	@echo "Cleaning..."
	@$(RM)
	@$(DEL)
	@$(DEL_URAGE)
	@$(DEL_TEST)
	@rm -f $(TEST_RESULTS) 2>/dev/null || true

# ---- Lint & Sanitize ----

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
	@echo "Build complete. Running executable..."
	./vyne_leak_test tests/network/socket.vy
	@echo "Leak check execution finished."

# ---- Include Dependencies ----

-include $(DEPS)

# ---- Help ----

help:
	@echo "$(BOLD)Vyne Build System$(RESET)"
	@echo ""
	@echo "$(CYAN)Targets:$(RESET)"
	@echo "  make              - Build vynec"
	@echo "  make test         - Run all tests (compiler, DSP, network, training)"
	@echo "  make test-compiler - Run compiler tests only"
	@echo "  make test-dsp     - Run DSP tests only"
	@echo "  make test-graphics - Run graphics tests only"
	@echo "  make test-network - Run network tests only"
	@echo "  make test-training - Run training tests only"
	@echo "  make test-vserv   - Run VServ tests only"
	@echo "  make test-all     - Run ALL tests"
	@echo "  make quick-test   - Run only fast tests"
	@echo "  make benchmark    - Run performance benchmarks"
	@echo "  make clean        - Remove build files"
	@echo "  make check-leaks  - Run with LeakSanitizer"
	@echo ""
	@echo "$(CYAN)Examples:$(RESET)"
	@echo "  make test                         # Run all tests"
	@echo "  make test-compiler                # Run compiler tests"
	@echo "  make test-dsp                     # Run audio DSP tests"
	@echo "  make quick-test                   # Run fast tests"
	@echo "  make ./vynec tests/array_test.vy  # Run single test"

.PHONY: all clean check-copies check-leaks help
.PHONY: test test-compiler test-dsp test-graphics test-network test-training test-vserv test-all
.PHONY: quick-test benchmark