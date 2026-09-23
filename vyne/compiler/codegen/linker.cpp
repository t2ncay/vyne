#include "linker.h"
#include "../ast/ast.h"
#include "../parser/parser.h"
#include "../lexer/lexer.h"
#include "../../utils/file_utils.h"

#include <filesystem>
#include <stdexcept>

namespace {

struct DiagnosticContextGuard {
    std::string savedFile;
    std::string savedSource;

    DiagnosticContextGuard() {
        savedFile   = Vyne::DiagnosticEngine::getCurrentFile();
        savedSource = Vyne::DiagnosticEngine::getSourceText();
    }

    ~DiagnosticContextGuard() {
        Vyne::DiagnosticEngine::setCurrentFile(savedFile);
        Vyne::DiagnosticEngine::setSourceText(savedSource);
    }
};

} // anonymous namespace

std::string VyneLinker::canonicalize(const std::string& baseDir,
                                     const std::string& rawPath,
                                     bool isExtern) {
    std::string cleanPath = rawPath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\')) {
        cleanPath.erase(0, 1);
    }

    std::filesystem::path p;
    if (isExtern) {
        p = std::filesystem::path(FileUtils::exeDir)
            / "vyne" / "modules" / "external" / cleanPath;
    } else {
        p = std::filesystem::path(baseDir) / cleanPath;
    }
    return std::filesystem::weakly_canonical(p).string();
}

std::shared_ptr<ProgramNode>
VyneLinker::parseFile(const std::string& canonicalPath,
                      const std::string& sourceDir) {
    auto it = parsedFiles.find(canonicalPath);
    if (it != parsedFiles.end()) return it->second;

    if (!std::filesystem::exists(canonicalPath) ||
        std::filesystem::is_directory(canonicalPath)) {
        throw std::runtime_error(
            "Vyne Linker Error: import not found at: " + canonicalPath);
    }

    const std::string& source = FileUtils::readFile(canonicalPath);

    DiagnosticContextGuard guard;
    Vyne::DiagnosticEngine::setCurrentFile(
        std::filesystem::path(canonicalPath).filename().string());
    Vyne::DiagnosticEngine::setSourceText(source);

    auto tokens = tokenize(source);

    SymbolContainer parseEnv;
    Parser parser(std::move(tokens));
    parser.setSourceDir(sourceDir);
    auto ast = parser.parseProgram(parseEnv);
    if (!ast) {
        throw std::runtime_error(
            "Vyne Linker Error: failed to parse import: " + canonicalPath);
    }

    std::shared_ptr<ProgramNode> shared(std::move(ast));
    parsedFiles.emplace(canonicalPath, shared);
    return shared;
}

void VyneLinker::visit(const std::string& canonicalPath,
                       const std::string& sourceDir,
                       const std::string& alias,
                       bool isExtern) {
    if (visiting.count(canonicalPath)) {
        std::string chain;
        for (const auto& p : visitChain) chain += p + " -> ";
        chain += canonicalPath;
        throw std::runtime_error(
            "Vyne Linker Error: import cycle detected: " + chain);
    }

    std::string key = canonicalPath + "|" + alias;
    if (scheduled.count(key)) return;
    scheduled.insert(key);

    visiting.insert(canonicalPath);
    visitChain.push_back(canonicalPath);

    auto ast = parseFile(canonicalPath, sourceDir);

    std::string thisDir =
        std::filesystem::path(canonicalPath).parent_path().string();

    for (const auto& stmt : ast->statements) {
        if (!stmt) continue;
        auto* imp = dynamic_cast<ImportNode*>(stmt.get());
        if (!imp) continue;

        std::string subPath = canonicalize(
            thisDir, imp->getFilePath(), imp->isExternImport());
        std::string subDir =
            std::filesystem::path(subPath).parent_path().string();

        visit(subPath, subDir, imp->getAlias(), imp->isExternImport());
    }

    visitChain.pop_back();
    visiting.erase(canonicalPath);

    CompileUnit unit;
    unit.canonicalPath = canonicalPath;
    unit.ast           = ast;
    unit.alias         = alias;
    unit.isExtern      = isExtern;
    order.push_back(std::move(unit));
}

std::vector<VyneLinker::CompileUnit>
VyneLinker::link(const std::string& entryPath) {
    parsedFiles.clear();
    visiting.clear();
    scheduled.clear();
    visitChain.clear();
    order.clear();

    std::filesystem::path entryAbs =
        std::filesystem::weakly_canonical(std::filesystem::path(entryPath));
    std::string canon = entryAbs.string();
    std::string dir   = entryAbs.parent_path().string();

    visit(canon, dir, "", false);
    return order;
}