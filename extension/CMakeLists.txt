cmake_minimum_required(VERSION 3.20)

project(reaper_vshook_loader LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN OFF)

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS universal build" FORCE)
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
  SWELL_PROVIDED_BY_APP
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

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
