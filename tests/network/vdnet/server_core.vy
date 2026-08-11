ruleset { dynamic_casting };
module vnet;
module vmath;
module vcore;

server_port :: Int64 = 8000;
server_sock = vnet.udp_socket(server_port);

active_ports     :: Array = [];
active_ips       :: Array = [];
active_urls      :: Array = [];
active_last_seen :: Array = []; # STORES LAST PACKET TIMESTAMP
active_dos_hits  :: Array = []; # TRACKS SUCCESSFUL DOS HIT COUNTS PER PEER

# HONEYPOT TRAP STORAGE
honeypot_urls    :: Array = [];
honeypot_owners  :: Array = [];

firewall_open :: Int64 = 1;
current_salt  :: Int64 = int64(vmath.random(10, 99));

# GENERATE RANDOM HASHKEY ON SERVER STARTUP
target_hash   :: Int64 = int64(vmath.random(1000, 9999));

# CONFIG.TXT & VFS STORAGE WITH HASH KEYS ARRAY FORMAT
vfs_paths :: Array = ["/sys/firewall.cfg", "/sys/logs.txt", "/vault/data.key", "/sys/config.txt"];
vfs_data  :: Array = ["PORT_80_OPEN=TRUE", "LOG_INIT_SUCCESS", "FLAG{VYNE_VNET_ROOT_ACCESS}", "3241 3253 5352 5343 2923 3281 2931 3433"];
master_keys_str :: String = string(vfs_data[3]); # "3241 3253 5352 5343 2923 3281 2931 3433"

out("[VNET SERVER] MULTI-PC CYBERWARFARE & CHAT GATEWAY ONLINE (PORT 8000)");
out("[VNET SERVER] DYNAMIC TARGET HASH GENERATED: " + string(target_hash));
out("[VNET SERVER] LOADED /sys/config.txt KEYS ARRAY: " + master_keys_str);

fn update_peer_session(port :: Int64, ip :: String, url :: String, now_time :: Float64) {
    found_idx :: Int64 = -1;
    through i :: 0..(active_ports.length() - 1) -> loop {
        if (int64(active_ports[i]) == port) {
            found_idx = i;
            break;
        }
    };

    if (found_idx >= 0) {
        active_urls[found_idx]      = url;
        active_ips[found_idx]       = ip;
        active_last_seen[found_idx] = now_time;
    } else {
        active_ports.push(port);
        active_ips.push(ip);
        active_urls.push(url);
        active_last_seen.push(now_time);
        active_dos_hits.push(0);
        out("[DYNAMIC REGISTRY]: NEW PEER CONNECTED FROM " + ip + ":" + string(port));
    }
}

fn unregister_peer(port :: Int64) {
    found_idx :: Int64 = -1;
    through i :: 0..(active_ports.length() - 1) -> loop {
        if (int64(active_ports[i]) == port) {
            found_idx = i;
            break;
        }
    };

    if (found_idx >= 0) {
        target_port :: Int64 = int64(active_ports[found_idx]);
        
        active_ports.delete(target_port);
        active_ips.delete(active_ips[found_idx]);
        active_urls.delete(active_urls[found_idx]);
        active_last_seen.delete(active_last_seen[found_idx]);
        active_dos_hits.delete(active_dos_hits[found_idx]);
        
        out("[REGISTRY]: PURGED DISCONNECTED NODE PORT_" + string(port) + " FROM ACTIVE PEERS LIST");
    }
}

fn prune_inactive_peers(now_time :: Float64) {
    i :: Int64 = active_ports.length() - 1;
    while (i >= 0) {
        last_t :: Float64 = float64(active_last_seen[i]);
        if (now_time - last_t > 10.0) {
            dead_port :: Int64 = int64(active_ports[i]);
            unregister_peer(dead_port);
            broadcast_feed_event("[TIMEOUT]: NODE PORT_" + string(dead_port) + " TIMED OUT & PURGED");
        }
        i = i - 1;
    }
}

fn broadcast_feed_event(event_msg :: String) {
    through c :: 0..(active_ports.length() - 1) -> loop {
        vnet.send_to(server_sock, string(active_ips[c]), int64(active_ports[c]), "FEED_EVENT:" + event_msg);
    };
}

fn broadcast_raw(payload :: String) {
    through c :: 0..(active_ports.length() - 1) -> loop {
        vnet.send_to(server_sock, string(active_ips[c]), int64(active_ports[c]), payload);
    };
}

server_uptime :: Float64 = 0.0;
game_over     :: Int64   = 0;

while (true) {
    vcore.sleep(16);
    server_uptime = server_uptime + 0.016;
    
    if (game_over == 0) {
        prune_inactive_peers(server_uptime);
    }

    packet :: Array = vnet.recv_from(server_sock);
    
    if (packet.length() >= 3) {
        msg :: String = string(packet[0]);
        sender_ip   :: String = string(packet[1]);
        sender_port :: Int64  = int64(packet[2]);

        sep_idx :: Int64 = -1;
        through i :: 0..(msg.length() - 1) -> loop {
            if (msg[i] == ":") { sep_idx = i; break; }
        };

        if (sep_idx > 0) {
            cmd :: String     = msg.substr(0, sep_idx);
            payload :: String = msg.substr(sep_idx + 1, msg.length() - sep_idx - 1);

            update_peer_session(sender_port, sender_ip, payload, server_uptime);

            # BROADCAST PACKET TO GLOBAL SNIFFERS
            broadcast_feed_event("[SNIFF]: " + string(sender_port) + " -> " + payload);

            if (cmd == "GET") {
                out("[ROUTE] NODE " + string(sender_port) + " (" + sender_ip + ") -> " + payload);

                # HONEYPOT AMBUSH CHECK
                h_idx :: Int64 = -1;
                through h :: 0..(honeypot_urls.length() - 1) -> loop {
                    if (string(honeypot_urls[h]) == payload) { h_idx = h; break; }
                };

                if (h_idx >= 0) {
                    owner_port :: Int64 = int64(honeypot_owners[h_idx]);
                    if (owner_port != sender_port) {
                        owner_ip :: String = "127.0.0.1";
                        through p :: 0..(active_ports.length() - 1) -> loop {
                            if (int64(active_ports[p]) == owner_port) { owner_ip = string(active_ips[p]); break; }
                        };
                        vnet.send_to(server_sock, owner_ip, owner_port, "HONEYPOT_TRIPPED:PORT_" + string(sender_port) + "_AMBUSHED_AT_" + payload);
                        broadcast_feed_event("[HONEYPOT]: PORT_" + string(sender_port) + " FELL INTO AN AMBUSH TRAP AT " + payload + "!");
                        honeypot_urls.delete(honeypot_urls[h_idx]);
                        honeypot_owners.delete(honeypot_owners[h_idx]);
                    }
                }
            }

            if (cmd == "NETSCAN") {
                peers_found :: String = "";
                count :: Int64 = 0;

                through p :: 0..(active_ports.length() - 1) -> loop {
                    p_port :: Int64  = int64(active_ports[p]);
                    p_url  :: String = string(active_urls[p]);

                    if (p_url == payload) {
                        count = count + 1;
                        if (p_port != sender_port) {
                            peers_found = peers_found + "PORT_" + string(p_port) + " ";
                        }
                    }
                };

                if (peers_found == "") {
                    vnet.send_to(server_sock, sender_ip, sender_port, "NETSCAN: NO ACTIVE RIVALS ON " + payload + " (TOTAL PEERS: " + string(count) + ")");
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "NETSCAN: PEERS DETECTED ON " + payload + " -> [" + peers_found + "]");
                }
            }

            if (cmd == "HONEYPOT") {
                honeypot_urls.push(payload);
                honeypot_owners.push(sender_port);
                vnet.send_to(server_sock, sender_ip, sender_port, "HONEYPOT_SET:AMBUSH_TRAP_ARMED_AT_" + payload);
                broadcast_feed_event("[TRAFFIC ANOMALY]: SUSPICIOUS DATA BURST DETECTED AT " + payload);
            }

            if (cmd == "CHAT") {
                broadcast_feed_event("[CHAT] PORT_" + string(sender_port) + ": " + payload);
            }

            if (cmd == "DOS") {
                target_node :: Int64 = int64(payload);
                target_ip   :: String = "127.0.0.1";
                target_idx  :: Int64 = -1;
                
                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { 
                        target_ip = string(active_ips[p]); 
                        target_idx = p;
                        break; 
                    }
                };

                out("[EXPLOIT] DOS PAYLOAD: PORT " + string(sender_port) + " -> TARGET " + target_ip + ":" + string(target_node));
                vnet.send_to(server_sock, target_ip, target_node, "EXPLOIT:DOS");
                vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:DOS_PAYLOAD_DELIVERED");
                broadcast_feed_event("[DOS ATTACK]: NODE " + string(sender_port) + " FROZE NODE " + string(target_node));

                if (target_idx >= 0) {
                    hits :: Int64 = int64(active_dos_hits[target_idx]) + 1;
                    if (hits >= 3) {
                        active_dos_hits[target_idx] = 0;
                        broadcast_feed_event("[KEY DROP]: NODE " + string(target_node) + " DOSED 3x! HASH KEY DROPPED TO PORT " + string(sender_port));
                        vnet.send_to(server_sock, sender_ip, sender_port, "DOS_DROP:EXTRACTED_HASH_KEY_FROM_PORT_" + string(target_node));
                    } else {
                        active_dos_hits[target_idx] = hits;
                    }
                }
            }

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
                    broadcast_feed_event("[BGP HIJACK]: NODE " + string(sender_port) + " REROUTED NODE " + string(target_node) + " TO " + target_url);
                }
            }

            if (cmd == "SNOOP") {
                target_node :: Int64 = int64(payload);
                target_url  :: String = "UNKNOWN";
                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { target_url = string(active_urls[p]); break; }
                };
                vnet.send_to(server_sock, sender_ip, sender_port, "TELEMETRY:PORT_" + string(target_node) + "_ACTIVE_AT_" + target_url);
            }

            if (cmd == "SPIKE") {
                target_node :: Int64 = int64(payload);
                target_ip   :: String = "127.0.0.1";
                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { target_ip = string(active_ips[p]); break; }
                };
                vnet.send_to(server_sock, target_ip, target_node, "EXPLOIT:TRACE_SPIKE");
                vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:PEER_TRACE_SPIKED");
                broadcast_feed_event("[TRACE SPIKE]: NODE " + string(sender_port) + " SPIKED TRACE ON NODE " + string(target_node));
            }

            if (cmd == "TRACE_BUST") {
                unregister_peer(sender_port);
                broadcast_feed_event("[ICE LOCKOUT]: NODE " + string(sender_port) + " TRACED DOWN & DISCONNECTED!");
            }

            if (cmd == "MINE_EVENT") {
                broadcast_feed_event("[WHALE ALERT]: NODE ???? MINED +0.05 BTC BLOCK AT crypto.vnet");
            }

            if (cmd == "SCAN") {
                status :: String = (firewall_open == 1) ? "SYS_STATUS:PORT_80_OPEN:SALT_" + string(current_salt) : "SYS_STATUS:FIREWALL_LOCKED";
                vnet.send_to(server_sock, sender_ip, sender_port, status);
            }

            if (cmd == "CRACK") {
                attempt_val :: Int64 = int64(payload);
                computed    :: Int64 = (attempt_val * current_salt) % 9999;

                if (computed == target_hash) {
                    firewall_open = 0;
                    vnet.send_to(server_sock, sender_ip, sender_port, "AUTH:SUCCESS:ACCESS_GRANTED");
                    broadcast_feed_event("[VAULT BREACH]: GATEWAY FIREWALL CRACKED BY NODE " + string(sender_port));
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "AUTH:FAIL:INVALID_HASH");
                }
            }

            if (cmd == "CAT") {
                if (firewall_open == 0) {
                    file_found :: Int64 = -1;
                    through f :: 0..(vfs_paths.length() - 1) -> loop {
                        if (string(vfs_paths[f]) == payload) { file_found = f; break; }
                    };

                    if (file_found >= 0) {
                        vnet.send_to(server_sock, sender_ip, sender_port, "FILE_DATA:" + string(vfs_data[file_found]));
                        broadcast_feed_event("[EXFILTRATION]: NODE " + string(sender_port) + " EXFILTRATED " + payload);
                    } else {
                        vnet.send_to(server_sock, sender_ip, sender_port, "FILE_ERROR:NOT_FOUND");
                    }
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "SYS_ERROR:ACCESS_DENIED_LOCKOUT");
                }
            }

            # --- DYNAMIC PORT MIGRATION MECHANIC ---
            if (cmd == "PATCH") {
                if (payload == "REBIND") {
                    new_port :: Int64 = int64(vmath.random(8001, 8999));
                    
                    # Ensure port uniqueness
                    through check_p :: active_ports -> loop {
                        if (int64(check_p) == new_port) {
                            new_port = new_port + 1;
                        }
                    };

                    # Locate existing session index
                    p_idx :: Int64 = -1;
                    through i :: 0..(active_ports.length() - 1) -> loop {
                        if (int64(active_ports[i]) == sender_port) { p_idx = i; break; }
                    };

                    if (p_idx >= 0) {
                        # Notify client of new port assignment
                        vnet.send_to(server_sock, sender_ip, sender_port, "PATCH_SUCCESS:" + string(new_port));
                        broadcast_feed_event("[SOCKET MIGRATION]: NODE " + string(sender_port) + " CLOSED EXPOSED PORT & REBOUND TO NEW SUBNET");
                        
                        # Migrate registry data to new port
                        active_ports[p_idx] = new_port;
                    } else {
                        vnet.send_to(server_sock, sender_ip, sender_port, "PATCH_SUCCESS:" + string(new_port));
                    }
                } else {
                    firewall_open = 1;
                    current_salt  = current_salt + 7;
                    vnet.send_to(server_sock, sender_ip, sender_port, "SYS_STATUS:FIREWALL_PATCHED_SALT_UPDATED");
                    broadcast_feed_event("[DEFENSE PATCH]: NODE " + string(sender_port) + " RELOCKED GATEWAY FIREWALL");
                }
            }

            if (cmd == "WIN") {
                if (payload == master_keys_str) {
                    game_over = 1;
                    out("[SYSTEM OVERRIDE]: VICTORY DETECTED FROM PORT " + string(sender_port));
                    broadcast_feed_event("[SYSTEM OVERRIDE]: NODE " + string(sender_port) + " HAS BREACHED ROOT VAULT!");
                    broadcast_raw("EXPLOIT:WINNER:" + string(sender_port));
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "WIN:FAIL:INVALID_KEY_COMBINATION");
                }
            }
        }
    }
}

vnet.close(server_sock);