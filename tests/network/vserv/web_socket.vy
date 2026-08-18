ruleset { dynamic_casting };

module vserv;

# WebSocket upgrade handler
fn handle_ws_upgrade(req, res) {
    out("WebSocket upgrade request received");
    
    # Perform the WebSocket handshake
    res = vserv.ws_upgrade(req, res);
    
    # Return the upgraded response
    return res;
}

# HTML page handler
fn handle_index(req, res) {
    html = "
<!DOCTYPE html>
<html>
<head><title>WebSocket Test</title></head>
<body>
    <h1>WebSocket Echo Test</h1>
    <input type='text' id='msg' placeholder='Type a message'>
    <button onclick='sendMsg()'>Send</button>
    <div id='output'></div>
    <script>
        let ws = new WebSocket('ws://localhost:8080/ws');
        ws.onmessage = (e) => document.getElementById('output').innerHTML += '<p>Server: ' + e.data + '</p>';
        ws.onopen = () => document.getElementById('output').innerHTML += '<p style=\"color:green\">✅ Connected</p>';
        function sendMsg() {
            let msg = document.getElementById('msg').value;
            ws.send(msg);
            document.getElementById('output').innerHTML += '<p>You: ' + msg + '</p>';
        }
    </script>
</body>
</html>
    ";
    
    res.set_header("Content-Type", "text/html");
    res.send(html);
}

fn start_websocket_server() {
    server = vserv.create_server(8080);
    
    # Register routes with function names (not inline functions)
    vserv.server_get(server, "/", handle_index);
    vserv.server_get(server, "/ws", handle_ws_upgrade);
    
    out("[VServ] WebSocket server on ws://localhost:8080/ws");
    out("[VServ] http://localhost:8080/");
    
    vserv.server_listen(server);
}

start_websocket_server();