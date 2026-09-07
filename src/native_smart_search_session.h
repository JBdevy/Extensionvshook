#pragma once
#include <cstdint>
#include <map>
#include <string>

// Main-thread state. Session + sequence reject delayed/duplicate app requests.
struct NativeSmartSearchSession {
  std::string client;
  uint64_t serial = 0, sequence = 0, activationSequence = 0;
  bool active = false, activationOk = false;
  std::string activationError;
  std::map<std::string, uint64_t> latestSerial;

  bool open(const std::string& owner, uint64_t generation, uint64_t revision) {
    if (owner.empty() || owner.size() > 128 || !generation || !revision) return false;
    const auto found = latestSerial.find(owner);
    if (found != latestSerial.end() && generation <= found->second) return false;
    if (found == latestSerial.end() && latestSerial.size() >= 64) latestSerial.erase(latestSerial.begin());
    latestSerial[owner] = generation;
    client = owner;
    serial = generation;
    sequence = revision;
    active = true;
    activationSequence = 0;
    activationOk = false;
    activationError.clear();
    return true;
  }

  bool accept(const std::string& owner, uint64_t generation, uint64_t revision) {
    if (!active || owner != client || generation != serial || revision <= sequence) return false;
    sequence = revision;
    return true;
  }
};
