# Build with the same GCC version/ABI baseline Valve uses for the
# official GoldSrc Linux DLLs (Steam Runtime "Scout", GCC 5.4.1) —
# intended to be run inside the registry.gitlab.steamos.cloud/steamrt/
# scout/sdk Docker image, which ships gcc-5/g++-5 alongside its
# default (older) gcc-4.8.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i386)

set(CMAKE_C_COMPILER gcc-5)
set(CMAKE_C_FLAGS -m32)
set(CMAKE_CXX_COMPILER g++-5)
set(CMAKE_CXX_FLAGS "-m32 -static-libgcc -static-libstdc++ -D_GLIBCXX_USE_CXX11_ABI=0")
