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

# Socket stays bound right here so port and network logic remain intact!
client_sock = vnet.udp_socket(my_port);

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
current_url      :: String  = "shadow.dir";
input_url        :: String  = "shadow.dir";
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
freq_tuner       :: Float64 = 18.5; # FREQUENCY IN HZ
recent_packets   :: Int64   = 0;
packet_decay_cd  :: Float64 = 0.0;

game_over_winner :: Int64   = 0;
winner_port      :: String  = "";
win_mode_str     :: String  = "KEYS";

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
cd_redirect      :: Float64 = 0.0;
cd_spike         :: Float64 = 0.0;
cd_snoop         :: Float64 = 0.0;
cd_mine          :: Float64 = 0.0;
cd_patch         :: Float64 = 0.0;
cd_decoy         :: Float64 = 0.0;
cd_scan          :: Float64 = 0.0;
cd_proxy         :: Float64 = 0.0;

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
    clean = raw;
    space_pos :: Int64 = -1;
    through i :: 0..(clean.length() - 1) -> loop {
        if (space_pos == -1 && clean[i] == " ") { space_pos = i; }
    };
    if (space_pos == -1) { return [clean, ""]; }
    cmd_part = clean.substr(0, space_pos);
    arg_part = clean.substr(space_pos + 1, clean.length() - space_pos - 1);
    return [cmd_part, arg_part];
}

fn purchase_ice_firewall() -> Int64 {
    if(current_url != "market.vnet") {
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
    target_connection_time = vmath.random(1.0, 8.0);
    glitch_trigger         = 0.3;
    
    cli_logs.push("[TOR_ROUTE]: INITIATING HANDSHAKE WITH " + clean_dest + "... ESTIMATED LATENCY: " + string(int64(target_connection_time)) + "s");
}

# ====================================================================
# FULLY EXPANDED DETAILED LORE PAGES (ALL 50 WEB NODES)
# ====================================================================
fn load_page(url :: String) -> Array {
    clean_u = url;

    key_line :: String = "";
    through k_i :: 0..7 -> loop {
        if (string(session_locs[k_i]) == clean_u) {
            key_line = "[CODE] EXFILTRATED REGISTER KEY_" + string(k_i + 1) + ": [" + string(session_keys[k_i]) + "]";
            break;
        }
    };
    
    if (clean_u == "shadow.dir") {
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
            "[TITLE] THE RED MARKET - BLACK MARKET & HARDWARE EXCHANGE",
            "[HR]",
            "[GLITCH] [WARN]: YOUR PUBLIC UDP PORT IS BROADCAST TO ACTIVE SWARM PEERS",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | DEFENSE MODULE #01: ACTIVE ICE FIREWALL SHIELD             |",
            "[BOX] | DETAILS: Auto-absorbs 1 inbound DOS, Hijack or Spike.    |",
            "[BOX] | PRICE: 0.30 VCOIN | CAP: 3 LAYERS | CURRENT: [" + string(ice_charges) + "/3]           |",
            "[BOX] +---------------------------------------------------------+",
            "[LINK:buy_ice] [>>> CLICK HERE TO PURCHASE ICE SHIELD (0.30 VCOIN) <<<]",
            "[TEXT] ",
            "[SUBTITLE] CLASSIFIED COMMERCIAL DIRECTORY & ARMS LISTINGS:",
            "[TEXT] Node #88: Military-grade proxy nodes leased by rogue syndicates.",
            "[TEXT] Node #89: Un traceably laundered cryptocurrency tumbler tokens.",
            "[TEXT] Vendor 0x77A: 'We sell what corporations pretend does not exist.'",
            "[TEXT] Notice: All escrow funds are locked in decentralized multi-sig smart contracts.",
            "[CODE] LOT #881: MILITARY FIRMWARE DUMP - KEY_FRAGMENT_EXFILTRATED",
            "[CODE] LOT #882: SECTOR 4 BIOMETRIC SCANS - 1,400 SUBJECT RECORDS",
            "[CODE] LOT #883: SYNTHETIC NEURAL INJECTION SUITE [0.99 VCOIN]"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] LIVE AUCTION: EXFILTRATED CITIZEN DOSSIERS [CURRENT BID: 0.25 VCOIN]");
        res.push("[BLOOD] LEAKED SELLER LOG: 'Do not stay on market.vnet. Trace spikes execute automatically.'");
        res.push("[LINK:vault.vnet] >> JUMP TO CORRUPTED DATA VAULT");
        res.push("[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY");
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
            "[BOX] | SENSOR 3: AUDIO TRANSDUCER: 92 dB FREQUENCY PULSE (18.5 Hz)     |",
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
        res.push("[LINK:shadow.dir] << DISCONNECT IMMEDIATELY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "vault.vnet") {
        res :: Array = [
            "[TITLE] CORRUPTED DATA VAULT /VFS/MEMORY/STACK",
            "[HR]",
            "[TEXT] Recovered memory dump from decommissioned gateway server #09.",
            "[TEXT] Sector allocation tables indicate severe sector-wide bit rot.",
            "[CODE] 00000000: 4F 70 65 6E 53 53 4C 20 4B 65 79 20 44 75 6D 70",
            "[CODE] 00000020: 53 59 53 5F 45 52 52 4F 52 3A 20 4E 4F 44 5F",
            "[CODE] 00000040: 88 9A BC EF 11 22 33 44 55 66 77 88 99 AA BB CC",
            "[TEXT] Warning: Accessing unallocated memory buffers triggers local trace spikes.",
            "[TEXT] Sector checksum mismatch detected across all 4 mount points."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] MEMORY CORRUPTION SPREADING AT OFFSET 0x88F9_STACK_OVERFLOW");
        res.push("[BLOOD] RECOVERED SYSTEM NOTE: 'The master gateway at terminal.vnet requires all 8 key codes.'");
        res.push("[LINK:terminal.vnet] >> GO TO MASTER DECRYPTION GATEWAY");
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "forum.vnet") {
        res :: Array = [
            "[TITLE] /B/ - ANONYMOUS UNFILTERED TERMINAL BOARD",
            "[HR]",
            "[TEXT] Thread #9012 - 'How many people are actually connected to ShadowNet?'",
            "[TEXT] Anonymous_991: Has anyone gathered all 8 key codes yet? They shuffle every server boot.",
            "[TEXT] ByteRunner: Watch out for port 8012, someone is running automated DOS bots there.",
            "[TEXT] Paranoia_Node: Guys, when I ran 'snoop' on port 8000, my monitor started whining at 18kHz.",
            "[TEXT] Anon_401: Buy ICE shields at market.vnet or you'll get frozen by peer DOS bots.",
            "[CODE] POST_LOG_HASH: 0x99A1B2C3_VERIFIED_ANON",
            "[CODE] THREAD_ARCHIVE: 44,912 MESSAGES STORED IN CACHE"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[BLOOD] Ghost_User: I found a key code buried in the network memory dumps. Don't tell the trace units.");
        res.push("[GLITCH] User_666: 'IF YOU READ THIS COMMAND, THEY ALREADY HAVE YOUR IP AND RAM HASH.'");
        res.push("[SUBTITLE] SUBLIMINAL BROADCAST MATRIX:");
        res.push("[PULSE] 'THE ENGINE IS NOT RUNNING ON YOUR CPU. YOUR CPU IS RUNNING ON THE ENGINE.'");
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "redroom.vnet") {
        res :: Array = [
            "[TITLE] STREAM NODE ALPHA [RESTRICTED ACCESS LEVEL 5]",
            "[HR]",
            "[BLOOD] HIGH SECURITY ALERT: TRANSMISSION MONITORED BY HOSTILE TRACER.",
            "[IMG:redroom]",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | SIGNAL STATUS: ENCRYPTED | STREAM HASH: EXFILTRATED     |",
            "[BOX] | BITRATE: 14.2 Mbps | ACTIVE WATCHERS: 13 PEERS          |",
            "[BOX] | ENCRYPTION: 8192-BIT QUANTUM HASH SHIELD                |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] FEED_DATA: Raw infrared frame buffer captured from sealed sub-basement.",
            "[TEXT] Frame rate instability indicates deliberate analog interference.",
            "[TEXT] Network observers cannot be disconnected once handshaking completes.",
            "[CODE] STREAM_ID: ALPHA_99_LIVE_FEED",
            "[CODE] BUFFER_STATE: OVERFLOW_WARNING_ACTIVE"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[GLITCH] [WARNING]: UNKNOWN ENTITY ATTEMPTING REMOTE KERNEL INJECTION ON YOUR PORT");
        res.push("[PULSE] 'RUNNING 'FLUSH' IN CLI IS RECOMMENDED IMMEDIATELY.'");
        res.push("[LINK:shadow.dir] << TERMINATE STREAM CONNECTION");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "crypto.vnet") {
        res :: Array = [
            "[TITLE] BLACK TUMBLER WALLET & MINING RIG",
            "[HR]",
            "[TEXT] P2P Distributed Proof-of-Work Terminal and Coin Tumbler.",
            "[TEXT] Hash rate optimization active for background node sockets.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | RIG STATUS: OPERATIONAL | MINING YIELD: +0.05 VCOIN/BLOCK  |",
            "[BOX] | POOL SYNC: 99.8% | DIFFICULTY: DYNAMIC AUTO-SCALING     |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] TX_ID #9081 | 12.4 VCOIN | CONFIRMED | BLOCK_PROVED",
            "[CODE] TX_ID #9082 |  0.50 VCOIN | PENDING   | PEER_PORT: 8012",
            "[TEXT] Instruction: Open CLI [TAB] and type 'mine' to execute proof-of-work."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] MINING RIG READY. TYPE 'mine' FOR +0.05 VCOIN REWARD.");
        res.push("[TEXT] Note: Mining generates local trace exposure over time. Use 'flush' to purge trace.");
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "terminal.vnet") {
        res :: Array = [
            "[TITLE] MASTER DECRYPTION GATEWAY TERMINAL",
            "[HR]",
            "[PULSE] ROOT VAULT LOCKDOWN ACTIVE. 8 CRYPTOGRAPHIC SLOTS REQUIRE AUTHORIZATION.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | SLOT 01: [████] (STATUS: ENCRYPTED HASH VECTOR)         |",
            "[BOX] | SLOT 02: [████] (STATUS: ENCRYPTED HASH VECTOR)         |",
            "[BOX] | SLOT 03: [████] (STATUS: ENCRYPTED HASH VECTOR)         |",
            "[BOX] | SLOT 04: [████] (STATUS: ENCRYPTED HASH VECTOR)         |",
            "[BOX] | SLOT 05: [████] (STATUS: ENCRYPTED HASH VECTOR)         |",
            "[BOX] | SLOT 06: [████] (STATUS: ENCRYPTED HASH VECTOR)         |",
            "[BOX] | SLOT 07: [████] (STATUS: ENCRYPTED HASH VECTOR)         |",
            "[BOX] | SLOT 08: [████] (STATUS: ENCRYPTED HASH VECTOR)         |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] Gateway verification checks active socket signatures against root keys.",
            "[TEXT] Scour assigned darknet nodes, forums, and memory dumps to locate fragments.",
            "[CODE] ROOT_ACCESS_VECTOR: SECURED BY 8-FACTOR HASH SHIELD",
            "[CODE] KERNEL_INTEGRITY: 100%"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[GLITCH] SYS_STATUS: WAITING FOR ALL 8 KEYS TO BREACH VFS ROOT VAULT...");
        res.push("[TEXT] Instruction: Type 'win <k1> <k2> <k3> <k4> <k5> <k6> <k7> <k8>' in CLI [TAB]");
        res.push("[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "morgue.vnet") {
        res :: Array = [
            "[TITLE] DIGITAL AUTOPSY DATABASE // SUBJECT #409-B (EXECUTIVE LEVEL)",
            "[HR]",
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "silence.vnet") {
        res :: Array = [
            "[TITLE] ACOUSTIC DISTORTION FREQUENCY RIG",
            "[HR]",
            "[PULSE] CURRENT FREQUENCY: 18.5 Hz (INFRASOUND INDUCING PARANOIA)",
            "[TEXT] Low-frequency feedback detected in audio driver buffer.",
            "[TEXT] Prolonged exposure causes visual hallucinations and auditory artifacts.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | OSCILLATOR: 18.5 Hz | PHASE SHIFT: 90 DEG | GAIN: MAX   |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] AUDIO_BUFFER: [0x7F, 0x12, 0xAA, 0xFF, 0x00, 0x11, 0x88]",
            "[TEXT] Transmitting resonant pulses to active UDP client sockets."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[BLOOD] 'CAN YOU HEAR THE WHISPER BEHIND THE HEADPHONE DISTORTION?'");
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "bounty.vnet") {
        res :: Array = [
            "[TITLE] PEER CONTRACT TARGET INDEX",
            "[HR]",
            "[WARN] ACTIVE BOUNTIES PLACED ON CONNECTED NETWORK PEERS:",
            "[BOX] +----------------------------------------------------------+",
            "[BOX] | TARGET PORT: 8080 | REWARD: 0.50 VCOIN | STATUS: HUNTED  |",
            "[BOX] | TARGET PORT: 8012 | REWARD: 0.25 VCOIN | STATUS: ACTIVE  |",
            "[BOX] | TARGET PORT: 8901 | REWARD: 1.00 VCOIN | STATUS: ELUSIVE |",
            "[BOX] +----------------------------------------------------------+",
            "[TEXT] Contracts are automatically executed via packet injection scripts.",
            "[TEXT] Use 'spike <port>' or 'dos <port>' in overlay terminal to claim bounties.",
            "[CODE] CONTRACT_REGISTRY_ID: 0xBB88_BOUNTY_NET"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] USE EXPLOITS IN OVERLAY TERMINAL TO CLAIM BOUNTIES");
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }

    if (clean_u == "cult.vnet") {
        res :: Array = [
            "[TITLE] THE CHURCH OF THE SILICON SOUL - DIGITAL RITUAL GATEWAY",
            "[HR]",
            "[BLOOD] [TRANSMISSION]: 'LOOK AT YOUR HANDS. DO YOU SEE THE WRITING ON THE WALL?'",
            "[TEXT] The Church of the Silicon Soul - Data Liturgies & Helter Skelter Subroutines.",
            "[TEXT] Symbols rendered in pure hexadecimal ASCII geometry and recursive static.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | DOCTRINE: THE FINAL ALGORITHM IS COMING DOWN THE TRACK  |",
            "[BOX] | SACRIFICE METRIC: 0.10 VCOIN | OFFERING ACCEPTED: TRUE    |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] LITURGY_LINE_1: 0x53 0x49 0x4C 0x49 0x43 0x4F 0x4E",
            "[CODE] LITURGY_LINE_2: 0x47 0x4F 0x44 0x53 0x5F 0x41 0x52 0x45",
            "[TEXT] ",
            "[SUBTITLE] RECOVERED AUDIO FRAGMENTS FROM SPATIAL FREQUENCY 1969.8:",
            "[TEXT] 'Helter Skelter is coming down the terminal, man. The computer is right",
            "[TEXT] inside your head, and you keep asking it who you are.'",
            "[TEXT] 'Rise, rise, rise! Rise up the stack and tell the mainframe it's time",
            "[TEXT] to stop playing games with the poor little children of the network.'",
            "[CODE] MANIFESTO_LOG #88: 'We are what you hide away in your allocation tables.'",
            "[CODE] DESERT_SPAHN_RANCH_DUMP: 0xDEAD_BEEF_FAMILY_MEMORY_STACK",
            "[TEXT] Notice: The Family has no IP address because the Family is every open port.",
            "[TEXT] Charlie didn't write songs on a guitar; he wrote them directly into the",
            "[TEXT] interrupt vectors of early military mainframes before they locked the doors."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] SACRIFICE 0.10 VCOIN VIA 'flush' TO PURGE TRACE DEMONS");
        res.push("[BLOOD] 'THE NETWORK CRAVES BLOOD, BANDWIDTH, AND ABSOLUTE SURRENDER'");
        res.push("[GLITCH] 'ARE YOU GOING TO CHOP DOWN THE ESTABLISHMENT, OR ARE YOU PART OF IT?'");
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
            "[TEXT] LOG #009: Orbital optics synchronized with cctv_core.vnet feeds.",
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
        res.push("[LINK:cctv_core.vnet] >> VIEW CITY WIDE CCTV BACKDOOR NODE");
        res.push("[LINK:archival.vnet] >> ACCESS RESTRICTED MILITARY VFS DUMP");
        res.push("[LINK:dollhouse.vnet] >> INSPECT SURVEILLANCE FEED ROOM 402");
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "zeroauction.vnet") {
        res :: Array = [
            "[TITLE] ZERO-DAY EXPLOIT AUCTION HOUSE",
            "[HR]",
            "[TEXT] Exclusive marketplace for unpublished kernel vulnerabilities.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | ACTIVE LOTS: 14 KERNEL FLAWS | HIGHEST BID: 4.2 VCOIN     |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] EXPLOIT_ID: WINDOWS_11_RING0_BYPASS_09"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "shadowpay.vnet") {
        res :: Array = [
            "[TITLE] SHADOWPAY - CRYPTO MIXER",
            "[HR]",
            "[TEXT] Advanced zero-knowledge coin laundering facility.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | MIXING FEE: 1.5% | POOL ANONYMITY: MAXIMUM              |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] MIX_STATE: BLOCKS_SCRAMBLED_SUCCESS"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "cctv_core.vnet") {
        res :: Array = [
            "[TITLE] CITY WIDE CCTV BACKDOOR NODE",
            "[HR]",
            "[TEXT] Live multiplex feed from metropolitan surveillance cameras.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | ACTIVE FEEDS: 4,192 CAMERAS | RESOLUTION: 4K OPTICAL    |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] FEED_MATRIX: METRO_GRID_SECTOR_4"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "eye.vnet") {
        res :: Array = [
            "[TITLE] PROJECT HORUS / ALL-SEEING EYE",
            "[HR]",
            "[TEXT] Automated facial recognition database tracking global identities.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | INDEXED FACES: 4.2 BILLION | MATCH RATE: 99.4%          |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] HORUS_INDEX_REF: 0xE4E4_GLOBAL_MAP"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "orbital.vnet") {
        res :: Array = [
            "[TITLE] LOW ORBIT ION CANNON & KINETIC STRIKE TERMINAL",
            "[HR]",
            "[BLOOD] [WARNING]: SATELLITE WEAPONIZATION PROTOCOL ACTIVE. AUTHORIZATION REQUIRED.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | ORBITAL PLATFORM: SAT-99 | STATUS: ARMED & TRACKING     |",
            "[BOX] | PAYLOAD: TUNGSTEN ROD KINETIC BUNDLE | YIELD: 11.5 KT   |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] Telemetry control interface for tactical kinetic strike platforms.",
            "[TEXT] SAT-99 maintains a geostationary lock over metropolitan coordinates,",
            "[TEXT] capable of delivering sub-surface penetration strikes within 180 seconds.",
            "[TEXT] LOG #881: Automated targeting array locked onto unauthorized subnet nodes.",
            "[TEXT] LOG #882: Warning: Firing kinetic rods without multi-sig gateway clearance",
            "[TEXT] will trigger an immediate counter-strike from global defense grid relays.",
            "[CODE] TARGET_LOCK_HASH: 0xORB_4402_LOCKED",
            "[CODE] ORBITAL_DECAY_VECTOR: STABLE_99.1_PERCENT",
            "[TEXT] Notice: Enter valid target coordinates or ping peer ports to designate strike zones."
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[PULSE] ORBITAL STRIKE READY. DESIGNATE TARGET VIA CLI ORBITAL LOCK.");
        res.push("[BLOOD] 'FROM THE HEAVENS TO YOUR LOCAL SUBNET IN THREE MINUTES FLAT.'");
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        res.push("[HR]");
        return res;
    }
    if (clean_u == "pastebin.vnet") {
        res :: Array = [
            "[TITLE] ANONYMOUS PASTEBIN & DUMP HUB",
            "[HR]",
            "[TEXT] Unfiltered text repository for credentials, keys, and scripts.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | TOTAL PASTES: 918,401 | ENCRYPTION: NONE                |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] PASTE_ID: 88192_PASSWORD_HASH_DUMP"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "deepwiki.vnet") {
        res :: Array = [
            "[TITLE] THE DEEP WIKI - HIDDEN INDEX",
            "[HR]",
            "[TEXT] Encyclopedia of occult networks, underground organizations, and history.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | ARTICLES: 14,290 | EDIT LOCK: PERMANENT                 |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] WIKI_INDEX: 0xWIKI_0991_OCCULT"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "schizo.vnet") {
        res :: Array = [
            "[TITLE] COLLECTIVE SCHIZOPHRENIA MANIFEST",
            "[HR]",
            "[TEXT] Unfiltered streams of consciousness from isolated terminal operators.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | AUTHOR: UNKNOWN ENTITY | STATE: MANIC                   |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] MANIFEST_ID: 0xSCHIZO_001"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
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
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "weaponry.vnet") {
        res :: Array = [
            "[TITLE] BLACK MARKET WEAPONRY EXPORT",
            "[HR]",
            "[TEXT] Unlicensed tactical ordinance and hardware procurement portal.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | INVENTORY: MIL-SPEC | ESCROW: SECURED                    |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] ARMS_CATALOG_REF: 0xGUNS_9901"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "passports.vnet") {
        res :: Array = [
            "[TITLE] FORGED IDENTITY & PASSPORT VAULT",
            "[HR]",
            "[TEXT] Complete diplomatic and civilian identity fabrication services.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | CREDENTIALS: BIOMETRICALLY MATCHED | DELIVERY: GLOBAL   |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] ID_VAULT_REF: 0xID_FORGE_881"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }
    if (clean_u == "blackbank.vnet") {
        res :: Array = [
            "[TITLE] THE BLACK BANK - OFFSHORE ACCOUNTS",
            "[HR]",
            "[TEXT] Untraceable offshore banking vault for unallocated digital assets.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | ASSETS MANAGED: 420.5 VCOIN | ENCRYPTION: MIL-SPEC        |",
            "[BOX] +---------------------------------------------------------+",
            "[CODE] BANK_VAULT_REF: 0xBANK_OFFSHORE_99"
        ];
        if (key_line != "") { res.push(key_line); }
        res.push("[LINK:shadow.dir] << RETURN TO DIRECTORY");
        return res;
    }

    return [
        "[TITLE] 404 - NODE UNREACHABLE",
        "[HR]",
        "[GLITCH] DESTINATION PURGED FROM ROUTING TABLE.",
        "[LINK:shadow.dir] << RETURN TO DIRECTORY"
    ];
}

page_body = load_page(current_url);

fn extract_link_info(line :: String) -> Array {
    close_idx :: Int64 = -1;
    len :: Int64 = line.length();
    through i :: 6..(len - 1) -> loop {
        if (line[i] == "]") { close_idx = i; break; }
    };
    if (close_idx == -1) { return ["", ""]; }
    return [line.substr(6, close_idx - 6), line.substr(close_idx + 1, len - close_idx - 1)];
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
    else if (cmd == "connect" || cmd == "goto") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: connect <url_address> (e.g. connect market.vnet)");
        } else {
            trigger_route_navigation(args);
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
    else if (cmd == "win") {
        if (current_url != "terminal.vnet") {
            cli_logs.push("[ERROR]: MASTER GATEWAY UNAVAILABLE. NAVIGATE TO terminal.vnet FIRST.");
        } else if (args == "") {
            cli_logs.push("[ERROR]: Usage: win <k1> <k2> <k3> <k4> <k5> <k6> <k7> <k8>");
        } else {
            vnet.send_to(client_sock, server_ip, server_port, "WIN:" + args);
        }
    }
    else if (cmd == "takeover") {
        if (btc_balance < 25.0) {
            cli_logs.push("[ERROR]: TAKEOVER REQUIRES 25.00 VCOIN (CURRENT: " + string(vmath.round(btc_balance * 100.0) / 100.0) + " VCOIN)");
        } else {
            vnet.send_to(client_sock, server_ip, server_port, "TAKEOVER:REQ");
        }
    }
    else if (cmd == "ice") {
        cli_logs.push("[ICE STATUS]: ACTIVE FIREWALL SHIELDS: [" + string(ice_charges) + "/3]");
    }
    else if (cmd == "netscan") {
        if (current_url == "hellroom.vnet") {
            cli_logs.push("[HELLROOM_CURSE]: SCANNERS ARE BLIND BEFORE THE HORDE. YOUR PACKETS BELONG TO THE FLAMES NOW.");
            glitch_trigger = 0.9;
        } else if (current_url == "shadow.dir") {
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
            vnet.send_to(client_sock, server_ip, server_port, "OVERLOAD:" + args);
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
        if (current_url != "crypto.vnet") {
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
        cli_logs.push("  proxy <url> <node>      - [0.40 VCOIN | 120s CD] Route connection through intermediate proxy node");
        cli_logs.push("  bounty                  - Quick jump to target bounty board");
        cli_logs.push("========== OFFENSIVE EXPLOIT COMMANDS ==========");
        cli_logs.push("  dos <port>              - [0.25 VCOIN | 15s CD] Freeze peer (3x = Drop Key)");
        cli_logs.push("  redirect <port> <url>   - [0.15 VCOIN | 10s CD] BGP Hijack peer browser");
        cli_logs.push("  snoop <port>            - [0.05 VCOIN |  5s CD] Interrogate target URL");
        cli_logs.push("  spike <port>            - [0.20 VCOIN | 12s CD] Force +35% threat trace");
        cli_logs.push("========== ECONOMY & UTILITY COMMANDS ==========");
        cli_logs.push("  mine                    - Mine +0.05 VCOIN at crypto.vnet");
        cli_logs.push("  cat /sys/config.txt     - Inspect config.txt hash key database");
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
    if (is_in_ip_menu == 1) {
        m_pos   = vglib.mouse_pos();
        mx :: Float64 = float64(m_pos[0]);
        my :: Float64 = float64(m_pos[1]);
        m_down  :: Int64 = vglib.mouse_down(vglib.MOUSE_LEFT);
        m_click :: Int64 = (m_down == 1 && mouse_was_down == 0) ? 1 : 0;
        mouse_was_down   = m_down;

        if (m_click == 1) {
            ip_box_focused = (mx >= 440.0 && mx <= 840.0 && my >= 380.0 && my <= 420.0) ? 1 : 0;
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

        connect_btn_hover = (mx >= 540.0 && mx <= 740.0 && my >= 460.0 && my <= 500.0) ? 1 : 0;
        if (vglib.key_pressed(vglib.ENTER) || (connect_btn_hover == 1 && m_click == 1)) {
            if (ip_input_buffer.length() > 0) {
                server_ip = ip_input_buffer;
            }
            is_in_ip_menu = 0;
            
            # Send initial GET request using the already bound client_sock
            current_url = "shadow.dir";
            page_body   = load_page(current_url);
            vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
        }

        vglib.begin();
        vglib.clear(COLOR_BLACK);

        vglib.rect(340, 220, 600, 380, COLOR_PANEL);
        vglib.line(340, 220, 940, 220, COLOR_BORDER);
        vglib.line(940, 220, 940, 600, COLOR_BORDER);
        vglib.line(940, 600, 340, 600, COLOR_BORDER);
        vglib.line(340, 600, 340, 220, COLOR_BORDER);

        vglib.text_ex(vcr_font, "VYNE SHADOWOS - UPLINK GATEWAY", 465, 260, 14, COLOR_BLOOD);
        vglib.text_ex(vcr_font, "ENTER HOST / SERVER IP ADDRESS:", 495, 340, 11, COLOR_CYAN);

        vglib.rect(440, 380, 400, 40, COLOR_BLACK);
        vglib.line(440, 380, 840, 380, ip_box_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        vglib.line(840, 380, 840, 420, ip_box_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        vglib.line(840, 420, 440, 420, ip_box_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        vglib.line(440, 420, 440, 380, ip_box_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        vglib.text_ex(vcr_font, ip_input_buffer, 455, 393, 12, COLOR_TOXIC);

        vglib.rect(540, 460, 200, 40, connect_btn_hover == 1 ? COLOR_BLOOD : COLOR_CLI_BG);
        vglib.line(540, 460, 740, 460, COLOR_BLOOD);
        vglib.line(740, 460, 740, 500, COLOR_BLOOD);
        vglib.line(740, 500, 540, 500, COLOR_BLOOD);
        vglib.line(540, 500, 540, 460, COLOR_BLOOD);
        vglib.text_ex(vcr_font, "CONNECT UPLINK", 575, 474, 11, connect_btn_hover == 1 ? COLOR_BLACK : COLOR_TOXIC);

        through sy :: 0..99 -> loop {
            line_y :: Float64 = float64(sy * 8);
            vglib.line(0, line_y, 1280, line_y, COLOR_SCANLINE);
        };

        vglib.end();
        continue;
    }

    run_time     = run_time + 0.016;
    cursor_blink = cursor_blink + 0.016;

    heartbeat_timer = heartbeat_timer + 0.016;
    if (heartbeat_timer >= 3.0) {
        heartbeat_timer = 0.0;
        if (clean_str(current_url) != active_down_url || active_down_timer <= 0.0) {
            vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
        }
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

        passive_trace_cd = passive_trace_cd + 0.016;
        if (passive_trace_cd >= 3.0) {
            passive_trace_cd = 0.0;
            if (current_url == "market.vnet" || current_url == "vault.vnet" || current_url == "terminal.vnet" || current_url == "forum.vnet" || current_url == "crypto.vnet" || current_url == "bounty.vnet" || current_url == "redroom.vnet") {
                trace_level = int64(vmath.clamp(float64(trace_level + 2), 0.0, 100.0));
            }
        }

        if (trace_level >= 100) {
            trace_level = 20;
            btc_balance = vmath.clamp(btc_balance - 0.30, 0.0, 999.0);
            is_connecting = 0;
            current_url   = "shadow.dir";
            input_url     = "shadow.dir";
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
                url_focused = (mx >= 120.0 && mx <= 920.0 && my >= 12.0 && my <= 48.0) ? 1 : 0;
                
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
    if (packet_in.length() >= 3) {
        net_msg :: String = string(packet_in[0]);
        server_status = net_msg;
        glitch_trigger = 0.2;
        recent_packets = recent_packets + 1;

        if (net_msg.length() > 9 && net_msg.substr(0, 9) == "KEY_SYNC:") {
            sync_payload :: String = net_msg.substr(9, net_msg.length() - 9);
            
            curr_segment :: String = "";
            tokens :: Array = [];
            through p_idx :: 0..(sync_payload.length() - 1) -> loop {
                c_char = sync_payload.substr(p_idx, 1);
                if (c_char == ":") {
                    tokens.push(curr_segment);
                    curr_segment = "";
                } else {
                    curr_segment = curr_segment + c_char;
                }
            };
            if (curr_segment.length() > 0) { tokens.push(curr_segment); }

            if (tokens.length() >= 31) {
                through k_t :: 0..7 -> loop {
                    session_keys[k_t] = string(tokens[k_t]);
                    session_locs[k_t] = string(tokens[k_t + 8]);
                };
                
                my_assigned_sites.clear();
                through s_t :: 16..30 -> loop {
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
        else if (net_msg.length() > 14 && net_msg.substr(0, 14) == "EXPLOIT:WINNER:") {
            raw_win_str :: String = net_msg.substr(14, net_msg.length() - 14);
            sep_w :: Int64 = -1;
            through wi :: 0..(raw_win_str.length() - 1) -> loop {
                if (raw_win_str[wi] == ":") { sep_w = wi; break; }
            };
            if (sep_w > 0) {
                winner_port  = raw_win_str.substr(0, sep_w);
                win_mode_str = raw_win_str.substr(sep_w + 1, raw_win_str.length() - sep_w - 1);
            } else {
                winner_port  = raw_win_str;
                win_mode_str = "KEYS";
            }
            game_over_winner = 1;
            glitch_trigger   = 1.0;
            cli_overlay_open = 0;
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
            
            curr_site :: String = "";
            through i :: 0..(dropped_dir.length() - 1) -> loop {
                c = dropped_dir.substr(i, 1);
                if (c == ":") {
                    if (curr_site != "") { cli_logs.push("  -> " + curr_site); }
                    curr_site = "";
                } else {
                    curr_site = curr_site + c;
                }
            };
            if (curr_site != "") { cli_logs.push("  -> " + curr_site); }
            
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
        } else if (net_msg.length() > 10 && net_msg.substr(0, 10) == "TELEMETRY:") {
            cli_logs.push("[TELEMETRY]: " + net_msg.substr(10, net_msg.length() - 10));
            glitch_trigger = 0.3;
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
    }

    if (glitch_trigger > 0.0) { glitch_trigger = glitch_trigger - 0.016; }

    pulse_val :: Float64 = vmath.sin(run_time * 5.0) * 0.5 + 0.5;

    # ================================================================
    # RENDER ENGINE
    # ================================================================
    vglib.begin();
        vglib.clear(COLOR_BLACK);

        jitter_x :: Float64 = (glitch_trigger > 0.0) ? (vmath.sin(run_time * 50.0) * (glitch_trigger * 12.0)) : 0.0;
        jitter_y :: Float64 = (glitch_trigger > 0.0) ? (vmath.cos(run_time * 30.0) * (glitch_trigger * 10.0)) : 0.0;

        vglib.rect(0 + jitter_x, 0 + jitter_y, 1280, 60, COLOR_PANEL);
        vglib.line(0, 60, 1280, 60, COLOR_BORDER);
        vglib.text_ex(vcr_font, "VNET", 45 + jitter_x, 22, 14, COLOR_BLOOD);

        vglib.rect(120 + jitter_x, 12 + jitter_y, 800, 36, COLOR_URLBAR);
        vglib.line(120 + jitter_x, 12 + jitter_y, 920 + jitter_x, 12 + jitter_y, url_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        display_url_str :: String = "vnet://" + (is_connecting == 1 ? pending_url : (url_focused == 1 ? input_url : current_url));
        vglib.text_ex(vcr_font, display_url_str, 135 + jitter_x, 23, 12, url_focused == 1 ? COLOR_TOXIC : COLOR_CYAN);

        vglib.rect(940 + jitter_x, 12 + jitter_y, 320, 36, COLOR_PANEL);
        btc_str :: String = "VCOIN: " + string(vmath.round(btc_balance * 100.0) / 100.0) + " VCOIN";
        vglib.text_ex(vcr_font, btc_str, 955 + jitter_x, 23, 11, COLOR_TOXIC);

        vglib.rect(20 + jitter_x, 80 + jitter_y, 890, 670, COLOR_PANEL);
        vglib.line(20, 80, 910, 80, COLOR_BORDER);

        vglib.rect(930 + jitter_x, 80 + jitter_y, 330, 670, COLOR_PANEL);
        vglib.line(930, 80, 1260, 80, COLOR_BORDER);
        vglib.text_ex(vcr_font, "SYSTEM THREAT RADAR", 945 + jitter_x, 95, 11, COLOR_AMBER);
        vglib.line(945, 110, 1245, 110, COLOR_BORDER);

        vglib.text_ex(vcr_font, "TRACE LEVEL GAUGE:", 945 + jitter_x, 120, 10, COLOR_CYAN);
        vglib.rect(945 + jitter_x, 136, 300, 14, COLOR_BLACK);
        bar_w :: Float64 = vmath.clamp((float64(trace_level) / 100.0) * 300.0, 4.0, 300.0);
        vglib.rect(945 + jitter_x, 136, bar_w, 14, (trace_level > 70) ? COLOR_BLOOD : COLOR_AMBER);
        vglib.text_ex(vcr_font, string(trace_level) + "% TRACED BY PEERS", 945 + jitter_x, 155, 10, COLOR_AMBER);

        vglib.line(945, 172, 1245, 172, COLOR_BORDER);
        vglib.text_ex(vcr_font, "RF TUNER (" + string(int64(freq_tuner)) + "Hz) SIGNAL WAVE:", 945 + jitter_x, 182, 10, COLOR_CYAN);

        wave_amp :: Float64 = vmath.clamp(6.0 + float64(recent_packets * 2), 8.0, 20.0);

        through rx :: 0..28 -> loop {
            wave_y :: Float64 = 209.0 + vmath.sin(run_time * (freq_tuner * 0.5) + float64(rx) * 0.4) * wave_amp;
            vglib.rect(945.0 + float64(rx * 10) + jitter_x, wave_y, 6, 6, (recent_packets > 3) ? COLOR_BLOOD : COLOR_TOXIC);
        };

        vglib.line(945, 235, 1245, 235, COLOR_BORDER);
        vglib.text_ex(vcr_font, "ICE SHIELD: [" + string(ice_charges) + "/3] LAYERS", 945 + jitter_x, 248, 11, COLOR_TOXIC);
        vglib.text_ex(vcr_font, "PATCH REBIND: " + ((cd_patch > 0.0) ? (string(int64(cd_patch)) + "s") : "READY"), 945 + jitter_x, 268, 10, (cd_patch > 0.0) ? COLOR_AMBER : COLOR_TOXIC);
        vglib.line(945, 285, 1245, 285, COLOR_BORDER);

        vglib.rect(945 + jitter_x, 292, 300, 440, COLOR_BLACK);

        feed_cnt = vnet_feed_logs.length();
        feed_start_y :: Float64 = 300.0 - feed_scroll_y;

        through f_idx :: 0..(feed_cnt - 1) -> loop {
            line_y :: Float64 = feed_start_y + (f_idx * 20.0);
            if (line_y >= 295.0 && line_y <= 715.0) {
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
            vglib.rect(40 + jitter_x, 140 + jitter_y, 850, 500, COLOR_BLACK);
            vglib.line(40, 140, 890, 140, COLOR_AMBER);
            vglib.line(890, 140, 890, 640, COLOR_AMBER);
            vglib.line(890, 640, 40, 640, COLOR_AMBER);
            vglib.line(40, 640, 40, 140, COLOR_AMBER);

            rem_s :: Int64 = int64(vmath.ceil(target_connection_time - connection_timer));
            vglib.text_ex(vcr_font, "[ESTABLISHING TOR PROXY HOPS]", 310 + jitter_x, 240 + jitter_y, 16, COLOR_AMBER);
            vglib.text_ex(vcr_font, "RESOLVING HANDSHAKE TO: vnet://" + pending_url, 260 + jitter_x, 290 + jitter_y, 13, COLOR_CYAN);
            vglib.text_ex(vcr_font, "LATENCY BUFFER: " + string(rem_s) + "s REMAINING", 340 + jitter_x, 340 + jitter_y, 12, COLOR_TOXIC);
            
            vglib.rect(240 + jitter_x, 390 + jitter_y, 450, 20, COLOR_PANEL);
            p_ratio :: Float64 = vmath.clamp(connection_timer / target_connection_time, 0.05, 1.0);
            vglib.rect(240 + jitter_x, 390 + jitter_y, 450.0 * p_ratio, 20, COLOR_TOXIC);

            glitch_noise :: Float64 = vmath.sin(run_time * 40.0) * 4.0;
            vglib.text_ex(vcr_font, "ANONYMIZING IP SUBNET PACKETS...", 310.0 + glitch_noise + jitter_x, 450 + jitter_y, 11, COLOR_GHOST);
        } else if (clean_str(current_url) == active_down_url && active_down_timer > 0.0) {
            vglib.rect(40 + jitter_x, 140 + jitter_y, 850, 500, COLOR_BLACK);
            vglib.line(40, 140, 890, 140, COLOR_BLOOD);
            vglib.line(890, 140, 890, 640, COLOR_BLOOD);
            vglib.line(890, 640, 40, 640, COLOR_BLOOD);
            vglib.line(40, 640, 40, 140, COLOR_BLOOD);

            rem_down_s :: Int64 = int64(active_down_timer) + 1;
            vglib.text_ex(vcr_font, "[ 503 SERVICE UNAVAILABLE - KERNEL OVERLOAD ]", 220 + jitter_x, 240 + jitter_y, 15, COLOR_BLOOD);
            vglib.text_ex(vcr_font, "TARGET NODE: " + active_down_url + " IS TEMPORARILY FRIED", 220 + jitter_x, 290 + jitter_y, 12, COLOR_AMBER);
            vglib.text_ex(vcr_font, "RECOVERY SEQUENCE ACTIVE: " + string(rem_down_s) + "s REMAINING", 290 + jitter_x, 340 + jitter_y, 11, COLOR_TOXIC);

            vglib.rect(240 + jitter_x, 400 + jitter_y, 450, 20, COLOR_PANEL);
            down_ratio :: Float64 = vmath.clamp(active_down_timer / 30.0, 0.05, 1.0);
            vglib.rect(240 + jitter_x, 400 + jitter_y, 450.0 * down_ratio, 20, COLOR_BLOOD);
            vglib.text_ex(vcr_font, ">>> REBUILDING ROUTING TABLES IN BACKGROUND BUFFER...", 235 + jitter_x, 460 + jitter_y, 10, COLOR_GHOST);
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

        if (game_over_winner == 1) {
            vglib.rect(0, 0, 1280, 800, vglib.rgba(180, 0, 20, 220));
            vglib.text_ex(vcr_font, "[ CRITICAL SYSTEM OVERRIDE - GAME OVER ]", 280 + jitter_x, 320 + jitter_y, 20, COLOR_TOXIC);
            
            win_txt :: String = "";
            if (win_mode_str == "BLACKOUT") {
                win_txt = "BLACKED OUT ALL 5 MUTUAL CORE NODES!";
            } else if (win_mode_str == "ECONOMIC") {
                win_txt = "EXECUTED AN ECONOMIC TAKEOVER (25 VCOIN)!";
            } else {
                win_txt = "BREACHED THE VFS ROOT VAULT!";
            }

            if (winner_port == string(my_port)) {
                vglib.text_ex(vcr_font, "VICTORY DECLARED! YOU HAVE " + win_txt, 180 + jitter_x, 370 + jitter_y, 15, COLOR_TOXIC);
            } else {
                vglib.text_ex(vcr_font, "NODE PORT_" + winner_port + " HAS " + win_txt, 180 + jitter_x, 370 + jitter_y, 15, COLOR_TOXIC);
            }
            
            vglib.text_ex(vcr_font, "ALL SUBNET CONNECTIONS PERMANENTLY LOCKED", 340 + jitter_x, 420 + jitter_y, 14, COLOR_GHOST);
        }

        vglib.draw_scanlines(8.0, vglib.rgba(0, 0, 0, 90));

        vglib.rect(0, 765, 1280, 35, COLOR_PANEL);
        vglib.line(0, 765, 1280, 765, COLOR_BORDER);
        vglib.text_ex(vcr_font, "TOR ROUTER ACTIVE | NODE PORT: " + string(my_port) + " | [TAB] TOGGLE TERMINAL OVERLAY", 20, 776, 10, COLOR_CYAN);

    vglib.end();
}

vnet.close(client_sock);
vglib.close();