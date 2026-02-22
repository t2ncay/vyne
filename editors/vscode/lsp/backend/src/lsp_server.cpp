#include "lsp_server.h"

// Parse the document and collect diagnostics
bool DocumentState::parse() {
    try {
        // Tokenize the source code
        tokens = tokenize(text);
        
        // Parse into AST
        Parser parser(std::move(tokens));
        ast = parser.parseProgram();
        
        // Clear old diagnostics on success
        diagnostics.clear();
        return true;
    } catch (const std::exception& e) {
        // Create a diagnostic from the error
        json diag;
        diag["message"] = e.what();
        diag["severity"] = 1; // 1 = Error
        diag["range"] = {
            {"start", {{"line", 0}, {"character", 0}}},
            {"end", {{"line", 0}, {"character", 10}}}
        };
        diagnostics.push_back(diag);
        return false;
    }
}

// Main server loop - reads from stdin, writes to stdout
void LspServer::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
        try {
            // Parse the JSON request
            json req = json::parse(line);
            
            // Handle the request
            json resp = handleRequest(req);
            
            // Send response if not empty
            if (!resp.empty()) {
                std::cout << resp.dump() << std::endl;
            }
        } catch (const std::exception& e) {
            // Send error response
            json error;
            error["jsonrpc"] = "2.0";
            error["id"] = nullptr;
            error["error"] = {{"code", -32700}, {"message", e.what()}};
            std::cout << error.dump() << std::endl;
        }
    }
}

// Route requests to appropriate handlers
json LspServer::handleRequest(const json& req) {
    std::string method = req["method"];
    json params = req.contains("params") ? req["params"] : json::object();
    json resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = req["id"];
    
    if (method == "initialize") {
        resp["result"] = handleInitialize(params);
    }
    else if (method == "shutdown") {
        resp["result"] = handleShutdown();
    }
    else if (method == "textDocument/didOpen") {
        handleDidOpen(params);
        return json::object(); // No response for notifications
    }
    else if (method == "textDocument/didChange") {
        handleDidChange(params);
        return json::object();
    }
    else if (method == "textDocument/didClose") {
        handleDidClose(params);
        return json::object();
    }
    else if (method == "textDocument/completion") {
        resp["result"] = handleCompletion(params);
    }
    else if (method == "textDocument/definition") {
        resp["result"] = handleDefinition(params);
    }
    else if (method == "textDocument/hover") {
        resp["result"] = handleHover(params);
    }
    else {
        resp["error"] = {{"code", -32601}, {"message", "Method not found"}};
    }
    
    return resp;
}

// Initialize the server
json LspServer::handleInitialize(const json& params) {
    return {
        {"capabilities", {
            {"textDocumentSync", 1}, // Full content sync
            {"completionProvider", {
                {"resolveProvider", false},
                {"triggerCharacters", {".", ":", " "}}
            }},
            {"definitionProvider", true},
            {"hoverProvider", true}
        }},
        {"serverInfo", {{"name", "vlang-lsp"}, {"version", "1.0.0"}}}
    };
}

// Shutdown
json LspServer::handleShutdown() {
    return nullptr;
}

// Document opened
void LspServer::handleDidOpen(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    std::string text = params["textDocument"]["text"];
    
    auto doc = std::make_unique<DocumentState>(uri, text);
    doc->parse();
    documents[uri] = std::move(doc);
    
    publishDiagnostics(uri);
}

// Document changed
void LspServer::handleDidChange(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    
    auto it = documents.find(uri);
    if (it != documents.end()) {
        // Update content (simplified - full content sync)
        for (const auto& change : params["contentChanges"]) {
            it->second->text = change["text"];
        }
        
        // Reparse
        it->second->parse();
        publishDiagnostics(uri);
    }
}

// Document closed
void LspServer::handleDidClose(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    documents.erase(uri);
}

// Send diagnostics to client
void LspServer::publishDiagnostics(const std::string& uri) {
    auto it = documents.find(uri);
    if (it == documents.end()) return;
    
    json notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/publishDiagnostics";
    notification["params"] = {
        {"uri", uri},
        {"diagnostics", it->second->diagnostics}
    };
    
    std::cout << notification.dump() << std::endl;
}

// Provide completion items
json LspServer::handleCompletion(const json& params) {
    json result = json::array();
    
    // Basic keywords - add all your language keywords here
    std::vector<std::string> keywords = {
        "group", "sub", "if", "else", "while", "through", 
        "return", "break", "continue", "module", "dismiss",
        "const", "use", "deploy", "as", "extern",
        "true", "false", "null",
        "loop", "collect", "unique", "every", "filter"
    };
    
    // Built-in functions
    std::vector<std::string> builtins = {
        "out", "sizeof", "type", "string", "number", "sequence"
    };
    
    for (const auto& k : keywords) {
        result.push_back({
            {"label", k},
            {"kind", 14}, // 14 = Keyword
            {"detail", "keyword"},
            {"insertText", k}
        });
    }
    
    for (const auto& b : builtins) {
        result.push_back({
            {"label", b},
            {"kind", 3}, // 3 = Function
            {"detail", "built-in"},
            {"insertText", b + "()"}
        });
    }
    
    return result;
}

// Find definition of symbol at position
json LspServer::handleDefinition(const json& params) {
    // TODO: Implement once we have location info in AST
    return json::array();
}

// Show hover information
json LspServer::handleHover(const json& params) {
    // TODO: Implement once we have symbol info
    return json::object();
}

// Implementation of runLspServer
int runLspServer() {
    LspServer server;
    server.run();
    return 0;
}