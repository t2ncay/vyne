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

vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);

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
    "TYPE 'patch' TO REBIND NEW PORT SOCKET (0.70 BTC | 120s CD)",
    "TYPE 'sniffer' TO INTERCEPT GLOBAL UDP PACKET TRAFFIC",
    "TYPE 'freq' TO TUNER RF SIGNAL ANALYZER",
    "TYPE 'honeypot <url>' TO SET AN AMBUSH TRAP (0.10 BTC)",
    "TYPE 'chat <msg>' TO BROADCAST REAL-TIME P2P MESSAGES",
    "TYPE 'buy ice' TO PURCHASE ICE DEFENSE SHIELD (0.30 BTC)",
    "TYPE 'win <k1> ... <k8>' TO OVERRIDE VFS ROOT VAULT",
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
btc_balance      :: Float64 = 1.00;
trace_level      :: Int64   = 14;
ice_charges      :: Int64   = 1;
dos_timer        :: Float64 = 0.0;
passive_trace_cd :: Float64 = 0.0;
heartbeat_timer  :: Float64 = 0.0;

# CYBERWARFARE MECHANIC STATES
sniffer_mode     :: Int64   = 0;
freq_tuner       :: Float64 = 18.5; # FREQUENCY IN HZ
recent_packets   :: Int64   = 0;
packet_decay_cd  :: Float64 = 0.0;

game_over_winner :: Int64   = 0;
winner_port      :: String  = "";

# EXPLOIT & UTILITY COMMAND COOLDOWNS
cd_dos           :: Float64 = 0.0;
cd_redirect      :: Float64 = 0.0;
cd_spike         :: Float64 = 0.0;
cd_snoop         :: Float64 = 0.0;
cd_mine          :: Float64 = 0.0;
cd_patch         :: Float64 = 0.0; # 120s REBIND COOLDOWN

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
    if (ice_charges >= 3) {
        cli_logs.push("[ERROR]: MAX ICE FIREWALL CAPACITY REACHED (3/3)");
        return 0;
    }
    if (btc_balance < 0.30) {
        cli_logs.push("[ERROR]: INSUFFICIENT BTC FOR ICE SHIELD (REQUIRES 0.30 BTC)");
        return 0;
    }
    btc_balance = btc_balance - 0.30;
    ice_charges = ice_charges + 1;
    glitch_trigger = 0.2;
    cli_logs.push("[STORE]: ICE FIREWALL LAYER INSTALLED! ACTIVE ICE: " + string(ice_charges) + "/3");
    return 1;
}

fn trigger_route_navigation(target_dest :: String) {
    if (is_connecting == 1) { return null; }
    
    pending_url            = target_dest;
    is_connecting          = 1;
    connection_timer       = 0.0;
    target_connection_time = vmath.random(1.0, 8.0);
    glitch_trigger         = 0.3;
    
    cli_logs.push("[TOR_ROUTE]: INITIATING HANDSHAKE WITH " + target_dest + "... ESTIMATED LATENCY: " + string(int64(target_connection_time)) + "s");
}

fn load_page(url :: String) -> Array {
    clean_u = url;
    
    if (clean_u == "shadow.dir") {
        return [
            "[TITLE] SHADOWNET ANONYMOUS DIRECTORY v4.09",
            "[HR]",
            "[GLITCH] [WARNING]: UNREGISTERED EYE CONTACT DETECTED THROUGH MONITOR GLASS.",
            "[PULSE] ALL ROUTED PACKETS ARE MIRRORED TO RESTRICTED VFS MEMORY STACKS.",
            "[TEXT] System Node #0091-B. Built upon decommissioned military routing tables.",
            "[HR]",
            "[SUBTITLE] CATEGORY: MARKETPLACES & BLACK DATA EXCHANGES",
            "[LINK:market.vnet] -> The Red Market (Defensive ICE & Stolen Keys)",
            "[LINK:crypto.vnet] -> Black Tumbler Wallet Ledger (Mining Rig)",
            "[LINK:bounty.vnet] -> Peer Hitman Contract Index (Target Bounties)",
            "[TEXT] ",
            "[SUBTITLE] CATEGORY: RESTRICTED STREAMS & SURVEILLANCE NODES",
            "[LINK:dollhouse.vnet] -> Surveillance Feed #0992 (Room 402 Live Telemetry)",
            "[LINK:redroom.vnet] -> Encrypted Stream Node Alpha (Hostile Entity Tracer)",
            "[LINK:morgue.vnet] -> Digital Autopsy Database (Unclaimed Terminal Subjects)",
            "[LINK:snuff.vnet] -> Corrupted Frame Buffer Archive (Pixel Bleed Raw Captures)",
            "[TEXT] ",
            "[SUBTITLE] CATEGORY: DATA VAULTS & ANONYMOUS BOARDS",
            "[LINK:vault.vnet] -> Corrupted VFS Memory Dump /vault/sys/",
            "[LINK:forum.vnet] -> /b/ - Terminal Whispers & Schizo Archives",
            "[LINK:terminal.vnet] -> Master Decryption Gateway Node (Vault Keys Required)",
            "[LINK:archival.vnet] -> Military Firmware Keys (Restricted Core Dump)",
            "[TEXT] ",
            "[SUBTITLE] CATEGORY: EXPERIMENTAL ANOMALIES & DARK LABS",
            "[LINK:asylum.vnet] -> Patient Telemetry #1988 (Sub-Level 4 Biometrics)",
            "[LINK:silence.vnet] -> Low-Frequency Acoustic Distortion Rig",
            "[LINK:blackout.vnet] -> Regional Power Grid Control Override",
            "[LINK:ghost.vnet] -> Spectral Signal Tracer Frequency Monitor",
            "[LINK:cult.vnet] -> Digital Ritual Cipher Gateway",
            "[LINK:void.vnet] -> Deep Web Abyss Terminal Node",
            "[HR]",
            "[BLOOD] 'THEY CAN SEE THROUGH THE CRT SCREEN... DON'T LOOK BACK.'",
            "[TEXT] Tip: Press [TAB] to toggle terminal overlay. Mine BTC at crypto.vnet."
        ];
    }
    if (clean_u == "market.vnet") {
        return [
            "[TITLE] THE RED MARKET - SECURITY & BLACK DATA EXCHANGE",
            "[HR]",
            "[GLITCH] [WARN]: YOUR PUBLIC UDP PORT IS BROADCAST TO ACTIVE SWARM PEERS",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | DEFENSE MODULE #01: ACTIVE ICE FIREWALL SHIELD             |",
            "[BOX] | DETAILS: Auto-absorbs 1 inbound DOS, Hijack or Spike.    |",
            "[BOX] | PRICE: 0.30 BTC | CAP: 3 LAYERS | CURRENT: [" + string(ice_charges) + "/3]           |",
            "[BOX] +---------------------------------------------------------+",
            "[LINK:buy_ice] [>>> CLICK HERE TO PURCHASE ICE SHIELD (0.30 BTC) <<<]",
            "[TEXT] ",
            "[SUBTITLE] RECENT EXFILTRATED DATA LOTS:",
            "[CODE] LOT #881: MILITARY FIRMWARE DUMP - KEY_FRAGMENT_2 [1337]",
            "[CODE] LOT #882: SECTOR 4 BIOMETRIC SCANS - 1,400 SUBJECT RECORDS",
            "[CODE] LOT #883: CABLE TELEMETRY DECRYPTION KEYS - /sys/keys.bin",
            "[PULSE] LIVE AUCTION: EXFILTRATED CITIZEN DOSSIERS [CURRENT BID: 0.25 BTC]",
            "[TEXT] ",
            "[BLOOD] LEAKED SELLER LOG: 'Do not stay on market.vnet past 03:00 AM. Trace spikes execute automatically.'",
            "[LINK:vault.vnet] >> JUMP TO CORRUPTED DATA VAULT",
            "[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY",
            "[HR]"
        ];
    }
    if (clean_u == "dollhouse.vnet") {
        return [
            "[TITLE] SURVEILLANCE FEED #0992 - ROOM 402",
            "[HR]",
            "[BLOOD] [CAM_402_NORTH]: HEAVY FOOTSTEPS ECHOING IN EASTERN HALLWAY...",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | SENSOR 1: AMBIENT TEMP 4.2 C (SUDDEN DROP DETECTED)      |",
            "[BOX] | SENSOR 2: OPTICAL DISTORTION AT CENTER FRAME            |",
            "[BOX] | REGISTERED VAULT KEY_3: [8008]                          |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] LOG #0412: Motion sensor tripped at 03:14:02. No physical entry logged.",
            "[TEXT] LOG #0413: Audio buffer capturing low metallic scratching under floorboards.",
            "[GLITCH] SENSOR ALERT: SOMETHING IS STANDING DIRECTLY BEHIND YOUR CRT MONITOR",
            "[PULSE] 'IF YOU HEAR IT BREATHING, DO NOT CLOSE THE BROWSER WINDOW.'",
            "[LINK:shadow.dir] << DISCONNECT IMMEDIATELY",
            "[HR]"
        ];
    }
    if (clean_u == "vault.vnet") {
        return [
            "[TITLE] CORRUPTED DATA VAULT /VFS/MEMORY/STACK",
            "[HR]",
            "[TEXT] Recovered memory dump from decommissioned gateway server #09.",
            "[CODE] 00000000: 4F 70 65 6E 53 53 4C 20 4B 65 79 20 44 75 6D 70",
            "[CODE] 00000010: 4B 45 59 5F 34 3A 20 5B 34 30 34 30 5D 20 4F 4B",
            "[CODE] 00000020: 53 59 53 5F 45 52 52 4F 52 3A 20 4E 4F 44 45 5F",
            "[CODE] 00000030: 50 41 4E 49 43 5F 4D 45 4D 4F 52 59 5F 42 4C 45",
            "[TEXT] ",
            "[PULSE] MEMORY CORRUPTION SPREADING AT OFFSET 0x88F9_STACK_OVERFLOW",
            "[BLOOD] RECOVERED SYSTEM NOTE: 'The master gateway at terminal.vnet requires all 8 key codes.'",
            "[TEXT] Key Code 4 identified: [4040]",
            "[LINK:terminal.vnet] >> GO TO MASTER DECRYPTION GATEWAY",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "forum.vnet") {
        return [
            "[TITLE] /B/ - ANONYMOUS UNFILTERED TERMINAL BOARD",
            "[HR]",
            "[TEXT] Thread #9012 - 'How many people are actually connected to ShadowNet?'",
            "[TEXT] Anonymous_991: Has anyone gathered all 8 key codes yet? I'm missing key 5 and 8.",
            "[BLOOD] Ghost_User: I found key 5 buried in this post signature [7712]. Don't tell the trace units.",
            "[TEXT] Paranoia_Node: Guys, when I ran 'snoop' on port 8000, my monitor started whining at 18kHz.",
            "[GLITCH] User_666: 'IF YOU READ THIS COMMAND, THEY ALREADY HAVE YOUR IP AND RAM HASH.'",
            "[TEXT] Anon_401: Buy ICE shields at market.vnet or you'll get frozen by peer DOS bots.",
            "[TEXT] ",
            "[SUBTITLE] SUBLIMINAL BROADCAST MATRIX:",
            "[PULSE] 'THE ENGINE IS NOT RUNNING ON YOUR CPU. YOUR CPU IS RUNNING ON THE ENGINE.'",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "redroom.vnet") {
        return [
            "[TITLE] STREAM NODE ALPHA [RESTRICTED ACCESS LEVEL 5]",
            "[HR]",
            "[BLOOD] HIGH SECURITY ALERT: TRANSMISSION MONITORED BY HOSTILE TRACER.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | SIGNAL STATUS: ENCRYPTED | STREAM HASH: [6660]          |",
            "[BOX] | BITRATE: 14.2 Mbps | ACTIVE WATCHERS: 13 PEERS          |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] FEED_DATA: Raw infrared frame buffer captured from sealed sub-basement.",
            "[GLITCH] [WARNING]: UNKNOWN ENTITY ATTEMPTING REMOTE KERNEL INJECTION ON YOUR PORT",
            "[PULSE] 'RUNNING 'FLUSH' IN CLI IS RECOMMENDED IMMEDIATELY.'",
            "[LINK:shadow.dir] << TERMINATE STREAM CONNECTION",
            "[HR]"
        ];
    }
    if (clean_u == "crypto.vnet") {
        return [
            "[TITLE] BLACK TUMBLER WALLET & MINING RIG",
            "[HR]",
            "[TEXT] P2P Distributed Proof-of-Work Terminal.",
            "[CODE] TX_ID #9081 | 12.4 BTC | CONFIRMED | KEY_7_HASH: [3141]",
            "[CODE] TX_ID #9082 |  0.50 BTC | PENDING   | PEER_PORT: 8012",
            "[TEXT] ",
            "[PULSE] MINING RIG READY. OPEN CLI OVERLAY [TAB] AND TYPE 'mine' FOR +0.05 BTC.",
            "[TEXT] Note: Mining generates local trace exposure over time. Use 'flush' to purge trace.",
            "[TEXT] Key Code 7 identified: [3141]",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "terminal.vnet") {
        return [
            "[TITLE] MASTER DECRYPTION GATEWAY TERMINAL",
            "[HR]",
            "[PULSE] FINAL NODE REACHED. INPUT ALL CODES INTO OVERLAY TERMINAL CLI.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | REGISTER CODE 1: [????] | REGISTER CODE 5: [7712]       |",
            "[BOX] | REGISTER CODE 2: [1337] | REGISTER CODE 6: [6660]       |",
            "[BOX] | REGISTER CODE 3: [8008] | REGISTER CODE 7: [3141]       |",
            "[BOX] | REGISTER CODE 4: [4040] | REGISTER CODE 8: [9999]       |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] Terminal Register Code 8: [9999]",
            "[GLITCH] SYS_STATUS: WAITING FOR ALL 8 KEYS TO BREACH VFS ROOT VAULT...",
            "[TEXT] Hint: Type 'win <k1> <k2> <k3> <k4> <k5> <k6> <k7> <k8>' in CLI",
            "[TEXT] Hint: Use 'cat /sys/config.txt' to view config keys database",
            "[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY"
        ];
    }
    if (clean_u == "morgue.vnet") {
        return [
            "[TITLE] DIGITAL AUTOPSY DATABASE - SUBJECT #409",
            "[HR]",
            "[BLOOD] SUBJECT STATUS: NO HEARTBEAT DETECTED | BRAIN ACTIVITY: 98%",
            "[BOX] | SUBJECT ID: #409-B | LOCATION: SECTOR 7 VAULT CORE      |",
            "[BOX] | CAUSE OF DEATH: HIGH-VOLTAGE KERNEL OVERLOAD              |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] BIO_LOG: Subject was found seated in front of terminal CRT display.",
            "[TEXT] BIO_LOG: Cornea patterns burned with inverted ASCII hex code.",
            "[GLITCH] ANOMALY: SUBJECT EYES OPENED DURING VFS MEMORY SCAN",
            "[PULSE] 'DO NOT LOOK INTO THE GLASS DISPLAY.'",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "silence.vnet") {
        return [
            "[TITLE] ACOUSTIC DISTORTION FREQUENCY RIG",
            "[HR]",
            "[PULSE] CURRENT FREQUENCY: 18.5 Hz (INFRASOUND INDUCING PARANOIA)",
            "[TEXT] Low-frequency feedback detected in audio driver buffer.",
            "[CODE] AUDIO_BUFFER: [0x7F, 0x12, 0xAA, 0xFF, 0x00, 0x11, 0x88]",
            "[BLOOD] 'CAN YOU HEAR THE WHISPER BEHIND THE HEADPHONE DISTORTION?'",
            "[TEXT] Transmitting resonant pulses to active UDP client sockets.",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "blackout.vnet") {
        return [
            "[TITLE] REGIONAL POWER GRID CONTROL MAINBOARD",
            "[HR]",
            "[WARN] SYSTEM DISPATCH: MAIN CIRCUIT BREAKERS TRIPPED",
            "[BOX] | SECTOR 7: DARK | SECTOR 8: DARK | MONITOR LEDS: FLICKERING |",
            "[TEXT] TELEMETRY: Emergency battery backup running at 14% capacity.",
            "[GLITCH] OVERRIDE KEY DETECTED IN BACKUP GENERATOR LOGS",
            "[PULSE] 'WHEN THE LIGHTS GO OUT, THE NETWORK STAYS ON.'",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "snuff.vnet") {
        return [
            "[TITLE] CORRUPTED FRAME BUFFER ARCHIVE",
            "[HR]",
            "[CODE] FRAME_001.RAW | STATUS: CORRUPTED | 0x00FF99_PIXEL_BLEED",
            "[CODE] FRAME_002.RAW | STATUS: CORRUPTED | SHADOW_GEOMETRY_DETECTED",
            "[BLOOD] RECOVERY ATTEMPTED: SHADOW FIGURES FOUND IN EVERY RENDERED FRAME",
            "[TEXT] Archive compiled from corrupted VGLib texture memory pointers.",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "asylum.vnet") {
        return [
            "[TITLE] PATIENT TELEMETRY - SUB-LEVEL 4",
            "[HR]",
            "[PULSE] PATIENT #1988 VITAL MONITORS GLITCHING DANGEROUSLY",
            "[BOX] | HEART RATE: 000 BPM | BODY TEMP: 24.2 C | STATUS: ACTIVE   |",
            "[TEXT] PATIENT LOG: 'He keeps repeating port numbers in his sleep.'",
            "[GLITCH] 'CONTAINMENT CELL DOOR OPENED FROM INSIDE THE NETWORK ROUTER'",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "bounty.vnet") {
        return [
            "[TITLE] PEER CONTRACT TARGET INDEX",
            "[HR]",
            "[WARN] ACTIVE BOUNTIES PLACED ON CONNECTED NETWORK PEERS:",
            "[BOX] | TARGET PORT: 8080 | REWARD: 0.50 BTC | STATUS: HUNTED     |",
            "[BOX] | TARGET PORT: 8012 | REWARD: 0.25 BTC | STATUS: ACTIVE     |",
            "[PULSE] USE 'spike <port>' OR 'dos <port>' IN OVERLAY TERMINAL TO CLAIM BOUNTIES",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "archival.vnet") {
        return [
            "[TITLE] RESTRICTED MILITARY VFS DUMP",
            "[HR]",
            "[CODE] /sys/firmware_v9.bin | SHA256: e3b0c44298fc1c149afbf4c8996fb924",
            "[TEXT] Decryption payload requires root clearance at terminal.vnet.",
            "[TEXT] Military key fragment recovered: KEY_1 [1001]",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "ghost.vnet") {
        return [
            "[TITLE] SPECTRAL SIGNAL FREQUENCY MONITOR",
            "[HR]",
            "[BLOOD] TRACING UNREGISTERED UDP PACKETS FROM PORT 0...",
            "[GLITCH] PACKET PAYLOAD: 'WE ARE INSIDE YOUR RAM MODULES'",
            "[PULSE] 'THE MEMORY LEAK IS NOT A BUG. IT IS AN INVITATION.'",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "cult.vnet") {
        return [
            "[TITLE] DIGITAL RITUAL CIPHER GATEWAY",
            "[HR]",
            "[PULSE] SACRIFICE 0.10 BTC VIA 'flush' PURGE TRACE DEMONS",
            "[TEXT] Symbols rendered in pure hexadecimal ASCII geometry.",
            "[BLOOD] 'THE NETWORK CRAVES BLOOD AND BANDWIDTH'",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "void.vnet") {
        return [
            "[TITLE] DEEP WEB ABYSS TERMINAL NODE",
            "[HR]",
            "[GLITCH] YOU HAVE REACHED THE END OF SHADOWNET ROUTING TABLES.",
            "[PULSE] THERE IS NOTHING HERE EXCEPT THE ECHO OF YOUR OWN PORT.",
            "[BLOOD] 'WHY ARE YOU STILL LOOKING AT THIS SCREEN?'",
            "[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY"
        ];
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
            vnet.send_to(client_sock, server_ip, server_port, "CHAT:" + args);
        }
    }
    else if (cmd == "patch") {
        if (cd_patch > 0.0) {
            cli_logs.push("[ERROR]: PORT REBIND COOLDOWN ACTIVE (" + string(int64(cd_patch) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.70) {
            cli_logs.push("[ERROR]: INSUFFICIENT BTC BALANCE (REQUIRES 0.70 BTC)");
        } else {
            btc_balance = btc_balance - 0.70;
            cd_patch = 120.0; # 2 MINUTES COOLDOWN
            vnet.send_to(client_sock, server_ip, server_port, "PATCH:REBIND");
            cli_logs.push("[SECURITY]: REQUESTING EMERGENCY PORT REBIND FROM GATEWAY...");
        }
    }
    else if (cmd == "sniffer") {
        sniffer_mode = (sniffer_mode == 1) ? 0 : 1;
        status_str :: String = (sniffer_mode == 1) ? "ENABLED" : "DISABLED";
        cli_logs.push("[SNIFFER]: GLOBAL PACKET INTERCEPTOR " + status_str);
    }
    else if (cmd == "freq") {
        if (args == "") {
            cli_logs.push("[FREQ]: CURRENT TUNED FREQUENCY: " + string(freq_tuner) + " Hz");
        } else {
            freq_tuner = vmath.clamp(float64(int64(args)), 1.0, 100.0);
            cli_logs.push("[FREQ]: TUNED TO " + string(freq_tuner) + " Hz");
        }
    }
    else if (cmd == "honeypot") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: honeypot <target_url>");
        } else if (btc_balance < 0.10) {
            cli_logs.push("[ERROR]: INSUFFICIENT BTC (REQUIRES 0.10 BTC)");
        } else {
            btc_balance = btc_balance - 0.10;
            vnet.send_to(client_sock, server_ip, server_port, "HONEYPOT:" + args);
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
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: win <k1> <k2> <k3> <k4> <k5> <k6> <k7> <k8>");
        } else {
            vnet.send_to(client_sock, server_ip, server_port, "WIN:" + args);
        }
    }
    else if (cmd == "ice") {
        cli_logs.push("[ICE STATUS]: ACTIVE FIREWALL SHIELDS: [" + string(ice_charges) + "/3]");
    }
    else if (cmd == "netscan") {
        cli_logs.push("[NETSCAN]: Scanning active peers on " + current_url + "...");
        vnet.send_to(client_sock, server_ip, server_port, "NETSCAN:" + current_url);
    }
    else if (cmd == "dos") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: dos <target_port>");
        } else if (cd_dos > 0.0) {
            cli_logs.push("[ERROR]: DOS EXPLOIT RECHARGING (" + string(int64(cd_dos) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.25) {
            cli_logs.push("[ERROR]: INSUFFICIENT BTC BALANCE (REQUIRES 0.25 BTC)");
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
            cli_logs.push("[ERROR]: INSUFFICIENT BTC BALANCE (REQUIRES 0.15 BTC)");
        } else {
            btc_balance = btc_balance - 0.15;
            cd_redirect = 10.0;
            vnet.send_to(client_sock, server_ip, server_port, "REDIRECT:" + target_p + ":" + target_u);
        }
    }
    else if (cmd == "snoop") {
        if (args == "") {
            cli_logs.push("[ERROR]: Usage: snoop <target_port>");
        } else if (cd_snoop > 0.0) {
            cli_logs.push("[ERROR]: SNOOP RECHARGING (" + string(int64(cd_snoop) + 1) + "s REMAINING)");
        } else if (btc_balance < 0.05) {
            cli_logs.push("[ERROR]: INSUFFICIENT BTC BALANCE (REQUIRES 0.05 BTC)");
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
            cli_logs.push("[ERROR]: INSUFFICIENT BTC BALANCE (REQUIRES 0.20 BTC)");
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
            cli_logs.push("[MINER]: SUCCESSFUL BLOCK PROOF! +0.05 BTC REWARD.");
        }
    }
    else if (cmd == "flush") {
        if (btc_balance < 0.10) {
            cli_logs.push("[ERROR]: INSUFFICIENT BTC FOR PROXY FLUSH (REQUIRES 0.10 BTC)");
        } else {
            btc_balance = btc_balance - 0.10;
            trace_level = int64(vmath.clamp(float64(trace_level - 30), 0.0, 100.0));
            cli_logs.push("[VPN]: ROUTE PURGED! TRACE REDUCED BY 30%.");
        }
    }
    else if (cmd == "wallet") {
        cli_logs.push("================ WALLET & DEFENSE ================");
        cli_logs.push("  BTC BALANCE  : " + string(vmath.round(btc_balance * 100.0) / 100.0) + " BTC");
        cli_logs.push("  TRACE THREAT : " + string(trace_level) + "%");
        cli_logs.push("  ICE SHIELDS  : [" + string(ice_charges) + "/3] LAYERS ACTIVE");
        cli_logs.push("  PATCH COOLDOWN: " + ((cd_patch > 0.0) ? (string(int64(cd_patch)) + "s") : "READY"));
        cli_logs.push("==================================================");
    }
    else if (cmd == "scan") {
        vnet.send_to(client_sock, server_ip, server_port, "SCAN:REQ");
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
        cli_logs.push("========== CYBERWARFARE & SENSING COMMANDS ==========");
        cli_logs.push("  patch                   - [0.70 BTC | 120s CD] Emergency rebind new socket port");
        cli_logs.push("  sniffer                 - Toggle global UDP packet sniffer mode");
        cli_logs.push("  freq <hz>               - Tune RF signal analyzer frequency");
        cli_logs.push("  honeypot <url>          - [0.10 BTC] Deploy ambush trap on URL");
        cli_logs.push("  buy ice                 - [0.30 BTC] Purchase 1 layer of ICE");
        cli_logs.push("  win <k1> ... <k8>       - Submit all 8 key codes to breach vault");
        cli_logs.push("  chat <msg> / msg <msg>  - Send real-time P2P message");
        cli_logs.push("  netscan                 - Discover active peers on current URL");
        cli_logs.push("========== OFFENSIVE EXPLOIT COMMANDS ==========");
        cli_logs.push("  dos <port>              - [0.25 BTC | 15s CD] Freeze peer (3x = Drop Key)");
        cli_logs.push("  redirect <port> <url>   - [0.15 BTC | 10s CD] BGP Hijack peer browser");
        cli_logs.push("  snoop <port>            - [0.05 BTC |  5s CD] Interrogate target URL");
        cli_logs.push("  spike <port>            - [0.20 BTC | 12s CD] Force +35% threat trace");
        cli_logs.push("========== ECONOMY & UTILITY COMMANDS ==========");
        cli_logs.push("  mine                    - Mine +0.05 BTC at crypto.vnet");
        cli_logs.push("  cat /sys/config.txt     - Inspect config.txt hash key database");
        cli_logs.push("  flush                   - [0.10 BTC] Lower trace level by -30%");
        cli_logs.push("  wallet                  - Display balance & trace stats");
        cli_logs.push("  clear                   - Wipe overlay terminal log buffer");
        cli_logs.push("====================================================");
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
    run_time     = run_time + 0.016;
    cursor_blink = cursor_blink + 0.016;

    heartbeat_timer = heartbeat_timer + 0.016;
    if (heartbeat_timer >= 3.0) {
        heartbeat_timer = 0.0;
        vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
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
        if (cd_patch > 0.0)    { cd_patch = cd_patch - 0.016; } # DECREMENT 120s PATCH CD

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
            if (current_url == "market.vnet" || current_url == "redroom.vnet" || current_url == "dollhouse.vnet" || current_url == "bounty.vnet") {
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
            cli_logs.push("[EMERGENCY_ICE]: TRACE REACHED 100%! NODE DISCONNECTED & FINED 0.30 BTC!");
        }

        if (dos_timer > 0.0) {
            dos_timer = dos_timer - 0.016;
            glitch_trigger = 0.5;
        }

        m_pos   = vglib.mouse_pos();
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
        }
    }

    packet_in :: Array = vnet.recv_from(client_sock);
    if (packet_in.length() >= 3) {
        net_msg :: String = string(packet_in[0]);
        server_status = net_msg;
        glitch_trigger = 0.2;
        recent_packets = recent_packets + 1;

        if (net_msg.length() > 14 && net_msg.substr(0, 14) == "EXPLOIT:WINNER:") {
            winner_port = net_msg.substr(14, net_msg.length() - 14);
            game_over_winner = 1;
            glitch_trigger   = 1.0;
            cli_overlay_open = 0;
        }
        else if (net_msg.length() > 14 && net_msg.substr(0, 14) == "PATCH_SUCCESS:") {
            new_assigned_port :: Int64 = int64(net_msg.substr(14, net_msg.length() - 14));
            
            # Close old socket & open new socket on new port
            vnet.close(client_sock);
            my_port = new_assigned_port;
            client_sock = vnet.udp_socket(my_port);
            
            glitch_trigger = 0.8;
            cli_logs.push("[SECURITY]: PORT REBOUND SUCCESSFUL! NEW ACTIVE PORT: " + string(my_port));
            
            # Send initial handshake from new port
            vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
        }
        else if (net_msg.length() > 17 && net_msg.substr(0, 17) == "HONEYPOT_TRIPPED:") {
            cli_logs.push("[AMBUSH ALERT]: " + net_msg.substr(17, net_msg.length() - 17));
            glitch_trigger = 0.6;
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
        }
        else {
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
                cli_logs.push("[REWARD]: EXFILTRATED DATA SOLD ON BLACK MARKET! +0.50 BTC");
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
        vglib.text_ex(vcr_font, "SHADOWNET v9.5", 15 + jitter_x, 22, 14, COLOR_BLOOD);

        vglib.rect(120 + jitter_x, 12 + jitter_y, 800, 36, COLOR_URLBAR);
        vglib.line(120 + jitter_x, 12 + jitter_y, 920 + jitter_x, 12 + jitter_y, url_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        display_url_str :: String = "vnet://" + (is_connecting == 1 ? pending_url : (url_focused == 1 ? input_url : current_url));
        vglib.text_ex(vcr_font, display_url_str, 135 + jitter_x, 23, 12, url_focused == 1 ? COLOR_TOXIC : COLOR_CYAN);

        vglib.rect(940 + jitter_x, 12 + jitter_y, 320, 36, COLOR_PANEL);
        btc_str :: String = "BTC: " + string(vmath.round(btc_balance * 100.0) / 100.0) + " BTC";
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
        
        through rx :: 0..28 -> loop {
            wave_y :: Float64 = 215.0 + vmath.sin(run_time * (freq_tuner * 0.5) + float64(rx) * 0.4) * (10.0 + float64(recent_packets * 4));
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
                f_txt_truncated :: String = truncate_str(f_txt, 32);

                f_col = COLOR_CYAN;
                if (f_txt.substr(0, 12) == "[TRACE SPIKE]") { f_col = COLOR_AMBER; }
                if (f_txt.substr(0, 12) == "[DOS ATTACK]") { f_col = COLOR_BLOOD; }
                if (f_txt.substr(0, 12) == "[ICE LOCKOUT]") { f_col = COLOR_BLOOD; }
                if (f_txt.substr(0, 12) == "[BGP HIJACK]") { f_col = COLOR_TOXIC; }
                if (f_txt.substr(0, 18) == "[SOCKET MIGRATION]") { f_col = COLOR_TOXIC; }
                if (f_txt.substr(0, 13) == "[WHALE ALERT]") { f_col = COLOR_TOXIC; }
                if (f_txt.substr(0, 10) == "[HONEYPOT]") { f_col = COLOR_AMBER; }
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

            rem_s :: Int64 = int64(target_connection_time - connection_timer) + 1;
            vglib.text_ex(vcr_font, "[ESTABLISHING TOR PROXY HOPS]", 310 + jitter_x, 240 + jitter_y, 16, COLOR_AMBER);
            vglib.text_ex(vcr_font, "RESOLVING HANDSHAKE TO: vnet://" + pending_url, 260 + jitter_x, 290 + jitter_y, 13, COLOR_CYAN);
            vglib.text_ex(vcr_font, "LATENCY BUFFER: " + string(rem_s) + "s REMAINING", 340 + jitter_x, 340 + jitter_y, 12, COLOR_TOXIC);
            
            vglib.rect(240 + jitter_x, 390 + jitter_y, 450, 20, COLOR_PANEL);
            p_ratio :: Float64 = vmath.clamp(connection_timer / target_connection_time, 0.05, 1.0);
            vglib.rect(240 + jitter_x, 390 + jitter_y, 450.0 * p_ratio, 20, COLOR_TOXIC);

            glitch_noise :: Float64 = vmath.sin(run_time * 40.0) * 4.0;
            vglib.text_ex(vcr_font, "ANONYMIZING IP SUBNET PACKETS...", 310.0 + glitch_noise + jitter_x, 450 + jitter_y, 11, COLOR_GHOST);
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
            
            if (winner_port == string(my_port)) {
                vglib.text_ex(vcr_font, "VICTORY DECLARED! YOU HAVE BREACHED THE VFS ROOT VAULT!", 210 + jitter_x, 370 + jitter_y, 16, COLOR_TOXIC);
            } else {
                vglib.text_ex(vcr_font, "NODE PORT_" + winner_port + " HAS BREACHED THE VFS ROOT VAULT FIRST!", 200 + jitter_x, 370 + jitter_y, 16, COLOR_TOXIC);
            }
            
            vglib.text_ex(vcr_font, "ALL SUBNET CONNECTIONS PERMANENTLY LOCKED", 340 + jitter_x, 420 + jitter_y, 14, COLOR_GHOST);
        }

        through sy :: 0..99 -> loop {
            line_y :: Float64 = float64(sy * 8);
            vglib.line(0, line_y, 1280, line_y, COLOR_SCANLINE);
        };

        vglib.rect(0, 765, 1280, 35, COLOR_PANEL);
        vglib.line(0, 765, 1280, 765, COLOR_BORDER);
        vglib.text_ex(vcr_font, "TOR ROUTER ACTIVE | NODE PORT: " + string(my_port) + " | [TAB] TOGGLE TERMINAL OVERLAY", 20, 776, 10, COLOR_CYAN);

    vglib.end();
}

vnet.close(client_sock);
vglib.close();