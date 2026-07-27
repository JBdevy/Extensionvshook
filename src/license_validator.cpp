#include "license_validator.h"
#include "license_public_key.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <bcrypt.h>
#elif defined(__APPLE__)
  #include <CommonCrypto/CommonDigest.h>
  #include <CoreFoundation/CoreFoundation.h>
  #include <Security/Security.h>
  #include <sys/stat.h>
#else
  #include <sys/stat.h>
#endif

namespace vshook_license {
namespace {

constexpr const char* kProduct = "VSLIVE";
constexpr const char* kFingerprintPrefix = "VSHOOK_DEVICE_V1";

std::string trim(std::string value)
{
  size_t first = 0;
  while (first < value.size() &&
         std::isspace(static_cast<unsigned char>(value[first]))) ++first;
  size_t last = value.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
  return value.substr(first, last - first);
}

std::string normalize(std::string value)
{
  value = trim(std::move(value));
  std::string out;
  out.reserve(value.size());
  for (unsigned char c : value) {
    if (!std::isspace(c)) out.push_back(static_cast<char>(std::toupper(c)));
  }
  return out;
}

std::string envValue(const char* name)
{
  if (!name) return {};
#ifdef _WIN32
  char* value = nullptr;
  size_t length = 0;
  if (_dupenv_s(&value, &length, name) != 0 || !value) return {};
  std::string out(value);
  std::free(value);
  return out;
#else
  const char* value = std::getenv(name);
  return value ? value : "";
#endif
}

std::string joinPath(const std::string& base, const std::string& child)
{
  if (base.empty()) return child;
  const char last = base.back();
  return base + ((last == '/' || last == '\\') ? "" : "/") + child;
}

std::string readFile(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

std::string sharedDirectory()
{
#ifdef _WIN32
  std::string value = envValue("PUBLIC");
  if (value.empty()) value = envValue("ALLUSERSPROFILE");
  return value.empty() ? "C:/Users/Public" : value;
#elif defined(__APPLE__)
  return "/Users/Shared";
#else
  std::string value = envValue("HOME");
  return value.empty() ? "." : value;
#endif
}

std::string machineIdPath()
{
  return joinPath(sharedDirectory(), "vslive_machine_id.dat");
}

std::string sanitizeAnchor(std::string value)
{
  std::string out;
  out.reserve(value.size());
  bool pendingSpace = false;
  for (unsigned char c : value) {
    if (std::isspace(c)) {
      pendingSpace = !out.empty();
      continue;
    }
    if (pendingSpace) out.push_back(' ');
    pendingSpace = false;
    out.push_back(static_cast<char>(c));
  }
  return normalize(out);
}

#ifdef _WIN32
std::string hardwareAnchor()
{
  char value[512]{};
  DWORD size = sizeof(value);
  const LSTATUS status = RegGetValueA(
    HKEY_LOCAL_MACHINE,
    "SOFTWARE\\Microsoft\\Cryptography",
    "MachineGuid",
    RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
    nullptr,
    value,
    &size);
  return status == ERROR_SUCCESS ? sanitizeAnchor(value) : std::string();
}
#elif defined(__APPLE__)
std::string runCapture(const char* command)
{
  FILE* pipe = command ? popen(command, "r") : nullptr;
  if (!pipe) return {};
  std::string output;
  char buffer[512]{};
  while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe)) {
    output += buffer;
  }
  pclose(pipe);
  return sanitizeAnchor(output);
}

std::string hardwareAnchor()
{
  std::string value = runCapture(
    "ioreg -rd1 -c IOPlatformExpertDevice | "
    "awk -F'\"' '/IOPlatformUUID/ {print $(NF-1)}'");
  if (value.empty()) {
    value = runCapture(
      "system_profiler SPHardwareDataType | "
      "awk -F': ' '/Hardware UUID/ {print $2}'");
  }
  return value;
}
#else
std::string hardwareAnchor() { return {}; }
#endif

std::string genericAnchor()
{
  std::string host = envValue("COMPUTERNAME");
  if (host.empty()) host = envValue("HOSTNAME");
  if (host.empty()) host = "UNKNOWNHOST";
  return sanitizeAnchor(host);
}

bool sha256(const unsigned char* data, size_t size,
            std::array<unsigned char, 32>& digest)
{
#ifdef _WIN32
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD objectLength = 0;
  DWORD resultLength = 0;
  std::vector<unsigned char> object;
  bool ok = false;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                  nullptr, 0) < 0) goto done;
  if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&objectLength),
                        sizeof(objectLength), &resultLength, 0) < 0) goto done;
  object.resize(objectLength);
  if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength,
                       nullptr, 0, 0) < 0) goto done;
  if (BCryptHashData(hash, const_cast<PUCHAR>(data),
                     static_cast<ULONG>(size), 0) < 0) goto done;
  if (BCryptFinishHash(hash, digest.data(),
                       static_cast<ULONG>(digest.size()), 0) < 0) goto done;
  ok = true;
done:
  if (hash) BCryptDestroyHash(hash);
  if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
  return ok;
#elif defined(__APPLE__)
  return CC_SHA256(data, static_cast<CC_LONG>(size), digest.data()) != nullptr;
#else
  (void)data; (void)size; (void)digest;
  return false;
#endif
}

std::string hexUpper(const unsigned char* data, size_t size)
{
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out(size * 2, '0');
  for (size_t i = 0; i < size; ++i) {
    out[i * 2] = hex[(data[i] >> 4) & 0x0F];
    out[i * 2 + 1] = hex[data[i] & 0x0F];
  }
  return out;
}

std::string deviceFingerprint()
{
  std::string anchor = hardwareAnchor();
  if (anchor.empty()) anchor = genericAnchor();
#ifdef _WIN32
  constexpr const char* platform = "win32";
#elif defined(__APPLE__)
  constexpr const char* platform = "darwin";
#else
  constexpr const char* platform = "linux";
#endif
  const std::string input =
    std::string(kFingerprintPrefix) + "|" + platform + "|" + normalize(anchor);
  std::array<unsigned char, 32> digest{};
  if (!sha256(reinterpret_cast<const unsigned char*>(input.data()),
              input.size(), digest)) return {};
  return hexUpper(digest.data(), digest.size());
}

std::string legacySimpleHash(const std::string& input)
{
  uint32_t h1 = 0x45D9u;
  uint32_t h2 = 0x2710u;
  for (size_t index = 0; index < input.size(); ++index) {
    const uint32_t i = static_cast<uint32_t>(index + 1);
    const uint32_t byte = static_cast<unsigned char>(input[index]);
    h1 = (h1 ^ (byte * i + 17u)) & 0xFFFFFFu;
    h2 = (h2 + ((byte + i - 1u) * 131u)) & 0xFFFFFFu;
    h1 = (h1 * 33u + h2) & 0xFFFFFFu;
    h2 = (h2 * 17u + h1) & 0xFFFFFFu;
  }
  const uint32_t value = (h1 << 12u) + h2;
  std::ostringstream out;
  out << std::uppercase << std::hex;
  out.width(8);
  out.fill('0');
  out << value;
  return out.str();
}

std::string machineId()
{
  const std::string cached = normalize(readFile(machineIdPath()));
  if (!cached.empty() &&
      std::all_of(cached.begin(), cached.end(),
        [](unsigned char c) { return std::isxdigit(c) != 0; })) {
    return cached;
  }
  std::string anchor = hardwareAnchor();
  if (anchor.empty()) anchor = genericAnchor();
  return legacySimpleHash(std::string(kProduct) + "|" + normalize(anchor));
}

bool decodeBase64Url(const std::string& input, std::vector<unsigned char>& out)
{
  out.clear();
  uint32_t accumulator = 0;
  int bits = 0;
  for (unsigned char c : input) {
    int value = -1;
    if (c >= 'A' && c <= 'Z') value = c - 'A';
    else if (c >= 'a' && c <= 'z') value = c - 'a' + 26;
    else if (c >= '0' && c <= '9') value = c - '0' + 52;
    else if (c == '-') value = 62;
    else if (c == '_') value = 63;
    else return false;
    accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xFF));
    }
  }
  return true;
}

std::string jsonString(const std::string& json, const std::string& key)
{
  const std::string token = "\"" + key + "\"";
  size_t pos = json.find(token);
  if (pos == std::string::npos) return {};
  pos = json.find(':', pos + token.size());
  if (pos == std::string::npos) return {};
  do { ++pos; } while (pos < json.size() &&
                       std::isspace(static_cast<unsigned char>(json[pos])));
  if (pos >= json.size() || json[pos] != '"') return {};
  std::string out;
  bool escaped = false;
  for (++pos; pos < json.size(); ++pos) {
    const char c = json[pos];
    if (escaped) {
      if (c == '"' || c == '\\' || c == '/') out.push_back(c);
      else return {};
      escaped = false;
    } else if (c == '\\') escaped = true;
    else if (c == '"') return out;
    else out.push_back(c);
  }
  return {};
}

int64_t jsonInteger(const std::string& json, const std::string& key,
                    bool& found)
{
  found = false;
  const std::string token = "\"" + key + "\"";
  size_t pos = json.find(token);
  if (pos == std::string::npos) return 0;
  pos = json.find(':', pos + token.size());
  if (pos == std::string::npos) return 0;
  do { ++pos; } while (pos < json.size() &&
                       std::isspace(static_cast<unsigned char>(json[pos])));
  if (pos >= json.size()) return 0;
  char* end = nullptr;
  const long long value = std::strtoll(json.c_str() + pos, &end, 10);
  found = end && end != json.c_str() + pos;
  return static_cast<int64_t>(value);
}

bool verifySignature(const std::string& message,
                     const std::vector<unsigned char>& signature)
{
  if (signature.empty()) return false;
#ifdef _WIN32
  std::array<unsigned char, 32> digest{};
  if (!sha256(reinterpret_cast<const unsigned char*>(message.data()),
              message.size(), digest)) return false;

  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_KEY_HANDLE key = nullptr;
  bool valid = false;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM,
                                  nullptr, 0) < 0) goto done;
  {
    const ULONG exponentSize =
      static_cast<ULONG>(sizeof(vshook_license_key::kRsaExponent));
    const ULONG modulusSize =
      static_cast<ULONG>(sizeof(vshook_license_key::kRsaModulus));
    std::vector<unsigned char> blob(
      sizeof(BCRYPT_RSAKEY_BLOB) + exponentSize + modulusSize);
    auto* header = reinterpret_cast<BCRYPT_RSAKEY_BLOB*>(blob.data());
    header->Magic = BCRYPT_RSAPUBLIC_MAGIC;
    header->BitLength = modulusSize * 8;
    header->cbPublicExp = exponentSize;
    header->cbModulus = modulusSize;
    header->cbPrime1 = 0;
    header->cbPrime2 = 0;
    unsigned char* cursor = blob.data() + sizeof(BCRYPT_RSAKEY_BLOB);
    std::copy(std::begin(vshook_license_key::kRsaExponent),
              std::end(vshook_license_key::kRsaExponent), cursor);
    cursor += exponentSize;
    std::copy(std::begin(vshook_license_key::kRsaModulus),
              std::end(vshook_license_key::kRsaModulus), cursor);
    if (BCryptImportKeyPair(algorithm, nullptr, BCRYPT_RSAPUBLIC_BLOB,
                            &key, blob.data(),
                            static_cast<ULONG>(blob.size()), 0) < 0) goto done;
  }
  {
    BCRYPT_PKCS1_PADDING_INFO padding{
      const_cast<wchar_t*>(BCRYPT_SHA256_ALGORITHM)
    };
    valid = BCryptVerifySignature(
      key, &padding, digest.data(), static_cast<ULONG>(digest.size()),
      const_cast<PUCHAR>(signature.data()),
      static_cast<ULONG>(signature.size()),
      BCRYPT_PAD_PKCS1) >= 0;
  }
done:
  if (key) BCryptDestroyKey(key);
  if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
  return valid;
#elif defined(__APPLE__)
  CFDataRef keyData = CFDataCreate(
    kCFAllocatorDefault,
    vshook_license_key::kRsaPkcs1Der,
    static_cast<CFIndex>(sizeof(vshook_license_key::kRsaPkcs1Der)));
  int bits = static_cast<int>(
    sizeof(vshook_license_key::kRsaModulus) * 8);
  CFNumberRef bitCount = CFNumberCreate(
    kCFAllocatorDefault, kCFNumberIntType, &bits);
  const void* keys[] = {
    kSecAttrKeyType, kSecAttrKeyClass, kSecAttrKeySizeInBits
  };
  const void* values[] = {
    kSecAttrKeyTypeRSA, kSecAttrKeyClassPublic, bitCount
  };
  CFDictionaryRef attributes = CFDictionaryCreate(
    kCFAllocatorDefault, keys, values, 3,
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks);
  CFErrorRef error = nullptr;
  SecKeyRef key = SecKeyCreateWithData(keyData, attributes, &error);
  CFDataRef messageData = CFDataCreate(
    kCFAllocatorDefault,
    reinterpret_cast<const UInt8*>(message.data()),
    static_cast<CFIndex>(message.size()));
  CFDataRef signatureData = CFDataCreate(
    kCFAllocatorDefault, signature.data(),
    static_cast<CFIndex>(signature.size()));
  bool valid = key && SecKeyVerifySignature(
    key,
    kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
    messageData,
    signatureData,
    &error);
  if (error) CFRelease(error);
  if (signatureData) CFRelease(signatureData);
  if (messageData) CFRelease(messageData);
  if (key) CFRelease(key);
  if (attributes) CFRelease(attributes);
  if (bitCount) CFRelease(bitCount);
  if (keyData) CFRelease(keyData);
  return valid;
#else
  (void)message;
  return false;
#endif
}

} // namespace

std::string installedTokenPath()
{
#ifdef _WIN32
  return joinPath(sharedDirectory(), "vshook_license_v3.token");
#else
  return joinPath(sharedDirectory(), ".vshook_license_v3.token");
#endif
}

Result validateInstalledLicense()
{
  const std::string token = trim(readFile(installedTokenPath()));
  if (token.empty() || token.size() > 16384) {
    return {false, Failure::Missing};
  }

  const size_t firstDot = token.find('.');
  const size_t secondDot = firstDot == std::string::npos
    ? std::string::npos
    : token.find('.', firstDot + 1);
  if (firstDot == std::string::npos || secondDot == std::string::npos ||
      token.find('.', secondDot + 1) != std::string::npos) {
    return {false, Failure::Invalid};
  }

  const std::string headerPart = token.substr(0, firstDot);
  const std::string payloadPart =
    token.substr(firstDot + 1, secondDot - firstDot - 1);
  const std::string signaturePart = token.substr(secondDot + 1);
  std::vector<unsigned char> headerBytes;
  std::vector<unsigned char> payloadBytes;
  std::vector<unsigned char> signature;
  if (!decodeBase64Url(headerPart, headerBytes) ||
      !decodeBase64Url(payloadPart, payloadBytes) ||
      !decodeBase64Url(signaturePart, signature)) {
    return {false, Failure::Invalid};
  }

  const std::string header(headerBytes.begin(), headerBytes.end());
  const std::string payload(payloadBytes.begin(), payloadBytes.end());
  if (jsonString(header, "alg") != "RS256" ||
      jsonString(header, "typ") != "VSHOOK-LICENSE" ||
      jsonString(header, "kid") != vshook_license_key::kKeyId) {
    return {false, Failure::Invalid};
  }
  if (!verifySignature(headerPart + "." + payloadPart, signature)) {
    return {false, Failure::Signature};
  }

  bool versionFound = false;
  const int64_t version = jsonInteger(payload, "v", versionFound);
  if (!versionFound || version != 1 ||
      jsonString(payload, "p") != kProduct) {
    return {false, Failure::Invalid};
  }
  if (normalize(jsonString(payload, "m")) != machineId()) {
    return {false, Failure::Machine};
  }
  if (normalize(jsonString(payload, "f")) != deviceFingerprint()) {
    return {false, Failure::Hardware};
  }

  bool nbfFound = false;
  bool expFound = false;
  const int64_t notBefore = jsonInteger(payload, "nbf", nbfFound);
  const int64_t expiresAt = jsonInteger(payload, "exp", expFound);
  const int64_t now = static_cast<int64_t>(std::time(nullptr));
  if (!nbfFound || !expFound || expiresAt <= 0) {
    return {false, Failure::Invalid};
  }
  if (notBefore > now + 300) return {false, Failure::NotYetValid};
  if (expiresAt <= now) return {false, Failure::Expired};
  return {true, Failure::None};
}

} // namespace vshook_license
