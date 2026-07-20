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

project(reaper_vshook_loader VERSION 3.0.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN OFF)

if(APPLE)
  enable_language(OBJCXX)
endif()


add_library(reaper_vshook SHARED
  src/vshook_extension.cpp
)

target_include_directories(reaper_vshook PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/sdk
  ${CMAKE_CURRENT_SOURCE_DIR}/WDL
  ${CMAKE_CURRENT_SOURCE_DIR}/WDL/swell
)

target_compile_definitions(reaper_vshook PRIVATE
  VSHOOK_EXTENSION_VERSION="${PROJECT_VERSION}"
  SWELL_PROVIDED_BY_APP
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

if(WIN32)
  target_link_libraries(reaper_vshook PRIVATE ws2_32)
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
    /Zc:threadSafeInit-
  )
endif()

set_target_properties(reaper_vshook PROPERTIES
  OUTPUT_NAME "reaper_vshook"
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
    "-framework AppKit"
    "-framework CoreFoundation"
  )

  target_link_options(reaper_vshook PRIVATE
    "-undefined" "dynamic_lookup"
    "-Wl,-exported_symbol,_ReaperPluginEntry"
  )
endif()
