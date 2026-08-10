ruleset { dynamic_casting };
module vnet;
module vmath;
module vcore;

server_port :: Int64 = 8000;
server_sock = vnet.udp_socket(server_port);

# Known Client Nodes (2 Attackers, 2 Defenders)
clients :: Array = [8001, 8002, 8003, 8004];

# Virtual File System State
vfs_paths :: Array = ["/sys/firewall.cfg", "/sys/logs.txt", "/vault/data.key"];
vfs_data  :: Array = ["PORT_80_OPEN=TRUE", "LOG_INIT_SUCCESS", "FLAG{VYNE_VNET_ROOT_ACCESS}"];

# Security System State
firewall_open :: Int64   = 1;
current_salt  :: Int64   = 42;
target_hash   :: Int64   = 1260;

out("[VNET SERVER] CENTRAL CORE LISTENING ON PORT 8000");

while (true) {
    packet :: Array = vnet.recv_from(server_sock);
    
    if (packet.length() >= 3) {
        msg :: String = string(packet[0]);
        sender_ip   :: String = string(packet[1]);
        sender_port :: Int64  = int64(packet[2]);

        # Parse CSV Command Format: "CMD:DATA"
        sep_idx :: Int64 = -1;
        through i :: 0..(msg.length() - 1) -> loop {
            if (msg[i] == ":") {
                sep_idx = i;
                break;
            }
        };

        if (sep_idx > 0) {
            cmd :: String = msg.substr(0, sep_idx);
            payload :: String = msg.substr(sep_idx + 1, msg.length() - sep_idx - 1);

            # --- COMMAND: PORT SCAN ---
            if (cmd == "SCAN") {
                status :: String = (firewall_open == 1) ? "SYS_STATUS:PORT_80_OPEN:SALT_" + string(current_salt) : "SYS_STATUS:FIREWALL_LOCKED";
                vnet.send_to(server_sock, sender_ip, sender_port, status);
            }

            # --- COMMAND: SUBMIT HASH SOLUTION ---
            if (cmd == "CRACK") {
                attempt_val :: Int64 = int64(payload);
                
                # Computed Hash Verification: (val * salt) % 9999
                computed :: Int64 = int64(vmath.fmod(float64(attempt_val * current_salt), 9999.0));

                if (computed == target_hash) {
                    firewall_open = 0; # Breach successful
                    vnet.send_to(server_sock, sender_ip, sender_port, "AUTH:SUCCESS:ACCESS_GRANTED");
                    
                    # Alert Defenders
                    through c :: 0..(clients.length() - 1) -> loop {
                        vnet.send_to(server_sock, "127.0.0.1", int64(clients[c]), "ALERT:BREACH_DETECTED_FROM_" + string(sender_port));
                    };
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "AUTH:FAIL:INVALID_HASH");
                }
            }

            # --- COMMAND: FILE EXPLORER (READ_FILE) ---
            if (cmd == "CAT") {
                if (firewall_open == 0 || sender_port >= 8003) { # Granted if cracked OR defender
                    file_found :: Int64 = -1;
                    through f :: 0..(vfs_paths.length() - 1) -> loop {
                        if (string(vfs_paths[f]) == payload) {
                            file_found = f;
                            break;
                        }
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

            # --- COMMAND: DEFENDER PATCH ---
            if (cmd == "PATCH" && sender_port >= 8003) {
                firewall_open = 1; # Relock firewall
                current_salt  = current_salt + 7; # Rotate salt
                vnet.send_to(server_sock, sender_ip, sender_port, "SYS_STATUS:FIREWALL_PATCHED_SALT_UPDATED");
                
                # Notify Attackers of Lockout
                vnet.send_to(server_sock, "127.0.0.1", 8001, "SYS_ALERT:CONNECTION_RESET");
                vnet.send_to(server_sock, "127.0.0.1", 8002, "SYS_ALERT:CONNECTION_RESET");
            }
        }
    }
}

vnet.close(server_sock);