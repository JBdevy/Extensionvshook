#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "native_control_mapping.h"

struct MediaItem { std::string id, name; double start = 0, length = 0; };
struct MediaTrack { std::string id, name; std::vector<MediaItem*> items; };
struct NativeUiPlaylistRecord { std::string name; };
struct Marker { int number; std::string name; double start; bool region = false; };
struct ReaProject { std::string name; std::vector<MediaTrack*> tracks; MediaTrack master; std::vector<NativeUiPlaylistRecord> playlists; std::vector<Marker> markers; };
static ReaProject* current;
static std::vector<ReaProject*> projects;
static auto CountTracks_ptr = +[](ReaProject* p) { return static_cast<int>(p->tracks.size()); };
static auto GetTrack_ptr = +[](ReaProject* p, int i) { return i >= 0 && i < static_cast<int>(p->tracks.size()) ? p->tracks[i] : nullptr; };
static auto GetMasterTrack_ptr = +[](ReaProject* p) { return &p->master; };
static auto GetTrackNumMediaItems_ptr = +[](MediaTrack* t) { return static_cast<int>(t->items.size()); };
static auto GetTrackMediaItem_ptr = +[](MediaTrack* t, int i) { return i >= 0 && i < static_cast<int>(t->items.size()) ? t->items[i] : nullptr; };
static auto GetMediaItemInfo_Value_ptr = +[](MediaItem* i, const char* key) { return std::string(key) == "D_POSITION" ? i->start : i->length; };
static auto EnumProjects_ptr = +[](int i, char* path, int size) -> ReaProject* {
  ReaProject* p = i < 0 ? current : (i < static_cast<int>(projects.size()) ? projects[i] : nullptr);
  if (p && path && size > 0) std::snprintf(path, static_cast<size_t>(size), "%s", p->name.c_str());
  return p;
};
static auto CountProjectMarkers_ptr = +[](ReaProject* p, int* m, int* r) { *m = *r = 0; for (const auto& x : p->markers) (x.region ? *r : *m)++; return *m + *r; };
static auto EnumProjectMarkers3_ptr = +[](ReaProject* p, int i, bool* r, double* start, double* end, const char** name, int* number, int* color) {
  if (i < 0 || i >= static_cast<int>(p->markers.size())) return false;
  const auto& m = p->markers[i]; *r = m.region; *start = *end = m.start; *name = m.name.c_str(); *number = m.number; *color = 0; return true;
};
static ReaProject* getCurrentProject(char*, int) { return current; }
static std::string nativeTrim(const std::string& s) { auto a = s.find_first_not_of(" \n\r\t"); return a == std::string::npos ? "" : s.substr(a, s.find_last_not_of(" \n\r\t") - a + 1); }
static std::string nativeLower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
static bool nativeLooksNumeric(const std::string& s) { if (s.empty()) return false; char* end = nullptr; std::strtod(s.c_str(), &end); return end && !*end; }
#include "mapping-json-under-test.h"
static int64_t nativeJsonInt64(const std::string& s, const char* k, int64_t fallback) { const auto t = nativeJsonExtractString(s, k); return nativeLooksNumeric(t) ? std::strtoll(t.c_str(), nullptr, 10) : fallback; }
static bool nativeJsonBoolValue(const std::string& s, const char* k, bool fallback) { const auto t = nativeJsonExtractString(s,k); return t.empty() ? fallback : t == "true" || t == "1"; }
static std::string nativeJsonString(const std::string& text) { std::string s = "\""; for (char c : text) { if (c == '\\' || c == '"') s += '\\'; if (c == '\n') s += "\\n"; else s += c; } return s + '"'; }
static std::string nativeNumber(double n, int digits) { std::ostringstream s; s << std::fixed << std::setprecision(digits) << n; return s.str(); }
static std::string nativeTrackGuid(MediaTrack* t, int) { return t->id; }
static std::string nativeTrackName(MediaTrack* t, int) { return t->name; }
static std::string nativeMediaItemGuid(MediaItem* i, const std::string&) { return i->id; }
static std::string nativeTakeName(MediaItem* i, const std::string&) { return i->name; }
static std::string nativeBasenameNoRpp(const std::string& s) { return s; }
static MediaTrack* nativeFindTrackById(ReaProject* p, const std::string& id) { if (id == "MASTER_TRACK") return &p->master; for (auto* t:p->tracks) if(t->id==id) return t; return nullptr; }
#include "mapping-track-under-test.h"
static auto nativeUiReadPlaylistRecords(ReaProject* p) { return p->playlists; }
static bool nativeParseMarkerNumberFromId(std::string id, int& number) { if (!id.empty() && id[0]=='m') id.erase(0,1); if (!nativeLooksNumeric(id)) return false; number=std::atoi(id.c_str()); return true; }
static std::vector<std::string> nativeSplit(const std::string& s, char c) { std::vector<std::string> v; std::istringstream in(s); std::string a; while(std::getline(in,a,c)) v.push_back(a); return v; }
struct NativeSongWindow { std::string id, name, type; double start=0,end=0; bool isBlock=false,isHashChild=false,isRegionChild=false; int sourceNumber=0,enumIndex=-1,playlistOrder=0; bool isHashParent=false; int premixNumber=0; };
#include "mapping-multiloop-under-test.h"
static std::vector<NativeSongWindow> g_nativeSongWindows, g_nativeActivePlaylistItems;
static std::string g_nativeSelectedTab="regions";
static bool nativeSongIdMatches(const NativeSongWindow& s,const std::string& id) { return s.id==id; }
struct NativeAppActivePanelModel { struct Row { std::string id,name; int order=0; bool block=false; double start=0,end=0; bool child=false; }; bool regionsPage=true; std::vector<Row> rows; };
static NativeAppActivePanelModel g_nativeAppActivePanelModel;
static std::vector<size_t> g_nativeMainVisibleRowIndices;
#include "mapping-row-under-test.h"
struct NativePremixResolvedItem { MediaItem* item=nullptr; MediaTrack* track=nullptr; std::string itemId,trackId; };
static NativePremixResolvedItem nativeResolvePremixMediaItemById(ReaProject* p,const std::string& id) { for(auto* t:p->tracks) for(auto* i:t->items) if(i->id==id) return {i,t,i->id,t->id}; return {}; }
#include "native_project_sync_mapping.inc"

int main() {
  using namespace NativeControlMapping;
  std::string json = R"({"nested":{"targetId":"nested"},"text":"escaped \"targetId\"","targetId":"old","array":[1,{"targetId":2}]})";
  assert(setField(json,"targetId", "\"new\""));
  assert(json.find("\"targetId\":\"nested\"") != std::string::npos);
  assert(json.find("\"targetId\":\"new\"") != std::string::npos);
  assert(setField(json,"added","false"));
  assert(json.find("\"added\":false") != std::string::npos);
  std::string empty="{ }"; assert(setField(empty,"n","42")); assert(empty=="{ \"n\":42}");
  Reference a{"2","Música Á","region",0,100,150,true};
  std::vector<Reference> refs={{"2","Other","region",0,0,50,true},{"9","Música Á","region",1,300,350,true}};
  assert(resolve(a,refs,false)==1); // Foreign region 2 must not beat song name.
  refs[0].kind="marker"; a.name="Other"; a.index=0; assert(resolve(a,refs,false)==-1);
  std::vector<Reference> dup={{"x","same","item",0,0,10,true},{"y","same","item",1,10,20,true}};
  Reference wanted{"old","same","item",-1,10,20,true}; assert(resolve(wanted,dup,true)==1);
  wanted.range=false; assert(resolve(wanted,dup,true)==-1);

  MediaItem ia{"itemA","Guitarra Á",10,20}, ib{"itemB","Guitarra Á",400,20};
  MediaTrack guitarA{"trackA","Guitarra",{&ia}}, drumA{"drumA","Drums",{}};
  MediaTrack guitarB{"trackB","Guitarra",{&ib}}, drumB{"drumB","Drums",{}};
  ReaProject pa{"A",{&drumA,&guitarA},{"masterA","Master",{}},{{"Set principal"},{"Encore"}},{{4,"Verso",100,false}}};
  ReaProject pb{"B",{&guitarB,&drumB},{"masterB","Master",{}},{{"Encore"},{"Set principal"}},{{4,"Outro",5,false},{8,"Verso",300,false}}};
  current=&pa; projects={&pa};
  auto mixer=nativeProjectSyncDescribeControl(R"({"type":"mixer_set_volume","targetId":"trackA","ratio":0.6})");
  auto item=nativeProjectSyncDescribeControl(R"({"type":"premix_item_set_volume","itemId":"itemA","ratio":0.7})");
  auto marker=nativeProjectSyncDescribeControl(R"({"type":"marker_go","markerId":"m4"})");
  auto playlist=nativeProjectSyncDescribeControl(R"({"type":"select_playlist","playlistId":"1","playlistName":"Set principal"})");
  g_nativeSongWindows={{"2","Música Á","region",100,150,false,false,false,2,0,0}};
  auto song=nativeProjectSyncDescribeControl(R"({"type":"select_region","targetId":"2","startPos":100,"endPos":150})");
  auto multiloop=nativeProjectSyncDescribeControl(R"({"type":"multiloop_track_set","songKey":"2","trackId":"trackA","slot":1})");
  auto queue=nativeProjectSyncDescribeControl(R"({"type":"queue_region_song","targetId":"2","startPos":100,"endPos":150})");
  auto play=nativeProjectSyncDescribeControl(R"({"type":"play","targetId":"2","startPos":110,"endPos":150,"exactPosition":true})");
  g_nativeAppActivePanelModel.rows={{"2","Música Á",1,false,100,150}};
  g_nativeMainVisibleRowIndices={0};
  auto selection=nativeProjectSyncDescribeControl(std::string("{\"type\":\"ui_music_selection_snapshot\",\"page\":\"regions\",\"keys\":")+nativeJsonString(nativeUiMainRowKey(g_nativeAppActivePanelModel.rows[0],true))+"}");
  current=&pb; projects={&pb};
  g_nativeSongWindows={{"2","Wrong song","region",0,50,false,false,false,2,0,0},{"9","Música Á","region",300,350,false,false,false,9,1,0}};
  g_nativeAppActivePanelModel.rows={{"2","Wrong song",1,false,0,50},{"9","Música Á",2,false,300,350}};
  g_nativeMainVisibleRowIndices={0,1};
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(mixer),"targetId")=="trackB");
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(item),"itemId")=="itemB");
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(marker),"markerId")=="m8");
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(playlist),"playlistId")=="2");
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(multiloop),"songKey")=="9");
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(multiloop),"trackId")=="trackB");
  const auto mapped=nativeProjectSyncMapControl(song);
  assert(nativeJsonExtractString(mapped,"targetId")=="9");
  assert(std::atof(nativeJsonExtractString(mapped,"startPos").c_str())==300);
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(queue),"targetId")=="9");
  assert(std::atof(nativeJsonExtractString(nativeProjectSyncMapControl(play),"startPos").c_str())==310);
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(selection),"keys")==nativeUiMainRowKey(g_nativeAppActivePanelModel.rows[1],true));
  assert(nativeJsonExtractString(nativeProjectSyncMapControl(selection),"focus")=="1");
  assert(nativeProjectSyncMapControl(R"({"type":"stop"})")==R"({"type":"stop"})");
  pb.tracks.clear(); assert(nativeProjectSyncMapControl(mixer).empty()); assert(nativeProjectSyncMapControl(item).empty());
  // Translation itself cannot create/move media, tracks, markers or playlists.
  assert(pa.tracks.size()==2 && pb.markers.size()==2 && pa.markers.size()==1);
  assert(pb.playlists.size()==2 && ia.start==10 && ib.start==400);
  std::cout << "Project Sync control mapping: PASS\n";
}
