#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct MediaItem {
  std::string id;
  double start;
  double length;
  bool muted = false;
};
struct MediaTrack {
  std::string name;
  std::vector<MediaItem> items;
};
struct ReaProject { std::vector<MediaTrack> tracks; };
#include "premix-types.h"

static std::vector<NativeUiPremixItemRow> g_nativeUiPremixRows;
static struct {
  bool active = false;
  ReaProject* project = nullptr;
  std::string itemId;
} g_nativePremixUniqueSolo;
static int countTracks(ReaProject* project) { return static_cast<int>(project->tracks.size()); }
static MediaTrack* getTrack(ReaProject* project, int index) { return &project->tracks.at(index); }
static int countItems(MediaTrack* track) { return static_cast<int>(track->items.size()); }
static MediaItem* getItem(MediaTrack* track, int index) { return &track->items.at(index); }
static double itemValue(MediaItem* item, const char* key) {
  if (std::strcmp(key, "D_POSITION") == 0) return item->start;
  if (std::strcmp(key, "D_LENGTH") == 0) return item->length;
  if (std::strcmp(key, "B_MUTE") == 0) return item->muted ? 1.0 : 0.0;
  return 0.0;
}
static double trackValue(MediaTrack*, const char*) { return 0.0; }
static auto CountTracks_ptr = &countTracks;
static auto GetTrack_ptr = &getTrack;
static auto GetTrackNumMediaItems_ptr = &countItems;
static auto GetTrackMediaItem_ptr = &getItem;
static auto GetMediaItemInfo_Value_ptr = &itemValue;
static auto GetMediaTrackInfo_Value_ptr = &trackValue;
static std::string nativeTrackName(MediaTrack* track, int) { return track->name; }
static std::string nativeTrackGuid(MediaTrack*, int index) { return "track-" + std::to_string(index); }
static std::string nativeMediaItemGuid(MediaItem* item, const std::string&) { return item->id; }
static std::string nativeTakeName(MediaItem* item, const std::string&) { return item->id; }
static double nativePremixMediaItemVolume(MediaItem*) { return 1.0; }
static double nativePremixVolumeToRatio(double volume) { return volume; }
static double nativeVolumeToDb(double volume) { return 20.0 * std::log10(volume); }
static std::string nativeJsonString(const std::string& value) {
  std::ostringstream result;
  result << std::quoted(value);
  return result.str();
}
static std::string nativeNumber(double value, int precision = 6) {
  std::ostringstream result;
  result << std::fixed << std::setprecision(precision) << value;
  return result.str();
}
#include "premix-scope-under-test.h"

static int failures = 0;
static void check(bool passed, const std::string& message) {
  if (!passed) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
static std::vector<std::string> jsonIds(const std::string& json) {
  const std::regex itemId("\"itemId\":\"([^\"]*)\"");
  std::vector<std::string> result;
  for (std::sregex_iterator it(json.begin(), json.end(), itemId), end; it != end; ++it)
    result.push_back((*it)[1].str());
  return result;
}
static std::string describe(const std::vector<std::string>& ids) {
  std::string result;
  for (const auto& id : ids) result += (result.empty() ? "" : ", ") + id;
  return result;
}
static void checkIds(const std::string& context, const std::vector<std::string>& actual,
                     const std::vector<std::string>& expected) {
  check(actual == expected, context + "\n  expected: " + describe(expected) +
    "\n  actual:   " + describe(actual));
}
static void checkSong(ReaProject* project, const NativeSongWindow& song,
                      const std::string& context, const std::vector<std::string>& expected) {
  checkIds(context + " JSON", jsonIds(nativeBuildPremixItemRowsJson(project, &song)), expected);
  g_nativeUiPremixRows.clear();
  nativeUiAppendPremixItems(project, song);
  std::vector<std::string> ids;
  for (size_t i = 0; i < g_nativeUiPremixRows.size(); ++i) {
    const auto& row = g_nativeUiPremixRows[i];
    ids.push_back(row.itemId);
    check(row.mediaItem && row.mediaTrack && row.itemNumber == static_cast<int>(i + 1),
      context + " native UI retains references and continuous numbering");
  }
  checkIds(context + " native UI", ids, expected);
}

int main() {
  ReaProject project;
  project.tracks.push_back({"Piano", {
    {"starts-before", 90.0, 15.0},
    {"covers-whole-song", 0.0, 300.0},
    {"starts-near-before", 99.9995, 3.0},
    {"starts-at-start", 100.0, 2.0},
    {"inside-muted", 104.0, 3.0, true},
    {"crosses-end", 118.0, 10.0},
    {"final-millisecond", 119.9995, 1.0},
    {"before", 80.0, 10.0},
    {"ends-at-start", 90.0, 10.0},
    {"starts-at-end", 120.0, 3.0},
    {"after", 125.0, 10.0}
  }});
  for (const std::string name : {"TELEPROMPT 1", "teleprompt-2", "Cifras", "MÉDIA", "Timecode"})
    project.tracks.push_back({name, {{"hidden-" + name, 105.0, 2.0}}});
  project.tracks.push_back({"Drums", {{"second-track", 101.0, 1.0}}});
  const std::vector<std::string> expected = {
    "starts-at-start", "inside-muted", "crosses-end", "final-millisecond", "second-track"
  };
  NativeSongWindow song;
  song.start = 100.0;
  song.end = 120.0;
  checkSong(&project, song, "regular song", expected);
  song.isHashChild = true;
  checkSong(&project, song, "legacy marker child", expected);
  song.isRegionChild = true;
  checkSong(&project, song, "region child", expected);
  song.isHashChild = false;
  song.isRegionChild = false;
  song.isHashParent = true;
  checkSong(&project, song, "family parent", expected);

  // Mixer grid keeps its existing overlap rule and numerical tolerance.
#if PREMIX_HAS_EXPLICIT_GRID_SCOPE
  const auto grid = nativeBuildPremixItemRowsJson(&project, &song, true);
#else
  song.isRegionChild = true;
  const auto grid = nativeBuildPremixItemRowsJson(&project, &song);
#endif
  checkIds("Mixer grid overlaps", jsonIds(grid), {
    "starts-before", "covers-whole-song", "starts-near-before", "starts-at-start",
    "inside-muted", "crosses-end", "second-track"
  });
  std::vector<std::string> catalogIds;
  for (const auto& item : project.tracks[0].items) catalogIds.push_back(item.id);
  catalogIds.push_back("second-track");
  checkIds("unscoped catalog", jsonIds(nativeBuildPremixItemRowsJson(&project, nullptr)), catalogIds);
  const auto json = nativeBuildPremixItemRowsJson(&project, &song);
  check(json.find("\"startPos\":118.000000,\"endPos\":128.000000") != std::string::npos,
    "Premix retains the complete extent of items that start inside and end after the song");
  checkSong(nullptr, song, "missing project", {});
  song.end = song.start;
  checkSong(&project, song, "empty interval", {});
  if (failures) {
    std::cerr << failures << " Premix item scope checks failed\n";
    return 1;
  }
  std::cout << "PREMIX_ITEM_SCOPE_OK: start-only UI/JSON for all song kinds, exact boundaries, "
    "technical tracks, complete item extents and preserved Mixer overlap/catalog\n";
}
