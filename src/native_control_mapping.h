#pragma once
#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <vector>

// Control references describe existing local entities. They never carry paths,
// item state, media bytes or instructions for creating project objects.
namespace NativeControlMapping {
struct Reference {
  std::string id;
  std::string name;
  std::string kind;
  int index = -1; // zero based, within the corresponding local list
  double start = 0.0;
  double end = 0.0;
  bool range = false;
};
inline std::string normalized(std::string text) {
  const auto space = [](unsigned char c) { return std::isspace(c) != 0; };
  text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), space));
  text.erase(std::find_if_not(text.rbegin(), text.rend(), space).base(), text.end());
  for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
inline bool sameKind(const Reference& a, const Reference& b) {
  return a.kind.empty() || b.kind.empty() || a.kind == b.kind;
}
inline bool sameRange(const Reference& a, const Reference& b) {
  return a.range && b.range && std::isfinite(a.start) && std::isfinite(a.end) &&
    std::isfinite(b.start) && std::isfinite(b.end) &&
    std::fabs(a.start - b.start) <= .003 && std::fabs(a.end - b.end) <= .003;
}
inline int resolve(const Reference& wanted, const std::vector<Reference>& local,
                   bool stableIds) {
  if (stableIds && !wanted.id.empty()) {
    for (size_t i = 0; i < local.size(); ++i)
      if (sameKind(wanted, local[i]) && wanted.id == local[i].id) return static_cast<int>(i);
  }
  const std::string name = normalized(wanted.name);
  std::vector<int> names;
  for (size_t i = 0; i < local.size(); ++i) {
    if (sameKind(wanted, local[i]) && !name.empty() && normalized(local[i].name) == name)
      names.push_back(static_cast<int>(i));
  }
  if (names.size() == 1) return names.front();
  if (names.size() > 1) {
    int match = -1;
    for (int i : names) if (sameRange(wanted, local[static_cast<size_t>(i)])) {
      if (match >= 0) { match = -2; break; }
      match = i;
    }
    if (match >= 0) return match;
    for (int i : names) if (local[static_cast<size_t>(i)].index == wanted.index && wanted.index >= 0) return i;
    return -1; // Ambiguous names must never silently choose the first object.
  }
  int range = -1;
  for (size_t i = 0; i < local.size(); ++i) {
    if (sameKind(wanted, local[i]) && sameRange(wanted, local[i])) {
      if (range >= 0) { range = -2; break; }
      range = static_cast<int>(i);
    }
  }
  if (range >= 0) return range;
  if (wanted.index >= 0) {
    for (size_t i = 0; i < local.size(); ++i)
      if (sameKind(wanted, local[i]) && local[i].index == wanted.index) return static_cast<int>(i);
  }
  // Region/marker numbers are not globally unique identities. Reuse a number
  // only for old packets without a semantic reference, never over name/range.
  if (!stableIds && name.empty() && !wanted.range && wanted.index < 0 && !wanted.id.empty()) {
    for (size_t i = 0; i < local.size(); ++i)
      if (sameKind(wanted, local[i]) && wanted.id == local[i].id) return static_cast<int>(i);
  }
  return -1;
}

// Replace only a top-level JSON value, preserving all unrelated payload fields.
// Values passed by callers are already JSON encoded. Nested objects and escaped
// strings are skipped so a label containing e.g. \"targetId\" cannot be edited.
inline bool setField(std::string& json, const std::string& key, const std::string& encoded) {
  const size_t first = json.find_first_not_of(" \r\n\t");
  if (first == std::string::npos || json[first] != '{') return false;
  size_t i = first + 1;
  while (i < json.size()) {
    i = json.find_first_not_of(" \r\n\t,", i);
    if (i == std::string::npos) return false;
    if (json[i] == '}') {
      const bool empty = json.find_first_not_of(" \r\n\t", first + 1) == i;
      json.insert(i, std::string(empty ? "" : ",") + "\"" + key + "\":" + encoded);
      return true;
    }
    if (json[i] != '"') return false;
    const size_t keyStart = ++i;
    bool escape = false;
    for (; i < json.size(); ++i) {
      const char c = json[i];
      if (!escape && c == '"') break;
      if (!escape && c == '\\') escape = true; else escape = false;
    }
    if (i >= json.size()) return false;
    const bool wanted = json.compare(keyStart, i - keyStart, key) == 0;
    i = json.find_first_not_of(" \r\n\t", i + 1);
    if (i == std::string::npos || json[i] != ':') return false;
    i = json.find_first_not_of(" \r\n\t", i + 1);
    if (i == std::string::npos) return false;
    const size_t valueStart = i;
    int depth = 0;
    bool quoted = false;
    escape = false;
    for (; i < json.size(); ++i) {
      const char c = json[i];
      if (quoted) {
        if (!escape && c == '"') quoted = false;
        if (!escape && c == '\\') escape = true; else escape = false;
      } else if (c == '"') quoted = true;
      else if (c == '{' || c == '[') ++depth;
      else if ((c == '}' || c == ']') && depth > 0) --depth;
      else if (depth == 0 && (c == ',' || c == '}')) break;
    }
    if (i >= json.size()) return false;
    if (wanted) { json.replace(valueStart, i - valueStart, encoded); return true; }
  }
  return false;
}
}
