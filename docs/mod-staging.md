# Mod-Staging

`scripts/stage-mod.sh` erzeugt die lauffaehigen Entwicklungsbaeume unter
`out/mod`:

```text
out/mod/
|- steam/rewolf/       # Steam/GoldSource-Mod mit GoldSource-DLLs
`- xash3d/             # Xash3D-Standalone-Laufzeit
   |- xash3d, libxash.so, ...
   `- rewolf/          # gleichnamige Mod mit Xash3D-DLLs
```

## Quelldaten

- `mod-src/base/` ist die gemeinsame Quelle fuer Laufzeitdateien beider
  Varianten, etwa `tiles.txt`, Dokumentation und `resource/background/`.
- `mod-src/steam/` enthaelt nur Steam/GoldSource-spezifische Dateien wie
  `liblist.gam`.
- `mod-src/xash3d/` enthaelt nur Xash3D-spezifische Dateien wie
  `gameinfo.txt`.
- `textures-src/rewolf/retail/` wird beim Staging zu `rewolf.wad` gepackt.
- Standardquelle für `rewolf.wad` ist `textures-src/base/rewolf/`. Fehlt sie,
  verwendet das Staging als Rückfall die ältere Quelle
  `textures-src/rewolf/retail/`.
- Zusätzlich packt das Staging die verfügbaren extrahierten WAD-Quellen
  `cached`, `decals`, `gfx` und `update`. `cached` und `gfx` werden korrekt
  als QPic-WADs gepackt; die anderen als Miptex-WADs.
- `./textures-src/extract_base_wads.sh` extrahiert sämtliche `.wad` aus
  `mod-src/base/` nach `textures-src/base/<wad-name>/`. Dieser Zielbaum ist
  abgeleitet und darf bei einer Neu-Extraktion ersetzt werden. Der Extraktor
  schreibt GoldSrc-Miptex-Texturen sowie QPic-Menü-/Ladebilder als BMP.
- `textures-src/xash-overrides/<wad-name>/` enthält optionale HD-TGAs. Sie
  überschreiben die automatisch aus `textures-src/base/<wad-name>/` erzeugten
  TGAs. Beim Xash3D-Staging landen sie unter `rewolf/materials/`; `decals`
  wird dabei nach `materials/decals/`, alle anderen WADs nach
  `materials/common/` geschrieben.
- `tools/stage_xash_materials.py` erzeugt 32-Bit-TGA-Fallbacks aus den
  palettierten BMPs. Bei `{`-Texturen wird der GoldSrc-Index 255 als
  Transparenz übernommen.
- Beim Xash3D-Staging erzeugt `stage-mod.sh` automatisch aktuelle
  `nearest-2x`-TGAs und verwendet sie als HD-Basis. Gleichnamige Dateien aus
  `textures-src/xash-overrides/<wad-name>/` behalten Vorrang.
  Vorhandene Vorschauen werden wiederverwendet; mit
  `GUNMAN_REFRESH_XASH_HD=1 ./scripts/stage-mod.sh` wird ihre Neuberechnung
  erzwungen.
- `scripts/upscale-xash-textures.sh` erzeugt getrennte 2x-Vorschauen mit den
  Profilen `nearest-2x`, `lanczos-2x` und `ai-2x`; Details stehen in
  `docs/xash-texture-upscaling.md`.
- Alle `.mdl` aus `models-src/` werden nach `models/` kopiert. Namenskonflikte
  brechen den Vorgang ab.
- `spr-src/` ist die Arbeitsquelle fuer Sprites; siehe
  `docs/sprite-tools.md`.

`maps/`, `sound/` und `sprites/` werden schon angelegt. Ihre rekonstruierten
Inhalte koennen spaeter aus eigenen Quellordnern in das Staging aufgenommen
werden.

## Ausfuehrung

```bash
./scripts/stage-mod.sh
```

Mit `--build` werden davor beide DLL-Ziele gebaut. Das Script ersetzt nur die
beiden erzeugten Zielordner `out/mod/steam` und `out/mod/xash3d`.
