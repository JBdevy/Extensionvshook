#include "native_mtc_timecode.h"

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace vshook;

static void expectTime(double seconds, double fps, bool dropFrame,
  int hour, int minute, int second, int frame, int code, bool nearest = false)
{
  const NativeMtcTimeFields value = nativeMtcTimeFields(seconds, fps, dropFrame, nearest);
  if (value.hour != hour || value.minute != minute || value.second != second ||
      value.frame != frame || value.rateCode != code) {
    std::cerr << "MTC mismatch at " << seconds << "s / " << fps << "fps: "
      << value.hour << ':' << value.minute << ':' << value.second << ':' << value.frame
      << " code " << value.rateCode << '\n';
    std::exit(1);
  }
  int parts[8];
  for (int i = 0; i < 8; ++i) parts[i] = nativeMtcQuarterFrameData(value, i) & 15;
  if ((parts[0] | (parts[1] << 4)) != frame ||
      (parts[2] | (parts[3] << 4)) != second ||
      (parts[4] | (parts[5] << 4)) != minute ||
      (parts[6] | ((parts[7] & 1) << 4)) != hour || (parts[7] >> 1) != code) {
    std::cerr << "MTC quarter-frame roundtrip failed\n";
    std::exit(1);
  }
}

int main()
{
  expectTime(470, 50, false, 0, 7, 50, 0, 1);
  expectTime(470, 60, false, 0, 7, 50, 0, 3);
  expectTime(470.5, 48, false, 0, 7, 50, 12, 0);
  expectTime(469.999999, 30, false, 0, 7, 50, 0, 3, true);
  expectTime(469.999999, 30, false, 0, 7, 49, 29, 3);
  expectTime(60.060, 23.976, false, 0, 1, 0, 0, 0);
  expectTime(60.060, 29.97, false, 0, 1, 0, 0, 3);
  expectTime(60.060, 29.97, true, 0, 1, 0, 2, 2);
  expectTime(60.060, 59.94, true, 0, 1, 0, 2, 2);
  expectTime(60.060, 59.94, false, 0, 1, 0, 0, 3);
  expectTime(17982.0 * 1001 / 30000, 29.97, true, 0, 10, 0, 0, 2);
  expectTime(2589408.0 * 1001 / 30000, 29.97, true, 0, 0, 0, 0, 2, true);
  expectTime(86400, 60, false, 0, 0, 0, 0, 3);
  expectTime(120, 27, false, 0, 2, 0, 0, 3);
  expectTime(120, std::numeric_limits<double>::quiet_NaN(), true, 0, 2, 0, 0, 3);
  expectTime(std::numeric_limits<double>::infinity(), 30, false, 0, 0, 0, 0, 3);
  for (double rawRate : {24.0, 25.0, 30.0, 48.0, 50.0, 60.0}) {
    const NativeMtcFrameRate rate = nativeMtcFrameRate(rawRate, false);
    if (rate.framesPerSecond != rate.nominalRate) return 1;
  }
  std::cout << "native MTC timecode tests passed\n";
}
