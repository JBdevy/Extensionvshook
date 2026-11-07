#pragma once
#include <string>

// Project Sync v3 synchronizes controls over independently opened projects.
// Media, item/track chunks, playlists, manifests and bulk settings never travel.
inline bool nativeProjectSyncIsControlCommand(const std::string& type)
{
  static const char* const controls[] = {
    "app_list_scroll",
    "at_bl_set",
    "at_bl_toggle",
    "atbl_toggle",
    "auto1_set",
    "auto1_toggle",
    "auto2_set",
    "auto2_toggle",
    "auto_bloco_set",
    "auto_bloco_toggle",
    "auto_set",
    "auto_stop",
    "auto_stop_set",
    "auto_stop_toggle",
    "auto_toggle",
    "autoplay1_set",
    "autoplay1_toggle",
    "autoplay2_set",
    "autoplay2_toggle",
    "autoplay_set",
    "autoplay_toggle",
    "autostop",
    "autostop_set",
    "autostop_toggle",
    "bpm_adjust",
    "bpm_focus",
    "bpm_set",
    "clear_queue",
    "clear_queued_song",
    "clear_selected_song",
    "clear_selection",
    "director_at_bl_toggle",
    "director_auto2_toggle",
    "director_auto_stop_toggle",
    "director_auto_toggle",
    "director_drawers_sync",
    "director_family_drawers_sync",
    "director_family_view_set",
    "director_family_view_toggle",
    "director_list_scroll",
    "director_live_reset_item",
    "director_live_set",
    "director_live_toggle",
    "director_loop_toggle",
    "director_pause",
    "director_play_button",
    "director_play_no_seek",
    "director_stop_break",
    "director_stop_no_seek",
    "edit_cursor_move",
    "edit_mode_stop_pause_set",
    "esc",
    "escape_key",
    "faderout_set",
    "key_escape",
    "live_reset_item",
    "live_set",
    "live_toggle",
    "loop_set",
    "loop_toggle",
    "lyrics_settings",
    "manual_stop_fadeout_set",
    "manual_stop_fadeout_set_all_tracks",
    "manual_stop_fadeout_set_duration",
    "manual_stop_fadeout_set_enabled",
    "manual_stop_fadeout_toggle_track",
    "marker_cancel",
    "marker_go",
    "marker_select",
    "mixer_bulk_unmute",
    "mixer_bulk_unsolo",
    "mixer_focus",
    "mixer_reset_volume",
    "mixer_route",
    "mixer_select_track",
    "mixer_set_route",
    "mixer_set_volume",
    "mixer_toggle_mute",
    "mixer_toggle_solo",
    "mixer_viewport_set",
    "multi_loop_bypass_set",
    "multi_loop_bypass_toggle",
    "multi_loop_close",
    "multi_loop_focus",
    "multi_loop_ms_toggle",
    "multi_loop_open",
    "multi_loop_slot_toggle",
    "multi_loop_track_limit_set",
    "multi_loop_track_set",
    "multi_loop_track_toggle",
    "multi_project_playlists_set",
    "multiloop_bypass_set",
    "multiloop_bypass_toggle",
    "multiloop_close",
    "multiloop_fade_adjust",
    "multiloop_fade_set",
    "multiloop_focus",
    "multiloop_loop_toggle",
    "multiloop_ms_set",
    "multiloop_ms_toggle",
    "multiloop_open",
    "multiloop_slot_set",
    "multiloop_slot_toggle",
    "multiloop_track_limit_set",
    "multiloop_track_set",
    "multiloop_track_toggle",
    "parts_visibility_set",
    "play",
    "play_button",
    "play_protection_set",
    "play_protection_toggle",
    "play_start",
    "play_stop",
    "play_stop_no_seek",
    "play_toggle",
    "playlist_multi_set",
    "playlist_multi_toggle",
    "playlist_select",
    "premix_close",
    "premix_focus_song",
    "premix_item_open",
    "premix_item_set_track_solo",
    "premix_item_set_unique_solo",
    "premix_item_set_volume",
    "premix_item_toggle_mute",
    "premix_open",
    "premix_open_song",
    "premix_refresh",
    "premix_screen_close",
    "premix_set_volume",
    "premix_toggle_mute",
    "preview_clear",
    "preview_set",
    "preview_toggle",
    "project_sync_track_state",
    "project_sync_viewport_v1",
    "queue_playlist_song",
    "queue_region_song",
    "recados_settings",
    "save_technical_notice_settings",
    "save_teleprompt_settings",
    "select_marker",
    "select_playlist",
    "select_playlist_song",
    "select_region",
    "set_active_playlist",
    "set_auto_stop",
    "set_autostop",
    "set_bpm_visibility",
    "set_page",
    "set_playlist",
    "set_tuner_visibility",
    "smart_search_activate",
    "smart_search_close",
    "smart_search_open",
    "smart_search_query",
    "stop",
    "stop_break",
    "stop_pause_mode_set",
    "stop_pause_mode_toggle",
    "technical_notice_settings",
    "teleprompt_settings",
    "teleprompt_window",
    "timecode_transport_sync",
    "timer_config",
    "timer_init_auto_toggle",
    "timer_reset",
    "timer_set_init_auto",
    "timer_set_mode",
    "timer_set_target",
    "timer_start",
    "timer_stop",
    "timer_stop_reset",
    "timer_toggle",
    "toggle_auto_stop",
    "toggle_autostop",
    "transport_stop_no_seek",
    "trigger_marker",
    "tuner_adjust",
    "tuner_focus",
    "tuner_reset",
    "tuner_set",
    "ui_music_selection_snapshot",
  };
  for (const char* control : controls) {
    if (type == control) return true;
  }
  return false;
}

inline bool nativeProjectSyncIsManagementCommand(const std::string& type)
{
  return type == "project_sync_role_handover" ||
    type == "project_sync_role_handover_state";
}

inline bool nativeProjectSyncIsRemovedCommand(const std::string& type)
{
  return type.rfind("project_sync_", 0) == 0 &&
    !nativeProjectSyncIsControlCommand(type) &&
    !nativeProjectSyncIsManagementCommand(type);
}

// Project Sync carries user actions, not a running timeline. Traditional LAN
// Timecode keeps its original pulse/cursor/viewport policy.
inline bool nativeProjectSyncIsIndependentControlCommand(const std::string& type)
{
  return nativeProjectSyncIsControlCommand(type) &&
    type != "timecode_transport_sync" &&
    type != "project_sync_viewport_v1" && type != "mixer_viewport_set";
}

// Native automation runs on the main thread. Nested transitions may reuse the
// same command handlers, but must never republish their effects as fresh input.
inline unsigned& nativeProjectSyncAutomaticDepth()
{
  static unsigned depth = 0;
  return depth;
}
class NativeProjectSyncAutomaticScope {
public:
  NativeProjectSyncAutomaticScope() { ++nativeProjectSyncAutomaticDepth(); }
  ~NativeProjectSyncAutomaticScope() { --nativeProjectSyncAutomaticDepth(); }
  NativeProjectSyncAutomaticScope(const NativeProjectSyncAutomaticScope&) = delete;
  NativeProjectSyncAutomaticScope& operator=(const NativeProjectSyncAutomaticScope&) = delete;
};
