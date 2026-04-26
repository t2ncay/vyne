#include "file_handler.h"

int runFile(const std::string& filename, SymbolContainer& env, const std::string& mode){
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
            auto start_transpile = std::chrono::high_resolution_clock::now();

            C_Emitter emitter;
            rootShared->compile(emitter);
            
            std::string vyne_funcs = emitter.getFunctionCode();
            std::string vyne_body = emitter.getBodyCode();
            
            std::string base_name = filename.substr(0, filename.find_last_of("."));
            std::string c_file_name = base_name + ".vy.c";
            std::string exe_name = base_name; 

            #ifdef _WIN32
                exe_name += ".exe";
            #endif

            std::ofstream out_file(c_file_name);
            out_file << "#include \"vyne/runtime/vyne_runtime.h\"\n\n";
            
            out_file << vyne_funcs << "\n";
            
            out_file << "int main(int argc, char* argv[]) {\n";
            out_file << vyne_body;
            out_file << "    return 0;\n";
            out_file << "}\n";
            out_file.close();

            auto end_transpile = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> transpile_ms = end_transpile - start_transpile;

            std::cout << GREEN << "C code generated: " << c_file_name << RESET << " (" << transpile_ms.count() << "ms)\n";

            // GCC
            std::string exeDir = FileUtils::getExeDir();
            std::string compile_cmd = "gcc \"" + c_file_name + "\" -o \"" + exe_name + "\" -I\"" + exeDir + "\" -O3 -w";
            
            std::cout << YELLOW << "Compiling with GCC..." << RESET << "\n";
            
            auto start_compile = std::chrono::high_resolution_clock::now();
            int compile_result = system(compile_cmd.c_str());
            auto end_compile = std::chrono::high_resolution_clock::now();
            
            std::chrono::duration<double, std::milli> compile_ms = end_compile - start_compile;

            if (compile_result == 0) {
                std::cout << BOLD << GREEN << "Successfully built: " << exe_name << RESET << " (" << compile_ms.count() << "ms)\n";
                
                std::cout << CYAN << "\n--- Vyne Execution Output ---\n" << RESET;
                
                auto start_exec = std::chrono::high_resolution_clock::now();
                
                std::string run_cmd = (exe_name.find('/') == std::string::npos && exe_name.find('\\') == std::string::npos) 
                                    ? "./" + exe_name : exe_name;
                int run_result = system(run_cmd.c_str());
                
                auto end_exec = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> exec_ms = end_exec - start_exec;

                std::cout << CYAN << "-----------------------------\n" << RESET;
                
                if (run_result != 0) {
                    std::cout << RED << "Runtime Error: Program exited with code " << run_result << RESET << "\n";
                }
                
                std::cout << MAGENTA << "Total Pipeline: " << (transpile_ms + compile_ms + exec_ms).count() << "ms" << RESET << "\n";
                return 0;
            } else {
                std::cerr << RED << "Compilation failed!" << RESET << "\n";
                return 1;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << RED << "Error: " << e.what() << RESET << "\n";
        return 1;
    }

    return 0;
}