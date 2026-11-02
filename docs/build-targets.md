# Build-Targets: Steam/GoldSrc und Xash3D

## CLion

Dieses Verzeichnis ist ein CLion-CMake-Projekt. In CLion den Ordner
`re-project/` öffnen und das Preset **CLion orchestration** wählen. Die
sichtbaren Targets sind:

- `steam-goldsource` — baut nur die 32-Bit-Game-DLLs für einen Stock-
  Steam/GoldSrc-Mod.
- `xash3d-engine` — lädt Xash3D FWGS sowie seine Build-Abhängigkeiten als
  öffentliche Archive und baut die 32-Bit-Engine.
- `xash3d` — baut Engine und anschließend dieselben Gunman-DLLs in eine
  getrennte Xash3D-Payload.
- `xash3d-fork-status` — zeigt den vorbereiteten Fork-/Upstream-Status.

Für Code-Navigation im SDK zusätzlich `src/halflife-updated-gm/` als eigenes
CMake-Projekt öffnen. Das ist absichtlich kein `add_subdirectory`: dessen
CMake-Konfiguration erwartet einen Mod-Installationspfad und den 32-Bit-
Linux-Toolchain-File.

## Steam/GoldSrc

```bash
./scripts/build-steam-goldsource.sh
```

Ergebnis: `out/steam-goldsource/rewolf/dlls/gunman.so` und
`out/steam-goldsource/rewolf/cl_dlls/client.so`.

Der Build schreibt **nicht** nach `../Gunman-ENG-GER/rewolf`. Die Payload ist
absichtlich nur ein Entwicklungsartefakt; Assets, Karten und Konfiguration
werden später in eine separate Mod-Kopie gestaged. Erst wenn diese Kopie
vollständig ist, darf sie in einem Steam-Half-Life-Verzeichnis registriert
werden.

Der Standard ist `GUNMAN_ENGINE_TARGET=goldsource` und kompiliert mit
`GUNMAN_ENGINE_GOLDSRC=1`. Der explizite Xash-DLL-Test ohne Engine-Build ist
ebenfalls möglich:

```bash
GUNMAN_ENGINE_TARGET=xash3d ./scripts/build-steam-goldsource.sh
```

Er setzt `Gunman_Xash3D=ON` und damit `GUNMAN_ENGINE_XASH3D=1`. Neue
engine-spezifische C++-Logik muss in
`dlls/gunman_engine_target.h` über den vorhandenen `#if`/`#else`-Rahmen
abgegrenzt werden. Plattform oder Betriebssystem dürfen nicht als Proxy für
die Engine dienen.

## Xash3D

```bash
./scripts/build-xash3d.sh
```

Der Builder lädt Xash3D FWGS und alle für den Standard-Linux-Client nötigen
Submodule als öffentliche Tar-Archive nach `.cache/xash3d-archives/`. Damit
ist er unabhängig vom hier gestörten GitHub-Git-Transport. Der Quellbaum wird
pro Lauf temporär unter `build/xash3d-archive.*` aufgebaut und nach einem
erfolgreichen Lauf entfernt (`--keep-source` erhält ihn). Bei einem Fehler
oder Abbruch bleibt er für Diagnose bzw. inkrementelle Fortsetzung erhalten.
Die Engine und die
Game-Payload landen getrennt unter `out/xash3d/engine/` und
`out/xash3d/rewolf/`.

Der SDK-Fork ist fest auf i386 konfiguriert. Deshalb baut das Skript Xash3D
ohne Wafs `-8`-Option ebenfalls 32-Bit. Ein 64-Bit-Xash kann die daraus
entstehenden 32-Bit-Spiel-DLLs nicht laden. Auf amd64 werden die i386-
Abhängigkeiten aus der offiziellen FWGS-Dokumentation benötigt. Fehlen
Compiler, Multilib oder SDL2-i386, installiert das Skript sie auf
Debian/Ubuntu automatisch über `apt-get` (bei Bedarf mit `sudo`) und prüft
anschließend einen echten `gcc -m32`-/SDL2-Link. Opus, Ogg, Vorbis, bzip2 und
weitere Engine-Submodule werden mit Wafs `--enable-bundled-deps` als i386
gebaut; dadurch werden keine inkompatiblen 64-Bit-Audiobibliotheken gelinkt.

Die Templates setzen die vorhandenen Gunman-Grenzen (`max_edicts 900`,
`max_tempents 500`, `max_beams 128`, `max_particles 4096`) fort. Erweiterte
Limits, Renderer/Shaders und HD-Texturen sind noch **keine** Build-Optionen:
sie erfordern eine spätere, separat getestete Engine-/Asset-Entscheidung und
dürfen die GoldSrc-Payload nicht verändern.

## Fork für die spätere Standalone-Engine

Der benötigte Fork kann erst nach Anlage eines GitHub-Forks im Projektkonto
bestimmt werden. Das Setup trennt danach die Remotes eindeutig:

```bash
./scripts/prepare-xash3d-fork.sh <dein-github-fork-url>
```

- `origin` zeigt anschließend auf den Projektfork und ist der einzige Remote
  für spätere Engine-Änderungen.
- `upstream` bleibt `https://github.com/FWGS/xash3d-fwgs.git` für Updates.

Die spätere Standalone-Arbeit — Start ohne Steam-Abhängigkeit sowie ein
BSP-Hintergrund im Main Menu — wird ausschließlich im Fork entwickelt. Vor
der Änderung müssen der aktive Menü-Codepfad und die passende Fork-/Branch-
Basis im dann aktuellen Xash-Checkout ermittelt werden; weder Menü- noch
Renderer-Code wird hier vorab spekulativ gepatcht.
