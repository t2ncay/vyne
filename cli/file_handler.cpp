#include "file_handler.h"
#include "../vyne/utils/sha256.h"
#include <map>

bool verifyIntegrity(const std::string& scriptPath) {
    namespace fs = std::filesystem;
    fs::path filePath(scriptPath);
    fs::path baseDir = filePath.parent_path();
    fs::path manifestPath = baseDir.empty() ? "checksums.dat" : baseDir / "checksums.dat";

    if (!fs::exists(manifestPath)) return true;

    std::ifstream manifest(manifestPath);
    if (!manifest.is_open()) return true;

    std::string relPath, expectedHash;
    std::map<std::string, std::string> expectedHashes;
    while (manifest >> relPath >> expectedHash) {
        expectedHashes[relPath] = expectedHash;
    }

    std::string fileKey = filePath.filename().string();

    auto it = expectedHashes.find(fileKey);
    if (it == expectedHashes.end()) {
        return true; 
    }

    std::string currentHash = SHA256::hashFile(scriptPath);
    if (currentHash != it->second) {
        std::cerr << RED << "[SECURITY ERROR] Integrity check failed for " 
                  << scriptPath << "! File modified or corrupted." << RESET << "\n";
        return false;
    }

    return true;
}

int runFile(const std::string& filename, SymbolContainer& env, const std::string& mode, bool enforceIntegrity) {
    if (enforceIntegrity && !verifyIntegrity(filename)) {
        return 1;
    }

    size_t dotPos = filename.find_last_of(".");
    if (dotPos == std::string::npos || filename.substr(dotPos + 1) != "vy") {
        std::cerr << RED << "Error: File must end in .vy ( .vyne )" << RESET << "\n";
        return 1;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << RED << "Could not open file: " << filename << RESET << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();

    try {
        auto tokens = tokenize(content);
        Parser parser(std::move(tokens));
        auto programRoot = parser.parseProgram(env);
        std::shared_ptr<ASTNode> rootShared = std::move(programRoot);

        if (mode == "ast") {
            auto start = std::chrono::high_resolution_clock::now();

            env.setSourceDir(filename);
            uint32_t globalId = StringPool::instance().intern("global");
            rootShared->evaluate(env, globalId);
            
            auto end = std::chrono::high_resolution_clock::now();
            parser.checkUnusedVariables(env);
            std::chrono::duration<double, std::milli> ms = end - start;
            std::cout << GREEN << "\nExecution finished in: " << ms.count() << "ms" << RESET;
            return 0;

        } else if (mode == "c") {

            auto tokens_for_count = tokenize(content);
            int tokenCount = (int)tokens_for_count.size();
            int lineCount  = (int)std::count(content.begin(), content.end(), '\n') + 1;

            std::cout << "\n";
            std::cout << BOLD << "  Compiling " << RESET << filename << "\n\n";

            auto start_transpile = std::chrono::high_resolution_clock::now();

            C_Emitter emitter;
            emitter.reset();
            emitter.setSourceDir(std::filesystem::absolute(filename).parent_path().string());
            rootShared->compile(emitter);

            std::string exeDir  = FileUtils::getExeDir();
            std::string runtime = exeDir + "/vyne/runtime/vyne_runtime.h";
            std::string cSource = emitter.finalize(runtime);

            std::string base    = filename.substr(0, filename.find_last_of("."));
            std::string cFile   = base + ".vy.c";
            std::string exeName = base;
            #ifdef _WIN32
                exeName += ".exe";
            #endif

            std::ofstream out(cFile);
            if (!out.is_open()) {
                std::cerr << RED << "  error" << RESET << "  could not write " << cFile << "\n";
                return 1;
            }
            out << cSource;
            out.close();

            auto end_transpile = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> transpile_ms = end_transpile - start_transpile;

            std::cout << GREEN << "  transpile" << RESET
                    << "  " << lineCount << " lines  "
                    << tokenCount << " tokens  "
                    << std::fixed << std::setprecision(2) << transpile_ms.count() << "ms\n";

            std::string compile_cmd = "gcc \"" + cFile + "\" -o \"" + exeName + "\""
                                    " -I\"" + exeDir + "\" -O3 -w";

            auto start_compile = std::chrono::high_resolution_clock::now();
            int compile_result = system(compile_cmd.c_str());
            auto end_compile   = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> compile_ms = end_compile - start_compile;

            if (compile_result != 0) {
                std::cerr << "\n" << RED << "  error" << RESET << "  gcc failed — see above\n\n";
                return 1;
            }

            std::string sizeStr = "?";
            if (std::filesystem::exists(exeName)) {
                uintmax_t bytes = std::filesystem::file_size(exeName);
                if      (bytes < 1024)             sizeStr = std::to_string(bytes) + " B";
                else if (bytes < 1024 * 1024)      sizeStr = std::to_string(bytes / 1024) + " KB";
                else                               sizeStr = std::to_string(bytes / (1024*1024)) + " MB";
            }

            std::cout << GREEN << "  compile  " << RESET
                    << "  gcc -O3  "
                    << std::fixed << std::setprecision(2) << compile_ms.count() << "ms\n";

            std::string run_cmd = (exeName.find('/') == std::string::npos &&
                                exeName.find('\\') == std::string::npos)
                                ? "./" + exeName : exeName;

            std::cout << "\n";
            std::cout << CYAN << "  >> output" << RESET << "\n";
            std::cout << CYAN << "  " << std::string(42, '-') << RESET << "\n\n";

            auto start_exec = std::chrono::high_resolution_clock::now();
            int  run_result = system(run_cmd.c_str());
            auto end_exec   = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> exec_ms = end_exec - start_exec;

            std::cout << "\n";
            std::cout << CYAN << "  " << std::string(42, '-') << RESET << "\n\n";

            double total_ms = (transpile_ms + compile_ms + exec_ms).count();

            vprintln("\n{}{}{}  >> summary {}", BOLD, YELLOW, "", RESET);
            vprintln("{} {} binary     {} ({}) {}", YELLOW, "  -", GREEN, exeName, CYAN, sizeStr, RESET);
            vprintln("     {}transpile {:.2f}ms", GREEN, transpile_ms.count());
            vprintln("     {}compile   {:.2f}ms", GREEN, compile_ms.count());
            vprintln("     {}execution {:.2f}ms", GREEN, exec_ms.count());
            vprintln("{} {} {}total      {:.2f}ms", YELLOW, "  -", BOLD, total_ms);

            if (run_result != 0)
                std::cout << RED << "  >> exited with code " << run_result << RESET << "\n\n";

            return 0;
        }

    } catch (const std::exception& e) {
        std::cerr << RED << "Error: " << e.what() << RESET << "\n";
        return 1;
    }

    return 0;
}