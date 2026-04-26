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
            C_Emitter emitter;
            rootShared->compile(emitter);
            
            std::string c_code = emitter.getBodyCode();
            
            std::string base_name = filename.substr(0, filename.find_last_of("."));
            std::string c_file_name = base_name + ".vy.c";
            std::string exe_name = base_name; 

        #ifdef _WIN32
            exe_name += ".exe";
        #endif

            std::ofstream out_file(c_file_name);
            out_file << "#include \"./vyne/runtime/vyne_runtime.h\"\n\n";
            out_file << "int main() {\n";
            out_file << c_code;
            out_file << "    return 0;\n";
            out_file << "}\n";
            out_file.close();

            std::cout << GREEN << "C code generated: " << c_file_name << RESET << "\n";

            std::string exeDir = FileUtils::getExeDir();
            std::string runtimePath = exeDir + "/runtime";
            std::string compile_cmd = "gcc \"" + c_file_name + "\" -o \"" + exe_name + 
                          "\" -I\"" + exeDir + "\" -O3 -w";
            
            std::cout << YELLOW << "Compiling with GCC..." << RESET << "\n";
            
            int result = system(compile_cmd.c_str());

            if (result == 0) {
                std::cout << BOLD << GREEN << "Successfully built: " << exe_name << RESET << "\n";
                // system(("./" + exe_name).c_str()); 
            } else {
                std::cerr << RED << "Compilation failed! Make sure GCC is installed and vyne_runtime.h is in the path." << RESET << "\n";
            }

            return result;
        }
    } catch (const std::exception& e) {
        std::cerr << RED << "Error: " << e.what() << RESET << "\n";
        return 1;
    }

    return 0;
}