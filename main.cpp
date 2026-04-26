#include "cli/repl.h"
#include "cli/file_handler.h"
#include "cli/packager.h"
#include "vyne/utils/file_utils.h"
#include "editors/vscode/lsp/backend/src/lsp_server.h"
#include <cstring>

int main(int argc, char* argv[]) {
    FileUtils::setExeDir(argv[0]);
    SymbolContainer env;
    
    uint32_t globalId = StringPool::instance().intern("global");
    env[globalId] = {}; 

    if (argc > 1 && strcmp(argv[1], "--lsp") == 0) {
        return runLspServer(env);
    }

    if (argc == 3 && strcmp(argv[1], "--build-game") == 0) {
        VynePackager packager(argv[2]);
        packager.build();
        return 0;
    }

    if (argc > 1) {
        std::string firstArg = argv[1];
        
        if (argc == 3) {
            std::string filename = argv[2];
            if (firstArg == "--ast") return runFile(filename, env, "ast");
            if (firstArg == "--bytecode") return runFile(filename, env, "bytecode");
            if (firstArg == "--c" || firstArg == "--compile") return runFile(filename, env, "c");
        }
        
        if (firstArg.substr(0, 2) != "--") {
            return runFile(firstArg, env, "ast");
        }

        std::cerr << "Error: Invalid arguments or file not found.\n";
        return 1;
    } 
    
    else {
        std::string input;
        init_REPL(input, env);
    }

    return 0;
}