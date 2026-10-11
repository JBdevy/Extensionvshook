#include "license_validator.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
  assert(argc == 2);
  std::ifstream fixtures(argv[1]);
  assert(fixtures.good());
  int count = 0;
  for (std::string line; std::getline(fixtures, line);) {
    std::istringstream input(line);
    std::string name, token, machine, hardware, now, expected;
    assert(std::getline(input, name, '\t'));
    assert(std::getline(input, token, '\t'));
    assert(std::getline(input, machine, '\t'));
    assert(std::getline(input, hardware, '\t'));
    assert(std::getline(input, now, '\t'));
    assert(std::getline(input, expected));
    const auto result = vshook_license::validateLocalActivation(
      token, machine, hardware, std::stoll(now));
    if (static_cast<int>(result.failure) != std::stoi(expected) ||
        result.valid != (std::stoi(expected) == 0)) {
      std::cerr << "Activation policy failed: " << name << "\n";
      return 1;
    }
    ++count;
  }
  assert(count >= 15);
  std::cout << "LICENSE_LOCAL_ACTIVATION_OK: " << count
            << " signed cases; long offline access, signature and device binding\n";
}
