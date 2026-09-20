#pragma once

#include <cstdint>
#include <string>

namespace vshook_license {

enum class Failure {
  None,
  Missing,
  Invalid,
  Signature,
  Machine,
  Hardware,
  NotYetValid,
  Expired,
  Clock
};

struct Result {
  bool valid = false;
  Failure failure = Failure::Invalid;
  bool offlineWarning = false;
  int64_t offlineWarningDeadline = 0;
  int offlineWarningNumber = 0;
};

struct OfflineGraceDecision {
  bool allowed = false;
  bool warning = false;
  int64_t deadline = 0;
};

Result validateInstalledLicense();
std::string installedTokenPath();
OfflineGraceDecision evaluateOfflineGrace(
  int64_t now,
  int64_t warningStartedAt,
  int warningCount,
  int64_t lastWarningAt,
  bool warningDeliveredNow = false);

} // namespace vshook_license
