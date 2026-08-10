ruleset { dynamic_casting };
module vnet;
module vmath;
module vcore;

server_port :: Int64 = 8000;
server_sock = vnet.udp_socket(server_port);

# Dynamic Active Peer Tables (Tracks Port, IP, and URL)
active_ports :: Array = [];
active_ips   :: Array = [];
active_urls  :: Array = [];

# Security State
firewall_open :: Int64 = 1;
current_salt  :: Int64 = 42;
target_hash   :: Int64 = 1260;

# Virtual File System Database
vfs_paths :: Array = ["/sys/firewall.cfg", "/sys/logs.txt", "/vault/data.key"];
vfs_data  :: Array = ["PORT_80_OPEN=TRUE", "LOG_INIT_SUCCESS", "FLAG{VYNE_VNET_ROOT_ACCESS}"];

out("[VNET SERVER] MULTI-PC CYBERWARFARE GATEWAY ONLINE (PORT 8000)");

fn update_peer_session(port :: Int64, ip :: String, url :: String) {
    found_idx :: Int64 = -1;
    through i :: 0..(active_ports.length() - 1) -> loop {
        if (int64(active_ports[i]) == port) {
            found_idx = i;
            break;
        }
    };

    if (found_idx >= 0) {
        active_urls[found_idx] = url;
        active_ips[found_idx]  = ip;
    } else {
        active_ports.push(port);
        active_ips.push(ip);
        active_urls.push(url);
        out("[DYNAMIC REGISTRY]: NEW PEER CONNECTED FROM " + ip + ":" + string(port));
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
                update_peer_session(sender_port, sender_ip, payload);
                out("[ROUTE] NODE " + string(sender_port) + " (" + sender_ip + ") -> " + payload);
            }

            # --- 2. DYNAMIC NETSCAN ---
            if (cmd == "NETSCAN") {
                update_peer_session(sender_port, sender_ip, payload);
                peers_found :: String = "";

                through p :: 0..(active_ports.length() - 1) -> loop {
                    p_port :: Int64  = int64(active_ports[p]);
                    p_url  :: String = string(active_urls[p]);

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

            # --- 3. EXPLOIT: DENIAL OF SERVICE (DOS) ---
            if (cmd == "DOS") {
                target_node :: Int64 = int64(payload);
                target_ip   :: String = "127.0.0.1";
                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { target_ip = string(active_ips[p]); break; }
                };
                out("[EXPLOIT] DOS PAYLOAD: PORT " + string(sender_port) + " -> TARGET " + target_ip + ":" + string(target_node));
                vnet.send_to(server_sock, target_ip, target_node, "EXPLOIT:DOS");
                vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:DOS_PAYLOAD_DELIVERED");
            }

            # --- 4. EXPLOIT: BGP ROUTE HIJACK (REDIRECT) ---
            if (cmd == "REDIRECT") {
                sep :: Int64 = -1;
                through i :: 0..(payload.length() - 1) -> loop {
                    if (payload[i] == ":") { sep = i; break; }
                };
                if (sep > 0) {
                    target_node :: Int64  = int64(payload.substr(0, sep));
                    target_url  :: String = payload.substr(sep + 1, payload.length() - sep - 1);
                    target_ip   :: String = "127.0.0.1";
                    through p :: 0..(active_ports.length() - 1) -> loop {
                        if (int64(active_ports[p]) == target_node) { target_ip = string(active_ips[p]); break; }
                    };
                    out("[EXPLOIT] ROUTE HIJACK: PORT " + string(sender_port) + " -> TARGET " + string(target_node) + " FORCED TO " + target_url);
                    vnet.send_to(server_sock, target_ip, target_node, "EXPLOIT:REDIRECT:" + target_url);
                    vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:HIJACK_COMPLETED");
                }
            }

            # --- 5. EXPLOIT: REMOTE TELEMETRY SNOOP ---
            if (cmd == "SNOOP") {
                target_node :: Int64 = int64(payload);
                target_url  :: String = "UNKNOWN";
                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { target_url = string(active_urls[p]); break; }
                };
                vnet.send_to(server_sock, sender_ip, sender_port, "TELEMETRY:PORT_" + string(target_node) + "_ACTIVE_AT_" + target_url);
            }

            # --- 6. EXPLOIT: TRACE SPIKE ---
            if (cmd == "SPIKE") {
                target_node :: Int64 = int64(payload);
                target_ip   :: String = "127.0.0.1";
                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { target_ip = string(active_ips[p]); break; }
                };
                vnet.send_to(server_sock, target_ip, target_node, "EXPLOIT:TRACE_SPIKE");
                vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:PEER_TRACE_SPIKED");
            }

            # --- 7. PORT SCAN ---
            if (cmd == "SCAN") {
                status :: String = (firewall_open == 1) ? "SYS_STATUS:PORT_80_OPEN:SALT_" + string(current_salt) : "SYS_STATUS:FIREWALL_LOCKED";
                vnet.send_to(server_sock, sender_ip, sender_port, status);
            }

            # --- 8. SUBMIT HASH SOLUTION ---
            if (cmd == "CRACK") {
                attempt_val :: Int64 = int64(payload);
                computed    :: Int64 = (attempt_val * current_salt) % 9999;

                if (computed == target_hash) {
                    firewall_open = 0;
                    vnet.send_to(server_sock, sender_ip, sender_port, "AUTH:SUCCESS:ACCESS_GRANTED");
                    
                    through c :: 0..(active_ports.length() - 1) -> loop {
                        vnet.send_to(server_sock, string(active_ips[c]), int64(active_ports[c]), "ALERT:BREACH_DETECTED_FROM_PORT_" + string(sender_port));
                    };
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "AUTH:FAIL:INVALID_HASH");
                }
            }

            # --- 9. CAT FILE EXFILTRATION ---
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

            # --- 10. DEFENDER PATCH ---
            if (cmd == "PATCH") {
                firewall_open = 1;
                current_salt  = current_salt + 7;
                vnet.send_to(server_sock, sender_ip, sender_port, "SYS_STATUS:FIREWALL_PATCHED_SALT_UPDATED");
            }
        }
    }
}

vnet.close(server_sock);