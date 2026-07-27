#pragma once

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
  Expired
};

struct Result {
  bool valid = false;
  Failure failure = Failure::Invalid;
};

Result validateInstalledLicense();
std::string installedTokenPath();

} // namespace vshook_license
