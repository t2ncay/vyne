#include "cli/repl.h"
#include "cli/file_handler.h"
#include "vyne/utils/file_utils.h"
#include "editors/vscode/lsp/backend/src/lsp_server.h"
#include <cstring>

// Forward declaration of LSP server runner
int runLspServer();

int main(int argc, char* argv[]) {
    FileUtils::setExeDir(argv[0]);
    SymbolContainer env;
    env["global"] = {};

    // Check for LSP mode first
    if (argc > 1 && strcmp(argv[1], "--lsp") == 0) {
        // Run LSP server
        return runLspServer();
    }

    if (argc == 3) {
        std::string flag = argv[1];
        std::string filename = argv[2];

        if (flag == "--ast") {
            runFile(filename, env, "ast");
        } else if (flag == "--bytecode") {
            runFile(filename, env, "bytecode");
        } else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    } else {
        std::string input;
        init_REPL(input, env);
    }

    return 0;
}