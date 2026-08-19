ruleset { dynamic_casting };

module vserv;

# Serve the main page
fn handle_index(req, res) {
    res["headers"]["Content-Type"] = "text/html";
    res["body"] = "
<!DOCTYPE html>
<html>
<head>
    <title>VServ Test</title>
    <link rel='stylesheet' href='/style.css'>
</head>
<body>
    <h1>VServ HTTP Server Working!</h1>
    <p>If you see this page with styling, the CSS is working!</p>
</body>
</html>
    ";
}

# Serve CSS file from root
fn handle_css(req, res) {
    file_response = vserv.serve_file("style.css");
    res["status"] = file_response["status"];
    res["headers"] = file_response["headers"];
    res["body"] = file_response["body"];
}

# Start the server
fn start_server() {
    server = vserv.create_server(8080);
    
    vserv.server_get(server, "/", handle_index);
    vserv.server_get(server, "/style.css", handle_css);
    
    out("[VServ] Server listening on port 8080");
    out("[VServ] http://localhost:8080/");
    
    vserv.server_listen(server);
}

start_server();