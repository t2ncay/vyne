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

OPENSSL_INC = C:/msys64/ucrt64/include
OPENSSL_LIB = C:/msys64/ucrt64/lib

COMMON_FLAGS = -std=c++23 -I. $(RAYLIB_INCLUDE) \
               -I./lsp/backend/src -I./lsp/backend/include \
               -I"$(OPENSSL_INC)" \
               -DCPPHTTPLIB_OPENSSL_SUPPORT -D_WIN32 -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER \
               -MMD -MP

CXXFLAGS = -O3 $(COMMON_FLAGS)
TEST_CXXFLAGS = -O0 -g $(COMMON_FLAGS)

TARGET = $(TARGET_BASE).exe
TEST_TARGET = vyne_test.exe
URAGE_LIB = urage.dll
URAGE_CFLAGS = -shared -DURAGE_BUILD_SHARED
RAYLIB_LIB_PATH = -L./vendor/raylib/lib

LDFLAGS = -mconsole -pthread $(RAYLIB_LIB_PATH)
LDFLAGS += -L"$(OPENSSL_LIB)" -lssl -lcrypto
LDFLAGS += -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lwinpthread -lcrypt32 -lbcrypt -lws2_32

# ---- PowerShell MkDir ----
MKDIR_P = powershell -Command "New-Item -ItemType Directory -Force -Path '$(subst /,\,$(1))'" > nul

SRCS = $(wildcard *.cpp) \
       $(wildcard cli/*.cpp) \
       $(wildcard vyne/*.cpp) \
       $(wildcard vyne/*/*.cpp) \
       $(wildcard vyne/*/*/*.cpp) \
       $(wildcard vyne/*/*/*/*.cpp) \
       $(wildcard editors/vscode/lsp/backend/src/*.cpp)

OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))
DEPS = $(OBJS:.o=.d)

all: $(URAGE_LIB) $(TARGET)

$(URAGE_LIB): $(URAGE_SRCS)
	$(CC) $(URAGE_CFLAGS) $(URAGE_INCLUDES) $(URAGE_SRCS) -o $(URAGE_LIB)

$(TARGET): $(OBJS) $(URAGE_LIB)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@$(call MKDIR_P,$(@D))
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_TARGET): $(OBJS) $(URAGE_LIB)
	$(CXX) $(OBJS) -o $(TEST_TARGET) $(LDFLAGS)

# ---- Test Runner ----

TEST_DIR = tests
TEST_RESULTS = test_results.txt

RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
CYAN = \033[0;36m
RESET = \033[0m
BOLD = \033[1m

TEST_COMPILER = $(wildcard $(TEST_DIR)/compiler/*.vy) $(wildcard $(TEST_DIR)/*.vy)
TEST_DSP = $(wildcard $(TEST_DIR)/dsp/*.vy)
TEST_GRAPHICS = $(wildcard $(TEST_DIR)/graphics/*.vy)
TEST_NETWORK = $(wildcard $(TEST_DIR)/network/*.vy)
TEST_TRAINING = $(wildcard $(TEST_DIR)/training/*.vy)
TEST_VSERV = $(wildcard $(TEST_DIR)/network/vserv/*.vy)

SKIP_TESTS = $(TEST_DIR)/graphics/game_test.vy \
             $(TEST_DIR)/graphics/drone_fpv.vy \
             $(TEST_DIR)/graphics/sector_shift.vy \
             $(TEST_DIR)/graphics/vcas.vy \
             $(TEST_DIR)/graphics/vdec_minigame.vy \
             $(TEST_DIR)/graphics/vterm.vy \
             $(TEST_DIR)/network/vserv/server.vy \
             $(TEST_DIR)/dsp/room_spatializer.vy \
             $(TEST_DIR)/dsp/v_studio.vy

TEST_RUNNER = .\$(TARGET)

.PHONY: test test-compiler test-dsp test-graphics test-network test-training test-vserv test-all quick-test benchmark clean check-copies check-leaks help

test: test-compiler test-dsp test-network test-training
	@echo ""
	@echo "$(BOLD)=== Test Summary ===$(RESET)"
	@if exist $(TEST_RESULTS) ( type $(TEST_RESULTS) ) else ( echo $(GREEN)All tests passed!$(RESET) )
	@-powershell -Command "if (Test-Path $(TEST_RESULTS)) { Remove-Item -Force $(TEST_RESULTS) }" 2>nul

test-compiler: $(TARGET)
	@echo "$(CYAN)Running compiler tests...$(RESET)"
	@echo "=== Compiler Tests ===" > $(TEST_RESULTS)
	@$(call run_tests,$(TEST_COMPILER),Compiler)

test-dsp: $(TARGET)
	@echo "$(CYAN)Running DSP tests...$(RESET)"
	@echo "=== DSP Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_DSP),DSP)

test-graphics: $(TARGET)
	@echo "$(CYAN)Running graphics tests...$(RESET)"
	@echo "=== Graphics Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_GRAPHICS),Graphics)

test-network: $(TARGET)
	@echo "$(CYAN)Running network tests...$(RESET)"
	@echo "=== Network Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_NETWORK),Network)

test-training: $(TARGET)
	@echo "$(CYAN)Running training tests...$(RESET)"
	@echo "=== Training Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_TRAINING),Training)

test-vserv: $(TARGET)
	@echo "$(CYAN)Running VServ tests...$(RESET)"
	@echo "=== VServ Tests ===" >> $(TEST_RESULTS)
	@$(call run_tests,$(TEST_VSERV),VServ)

test-all: test-compiler test-dsp test-graphics test-network test-training test-vserv
	@echo ""
	@echo "$(BOLD)=== Full Test Summary ===$(RESET)"
	@if exist $(TEST_RESULTS) ( type $(TEST_RESULTS) ) else ( echo $(GREEN)All tests passed!$(RESET) )
	@-powershell -Command "if (Test-Path $(TEST_RESULTS)) { Remove-Item -Force $(TEST_RESULTS) }" 2>nul

define run_tests
	total=0; passed=0; failed=0; skipped=0; \
	for test in $(1); do \
		total=$$((total + 1)); \
		skip=0; \
		for skip_test in $(SKIP_TESTS); do \
			if [ "$$test" = "$$skip_test" ]; then skip=1; break; fi; \
		done; \
		if [ $$skip -eq 1 ]; then \
			skipped=$$((skipped + 1)); \
			echo "$(YELLOW)  SKIP: $$(basename $$test)$(RESET)"; \
			continue; \
		fi; \
		echo "  Running: $$(basename $$test)"; \
		$(TEST_RUNNER) "$$test" > nul 2>&1; \
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
	if [ $$failed -gt 0 ]; then echo "$(RED)  Some $(2) tests failed!$(RESET)"; fi
endef

benchmark: $(TARGET)
	@echo "$(CYAN)Running benchmarks...$(RESET)"
	@echo "=== Benchmarks ==="
	@$(TEST_RUNNER) --benchmark $(TEST_DIR)/fib_test.vy
	@$(TEST_RUNNER) --benchmark $(TEST_DIR)/factorial_test.vy
	@$(TEST_RUNNER) --benchmark $(TEST_DIR)/bubble_sort_test.vy

quick-test: $(TARGET)
	@echo "$(CYAN)Running quick tests...$(RESET)"
	@$(TEST_RUNNER) $(TEST_DIR)/array_test.vy > nul 2>&1 && echo "$(GREEN)✓ array_test$(RESET)" || echo "$(RED)✗ array_test$(RESET)"
	@$(TEST_RUNNER) $(TEST_DIR)/for_test.vy > nul 2>&1 && echo "$(GREEN)✓ for_test$(RESET)" || echo "$(RED)✗ for_test$(RESET)"
	@$(TEST_RUNNER) $(TEST_DIR)/function_test.vy > nul 2>&1 && echo "$(GREEN)✓ function_test$(RESET)" || echo "$(RED)✗ function_test$(RESET)"
	@$(TEST_RUNNER) $(TEST_DIR)/enum_test.vy > nul 2>&1 && echo "$(GREEN)✓ enum_test$(RESET)" || echo "$(RED)✗ enum_test$(RESET)"
	@$(TEST_RUNNER) $(TEST_DIR)/interface_test.vy > nul 2>&1 && echo "$(GREEN)✓ interface_test$(RESET)" || echo "$(RED)✗ interface_test$(RESET)"
	@$(TEST_RUNNER) $(TEST_DIR)/map_test.vy > nul 2>&1 && echo "$(GREEN)✓ map_test$(RESET)" || echo "$(RED)✗ map_test$(RESET)"
	@$(TEST_RUNNER) $(TEST_DIR)/string_test.vy > nul 2>&1 && echo "$(GREEN)✓ string_test$(RESET)" || echo "$(RED)✗ string_test$(RESET)"
	@$(TEST_RUNNER) $(TEST_DIR)/pipeline_test.vy > nul 2>&1 && echo "$(GREEN)✓ pipeline_test$(RESET)" || echo "$(RED)✗ pipeline_test$(RESET)"

clean:
	@echo "Cleaning..."
	@-powershell -Command "if (Test-Path $(BUILD_DIR)) { Remove-Item -Recurse -Force $(BUILD_DIR) }" 2>nul
	@-powershell -Command "if (Test-Path $(TARGET)) { Remove-Item -Force $(TARGET) }" 2>nul
	@-powershell -Command "if (Test-Path $(URAGE_LIB)) { Remove-Item -Force $(URAGE_LIB) }" 2>nul
	@-powershell -Command "if (Test-Path $(TEST_TARGET)) { Remove-Item -Force $(TEST_TARGET) }" 2>nul
	@-powershell -Command "if (Test-Path $(TEST_RESULTS)) { Remove-Item -Force $(TEST_RESULTS) }" 2>nul

check-copies:
	@echo "Scanning for basic compiler copy/move warnings..."
	@-powershell -Command "if (Test-Path warning.txt) { Remove-Item -Force warning.txt }" 2>nul
	@$(foreach src,$(SRCS), \
		echo Checking $(src)...; \
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

-include $(DEPS)

help:
	@echo $(BOLD)Vyne Build System$(RESET)
	@echo ""
	@echo $(CYAN)Targets:$(RESET)
	@echo "  make              - Build vynec"
	@echo "  make test         - Run all tests (compiler, DSP, network, training)"
	@echo "  make test-compiler - Run compiler tests only"
	@echo "  make test-dsp      - Run DSP tests only"
	@echo "  make test-graphics - Run graphics tests only"
	@echo "  make test-network  - Run network tests only"
	@echo "  make test-training - Run training tests only"
	@echo "  make test-vserv    - Run VServ tests only"
	@echo "  make test-all      - Run ALL tests"
	@echo "  make quick-test    - Run only fast tests"
	@echo "  make benchmark     - Run performance benchmarks"
	@echo "  make clean        - Remove build files"
	@echo "  make check-leaks   - Run with LeakSanitizer"