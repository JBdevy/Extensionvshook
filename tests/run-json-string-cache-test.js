const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const source = fs.readFileSync(path.join(__dirname, '../src/vshook_extension.cpp'), 'utf8');
const start = source.indexOf('static std::string nativeJsonEscape(');
const end = source.indexOf('static std::string nativeTrim(', start);
if (start < 0 || end < start) throw new Error('Production JSON/cache implementation missing');
const production = source.slice(start, end).replace('return std::string("\\\"") + nativeJsonEscape(value)', '++serializations; return std::string("\\\"") + nativeJsonEscape(value)');
if (!production.includes('++serializations')) throw new Error('Serialization counter was not inserted');
const fixture = `
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
static size_t serializations = 0;
${production}
int main() {
  NativeJsonStringCache cache;
  const std::string controls("\\0\\b\\f\\n\\r\\t\\x01\\x1f", 8);
  const std::string cases[] = {"", "plain", "\\\"quoted\\\"", "C:\\\\Music\\\\track.wav", controls, u8"Canção 🎹", "", "same-size-A", "same-size-B"};
  for (const auto& value : cases) {
    const auto expected = nativeJsonString(value);
    assert(cache.get(value) == expected);
    const size_t before = serializations;
    for (int i = 0; i < 100; ++i) {
      const std::string copy(value); // a fresh object with identical content
      assert(cache.get(copy) == expected);
    }
    assert(serializations == before);
  }
  std::string mutableValue = "old";
  assert(cache.get(mutableValue) == "\\\"old\\\"");
  mutableValue[0] = 'n';
  assert(cache.get(mutableValue) == "\\\"nld\\\"");
  for (int i = 0; i < 256; ++i) {
    std::string bytes(1, static_cast<char>(i));
    assert(cache.get(bytes) == nativeJsonString(bytes));
  }
  NativeJsonStringCache other;
  other.get("different project");
  assert(cache.get("") == "\\\"\\\"");
  assert(other.get("different project") == "\\\"different project\\\"");
  std::string manifest;
  const std::string row = R"({"track":"Piano","path":"C:\\Audio\\Faixa.wav","items":[1,2,3]},)";
  while (manifest.size() < 1024 * 1024) manifest += row;
  constexpr int iterations = 60;
  volatile size_t bytes = 0;
  auto begin = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) bytes += nativeJsonString(manifest).size();
  auto plain = std::chrono::steady_clock::now() - begin;
  const size_t before = serializations;
  begin = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) bytes += cache.get(manifest).size();
  auto cached = std::chrono::steady_clock::now() - begin;
  assert(serializations - before == 1);
  // Content changes invalidate even without a revision or size change.
  manifest[manifest.size() / 2] = 'X';
  assert(cache.get(manifest) == nativeJsonString(manifest));
  const auto micros = [](auto value) { return std::chrono::duration_cast<std::chrono::microseconds>(value).count(); };
  std::cout << "JSON_STRING_CACHE_OK: exact bytes, controls, UTF-8, empty, in-place mutation, independent fields and one conversion per unchanged document\\n";
  std::cout << "BENCHMARK repeated_1MiB_document=" << iterations << " uncached_us=" << micros(plain) << " cached_us=" << micros(cached) << " bytes=" << bytes << "\\n";
}
`;
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'vshook-json-cache-'));
try {
  const cpp = path.join(temporary, 'test.cpp'); fs.writeFileSync(cpp, fixture);
  const windows = process.platform === 'win32';
  const executable = path.join(temporary, windows ? 'test.exe' : 'test');
  const args = windows ? ['/nologo', '/std:c++17', '/O2', '/EHsc', '/utf-8', '/Fe:' + executable, cpp] : ['-std=c++17', '-O2', '-Wall', '-Wextra', cpp, '-o', executable];
  for (const [command, parameters] of [[windows ? 'cl.exe' : (process.env.CXX || 'c++'), args], [executable, []]]) {
    const result = spawnSync(command, parameters, {cwd: temporary, encoding: 'utf8'});
    process.stdout.write(result.stdout || ''); process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(command + ' failed: ' + result.status);
  }
} finally { fs.rmSync(temporary, {recursive: true, force: true}); }
