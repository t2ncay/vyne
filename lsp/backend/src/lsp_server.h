#ifndef LSP_SERVER_H
#define LSP_SERVER_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "../../../vyne/compiler/lexer/lexer.h"    // Path to your lexer
#include "../../../vyne/compiler/parser/parser.h"   // Path to your parser
#include "../../../vyne/compiler/ast/ast.h"      // Path to your ast

using json = nlohmann::json;

// Source location structure
struct SourceLocation {
    int startLine;
    int startCol;
    int endLine;
    int endCol;
    
    json toJson() const {
        return {
            {"start", {{"line", startLine}, {"character", startCol}}},
            {"end", {{"line", endLine}, {"character", endCol}}}
        };
    }
};

// Represents a single open document
class DocumentState {
public:
    std::string uri;
    std::string text;
    std::vector<Token> tokens;
    std::unique_ptr<ProgramNode> ast;
    std::vector<json> diagnostics;
    
    DocumentState(const std::string& u, const std::string& t) : uri(u), text(t) {}
    
    // Parse the document and generate diagnostics
    bool parse();
};

// Main LSP Server class
class LspServer {
private:
    std::map<std::string, std::unique_ptr<DocumentState>> documents;
    
    // Handler methods for LSP requests
    json handleInitialize(const json& params);
    json handleShutdown();
    void handleDidOpen(const json& params);
    void handleDidChange(const json& params);
    void handleDidClose(const json& params);
    json handleCompletion(const json& params);
    json handleDefinition(const json& params);
    json handleHover(const json& params);
    
    // Send diagnostics back to client
    void publishDiagnostics(const std::string& uri);
    
public:
    void run();
    json handleRequest(const json& req);
};

int runLspServer();

#endif