#pragma once

#include "vserv.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <vector>

// For SHA1 (WebSocket)
#include <openssl/sha.h>

// ============================================================
// Platform-specific socket includes
// ============================================================
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    
    typedef SOCKET SocketHandle;
    #define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
    #define close_socket closesocket
    #define socket_errno WSAGetLastError()
    
    // Windows doesn't have these in sys/socket.h
    #ifndef SHUT_RDWR
    #define SHUT_RDWR 2
    #endif
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    
    typedef int SocketHandle;
    #define IS_INVALID_SOCKET(s) ((s) < 0)
    #define close_socket close
    #define socket_errno errno
    #define INVALID_SOCKET -1
#endif

extern SymbolContainer* g_vserv_env;

namespace VServNative {
    Value native_parse_request(std::vector<Value>& args);
    Value native_create_response(std::vector<Value>& args);
    Value native_response_set_status(std::vector<Value>& args);
    Value native_response_set_header(std::vector<Value>& args);
    Value native_response_send(std::vector<Value>& args);
    Value native_create_server(std::vector<Value>& args);
    Value native_server_get(std::vector<Value>& args);
    Value native_server_post(std::vector<Value>& args);
    Value native_server_use(std::vector<Value>& args);
    Value native_server_listen(std::vector<Value>& args);
    Value native_serve_file(std::vector<Value>& args);
    Value native_create_app(std::vector<Value>& args);
    Value native_app_get(std::vector<Value>& args);
    Value native_app_listen(std::vector<Value>& args);
    Value native_ws_upgrade(std::vector<Value>& args);
    Value native_ws_send(std::vector<Value>& args);
}

// ============================================================
// Helper Functions
// ============================================================

static inline std::string get_status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Unknown";
    }
}

static inline std::string get_file_extension(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    return path.substr(dot + 1);
}

static inline std::string get_mime_type(const std::string& ext) {
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js" || ext == "mjs") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "jsonld") return "application/ld+json";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "webp") return "image/webp";
    if (ext == "txt") return "text/plain";
    if (ext == "xml") return "application/xml";
    if (ext == "pdf") return "application/pdf";
    if (ext == "zip") return "application/zip";
    if (ext == "gz") return "application/gzip";
    if (ext == "mp4") return "video/mp4";
    if (ext == "webm") return "video/webm";
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "wav") return "audio/wav";
    if (ext == "ogg") return "audio/ogg";
    if (ext == "wasm") return "application/wasm";
    return "application/octet-stream";
}

static inline std::string get_header(const Value& req, const std::string& name) {
    if (req.getType() != Value::MAP) return "";
    auto map = req.asMap();
    uint32_t headersId = StringPool::intern("headers");
    auto it = map.find(headersId);
    if (it == map.end()) return "";
    if (it->second.getType() != Value::MAP) return "";
    
    auto headers = it->second.asMap();
    uint32_t keyId = StringPool::intern(name);
    auto hit = headers.find(keyId);
    if (hit == headers.end()) return "";
    return hit->second.asString();
}

static inline std::string base64_encode(const unsigned char* data, size_t len) {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string result;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                result += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            result += base64_chars[char_array_4[j]];

        while (i++ < 3)
            result += '=';
    }

    return result;
}

static inline void init_winsock() {
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("Failed to initialize Winsock");
        }
        initialized = true;
    }
#endif
}

// ============================================================
// Forward declarations for VServNative functions
// ============================================================

namespace VServNative {
    // These are declared in other .cpp files
    Value native_parse_request(std::vector<Value>& args);
    Value native_create_response(std::vector<Value>& args);
    Value native_response_set_status(std::vector<Value>& args);
    Value native_response_set_header(std::vector<Value>& args);
    Value native_response_send(std::vector<Value>& args);
    Value native_create_server(std::vector<Value>& args);
    Value native_server_get(std::vector<Value>& args);
    Value native_server_post(std::vector<Value>& args);
    Value native_server_use(std::vector<Value>& args);
    Value native_server_listen(std::vector<Value>& args);
    Value native_serve_file(std::vector<Value>& args);
    Value native_create_app(std::vector<Value>& args);
    Value native_app_get(std::vector<Value>& args);
    Value native_app_listen(std::vector<Value>& args);
    Value native_ws_upgrade(std::vector<Value>& args);
    Value native_ws_send(std::vector<Value>& args);
}

// ============================================================
// Request Handling
// ============================================================

static inline void handle_request(Server* server, int client_sock) {
    char buffer[8192] = {0};
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes <= 0) {
        close_socket(client_sock);
        return;
    }
    
    std::string raw_request(buffer, bytes);
    
    // Parse request
    std::vector<Value> parse_args;
    parse_args.push_back(Value(raw_request));
    Value req_val = VServNative::native_parse_request(parse_args);
    
    // Create response
    std::vector<Value> create_resp_args;
    Value resp_val = VServNative::native_create_response(create_resp_args);
    
    // Apply middlewares
    for (const auto& mw : server->middlewares) {
        // TODO: Call middleware function
        // mw.asFunction().nativeFn({req_val, resp_val});
    }
    
    // Find matching route
    auto req_map = req_val.asMap();
    uint32_t methodId = StringPool::intern("method");
    uint32_t pathId = StringPool::intern("path");
    
    std::string method = req_map[methodId].asString();
    std::string path = req_map[pathId].asString();
    
    bool handled = false;
    for (const auto& route : server->routes) {
        if (route.first == method && route.second.first == path) {
            auto funcData = route.second.second.asFunction();
            
            if (!funcData) {
                throw std::runtime_error("Handler is not a function!");
            }
            
            if (funcData->isNative) {
                std::vector<Value> handler_args = {req_val, resp_val};
                funcData->nativeFn(handler_args);
            } else {
                if (funcData->body.empty()) {
                    throw std::runtime_error("Function body is empty!");
                }
                
                if (!g_vserv_env) {
                    throw std::runtime_error("Global environment not set!");
                }
                
                static uint64_t call_id = 0;
                std::string scopeName = "call_" + std::to_string(call_id++);
                uint32_t scopeId = StringPool::intern(scopeName);
                (*g_vserv_env)[scopeId] = SymbolTable();
                
                static uint32_t reqId = StringPool::intern("req");
                static uint32_t resId = StringPool::intern("res");
                (*g_vserv_env)[scopeId][reqId] = req_val;
                (*g_vserv_env)[scopeId][resId] = resp_val;
                
                for (const auto& stmt : funcData->body) {
                    if (stmt) stmt->evaluate(*g_vserv_env, scopeId);
                }
            }
            
            handled = true;
            break;
        }
    }
    
    if (!handled) {
        // 404 Not Found
        std::vector<Value> status_args;
        status_args.push_back(resp_val);
        status_args.push_back(Value((int64_t)404));
        resp_val = VServNative::native_response_set_status(status_args);
        
        // Set the 404 body
        auto resp_map = resp_val.asMap();
        resp_map[StringPool::intern("body")] = Value("404 - Not Found");
        resp_val = Value(resp_map);
    }
    
    // Send response
    std::vector<Value> send_args;
    send_args.emplace_back(resp_val);
    Value response_str = VServNative::native_response_send(send_args);
    send(client_sock, response_str.asString().c_str(), response_str.asString().length(), 0);
    
    close_socket(client_sock);
}