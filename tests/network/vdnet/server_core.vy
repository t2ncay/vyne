ruleset { dynamic_casting };
module vnet;
module vmath;   
module vcore;

server_port :: Int64 = 8000;
server_sock = vnet.udp_socket(server_port);

active_ports     :: Array = [];
active_ips       :: Array = [];
active_urls      :: Array = [];
active_last_seen :: Array = [];
active_dos_hits  :: Array = []; 
active_dirs      :: Array = []; # STORES EACH PEER'S 15 ASSIGNED SITES

# DECOY TRAP STORAGE
decoy_targets    :: Array = [];
decoy_owners     :: Array = [];
decoy_urls       :: Array = [];
decoy_timer      :: Float64 = 0.0;

firewall_open :: Int64 = 1;
current_salt  :: Int64 = int64(vmath.random(10, 99));

# GENERATE RANDOM HASHKEY ON SERVER STARTUP
target_hash   :: Int64 = int64(vmath.random(1000, 9999));

# ====================================================================
# MASTER ROSTER OF 50 DARKNET WEBSITES
# ====================================================================
all_50_sites :: Array = [
    # THE ORIGINAL 19 LORE-HEAVY SITES
    "market.vnet", "dollhouse.vnet", "vault.vnet", "forum.vnet", 
    "redroom.vnet", "crypto.vnet", "morgue.vnet", "silence.vnet", 
    "blackout.vnet", "snuff.vnet", "asylum.vnet", "bounty.vnet", 
    "archival.vnet", "ghost.vnet", "cult.vnet", "void.vnet",
    "skinwalker.vnet", "watchtower.vnet", "terminal.vnet",
    # 31 EXPANDED DEEP WEB NODES (For a total of 50)
    "silkroad.vnet", "zeroauction.vnet", "leaks.vnet", "shadowpay.vnet", 
    "cctv_core.vnet", "subcell.vnet", "feed99.vnet", "eye.vnet", 
    "orbital.vnet", "pastebin.vnet", "whisper.vnet", "deepwiki.vnet", 
    "dump.vnet", "index.vnet", "schizo.vnet", "project9.vnet", 
    "necro.vnet", "echolab.vnet", "abyss.vnet", "zeroday.vnet", 
    "deadchannel.vnet", "phantom.vnet", "glitch.vnet", "stasis.vnet", 
    "signal0.vnet", "entropy.vnet", "hive.vnet", "nexus.vnet", 
    "weaponry.vnet", "passports.vnet", "blackbank.vnet"
];

# MUTUAL SITES GUARANTEED TO BE IN EVERY PLAYER'S DIRECTORY
mutual_sites :: Array = [
    "market.vnet", "vault.vnet", "terminal.vnet", 
    "forum.vnet", "crypto.vnet", "bounty.vnet", "redroom.vnet", "hellroom.vnet"
];

# ====================================================================
# GLOBAL PROCEDURAL KEY GENERATION (8 KEYS SCATTERED ACROSS 50 SITES)
# ====================================================================
k1 :: Int64 = int64(vmath.random(1000, 9999));
k2 :: Int64 = int64(vmath.random(1000, 9999));
k3 :: Int64 = int64(vmath.random(1000, 9999));
k4 :: Int64 = int64(vmath.random(1000, 9999));
k5 :: Int64 = int64(vmath.random(1000, 9999));
k6 :: Int64 = int64(vmath.random(1000, 9999));
k7 :: Int64 = int64(vmath.random(1000, 9999));
k8 :: Int64 = int64(vmath.random(1000, 9999));

master_keys_str :: String = string(k1) + " " + string(k2) + " " + string(k3) + " " + string(k4) + " " + string(k5) + " " + string(k6) + " " + string(k7) + " " + string(k8);

global_key_locations :: Array = [];
used_key_indices     :: Array = [];

# Randomly select 8 unique sites out of the 50 to hide the keys globally
through k_idx :: 0..7 -> loop {
    rand_idx :: Int64 = int64(vmath.random(0, all_50_sites.length() - 1));
    is_used  :: Int64 = 0;
    
    through u :: used_key_indices -> loop {
        if (int64(u) == rand_idx) { is_used = 1; break; }
    };
    
    while (is_used == 1) {
        rand_idx = (rand_idx + 1) % all_50_sites.length();
        is_used = 0;
        through u2 :: used_key_indices -> loop {
            if (int64(u2) == rand_idx) { is_used = 1; break; }
        };
    }
    
    used_key_indices.push(rand_idx);
    global_key_locations.push(string(all_50_sites[rand_idx]));
};

# CONFIG.TXT & VFS STORAGE WITH PROCEDURAL MASTER KEYS
vfs_paths :: Array = ["/sys/firewall.cfg", "/sys/logs.txt", "/vault/data.key", "/sys/config.txt"];
vfs_data  :: Array = ["PORT_80_OPEN=TRUE", "LOG_INIT_SUCCESS", "FLAG{VYNE_VNET_ROOT_ACCESS}", master_keys_str];

out("[VNET SERVER] MULTI-PC CYBERWARFARE & CHAT GATEWAY ONLINE (PORT 8000)");
out("[VNET SERVER] DYNAMIC TARGET HASH GENERATED: " + string(target_hash));
out("[VNET SERVER] GENERATED MASTER KEYS: " + master_keys_str);

# ====================================================================
# DEBUG KEYCODES & ROOM LOCATIONS
# ====================================================================
out("==================================================");
out("[DEBUG KEYCODES & ROOM LOCATIONS]");
out("  KEY 1 (" + string(k1) + ") -> " + string(global_key_locations[0]));
out("  KEY 2 (" + string(k2) + ") -> " + string(global_key_locations[1]));
out("  KEY 3 (" + string(k3) + ") -> " + string(global_key_locations[2]));
out("  KEY 4 (" + string(k4) + ") -> " + string(global_key_locations[3]));
out("  KEY 5 (" + string(k5) + ") -> " + string(global_key_locations[4]));
out("  KEY 6 (" + string(k6) + ") -> " + string(global_key_locations[5]));
out("  KEY 7 (" + string(k7) + ") -> " + string(global_key_locations[6]));
out("  KEY 8 (" + string(k8) + ") -> " + string(global_key_locations[7]));
out("==================================================");

# PROXY ROUTE STORAGE
proxy_chains_source :: Array = [];
proxy_chains_proxy  :: Array = [];
proxy_chains_target :: Array = [];

# OVERLOAD STATE STORAGE
market_overloaded_timer :: Float64 = 0.0;
overload_cooldowns      :: Array   = [];
overloaded_urls   :: Array = [];
overloaded_timers :: Array = [];

fn update_peer_session(port :: Int64, ip :: String, url :: String, now_time :: Float64) -> Int64 {
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
        return 0; # Existing peer
    } else {
        assigned_16 :: Array = [];
        
        through m :: 0..7 -> loop {
            assigned_16.push(string(mutual_sites[m]));
        };
        
        while (assigned_16.length() < 16) {
            rand_s :: Int64 = int64(vmath.random(0, all_50_sites.length() - 1));
            site_n :: String = string(all_50_sites[rand_s]);
            
            already :: Int64 = 0;
            through u :: 0..(assigned_16.length() - 1) -> loop {
                if (string(assigned_16[u]) == site_n) { already = 1; break; }
            };
            if (already == 0) {
                assigned_16.push(site_n);
            }
        }
        
        dir_payload :: String = "";
        through s_idx :: 0..15 -> loop {
            dir_payload = dir_payload + string(assigned_16[s_idx]) + ((s_idx < 15) ? ":" : "");
        };

        active_ports.push(port);
        active_ips.push(ip);
        active_urls.push(url);
        active_last_seen.push(now_time);
        active_dos_hits.push(0);
        active_dirs.push(dir_payload);
        out("[DYNAMIC REGISTRY]: NEW PEER CONNECTED FROM " + ip + ":" + string(port));

        return 1;
    }
}

fn clean_str(raw :: String) -> String {
    out_str = raw;
    while (out_str.length() > 0) {
        c = out_str.substr(0, 1);
        if (c == " " || c == "\"" || c == "'" || c == "\t" || c == "\r" || c == "\n") {
            out_str = out_str.substr(1, out_str.length() - 1);
        } else { break; }
    }
    while (out_str.length() > 0) {
        idx = out_str.length() - 1;
        c   = out_str.substr(idx, 1);
        if (c == " " || c == "\"" || c == "'" || c == "\t" || c == "\r" || c == "\n") {
            out_str = out_str.substr(0, idx);
        } else { break; }
    }
    return out_str;
}

fn mask_port(port_val :: Int64) -> String {
    p_str :: String = string(port_val);
    if (p_str.length() >= 4) {
        return p_str.substr(0, p_str.length() - 1) + "X";
    }
    return p_str;
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
        active_ports.delete_at(found_idx);
        active_ips.delete_at(found_idx);
        active_urls.delete_at(found_idx);
        active_last_seen.delete_at(found_idx);
        active_dos_hits.delete_at(found_idx);
        active_dirs.delete_at(found_idx);
        
        out("[REGISTRY]: PURGED DISCONNECTED NODE PORT_" + string(port) + " FROM ACTIVE PEERS LIST");
    }
}

fn prune_inactive_peers(now_time :: Float64) {
    i :: Int64 = active_ports.length() - 1;
    while (i >= 0) {
        last_t :: Float64 = float64(active_last_seen[i]);
        if (now_time - last_t > 10.0) { # 10s grace window for 3s heartbeats
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

fn send_key_sync_payload(ip :: String, port :: Int64) {
    found_idx :: Int64 = -1;
    through i :: 0..(active_ports.length() - 1) -> loop {
        if (int64(active_ports[i]) == port) {
            found_idx = i;
            break;
        }
    };

    peer_dir_str :: String = "";
    if (found_idx >= 0) {
        peer_dir_str = string(active_dirs[found_idx]);
    }

    sync_str :: String = "KEY_SYNC:" + 
        string(k1) + ":" + string(k2) + ":" + string(k3) + ":" + string(k4) + ":" +
        string(k5) + ":" + string(k6) + ":" + string(k7) + ":" + string(k8) + ":" +
        string(global_key_locations[0]) + ":" + string(global_key_locations[1]) + ":" +
        string(global_key_locations[2]) + ":" + string(global_key_locations[3]) + ":" +
        string(global_key_locations[4]) + ":" + string(global_key_locations[5]) + ":" +
        string(global_key_locations[6]) + ":" + string(global_key_locations[7]) + ":" + 
        peer_dir_str;
    
    vnet.send_to(server_sock, ip, port, sync_str);
}

fn check_and_trip_decoy(target :: String, attacker_port :: Int64) {
    through p_idx :: 0..(proxy_chains_source.length() - 1) -> loop {
        if (int64(proxy_chains_source[p_idx]) == attacker_port) {
            if (string(proxy_chains_target[p_idx]) == target) {
                proxy_node_name :: String = string(proxy_chains_proxy[p_idx]);
                broadcast_feed_event("[PROXY SHIELD]: PROXY NODE " + proxy_node_name + " ABSORBED AMBUSH FOR PORT_" + string(attacker_port));
                return null;
            }
        }
    };

    h_idx :: Int64 = -1;
    through h :: 0..(decoy_targets.length() - 1) -> loop {
        if (string(decoy_urls[h]) == target) { 
            h_idx = h; 
            break; 
        }
    };

    if (h_idx >= 0) {
        owner_port :: Int64 = int64(decoy_owners[h_idx]);
        if (owner_port != attacker_port) {
            owner_ip :: String = "127.0.0.1";
            through p :: 0..(active_ports.length() - 1) -> loop {
                if (int64(active_ports[p]) == owner_port) { owner_ip = string(active_ips[p]); break; }
            };
            vnet.send_to(server_sock, owner_ip, owner_port, "DECOY_TRIPPED:PORT_" + string(attacker_port) + "_AMBUSHED_AT_" + target);
            broadcast_feed_event("[DECOY TRAP]: PORT_XXXX TRIPPED A DECOY AT " + target + "!");
            
            decoy_targets.delete(decoy_targets[h_idx]);
            decoy_owners.delete(decoy_owners[h_idx]);
            decoy_urls.delete(decoy_urls[h_idx]);
        }
    }
}

server_uptime :: Float64 = 0.0;
game_over     :: Int64   = 0;

while (true) {
    vcore.sleep(16);
    server_uptime = server_uptime + 0.016;
    
    if (game_over == 0) {
        prune_inactive_peers(server_uptime);
        
        t_idx :: Int64 = overloaded_timers.length() - 1;
        while (t_idx >= 0) {
            curr_t :: Float64 = float64(overloaded_timers[t_idx]) - 0.016;
            if (curr_t <= 0.0) {
                url_rm :: String = string(overloaded_urls[t_idx]);
                timer_rm :: Float64 = float64(overloaded_timers[t_idx]);
                overloaded_urls.delete(url_rm);
                overloaded_timers.delete(timer_rm);
            } else {
                overloaded_timers[t_idx] = curr_t;
            }
            t_idx = t_idx - 1;
        }
        
        decoy_timer = decoy_timer + 0.016;
        if (decoy_timer >= 30.0) {
            decoy_timer = 0.0;
            if (decoy_targets.length() > 0) {
                decoy_targets.clear();
                decoy_owners.clear();
                decoy_urls.clear();
                broadcast_feed_event("[SYS_CLEANUP]: ALL ACTIVE DECOYS WIPED (30s CYCLE)");
            }
        }
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

            if (cmd == "PING") {
                # Silent heartbeat keepalive — updates last_seen without feed spam
            } else {
                broadcast_feed_event("[SNIFF]: " + mask_port(sender_port) + " -> " + payload);
            }

            if (cmd == "GET") {
                site_is_down :: Int64 = 0;
                through o_i :: 0..(overloaded_urls.length() - 1) -> loop {
                    if (string(overloaded_urls[o_i]) == payload) {
                        site_is_down = 1;
                        break;
                    }
                };

                if (site_is_down == 1) {
                    vnet.send_to(server_sock, sender_ip, sender_port, "EXPLOIT:SITE_OVERLOADED:" + payload);
                } else {
                    out("[ROUTE] NODE " + string(sender_port) + " (" + sender_ip + ") -> " + payload);
                    
                    is_new_peer :: Int64 = update_peer_session(sender_port, sender_ip, payload, server_uptime);
                    if (is_new_peer == 1) {
                        send_key_sync_payload(sender_ip, sender_port);
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

                through d :: 0..(decoy_urls.length() - 1) -> loop {
                    if (string(decoy_urls[d]) == payload) {
                        owner_p :: Int64 = int64(decoy_owners[d]);
                        if (owner_p != sender_port) {
                            peers_found = peers_found + "PORT_" + string(decoy_targets[d]) + " ";
                            count = count + 1;
                        }
                    }
                };

                bot_decoy_1 :: Int64 = 8000 + (payload.length() * 17) % 900;
                bot_decoy_2 :: Int64 = 8000 + (payload.length() * 31 + 43) % 900;
                
                if (bot_decoy_1 != sender_port) { peers_found = peers_found + "PORT_" + string(bot_decoy_1) + " "; count = count + 1; }
                if (bot_decoy_2 != sender_port) { peers_found = peers_found + "PORT_" + string(bot_decoy_2) + " "; count = count + 1; }

                vnet.send_to(server_sock, sender_ip, sender_port, "NETSCAN: PEERS DETECTED ON " + payload + " -> [" + peers_found + "]");
            }

            if (cmd == "DECOY") {
                sep_d :: Int64 = -1;
                through i :: 0..(payload.length() - 1) -> loop {
                    if (payload[i] == ":") { sep_d = i; break; }
                };
                
                if (sep_d > 0) {
                    target_url :: String = payload.substr(0, sep_d);
                    dummy_port_str :: String = payload.substr(sep_d + 1, payload.length() - sep_d - 1);
                    
                    dummy_port_val :: Int64 = int64(dummy_port_str);
                    
                    decoy_targets.push(dummy_port_val);
                    decoy_urls.push(target_url);
                    decoy_owners.push(sender_port);
                    
                    vnet.send_to(server_sock, sender_ip, sender_port, "DECOY_SET:DECOY_DEPLOYED_ON_" + target_url + "_AS_PORT_" + dummy_port_str);
                    broadcast_feed_event("[NETWORK]: DECOY PORT " + dummy_port_str + " DEPLOYED AT " + target_url);
                }
            }
            if (cmd == "CHAT") {
                sep_c :: Int64 = -1;
                through i :: 0..(payload.length() - 1) -> loop {
                    if (payload[i] == ":") { sep_c = i; break; }
                };
                if (sep_c > 0) {
                    handle_part :: String = payload.substr(0, sep_c);
                    msg_part    :: String = payload.substr(sep_c + 1, payload.length() - sep_c - 1);
                    broadcast_feed_event("[CHAT] <" + handle_part + ">: " + msg_part);
                } else {
                    broadcast_feed_event("[CHAT] PORT_" + string(sender_port) + ": " + payload);
                }
            }
            if (cmd == "DOS") {
                target_node_str :: String = payload;
                target_node :: Int64 = int64(target_node_str);
                
                decoy_idx :: Int64 = -1;
                through d :: 0..(decoy_targets.length() - 1) -> loop {
                    if (int64(decoy_targets[d]) == target_node) {
                        decoy_idx = d;
                        break;
                    }
                };

                target_ip   :: String = "127.0.0.1";
                target_idx  :: Int64 = -1;
                
                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { 
                        target_ip = string(active_ips[p]); 
                        target_idx = p;
                        break; 
                    }
                };

                if (decoy_idx >= 0) {
                    owner_port :: Int64 = int64(decoy_owners[decoy_idx]);
                    if (owner_port != sender_port) {
                        owner_ip :: String = "127.0.0.1";
                        through p :: 0..(active_ports.length() - 1) -> loop {
                            if (int64(active_ports[p]) == owner_port) { owner_ip = string(active_ips[p]); break; }
                        };
                        
                        vnet.send_to(server_sock, owner_ip, owner_port, "DECOY_TRIPPED:PORT_" + string(sender_port) + "_ATTACKED_DECOY_PORT_" + string(target_node));
                        broadcast_feed_event("[DECOY TRAP]: PORT_" + string(sender_port) + " TRIED TO DOS DECOY PORT " + string(target_node) + "! TRAP SPRUNG.");
                        
                        vnet.send_to(server_sock, sender_ip, sender_port, "EXPLOIT:DOS");
                        vnet.send_to(server_sock, sender_ip, sender_port, "EXPLOIT_REFUND");
                        
                        decoy_targets.delete(decoy_targets[decoy_idx]);
                        decoy_owners.delete(decoy_owners[decoy_idx]);
                        decoy_urls.delete(decoy_urls[decoy_idx]);
                    }
                } else if (target_idx < 0) {
                    vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:DOS_PAYLOAD_DELIVERED");
                    vnet.send_to(server_sock, sender_ip, sender_port, "EXPLOIT_REFUND");
                    broadcast_feed_event("[DECOY DUST]: NODE PORT_" + string(sender_port) + " HIT EMPTY SOCKET PORT_" + string(target_node) + "! -0.05 VCOIN & COOLDOWN BYPASSED.");
                } else {
                    out("[EXPLOIT] DOS PAYLOAD: PORT " + string(sender_port) + " -> TARGET " + target_ip + ":" + string(target_node));
                    vnet.send_to(server_sock, target_ip, target_node, "EXPLOIT:DOS");
                    vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:DOS_PAYLOAD_DELIVERED");
                    broadcast_feed_event("[DOS ATTACK]: NODE " + string(sender_port) + " FROZE NODE " + string(target_node));

                    hits :: Int64 = int64(active_dos_hits[target_idx]) + 1;
                    active_dos_hits[target_idx] = hits;
                    
                    if (hits == 2) {
                        target_history_str :: String = string(active_dirs[target_idx]);
                        
                        target_assigned :: Array = [];
                        curr_seg :: String = "";
                        through i :: 0..(target_history_str.length() - 1) -> loop {
                            c = target_history_str.substr(i, 1);
                            if (c == ":") {
                                if (curr_seg != "") { target_assigned.push(curr_seg); }
                                curr_seg = "";
                            } else {
                                curr_seg = curr_seg + c;
                            }
                        };
                        if (curr_seg != "") { target_assigned.push(curr_seg); }

                        non_mutuals :: Array = [];
                        through s_idx :: 0..(target_assigned.length() - 1) -> loop {
                            site :: String = string(target_assigned[s_idx]);
                            is_mutual :: Int64 = 0;
                            through m_idx :: 0..(mutual_sites.length() - 1) -> loop {
                                if (site == string(mutual_sites[m_idx])) {
                                    is_mutual = 1;
                                    break;
                                }
                            };
                            if (is_mutual == 0) {
                                non_mutuals.push(site);
                            }
                        };

                        drop_payload :: String = "";
                        leak_count :: Int64 = 0;
                        through n_idx :: 0..(non_mutuals.length() - 1) -> loop {
                            if (leak_count < 5) {
                                drop_payload = drop_payload + string(non_mutuals[n_idx]) + ":";
                                leak_count = leak_count + 1;
                            }
                        };

                        broadcast_feed_event("[DATA LEAK]: NODE " + string(target_node) + " DOSED 2x! 5 HIDDEN NODES EXFILTRATED.");
                        vnet.send_to(server_sock, sender_ip, sender_port, "DOS_DROP_DIR:" + drop_payload);
                    } else if (hits >= 3) {
                        active_dos_hits[target_idx] = 0;
                        broadcast_feed_event("[KEY DROP]: NODE " + string(target_node) + " DOSED 3x! HASH KEY DROPPED TO PORT " + string(sender_port));
                        vnet.send_to(server_sock, sender_ip, sender_port, "DOS_DROP:EXTRACTED_HASH_KEY_FROM_PORT_" + string(target_node));
                    }
                }
            }

            if (cmd == "PROXY") {
                sep_p :: Int64 = -1;
                through i :: 0..(payload.length() - 1) -> loop {
                    if (payload[i] == ":") { sep_p = i; break; }
                };
                
                if (sep_p > 0) {
                    proxy_node  :: String = payload.substr(0, sep_p);
                    target_url  :: String = payload.substr(sep_p + 1, payload.length() - sep_p - 1);
                    
                    proxy_chains_source.push(sender_port);
                    proxy_chains_proxy.push(proxy_node);
                    proxy_chains_target.push(target_url);
                    
                    vnet.send_to(server_sock, sender_ip, sender_port, "PROXY_SET:ROUTING_VIA_" + proxy_node + "_TO_" + target_url);
                    broadcast_feed_event("[SPY ROUTE]: PORT_" + string(sender_port) + " ENCRYPTED CHAIN THROUGH " + proxy_node + " TO " + target_url);
                }
            }

            if (cmd == "REDIRECT") {
                sep :: Int64 = -1;
                through i :: 0..(payload.length() - 1) -> loop {
                    if (payload[i] == ":") { sep = i; break; }
                };
                if (sep > 0) {
                    target_node_str :: String = payload.substr(0, sep);
                    check_and_trip_decoy(target_node_str, sender_port);
                    
                    target_node :: Int64  = int64(target_node_str);
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
                check_and_trip_decoy(payload, sender_port);
                
                target_node :: Int64 = int64(payload);
                target_url  :: String = "UNKNOWN";
                found_peer  :: Int64  = 0;

                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { 
                        target_url = string(active_urls[p]); 
                        found_peer = 1;
                        break; 
                    }
                };

                if (found_peer == 1) {
                    vnet.send_to(server_sock, sender_ip, sender_port, "TELEMETRY:PORT_" + string(target_node) + "_ACTIVE_AT_" + target_url);
                } else {
                    rand_bot_site :: String = string(all_50_sites[(target_node * 13) % 50]);
                    vnet.send_to(server_sock, sender_ip, sender_port, "TELEMETRY:PORT_" + string(target_node) + "_ACTIVE_AT_" + rand_bot_site + " [BOT_DECOY]");
                    broadcast_feed_event("[SNOOP]: NODE PORT_" + string(sender_port) + " INTERROGATED A PHANTOM DECOY PORT!");
                }
            }

            if (cmd == "SPIKE") {
                check_and_trip_decoy(payload, sender_port);
                
                target_node :: Int64 = int64(payload);
                target_ip   :: String = "127.0.0.1";
                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (int64(active_ports[p]) == target_node) { target_ip = string(active_ips[p]); break; }
                };
                vnet.send_to(server_sock, target_ip, target_node, "EXPLOIT:TRACE_SPIKE");
                vnet.send_to(server_sock, sender_ip, sender_port, "ATTACK_SUCCESS:PEER_TRACE_SPIKED");
                broadcast_feed_event("[TRACE SPIKE]: NODE " + string(sender_port) + " SPIKED TRACE ON NODE " + string(target_node));
            }

            if (cmd == "SNIFFER_STATUS") {
                state_str :: String = payload;
                if (state_str == "1") {
                    broadcast_feed_event("[WARNING]: PEER PORT_" + string(sender_port) + " ENTERED PROMISCUOUS SNIFFING MODE!");
                } else {
                    broadcast_feed_event("[NETWORK]: PEER PORT_" + string(sender_port) + " DISABLED PACKET SNIFFER.");
                }
            }

            if (cmd == "TRACE_BUST") {
                unregister_peer(sender_port);
                broadcast_feed_event("[ICE LOCKOUT]: NODE " + string(sender_port) + " TRACED DOWN & DISCONNECTED!");
            }

            if (cmd == "MINE_EVENT") {
                broadcast_feed_event("[WHALE ALERT]: NODE ???? MINED +0.05 VCOIN BLOCK AT crypto.vnet");
            }

            if (cmd == "SCAN") {
                rand_idx  :: Int64  = int64(vmath.random(0, all_50_sites.length() - 1));
                secret_url :: String = string(all_50_sites[rand_idx]);

                vnet.send_to(server_sock, sender_ip, sender_port, "TELEMETRY:DISCOVERED_HIDDEN_NODE -> vnet://" + secret_url);
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

            if (cmd == "OVERLOAD") {
                target_site :: String = clean_str(payload);
                if (target_site.length() >= 7 && target_site.substr(0, 7) == "vnet://") {
                    target_site = clean_str(target_site.substr(7, target_site.length() - 7));
                }
                
                is_already_down :: Int64 = 0;
                through o_i :: 0..(overloaded_urls.length() - 1) -> loop {
                    if (string(overloaded_urls[o_i]) == target_site) {
                        is_already_down = 1;
                        break;
                    }
                };

                if (is_already_down == 1) {
                    vnet.send_to(server_sock, sender_ip, sender_port, "OVERLOAD_FAIL:ALREADY_OVERLOADED");
                } else {
                    overloaded_urls.push(target_site);
                    overloaded_timers.push(30.0); # 30 second lockout
                    broadcast_feed_event("[GRID OVERLOAD]: NODE PORT_" + string(sender_port) + " FRIED " + target_site + "!");
                    broadcast_raw("EXPLOIT:SITE_OVERLOADED:" + target_site);

                    # =========================================================
                    # WIN GOAL 1: GRID BLACKOUT CASCADE (5 MUTUAL CORE NODES)
                    # =========================================================
                    m_mkt :: Int64 = 0;
                    m_vlt :: Int64 = 0;
                    m_trm :: Int64 = 0;
                    m_cry :: Int64 = 0;
                    m_hel :: Int64 = 0;

                    through chk_i :: 0..(overloaded_urls.length() - 1) -> loop {
                        chk_u :: String = string(overloaded_urls[chk_i]);
                        if (chk_u == "market.vnet")   { m_mkt = 1; }
                        if (chk_u == "vault.vnet")    { m_vlt = 1; }
                        if (chk_u == "terminal.vnet") { m_trm = 1; }
                        if (chk_u == "crypto.vnet")   { m_cry = 1; }
                        if (chk_u == "hellroom.vnet") { m_hel = 1; }
                    };

                    if (m_mkt == 1 && m_vlt == 1 && m_trm == 1 && m_cry == 1 && m_hel == 1) {
                        game_over = 1;
                        out("[GRID BLACKOUT]: VICTORY DETECTED FROM PORT " + string(sender_port));
                        broadcast_feed_event("[GRID BLACKOUT]: NODE PORT_" + string(sender_port) + " FRIED ALL 5 MUTUAL CORE NODES!");
                        broadcast_raw("EXPLOIT:WINNER:" + string(sender_port) + ":BLACKOUT");
                    }
                }
            }

            # =========================================================
            # WIN GOAL 2: ECONOMIC TAKEOVER (25.0 VCOIN)
            # =========================================================
            if (cmd == "TAKEOVER") {
                game_over = 1;
                out("[ECONOMIC TAKEOVER]: VICTORY DETECTED FROM PORT " + string(sender_port));
                broadcast_feed_event("[ECONOMIC TAKEOVER]: NODE PORT_" + string(sender_port) + " BOUGHT OUT VNET NETWORK!");
                broadcast_raw("EXPLOIT:WINNER:" + string(sender_port) + ":ECONOMIC");
            }

            if (cmd == "ICE_BOUGHT") {
                broadcast_feed_event("[BLACK MARKET]: NODE PORT_XXXX REINFORCED ICE FIREWALL SHIELD.");
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

            if (cmd == "PATCH") {
                if (payload == "REBIND") {
                    new_port :: Int64 = int64(vmath.random(8001, 8999));
                    
                    through check_p :: active_ports -> loop {
                        if (int64(check_p) == new_port) {
                            new_port = new_port + 1;
                        }
                    };

                    p_idx :: Int64 = -1;
                    through i :: 0..(active_ports.length() - 1) -> loop {
                        if (int64(active_ports[i]) == sender_port) { p_idx = i; break; }
                    };

                    if (p_idx >= 0) {
                        vnet.send_to(server_sock, sender_ip, sender_port, "PATCH_SUCCESS:" + string(new_port));
                        broadcast_feed_event("[SOCKET MIGRATION]: NODE " + string(sender_port) + " CLOSED EXPOSED PORT & REBOUND TO NEW SUBNET");
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

            # =========================================================
            # ORIGINAL WIN CONDITION: CRYPTOGRAPHIC ROOT BREACH
            # =========================================================
            if (cmd == "WIN") {
                if (payload == master_keys_str) {
                    game_over = 1;
                    out("[SYSTEM OVERRIDE]: VICTORY DETECTED FROM PORT " + string(sender_port));
                    broadcast_feed_event("[SYSTEM OVERRIDE]: NODE " + string(sender_port) + " HAS BREACHED ROOT VAULT!");
                    broadcast_raw("EXPLOIT:WINNER:" + string(sender_port) + ":KEYS");
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "WIN:FAIL:INVALID_KEY_COMBINATION");
                }
            }
        }
    }
}

vnet.close(server_sock);