ruleset { dynamic_casting };

interface Response {
    headers :: Map,
    body :: String
    
    set_header(key :: String, value :: String) {
        self.headers[key] = value;
    }

    send(data :: Map) {
        self.body = data;
    }
}

fn create_response() -> Response {
    return Response({}, "");
}

resp = create_response();
resp.set_header("Content-Type", "application/json");
resp.send({"message": "OK"});
out(resp.headers["Content-Type"]);
out(resp.body);