#include "vserv_common.h"

namespace VServNative {

Value native_parse_request(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("parse_request() requires raw_data");
    
    std::string raw = args[0].asString();
    Request req;
    
    // Parse request line
    size_t pos = raw.find("\r\n");
    std::string request_line = raw.substr(0, pos);
    
    size_t space1 = request_line.find(' ');
    size_t space2 = request_line.find(' ', space1 + 1);
    
    req.method = request_line.substr(0, space1);
    req.path = request_line.substr(space1 + 1, space2 - space1 - 1);
    
    // Parse headers
    size_t header_start = pos + 2;
    while (true) {
        size_t header_end = raw.find("\r\n", header_start);
        if (header_end == header_start) break;
        
        std::string header_line = raw.substr(header_start, header_end - header_start);
        size_t colon = header_line.find(':');
        if (colon != std::string::npos) {
            std::string key = header_line.substr(0, colon);
            std::string value = header_line.substr(colon + 2);
            req.headers[key] = value;
        }
        header_start = header_end + 2;
    }
    
    // Parse body
    size_t body_start = raw.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        req.body = raw.substr(body_start + 4);
    }
    
    // Return request as Value
    std::unordered_map<uint32_t, Value> req_map;
    req_map[StringPool::intern("method")] = Value(req.method);
    req_map[StringPool::intern("path")] = Value(req.path);
    
    // Build headers map
    std::unordered_map<uint32_t, Value> headers_map;
    for (const auto& [key, value] : req.headers) {
        headers_map[StringPool::intern(key)] = Value(value);
    }
    req_map[StringPool::intern("headers")] = Value(headers_map);
    req_map[StringPool::intern("body")] = Value(req.body);
    
    return Value(req_map);
}

Value native_create_response(std::vector<Value>& args) {
    std::unordered_map<uint32_t, Value> resp;
    resp[StringPool::intern("status")] = Value((int64_t)200);
    resp[StringPool::intern("headers")] = Value(std::unordered_map<uint32_t, Value>{});
    resp[StringPool::intern("body")] = Value("");
    
    return Value(resp);
}

Value native_response_set_status(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("set_status() requires response and status_code");
    
    auto resp = args[0].asMap();
    resp[StringPool::intern("status")] = Value(args[1].asInt());
    return Value(resp);
}

Value native_response_set_header(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("set_header() requires response, key, value");
    
    auto resp = args[0].asMap();
    auto headers = resp[StringPool::intern("headers")].asMap();
    headers[StringPool::intern(args[1].asString())] = Value(args[2].asString());
    resp[StringPool::intern("headers")] = Value(headers);
    return Value(resp);
}

Value native_response_send(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("send() requires response");
    
    auto resp = args[0].asMap();
    int status = (int)resp[StringPool::intern("status")].asInt();
    std::string body = resp[StringPool::intern("body")].asString();
    auto headers = resp[StringPool::intern("headers")].asMap();
    
    // Build HTTP response
    std::string response = "HTTP/1.1 " + std::to_string(status) + " " + get_status_text(status) + "\r\n";
    
    // Add headers
    for (const auto& [key, value] : headers) {
        response += StringPool::get(key) + ": " + value.asString() + "\r\n";
    }
    
    response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    response += "\r\n";
    response += body;
    
    return Value(response);
}

} // namespace VServNative