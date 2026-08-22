// test_main.cpp — the native gate ROSTER and the two harnesses that run it.
//
// The gate bodies live in test_<subject>.cpp beside this file; this one holds the
// single list of what runs. MAL_RUN_ALL_GATES is expanded three times: once to
// declare every gate, and once by each harness to call them —
//   * ctest (CMake `mal_tests`)      — the counting CHECK + the hand-rolled main().
//   * `pio test -e native` (Unity)   — CHECK maps to a Unity assertion and each gate
//                                      is a RUN_TEST (PlatformIO defines
//                                      PIO_UNIT_TESTING for test builds).
// So the roster cannot drift from either harness, and a gate defined but never
// listed here is caught by tools/check_test_roster.py rather than by the linker.
#include "test_gates.h"

#ifndef PIO_UNIT_TESTING
int g_failures = 0;
#endif

// The gate roster — listed once, expanded by whichever harness compiles the
// file. RUN(fn) is supplied per harness (a plain call for ctest, RUN_TEST for
// Unity). Keep this in milestone order; add new gates here, not in main().
#define MAL_RUN_ALL_GATES(RUN)              \
    /* T0–T2 — pipeline + idle + STAT */    \
    RUN(test_sprite_roundtrip)              \
    RUN(test_upscale_boundaries)            \
    RUN(test_palette_luminance_ordered)     \
    RUN(test_idle_habitat)                  \
    RUN(test_hunger_alert_gated)            \
    RUN(test_idle_breathe_animates)         \
    RUN(test_sd_icon_reveal_window)         \
    RUN(test_capture_badge_phases)          \
    RUN(test_idle_status_icons_grayscale)   \
    RUN(test_sd_recheck_request_seam)       \
    RUN(test_pet_model_zones)               \
    RUN(test_care_branch_and_clamp)         \
    RUN(test_hunger_decay)                  \
    RUN(test_content_registry)              \
    RUN(test_grayscale_gate)                \
    RUN(test_stat_paging_loadout_xp)        \
    RUN(test_loadout_rows_model)            \
    RUN(test_effect_text_templates_resolve) \
    RUN(test_effect_text_fits_its_screen_budget) \
    RUN(test_stat_loadout_b_scroll)         \
    RUN(test_stat_prose_windows_tile_the_list) \
    /* Carousel */                          \
    RUN(test_carousel_summon)               \
    RUN(test_carousel_bookwrap)             \
    RUN(test_carousel_focus_grayscale)      \
    RUN(test_carousel_enter_back)           \
    RUN(test_caption_pinned_top)            \
    RUN(test_carousel_autodefocus)          \
    RUN(test_carousel_ui_modes)             \
    /* The raising loop */                  \
    RUN(test_inventory)                     \
    RUN(test_event_log)                     \
    RUN(test_inventory_rows_grouped)        \
    RUN(test_save_v54_renames_the_snack_item_id) \
    RUN(test_inventory_rows_rarity_desc)    \
    RUN(test_inventory_scrollbar)           \
    RUN(test_feed_and_maint_model)          \
    RUN(test_feeding_flow)                  \
    RUN(test_item_context_gate)             \
    RUN(test_maint_flow)                    \
    RUN(test_defrag_costs_stage_bits)       \
    RUN(test_defrag_gated_when_broke)       \
    RUN(test_defrag_variants)               \
    RUN(test_defrag_tool_gated_in_items)    \
    RUN(test_defrag_count_freeze_thaw)      \
    RUN(test_arch_release_stored_frees_slot) \
    RUN(test_restore_point_shield_blocks_next_mistake) \
    RUN(test_restore_point_once_per_lifetime) \
    RUN(test_restore_shield_covers_maint_fail) \
    RUN(test_yubi_cookie_once_per_lifetime) \
    RUN(test_lockout_expire_mistakes)       \
    RUN(test_backup_drive_arms_and_lapses)  \
    RUN(test_inert_use_keeps_the_item)      \
    RUN(test_recipes_are_not_for_sale_at_any_price) \
    RUN(test_recipes_wait_on_the_merge_hub) \
    RUN(test_browns_recipes_wait_on_meeting_both_dishes) \
    RUN(test_recipes_persist) \
    RUN(test_recipes_past_the_legacy_mask_persist) \
    RUN(test_tiramisudo_upgrades_once_then_feeds) \
    RUN(test_tiramisudo_regen_actually_runs_faster) \
    RUN(test_tiramisudo_upgrade_survives_the_rack) \
    RUN(test_save_v50_bandwidth_regen_upgrade) \
    RUN(test_buffs_page_lists_the_bandwidth_upgrade) \
    RUN(test_four_ingredient_recipe_consumes_all_inputs) \
    RUN(test_stacking_food_run_climbs_then_resets_on_decay) \
    RUN(test_loot_pools_resolve_and_carry_weight) \
    RUN(test_lockout_resolve_pay)           \
    RUN(test_lockout_resolve_feed)          \
    RUN(test_items_grayscale)               \
    /* The egg's hatch */                   \
    RUN(test_hatch_lays_egg_at_idle)        \
    RUN(test_hatch_opens_the_decryption_board) \
    /* DISK DECRYPTION — the Ransomware line's board */ \
    RUN(test_decryption_three_locks_play_a_row) \
    RUN(test_decryption_counts_exact_and_elsewhere) \
    RUN(test_decryption_scores_the_attempts_it_saved) \
    RUN(test_decryption_duplicate_rule_is_the_key) \
    RUN(test_decryption_hatch_pays_the_clock) \
    RUN(test_decryption_hatch_ignores_the_arcade_dial) \
    RUN(test_decryption_arcade_pays_on_score) \
    RUN(test_decryption_grayscale)            \
    RUN(test_hatch_waits_out)               \
    RUN(test_hatch_network_accelerates)     \
    RUN(test_hatch_egg_vitals_frozen)       \
    RUN(test_hatch_grayscale)               \
    RUN(test_hatch_seam_skips)              \
    RUN(test_egg_menu_locks)                \
    RUN(test_egg_items_quest_only)          \
    RUN(test_egg_exploit_chord_hatches)     \
    RUN(test_egg_cannot_explore)            \
    RUN(test_evolve_remaining_readout)      \
    /* Task 1 — dev reset shortcut */       \
    RUN(test_reset_to_hatch)                \
    /* Evolution boundary stub */           \
    RUN(test_evolution_boot_to_process)     \
    RUN(test_evolution_c_disabled_b_gated)  \
    RUN(test_evolution_triggers_and_gates)  \
    RUN(test_evolution_script_dwell_longer_than_process)  \
    RUN(test_evolution_full_chain)          \
    RUN(test_evolution_grayscale)           \
    /* Hardening */                         \
    RUN(test_remaining_screens_grayscale)   \
    RUN(test_event_driven_repaint)          \
    RUN(test_sprite_grayscale_legibility)   \
    /* CFG submenu */                       \
    RUN(test_cfg_uimode_toggle)             \
    RUN(test_cfg_travel_confirm_asks_twice) \
    RUN(test_travel_sleep_credits_nothing_to_the_gap) \
    RUN(test_cfg_group_back_resumes_row)    \
    RUN(test_cfg_radio_reports_owner)       \
    RUN(test_cfg_radio_rows_follow_arbiter_priority) \
    RUN(test_cfg_sysinfo_sd_recheck)        \
    RUN(test_cfg_list_scroll_offset)        \
    RUN(test_cfg_brightness_apply)          \
    RUN(test_cfg_dev_reset_row)             \
    RUN(test_cfg_factory_reset_hold)        \
    RUN(test_cfg_factory_reset_scopes_differ) \
    RUN(test_cfg_screens_grayscale)         \
    /* ARCH submenu */                      \
    RUN(test_arch_list_and_record)          \
    /* MODS submenu */                      \
    RUN(test_loadout_permanent_mods)        \
    RUN(test_move_loadout)                  \
    /* Combat engine */                     \
    RUN(test_combat_deterministic)          \
    RUN(test_polymorph_pays_once_per_distinct_move) \
    RUN(test_wild_pick_weights_its_bands)   \
    RUN(test_wildcard_slot_casts_the_pool_not_itself) \
    RUN(test_mutation_engine_counts_effects_not_moves) \
    RUN(test_metamorphic_content_builds_a_real_pool) \
    RUN(test_wildcard_lock_freezes_a_slot)  \
    RUN(test_override_header_names_the_bands_present) \
    RUN(test_every_mod_reaches_the_fight)   \
    RUN(test_ledger_grudge_scales_with_the_pool) \
    RUN(test_combat_no_consecutive)         \
    RUN(test_brace_only_defend_is_not_recast) \
    RUN(test_steal_max_health_moves_the_pool) \
    RUN(test_chained_move_plays_both_halves) \
    RUN(test_level_stat_curves)             \
    RUN(test_defence_tier_retains_an_unspent_brace) \
    RUN(test_ransom_seizes_the_attack_that_hits_a_full_wall) \
    RUN(test_obfuscation_pool_salts_whoever_reads_it) \
    RUN(test_combat_override_breaks_rule)   \
    RUN(test_combat_override_item_use)      \
    RUN(test_exploit_uses_per_battle)       \
    RUN(test_combat_item_in_game)           \
    RUN(test_combat_channel)                \
    RUN(test_combat_mod_passives)           \
    RUN(test_combat_builders_and_flee)      \
    RUN(test_mods_equip_flow)               \
    RUN(test_mods_overwrite_confirm)        \
    RUN(test_mod_detail_oneshot)            \
    RUN(test_loadout_one_slot_per_mod)      \
    /* TRAIN / EXPL shells */               \
    RUN(test_loadout_expl_nav)              \
    /* Persistence */                       \
    RUN(test_save_roundtrip)                \
    RUN(test_save_version_and_empty)        \
    RUN(test_save_from_a_newer_build_still_loads) \
    RUN(test_boot_from_save_vs_hatch)       \
    RUN(test_reboot_preserves_active_creature_level) \
    RUN(test_persistence_reset_clears_store)\
    RUN(test_hackertag_editor)              \
    RUN(test_arch_store_and_deploy)         \
    RUN(test_stat_footer_and_generation)    \
    RUN(test_arch_rack_grayscale)           \
    /* Single-frame creatures */            \
    RUN(test_idle_frame_single_frame_safe)  \
    RUN(test_idle_wander_stays_inside_the_living_box) \
    RUN(test_idle_wander_reads_differently_per_locomotion) \
    RUN(test_idle_wander_crawler_never_leaves_the_floor) \
    RUN(test_idle_wander_rehomes_when_the_mover_changes) \
    RUN(test_habitat_moves_a_pet_and_parks_an_egg) \
    /* Sim-Battle + combat integration */   \
    RUN(test_sim_battle_end_to_end)         \
    RUN(test_backup_drive_save_not_spent_in_sim_battle) \
    RUN(test_backup_drive_save_armed_into_wild_combat) \
    RUN(test_rig_auto_backup_arms_save_on_explore) \
    RUN(test_rig_continuous_backup_rearms_mid_run) \
    RUN(test_creature_level_curve_and_invariant) \
    RUN(test_creature_level_feeds_combat)   \
    RUN(test_defense_diminishing_returns)   \
    RUN(test_rollback_item)                 \
    RUN(test_creature_level_persist_evolution_reset_egg) \
    RUN(test_arch_store_deploy_preserves_creature_level) \
    RUN(test_arch_deploy_loadout_is_per_pet) \
    RUN(test_combat_auto_pacing)            \
    RUN(test_combat_length_in_band)         \
    RUN(test_wild_encounter_challenge_buff) \
    RUN(test_explore_subarea_ramp)          \
    RUN(test_line_move_gating)              \
    RUN(test_ransomware_stacking)           \
    RUN(test_ransom_note)                   \
    RUN(test_ransom_note_shows_up_in_pve)   \
    RUN(test_wild_subarea_level_and_xp_scaling) \
    RUN(test_move_evolution_gating)         \
    /* Move-slot rework #11/#12 */          \
    RUN(test_move_pool_per_slot_fallback)   \
    RUN(test_move_slot_type_lock)           \
    RUN(test_move_slot_stamping_locks_at_unlock) \
    RUN(test_save_v24_slot_kinds_roundtrip) \
    RUN(test_combat_override_in_game)       \
    RUN(test_combat_stakes_live_vs_safe)    \
    RUN(test_combat_screen_grayscale)       \
    RUN(test_move_loadout_persist)          \
    /* Evolution branching + Critical System Failure */ \
    RUN(test_evolution_branch_selection)    \
    RUN(test_evolution_branch_mechanics)    \
    RUN(test_combat_loss_never_kills)       \
    RUN(test_csf_fires_and_archives)        \
    RUN(test_csf_recovery_disarms)          \
    RUN(test_csf_record_persists)           \
    RUN(test_csf_window_survives_reboot)    \
    RUN(test_csf_recovery_clears_burned_window) \
    RUN(test_save_records_roundtrip)        \
    RUN(test_save_hacker_rank_roundtrip)    \
    RUN(test_audit_scan_toggle_persists)    \
    RUN(test_csf_grayscale)                 \
    RUN(test_arch_record_readonly)          \
    /* EXPL explore-mode wiring */          \
    RUN(test_explore_arm_returns_to_idle)         \
    RUN(test_walk_event_roll_reaches_encounter)   \
    RUN(test_explore_autosteps_hands_free)        \
    RUN(test_explore_every_step_is_an_event)      \
    RUN(test_encounter_fight_live_combat_win)     \
    RUN(test_refarm_diminishing_rewards)          \
    RUN(test_bandwidth_farming_resource)          \
    RUN(test_hacker_face_toggle)                  \
    RUN(test_hacker_shop_bandwidth_upgrade)       \
    RUN(test_rig_cost_curve_formulas)              \
    RUN(test_hacker_shop_rack_slot_upgrade)       \
    RUN(test_hacker_shop_scraping_and_datamining_bonus) \
    RUN(test_hacker_shop_frag_reducer)            \
    RUN(test_hacker_shop_frag_trigger)            \
    RUN(test_hacker_shop_hunger_xp_farming)       \
    RUN(test_hacker_shop_combat_xp_boost)         \
    RUN(test_cache_not_openable_from_items)       \
    RUN(test_merge_hub_windows_its_roster)        \
    RUN(test_battle_fatigue)                      \
    RUN(test_explore_auto_continues_after_fight)  \
    RUN(test_post_encounter_reports_bandwidth_shield) \
    RUN(test_post_encounter_reports_frag_rise_when_unshielded) \
    RUN(test_post_encounter_level_line_renders) \
    RUN(test_post_encounter_never_for_sim_battle) \
    RUN(test_expl_sector_linear_gating)           \
    RUN(test_hint_bands_fit_the_canvas)            \
    RUN(test_expl_names_stay_scrollable)           \
    RUN(test_expl_level_scoped_rows)              \
    RUN(test_combat_carry_health)                 \
    RUN(test_bits_reward_bounds)                   \
    RUN(test_explore_streak_unlocks_boss_then_clears) \
    RUN(test_auto_progress_steps_positionally)    \
    RUN(test_auto_progress_gauntlet_rolls_to_next_area) \
    RUN(test_expl_nested_row_helpers)             \
    RUN(test_area_boss_gauntlet_composition)      \
    RUN(test_sub_boss_rounds_and_escorts)         \
    RUN(test_every_generic_move_is_carried)       \
    RUN(test_boss_threat_moves_area_adjacent)     \
    RUN(test_boss_teaches_its_own_apex_move)      \
    RUN(test_expl_nested_list_nav)                \
    RUN(test_deepweb_dive)                        \
    RUN(test_zone_titles_equip_via_cfg_and_persist) \
    RUN(test_zone_titles_picker_skips_locked)     \
    RUN(test_combat_force_enemy_first)            \
    RUN(test_encounter_sinkhole_bypass)           \
    RUN(test_sinkhole_xp_persists_immediately)    \
    /* The Wi-Fi network explore event */   \
    RUN(test_wifi_event_reached)                  \
    RUN(test_wifi_networks_seen_counter)          \
    RUN(test_wifi_sleeping_guardian_grants_loot)  \
    RUN(test_wifi_open_cache_grants_loot)         \
    RUN(test_wifi_awakened_guardian_enters_combat) \
    RUN(test_wifi_never_awakens_when_not_hot)     \
    RUN(test_wifi_friendly_visit_grants_ally_buff) \
    RUN(test_wifi_auto_plays_out_after_hold)      \
    RUN(test_wifi_discovery_kind)                 \
    RUN(test_camo_ramp_is_a_value_scale)          \
    RUN(test_camo_rich_palette_is_mostly_the_real_thing) \
    RUN(test_camo_level_holds_and_releases)       \
    RUN(test_camo_zero_is_the_plain_draw)         \
    RUN(test_camo_flash_composes_over_it)         \
    RUN(test_camo_holds_through_a_counter_strike) \
    RUN(test_borrowed_colours_outlive_the_rivals_turn) \
    RUN(test_camo_only_copies_a_move_the_rival_has) \
    RUN(test_absorb_sweep_endpoints)              \
    RUN(test_absorb_phase_clock)                  \
    RUN(test_combat_outro_kind)                   \
    RUN(test_rival_prize_mask_matches_the_drop_filter) \
    RUN(test_combat_kit_row_fits_the_panel)       \
    RUN(test_combat_kit_page_holds_the_widest_boss) \
    /*Backlog / — shop explore events + the two shop consumables */ \
    RUN(test_shop_event_buy_decrements)           \
    RUN(test_shop_buy_gated_when_broke)           \
    RUN(test_shop_sold_out_after_stock)           \
    RUN(test_shop_cursor_cycles_all_listings)     \
    RUN(test_mod_shop_buy_grants_mod)             \
    RUN(test_mod_shop_buy_gated_by_item_cost)     \
    RUN(test_shop_grayscale)                      \
    RUN(test_shop_auto_leaves_after_hold)         \
    RUN(test_sealed_cache_walk_find)              \
    RUN(test_sealed_cache_open_grants_reward)     \
    RUN(test_sealed_cache_item_shape)             \
    RUN(test_cache_epic_pool_and_multidrop)       \
    RUN(test_cache_common_pool_single_drop)       \
    RUN(test_item_earn_coverage)                  \
    RUN(test_cache_yield_reveal_and_dismiss)      \
    RUN(test_warp_item_shape)                     \
    RUN(test_warp_key_inert_from_items)           \
    RUN(test_access_token_warps_to_shop)          \
    RUN(test_safe_mode_key_warps_to_rest)         \
    RUN(test_explore_warp_no_keys_is_noop)        \
    RUN(test_warp_key_walk_find)                  \
    RUN(test_null_noodles_effects)                \
    RUN(test_null_noodles_happy_pull_from_below)  \
    RUN(test_r007_b33r_effects)                   \
    /* Hacker Rank — XP model (S60) + device-scan dedup (J.21b) */ \
    RUN(test_hacker_rank_xp_model)                \
    RUN(test_register_network_queue_dedup)         \
    RUN(test_peer_hello_round_trip)               \
    RUN(test_register_peer_counts_encounters_not_frames) \
    RUN(test_register_peer_ignores_foreign_traffic) \
    RUN(test_peer_rows_live_first_and_deduped)    \
    RUN(test_peers_screen_grayscale_live_vs_remembered) \
    RUN(test_peers_screen_raises_link_without_touching_config) \
    RUN(test_peers_screen_outlives_the_menu_idle_timer) \
    RUN(test_pvp_frame_round_trip)          \
    RUN(test_pvp_host_election_agrees_on_both_sides) \
    RUN(test_pvp_commit_hash_catches_divergence) \
    RUN(test_pvp_same_seed_plays_the_same_fight) \
    RUN(test_pvp_seating_order_is_load_bearing) \
    RUN(test_pvp_two_devices_duel_end_to_end) \
    RUN(test_pvp_duel_marks_the_opponent_species_seen) \
    RUN(test_pvp_invite_retries_do_not_decline_the_pending_challenge) \
    RUN(test_pvp_challenge_needs_a_human_on_the_link_screen) \
    RUN(test_pvp_unanswered_challenge_times_out) \
    RUN(test_pvp_link_screen_verdict_grayscale) \
    RUN(test_pvp_guest_sees_its_own_pet_on_the_bottom_gauge) \
    RUN(test_combat_seats_local_pet_on_the_left) \
    RUN(test_combat_stage_seats_never_overlap) \
    RUN(test_combat_strike_mark_travels_toward_its_target) \
    RUN(test_combat_windup_reads_apart_from_impact) \
    /* ROCK THE DOCK — the operator bracket in The Pirate Bayou */ \
    RUN(test_tourney_handles_fit_an_operator_tag) \
    RUN(test_tourney_entrants_are_derived_from_the_seed) \
    RUN(test_tourney_entrants_are_legal_petware) \
    RUN(test_a_fighter_can_fire_its_own_exploit) \
    RUN(test_tourney_headless_matches_terminate_and_repeat) \
    RUN(test_tourney_rounds_halve_the_field) \
    RUN(test_tourney_run_from_the_expl_row) \
    RUN(test_tourney_screen_grayscale) \
    RUN(test_tourney_screen_copy_fits_its_panel) \
    RUN(test_tourney_screen_outlives_the_menu_idle_timer) \
    RUN(test_tourney_brief_fits_its_page) \
    RUN(test_tourney_scout_shows_the_rivals_whole_kit) \
    RUN(test_tourney_run_survives_a_reboot) \
    RUN(test_combat_ransom_pool_grayscale)  \
    RUN(test_combat_panel_reports_every_live_state) \
    RUN(test_combat_panel_pages_cycle)      \
    RUN(test_save_v37_link_roundtrip)             \
    RUN(test_save_v38_net_slot_is_reserved)              \
    RUN(test_radio_consents_are_independent)      \
    RUN(test_save_defers_when_the_heap_is_too_low) \
    RUN(test_a_refused_write_is_not_a_save) \
    RUN(test_a_hatch_the_store_refused_still_reaches_flash) \
    RUN(test_net_access_needs_a_live_job)     \
    RUN(test_net_attempt_outcomes)                \
    RUN(test_qr_setup_step_appears_only_until_provisioned) \
    RUN(test_update_check_requires_network_and_source) \
    RUN(test_update_job_holds_the_radio_past_the_screen) \
    RUN(test_updates_screen_outlives_the_menu_idle_timer) \
    RUN(test_update_job_dies_when_the_join_fails)        \
    RUN(test_update_install_needs_a_confirmed_finding) \
    RUN(test_update_install_takes_two_yeses)      \
    RUN(test_flasher_url_follows_the_publish_host) \
    RUN(test_flasher_row_is_last_and_needs_no_network) \
    RUN(test_update_install_progress_is_not_terminal) \
    RUN(test_tar_rejects_escaping_names)          \
    RUN(test_tar_reads_entries)                   \
    RUN(test_manifest_parses_published_shape)     \
    RUN(test_manifest_rejects_junk_and_unusable_rows) \
    RUN(test_manifest_tolerates_a_newer_publish)  \
    RUN(test_version_marker_round_trips)          \
    RUN(test_version_marker_tolerates_real_files) \
    RUN(test_version_marker_absent_reads_as_unknown) \
    RUN(test_peer_hello_rejects_malformed)        \
    RUN(test_peer_hello_sanitizes_hostile_fields) \
    RUN(test_peer_ledger_new_and_refresh)         \
    RUN(test_network_ledger_new_and_repeat)       \
    RUN(test_network_ledger_in_top_n)             \
    RUN(test_network_discovery_repeat_familiar_vs_home_turf) \
    RUN(test_network_discovery_empty_queue_penalty_throttles) \
    RUN(test_hacker_rank_up_grants_reward)        \
    /* Audit handshake capture (pcap + policy SM, save v6) */ \
    RUN(test_pcap_global_header)                  \
    RUN(test_pcap_writer_stream)                  \
    RUN(test_pcap_writer_requires_begin)          \
    RUN(test_audit_capture_state_machine)         \
    RUN(test_audit_capture_seal_paths)            \
    RUN(test_audit_capture_toggle_persists)       \
    RUN(test_audit_mode_enforces_scan_dependency) \
    RUN(test_audit_legacy_capture_without_scan_normalizes_on_load) \
    RUN(test_eapol_parse_classifies_messages)     \
    RUN(test_eapol_parse_rejects_non_eapol)       \
    RUN(test_handshake_tracker_first_crackable)   \
    RUN(test_audit_capture_arm_window_self_seals) \
    RUN(test_audit_capture_seal_active_from_armed) \
    RUN(test_register_handshake_dedup)            \
    RUN(test_audit_ledgers_persist_no_recredit)   \
    RUN(test_save_v7_ledger_roundtrip)            \
    /*pcap-blowup fix — filename-by-network + discovery rewards */ \
    RUN(test_pcap_naming_sanitizes_and_falls_back_to_bssid) \
    RUN(test_handshake_already_captured_query)    \
    RUN(test_network_discovery_reward_fires_once_per_network) \
    RUN(test_network_discovery_reward_rarity_ratio) \
    RUN(test_handshake_capture_reward_fires_once_per_handshake) \
    RUN(test_handshake_capture_reward_rarity_ratio) \
    /* Move drop tables + malbeast roster depth */ \
    RUN(test_move_loadout_grant_no_duplicate)     \
    RUN(test_wild_win_can_drop_a_move)            \
    /* Mods into combat (data-driven effects, earn path, equip-level gate) */ \
    RUN(test_mod_effects_data_driven)             \
    RUN(test_backup_drive_death_save_ignores_survivable_hits) \
    RUN(test_backup_drive_death_save_restores_half_max_health) \
    RUN(test_backup_drive_death_save_covers_a_fatal_dot) \
    RUN(test_backup_drive_death_save_loses_to_overkill) \
    RUN(test_mod_thorns_and_deathblast)           \
    RUN(test_mod_ecc_memory_hitcap)               \
    RUN(test_mod_load_balancer_split)             \
    RUN(test_mod_watchdog_timer)                  \
    RUN(test_mod_faraday_cage)                    \
    RUN(test_mod_content_rarity_tier)             \
    RUN(test_mod_earn_tables_and_reqlevel)        \
    RUN(test_mod_equip_ladder_is_ordered_and_dense) \
    RUN(test_mod_niche_flavour_data_driven)       \
    RUN(test_mod_botnet_swarm_and_airgap_ward)    \
    RUN(test_mod_state_combine_rules)             \
    RUN(test_mod_prowlware_rank_computation)      \
    RUN(test_mod_prowlware_combat_effect)         \
    RUN(test_mod_canary_trap)                     \
    RUN(test_mod_meltdown_core)                   \
    RUN(test_mod_zero_day_exploit)                \
    RUN(test_mod_tripwire)                        \
    RUN(test_phishing_bubble_steal)               \
    RUN(test_phishing_perfect_bite)               \
    RUN(test_phishing_frenzy_survives_the_bubble) \
    RUN(test_phishing_frenzy_breaks_when_exposed) \
    RUN(test_phishing_frenzy_lean_ratchets_until_the_bubble_pops) \
    RUN(test_phishing_shield_pool)                \
    RUN(test_worm_shared_resources_speed)         \
    RUN(test_worm_replica_arithmetic)             \
    RUN(test_worm_replication_in_combat)          \
    RUN(test_speed_action_economy)                \
    RUN(test_min_damage_penetration)              \
    RUN(test_mod_hard_line_gate)                  \
    RUN(test_mod_equip_level_gate)                \
    RUN(test_mod_picker_windows_large_pool)       \
    RUN(test_save_v18_mod_reqlevel)               \
    RUN(test_mod_storage_cap_bounds_the_pool_and_the_shop_raises_it) \
    RUN(test_save_mod_count_nibbles_pack_two_mods_per_byte) \
    RUN(test_save_mod_count_for_a_retired_wire_is_dropped) \
    RUN(test_save_v21_shield_roundtrip)           \
    RUN(test_save_v22_frag_trigger_roundtrip) \
    RUN(test_save_v23_shop_unlocks_roundtrip) \
    /* ITEMS type-tabs + VAULT bulk-open (Hacker SHOP, save v23) */ \
    RUN(test_hacker_shop_item_tabs_buy)           \
    RUN(test_hacker_shop_bulk_open_buy)           \
    RUN(test_item_filter_narrows_rows)            \
    RUN(test_item_hold_b_cycles_filter_tap_opens) \
    RUN(test_hacker_shop_item_picker_buy)         \
    RUN(test_item_category_filters_split_quest)   \
    RUN(test_item_picker_tiles_count_units)       \
    RUN(test_item_picker_grayscale)               \
    RUN(test_item_picker_nav_drills_in_and_back)  \
    RUN(test_item_picker_skipped_in_lockout)      \
    RUN(test_item_hold_b_follows_picker_axis)     \
    /* The shared list-navigation contract (game_listnav.cpp) */ \
    RUN(test_list_hold_a_repeats_the_step)        \
    RUN(test_list_hold_c_steps_back_tap_cancels)  \
    RUN(test_list_contract_skips_non_lists)       \
    RUN(test_mod_picker_orders_fittable_first)    \
    RUN(test_mod_picker_cursor_matches_drawn_order) \
    RUN(test_list_rows_leave_room_for_the_name)   \
    RUN(test_vault_bulk_open_consumes_all_of_rarity) \
    RUN(test_vault_hold_b_bulk_opens_tap_opens_one) \
    RUN(test_ap_toggle_via_cfg_and_persist)       \
    RUN(test_pedia_qr_two_pages_no_timeout)       \
    RUN(test_wild_malbeast_roster_variants)       \
    RUN(test_wild_and_roster_names_disjoint)      \
    /* Branching creature system scaffold */ \
    RUN(test_evolution_routing_tables)            \
    RUN(test_hatch_pool_ransomware)               \
    RUN(test_canine_line_evolution_branch)        \
    RUN(test_pingcub_rejoins_the_bear_line)       \
    RUN(test_cat_line_evolution_branch)           \
    RUN(test_creature_clips_fit_their_sheets)     \
    RUN(test_cat_line_clip_wiring)                \
    RUN(test_combatant_carries_its_creature)      \
    RUN(test_wander_travelling_tracks_the_trip)   \
    RUN(test_fight_pose_precedence)               \
    RUN(test_frog_line_linear)                    \
    RUN(test_anglerfish_deepdive_hatch_gate)      \
    RUN(test_anglerfish_line_evolution_branch)    \
    RUN(test_trojan_content)                      \
    RUN(test_trojan_cross_line_divert)            \
    RUN(test_worm_script_divert_branch)           \
    RUN(test_trojan_combat)                       \
    RUN(test_mirror_gates_metamorphic_line)       \
    RUN(test_line_select_phishing_egg)            \
    RUN(test_line_select_grayscale)               \
    RUN(test_eggpick_win_halves_incubation)       \
    RUN(test_eggpick_miss_keeps_full_incubation)  \
    RUN(test_eggpick_aim_is_not_cancel)           \
    RUN(test_eggpick_target_never_wraps)          \
    RUN(test_eggpick_line_has_nothing_mid_clock)    \
    RUN(test_hatch_reveal_plays_the_animation)    \
    RUN(test_hatch_reveal_covers_the_decryption_line) \
    RUN(test_eggpick_grayscale)                   \
    RUN(test_frog_line_move_access)               \
    RUN(test_dominant_signal_from_care)           \
    RUN(test_battery_percent_from_mv)           \
    RUN(test_firmware_version_ordering)         \
    /*Web 'Pedia slice — HackerTag rename + the state JSON builder */ \
    RUN(test_set_hacker_tag_validates)            \
    RUN(test_pedia_state_json_shape)              \
    RUN(test_pedia_state_publishes_the_pets_learnable_kit) \
    RUN(test_pedia_state_publishes_the_pets_one_shot_items) \
    RUN(test_pedia_state_json_fresh_hatch_egg)    \
    RUN(test_pedia_state_kitchen_axes)            \
    RUN(test_wild_malbeast_index_mapping)              \
    RUN(test_save_v25_roundtrip)                       \
    RUN(test_save_v27_roundtrip)                        \
    RUN(test_save_v32_roundtrip)                        \
    RUN(test_crew_exploit_negates_next_hits)            \
    RUN(test_crew_roster_exploits_are_well_formed)      \
    RUN(test_crew_escalation_banks_damage_as_power)     \
    RUN(test_crew_net_neutrality_resets_then_floors_the_leans) \
    RUN(test_crew_mitm_copies_enemy_buffs)              \
    RUN(test_crew_backup_plan_b_saves_and_rallies)      \
    RUN(test_crew_backup_plan_b_clock_runs_out)         \
    RUN(test_crew_requires_home_network)                \
    RUN(test_crew_screen_pick_home_then_enlist)         \
    RUN(test_crew_sides_filter_the_roster)              \
    RUN(test_crew_detail_gates_enlist_on_the_home_net)  \
    RUN(test_crew_exploit_descriptions_resolve_and_fit) \
    RUN(test_crew_picker_offers_networks_in_range)   \
    RUN(test_crew_picker_unaffected_by_saturated_reward_queue)   \
    RUN(test_crew_picker_drops_networks_out_of_range)   \
    RUN(test_crew_screen_raises_scan_without_touching_config) \
    RUN(test_crew_screen_outlives_the_menu_idle_timer)   \
    RUN(test_crew_exploit_in_combat_picker)             \
    RUN(test_save_v36_crew_roundtrip)                   \
    RUN(test_pedia_state_json_reveal_states)           \
    RUN(test_pedia_raised_tally_survives_evolution)    \
    RUN(test_save_v39_raised_tally_roundtrip) \
    RUN(test_renamed_ids_table_invariants)  \
    RUN(test_ladder_inserts_table_invariants) \
    RUN(test_area_icons_are_keyed_by_area_id) \
    RUN(test_stacker_shaves_the_overhang)     \
    RUN(test_stacker_missing_entirely_loses)  \
    RUN(test_stacker_clearing_every_row_wins) \
    RUN(test_stacker_run_stays_on_board_and_contiguous) \
    RUN(test_stacker_row_width_reports_the_survivors) \
    RUN(test_stacker_score_pays_for_height_twice) \
    RUN(test_stacker_win_credits_the_tally_and_the_shape_rows) \
    RUN(test_stacker_short_board_pays_what_it_stacked_and_costs_nothing) \
    RUN(test_stacker_cleared_board_wipes_the_disk) \
    RUN(test_stacker_stopping_early_banks_the_board) \
    RUN(test_stacker_wins_ladder_sweeps_and_persists) \
    /* GAMES — the arcade: the till, the dial, and the stakes it must not touch */ \
    RUN(test_arcade_pays_the_attempt)             \
    RUN(test_arcade_perfect_board_pays_the_bonus) \
    RUN(test_arcade_stacker_leaves_the_disk_alone) \
    RUN(test_arcade_difficulty_paces_the_run)     \
    RUN(test_arcade_clutch_rounds_follow_the_dial) \
    RUN(test_arcade_isolation_is_off_the_clock)   \
    RUN(test_arcade_ladders_split_plays_wins_losses) \
    RUN(test_stack_overflow_fires_on_the_second_row) \
    RUN(test_arcade_tallies_persist)              \
    /* THE DECRYPTOGRAM — the quote board, its pool's fit, and both doors in */ \
    RUN(test_cryptogram_quotes_fit_the_panel)     \
    RUN(test_cryptogram_opens_at_least_three_letters) \
    RUN(test_cryptogram_difficulty_opens_more)    \
    RUN(test_cryptogram_right_letter_opens_every_instance) \
    RUN(test_cryptogram_one_wrong_letter_ends_the_run) \
    RUN(test_cryptogram_cursors_run_both_ways) \
    RUN(test_cryptogram_chord_drops_the_letter_but_never_leaves) \
    RUN(test_cryptogram_held_cursor_repeats)     \
    RUN(test_quote_prize_ladder_covers_every_recipe) \
    RUN(test_quote_prize_pays_the_next_recipe) \
    RUN(test_quote_prize_falls_back_without_a_kitchen) \
    RUN(test_cryptogram_vault_ticket_pays_bits_and_an_upgrade) \
    RUN(test_cryptogram_losing_ratchets_the_quote_easier) \
    RUN(test_cryptogram_gated_quotes_wait_for_their_achievement) \
    RUN(test_cryptogram_states_persist)           \
    RUN(test_cryptogram_arcade_cabinet_waits_for_eight_wins) \
    RUN(test_cryptogram_arcade_replays_a_solved_quote_for_bits) \
    RUN(test_cryptogram_grayscale)                \
    /* Isolation Protocol — the Worm egg's hatch minigame + its line gate */ \
    RUN(test_isolation_opens_on_the_middle_row)   \
    RUN(test_isolation_step_moves_without_growing) \
    RUN(test_isolation_double_turn_is_one_quarter) \
    RUN(test_isolation_wall_ends_the_run)         \
    RUN(test_isolation_self_collision_ends_the_run) \
    RUN(test_isolation_cycle_run_finishes_clean)  \
    RUN(test_worm_line_gated_on_second_instance)  \
    RUN(test_isolation_opens_on_a_laid_worm_egg)  \
    RUN(test_isolation_steps_on_its_own_cadence)  \
    RUN(test_isolation_crash_banks_a_minute_per_byte) \
    RUN(test_isolation_clean_run_hatches_and_unlocks) \
    /* CHROMATOPHORE: the Metamorphic egg's hatch board — the rules, the screen, the \
       colour source it is built on, and the egg-locomotion rule its jellyfish egg is \
       the reason for (test_chroma.cpp). */ \
    RUN(test_chroma_opens_on_a_change)            \
    RUN(test_chroma_settled_match_survives_the_sweep) \
    RUN(test_chroma_caught_midchange_is_spotted)  \
    RUN(test_chroma_wrong_skin_is_spotted)        \
    RUN(test_chroma_pressing_the_worn_skin_is_inert) \
    RUN(test_chroma_window_shrinks_to_a_floor)    \
    RUN(test_chroma_water_holds_still_unless_switching) \
    RUN(test_chroma_switching_moves_the_water_mid_window) \
    RUN(test_chroma_clean_run_takes_every_round)  \
    RUN(test_chroma_opens_on_a_laid_metamorphic_egg) \
    RUN(test_chroma_runs_on_real_time_with_the_clock_frozen) \
    RUN(test_chroma_passes_pay_the_incubation_clock) \
    RUN(test_chroma_clean_run_buys_the_whole_clock) \
    RUN(test_chroma_clean_hatch_pops_the_egg_and_unlocks) \
    RUN(test_chroma_three_buttons_are_three_skins) \
    RUN(test_chroma_subject_falls_back_to_the_egg) \
    RUN(test_chroma_grayscale)                    \
    RUN(test_chroma_egg_drifts_because_its_row_says_so) \
    RUN(test_camo_ramp_from_a_named_tone_is_a_value_scale) \
    RUN(test_chroma_endless_run_has_no_finish)    \
    RUN(test_isolation_endless_run_has_no_finish) \
    RUN(test_arcade_endless_chroma_records_a_high_score) \
    RUN(test_arcade_high_score_survives_a_reboot) \
    RUN(test_arcade_high_score_tail_is_rollback_safe) \
    RUN(test_new_egg_line_banner_holds_until_a_press) \
    RUN(test_ordinary_banner_still_retires_on_its_own) \
    RUN(test_rolled_defrag_takes_its_fixed_bite) \
    RUN(test_replication_ghost_is_raised_by_a_failed_defrag_on_a_critical_disk) \
    RUN(test_replication_ghost_never_raised_off_the_worm_line) \
    RUN(test_unlinkguine_cures_the_ghost_and_unlocks) \
    RUN(test_ghost_also_clears_through_a_successful_av_scan) \
    RUN(test_perishable_food_spoils_on_a_feeding) \
    RUN(test_icon_tint_and_theme_indirection) \
    RUN(test_full_pedia_achievement_reads_the_raised_tally) \
    /* Achievements: the catalogue's own invariants, the data-driven ladder sweep, \
       the kGoalAll sentinel, the home-screen banner queue, and v40 save migration */ \
    RUN(test_mod_table_wires_are_unique_and_in_range) \
    RUN(test_achievement_table_is_well_formed) \
    RUN(test_achievement_ladder_unlocks_and_pays) \
    RUN(test_recipes_known_counts_methods_not_dishes) \
    RUN(test_backup_drive_achievement_mapping) \
    RUN(test_achievement_goal_all_tracks_the_set_size) \
    RUN(test_achievement_banner_announces_on_the_home_screen) \
    RUN(test_achievement_banner_waits_for_the_home_screen) \
    RUN(test_achievement_banner_collapses_a_burst) \
    RUN(test_save_v40_achievements_roundtrip) \
    RUN(test_species_dive_records_feed_the_depth_rows) \
    RUN(test_collected_items_survive_being_spent) \
    RUN(test_pedia_state_json_rack_and_record_hatched) \
    RUN(test_pedia_first_brute_force_achievement)      \
    RUN(test_pedia_evolution_achievements_leave_the_sibling_locked) \
    RUN(test_every_combatant_sprite_resolves)

// Every gate's declaration, derived from the roster itself — so a listed gate can
// never be missing one, and a roster entry that names nothing fails to LINK rather
// than quietly running one gate fewer.
#define MAL_DECLARE_GATE(fn) void fn();
MAL_RUN_ALL_GATES(MAL_DECLARE_GATE)
#undef MAL_DECLARE_GATE

#ifdef PIO_UNIT_TESTING
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
#define MAL_RUN_GATE(fn) RUN_TEST(fn);
    MAL_RUN_ALL_GATES(MAL_RUN_GATE)
#undef MAL_RUN_GATE
    return UNITY_END();
}
#else
int main() {
    std::puts("running native gates...");
#define MAL_RUN_GATE(fn) fn();
    MAL_RUN_ALL_GATES(MAL_RUN_GATE)
#undef MAL_RUN_GATE
    if (g_failures == 0) {
        std::puts("OK — all gates passed");
        return 0;
    }
    std::printf("FAILED — %d check(s)\n", g_failures);
    return 1;
}
#endif
