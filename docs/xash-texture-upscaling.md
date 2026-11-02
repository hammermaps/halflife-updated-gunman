# Xash3D texture upscaling

Die Quellen unter `textures-src/base/` werden nie verändert. Kandidaten landen
zuerst getrennt in `textures-src/xash-previews/<profil>/<wad-name>/`.
Nur der explizite Schalter `--publish` kopiert sie nach
`textures-src/xash-overrides/<wad-name>/`, von wo `stage-mod.sh` sie für
Xash3D übernimmt.

```bash
# Pixelgenaue Verdopplung; gut für HUD, Schrift, Logos und Decals.
./scripts/upscale-xash-nearest.sh decals

# Glatte 2x-Vergrößerung; gut als Vergleichskandidat für Welttexturen.
./scripts/upscale-xash-lanczos.sh gmdemo

# Nach Sichtprüfung den gerade erzeugten Satz als Xash-Override aktivieren.
./scripts/upscale-xash-lanczos.sh gmdemo --publish
```

`lanczos-2x` benötigt ImageMagick. `ai-2x` verwendet bewusst den portablen
`waifu2x-ncnn-vulkan`-Aufruf und benötigt dessen Pfad:

```bash
./scripts/upscale-xash-ai.sh gmdemo
```

`upscale-xash-ai.sh` lädt bei der ersten Ausführung automatisch die offizielle
portable Linux-Ausgabe von `waifu2x-ncnn-vulkan` nach `.cache/tools/`. Ein
vorhandener `GUNMAN_AI_UPSCALER` hat Vorrang. Mit
`GUNMAN_WAIFU2X_VERSION=<release>` kann die Release-Version bewusst gewählt
werden.

Falls Vulkan nur über einen Software-Renderer verfügbar ist, erzwingt
`GUNMAN_AI_GPU=-1` den CPU-Modus:

```bash
GUNMAN_AI_GPU=-1 ./scripts/upscale-xash-ai.sh cached
```

KI-Ergebnisse nur für Welttexturen verwenden und einzeln prüfen: sie können
falsche Details erzeugen. Für `cached`, `gfx`, `fonts` und `decals` ist
`nearest-2x` die empfohlene Voreinstellung. Transparenz bei GoldSrc-Namen mit
`{` wird im nearest-Profil korrekt aus Palette-Index 255 in TGA-Alpha
übernommen.
