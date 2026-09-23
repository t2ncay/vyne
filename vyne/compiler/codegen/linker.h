#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class ProgramNode;

// VyneLinker
// ----------
// Resolves the import graph for the transpiler *before* codegen runs.
// The interpreter path does not use this class.
//
// Responsibilities:
//   1. Parse every transitively-imported file exactly once.
//   2. Detect cycles and report them with a readable chain.
//   3. Emit compile units in topological order (leaves first, entry last).
//
// The emitter then iterates the returned units and calls ProgramNode::compile
// (or compileAliased) on each. Because we produce a single C translation
// unit, this is the equivalent of a "unity build" linker.

class VyneLinker {
public:
    struct CompileUnit {
        std::string canonicalPath;             // absolute, normalized
        std::shared_ptr<ProgramNode> ast;
        std::string alias;                     // "" for plain import
        bool isExtern = false;
    };

    // Returns units in topological order (leaves first, entry last).
    // Throws std::runtime_error on missing files or import cycles.
    std::vector<CompileUnit> link(const std::string& entryPath);

private:
    std::unordered_map<std::string, std::shared_ptr<ProgramNode>> parsedFiles;
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> scheduled;   // "path|alias"
    std::vector<std::string> visitChain;
    std::vector<CompileUnit> order;

    void visit(const std::string& canonicalPath,
               const std::string& sourceDir,
               const std::string& alias,
               bool isExtern);

    std::string canonicalize(const std::string& baseDir,
                             const std::string& rawPath,
                             bool isExtern);

    std::shared_ptr<ProgramNode> parseFile(const std::string& canonicalPath,
                                           const std::string& sourceDir);
};