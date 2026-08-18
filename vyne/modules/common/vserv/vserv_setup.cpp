#include "vserv_common.h"

// ============================================================
// VServ Setup - Register all native functions
// ============================================================

SymbolContainer* g_vserv_env = nullptr;

void setupVServ(SymbolContainer& env, StringPool& pool) {
    g_vserv_env = &env; 
    
    init_winsock();
    
    const std::string& path = "vserv";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }
    
    auto& vserv = env[path];
    
    vserv[pool.intern("create_server")] = Value(VServNative::native_create_server);
    vserv[pool.intern("server_get")] = Value(VServNative::native_server_get);
    vserv[pool.intern("server_post")] = Value(VServNative::native_server_post);
    vserv[pool.intern("server_put")] = Value(VServNative::native_server_post); // TODO: Add proper PUT
    vserv[pool.intern("server_delete")] = Value(VServNative::native_server_post); // TODO: Add proper DELETE
    vserv[pool.intern("server_use")] = Value(VServNative::native_server_use);
    vserv[pool.intern("server_listen")] = Value(VServNative::native_server_listen);
    
    vserv[pool.intern("parse_request")] = Value(VServNative::native_parse_request);
    vserv[pool.intern("create_response")] = Value(VServNative::native_create_response);
    vserv[pool.intern("set_status")] = Value(VServNative::native_response_set_status);
    vserv[pool.intern("set_header")] = Value(VServNative::native_response_set_header);
    vserv[pool.intern("send")] = Value(VServNative::native_response_send);
    
    vserv[pool.intern("serve_file")] = Value(VServNative::native_serve_file);
    
    // Web framework (Express-like)
    vserv[pool.intern("create_app")] = Value(VServNative::native_create_app);
    vserv[pool.intern("app_get")] = Value(VServNative::native_app_get);
    vserv[pool.intern("app_post")] = Value(VServNative::native_app_get); // TODO: Add proper POST
    vserv[pool.intern("app_listen")] = Value(VServNative::native_app_listen);
    
    // WebSocket
    vserv[pool.intern("ws_upgrade")] = Value(VServNative::native_ws_upgrade);
    vserv[pool.intern("ws_send")] = Value(VServNative::native_ws_send);
    
    // HTTP status constants
    vserv[pool.intern("OK")] = Value((int64_t)200).setReadOnly();
    vserv[pool.intern("CREATED")] = Value((int64_t)201).setReadOnly();
    vserv[pool.intern("ACCEPTED")] = Value((int64_t)202).setReadOnly();
    vserv[pool.intern("NO_CONTENT")] = Value((int64_t)204).setReadOnly();
    vserv[pool.intern("MOVED_PERMANENTLY")] = Value((int64_t)301).setReadOnly();
    vserv[pool.intern("FOUND")] = Value((int64_t)302).setReadOnly();
    vserv[pool.intern("BAD_REQUEST")] = Value((int64_t)400).setReadOnly();
    vserv[pool.intern("UNAUTHORIZED")] = Value((int64_t)401).setReadOnly();
    vserv[pool.intern("FORBIDDEN")] = Value((int64_t)403).setReadOnly();
    vserv[pool.intern("NOT_FOUND")] = Value((int64_t)404).setReadOnly();
    vserv[pool.intern("METHOD_NOT_ALLOWED")] = Value((int64_t)405).setReadOnly();
    vserv[pool.intern("SERVER_ERROR")] = Value((int64_t)500).setReadOnly();
    vserv[pool.intern("NOT_IMPLEMENTED")] = Value((int64_t)501).setReadOnly();
    vserv[pool.intern("BAD_GATEWAY")] = Value((int64_t)502).setReadOnly();
    vserv[pool.intern("SERVICE_UNAVAILABLE")] = Value((int64_t)503).setReadOnly();
    
    // MIME types
    vserv[pool.intern("MIME_HTML")] = Value("text/html").setReadOnly();
    vserv[pool.intern("MIME_JSON")] = Value("application/json").setReadOnly();
    vserv[pool.intern("MIME_CSS")] = Value("text/css").setReadOnly();
    vserv[pool.intern("MIME_JS")] = Value("application/javascript").setReadOnly();
    vserv[pool.intern("MIME_PNG")] = Value("image/png").setReadOnly();
    vserv[pool.intern("MIME_JPG")] = Value("image/jpeg").setReadOnly();
    vserv[pool.intern("MIME_GIF")] = Value("image/gif").setReadOnly();
    vserv[pool.intern("MIME_TXT")] = Value("text/plain").setReadOnly();
    vserv[pool.intern("MIME_XML")] = Value("application/xml").setReadOnly();
    vserv[pool.intern("MIME_PDF")] = Value("application/pdf").setReadOnly();
    vserv[pool.intern("MIME_WASM")] = Value("application/wasm").setReadOnly();
    vserv[pool.intern("MIME_MP4")] = Value("video/mp4").setReadOnly();
    vserv[pool.intern("MIME_WEBM")] = Value("video/webm").setReadOnly();
    vserv[pool.intern("MIME_MP3")] = Value("audio/mpeg").setReadOnly();
    vserv[pool.intern("MIME_WAV")] = Value("audio/wav").setReadOnly();
    vserv[pool.intern("MIME_OGG")] = Value("audio/ogg").setReadOnly();
    
    // Version
    vserv[pool.intern("version")] = Value("v0.1.0-alpha").setReadOnly();
}