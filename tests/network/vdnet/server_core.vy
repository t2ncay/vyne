ruleset { dynamic_casting };
module vnet;
module vmath;
module vcore;

server_port :: Int64 = 8000;
server_sock = vnet.udp_socket(server_port);

# Dynamic Active Peer Tables
active_ports :: Array = [];
active_urls  :: Array = [];

# Security State
firewall_open :: Int64 = 1;
current_salt  :: Int64 = 42;
target_hash   :: Int64 = 1260;

# Virtual File System Database
vfs_paths :: Array = ["/sys/firewall.cfg", "/sys/logs.txt", "/vault/data.key"];
vfs_data  :: Array = ["PORT_80_OPEN=TRUE", "LOG_INIT_SUCCESS", "FLAG{VYNE_VNET_ROOT_ACCESS}"];

out("[VNET SERVER] DYNAMIC PEER GATEWAY ONLINE (PORT 8000)");

# Dynamic Session Helper: Register/Update Player URL
fn update_peer_session(port :: Int64, url :: String) {
    found_idx :: Int64 = -1;
    through i :: 0..(active_ports.length() - 1) -> loop {
        if (int64(active_ports[i]) == port) {
            found_idx = i;
            break;
        }
    };

    if (found_idx >= 0) {
        active_urls[found_idx] = url;
    } else {
        active_ports.push(port);
        active_urls.push(url);
        out("[DYNAMIC REGISTRY]: NEW PEER CONNECTED ON PORT " + string(port));
    }
}

while (true) {
    packet :: Array = vnet.recv_from(server_sock);
    
    if (packet.length() >= 3) {
        msg :: String = string(packet[0]);
        sender_ip   :: String = string(packet[1]);
        sender_port :: Int64  = int64(packet[2]);

        # Parse CSV Command Format: "CMD:DATA"
        sep_idx :: Int64 = -1;
        through i :: 0..(msg.length() - 1) -> loop {
            if (msg[i] == ":") { sep_idx = i; break; }
        };

        if (sep_idx > 0) {
            cmd :: String     = msg.substr(0, sep_idx);
            payload :: String = msg.substr(sep_idx + 1, msg.length() - sep_idx - 1);

            # --- 1. DYNAMIC ROUTE TRACKER ---
            if (cmd == "GET") {
                update_peer_session(sender_port, payload);
                out("[ROUTE] NODE " + string(sender_port) + " -> " + payload);
            }

            # --- 2. DYNAMIC NETSCAN (SCANS ALL RANDOM CLIENT PORTS) ---
            if (cmd == "NETSCAN") {
                update_peer_session(sender_port, payload);
                peers_found :: String = "";

                through p :: 0..(active_ports.length() - 1) -> loop {
                    p_port :: Int64  = int64(active_ports[p]);
                    p_url  :: String = string(active_urls[p]);

                    # Check if another node is on the same URL
                    if (p_port != sender_port && p_url == payload) {
                        peers_found = peers_found + "PORT_" + string(p_port) + " ";
                    }
                };

                if (peers_found == "") {
                    vnet.send_to(server_sock, sender_ip, sender_port, "NETSCAN: NO ACTIVE PEERS ON " + payload);
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "NETSCAN: PEERS DETECTED ON " + payload + " -> [" + peers_found + "]");
                }
            }

            # --- 3. P2P ATTACK DISPATCH ---
            if (cmd == "ATTACK") {
                target_node :: Int64 = int64(payload);
                out("[SECURITY ALERT] ATTACK DISPATCHED: PORT " + string(sender_port) + " -> TARGET PORT " + string(target_node));
                vnet.send_to(server_sock, "127.0.0.1", target_node, "ALERT:INBOUND_DOS_ATTACK_FROM_PORT_" + string(sender_port));
                vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:PACKET_DELIVERED_TO_PORT_" + string(target_node));
            }

            # --- 4. PORT SCAN ---
            if (cmd == "SCAN") {
                status :: String = (firewall_open == 1) ? "SYS_STATUS:PORT_80_OPEN:SALT_" + string(current_salt) : "SYS_STATUS:FIREWALL_LOCKED";
                vnet.send_to(server_sock, sender_ip, sender_port, status);
            }

            # --- 5. SUBMIT HASH SOLUTION ---
            if (cmd == "CRACK") {
                attempt_val :: Int64 = int64(payload);
                computed    :: Int64 = (attempt_val * current_salt) % 9999;

                if (computed == target_hash) {
                    firewall_open = 0;
                    vnet.send_to(server_sock, sender_ip, sender_port, "AUTH:SUCCESS:ACCESS_GRANTED");
                    
                    # Alert all connected active ports
                    through c :: 0..(active_ports.length() - 1) -> loop {
                        vnet.send_to(server_sock, "127.0.0.1", int64(active_ports[c]), "ALERT:BREACH_DETECTED_FROM_PORT_" + string(sender_port));
                    };
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "AUTH:FAIL:INVALID_HASH");
                }
            }

            # --- 6. CAT FILE EXFILTRATION ---
            if (cmd == "CAT") {
                if (firewall_open == 0) {
                    file_found :: Int64 = -1;
                    through f :: 0..(vfs_paths.length() - 1) -> loop {
                        if (string(vfs_paths[f]) == payload) { file_found = f; break; }
                    };

                    if (file_found >= 0) {
                        vnet.send_to(server_sock, sender_ip, sender_port, "FILE_DATA:" + string(vfs_data[file_found]));
                    } else {
                        vnet.send_to(server_sock, sender_ip, sender_port, "FILE_ERROR:NOT_FOUND");
                    }
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "SYS_ERROR:ACCESS_DENIED_LOCKOUT");
                }
            }

            # --- 7. DEFENDER PATCH ---
            if (cmd == "PATCH") {
                firewall_open = 1;
                current_salt  = current_salt + 7;
                vnet.send_to(server_sock, sender_ip, sender_port, "SYS_STATUS:FIREWALL_PATCHED_SALT_UPDATED");
            }
        }
    }
}

vnet.close(server_sock);