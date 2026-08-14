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

# SERVER-SIDE RANDOM NETWORK INSTABILITY TIMER
server_outage_timer :: Float64 = 0.0;

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
    "cctv-core.vnet", "subcell.vnet", "feed99.vnet", "eye.vnet", 
    "orbital.vnet", "pastebin.vnet", "whisper.vnet", "deepwiki.vnet", 
    "dump.vnet", "index.vnet", "schizo.vnet", "project9.vnet", 
    "necro.vnet", "echolab.vnet", "abyss.vnet", "zeroday.vnet", 
    "deadchannel.vnet", "phantom.vnet", "glitch.vnet", "stasis.vnet", 
    "signal0.vnet", "entropy.vnet", "hive.vnet", "nexus.vnet", 
    "weaponry.vnet", "passports.vnet", "blackbank.vnet","stasi.vnet", "substation04.vnet", "deepocean.vnet", "darkdrop.vnet"
];

# MUTUAL SITES GUARANTEED TO BE IN EVERY PLAYER'S DIRECTORY
mutual_sites :: Array = [
    "market.vnet", "vault.vnet", "terminal.vnet", 
    "forum.vnet", "crypto.vnet", "bounty.vnet", "redroom.vnet", "hellroom.vnet"
];

session_salt :: Int64 = int64(vmath.random(100000, 999999));
hashed_50_sites :: Array = [];

through i :: 0..(all_50_sites.length() - 1) -> loop {
    raw_name :: String = string(all_50_sites[i]);
    
    base_name :: String = raw_name;
    if (raw_name.length() > 5 && raw_name.substr(raw_name.length() - 5, 5) == ".vnet") {
        base_name = raw_name.substr(0, raw_name.length() - 5);
    }
    
    hash_code :: String = vnet.hash_site(raw_name, session_salt);
    hashed_url :: String = base_name + "_" + hash_code + ".vnet";
    hashed_50_sites.push(hashed_url);
};

# ====================================================================
# GLOBAL PROCEDURAL BIT-ENCRYPTED KEYS & OFFSET SHIFTS
# ====================================================================

MULTIPLIER :: Int64 = 37;
MODULUS    :: Int64 = 100000;

k1 :: Int64 = int64(vmath.random(1000, 9999));
k2 :: Int64 = int64(vmath.random(1000, 9999));
k3 :: Int64 = int64(vmath.random(1000, 9999));
k4 :: Int64 = int64(vmath.random(1000, 9999));
k5 :: Int64 = int64(vmath.random(1000, 9999));
k6 :: Int64 = int64(vmath.random(1000, 9999));
k7 :: Int64 = int64(vmath.random(1000, 9999));
k8 :: Int64 = int64(vmath.random(1000, 9999));

shift_1 :: Int64 = int64(vmath.random(1, 7));
shift_2 :: Int64 = int64(vmath.random(1, 7));
shift_3 :: Int64 = int64(vmath.random(1, 7));
shift_4 :: Int64 = int64(vmath.random(1, 7));
shift_5 :: Int64 = int64(vmath.random(1, 7));
shift_6 :: Int64 = int64(vmath.random(1, 7));
shift_7 :: Int64 = int64(vmath.random(1, 7));
shift_8 :: Int64 = int64(vmath.random(1, 7));

scrambled_k1 :: Int64 = (k1 * MULTIPLIER + shift_1) % MODULUS;
scrambled_k2 :: Int64 = (k2 * MULTIPLIER + shift_2) % MODULUS;
scrambled_k3 :: Int64 = (k3 * MULTIPLIER + shift_3) % MODULUS;
scrambled_k4 :: Int64 = (k4 * MULTIPLIER + shift_4) % MODULUS;
scrambled_k5 :: Int64 = (k5 * MULTIPLIER + shift_5) % MODULUS;
scrambled_k6 :: Int64 = (k6 * MULTIPLIER + shift_6) % MODULUS;
scrambled_k7 :: Int64 = (k7 * MULTIPLIER + shift_7) % MODULUS;
scrambled_k8 :: Int64 = (k8 * MULTIPLIER + shift_8) % MODULUS;

master_keys_str :: String = string(k1) + " " + string(k2) + " " + string(k3) + " " + string(k4) + " " + string(k5) + " " + string(k6) + " " + string(k7) + " " + string(k8);

global_key_locations :: Array = [];
used_key_indices     :: Array = [];

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
    global_key_locations.push(string(hashed_50_sites[rand_idx]));
};

# CONFIG.TXT & VFS STORAGE WITH PROCEDURAL MASTER KEYS
vfs_paths :: Array = ["/sys/firewall.cfg", "/sys/logs.txt", "/vault/data.key", "/sys/config.txt"];
vfs_data  :: Array = ["PORT_80_OPEN=TRUE", "LOG_INIT_SUCCESS", "FLAG{VYNE_VNET_ROOT_ACCESS}", master_keys_str];

out("[VNET SERVER] MULTI-PC CYBERWARFARE & CHAT GATEWAY ONLINE (PORT 8000)");
out("[VNET SERVER] DYNAMIC TARGET HASH GENERATED: " + string(target_hash));
out("[VNET SERVER] GENERATED MASTER KEYS: " + master_keys_str);

# ====================================================================
# DEBUG KEYCODES, SHIFT VALUES & ROOM LOCATIONS
# ====================================================================
out("=========================================================================");
out("[DEBUG KEYCODES & MASTER ROOM LOCATIONS]");
through debug_k :: 0..7 -> loop {
    site_idx   :: Int64  = int64(used_key_indices[debug_k]);
    clean_site :: String = string(all_50_sites[site_idx]);
    hash_site  :: String = string(global_key_locations[debug_k]);
    
    k_val   :: Int64 = (debug_k == 0) ? k1 : ((debug_k == 1) ? k2 : ((debug_k == 2) ? k3 : ((debug_k == 3) ? k4 : ((debug_k == 4) ? k5 : ((debug_k == 5) ? k6 : ((debug_k == 6) ? k7 : k8))))));
    s_val   :: Int64 = (debug_k == 0) ? shift_1 : ((debug_k == 1) ? shift_2 : ((debug_k == 2) ? shift_3 : ((debug_k == 3) ? shift_4 : ((debug_k == 4) ? shift_5 : ((debug_k == 5) ? shift_6 : ((debug_k == 6) ? shift_7 : shift_8))))));
    sc_val  :: Int64 = (debug_k == 0) ? scrambled_k1 : ((debug_k == 1) ? scrambled_k2 : ((debug_k == 2) ? scrambled_k3 : ((debug_k == 3) ? scrambled_k4 : ((debug_k == 4) ? scrambled_k5 : ((debug_k == 5) ? scrambled_k6 : ((debug_k == 6) ? scrambled_k7 : scrambled_k8))))));

    out("  KEY " + string(debug_k + 1) + " [" + string(k_val) + "] | Shift: " + string(s_val) + " | Scrambled: " + string(sc_val));
    out("        -> Canonical : " + clean_site);
    out("        -> Hashed URL: " + hash_site);
    out("-------------------------------------------------------------------------");
};
out("=========================================================================");

# PROXY ROUTE STORAGE
proxy_chains_source :: Array = [];
proxy_chains_proxy  :: Array = [];
proxy_chains_target :: Array = [];

# RIVAL BOT STALKER STATE
bot_port  :: Int64   = 8999;
bot_url   :: String  = "market.vnet";
bot_timer :: Float64 = 0.0;

# Add at top of server initialization:
raid_event_timer :: Float64 = 0.0;

# OVERLOAD STATE STORAGE
market_overloaded_timer :: Float64 = 0.0;
overload_cooldowns      :: Array   = [];
overloaded_urls         :: Array = [];
overloaded_timers       :: Array = [];

fn resolve_canonical(input_url :: String) -> String {
    clean_u :: String = input_url;
    if (clean_u.length() >= 7 && clean_u.substr(0, 7) == "vnet://") {
        clean_u = clean_u.substr(7, clean_u.length() - 7);
    }
    clean_u = clean_str(clean_u);

    through idx :: 0..(all_50_sites.length() - 1) -> loop {
        if (string(all_50_sites[idx]) == clean_u) {
            return string(all_50_sites[idx]);
        }
    };

    through idx :: 0..(hashed_50_sites.length() - 1) -> loop {
        if (string(hashed_50_sites[idx]) == clean_u) {
            return string(all_50_sites[idx]);
        }
    };

    return clean_u;
}

fn resolve_hash(raw_url :: String) -> String {
    through idx :: 0..(all_50_sites.length() - 1) -> loop {
        if (string(all_50_sites[idx]) == raw_url) {
            return string(hashed_50_sites[idx]);
        }
    };
    return raw_url;
}

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
        assigned_20 :: Array = [];

        through m_i :: 0..(mutual_sites.length() - 1) -> loop {
            raw_m_site :: String = string(mutual_sites[m_i]);
            hashed_m_site :: String = resolve_hash(raw_m_site);
            assigned_20.push(hashed_m_site);
        };

        while (assigned_20.length() < 20) {
            rand_s :: Int64 = int64(vmath.random(0, all_50_sites.length() - 1));
            site_name :: String = string(hashed_50_sites[rand_s]);
            
            already :: Int64 = 0;
            through u :: 0..(assigned_20.length() - 1) -> loop {
                if (string(assigned_20[u]) == site_name) { 
                    already = 1; 
                    break; 
                }
            };
            if (already == 0) {
                assigned_20.push(site_name);
            }
        }
        
        dir_payload :: String = "";
        through s_idx :: 0..19 -> loop {
            dir_payload = dir_payload + string(assigned_20[s_idx]) + ((s_idx < 19) ? ":" : "");
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

fn send_key_sync_payload(ip :: String, port :: Int64, dir_payload :: String) {
    sync_str :: String = "KEY_SYNC:" + 
        string(scrambled_k1) + ":" + string(scrambled_k2) + ":" +
        string(scrambled_k3) + ":" + string(scrambled_k4) + ":" +
        string(scrambled_k5) + ":" + string(scrambled_k6) + ":" +
        string(scrambled_k7) + ":" + string(scrambled_k8) + ":" +
        string(global_key_locations[0]) + ":" + string(global_key_locations[1]) + ":" +
        string(global_key_locations[2]) + ":" + string(global_key_locations[3]) + ":" +
        string(global_key_locations[4]) + ":" + string(global_key_locations[5]) + ":" +
        string(global_key_locations[6]) + ":" + string(global_key_locations[7]) + ":" +
        dir_payload;
    
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

        server_outage_timer = server_outage_timer + 0.016;
        if (server_outage_timer >= 30.0) {
            server_outage_timer = 0.0;
            
            if (vmath.random(0.0, 100.0) < 90.0) {
                rand_idx :: Int64 = int64(vmath.random(0, all_50_sites.length() - 1));
                rand_site :: String = string(all_50_sites[rand_idx]);
                
                is_already_down :: Int64 = 0;
                through chk_i :: 0..(overloaded_urls.length() - 1) -> loop {
                    if (string(overloaded_urls[chk_i]) == rand_site) {
                        is_already_down = 1;
                        break;
                    }
                };

                if (is_already_down == 0) {
                    overloaded_urls.push(rand_site);
                    overloaded_timers.push(20.0);
                    
                    broadcast_feed_event("[NET_DESYNC]: BGP ROUTER DROPPED SUBNET " + rand_site + " (SITE UNAVAILABLE)");
                    broadcast_raw("EXPLOIT:SITE_OVERLOADED:" + rand_site);
                }
            }
        }
        
        t_idx :: Int64 = overloaded_timers.length() - 1;
        while (t_idx >= 0) {
            curr_t :: Float64 = float64(overloaded_timers[t_idx]) - 0.016;
            if (curr_t <= 0.0) {
                overloaded_urls.delete_at(t_idx);
                overloaded_timers.delete_at(t_idx);
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

    # ----------------------------------------------------------------
    # RIVAL BOT STALKER & ATTACK ENGINE (ACTIVE HUNTER LOGIC)
    # ----------------------------------------------------------------
    bot_timer = bot_timer + 0.016;
    raid_event_timer = raid_event_timer + 0.016;

    if (bot_timer >= 120.0) {
        bot_timer = 0.0;
        
        target_found :: Int64 = 0;
        if (active_ports.length() > 0) {
            rand_p_idx :: Int64 = int64(vmath.random(0, active_ports.length() - 1));
            target_p_port :: Int64  = int64(active_ports[rand_p_idx]);
            target_p_url  :: String = string(active_urls[rand_p_idx]);
            
            if (target_p_port != bot_port && target_p_url != "vnet.dir") {
                bot_url = target_p_url;
                target_found = 1;
            }
        }

        if (target_found == 0) {
            rand_b_idx :: Int64 = int64(vmath.random(0, all_50_sites.length() - 1));
            bot_url = string(all_50_sites[rand_b_idx]);
        }

        through p_i :: 0..(active_ports.length() - 1) -> loop {
            p_port :: Int64  = int64(active_ports[p_i]);
            p_url  :: String = string(active_urls[p_i]);
            p_ip   :: String = string(active_ips[p_i]);

            if (p_port != bot_port && p_url == bot_url && p_url != "vnet.dir") {
                vnet.send_to(server_sock, p_ip, p_port, "EXPLOIT:BOT_STALK");
                broadcast_feed_event("[WARNING]: SPECTRE_BOT LOCKED ONTO PORT_" + string(p_port) + " AT " + bot_url);
            }
        };
    }

    if (raid_event_timer >= 120.0) {
        raid_event_timer = 0.0;
        if (active_ports.length() > 0) {
            out("[FEDERAL E-RAID]: SHADOW PMC INITIATING NETWORK-WIDE PURGE EVENT!");
            broadcast_feed_event("[FEDERAL E-RAID]: SHADOW PMC INTERCEPT ACTIVE! 30s PURGE WINDOW!");
            broadcast_raw("EXPLOIT:FEDERAL_RAID");
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
                update_peer_session(sender_port, sender_ip, payload, server_uptime);
            } else {
                broadcast_feed_event("[SNIFF]: " + mask_port(sender_port) + " -> " + payload);
            }

            if (cmd == "GET") {
                clean_payload :: String = clean_str(payload);
                if (clean_payload.length() >= 7 && clean_payload.substr(0, 7) == "vnet://") {
                    clean_payload = clean_str(clean_payload.substr(7, clean_payload.length() - 7));
                }

                canonical_site :: String = resolve_canonical(clean_payload);

                site_is_down :: Int64 = 0;
                through o_i :: 0..(overloaded_urls.length() - 1) -> loop {
                    target_down :: String = string(overloaded_urls[o_i]);
                    if (target_down == canonical_site || target_down == clean_payload) {
                        site_is_down = 1;
                        break;
                    }
                };

                if (site_is_down == 1) {
                    vnet.send_to(server_sock, sender_ip, sender_port, "EXPLOIT:SITE_OVERLOADED:" + clean_payload);
                } else {
                    out("[ROUTE] NODE " + string(sender_port) + " (" + sender_ip + ") -> " + canonical_site);
                    
                    is_new_peer :: Int64 = update_peer_session(sender_port, sender_ip, clean_payload, server_uptime);
                    if (is_new_peer == 1) {
                        p_idx :: Int64 = active_ports.length() - 1;
                        send_key_sync_payload(sender_ip, sender_port, string(active_dirs[p_idx]));
                    }
                }
            }

            if (cmd == "ION_STRIKE") {
                target_param :: String = clean_str(payload);
                struck_count :: Int64 = 0;
                is_port_hit  :: Int64 = 0;

                through p :: 0..(active_ports.length() - 1) -> loop {
                    if (string(active_ports[p]) == target_param) {
                        p_ip   :: String = string(active_ips[p]);
                        p_port :: Int64  = int64(active_ports[p]);
                        vnet.send_to(server_sock, p_ip, p_port, "EXPLOIT:BRAINDEAD:ION_STRIKE_DIRECT_HIT");
                        struck_count = struck_count + 1;
                        is_port_hit  = 1;
                    }
                };

                if (is_port_hit == 0) {
                    canonical_target :: String = resolve_canonical(target_param);

                    through p :: 0..(active_ports.length() - 1) -> loop {
                        p_url :: String = resolve_canonical(string(active_urls[p]));
                        if (p_url == canonical_target) {
                            p_port :: Int64  = int64(active_ports[p]);
                            p_ip   :: String = string(active_ips[p]);
                            vnet.send_to(server_sock, p_ip, p_port, "EXPLOIT:BRAINDEAD:ION_STRIKE_SITE_OBLITERATED:" + canonical_target);
                            struck_count = struck_count + 1;
                        }
                    };

                    through idx :: 0..(all_50_sites.length() - 1) -> loop {
                        if (string(all_50_sites[idx]) == canonical_target) {
                            new_salt :: Int64 = int64(vmath.random(100000, 999999));
                            new_hash :: String = vnet.hash_site(canonical_target, new_salt);
                            
                            base_n :: String = canonical_target.substr(0, canonical_target.length() - 5);
                            hashed_50_sites[idx] = base_n + "_" + new_hash + ".vnet";
                            break;
                        }
                    };
                }

                out("[ION STRIKE]: SAT-99 ION BEAM FIRED AT " + target_param + "! " + string(struck_count) + " AGENT(S) OBLITERATED.");
                broadcast_feed_event("[ION STRIKE]: SAT-99 ION CANNON FIRED AT " + target_param + "! " + string(struck_count) + " AGENT(S) FRIED & DESYNCED!");
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
            if (msg == "SCAN:REQ") {
                rand_idx :: Int64 = int64(vmath.random(0, all_50_sites.length() - 1));
                raw_site :: String = string(all_50_sites[rand_idx]);
                
                hashed_site :: String = resolve_hash(raw_site);
                
                vnet.send_to(server_sock, sender_ip, sender_port, "SCAN_RESULT:" + hashed_site);
            }
            if (msg == "SATSCAN:REQ") {
                selected_indices :: Array = [];
                
                while (selected_indices.length() < 10) {
                    rand_i :: Int64 = int64(vmath.random(0, all_50_sites.length() - 1));
                    already :: Int64 = 0;
                    through chk :: selected_indices -> loop {
                        if (int64(chk) == rand_i) { already = 1; break; }
                    };
                    if (already == 0) {
                        selected_indices.push(rand_i);
                    }
                }

                res_payload :: String = "SATSCAN_RES:";
                through s_idx :: 0..9 -> loop {
                    site_i :: Int64 = int64(selected_indices[s_idx]);
                    raw_s :: String = string(all_50_sites[site_i]);
                    hashed_s :: String = resolve_hash(raw_s);

                    traffic_kbps :: Int64 = int64(vmath.random(120, 9800));
                    node_status  :: String = "NOMINAL";
                    if (traffic_kbps > 7000) {
                        node_status = "CRITICAL";
                    } else if (traffic_kbps > 3000) {
                        node_status = "ELEVATED";
                    }

                    res_payload = res_payload + hashed_s + "|" + string(traffic_kbps) + "|" + node_status + ((s_idx < 9) ? ";" : "");
                };

                vnet.send_to(server_sock, sender_ip, sender_port, res_payload);
                broadcast_feed_event("[SATSCAN]: PORT_" + string(sender_port) + " EXECUTED SAT-99 SUB-ORBITAL TELEMETRY SWEEP");
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
                        
                        decoy_targets.delete_at(h_idx);
                        decoy_owners.delete_at(h_idx);
                        decoy_urls.delete_at(h_idx);
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
                        if (non_mutuals.length() > 0) {
                            through n_idx :: 0..(non_mutuals.length() - 1) -> loop {
                                if (leak_count < 5) {
                                    drop_payload = drop_payload + string(non_mutuals[n_idx]) + ":";
                                    leak_count = leak_count + 1;
                                }
                            };
                        }

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

            # =========================================================
            # WIN GOAL 1: GRID BLACKOUT CASCADE (5 MUTUAL CORE NODES)
            # =========================================================
            if (cmd == "OVERLOAD") {
                target_site :: String = clean_str(payload);
                handle_part :: String = "";

                sep_ov :: Int64 = -1;
                through i :: 0..(payload.length() - 1) -> loop {
                    if (payload[i] == ":") { sep_ov = i; break; }
                };
                if (sep_ov > 0) {
                    target_site = clean_str(payload.substr(0, sep_ov));
                    handle_part = clean_str(payload.substr(sep_ov + 1, payload.length() - sep_ov - 1));
                }

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
                    overloaded_timers.push(45.0); # 30 second lockout
                    broadcast_feed_event("[GRID OVERLOAD]: NODE PORT_" + string(sender_port) + " FRIED " + target_site + "!");
                    broadcast_raw("EXPLOIT:SITE_OVERLOADED:" + target_site);

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
                        broadcast_raw("EXPLOIT:WINNER:" + string(sender_port) + ":BLACKOUT:" + handle_part);
                    }
                }
            }

            # =========================================================
            # WIN GOAL 2: ECONOMIC TAKEOVER (25.0 VCOIN)
            # =========================================================
            if (cmd == "TAKEOVER") {
                game_over = 1;
                handle_part :: String = (payload != "") ? payload : ("PORT_" + string(sender_port));
                out("[ECONOMIC TAKEOVER]: VICTORY DETECTED FROM PORT " + string(sender_port));
                broadcast_feed_event("[ECONOMIC TAKEOVER]: NODE PORT_" + string(sender_port) + " BOUGHT OUT VNET NETWORK!");
                broadcast_raw("EXPLOIT:WINNER:" + string(sender_port) + ":ECONOMIC:" + handle_part);
            }

            # =========================================================
            # ORIGINAL WIN CONDITION: CRYPTOGRAPHIC ROOT BREACH
            # =========================================================
            if (cmd == "WIN") {
                win_keys    :: String = payload;
                handle_part :: String = "";

                sep_w :: Int64 = -1;
                through i :: 0..(payload.length() - 1) -> loop {
                    if (payload[i] == ":") { sep_w = i; break; }
                };
                if (sep_w > 0) {
                    win_keys    = payload.substr(0, sep_w);
                    handle_part = payload.substr(sep_w + 1, payload.length() - sep_w - 1);
                }

                if (win_keys == master_keys_str) {
                    game_over = 1;
                    out("[SYSTEM OVERRIDE]: VICTORY DETECTED FROM PORT " + string(sender_port));
                    broadcast_feed_event("[SYSTEM OVERRIDE]: NODE " + string(sender_port) + " HAS BREACHED ROOT VAULT!");
                    broadcast_raw("EXPLOIT:WINNER:" + string(sender_port) + ":KEYS:" + handle_part);
                } else {
                    vnet.send_to(server_sock, sender_ip, sender_port, "WIN:FAIL:INVALID_KEY_COMBINATION");
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
        }
    }
}

vnet.close(server_sock);