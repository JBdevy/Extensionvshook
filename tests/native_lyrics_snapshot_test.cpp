#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

struct MediaItem { double start, length; std::string notes; };
struct MediaTrack { std::vector<MediaItem*> items; };
struct ReaProject { std::vector<MediaTrack*> tracks; };
static size_t itemReads = 0, valueReads = 0, notesReads = 0;
static int countTracks(ReaProject* p) { return static_cast<int>(p->tracks.size()); }
static MediaTrack* trackAt(ReaProject* p, int i) { return p->tracks.at(i); }
static int countItems(MediaTrack* t) { return static_cast<int>(t->items.size()); }
static MediaItem* itemAt(MediaTrack* t, int i) { ++itemReads; return t->items.at(i); }
static double itemValue(MediaItem* i, const char* key) {
  ++valueReads;
  return std::string(key) == "D_POSITION" ? i->start : i->length;
}
static bool notesOf(MediaItem* i, const char*, char* out, bool write) {
  assert(!write);
  ++notesReads;
  std::strcpy(out, i->notes.c_str());
  return true;
}
static auto CountTracks_ptr = &countTracks;
static auto GetTrack_ptr = &trackAt;
static auto GetTrackNumMediaItems_ptr = &countItems;
static auto GetTrackMediaItem_ptr = &itemAt;
static auto GetMediaItemInfo_Value_ptr = &itemValue;
static auto GetSetMediaItemInfo_String_ptr = &notesOf;
static std::string nativeTrim(const std::string& s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  return first == std::string::npos ? "" :
    s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
#include "lyrics-under-test.h"

// Frozen 1.0.5 range lookup: independent oracle for ordering and boundaries.
static std::string originalRange(ReaProject* p, double start, double end) {
  std::string best;
  for (int ti = 0; ti < CountTracks_ptr(p); ++ti) {
    auto* track = GetTrack_ptr(p, ti);
    if (!track) continue;
    for (int ii = 0; ii < GetTrackNumMediaItems_ptr(track); ++ii) {
      auto* item = GetTrackMediaItem_ptr(track, ii);
      if (!item) continue;
      const double pos = GetMediaItemInfo_Value_ptr(item, "D_POSITION");
      const double itemEnd = pos + std::max(0.0, GetMediaItemInfo_Value_ptr(item, "D_LENGTH"));
      const bool inside = pos >= start - 0.0005 && pos < end - 0.0005;
      if (!inside && !(itemEnd > start + 0.0005 && pos < end - 0.0005)) continue;
      const auto notes = nativeMediaItemNotes(item);
      if (notes.empty()) continue;
      if (inside) return notes;
      if (best.empty()) best = notes;
    }
  }
  return best;
}

int main() {
  MediaItem overlap{-10, 100, "overlap"}, inside{10, 0, "  inside\n"};
  MediaItem duplicate{10, 2, "later track"}, outside{40, 2, "outside"};
  MediaTrack first{{&overlap, nullptr, &inside}}, second{{&duplicate, &outside}};
  ReaProject project{{&first, nullptr, &second}};
  NativeLyricsRangeReader reader(&project);
  assert(reader.forRange(0, 20) == "inside");
  assert(reader.forRange(11, 20) == "overlap");
  assert(reader.forRange(40, 50) == "outside");
  assert(reader.forRange(100, 200).empty());
  assert(reader.forRange(10, 10.0005) == originalRange(&project, 10, 10.0005));
  assert(reader.forRange(10.0005, 12) == originalRange(&project, 10.0005, 12));
  // The reader ends with its snapshot. Edits/deletion/undo are read afresh.
  inside.notes = "edited";
  assert(NativeLyricsRangeReader(&project).forRange(0, 20) == "edited");
  first.items.pop_back();
  assert(NativeLyricsRangeReader(&project).forRange(0, 20) == "later track");
  first.items.push_back(&inside);
  assert(NativeLyricsRangeReader(&project).forRange(0, 20) == "edited");
  assert(NativeLyricsRangeReader(nullptr).forRange(0, 20).empty());
  GetTrack_ptr = nullptr;
  assert(NativeLyricsRangeReader(&project).forRange(0, 20).empty());
  GetTrack_ptr = &trackAt;
  itemReads = 0;
  NativeLyricsRangeReader earlyReader(&project);
  assert(earlyReader.forRange(-10, 20) == "overlap");
  assert(itemReads == 1); // A simple first-item hit must not scan the rest.
  assert(earlyReader.forRange(40, 50) == "outside");
  const auto prefixReads = itemReads;
  assert(earlyReader.forRange(0, 20) == "edited");
  assert(itemReads == prefixReads);

  // Large project without notes exercises the worst repeated project walk.
  constexpr int songCount = 200, trackCount = 32;
  std::vector<MediaItem> items;
  items.reserve(songCount * trackCount);
  std::vector<MediaTrack> tracks(trackCount);
  ReaProject large;
  for (auto& track : tracks) {
    large.tracks.push_back(&track);
    for (int song = 0; song < songCount; ++song) {
      items.push_back({song * 30.0, 30, ""});
      track.items.push_back(&items.back());
    }
  }
  itemReads = valueReads = notesReads = 0;
  auto begin = std::chrono::steady_clock::now();
  for (int song = 0; song < songCount; ++song)
    assert(originalRange(&large, song * 30, (song + 1) * 30).empty());
  const auto oldUs = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now() - begin).count();
  const size_t oldValues = valueReads;
  itemReads = valueReads = notesReads = 0;
  begin = std::chrono::steady_clock::now();
  NativeLyricsRangeReader largeReader(&large);
  for (int song = 0; song < songCount; ++song)
    assert(largeReader.forRange(song * 30, (song + 1) * 30).empty());
  const auto newUs = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now() - begin).count();
  assert(itemReads == items.size() && valueReads == items.size() * 2);
  assert(notesReads == items.size());
  assert(oldValues == valueReads * songCount);
  std::cout << "LYRICS_SNAPSHOT_WORK_OK: 200 songs/32 tracks, position/length reads "
    << oldValues << " -> " << valueReads << ", synthetic host time "
    << oldUs << "us -> " << newUs << "us\n";

  // Overlapping/nested regions, empty notes and arbitrary item ordering.
  std::mt19937 random(105);
  for (auto& item : items) {
    item.start = (random() % 2000) * 0.1;
    item.length = (random() % 1000) * 0.1;
    if (random() % 5 == 0) item.notes = "text " + std::to_string(random());
  }
  NativeLyricsRangeReader randomReader(&large);
  for (int query = 0; query < 200; ++query) {
    const double start = (random() % 1800) * 0.1;
    const double end = start + (random() % 200) * 0.1;
    assert(randomReader.forRange(start, end) == originalRange(&large, start, end));
  }
  std::cout << "LYRICS_PRIORITY_EDITS_AND_RANDOM_OVERLAPS_OK\n";
}
