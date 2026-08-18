ruleset { dynamic_casting };

module vserv;

fn handle_index(req, res) {
    res.set_header("Content-Type", "text/html");
    res.send("<h1>VServ HTTP Server Working!</h1>");
}

fn handle_api(req, res) {
    res.set_header("Content-Type", "application/json");
    res.send({"status": "ok", "message": "VServ is running!"});
}

fn start_server() {
    server = vserv.create_server(8080);
    
    vserv.server_get(server, "/", handle_index);
    vserv.server_get(server, "/api/test", handle_api);
    
    out("[VServ] Server listening on port 8080");
    out("[VServ] http://localhost:8080/");
    out("[VServ] http://localhost:8080/api/test");
    
    vserv.server_listen(server);
}

start_server();