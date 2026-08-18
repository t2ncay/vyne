#include "vserv_common.h"

namespace VServNative {

Value native_ws_upgrade(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("ws_upgrade() requires request and response");
    
    // Parse WebSocket handshake
    std::string key = get_header(args[0], "Sec-WebSocket-Key");
    if (key.empty()) throw std::runtime_error("Missing WebSocket key");
    
    // Compute accept key
    key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char hash[20];
    SHA1((unsigned char*)key.c_str(), key.length(), hash);
    
    std::string accept = base64_encode(hash, 20);
    
    // Set response headers
    auto resp = args[1].asMap();
    auto headers = resp[StringPool::intern("headers")].asMap();
    headers[StringPool::intern("Upgrade")] = Value("websocket");
    headers[StringPool::intern("Connection")] = Value("Upgrade");
    headers[StringPool::intern("Sec-WebSocket-Accept")] = Value(accept);
    resp[StringPool::intern("headers")] = Value(headers);
    resp[StringPool::intern("status")] = Value((int64_t)101);
    
    return Value(resp);
}

Value native_ws_send(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("ws_send() requires socket and message");
    
    int socket = (int)args[0].asInt();
    std::string msg = args[1].asString();
    
    // Send WebSocket frame
    std::vector<char> frame;
    frame.push_back(0x81); // FIN + Opcode 0x1 (text)
    
    size_t len = msg.length();
    if (len <= 125) {
        frame.push_back((char)len);
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back((char)((len >> 8) & 0xFF));
        frame.push_back((char)(len & 0xFF));
    } else {
        // For larger messages (rare), we'd need a more complex frame
        frame.push_back(127);
        // 64-bit length not implemented here for simplicity
        throw std::runtime_error("WebSocket message too large (>65535 bytes)");
    }
    
    frame.insert(frame.end(), msg.begin(), msg.end());
    
    send(socket, frame.data(), frame.size(), 0);
    return Value(true);
}

} // namespace VServNative