#include "cli/repl.h"
#include "cli/file_handler.h"
#include "vyne/utils/file_utils.h"

int main(int argc, char* argv[]) {
    FileUtils::setExeDir(argv[0]);
    SymbolContainer env;
    env["global"] = {};

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