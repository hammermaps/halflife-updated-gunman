# Cross-compile with real MSVC (via mstorsjo/msvc-wine, running cl.exe
# under Wine) instead of MinGW, so the resulting client.dll can link
# against the prebuilt, MSVC-built utils/vgui/lib/win32_vc16/vgui.lib
# import library (MinGW/GCC cannot link its MSVC-mangled C++ symbols,
# see build-steam-goldsource-windows.sh's header comment).
#
# Requires MSVC_WINE_BIN (the msvc-wine bin/x86 directory, containing
# the cl/link/lib/rc wrapper scripts) to be set, e.g.:
#   -DMSVC_WINE_BIN=/path/to/opt-msvc/bin/x86
if(NOT DEFINED MSVC_WINE_BIN)
	message(FATAL_ERROR "MSVC_WINE_BIN must be set to the msvc-wine bin/x86 directory")
endif()

# Without this, try_compile()'s isolated scratch project (used for compiler
# ABI/feature detection) doesn't see MSVC_WINE_BIN and re-triggers the
# FATAL_ERROR above.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MSVC_WINE_BIN)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(CMAKE_C_COMPILER "${MSVC_WINE_BIN}/cl")
set(CMAKE_CXX_COMPILER "${MSVC_WINE_BIN}/cl")
set(CMAKE_RC_COMPILER "${MSVC_WINE_BIN}/rc")
set(CMAKE_MT "${MSVC_WINE_BIN}/mt")
set(CMAKE_LINKER "${MSVC_WINE_BIN}/link")
set(CMAKE_AR "${MSVC_WINE_BIN}/lib")

# CMake defaults to "-external:I<path>" (no space) for isystem/PRIVATE
# external include dirs with this MSVC version. That flag form doesn't
# resolve paths correctly when cl.exe runs under Wine (fatal error
# C1083, even though the files exist) - force plain "/I" instead.
# set() alone gets overwritten by Modules/Compiler/MSVC-CXX.cmake's own
# unconditional set() later in the compiler-detection pass; force it via
# the cache so that later set() (non-FORCE) calls can't win.
set(CMAKE_INCLUDE_SYSTEM_FLAG_C "/I" CACHE STRING "" FORCE)
set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "/I" CACHE STRING "" FORCE)

# mt.exe (manifest embedding) reliably fails under Wine (exit code 0x1,
# no useful diagnostic). Game DLLs don't need a manifest, so disable
# generation instead of embedding it, which also skips the mt.exe step
# CMake's Ninja/MSVC link wrapper would otherwise invoke.
set(CMAKE_EXE_LINKER_FLAGS_INIT "/MANIFEST:NO")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/MANIFEST:NO")
