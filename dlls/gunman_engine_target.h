#pragma once

// Target boundary for the Gunman reconstruction.  Both engines currently
// share the classic HLSDK callback ABI; engine extensions belong behind this
// boundary only after their FWGS contract has been compared and verified.
#if defined(GUNMAN_ENGINE_XASH3D) && defined(GUNMAN_ENGINE_GOLDSRC)
#error "Gunman DLL build cannot target Xash3D and GoldSrc at the same time"
#endif

#if defined(GUNMAN_ENGINE_XASH3D)

inline constexpr bool kGunmanTargetIsXash3D = true;
inline constexpr bool kGunmanTargetIsGoldSrc = false;
inline constexpr const char* kGunmanEngineTargetName = "Xash3D FWGS";

#else

// GoldSrc is the conservative default.  It deliberately also covers a build
// with no target definition, so older/manual project files remain compatible.
inline constexpr bool kGunmanTargetIsXash3D = false;
inline constexpr bool kGunmanTargetIsGoldSrc = true;
inline constexpr const char* kGunmanEngineTargetName = "Steam GoldSrc";

#endif
