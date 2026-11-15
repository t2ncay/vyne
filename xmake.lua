-- ============================================================================
-- Vyne — xmake build script (cross-platform)
-- ============================================================================
set_project("vyne")
set_version("0.8.0")
set_encodings("utf-8")

-- ---------------------------------------------------------------------------
-- Platform detection
-- ---------------------------------------------------------------------------
local is_windows = is_plat("windows")
local is_linux = is_plat("linux")
local is_macos = is_plat("macosx")
local is_unix = is_linux or is_macos

local EXE_SUFFIX = is_windows and ".exe" or ""
local ROOT = os.scriptdir()

-- Windows: force MinGW (MSVC lacks the C++23 bits we rely on). Linux/macOS:
-- let xmake pick the system compiler.
if is_windows then
	set_toolchains("mingw")
end

add_rules("mode.debug", "mode.release")

-- ---------------------------------------------------------------------------
-- Third-party dependencies
-- ---------------------------------------------------------------------------
-- No external native dependencies are linked. raylib was only used by the
-- LSP backend; OpenSSL was only used by vserv. Both are deprecated for now.
--
-- The vserv module is stubbed out on the source side — see the note in
-- ast.cpp near the module includes. Do not add files with
-- `#include <openssl/...>` back into the build until OpenSSL is restored.
--
-- When a native dependency is needed again, use xmake's package system:
--
--     add_requires("package")      -- top of file, before any target
--     add_packages("package")      -- inside target("vynec")
--
-- It injects the right -I and -L for the active toolchain on every
-- platform and avoids hardcoded paths.

-- ---------------------------------------------------------------------------
-- Defines
-- ---------------------------------------------------------------------------
local COMMON_DEFINES = {}
if is_windows then
	table.join2(COMMON_DEFINES, {
		"_WIN32",
		"WIN32_LEAN_AND_MEAN",
		"NOGDI",
		"NOUSER",
		-- FTXUI (the `vynec init` TUI) requires these on Windows.
		"UNICODE",
		"_UNICODE",
		"NOMINMAX",
	})
end

-- ---------------------------------------------------------------------------
-- Link libraries
-- ---------------------------------------------------------------------------
-- Windows: Win32 system libraries used by the runtime modules
-- (graphics, audio, timing, sockets).
-- Unix:    pthread for threading, dl for dlopen, m for libm.
local COMMON_LINKS
if is_windows then
	COMMON_LINKS = {
		"opengl32",
		"gdi32",
		"winmm",
		"shell32",
		"winpthread",
		"crypt32",
		"bcrypt",
		"ws2_32",
	}
else
	COMMON_LINKS = {
		"pthread",
		"dl",
		"m",
	}
end

-- ---------------------------------------------------------------------------
-- Sources
-- ---------------------------------------------------------------------------
-- The LSP backend under editors/vscode/lsp/backend/src/ is intentionally
-- not compiled. It pulls in raylib and is not needed for the `vynec foo.vy`
-- path. Re-add it (plus the raylib dependency) if you want `--lsp` back.
local VYNEC_SOURCES = {
	"*.cpp",
	"cli/*.cpp",
	"vyne/**.cpp",
}

-- ---------------------------------------------------------------------------
-- vynec — the compiler binary
-- ---------------------------------------------------------------------------
target("vynec")
set_kind("binary")
set_filename("vynec" .. EXE_SUFFIX)
set_targetdir(ROOT)
-- Distributions include compiler-managed presets and user-facing templates.
add_installfiles("share/vyne/(**)", { prefixdir = "share/vyne" })
-- Runtime lookup is relative to the compiler executable.
add_installfiles("vyne/runtime/(**)", { prefixdir = "bin/vyne/runtime" })
add_installfiles("vyne/modules/(**)", { prefixdir = "share/vyne/modules" })
add_installfiles("vendor/arduino/(**)", { prefixdir = "bin/vendor/arduino" })
add_installfiles("vendor/stb/(**)", { prefixdir = "bin/vendor/stb" })
add_installfiles("vendor/raylib/(**)", { prefixdir = "bin/vendor/raylib" })

add_files(VYNEC_SOURCES)

add_includedirs(".")
add_includedirs("vendor")

add_defines(COMMON_DEFINES)

add_cxxflags("-std=c++23", { force = true })

-- Windows-only link libraries. ws2_32 is the WinSock2 stack; on
-- Linux the socket API is part of libc and needs no extra library.
if is_plat("windows") then
	add_syslinks("ws2_32")
	add_ldflags("-mconsole", { force = true })
end

-- POSIX threads. On Linux this is -lpthread; on Windows the
-- compiler either uses its own threading runtime (MSVC) or ships
-- a winpthreads shim (MinGW). Guard so the Linux build links
-- cleanly and a MinGW build can override if needed.
if is_plat("linux") or is_plat("macosx") or is_plat("bsd") then
	add_ldflags("-pthread")
end

add_links(COMMON_LINKS)
if is_mode("release") then
	add_cxxflags("-O3", { force = true })
elseif is_mode("debug") then
	add_cxxflags("-O0", "-g", { force = true })
end
target_end()

-- ===========================================================================
-- Test infrastructure
-- ===========================================================================

-- Test tasks launch a host executable even before a target configuration loads.
local VYNEC_EXE = path.join(ROOT, "vynec" .. (os.host() == "windows" and ".exe" or ""))

local function _collect(patterns)
	local out = {}
	for _, p in ipairs(patterns) do
		for _, f in ipairs(os.files(p) or {}) do
			table.insert(out, f)
		end
	end
	table.sort(out)
	return out
end

-- The manifest inventories actual fixtures and checks compile/runtime/output
-- contracts. Interactive examples are not indiscriminately launched as tests.
local function _python(task_os)
	local configured = task_os.getenv("PYTHON")
	if configured and #configured > 0 then return configured end
	return task_os.host() == "windows" and "python" or "python3"
end

local function _run_group(name, task_os, task_try)
	-- File-scope DSL functions do not inherit on_run's sandbox helpers.
	return task_try({
		function()
			task_os.execv(_python(task_os), {
				path.join(ROOT, "scripts/run_tests.py"),
				"--suite", name:lower(), "--compiler", VYNEC_EXE,
			}, { curdir = ROOT })
			return 0
		end,
		{ catch = function(err) print("%s suite failed: %s", name, err) end },
	}) or 1
end

-- ---------------------------------------------------------------------------
-- Individual test tasks
-- ---------------------------------------------------------------------------

task("test-compiler")
set_category("test")
set_menu({ description = "Run compiler tests" })
on_run(function()
	if _run_group("Compiler", os, try) > 0 then
		raise("Compiler tests failed")
	end
end)
task_end()

task("test-safety")
set_category("test")
set_menu({ description = "Run phase-checked safety regressions" })
on_run(function()
	if _run_group("Safety", os, try) > 0 then raise("Safety tests failed") end
end)
task_end()

task("test-ast")
set_category("test")
set_menu({ description = "Run registered interpreter regressions" })
on_run(function()
	if _run_group("AST", os, try) > 0 then raise("Interpreter tests failed") end
end)
task_end()

task("test-optimization")
set_category("test")
set_menu({ description = "Run optimization and runner acceptance" })
on_run(function()
	if _run_group("Optimization", os, try) > 0 then raise("Optimization tests failed") end
end)
task_end()

task("test-dsp")
set_category("test")
set_menu({ description = "Run DSP tests" })
on_run(function()
	if _run_group("DSP", os, try) > 0 then
		raise("DSP tests failed")
	end
end)
task_end()

task("test-graphics")
set_category("test")
set_menu({ description = "Run graphics tests" })
on_run(function()
	if _run_group("Graphics", os, try) > 0 then
		raise("Graphics tests failed")
	end
end)
task_end()

task("test-network")
set_category("test")
set_menu({ description = "Run network tests" })
on_run(function()
	if _run_group("Network", os, try) > 0 then
		raise("Network tests failed")
	end
end)
task_end()

task("test-training")
set_category("test")
set_menu({ description = "Run training tests" })
on_run(function()
	if _run_group("Training", os, try) > 0 then
		raise("Training tests failed")
	end
end)
task_end()

task("test-vserv")
set_category("test")
set_menu({ description = "Run VServ tests" })
on_run(function()
	if _run_group("VServ", os, try) > 0 then
		raise("VServ tests failed")
	end
end)
task_end()

task("test-codegen")
set_category("test")
set_menu({ description = "Run codegen golden tests" })
on_run(function()
	if _run_group("Codegen", os, try) > 0 then raise("Codegen golden tests failed") end
end)
task_end()

task("test-init")
set_category("test")
set_menu({ description = "Run initialization and hardware configuration acceptance" })
on_run(function()
	os.execv(_python(os), { path.join(ROOT, "scripts/test_hardware.py") }, {
		curdir = ROOT, envs = { VYNEC = VYNEC_EXE },
	})
end)
task_end()

task("test-hardware")
set_category("test")
set_menu({ description = "Run fresh-project hardware configuration acceptance tests" })
on_run(function()
    os.execv(_python(os), { path.join(ROOT, "scripts/test_hardware.py") }, { curdir = ROOT, envs = { VYNEC = VYNEC_EXE } })
end)
task_end()

task("test-vplot")
set_category("test")
set_menu({ description = "Run universal plotting acceptance in examples/projects/plotter-esp32" })
on_run(function()
    os.execv(_python(os), { path.join(ROOT, "examples/projects/plotter-esp32/tests/run.py") }, { curdir = ROOT, envs = { VYNEC = VYNEC_EXE } })
end)
task_end()

-- ---------------------------------------------------------------------------
-- Aggregate tasks
-- ---------------------------------------------------------------------------

task("test")
set_category("test")
set_menu({ description = "Run registered core, safety, golden, interpreter and optimization tests" })
on_run(function()
	local failed = 0
	for _, suite in ipairs({ "Compiler", "Safety", "Codegen", "AST", "Optimization" }) do
		failed = failed + _run_group(suite, os, try)
	end
	if failed > 0 then
		raise("Some test groups failed")
	end
end)
task_end()

task("test-all")
set_category("test")
set_menu({ description = "Run every registered automated suite, including numerical and headless graphics acceptance" })
on_run(function()
	local failed = 0
	for _, suite in ipairs({ "Compiler", "Safety", "Codegen", "AST", "DSP", "Training", "Graphics", "Optimization" }) do
		failed = failed + _run_group(suite, os, try)
	end
	if failed > 0 then
		raise("Some test groups failed")
	end
end)
task_end()

task("quick-test")
set_category("test")
set_menu({ description = "Run fast smoke tests" })
on_run(function()
	if _run_group("Compiler", os, try) > 0 then raise("Quick smoke tests failed") end
end)
task_end()

task("benchmark")
set_category("test")
set_menu({ description = "Run performance benchmarks" })
on_run(function()
	os.execv(VYNEC_EXE, {
		"bench", path.join(ROOT, "examples/optimization/profile_safe.vy"),
		"--report", path.join(ROOT, "build/benchmark.json"),
	}, { curdir = ROOT })
end)
task_end()

task("check-copies")
set_category("action")
set_menu({ description = "Scan for copy/move warnings" })
on_run(function()
	local warn_file = path.join(ROOT, "warning.txt")
	io.writefile(warn_file, "")

	local sources = _collect(VYNEC_SOURCES)
	local base_args = {
		"-std=c++23",
		"-I.",
		"-Wpessimizing-move",
		"-Wredundant-move",
		"-fsyntax-only",
	}
	if is_windows then
		table.insert(base_args, "-D_WIN32")
	end

	for _, src in ipairs(sources) do
		print("Checking " .. src .. "...")
		local args = table.clone(base_args)
		table.insert(args, src)
		try({
			function()
				os.execv("g++", args, {
					stdout = warn_file, stderr = warn_file,
					appendout = true, appenderr = true,
				})
			end,
			{ catch = function() end },
		})
	end
	print("Scan complete. Results written to warning.txt")
end)
task_end()

task("check-leaks")
set_category("action")
set_menu({ description = "Build the compiler with ASan/UBSan and run registered safety acceptance" })
on_run(function()
	if os.host() == "windows" then raise("check-leaks requires a supported POSIX sanitizer toolchain") end
	local sanitizer = "-fsanitize=address,undefined"

	local sources = _collect(VYNEC_SOURCES)
	local out_exe = path.join(ROOT, "vyne_leak_release" .. EXE_SUFFIX)

	local args = { "-std=c++23", "-O1", "-g", sanitizer }
	table.insert(args, "-I.")
	table.insert(args, "-Ivendor")
	for _, d in ipairs(COMMON_DEFINES) do
		table.insert(args, "-D" .. d)
	end

	for _, s in ipairs(sources) do
		table.insert(args, s)
	end

	table.insert(args, "-o")
	table.insert(args, out_exe)

	for _, l in ipairs(COMMON_LINKS) do
		table.insert(args, "-l" .. l)
	end
	if is_windows then
		table.insert(args, "-mconsole")
	end
	table.insert(args, "-pthread")

	print("Building instrumented compiler...")
	os.execv("g++", args)

	os.execv(_python(os), {
		path.join(ROOT, "scripts/run_tests.py"), "--suite", "safety", "--compiler", out_exe,
	}, { curdir = ROOT })
end)
task_end()
