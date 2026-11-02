<p align="center">
  <img src="gunman_chronicles_originalgetreu.svg" alt="Gunman Chronicles SDK" width="100%">
</p>

# Gunman Chronicles SDK

🇬🇧 [English version](README.en.md)

Ein Rekonstruktions-SDK für **Gunman Chronicles** (Rewolf Software, 2000), einen Half-Life-Mod auf der GoldSrc-Engine. Der Code wird als saubere Neuimplementierung der Spiellogik auf Basis des Half-Life-Updated-SDK geschrieben. Die Grundlage sind belegte Befunde aus Reverse Engineering der originalen Binärdateien und Karten, nicht Vermutungen.

> **Status: Alpha 1 – frühe Phase.** Grundbausteine und ein großer Teil der Monster sind vorhanden. Der Umfang ist nicht vollständig, und es gibt bekannte Fehler und offene Punkte (siehe [Bekannte Probleme](#bekannte-probleme)). Nicht alles ist durch Laufzeittests bestätigt.

Die Basis ist das Half-Life-SDK aus dem Projekt [Half-Life Updated](https://github.com/twhl-community/halflife-updated). Lizenz und Original-Hinweise stehen in [`README_HLSDK.md`](README_HLSDK.md).

---

## Inhalt

- [Ziel](#ziel)
- [Stand](#stand)
- [Projektstruktur](#projektstruktur)
- [Dokumentation: `docs/` und `findings/`](#dokumentation-docs-und-findings)
- [Befund-Regeln](#befund-regeln)
- [Wichtige Befunde](#wichtige-befunde)
- [Monster und Entities](#monster-und-entities)
- [Bauen](#bauen)
- [Arbeitsweise](#arbeitsweise)
- [Bekannte Probleme](#bekannte-probleme)
- [Lizenz](#lizenz)

---

## Ziel

Gunman Chronicles soll als Quelltext-SDK nachgebaut werden, damit Mapper und Entwickler die Spiellogik lesen, ändern und mit eigenen Inhalten erweitern können. Dazu gehören:

- **Server-DLL** (`dlls/`): Monster, Waffen, Trigger, Items, Spielregeln, Dekorationsobjekte
- **Client-DLL** (`cl_dll/`): HUD, Waffen-Events, Zoom, Temperatur-Anzeige, Tank-Anzeige, Particleman
- **Gemeinsamer Code** (`game_shared/`, `pm_shared/`, `common/`, `public/`) und die Entity-Definitionen (`fgd/halflife.fgd`)

Das Ziel ist **Verhaltensnähe zum Original**. Abweichungen werden dokumentiert und nicht still übernommen.

## Stand

| Bereich | Stand |
| --- | --- |
| Spielerlogik, Waffen, Basis-Monster (HL-Erbe) | größtenteils vorhanden, teils fehlerhaft |
| Gunman-spezifische Monster (Rust-, Mayan-, Rebar-Karten usw.) | großteils vorhanden, teils mit Platzhalter-Werten |
| Gunman-eigene Entities (`gunman_custom_entities.cpp`) | Grundgerüst, wird klassenweise ergänzt |
| Client-HUD (Gesundheit, Munition, Zoom, Temperatur, Tank) | vorhanden, im Detail ungetestet |
| FGD (`fgd/halflife.fgd`) | erweitert, nicht vollständig gegen alle Karten geprüft |
| Build (CMake, Linux/Makefile, Visual Studio) | vorhanden, Xash3D- und GoldSrc-Ziel getrennt |

Der laufende Fortschritt steht im Session-Log in [`STATUS.md`](STATUS.md) (neueste Einträge oben).

## Projektstruktur

| Ordner / Datei | Inhalt |
| --- | --- |
| `dlls/` | Server-Logik (Monster, Waffen, Trigger, kartenspezifische Dateien) |
| `cl_dll/` | Client-Logik (HUD, Waffen-Events, Zoom, Tank, Temperatur) |
| `game_shared/` | Gemeinsamer Code, u. a. Bot- und Navigationssystem |
| `pm_shared/` | Bewegungs-Code (Player Movement) |
| `common/`, `public/` | Gemeinsame Header und Schnittstellen |
| `engine/`, `network/`, `external/` | Engine-Schnittstellen und Abhängigkeiten (SDL2) |
| `fgd/halflife.fgd` | Entity-Definitionen für den Hammer-Editor |
| `cmake/` | Toolchains und Installationsskripte |
| `linux/` | Makefile und Linux-Build-Hilfen |
| `projects/vs2019/` | Visual-Studio-Projekte |
| `utils/` | Half-Life-Werkzeuge (Compiler, Viewer, Packer usw.) |
| `docs/` | Anleitungen zu Build, Staging und Asset-Werkzeugen (siehe unten) |
| `findings/` | Reverse-Engineering-Befunde zum Original (siehe unten) |
| `STATUS.md` | Laufendes Entwicklungs-Log |

### Dateinamen-Konvention in `dlls/`

Die Dateien sind nach Map oder Gegnergruppe benannt, nicht nach Klassen:

- `human_*.cpp`: menschliche Figuren (Gunman, Bandit, Scientist, Demoman, Chopper, Unarmed) auf Basis von `human_base.cpp`
- `rust*_misc.cpp`: Monster und Objekte der Rust-Karten
- `mayan*_misc.cpp`: Monster und Objekte der Mayan-Karten
- `rebar_*.cpp`: Rebar-Karten (Fauna, Misc)
- `cinematic2_misc.cpp`, `end_misc.cpp`, `west1_misc.cpp`: Zwischensequenzen und Abschlusskarten
- `gunman_custom_entities.cpp`: Entities ohne SDK-Vorbild
- `beamgun.cpp`, `chemgun.cpp`, `dml*.cpp`, `minigun.cpp`, `vehicle_tank.cpp`: Waffen und Fahrzeuge
- `gunman_engine_target.h`: Auswahl des Engine-Ziels (GoldSrc oder Xash3D)

## Dokumentation: `docs/` und `findings/`

### `docs/`: Anleitungen für Entwickler

| Datei | Inhalt |
| --- | --- |
| [`docs/build-targets.md`](docs/build-targets.md) | Build-Ziele für GoldSrc und Xash3D, CLion-Targets, Ausgabepfade |
| [`docs/mod-staging.md`](docs/mod-staging.md) | Aufbau des lauffähigen Mod-Baums (`out/mod`), Quelldaten, WAD-Packen |
| [`docs/sprite-tools.md`](docs/sprite-tools.md) | Python-Werkzeug `spr_tool.py` für das GoldSrc-SPR-Format |
| [`docs/steam-detail-textures.md`](docs/steam-detail-textures.md) | Erzeugung von Detail-Texturen für Steam/GoldSrc |
| [`docs/xash-texture-upscaling.md`](docs/xash-texture-upscaling.md) | Textur-Hochskalierung für Xash3D (nearest, lanczos, KI) |
| [`docs/tutorials/`](docs/tutorials) | Einrichtung von dotnet-script für das Packen der Mod-Dateien |

Die `docs/`-Anleitungen verweisen auf Skripte wie `scripts/build-xash3d.sh`. Diese Skripte liegen **nicht** in diesem Repository, sondern im separaten Arbeitsverzeichnis der Reverse-Engineering-Arbeit (siehe [Arbeitsweise](#arbeitsweise)). Die Anleitungen beschreiben deshalb den Ablauf, nicht einen in diesem Repo lauffähigen Befehl.

### `findings/`: Befunde zum Original

Der Ordner enthält **302 Dateien** (Markdown, Textdateien, CSV, TSV), die aus der Reverse-Engineering-Arbeit an `gunman.dll`, `client.dll` und den Retail-Karten stammen. Die Dateien sind nach Teilgebieten sortiert:

| Unterordner | Markdown-Dateien | Inhalt |
| --- | --- | --- |
| `entities/` | 198 | Entity-Klassen: Vtable-Zuordnung, Familien, Klassen-Listen, Entity-Review |
| `weapons/` | 22 | Waffen: `PrimaryAttack`, Save-Felder, Feuer-Helfer, Sonderfälle |
| `client/` | 5 | Client-DLL: HUD, Event-Funktionen, Klassen |
| `engine/` | 1 | Engine-Hinweise (Xash3D) |
| `fgd-diff/` | 2 | Abgleich der FGD mit dem Spiel |
| `symbols/` | 1 | Symbol-Quellen |
| Hauptordner | 31 | Übergreifende Auswertungen, Audits, Plattform-Vergleiche |

Zusätzlich gibt es Rohdaten ohne Markdown: Exporttabellen (`*_exports_raw.txt`), Ghidra-Funktionslisten (`*_functions_ghidra.tsv`), Klassennamen-Listen (`*_classnames_*.txt`), Vtable-Auflösungen (`entity_vtable_resolution_*.txt`) und Animationsreports (`animation_activity_report.csv`).

## Befund-Regeln

Die Befunde folgen den Regeln aus der Projektcharta (`agents.md` im Arbeitsverzeichnis):

1. **Primärquelle vor Plausibilität.** Ein Verhalten gilt erst als Fakt, wenn es durch eine Retail-Binärdatei, eine Retail-BSP, lokale Assets, Map-Quellen oder einen reproduzierbaren Laufzeittest belegt ist.
2. **Hypothesen sind als Hypothesen markiert.** Jeder Befund nennt Quelle, Methode, Konfidenz und offene Fragen.
3. **Design-Ideen sind getrennt.** Vorschläge, die kein Befund über das Original sind, stehen in `findings/rebuild_ideas.md` und werden nicht als Originalverhalten ausgegeben.
4. **Externe Quellen ersetzen keine Primärbelege.** Sie dienen als Vergleich (siehe [Wichtige Befunde](#wichtige-befunde)).
5. **Retail ist die Vergleichsbasis.** Die E3-Beta und andere Varianten werden ausdrücklich gekennzeichnet.

## Wichtige Befunde

| Datei | Warum sie wichtig ist |
| --- | --- |
| [`findings/sdk_foundation_matrix.md`](findings/sdk_foundation_matrix.md) | Ordnet Gunman-Klassen in drei Stufen ein: **A** direkt aus dem SDK übernehmbar, **B** von einer SDK-Basisklasse ableiten, **C** nur Muster, eigenständig schreiben. Die wichtigste Datei für die Architektur. |
| [`findings/hlsdk_portable_compatibility.md`](findings/hlsdk_portable_compatibility.md) | Vergleich mit FWGS `hlsdk-portable` und die Zielschicht `GUNMAN_ENGINE_GOLDSRC` / `GUNMAN_ENGINE_XASH3D`. |
| [`findings/entity_class_families.md`](findings/entity_class_families.md) | Zuordnung von 208 Map-Klassennamen zu C++-Klassen über den Vtable-Zeiger. Mehrere Klassennamen können dieselbe Klasse sein. |
| [`findings/open_items_audit_2026-09-05.md`](findings/open_items_audit_2026-09-05.md) | Prüft alle offenen Punkte der Befunde gegen den Code. Hat mehrere Einträge als erledigt markiert und zwei falsche Befunde korrigiert. |
| [`findings/gunman-runtime-open-items-2026-09-04.md`](findings/gunman-runtime-open-items-2026-09-04.md) | Liste der Laufzeit-Lücken mit benötigtem Test. Stand 04.09.; einzelne Punkte sind inzwischen erledigt (siehe Audit). |
| [`findings/weapons/primaryattack_breakthrough.md`](findings/weapons/primaryattack_breakthrough.md) | Die echten `PrimaryAttack`-Adressen der Waffen. Vor jeder Waffenänderung lesen, weil einzelne `weapon_*.md` darauf gestützt sind. |
| [`findings/rebuild_ideas.md`](findings/rebuild_ideas.md) | Design-Ideen für einen künftigen Rebuild. **Keine** Befunde über das Original. |
| [`findings/freegunman_crossref.md`](findings/freegunman_crossref.md) | Vergleich mit `eukara/freegunman` (QuakeC, FTEQW). Nur Verhaltens- und Strukturreferenz, kein Code. |
| [`findings/official_manual_and_website.md`](findings/official_manual_and_website.md) | Handbuch und archivierte Herstellerseiten als Primärquellen für Spielregeln. |

**Funktionsstatus:** Eine verlinkte Liste aller Gunman-Entities mit Code-Stelle, Review-Status und Befund steht in [`docs/gunman-funktionsstatus.md`](docs/gunman-funktionsstatus.md).

**Hinweis zur Vollständigkeit:** Die Ordner `docs/` und `findings/` liegen im Repo als Kopie der Arbeitsstände. Die vollständige Befundsammlung umfasst weitere Dateien (Entscheidungsprotokolle, Ghidra-Skripte, Karten-Quellen), die nicht im Repo liegen.

## Monster und Entities

Die Klassennamen stehen als `LINK_ENTITY_TO_CLASS` in den Quelldateien. Auswahl nach Gruppe:

**Menschen (verbündet oder neutral)**
`monster_human_gunman`, `monster_human_unarmed`, `monster_human_bandit` / `monster_human_grunt`, `monster_human_scientist`, `monster_human_demoman`, `monster_human_chopper` / `monster_apache`, `monster_manta`

**Rust-Karten**
`monster_rustbit`, `monster_rustbit_friendly`, `monster_beak`, `monster_rustgunr` / `monster_gunner`, `monster_gunner_friendly`, `monster_rustbot`, `monster_rustbot_friendly`, `monster_rustflier`, `monster_xenome`, `monster_tubequeen`, `monster_tube`, `monster_maggot`, `monster_critter`, `monster_beakbirther`

**Mayan-Karten**
`monster_hatchetfish`, `monster_tank`, `monster_darttrap`, `monster_raptor` / `monster_rheptor`, `monster_renesaur`, `monster_gator`, `monster_microraptor`, `monster_largescorpion`, `monster_scorpion`, `lava_god`

**Rebar, Endgegner und Sonstiges**
`monster_tube_embryo`, `monster_xenome_embryo`, `monster_aigirl`, `entity_clustergod`, `monster_endboss`, `monster_trainingbot`, `lightning_bug`, `ball_lightning`

**Nur Cut-Content (nicht eingebunden)**
`monster_penta`, `monster_battery` liegen in `cutcontent_penta_battery.cpp`.

**Waffen und Munition**
`weapon_beamgun`, `weapon_chemgun`, `weapon_dml`, `weapon_minigun`, `weapon_aicore`, Munitions-Entities (`ammo_*`)

**Map-Logik und Dekoration**
`trigger_gunmanteleport`, `trigger_kill`, `trigger_tank*`, `item_armor`, `item_gascan`, `player_speaker`, `random_speaker`, `entity_digit`, `decore_*` (zahlreiche Dekorationsobjekte)

Eine vollständige Liste ergibt sich aus `grep -rn LINK_ENTITY_TO_CLASS dlls/`.

## Bauen

Die Grundlagen stammen aus dem Half-Life SDK. Details stehen in [`BUILDING.md`](BUILDING.md).

**Linux (CMake, GCC 9 oder neuer, `g++-multilib`, `libgl1-mesa-dev`):**

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/LinuxToolchain.cmake \
         -DCMAKE_INSTALL_PREFIX=/pfad/zum/Half-Life/gunman
cmake --build .
```

**Engine-Ziel wählen:**

| Option | Wirkung |
| --- | --- |
| `-DGunman_Xash3D=OFF` (Standard) | Build für Steam GoldSrc, setzt `GUNMAN_ENGINE_GOLDSRC=1` |
| `-DGunman_Xash3D=ON` | Build für Xash3D FWGS, setzt `GUNMAN_ENGINE_XASH3D=1` |

Engine-spezifischer Code gehört hinter die `#if`/`#else`-Grenze in `dlls/gunman_engine_target.h`. Die Plattform (Linux, Windows) darf nicht als Ersatz für das Engine-Ziel dienen.

**Weitere Optionen:** `HalfLife_LTO` (Link-Time-Optimierung), `HalfLife_HLDS_DIRECTORY` (Dedicated Server).

**Windows:** Visual Studio 2019 oder neuer, Projekte in `projects/vs2019/`, Plattform Win32.

> Hinweis: Die Build-Anleitung stammt aus dem Half-Life SDK. Ob der aktuelle Stand mit allen genannten Toolchains fehlerfrei baut, ist nicht in jedem Fall geprüft.

## Arbeitsweise

Die Entwicklung erfolgt Klasse für Klasse, in der Reihenfolge, in der die Karten der Story sie brauchen:

1. **Befunde lesen.** Vorhandene Befunde und `STATUS.md` prüfen, bevor etwas neu untersucht wird.
2. **Klasse bestimmen.** Verhalten, Felder und Callbacks aus der Dekompilierung von `gunman.dll` ableiten. Den Klassennamen über Exporte, Vtables und Karten-Entities zuordnen.
3. **Im SDK-Stil nachbauen.** Die Stufe aus der Matrix (A, B oder C) bestimmt, ob SDK-Code übernommen, abgeleitet oder neu geschrieben wird. Quelladressen (z. B. `@0x100ae360`) und Vtable-Adressen stehen im Kommentarkopf.
4. **Bauen und prüfen.** Build muss erfolgreich sein. Laufzeittests laufen über das Xash3D-Verfahren aus den `docs/`-Anleitungen.
5. **Dokumentieren.** Eintrag in `STATUS.md` mit dem Stand: was portiert wurde, was offen ist, was nicht getestet wurde.

Das Reverse-Engineering-Arbeitsverzeichnis (Ghidra-Projekte, Extraktionswerkzeuge, Karten-Quellen, Staging-Skripte) liegt **außerhalb** dieses Repositorys. Nur exportierte und reproduzierbare Ergebnisse gehören hierher.

## Bekannte Probleme

Aus `STATUS.md`, `findings/` und dem Code. Stand: 05.09.2026 für die Befunde, 02.11.2026 für `STATUS.md`.

**Offen laut Befunden und Code:**

- **Scientist-Target-Callback** (Session 309): Der parallel ausgelöste Retail-Callback ist nicht portiert, weil das SDK-Feld und das Keyvalue noch ungeklärt sind.
- **Gunner-Blutspray** (Session 275): Der Ursprung des Retail-Effekts ist offen. Aktuell wird `TraceResult::vecEndPos` verwendet. Ein gezielter Sicht- und Angriffstest fehlt.
- **Freundliche Gunman-Kämpfe**: Der Kampf ist eine generische Hitscan-Annäherung. Die Event-IDs und `gn_kill`/`gn_attack`-Abläufe sind nicht rekonstruiert (laut Laufzeit-Lückenliste).
- **Scripted-Sequenzen mit Spielerbezug** (`m_fMoveTo` 6/7/8): Motion-Schedules sind abgesichert, die Auswahl des Spielers ist nicht rekonstruiert.
- **Rustbot und Xenome**: Mehrere Lebenspunkt- und Schadenswerte sind Platzhalter, der Rustbot-Strahl ist eine Näherung.
- **Waffenbefunde**: `weapon_minigun` hat eine ungeklärte Interface-Flag-Prüfung (`DAT_101322d8`). `weapon_beamgun` nutzt laut Befund möglicherweise `p_egon.mdl` aus `valve/` statt aus dem Gunman-Ordner.
- **DML-Customization**: Nicht reproduziert sind das Mehrraketen-Schwarm-Verhalten, die Zielsuche über `laser_spot` und der Grenaden-Tripwire. Die Items sind in keiner Karte platziert, Tests nur über die Konsole.
- **Laufzeittests fehlen** für viele Monster. Nur einzelne Pfade wurden in der Karte RUST4A getestet.
- **TODO/FIXME-Marker**: rund 60 Stellen in etwa 33 Dateien von `dlls/` und `cl_dll/`.
- **Cut-Content** (`cutcontent_penta_battery.cpp`) ist nicht eingebunden.
- **FGD und Karten-Keyvalues** sind nicht vollständig gegen alle Karten geprüft.

**Bereits erledigt (laut Audit vom 05.09.2026):** `weapon_chemgun`, `weapon_dmlGrenade`, `weapon_beamgun` (`PrimaryAttack` korrigiert), `weapon_minigun` (Streuung und Hitze), `weapon_dml` (Ziel-Lock und Dual-Salve), die Gunner-Familie (war bereits implementiert, Befund korrigiert).

Fehler bitte mit Karten-Name, Klasse und Schritten melden.

## Lizenz

Dieses Projekt basiert auf dem **Half-Life 1 SDK** von Valve Corp. und unterliegt dessen Lizenz (siehe [`LICENSE`](LICENSE) und [`README_HLSDK.md`](README_HLSDK.md)). Die Lizenz verlangt, dass Verteilungen des SDK oder wesentlicher Teile den Copyright-Hinweis und den Lizenztext enthalten.

Gunman Chronicles ist ein eigenständiges Spiel. Die Rechte an seinen Inhalten (Figuren, Karten, Assets) liegen bei den jeweiligen Rechteinhabern und sind nicht durch die Lizenz dieses SDK abgedeckt.