# Steam and GoldSrc detail textures

scripts/build-steam-detail-textures.sh scans every BSP in mod-src/base/maps.
It reads the BSP texture lump, resolves names against the extracted WAD BMP
sources, and generates these directories:

    textures-src/steam-detail/generated/gfx/detail
    textures-src/steam-detail/generated/maps

The standard outline profile creates a restrained 128 pixel monochrome
high-frequency tile with additional local contrast and edge sharpening.
It overlays the original WAD or BSP texture; it is not an HD replacement.
Transparent, water and sky textures are excluded.

Set GUNMAN_STEAM_DETAIL_PROFILE=neutral before the script or stage command to
generate the former softer high-frequency-only variant for comparison.

Place reviewed alternatives at textures-src/steam-detail-overrides using the
same texture filename. They replace a generated tile. stage-mod.sh builds
these assets and copies them only into the Steam output tree. The staged
Steam autoexec.cfg explicitly sets r_detailtextures to 1; valve.rc executes
that file during startup.

Run manually:

    ./scripts/build-steam-detail-textures.sh
