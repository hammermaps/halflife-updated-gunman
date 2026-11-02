# Cross-compile the 32-bit Windows (win32) game DLLs on Linux using
# MinGW-w64, so the resulting hl.dll/client.dll can be tested through
# Proton instead of the native Linux GoldSrc engine.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

# The plain i686-w64-mingw32-gcc/g++ alternatives can point at the
# "win32" thread-model build, which has no C++11 <thread>/<mutex>/
# <condition_variable> support (SDK code such as cl_dll/inputw32.cpp
# uses std::thread). Use the "posix" variant explicitly.
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Static-link the MinGW runtime so the DLLs don't need
# libgcc_s_dw2-1.dll/libstdc++-6.dll to be present next to them.
#
# MinGW-w64 only ships "windows.h" (lowercase); some upstream SDK files
# #include <Windows.h>, which fails to resolve on a case-sensitive
# filesystem. -idirafter with a small local shim (Windows.h that just
# includes windows.h) fixes this without patching the SDK source.
set(CMAKE_CXX_FLAGS "-static-libgcc -static-libstdc++ -idirafter \"${CMAKE_CURRENT_LIST_DIR}/../../../scripts/mingw-case-shim\"")
set(CMAKE_C_FLAGS "-idirafter \"${CMAKE_CURRENT_LIST_DIR}/../../../scripts/mingw-case-shim\"")
