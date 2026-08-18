#include "vserv_common.h"

namespace VServNative {

// ============================================================
// HTTP SERVER CORE
// ============================================================

Value native_create_server(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("create_server() requires port");
    int port = (int)args[0].asInt();
    
    Server* server = new Server();
    server->port = port;
    server->socket = -1;
    server->running = false;
    server->routes.clear();
    server->middlewares.clear();
    
    return Value(reinterpret_cast<int64_t>(server));
}

Value native_server_get(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("server_get() requires server_ptr, path, and handler");
    
    Server* server = reinterpret_cast<Server*>(args[0].asInt());
    std::string path = args[1].asString();
    Value handler = args[2];
    
    server->routes.push_back(std::make_pair("GET", std::make_pair(path, handler)));
    return Value(true);
}

Value native_server_post(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("server_post() requires server_ptr, path, and handler");
    
    Server* server = reinterpret_cast<Server*>(args[0].asInt());
    std::string path = args[1].asString();
    Value handler = args[2];
    
    server->routes.push_back(std::make_pair("POST", std::make_pair(path, handler)));
    return Value(true);
}

Value native_server_use(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("server_use() requires server_ptr and middleware");
    
    Server* server = reinterpret_cast<Server*>(args[0].asInt());
    Value middleware = args[1];
    
    server->middlewares.push_back(middleware);
    return Value(true);
}

Value native_server_listen(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("listen() requires server_ptr");
    
    Server* server = reinterpret_cast<Server*>(args[0].asInt());
    
    // Initialize Winsock on Windows
    init_winsock();
    
    server->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (IS_INVALID_SOCKET(server->socket)) {
        throw std::runtime_error("Failed to create socket");
    }
    
    int opt = 1;
    setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);
    
    if (bind(server->socket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close_socket(server->socket);
        throw std::runtime_error("Failed to bind socket to port " + std::to_string(server->port));
    }
    
    if (listen(server->socket, 10) < 0) {
        close_socket(server->socket);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    server->running = true;
    
    while (server->running) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server->socket, (sockaddr*)&client_addr, &client_len);
        
        if (IS_INVALID_SOCKET(client_sock)) continue;
        
        handle_request(server, client_sock);
    }
    
    close_socket(server->socket);
    return Value(true);
}

} // namespace VServNative