if(NOT EXISTS "${VSHOOK_BINARY}")
  message(FATAL_ERROR "VS Hook binary not found: ${VSHOOK_BINARY}")
endif()

execute_process(COMMAND /usr/bin/lipo -archs "${VSHOOK_BINARY}"
  RESULT_VARIABLE status OUTPUT_VARIABLE architectures ERROR_VARIABLE diagnostic)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Cannot inspect VS Hook architectures: ${diagnostic}")
endif()
string(STRIP "${architectures}" architectures)
string(REPLACE " " ";" architectures "${architectures}")
foreach(architecture IN LISTS architectures)
  execute_process(COMMAND /usr/bin/otool -arch "${architecture}" -L "${VSHOOK_BINARY}"
    RESULT_VARIABLE status OUTPUT_VARIABLE dependencies ERROR_VARIABLE diagnostic)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Cannot inspect VS Hook dependencies: ${diagnostic}")
  endif()
  if(dependencies MATCHES "libswiftCoreMedia[.]dylib")
    message(FATAL_ERROR
      "VS Hook ${architecture} links CoreMedia C functions through libswiftCoreMedia. "
      "This breaks loading on older macOS versions despite the 10.13 target. "
      "Reconfigure with -DCMAKE_OSX_SYSROOT=<path to MacOSX26.5.sdk or an earlier SDK>.")
  endif()
  if(NOT dependencies MATCHES "/System/Library/Frameworks/CoreMedia[.]framework/Versions/A/CoreMedia")
    message(FATAL_ERROR "VS Hook ${architecture} must link the system CoreMedia framework directly.")
  endif()
endforeach()
message(STATUS "VS Hook CoreMedia compatibility verified: ${architectures}")
