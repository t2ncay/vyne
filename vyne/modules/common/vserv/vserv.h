#pragma once

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

struct Server {
    int port;
    int socket;
    bool running;
    std::vector<std::pair<std::string, std::pair<std::string, Value>>> routes;
    std::vector<Value> middlewares;
};

struct Request {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<uint32_t, Value> headers_map;
};

void setupVServ(SymbolContainer& env, StringPool& pool);