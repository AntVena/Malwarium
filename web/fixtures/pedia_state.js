// pedia_state.js — SAMPLE state fixture, standing in for the device's live
// GET /pedia_state.json. Browser preview only; never served on-device.
// The ESP32 replaces this by serving /pedia_state.json live; app.js prefers the live
// fetch and falls back to this embedded fixture so index.html runs from disk (file://).
// Every reveal state appears at least once on purpose — this is the design QA payload.
window.PEDIA_STATE_FIXTURE = {
  "hacker_tag": "NETRUNNER_99",
  "update_url": "https://example.invalid/malwarium/manifest.json",
  "currency_bits": 42048,
  "hacker_rank": 4,
  "networks_seen": 137,

  // extension: the landing page's live pet card
  "active_pet": { "species": "paypup", "uptime": "02:14:36", "frag_pct": 12, "mistakes": 1 },

  "pets": {
    "cryptoshell": "hatched",
    "paypup": "hatched",
    "malbear": "seen",
    "bruinforce": "locked",
    "berserkernel": "locked",
    "cachemutt": "seen"
  },

  // extension: wild roster — seen on encounter, defeated = full reveal (D-P3)
  "malbeasts": {
    "glitchhog": "defeated",
    "segfault_pup": "seen",
    "packet_wraith": "seen",
    "cache_ghoul": "locked",
    "buffer_wyrm": "locked",
    "kernel_leviathan": "locked"
  },

  "items": {
    "airgap_snack": "unlocked",
    "tortilla_chip": "unlocked",
    "yubi_cookie": "locked",
    "restore_point": "locked",
    "decrypt_key": "unlocked",
    "backup_drive": "unlocked",
    "sinkhole_trap": "locked",
    "disk_scrubber": "unlocked",
    "null_noodles": "unlocked",
    "r007_b33r": "unlocked",
    "sealed_cache": "unlocked",
    "sealed_cache_common": "unlocked",
    "sealed_cache_uncommon": "locked",
    "sealed_cache_rare": "locked",
    "sealed_cache_epic": "locked",
    "access_token": "unlocked",
    "safe_mode_key": "locked",
    "rollback": "locked",
    "decryptogram": "unlocked"
  },

  "mods": {
    "clock_speed_boost": "unlocked",
    "packet_sniffer": "unlocked",
    "crypto_coprocessor": "unlocked",
    "tpm_chip": "unlocked",
    "firewall_patch": "unlocked",
    "solid_state_cache": "locked",
    "watchdog_timer": "locked",
    "overclock_chip": "locked",
    "heat_sink": "locked",
    "honeytoken": "locked",
    "cipher_asic": "locked",
    "faraday_cage": "locked",
    "deadman_switch": "locked",
    "raid_mirror": "locked",
    "ecc_memory": "locked",
    "load_balancer": "locked"
  },

  "moves": {
    "quick_jab": "owned",
    "packet_storm": "owned",
    "fork_bomb": "owned",
    "checksum_guard": "owned",
    "buffer_overflow": "locked",
    "rootkit_strike": "locked",
    "null_route": "owned",
    "payload_drop": "owned",
    "double_extortion": "locked",
    "mbr_wipe": "locked",
    "aes_lockbox": "owned",
    "rsa_vault": "locked",
    "full_disk_encryption": "locked",
    "spoof_bubble": "locked",
    "proxy_shell": "locked",
    "bathyspoof": "locked",
    "smish_hook": "locked",
    "spear_strike": "locked",
    "whaling_harpoon": "locked",
    "system_hang": "locked",
    "data_rot": "locked"
  },

  "achievements": {
    "FIRST_BRUTE_FORCE": "complete",
    "SURVIVED_LOCKOUT": "complete",
    "FLAWLESS_RUN": "incomplete",
    "GONE_ROGUE": "incomplete",
    "WORM_WHISPERER": "incomplete",
    "FULL_PEDIA_L1": "incomplete",
    "BIT_BARON": "complete",
    "AIR_GAPPED": "incomplete",
    "GENERATION_X": "incomplete",
    "DEVTOOLS_INTRUDER": "incomplete"
  },

  // How far along each unfinished COUNTABLE ladder is. The device emits one entry
  // per non-Event row (pedia_state.cpp); a sample of the shapes is enough here —
  // a barely-started ladder, a nearly-done one, and a 0/N — so the cards' bars are
  // exercised offline. A row absent from this map correctly renders no bar.
  "achievement_progress": {
    "FULL_PEDIA_L1":  { "value": 2,    "goal": 9 },
    "GENERATION_X":   { "value": 7,    "goal": 10 },
    "SPECIES_3":      { "value": 2,    "goal": 3 },
    "BOSS_10":        { "value": 1,    "goal": 10 },
    "SUBS_5":         { "value": 0,    "goal": 5 },
    "MALBEAST_3":     { "value": 2,    "goal": 3 },
    "CUISINE_ALL":    { "value": 11,   "goal": 29 },
    "NETS_100":       { "value": 43,   "goal": 100 },
    "RIG_MAXED":      { "value": 6,    "goal": 479 },
    "BITS_50K":       { "value": 42048, "goal": 50000 }
  },

  // extension: the Archive tab — past pets, dignified vs crashed
  "archive": [
    { "name": "BITSY", "species": "bruinforce", "stage": "DAEMON", "cause": "retired",
      "record": "0 care mistakes \u00b7 14 days uptime \u00b7 graduated with honors" },
    { "name": "NULLBYTE", "species": "malbear", "stage": "SCRIPT", "cause": "crashed",
      "record": "frag 100% \u00b7 lockout expired \u00b7 last seen chewing the UI" },
    { "name": "SNACC_DAEMON", "species": "cachemutt", "stage": "PROCESS", "cause": "crashed",
      "record": "cache overflow \u00b7 4 care mistakes \u00b7 it hoarded too hard" }
  ]
};
