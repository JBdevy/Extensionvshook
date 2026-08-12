#include "native_static_image_decoder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <wincodec.h>
#include <wrl/client.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/CGImageProperties.h>
#include <ImageIO/ImageIO.h>
#include <sys/stat.h>
#endif

namespace vshook_static_image {
namespace {

constexpr std::size_t kMaximumCacheEntries = 8;
constexpr std::size_t kMaximumCacheBytes = 64u * 1024u * 1024u;

struct FileIdentity {
  std::string cachePath;
  std::uint64_t size = 0;
  std::int64_t modifiedSeconds = 0;
  std::uint32_t modifiedSubsecond = 0;
#ifdef _WIN32
  std::wstring nativePath;
#else
  std::string nativePath;
#endif
};

struct CacheKey {
  std::string path;
  std::uint64_t size = 0;
  std::int64_t modifiedSeconds = 0;
  std::uint32_t modifiedSubsecond = 0;

  bool operator==(const CacheKey& other) const noexcept
  {
    return path == other.path &&
      size == other.size &&
      modifiedSeconds == other.modifiedSeconds &&
      modifiedSubsecond == other.modifiedSubsecond;
  }
};

struct CacheKeyHash {
  std::size_t operator()(const CacheKey& key) const noexcept
  {
    std::size_t value = std::hash<std::string>{}(key.path);
    const auto mix = [&](std::size_t next) {
      value ^= next + static_cast<std::size_t>(0x9e3779b9u) +
        (value << 6) + (value >> 2);
    };
    mix(std::hash<std::uint64_t>{}(key.size));
    mix(std::hash<std::int64_t>{}(key.modifiedSeconds));
    mix(std::hash<std::uint32_t>{}(key.modifiedSubsecond));
    return value;
  }
};

struct CachedImage {
  std::shared_ptr<const std::vector<std::uint8_t>> storage;
  int width = 0;
  int height = 0;
  int stride = 0;
};

struct CacheEntry {
  std::shared_ptr<const CachedImage> image;
  std::size_t byteCount = 0;
  std::uint64_t lastUse = 0;
};

struct CacheState {
  std::mutex mutex;
  std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> entries;
  std::size_t byteCount = 0;
  std::uint64_t clock = 0;
  std::uint64_t generation = 0;
};

// Namespace-scope initialization is intentional. The Windows target disables
// guarded function-local statics, while this cache may be called concurrently.
CacheState g_cache;

void setStatus(DecodeStatus* output, DecodeStatus value) noexcept
{
  if (output) *output = value;
}

CacheKey cacheKey(const FileIdentity& identity)
{
  return {
    identity.cachePath,
    identity.size,
    identity.modifiedSeconds,
    identity.modifiedSubsecond,
  };
}

bool sameFileVersion(
  const FileIdentity& left,
  const FileIdentity& right) noexcept
{
  return left.cachePath == right.cachePath &&
    left.size == right.size &&
    left.modifiedSeconds == right.modifiedSeconds &&
    left.modifiedSubsecond == right.modifiedSubsecond;
}

void publish(
  const std::shared_ptr<const CachedImage>& image,
  DecodedImage& output) noexcept
{
  output = {};
  if (!image || !image->storage || image->storage->empty()) return;
  output.storage = image->storage;
  output.pixels = output.storage->data();
  output.width = image->width;
  output.height = image->height;
  output.stride = image->stride;
}

bool checkedLayout(
  int width,
  int height,
  int& stride,
  std::size_t& byteCount) noexcept
{
  if (width <= 0 || height <= 0 ||
      width > std::numeric_limits<int>::max() / 4) {
    return false;
  }
  stride = width * 4;
  const std::size_t rowBytes =
    static_cast<std::size_t>(stride);
  if (static_cast<std::size_t>(height) >
      std::numeric_limits<std::size_t>::max() / rowBytes) {
    return false;
  }
  byteCount = rowBytes * static_cast<std::size_t>(height);
  return true;
}

bool fullHdOutputSize(
  std::uint64_t sourceWidth,
  std::uint64_t sourceHeight,
  int& outputWidth,
  int& outputHeight) noexcept
{
  if (sourceWidth == 0 || sourceHeight == 0) return false;
  const bool portrait = sourceHeight > sourceWidth;
  const double maximumWidth = portrait ? 1080.0 : 1920.0;
  const double maximumHeight = portrait ? 1920.0 : 1080.0;
  const double scale = std::min(
    1.0,
    std::min(
      maximumWidth / static_cast<double>(sourceWidth),
      maximumHeight / static_cast<double>(sourceHeight)));
  const double scaledWidth =
    static_cast<double>(sourceWidth) * scale;
  const double scaledHeight =
    static_cast<double>(sourceHeight) * scale;
  outputWidth = std::max(
    1,
    std::min(
      static_cast<int>(maximumWidth),
      static_cast<int>(std::floor(scaledWidth + 0.5))));
  outputHeight = std::max(
    1,
    std::min(
      static_cast<int>(maximumHeight),
      static_cast<int>(std::floor(scaledHeight + 0.5))));
  return true;
}

bool lookupCache(
  const CacheKey& key,
  DecodedImage& output,
  std::uint64_t& generation)
{
  std::lock_guard<std::mutex> lock(g_cache.mutex);
  generation = g_cache.generation;
  const auto found = g_cache.entries.find(key);
  if (found == g_cache.entries.end()) return false;
  found->second.lastUse = ++g_cache.clock;
  publish(found->second.image, output);
  return static_cast<bool>(output);
}

void evictCacheLocked()
{
  while (g_cache.entries.size() > kMaximumCacheEntries ||
         g_cache.byteCount > kMaximumCacheBytes) {
    auto oldest = g_cache.entries.end();
    for (auto candidate = g_cache.entries.begin();
         candidate != g_cache.entries.end(); ++candidate) {
      if (oldest == g_cache.entries.end() ||
          candidate->second.lastUse < oldest->second.lastUse) {
        oldest = candidate;
      }
    }
    if (oldest == g_cache.entries.end()) break;
    g_cache.byteCount -= std::min(
      g_cache.byteCount, oldest->second.byteCount);
    g_cache.entries.erase(oldest);
  }
}

std::shared_ptr<const CachedImage> insertCache(
  const CacheKey& key,
  const std::shared_ptr<const CachedImage>& decoded,
  std::uint64_t observedGeneration)
{
  std::lock_guard<std::mutex> lock(g_cache.mutex);
  const auto existing = g_cache.entries.find(key);
  if (existing != g_cache.entries.end()) {
    existing->second.lastUse = ++g_cache.clock;
    return existing->second.image;
  }
  // invalidate()/clearCache() racing with a decode must not be undone by the
  // decode that began before the invalidation.
  if (observedGeneration != g_cache.generation) return decoded;

  for (auto entry = g_cache.entries.begin();
       entry != g_cache.entries.end();) {
    if (entry->first.path == key.path) {
      g_cache.byteCount -= std::min(
        g_cache.byteCount, entry->second.byteCount);
      entry = g_cache.entries.erase(entry);
    } else {
      ++entry;
    }
  }

  const std::size_t bytes =
    decoded && decoded->storage ? decoded->storage->size() : 0;
  CacheEntry entry;
  entry.image = decoded;
  entry.byteCount = bytes;
  entry.lastUse = ++g_cache.clock;
  g_cache.entries.emplace(key, std::move(entry));
  g_cache.byteCount += bytes;
  evictCacheLocked();
  return decoded;
}

#ifdef _WIN32

using Microsoft::WRL::ComPtr;

bool utf8ToWide(
  const std::string& value,
  std::wstring& output) noexcept
{
  output.clear();
  if (value.empty()) return false;
  const int count = MultiByteToWideChar(
    CP_UTF8, MB_ERR_INVALID_CHARS,
    value.data(), static_cast<int>(value.size()),
    nullptr, 0);
  if (count <= 0) return false;
  try {
    output.resize(static_cast<std::size_t>(count));
  } catch (...) {
    return false;
  }
  return MultiByteToWideChar(
    CP_UTF8, MB_ERR_INVALID_CHARS,
    value.data(), static_cast<int>(value.size()),
    output.data(), count) == count;
}

bool wideToUtf8(
  const std::wstring& value,
  std::string& output) noexcept
{
  output.clear();
  if (value.empty()) return false;
  const int count = WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS,
    value.data(), static_cast<int>(value.size()),
    nullptr, 0, nullptr, nullptr);
  if (count <= 0) return false;
  try {
    output.resize(static_cast<std::size_t>(count));
  } catch (...) {
    return false;
  }
  return WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS,
    value.data(), static_cast<int>(value.size()),
    output.data(), count, nullptr, nullptr) == count;
}

bool normalizedWindowsPath(
  const std::string& utf8Path,
  std::wstring& nativePath,
  std::string& cachePath) noexcept
{
  std::wstring widePath;
  if (!utf8ToWide(utf8Path, widePath)) return false;
  const DWORD required = GetFullPathNameW(
    widePath.c_str(), 0, nullptr, nullptr);
  if (required == 0) return false;
  std::wstring absolute;
  try {
    absolute.resize(static_cast<std::size_t>(required));
  } catch (...) {
    return false;
  }
  const DWORD written = GetFullPathNameW(
    widePath.c_str(), required, absolute.data(), nullptr);
  if (written == 0 || written >= required) return false;
  absolute.resize(static_cast<std::size_t>(written));
  if (!wideToUtf8(absolute, cachePath)) return false;
  nativePath = std::move(absolute);
  return true;
}

bool readFileIdentity(
  const std::string& utf8Path,
  FileIdentity& output) noexcept
{
  output = {};
  if (!normalizedWindowsPath(
        utf8Path, output.nativePath, output.cachePath)) {
    return false;
  }
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  if (!GetFileAttributesExW(
        output.nativePath.c_str(),
        GetFileExInfoStandard, &attributes) ||
      (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    output = {};
    return false;
  }
  output.size =
    (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) |
    static_cast<std::uint64_t>(attributes.nFileSizeLow);
  const std::uint64_t ticks =
    (static_cast<std::uint64_t>(
       attributes.ftLastWriteTime.dwHighDateTime) << 32) |
    static_cast<std::uint64_t>(
      attributes.ftLastWriteTime.dwLowDateTime);
  output.modifiedSeconds = static_cast<std::int64_t>(
    ticks / 10000000ull);
  output.modifiedSubsecond = static_cast<std::uint32_t>(
    ticks % 10000000ull);
  return true;
}

class ComApartment {
public:
  ComApartment() noexcept
    : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
  {
    uninitialize_ = result_ == S_OK || result_ == S_FALSE;
  }

  ~ComApartment()
  {
    if (uninitialize_) CoUninitialize();
  }

  bool available() const noexcept
  {
    // A UI may already own a single-threaded apartment. COM is usable there;
    // this call simply cannot change its apartment model.
    return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
  }

private:
  HRESULT result_ = E_FAIL;
  bool uninitialize_ = false;
};

unsigned int exifOrientation(
  IWICBitmapFrameDecode* frame) noexcept
{
  if (!frame) return 1;
  ComPtr<IWICMetadataQueryReader> metadata;
  if (FAILED(frame->GetMetadataQueryReader(&metadata)) || !metadata) {
    return 1;
  }
  const wchar_t* queries[] = {
    L"/app1/ifd/{ushort=274}",
    L"/ifd/{ushort=274}",
  };
  for (const wchar_t* query : queries) {
    PROPVARIANT value;
    PropVariantInit(&value);
    const HRESULT result =
      metadata->GetMetadataByName(query, &value);
    unsigned int orientation = 1;
    if (SUCCEEDED(result)) {
      if (value.vt == VT_UI1) orientation = value.bVal;
      else if (value.vt == VT_UI2) orientation = value.uiVal;
      else if (value.vt == VT_UI4) orientation = value.ulVal;
      else if (value.vt == VT_I2 && value.iVal > 0) {
        orientation = static_cast<unsigned int>(value.iVal);
      } else if (value.vt == VT_I4 && value.lVal > 0) {
        orientation = static_cast<unsigned int>(value.lVal);
      }
    }
    PropVariantClear(&value);
    if (SUCCEEDED(result) && orientation >= 1 && orientation <= 8) {
      return orientation;
    }
  }
  return 1;
}

WICBitmapTransformOptions transformForOrientation(
  unsigned int orientation) noexcept
{
  switch (orientation) {
    case 2: return WICBitmapTransformFlipHorizontal;
    case 3: return WICBitmapTransformRotate180;
    case 4: return WICBitmapTransformFlipVertical;
    // IWICBitmapFlipRotator applies the flip before the rotation. EXIF 5 is
    // the main-diagonal transpose; EXIF 7 is the anti-diagonal transpose.
    case 5: return static_cast<WICBitmapTransformOptions>(
      WICBitmapTransformRotate270 |
      WICBitmapTransformFlipHorizontal);
    case 6: return WICBitmapTransformRotate90;
    case 7: return static_cast<WICBitmapTransformOptions>(
      WICBitmapTransformRotate90 |
      WICBitmapTransformFlipHorizontal);
    case 8: return WICBitmapTransformRotate270;
    default: return WICBitmapTransformRotate0;
  }
}

DecodeStatus statusForWicFailure(HRESULT result) noexcept
{
  if (result == WINCODEC_ERR_UNKNOWNIMAGEFORMAT ||
      result == WINCODEC_ERR_COMPONENTNOTFOUND) {
    return DecodeStatus::unsupportedFormat;
  }
  return DecodeStatus::decodeFailed;
}

std::shared_ptr<const CachedImage> decodeNative(
  const FileIdentity& identity,
  DecodeStatus& status)
{
  ComApartment apartment;
  if (!apartment.available()) {
    status = DecodeStatus::decodeFailed;
    return {};
  }

  ComPtr<IWICImagingFactory> factory;
  HRESULT result = CoCreateInstance(
    CLSID_WICImagingFactory, nullptr,
    CLSCTX_INPROC_SERVER,
    IID_PPV_ARGS(&factory));
  if (FAILED(result) || !factory) {
    status = statusForWicFailure(result);
    return {};
  }

  ComPtr<IWICBitmapDecoder> decoder;
  result = factory->CreateDecoderFromFilename(
    identity.nativePath.c_str(), nullptr, GENERIC_READ,
    WICDecodeMetadataCacheOnLoad, &decoder);
  if (FAILED(result) || !decoder) {
    status = statusForWicFailure(result);
    return {};
  }

  UINT frameCount = 0;
  result = decoder->GetFrameCount(&frameCount);
  if (FAILED(result) || frameCount == 0) {
    status = DecodeStatus::decodeFailed;
    return {};
  }

  ComPtr<IWICBitmapFrameDecode> frame;
  result = decoder->GetFrame(0, &frame);
  if (FAILED(result) || !frame) {
    status = statusForWicFailure(result);
    return {};
  }

  ComPtr<IWICBitmapSource> source;
  result = frame.As(&source);
  if (FAILED(result) || !source) {
    status = DecodeStatus::decodeFailed;
    return {};
  }

  const WICBitmapTransformOptions transform =
    transformForOrientation(exifOrientation(frame.Get()));
  if (transform != WICBitmapTransformRotate0) {
    ComPtr<IWICBitmapFlipRotator> rotator;
    result = factory->CreateBitmapFlipRotator(&rotator);
    if (FAILED(result) || !rotator) {
      status = DecodeStatus::decodeFailed;
      return {};
    }
    result = rotator->Initialize(source.Get(), transform);
    if (FAILED(result)) {
      status = DecodeStatus::decodeFailed;
      return {};
    }
    ComPtr<IWICBitmapSource> rotated;
    result = rotator.As(&rotated);
    if (FAILED(result) || !rotated) {
      status = DecodeStatus::decodeFailed;
      return {};
    }
    source = std::move(rotated);
  }

  UINT sourceWidth = 0;
  UINT sourceHeight = 0;
  result = source->GetSize(&sourceWidth, &sourceHeight);
  if (FAILED(result) || sourceWidth == 0 || sourceHeight == 0) {
    status = DecodeStatus::decodeFailed;
    return {};
  }

  int width = 0;
  int height = 0;
  if (!fullHdOutputSize(
        sourceWidth, sourceHeight, width, height)) {
    status = DecodeStatus::decodeFailed;
    return {};
  }
  if (sourceWidth != static_cast<UINT>(width) ||
      sourceHeight != static_cast<UINT>(height)) {
    ComPtr<IWICBitmapScaler> scaler;
    result = factory->CreateBitmapScaler(&scaler);
    if (FAILED(result) || !scaler) {
      status = DecodeStatus::decodeFailed;
      return {};
    }
    result = scaler->Initialize(
      source.Get(), static_cast<UINT>(width),
      static_cast<UINT>(height), WICBitmapInterpolationModeFant);
    if (FAILED(result)) {
      status = DecodeStatus::decodeFailed;
      return {};
    }
    ComPtr<IWICBitmapSource> scaled;
    result = scaler.As(&scaled);
    if (FAILED(result) || !scaled) {
      status = DecodeStatus::decodeFailed;
      return {};
    }
    source = std::move(scaled);
  }

  ComPtr<IWICFormatConverter> converter;
  result = factory->CreateFormatConverter(&converter);
  if (FAILED(result) || !converter) {
    status = DecodeStatus::decodeFailed;
    return {};
  }
  result = converter->Initialize(
    source.Get(), GUID_WICPixelFormat32bppPBGRA,
    WICBitmapDitherTypeNone, nullptr, 0.0,
    WICBitmapPaletteTypeCustom);
  if (FAILED(result)) {
    status = statusForWicFailure(result);
    return {};
  }

  int stride = 0;
  std::size_t byteCount = 0;
  if (!checkedLayout(width, height, stride, byteCount) ||
      byteCount > std::numeric_limits<UINT>::max()) {
    status = DecodeStatus::decodeFailed;
    return {};
  }
  auto pixels =
    std::make_shared<std::vector<std::uint8_t>>(byteCount);
  result = converter->CopyPixels(
    nullptr, static_cast<UINT>(stride),
    static_cast<UINT>(byteCount), pixels->data());
  if (FAILED(result)) {
    status = statusForWicFailure(result);
    return {};
  }

  auto image = std::make_shared<CachedImage>();
  image->storage = std::move(pixels);
  image->width = width;
  image->height = height;
  image->stride = stride;
  status = DecodeStatus::ok;
  return image;
}

#elif defined(__APPLE__)

bool readFileIdentity(
  const std::string& utf8Path,
  FileIdentity& output) noexcept
{
  output = {};
  if (utf8Path.empty()) return false;
  struct stat information{};
  if (stat(utf8Path.c_str(), &information) != 0 ||
      !S_ISREG(information.st_mode) || information.st_size < 0) {
    return false;
  }
  try {
    output.cachePath = utf8Path;
    output.nativePath = utf8Path;
  } catch (...) {
    output = {};
    return false;
  }
  output.size = static_cast<std::uint64_t>(information.st_size);
  output.modifiedSeconds = static_cast<std::int64_t>(
    information.st_mtimespec.tv_sec);
  output.modifiedSubsecond = static_cast<std::uint32_t>(
    information.st_mtimespec.tv_nsec);
  return true;
}

std::uint64_t imagePropertyInteger(
  CFDictionaryRef properties,
  CFStringRef key) noexcept
{
  if (!properties || !key) return 0;
  const auto value = static_cast<CFTypeRef>(
    CFDictionaryGetValue(properties, key));
  if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) return 0;
  std::int64_t integer = 0;
  if (!CFNumberGetValue(
        static_cast<CFNumberRef>(value),
        kCFNumberSInt64Type, &integer) || integer <= 0) {
    return 0;
  }
  return static_cast<std::uint64_t>(integer);
}

std::shared_ptr<const CachedImage> decodeNative(
  const FileIdentity& identity,
  DecodeStatus& status)
{
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
    kCFAllocatorDefault,
    reinterpret_cast<const UInt8*>(identity.nativePath.data()),
    static_cast<CFIndex>(identity.nativePath.size()), false);
  if (!url) {
    status = DecodeStatus::fileUnavailable;
    return {};
  }
  CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
  CFRelease(url);
  if (!source) {
    status = DecodeStatus::unsupportedFormat;
    return {};
  }
  if (CGImageSourceGetCount(source) == 0 ||
      !CGImageSourceGetType(source)) {
    CFRelease(source);
    status = DecodeStatus::unsupportedFormat;
    return {};
  }

  CFDictionaryRef properties =
    CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr);
  std::uint64_t sourceWidth = imagePropertyInteger(
    properties, kCGImagePropertyPixelWidth);
  std::uint64_t sourceHeight = imagePropertyInteger(
    properties, kCGImagePropertyPixelHeight);
  const std::uint64_t orientation = imagePropertyInteger(
    properties, kCGImagePropertyOrientation);
  if (properties) CFRelease(properties);
  if (orientation >= 5 && orientation <= 8) {
    std::swap(sourceWidth, sourceHeight);
  }
  int requestedWidth = 0;
  int requestedHeight = 0;
  if (!fullHdOutputSize(
        sourceWidth, sourceHeight,
        requestedWidth, requestedHeight)) {
    CFRelease(source);
    status = DecodeStatus::decodeFailed;
    return {};
  }

  const int maximumPixelSize =
    std::max(requestedWidth, requestedHeight);
  CFNumberRef maximumPixelNumber = CFNumberCreate(
    kCFAllocatorDefault, kCFNumberIntType, &maximumPixelSize);
  CFMutableDictionaryRef options = CFDictionaryCreateMutable(
    kCFAllocatorDefault, 4,
    &kCFTypeDictionaryKeyCallBacks,
    &kCFTypeDictionaryValueCallBacks);
  if (!maximumPixelNumber || !options) {
    if (maximumPixelNumber) CFRelease(maximumPixelNumber);
    if (options) CFRelease(options);
    CFRelease(source);
    status = DecodeStatus::outOfMemory;
    return {};
  }
  CFDictionarySetValue(
    options, kCGImageSourceCreateThumbnailFromImageAlways,
    kCFBooleanTrue);
  CFDictionarySetValue(
    options, kCGImageSourceCreateThumbnailWithTransform,
    kCFBooleanTrue);
  CFDictionarySetValue(
    options, kCGImageSourceShouldCacheImmediately,
    kCFBooleanTrue);
  CFDictionarySetValue(
    options, kCGImageSourceThumbnailMaxPixelSize,
    maximumPixelNumber);
  CGImageRef thumbnail = CGImageSourceCreateThumbnailAtIndex(
    source, 0, options);
  CFRelease(options);
  CFRelease(maximumPixelNumber);
  CFRelease(source);
  if (!thumbnail) {
    status = DecodeStatus::decodeFailed;
    return {};
  }

  const std::size_t thumbnailWidth = CGImageGetWidth(thumbnail);
  const std::size_t thumbnailHeight = CGImageGetHeight(thumbnail);
  int width = 0;
  int height = 0;
  if (!fullHdOutputSize(
        thumbnailWidth, thumbnailHeight, width, height)) {
    CGImageRelease(thumbnail);
    status = DecodeStatus::decodeFailed;
    return {};
  }
  int stride = 0;
  std::size_t byteCount = 0;
  if (!checkedLayout(width, height, stride, byteCount)) {
    CGImageRelease(thumbnail);
    status = DecodeStatus::decodeFailed;
    return {};
  }

  auto pixels =
    std::make_shared<std::vector<std::uint8_t>>(byteCount, 0);
  CGColorSpaceRef colorSpace =
    CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  if (!colorSpace) colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = colorSpace
    ? CGBitmapContextCreate(
        pixels->data(), static_cast<std::size_t>(width),
        static_cast<std::size_t>(height), 8,
        static_cast<std::size_t>(stride), colorSpace,
        kCGBitmapByteOrder32Little |
          kCGImageAlphaPremultipliedFirst)
    : nullptr;
  if (colorSpace) CGColorSpaceRelease(colorSpace);
  if (!context) {
    CGImageRelease(thumbnail);
    status = DecodeStatus::outOfMemory;
    return {};
  }
  CGContextSetBlendMode(context, kCGBlendModeCopy);
  CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
  // Quartz usa o eixo Y crescente para cima. Sem esta CTM a primeira linha
  // do CGBitmapContext corresponde ao rodape visual. A API publica sempre a
  // primeira linha da memoria como o topo (igual ao WIC/CopyPixels).
  CGContextTranslateCTM(
    context, 0.0, static_cast<CGFloat>(height));
  CGContextScaleCTM(context, 1.0, -1.0);
  CGContextDrawImage(
    context,
    CGRectMake(
      0.0, 0.0,
      static_cast<CGFloat>(width),
      static_cast<CGFloat>(height)),
    thumbnail);
  CGContextFlush(context);
  CGContextRelease(context);
  CGImageRelease(thumbnail);

  auto image = std::make_shared<CachedImage>();
  image->storage = std::move(pixels);
  image->width = width;
  image->height = height;
  image->stride = stride;
  status = DecodeStatus::ok;
  return image;
}

#else

bool readFileIdentity(
  const std::string&,
  FileIdentity&) noexcept
{
  return false;
}

std::shared_ptr<const CachedImage> decodeNative(
  const FileIdentity&,
  DecodeStatus& status)
{
  status = DecodeStatus::unsupportedPlatform;
  return {};
}

#endif

std::string normalizedCachePath(const std::string& utf8Path)
{
#ifdef _WIN32
  std::wstring nativePath;
  std::string cachePath;
  if (normalizedWindowsPath(utf8Path, nativePath, cachePath)) {
    return cachePath;
  }
#endif
  return utf8Path;
}

} // namespace

bool decodeFile(
  const std::string& utf8Path,
  DecodedImage& output,
  DecodeStatus* status) noexcept
{
  output = {};
  setStatus(status, DecodeStatus::invalidArgument);
  if (utf8Path.empty()) return false;

  try {
#if !defined(_WIN32) && !defined(__APPLE__)
    setStatus(status, DecodeStatus::unsupportedPlatform);
    return false;
#else
    FileIdentity identity;
    if (!readFileIdentity(utf8Path, identity)) {
      setStatus(status, DecodeStatus::fileUnavailable);
      return false;
    }

    // A segunda passagem cobre uma substituicao atomica do arquivo enquanto
    // o decoder lia a primeira versao. Nunca associa pixels antigos ao mtime
    // novo nem grava esse resultado sob uma chave incorreta.
    for (int attempt = 0; attempt < 2; ++attempt) {
      const CacheKey key = cacheKey(identity);
      std::uint64_t observedGeneration = 0;
      if (lookupCache(key, output, observedGeneration)) {
        setStatus(status, DecodeStatus::ok);
        return true;
      }

      DecodeStatus nativeStatus = DecodeStatus::decodeFailed;
      std::shared_ptr<const CachedImage> decoded =
        decodeNative(identity, nativeStatus);
      if (!decoded) {
        setStatus(status, nativeStatus);
        return false;
      }

      FileIdentity afterDecode;
      if (!readFileIdentity(utf8Path, afterDecode)) {
        setStatus(status, DecodeStatus::fileUnavailable);
        return false;
      }
      if (!sameFileVersion(identity, afterDecode)) {
        identity = std::move(afterDecode);
        continue;
      }

      // Cache allocation failure cannot invalidate pixels already decoded.
      // Publish the immutable snapshot even if there is no room for its key.
      std::shared_ptr<const CachedImage> published = decoded;
      try {
        published = insertCache(
          key, decoded, observedGeneration);
      } catch (...) {
        published = decoded;
      }
      publish(published, output);
      if (!output) {
        setStatus(status, DecodeStatus::decodeFailed);
        return false;
      }
      setStatus(status, DecodeStatus::ok);
      return true;
    }
    setStatus(status, DecodeStatus::fileChangedDuringDecode);
    return false;
#endif
  } catch (const std::bad_alloc&) {
    output = {};
    setStatus(status, DecodeStatus::outOfMemory);
    return false;
  } catch (...) {
    output = {};
    setStatus(status, DecodeStatus::decodeFailed);
    return false;
  }
}

void invalidate(const std::string& utf8Path) noexcept
{
  if (utf8Path.empty()) return;
  try {
    const std::string path = normalizedCachePath(utf8Path);
    std::lock_guard<std::mutex> lock(g_cache.mutex);
    ++g_cache.generation;
    for (auto entry = g_cache.entries.begin();
         entry != g_cache.entries.end();) {
      if (entry->first.path == path) {
        g_cache.byteCount -= std::min(
          g_cache.byteCount, entry->second.byteCount);
        entry = g_cache.entries.erase(entry);
      } else {
        ++entry;
      }
    }
  } catch (...) {
    // Invalidation is best-effort and must stay safe during teardown.
  }
}

void clearCache() noexcept
{
  try {
    std::lock_guard<std::mutex> lock(g_cache.mutex);
    ++g_cache.generation;
    g_cache.entries.clear();
    g_cache.byteCount = 0;
  } catch (...) {
    // std::mutex::lock only throws on an invalid mutex; never escape unload.
  }
}

} // namespace vshook_static_image
