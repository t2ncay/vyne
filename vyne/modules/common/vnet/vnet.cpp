#include "vnet.h"

static const Value EMPTY_PACKET_LIST = Value(std::vector<Value>{});
static std::unordered_map<std::string, in_addr> ip_cache;

namespace VNetNative {

bool net_initialized = false;

void ensure_init() {
    if (net_initialized) return;
#if defined(_WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw std::runtime_error("vnet: Failed to initialize Winsock");
    }
#endif
    net_initialized = true;
}

Value native_create_udp_socket(std::vector<Value>& args) {
    ensure_init();
    int port = (args.empty()) ? 0 : static_cast<int>(args[0].asInt());

    SocketHandle sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    int bufSize = 256 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&bufSize, sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&bufSize, sizeof(bufSize));

    if (IS_INVALID_SOCKET(sock)) {
        throw std::runtime_error("vnet: Failed to create UDP socket");
    }

    #if defined(_WIN32)
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
    #else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    #endif

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    bindAddr.sin_port = htons(port);

    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
        closesocket(sock);
        throw std::runtime_error("vnet: Failed to bind socket to port " + std::to_string(port));
    }

    return Value(static_cast<int64_t>(sock));
}

Value native_send_to(std::vector<Value>& args) {
    if (args.size() < 4) {
        throw std::runtime_error("send_to() requires socket_handle, dest_ip, dest_port, message");
    }

    SocketHandle sock = static_cast<SocketHandle>(args[0].asInt());
    const std::string& ip = args[1].asString();
    int port              = static_cast<int>(args[2].asInt());
    const std::string& msg = args[3].asString();

    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port   = htons(port);

    auto it = ip_cache.find(ip);
    if (it != ip_cache.end()) {
        destAddr.sin_addr = it->second;
    } else {
        inet_pton(AF_INET, ip.c_str(), &destAddr.sin_addr);
        ip_cache[ip] = destAddr.sin_addr; // Cache for subsequent frames
    }

    int sentBytes = sendto(sock, msg.c_str(), static_cast<int>(msg.length()), 0,
                           (sockaddr*)&destAddr, sizeof(destAddr));

    return Value(static_cast<int64_t>(sentBytes));
}

Value native_recv_from(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("recv_from() requires socket_handle");

    SocketHandle sock = static_cast<SocketHandle>(args[0].asInt());

    char buffer[2048];
    sockaddr_in senderAddr{};
    #if defined(_WIN32)
        int addrLen = sizeof(senderAddr);
    #else
        socklen_t addrLen = sizeof(senderAddr);
    #endif

    int bytesRead = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                             (sockaddr*)&senderAddr, &addrLen);

    if (bytesRead <= 0) {
        return EMPTY_PACKET_LIST;
    }

    buffer[bytesRead] = '\0';
    char senderIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &senderAddr.sin_addr, senderIP, INET_ADDRSTRLEN);
    int senderPort = ntohs(senderAddr.sin_port);

    std::vector<Value> result = {
        Value(std::string(buffer)),
        Value(std::string(senderIP)),
        Value(static_cast<int64_t>(senderPort))
    };

    return Value(result);
}

Value native_close_socket(std::vector<Value>& args) {
    if (args.empty()) return Value();
    SocketHandle sock = static_cast<SocketHandle>(args[0].asInt());
    closesocket(sock);
    return Value(true);
}

Value native_parse_package(std::vector<Value>& args) {
    if (args.size() < 2) return Value(std::vector<Value>{});

    const std::string& str = args[0].asString();
    char delim = args[1].asString()[0];

    size_t pos = str.find(delim);
    if (pos == std::string::npos) {
        return Value(std::vector<Value>{ Value(str), Value("") });
    }

    return Value(std::vector<Value>{
        Value(str.substr(0, pos)),
        Value(str.substr(pos + 1))
    });
}

Value native_hash_site(std::vector<Value>& args) {
    if (args.empty()) return Value(std::string(""));

    const std::string& site = args[0].asString();
    uint32_t salt = (args.size() > 1) ? static_cast<uint32_t>(args[1].asInt()) : 0x811c9dc5;

    uint32_t hash = salt;
    for (char c : site) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }

    char hexBuf[16];
    snprintf(hexBuf, sizeof(hexBuf), "%08x", hash);
    return Value(std::string(hexBuf));
}

} // namespace VNetNative

void setupVNet(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "vnet";
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vnet = env[path];
    vnet[pool.intern("udp_socket")]    = Value(VNetNative::native_create_udp_socket);
    vnet[pool.intern("send_to")]       = Value(VNetNative::native_send_to);
    vnet[pool.intern("recv_from")]     = Value(VNetNative::native_recv_from);
    vnet[pool.intern("close")]         = Value(VNetNative::native_close_socket);
    vnet[pool.intern("parse_package")] = Value(VNetNative::native_parse_package);
    vnet[pool.intern("hash_site")]     = Value(VNetNative::native_hash_site);
}