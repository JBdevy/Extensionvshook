#include "native_video_decoder.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

namespace vshook_video {

using Microsoft::WRL::ComPtr;

namespace {

std::wstring utf8ToWide(const std::string& value)
{
  if (value.empty()) return {};
  const int count = MultiByteToWideChar(
    CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (count <= 1) return {};
  std::wstring wide(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(
    CP_UTF8, 0, value.c_str(), -1, wide.data(), count);
  if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
  return wide;
}

double hnsToSeconds(LONGLONG value)
{
  return static_cast<double>(value) / 10000000.0;
}

LONGLONG secondsToHns(double value)
{
  const double safe = std::max(0.0, value);
  return static_cast<LONGLONG>(
    std::llround(safe * 10000000.0));
}

struct BilinearAxisSample {
  int first = 0;
  int second = 0;
  int weight = 0;
};

bool checkedSizeProduct(
  std::size_t left,
  std::size_t right,
  std::size_t& result)
{
  if (left != 0 &&
      right > std::numeric_limits<std::size_t>::max() / left) {
    return false;
  }
  result = left * right;
  return true;
}

bool checkedSizeSum(
  std::size_t left,
  std::size_t right,
  std::size_t& result)
{
  if (right >
      std::numeric_limits<std::size_t>::max() - left) {
    return false;
  }
  result = left + right;
  return true;
}

bool checkedBgraLayout(
  int width,
  int height,
  std::size_t& stride,
  std::size_t& byteCount)
{
  if (width <= 0 || height <= 0 ||
      width > std::numeric_limits<int>::max() / 4) {
    return false;
  }
  stride = static_cast<std::size_t>(width) * 4u;
  return checkedSizeProduct(
    stride, static_cast<std::size_t>(height), byteCount);
}

void buildBilinearAxis(
  int sourceSize,
  int outputSize,
  std::vector<BilinearAxisSample>& samples)
{
  samples.resize(static_cast<std::size_t>(outputSize));
  if (sourceSize <= 1) {
    std::fill(
      samples.begin(), samples.end(), BilinearAxisSample{});
    return;
  }

  for (int outputIndex = 0;
       outputIndex < outputSize;
       ++outputIndex) {
    const double sourcePosition =
      (static_cast<double>(outputIndex) + 0.5) *
        static_cast<double>(sourceSize) /
        static_cast<double>(outputSize) -
      0.5;
    int first =
      static_cast<int>(std::floor(sourcePosition));
    double fraction = sourcePosition - first;
    if (first < 0) {
      first = 0;
      fraction = 0.0;
    } else if (first >= sourceSize - 1) {
      first = sourceSize - 1;
      fraction = 0.0;
    }
    const int second = std::min(sourceSize - 1, first + 1);
    samples[static_cast<std::size_t>(outputIndex)] = {
      first,
      second,
      std::max(
        0,
        std::min(
          256,
          static_cast<int>(
            std::lround(fraction * 256.0))))
    };
  }
}

int bilinearByte(
  int topLeft,
  int topRight,
  int bottomLeft,
  int bottomRight,
  int horizontalWeight,
  int verticalWeight)
{
  const int inverseHorizontal = 256 - horizontalWeight;
  const int inverseVertical = 256 - verticalWeight;
  const int top =
    topLeft * inverseHorizontal +
    topRight * horizontalWeight;
  const int bottom =
    bottomLeft * inverseHorizontal +
    bottomRight * horizontalWeight;
  return (
    top * inverseVertical +
    bottom * verticalWeight +
    32768) >> 16;
}

} // namespace

struct Decoder::Impl {
  struct PlaybackRequest {
    std::string path;
    std::string playbackKey;
    double sourceTime = 0.0;
    double playbackRate = 1.0;
    int requestedWidth = 1;
    int requestedHeight = 1;
    bool playing = false;
    std::uint64_t serial = 0;
    std::chrono::steady_clock::time_point sampledAt =
      std::chrono::steady_clock::now();
  };

  // IMFSourceReader e todos os objetos do Media Foundation pertencem somente
  // à worker. A thread do REAPER apenas publica o relógio desejado e lê um
  // snapshot imutável do último quadro pronto.
  std::mutex stateMutex;
  std::condition_variable stateChanged;
  std::thread worker;
  PlaybackRequest request;
  bool hasRequest = false;
  bool stopRequested = false;
  std::shared_ptr<const std::vector<std::uint8_t>> publishedPixels;
  std::string publishedPath;
  std::string publishedPlaybackKey;
  int publishedWidth = 0;
  int publishedHeight = 0;
  int publishedStride = 0;
  double publishedTimestamp = -1.0;
  bool hasObservedClock = false;
  std::string lastObservedPath;
  std::string lastObservedPlaybackKey;
  double lastObservedSourceTime = 0.0;
  double lastObservedPlaybackRate = 1.0;
  bool lastObservedPlaying = false;
  std::chrono::steady_clock::time_point lastObservedAt =
    std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point lastClockCorrectionAt =
    std::chrono::steady_clock::time_point::min();

  ComPtr<IMFSourceReader> reader;
  ComPtr<ID3D11Device> d3dDevice;
  ComPtr<ID3D11DeviceContext> d3dContext;
  ComPtr<IMFDXGIDeviceManager> dxgiDeviceManager;
  ComPtr<ID3D11VideoDevice> videoDevice;
  ComPtr<ID3D11VideoContext> videoContext;
  ComPtr<ID3D11VideoProcessorEnumerator> videoProcessorEnumerator;
  ComPtr<ID3D11VideoProcessor> videoProcessor;
  ComPtr<ID3D11Texture2D> videoOutputTexture;
  ComPtr<ID3D11Texture2D> videoReadbackTexture;
  ComPtr<ID3D11VideoProcessorOutputView> videoOutputView;
  UINT dxgiResetToken = 0;
  int gpuInputWidth = 0;
  int gpuInputHeight = 0;
  int gpuOutputWidth = 0;
  int gpuOutputHeight = 0;
  UINT frameRateNumerator = 30;
  UINT frameRateDenominator = 1;
  bool d3dAvailable = false;
  bool readerUsesD3D = false;
  std::string forceSoftwarePath;
  std::string path;
  std::vector<std::uint8_t> pixels;
  int sourceWidth = 0;
  int sourceHeight = 0;
  int outputWidth = 0;
  int outputHeight = 0;
  bool outputIsNv12 = false;
  std::vector<BilinearAxisSample> sourceXForOutput;
  std::vector<BilinearAxisSample> sourceYForOutput;
  int yScaleTable[256]{};
  int uBlueTable[256]{};
  int uGreenTable[256]{};
  int vRedTable[256]{};
  int vGreenTable[256]{};
  LONG defaultStride = 0;
  double frameDuration = 1.0 / 30.0;
  double frameTimestamp = -1.0;
  bool hasFrame = false;
  bool endOfStream = false;
  bool comInitialized = false;
  bool mediaFoundationStarted = false;
  std::atomic<int> statusCode{0};

  Impl()
  {
    for (int value = 0; value < 256; ++value) {
      yScaleTable[value] =
        298 * std::max(0, value - 16);
      const int chroma = value - 128;
      uBlueTable[value] = 516 * chroma;
      uGreenTable[value] = -100 * chroma;
      vRedTable[value] = 409 * chroma;
      vGreenTable[value] = -208 * chroma;
    }
    worker = std::thread([this]() { workerLoop(); });
  }

  ~Impl()
  {
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      stopRequested = true;
    }
    stateChanged.notify_all();
    if (worker.joinable()) worker.join();
  }

  void clearGpuProcessor()
  {
    videoOutputView.Reset();
    videoReadbackTexture.Reset();
    videoOutputTexture.Reset();
    videoProcessor.Reset();
    videoProcessorEnumerator.Reset();
    gpuInputWidth = 0;
    gpuInputHeight = 0;
    gpuOutputWidth = 0;
    gpuOutputHeight = 0;
  }

  bool initializeD3D()
  {
    if (d3dAvailable &&
        d3dDevice && d3dContext &&
        dxgiDeviceManager &&
        videoDevice && videoContext) {
      return true;
    }

    clearGpuProcessor();
    videoContext.Reset();
    videoDevice.Reset();
    dxgiDeviceManager.Reset();
    d3dContext.Reset();
    d3dDevice.Reset();
    d3dAvailable = false;
    dxgiResetToken = 0;

    constexpr UINT creationFlags =
      D3D11_CREATE_DEVICE_BGRA_SUPPORT |
      D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    const D3D_FEATURE_LEVEL requestedLevels[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selectedLevel =
      D3D_FEATURE_LEVEL_10_0;
    HRESULT result = D3D11CreateDevice(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      creationFlags,
      requestedLevels,
      static_cast<UINT>(
        sizeof(requestedLevels) /
        sizeof(requestedLevels[0])),
      D3D11_SDK_VERSION,
      &d3dDevice,
      &selectedLevel,
      &d3dContext);
    if (result == E_INVALIDARG) {
      // Windows 7 sem o Platform Update não reconhece 11.1 na lista.
      result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        requestedLevels + 1,
        static_cast<UINT>(
          sizeof(requestedLevels) /
          sizeof(requestedLevels[0]) - 1),
        D3D11_SDK_VERSION,
        &d3dDevice,
        &selectedLevel,
        &d3dContext);
    }
    if (FAILED(result) || !d3dDevice || !d3dContext) {
      return false;
    }

    result = d3dDevice.As(&videoDevice);
    if (FAILED(result) || !videoDevice) {
      d3dContext.Reset();
      d3dDevice.Reset();
      return false;
    }
    result = d3dContext.As(&videoContext);
    if (FAILED(result) || !videoContext) {
      videoDevice.Reset();
      d3dContext.Reset();
      d3dDevice.Reset();
      return false;
    }

    result = MFCreateDXGIDeviceManager(
      &dxgiResetToken, &dxgiDeviceManager);
    if (FAILED(result) || !dxgiDeviceManager) {
      videoContext.Reset();
      videoDevice.Reset();
      d3dContext.Reset();
      d3dDevice.Reset();
      return false;
    }
    result = dxgiDeviceManager->ResetDevice(
      d3dDevice.Get(), dxgiResetToken);
    if (FAILED(result)) {
      dxgiDeviceManager.Reset();
      videoContext.Reset();
      videoDevice.Reset();
      d3dContext.Reset();
      d3dDevice.Reset();
      return false;
    }

    d3dAvailable = true;
    return true;
  }

  void clearReader()
  {
    reader.Reset();
    clearGpuProcessor();
    readerUsesD3D = false;
    path.clear();
    pixels.clear();
    sourceWidth = 0;
    sourceHeight = 0;
    outputWidth = 0;
    outputHeight = 0;
    outputIsNv12 = false;
    sourceXForOutput.clear();
    sourceYForOutput.clear();
    defaultStride = 0;
    frameDuration = 1.0 / 30.0;
    frameRateNumerator = 30;
    frameRateDenominator = 1;
    frameTimestamp = -1.0;
    hasFrame = false;
    endOfStream = false;
  }

  bool configureOutputSize(
    int requestedWidth,
    int requestedHeight)
  {
    if (sourceWidth <= 0 || sourceHeight <= 0) return false;
    const bool portraitSource = sourceHeight > sourceWidth;
    const int maximumOutputWidth =
      portraitSource ? 1080 : 1920;
    const int maximumOutputHeight =
      portraitSource ? 1920 : 1080;
    const int safeRequestedWidth = std::min(
      maximumOutputWidth,
      std::max(1, requestedWidth));
    const int safeRequestedHeight = std::min(
      maximumOutputHeight,
      std::max(1, requestedHeight));
    const double requestedScale = std::min(
      1.0,
      std::min(
        static_cast<double>(safeRequestedWidth) /
          static_cast<double>(sourceWidth),
        static_cast<double>(safeRequestedHeight) /
          static_cast<double>(sourceHeight)));

    // A saída BGRA acompanha os pixels que realmente serão desenhados. O
    // maior eixo sobe em pequenos degraus para uma sequência de WM_SIZE não
    // recriar texturas a cada pixel. O teto do teleprompt é Full HD, orientado
    // conforme a fonte, sem reintroduzir a antiga redução fixa para 960x540.
    double outputScale = requestedScale;
    if (requestedScale < 1.0) {
      const int sourceLongEdge =
        std::max(sourceWidth, sourceHeight);
      const double requestedLongEdge =
        sourceLongEdge * requestedScale;
      const int quantum =
        requestedLongEdge >= 1024.0 ? 16 :
        requestedLongEdge >= 256.0 ? 8 :
        requestedLongEdge >= 64.0 ? 4 : 2;
      const double quantizedLongEdge =
        std::max(
          1.0,
          std::floor(requestedLongEdge / quantum) * quantum);
      outputScale = std::min(
        requestedScale,
        quantizedLongEdge /
          static_cast<double>(sourceLongEdge));
    }

    const int wantedWidth = std::max(
      1,
      std::min(
        std::min(sourceWidth, safeRequestedWidth),
        static_cast<int>(
          std::lround(sourceWidth * outputScale))));
    const int wantedHeight = std::max(
      1,
      std::min(
        std::min(sourceHeight, safeRequestedHeight),
        static_cast<int>(
          std::lround(sourceHeight * outputScale))));
    if (wantedWidth == outputWidth &&
        wantedHeight == outputHeight) {
      return true;
    }

    std::size_t destinationStride = 0;
    std::size_t destinationBytes = 0;
    if (!checkedBgraLayout(
          wantedWidth,
          wantedHeight,
          destinationStride,
          destinationBytes)) {
      statusCode = -106;
      return false;
    }

    std::vector<std::uint8_t> resizedPixels;
    std::vector<BilinearAxisSample> resizedX;
    std::vector<BilinearAxisSample> resizedY;
    if (destinationBytes > resizedPixels.max_size() ||
        static_cast<std::size_t>(wantedWidth) >
          resizedX.max_size() ||
        static_cast<std::size_t>(wantedHeight) >
          resizedY.max_size()) {
      statusCode = -107;
      return false;
    }
    try {
      resizedPixels.assign(destinationBytes, 0);
      buildBilinearAxis(
        sourceWidth, wantedWidth, resizedX);
      buildBilinearAxis(
        sourceHeight, wantedHeight, resizedY);
    } catch (const std::bad_alloc&) {
      statusCode = -107;
      return false;
    } catch (const std::length_error&) {
      statusCode = -107;
      return false;
    }

    clearGpuProcessor();
    outputWidth = wantedWidth;
    outputHeight = wantedHeight;
    pixels.swap(resizedPixels);
    sourceXForOutput.swap(resizedX);
    sourceYForOutput.swap(resizedY);
    // O quadro em cache possui a dimensão anterior; solicita novamente quando
    // a janela cruza o próximo degrau de resolução.
    hasFrame = false;
    frameTimestamp = -1.0;
    return true;
  }

  bool open(const std::string& utf8Path)
  {
    if (reader && path == utf8Path) return true;
    clearReader();
    if (!mediaFoundationStarted || utf8Path.empty()) {
      statusCode = -100;
      return false;
    }

    const std::wstring widePath = utf8ToWide(utf8Path);
    if (widePath.empty()) {
      statusCode = -101;
      return false;
    }

    auto createReader =
      [&](bool accelerated) -> HRESULT {
        reader.Reset();
        readerUsesD3D = false;
        ComPtr<IMFAttributes> attributes;
        HRESULT createResult =
          MFCreateAttributes(&attributes, 7);
        if (FAILED(createResult)) return createResult;
        attributes->SetUINT32(
          MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        attributes->SetUINT32(
          MF_READWRITE_DISABLE_CONVERTERS, FALSE);
        if (accelerated) {
          // ENABLE_VIDEO_PROCESSING é um conversor por software destinado a
          // miniaturas. A documentação da Media Foundation proíbe combiná-lo
          // com D3D_MANAGER.
          attributes->SetUINT32(
            MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, FALSE);
          attributes->SetUnknown(
            MF_SOURCE_READER_D3D_MANAGER,
            dxgiDeviceManager.Get());
          attributes->SetUINT32(
            MF_SOURCE_READER_DISABLE_DXVA, FALSE);
        } else {
          attributes->SetUINT32(
            MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        }

        createResult = MFCreateSourceReaderFromURL(
          widePath.c_str(), attributes.Get(), &reader);
        if (FAILED(createResult) || !reader) {
          reader.Reset();
          return FAILED(createResult)
            ? createResult
            : E_FAIL;
        }
        reader->SetStreamSelection(
          MF_SOURCE_READER_ALL_STREAMS, FALSE);
        createResult = reader->SetStreamSelection(
          MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        if (FAILED(createResult)) {
          reader.Reset();
          return createResult;
        }
        readerUsesD3D = accelerated;
        return S_OK;
      };

    auto setOutputSubtype =
      [&](const GUID& subtype) -> HRESULT {
        if (!reader) return E_POINTER;
        ComPtr<IMFMediaType> outputType;
        HRESULT typeResult =
          MFCreateMediaType(&outputType);
        if (FAILED(typeResult)) return typeResult;
        typeResult = outputType->SetGUID(
          MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (FAILED(typeResult)) return typeResult;
        typeResult = outputType->SetGUID(
          MF_MT_SUBTYPE, subtype);
        if (FAILED(typeResult)) return typeResult;
        return reader->SetCurrentMediaType(
          MF_SOURCE_READER_FIRST_VIDEO_STREAM,
          nullptr, outputType.Get());
      };

    if (!forceSoftwarePath.empty() &&
        forceSoftwarePath != utf8Path) {
      forceSoftwarePath.clear();
    }
    bool requestedAcceleration =
      forceSoftwarePath != utf8Path &&
      initializeD3D();
    HRESULT result = createReader(requestedAcceleration);
    if (FAILED(result) && requestedAcceleration) {
      // Driver, codec ou versão do Windows pode rejeitar DXVA. A janela não
      // pode ficar preta por isso: reabre a mesma mídia no caminho compatível.
      requestedAcceleration = false;
      result = createReader(false);
    }
    if (FAILED(result)) {
      statusCode = static_cast<int>(result);
      clearReader();
      return false;
    }

    result = setOutputSubtype(MFVideoFormat_NV12);
    if (FAILED(result) && readerUsesD3D) {
      // Alguns decoders aceitam o D3D manager, mas não publicam NV12 em uma
      // superfície compatível. Recriar sem D3D preserva a reprodução.
      const HRESULT softwareResult = createReader(false);
      result = SUCCEEDED(softwareResult)
        ? setOutputSubtype(MFVideoFormat_NV12)
        : softwareResult;
    }
    if (FAILED(result)) {
      // Formatos incomuns ainda podem usar o conversor RGB antigo. O caminho
      // normal permanece NV12 para evitar a conversão integral em RGB32.
      result = setOutputSubtype(MFVideoFormat_RGB32);
      if (FAILED(result)) {
        statusCode = static_cast<int>(result);
        clearReader();
        return false;
      }
    }

    ComPtr<IMFMediaType> currentType;
    result = reader->GetCurrentMediaType(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM, &currentType);
    if (FAILED(result) || !currentType) {
      statusCode = static_cast<int>(result);
      clearReader();
      return false;
    }

    UINT32 frameWidth = 0;
    UINT32 frameHeight = 0;
    result = MFGetAttributeSize(
      currentType.Get(), MF_MT_FRAME_SIZE,
      &frameWidth, &frameHeight);
    if (FAILED(result) ||
        frameWidth == 0 || frameHeight == 0 ||
        frameWidth >
          static_cast<UINT32>(
            std::numeric_limits<int>::max()) ||
        frameHeight >
          static_cast<UINT32>(
            std::numeric_limits<int>::max())) {
      statusCode = FAILED(result)
        ? static_cast<int>(result)
        : -106;
      clearReader();
      return false;
    }
    sourceWidth = static_cast<int>(frameWidth);
    sourceHeight = static_cast<int>(frameHeight);
    GUID currentSubtype = GUID_NULL;
    outputIsNv12 =
      SUCCEEDED(currentType->GetGUID(
        MF_MT_SUBTYPE, &currentSubtype)) &&
      IsEqualGUID(currentSubtype, MFVideoFormat_NV12);
    if (!outputIsNv12 &&
        sourceWidth >
          std::numeric_limits<LONG>::max() / 4) {
      statusCode = -106;
      clearReader();
      return false;
    }

    UINT32 numerator = 0;
    UINT32 denominator = 0;
    if (SUCCEEDED(MFGetAttributeRatio(
          currentType.Get(), MF_MT_FRAME_RATE,
          &numerator, &denominator)) &&
        numerator > 0 && denominator > 0) {
      frameRateNumerator = numerator;
      frameRateDenominator = denominator;
      frameDuration =
        static_cast<double>(denominator) /
        static_cast<double>(numerator);
    }
    frameDuration =
      std::max(1.0 / 240.0, std::min(1.0, frameDuration));

    UINT32 strideValue = 0;
    if (SUCCEEDED(currentType->GetUINT32(
          MF_MT_DEFAULT_STRIDE, &strideValue))) {
      defaultStride = static_cast<LONG>(strideValue);
    } else {
      LONG calculatedStride = 0;
      if (SUCCEEDED(MFGetStrideForBitmapInfoHeader(
            (outputIsNv12
              ? MFVideoFormat_NV12
              : MFVideoFormat_RGB32).Data1,
            sourceWidth, &calculatedStride))) {
        defaultStride = calculatedStride;
      }
    }
    if (defaultStride == 0) {
      defaultStride = outputIsNv12
        ? static_cast<LONG>(sourceWidth)
        : static_cast<LONG>(sourceWidth * 4);
    }

    path = utf8Path;
    statusCode = 1;
    return true;
  }

  bool seek(double sourceTime)
  {
    if (!reader) return false;
    PROPVARIANT position;
    PropVariantInit(&position);
    position.vt = VT_I8;
    position.hVal.QuadPart = secondsToHns(sourceTime);
    const HRESULT result =
      reader->SetCurrentPosition(GUID_NULL, position);
    PropVariantClear(&position);
    if (FAILED(result)) {
      statusCode = static_cast<int>(result);
      return false;
    }
    hasFrame = false;
    endOfStream = false;
    frameTimestamp = -1.0;
    return true;
  }

  bool configureGpuProcessor()
  {
    if (!d3dAvailable ||
        !videoDevice || !videoContext ||
        sourceWidth <= 0 || sourceHeight <= 0 ||
        outputWidth <= 0 || outputHeight <= 0) {
      return false;
    }
    if (videoProcessorEnumerator &&
        videoProcessor &&
        videoOutputTexture &&
        videoReadbackTexture &&
        videoOutputView &&
        gpuInputWidth == sourceWidth &&
        gpuInputHeight == sourceHeight &&
        gpuOutputWidth == outputWidth &&
        gpuOutputHeight == outputHeight) {
      return true;
    }

    clearGpuProcessor();

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content{};
    content.InputFrameFormat =
      D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content.InputFrameRate.Numerator =
      std::max<UINT>(1, frameRateNumerator);
    content.InputFrameRate.Denominator =
      std::max<UINT>(1, frameRateDenominator);
    content.InputWidth =
      static_cast<UINT>(sourceWidth);
    content.InputHeight =
      static_cast<UINT>(sourceHeight);
    content.OutputFrameRate = content.InputFrameRate;
    content.OutputWidth =
      static_cast<UINT>(outputWidth);
    content.OutputHeight =
      static_cast<UINT>(outputHeight);
    content.Usage =
      D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT result =
      videoDevice->CreateVideoProcessorEnumerator(
        &content, &videoProcessorEnumerator);
    if (FAILED(result) || !videoProcessorEnumerator) {
      clearGpuProcessor();
      return false;
    }
    result = videoDevice->CreateVideoProcessor(
      videoProcessorEnumerator.Get(),
      0,
      &videoProcessor);
    if (FAILED(result) || !videoProcessor) {
      clearGpuProcessor();
      return false;
    }

    D3D11_TEXTURE2D_DESC outputDescription{};
    outputDescription.Width =
      static_cast<UINT>(outputWidth);
    outputDescription.Height =
      static_cast<UINT>(outputHeight);
    outputDescription.MipLevels = 1;
    outputDescription.ArraySize = 1;
    outputDescription.Format =
      DXGI_FORMAT_B8G8R8A8_UNORM;
    outputDescription.SampleDesc.Count = 1;
    outputDescription.Usage = D3D11_USAGE_DEFAULT;
    outputDescription.BindFlags =
      D3D11_BIND_RENDER_TARGET;
    result = d3dDevice->CreateTexture2D(
      &outputDescription,
      nullptr,
      &videoOutputTexture);
    if (FAILED(result) || !videoOutputTexture) {
      clearGpuProcessor();
      return false;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDescription{};
    outputViewDescription.ViewDimension =
      D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputViewDescription.Texture2D.MipSlice = 0;
    result = videoDevice->CreateVideoProcessorOutputView(
      videoOutputTexture.Get(),
      videoProcessorEnumerator.Get(),
      &outputViewDescription,
      &videoOutputView);
    if (FAILED(result) || !videoOutputView) {
      clearGpuProcessor();
      return false;
    }

    D3D11_TEXTURE2D_DESC readbackDescription =
      outputDescription;
    readbackDescription.Usage = D3D11_USAGE_STAGING;
    readbackDescription.BindFlags = 0;
    readbackDescription.CPUAccessFlags =
      D3D11_CPU_ACCESS_READ;
    result = d3dDevice->CreateTexture2D(
      &readbackDescription,
      nullptr,
      &videoReadbackTexture);
    if (FAILED(result) || !videoReadbackTexture) {
      clearGpuProcessor();
      return false;
    }

    gpuInputWidth = sourceWidth;
    gpuInputHeight = sourceHeight;
    gpuOutputWidth = outputWidth;
    gpuOutputHeight = outputHeight;
    return true;
  }

  bool copyGpuSample(
    IMFSample* sample,
    LONGLONG timestamp)
  {
    if (!sample || !readerUsesD3D || !outputIsNv12 ||
        !configureGpuProcessor()) {
      return false;
    }

    ComPtr<IMFMediaBuffer> mediaBuffer;
    HRESULT result =
      sample->GetBufferByIndex(0, &mediaBuffer);
    if (FAILED(result) || !mediaBuffer) {
      return false;
    }

    ComPtr<IMFDXGIBuffer> dxgiBuffer;
    result = mediaBuffer.As(&dxgiBuffer);
    if (FAILED(result) || !dxgiBuffer) {
      return false;
    }

    ComPtr<ID3D11Texture2D> inputTexture;
    result = dxgiBuffer->GetResource(
      IID_PPV_ARGS(&inputTexture));
    if (FAILED(result) || !inputTexture) {
      return false;
    }
    UINT subresourceIndex = 0;
    result = dxgiBuffer->GetSubresourceIndex(
      &subresourceIndex);
    if (FAILED(result)) {
      return false;
    }

    D3D11_TEXTURE2D_DESC inputDescription{};
    inputTexture->GetDesc(&inputDescription);
    const UINT mipLevels =
      std::max<UINT>(1, inputDescription.MipLevels);
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDescription{};
    inputViewDescription.FourCC = 0;
    inputViewDescription.ViewDimension =
      D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputViewDescription.Texture2D.MipSlice =
      subresourceIndex % mipLevels;
    inputViewDescription.Texture2D.ArraySlice =
      subresourceIndex / mipLevels;

    ComPtr<ID3D11VideoProcessorInputView> inputView;
    result = videoDevice->CreateVideoProcessorInputView(
      inputTexture.Get(),
      videoProcessorEnumerator.Get(),
      &inputViewDescription,
      &inputView);
    if (FAILED(result) || !inputView) {
      return false;
    }

    RECT sourceRectangle{
      0, 0, sourceWidth, sourceHeight
    };
    RECT destinationRectangle{
      0, 0, outputWidth, outputHeight
    };
    videoContext->VideoProcessorSetStreamFrameFormat(
      videoProcessor.Get(),
      0,
      D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    videoContext->VideoProcessorSetStreamSourceRect(
      videoProcessor.Get(),
      0,
      TRUE,
      &sourceRectangle);
    videoContext->VideoProcessorSetStreamDestRect(
      videoProcessor.Get(),
      0,
      TRUE,
      &destinationRectangle);
    videoContext->VideoProcessorSetOutputTargetRect(
      videoProcessor.Get(),
      TRUE,
      &destinationRectangle);

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.pInputSurface = inputView.Get();
    result = videoContext->VideoProcessorBlt(
      videoProcessor.Get(),
      videoOutputView.Get(),
      0,
      1,
      &stream);
    if (FAILED(result)) {
      return false;
    }

    d3dContext->CopyResource(
      videoReadbackTexture.Get(),
      videoOutputTexture.Get());
    std::size_t destinationStride = 0;
    std::size_t destinationBytes = 0;
    if (!checkedBgraLayout(
          outputWidth,
          outputHeight,
          destinationStride,
          destinationBytes) ||
        destinationBytes != pixels.size()) {
      statusCode = -106;
      return false;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    result = d3dContext->Map(
      videoReadbackTexture.Get(),
      0,
      D3D11_MAP_READ,
      0,
      &mapped);
    if (FAILED(result) || !mapped.pData ||
        static_cast<std::size_t>(mapped.RowPitch) <
          destinationStride) {
      if (SUCCEEDED(result)) {
        d3dContext->Unmap(
          videoReadbackTexture.Get(), 0);
      }
      return false;
    }

    const auto* source =
      static_cast<const std::uint8_t*>(mapped.pData);
    for (int row = 0; row < outputHeight; ++row) {
      std::memcpy(
        pixels.data() +
          static_cast<std::size_t>(row) *
            destinationStride,
        source +
          static_cast<std::size_t>(row) *
            mapped.RowPitch,
        destinationStride);
    }
    d3dContext->Unmap(
      videoReadbackTexture.Get(), 0);

    frameTimestamp = hnsToSeconds(timestamp);
    hasFrame = true;
    statusCode = 3;
    return true;
  }

  bool copySample(IMFSample* sample, LONGLONG timestamp)
  {
    std::size_t destinationStride = 0;
    std::size_t destinationBytes = 0;
    if (!sample ||
        sourceWidth <= 0 || sourceHeight <= 0 ||
        outputWidth <= 0 || outputHeight <= 0 ||
        pixels.empty() ||
        !checkedBgraLayout(
          outputWidth,
          outputHeight,
          destinationStride,
          destinationBytes) ||
        destinationBytes != pixels.size()) {
      return false;
    }
    if (copyGpuSample(sample, timestamp)) {
      return true;
    }
    ComPtr<IMFMediaBuffer> mediaBuffer;
    HRESULT result =
      sample->ConvertToContiguousBuffer(&mediaBuffer);
    if (FAILED(result) || !mediaBuffer) {
      statusCode = static_cast<int>(result);
      return false;
    }

    if (outputIsNv12) {
      BYTE* data = nullptr;
      DWORD maxLength = 0;
      DWORD currentLength = 0;
      result = mediaBuffer->Lock(
        &data, &maxLength, &currentLength);
      if (FAILED(result) || !data) {
        statusCode = static_cast<int>(result);
        return false;
      }

      const std::int64_t strideMagnitude =
        defaultStride < 0
          ? -static_cast<std::int64_t>(defaultStride)
          : static_cast<std::int64_t>(defaultStride);
      if (strideMagnitude <= 0 ||
          strideMagnitude >
            std::numeric_limits<int>::max() ||
          sourceWidth >
            std::numeric_limits<int>::max() - 1) {
        mediaBuffer->Unlock();
        statusCode = -105;
        return false;
      }
      const int packedStride =
        sourceWidth + (sourceWidth & 1);
      int stride = std::max(
        packedStride,
        static_cast<int>(strideMagnitude));
      const std::size_t chromaRows =
        (static_cast<std::size_t>(sourceHeight) + 1u) / 2u;
      auto nv12Layout =
        [&](int candidateStride,
            std::size_t& yBytes,
            std::size_t& requiredBytes) -> bool {
          std::size_t chromaBytes = 0;
          return candidateStride >= packedStride &&
            checkedSizeProduct(
              static_cast<std::size_t>(candidateStride),
              static_cast<std::size_t>(sourceHeight),
              yBytes) &&
            checkedSizeProduct(
              static_cast<std::size_t>(candidateStride),
              chromaRows,
              chromaBytes) &&
            checkedSizeSum(
              yBytes, chromaBytes, requiredBytes);
        };
      std::size_t yPlaneBytes = 0;
      std::size_t requiredBytes = 0;
      if (!nv12Layout(
            stride, yPlaneBytes, requiredBytes) ||
          static_cast<std::size_t>(currentLength) <
            requiredBytes) {
        // Alguns decoders não publicam o padding na mídia final.
        stride = packedStride;
        if (!nv12Layout(
              stride, yPlaneBytes, requiredBytes)) {
          mediaBuffer->Unlock();
          statusCode = -105;
          return false;
        }
      }
      if (static_cast<std::size_t>(currentLength) <
          requiredBytes) {
        mediaBuffer->Unlock();
        statusCode = -105;
        return false;
      }

      const BYTE* yPlane = data;
      const BYTE* uvPlane = data + yPlaneBytes;
      auto byteClamp = [](int value) -> std::uint8_t {
        return static_cast<std::uint8_t>(
          std::max(0, std::min(255, value)));
      };
      for (int y = 0; y < outputHeight; ++y) {
        const BilinearAxisSample& sourceY =
          sourceYForOutput[static_cast<std::size_t>(y)];
        const BYTE* yRowTop =
          yPlane +
          static_cast<std::size_t>(sourceY.first) * stride;
        const BYTE* yRowBottom =
          yPlane +
          static_cast<std::size_t>(sourceY.second) * stride;
        const BYTE* uvRowTop =
          uvPlane +
          static_cast<std::size_t>(sourceY.first / 2) * stride;
        const BYTE* uvRowBottom =
          uvPlane +
          static_cast<std::size_t>(sourceY.second / 2) * stride;
        std::uint8_t* destinationRow =
          pixels.data() +
          static_cast<std::size_t>(y) *
            destinationStride;
        for (int x = 0; x < outputWidth; ++x) {
          const BilinearAxisSample& sourceX =
            sourceXForOutput[static_cast<std::size_t>(x)];
          const int uvLeft = std::min(
            stride - 2, sourceX.first & ~1);
          const int uvRight = std::min(
            stride - 2, sourceX.second & ~1);
          const int yValue = bilinearByte(
            yRowTop[sourceX.first],
            yRowTop[sourceX.second],
            yRowBottom[sourceX.first],
            yRowBottom[sourceX.second],
            sourceX.weight,
            sourceY.weight);
          const int uValue = bilinearByte(
            uvRowTop[uvLeft],
            uvRowTop[uvRight],
            uvRowBottom[uvLeft],
            uvRowBottom[uvRight],
            sourceX.weight,
            sourceY.weight);
          const int vValue = bilinearByte(
            uvRowTop[uvLeft + 1],
            uvRowTop[uvRight + 1],
            uvRowBottom[uvLeft + 1],
            uvRowBottom[uvRight + 1],
            sourceX.weight,
            sourceY.weight);
          const int scaledLuma = yScaleTable[yValue];
          destinationRow[x * 4 + 0] =
            byteClamp(
              (scaledLuma + uBlueTable[uValue] + 128) >> 8);
          destinationRow[x * 4 + 1] =
            byteClamp(
              (scaledLuma +
               uGreenTable[uValue] +
               vGreenTable[vValue] + 128) >> 8);
          destinationRow[x * 4 + 2] =
            byteClamp(
              (scaledLuma + vRedTable[vValue] + 128) >> 8);
          destinationRow[x * 4 + 3] = 255;
        }
      }
      mediaBuffer->Unlock();
      frameTimestamp = hnsToSeconds(timestamp);
      hasFrame = true;
      statusCode = 2;
      return true;
    }

    BYTE* scanline0 = nullptr;
    LONG pitch = defaultStride;
    bool locked2d = false;
    ComPtr<IMF2DBuffer> buffer2d;
    if (SUCCEEDED(mediaBuffer.As(&buffer2d)) && buffer2d) {
      result = buffer2d->Lock2D(&scanline0, &pitch);
      locked2d = SUCCEEDED(result);
    }

    BYTE* contiguous = nullptr;
    DWORD maxLength = 0;
    DWORD currentLength = 0;
    if (!locked2d) {
      result = mediaBuffer->Lock(
        &contiguous, &maxLength, &currentLength);
      if (FAILED(result) || !contiguous) {
        statusCode = static_cast<int>(result);
        return false;
      }
      scanline0 = contiguous;
      pitch = defaultStride;
    }

    const std::size_t rowBytes =
      static_cast<std::size_t>(sourceWidth) * 4u;
    const std::int64_t absolutePitch64 =
      pitch < 0
        ? -static_cast<std::int64_t>(pitch)
        : static_cast<std::int64_t>(pitch);
    if (absolutePitch64 <= 0 ||
        static_cast<std::uint64_t>(absolutePitch64) <
          static_cast<std::uint64_t>(rowBytes) ||
        static_cast<std::uint64_t>(absolutePitch64) >
          static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
      if (locked2d) buffer2d->Unlock2D();
      else mediaBuffer->Unlock();
      statusCode = -102;
      return false;
    }
    const std::size_t absolutePitch =
      static_cast<std::size_t>(absolutePitch64);
    std::size_t lastRowOffset = 0;
    std::size_t requiredBytes = 0;
    if (!checkedSizeProduct(
          absolutePitch,
          static_cast<std::size_t>(sourceHeight - 1),
          lastRowOffset) ||
        !checkedSizeSum(
          lastRowOffset, rowBytes, requiredBytes) ||
        lastRowOffset >
          static_cast<std::size_t>(
            std::numeric_limits<std::ptrdiff_t>::max())) {
      if (locked2d) buffer2d->Unlock2D();
      else mediaBuffer->Unlock();
      statusCode = -102;
      return false;
    }

    // IMFMediaBuffer::Lock aponta para o início físico do bloco. Em RGB
    // bottom-up isso é a última linha lógica, ao contrário de Lock2D, que já
    // entrega scanline0 na linha superior exibida.
    if (!locked2d) {
      if (requiredBytes >
          static_cast<std::size_t>(currentLength)) {
        mediaBuffer->Unlock();
        statusCode = -102;
        return false;
      }
      if (pitch < 0) {
        scanline0 = contiguous + lastRowOffset;
      }
    }

    // scanline0 representa a primeira linha lógica da imagem. O sinal do
    // pitch já informa a direção necessária para alcançar as linhas seguintes;
    // inverter manualmente o índice deixa o vídeo de cabeça para baixo.
    for (int y = 0; y < outputHeight; ++y) {
      const BilinearAxisSample& sourceY =
        sourceYForOutput[static_cast<std::size_t>(y)];
      const BYTE* sourceRowTop =
        scanline0 +
        static_cast<std::ptrdiff_t>(sourceY.first) * pitch;
      const BYTE* sourceRowBottom =
        scanline0 +
        static_cast<std::ptrdiff_t>(sourceY.second) * pitch;
      std::uint8_t* destinationRow =
        pixels.data() +
        static_cast<std::size_t>(y) *
          destinationStride;
      for (int x = 0; x < outputWidth; ++x) {
        const BilinearAxisSample& sourceX =
          sourceXForOutput[static_cast<std::size_t>(x)];
        const std::size_t leftOffset =
          static_cast<std::size_t>(sourceX.first) * 4u;
        const std::size_t rightOffset =
          static_cast<std::size_t>(sourceX.second) * 4u;
        for (int channel = 0; channel < 3; ++channel) {
          destinationRow[x * 4 + channel] =
            static_cast<std::uint8_t>(bilinearByte(
              sourceRowTop[leftOffset + channel],
              sourceRowTop[rightOffset + channel],
              sourceRowBottom[leftOffset + channel],
              sourceRowBottom[rightOffset + channel],
              sourceX.weight,
              sourceY.weight));
        }
        destinationRow[x * 4 + 3] = 255;
      }
    }

    if (locked2d) buffer2d->Unlock2D();
    else mediaBuffer->Unlock();

    frameTimestamp = hnsToSeconds(timestamp);
    hasFrame = true;
    statusCode = 2;
    return true;
  }

  bool readUntil(double sourceTime, bool forceSeek)
  {
    if (!reader) return false;

    const bool needsSeek =
      forceSeek ||
      !hasFrame ||
      sourceTime + frameDuration < frameTimestamp;
    if (needsSeek && !seek(sourceTime)) return false;

    if (hasFrame &&
        sourceTime <= frameTimestamp + frameDuration * 0.75) {
      return true;
    }

    // Após seek o Source Reader pode começar no keyframe anterior. O teto é
    // alto o bastante para GOPs longos, mas impede uma mídia danificada de
    // prender a thread da interface indefinidamente.
    constexpr int maximumSamplesPerRequest = 900;
    for (int attempt = 0;
         attempt < maximumSamplesPerRequest;
         ++attempt) {
      DWORD actualStream = 0;
      DWORD flags = 0;
      LONGLONG timestamp = 0;
      ComPtr<IMFSample> sample;
      const HRESULT result = reader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0, &actualStream, &flags, &timestamp, &sample);
      if (FAILED(result)) {
        statusCode = static_cast<int>(result);
        return hasFrame;
      }
      if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
        endOfStream = true;
        return hasFrame;
      }
      if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0) {
        // O formato solicitado continua RGB32; dimensões novas serão
        // reaplicadas na próxima abertura da mídia.
      }
      if (!sample) continue;
      const double sampleTime = hnsToSeconds(timestamp);
      // Depois de um seek, o decoder pode atravessar os quadros desde o
      // keyframe anterior. Eles precisam ser decodificados pelo codec, mas não
      // precisam passar pela conversão NV12->BGRA. Converte somente o quadro
      // que realmente será exibido.
      if (sampleTime + frameDuration * 0.5 < sourceTime) continue;
      return copySample(sample.Get(), timestamp);
    }
    statusCode = -103;
    return hasFrame;
  }

  void clearPublished()
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    publishedPixels.reset();
    publishedPath.clear();
    publishedPlaybackKey.clear();
    publishedWidth = 0;
    publishedHeight = 0;
    publishedStride = 0;
    publishedTimestamp = -1.0;
  }

  void publishCurrentFrame(
    const PlaybackRequest& processed)
  {
    if (!hasFrame || pixels.empty() ||
        outputWidth <= 0 || outputHeight <= 0) {
      return;
    }

    // A cópia acontece na worker. A interface recebe um snapshot imutável e
    // pode desenhá-lo sem bloquear o decoder ou correr risco de data race.
    std::shared_ptr<std::vector<std::uint8_t>> snapshot;
    try {
      snapshot =
        std::make_shared<std::vector<std::uint8_t>>(pixels);
    } catch (const std::bad_alloc&) {
      statusCode = -107;
      return;
    } catch (const std::length_error&) {
      statusCode = -107;
      return;
    }
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!hasRequest ||
        request.path != path ||
        request.playbackKey != processed.playbackKey) {
      return;
    }
    publishedPixels = std::move(snapshot);
    publishedPath = path;
    publishedPlaybackKey = processed.playbackKey;
    publishedWidth = outputWidth;
    publishedHeight = outputHeight;
    publishedStride = outputWidth * 4;
    publishedTimestamp = frameTimestamp;
  }

  PlaybackRequest latestRequest()
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    return request;
  }

  bool shouldStop()
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    return stopRequested;
  }

  void waitForWork(
    const PlaybackRequest& processed,
    std::chrono::milliseconds duration)
  {
    std::unique_lock<std::mutex> lock(stateMutex);
    stateChanged.wait_for(lock, duration, [&]() {
      return stopRequested ||
        !hasRequest ||
        request.serial != processed.serial;
    });
  }

  void workerLoop()
  {
    const HRESULT comResult =
      CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    comInitialized =
      comResult == S_OK || comResult == S_FALSE;
    const HRESULT mfResult =
      MFStartup(MF_VERSION, MFSTARTUP_LITE);
    mediaFoundationStarted = SUCCEEDED(mfResult);
    if (!mediaFoundationStarted) {
      statusCode = static_cast<int>(mfResult);
    }

    double lastPublishedTimestamp = -1.0;
    std::string lastPublishedPath;
    std::string lastPublishedPlaybackKey;
    int lastPublishedWidth = 0;
    int lastPublishedHeight = 0;

    for (;;) {
      PlaybackRequest current;
      {
        std::unique_lock<std::mutex> lock(stateMutex);
        stateChanged.wait(lock, [&]() {
          return stopRequested || hasRequest;
        });
        if (stopRequested) break;
        current = request;
      }

      if (current.path.empty()) {
        clearReader();
        forceSoftwarePath.clear();
        clearPublished();
        lastPublishedTimestamp = -1.0;
        lastPublishedPath.clear();
        lastPublishedPlaybackKey.clear();
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          if (request.serial == current.serial) {
            hasRequest = false;
          }
        }
        continue;
      }

      if (!mediaFoundationStarted ||
          !open(current.path) ||
          !configureOutputSize(
            current.requestedWidth,
            current.requestedHeight)) {
        waitForWork(current, std::chrono::milliseconds(80));
        continue;
      }

      const auto now = std::chrono::steady_clock::now();
      const double elapsed =
        current.playing
          ? std::chrono::duration<double>(
              now - current.sampledAt).count()
          : 0.0;
      const double targetTime = std::max(
        0.0,
        current.sourceTime +
          elapsed * current.playbackRate);

      // O alvo sempre nasce do relógio do REAPER, portanto não existe desvio
      // acumulado. Fluxo normal avança sequencialmente; seek só acontece em
      // início/troca de mídia, salto real no grid ou retorno de loop.
      const double backwardTolerance =
        std::max(0.050, frameDuration * 1.5);
      const bool jumpedBackward =
        hasFrame &&
        targetTime < frameTimestamp - backwardTolerance;
      const bool jumpedForward =
        hasFrame &&
        targetTime > frameTimestamp + 2.0;
      const bool forceSeek =
        !hasFrame || jumpedBackward || jumpedForward;

      bool decoded = false;
      if (forceSeek) {
        decoded = readUntil(targetTime, true);
      } else if (!current.playing) {
        if (std::abs(targetTime - frameTimestamp) >=
            std::max(0.020, frameDuration * 0.70)) {
          decoded = readUntil(
            targetTime,
            targetTime < frameTimestamp);
        }
      } else if (
          targetTime >= frameTimestamp + frameDuration * 0.65) {
        decoded = readUntil(targetTime, false);
      }

      if (!decoded &&
          readerUsesD3D &&
          statusCode.load() < 0) {
        // A negociação do formato pode funcionar e o driver ainda rejeitar
        // a superfície na primeira conversão. Reabre uma única vez em
        // software para este arquivo, em vez de manter a janela preta.
        forceSoftwarePath = current.path;
        clearReader();
        waitForWork(
          current, std::chrono::milliseconds(1));
        continue;
      }

      if (decoded && hasFrame &&
          (lastPublishedPath != path ||
           lastPublishedPlaybackKey != current.playbackKey ||
           lastPublishedWidth != outputWidth ||
           lastPublishedHeight != outputHeight ||
           std::abs(
             lastPublishedTimestamp - frameTimestamp) >
             0.000001)) {
        publishCurrentFrame(current);
        lastPublishedPath = path;
        lastPublishedPlaybackKey = current.playbackKey;
        lastPublishedWidth = outputWidth;
        lastPublishedHeight = outputHeight;
        lastPublishedTimestamp = frameTimestamp;
      }

      if (shouldStop()) break;
      if (current.playing && !endOfStream && hasFrame) {
        const auto afterDecode =
          std::chrono::steady_clock::now();
        const double liveTarget = std::max(
          0.0,
          current.sourceTime +
            std::chrono::duration<double>(
              afterDecode - current.sampledAt).count() *
              current.playbackRate);
        const double secondsUntilNext =
          (frameTimestamp + frameDuration * 0.65 - liveTarget) /
          std::max(0.1, current.playbackRate);
        const int waitMs = std::max(
          1, std::min(
            20,
            static_cast<int>(
              std::lround(secondsUntilNext * 1000.0))));
        waitForWork(
          current, std::chrono::milliseconds(waitMs));
      } else {
        waitForWork(
          current,
          std::chrono::milliseconds(
            endOfStream ? 80 : 30));
      }
    }

    clearReader();
    videoContext.Reset();
    videoDevice.Reset();
    dxgiDeviceManager.Reset();
    d3dContext.Reset();
    d3dDevice.Reset();
    d3dAvailable = false;
    if (mediaFoundationStarted) {
      MFShutdown();
      mediaFoundationStarted = false;
    }
    if (comInitialized) {
      CoUninitialize();
      comInitialized = false;
    }
  }

  void submit(
    const std::string& utf8Path,
    const std::string& playbackKey,
    double sourceTime,
    bool playing,
    double playbackRate,
    int requestedWidth,
    int requestedHeight)
  {
    const double safeTime = std::max(0.0, sourceTime);
    const double safeRate =
      std::max(0.1, std::min(4.0, playbackRate));
    const int safeWidth = std::max(1, requestedWidth);
    const int safeHeight = std::max(1, requestedHeight);
    const auto now = std::chrono::steady_clock::now();
    bool changed = false;
    {
      std::lock_guard<std::mutex> lock(stateMutex);

      const bool hadActiveRequest =
        hasRequest && !request.path.empty();
      const bool identityChanged =
        !hadActiveRequest ||
        request.path != utf8Path ||
        request.playbackKey != playbackKey;
      const bool playbackStateChanged =
        !hadActiveRequest ||
        request.playing != playing ||
        std::abs(request.playbackRate - safeRate) > 0.001;
      const bool outputSizeChanged =
        !hadActiveRequest ||
        request.requestedWidth != safeWidth ||
        request.requestedHeight != safeHeight;

      bool explicitTransportJump = false;
      if (hasObservedClock &&
          lastObservedPath == utf8Path &&
          lastObservedPlaybackKey == playbackKey &&
          lastObservedPlaying && playing &&
          std::abs(lastObservedPlaybackRate - safeRate) <= 0.001) {
        const double expectedExternalTime =
          lastObservedSourceTime +
          std::chrono::duration<double>(
            now - lastObservedAt).count() * safeRate;
        // Mudanças reais no cursor (seek, retorno de loop ou troca de corte)
        // continuam imediatas. Repetições/atrasos pequenos da amostra do
        // REAPER não reiniciam o relógio local do vídeo.
        explicitTransportJump =
          std::abs(safeTime - expectedExternalTime) > 0.30;
      }

      double localTime = request.sourceTime;
      if (hadActiveRequest && request.playing) {
        localTime +=
          std::chrono::duration<double>(
            now - request.sampledAt).count() *
          request.playbackRate;
      }
      const double localDrift = safeTime - localTime;
      const bool correctionWindowOpen =
        lastClockCorrectionAt ==
          std::chrono::steady_clock::time_point::min() ||
        std::chrono::duration<double>(
          now - lastClockCorrectionAt).count() >= 1.8;
      // Mesmo critério da antiga Hook Center: enquanto toca, o arquivo segue
      // seu próprio relógio. Só há correção periódica quando o desvio passa a
      // ser perceptível; não existe micro-seek a cada atualização da janela.
      const bool driftNeedsCorrection =
        hadActiveRequest && playing &&
        std::abs(localDrift) > 2.0 &&
        correctionWindowOpen;
      const bool stoppedPositionChanged =
        hadActiveRequest && !playing &&
        std::abs(request.sourceTime - safeTime) > 0.008;
      const bool synchronizeClock =
        identityChanged ||
        playbackStateChanged ||
        explicitTransportJump ||
        driftNeedsCorrection ||
        stoppedPositionChanged;

      lastObservedPath = utf8Path;
      lastObservedPlaybackKey = playbackKey;
      lastObservedSourceTime = safeTime;
      lastObservedPlaybackRate = safeRate;
      lastObservedPlaying = playing;
      lastObservedAt = now;
      hasObservedClock = !utf8Path.empty();

      changed =
        synchronizeClock ||
        outputSizeChanged ||
        !hasRequest;
      if (!changed) return;
      request.path = utf8Path;
      request.playbackKey = playbackKey;
      if (synchronizeClock) {
        request.sourceTime = safeTime;
        request.sampledAt = now;
        lastClockCorrectionAt = now;
      }
      request.playing = playing;
      request.playbackRate = safeRate;
      request.requestedWidth = safeWidth;
      request.requestedHeight = safeHeight;
      ++request.serial;
      hasRequest = true;
    }
    stateChanged.notify_one();
  }

  bool currentFrame(
    const std::string& requestedPath,
    const std::string& requestedPlaybackKey,
    DecodedFrame& output)
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!publishedPixels ||
        publishedPixels->empty() ||
        publishedPath != requestedPath ||
        publishedPlaybackKey != requestedPlaybackKey ||
        publishedWidth <= 0 ||
        publishedHeight <= 0 ||
        publishedStride < publishedWidth * 4) {
      return false;
    }
    output.storage = publishedPixels;
    output.pixels = output.storage->data();
    output.width = publishedWidth;
    output.height = publishedHeight;
    output.stride = publishedStride;
    output.timestamp = publishedTimestamp;
    return true;
  }

  void requestReset()
  {
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      const std::uint64_t nextSerial = request.serial + 1;
      request = {};
      request.serial = nextSerial;
      request.path.clear();
      hasRequest = true;
      publishedPixels.reset();
      publishedPath.clear();
      publishedPlaybackKey.clear();
      publishedWidth = 0;
      publishedHeight = 0;
      publishedStride = 0;
      publishedTimestamp = -1.0;
      hasObservedClock = false;
      lastObservedPath.clear();
      lastObservedPlaybackKey.clear();
      lastObservedSourceTime = 0.0;
      lastObservedPlaybackRate = 1.0;
      lastObservedPlaying = false;
      lastObservedAt = std::chrono::steady_clock::now();
      lastClockCorrectionAt =
        std::chrono::steady_clock::time_point::min();
    }
    stateChanged.notify_one();
  }
};

Decoder::Decoder()
  : impl_(std::make_unique<Impl>())
{
}

Decoder::~Decoder() = default;

bool Decoder::frameAt(
  const std::string& utf8Path,
  const std::string& playbackKey,
  double sourceTime,
  bool playing,
  double playbackRate,
  int requestedWidth,
  int requestedHeight,
  DecodedFrame& output)
{
  output = {};
  if (!impl_) return false;
  impl_->submit(
    utf8Path,
    playbackKey,
    sourceTime,
    playing,
    playbackRate,
    requestedWidth,
    requestedHeight);
  return impl_->currentFrame(
    utf8Path, playbackKey, output);
}

void Decoder::reset()
{
  if (impl_) impl_->requestReset();
}

int Decoder::status() const
{
  return impl_ ? impl_->statusCode.load() : -199;
}

} // namespace vshook_video

#endif
