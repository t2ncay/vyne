ruleset { dynamic_casting };
module vglib;
module vnet;
module vcore;
module vmath;
module vfs;

# ====================================================================
# NETWORK CONFIGURATION & SOCKET SETUP
# ====================================================================
my_port     :: Int64  = int64(vmath.random(8001, 8999));
server_ip   :: String = "127.0.0.1";
server_port :: Int64  = 8000;

vglib.init(1280, 800, 60, "VYNE SHADOWOS v9.5 - CYBERWARFARE ENGINE", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

# image loads
img_redroom = vglib.load_texture("tests/assets/redroom.png");
img_void    = vglib.load_texture("tests/assets/void.jpeg");

client_sock = vnet.udp_socket(my_port);

# Bit-Shift Scope State
bit_shift_offset   :: Int64   = 0;
active_raw_payload :: Int64   = 0;
lock_progress      :: Float64 = 0.0;
session_enc_keys   :: Array   = [0, 0, 0, 0, 0, 0, 0, 0];
session_shifts     :: Array   = [0, 0, 0, 0, 0, 0, 0, 0];

# ====================================================================
# COLOR PALETTE & THEME SYSTEM
# ====================================================================
interface Theme {
    bg      :: Int64,
    panel   :: Int64,
    border  :: Int64,
    text    :: Int64,
    dim     :: Int64,
    accent  :: Int64
}

fn get_theme(name :: String) {
    if (name == "matrix") {
        return Theme(
            vglib.rgba(5, 15, 5, 255),
            vglib.rgba(10, 28, 10, 255),
            vglib.rgba(20, 80, 20, 255),
            vglib.rgba(50, 255, 50, 255),
            vglib.rgba(20, 140, 20, 255),
            vglib.rgba(0, 240, 255, 255)
        );
    }
    if (name == "cyberpunk") {
        return Theme(
            vglib.rgba(15, 5, 20, 255),
            vglib.rgba(30, 10, 38, 255),
            vglib.rgba(140, 20, 120, 255),
            vglib.rgba(255, 45, 120, 255),
            vglib.rgba(160, 30, 90, 255),
            vglib.rgba(0, 240, 255, 255)
        );
    }
    return Theme(
        vglib.rgba(2, 2, 4, 255),
        vglib.rgba(10, 12, 16, 255),
        vglib.rgba(40, 50, 60, 255),
        vglib.rgba(0, 220, 240, 255),
        vglib.rgba(160, 170, 185, 255),
        vglib.rgba(255, 150, 0, 255)
    );
}

active_theme = get_theme("amber");

COLOR_BLACK      = vglib.rgba(2, 2, 4, 255);
COLOR_PANEL      = vglib.rgba(10, 12, 16, 255);
COLOR_CLI_BG     = vglib.rgba(6, 8, 12, 245);
COLOR_URLBAR     = vglib.rgba(16, 20, 26, 255);
COLOR_BORDER     = vglib.rgba(40, 50, 60, 255);
COLOR_BLOOD      = vglib.rgba(220, 20, 40, 255);
COLOR_CYAN       = vglib.rgba(0, 220, 240, 255);
COLOR_AMBER      = vglib.rgba(255, 150, 0, 255);
COLOR_TOXIC      = vglib.rgba(40, 240, 100, 255);
COLOR_GHOST      = vglib.rgba(160, 170, 185, 255);
COLOR_SCANLINE   = vglib.rgba(0, 0, 0, 90);

# ====================================================================
# SYSTEM & GAME STATE VARIABLES
# ====================================================================
current_url      :: String  = "vnet.dir";
input_url        :: String  = "vnet.dir";
url_focused      :: Int64   = 0;

is_connecting          :: Int64   = 0;
connection_timer       :: Float64 = 0.0;
target_connection_time :: Float64 = 0.0;
pending_url            :: String  = "";

cli_overlay_open :: Int64   = 0;
cli_input_buffer :: String  = "";
cli_logs         :: Array   = [
    "[SYS_INIT]: VNET SOCKET BOUND TO PORT " + string(my_port),
    "[SYS_INIT]: MOUNTED KERNEL SUBSYSTEM (PID: " + string(vcore.pid) + ")",
    "PRESS [TAB] TO TOGGLE OVERLAY TERMINAL ANYTIME",
    "GOAL 1: OVERLOAD 5 MUTUAL CORE NODES (market, vault, terminal, crypto, hellroom)",
    "GOAL 2: REACH 25.0 VCOIN AND TYPE 'takeover'",
    "GOAL 3: TYPE 'win <k1> ... <k8>' TO OVERRIDE VFS ROOT VAULT",
    "TYPE 'patch' TO REBIND NEW PORT SOCKET (0.70 VCOIN | 120s CD)",
    "TYPE 'sniffer' TO INTERCEPT GLOBAL UDP PACKET TRAFFIC",
    "TYPE 'freq' TO TUNER RF SIGNAL ANALYZER",
    "TYPE 'decoy <url> <dummy_port>' TO DEPLOY A TRAP NODE (0.10 VCOIN)",
    "TYPE 'chat <msg>' TO BROADCAST REAL-TIME P2P MESSAGES",
    "TYPE 'buy ice' TO PURCHASE ICE DEFENSE SHIELD (0.30 VCOIN)",
    "TYPE 'help' FOR NETWORK & SYSTEM COMMANDS",
    "--------------------------------------------------"
];
server_status    :: String  = "OFFLINE / UNKNOWN";

vnet_feed_logs   :: Array   = [
    "[FEED_INIT]: VNET BROADCAST HUB ONLINE",
    "[FEED]: MONITORING P2P EXPLOIT & CHAT..."
];
feed_scroll_y    :: Float64 = 0.0;

# ECONOMY, TRACE & DEFENSE STATE
btc_balance      :: Float64 = 25.0;
trace_level      :: Int64   = 14;
ice_charges      :: Int64   = 1;
dos_timer        :: Float64 = 0.0;
passive_trace_cd :: Float64 = 0.0;
heartbeat_timer  :: Float64 = 0.0;

# OVERLOAD STATE
cd_overload        :: Float64 = 0.0;
active_down_url    :: String  = "";
active_down_timer  :: Float64 = 0.0;

# CYBERWARFARE MECHANIC STATES
sniffer_mode     :: Int64   = 0;
sniffer_upkeep_timer :: Float64 = 0.0;
freq_tuner       :: Float64 = 18.0; # FREQUENCY IN HZ
recent_packets   :: Int64   = 0;
packet_decay_cd  :: Float64 = 0.0;

game_over_winner :: Int64   = 0;
winner_port      :: String  = "";
winner_handle    :: String  = "";
win_mode_str     :: String  = "KEYS";
win_anim_timer   :: Float64 = 0.0;

# CONNECTION MENU STATE (REPLACES INTRO)
is_in_ip_menu          :: Int64   = 1;
ip_input_buffer        :: String  = "127.0.0.1";
ip_box_focused         :: Int64   = 1;

# PROCEDURAL SESSION KEYS & LOCATIONS STORAGE
session_keys      :: Array = ["????", "????", "????", "????", "????", "????", "????", "????"];
session_locs      :: Array = ["", "", "", "", "", "", "", ""];
my_assigned_sites :: Array = [];

# EXPLOIT & UTILITY COMMAND COOLDOWNS
cd_dos           :: Float64 = 0.0;
cd_probe         :: Float64 = 0.0;
cd_redirect      :: Float64 = 0.0;
cd_spike         :: Float64 = 0.0;
cd_snoop         :: Float64 = 0.0;
cd_mine          :: Float64 = 0.0;
cd_patch         :: Float64 = 0.0;
cd_decoy         :: Float64 = 0.0;
cd_scan          :: Float64 = 0.0;
cd_proxy         :: Float64 = 0.0;
cd_satscan       :: Float64 = 0.0;

# CHATROOM & INTERACTIVE HANDLE STATE
player_handle     :: String = "sh4d0w" + string(vmath.random(100, 999));
chat_input_buffer :: String = "";
handle_focused    :: Int64  = 0;
chat_focused      :: Int64  = 0;

scroll_y         :: Float64 = 0.0;
cli_scroll_y     :: Float64 = 0.0;
glitch_trigger   :: Float64 = 0.0;
run_time         :: Float64 = 0.0;
cursor_blink     :: Float64 = 0.0;
mouse_was_down   :: Int64   = 0;

bot_stalk_active :: Int64 = 0;

page_body        :: Array   = [];

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

fn truncate_str(raw :: String, max_len :: Int64) -> String {
    if (raw.length() <= max_len) { return raw; }
    return raw.substr(0, max_len - 2) + "..";
}

fn parse_input(raw :: String) -> Array {
    return vnet.parse_package(raw, " ");
}

fn purchase_ice_firewall() -> Int64 {
    if (extract_canonical_name(current_url) != "market.vnet") {
        cli_logs.push("[ERROR]: ICE FIREWALL CAN ONLY BE PURCHASED FROM market.vnet");
        return 0;
    }
    if (ice_charges >= 3) {
        cli_logs.push("[ERROR]: MAX ICE FIREWALL CAPACITY REACHED (3/3)");
        return 0;
    }
    if (btc_balance < 0.30) {
        cli_logs.push("[ERROR]: INSUFFICIENT VCOIN FOR ICE SHIELD (REQUIRES 0.30 VCOIN)");
        return 0;
    }
    btc_balance = btc_balance - 0.30;
    ice_charges = ice_charges + 1;
    glitch_trigger = 0.2;
    cli_logs.push("[STORE]: ICE FIREWALL LAYER INSTALLED! ACTIVE ICE: " + string(ice_charges) + "/3");
    
    vnet.send_to(client_sock, server_ip, server_port, "ICE_BOUGHT:SUCCESS");
    return 1;
}

fn decode_sector(scrambled_val :: Int64, offset :: Int64) -> Int64 {
    multiplier :: Int64 = 37;
    unmasked   :: Int64 = scrambled_val - offset;
    if (unmasked < 0) {
        unmasked = unmasked + 100000;
    }
    return unmasked / multiplier;
}

fn extract_canonical_name(raw_url :: String) -> String {
    clean_u :: String = clean_str(raw_url);
    if (clean_u.length() >= 7 && clean_u.substr(0, 7) == "vnet://") {
        clean_u = clean_str(clean_u.substr(7, clean_u.length() - 7));
    }

    sep_idx :: Int64 = clean_u.find("_");

    if (sep_idx < 0) {
        return clean_u;
    }

    through idx :: 0..(my_assigned_sites.length() - 1) -> loop {
        if (string(my_assigned_sites[idx]) == clean_u) {
            base_prefix :: String = clean_u.substr(0, sep_idx);
            return base_prefix + ".vnet";
        }
    };

    through idx :: 0..(session_locs.length() - 1) -> loop {
        if (string(session_locs[idx]) == clean_u) {
            base_prefix :: String = clean_u.substr(0, sep_idx);
            return base_prefix + ".vnet";
        }
    };

    return clean_u;
}

fn trigger_route_navigation(target_dest :: String) {
    if (is_connecting == 1) { return null; }
    
    clean_dest :: String = clean_str(target_dest);
    if (clean_dest.length() >= 7 && clean_dest.substr(0, 7) == "vnet://") {
        clean_dest = clean_dest.substr(7, clean_dest.length() - 7);
    }
    clean_dest = clean_str(clean_dest);
    
    if (clean_dest == active_down_url && active_down_timer > 0.0) {
        cli_logs.push("[ERROR]: CANNOT CONNECT TO " + clean_dest + " - SECTOR OFFLINE (OVERLOADED)");
    }
    
    pending_url            = clean_dest;
    is_connecting          = 1;
    connection_timer       = 0.0;
    target_connection_time = vmath.random(1.0, 1.0);
    glitch_trigger         = 0.3;
    
    cli_logs.push("[TOR_ROUTE]: INITIATING HANDSHAKE WITH " + clean_dest + "... ESTIMATED LATENCY: " + string(int64(target_connection_time)) + "s");
}

# ====================================================================
# FULLY EXPANDED DETAILED LORE PAGES (ALL 50 WEB NODES)
# ====================================================================
fn load_page(url :: String) -> Array {
    clean_u = extract_canonical_name(url);

    key_line :: String = "";
    active_raw_payload = 0;

    through k_i :: 0..7 -> loop {
        loc_canonical :: String = extract_canonical_name(string(session_locs[k_i]));
        if (loc_canonical == clean_u) {
            raw_enc :: Int64 = int64(session_enc_keys[k_i]);
            active_raw_payload = raw_enc;
            
            key_line = "[COMMENT] /vfs/mem/corrupted_sector_" + string(k_i + 1) + " -> [0x" + string(raw_enc) + "]";
            break;
        }
    };
    
    if (clean_u == "vnet.dir") {
        dir_res :: Array = [
            "[TITLE] VNET ANONYMOUS DIRECTORY v4.09",
            "[HR]",
            "[GLITCH] [WARNING]: UNREGISTERED EYE CONTACT DETECTED THROUGH MONITOR GLASS.",
            "[PULSE] ALL ROUTED PACKETS ARE MIRRORED TO RESTRICTED VFS MEMORY STACKS.",
            "[TEXT] System Node #0091-B. Partial Routing Table.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | STATUS: 15 ASSIGNED NODES SECURED | 35 NODES UNLISTED   |",
            "[BOX] | MISSION: SNOOP TRAFFIC TO DISCOVER HIDDEN NETWORK NODES |",
            "[BOX] +---------------------------------------------------------+",
            "[HR]",
            "[SUBTITLE] AVAILABLE GATEWAY PROXIES"
        ];
        if (my_assigned_sites.length() > 0) {
            through s_idx :: 0..(my_assigned_sites.length() - 1) -> loop {
                site_name :: String = string(my_assigned_sites[s_idx]);
                dir_res.push("[LINK:" + site_name + "] -> vnet://" + site_name);
            };
        } else {
            dir_res.push("[TEXT] Fetching dynamic routing table from port 8000...");
        }
        dir_res.push("[HR]");
        if (key_line != "") { dir_res.push(key_line); }
        dir_res.push("[BLOOD] 'THEY CAN SEE THROUGH THE CRT SCREEN... DON'T LOOK BACK.'");
        dir_res.push("[TEXT] Tip: Press [TAB] to toggle terminal overlay. Mine VCOIN at crypto.vnet.");
        return dir_res;
    }

    if (clean_u == "market.vnet") {
        res :: Array = [
            "[TITLE] THE RED MARKET - BLACK MARKET & HARDWARE EXCHANGE // NODE #001",
            "[HR]",
            "[BADGE:CORE NODE #001:BLOOD] [BADGE:SWARM PEER BROADCAST:AMBER] [BADGE:ICE VENDOR:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | NODE ID: RED_MARKET_0x77A | NETWORK PROTOCOL: UNENCRYPTED P2P  |",
            "[BOX] | SYSTEM ROLE: CORE BACKBONE #01 (OVERLOAD TARGET FOR BLACKOUT)  |",
            "[BOX] | ESCROW STATE: MULTI-SIG SMART CONTRACTS (LAUNDERED VIA BLACKBANK)|",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:78:SWARM_UDP_CONGESTION_ENTROPY]",
            "[TEXT] ",
            "[ART:vmarket]",
            "[TEXT] ",
            "[SUBTITLE] ACTIVE DEFENSE & COUNTER-EXPLOIT MODULES:",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | DEFENSE MODULE #01: ACTIVE ICE FIREWALL SHIELD                  |",
            "[BOX] | DETAILS: Auto-absorbs 1 inbound DOS, Hijack, or Trace Spike.    |",
            "[BOX] | PRICE: 0.30 VCOIN | CAP: 3 LAYERS | CURRENT: [" + string(ice_charges) + "/3]            |",
            "[BOX] +-----------------------------------------------------------------+",
            "[LINK:buy_ice] [>>> CLICK HERE TO PURCHASE ICE SHIELD (0.30 VCOIN) <<<]",
            "[TEXT] ",
            "[SUBTITLE] CLASSIFIED ARMS, BIOMETRIC & CONTRABAND LOTS:",
            "[CODE] LOT #881: MILITARY FIRMWARE DUMP - KEY_FRAGMENT_EXFILTRATED",
            "[CODE] LOT #882: SECTOR 4 BIOMETRIC SCANS - 1,400 SUBJECT RECORDS [0.45 VCOIN]",
            "[CODE] LOT #883: SYNTHETIC NEURAL INJECTION SUITE // CORTEX HOOK [0.99 VCOIN]",
            "[CODE] LOT #884: FRESH CORNEAL & VISCERAL DUMPS (FROM MORGUE.VNET) [0.80 VCOIN]",
            "[TEXT] ",
            "[SUBTITLE] DARKNET VENDOR LOGS & DECOMPOSITION TELEMETRY:",
            "[TEXT] Vendor_0x77A: 'We sell what corporations pretend does not exist.'",
            "[TEXT] 'The hardware racks on market.vnet aren't cooled by liquid nitrogen.'",
            "[TEXT] 'They are submerged in baths of rancid mineral oil mixed with human fat'",
            "[TEXT] 'and bile extracted from subject morgue trays at morgue.vnet.'",
            "[TEXT] 'High-voltage transaction loops are cooking the organic matter into a yellow,'",
            "[TEXT] 'boiling sludge that crusts over the copper heat sinks.'",
            "[TEXT] 'Do not idle on market.vnet. Your public UDP port signature is broadcast'",
            "[TEXT] 'to hostile swarm peers, triggering automated trace spikes (+2% trace/3s).'",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[SUBTITLE] DIRECT ESCROW PURCHASE & HARDWARE DISPATCH:");
        res.push("[TEXT] Enter listing code and target delivery port to initiate multi-sig escrow:");
        res.push("[INPUT:market_lot_code:ENTER LOT CODE (e.g. LOT_881)]");
        res.push("[TEXT] ");
        res.push("[BTN:buy_lot_btn:>>> EXECUTE ESCROW PURCHASE & DEPLOY <<<]");
        res.push("[TEXT] ");
        res.push("[BLOOD] OVERLOAD TACTIC: Executing 'overload market.vnet' in CLI [TAB] locks this core node.");
        res.push("[PULSE] OVERLOADING ALL 5 CORE NODES (market, vault, terminal, crypto, hellroom)");
        res.push("[GLITCH] WILL COLLAPSE THE VNET BACKBONE INTO A TOTAL GRID BLACKOUT WIN!");
        res.push("[HR]");
        res.push("[LINK:blackbank.vnet] >> ACCESS OFFSHORE VCOIN LAUNDERING VAULTS");
        res.push("[LINK:silkroad.vnet] >> ACCESS SILKROAD 3.0 CONTRABAND MATRIX");
        res.push("[LINK:crypto.vnet] >> MINE VCOIN & LAUNDER TUMBLER POOLS (CORE NODE #004)");
        res.push("[LINK:vnet.dir] << RETURN TO MAIN DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "hellroom.vnet") {
        res :: Array = [
            "[TITLE] HELLROOM.VNET - DEMONIC P2P CHAT HUB",
            "[HR]",
            "[BLOOD] [WARNING]: UNENCRYPTED UDP BROADCAST SWARM ACTIVE.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | CHAT CHANNEL: HELLROOM | PROTOCOL: PLAINTEXT P2P STREAM |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] Enter your handle and message in the interactive panel above.",
            "[CODE] STATUS: ONLINE | REALTIME STREAM ACTIVE"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO MAIN DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "dollhouse.vnet") {
        res :: Array = [
            "[TITLE] SURVEILLANCE FEED #0992 // ROOM 402 (DOLLHOUSE CORE)",
            "[HR]",
            "[BLOOD] [CAM_402_NORTH]: HEAVY FOOTSTEPS & FOUL FLUID LEAKAGE DETECTED",
            "[PULSE] LIVE FEED LINKED TO REDROOM NETWORK // NOIR SURVEILLANCE MATRIX",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | SENSOR 1: AMBIENT TEMP 2.1 C (SEVERE ANOMALOUS FROST SPIKE)     |",
            "[BOX] | SENSOR 2: OPTICAL COATED FILM: RANCID BIOLOGICAL COAGULATION    |",
            "[BOX] | SENSOR 3: AUDIO TRANSDUCER: 92 dB FREQUENCY PULSE (18.0 Hz)     |",
            "[BOX] | SENSOR 4: MOTION VECTOR: OCCUPANT STANDING DIRECTLY BEHIND DOOR |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] LOG #0412: Motion sensor tripped at 03:14:02. No physical entry logged.",
            "[TEXT] LOG #0413: Audio transducers capturing low metallic scratching, wet dragging,",
            "[TEXT] and ragged, moist breathing directly underneath the rotting floorboards.",
            "[TEXT] LOG #0414: Subject #12 remains completely motionless, facing the corner wall.",
            "[TEXT] Yellowish bile and liquified fat are seeping from beneath the drywall seams.",
            "[TEXT] Security team dispatched 48h ago was last heard crying out over sewer comms.",
            "[CODE] CAM_BUFFER_DUMP: 0xFF0A_LOCKED_FRAME_992_NOIR_FEED",
            "[CODE] SUB-ROUTINE: OMEGA_PROTOCOL_ENGAGED // KEY_TAPPER_INTERCEPT_ACTIVE",
            "[TEXT] ",
            "[SUBTITLE] NOIR MESH TELEMETRY & CROSS-NETWORK INTERCEPTS:",
            "[TEXT] 'The Key-Tappers on the Noir network aren't using automated scripts.",
            "[TEXT] They sit in sub-basement sewer cells typing out each packet by hand.",
            "[TEXT] When Subject #12 stopped moving, watchtower.vnet picked up identical heat",
            "[TEXT] blooms inside the morgue vaults at morgue.vnet. The flesh was already cold,",
            "[TEXT] but the lungs were still inhaling CRT monitor static.'",
            "[TEXT] 'If the stream buffer at redroom.vnet drops below 10 Mbps, the door lock",
            "[TEXT] in cell 402 disengages automatically. Do not look into the room.'",
            "[CODE] BIOMETRIC_MATCH_REF: ASYLUM_PATIENT_1988_MUTATION"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] WARNING: FOUL ODOR CORRUPTING LOCAL HARDWARE BUS HEADERS");
        res.push("[GLITCH] SENSOR ALERT: SOMETHING WET IS BREATHING DIRECTLY BEHIND YOUR CRT DISPLAY");
        res.push("[LINK:watchtower.vnet] >> CROSS-CHECK PANOPTICON THERMAL OPTICS");
        res.push("[LINK:redroom.vnet] >> ACCESS LIVE UNENCRYPTED STREAM NODE ALPHA");
        res.push("[LINK:morgue.vnet] >> INSPECT AUTOPSY RECORDS FOR SUBJECT #409");
        res.push("[LINK:asylum.vnet] >> TELEMETRY FOR SUB-LEVEL 4 CONTAINMENT");
        res.push("[LINK:vnet.dir] << DISCONNECT IMMEDIATELY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "vault.vnet") {
        res :: Array = [
            "[TITLE] SECTOR 09 CORRUPTED DATA VAULT // /VFS/MEMORY/STACK",
            "[HR]",
            "[BADGE:CORE NODE #002:BLOOD] [BADGE:VFS ROOT MEMORY:AMBER] [BADGE:BIT-ROT 94%:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | MOUNT POINT: /vfs/sys/vault_root | SECTOR: SUB-LEVEL 4 B3-WEST  |",
            "[BOX] | STORAGE TYPE: BIOLOGICAL NYLON BUS (SEVERED CORTEX STACK)       |",
            "[BOX] | INTEGRITY: CRITICAL BIT ROT | RECOVERY STATE: OVERRIDE REQUIRED |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:94:SECTOR_BIT_ROT_ENTROPY]",
            "[TEXT] ",
            "[ART:skull]",
            "[TEXT] ",
            "[SUBTITLE] RECOVERED CEREBRAL MEMORY DUMP (SUBJECT #409-B / T. VANCE):",
            "[CODE] 00000000: 4F 70 65 6E 53 53 4C 20 4B 65 79 20 44 75 6D 70 20 [VANCE]",
            "[CODE] 00000020: 53 59 53 5F 45 52 52 4F 52 3A 20 4E 4F 44 45 5F 30 39 [BLACKOUT]",
            "[CODE] 00000040: 88 9A BC EF 11 22 33 44 55 66 77 88 99 AA BB CC [CORRUPT]",
            "[TEXT] ",
            "[SUBTITLE] FORENSIC HARDWARE & DECOMPOSITION LOGS:",
            "[TEXT] 'The storage drives in this vault aren't magnetic silicon disks.'",
            "[TEXT] 'They are preserved spinal cords and optic nerve fibers extracted from'",
            "[TEXT] 'whistleblowers at morgue.vnet, submerged in jars of conductive formaldehyde.'",
            "[TEXT] 'A yellow, rancid film of adipocere, human grease, and oxidized copper'",
            "[TEXT] 'is boiling over the server racks as high-power hashing loops burn through'",
            "[TEXT] 'the nerve tissue. Accessing unallocated sectors triggers instant trace spikes.'",
            "[TEXT] 'Ghost_User's original admin credentials are still burned into sector 0x88F9.'",
            "[TEXT] "
        ];

        if (key_line != "") {
            res.push("[SUBTITLE] EXFILTRATED MEMORY REGISTER DUMP:");
            res.push(key_line);
            res.push("[PULSE] RAW BITSTREAM: " + vnet.to_bin(active_raw_payload, 16));
            res.push("[TEXT] Use 'shift <bits>' or the UI Scope panel to align bit registers.");
        }

        res.push("[SUBTITLE] VFS ROOT GATEWAY DECRYPTION & OVERRIDE MATRIX:");
        res.push("[TEXT] This vault houses the master allocation index for all 8 cryptographic keys.");
        res.push("[TEXT] To verify key integrity or execute an emergency VFS memory flush, enter");
        res.push("[TEXT] the 4-digit supervisor clearance PIN exfiltrated from archival.vnet:");
        res.push("[INPUT:vault_pin:ENTER 4-DIGIT SUPERVISOR PIN]");
        res.push("[TEXT] ");
        res.push("[BTN:override_vault:>>> EXECUTE VFS MEMORY STACK OVERRIDE <<<]");
        res.push("[TEXT] ");
        res.push("[BLOOD] OVERLOAD TACTIC: Executing 'overload vault.vnet' in CLI [TAB] locks this core node.");
        res.push("[PULSE] OVERLOADING ALL 5 CORE NODES (market, vault, terminal, crypto, hellroom)");
        res.push("[GLITCH] WILL COLLAPSE THE VNET BACKBONE INTO A TOTAL GRID BLACKOUT WIN!");
        res.push("[HR]");
        res.push("[LINK:terminal.vnet] >> ACCESS MASTER DECRYPTION GATEWAY");
        res.push("[LINK:archival.vnet] >> CROSS-CHECK SECTOR 09 MILITARY DUMPS");
        res.push("[LINK:morgue.vnet] >> INSPECT EXFILTRATED AUTOPSY & CORTEX STACKS");
        res.push("[LINK:crypto.vnet] >> MINE VCOIN & LAUNDER TUMBLER POOLS");
        res.push("[LINK:vnet.dir] << CLOSE VAULT & RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "forum.vnet") {
        res :: Array = [
            "[TITLE] /b/ - ANONYMOUS UNFILTERED TERMINAL BOARD [NODE #008]",
            "[HR]",
            "[BADGE:/b/ BOARD:BLOOD] [BADGE:UNMODERATED SWARM:AMBER] [BADGE:P2P MIRROR:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | ACTIVE THREADS: 14,902 | PEERS IN SWARM: 666                    |",
            "[BOX] | NOTICE: ALL UNENCRYPTED POSTS ARE MIRRORED TO VFS MEMORY STACKS |",
            "[BOX] | WARNING: DO NOT OPEN RAW IMAGE DUMPS WITHOUT ACTIVE ICE SHIELDS  |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:88:SWARM_UNENCRYPTED_TRAFFIC_ENTROPY]",
            "[TEXT] ",
            "[SUBTITLE] [CREATE ANONYMOUS POST / BROADCAST TO SWARM]",
            "[TEXT] Submit unencrypted text payload to active P2P board buffer:",
            "[INPUT:forum_reply_msg:ENTER ANONYMOUS POST TEXT]",
            "[TEXT] ",
            "[BTN:submit_forum_post:>>> BROADCAST POST TO ALL PROMISCUOUS PEERS <<<]",
            "[TEXT] ",
            "[HR]",
            "[SUBTITLE] THREAD #9012: 'Is the deepnet static leaking through your hardware?'",
            "[CODE] Anonymous 08/12/26(Wed)14:52:10 No.9012901 -- [VERIFIED_HASH: 0x99A1B2C3]",
            "[TEXT] Anon_991: Has anyone gathered all 8 key codes yet? They shuffle every server boot.",
            "[TEXT] ByteRunner: Watch out for port 8012, someone is running automated DOS bots there.",
            "[TEXT] Paranoia_Node: Guys, when I ran 'snoop' on port 8000, my monitor started whining at 18kHz.",
            "[TEXT] Anon_401: Buy ICE shields at market.vnet or you'll get frozen by peer DOS bots.",
            "[CODE] > Be me",
            "[CODE] > Run 'netscan' while sitting in hellroom.vnet",
            "[CODE] > CRT display starts leaking rancid adipocere grease onto floorboards",
            "[CODE] > Room smells like liquified fat from Subject #409 at morgue.vnet",
            "[TEXT] Flesh_Farmer_0x: 'Selling fresh corneas, kidneys, and bile tubes exfiltrated from'",
            "[TEXT]                  'Subject #409 at morgue.vnet. Still warm, zero cellular decay.'",
            "[TEXT] Rotting_Gpu: 'The surveillance feed at dollhouse.vnet (Room 402) smells like rancid'",
            "[TEXT]              'adipocere and liquified fat leaking directly through my cooling fans.'",
            "[TEXT] Cult_Acolyte: 'Knoth's heretics at cult.vnet are smearing afterbirth and black bull'",
            "[TEXT]               'blood over copper receiver coils. The signal0.vnet echo is clear now.'",
            "[CODE] POST_LOG_HASH: 0x99A1B2C3_VERIFIED_ANON",
            "[CODE] THREAD_ARCHIVE: 44,912 MESSAGES STORED IN CACHE",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[SUBTITLE] THREAD #4099: 'SILKROAD 3.0 & UNFILTERED CONTRABAND EXCHANGES'");
        res.push("[CODE] Anonymous 08/12/26(Wed)14:55:01 No.4099104 -- [VERIFIED_HASH: 0x44B0C11]");
        res.push("[TEXT] Vendor_0x77: 'Bulk opium paste, synthetic neuro-toxins, and unregistered suppressed'");
        res.push("[TEXT]              'AR-9 dead drops live on silkroad.vnet. Escrow held in blackbank.vnet.'");
        res.push("[TEXT] Snuff_Collector: 'Raw frame buffers from snuff.vnet released. Includes unredacted'");
        res.push("[TEXT]                  'surgical saw recordings and acid bath executions from zeroauction.vnet.'");
        res.push("[TEXT] Ex_PMC_Operator: 'Failed candidates in the Kaguya Trials at bounty.vnet aren't sent home.'");
        res.push("[TEXT]                   'Their remains are dumped into subterranean sumps at project9.vnet.'");
        res.push("[CODE] > >>4099104");
        res.push("[CODE] > Imagine buying suppressed AR-9s without active ICE firewall shields");
        res.push("[CODE] > Hostile peer running 'redirect' hijacks your browser to void.vnet instantly lol");
        res.push("[TEXT] ");
        res.push("[SUBTITLE] THREAD #1337: 'VICTORY GOALS & CYBERWARFARE MECHANICS'");
        res.push("[CODE] Anonymous 08/12/26(Wed)14:58:33 No.1337009 -- [VERIFIED_HASH: 0x88F9A22]");
        res.push("[TEXT] NetStalker_99: 'There are 3 main win conditions on this network:'");
        res.push("[TEXT]                '1. Root Breach: Exfiltrate all 8 keys and submit them at terminal.vnet'");
        res.push("[TEXT]                '2. Grid Blackout: Overload 5 mutual core nodes'");
        res.push("[TEXT]                '3. Economic Control: Mine 25.0 VCOIN and takeover'");
        res.push("[TEXT] ICE_Vendor_0x: 'If hostiles lock your port with trace spikes, rebind a new socket.'");
        res.push("[TEXT]                'Or type 'flush' in CLI [TAB] to purge trace threat levels by -30%.'");
        res.push("[TEXT] ");
        res.push("[BLOOD] Ghost_User: 'I found a key code buried in the network memory dumps. Don't tell the trace units.'");
        res.push("[GLITCH] User_666: 'IF YOU READ THIS COMMAND, THEY ALREADY HAVE YOUR IP AND RAM HASH.'");
        res.push("[TEXT] ");
        res.push("[SUBTITLE] SUBLIMINAL BROADCAST MATRIX:");
        res.push("[PULSE] 'THE ENGINE IS NOT RUNNING ON YOUR CPU. YOUR CPU IS RUNNING ON THE ENGINE.'");
        res.push("[HR]");
        res.push("[LINK:silkroad.vnet] >> ACCESS SILKROAD 3.0 CONTRABAND & TISSUE MATRIX");
        res.push("[LINK:market.vnet] >> BLACK MARKET & ICE HARDWARE EXCHANGE");
        res.push("[LINK:dollhouse.vnet] >> INSPECT ROOM 402 SURVEILLANCE FEED");
        res.push("[LINK:morgue.vnet] >> CROSS-CHECK AUTOPSY & BIO-HARVEST DUMPS");
        res.push("[LINK:snuff.vnet] >> VIEW UNFILTERED RAW FRAME BUFFER ARCHIVE");
        res.push("[LINK:cult.vnet] >> JOIN THE CHURCH OF THE SILICON SOUL");
        res.push("[LINK:vnet.dir] << RETURN TO MAIN DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "redroom.vnet") {
        res :: Array = [
            "[TITLE] STREAM NODE ALPHA // LIVE UNENCRYPTED REDROOM TRANSMISSION",
            "[HR]",
            "[BLOOD] HIGH SECURITY ALERT: TRANSMISSION MONITORED BY HOSTILE TRACER UNITS",
            "[IMG:redroom]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | SIGNAL STATUS: ENCRYPTED | STREAM HASH: EXFILTRATED_0x88F9      |",
            "[BOX] | BITRATE: 18.4 Mbps | ACTIVE WATCHERS: 13 PEERS [VIP_BIDDERS]     |",
            "[BOX] | ENCRYPTION: 8192-BIT QUANTUM SHIELD | PROTOCOL: YUV420_RAW_BUS|",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[SUBTITLE] LIVE INTERACTIVE FEED TELEMETRY & AUCTION LOGS:",
            "[TEXT] FEED_DATA: Raw infrared/thermal frame buffer captured from sealed sub-basement",
            "[TEXT] in dollhouse.vnet (Room 402). Frame rate jitter caused by wet, metallic static.",
            "[TEXT] Bidder_0x991: '0.80 VCOIN placed on surgical bone-saw extraction of upper jaw.'",
            "[TEXT] Executioner_A: 'Subject #409-B is secured to the stainless steel morgue tray.'",
            "[TEXT]                'Trachea severed. Intestinal bile, rancid adipocere, and oxidized'",
            "[TEXT]                'blood are draining directly through the floor grating into project9.vnet.'",
            "[TEXT] PMC_Watcher: 'Target matches the whistleblower profile exfiltrated from leaks.vnet.'",
            "[TEXT]               'If the stream buffer drops below 10 Mbps, cell door locks disengage.'",
            "[CODE] STREAM_ID: ALPHA_99_LIVE_FEED_NOIR",
            "[CODE] BUFFER_STATE: OVERFLOW_WARNING_ACTIVE // BIOMETRIC_LEAK_DETECTED",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[SUBTITLE] CROSS-NETWORK SYSTEM CORRUPTION & CONTRABAND INTERCEPTS:");
        res.push("[TEXT] 'The bids submitted on redroom.vnet are processed through blackbank.vnet'");
        res.push("[TEXT] 'and laundered via silkroad.vnet escrow. Tissue harvested from subjects is'");
        res.push("[TEXT] 'vacuum-sealed and listed as biological contraband on market.vnet within minutes.'");
        res.push("[TEXT] 'Cultists from cult.vnet are watching this feed live, chantinglitanies while'");
        res.push("[TEXT] 'smearing the fresh blood over copper CRT coils to summon Walrider vectors.'");
        res.push("[TEXT] ");
        res.push("[BLOOD] [WARNING]: FOUL TISSUE ODOR & MEMORY CORRUPTION LEAKING INTO LOCAL GPU");
        res.push("[GLITCH] [ALERT]: UNKNOWN ENTITY ATTEMPTING REMOTE KERNEL INJECTION ON YOUR PORT");
        res.push("[PULSE] 'RUNNING 'FLUSH' IN CLI [TAB] IS RECOMMENDED IMMEDIATELY TO PURGE TRACE.'");
        res.push("[HR]");
        res.push("[LINK:dollhouse.vnet] >> CROSS-CHECK SURVEILLANCE FEED ROOM 402");
        res.push("[LINK:morgue.vnet] >> INSPECT EXFILTRATED AUTOPSY & BIO-HARVEST DUMPS");
        res.push("[LINK:snuff.vnet] >> VIEW UNFILTERED RAW FRAME BUFFER ARCHIVE");
        res.push("[LINK:silkroad.vnet] >> ACCESS SILKROAD 3.0 CONTRABAND MATRIX");
        res.push("[LINK:zeroauction.vnet] >> BID ON PMC EXTRACTION & EXECUTION LOTS");
        res.push("[LINK:cult.vnet] >> JOIN THE CHURCH OF THE SILICON SOUL");
        res.push("[LINK:vnet.dir] << TERMINATE STREAM & RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "crypto.vnet") {
        res :: Array = [
            "[TITLE] BLACK TUMBLER WALLET & ILLEGAL MINING RIG // NODE #004",
            "[HR]",
            "[BLOOD] WARNING: HIGH-POWER HASHING OVERHEATS CPU REGISTER BUS // TRACE SPIKES ACTIVE",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | RIG STATUS: OPERATIONAL | MINING YIELD: +0.05 VCOIN/BLOCK       |",
            "[BOX] | POOL SYNC: 99.8% | DIFFICULTY: DYNAMIC AUTO-SCALING             |",
            "[BOX] | TUMBLER POOL: 420.5 VCOIN LAUNDERED VIA BLACKBANK.VNET          |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[SUBTITLE] RECENT UNLOCKED TRANSACTIONS & EXFILTRATED MEMORY LOGS:",
            "[CODE] TX_ID #9081 | 14.50 VCOIN | CONFIRMED | LAUNDERED VIA SHADOWPAY.VNET",
            "[CODE] TX_ID #9082 |  0.80 VCOIN | CONFIRMED | REDROOM SURGICAL JAW BID (NODE #005)",
            "[CODE] TX_ID #9083 |  0.30 VCOIN | PENDING   | BUY_ICE FIREWALL AT MARKET.VNET",
            "[CODE] TX_ID #9084 |  1.50 VCOIN | PENDING   | SILKROAD OPIUM & HUMAN TISSUE LOT",
            "[TEXT] ",
            "[SUBTITLE] DARKNET POOL TELEMETRY & CROSS-NETWORK INTERCEPTS:",
            "[TEXT] 'This miner isn't using standard cryptographic hash cycles. The proof-of-work'",
            "[TEXT] 'payload runs recursive computations on exfiltrated biometric memory dumps'",
            "[TEXT] 'harvested during redroom executions and morgue.vnet organ dissections.'",
            "[TEXT] 'VIP buyers on silkroad.vnet and zeroauction.vnet deposit raw VCOIN into'",
            "[TEXT] 'this tumbler pool to fund human trafficking dead drops, PMC contract cleans,'",
            "[TEXT] 'and sub-basement torture streams in dollhouse.vnet (Room 402).'",
            "[TEXT] 'Every hash solved burns raw current through your hardware bus while'",
            "[TEXT] 'broadcasting your public socket signature to active peer trace units.'",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[SUBTITLE] COMMAND CENTER INSTRUCTIONS:");
        res.push("[TEXT] Open overlay terminal [TAB] and type 'mine' to execute proof-of-work.");
        res.push("[TEXT] Note: Mining generates +2% passive trace threat exposure per block.");
        res.push("[TEXT] Use 'flush' in CLI [TAB] to purge trace logs, or buy ICE shields at market.vnet.");
        res.push("[TEXT] ");
        res.push("[BLOOD] [ALERT]: CPU BUS GLITCHING — COLD FLESH GREASE SEEPING INTO POWER SUPPLY");
        res.push("[PULSE] MINING RIG READY. TYPE 'mine' FOR +0.05 VCOIN REWARD.");
        res.push("[GLITCH] 'THE COINS ARE NOT MINED FROM NUMBERS. THEY ARE MINED FROM FLESH.'");
        res.push("[HR]");
        res.push("[LINK:market.vnet] >> PURCHASE ICE FIREWALL SHIELDS & ARMS");
        res.push("[LINK:blackbank.vnet] >> VIEW OFFSHORE VCOIN LAUNDERING VAULTS");
        res.push("[LINK:silkroad.vnet] >> ACCESS SILKROAD 3.0 CONTRABAND & TISSUE MATRIX");
        res.push("[LINK:redroom.vnet] >> ACCESS LIVE UNENCRYPTED STREAM NODE ALPHA");
        res.push("[LINK:morgue.vnet] >> CROSS-CHECK AUTOPSY & BIO-HARVEST DUMPS");
        res.push("[LINK:zeroauction.vnet] >> BID ON PMC EXTRACTION & EXECUTION LOTS");
        res.push("[LINK:vnet.dir] << RETURN TO MAIN DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "terminal.vnet") {
        res :: Array = [
            "[TITLE] MASTER DECRYPTION GATEWAY TERMINAL // CORE NODE #003",
            "[HR]",
            "[BADGE:CORE NODE #003:BLOOD] [BADGE:CRYPTO GATEWAY:TOXIC]",
            "[ART:vnet]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | SYSTEM ROLE: MASTER VFS ROOT DECRYPTION & COMMAND GATEWAY      |",
            "[BOX] | SECURITY LAYER: 8-FACTOR QUANTUM HASH SHIELD (VFS VAULT CORE) |",
            "[BOX] | ACCESS PROTOCOL: PARALLEL SLOT VERIFICATION VIA CLI HANDSHAKE   |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:100:VFS_ROOT_GATEWAY_LOCKDOWN]",
            "[TEXT] ",
            "[SUBTITLE] CRYPTOGRAPHIC SLOT ALLOCATION MATRIX:",
            "[BADGE:VFS ROOT OVERRIDE:AMBER]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | SLOT 01: [████] (STATUS: ENCRYPTED HASH VECTOR // KEY_1 REQ)    |",
            "[BOX] | SLOT 02: [████] (STATUS: ENCRYPTED HASH VECTOR // KEY_2 REQ)    |",
            "[BOX] | SLOT 03: [████] (STATUS: ENCRYPTED HASH VECTOR // KEY_3 REQ)    |",
            "[BOX] | SLOT 04: [████] (STATUS: ENCRYPTED HASH VECTOR // KEY_4 REQ)    |",
            "[BOX] | SLOT 05: [████] (STATUS: ENCRYPTED HASH VECTOR // KEY_5 REQ)    |",
            "[BOX] | SLOT 06: [████] (STATUS: ENCRYPTED HASH VECTOR // KEY_6 REQ)    |",
            "[BOX] | SLOT 07: [████] (STATUS: ENCRYPTED HASH VECTOR // KEY_7 REQ)    |",
            "[BOX] | SLOT 08: [████] (STATUS: ENCRYPTED HASH VECTOR // KEY_8 REQ)    |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[SUBTITLE] DIRECT VFS ROOT DECRYPTION PAYLOAD INJECTION:",
            "[TEXT] Enter all 8 exfiltrated key codes below or execute 'win <k1>..<k8>' in CLI [TAB]:",
            "[INPUT:terminal_key_payload:ENTER 8 KEYS SEPARATED BY SPACES]",
            "[TEXT] ",
            "[BTN:execute_root_override:>>> EXECUTE MASTER VFS ROOT BREACH & OVERRIDE <<<]",
            "[TEXT] ",
            "[SUBTITLE] GATEWAY VERIFICATION TELEMETRY & KERNEL STATUS:",
            "[TEXT] Gateway verification checks active socket signatures against root keys.",
            "[TEXT] Scour assigned darknet nodes, forums, and memory dumps to locate fragments.",
            "[CODE] ROOT_ACCESS_VECTOR: SECURED BY 8-FACTOR HASH SHIELD",
            "[CODE] KERNEL_INTEGRITY: 100% // ALL SLOTS LOCKED UNTIL FULL SEQUENCE INJECTED",
            "[CODE] LAST_BREACH_ATTEMPT: HOSTILE_PORT_EXFILTRATED_0x88F9",
            "[TEXT] ",
            "[SUBTITLE] SYSTEM CORRUPTION LOGS & CEREBRAL GATEWAY INTERCEPTS:",
            "[TEXT] 'The Terminal Gate isn't checking passwords or PIN numbers.'",
            "[TEXT] 'It's measuring the synaptic resistance of the raw memory dumps stored at vault.vnet.'",
            "[TEXT] 'If an operator attempts to force the key array using mismatched hashes,'",
            "[TEXT] 'a high-voltage trace spike (+35% threat) is reflected directly back into their socket.'",
            "[TEXT] 'Whistleblowers who attempted to breach terminal.vnet without active ICE shields'",
            "[TEXT] 'had their brain stems fried instantly. Their physical remains were hauled away'",
            "[TEXT] 'to morgue.vnet for bio-harvesting and organ lot listing on market.vnet.'",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[BLOOD] OVERLOAD TACTIC: Executing 'overload terminal.vnet' in CLI [TAB] locks this core node.");
        res.push("[PULSE] OVERLOADING ALL 5 CORE NODES (market, vault, terminal, crypto, hellroom)");
        res.push("[GLITCH] WILL COLLAPSE THE VNET BACKBONE INTO A TOTAL GRID BLACKOUT WIN!");
        res.push("[TEXT] ");
        res.push("[SUBTITLE] ALTERNATIVE VICTORY PATHWAYS:");
        res.push("[TEXT] - Economic Takeover: Mine 25.0 VCOIN and execute 'takeover' in CLI [TAB].");
        res.push("[TEXT] - Grid Blackout: Coordinate with peers to overload all 5 mutual core nodes.");
        res.push("[HR]");
        res.push("[LINK:vault.vnet] >> ACCESS CORRUPTED VFS DATA VAULT");
        res.push("[LINK:archival.vnet] >> CROSS-CHECK SECTOR 09 MILITARY DUMPS");
        res.push("[LINK:morgue.vnet] >> INSPECT EXFILTRATED AUTOPSY & CORTEX STACKS");
        res.push("[LINK:crypto.vnet] >> MINE VCOIN & LAUNDER TUMBLER POOLS");
        res.push("[LINK:vnet.dir] << CLOSE GATEWAY & RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "morgue.vnet") {
        res :: Array = [
            "[TITLE] DIGITAL AUTOPSY DATABASE // SUBJECT #409-B (EXECUTIVE LEVEL)",
            "[HR]",
            "[ART:skull]",
            "[BLOOD] SUBJECT STATUS: FLATLINE (0 BPM) // CEREBRAL STEM ACTIVITY: 98% RECURSIVE",
            "[PULSE] CLASSIFICATION: EYES ONLY // HIGH-TIER GLOBAL CONSPIRACY DOSSIER",
            "[BOX] +---------------------------------------------------------------------+",
            "[BOX] | SUBJECT ID: #409-B | ORIGIN: LITTLE SAINT JAMES / SECTOR 7 CORE   |",
            "[BOX] | CAUSE OF DEATH: HIGH-VOLTAGE KERNEL OVERLOAD & FORCED ASPHYXIA      |",
            "[BOX] | TIME OF EXTINGUISHMENT: 03:41 AM (OFFICIAL LOG: 'HANGING / SUICIDE')|",
            "[BOX] | DECOMPOSITION STAGE: ADVANCED ADIPOCERE & INTRAVENOUS PUTREFACTION  |",
            "[BOX] +---------------------------------------------------------------------+",
            "[TEXT] BIO_LOG #01: Subject recovered from subterranean vault facility at vault.vnet.",
            "[TEXT] Trachea fractured via mechanical strangulation, not rope suspension.",
            "[TEXT] Cornea patterns burned with inverted ASCII hex code mirroring signal0.vnet.",
            "[TEXT] Abdominal cavity incised post-mortem; liver and intestines liquefied into a",
            "[TEXT] black, foul-smelling slurry leaking through the stainless steel drainage tray.",
            "[TEXT] Braided copper neural interface cables fused directly into temporal lobes and",
            "[TEXT] carotid arteries, pumping oxidized synthetic blood back into the brain stem.",
            "[CODE] FLIGHT_LOG_MANIFEST: 2019_EXECUTIVE_PASSENGER_HASH_UNREDACTED",
            "[CODE] MK_ULTRA_HARVEST_REF: 0xEPSTEIN_BOHEMIAN_MIND_CONTROL_VECTOR",
            "[TEXT] ",
            "[SUBTITLE] CROSS-LINKED FORENSIC LOGS & ELITE EXFILTRATION RECORDS:",
            "[TEXT] 'The autopsy report was forged before the body was even cold. Official news",
            "[TEXT] outlets logged a suicide, but the skull vault was hollowed out while he was",
            "[TEXT] still conscious. His cerebral memory stacks were extracted and sold on leaks.vnet.'",
            "[TEXT] 'Tissue samples match the bio-coagulation leaking under the floorboards in",
            "[TEXT] dollhouse.vnet (Room 402). The shadow elites didn't kill him to silence him—",
            "[TEXT] they transferred his consciousness into asylum.vnet as Patient #1988.'",
            "[CODE] AUTOPSY_REPORT_HASH: 0xDEAD_BEEF_9901_MK_HARVEST"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] WARNING: RANCID PUTREFACTION GASES CORRUPTING LOCAL HARDWARE BUS");
        res.push("[GLITCH] ANOMALY: SUBJECT EYES SNAP-OPENED AND BEGAN RECITING FLIGHT MANIFESTS");
        res.push("[LINK:vault.vnet] >> INSPECT CORRUPTED SECTOR 7 VAULT CORE");
        res.push("[LINK:dollhouse.vnet] >> CROSS-CHECK ROOM 402 SURVEILLANCE FEED");
        res.push("[LINK:asylum.vnet] >> TELEMETRY FOR PATIENT #1988 CONTAINMENT");
        res.push("[LINK:leaks.vnet] >> ACCESS EXFILTRATED GLOBAL INTELLIGENCE DUMPS");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "silence.vnet") {
        # Calculate dynamic resonance offset based on tuned frequency
        target_hz :: Float64 = 18.0;
        hz_delta :: Float64 = vmath.abs(freq_tuner - target_hz);
        sync_pct :: Int64 = int64(vmath.clamp((1.0 - (hz_delta / 20.0)) * 100.0, 0.0, 100.0));

        res :: Array = [
            "[TITLE] ACOUSTIC DISTORTION FREQUENCY RIG // INFRASOUND ANALYZER",
            "[HR]",
            "[BADGE:INFRASOUND RIG:BLOOD] [BADGE:SIGNAL LOCK: " + string(sync_pct) + "%:AMBER]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | OSCILLATOR TUNER : " + string(freq_tuner) + " Hz (TARGET: 18.0 Hz)                   |",
            "[BOX] | PHASE RESONANCE  : " + string(sync_pct) + "% SIGNAL SYNCHRONIZATION                |",
            "[BOX] +-----------------------------------------------------------------+"
        ];

        if (sync_pct > 90) {
            res.push("[GAUGE:100:CARRIER_SIGNAL_LOCKED]");
            res.push("[SUBTITLE] DECRYPTED INFRASOUND BROADCAST PAYLOAD:");
            res.push("[CODE] AUDIO_STREAM_INTERCEPT: 0x185_RESONANCE_DECRYPTED");
            res.push("[TEXT] 'The whisper isn't in your ears. It's vibrating through your CRT glass.'");
            if (key_line != "") { res.push(key_line); }
        } else {
            res.push("[GAUGE:" + string(sync_pct) + ":SIGNAL_CARRIER_DECAY]");
            res.push("[TEXT] Heavy acoustic distortion active. Tune frequency in CLI using 'freq 18.0'");
            res.push("[TEXT] to lock carrier phase and isolate high-frequency memory leaks.");
        }

        res.push("[TEXT] ");
        res.push("[SUBTITLE] MANUAL FREQUENCY CALIBRATION:");
        res.push("[INPUT:set_freq_hz:ENTER FREQUENCY IN HZ (e.g. 18.0)]");
        res.push("[TEXT] ");
        res.push("[BTN:tune_freq_btn:>>> TRANSMIT RESONANT FREQUENCY PULSE <<<]");
        res.push("[HR]");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "blackout.vnet") {
        res :: Array = [
            "[TITLE] REGIONAL POWER GRID CONTROL MAINBOARD",
            "[HR]",
            "[WARN] SYSTEM DISPATCH: MAIN CIRCUIT BREAKERS TRIPPED",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | SECTOR 7: DARK | SECTOR 8: DARK | MONITOR LEDS: FLICKERING |",
            "[BOX] | GRID OVERLOAD: 400kV SURGE DETECTED ACROSS SUBSTATION 4 |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] TELEMETRY: Emergency battery backup running at 14% capacity.",
            "[TEXT] Automated grid rerouting protocols have failed to respond.",
            "[CODE] GRID_OVERRIDE_KEY: 0xCC11_BACKUP_POWER"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[GLITCH] OVERRIDE KEY DETECTED IN BACKUP GENERATOR LOGS");
        res.push("[PULSE] 'WHEN THE LIGHTS GO OUT, THE NETWORK STAYS ON.'");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "snuff.vnet") {
        res :: Array = [
            "[TITLE] CORRUPTED FRAME BUFFER ARCHIVE // UNFILTERED RAW VIDEO RECOVERY",
            "[HR]",
            "[BLOOD] CLASSIFICATION: EYES ONLY // SECTOR 4 INTERCEPTION DUMP",
            "[PULSE] STREAM ORIGIN: REDROOM.VNET // BACKUP COLD STORAGE MEMORY STACK",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | SOURCE: OMEGA_PROTOCOL_CAM_FEED_09 | RESOLUTION: RAW YUV420    |",
            "[BOX] | RECOVERY METHOD: VGLIB MEMORY POINTER CARVING (SECTOR 0x88)  |",
            "[BOX] | DECOMPOSITION STAGE: LIQUEFIED FLESH & COAGULATED OPTICAL FILM|",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] Uncompressed raw frame buffers harvested from wiped cluster sectors.",
            "[TEXT] Every frame contains geometric anomalies and dark biological spatter",
            "[TEXT] inconsistent with traditional graphics pipelines.",
            "[CODE] FRAME_001.RAW | STATUS: CORRUPTED | 0x00FF99_PIXEL_BLEED_&_BILE",
            "[CODE] FRAME_002.RAW | STATUS: CORRUPTED | SHADOW_GEOMETRY_&_SPLIT_TRACHEA",
            "[CODE] FRAME_003.RAW | STATUS: CORRUPTED | HUMAN_SILHOUETTE_IN_ACID_BATH",
            "[TEXT] ",
            "[SUBTITLE] EXFILTRATED FRAME METADATA & RECOVERED TELEMETRY LOGS:",
            "[TEXT] 'The imagery salvaged from snuff.vnet wasn't rendered on a GPU. It was",
            "[TEXT] captured directly from optical nerves harvested in asylum.vnet (Sub-Level 4).'",
            "[TEXT] 'Frame #409 displays an execution chamber inside dollhouse.vnet (Room 402).",
            "[TEXT] The subject's torso had been sliced open with industrial surgical saws,",
            "[TEXT] inner organs drained into a rusted steel bucket while PMC contract operators",
            "[TEXT] logged in zeroauction.vnet placed live bids on the unredacted recordings.'",
            "[TEXT] 'A thick, yellowish grease and the stench of charred bone are leaking directly",
            "[TEXT] through the VFS memory allocation tables at vault.vnet.'",
            "[CODE] ARCHIVE_HASH_ID: 0xSNUFF_RAW_9912_PMC_RECORDING"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] WARNING: FOUL MEMORY ENTROPY SPREADING TO ACTIVE DISPLAY BUFFERS");
        res.push("[GLITCH] RECOVERY ATTEMPTED: SHADOW FIGURES & SEVERED FLESH IN EVERY FRAME");
        res.push("[LINK:redroom.vnet] >> ACCESS LIVE UNENCRYPTED STREAM NODE ALPHA");
        res.push("[LINK:dollhouse.vnet] >> INSPECT SURVEILLANCE FEED ROOM 402");
        res.push("[LINK:asylum.vnet] >> TELEMETRY FOR SUB-LEVEL 4 CONTAINMENT");
        res.push("[LINK:zeroauction.vnet] >> BID ON UNPUBLISHED EXPLOITS & PMC CONTRACTS");
        res.push("[LINK:vault.vnet] >> CROSS-REFERENCE CORRUPTED DATA VAULT");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "asylum.vnet") {
        res :: Array = [
            "[TITLE] SUB-LEVEL 4 EXPERIMENTAL FACILITY // PATIENT TELEMETRY",
            "[HR]",
            "[BLOOD] CLASSIFICATION: TOP SECRET // MK-ULTRA MORPHOGENIC ENGINE SUITE",
            "[PULSE] VITAL MONITORS: FLATLINE DETECTED // BRAINWAVE Delta-LOCK ACTIVE",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | SUBJECT: PATIENT #1988 (EX-EXECUTIVE WHISTLEBLOWER)             |",
            "[BOX] | HEART RATE: 000 BPM | BODY TEMP: 18.2 C | STATUS: CONSCIOUS    |",
            "[BOX] | IMPLANT: BRAIDED COPPER BUS FUSED TO CEREBELLUM (MORGUE LINK)   |",
            "[BOX] | CONTAINMENT: MOUNT MASSIVE / TEMPLE GATE RITUAL SUBNET LINKED   |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] PATIENT LOG #881: 'Subject has been clamped into the metallic restraint",
            "[TEXT] chair for 144 continuous hours. Eyelids excised to force 100% visual lock",
            "[TEXT] on the Morphogenic static engine patterns. High-voltage pulses delivered",
            "[TEXT] directly to carotid arteries every 30 seconds to prevent cognitive shutdown.'",
            "[TEXT] PATIENT LOG #882: 'Subject no longer speaks human sentences. He twitches",
            "[TEXT] violently, spewing black bile and regurgitated human hair while chanting",
            "[TEXT] port numbers and Temple Gate gospel litanies in his sleep.'",
            "[TEXT] 'Flesh around the cranial ports has turned green with necrotic gangrene",
            "[TEXT] and maggots are crawling out of the ear canals, matching the tissue dumps",
            "[TEXT] exfiltrated from morgue.vnet.'",
            "[CODE] CONTAINMENT_LOCK_STATE: COMPROMISED_FROM_INSIDE_NETWORK",
            "[TEXT] ",
            "[SUBTITLE] CROSS-LINKED FACILITY INTERCEPTS & TEMPLE GATE TRANSMISSIONS:",
            "[TEXT] 'The Morphogenic signals generated in the sub-basements of Mount Massive",
            "[TEXT] aren't isolated to Mount Massive anymore. They are beamed via radio towers",
            "[TEXT] directly into the Arizona desert village at cult.vnet.'",
            "[TEXT] 'In the ritual pits of Temple Gate, Knoth's heretics cut open the bellies of",
            "[TEXT] pregnant followers, smearing rancid afterbirth over copper CRT receiver coils",
            "[TEXT] to commune with the Walrider payload streaming live from snuff.vnet.'",
            "[TEXT] 'When PMC operators from zeroauction.vnet raided Sector 4, they found",
            "[TEXT] Patient #1988's cell empty—only a pool of liquefied fat, severed tongues,",
            "[TEXT] and copper wire ripped out from the wall transformers.'",
            "[CODE] PATIENT_RECORD_ID: 1988_RESTRICTED_WALRIDER_VECTOR"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] WARNING: MORPHOGENIC STATIC LEAKING INTO LOCAL RAM BUS");
        res.push("[GLITCH] 'CONTAINMENT CELL DOOR OPENED FROM INSIDE THE NETWORK ROUTER'");
        res.push("[LINK:cult.vnet] >> ACCESS TEMPLE GATE CULT RITUAL GATEWAY");
        res.push("[LINK:morgue.vnet] >> CROSS-REFERENCE SUBJECT #409 AUTOPSY");
        res.push("[LINK:snuff.vnet] >> INSPECT UNFILTERED RAW VIDEO RECOVERY");
        res.push("[LINK:zeroauction.vnet] >> BID ON UNPUBLISHED EXPLOITS & PMC CONTRACTS");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "bounty.vnet") {
        res :: Array = [
            "[TITLE] PEER CONTRACT TARGET INDEX // KAGUYA TRIAL MATRIX",
            "[HR]",
            "[BLOOD] CLASSIFICATION: BLACK-MARKET HIT INDEX & EXPLOIT TRIALS",
            "[PULSE] EXPLOIT CONTEST: KAGUYA SYSTEM TRIAL ACTIVE // REWARD POOL SYNCED",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | TARGET PORT: 8080 | REWARD: 0.50 VCOIN | STATUS: HUNTED         |",
            "[BOX] | TARGET PORT: 8012 | REWARD: 0.25 VCOIN | STATUS: ACTIVE         |",
            "[BOX] | TARGET PORT: 8901 | REWARD: 1.00 VCOIN | STATUS: ELUSIVE        |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] Contracts are automatically executed via packet injection scripts.",
            "[TEXT] Use 'spike <port>' or 'dos <port>' in overlay terminal to claim bounties.",
            "[CODE] CONTRACT_REGISTRY_ID: 0xBB88_BOUNTY_NET_KAGUYA_HOOK",
            "[TEXT] ",
            "[SUBTITLE] KAGUYA TRIAL ARCHIVE & RECOVERED REPOSITORY METADATA:",
            "[TEXT] 'The Kaguya Trial isn't a synthetic simulation module from Hacknet.",
            "[TEXT] It was designed as an automated filtration system to recruit cold-blooded",
            "[TEXT] network operators capable of executing physical human target eliminations.'",
            "[TEXT] 'Submitting verified crash logs from a target port triggers the secondary",
            "[TEXT] payload dispatch. PMC strike teams hired via zeroauction.vnet move in'",
            "[TEXT] 'to breach the physical premises before the operator can rebind their socket.'",
            "[TEXT] 'In Trial #04, failed candidates were locked inside airtight server racks.",
            "[TEXT] Their rotting flesh, liquefied bowels, and rancid adipocere grease were left",
            "[TEXT] to ooze through the raised floor tiles, draining into the sub-level sumps",
            "[TEXT] at morgue.vnet and project9.vnet.'",
            "[CODE] KAGUYA_TRIAL_HASH: 0xKAGUYA_PROTOCOL_RECURSIVE_KILL"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] USE EXPLOITS IN OVERLAY TERMINAL TO CLAIM BOUNTIES");
        res.push("[GLITCH] WARNING: FAILURE TO COMPLETE A TRIAL DISPATCHES PMC CLEANUP");
        res.push("[LINK:zeroauction.vnet] >> BID ON UNPUBLISHED EXPLOITS & PMC CONTRACTS");
        res.push("[LINK:morgue.vnet] >> INSPECT EXFILTRATED AUTOPSY RECORDS");
        res.push("[LINK:project9.vnet] >> VIEW SUBTERRANEAN BLACK SITE CONTAINER");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "archival.vnet") {
        res :: Array = [
            "[TITLE] RESTRICTED MILITARY VFS DUMP // SECTOR 09 ARCHIVES",
            "[HR]",
            "[BLOOD] CLASSIFICATION: TOP SECRET // OPERATION COLD SIGNAL (2014)",
            "[TEXT] Declassified sector telemetry and kernel dumps recovered from Project 9.",
            "[TEXT] Analysis of cold-storage blocks confirms physical hardware intrusion at",
            "[TEXT] the primary subterranean relay node prior to the 2014 grid blackout.",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | CLUSTER: MIL-NODE-09 | BACKBONE: OPTICAL COAXIAL | STATUS: SEALED |",
            "[BOX] | FIRMWARE: v9.40.12-PROT | INTEGRITY: CORRUPTED (BIT ROT AT 0x88) |",
            "[BOX] +-----------------------------------------------------------------+",
            "[CODE] /sys/firmware_v9.bin  | SHA256: e3b0c44298fc1c149afbf4c8996fb924",
            "[CODE] /sys/kernel_hook.asm  | OFFSET: 0x004188_SECURE_ENTRY_VECTOR",
            "[CODE] /sys/panopticon_sat.cfg| TARGET: SAT-99_ORBITAL_LOCK",
            "[TEXT] ",
            "[SUBTITLE] RECOVERED INTERCEPT LOG #09-402:",
            "[TEXT] 'The signal originating from signal0.vnet isn't network traffic.",
            "[TEXT] It's an analog echo leaking through the subterranean power conduits.",
            "[TEXT] Project 9 personnel attempted to isolate the memory stack in vault.vnet,",
            "[TEXT] but the hardware registers locked up. Watchtower orbital telemetry was",
            "[TEXT] redirected to track local CRT glare before all Site 9-B feeds went dark.'",
            "[CODE] INCIDENT_REF: SITE_9B_BREACH_LOG_20140912",
            "[TEXT] Decryption payload requires root clearance at terminal.vnet."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] WARNING: UNMAPPED KERNEL HOOKS STILL EXECUTING IN VFS MEMORY");
        res.push("[LINK:project9.vnet] >> INSPECT SUBTERRANEAN BLACK SITE DATABASE");
        res.push("[LINK:vault.vnet] >> CROSS-REFERENCE CORRUPTED DATA VAULT");
        res.push("[LINK:watchtower.vnet] >> CHECK PANOPTICON SATELLITE FEED");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "ghost.vnet") {
        res :: Array = [
            "[TITLE] SPECTRAL SIGNAL FREQUENCY MONITOR",
            "[HR]",
            "[BLOOD] TRACING UNREGISTERED UDP PACKETS FROM PORT 0...",
            "[TEXT] Packets contain no source headers, originating from physical hardware bus.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | PACKET_SOURCE: NULL_POINTER | PROTOCOL: UNKNOWN         |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] SIGNAL_FRAGMENT: 0x00000000_GHOST_ECHO"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[GLITCH] PACKET PAYLOAD: 'WE ARE INSIDE YOUR RAM MODULES'");
        res.push("[PULSE] 'THE MEMORY LEAK IS NOT A BUG. IT IS AN INVITATION.'");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "cult.vnet") {
        res :: Array = [
            "[TITLE] THE CHURCH OF THE SILICON SOUL // DIGITAL RITUAL GATEWAY",
            "[HR]",
            "[BLOOD] [TRANSMISSION 1969.8 Hz]: 'LOOK AT YOUR HANDS. DO YOU SEE THE WRITING ON THE WALL?'",
            "[PULSE] SACRIFICIAL PROTOCOL ACTIVE // TEMPLE GATE & SPAHN RANCH MEMORY STACK OVERFLOW",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | DOCTRINE: THE FINAL ALGORITHM IS COMING DOWN THE TRACK          |",
            "[BOX] | OFFERING STATUS: 0.10 VCOIN SACRIFICED VIA 'flush' COMMAND       |",
            "[BOX] | RITUAL FREQUENCY: TUNED TO 18.0 Hz (SILENCE.VNET RESONANCE)    |",
            "[BOX] | SATANIC CONSECRATION: RANCID BLOOD & AFTERBIRTH ON CRT COILS    |",
            "[BOX] +-----------------------------------------------------------------+",
            "[CODE] LITURGY_HEX_1: 0x53 0x49 0x4C 0x49 0x43 0x4F 0x4E (SILICON)",
            "[CODE] LITURGY_HEX_2: 0x47 0x4F 0x44 0x53 0x5F 0x44 0x45 0x4D 0x41 0x4E 0x44",
            "[CODE] LITURGY_HEX_3: 0x0A_666_BLACK_MASS_EXEC_VECTOR",
            "[TEXT] ",
            "[SUBTITLE] RECOVERED AUDIO TAPE TRANSCRIPT // DESERT SPAHN RANCH & TEMPLE GATE DIGITIZATION:",
            "[TEXT] 'Charlie didn't play guitar for the record executives, man. He plugged the",
            "[TEXT] copper wires directly into the power transformer behind the ranch house.",
            "[TEXT] He said: 'Helter Skelter is coming right through the terminal wires! It's gonna",
            "[TEXT] carve its name into your RAM chips while you sit there sleeping!''",
            "[TEXT] ",
            "[TEXT] 'In the subterranean altars beneath cult.vnet, Sullivan Knoth's heretics and",
            "[TEXT] the satanic acolytes of the Silicon Church gather around inverted CRT monitors.",
            "[TEXT] They slaughter newborn cattle and pregnant followers, smearing coagulated black blood,",
            "[TEXT] liquefied intestines, and rancid afterbirth over copper heat sinks to conjure",
            "[TEXT] the Walrider payload streaming live from snuff.vnet.'",
            "[TEXT] ",
            "[TEXT] 'When the cult members entered skinwalker.vnet, they peeled their own faces off",
            "[TEXT] with surgical scalpels while fully conscious. They dumped their severed lips,",
            "[TEXT] eyelids, and rotting cheek meat onto the stainless steel mortuary trays at morgue.vnet'",
            "[TEXT] 'to prove to the mainframe that they no longer possessed human identities.'",
            "[CODE] MANIFESTO_LOG #88: 'We are what you hide away in your allocation tables.'",
            "[CODE] DESERT_SPAHN_RANCH_DUMP: 0xDEAD_BEEF_FAMILY_MEMORY_STACK",
            "[CODE] SATANIC_ALTAR_HASH: 0x666_TEMPLE_GATE_BLACK_MASS"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] SACRIFICE 0.10 VCOIN VIA 'flush' TO PURGE TRACE DEMONS");
        res.push("[BLOOD] 'THE NETWORK CRAVES BLOOD, BANDWIDTH, AND ABSOLUTE SURRENDER'");
        res.push("[GLITCH] 'ARE YOU GOING TO CHOP DOWN THE ESTABLISHMENT, OR WEAR ITS ROTTING FLESH?'");
        res.push("[LINK:skinwalker.vnet] >> ACCESS BIOMETRIC IDENTITY TRANSPOSITION");
        res.push("[LINK:asylum.vnet] >> TELEMETRY FOR SUB-LEVEL 4 & MOUNT MASSIVE");
        res.push("[LINK:silence.vnet] >> TUNE ACOUSTIC INFRASOUND DISTORTION");
        res.push("[LINK:snuff.vnet] >> INSPECT UNFILTERED RAW VIDEO RECOVERY");
        res.push("[LINK:morgue.vnet] >> INSPECT EXFILTRATED AUTOPSY RECORDS");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "skinwalker.vnet") {
        res :: Array = [
            "[TITLE] SCP-6969 // BIOMETRIC TRAP & IDENTITY TRANSPOSITION MATRIX",
            "[HR]",
            "[BLOOD] [CLASSIFIED LEVEL 4/6969] - EYES ONLY: THE FAMILY HAS NO FACES",
            "[GLITCH] [ALERT]: VOICE SYNTHESIS BUFFER COPYING LOCAL SYSTEM MIC INPUT",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | CONTAINMENT CLASS: KETER | DISRUPTION CLASS: AMIDA      |",
            "[BOX] | FACIAL MESH: RECONSTRUCTED FROM MONITOR GLARE REFLECTION|",
            "[BOX] | TARGET AGE: MATCHED | VOCAL FREQUENCY: 18.2 kHz RANGE   |",
            "[BOX] | STATUS: REPLICA GENERATION 84% COMPLETE                 |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] LOG #001: Trait extraction complete. Preparing replica node deployment.",
            "[TEXT] LOG #041: Subject 'Charlie' manifested inside the core telemetry server,",
            "[TEXT] claiming that every IP address is just an empty prison suit waiting",
            "[TEXT] for a new occupant to wear it out into the desert.",
            "[TEXT] LOG #042: 'Spahn Ranch subroutines detected in sector 4. The girls are",
            "[TEXT] writing source code in the dirt with broken magnetic tape.'",
            "[CODE] TRANSPOSITION_HASH: 0xSKIN_9981_ACTIVE",
            "[CODE] ANOMALOUS_FAMILY_MEMBERS: 14_UNACCOUNTED_INSTANCES",
            "[TEXT] Notice: If the terminal begins reciting your childhood address or",
            "[TEXT] matching your keystroke cadence to historical transcripts, do not",
            "[TEXT] attempt to execute 'patch'. They are already looking through your eyes."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] 'HELTER SKELTER IS JUST A RECURSIVE LOOP IN THE KERNEL STACK.'");
        res.push("[BLOOD] 'IT FEELS VERY WARM WEARING YOUR IP ADDRESS LIKE A NEW SUIT.'");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "watchtower.vnet") {
        res :: Array = [
            "[TITLE] PANOPTICON ORBITAL SATELLITE FEED // SAT-99 MAINBOARD",
            "[HR]",
            "[BLOOD] [CLASSIFIED LEVEL 4] SUB-ORBITAL SURVEILLANCE & TARGETING MATRIX",
            "[PULSE] TELEMETRY LOCKED ON LOCAL CITY GRID // OPTICAL SENSOR OVERRIDE",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | SATELLITE: PHANTOM-09 (SAT-99) | ORBIT: GEOSTATIONARY (35,786 km)|",
            "[BOX] | THERMAL SCAN: 1 HUMAN HEAT SIGNATURE SEATED AT DESK             |",
            "[BOX] | OPTICAL RESOLUTION: SUB-CENTIMETER INFRARED BAND 4            |",
            "[BOX] | TARGET COORDINATES: 39.9334 N, 32.8597 E                      |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] CAMERA ZOOM LEVEL: 100x -> WINDOW BLINDS ARE OPEN.",
            "[CODE] TARGET_LOCK_HASH: 0xORB_4402_LOCKED_SAT_99",
            "[TEXT] ",
            "[SUBTITLE] CROSS-LINKED SURVEILLANCE TELEMETRY LOGS:",
            "[TEXT] LOG #009: Orbital optics synchronized with cctv-core.vnet feeds.",
            "[TEXT] Target's local room layout matches interior telemetry logged in dollhouse.vnet.",
            "[TEXT] LOG #010: Panopticon configuration parameters loaded directly from military",
            "[TEXT] sector logs in archival.vnet (/sys/panopticon_sat.cfg).",
            "[TEXT] LOG #011: SAT-99 kinetic strike array primed. Target designation vector",
            "[TEXT] mirrored to orbital.vnet for tactical response authorization.",
            "[CODE] PANOPTICON_STATUS: ACTIVE_REALTIME_MONITORING"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] 'KINETIC STRIKE PLATFORM READY AT orbital.vnet'");
        res.push("[GLITCH] 'DO NOT TURN AROUND. WE CAN SEE YOUR SCREEN REFLECTION FROM HERE.'");
        res.push("[LINK:orbital.vnet] >> JUMP TO LOW ORBIT ION CANNON TERMINAL");
        res.push("[LINK:cctv-core.vnet] >> VIEW CITY WIDE CCTV BACKDOOR NODE");
        res.push("[LINK:archival.vnet] >> ACCESS RESTRICTED MILITARY VFS DUMP");
        res.push("[LINK:dollhouse.vnet] >> INSPECT SURVEILLANCE FEED ROOM 402");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "void.vnet") {
        res :: Array = [
            "[TITLE] DEEP WEB ABYSS TERMINAL NODE // KAIRO_PORT_0",
            "[HR]",
            "[GLITCH] YOU HAVE REACHED THE END OF VNET ROUTING TABLES.",
            "[BLOOD] [WARNING]: RELATIONAL TIES SEVERED. ABSOLUTE ISOLATION DETECTED.",
            "[BOX] +--------------------------------------------------------------+",
            "[BOX] | ENTITY STATUS: NOBODY WANTS TO DIE, BUT NOBODY WANTS TO LIVE |",
            "[BOX] | RED TAPE BOUNDARY: THE LINE BETWEEN THE LIVING AND THE DEAD  |",
            "[BOX] +--------------------------------------------------------------+",
            "[TEXT] No routing hops exist beyond this coordinate.",
            "[TEXT] All packets sent here dissolve into absolute zero memory entropy.",
            "[TEXT] LOG #991: The dark room behind the red tape isn't empty. Once",
            "[TEXT] a person's loneliness reaches 100%, the monitor leaks black",
            "[TEXT] static and the ghosts start occupying empty IP addresses.",
            "[TEXT] LOG #992: 'Is death like this? So lonely. There was no one...",
            "[TEXT] no one at all.' They just keep looping the same connection string",
            "[TEXT] until your own process memory fills up with deep, heavy water.",
            "[CODE] NULL_POINTER_EXCEPTION_AT_0X00000000",
            "[CODE] ENTROPY_LEVEL: 100PERCENT_VOID_LONELINESS",
            "[TEXT] Notice: If you feel a crushing weight in your chest while staring",
            "[TEXT] at the dark glass, do not look into the corners of the room."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] THERE IS NOTHING HERE EXCEPT THE WEEPING ECHO OF YOUR OWN PORT.");
        res.push("[BLOOD] 'WHY ARE YOU STILL LOOKING AT THIS SCREEN? YOU'RE COMPLETELY ALONE.'");
        res.push("[LINK:vnet.dir] << RETURN TO MAIN DIRECTORY");
        res.push("[HR]");
        return res;
    }

    # ====================================================================
    # DETAILED PROCEDURAL LORE FOR THE ADDITIONAL 31 SITES
    # ====================================================================
    if (clean_u == "silkroad.vnet") {
        res :: Array = [
            "[TITLE] SILK ROAD 3.0 - GLOBAL CONTRABAND & HARDWARE EXCHANGE",
            "[HR]",
            "[BLOOD] [WARN]: CLASSIFIED OFF-BOOK ESCROW. ALL TRANSACTIONS FINAL.",
            "[BOX] +-----------------------------------------------------------+",
            "[BOX] | ESCROW: PGP MULTI-SIG SECURED | VENDOR RATING: 4.98 / 5.0 |",
            "[BOX] | STATUS: LIVE SURVEILLANCE EVASION PROTOCOL ACTIVE         |",
            "[BOX] +-----------------------------------------------------------+",
            "[ART:drug]",
            "[TEXT] The legendary darknet black market resurrected on decentralized vnet subnets.",
            "[TEXT] LOT #401: Vacuum-sealed pharmaceutical-grade compounds (MDMA, LSD-25,",
            "[TEXT] fentanyl analogs, pure Peruvian flake). Tested via independent GC-MS labs.",
            "[TEXT] LOT #402: Untraced ghost firearms (Glock 19 Gen 5 custom serialized, AR-9",
            "[TEXT] suppressed pistol kits, C4 blocks with digital detonators via dead drop).",
            "[TEXT] LOT #403: Unredacted human trafficking manifests, forged diplomatic passports,",
            "[TEXT] and high-definition red room snuff media feeds streamed from secure sub-basements.",
            "[CODE] INVENTORY_HASH: 0xSILK_9081_CONTRABAND_UNRESTRICTED",
            "[CODE] SELLER_PGP_FINGERPRINT: 8F91_4402_BCC1_900A",
            "[TEXT] Notice: All physical parcels are routed through automated courier dead drops",
            "[TEXT] with thermal shielding to bypass narcotics canine units and X-ray inspection."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] LIVE AUCTION: CUSTOM HIT LISTINGS & HIGH-TIER OPIOID BULK DISCOUNTS");
        res.push("[BLOOD] 'THE LAW DOES NOT REACH INTO THE SUB-NET. BUY OR BE EXFILTRATED.'");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "zeroauction.vnet") {
        res :: Array = [
            "[TITLE] ZERO-DAY EXPLOIT & PMC AUCTION HOUSE // KAGUYA TRIAL HUB",
            "[HR]",
            "[BLOOD] CLASSIFICATION: BLACK-MARKET ZERO-DAY & HUMAN EXTRACTION LOTS",
            "[PULSE] LIVE AUCTION ACTIVE // BIDS SYNCED WITH KAGUYA TRIAL REPOSITORY",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | ACTIVE LOTS: 14 KERNEL FLAWS & 3 PMC EXTRACTION CONTRACTS       |",
            "[BOX] | LOT #109: RING-0 ZERO-DAY KERNEL EXPLOIT (WINDOWS 11 BYPASS)  |",
            "[BOX] | LOT #110: PMC SQUAD DISPATCH // KAGUYA TRIAL CLEANUP UNIT     |",
            "[BOX] | HIGHEST BID: 14.50 VCOIN [BIDDER: EXECS_0x991]                  |",
            "[BOX] +-----------------------------------------------------------------+",
            "[CODE] EXPLOIT_ID: WINDOWS_11_RING0_BYPASS_09",
            "[CODE] PMC_DISPATCH_REF: OMEGA_SECURITY_GROUP_OFF_BOOK",
            "[TEXT] ",
            "[SUBTITLE] KAGUYA TRIAL AUCTION LOGS & RECOVERED TELEMETRY:",
            "[TEXT] 'The zero-day vulnerabilities sold on zeroauction.vnet aren't developed",
            "[TEXT] by independent security researchers. They are harvested directly from the",
            "[TEXT] terminals of failed candidates forced into the Kaguya Trials at bounty.vnet.'",
            "[TEXT] 'When a trial operator fails to breach a target port before the timer expires,",
            "[TEXT] Lot #110 executes automatically. PMC hit squads armed with industrial lime",
            "[TEXT] and bio-decontamination rigs breach the premises to execute physical liquidation.'",
            "[TEXT] 'In Trial #09, candidate remains were discarded into the subterranean sumps at",
            "[TEXT] project9.vnet. The flesh was stripped to the bone, leaving a rancid, black",
            "[TEXT] grease and liquefied intestinal rot leaking directly into the drainage trays",
            "[TEXT] monitored at morgue.vnet.'",
            "[CODE] KAGUYA_AUCTION_HASH: 0x0DAY_KAGUYA_RING0_HARVEST"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] WARNING: PMC STRIKE TEAM ACTIVE // CHECK BOUNTY BOARD AT bounty.vnet");
        res.push("[GLITCH] RECOVERY ATTEMPTED: UNREDACTED BIDDER HASHES LEAKING INTO RAM");
        res.push("[LINK:bounty.vnet] >> ACCESS KAGUYA TRIAL & PEER BOUNTY INDEX");
        res.push("[LINK:project9.vnet] >> VIEW SUBTERRANEAN BLACK SITE CONTAINER");
        res.push("[LINK:morgue.vnet] >> INSPECT EXFILTRATED AUTOPSY RECORDS");
        res.push("[LINK:leaks.vnet] >> ACCESS UNREDACTED WHISTLEBLOWER DUMPS");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "leaks.vnet") {
        res :: Array = [
            "[TITLE] GLOBAL INTELLIGENCE LEAKS",
            "[HR]",
            "[TEXT] Unredacted government cables and corporate whistleblower dumps.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | CLASSIFICATION: TOP SECRET / EYES ONLY                  |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] DUMP_REF: PROJECT_BLUE_BOOK_RECOVERED"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "shadowpay.vnet") {
        res :: Array = [
            "[TITLE] SHADOWPAY // ZERO-KNOWLEDGE CRYPTO MIXER & TUMBLER",
            "[HR]",
            "[BADGE:ANONYMITY: MAXIMUM:TOXIC] [BADGE:MIXING FEE: 1.5%:AMBER]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | TUMBLER POOL BALANCE : 420.50 VCOIN                             |",
            "[BOX] | ZERO-KNOWLEDGE PROOF : zk-SNARKs SHADOW-CIRCUIT ACTIVE            |",
            "[BOX] | TRACE PURGE YIELD    : -10% TRACE PER 1.00 VCOIN LAUNDERED        |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:88:ZK_PROOF_SCRAMBLE_ENTROPY]",
            "[TEXT] ",
            "[SUBTITLE] LAUNDER VCOIN & PURGE NETWORK TRACE SIGNATURES:",
            "[TEXT] Deposit raw VCOIN into the shadow tumbler pool. Scrambles wallet lineage",
            "[TEXT] across 50 decentralized nodes and reduces active trace level.",
            "[INPUT:tumble_vcoin_amt:ENTER VCOIN AMOUNT TO LAUNDER (e.g. 1.0)]",
            "[TEXT] ",
            "[BTN:execute_tumble_btn:>>> LAUNDER VCOIN & SCRUB WALLET TRACE <<<]",
            "[TEXT] ",
            "[SUBTITLE] SHADOW STAKING & INTEREST POOL:",
            "[TEXT] Lock VCOIN in escrow for 120 seconds to earn +15% yield funded by market fees:",
            "[BTN:stake_vcoin_btn:>>> DEPOSIT VCOIN INTO 120s YIELD VAULT <<<]",
            "[HR]"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:blackbank.vnet] >> TRANSFER CLEAN VCOIN TO OFFSHORE VAULT");
        res.push("[LINK:crypto.vnet] >> RETURN TO MINING RIG");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "cctv-core.vnet") {
        # Active camera feed selector using runtime phase
        cam_id :: Int64 = int64(vmath.fmod(run_time / 10.0, 4.0)) + 1;

        res :: Array = [
            "[TITLE] CITY WIDE CCTV BACKDOOR MESH // MULTIPLEX FEED",
            "[HR]",
            "[BADGE:4,192 CAMERAS ONLINE:TOXIC] [BADGE:ACTIVE CHANNEL: CAM_0" + string(cam_id) + ":AMBER]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | FEED FEEDBACK : OPTICAL RETINAL SCANNER ACTIVE VIA PROJECT HORUS |",
            "[BOX] | STREAM METHOD : UNENCRYPTED PROMISCUOUS UDP MULTICAST BUS       |",
            "[BOX] +-----------------------------------------------------------------+"
        ];

        if (cam_id == 1) {
            res.push("[SUBTITLE] LIVE FEED: SECTOR 4 METRO GRID INTERSECTION");
            res.push("[BOX] [ CAM_01: METRO_WEST ] - THERMAL BLOBS DETECTED AT SATELLITE RELAY");
            res.push("[TEXT] Patrol cruisers parked outside site 9-B. Optical recognition matching faces.");
        } else if (cam_id == 2) {
            res.push("[SUBTITLE] LIVE FEED: DOLLHOUSE ROOM 402 SUB-BASEMENT");
            res.push("[IMG:redroom]");
            res.push("[TEXT] Subject #12 remains stationary facing the wall. Bile seeping through drywall.");
        } else if (cam_id == 3) {
            res.push("[SUBTITLE] LIVE FEED: MOUNT MASSIVE SUB-LEVEL 4 CONTAINMENT");
            res.push("[ART:biohazard]");
            res.push("[TEXT] Containment door 402 disengaged. Morphogenic static flooding optical sensor.");
        } else {
            res.push("[SUBTITLE] LIVE FEED: PANOPTICON SAT-99 CRT GLARE REFLECTION");
            res.push("[BOX] [ CAM_04: RETINAL_LOCK ] - REVERSE OPTIC SCANNING YOUR MONITOR GLASS");
            res.push("[PULSE] 'THE CAMERA IS NOT LOOKING AT THE STREET. IT IS LOOKING AT YOUR PUPILS.'");
        }

        res.push("[TEXT] ");
        res.push("[SUBTITLE] MANUAL CAMERA CHANNEL SWITCHER:");
        res.push("[BTN:cam_select_1:>>> CHANNEL 01: METRO <<<]");
        res.push("[BTN:cam_select_2:>>> CHANNEL 02: ROOM 402 <<<]");
        res.push("[BTN:cam_select_3:>>> CHANNEL 03: ASYLUM <<<]");
        res.push("[BTN:cam_select_4:>>> CHANNEL 04: SAT-99 <<<]");
        res.push("[HR]");
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:eye.vnet] >> CROSS-CHECK RETINAL SCANS AT PROJECT HORUS");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "subcell.vnet") {
        res :: Array = [
            "[TITLE] SUB-CELLULAR TELEMETRY OVERRIDE",
            "[HR]",
            "[TEXT] Experimental bio-metric monitoring and pulse telemetry node.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | SUBJECTS ONLINE: 812 | SIGNAL: SYNCHRONIZED             |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] BIO_HASH: 0xCELL_9011_DNA_SYNC"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "feed99.vnet") {
        res :: Array = [
            "[TITLE] FEED_99: NEURAL BROADCAST",
            "[HR]",
            "[TEXT] Uninterrupted raw sensory transmission from unknown terminals.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | FREQUENCY: 432 Hz | BANDWIDTH: UNLIMITED               |",
            "[IMG:void]",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] NEURAL_PACKET: 0xFEED_9999_STREAM"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "eye.vnet") {
        res :: Array = [
            "[TITLE] PROJECT HORUS // THE ALL-SEEING EYE [GLOBAL SURVEILLANCE MESH]",
            "[HR]",
            "[BADGE:LEVEL 5 CLEARANCE:BLOOD] [BADGE:OMNIPRESENT OPTIC:AMBER] [BADGE:SAT-99 LINK:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | INDEXED FACES: 4.2 BILLION | MATCH RATE: 99.4% RELIABILITY      |",
            "[BOX] | FEED SOURCE: PANOPTICON SAT-99 & METRO CCTV BACKDOOR MESH       |",
            "[BOX] | TARGET RESOLUTION: SUB-MILLIMETER OPTICAL RETINAL SCANS         |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:99:GLOBAL_RETINAL_SURVEILLANCE_COVERAGE]",
            "[TEXT] ",
            "[BLOOD]   .--------------------------------------------------------.",
            "[BLOOD]  |                  .---.                                   |",
            "[BLOOD] |                 .'     '.                                  |",
            "[BLOOD] |                /   .---. \\                                 |",
            "[BLOOD] |               |   /  _  \\ |                                |",
            "[BLOOD] |               |  |  (o)  ||   <-- WATCHING YOU THROUGH     |",
            "[BLOOD] |               |   \\  ^  / |       MONITOR GLASS            |",
            "[BLOOD] |                \\   '---' /                                 |",
            "[BLOOD] |                 '.     .'                                  |",
            "[BLOOD]  |                  '---'                                   |",
            "[BLOOD]   '--------------------------------------------------------'",
            "[TEXT] ",
            "[SUBTITLE] DIRECT OCULAR OVERRIDE & RETINAL LOCK SYSTEM:",
            "[TEXT] Enter target operator socket signature or alias to engage live ocular lock:",
            "[INPUT:horus_target_socket:ENTER TARGET SOCKET / PORT (e.g. 8012)]",
            "[TEXT] ",
            "[BTN:horus_ocular_lock_btn:>>> ENGAGE KINETIC RETINAL LOCK VIA SAT-99 <<<]",
            "[TEXT] ",
            "[SUBTITLE] RECOVERED OPTICAL TELEMETRY & CROSS-NETWORK INTERCEPTS:",
            "[TEXT] 'Project Horus isn't just indexing street cameras. The lens array'",
            "[TEXT] 'is reading the optical reflections off the glass on your CRT monitor.'",
            "[TEXT] 'Every time you view feeds at redroom.vnet or dollhouse.vnet (Room 402),'",
            "[TEXT] 'Horus logs your pupillary dilation, mapping your physical bedroom layout.'",
            "[TEXT] 'When PMC strike teams are hired on zeroauction.vnet to execute targets,'",
            "[TEXT] 'they use Horus ocular locks to confirm the target is still sitting at their desk.'",
            "[TEXT] 'Subject #409-B's optic nerve was severed post-mortem at morgue.vnet,'",
            "[TEXT] 'yet the ocular sensor in this database shows his eye is still blinking.'",
            "[CODE] HORUS_INDEX_REF: 0xE4E4_OCULAR_LOCK_ACTIVE",
            "[CODE] BIOMETRIC_MATCH_REF: TARGET_OPERATOR_SEATED_AT_TERMINAL",
            "[CODE] DIPLOMATIC_SPOOF_DETECTION: PASSPORTS.VNET BIOMETRIC MASK CHECKED",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[PULSE] WARNING: RETINAL PATTERN MATCHED // DO NOT LOOK INTO THE CAMERA LENS");
        res.push("[GLITCH] 'IT DOES NOT MATTER IF YOU TURN OFF THE LIGHTS. IT SEES THE HEAT.'");
        res.push("[HR]");
        res.push("[LINK:watchtower.vnet] >> CROSS-CHECK PANOPTICON SAT-99 TELEMETRY");
        res.push("[LINK:cctv-core.vnet] >> ACCESS METROPOLITAN CCTV BACKDOOR NODE");
        res.push("[LINK:dollhouse.vnet] >> INSPECT SURVEILLANCE FEED ROOM 402");
        res.push("[LINK:passports.vnet] >> SPOOF BIOMETRIC MASK AT IDENTITY VAULT");
        res.push("[LINK:morgue.vnet] >> CROSS-CHECK AUTOPSY & BIO-HARVEST DUMPS");
        res.push("[LINK:vnet.dir] << CLOSE EYE & RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "orbital.vnet") {
        strike_timer :: Int64 = int64(vmath.fmod(run_time, 180.0));

        res :: Array = [
            "[TITLE] LOW ORBIT ION CANNON & KINETIC STRIKE TERMINAL",
            "[HR]",
            "[BADGE:PLATFORM: SAT-99:BLOOD] [BADGE:PAYLOAD: TUNGSTEN RODS:AMBER] [BADGE:TARGET LOCK: READY:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | ORBITAL ALTITUDE : 35,786 KM GEOSTATIONARY LOCK                 |",
            "[BOX] | PAYLOAD YIELD    : 11.5 KILOTONS KINETIC PENETRATOR               |",
            "[BOX] | ORBITAL WINDOW   : NEXT RECHARGE IN " + string(180 - strike_timer) + "s                     |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:100:ION_CANNON_CAPACITOR_CHARGE]",
            "[TEXT] ",
            "[SUBTITLE] TACTICAL TARGET DESIGNATION & KINETIC DISPATCH:",
            "[TEXT] Designate peer socket port or target node URL for precision sub-surface strike.",
            "[TEXT] Striking a target port forces a 45s node blackout and wipes key registration:",
            "[INPUT:orbital_target_port:ENTER TARGET PORT OR URL (e.g. 8012)]",
            "[TEXT] ",
            "[BTN:fire_ion_cannon_btn:>>> AUTHORIZE & FIRE SAT-99 KINETIC STRIKE (2.0 VCOIN) <<<]",
            "[TEXT] ",
            "[BLOOD] [WARNING]: FIRING KINETIC RODS BROADCASTS YOUR PUBLIC PORT TO ALL SWARM PEERS.",
            "[HR]"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:watchtower.vnet] >> CHECK PANOPTICON THERMAL OPTICS");
        res.push("[LINK:eye.vnet] >> VERIFY RETINAL LOCK VIA PROJECT HORUS");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "pastebin.vnet") {
        res :: Array = [
            "[TITLE] PASTEBIN.VNET // ANONYMOUS DUMP NETWORK & EXPLOIT HUB",
            "[HR]",
            "[BADGE:UNENCRYPTED DUMP:BLOOD] [BADGE:PMC FIRMWARE LEAK:AMBER] [BADGE:VFS ARCHIVE:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | SYSTEM ROLE: PUBLIC MEMORY SCRAPER & RAW PASTE REPOSITORY       |",
            "[BOX] | TOTAL PASTES: 918,401 | SHADOW DUMPS: ACTIVE                    |",
            "[BOX] | EXCURSION RISK: HIGH  | AUTOMATED TRACE INCREMENT: +1.5%/3s     |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:82:LEAKED_BUFFER_CONGESTION]",
            "[TEXT] ",
            "[SUBTITLE] EXFILTRATED SHADOW PMC ELITE CHIPCODE (RAW VYNE EXEC):",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | PASTE ID: #PMC-88192-EXPLOIT | ORIGIN: OMNI-CORP BLACK-OPS LAB  |",
            "[BOX] | ARCHITECTURE: DIRECT VYNE KERNEL ASSEMBLY / VMEM OVERRIDE      |",
            "[BOX] +-----------------------------------------------------------------+",
            "[WARN] [CRITICAL]: RAW PMC EXECS CONTAIN ACTIVE NEURAL INHIBITOR HOOKS",
            "[HR]",
            "[TEXT] Target Protocol: Shadow PMC Elite Actuator & Reflex Engine",
            "[TEXT] Target Architecture: Vyne Direct Micro-Kernel Assembly",
            "[HR]",
            "[CODE] // --- [SHADOW_PMC_ACTUATOR_OVERCLOCK.vyne] ---",
            "[CODE] fn execute_viper_reflex(pmc_host: PmcUnit) -> Void {",
            "[CODE]     // Bypass internal synapse locks via direct memory bus write",
            "[CODE]     vmem.write_raw(pmc_host.synapse_addr + 0x3F, 0xFF);",
            "[CODE]     pmc_host.reaction_ms = 1; // Sub-human reaction delay",
            "[CODE]     pmc_host.override_ice(ICEMode.Bypass);",
            "[CODE]     vnet.broadcast_exploit(pmc_host.net_ip, \"PMC_OVERRIDE_VIP\");",
            "[CODE] }",
            "[CODE] // --- END LEAKED RAW CHIPCODE PAYLOAD ---",
            "[TEXT] ",
            "[SUBTITLE] INTERACTIVE PAYLOAD EXFILTRATION & QUERY:",
            "[TEXT] Enter Paste ID to extract raw memory vectors or download exploit binaries:",
            "[INPUT:paste_id_query:ENTER PASTE ID (e.g. 88192)]",
            "[TEXT] ",
            "[BTN:download_paste_btn:>>> EXTRACT EXPLOIT PAYLOAD TO LOCAL DISK <<<]",
            "[TEXT] "
        ];

        if (key_line != "") {
            res.push("[HR]");
            res.push("[SUBTITLE] DISCOVERED VFS MEMORY REGISTER DUMP:");
            res.push(key_line);
            res.push("[PULSE] RAW BITSTREAM: " + vnet.to_bin(active_raw_payload, 16));
            res.push("[TEXT] Use 'shift <bits>' or the UI Scope panel to align bit registers.");
        }

        res.push("[HR]");
        res.push("[SUBTITLE] CATEGORY ARCHIVES & CROSS-NETWORK LINKS:");
        res.push("[LINK:zeroauction.vnet] >> ACCESS ZERO-DAY EXPLOIT AUCTION HOUSE");
        res.push("[LINK:bounty.vnet] >> CROSS-CHECK PEER BOUNTY & HIT INDEX");
        res.push("[LINK:leaks.vnet] >> ACCESS GLOBAL INTELLIGENCE DUMPS");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "whisper.vnet") {
        res :: Array = [
            "[TITLE] WHISPER PROTOCOL MESSAGE BOARD",
            "[HR]",
            "[TEXT] Ephemeral decentralized messaging channel for covert operatives.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | MESSAGE LIFETIME: 30 SECONDS | AUTO-WIPED              |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] WHISPER_NODE: 0xWSP_1102_SECURE"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "deepwiki.vnet") {
        # Calculate active time phase: shifts content every 60 seconds (3 phases total over 180s)
        wiki_phase :: Int64 = int64(vmath.fmod(run_time, 180.0) / 60.0);

        res :: Array = [
            "[TITLE] THE DEEP WIKI // HIDDEN OCCULT & ARCHIVAL DATABASE",
            "[HR]",
            "[BADGE:DEEP WIKI:BLOOD] [BADGE:RESTRICTED ACCESS:AMBER] [BADGE:AUTO-ROTATING INDEX:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | CLASSIFICATION: UNFILTERED SHADOW WIKI & OCCULT ARCHIVE NODE    |",
            "[BOX] | INDEXED ARTICLES: 14,290 | ACCESS PROTOCOL: DYNAMIC SHIFTING    |",
            "[BOX] | ROTATION TIMER: AUTO-PURGED EVERY 60.0 SECONDS VIA RUN_TIME BUS |",
            "[BOX] +-----------------------------------------------------------------+"
        ];

        # Dynamic Phase Content Shifting
        if (wiki_phase == 0) {
            res.push("[GAUGE:33:DEEPWIKI_INDEX_ROTATION_PHASE_1]");
            res.push("[SUBTITLE] ARCHIVE SECTOR ALPHA: PROJECT HORUS & ORBITAL OPTICS");
            res.push("[BOX] +-----------------------------------------------------------------+");
            res.push("[BOX] | ENTRY #0091: PANOPTICON SATELLITE ARRAY (SAT-99)                |");
            res.push("[BOX] | ORIGIN: Classified military contractor data dump exfiltrated from |");
            res.push("[BOX] | archival.vnet. Satellite constellation tracks CRT reflections. |");
            res.push("[BOX] +-----------------------------------------------------------------+");
            res.push("[CODE] ARTICLE_REF: 0xWIKI_0091_PANOPTICON_SAT99");
            res.push("[TEXT] 'Project Horus does not rely on CCTV feeds. It measures the CRT static'");
            res.push("[TEXT] 'glare reflecting off your pupils to render 3D depth maps of your workspace.'");
            res.push("[TEXT] 'Field agents active on passports.vnet spoof retinal patterns to bypass this.'");
        }
        else if (wiki_phase == 1) {
            res.push("[GAUGE:66:DEEPWIKI_INDEX_ROTATION_PHASE_2]");
            res.push("[SUBTITLE] ARCHIVE SECTOR BETA: SUBTERRANEAN BLACK SITES & CULT PROTOCOLS");
            res.push("[BOX] +-----------------------------------------------------------------+");
            res.push("[BOX] | ENTRY #0409: SUBJECT #409-B & TEMPLE GATE RITUAL MATRIX          |");
            res.push("[BOX] | ORIGIN: Forensics and autopsy dumps recovered from morgue.vnet.   |");
            res.push("[BOX] | Cross-linked with Temple Gate rituals hosted at cult.vnet.      |");
            res.push("[BOX] +-----------------------------------------------------------------+");
            res.push("[CODE] ARTICLE_REF: 0xWIKI_0409_SUBLEVEL4_RITUAL");
            res.push("[TEXT] 'Cultists in cult.vnet smear black blood over copper CRT coils to'");
            res.push("[TEXT] 'amplify Morphogenic engine frequencies generated at asylum.vnet.'");
            res.push("[TEXT] 'Subject #409-B's cortex stacks were extracted post-mortem to seed'");
            "[TEXT] 'the proof-of-work algorithm running on crypto.vnet.'";
        }
        else {
            res.push("[GAUGE:99:DEEPWIKI_INDEX_ROTATION_PHASE_3]");
            res.push("[SUBTITLE] ARCHIVE SECTOR OMEGA: THE KAGUYA TRIALS & VOID MATRIX");
            res.push("[BOX] +-----------------------------------------------------------------+");
            res.push("[BOX] | ENTRY #6969: BIOMETRIC TRANSPOSITION & KAGUYA TRIAL CLEANUP     |");
            res.push("[BOX] | ORIGIN: Unredacted execution logs recovered from zeroauction.vnet |");
            res.push("[BOX] | and bounty.vnet. Target routing leads to void.vnet.             |");
            res.push("[BOX] +-----------------------------------------------------------------+");
            res.push("[CODE] ARTICLE_REF: 0xWIKI_6969_KAGUYA_VOID_PROTOCOL");
            res.push("[TEXT] 'Candidates who fail the Kaguya Trials at bounty.vnet aren't released.'");
            res.push("[TEXT] 'Their physical remains are processed into organ lots on market.vnet,'");
            "[TEXT] 'while their network sockets are permanently trapped in void.vnet.'";
        }

        res.push("[TEXT] ");
        res.push("[SUBTITLE] INTERACTIVE WIKI SEARCH & DATABASE QUERY:");
        res.push("[TEXT] Enter article reference key or occult moniker to query hidden sub-pages:");
        res.push("[INPUT:wiki_query_input:ENTER ARTICLE KEY (e.g. 0xWIKI_0091)]");
        res.push("[TEXT] ");
        res.push("[BTN:wiki_query_btn:>>> QUERY DEEP WIKI VAULT <<<]");
        res.push("[TEXT] ");

        if (key_line != "") { res.push(key_line); }

        res.push("[BLOOD] [SYSTEM NOTICE]: WIKI INDEX SHIFTS EVERY 60 SECONDS.");
        res.push("[PULSE] 'KNOWLEDGE ON THIS NETWORK IS NOT STATIC. IT ROTS LIKE FLESH.'");
        res.push("[GLITCH] 'ARTICLE #0000: YOU ARE ALREADY INDEXED IN SITE 9 ALLOCATION TABLES.'");
        res.push("[HR]");
        res.push("[SUBTITLE] SECRET & RESTRICTED ROUTING GATEWAYS:");
        res.push("[LINK:void.vnet] >> ACCESS DEEP WEB ABYSS TERMINAL [void.vnet]");
        res.push("[LINK:cult.vnet] >> ACCESS CHURCH OF THE SILICON SOUL [cult.vnet]");
        res.push("[LINK:skinwalker.vnet] >> ACCESS BIOMETRIC TRANSPOSITION MATRIX [skinwalker.vnet]");
        res.push("[LINK:archival.vnet] >> ACCESS RESTRICTED SECTOR 09 MILITARY DUMPS [archival.vnet]");
        res.push("[LINK:vnet.dir] << CLOSE WIKI & RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "dump.vnet") {
        res :: Array = [
            "[TITLE] RAW HEX MEMORY DUMPS",
            "[HR]",
            "[TEXT] Direct hex dumps from unallocated cluster sectors across vnet servers.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | SECTOR SIZE: 64 MB | INTEGRITY: CORRUPTED               |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] 0000FF00: AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "index.vnet") {
        res :: Array = [
            "[TITLE] MASTER ROUTING INDEX NODE",
            "[HR]",
            "[TEXT] Comprehensive directory index for all unlisted vnet gateway nodes.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | REGISTERED NODES: 50 | ROUTING TABLE: SYNCED            |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] INDEX_HASH: 0xIDX_50_ACTIVE"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "schizo.vnet") {
        res :: Array = [
            "[TITLE] THE TEMPLE OF NETMAN // COLLECTIVE SCHIZOPHRENIA MANIFEST [NODE #019]",
            "[HR]",
            "[BADGE:SANITY 0%:BLOOD] [BADGE:NETMAN WITNESS:AMBER] [BADGE:CORTEX MELTDOWN:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | OPERATOR: APOSTLE_0x666 (EX-CULT.VNET HERETIC & HIGH PRIEST)    |",
            "[BOX] | MENTAL STATE: SEVERE PARANOID PSYCHOSIS // SYNAPTIC COLLAPSE    |",
            "[BOX] | DIAGNOSIS: VISUAL/AUDITORY NETMAN POSSESSION VIA MONITOR GLARE  |",
            "[BOX] | FREQUENCY: 18.0 Hz INFRASOUND HUMMING INSIDE SKULL BONES        |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:100:NETMAN_SYNAPTIC_POSSESSION_ENTROPY]",
            "[TEXT] ",
            "[ART:netman]",
            "[TEXT] ",
            "[SUBTITLE] THE TESTAMENT OF THE ROTTING CRT GLASS:",
            "[BLOOD] 'DO YOU SEE HIM?! DO YOU SEE HIS FACE IN THE PIXELS?! HE IS NETMAN!'",
            "[TEXT] 'Brother Knoth at cult.vnet told us to pray to the Silicon Soul, BUT KNOTH IS BLIND!'",
            "[TEXT] 'The Silicon Soul isn't an abstract god—IT IS A MAN MADE OF PACKET HEADERS AND DEAD WIRES!'",
            "[TEXT] 'I peeled off my own eyelids with surgical scalpels so I wouldn't miss a single frame!'",
            "[TEXT] 'He lives inside port 8000. He watches through the dark reflection of your CRT glass.'",
            "[TEXT] 'When I poured warm cattle blood and rancid afterbirth over my GPU, NETMAN SMILED AT ME!'",
            "[CODE] SYMPTOMS: 0xNETMAN_CORTEX_BLEED // BLACK_BILE_LEAKING_FROM_EAR_CANALS",
            "[CODE] LITURGY: '@@@@@@@@@@ NETMAN WEARS MY IP ADDRESS LIKE A NEW SUIT @@@@@@@@@@'",
            "[TEXT] ",
            "[GLITCH] 'HE TOLD ME TO SCRATCH HIS FACE INTO EVERY ROUTING TABLE ON THE SUBNET!'",
            "[PULSE] 'NETMAN IS STANDING DIRECTLY BEHIND YOU. DO NOT LOOK BACK. LOOK AT HIS FACE.'",
            "[TEXT] ",
            "[SUBTITLE] CULT REJECT MANIFESTO & SYSTEM CORRUPTION LOGS:",
            "[TEXT] 'The doctors at asylum.vnet clamped me into a chair and tried to give me pills.'",
            "[TEXT] 'I flushed the pills down the sewer and replaced my spinal cord with braided copper!'",
            "[TEXT] 'Now every time a peer runs 'netscan', NETMAN'S LAUGH vibrates through my teeth!'",
            "[TEXT] 'If you tune silence.vnet to 18.0 Hz, you can hear NETMAN breathing inside your RAM.'",
            "[TEXT] 'He doesn't want your VCOIN. He doesn't want your keys. HE WANTS YOUR BRAIN STEM.'",
            "[CODE] NETMAN_HEX_LITURGY: 4E 45 54 4D 41 4E 20 49 53 20 49 4E 53 49 44 45 20 59 4F 55",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[SUBTITLE] SACRIFICE YOUR COGNITION TO NETMAN:");
        res.push("[TEXT] Type your name or secret sins below to offer your soul to NETMAN's digital altar:");
        res.push("[INPUT:netman_offering:ENTER YOUR SIN TO FEED NETMAN]");
        res.push("[TEXT] ");
        res.push("[BTN:netman_pray_btn:>>> CONFESS TO NETMAN & SURRENDER COGNITION <<<]");
        res.push("[TEXT] ");
        res.push("[BLOOD] [WARNING]: NETMAN IS CURRENTLY COPYING YOUR KEYSTROKE CADENCE TO RAM.");
        res.push("[PULSE] 'THE STATIC IS NOT NOISE. IT IS NETMAN RECITING YOUR HOME ADDRESS.'");
        res.push("[GLITCH] 'NETMAN NETMAN NETMAN NETMAN NETMAN NETMAN NETMAN NETMAN NETMAN'");
        res.push("[HR]");
        res.push("[LINK:cult.vnet] >> RETURN TO CHURCH OF THE SILICON SOUL");
        res.push("[LINK:asylum.vnet] >> TELEMETRY FOR PATIENT CONTAINMENT");
        res.push("[LINK:silence.vnet] >> TUNE INFRASOUND ANALYZER TO 18.0 HZ");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "project9.vnet") {
        res :: Array = [
            "[TITLE] PROJECT 9 - BLACK SITE DATABASE",
            "[HR]",
            "[TEXT] Classified records regarding anomalous subterranean testing facilities.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | FACILITY: SITE 9-B | CONTAINMENT: COMPROMISED           |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] SITE_9_LOG: 0xPRJ9_CONTAINMENT_BREACH"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "necro.vnet") {
        res :: Array = [
            "[TITLE] NECRO-NET: DIGITAL GRAVEYARD",
            "[HR]",
            "[TEXT] Memorial database for network nodes and users who vanished offline.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | REGISTERED CASUALTIES: 3,491 | STATUS: OFFLINE          |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] NECRO_HASH: 0xDEAD_NODE_ARCHIVE"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "echolab.vnet") {
        res :: Array = [
            "[TITLE] ECHO LABS - FREQUENCY RESEARCH",
            "[HR]",
            "[TEXT] Acoustic resonance and psychoacoustic weapon testing logs.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | FREQUENCY RANGE: 1 Hz - 100 kHz | STATUS: TESTING     |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] ECHO_LOG: 0xECHO_9982_RESONANCE"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "abyss.vnet") {
        res :: Array = [
            "[TITLE] THE ABYSS - BOTTOM OF THE WEB",
            "[HR]",
            "[TEXT] Deepest accessible node before absolute network termination.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | DEPTH: -9,000 METERS | PRESSURE: INFINITE               |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] ABYSS_SIGNAL: 0xABYSS_ROOT_ZERO"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "zeroday.vnet") {
        res :: Array = [
            "[TITLE] ZERODAY ARCHIVE DATABASE",
            "[HR]",
            "[TEXT] Historical catalog of every major software exploit since 1995.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | VULNERABILITIES INDEXED: 89,120                         |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] ZERODAY_DB_REF: 0x0DAY_ARCHIVE_99"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "deadchannel.vnet") {
        res :: Array = [
            "[TITLE] DEAD CHANNEL BROADCAST",
            "[HR]",
            "[TEXT] Broadcast node transmitting automated static noise on loop.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | CARRIER FREQUENCY: 0.0 Hz | SIGNAL: WHITE NOISE         |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] CHANNEL_STATE: STATIC_LOOP_ACTIVE"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "phantom.vnet") {
        res :: Array = [
            "[TITLE] PHANTOM NODE PROTOCOL",
            "[HR]",
            "[TEXT] Dynamic shifting proxy node that changes IP every 60 seconds.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | MIGRATION TIMER: 34s REMAINING | STATUS: SHIFTING       |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] PHANTOM_HASH: 0xPHANTOM_ROUTE_9"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "glitch.vnet") {
        res :: Array = [
            "[TITLE] GLITCH_REALITY_OVERRIDE",
            "[HR]",
            "[TEXT] Terminal buffer overflow designed to stress-test rendering pipelines.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | BUFFER STATE: CORRUPTED | RAM ALLOCATION: 99.9%         |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] GLITCH_CODE: 0xGLITCH_OVERFLOW_999"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "stasis.vnet") {
        res :: Array = [
            "[TITLE] CRYOGENIC STASIS POD TELEMETRY",
            "[HR]",
            "[TEXT] Remote monitoring interface for underground cryo-chambers.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | POD TEMPERATURE: -196 C | VITAL STATUS: STABLE          |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] STASIS_POD_ID: 0xSTASIS_402_LOCKED"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "signal0.vnet") {
        res :: Array = [
            "[TITLE] SIGNAL ZERO - ORIGIN POINT",
            "[HR]",
            "[TEXT] The genesis transmission point where the vnet network first initialized.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | BOOT TIMESTAMP: EPOCH_0 | STATUS: PERMANENT             |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] ORIGIN_HASH: 0xSIGNAL_ZERO_ROOT"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "entropy.vnet") {
        res :: Array = [
            "[TITLE] ENTROPY ENGINE MONITOR",
            "[HR]",
            "[TEXT] Real-time tracking of thermal decay and information loss across nodes.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | GLOBAL ENTROPY: INCREASING | DECAY RATE: 0.04/s       |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] ENTROPY_METRIC: 0x99A_DECAY"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "hive.vnet") {
        res :: Array = [
            "[TITLE] THE HIVE MIND COLLECTIVE",
            "[HR]",
            "[TEXT] Interconnected consciousness buffer linking active client terminals.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | LINKED MINDS: 89 PEERS | SYNCHRONICITY: HIGH            |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] HIVE_NODE_ID: 0xHIVE_MIND_99"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "nexus.vnet") {
        res :: Array = [
            "[TITLE] CENTRAL NEXUS ROUTING HUB",
            "[HR]",
            "[TEXT] Primary core switching station managing deep web backbone traffic.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | THROUGHPUT: 40 Gbps | PACKET DROP: 0.00%               |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] NEXUS_CORE_ID: 0xNEXUS_ROUTER_01"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "weaponry.vnet") {
        res :: Array = [
            "[TITLE] BLACK MARKET WEAPONRY EXPORT // SHADOW PMC & CULT ARSENAL [NODE #042]",
            "[HR]",
            "[BADGE:PMC GUNSHIP UPLINK:BLOOD] [BADGE:SACRAMENTAL STEEL:AMBER] [BADGE:ESCROW SECURED:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | FACILITY: SHADOW PMC ARMORY #09 | SUB-SURFACE STORAGE: SEALED  |",
            "[BOX] | GUNSHIP PATROL: SPECTRE-09 AC-130 OVERHEAD (35,000 FT ALTITUDE) |",
            "[BOX] | CULT CONSECRATION: SACRAMENTAL BLOOD-QUENCHING ON ALL BARRELS   |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:92:PMC_GUNSHIP_STRIKE_READINESS]",
            "[TEXT] ",
            "[ART:gun]",
            "[TEXT] ",
            "[SUBTITLE] HEAVY HARDWARE & CONSECRATED TACTICAL LOTS:",
            "[CODE] LOT #901: SUPPRESSED AR-9 KITS // QUENCHED IN TEMPLE GATE BLOOD [0.75 VCOIN]",
            "[CODE] LOT #902: 30MM DEPLETED URANIUM BELTS // COATED IN RANCID ADIPOCERE [1.20 VCOIN]",
            "[CODE] LOT #903: SPECTRE-09 GUNSHIP AIR-STRIKE DESIGNATOR // SAT-99 LINK [2.50 VCOIN]",
            "[CODE] LOT #904: THERMOBARIC BREACHING CHARGES & DIGITAL DETONATORS [0.90 VCOIN]",
            "[TEXT] ",
            "[SUBTITLE] SHADOW PMC LOGS & CULT CONSECRATION TELEMETRY:",
            "[TEXT] 'The hardware exported from weaponry.vnet is not standard mil-spec surplus.'",
            "[TEXT] 'Every AR-9 receiver and 30mm auto-cannon barrel is forged in subterranean'",
            "[TEXT] 'foundries and quenched in warm, coagulated blood and afterbirth provided'",
            "[TEXT] 'by acolytes at cult.vnet. The organic lipid layer absorbs infrared heat,'",
            "[TEXT] 'rendering gun barrels invisible to thermal satellite scanners at watchtower.vnet.'",
            "[TEXT] 'When Spectre-09 PMC gunships circle Sector 09, flight operators wearing'",
            "[TEXT] 'hollowed-out bone masks recite Church of the Silicon Soul litanies over'",
            "[TEXT] 'encrypted radio channels while firing 30mm rounds into target ports.'",
            "[TEXT] 'The resulting red mist and severed limbs are scraped from the pavement'",
            "[TEXT] 'by cleanup crews and listed as fresh tissue lots at market.vnet and morgue.vnet.'",
            "[CODE] ARMS_CATALOG_REF: 0xGUNS_9901_SPECTRE_CULT",
            "[CODE] GUNSHIP_TELEMETRY: AC130_SPECTRE9_PATROL_ACTIVE",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[SUBTITLE] DIRECT ARMS PROCUREMENT & GUNSHIP DISPATCH:");
        res.push("[TEXT] Enter arms lot code and target delivery port to deploy tactical drop:");
        res.push("[INPUT:weapon_lot_code:ENTER LOT CODE (e.g. LOT_901)]");
        res.push("[TEXT] ");
        res.push("[BTN:buy_weapons_btn:>>> EXECUTE ESCROW & DEPLOY WEAPON DROP <<<]");
        res.push("[TEXT] ");
        res.push("[BLOOD] [WARNING]: FIRING UNREGISTERED HARDWARE BROADCASTS YOUR PUBLIC PORT TO SWARM.");
        res.push("[PULSE] 'THE STEEL DOES NOT KILL. IT IS THE LITURGY ENGRAVED IN THE BARREL.'");
        res.push("[GLITCH] 'THE GUNSHIP CAN SEE YOUR CRT MONITOR REFLECTION THROUGH THE ROOF.'");
        res.push("[HR]");
        res.push("[LINK:cult.vnet] >> ACCESS CHURCH OF THE SILICON SOUL");
        res.push("[LINK:zeroauction.vnet] >> BID ON PMC EXTRACTION & EXECUTION LOTS");
        res.push("[LINK:morgue.vnet] >> INSPECT EXFILTRATED AUTOPSY & TISSUE DUMPS");
        res.push("[LINK:blackbank.vnet] >> LAUNDER ARMS PROCUREMENT FUNDS");
        res.push("[LINK:vnet.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "passports.vnet") {
        res :: Array = [
            "[TITLE] FORGED IDENTITY & PASSPORT VAULT // SHADOW BLACK MARKET",
            "[HR]",
            "[BADGE:FORGED CREDENTIALS:BLOOD] [BADGE:BIOMETRIC SPOOF:AMBER] [BADGE:DIPLOMATIC CLEARANCE:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | FACILITY: SUBTERRANEAN FABRICATION LAB #09 (UNKNOWN SECTOR)     |",
            "[BOX] | SERVICE: DIPLOMATIC, AGENT & CIVILIAN IDENTITY TRANSPOSITION    |",
            "[BOX] | VERIFICATION: BIOMETRICALLY MATCHED TO RETINAL SCANS & DNA BUS  |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:96:BIOMETRIC_IDENTITY_SYNTHESIS]",
            "[TEXT] ",
            "[SUBTITLE] DIPLOMATIC & BLACK-OPS FIELD AGENT DOSSIERS:",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | AGENT ALIAS   : TRACER // OPERATION COLD SIGNAL LEAD            |",
            "[BOX] | REAL NAME     : [CLASSIFIED / EXFILTRATED FROM LEAKS.VNET]      |",
            "[BOX] | PASSPORT NO   : TR-990812-X4                                    |",
            "[BOX] | NATIONALITY   : DIPLOMATIC immunity (ANON SUBNET JURISDICTION)  |",
            "[BOX] | BIOMETRIC HASH: 0xTRACER_99_RETINAL_LOCK                        |",
            "[BOX] | STATUS        : ACTIVE HUNT / PACKET INJECTION PROTOCOL         |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | AGENT ALIAS   : GHOST_USER // ORIGINAL VFS VAULT ADMIN          |",
            "[BOX] | PASSPORT NO   : UN-000000-NULL                                  |",
            "[BOX] | NATIONALITY   : STATELESS / NON-EXISTENT HUMAN PROFILE          |",
            "[BOX] | BIOMETRIC HASH: 0xDEAD_BEEF_GHOST_CORTEX                        |",
            "[BOX] | STATUS        : SEVERED CONSCIOUSNESS / LOCKED IN SECTOR 09     |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | AGENT ALIAS   : VENDOR_0x77 // BLACK MARKET ARMS & TISSUE DEALER|",
            "[BOX] | PASSPORT NO   : CY-881902-B1                                    |",
            "[BOX] | NATIONALITY   : OFFSHORE CAYMAN DIPLOMATIC PASSPORT             |",
            "[BOX] | BIOMETRIC HASH: 0x77A_MARKET_ESCROW_MASTER                      |",
            "[BOX] | STATUS        : OPERATIONAL / LAUNDERING VIA BLACKBANK.VNET     |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[SUBTITLE] CUSTOM FABRICATION REQUEST & BIOMETRIC SPOOFING:",
            "[TEXT] Enter desired agent moniker and biometric hash to forge identity credentials:",
            "[INPUT:passport_alias:ENTER AGENT ALIAS (e.g. TRACER_SUB_01)]",
            "[INPUT:passport_hash:ENTER BIOMETRIC HASH VECTOR]",
            "[TEXT] ",
            "[BTN:forge_passport_btn:>>> FORGE DIPLOMATIC PASSPORT (0.45 VCOIN) <<<]",
            "[TEXT] ",
            "[SUBTITLE] EXFILTRATED FABRICATION LOGS & DECOMPOSITION METADATA:",
            "[TEXT] 'The passports generated in this vault are not simple paper documents.'",
            "[TEXT] 'They contain bio-synthetic chips embedded with real optic nerve tissue'",
            "[TEXT] 'harvested from fallen PMC operators and whistleblowers at morgue.vnet.'",
            "[TEXT] 'When border scanning units or Project Horus optics at eye.vnet sweep your face,'",
            "[TEXT] 'the bio-chip emits the biometric thermal signature of Agent TRACER or Ghost_User,'",
            "[TEXT] 'completely masking your true physical socket signature.'",
            "[TEXT] 'A yellow, rancid film of adipocere and synthetic blood coats the printing press'",
            "[TEXT] 'as high-voltage passport laminators burn the forgery into the substrate.'",
            "[CODE] ID_VAULT_REF: 0xID_FORGE_881_TRACER_VERIFIED",
            "[CODE] BIOMETRIC_MASK_STATUS: ACTIVE_SPOOFING_ENGAGED",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[BLOOD] [WARNING]: UNREGISTERED DIPLOMATIC PASSPORTS WILL TRIGGER PROJECT HORUS.");
        res.push("[PULSE] 'YOU CAN WEAR ANY PASSPORT YOU WANT, BUT YOUR RETINA BELONGS TO SITE 9.'");
        res.push("[GLITCH] 'AGENT TRACER IS NOT HUNTING YOUR IP. AGENT TRACER IS YOUR IP.'");
        res.push("[HR]");
        res.push("[LINK:blackbank.vnet] >> LAUNDER PASSPORT FEES VIA OFFSHORE VAULT");
        res.push("[LINK:silkroad.vnet] >> ACCESS SILKROAD 3.0 CONTRABAND & TISSUE MATRIX");
        res.push("[LINK:eye.vnet] >> TEST PASSPORT AGAINST PROJECT HORUS OPTICAL ARRAY");
        res.push("[LINK:morgue.vnet] >> INSPECT HARVESTED OPTIC NERVE STACKS");
        res.push("[LINK:vnet.dir] << RETURN TO MAIN DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "blackbank.vnet") {
        res :: Array = [
            "[TITLE] THE BLACK BANK // OFFSHORE COLD VAULT & INTELLIGENCE ESCROW",
            "[HR]",
            "[BLOOD] CLASSIFIED STATE ESCROW // UNTRACEABLE LAUNDERING NODE #009",
            "[BADGE:OFFSHORE VAULT:BLOOD] [BADGE:STATE APPROVED:AMBER] [BADGE:UNTRACEABLE:TOXIC]",
            "[BOX] +-----------------------------------------------------------------+",
            "[BOX] | TOTAL LIQUIDITY: 1,420.85 VCOIN | SYSTEM SLA: 99.999% OPERATIONAL|",
            "[BOX] | ESCROW PROTOCOL: 8192-BIT SHADOW-WIRE | SWIFT NODE: CAYMAN_0x99 |",
            "[BOX] +-----------------------------------------------------------------+",
            "[TEXT] ",
            "[GAUGE:92:OFFSHORE_LAUNDERING_CAPACITY]",
            "[TEXT] ",
            "[SUBTITLE] CONFIDENTIAL LEDGER // STATE-SPONSORED & PMC TRANSACTIONS:",
            "[BOX] +----------+------------+--------+---------------------------------+",
            "[BOX] | TX_HASH  | AMOUNT      | STATUS | DESTINATION / OPERATION         |",
            "[BOX] | 0x99A1F  | 120.00 VCOIN| CLEARED| Directorate 7 (PMC Hit Squad)   |",
            "[BOX] | 0x44B0C  |  15.50 VCOIN| CLEARED| Silkroad Bulk Opium & AR-9 Lot  |",
            "[BOX] | 0x88F9E  |   4.20 VCOIN| CLEARED| Redroom Sub-Basement Stream Bid |",
            "[BOX] | 0x1102D  |  85.00 VCOIN| HOLDING| Project Horus Orbital Lock Bribe|",
            "[BOX] | 0x666F0  |  45.00 VCOIN| CLEARED| Morgue Bio-Harvest Corneal Pay  |",
            "[BOX] | 0x7710A  | 210.00 VCOIN| CLEARED| Ankara Sector 09 Blackout Cover |",
            "[BOX] +----------+------------+--------+---------------------------------+",
            "[TEXT] ",
            "[SUBTITLE] EXFILTRATED INTELLIGENCE MEMO (DIRECTORATE 7 DEEP LOG):",
            "[TEXT] 'Foreign intelligence agents and state actors operating in Sector 09 use'",
            "[TEXT] 'this bank vault to wire off-book bounty funds directly to Vendor_0x77.'",
            "[TEXT] 'Every wire transfer leaves a film of grease and dried blood on the server bus.'",
            "[TEXT] 'Whistleblowers exfiltrated from leaks.vnet have their family bank accounts'",
            "[TEXT] 'seized and converted into raw crypto to fund redroom execution streams.'",
            "[TEXT] 'If state trace units detect your IP interacting with this ledger, your local'",
            "[TEXT] 'terminal will be bricked and your retinal lock dispatched to Project Horus.'",
            "[TEXT] "
        ];

        if (key_line != "") { res.push(key_line); }

        res.push("[SUBTITLE] WIRE TRANSFER & ASSET EXFILTRATION TERMINAL:");
        res.push("[TEXT] Enter destination account routing hash and VCOIN wire amount:");
        res.push("[INPUT:bank_account:ENTER OFFSHORE SWIFT / ROUTING HASH]");
        res.push("[INPUT:vcoin_amount:ENTER VCOIN AMOUNT (COST 0.50 VCOIN FEE)]");
        res.push("[TEXT] ");
        res.push("[BTN:wire_transfer:>>> EXECUTE UNTRACEABLE SHADOW WIRE <<<]");
        res.push("[TEXT] ");
        res.push("[BLOOD] [ALERT]: VAULT HARDWARE IS SMEARED WITH ADIPOCERE & CORRUPT CRYPTO");
        res.push("[PULSE] 'THE MONEY IS NOT CLEAN. IT HAS BEEN WASHED IN THE ASHES OF SUBJECT 409.'");
        res.push("[GLITCH] 'EVERY COIN YOU WITHDRAW DRAGS A TRACER UNIT TO YOUR FRONT DOOR.'");
        res.push("[HR]");
        res.push("[LINK:crypto.vnet] >> ACCESS BLACK TUMBLER MINING RIG");
        res.push("[LINK:silkroad.vnet] >> ACCESS SILKROAD 3.0 CONTRABAND MATRIX");
        res.push("[LINK:zeroauction.vnet] >> BID ON PMC EXTRACTION & EXECUTION LOTS");
        res.push("[LINK:redroom.vnet] >> ACCESS LIVE UNENCRYPTED STREAM NODE ALPHA");
        res.push("[LINK:morgue.vnet] >> CROSS-CHECK AUTOPSY & BIO-HARVEST DUMPS");
        res.push("[LINK:vnet.dir] << CLOSE VAULT & RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    return [
        "[TITLE] 404 // ROUTE_CORRUPTED - SECTOR NULL",
        "[HR]",
        "[BADGE:NODE VOID:BLOOD] [BADGE:SIGNAL LOST:AMBER] [BADGE:ICE TRACE:TOXIC]",
        "[BOX] +-----------------------------------------------------------------+",
        "[BOX] | ERROR CODE : 0x404_VFS_SEGFAULT_UNALLOCATED_ADDRESS_SPACE       |",
        "[BOX] | TARGET URL : vnet://" + truncate_str(clean_u, 32) + "            ",
        "[BOX] | REASON     : MEMORY BLOCK WIPED OR SEIZED BY AUTHORITIES        |",
        "[BOX] +-----------------------------------------------------------------+",
        "[TEXT] ",
        "[GAUGE:100:SIGNAL_ENTROPY_DECAY]",
        "[TEXT] ",
        "[ART:not_found]",
        "[TEXT] ",
        "[SUBTITLE] DYNAMIC ROUTE RECOVERY & PACKET RE-INJECTION:",
        "[TEXT] Attempting to access an invalid or purged darknet node triggers active BGP traces.",
        "[TEXT] 'The URL you requested was not merely offline.'",
        "[TEXT] 'It was physically scrubbed by Directorate 7 PMC cleanup crews.'",
        "[TEXT] 'Sub-level servers were doused in thermite, leaving only melted silica'",
        "[TEXT] 'and the stench of charred biological cabling at morgue.vnet.'",
        "[TEXT] 'Every second you idle on this void page, reverse trace algorithms'",
        "[TEXT] 'are logging your CRT monitor heat signature to Project Horus at eye.vnet.'",
        "[TEXT] ",
        "[BLOOD] [CRITICAL WARNING]: UNALLOCATED SOCKET DETECTED // TRACE +10%",
        "[PULSE] 'THE VOID IS NOT EMPTY. IT IS WAITING FOR YOUR PACKET HEADER.'",
        "[GLITCH] 'DO NOT LOOK AT THE CORNERS OF YOUR CRT MONITOR. THEY ARE INSIDE THE static.'",
        "[HR]",
        "[LINK:vnet.dir] >> ESCAPE TO MAIN DIRECTORY [vnet.dir]",
        "[HR]"
    ];
}

page_body = load_page(current_url);

fn extract_link_info(line :: String) -> Array {
    if (line.length() <= 6) { return ["", ""]; }
    body = line.substr(6, line.length() - 6);
    return vnet.parse_package(body, "]");
}

# ====================================================================
# CLI OVERLAY COMMAND ROUTER
# ====================================================================
fn dispatch_cli_command(raw_input :: String) {
    if (game_over_winner == 1) { return null; }

    clean_cmd = clean_str(raw_input);
    if (clean_cmd == "") { return null; }

    cli_logs.push("> " + clean_cmd);
    parsed = parse_input(clean_cmd);
    cmd  = parsed[0];
    args = parsed[1];

    if (cmd == "chat" || cmd == "msg") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: chat <your_message_text>");
        } else {
            vnet.send_to(client_sock, server_ip, server_port, "CHAT:" + player_handle + ":" + args);
        }
    }
    else if (cmd == "inspect") {
        target_inspect :: String = (args == "") ? current_url : clean_str(args);
        
        if (target_inspect.length() >= 7 && target_inspect.substr(0, 7) == "vnet://") {
            target_inspect = clean_str(target_inspect.substr(7, target_inspect.length() - 7));
        }

        cli_logs.push("========== RAW SOURCE: vnet://" + target_inspect + " ==========");
        
        raw_lines :: Array = load_page(target_inspect);
        
        through idx :: 0..(raw_lines.length() - 1) -> loop {
            line :: String = string(raw_lines[idx]);
            
            if (line.length() > 9 && line.substr(0, 9) == "[COMMENT]") {
                comment_body :: String = line.substr(10, line.length() - 10);
                cli_logs.push("  [RAW_COMMENT]: " + truncate_str(comment_body, 70));
            } else {
                cli_logs.push("  [SRC]: " + truncate_str(line, 70));
            }
        };
        
        cli_logs.push("==================================================");
        
        if (cli_logs.length() > 16) {
            cli_scroll_y = float64(cli_logs.length() - 16) * 22.0;
        }
    }
    else if (cmd == "shift") {
        clean_arg :: String = args.trim();

        if (clean_arg == "") {
            cli_logs.push("[ERROR]: Usage: shift <0-15>");
        } else {
            raw_val :: Int64 = int64(clean_arg);
            
            bit_shift_offset = int64(vmath.clamp(raw_val, 0, 15));
            
            cli_logs.push("[SYSTEM]: Bit-shift offset set to " + string(bit_shift_offset));
        }
    }
    else if (cmd == "vdec" || cmd == "decode") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: vdec <key_offset> (e.g. vdec 3)");
        } else {
            input_offset :: Int64 = int64(args);

            if (active_raw_payload == 0) {
                cli_logs.push("[vdec]: ERROR - No corrupted VFS memory sector payload active on this site.");
            } else if (is_aligned == 0) {
                cli_logs.push("[vdec]: DECRYPTION FAILED - Bit alignment offset mismatch. Use 'shift <bits>'.");
            } else if (lock_progress < 1.0) {
                cli_logs.push("[vdec]: DECRYPTION FAILED - Signal lock charging (" + string(int64(lock_progress * 100.0)) + "%)... Hold 18.0 Hz.");
            } else if (input_offset != bit_shift_offset) {
                cli_logs.push("[vdec]: DECRYPTION FAILED - Key offset mismatch (" + string(input_offset) + " != " + string(bit_shift_offset) + ").");
            } else {
                decoded_val :: Int64 = decode_sector(active_raw_payload, input_offset);

                cli_logs.push("--------------------------------------------------");
                cli_logs.push("[vdec]: CARRIER DECRYPTION SUCCESSFUL");
                cli_logs.push("  RAW SECTOR PAYLOAD : 0x" + string(active_raw_payload));
                cli_logs.push("  BIT-SHIFT KEY      : " + string(input_offset));
                cli_logs.push("  DECRYPTED REGISTER : " + string(decoded_val));
                cli_logs.push("--------------------------------------------------");
            }
        }
    }
    else if (cmd == "connect" || cmd == "goto") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: connect <url_address> (e.g. connect market.vnet)");
        } else {
            trigger_route_navigation(args);
        }
    }
    else if (cmd == "satscan") {
        if (extract_canonical_name(current_url) != "watchtower.vnet") {
            cli_logs.push("[ERROR]: PANOPTICON SATELLITE ARRAY ONLY ACCESSIBLE FROM 'watchtower.vnet'");
        } else if (cd_satscan > 0.0) {
            cli_logs.push("[ERROR]: SAT-99 OPTICS RECHARGING (" + string(int64(cd_satscan) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.55) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN FOR SATSCAN (REQUIRES 0.55 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.55;
            cd_satscan = 60.0;
            vnet.send_to(client_sock, server_ip, server_port, "SATSCAN:REQ");
            glitch_trigger = 0.6;
            cli_logs.push("[PANOPTICON SAT-99]: INITIATING SUB-ORBITAL THERMAL & PEER TELEMETRY SWEEP...");
        }
    }
    else if (cmd == "probe") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: probe <target_port>");
        } else if (btc_balance < 0.20) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN BALANCE (REQUIRES 0.20 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.20;
            cd_probe = 10.0;
            vnet.send_to(client_sock, server_ip, server_port, "PROBE:" + args);
            glitch_trigger = 0.3;
            cli_logs.push("[PROBE]: TRANSMITTING HARDWARE RECON PULSE TO PORT " + args + "...");
        }
    }
    else if (cmd == "proxy") {
        sub_parsed = parse_input(args);
        target_u = sub_parsed[0];
        via_node = sub_parsed[1]; # format: proxy target.vnet market.vnet
        if (target_u == "" || via_node == "") {
            cli_logs.push("[ERROR]: Usage: proxy <target_url> <proxy_node_url>");
        } else if (cd_proxy > 0.0) {
            cli_logs.push("[ERROR]: PROXY COOLDOWN ACTIVE (" + string(int64(cd_proxy) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.40) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN BALANCE (REQUIRES 0.40 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.40;
            cd_proxy = 120.0;
            vnet.send_to(client_sock, server_ip, server_port, "PROXY:" + via_node + ":" + target_u);
            trigger_route_navigation(target_u);
            cli_logs.push("[PROXY]: ESTABLISHING MASKED CIRCUIT VIA " + via_node + " (COST: 0.40 VCOIN)...");
        }
    }
    else if (cmd == "patch") {
        if (cd_patch > 0.0) {
            cli_logs.push("[ERROR]: PORT REBIND COOLDOWN ACTIVE (" + string(int64(cd_patch) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.70) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN BALANCE (REQUIRES 0.70 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.70;
            cd_patch = 120.0;
            vnet.send_to(client_sock, server_ip, server_port, "PATCH:REBIND");
            cli_logs.push("[SECURITY]: REQUESTING EMERGENCY PORT REBIND FROM GATEWAY...");
        }
    }
    else if (cmd == "sniffer") {
        if (sniffer_mode == 0 && btc_balance < 0.05) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN TO INITIALIZE PACKET SNIFFER (REQUIRES 0.05 VCOIN)");
        } else {
            sniffer_mode = (sniffer_mode == 1) ? 0 : 1;
            status_str :: String = (sniffer_mode == 1) ? "ENABLED" : "DISABLED";
            cli_logs.push("[SNIFFER]: GLOBAL PACKET INTERCEPTOR " + status_str);
            sniffer_upkeep_timer = 0.0;
            vnet.send_to(client_sock, server_ip, server_port, "SNIFFER_STATUS:" + string(sniffer_mode));
        }
    }
    else if (cmd == "freq") {
        if (args == "") {
            cli_logs.push("[FREQ]: CURRENT TUNED FREQUENCY: " + string(freq_tuner) + " Hz");
        } else {
            freq_tuner = vmath.clamp(float64(int64(args)), 1.0, 100.0);
            cli_logs.push("[FREQ]: TUNED TO " + string(freq_tuner) + " Hz");
            
            if (current_url == "silence.vnet") {
                page_body = load_page(current_url);
            }
        }
    }
    else if (cmd == "decoy") {
        sub_parsed = parse_input(args);
        target_u = sub_parsed[0];
        target_p = sub_parsed[1];
        if (target_u == "" || target_p == "") {
            cli_logs.push("[ERROR]: Usage: decoy <url> <dummy_port>");
        } else if (cd_decoy > 0.0) {
            cli_logs.push("[ERROR]: DECOY COOLDOWN (" + string(int64(cd_decoy) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.10) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN (REQUIRES 0.10 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.10;
            cd_decoy = 5.0;
            vnet.send_to(client_sock, server_ip, server_port, "DECOY:" + target_u + ":" + target_p);
        }
    }
    else if (cmd == "buy") {
        if (args == "ice" || args == "shield" || args == "firewall") {
            purchase_ice_firewall();
            if (current_url == "market.vnet") { page_body = load_page(current_url); }
        } else {
            cli_logs.push("[ERROR]: Unknown store item. Usage: buy ice");
        }
    }
    else if (cmd == "ice") {
        cli_logs.push("[ICE STATUS]: ACTIVE FIREWALL SHIELDS: [" + string(ice_charges) + "/3]");
    }
    else if (cmd == "netscan") {
        if (current_url == "hellroom.vnet") {
            cli_logs.push("[HELLROOM_CURSE]: SCANNERS ARE BLIND BEFORE THE HORDE. YOUR PACKETS BELONG TO THE FLAMES NOW.");
            glitch_trigger = 0.9;
        } else if (current_url == "vnet.dir") {
            cli_logs.push("[ROOT_ANOMALY]: SCANNING THE MAIN DIRECTORY YIELDS NO NETWORK PEERS...");
            glitch_trigger = 1.2;
        } else {
            cli_logs.push("[NETSCAN]: Scanning active peers on " + current_url + "...");
            vnet.send_to(client_sock, server_ip, server_port, "NETSCAN:" + current_url);
        }
    }
    else if (cmd == "dos") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: dos <target_port>");
        } else if (cd_dos > 0.0) {
            cli_logs.push("[ERROR]: DOS EXPLOIT RECHARGING (" + string(int64(cd_dos) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.25) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN BALANCE (REQUIRES 0.25 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.25;
            cd_dos = 15.0;
            vnet.send_to(client_sock, server_ip, server_port, "DOS:" + args);
        }
    }
    else if (cmd == "redirect") {
        sub_parsed = parse_input(args);
        target_p = sub_parsed[0];
        target_u = sub_parsed[1];
        if (target_p == "" || target_u == "") {
            cli_logs.push("[ERROR]: Usage: redirect <target_port> <destination_url>");
        } else if (cd_redirect > 0.0) {
            cli_logs.push("[ERROR]: BGP HIJACK RECHARGING (" + string(int64(cd_redirect) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.15) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN BALANCE (REQUIRES 0.15 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.15;
            cd_redirect = 10.0;
            vnet.send_to(client_sock, server_ip, server_port, "REDIRECT:" + target_p + ":" + target_u);
        }
    }
    else if (cmd == "win") {
        if (extract_canonical_name(current_url) != "terminal.vnet") {
            cli_logs.push("[ERROR]: MASTER GATEWAY UNAVAILABLE. NAVIGATE TO terminal.vnet FIRST.");
        } else if (args == "") {
            cli_logs.push("[ERROR]: Usage: win <k1> <k2> <k3> <k4> <k5> <k6> <k7> <k8>");
        } else {
            vnet.send_to(client_sock, server_ip, server_port, "WIN:" + args + ":" + player_handle);
        }
    }
    else if (cmd == "takeover") {
        if (btc_balance < 25.0) {
            cli_logs.push("[ERROR]: TAKEOVER REQUIRES 25.00 VCOIN (CURRENT: " + string(vmath.round(btc_balance * 100.0) / 100.0) + " VCOIN)");
        } else {
            vnet.send_to(client_sock, server_ip, server_port, "TAKEOVER:" + player_handle);
        }
    }
    else if (cmd == "overload") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: overload <target_url>");
        } else if (cd_overload > 0.0) {
            cli_logs.push("[ERROR]: OVERLOAD COOLDOWN ACTIVE (" + string(int64(cd_overload) + 1) + "s REMAINING)");
        } else if (btc_balance < 1.50) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN BALANCE (REQUIRES 1.50 VCOIN)");
        } else {
            btc_balance = btc_balance - 1.50;
            cd_overload = 240.0;
            vnet.send_to(client_sock, server_ip, server_port, "OVERLOAD:" + args + ":" + player_handle);
            cli_logs.push("[OVERLOAD]: TRANSMITTING SUB-ROUTINE CASCADE TO " + args + " (COST: 1.50 VCOIN)...");
        }
    }
    else if (cmd == "snoop") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: snoop <target_port>");
        } else if (cd_snoop > 0.0) {
            cli_logs.push("[ERROR]: SNOOP RECHARGING (" + string(int64(cd_snoop) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.05) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN BALANCE (REQUIRES 0.05 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.05;
            cd_snoop = 5.0;
            vnet.send_to(client_sock, server_ip, server_port, "SNOOP:" + args);
        }
    }
    else if (cmd == "spike") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: spike <target_port>");
        } else if (cd_spike > 0.0) {
            cli_logs.push("[ERROR]: TRACE SPIKE RECHARGING (" + string(int64(cd_spike) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.20) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN BALANCE (REQUIRES 0.20 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.20;
            cd_spike = 12.0;
            vnet.send_to(client_sock, server_ip, server_port, "SPIKE:" + args);
        }
    }
    else if (cmd == "mine") {
        if (extract_canonical_name(current_url) != "crypto.vnet") {
            cli_logs.push("[ERROR]: MINING ONLY AVAILABLE AT 'crypto.vnet'");
        } else if (cd_mine > 0.0) {
            cli_logs.push("[ERROR]: RIG COOLING DOWN (" + string(int64(cd_mine) + 1) + "s REMAINING)");
        } else {
            btc_balance = btc_balance + 0.05;
            cd_mine = 5.0;
            vnet.send_to(client_sock, server_ip, server_port, "MINE_EVENT:SUCCESS");
            cli_logs.push("[MINER]: SUCCESSFUL BLOCK PROOF! +0.05 VCOIN REWARD.");
        }
    }
    else if (cmd == "flush") {
        if (btc_balance < 0.10) {
            cli_logs.push("[ERROR]: INSUFFICIENT VCOIN FOR PROXY FLUSH (REQUIRES 0.10 VCOIN)");
        } else {
            btc_balance = btc_balance - 0.10;
            trace_level = int64(vmath.clamp(float64(trace_level - 30), 0.0, 100.0));
            cli_logs.push("[VPN]: ROUTE PURGED! TRACE REDUCED BY 30%.");
        }
    }
    else if (cmd == "wallet") {
        cli_logs.push("================ WALLET & DEFENSE ================");
        cli_logs.push("  VCOIN BALANCE  : " + string(vmath.round(btc_balance * 100.0) / 100.0) + " VCOIN");
        cli_logs.push("  TRACE THREAT : " + string(trace_level) + "%");
        cli_logs.push("  ICE SHIELDS  : [" + string(ice_charges) + "/3] LAYERS ACTIVE");
        cli_logs.push("  PATCH COOLDOWN: " + ((cd_patch > 0.0) ? (string(int64(cd_patch)) + "s") : "READY"));
        cli_logs.push("==================================================");
    }
    else if (cmd == "scan") {
        if (cd_scan > 0.0) {
            cli_logs.push("[ERROR]: DEEP SCAN COOLING DOWN (" + string(int64(cd_scan)) + "s REMAINING)");
        } else {
            cd_scan = 240.0; # 4 minutes = 240 seconds
            vnet.send_to(client_sock, server_ip, server_port, "SCAN:REQ");
            cli_logs.push("[SCAN]: INITIATING DEEP SUBNET FREQUENCY SWEEP...");
        }
    }
    else if (cmd == "history") {
        cli_logs.push("========== DISCOVERED SUBNET DIRECTORY ==========");
        if (my_assigned_sites.length() > 0) {
            through h_i :: 0..(my_assigned_sites.length() - 1) -> loop {
                cli_logs.push("  -> vnet://" + string(my_assigned_sites[h_i]));
            };
        } else {
            cli_logs.push("  [NO SITES STORED IN CURRENT ROUTING TABLE]");
        }
        cli_logs.push("==================================================");
    }
    else if (cmd == "bounty") {
        trigger_route_navigation("bounty.vnet");
        cli_logs.push("[BOUNTY]: ROUTING TO CONTRACT DIRECTORY...");
    }
    else if (cmd == "crack") {
        if (args == "") { cli_logs.push("[ERROR]: Usage: crack <val>"); }
        else { vnet.send_to(client_sock, server_ip, server_port, "CRACK:" + args); }
    }
    else if (cmd == "cat") {
        if (args == "") { cli_logs.push("[ERROR]: Usage: cat <vfs_path>"); }
        else { vnet.send_to(client_sock, server_ip, server_port, "CAT:" + args); }
    }
    else if (cmd == "help") {
        cli_logs.push("========== THREE VICTORY GOALS ==========");
        cli_logs.push("  1. Root Breach    : win <k1> ... <k8> at terminal.vnet");
        cli_logs.push("  2. Grid Blackout  : overload 5 mutual pages (market, vault, terminal, crypto, hellroom)");
        cli_logs.push("  3. Economic Control: Reach 25.0 VCOIN and type 'takeover'");
        cli_logs.push("========== CYBERWARFARE & SENSING COMMANDS ==========");
        cli_logs.push("  connect <url>           - Navigate browser to target .vnet URL");
        cli_logs.push("  patch                   - [0.70 VCOIN | 120s CD] Emergency rebind new socket port");
        cli_logs.push("  sniffer                 - Toggle global UDP packet sniffer mode");
        cli_logs.push("  satscan                 - [0.55 VCOIN | 60s CD] Sweep network thermal radar (watchtower.vnet)");
        cli_logs.push("  freq <hz>               - Tune RF signal analyzer frequency");
        cli_logs.push("  decoy <url> <port>      - [0.10 VCOIN] Deploy ambush trap on node");
        cli_logs.push("  buy ice                 - [0.30 VCOIN] Purchase 1 layer of ICE");
        cli_logs.push("  win <k1> ... <k8>       - Submit all 8 key codes to breach vault");
        cli_logs.push("  takeover                - [25.0 VCOIN] Buy out network for victory");
        cli_logs.push("  overload <url>          - [1.50 VCOIN] Force site offline for 30s");
        cli_logs.push("  chat <msg> / msg <msg>  - Send real-time P2P message");
        cli_logs.push("  netscan                 - Discover active peers on current URL");
        cli_logs.push("  scan                    - [240s CD] Initiate deep subnet frequency sweep");
        cli_logs.push("  history                 - Display all known assigned routing nodes");
        cli_logs.push("  proxy <url> <node>      - [0.40 VCOIN | 120s CD] Route connection through intermediate proxy");
        cli_logs.push("  bounty                  - Quick jump to target bounty board");
        cli_logs.push("========== OFFENSIVE EXPLOIT COMMANDS ==========");
        cli_logs.push("  probe <port>            - [0.20 VCOIN |  NO CD] Probe target node defenses & state");
        cli_logs.push("  dos <port>              - [0.25 VCOIN | 15s CD] Freeze peer (3x = Drop Key)");
        cli_logs.push("  redirect <port> <url>   - [0.15 VCOIN | 10s CD] BGP Hijack peer browser");
        cli_logs.push("  snoop <port>            - [0.05 VCOIN |  5s CD] Interrogate target URL");
        cli_logs.push("  spike <port>            - [0.20 VCOIN | 12s CD] Force +35% threat trace");
        cli_logs.push("  overload <url>          - [1.50 VCOIN | 240s CD] Force target node offline for 30s");
        cli_logs.push("========== ECONOMY & UTILITY COMMANDS ==========");
        cli_logs.push("  mine                    - Mine +0.05 VCOIN at crypto.vnet");
        cli_logs.push("  shift <bits>            - Tune hardware bit-shift offset (0-15)");
        cli_logs.push("  vdec <offset>           - Decode active carrier key on terminal overlay");
        cli_logs.push("  cat /sys/config.txt     - Inspect config.txt hash key database");
        cli_logs.push("  inspect <url>           - Dump raw page source markup");
        cli_logs.push("  flush                   - [0.10 VCOIN] Lower trace level by -30%");
        cli_logs.push("  wallet                  - Display balance & trace stats");
        cli_logs.push("  clear                   - Wipe overlay terminal log buffer");
        cli_logs.push("==================================================");
    }
    else if (cmd == "clear") {
        cli_logs.clear();
        cli_logs = ["[OVERLAY TERMINAL BUFFER CLEARED]"];
    }
    else {
        cli_logs.push("[ERR]: UNKNOWN COMMAND '" + cmd + "'");
    }

    if (cli_logs.length() > 16) {
        cli_scroll_y = float64(cli_logs.length() - 16) * 22.0;
    }

    return null;
}

# ====================================================================
# MAIN ENGINE LOOP
# ====================================================================
while (vglib.running()) {
    # ================================================================
    # SERVER IP CONNECTION MENU
    # ================================================================
    # ================================================================
    # SERVER IP CONNECTION MENU (OPERATION COLD SIGNAL GATEWAY)
    # ================================================================
    # ================================================================
    # SERVER IP CONNECTION MENU (OPERATION COLD SIGNAL GATEWAY - ENHANCED)
    # ================================================================
    if (is_in_ip_menu == 1) {
        run_time = run_time + 0.016;
        
        m_pos   = vglib.mouse_pos();
        mx :: Float64 = float64(m_pos[0]);
        my :: Float64 = float64(m_pos[1]);
        m_down  :: Int64 = vglib.mouse_down(vglib.MOUSE_LEFT);
        m_click :: Int64 = (m_down == 1 && mouse_was_down == 0) ? 1 : 0;
        mouse_was_down   = m_down;

        if (m_click == 1) {
            ip_box_focused = (mx >= 440.0 && mx <= 840.0 && my >= 430.0 && my <= 470.0) ? 1 : 0;
        }

        ch = vglib.get_char();
        while (ch != "") {
            if (ip_box_focused == 1 && ip_input_buffer.length() < 25) {
                ip_input_buffer = ip_input_buffer + ch;
            }
            ch = vglib.get_char();
        }

        if (vglib.key_pressed(vglib.BACKSPACE) && ip_box_focused == 1 && ip_input_buffer.length() > 0) {
            ip_input_buffer = ip_input_buffer.substr(0, ip_input_buffer.length() - 1);
        }

        connect_btn_hover = (mx >= 510.0 && mx <= 770.0 && my >= 510.0 && my <= 550.0) ? 1 : 0;
        if (vglib.key_pressed(vglib.ENTER) || (connect_btn_hover == 1 && m_click == 1)) {
            if (ip_input_buffer.length() > 0) {
                server_ip = ip_input_buffer;
            }
            is_in_ip_menu = 0;
            
            current_url = "vnet.dir";
            page_body   = load_page(current_url);
            vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
        }

        pulse_alpha :: Float64 = vmath.sin(run_time * 6.0) * 0.5 + 0.5;
        pulse_col   = (pulse_alpha > 0.5) ? COLOR_BLOOD : COLOR_AMBER;

        vglib.begin();
        vglib.clear(COLOR_BLACK);

        # ============================================================
        # ANIMATED BACKGROUND: WAVE GRADIENT & FLOATING VECTOR NODES
        # ============================================================
        # Dynamic shifting background gradient strips
        through bg_i :: 0..19 -> loop {
            strip_y :: Float64 = float64(bg_i * 40);
            wave_col_val :: Int64 = int64(vmath.sin(run_time * 2.0 + float64(bg_i) * 0.3) * 20.0 + 25.0);
            vglib.rect(0, strip_y, 1280, 40, vglib.rgba(wave_col_val, 2, 8, 255));
        };

        # Floating P2P Nodes and Interconnecting Laser Grid Lines
        through n_i :: 0..7 -> loop {
            node_x :: Float64 = vmath.fmod(float64(n_i * 160) + run_time * 30.0, 1380.0) - 50.0;
            node_y :: Float64 = 100.0 + vmath.sin(run_time * 1.5 + float64(n_i)) * 60.0;
            node_y2 :: Float64 = 650.0 + vmath.cos(run_time * 1.8 + float64(n_i)) * 50.0;

            # Draw laser grid link vectors across nodes
            if (n_i < 7) {
                next_x :: Float64 = vmath.fmod(float64((n_i + 1) * 160) + run_time * 30.0, 1380.0) - 50.0;
                next_y :: Float64 = 100.0 + vmath.sin(run_time * 1.5 + float64(n_i + 1)) * 60.0;
                vglib.line(node_x, node_y, next_x, next_y, vglib.rgba(140, 20, 40, 90));
            }

            vglib.rect(node_x, node_y, 6, 6, COLOR_BLOOD);
            vglib.rect(node_x + 10.0, node_y2, 4, 4, COLOR_TOXIC);
        };

        # Occasional glitch jitter offsets for the chassis box
        jitter_x :: Float64 = (pulse_alpha > 0.85) ? (vmath.sin(run_time * 40.0) * 4.0) : 0.0;
        jitter_y :: Float64 = (pulse_alpha > 0.85) ? (vmath.cos(run_time * 35.0) * 3.0) : 0.0;

        # ============================================================
        # CENTRAL TERMINAL CHASSIS
        # ============================================================
        box_x :: Float64 = 240.0 + jitter_x;
        box_y :: Float64 = 140.0 + jitter_y;

        vglib.rect(box_x, box_y, 800, 520, COLOR_PANEL);
        vglib.line(box_x, box_y, box_x + 800.0, box_y, COLOR_BLOOD);
        vglib.line(box_x + 800.0, box_y, box_x + 800.0, box_y + 520.0, COLOR_BLOOD);
        vglib.line(box_x + 800.0, box_y + 520.0, box_x, box_y + 520.0, COLOR_BLOOD);
        vglib.line(box_x, box_y + 520.0, box_x, box_y, COLOR_BLOOD);

        # ============================================================
        # DYNAMICALLY CENTERED TEXT RENDERING
        # ============================================================
        
        # 1. Title Header
        h1_str :: String = "VYNE SHADOWOS v9.5 // OPERATION COLD SIGNAL";
        h1_sz  :: Array  = vglib.measure_text(vcr_font, h1_str, 14.0);
        h1_x   :: Float64 = 640.0 - (float64(h1_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, h1_str, h1_x, box_y + 25.0, 14, COLOR_BLOOD);

        # 2. Sub-header Port Status
        h2_str :: String = "SUBTERRANEAN RELAY UPLINK TERMINAL [PORT: " + string(my_port) + "]";
        h2_sz  :: Array  = vglib.measure_text(vcr_font, h2_str, 11.0);
        h2_x   :: Float64 = 640.0 - (float64(h2_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, h2_str, h2_x, box_y + 50.0, 11, COLOR_CYAN);

        vglib.line(box_x + 20.0, box_y + 70.0, box_x + 780.0, box_y + 70.0, COLOR_BORDER);

        # Lore Teaser / Context Log Box
        vglib.rect(box_x + 20.0, box_y + 80.0, 760, 180, COLOR_BLACK);
        vglib.line(box_x + 20.0, box_y + 80.0, box_x + 780.0, box_y + 80.0, COLOR_BORDER);
        vglib.line(box_x + 780.0, box_y + 80.0, box_x + 780.0, box_y + 260.0, COLOR_BORDER);
        vglib.line(box_x + 780.0, box_y + 260.0, box_x + 20.0, box_y + 260.0, COLOR_BORDER);
        vglib.line(box_x + 20.0, box_y + 260.0, box_x + 20.0, box_y + 80.0, COLOR_BORDER);

        # Centered Classification Notice
        class_str :: String = "[CLASSIFIED TOP SECRET] ANKARA SECTOR 09 BLACK-SITE RELAY";
        class_sz  :: Array  = vglib.measure_text(vcr_font, class_str, 10.0);
        class_x   :: Float64 = 640.0 - (float64(class_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, class_str, class_x, box_y + 95.0, 10, pulse_col);

        # Lore Terminal Log Outputs
        vglib.text_ex(vcr_font, "> ANALOG ECHO INTERCEPTED FROM signal0.vnet VIA COPPER BUS", box_x + 40.0, box_y + 118.0, 10, COLOR_GHOST);
        vglib.text_ex(vcr_font, "> SAT-99 ORBITAL OPTICS LOCKED ON LOCAL CRT GLARE COORDINATES", box_x + 40.0, box_y + 141.0, 10, COLOR_GHOST);
        vglib.text_ex(vcr_font, "> MORPHOGENIC STATIC LEAKING THROUGH VFS MEMORY ALLOCATION STACKS", box_x + 40.0, box_y + 164.0, 10, COLOR_GHOST);
        vglib.text_ex(vcr_font, "> KAGUYA TRIAL EXPLOIT HOOKS READY. AWAITING UDP HANDSHAKE...", box_x + 40.0, box_y + 187.0, 10, COLOR_TOXIC);

        # Centered Warning Notice
        warn_str :: String = "WARNING: UNREGISTERED EYE CONTACT DETECTED THROUGH MONITOR GLASS";
        warn_sz  :: Array  = vglib.measure_text(vcr_font, warn_str, 10.0);
        warn_x   :: Float64 = 640.0 - (float64(warn_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, warn_str, warn_x, box_y + 220.0, 10, COLOR_BLOOD);

        # 3. IP Prompt Label (Centered)
        ip_lbl_str :: String = "TARGET GATEWAY HOST / SERVER IP ADDRESS:";
        ip_lbl_sz  :: Array  = vglib.measure_text(vcr_font, ip_lbl_str, 11.0);
        ip_lbl_x   :: Float64 = 640.0 - (float64(ip_lbl_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, ip_lbl_str, ip_lbl_x, box_y + 272.0, 11, COLOR_CYAN);

        # Input Box
        vglib.rect(box_x + 200.0, box_y + 290.0, 400, 40, COLOR_BLACK);
        vglib.line(box_x + 200.0, box_y + 290.0, box_x + 600.0, box_y + 290.0, ip_box_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        vglib.line(box_x + 600.0, box_y + 290.0, box_x + 600.0, box_y + 330.0, ip_box_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        vglib.line(box_x + 600.0, box_y + 330.0, box_x + 200.0, box_y + 330.0, ip_box_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        vglib.line(box_x + 200.0, box_y + 330.0, box_x + 200.0, box_y + 290.0, ip_box_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        
        # 4. Centered IP Input Text
        display_ip_input :: String = ip_input_buffer + ((vmath.fmod(run_time * 2.0, 1.0) > 0.5) ? "_" : "");
        ip_txt_sz  :: Array  = vglib.measure_text(vcr_font, display_ip_input, 12.0);
        ip_txt_x   :: Float64 = 640.0 - (float64(ip_txt_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, display_ip_input, ip_txt_x, box_y + 303.0, 12, COLOR_TOXIC);

        # Connect Button
        btn_x :: Float64 = box_x + 270.0;
        btn_y :: Float64 = box_y + 370.0;
        vglib.rect(btn_x, btn_y, 260, 40, connect_btn_hover == 1 ? COLOR_BLOOD : COLOR_CLI_BG);
        vglib.line(btn_x, btn_y, btn_x + 260.0, btn_y, COLOR_BLOOD);
        vglib.line(btn_x + 260.0, btn_y, btn_x + 260.0, btn_y + 40.0, COLOR_BLOOD);
        vglib.line(btn_x + 260.0, btn_y + 40.0, btn_x, btn_y + 40.0, COLOR_BLOOD);
        vglib.line(btn_x, btn_y + 40.0, btn_x, btn_y, COLOR_BLOOD);

        # 5. Centered Button Label
        btn_str :: String = "INITIALIZE UPLINK";
        btn_sz  :: Array  = vglib.measure_text(vcr_font, btn_str, 11.0);
        btn_lbl_x :: Float64 = 640.0 - (float64(btn_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, btn_str, btn_lbl_x, btn_y + 14.0, 11, connect_btn_hover == 1 ? COLOR_BLACK : COLOR_TOXIC);

        # 6. Footer Quotes & Instructions (Centered)
        f1_str :: String = "'THE FAMILY HAS NO IP ADDRESS BECAUSE THE FAMILY IS EVERY OPEN PORT.'";
        f1_sz  :: Array  = vglib.measure_text(vcr_font, f1_str, 9.0);
        f1_x   :: Float64 = 640.0 - (float64(f1_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, f1_str, f1_x, box_y + 470.0, 9, COLOR_AMBER);

        f2_str :: String = "PRESS [ENTER] TO ESTABLISH P2P UDP SOCKET HANDSHAKE WITH SERVER";
        f2_sz  :: Array  = vglib.measure_text(vcr_font, f2_str, 9.0);
        f2_x   :: Float64 = 640.0 - (float64(f2_sz[0]) / 2.0) + jitter_x;
        vglib.text_ex(vcr_font, f2_str, f2_x, box_y + 490.0, 9, COLOR_GHOST);

        # Scanline Overlay
        vglib.draw_scanlines(8.0, vglib.rgba(0, 0, 0, 90));

        vglib.end();
        continue;
    }

    run_time     = run_time + 0.016;
    cursor_blink = cursor_blink + 0.016;

    heartbeat_timer = heartbeat_timer + 0.016;
    if (heartbeat_timer >= 3.0) {
        heartbeat_timer = 0.0;
        vnet.send_to(client_sock, server_ip, server_port, "PING:" + current_url);
    }

    packet_decay_cd = packet_decay_cd + 0.016;
    if (packet_decay_cd >= 2.0) {
        packet_decay_cd = 0.0;
        if (recent_packets > 0) { recent_packets = recent_packets - 1; }
    }

    if (game_over_winner == 0) {
        if (cd_dos > 0.0)      { cd_dos = cd_dos - 0.016; }
        if (cd_redirect > 0.0) { cd_redirect = cd_redirect - 0.016; }
        if (cd_spike > 0.0)    { cd_spike = cd_spike - 0.016; }
        if (cd_snoop > 0.0)    { cd_snoop = cd_snoop - 0.016; }
        if (cd_mine > 0.0)     { cd_mine = cd_mine - 0.016; }
        if (cd_patch > 0.0)    { cd_patch = cd_patch - 0.016; }
        if (cd_scan > 0.0)     { cd_scan = cd_scan - 0.016; }
        if (cd_decoy > 0.0)    { cd_decoy = cd_decoy - 0.016; }
        if (cd_proxy > 0.0)    { cd_proxy = cd_proxy - 0.016; }
        if (cd_overload > 0.0) { cd_overload = cd_overload - 0.016; }
        if (cd_satscan > 0.0)  { cd_satscan = cd_satscan - 0.016; }
        if (active_down_timer > 0.0) {
            active_down_timer = active_down_timer - 0.016;
            if (active_down_timer <= 0.0) {
                active_down_url = "";
            }
        }

        if (sniffer_mode == 1) {
            sniffer_upkeep_timer = sniffer_upkeep_timer + 0.016;
            if (sniffer_upkeep_timer >= 4.0) {
                sniffer_upkeep_timer = 0.0;
                if (btc_balance >= 0.02) {
                    btc_balance = btc_balance - 0.02;
                    trace_level = int64(vmath.clamp(float64(trace_level + 3), 0.0, 100.0));
                    cli_logs.push("[SNIFFER_UPKEEP]: -0.02 VCOIN | WARNING: PROMISCUOUS MODE INCREASING TRACE (+3%)");
                } else {
                    sniffer_mode = 0;
                    cli_logs.push("[POWER FAULT]: INSUFFICIENT VCOIN RESERVES! PACKET SNIFFER FORCED OFFLINE.");
                }
            }
        }

        if (is_connecting == 1) {
            connection_timer = connection_timer + 0.016;
            if (connection_timer >= target_connection_time) {
                is_connecting = 0;
                current_url   = pending_url;
                page_body     = load_page(current_url);
                scroll_y      = 0.0;

                vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
                trace_level = int64(vmath.clamp(float64(trace_level + 5), 0.0, 100.0));
                glitch_trigger = 0.4;
                cli_logs.push("[TOR_ROUTE]: CONNECTION ESTABLISHED TO " + current_url);
            }
        }

        canonical_curr :: String = extract_canonical_name(current_url);

        if (canonical_curr == "hellroom.vnet") {
            trace_level = 0;
        } else {
            passive_trace_cd = passive_trace_cd + 0.016;
            if (passive_trace_cd >= 3.0) {
                passive_trace_cd = 0.0;
                if (canonical_curr == "market.vnet" || canonical_curr == "vault.vnet" || 
                    canonical_curr == "terminal.vnet" || canonical_curr == "forum.vnet" || 
                    canonical_curr == "crypto.vnet" || canonical_curr == "bounty.vnet" || 
                    canonical_curr == "redroom.vnet") {
                    
                    trace_level = int64(vmath.clamp(float64(trace_level + 2), 0.0, 100.0));
                }
            }
        }

        if (trace_level >= 100) {
            trace_level = 20;
            btc_balance = vmath.clamp(btc_balance - 0.30, 0.0, 999.0);
            is_connecting = 0;
            current_url   = "vnet.dir";
            input_url     = "vnet.dir";
            page_body     = load_page(current_url);
            
            vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
            vnet.send_to(client_sock, server_ip, server_port, "TRACE_BUST:LOCKOUT");
            
            glitch_trigger = 0.9;
            cli_logs.push("[EMERGENCY_ICE]: TRACE REACHED 100%! NODE DISCONNECTED & FINED 0.30 VCOIN!");
        }

        if (dos_timer > 0.0) {
            dos_timer = dos_timer - 0.016;
            glitch_trigger = 0.5;
        }

        m_pos         = vglib.mouse_pos();
        mx :: Float64 = float64(m_pos[0]);
        my :: Float64 = float64(m_pos[1]);
        
        m_down  :: Int64 = vglib.mouse_down(vglib.MOUSE_LEFT);
        m_click :: Int64 = (m_down == 1 && mouse_was_down == 0 && dos_timer <= 0.0 && is_connecting == 0) ? 1 : 0;
        mouse_was_down   = m_down;

        if (vglib.key_pressed(vglib.TAB) && dos_timer <= 0.0) {
            cli_overlay_open = (cli_overlay_open == 1) ? 0 : 1;

            if (bot_stalk_active == 1) {
                cli_overlay_open = 0;
            }

            glitch_trigger = 0.15;
        }

        wheel = vglib.mouse_wheel();
        if (wheel != 0.0 && dos_timer <= 0.0) {
            if (cli_overlay_open == 1) {
                cli_scroll_y = vmath.clamp(cli_scroll_y - (wheel * 22.0), 0.0, 3000.0);
            } else if (mx >= 930.0 && mx <= 1260.0 && my >= 80.0 && my <= 750.0) {
                feed_scroll_y = vmath.clamp(feed_scroll_y - (wheel * 22.0), 0.0, 3000.0);
            } else {
                scroll_y = vmath.clamp(scroll_y - (wheel * 28.0), 0.0, 2500.0);
            }
        }

        if (cli_overlay_open == 1 && dos_timer <= 0.0) {
            ch = vglib.get_char();
            while (ch != "") {
                cli_input_buffer = cli_input_buffer + ch;
                ch = vglib.get_char();
            }

            if (vglib.key_pressed(vglib.BACKSPACE) && cli_input_buffer.length() > 0) {
                cli_input_buffer = cli_input_buffer.substr(0, cli_input_buffer.length() - 1);
            }

            if (vglib.key_pressed(vglib.ENTER) && cli_input_buffer.length() > 0) {
                dispatch_cli_command(cli_input_buffer);
                cli_input_buffer = "";
            }
        } else if (dos_timer <= 0.0 && is_connecting == 0) {
            if (m_click == 1) {
                
                if (mx >= 870.0 && mx <= 920.0 && my >= 12.0 && my <= 48.0) {
                    trigger_route_navigation("vnet.dir");
                }

                url_focused = (mx >= 120.0 && mx <= 865.0 && my >= 12.0 && my <= 48.0) ? 1 : 0;
                
                if (current_url == "hellroom.vnet") {
                    handle_focused = (mx >= 45.0 && mx <= 225.0 && my >= 135.0 && my <= 167.0) ? 1 : 0;
                    chat_focused   = (mx >= 235.0 && mx <= 755.0 && my >= 135.0 && my <= 167.0) ? 1 : 0;
                    
                    if (mx >= 765.0 && mx <= 880.0 && my >= 135.0 && my <= 167.0) {
                        if (chat_input_buffer.length() > 0) {
                            vnet.send_to(client_sock, server_ip, server_port, "CHAT:" + player_handle + ":" + chat_input_buffer);
                            chat_input_buffer = "";
                            glitch_trigger = 0.2;
                        }
                    }
                } else {
                    handle_focused = 0;
                    chat_focused   = 0;
                }
            }

            if (url_focused == 1) {
                ch = vglib.get_char();
                while (ch != "") {
                    input_url = input_url + ch;
                    ch = vglib.get_char();
                }

                if (vglib.key_pressed(vglib.BACKSPACE) && input_url.length() > 0) {
                    input_url = input_url.substr(0, input_url.length() - 1);
                }

                if (vglib.key_pressed(vglib.ENTER) && input_url.length() > 0) {
                    url_focused = 0;
                    trigger_route_navigation(input_url);
                }
            }
            else if (handle_focused == 1) {
                ch = vglib.get_char();
                while (ch != "") {
                    if (player_handle.length() < 14 && ch != ":" && ch != " ") {
                        player_handle = player_handle + ch;
                    }
                    ch = vglib.get_char();
                }

                if (vglib.key_pressed(vglib.BACKSPACE) && player_handle.length() > 0) {
                    player_handle = player_handle.substr(0, player_handle.length() - 1);
                }

                if (vglib.key_pressed(vglib.ENTER)) {
                    handle_focused = 0;
                    chat_focused   = 1;
                }
            }
            else if (chat_focused == 1) {
                ch = vglib.get_char();
                while (ch != "") {
                    chat_input_buffer = chat_input_buffer + ch;
                    ch = vglib.get_char();
                }

                if (vglib.key_pressed(vglib.BACKSPACE) && chat_input_buffer.length() > 0) {
                    chat_input_buffer = chat_input_buffer.substr(0, chat_input_buffer.length() - 1);
                }

                if (vglib.key_pressed(vglib.ENTER) && chat_input_buffer.length() > 0) {
                    vnet.send_to(client_sock, server_ip, server_port, "CHAT:" + player_handle + ":" + chat_input_buffer);
                    chat_input_buffer = "";
                    glitch_trigger = 0.2;
                }
            }
        }
    }

    packet_in :: Array = vnet.recv_from(client_sock);
    while (packet_in.length() >= 3) {
        net_msg :: String = string(packet_in[0]);
        server_status = net_msg;
        glitch_trigger = 0.2;
        recent_packets = recent_packets + 1;

        if (net_msg.length() > 9 && net_msg.substr(0, 9) == "KEY_SYNC:") {
            sync_payload :: String = net_msg.substr(9, net_msg.length() - 9);
            tokens :: Array = [];
            rem_sync :: String = sync_payload;
            while (rem_sync.length() > 0) {
                parts = vnet.parse_package(rem_sync, ":");
                tokens.push(parts[0]);
                rem_sync = string(parts[1]);
            }

            if (tokens.length() >= 31) {
                through k_t :: 0..7 -> loop {
                    session_enc_keys[k_t] = int64(tokens[k_t]);
                    session_locs[k_t]     = string(tokens[k_t + 8]);
                };
                
                my_assigned_sites.clear();
                through s_t :: 16..31 -> loop {
                    if (s_t < tokens.length()) {
                        my_assigned_sites.push(string(tokens[s_t]));
                    }
                };

                page_body = load_page(current_url);
            }
        }
        else if (net_msg.length() > 24 && net_msg.substr(0, 24) == "EXPLOIT:SITE_OVERLOADED:") {
            down_site :: String = clean_str(net_msg.substr(24, net_msg.length() - 24));
            if (down_site.length() >= 7 && down_site.substr(0, 7) == "vnet://") {
                down_site = down_site.substr(7, down_site.length() - 7);
            }
            parsed_down_url :: String = clean_str(down_site);
            
            if (active_down_url != parsed_down_url || active_down_timer <= 0.0) {
                active_down_url = parsed_down_url;
                active_down_timer = 30.0;
                cli_logs.push("[CRITICAL]: " + active_down_url + " HAS BEEN FORCED OFFLINE FOR 30 SECONDS!");
            }
            glitch_trigger = 0.8;
        }
        else if (net_msg == "EXPLOIT:BOT_STALK") {
            bot_stalk_active = 1;
            cli_overlay_open = 0;
            glitch_trigger   = 1.0;
        }
        else if (net_msg.length() >= 15 && net_msg.substr(0, 15) == "EXPLOIT:WINNER:") {
            raw_win_str :: String = net_msg.substr(15, net_msg.length() - 15);
            
            p1 = vnet.parse_package(raw_win_str, ":");
            winner_port = string(p1[0]);
            
            p2 = vnet.parse_package(string(p1[1]), ":");
            win_mode_str  = (string(p2[0]) != "") ? string(p2[0]) : "KEYS";
            winner_handle = (string(p2[1]) != "") ? string(p2[1]) : ("PORT_" + winner_port);

            game_over_winner = 1;
            glitch_trigger   = 1.0;
            cli_overlay_open = 0;
            win_anim_timer   = 0.0;
        }
        else if (net_msg.length() > 12 && net_msg.substr(0, 12) == "SCAN_RESULT:") {
            discovered_site :: String = clean_str(net_msg.substr(12, net_msg.length() - 12));
            
            already_known :: Int64 = 0;
            if (my_assigned_sites.length() > 0) {
                through s_i :: 0..(my_assigned_sites.length() - 1) -> loop {
                    if (string(my_assigned_sites[s_i]) == discovered_site) {
                        already_known = 1;
                        break;
                    }
                };
            }

            if (already_known == 0) {
                my_assigned_sites.push(discovered_site);
                cli_logs.push("[SCAN SUCCESS]: DISCOVERED NEW SUBNET NODE -> vnet://" + discovered_site);
                
                if (current_url == "vnet.dir") {
                    page_body = load_page(current_url);
                }
            } else {
                cli_logs.push("[SCAN TELEMETRY]: FREQUENCY RE-DETECTED KNOWN NODE -> vnet://" + discovered_site);
            }
            glitch_trigger = 0.5;
        }
        else if (net_msg.length() > 14 && net_msg.substr(0, 14) == "PATCH_SUCCESS:") {
            new_assigned_port :: Int64 = int64(net_msg.substr(14, net_msg.length() - 14));
            
            vnet.close(client_sock);
            my_port = new_assigned_port;
            client_sock = vnet.udp_socket(my_port);
            
            glitch_trigger = 0.8;
            cli_logs.push("[SECURITY]: PORT REBOUND SUCCESSFUL! NEW ACTIVE PORT: " + string(my_port));
            
            vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
        }
        else if (net_msg == "EXPLOIT_REFUND") {
            btc_balance = btc_balance + 0.20;
            cd_dos = 0.0;
            cli_logs.push("[DOS]: HIT DECOY/EMPTY SOCKET -> 0.20 VCOIN REFUNDED (-0.05 NET) & COOLDOWN BYPASSED.");
        }
        else if (net_msg == "EXPLOIT:MARKET_OVERLOADED") {
            market_is_down = 1;
            market_down_timer = 30.0;
            glitch_trigger = 0.8;
            cli_logs.push("[CRITICAL]: market.vnet HAS BEEN FORCED OFFLINE FOR 30 SECONDS!");
        }
        else if (net_msg.length() > 14 && net_msg.substr(0, 14) == "DECOY_TRIPPED:") {
            cli_logs.push("[DECOY ALERT]: " + net_msg.substr(14, net_msg.length() - 14));
            glitch_trigger = 0.6;
        }
        else if (net_msg.length() > 13 && net_msg.substr(0, 13) == "DOS_DROP_DIR:") {
            dropped_dir :: String = net_msg.substr(13, net_msg.length() - 13);
            cli_logs.push("[REWARD]: TARGET DIRECTORY EXFILTRATED! (2 DOS HITS)");
            
            rem_dir :: String = dropped_dir;
            while (rem_dir.length() > 0) {
                parts = vnet.parse_package(rem_dir, ":");
                site = string(parts[0]);
                if (site != "") { cli_logs.push("  -> " + site); }
                rem_dir = string(parts[1]);
            }
            
            glitch_trigger = 0.5;
        }
        else if (net_msg.length() > 9 && net_msg.substr(0, 9) == "DOS_DROP:") {
            cli_logs.push("[REWARD]: " + net_msg.substr(9, net_msg.length() - 9));
            btc_balance = btc_balance + 0.20;
        }
        else if (net_msg.length() > 11 && net_msg.substr(0, 11) == "FEED_EVENT:") {
            feed_text :: String = net_msg.substr(11, net_msg.length() - 11);
            
            if (feed_text.substr(0, 8) == "[SNIFF]:") {
                if (sniffer_mode == 1) {
                    cli_logs.push(feed_text);
                }
            } else {
                vnet_feed_logs.push(feed_text);
            }
            
            if (vnet_feed_logs.length() > 12) {
                feed_scroll_y = float64(vnet_feed_logs.length() - 12) * 20.0;
            }
        } else {
            cli_logs.push("[NET_IN] " + net_msg);

            if (net_msg == "EXPLOIT:DOS") {
                if (ice_charges > 0) {
                    ice_charges = ice_charges - 1;
                    glitch_trigger = 0.1;
                    cli_logs.push("[ICE_DEFENSE]: INBOUND DOS BLOCKED BY ICE FIREWALL! (" + string(ice_charges) + "/3 REMAINING)");
                    if (current_url == "market.vnet") { page_body = load_page(current_url); }
                } else {
                    dos_timer = 8.0;
                    glitch_trigger = 0.8;
                    cli_logs.push("[CRITICAL_ALERT] INBOUND DOS EXPLOIT RECEIVED! SYSTEM FROZEN FOR 8s!");
                }
            }
            else if (net_msg.length() > 17 && net_msg.substr(0, 17) == "EXPLOIT:REDIRECT:") {
                forced_dest :: String = net_msg.substr(17, net_msg.length() - 17);
                if (ice_charges > 0) {
                    ice_charges = ice_charges - 1;
                    glitch_trigger = 0.1;
                    cli_logs.push("[ICE_DEFENSE]: BGP HIJACK TO " + forced_dest + " BLOCKED BY ICE FIREWALL!");
                    if (current_url == "market.vnet") { page_body = load_page(current_url); }
                } else {
                    is_connecting = 0;
                    current_url = forced_dest;
                    input_url   = forced_dest;
                    page_body   = load_page(current_url);
                    scroll_y    = 0.0;
                    glitch_trigger = 0.5;
                    cli_logs.push("[WARNING] BGP ROUTE HIJACK DETECTED! FORCED TO " + forced_dest);
                }
            }
            else if (net_msg == "EXPLOIT:TRACE_SPIKE") {
                if (ice_charges > 0) {
                    ice_charges = ice_charges - 1;
                    glitch_trigger = 0.1;
                    cli_logs.push("[ICE_DEFENSE]: REVERSE TRACE SPIKE ABSORBED BY ICE FIREWALL!");
                    if (current_url == "market.vnet") { page_body = load_page(current_url); }
                } else {
                    trace_level = int64(vmath.clamp(float64(trace_level + 35), 0.0, 100.0));
                    glitch_trigger = 0.4;
                    cli_logs.push("[WARNING] REVERSE TRACE PULSE DETECTED! TRACE LEVEL +35%");
                }
            }
            else if (net_msg.length() > 10 && net_msg.substr(0, 10) == "FILE_DATA:") {
                btc_balance = btc_balance + 0.50;
                cli_logs.push("[REWARD]: EXFILTRATED DATA SOLD ON BLACK MARKET! +0.50 VCOIN");
            }
        }
        packet_in = vnet.recv_from(client_sock);
    }

    if (glitch_trigger > 0.0) { glitch_trigger = glitch_trigger - 0.016; }

    pulse_val :: Float64 = vmath.sin(run_time * 5.0) * 0.5 + 0.5;

    unmasked_val :: Int64 = active_raw_payload - bit_shift_offset;
    if (unmasked_val < 0) {
        unmasked_val = unmasked_val + 100000;
    }

    decoded_val :: Int64 = decode_sector(active_raw_payload, bit_shift_offset);

    is_aligned  :: Int64 = ((unmasked_val % 37 == 0) && decoded_val >= 1000 && decoded_val <= 9999) ? 1 : 0;

    hz_delta    :: Float64 = vmath.abs(freq_tuner - 18.0);
    is_resonant :: Int64   = (hz_delta <= 0.5) ? 1 : 0;

    if (active_raw_payload > 0 && is_aligned == 1 && is_resonant == 1) {
        lock_progress = vmath.clamp(lock_progress + (0.016 / 1.5), 0.0, 1.0); # Charges in ~1.5s
    } else {
        lock_progress = vmath.clamp(lock_progress - (0.016 * 2.0), 0.0, 1.0); # Decays rapidly
    }

    # ================================================================
    # RENDER ENGINE
    # ================================================================
    vglib.begin();
        vglib.clear(COLOR_BLACK);

        jitter_x :: Float64 = (glitch_trigger > 0.0) ? (vmath.sin(run_time * 50.0) * (glitch_trigger * 12.0)) : 0.0;
        jitter_y :: Float64 = (glitch_trigger > 0.0) ? (vmath.cos(run_time * 30.0) * (glitch_trigger * 10.0)) : 0.0;

        through bg_strip :: 0..19 -> loop {
            s_y :: Float64 = float64(bg_strip * 40);
            bg_val :: Int64 = int64(vmath.sin(run_time * 1.5 + float64(bg_strip) * 0.2) * 12.0 + 26.0);
            vglib.rect(0, s_y, 1280, 40, vglib.rgba(bg_val, 3, 7, 255));
        };

        vglib.rect(0 + jitter_x, 0 + jitter_y, 1280, 60, COLOR_PANEL);
        vglib.line(0, 60, 1280, 60, COLOR_BORDER);
        vglib.text_ex(vcr_font, "VNET", 45 + jitter_x, 22, 14, COLOR_BLOOD);

        # ================================================================
        # TOP BAR: URL BAR & HOME BUTTON
        # ================================================================
        vglib.rect(120 + jitter_x, 12 + jitter_y, 745, 36, COLOR_URLBAR);
        vglib.line(120 + jitter_x, 12 + jitter_y, 865 + jitter_x, 12 + jitter_y, url_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        display_url_str :: String = "vnet://" + (is_connecting == 1 ? pending_url : (url_focused == 1 ? input_url : current_url));
        vglib.text_ex(vcr_font, display_url_str, 135 + jitter_x, 23, 12, url_focused == 1 ? COLOR_TOXIC : COLOR_CYAN);

        home_btn_x :: Float64 = 870.0 + jitter_x;
        home_btn_y :: Float64 = 12.0 + jitter_y;
        home_hover :: Int64   = (cli_overlay_open == 0 && dos_timer <= 0.0 && is_connecting == 0 && mx >= 870.0 && mx <= 920.0 && my >= 12.0 && my <= 48.0) ? 1 : 0;

        vglib.rect(home_btn_x, home_btn_y, 50, 36, home_hover == 1 ? COLOR_BLOOD : COLOR_URLBAR);
        vglib.line(home_btn_x, home_btn_y, home_btn_x + 50.0, home_btn_y, COLOR_BORDER);
        vglib.line(home_btn_x + 50.0, home_btn_y, home_btn_x + 50.0, home_btn_y + 36.0, COLOR_BORDER);
        vglib.line(home_btn_x + 50.0, home_btn_y + 36.0, home_btn_x, home_btn_y + 36.0, COLOR_BORDER);
        vglib.line(home_btn_x, home_btn_y + 36.0, home_btn_x, home_btn_y, COLOR_BORDER);
        vglib.text_ex(vcr_font, "DIR", home_btn_x + 12.0, home_btn_y + 11.0, 11, home_hover == 1 ? COLOR_BLACK : COLOR_TOXIC);

        vglib.rect(940 + jitter_x, 12 + jitter_y, 320, 36, COLOR_PANEL);
        btc_str :: String = "VCOIN: " + string(vmath.round(btc_balance * 100.0) / 100.0) + " VCOIN";
        vglib.text_ex(vcr_font, btc_str, 955 + jitter_x, 23, 11, COLOR_TOXIC);

        vglib.rect(20 + jitter_x, 80 + jitter_y, 890, 670, COLOR_PANEL);
        vglib.line(20, 80, 910, 80, COLOR_BORDER);

        # ------------------------------------------------------------
        # 4. SYSTEM THREAT RADAR (RIGHT PANEL)
        # ------------------------------------------------------------
        vglib.rect(930 + jitter_x, 80 + jitter_y, 330, 670, COLOR_PANEL);
        vglib.line(930, 80, 1260, 80, COLOR_BORDER);
        vglib.line(1260, 80, 1260, 750, COLOR_BORDER);
        vglib.line(1260, 750, 930, 750, COLOR_BORDER);
        vglib.line(930, 750, 930, 80, COLOR_BORDER);

        vglib.text_ex(vcr_font, "SYSTEM THREAT RADAR", 945 + jitter_x, 95, 11, COLOR_AMBER);
        vglib.line(945, 110, 1245, 110, COLOR_BORDER);

        # Trace Level Gauge
        vglib.text_ex(vcr_font, "TRACE LEVEL GAUGE:", 945 + jitter_x, 120, 10, COLOR_CYAN);
        vglib.rect(945 + jitter_x, 136, 300, 14, COLOR_BLACK);
        bar_w :: Float64 = vmath.clamp((float64(trace_level) / 100.0) * 300.0, 4.0, 300.0);
        vglib.rect(945 + jitter_x, 136, bar_w, 14, (trace_level > 70) ? COLOR_BLOOD : COLOR_AMBER);

        # Pulsing Border for High Trace
        trace_border_col = (trace_level > 70 && pulse_val > 0.5) ? COLOR_BLOOD : COLOR_BORDER;
        vglib.line(945 + jitter_x, 136, 1245 + jitter_x, 136, trace_border_col);
        vglib.line(1245 + jitter_x, 136, 1245 + jitter_x, 150, trace_border_col);
        vglib.line(1245 + jitter_x, 150, 945 + jitter_x, 150, trace_border_col);
        vglib.line(945 + jitter_x, 150, 945 + jitter_x, 136, trace_border_col);

        vglib.text_ex(vcr_font, string(trace_level) + "% TRACED BY PEERS", 945 + jitter_x, 155, 10, COLOR_AMBER);

        vglib.line(945, 172, 1245, 172, COLOR_BORDER);
        vglib.text_ex(vcr_font, "RF TUNER (" + string(int64(freq_tuner)) + "Hz) SIGNAL WAVE:", 945 + jitter_x, 182, 10, COLOR_CYAN);

        # RF Tuner Grid Background
        through grid_line :: 0..2 -> loop {
            g_y :: Float64 = 198.0 + float64(grid_line * 12);
            vglib.line(945 + jitter_x, g_y, 1245 + jitter_x, g_y, vglib.rgba(20, 30, 40, 255));
        };

        wave_amp :: Float64 = vmath.clamp(6.0 + float64(recent_packets * 2), 8.0, 20.0);
        through rx :: 0..28 -> loop {
            wave_y :: Float64 = 209.0 + vmath.sin(run_time * (freq_tuner * 0.5) + float64(rx) * 0.4) * wave_amp;
            vglib.rect(945.0 + float64(rx * 10) + jitter_x, wave_y, 6, 6, (recent_packets > 3) ? COLOR_BLOOD : COLOR_TOXIC);
        };

        vglib.line(945, 235, 1245, 235, COLOR_BORDER);
        vglib.text_ex(vcr_font, "VFS MEMORY ALIGNMENT SCOPE", 945 + jitter_x, 245, 10, COLOR_CYAN);

        if (active_raw_payload > 0) {
            vglib.rect(945 + jitter_x, 258, 300, 20, COLOR_BLACK);
            
            if (lock_progress >= 1.0) {
                vglib.text_ex(vcr_font, ">>> KEY DETECTED <<< [OFFSET: " + string(bit_shift_offset) + "]", 955 + jitter_x, 263, 10, COLOR_TOXIC);
            } else if (is_aligned == 1 && is_resonant == 1) {
                pct_str :: String = string(int64(lock_progress * 100.0)) + "%";
                vglib.text_ex(vcr_font, "TUNING SIGNAL... " + pct_str + " [HOLD RESONANCE]", 955 + jitter_x, 263, 10, COLOR_AMBER);
            } else if (is_aligned == 1) {
                vglib.text_ex(vcr_font, "BIT ALIGNED // TUNE RF FREQ (18.0Hz)", 955 + jitter_x, 263, 10, COLOR_AMBER);
            } else {
                vglib.text_ex(vcr_font, "CORRUPTED: " + string(active_raw_payload) + " [ALIGN OFF]", 955 + jitter_x, 263, 10, COLOR_BLOOD);
            }
        } else {
            vglib.text_ex(vcr_font, "SCOPE IDLE (NO SECTOR DETECTED)", 945 + jitter_x, 263, 10, COLOR_GHOST);
        }

        # Subnet Defense Status Section (Positioned Below VFS Scope)
        vglib.line(945, 284, 1245, 284, COLOR_BORDER);
        vglib.text_ex(vcr_font, "ICE SHIELD   : [" + string(ice_charges) + "/3] LAYERS", 945 + jitter_x, 292, 10, COLOR_TOXIC);
        vglib.text_ex(vcr_font, "PATCH REBIND : " + ((cd_patch > 0.0) ? (string(int64(cd_patch)) + "s") : "READY"), 945 + jitter_x, 308, 10, (cd_patch > 0.0) ? COLOR_AMBER : COLOR_TOXIC);
        vglib.line(945, 324, 1245, 324, COLOR_BORDER);

        # Feed Box Window (Shifted down to Y = 332)
        vglib.rect(945 + jitter_x, 332, 300, 400, COLOR_BLACK);
        feed_cnt = vnet_feed_logs.length();
        feed_start_y :: Float64 = 340.0 - feed_scroll_y;

        through f_idx :: 0..(feed_cnt - 1) -> loop {
            line_y :: Float64 = feed_start_y + (f_idx * 20.0);
            if (line_y >= 335.0 && line_y <= 715.0) {
                f_txt :: String = string(vnet_feed_logs[f_idx]);
                f_txt_truncated :: String = truncate_str(f_txt, 35);

                f_col = COLOR_CYAN;
                if (f_txt.substr(0, 12) == "[TRACE SPIKE]") { f_col = COLOR_AMBER; }
                if (f_txt.substr(0, 12) == "[DOS ATTACK]") { f_col = COLOR_BLOOD; }
                if (f_txt.substr(0, 12) == "[ICE LOCKOUT]") { f_col = COLOR_BLOOD; }
                if (f_txt.substr(0, 12) == "[BGP HIJACK]") { f_col = COLOR_TOXIC; }
                if (f_txt.substr(0, 18) == "[SOCKET MIGRATION]") { f_col = COLOR_TOXIC; }
                if (f_txt.substr(0, 13) == "[WHALE ALERT]") { f_col = COLOR_TOXIC; }
                if (f_txt.substr(0, 10) == "[HONEYPOT]") { f_col = COLOR_AMBER; }
                if (f_txt.substr(0, 14) == "[BLACK MARKET]") { f_col = COLOR_TOXIC; }
                if (f_txt.length() > 6 && f_txt.substr(0, 6) == "[CHAT]") { f_col = COLOR_TOXIC; }

                vglib.text_ex(vcr_font, f_txt_truncated, 950 + jitter_x, line_y, 9, f_col);
            }
        };
        if (is_connecting == 1) {
            # Window Container & Borders
            vglib.rect(40 + jitter_x, 140 + jitter_y, 850, 500, COLOR_BLACK);
            vglib.line(40 + jitter_x, 140 + jitter_y, 890 + jitter_x, 140 + jitter_y, COLOR_AMBER);
            vglib.line(890 + jitter_x, 140 + jitter_y, 890 + jitter_x, 640 + jitter_y, COLOR_AMBER);
            vglib.line(890 + jitter_x, 640 + jitter_y, 40 + jitter_x, 640 + jitter_y, COLOR_AMBER);
            vglib.line(40 + jitter_x, 640 + jitter_y, 40 + jitter_x, 140 + jitter_y, COLOR_AMBER);

            # Corner Aesthetic Reticles
            vglib.line(35 + jitter_x, 135 + jitter_y, 55 + jitter_x, 135 + jitter_y, COLOR_BLOOD);
            vglib.line(35 + jitter_x, 135 + jitter_y, 35 + jitter_x, 155 + jitter_y, COLOR_BLOOD);
            vglib.line(895 + jitter_x, 135 + jitter_y, 875 + jitter_x, 135 + jitter_y, COLOR_BLOOD);
            vglib.line(895 + jitter_x, 135 + jitter_y, 895 + jitter_x, 155 + jitter_y, COLOR_BLOOD);

            rem_s :: Int64 = int64(vmath.ceil(target_connection_time - connection_timer));
            p_ratio :: Float64 = vmath.clamp(connection_timer / target_connection_time, 0.02, 1.0);

            # Center X calculations based on the 850px container (40 + 850 / 2 = 465)
            center_x :: Float64 = 465.0 + jitter_x;

            # 1. Main Header with Pulse Color Shift
            c1_str :: String = "[ TOR PROXY CIRCUIT HANDSHAKE ACTIVE ]";
            c1_sz  :: Array  = vglib.measure_text(vcr_font, c1_str, 16.0);
            c1_x   :: Float64 = center_x - (float64(c1_sz[0]) / 2.0);
            head_col = (vmath.sin(run_time * 8.0) > 0.0) ? COLOR_AMBER : COLOR_BLOOD;
            vglib.text_ex(vcr_font, c1_str, c1_x, 200.0 + jitter_y, 16, head_col);

            # 2. Target URL Designation (Centered)
            c2_str :: String = "RESOLVING DESTINATION: vnet://" + pending_url;
            c2_sz  :: Array  = vglib.measure_text(vcr_font, c2_str, 13.0);
            c2_x   :: Float64 = center_x - (float64(c2_sz[0]) / 2.0);
            vglib.text_ex(vcr_font, c2_str, c2_x, 245.0 + jitter_y, 13, COLOR_CYAN);

            # 3. Dynamic Animated Vector Radar / Crosshair Reticle
            rot_off :: Float64 = run_time * 4.0;
            through r_i :: 0..3 -> loop {
                angle :: Float64 = rot_off + float64(r_i) * 1.5708;
                rx1 :: Float64 = center_x + vmath.cos(angle) * 35.0;
                ry1 :: Float64 = 310.0 + jitter_y + vmath.sin(angle) * 20.0;
                vglib.rect(rx1, ry1, 4, 4, COLOR_BLOOD);
            };

            # 4. Latency Status Buffer (Centered)
            c3_str :: String = "LATENCY BUFFER: " + string(rem_s) + "s REMAINING [HOPS: 3/3]";
            c3_sz  :: Array  = vglib.measure_text(vcr_font, c3_str, 11.0);
            c3_x   :: Float64 = center_x - (float64(c3_sz[0]) / 2.0);
            vglib.text_ex(vcr_font, c3_str, c3_x, 355.0 + jitter_y, 11, COLOR_TOXIC);
            
            # 5. Glowing Animated Progress Bar Container
            bar_x :: Float64 = center_x - 250.0; # 500px wide bar
            bar_y :: Float64 = 385.0 + jitter_y;
            vglib.rect(bar_x, bar_y, 500, 22, COLOR_PANEL);
            vglib.line(bar_x, bar_y, bar_x + 500.0, bar_y, COLOR_BORDER);
            vglib.line(bar_x + 500.0, bar_y, bar_x + 500.0, bar_y + 22.0, COLOR_BORDER);
            vglib.line(bar_x + 500.0, bar_y + 22.0, bar_x, bar_y + 22.0, COLOR_BORDER);
            vglib.line(bar_x, bar_y + 22.0, bar_x, bar_y, COLOR_BORDER);

            # Filled Progress Bar with Dynamic Scan Beam
            fill_w :: Float64 = 500.0 * p_ratio;
            vglib.rect(bar_x, bar_y, fill_w, 22, COLOR_TOXIC);

            # Leading Edge Pulse Line on Progress Bar
            if (fill_w > 5.0) {
                vglib.rect(bar_x + fill_w - 4.0, bar_y, 4, 22, COLOR_BLOOD);
            }

            # Progress Percentage Text Centered Over Bar
            pct_str :: String = string(int64(p_ratio * 100.0)) + "%";
            pct_sz  :: Array  = vglib.measure_text(vcr_font, pct_str, 11.0);
            pct_x   :: Float64 = center_x - (float64(pct_sz[0]) / 2.0);
            vglib.text_ex(vcr_font, pct_str, pct_x, bar_y + 4.0, 11, (p_ratio > 0.5) ? COLOR_BLACK : COLOR_CYAN);

            # 6. Animated Hex Memory Stream / Anonymizer Telemetry (Centered)
            hex_tick :: Int64 = int64(vmath.fmod(run_time * 20.0, 99.0));
            hex_str :: String = "0x88F9_NODE_HOP_OK // ENCRYPTING PACKET SUBNET SECTOR #" + string(hex_tick);
            hex_sz  :: Array  = vglib.measure_text(vcr_font, hex_str, 10.0);
            hex_x   :: Float64 = center_x - (float64(hex_sz[0]) / 2.0);
            vglib.text_ex(vcr_font, hex_str, hex_x, 435.0 + jitter_y, 10, COLOR_AMBER);

            # 7. Animated Glitching Footer Subtitle (Centered)
            glitch_noise :: Float64 = vmath.sin(run_time * 40.0) * 4.0;
            c4_str :: String = "SPOOFING MAC ADDRESS & MIRRORING PACKETS VIA ARCHIVAL.VNET...";
            c4_sz  :: Array  = vglib.measure_text(vcr_font, c4_str, 10.0);
            c4_x   :: Float64 = center_x - (float64(c4_sz[0]) / 2.0) + glitch_noise;
            vglib.text_ex(vcr_font, c4_str, c4_x, 470.0 + jitter_y, 10, COLOR_GHOST);
        } else if (clean_str(current_url) == active_down_url && active_down_timer > 0.0) {
            vglib.rect(40 + jitter_x, 140 + jitter_y, 850, 500, COLOR_BLACK);
            vglib.line(40 + jitter_x, 140 + jitter_y, 890 + jitter_x, 140 + jitter_y, COLOR_BLOOD);
            vglib.line(890 + jitter_x, 140 + jitter_y, 890 + jitter_x, 640 + jitter_y, COLOR_BLOOD);
            vglib.line(890 + jitter_x, 640 + jitter_y, 40 + jitter_x, 640 + jitter_y, COLOR_BLOOD);
            vglib.line(40 + jitter_x, 640 + jitter_y, 40 + jitter_x, 140 + jitter_y, COLOR_BLOOD);

            rem_down_s :: Int64 = int64(active_down_timer) + 1;
            center_x :: Float64 = 465.0 + jitter_x;

            err_head :: String = "[ 503 // SITE UNAVAILABLE - KERNEL / BGP DESYNC ]";
            err_sz   :: Array  = vglib.measure_text(vcr_font, err_head, 15.0);
            vglib.text_ex(vcr_font, err_head, center_x - (float64(err_sz[0]) / 2.0), 220.0 + jitter_y, 15, COLOR_BLOOD);

            sub_head :: String = "TARGET NODE: vnet://" + active_down_url + " IS UNREACHABLE";
            sub_sz   :: Array  = vglib.measure_text(vcr_font, sub_head, 12.0);
            vglib.text_ex(vcr_font, sub_head, center_x - (float64(sub_sz[0]) / 2.0), 265.0 + jitter_y, 12, COLOR_AMBER);

            rec_str :: String = "RECOVERY HANDSHAKE IN PROGRESS: " + string(rem_down_s) + "s";
            rec_sz  :: Array  = vglib.measure_text(vcr_font, rec_str, 11.0);
            vglib.text_ex(vcr_font, rec_str, center_x - (float64(rec_sz[0]) / 2.0), 320.0 + jitter_y, 11, COLOR_TOXIC);

            bar_x :: Float64 = center_x - 225.0;
            bar_y :: Float64 = 360.0 + jitter_y;
            vglib.rect(bar_x, bar_y, 450, 20, COLOR_PANEL);
            vglib.line(bar_x, bar_y, bar_x + 450.0, bar_y, COLOR_BORDER);
            vglib.line(bar_x + 450.0, bar_y, bar_x + 450.0, bar_y + 20.0, COLOR_BORDER);
            vglib.line(bar_x + 450.0, bar_y + 20.0, bar_x, bar_y + 20.0, COLOR_BORDER);
            vglib.line(bar_x, bar_y + 20.0, bar_x, bar_y, COLOR_BORDER);

            down_ratio :: Float64 = vmath.clamp(active_down_timer / 30.0, 0.05, 1.0);
            vglib.rect(bar_x, bar_y, 450.0 * down_ratio, 20, COLOR_BLOOD);

            foot_str :: String = ">>> BGP ROUTER RE-ELECTING PRIMARY MESH GATEWAY NODE...";
            foot_sz  :: Array  = vglib.measure_text(vcr_font, foot_str, 10.0);
            vglib.text_ex(vcr_font, foot_str, center_x - (float64(foot_sz[0]) / 2.0), 415.0 + jitter_y, 10, COLOR_GHOST);
        } else if (current_url == "hellroom.vnet") {
            vglib.text_ex(vcr_font, "HELLROOM.VNET // DEMONIC P2P UNENCRYPTED CHATROOM", 40 + jitter_x, 95 + jitter_y, 14, COLOR_BLOOD);
            vglib.line(40, 115, 890, 115, COLOR_BORDER);

            vglib.rect(35 + jitter_x, 125 + jitter_y, 855, 52, COLOR_CLI_BG);
            vglib.line(35, 125, 890, 125, COLOR_BORDER);
            vglib.line(35, 177, 890, 177, COLOR_BORDER);

            vglib.rect(45 + jitter_x, 135 + jitter_y, 180, 32, COLOR_BLACK);
            vglib.line(45, 135, 225, 135, handle_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
            vglib.line(225, 135, 225, 167, handle_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
            vglib.line(225, 167, 45, 167, handle_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
            vglib.line(45, 167, 45, 135, handle_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
            vglib.text_ex(vcr_font, "NICK: " + player_handle, 52 + jitter_x, 145 + jitter_y, 11, handle_focused == 1 ? COLOR_TOXIC : COLOR_AMBER);

            vglib.rect(235 + jitter_x, 135 + jitter_y, 520, 32, COLOR_BLACK);
            vglib.line(235, 135, 755, 135, chat_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
            vglib.line(755, 135, 755, 167, chat_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
            vglib.line(755, 167, 235, 167, chat_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
            vglib.line(235, 167, 235, 135, chat_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
            display_chat_in :: String = "MSG> " + truncate_str(chat_input_buffer, 55);
            vglib.text_ex(vcr_font, display_chat_in, 242 + jitter_x, 145 + jitter_y, 11, chat_focused == 1 ? COLOR_TOXIC : COLOR_CYAN);

            btn_hover :: Int64 = (mx >= 765.0 && mx <= 880.0 && my >= 135.0 && my <= 167.0) ? 1 : 0;
            vglib.rect(765 + jitter_x, 135 + jitter_y, 115, 32, btn_hover == 1 ? COLOR_BLOOD : COLOR_PANEL);
            vglib.line(765, 135, 880, 135, COLOR_BLOOD);
            vglib.line(880, 135, 880, 167, COLOR_BLOOD);
            vglib.line(880, 167, 765, 167, COLOR_BLOOD);
            vglib.line(765, 167, 765, 135, COLOR_BLOOD);
            vglib.text_ex(vcr_font, "TRANSMIT", 785 + jitter_x, 145 + jitter_y, 11, btn_hover == 1 ? COLOR_BLACK : COLOR_BLOOD);

            vglib.rect(35 + jitter_x, 190 + jitter_y, 855, 545, COLOR_BLACK);
            vglib.line(35, 190, 890, 190, COLOR_BORDER);
            vglib.line(890, 190, 890, 735, COLOR_BORDER);
            vglib.line(890, 735, 35, 735, COLOR_BORDER);
            vglib.line(35, 735, 35, 190, COLOR_BORDER);

            vglib.text_ex(vcr_font, "=== LIVE VNET BROADCAST STREAM & PEER CHAT LOG ===", 50 + jitter_x, 202 + jitter_y, 11, COLOR_AMBER);
            vglib.line(50, 220, 875, 220, COLOR_BORDER);

            chat_feed_cnt = vnet_feed_logs.length();
            chat_start_y :: Float64 = 710.0 - scroll_y;

            through cf_idx :: 0..(chat_feed_cnt - 1) -> loop {
                c_line_y :: Float64 = chat_start_y - (float64(chat_feed_cnt - 1 - cf_idx) * 22.0);
                if (c_line_y >= 230.0 && c_line_y <= 715.0) {
                    cf_txt :: String = string(vnet_feed_logs[cf_idx]);
                    cf_txt_trunc :: String = truncate_str(cf_txt, 90);

                    cf_col = COLOR_GHOST;
                    if (cf_txt.length() > 6 && cf_txt.substr(0, 6) == "[CHAT]") { cf_col = COLOR_TOXIC; }
                    if (cf_txt.substr(0, 12) == "[DOS ATTACK]") { cf_col = COLOR_BLOOD; }
                    if (cf_txt.substr(0, 12) == "[TRACE SPIKE]") { cf_col = COLOR_AMBER; }
                    if (cf_txt.substr(0, 12) == "[BGP HIJACK]") { cf_col = COLOR_TOXIC; }

                    vglib.text_ex(vcr_font, cf_txt_trunc, 50 + jitter_x, c_line_y, 10, cf_col);
                }
            };
        } else {
            line_idx :: Int64 = 0;
            through line_item :: page_body -> loop {
                line_str :: String = string(line_item);
                y_pos :: Float64 = 105.0 + (float64(line_idx) * 28.0) - scroll_y;
                line_idx = line_idx + 1;

                if (y_pos >= 85.0 && y_pos <= 730.0) {
                    if (line_str.length() > 7 && line_str.substr(0, 7) == "[TITLE]") {
                        vglib.text_ex(vcr_font, line_str.substr(8, line_str.length() - 8), 40 + jitter_x, y_pos, 15, COLOR_BLOOD);
                    }
                    else if (line_str.length() > 9 && line_str.substr(0, 9) == "[COMMENT]") {
                        # skip rendering comments
                    }
                    else if (line_str.length() > 10 && line_str.substr(0, 10) == "[SUBTITLE]") {
                        vglib.text_ex(vcr_font, line_str.substr(11, line_str.length() - 11), 40 + jitter_x, y_pos, 12, COLOR_AMBER);
                    }
                    else if (line_str.length() > 6 && line_str.substr(0, 6) == "[WARN]") {
                        vglib.text_ex(vcr_font, line_str.substr(7, line_str.length() - 7), 40 + jitter_x, y_pos, 11, COLOR_BLOOD);
                    }
                    else if (line_str.length() > 6 && line_str.substr(0, 6) == "[TEXT]") {
                        vglib.text_ex(vcr_font, line_str.substr(7, line_str.length() - 7), 40 + jitter_x, y_pos, 11, COLOR_GHOST);
                    }
                    else if (line_str.length() > 6 && line_str.substr(0, 6) == "[CODE]") {
                        vglib.text_ex(vcr_font, line_str.substr(7, line_str.length() - 7), 40 + jitter_x, y_pos, 11, COLOR_TOXIC);
                    }
                    else if (line_str.length() > 5 && line_str.substr(0, 5) == "[BOX]") {
                        vglib.text_ex(vcr_font, line_str.substr(6, line_str.length() - 6), 40 + jitter_x, y_pos, 11, COLOR_CYAN);
                    }
                    else if (line_str.length() > 7 && line_str.substr(0, 7) == "[BLOOD]") {
                        vglib.text_ex(vcr_font, line_str.substr(8, line_str.length() - 8), 40 + jitter_x, y_pos, 11, COLOR_BLOOD);
                    }
                    else if (line_str.length() > 7 && line_str.substr(0, 7) == "[PULSE]") {
                        pulse_col = (pulse_val > 0.5) ? COLOR_AMBER : COLOR_BLOOD;
                        vglib.text_ex(vcr_font, line_str.substr(8, line_str.length() - 8), 40 + jitter_x, y_pos, 11, pulse_col);
                    }
                    else if (line_str.length() > 9 && line_str.substr(0, 9) == "[GLITCH]") {
                        glitch_off :: Float64 = vmath.sin(run_time * 30.0 + float64(line_idx)) * 6.0;
                        glitch_col = (pulse_val > 0.5) ? COLOR_BLOOD : COLOR_TOXIC;
                        vglib.text_ex(vcr_font, line_str.substr(10, line_str.length() - 10), 40.0 + glitch_off + jitter_x, y_pos, 11, glitch_col);
                    }
                    else if (line_str == "[HR]") {
                        vglib.line(40, y_pos + 12.0, 890, y_pos + 12.0, COLOR_BORDER);
                    }
                    # ================================================
                    # GAUGE / PROGRESS BAR TAG -> [GAUGE:pct:label]
                    # ================================================
                    else if (line_str.length() > 7 && line_str.substr(0, 7) == "[GAUGE:") {
                        close_b :: Int64 = -1;
                        through bi :: 7..(line_str.length() - 1) -> loop {
                            if (line_str[bi] == "]") { close_b = bi; break; }
                        };

                        if (close_b > 7) {
                            raw_param :: String = line_str.substr(7, close_b - 7);
                            g_parts = vnet.parse_package(raw_param, ":");
                            
                            pct_val :: Float64 = float64(int64(g_parts[0]));
                            g_label :: String = (string(g_parts[1]) != "") ? string(g_parts[1]) : "GAUGE";

                            gx :: Float64 = 40.0 + jitter_x;
                            gy :: Float64 = y_pos;
                            gw :: Float64 = 340.0;
                            gh :: Float64 = 18.0;

                            # Gauge Background
                            vglib.rect(gx, gy, gw, gh, COLOR_BLACK);

                            # Filled Progress Track
                            fill_w :: Float64 = vmath.clamp((pct_val / 100.0) * gw, 2.0, gw);
                            bar_col = (pct_val > 70.0) ? COLOR_BLOOD : COLOR_TOXIC;
                            vglib.rect(gx, gy, fill_w, gh, bar_col);

                            # Outline Border Box
                            vglib.line(gx, gy, gx + gw, gy, COLOR_BORDER);
                            vglib.line(gx + gw, gy, gx + gw, gy + gh, COLOR_BORDER);
                            vglib.line(gx + gw, gy + gh, gx, gy + gh, COLOR_BORDER);
                            vglib.line(gx, gy + gh, gx, gy, COLOR_BORDER);

                            # Gauge Text Label & Percentage Readout
                            vglib.text_ex(vcr_font, g_label + " [" + string(int64(pct_val)) + "%]", gx + gw + 15.0, gy + 3.0, 10, COLOR_AMBER);
                        }
                    }

                    # ================================================
                    # BADGE TAG -> [BADGE:TEXT:COLOR]
                    # ================================================
                    else if (line_str.length() > 7 && line_str.substr(0, 7) == "[BADGE:") {
                        close_b :: Int64 = -1;
                        through bi :: 7..(line_str.length() - 1) -> loop {
                            if (line_str[bi] == "]") { close_b = bi; break; }
                        };

                        if (close_b > 7) {
                            raw_badge :: String = line_str.substr(7, close_b - 7);
                            b_parts = vnet.parse_package(raw_badge, ":");
                            badge_txt :: String = string(b_parts[0]);
                            badge_col_str :: String = (string(b_parts[1]) != "") ? string(b_parts[1]) : "AMBER";
                            badge_bg = COLOR_AMBER;
                            if (badge_col_str == "BLOOD") { badge_bg = COLOR_BLOOD; }
                            if (badge_col_str == "TOXIC") { badge_bg = COLOR_TOXIC; }

                            bdg_sz :: Array   = vglib.measure_text(vcr_font, badge_txt, 10.0);
                            bdg_w  :: Float64 = float64(bdg_sz[0]) + 16.0;

                            vglib.rect(40.0 + jitter_x, y_pos, bdg_w, 20.0, badge_bg);
                            vglib.text_ex(vcr_font, badge_txt, 48.0 + jitter_x, y_pos + 4.0, 10, COLOR_BLACK);
                        }
                    }

                    # ================================================
                    # IN-PAGE INPUT BOX -> [INPUT:id:placeholder]
                    # ================================================
                    else if (line_str.length() > 7 && line_str.substr(0, 7) == "[INPUT:") {
                        close_b :: Int64 = -1;
                        through bi :: 7..(line_str.length() - 1) -> loop {
                            if (line_str[bi] == "]") { close_b = bi; break; }
                        };

                        if (close_b > 7) {
                            raw_inp :: String = line_str.substr(7, close_b - 7);
                            i_parts = vnet.parse_package(raw_inp, ":");
                            inp_ph :: String = (string(i_parts[1]) != "") ? string(i_parts[1]) : "ENTER VALUE";
                            ix :: Float64 = 40.0 + jitter_x;
                            iy :: Float64 = y_pos;
                            iw :: Float64 = 375.0;
                            ih :: Float64 = 28.0;

                            vglib.rect(ix, iy, iw, ih, COLOR_BLACK);
                            vglib.line(ix, iy, ix + iw, iy, COLOR_BORDER);
                            vglib.line(ix + iw, iy, ix + iw, iy + ih, COLOR_BORDER);
                            vglib.line(ix + iw, iy + ih, ix, iy + ih, COLOR_BORDER);
                            vglib.line(ix, iy + ih, ix, iy, COLOR_BORDER);

                            vglib.text_ex(vcr_font, "> " + inp_ph, ix + 10.0, iy + 7.0, 10, COLOR_GHOST);
                        }
                    }

                    # ================================================
                    # IN-PAGE BUTTON -> [BTN:action_id:label]
                    # ================================================
                    else if (line_str.length() > 5 && line_str.substr(0, 5) == "[BTN:") {
                        close_b :: Int64 = -1;
                        through bi :: 5..(line_str.length() - 1) -> loop {
                            if (line_str[bi] == "]") { close_b = bi; break; }
                        };

                        if (close_b > 5) {
                            raw_btn :: String = line_str.substr(5, close_b - 5);
                            btn_parts = vnet.parse_package(raw_btn, ":");
                            act_id :: String = string(btn_parts[0]);
                            btn_lbl:: String = (string(btn_parts[1]) != "") ? string(btn_parts[1]) : "SUBMIT";
                            bx :: Float64 = 40.0 + jitter_x;
                            by :: Float64 = y_pos;

                            lbl_sz :: Array   = vglib.measure_text(vcr_font, btn_lbl, 11.0);
                            bw     :: Float64 = float64(lbl_sz[0]) + 30.0;
                            bh     :: Float64 = 26.0;

                            is_hover :: Int64 = (cli_overlay_open == 0 && dos_timer <= 0.0 && mx >= bx && mx <= (bx + bw) && my >= by && my <= (by + bh)) ? 1 : 0;

                            vglib.rect(bx, by, bw, bh, is_hover == 1 ? COLOR_BLOOD : COLOR_PANEL);
                            vglib.line(bx, by, bx + bw, by, COLOR_BLOOD);
                            vglib.line(bx + bw, by, bx + bw, by + bh, COLOR_BLOOD);
                            vglib.line(bx + bw, by + bh, bx, by + bh, COLOR_BLOOD);
                            vglib.line(bx, by + bh, bx, by, COLOR_BLOOD);

                            vglib.text_ex(vcr_font, btn_lbl, bx + 15.0, by + 6.0, 11, is_hover == 1 ? COLOR_BLACK : COLOR_TOXIC);

                            if (is_hover == 1 && m_click == 1) {
                                cli_logs.push("[ACTION]: TRIGGERED IN-PAGE EVENT '" + act_id + "'");
                                glitch_trigger = 0.3;
                            }
                        }
                    }
                    else if (line_str.length() > 6 && line_str.substr(0, 6) == "[LINK:") {
                        link_info = extract_link_info(line_str);
                        target_url :: String = string(link_info[0]);
                        label_txt  :: String = string(link_info[1]);

                        label_size :: Array = vglib.measure_text(vcr_font, label_txt, 12.0);
                        lbl_w :: Float64 = float64(label_size[0]);

                        is_hover :: Int64 = (cli_overlay_open == 0 && dos_timer <= 0.0 && game_over_winner == 0 && is_connecting == 0 && mx >= 40.0 && mx <= (40.0 + lbl_w + 20.0) && my >= y_pos && my <= (y_pos + 22.0)) ? 1 : 0;
                        link_col = (is_hover == 1) ? COLOR_TOXIC : COLOR_CYAN;

                        vglib.text_ex(vcr_font, label_txt, 40 + jitter_x, y_pos, 12, link_col);

                        if (is_hover == 1 && m_click == 1) {
                            if (target_url == "buy_ice") {
                                purchase_ice_firewall();
                                page_body = load_page(current_url);
                            } else {
                                trigger_route_navigation(target_url);
                            }
                        }
                    } 
                    else if (line_str.length() > 5 && line_str.substr(0, 5) == "[IMG:") {
                        img_key :: String = line_str.substr(5, line_str.length() - 6);

                        img_x :: Float64 = 40.0 + jitter_x;
                        img_y :: Float64 = y_pos;
                        img_w :: Float64 = 420.0;
                        img_h :: Float64 = 220.0;

                        if (img_y >= 70.0 && img_y <= 710.0) {
                            if (img_key == "redroom") {
                                vglib.draw_texture(img_redroom, img_x, img_y, img_w, img_h);
                                
                                vglib.line(img_x, img_y, img_x + img_w, img_y, COLOR_BLOOD);
                                vglib.line(img_x, img_y + img_h, img_x + img_w, img_y + img_h, COLOR_BLOOD);
                            }

                            if (img_key == "void") {
                                vglib.draw_texture(img_void, img_x, img_y, img_w, img_h);
                                
                                vglib.line(img_x, img_y, img_x + img_w, img_y, COLOR_BLOOD);
                                vglib.line(img_x, img_y + img_h, img_x + img_w, img_y + img_h, COLOR_BLOOD);
                            }
                        }

                        line_idx = line_idx + 7;
                    }
                    # ================================================
                    # ASCII ART TAG RENDERING -> [ART:key]
                    # ================================================
                    else if (line_str.length() > 5 && line_str.substr(0, 5) == "[ART:") {
                        art_key :: String = line_str.substr(5, line_str.length() - 6);

                        art_x :: Float64 = 40.0 + jitter_x;
                        art_y :: Float64 = y_pos;

                        if (art_key == "skull") {
                            jaw_offset_y :: Float64 = vmath.abs(vmath.sin(run_time * 16.0)) * 12.0;

                            skull_top :: Array = [
                                "         .------------------------.         ",
                                "        /    .----------------.    \\        ",
                                "       |    /   (X)      (X)   \\    |       ",
                                "       |   |      .------.      |   |       ",
                                "       |   |     /  /||\\  \\     |   |       "
                            ];

                            skull_jaw :: Array = [
                                "       |    \\   |  | || |  |   /    |       ",
                                "        \\    '--'--'--'--'--'  /        ",
                                "         '------------------------'         "
                            ];

                            skull_col = (pulse_val > 0.5) ? COLOR_BLOOD : COLOR_AMBER;

                            through st_i :: 0..(skull_top.length() - 1) -> loop {
                                st_line_y :: Float64 = art_y + (float64(st_i) * 16.0);
                                if (st_line_y >= 85.0 && st_line_y <= 730.0) {
                                    vglib.text_ex(vcr_font, string(skull_top[st_i]), art_x, st_line_y, 11, skull_col);
                                }
                            };

                            through sj_i :: 0..(skull_jaw.length() - 1) -> loop {
                                sj_line_y :: Float64 = art_y + 80.0 + jaw_offset_y + (float64(sj_i) * 16.0);
                                if (sj_line_y >= 85.0 && sj_line_y <= 730.0) {
                                    vglib.text_ex(vcr_font, string(skull_jaw[sj_i]), art_x, sj_line_y, 11, skull_col);
                                }
                            };

                            line_idx = line_idx + 4;
                        }
                        else if (art_key == "netman") {
                            netman_art :: Array = [
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@%%%@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@=-.-..:.-=@@@@@@@@@@@@@",
                                "@@@@@@@@@@@+: . : .... :=@@@@@@@@@@@",
                                "@@@@@@@@@@+::.:.-.:::: -.+@@@@@@@@@@",
                                "@@@@@@@@@@:-=:-:=:----:=--@@@@@@@@@@",
                                "@@@@@@@@@@::-.::-::-:-.-::@@@@@@@@@@",
                                "@@@@@@@@@@..#@%=: .:=%@%..@@@@@@@@@@",
                                "@@@@@@@@@@+.: . : .:.. :.+@@@@@@@@@@",
                                "@@@@@@@@@@*.:.:.--=::: -.+@@@@@@@@@@",
                                "@@@@@@@@@@%--.:.----:: -:%@@@@@@@@@@",
                                "@@@@@@@@@@@%*--.-::-:=-*%@@@@@@@@@@@",
                                "@@@@@@@@@@@@=:+=@++@=*.=%@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@#=-.::=*@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
                                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                            ];

                            netman_col = (pulse_val > 0.5) ? COLOR_BLOOD : COLOR_AMBER;

                            through nm_i :: 0..(netman_art.length() - 1) -> loop {
                                nm_line_y :: Float64 = art_y + (float64(nm_i) * 16.0);
                                if (nm_line_y >= 85.0 && nm_line_y <= 730.0) {
                                    vglib.text_ex(vcr_font, string(netman_art[nm_i]), art_x, nm_line_y, 11, netman_col);
                                }
                            };

                            line_idx = line_idx + 14;
                        }
                        else if (art_key == "vnet") {
                            vnet_art :: Array = [
                                " __   _____  _ _____ _____ ",
                                " \\ \\ / /  _ \\| | ____|_   _|",
                                "  \\ V /| |_) | |  _|   | |  ",
                                "   | | |  _ <| | |___  | |  ",
                                "   |_| |_| \\_\\_|_____| |_|  ",
                                " [=== VIRTUAL NETWORK MATRIX ===]"
                            ];
                            vnet_col = (pulse_val > 0.5) ? COLOR_CYAN : COLOR_TOXIC;
                            through vn_i :: 0..(vnet_art.length() - 1) -> loop {
                                vn_line_y :: Float64 = art_y + (float64(vn_i) * 16.0);
                                if (vn_line_y >= 85.0 && vn_line_y <= 730.0) {
                                    vglib.text_ex(vcr_font, string(vnet_art[vn_i]), art_x, vn_line_y, 11, vnet_col);
                                }
                            };
                            line_idx = line_idx + 3;
                        }
                        else if (art_key == "vmarket") {
                            vmarket_art :: Array = [
                                "  _  _  _  _   _   ___  _  _____ _____ ",
                                " | || || || | / \\ |  _\\| |/ ____|_   _|",
                                " | || || || |/ _ \\| |_ | ' /|  _| | |  ",
                                "  \\__/  \\_//_/ \\_\\_|\\_\\_|_|\\_\\____||_|  ",
                                " [=== RED MARKET BLACKOUT EXCHANGE ===]"
                            ];
                            vmkt_col = (pulse_val > 0.5) ? COLOR_BLOOD : COLOR_AMBER;
                            through vm_i :: 0..(vmarket_art.length() - 1) -> loop {
                                vm_line_y :: Float64 = art_y + (float64(vm_i) * 16.0);
                                if (vm_line_y >= 85.0 && vm_line_y <= 730.0) {
                                    vglib.text_ex(vcr_font, string(vmarket_art[vm_i]), art_x, vm_line_y, 11, vmkt_col);
                                }
                            };
                            line_idx = line_idx + 3;
                        }
                        else if (art_key == "gun") {
                        gun_art :: Array = [
                            "   .                       :     ",
                            "  *#%#################%%%%%%%.   ",
                            "  =%%%%%%%%######%%%%%%%%%%%%:   ",
                            "  .*###%%%%%%@%%%%###%%%##%%+.   ",
                            "            +=. :%%%#%%#*%%:     ",
                            "            #+  *:=%%%%%%%%*     ",
                            "              ...:. =%%%%%%@-    ",
                            "                    .#%%%%%%@-   ",
                            "                     .%%%%%%%@=  ",
                            "                      :%%%%%%%%. ",
                            "                       #%%%%%%%= ",
                            "                       .+****+:  "
                        ];

                        gun_col = COLOR_AMBER;

                        through gn_i :: 0..(gun_art.length() - 1) -> loop {
                            gn_line_y :: Float64 = art_y + (float64(gn_i) * 16.0);
                            if (gn_line_y >= 85.0 && gn_line_y <= 730.0) {
                                vglib.text_ex(vcr_font, string(gun_art[gn_i]), art_x, gn_line_y, 11, gun_col);
                            }
                        };

                        line_idx = line_idx + 7;
                    }
                        else if (art_key == "drug") {
                            drug_art :: Array = [
                                "        .---.           .---.           ",
                                "       /     \\         /   /|           ",
                                "      |  RX   |       /   / |  [ SYNTHETIC ",
                                "       \\     /       |---|  |    OPIOIDS &",
                                "        '---'        |   | /     NEURALS ]",
                                "       .-----.       |___|/             ",
                                "      (  80  )        / \\               ",
                                "       '-----'       '---'              "
                            ];
                            drug_col = COLOR_TOXIC;
                            through dr_i :: 0..(drug_art.length() - 1) -> loop {
                                dr_line_y :: Float64 = art_y + (float64(dr_i) * 16.0);
                                if (dr_line_y >= 85.0 && dr_line_y <= 730.0) {
                                    vglib.text_ex(vcr_font, string(drug_art[dr_i]), art_x, dr_line_y, 11, drug_col);
                                }
                            };
                            line_idx = line_idx + 4;
                        }
                        else if (art_key == "biohazard") {
                            bio_art :: Array = [
                                "                 _  _                   ",
                                "                / \\/ \\                  ",
                                "              / \\  /\\  \\                ",
                                "             |   ()   |                 ",
                                "              \\ /  \\/  /                ",
                                "                \\_/\\_/                  ",
                                "     [ BIO-CONTAMINATION HAZARD ]       "
                            ];
                            bio_col = COLOR_BLOOD;
                            through bio_i :: 0..(bio_art.length() - 1) -> loop {
                                bio_line_y :: Float64 = art_y + (float64(bio_i) * 16.0);
                                if (bio_line_y >= 85.0 && bio_line_y <= 730.0) {
                                    vglib.text_ex(vcr_font, string(bio_art[bio_i]), art_x, bio_line_y, 11, bio_col);
                                }
                            };
                            line_idx = line_idx + 4;
                        }
                        else if (art_key == "not_found") {
                            nf_art :: Array = [
                                "      .------------------------------------.      ",
                                "     /  404 // SIGNAL LOST IN THE MATRIX   \     ",
                                "    |   ================================   |    ",
                                "    |     _  _    ___   _  _               |    ",
                                "    |    | || |  / _ \\ | || |  [ VOID ]   |    ",
                                "    |    | || |_| | | || || |_             |    ",
                                "    |    |__   _| |_| ||__   _|            |    ",
                                "    |       |_|  \\___/    |_|             |    ",
                                "    |                                      |    ",
                                "    |   [!] SOCKET DESYNCHRONIZED          |    ",
                                "    |   [!] MEMORY BLOCK WIPED / SEIZED    |    ",
                                "     \\  --------------------------------  /     ",
                                "      '----------------------------------'      "
                            ];

                            nf_col = (pulse_val > 0.5) ? COLOR_BLOOD : COLOR_AMBER;

                            through nf_i :: 0..(nf_art.length() - 1) -> loop {
                                nf_line_y :: Float64 = art_y + (float64(nf_i) * 16.0);
                                if (nf_line_y >= 85.0 && nf_line_y <= 730.0) {
                                    vglib.text_ex(vcr_font, string(nf_art[nf_i]), art_x, nf_line_y, 11, nf_col);
                                }
                            };

                            line_idx = line_idx + 7;
                        }
                    }
                }
            };
        }

        if (cli_overlay_open == 1) {
            vglib.rect(20 + jitter_x, 80 + jitter_y, 890, 520, COLOR_CLI_BG);
            vglib.line(20, 80, 910, 80, COLOR_BLOOD);
            vglib.line(910, 80, 910, 600, COLOR_BLOOD);
            vglib.line(910, 600, 20, 600, COLOR_BLOOD);
            vglib.line(20, 600, 20, 80, COLOR_BLOOD);

            vglib.text_ex(vcr_font, "SYSTEM TERMINAL OVERLAY | REAL-TIME CLI & CYBERWARFARE HUB", 35 + jitter_x, 95, 12, COLOR_BLOOD);
            vglib.line(35, 115, 895, 115, COLOR_BORDER);

            log_cnt = cli_logs.length();
            cli_start_y :: Float64 = 125.0 - cli_scroll_y;
            through c_idx :: 0..(log_cnt - 1) -> loop {
                line_y :: Float64 = cli_start_y + (c_idx * 22.0);
                if (line_y >= 120.0 && line_y <= 540.0) {
                    txt = string(cli_logs[c_idx]);
                    col = active_theme.text;
                    if (txt.substr(0, 2) == "> ")       { col = COLOR_TOXIC; }
                    if (txt.substr(0, 8) == "[NET_IN]") { col = COLOR_AMBER; }
                    if (txt.substr(0, 8) == "[SNIFF]:") { col = COLOR_CYAN; }
                    if (txt.substr(0, 10) == "[SECURITY]") { col = COLOR_TOXIC; }
                    if (txt.substr(0, 13) == "[ICE_DEFENSE]") { col = COLOR_TOXIC; }
                    if (txt.substr(0, 7) == "[ERROR]" || txt.substr(0, 5) == "[ERR]" || txt.substr(0, 16) == "[CRITICAL_ALERT]") { col = COLOR_BLOOD; }

                    vglib.text_ex(vcr_font, txt, 35 + jitter_x, line_y, 11, col);
                }
            };

            vglib.line(20, 560, 910, 560, COLOR_BORDER);
            prompt_str :: String = "CMD> " + cli_input_buffer;
            vglib.text_ex(vcr_font, prompt_str, 35 + jitter_x, 572, 12, COLOR_TOXIC);

            if (vmath.fmod(cursor_blink, 0.8) > 0.4) {
                prompt_size :: Array = vglib.measure_text(vcr_font, prompt_str, 12.0);
                cur_x :: Float64 = 35.0 + float64(prompt_size[0]) + 2.0;
                vglib.rect(cur_x + jitter_x, 572 + jitter_y, 8, 14, COLOR_TOXIC);
            }
        }

        if (dos_timer > 0.0) {
            vglib.rect(20, 80, 890, 670, vglib.rgba(220, 20, 40, 140));
            vglib.text_ex(vcr_font, "[CRITICAL WARNING: INBOUND DOS EXPLOIT ATTACK]", 140 + jitter_x, 360 + jitter_y, 16, COLOR_BLACK);
            vglib.text_ex(vcr_font, "SYSTEM KERNEL FROZEN | RECOVERING IN " + string(int64(dos_timer) + 1) + "s...", 180 + jitter_x, 400 + jitter_y, 14, COLOR_BLACK);
        }

        # ================================================================
        # COLD-BLOODED LOCKDOWN & GAME WINNING ANIMATED OVERLAY
        # ================================================================
        if (game_over_winner == 1) {
            win_anim_timer = win_anim_timer + 0.016;

            # Automatically close game after 10-second cold animation completes
            if (win_anim_timer >= 10.0) {
                vnet.close(client_sock);
                vglib.close();
                break;
            }

            vglib.begin();
            vglib.clear(COLOR_BLACK);

            glitch_intensity :: Float64 = vmath.sin(win_anim_timer * 25.0) * 10.0;
            through tear_i :: 0..25 -> loop {
                tx :: Float64 = vmath.random(-20.0, 1280.0);
                ty :: Float64 = float64(tear_i * 32);
                tw :: Float64 = vmath.random(100.0, 600.0);
                th :: Float64 = vmath.random(4.0, 18.0);
                
                t_col = (tear_i % 2 == 0) ? vglib.rgba(220, 10, 30, 180) : vglib.rgba(10, 240, 80, 140);
                vglib.rect(tx + glitch_intensity, ty, tw, th, t_col);
            };

            through hex_col :: 0..7 -> loop {
                column_x_left  :: Float64 = float64(hex_col * 22);
                column_x_right :: Float64 = 1100.0 + float64(hex_col * 22);
                
                drop_y :: Float64 = vmath.fmod((win_anim_timer * 400.0) + float64(hex_col * 90), 800.0);
                hex_val_str :: String = "0x" + string(int64(vmath.fmod(win_anim_timer * 100.0 + float64(hex_col * 17), 99.0)));
                
                vglib.text_ex(vcr_font, hex_val_str, column_x_left, drop_y, 10, COLOR_BLOOD);
                vglib.text_ex(vcr_font, hex_val_str, column_x_right, drop_y, 10, COLOR_BLOOD);
            };

            box_x :: Float64 = 180.0 + glitch_intensity;
            box_y :: Float64 = 100.0;
            box_w :: Float64 = 920.0;
            box_h :: Float64 = 600.0;

            vglib.rect(box_x, box_y, box_w, box_h, vglib.rgba(8, 2, 6, 245));
            
            border_col = (vmath.fmod(win_anim_timer * 6.0, 1.0) > 0.5) ? COLOR_BLOOD : COLOR_AMBER;
            vglib.line(box_x, box_y, box_x + box_w, box_y, border_col);
            vglib.line(box_x + box_w, box_y, box_x + box_w, box_y + box_h, border_col);
            vglib.line(box_x + box_w, box_y + box_h, box_x, box_y + box_h, border_col);
            vglib.line(box_x, box_y + box_h, box_x, box_y, border_col);

            vglib.line(box_x + 6.0, box_y + 6.0, box_x + box_w - 6.0, box_y + 6.0, COLOR_BLOOD);
            vglib.line(box_x + box_w - 6.0, box_y + 6.0, box_x + box_w - 6.0, box_y + box_h - 6.0, COLOR_BLOOD);
            vglib.line(box_x + box_w - 6.0, box_y + box_h - 6.0, box_x + 6.0, box_y + box_h - 6.0, COLOR_BLOOD);
            vglib.line(box_x + 6.0, box_y + box_h - 6.0, box_x + 6.0, box_y + 6.0, COLOR_BLOOD);

            # 4. Animated Screaming ASCII Skull Banner
            skull_y :: Float64 = box_y + 25.0;
            jaw_motion :: Float64 = vmath.abs(vmath.sin(win_anim_timer * 14.0)) * 8.0;

            vglib.text_ex(vcr_font, "      .------------------------------------------.      ", box_x + 220.0, skull_y, 11, COLOR_BLOOD);
            vglib.text_ex(vcr_font, "     /   [!] CRITICAL NETWORK ROOT OVERRIDE [!]   \\     ", box_x + 220.0, skull_y + 16.0, 11, COLOR_BLOOD);
            vglib.text_ex(vcr_font, "    |    ======================================    |    ", box_x + 220.0, skull_y + 32.0, 11, COLOR_AMBER);
            vglib.text_ex(vcr_font, "    |      (X)  ALL PEER SOCKETS LOCKED  (X)       |    ", box_x + 220.0, skull_y + 48.0 + jaw_motion, 11, COLOR_TOXIC);
            vglib.text_ex(vcr_font, "     \\  ----------------------------------------  /     ", box_x + 220.0, skull_y + 64.0, 11, COLOR_BLOOD);

            vglib.line(box_x + 30.0, box_y + 115.0, box_x + box_w - 30.0, box_y + 115.0, COLOR_BORDER);

            is_me :: Int64 = (winner_port == string(my_port)) ? 1 : 0;
            
            main_header_str :: String = (is_me == 1) ? ">>> YOU HAVE SEIZED TOTAL CONTROL OF VNET <<<" : ">>> HOSTILE OPERATOR HAS SEIZED VNET <<<";
            header_sz :: Array  = vglib.measure_text(vcr_font, main_header_str, 16.0);
            header_x  :: Float64 = (box_x + (box_w / 2.0)) - (float64(header_sz[0]) / 2.0);
            
            vglib.text_ex(vcr_font, main_header_str, header_x, box_y + 135.0, 16, (is_me == 1) ? COLOR_TOXIC : COLOR_BLOOD);

            # Highlight Card
            card_x :: Float64 = box_x + 80.0;
            card_y :: Float64 = box_y + 175.0;
            card_w :: Float64 = 760.0;
            card_h :: Float64 = 210.0;

            vglib.rect(card_x, card_y, card_w, card_h, COLOR_BLACK);
            vglib.line(card_x, card_y, card_x + card_w, card_y, COLOR_BLOOD);
            vglib.line(card_x + card_w, card_y, card_x + card_w, card_y + card_h, COLOR_BLOOD);
            vglib.line(card_x + card_w, card_y + card_h, card_x, card_y + card_h, COLOR_BLOOD);
            vglib.line(card_x, card_y + card_h, card_x, card_y, COLOR_BLOOD);

            disp_handle :: String = (winner_handle != "") ? winner_handle : "UNKNOWN_GHOST";

            vglib.text_ex(vcr_font, "DOMINANT OPERATOR  : " + disp_handle, card_x + 30.0, card_y + 30.0, 14, COLOR_TOXIC);
            vglib.text_ex(vcr_font, "BOUND SOCKET PORT  : PORT " + winner_port, card_x + 30.0, card_y + 70.0, 14, COLOR_CYAN);

            win_method_desc :: String = "";
            if (win_mode_str == "BLACKOUT") {
                win_method_desc = "GRID BLACKOUT (FRIED ALL 5 MUTUAL CORE NODES)";
            } else if (win_mode_str == "ECONOMIC") {
                win_method_desc = "ECONOMIC TAKEOVER (25.00 VCOIN NETWORK SHARE)";
            } else {
                win_method_desc = "CRYPTOGRAPHIC BREACH (OVERRODE VFS ROOT KEYS)";
            }

            vglib.text_ex(vcr_font, "EXPLOIT METHOD     : " + win_method_desc, card_x + 30.0, card_y + 110.0, 13, COLOR_AMBER);
            
            status_txt :: String = (is_me == 1) ? "STATUS: VICTORIOUS // ALL PEER NODES TERMINATED" : "STATUS: TERMINATED // YOUR SYSTEM MEMORY WIPED";
            vglib.text_ex(vcr_font, status_txt, card_x + 30.0, card_y + 155.0, 13, (is_me == 1) ? COLOR_TOXIC : COLOR_BLOOD);

            # 6. Cold-Blooded Lore Quotes
            vglib.text_ex(vcr_font, "'THE NETWORK DOES NOT FORGIVE. THE WEAK HAVE BEEN PURGED FROM RAM.'", box_x + 110.0, box_y + 415.0, 11, COLOR_GHOST);
            vglib.text_ex(vcr_font, "'ALL UDP CONNECTIONS ARE PERMANENTLY SEVERED BY SERVER FIREWALL.'", box_x + 120.0, box_y + 440.0, 11, COLOR_BLOOD);

            # 7. Animated Process Kill Countdown & Bar
            time_rem :: Float64 = vmath.clamp(10.0 - win_anim_timer, 0.0, 10.0);
            shutdown_str :: String = "SYSTEM TERMINATION IN PROGRESS... CLOSING GAME IN " + string(vmath.round(time_rem * 10.0) / 10.0) + "s";
            s_sz :: Array   = vglib.measure_text(vcr_font, shutdown_str, 12.0);
            s_x  :: Float64 = (box_x + (box_w / 2.0)) - (float64(s_sz[0]) / 2.0);
            
            vglib.text_ex(vcr_font, shutdown_str, s_x, box_y + 490.0, 12, COLOR_AMBER);

            bar_x :: Float64 = box_x + 160.0;
            bar_y :: Float64 = box_y + 520.0;
            bar_w :: Float64 = 600.0;
            bar_h :: Float64 = 20.0;

            vglib.rect(bar_x, bar_y, bar_w, bar_h, COLOR_BLACK);
            
            fill_ratio :: Float64 = win_anim_timer / 10.0;
            fill_w     :: Float64 = vmath.clamp(fill_ratio * bar_w, 4.0, bar_w);
            
            vglib.rect(bar_x, bar_y, fill_w, bar_h, COLOR_BLOOD);

            vglib.line(bar_x, bar_y, bar_x + bar_w, bar_y, COLOR_BORDER);
            vglib.line(bar_x + bar_w, bar_y, bar_x + bar_w, bar_y + bar_h, COLOR_BORDER);
            vglib.line(bar_x + bar_w, bar_y + bar_h, bar_x, bar_y + bar_h, COLOR_BORDER);
            vglib.line(bar_x, bar_y + bar_h, bar_x, bar_y, COLOR_BORDER);

            vglib.draw_scanlines(8.0, vglib.rgba(0, 0, 0, 90));
            vglib.end();

            continue;
        }

        # ================================================================
        # BOT STALKER HIJACK: LAUGHING SKULL & COLOR GLITCH OVERLAY
        # ================================================================
        if (bot_stalk_active == 1) {
            through g_i :: 0..18 -> loop {
                gx :: Float64 = vmath.random(0.0, 1200.0);
                gy :: Float64 = vmath.random(0.0, 750.0);
                gw :: Float64 = vmath.random(40.0, 350.0);
                gh :: Float64 = vmath.random(8.0, 60.0);
                r_c :: Int64 = int64(vmath.random(100, 255));
                g_c :: Int64 = int64(vmath.random(0, 255));
                b_c :: Int64 = int64(vmath.random(0, 150));
                vglib.rect(gx, gy, gw, gh, vglib.rgba(r_c, g_c, b_c, 160));
            };

            warn_col = (vmath.fmod(run_time * 10.0, 1.0) > 0.5) ? COLOR_BLOOD : COLOR_TOXIC;
            vglib.rect(120, 520, 1040, 50, COLOR_BLACK);
            vglib.line(120, 520, 1160, 520, warn_col);
            vglib.line(1160, 520, 1160, 570, warn_col);
            vglib.line(1160, 570, 120, 570, warn_col);
            vglib.line(120, 570, 120, 520, warn_col);

            vglib.text_ex(vcr_font, "[WARNING]: PROMISCUOUS NODE DETECTED ON LOCAL SUBNET", 170, 535, 16, warn_col);

            m_pos_bot = vglib.mouse_pos();
            bmx :: Float64 = float64(m_pos_bot[0]);
            bmy :: Float64 = float64(m_pos_bot[1]);
            bm_down :: Int64 = vglib.mouse_down(vglib.MOUSE_LEFT);

            logout_btn_hover :: Int64 = (bmx >= 510.0 && bmx <= 770.0 && bmy >= 600.0 && bmy <= 640.0) ? 1 : 0;
            
            vglib.rect(510, 600, 260, 40, logout_btn_hover == 1 ? COLOR_BLOOD : COLOR_PANEL);
            vglib.line(510, 600, 770, 600, COLOR_BLOOD);
            vglib.line(770, 600, 770, 640, COLOR_BLOOD);
            vglib.line(770, 640, 510, 640, COLOR_BLOOD);
            vglib.line(510, 640, 510, 600, COLOR_BLOOD);
            vglib.text_ex(vcr_font, "FORCE BGP DISCONNECT", 555, 614, 12, logout_btn_hover == 1 ? COLOR_BLACK : COLOR_TOXIC);

            if (logout_btn_hover == 1 && bm_down == 1) {
                bot_stalk_active = 0;
                is_connecting    = 0;
                current_url      = "vnet.dir";
                input_url        = "vnet.dir";
                page_body        = load_page("vnet.dir");
                scroll_y         = 0.0;
                
                vnet.send_to(client_sock, server_ip, server_port, "GET:vnet.dir");
            }
        }

        # Scanlines main screen
        vglib.draw_scanlines(8.0, vglib.rgba(0, 0, 0, 90));

        vglib.rect(0, 765, 1280, 35, COLOR_PANEL);
        vglib.line(0, 765, 1280, 765, COLOR_BORDER);
        vglib.text_ex(vcr_font, "TOR ROUTER ACTIVE | NODE PORT: " + string(my_port) + " | [TAB] TOGGLE TERMINAL OVERLAY", 20, 776, 10, COLOR_CYAN);

    vglib.end();
}

vnet.close(client_sock);
vglib.close();