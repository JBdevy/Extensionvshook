#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

struct ReaProject {};
#include "premix-song-type.h"

static std::mutex g_nativeMutex;
static std::string g_nativePremixSelectedSongId;
static double g_nativePremixSelectedSongStart = 0.0;
static double g_nativePremixSelectedSongEnd = 0.0;
static std::string g_nativeCurrentPlayingId;
static std::string g_nativeSelectedId;
static const char* emptyExtState(const char*, const char*) { return ""; }
static auto GetExtState_ptr = &emptyExtState;

struct NativePremixPreset {};
using NativePremixDatabase = std::map<std::string, NativePremixPreset>;
static NativePremixDatabase nativeLoadPremixDatabase() { return {}; }
static std::map<std::string, bool> nativeLoadPremixEnabledKeys() { return {}; }
static std::string nativePremixSongKey(const NativeSongWindow& song) { return song.id; }
static std::string nativeJsonString(const std::string& value) {
  std::ostringstream result;
  result << std::quoted(value);
  return result.str();
}
static std::string nativeNumber(double value) {
  std::ostringstream result;
  result << value;
  return result.str();
}
static std::vector<const NativeSongWindow*> nativePremixFamilyChildren(
  const std::vector<NativeSongWindow>&, const NativeSongWindow*) { return {}; }
static std::string nativeBuildPremixItemRowsJson(ReaProject*, const NativeSongWindow* song) {
  return song ? "[{\"id\":" + nativeJsonString("item-for-" + song->name) + "}]" : "[]";
}
static std::string nativeBuildPremixSongSectionsJson(
  ReaProject*, const std::vector<const NativeSongWindow*>&) { return "[]"; }
static std::string nativeBuildPremixSongsJson(
  const std::vector<NativeSongWindow>&, const std::map<std::string, bool>&) { return "[]"; }
static std::string nativeBuildPremixTrackRowsJson(
  ReaProject*, const NativePremixPreset*, bool) { return "[]"; }

#include "premix-selection-under-test.h"

static int failures = 0;
static void check(bool passed, const std::string& description) {
  if (!passed) {
    ++failures;
    std::cerr << "FAIL: " << description << '\n';
  }
}
static NativeSongWindow makeSong(const std::string& id, const std::string& name,
                                 double start, double end) {
  NativeSongWindow song;
  song.id = id;
  song.name = name;
  song.start = start;
  song.end = end;
  song.sourceNumber = std::stoi(id);
  return song;
}
static void request(const std::string& id, double start = 0.0, double end = 0.0) {
  g_nativePremixSelectedSongId = id;
  g_nativePremixSelectedSongStart = start;
  g_nativePremixSelectedSongEnd = end;
}
static void checkIdentity(const std::vector<NativeSongWindow>& songs,
                          const std::string& expectedId, const NativeSongWindow& expectedSong,
                          const std::string& context) {
  ReaProject project;
  const auto json = nativeBuildPremixJson(&project, songs);
  check(json.find("\"selectedSongId\":" + nativeJsonString(expectedId) + ",") != std::string::npos,
    context + " response acknowledges the requested song identity");
  check(json.find("\"selectedCanonicalSongId\":" + nativeJsonString(expectedSong.id) + ",") != std::string::npos,
    context + " response exposes canonical identity");
  check(json.find("\"selectedSongName\":" + nativeJsonString(expectedSong.name) + ",") != std::string::npos,
    context + " response selects the correct song");
  check(json.find("\"selectedSongStart\":" + nativeNumber(expectedSong.start) + ",") != std::string::npos &&
    json.find("\"selectedSongEnd\":" + nativeNumber(expectedSong.end) + ",") != std::string::npos,
    context + " response retains selected bounds");
  check(json.find("item-for-" + expectedSong.name) != std::string::npos,
    context + " item publisher receives the selected song");
}

int main() {
  auto first = makeSong("99", "unrelated-first-song", 0.0, 20.0);
  auto region = makeSong("12", "same-number-region", 20.0, 40.0);
  auto parent = makeSong("8", "legacy-parent", 100.0, 160.0);
  parent.isHashParent = true;
  auto child = makeSong("12", "legacy-marker-child", 100.0, 120.0);
  child.isHashChild = true;
  child.parentId = parent.id;
  std::vector<NativeSongWindow> songs{first, region, parent, child};

  request("12", 100.0, 120.0);
  check(nativeFindPremixSelectedSong(songs) == &songs[3], "canonical numeric ID uses exact bounds");
  checkIdentity(songs, "12", child, "canonical child");

  // Drawer-derived legacy children are sent as m12; real regions remain 12.
  request("m12", 100.0, 120.0);
  check(nativeFindPremixSelectedSong(songs) == &songs[3], "marker alias resolves to the legacy child");
  checkIdentity(songs, "m12", child, "marker child alias");
  request("m12");
  check(nativeFindPremixSelectedSong(songs) == &songs[3], "marker alias without bounds excludes same-number region");
  checkIdentity(songs, "m12", child, "marker alias without bounds");

  request("12", 20.0, 40.0);
  check(nativeFindPremixSelectedSong(songs) == &songs[1], "canonical region retains its existing ID");
  checkIdentity(songs, "12", region, "regular region");
  request("8", 100.0, 160.0);
  check(nativeFindPremixSelectedSong(songs) == &songs[2], "parent canonical selection is preserved");
  checkIdentity(songs, "8", parent, "family parent");

  // Invalid aliases must never acknowledge an unrelated fallback as requested.
  for (const std::string invalid : {"m99", "m12x", "unknown"}) {
    request(invalid, 0.0, 20.0);
    check(nativeFindPremixSelectedSong(songs) == &songs[0], invalid + " uses existing fallback");
    checkIdentity(songs, "99", first, invalid + " fallback");
  }
  auto regionChild = child;
  regionChild.isRegionChild = true;
  regionChild.name = "region-child";
  songs[3] = regionChild;
  request("m12", 100.0, 120.0);
  check(nativeFindPremixSelectedSong(songs) == &songs[0], "marker aliases never match region children");
  checkIdentity(songs, "99", first, "region child rejects marker alias");
  songs[3] = child;
  songs[3].isBlock = true;
  check(nativeFindPremixSelectedSong(songs) == &songs[0], "marker aliases never match blocks");

  // Existing implicit transport and selection fallback order remains intact.
  request("");
  g_nativeCurrentPlayingId = "12";
  g_nativeSelectedId = "99";
  check(nativeFindPremixSelectedSong(songs) == &songs[1], "playing song remains first implicit fallback");
  g_nativeCurrentPlayingId.clear();
  check(nativeFindPremixSelectedSong(songs) == &songs[0], "selected song remains second implicit fallback");
  g_nativeSelectedId.clear();
  check(nativeFindPremixSelectedSong({}) == nullptr, "empty project has no selected song");

  if (failures) {
    std::cerr << failures << " Premix selection checks failed\n";
    return 1;
  }
  std::cout << "PREMIX_SELECTION_OK: marker aliases, duplicate numeric IDs, canonical regions, "
    "parents, response identity and safe fallbacks\n";
}
