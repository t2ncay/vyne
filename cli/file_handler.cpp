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
        } else {
            std::cout << GREEN << "Compiling to Bytecode..." << RESET << "\n";
    
            Chunk chunk = compile(rootShared);
            disassembleChunk(chunk, filename);

            VM vm(env); 

            std::cout << GREEN << "Running VM...\n" << RESET;
            
            auto start = std::chrono::high_resolution_clock::now();

            InterpretResult result = vm.interpret(chunk);

            auto end = std::chrono::high_resolution_clock::now();
            
            std::chrono::duration<double, std::milli> ms = end - start;

            if (result == INTERPRET_OK) {
                std::cout << GREEN << "\nVM Execution finished in: " << ms.count() << "ms" << RESET << "\n";
            }

            return (result == INTERPRET_OK) ? 0 : 70;
        }
    } catch (const std::exception& e) {
        std::cerr << RED << "Error: " << e.what() << RESET << "\n";
        return 1;
    }

    return 0;
}