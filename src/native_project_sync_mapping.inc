// Included after the native command helpers. All REAPER reads happen on the UI
// command thread; this module only translates references to existing objects.
static std::string nativeControlFirst(const std::string& command,
                                     std::initializer_list<const char*> keys)
{
  for (const char* key : keys) {
    const std::string value = nativeJsonExtractString(command, key);
    if (!value.empty()) return value;
  }
  return {};
}
static void nativeControlSet(std::string& command, const std::string& key,
                             const std::string& value)
{
  NativeControlMapping::setField(command, key, nativeJsonString(value));
}
static void nativeControlWriteReference(std::string& command, const std::string& prefix,
                                       const NativeControlMapping::Reference& ref)
{
  nativeControlSet(command, prefix + "Id", ref.id);
  nativeControlSet(command, prefix + "Name", ref.name);
  nativeControlSet(command, prefix + "Kind", ref.kind);
  NativeControlMapping::setField(command, prefix + "Index", std::to_string(ref.index));
  NativeControlMapping::setField(command, prefix + "Range", ref.range ? "true" : "false");
  if (ref.range) {
    NativeControlMapping::setField(command, prefix + "Start", nativeNumber(ref.start, 9));
    NativeControlMapping::setField(command, prefix + "End", nativeNumber(ref.end, 9));
  }
}
static NativeControlMapping::Reference nativeControlReadReference(
  const std::string& command, const std::string& prefix)
{
  NativeControlMapping::Reference ref;
  ref.id = nativeJsonExtractString(command, prefix + "Id");
  ref.name = nativeJsonExtractString(command, prefix + "Name");
  ref.kind = nativeJsonExtractString(command, prefix + "Kind");
  ref.index = static_cast<int>(nativeJsonInt64(command, (prefix + "Index").c_str(), -1));
  ref.range = nativeJsonBoolValue(command, (prefix + "Range").c_str(), false);
  ref.start = std::atof(nativeJsonExtractString(command, prefix + "Start").c_str());
  ref.end = std::atof(nativeJsonExtractString(command, prefix + "End").c_str());
  return ref;
}
static NativeControlMapping::Reference nativeControlTrackReference(ReaProject* project,
                                                                  MediaTrack* track)
{
  NativeControlMapping::Reference ref;
  ref.kind = "track";
  if (!project || !track) return ref;
  if (GetMasterTrack_ptr && track == GetMasterTrack_ptr(project)) {
    ref.id = "MASTER_TRACK";
    ref.name = "Master";
    return ref;
  }
  if (!CountTracks_ptr || !GetTrack_ptr) return ref;
  for (int i = 0, count = CountTracks_ptr(project); i < count; ++i) {
    if (GetTrack_ptr(project, i) != track) continue;
    ref.id = nativeTrackGuid(track, i);
    ref.name = nativeTrackName(track, i);
    ref.index = i;
    break;
  }
  return ref;
}
static NativeControlMapping::Reference nativeControlSongReference(const NativeSongWindow& song,
                                                                  int index)
{
  NativeControlMapping::Reference ref;
  ref.id = song.id;
  ref.name = song.name;
  ref.kind = song.isBlock ? "block" :
    ((song.isHashChild || song.isRegionChild || song.type == "marker") ? "marker" : "region");
  ref.index = index;
  ref.range = true;
  ref.start = song.start;
  ref.end = song.end;
  return ref;
}
static std::vector<NativeControlMapping::Reference> nativeControlSongReferences(
  const std::vector<NativeSongWindow>& songs)
{
  std::vector<NativeControlMapping::Reference> refs;
  refs.reserve(songs.size());
  for (size_t i = 0; i < songs.size(); ++i)
    refs.push_back(nativeControlSongReference(songs[i], static_cast<int>(i)));
  return refs;
}
static bool nativeControlIsItemCommand(const std::string& type)
{
  return type == "premix_set_volume" || type == "premix_item_set_volume" ||
    type == "premix_toggle_mute" || type == "premix_item_toggle_mute" ||
    type == "premix_item_set_track_solo" || type == "premix_item_set_unique_solo";
}
static bool nativeControlIsSongCommand(const std::string& type)
{
  return type == "smart_search_activate" || type == "select_region" || type == "select_playlist_song" ||
    type == "queue_region_song" || type == "queue_playlist_song" ||
    type == "play" || type == "play_start" || type == "play_button" ||
    type == "play_toggle" || type == "play_stop" ||
    type == "premix_open" || type == "premix_open_song" ||
    type == "premix_item_open" || type == "premix_focus_song" ||
    type == "multiloop_focus" || type == "multiloop_open" ||
    type == "multi_loop_focus" || type == "multi_loop_open" ||
    type == "live_reset_item" || type == "director_live_reset_item";
}
static std::vector<NativeControlMapping::Reference> nativeControlItemReferences(
  MediaTrack* track, int trackIndex)
{
  std::vector<NativeControlMapping::Reference> refs;
  if (!track || !GetTrackNumMediaItems_ptr || !GetTrackMediaItem_ptr || !GetMediaItemInfo_Value_ptr) return refs;
  for (int i = 0, count = GetTrackNumMediaItems_ptr(track); i < count; ++i) {
    MediaItem* item = GetTrackMediaItem_ptr(track, i);
    if (!item) continue;
    NativeControlMapping::Reference ref;
    ref.id = nativeMediaItemGuid(item, "premix_item_" + std::to_string(trackIndex + 1) + "_" + std::to_string(i + 1));
    ref.name = nativeTakeName(item, "");
    ref.kind = "item";
    ref.index = i;
    ref.range = true;
    ref.start = GetMediaItemInfo_Value_ptr(item, "D_POSITION");
    ref.end = ref.start + GetMediaItemInfo_Value_ptr(item, "D_LENGTH");
    refs.push_back(std::move(ref));
  }
  return refs;
}
static NativeControlMapping::Reference nativeControlRowReference(
  const NativeAppActivePanelModel::Row& row, int index, bool regions)
{
  NativeControlMapping::Reference ref;
  ref.id = nativeUiMainRowKey(row, regions);
  ref.name = row.name;
  ref.kind = row.block ? "block" : (row.child ? "marker" : "song");
  ref.index = index;
  ref.range = true;
  ref.start = row.start;
  ref.end = row.end;
  return ref;
}
static bool nativeControlIsPlaylistCommand(const std::string& type)
{
  return type == "select_playlist" || type == "playlist_select" ||
    type == "set_playlist" || type == "set_active_playlist";
}
static ReaProject* nativeControlPlaylistProject(const std::string& command)
{
  ReaProject* current = getCurrentProject(nullptr, 0);
  if (!EnumProjects_ptr) return current;
  const std::string name = nativeJsonExtractString(command, "projectName");
  if (!name.empty()) {
    ReaProject* match = nullptr;
    for (int i = 0; i < 64; ++i) {
      char path[2048] = "";
      ReaProject* candidate = EnumProjects_ptr(i, path, static_cast<int>(sizeof(path)));
      if (!candidate) break;
      if (NativeControlMapping::normalized(nativeBasenameNoRpp(path)) != NativeControlMapping::normalized(name)) continue;
      if (match) { match = nullptr; break; }
      match = candidate;
    }
    if (match) return match;
  }
  const std::string index = nativeControlFirst(command, {"projectIndex", "projectTabIndex"});
  if (nativeLooksNumeric(index)) {
    const int ordinal = std::atoi(index.c_str());
    if (ordinal >= 0 && ordinal < 64) {
      if (ReaProject* candidate = EnumProjects_ptr(ordinal, nullptr, 0)) return candidate;
    }
  }
  return current;
}
static std::vector<NativeControlMapping::Reference> nativeControlPlaylistReferences(ReaProject* project)
{
  std::vector<NativeControlMapping::Reference> refs;
  const auto playlists = nativeUiReadPlaylistRecords(project);
  for (size_t i = 0; i < playlists.size(); ++i) {
    NativeControlMapping::Reference ref;
    ref.id = std::to_string(i + 1);
    ref.name = playlists[i].name;
    ref.kind = "playlist";
    ref.index = static_cast<int>(i);
    refs.push_back(std::move(ref));
  }
  return refs;
}
static bool nativeControlIsMarkerCommand(const std::string& type)
{
  return type == "marker_select" || type == "select_marker" ||
    type == "marker_go" || type == "trigger_marker";
}
static std::vector<NativeControlMapping::Reference> nativeControlMarkerReferences(ReaProject* project)
{
  std::vector<NativeControlMapping::Reference> refs;
  if (!project || !CountProjectMarkers_ptr || !EnumProjectMarkers3_ptr) return refs;
  int markers = 0, regions = 0;
  int count = CountProjectMarkers_ptr(project, &markers, &regions);
  count = std::max(count, markers + regions);
  for (int i = 0; i < count; ++i) {
    bool region = false;
    double start = 0.0, end = 0.0;
    const char* name = nullptr;
    int number = 0, color = 0;
    if (!EnumProjectMarkers3_ptr(project, i, &region, &start, &end, &name, &number, &color) || region) continue;
    NativeControlMapping::Reference ref;
    ref.id = "m" + std::to_string(number);
    ref.name = name ? name : "";
    ref.kind = "marker";
    ref.index = static_cast<int>(refs.size());
    ref.start = ref.end = start;
    ref.range = true;
    refs.push_back(std::move(ref));
  }
  return refs;
}
static std::string nativeProjectSyncDescribeControl(const std::string& raw)
{
  std::string command = raw;
  const std::string type = nativeLower(nativeTrim(nativeJsonExtractString(command, "type")));
  ReaProject* project = getCurrentProject(nullptr, 0);
  if (!project) return command;
  const bool multiLoop = type.rfind("multiloop_", 0) == 0 || type.rfind("multi_loop_", 0) == 0;
  std::string id = nativeControlFirst(command, {"songId", "targetId", "selectedPlaylistSongId", "selectedRegionId", "resultId", "id"});
  if (multiLoop) {
    const std::string key = nativeControlFirst(command, {"songKey", "premixKey"});
    if (!key.empty()) for (const auto& song : g_nativeSongWindows) {
      if (!song.isBlock && nativeMultiLoopSongKey(song) == key) { id = song.id; break; }
    }
  }
  if (nativeControlIsPlaylistCommand(type)) {
    const auto refs = nativeControlPlaylistReferences(nativeControlPlaylistProject(command));
    NativeControlMapping::Reference wanted;
    wanted.name = nativeControlFirst(command, {"playlistName", "activePlaylistName", "currentPlaylistName", "name"});
    wanted.id = nativeControlFirst(command, {"playlistId", "activePlaylistId", "targetId", "id"});
    if (nativeLooksNumeric(wanted.id)) wanted.index = std::atoi(wanted.id.c_str()) - 1;
    const int match = NativeControlMapping::resolve(wanted, refs, false);
    if (match >= 0) nativeControlWriteReference(command, "_ctlPlaylist", refs[static_cast<size_t>(match)]);
  }
  if (nativeControlIsMarkerCommand(type)) {
    const auto refs = nativeControlMarkerReferences(project);
    const std::string marker = nativeControlFirst(command, {"markerId", "selectedMarkerId", "targetId", "id"});
    int number = 0;
    if (nativeParseMarkerNumberFromId(marker, number)) {
      for (const auto& ref : refs) if (ref.id == "m" + std::to_string(number)) {
        nativeControlWriteReference(command, "_ctlMarker", ref);
        break;
      }
    }
  }
  if (type == "ui_music_selection_snapshot") {
    const bool regions = nativeJsonExtractString(command, "page") == "regions";
    if (g_nativeAppActivePanelModel.regionsPage != regions) return command;
    const auto keys = nativeSplit(nativeJsonExtractString(command, "keys"), '\n');
    int count = 0;
    for (const auto& key : keys) {
      for (size_t i = 0; i < g_nativeAppActivePanelModel.rows.size(); ++i) {
        const auto& row = g_nativeAppActivePanelModel.rows[i];
        if (nativeUiMainRowKey(row, regions) != key) continue;
        nativeControlWriteReference(command, "_ctlRow" + std::to_string(count++), nativeControlRowReference(row, static_cast<int>(i), regions));
        break;
      }
      if (count >= 256) break;
    }
    NativeControlMapping::setField(command, "_ctlRows", std::to_string(count));
    return command;
  }
  if ((nativeControlIsSongCommand(type) || multiLoop) && !id.empty()) {
    const bool playlist = !multiLoop && (type == "select_playlist_song" || type == "queue_playlist_song" ||
      (type == "smart_search_activate" ? !g_nativeAppActivePanelModel.regionsPage : type != "select_region" && type != "queue_region_song" && g_nativeSelectedTab == "playlist"));
    const auto& songs = playlist && !g_nativeActivePlaylistItems.empty() ? g_nativeActivePlaylistItems : g_nativeSongWindows;
    const double start = std::atof(nativeControlFirst(command, {"startPos", "start_pos", "selectedStartPos", "resultStart"}).c_str());
    const double end = std::atof(nativeControlFirst(command, {"endPos", "end_pos", "selectedEndPos"}).c_str());
    const NativeSongWindow* chosen = nullptr;
    int chosenIndex = -1;
    double best = std::numeric_limits<double>::max();
    for (size_t i = 0; i < songs.size(); ++i) {
      if (songs[i].isBlock || !nativeSongIdMatches(songs[i], id)) continue;
      const double score = std::fabs(start - songs[i].start) + std::fabs(end - songs[i].end);
      if (!chosen || score < best) { chosen = &songs[i]; chosenIndex = static_cast<int>(i); best = score; }
    }
    if (chosen) {
      nativeControlWriteReference(command, "_ctlSong", nativeControlSongReference(*chosen, chosenIndex));
      nativeControlSet(command, "_ctlSongScope", playlist ? "playlist" : "regions");
    }
  }
  if (nativeControlIsItemCommand(type)) {
    const std::string itemId = nativeControlFirst(command, {"itemId", "mediaItemId", "targetId", "trackId", "guid"});
    const auto item = nativeResolvePremixMediaItemById(project, itemId);
    if (item.item && item.track) {
      const auto track = nativeControlTrackReference(project, item.track);
      nativeControlWriteReference(command, "_ctlTrack", track);
      for (const auto& ref : nativeControlItemReferences(item.track, track.index)) {
        if (ref.id == item.itemId) { nativeControlWriteReference(command, "_ctlItem", ref); break; }
      }
      return command;
    }
  }
  const bool trackTarget = type.rfind("mixer_", 0) == 0 || nativeControlIsItemCommand(type) ||
    type == "manual_stop_fadeout_toggle_track" || type.find("loop_track_") != std::string::npos;
  if (trackTarget) {
    const std::string trackId = nativeControlFirst(command, {"trackId", "targetId", "guid", "id"});
    MediaTrack* track = nativeFindTrackById(project, trackId);
    if (track) nativeControlWriteReference(command, "_ctlTrack", nativeControlTrackReference(project, track));
  }
  return command;
}

static std::string nativeProjectSyncMapControl(const std::string& raw)
{
  std::string command = raw;
  const std::string type = nativeLower(nativeTrim(nativeJsonExtractString(command, "type")));
  ReaProject* project = getCurrentProject(nullptr, 0);
  if (!project) return command;
  const auto replaceIds = [&](std::initializer_list<const char*> keys, const std::string& id) {
    for (const char* key : keys) if (!nativeJsonExtractString(command, key).empty()) nativeControlSet(command, key, id);
  };
  const auto playlistRef = nativeControlReadReference(command, "_ctlPlaylist");
  if (nativeControlIsPlaylistCommand(type) && !playlistRef.id.empty()) {
    ReaProject* target = nativeControlPlaylistProject(command);
    const auto refs = nativeControlPlaylistReferences(target);
    const int match = NativeControlMapping::resolve(playlistRef, refs, false);
    if (match < 0) return {};
    const auto& ref = refs[static_cast<size_t>(match)];
    replaceIds({"playlistId", "activePlaylistId", "targetId", "id"}, ref.id);
    nativeControlSet(command, "playlistName", ref.name);
    // Resolve a local existing tab without ever opening a project file.
    if (EnumProjects_ptr) for (int i = 0; i < 64; ++i) {
      ReaProject* candidate = EnumProjects_ptr(i, nullptr, 0);
      if (!candidate) break;
      if (candidate == target) {
        NativeControlMapping::setField(command, "projectIndex", std::to_string(i));
        nativeControlSet(command, "projectId", "");
        nativeControlSet(command, "projectPath", "");
        break;
      }
    }
  }
  const auto markerRef = nativeControlReadReference(command, "_ctlMarker");
  if (nativeControlIsMarkerCommand(type) && !markerRef.id.empty()) {
    const auto refs = nativeControlMarkerReferences(project);
    const int match = NativeControlMapping::resolve(markerRef, refs, false);
    if (match < 0) return {};
    const auto& ref = refs[static_cast<size_t>(match)];
    replaceIds({"markerId", "selectedMarkerId", "targetId", "id"}, ref.id);
    nativeControlSet(command, "markerId", ref.id);
  }
  if (type == "ui_music_selection_snapshot" && !nativeJsonExtractString(command, "_ctlRows").empty()) {
    const bool regions = nativeJsonExtractString(command, "page") == "regions";
    if (g_nativeAppActivePanelModel.regionsPage != regions) return {}; // Never apply foreign row keys.
    std::vector<NativeControlMapping::Reference> rows;
    for (size_t i = 0; i < g_nativeAppActivePanelModel.rows.size(); ++i)
      rows.push_back(nativeControlRowReference(g_nativeAppActivePanelModel.rows[i], static_cast<int>(i), regions));
    std::string keys;
    const int count = std::max(0, std::min(256, static_cast<int>(nativeJsonInt64(command, "_ctlRows", 0))));
    int first = -1, last = -1;
    for (int i = 0; i < count; ++i) {
      const int match = NativeControlMapping::resolve(nativeControlReadReference(command, "_ctlRow" + std::to_string(i)), rows, false);
      if (match < 0) continue;
      if (!keys.empty()) keys += '\n';
      keys += rows[static_cast<size_t>(match)].id;
      const auto visible = std::find(g_nativeMainVisibleRowIndices.begin(),
        g_nativeMainVisibleRowIndices.end(), static_cast<size_t>(match));
      if (visible != g_nativeMainVisibleRowIndices.end()) {
        const int visualIndex = static_cast<int>(visible - g_nativeMainVisibleRowIndices.begin());
        if (first < 0) first = visualIndex;
        last = visualIndex;
      }
    }
    nativeControlSet(command, "keys", keys);
    NativeControlMapping::setField(command, "anchor", std::to_string(first));
    NativeControlMapping::setField(command, "focus", std::to_string(last));
  }
  const auto trackRef = nativeControlReadReference(command, "_ctlTrack");
  if (!trackRef.id.empty()) {
    MediaTrack* track = nativeProjectSyncResolveTrack(project, trackRef.id, trackRef.name, trackRef.index + 1);
    if (!track) return {};
    const auto localTrack = nativeControlTrackReference(project, track);
    const auto itemRef = nativeControlReadReference(command, "_ctlItem");
    if (!itemRef.id.empty()) {
      const auto items = nativeControlItemReferences(track, localTrack.index);
      const int index = NativeControlMapping::resolve(itemRef, items, true);
      if (index < 0) return {};
      const auto& item = items[static_cast<size_t>(index)];
      replaceIds({"itemId", "mediaItemId", "targetId", "guid"}, item.id);
      nativeControlSet(command, "itemId", item.id);
      nativeControlSet(command, "trackId", localTrack.id);
    } else {
      replaceIds({"targetId", "trackId", "guid", "id"}, localTrack.id);
    }
  }
  const auto songRef = nativeControlReadReference(command, "_ctlSong");
  if (!songRef.id.empty()) {
    const bool playlist = nativeJsonExtractString(command, "_ctlSongScope") == "playlist";
    const auto& songs = playlist && !g_nativeActivePlaylistItems.empty() ? g_nativeActivePlaylistItems : g_nativeSongWindows;
    const auto refs = nativeControlSongReferences(songs);
    const int index = NativeControlMapping::resolve(songRef, refs, false);
    if (index < 0) return {};
    const auto& song = songs[static_cast<size_t>(index)];
    replaceIds({"songId", "selectedPlaylistSongId", "selectedRegionId", "resultId"}, song.id);
    if (trackRef.id.empty()) replaceIds({"targetId", "id"}, song.id);
    if (type.rfind("multiloop_", 0) == 0 || type.rfind("multi_loop_", 0) == 0) {
      nativeControlSet(command, "songKey", nativeMultiLoopSongKey(song));
      nativeControlSet(command, "songName", song.name);
    }
    if (type == "smart_search_activate") NativeControlMapping::setField(command, "resultStart", nativeNumber(song.start, 9));
    nativeControlSet(command, "songId", song.id);
    double targetStart = song.start;
    if (nativeJsonBoolValue(command, "exactPosition", false)) {
      const std::string original = nativeControlFirst(raw, {"position", "startPos", "start_pos", "selectedStartPos"});
      if (nativeLooksNumeric(original)) {
        const double offset = std::atof(original.c_str()) - songRef.start;
        if (std::isfinite(offset)) targetStart = std::max(0.0, song.start + offset);
      }
    }
    if (!nativeJsonExtractString(command, "position").empty())
      NativeControlMapping::setField(command, "position", nativeNumber(targetStart, 9));
    for (const char* key : {"startPos", "start_pos", "selectedStartPos"})
      NativeControlMapping::setField(command, key, nativeNumber(targetStart, 9));
    for (const char* key : {"endPos", "end_pos", "selectedEndPos"})
      NativeControlMapping::setField(command, key, nativeNumber(song.end, 9));
    NativeControlMapping::setField(command, "playlistItemIndex", std::to_string(song.playlistOrder));
    // Marker enum indices and source numbers belong only to the sender's file.
    if (nativeJsonBoolValue(command, "seekToMarker", false)) {
      NativeControlMapping::setField(command, "markerNumber", std::to_string(song.sourceNumber));
      NativeControlMapping::setField(command, "markerEnumIndex", std::to_string(song.enumIndex));
    }
  }
  return command;
}
