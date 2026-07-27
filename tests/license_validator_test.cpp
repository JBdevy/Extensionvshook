#include "../src/license_validator.h"

#include <iostream>

int main()
{
  const vshook_license::Result result =
    vshook_license::validateInstalledLicense();
  std::cout << (result.valid ? "LICENSE_VALID" : "LICENSE_INVALID")
            << " " << static_cast<int>(result.failure) << "\n";
  return result.valid ? 0 : 1;
}
