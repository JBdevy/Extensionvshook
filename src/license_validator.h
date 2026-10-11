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
  NotYetValid
};

struct Result {
  bool valid = false;
  Failure failure = Failure::Invalid;
};

Result validateInstalledLicense();
std::string installedTokenPath();
// Pure local verification: no helper process, network, renewal deadline or
// auxiliary clock/warning files. The installed wrapper supplies this device.
Result validateLocalActivation(
  const std::string& token,
  const std::string& machine,
  const std::string& fingerprint,
  int64_t now);

} // namespace vshook_license
