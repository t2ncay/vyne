ruleset { dynamic_casting };
module vglib;
module vnet;
module vmath;

# Initialize Vyne Window
vglib.init(1280, 720, 60, "VYNE REAL-TIME MULTIPLAYER SANDBOX", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

# Listening on 7002, sending to 7001
my_port   :: Int64  = 7002;
peer_ip   :: String = "127.0.0.1";
peer_port :: Int64  = 7001;

# Create Non-Blocking UDP Socket
net_sock = vnet.udp_socket(my_port);

# Position local player on the right
my_x :: Float64 = 900.0;
my_y :: Float64 = 360.0;

peer_x :: Float64 = -100.0; # Offscreen until connected
peer_y :: Float64 = -100.0;
peer_active :: Int64 = 0;

COLOR_BG    = vglib.rgba(12, 14, 20, 255);
COLOR_PANEL = vglib.rgba(22, 28, 40, 255);
COLOR_LOCAL = vglib.rgba(0, 240, 255, 255);   # Electric Cyan (Local Player)
COLOR_PEER  = vglib.rgba(255, 45, 120, 255);  # Neon Pink (Remote Friend)

speed :: Float64 = 5.0;

while (vglib.running()) {
    # 1. Local WASD Movement
    if (vglib.key_down(vglib.W)) { my_y = my_y - speed; }
    if (vglib.key_down(vglib.S)) { my_y = my_y + speed; }
    if (vglib.key_down(vglib.A)) { my_x = my_x - speed; }
    if (vglib.key_down(vglib.D)) { my_x = my_x + speed; }

    # 2. Broadcast Local Position to Friend
    packet_out :: String = string(vmath.round(my_x)) + "," + string(vmath.round(my_y));
    vnet.send_to(net_sock, peer_ip, peer_port, packet_out);

    # 3. Non-blocking Receive Packet Queue
    packet_in :: Array = vnet.recv_from(net_sock);
    if (packet_in.length() >= 3) {
        msg :: String = string(packet_in[0]);
        
        # Split CSV coordinates (e.g., "350,420")
        comma_idx = msg.find(",");
        if (comma_idx > 0) {
            px_str = msg.substr(0, comma_idx);
            py_str = msg.substr(comma_idx + 1, msg.length() - comma_idx - 1);
            
            peer_x = float64(px_str);
            peer_y = float64(py_str);
            peer_active = 1;
        }
    }

    # 4. Render Frame
    vglib.begin();
        vglib.clear(COLOR_BG);

        # Grid Background
        vglib.grid(20, 2.0);

        # Render Remote Peer (Pink Circle)
        if (peer_active == 1) {
            vglib.circle(peer_x, peer_y, 24.0, COLOR_PEER);
            vglib.text_ex(vcr_font, "FRIEND", peer_x - 25.0, peer_y - 45.0, 10, COLOR_PEER);
        }

        # Render Local Player (Cyan Square)
        vglib.rect(my_x - 20.0, my_y - 20.0, 40.0, 40.0, COLOR_LOCAL);
        vglib.text_ex(vcr_font, "YOU", my_x - 12.0, my_y - 40.0, 10, COLOR_LOCAL);

        # HUD Status Bar
        vglib.rect(20, 20, 380, 90, COLOR_PANEL);
        vglib.text_ex(vcr_font, "VYNE VNET MULTIPLAYER ENGINE", 35, 32, 11, COLOR_LOCAL);
        vglib.text_ex(vcr_font, "BIND PORT: " + string(my_port) + " | TARGET: " + string(peer_port), 35, 52, 10, vglib.rgba(255, 170, 0, 255));
        
        status_txt = (peer_active == 1) ? "STATUS: CONNECTED (PEER ACTIVE)" : "STATUS: WAITING FOR PEER...";
        vglib.text_ex(vcr_font, status_txt, 35, 72, 10, (peer_active == 1) ? COLOR_LOCAL : COLOR_PEER);

    vglib.end();
}

vnet.close(net_sock);
vglib.close();