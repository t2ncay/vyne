#include "vserv_common.h"

namespace VServNative {

Value native_create_app(std::vector<Value>& args) {
    std::unordered_map<uint32_t, Value> app;
    app[StringPool::intern("routes")] = Value(std::vector<Value>{});
    app[StringPool::intern("middleware")] = Value(std::vector<Value>{});
    app[StringPool::intern("port")] = Value((int64_t)8080);
    
    return Value(app);
}

Value native_app_get(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("app_get() requires app, path, handler");
    
    auto app = args[0].asMap();
    std::string path = args[1].asString();
    Value handler = args[2];
    
    auto routes = app[StringPool::intern("routes")].asList();
    std::unordered_map<uint32_t, Value> route;
    route[StringPool::intern("method")] = Value("GET");
    route[StringPool::intern("path")] = Value(path);
    route[StringPool::intern("handler")] = handler;
    routes.push_back(Value(route));
    app[StringPool::intern("routes")] = Value(routes);
    
    return Value(app);
}

Value native_app_listen(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("app_listen() requires app");
    
    auto app = args[0].asMap();
    int port = (int)app[StringPool::intern("port")].asInt();
    
    // Create and start server
    std::vector<Value> create_args = { Value((int64_t)port) };
    Value server = native_create_server(create_args);
    
    // Register routes
    auto routes = app[StringPool::intern("routes")].asList();
    for (const auto& route_val : routes) {
        auto route = route_val.asMap();
        std::string method = route[StringPool::intern("method")].asString();
        std::string path = route[StringPool::intern("path")].asString();
        Value handler = route[StringPool::intern("handler")];
        
        std::vector<Value> route_args = { server, Value(path), handler };
        if (method == "GET") {
            native_server_get(route_args);
        } else if (method == "POST") {
            native_server_post(route_args);
        }
    }
    
    std::vector<Value> listen_args = { server };
    return native_server_listen(listen_args);
}

} // namespace VServNative