cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

if(APPLE)
  # Nao force target/arquitetura aqui.
  # Igual a estrategia da SWS: o workflow define o deployment target por arquitetura.
  # x86_64 precisa continuar carregando no macOS 10.13; arm64 exige macOS 11+.
  if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "macOS architecture")
  endif()

  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" AND NOT CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
      set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    else()
      set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    endif()
  endif()
endif()

project(reaper_vshook_loader VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN OFF)
option(VSHOOK_BUILD_TESTS "Build native VS Hook license tests" OFF)

# O runtime do FFmpeg e carregado dinamicamente de
# REAPER/UserPlugins/VSHookRuntime/FFmpeg. Durante a compilacao precisamos
# apenas dos headers da mesma serie ABI usada pelo runtime (FFmpeg 8.1.x).
find_path(VSHOOK_FFMPEG_INCLUDE_DIR
  NAMES libavcodec/avcodec.h
  HINTS
    "$ENV{VSHOOK_FFMPEG_SOURCE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ffmpeg-8.1.2"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg-8.1.2"
)
if(NOT VSHOOK_FFMPEG_INCLUDE_DIR)
  include(FetchContent)
  FetchContent_Declare(vshook_ffmpeg_headers
    URL "https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_GetProperties(vshook_ffmpeg_headers)
  if(NOT vshook_ffmpeg_headers_POPULATED)
    FetchContent_Populate(vshook_ffmpeg_headers)
  endif()
  set(VSHOOK_FFMPEG_INCLUDE_DIR
    "${vshook_ffmpeg_headers_SOURCE_DIR}")
endif()

# A raiz do source do FFmpeg contem um arquivo texto chamado "version".
# No macOS ele nao pode entrar como -I normal, pois passaria na frente do
# header C++ <version> da libc++. -idirafter deixa os headers do sistema com
# prioridade e ainda permite incluir libavcodec/*, libavformat/* etc.
function(vshook_add_ffmpeg_headers target_name)
  target_include_directories(${target_name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ffmpeg_compat
  )
  if(APPLE)
    target_compile_options(${target_name} PRIVATE
      -idirafter
      "${VSHOOK_FFMPEG_INCLUDE_DIR}"
    )
  else()
    target_include_directories(${target_name} PRIVATE
      "${VSHOOK_FFMPEG_INCLUDE_DIR}"
    )
  endif()
endfunction()

if(APPLE)
  enable_language(OBJCXX)
endif()

if(VSHOOK_BUILD_TESTS)
  add_executable(vshook_license_test
    src/license_validator.cpp
    tests/license_validator_test.cpp
  )
  target_include_directories(vshook_license_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  if(WIN32)
    target_compile_definitions(vshook_license_test PRIVATE
      WIN32_LEAN_AND_MEAN
      NOMINMAX
    )
    target_link_libraries(vshook_license_test PRIVATE advapi32 bcrypt)
  elseif(APPLE)
    target_link_libraries(vshook_license_test PRIVATE
      "-framework CoreFoundation"
      "-framework Security"
    )
  endif()

  if(WIN32 OR APPLE)
    add_executable(vshook_video_decoder_smoke_test
      src/native_video_decoder_ffmpeg.cpp
      tests/native_video_decoder_smoke_test.cpp
    )
    target_include_directories(vshook_video_decoder_smoke_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    vshook_add_ffmpeg_headers(vshook_video_decoder_smoke_test)
    if(WIN32)
      target_compile_definitions(vshook_video_decoder_smoke_test PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
      )
      target_link_libraries(vshook_video_decoder_smoke_test PRIVATE winmm)
    endif()
  endif()
endif()

add_library(reaper_vshook SHARED
  src/js_api_compat.cpp
  src/license_validator.cpp
  src/native_static_image_decoder.cpp
  src/vshook_extension.cpp
)

if(WIN32)
  target_sources(reaper_vshook PRIVATE
    src/native_video_decoder_ffmpeg.cpp
  )
elseif(APPLE)
  target_sources(reaper_vshook PRIVATE
    src/native_keyboard_mac.mm
    src/native_teleprompt_mac.mm
    src/native_video_decoder_ffmpeg.cpp
  )
endif()

target_include_directories(reaper_vshook PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/sdk
  ${CMAKE_CURRENT_SOURCE_DIR}/WDL
  ${CMAKE_CURRENT_SOURCE_DIR}/WDL/swell
)
vshook_add_ffmpeg_headers(reaper_vshook)

target_compile_definitions(reaper_vshook PRIVATE
  VSHOOK_EXTENSION_VERSION="${PROJECT_VERSION}"
  SWELL_PROVIDED_BY_APP
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

if(WIN32)
  target_link_libraries(reaper_vshook PRIVATE
    ws2_32
    comctl32
    comdlg32
    shell32
    ole32
    advapi32
    bcrypt
    d3d11
    dxgi
    mfplat
    mfreadwrite
    mfuuid
    propsys
    normaliz
    windowscodecs
    winmm
  )
endif()

if(MSVC)
  # Igual ao padrao da SWS: runtime C/C++ estatico.
  # Evita clientes Windows sem VC++ Redistributable ficarem sem carregar a DLL,
  # o que faz o menu Extensions nem aparecer.
  set_property(TARGET reaper_vshook PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
  target_compile_options(reaper_vshook PRIVATE
    /utf-8
    /EHsc
    /Zc:threadSafeInit-
  )
endif()

set_target_properties(reaper_vshook PROPERTIES
  # O nome do produto precisa comecar com reaper_ para o
  # REAPER descobrir automaticamente a extensao.
  OUTPUT_NAME "reaper_VSHookExt"
)

if(APPLE)
  set_target_properties(reaper_vshook PROPERTIES
    PREFIX ""
    SUFFIX ".dylib"
  )

  target_sources(reaper_vshook PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/WDL/swell/swell-modstub.mm
  )

  target_link_libraries(reaper_vshook PRIVATE
    "-framework Accelerate"
    "-framework AppKit"
    "-framework AVFoundation"
    "-framework CoreGraphics"
    "-framework CoreFoundation"
    "-framework ImageIO"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework Security"
  )

  target_link_options(reaper_vshook PRIVATE
    "-undefined" "dynamic_lookup"
    "-Wl,-exported_symbol,_ReaperPluginEntry"
    "-Wl,-rpath,@loader_path/VSHookRuntime/FFmpeg/lib"
  )
endif()
