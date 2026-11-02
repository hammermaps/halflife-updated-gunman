# Gunman Chronicles SDK

🇩🇪 [Deutsche Version](README.md)

A reconstruction SDK for **Gunman Chronicles** (Rewolf Software, 2000), a Half-Life mod on the GoldSrc engine. The code is a clean-room reimplementation of the game logic, built on the Half-Life Updated SDK. It is based on evidence gathered by reverse engineering the original binaries and maps, not on guesswork.

> **Status: Alpha 1, early stage.** Core building blocks and a large part of the monsters are in place. The scope is not complete, and there are known bugs and open items (see [Known issues](#known-issues)). Not everything has been confirmed by runtime tests.

The base is the Half-Life SDK from the [Half-Life Updated](https://github.com/twhl-community/halflife-updated) project. License and original notices are in [`README_HLSDK.md`](README_HLSDK.md).

---

## Contents

- [Goal](#goal)
- [Status](#status)
- [Project layout](#project-layout)
- [Documentation: `docs/` and `findings/`](#documentation-docs-and-findings)
- [Evidence rules](#evidence-rules)
- [Key findings](#key-findings)
- [Monsters and entities](#monsters-and-entities)
- [Building](#building)
- [Workflow](#workflow)
- [Known issues](#known-issues)
- [License](#license)

---

## Goal

The aim is to rebuild Gunman Chronicles as a source SDK, so that mappers and developers can read, change, and extend the game logic. This covers:

- **Server DLL** (`dlls/`): monsters, weapons, triggers, items, game rules, decoration objects
- **Client DLL** (`cl_dll/`): HUD, weapon events, zoom, temperature display, tank display, particleman
- **Shared code** (`game_shared/`, `pm_shared/`, `common/`, `public/`) and entity definitions (`fgd/halflife.fgd`)

The goal is **behavioral fidelity to the original**. Deviations are documented, not silently accepted.

## Status

| Area | State |
| --- | --- |
| Player logic, weapons, base monsters (inherited from HL) | mostly present, partly buggy |
| Gunman-specific monsters (Rust, Mayan, Rebar maps, etc.) | largely present, some use placeholder values |
| Gunman-only entities (`gunman_custom_entities.cpp`) | skeleton, filled in class by class |
| Client HUD (health, ammo, zoom, temperature, tank) | present, not tested in detail |
| FGD (`fgd/halflife.fgd`) | extended, not fully checked against all maps |
| Build (CMake, Linux/Makefile, Visual Studio) | present, Xash3D and GoldSrc targets are separate |

Ongoing progress is logged in [`STATUS.md`](STATUS.md) (newest entries first).

## Project layout

| Folder / file | Contents |
| --- | --- |
| `dlls/` | Server logic (monsters, weapons, triggers, map-specific files) |
| `cl_dll/` | Client logic (HUD, weapon events, zoom, tank, temperature) |
| `game_shared/` | Shared code, including the bot and navigation system |
| `pm_shared/` | Player movement code |
| `common/`, `public/` | Shared headers and interfaces |
| `engine/`, `network/`, `external/` | Engine interfaces and dependencies (SDL2) |
| `fgd/halflife.fgd` | Entity definitions for the Hammer editor |
| `cmake/` | Toolchains and install scripts |
| `linux/` | Makefile and Linux build helpers |
| `projects/vs2019/` | Visual Studio projects |
| `utils/` | Half-Life tools (compilers, viewers, packers, etc.) |
| `docs/` | Guides on building, staging, and asset tools (see below) |
| `findings/` | Reverse-engineering findings about the original (see below) |
| `STATUS.md` | Ongoing development log |

### File naming in `dlls/`

Files are named after maps or enemy groups, not after classes:

- `human_*.cpp`: human characters (Gunman, Bandit, Scientist, Demoman, Chopper, Unarmed) based on `human_base.cpp`
- `rust*_misc.cpp`: monsters and objects from the Rust maps
- `mayan*_misc.cpp`: monsters and objects from the Mayan maps
- `rebar_*.cpp`: Rebar maps (fauna, misc)
- `cinematic2_misc.cpp`, `end_misc.cpp`, `west1_misc.cpp`: cinematics and end maps
- `gunman_custom_entities.cpp`: entities with no SDK counterpart
- `beamgun.cpp`, `chemgun.cpp`, `dml*.cpp`, `minigun.cpp`, `vehicle_tank.cpp`: weapons and vehicles
- `gunman_engine_target.h`: selects the engine target (GoldSrc or Xash3D)

## Documentation: `docs/` and `findings/`

### `docs/`: developer guides

| File | Contents |
| --- | --- |
| [`docs/build-targets.md`](docs/build-targets.md) | GoldSrc and Xash3D build targets, CLion targets, output paths |
| [`docs/mod-staging.md`](docs/mod-staging.md) | Layout of the runnable mod tree (`out/mod`), source data, WAD packing |
| [`docs/sprite-tools.md`](docs/sprite-tools.md) | `spr_tool.py`, a Python tool for the GoldSrc SPR format |
| [`docs/steam-detail-textures.md`](docs/steam-detail-textures.md) | Generating detail textures for Steam/GoldSrc |
| [`docs/xash-texture-upscaling.md`](docs/xash-texture-upscaling.md) | Texture upscaling for Xash3D (nearest, lanczos, AI) |
| [`docs/tutorials/`](docs/tutorials) | Setting up dotnet-script for packing mod files |

The `docs/` guides refer to scripts such as `scripts/build-xash3d.sh`. Those scripts are **not** in this repository. They live in the separate reverse-engineering workspace (see [Workflow](#workflow)). The guides therefore describe the procedure, not a command that runs inside this repo.

### `findings/`: findings about the original

The folder contains **302 files** (Markdown, text, CSV, TSV) from the reverse-engineering work on `gunman.dll`, `client.dll`, and the retail maps. They are sorted by subject:

| Subfolder | Markdown files | Contents |
| --- | --- | --- |
| `entities/` | 198 | Entity classes: vtable mapping, families, class lists, entity review |
| `weapons/` | 22 | Weapons: `PrimaryAttack`, save fields, fire helpers, special cases |
| `client/` | 5 | Client DLL: HUD, event functions, classes |
| `engine/` | 1 | Engine notes (Xash3D) |
| `fgd-diff/` | 2 | Comparison of the FGD with the game |
| `symbols/` | 1 | Symbol sources |
| Top level | 31 | Cross-cutting analyses, audits, platform comparisons |

There are also raw data files without Markdown: export tables (`*_exports_raw.txt`), Ghidra function lists (`*_functions_ghidra.tsv`), class name lists (`*_classnames_*.txt`), vtable resolutions (`entity_vtable_resolution_*.txt`), and an animation report (`animation_activity_report.csv`).

## Evidence rules

The findings follow the rules of the project charter (`agents.md` in the workspace):

1. **Primary source before plausibility.** A behavior counts as fact only when a retail binary, a retail BSP, local assets, map sources, or a reproducible runtime test supports it.
2. **Hypotheses are labeled as hypotheses.** Each finding names its source, method, confidence, and open questions.
3. **Design ideas are kept separate.** Proposals that are not findings about the original live in `findings/rebuild_ideas.md` and are not presented as original behavior.
4. **External sources do not replace primary evidence.** They serve as comparisons (see [Key findings](#key-findings)).
5. **Retail is the reference.** The E3 beta and other variants are always marked as such.

## Key findings

| File | Why it matters |
| --- | --- |
| [`findings/sdk_foundation_matrix.md`](findings/sdk_foundation_matrix.md) | Sorts Gunman classes into three levels: **A** reusable from the SDK as is, **B** derive from an SDK base class, **C** pattern only, write from scratch. The most important file for architecture. |
| [`findings/hlsdk_portable_compatibility.md`](findings/hlsdk_portable_compatibility.md) | Comparison with FWGS `hlsdk-portable` and the target layer `GUNMAN_ENGINE_GOLDSRC` / `GUNMAN_ENGINE_XASH3D`. |
| [`findings/entity_class_families.md`](findings/entity_class_families.md) | Mapping of 208 map class names to C++ classes via the vtable pointer. Several class names can be the same class. |
| [`findings/open_items_audit_2026-09-05.md`](findings/open_items_audit_2026-09-05.md) | Checks every open item in the findings against the code. Marked several items as done and corrected two wrong findings. |
| [`findings/gunman-runtime-open-items-2026-09-04.md`](findings/gunman-runtime-open-items-2026-09-04.md) | List of runtime gaps and the test each needs. Dated 04.09.; some items are now done (see the audit). |
| [`findings/weapons/primaryattack_breakthrough.md`](findings/weapons/primaryattack_breakthrough.md) | The real `PrimaryAttack` addresses of the weapons. Read before changing any weapon, because some `weapon_*.md` files were built on an earlier wrong premise. |
| [`findings/rebuild_ideas.md`](findings/rebuild_ideas.md) | Design ideas for a future rebuild. **Not** findings about the original. |
| [`findings/freegunman_crossref.md`](findings/freegunman_crossref.md) | Comparison with `eukara/freegunman` (QuakeC, FTEQW). Used only as a behavior and structure reference, no code is reused. |
| [`findings/official_manual_and_website.md`](findings/official_manual_and_website.md) | Manual and archived publisher sites as primary sources for game rules. |

**Implementation status:** A linked list of all Gunman entities with code location, review status, and findings is in [`docs/gunman-funktionsstatus.md`](docs/gunman-funktionsstatus.md) (German).

**Note on completeness:** `docs/` and `findings/` in this repo are copies of the working state. The full collection includes further files (decision records, Ghidra scripts, map sources) that are not in the repo.

## Monsters and entities

Class names are registered with `LINK_ENTITY_TO_CLASS` in the source files. Selection by group:

**Humans (allied or neutral)**
`monster_human_gunman`, `monster_human_unarmed`, `monster_human_bandit` / `monster_human_grunt`, `monster_human_scientist`, `monster_human_demoman`, `monster_human_chopper` / `monster_apache`, `monster_manta`

**Rust maps**
`monster_rustbit`, `monster_rustbit_friendly`, `monster_beak`, `monster_rustgunr` / `monster_gunner`, `monster_gunner_friendly`, `monster_rustbot`, `monster_rustbot_friendly`, `monster_rustflier`, `monster_xenome`, `monster_tubequeen`, `monster_tube`, `monster_maggot`, `monster_critter`, `monster_beakbirther`

**Mayan maps**
`monster_hatchetfish`, `monster_tank`, `monster_darttrap`, `monster_raptor` / `monster_rheptor`, `monster_renesaur`, `monster_gator`, `monster_microraptor`, `monster_largescorpion`, `monster_scorpion`, `lava_god`

**Rebar, bosses and misc.**
`monster_tube_embryo`, `monster_xenome_embryo`, `monster_aigirl`, `entity_clustergod`, `monster_endboss`, `monster_trainingbot`, `lightning_bug`, `ball_lightning`

**Cut content only (not registered)**
`monster_penta`, `monster_battery` are in `cutcontent_penta_battery.cpp`.

**Weapons and ammo**
`weapon_beamgun`, `weapon_chemgun`, `weapon_dml`, `weapon_minigun`, `weapon_aicore`, ammo entities (`ammo_*`)

**Map logic and decoration**
`trigger_gunmanteleport`, `trigger_kill`, `trigger_tank*`, `item_armor`, `item_gascan`, `player_speaker`, `random_speaker`, `entity_digit`, `decore_*` (many decoration objects)

A full list is available with `grep -rn LINK_ENTITY_TO_CLASS dlls/`.

## Building

The basics come from the Half-Life SDK. Details are in [`BUILDING.md`](BUILDING.md).

**Linux (CMake, GCC 9 or newer, `g++-multilib`, `libgl1-mesa-dev`):**

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/LinuxToolchain.cmake \
         -DCMAKE_INSTALL_PREFIX=/path/to/Half-Life/gunman
cmake --build .
```

**Choosing the engine target:**

| Option | Effect |
| --- | --- |
| `-DGunman_Xash3D=OFF` (default) | Build for Steam GoldSrc, sets `GUNMAN_ENGINE_GOLDSRC=1` |
| `-DGunman_Xash3D=ON` | Build for Xash3D FWGS, sets `GUNMAN_ENGINE_XASH3D=1` |

Engine-specific code goes behind the `#if`/`#else` boundary in `dlls/gunman_engine_target.h`. The platform (Linux, Windows) must not be used as a stand-in for the engine target.

**Other options:** `HalfLife_LTO` (link-time optimization), `HalfLife_HLDS_DIRECTORY` (dedicated server).

**Windows:** Visual Studio 2019 or newer, projects in `projects/vs2019/`, platform Win32.

> Note: the build instructions come from the Half-Life SDK. It has not been verified in every case that the current state builds cleanly with all listed toolchains.

## Workflow

Development proceeds class by class, in the order the story's maps need them:

1. **Read the findings.** Check existing findings and `STATUS.md` before investigating anything again.
2. **Identify the class.** Derive behavior, fields, and callbacks from the decompilation of `gunman.dll`. Map the class name via exports, vtables, and map entities.
3. **Rebuild in SDK style.** The level from the matrix (A, B, or C) decides whether SDK code is reused, derived from, or written fresh. Source addresses (e.g. `@0x100ae360`) and vtable addresses go in the comment header.
4. **Build and check.** The build must succeed. Runtime tests run through the Xash3D procedure described in the `docs/` guides.
5. **Document.** Add an entry to `STATUS.md` with what was ported, what is open, and what was not tested.

The reverse-engineering workspace (Ghidra projects, extraction tools, map sources, staging scripts) lives **outside** this repository. Only exported and reproducible results belong here.

## Known issues

From `STATUS.md`, `findings/`, and the code. As of 05.09.2026 for the findings, 02.11.2026 for `STATUS.md`.

**Open according to findings and code:**

- **Scientist target callback** (session 309): The retail callback that fires in parallel is not ported, because the SDK field and the keyvalue are still unclear.
- **Gunner blood spray** (session 275): The origin of the retail effect is open. Currently `TraceResult::vecEndPos` is used. A targeted line-of-sight and attack test is missing.
- **Friendly Gunman combat**: Combat is a generic hitscan approximation. Event IDs and the `gn_kill`/`gn_attack` sequences are not reconstructed (per the runtime gap list).
- **Scripted sequences that depend on the player** (`m_fMoveTo` 6/7/8): Motion schedules are guarded, but selecting the player is not reconstructed.
- **Rustbot and xenome**: several health and damage values are placeholders; the Rustbot beam rig is an approximation.
- **Weapon findings**: `weapon_minigun` has an unresolved interface flag check (`DAT_101322d8`). According to the findings, `weapon_beamgun` may load `p_egon.mdl` from `valve/` instead of the Gunman folder.
- **DML customization**: not reproduced are the multi-rocket swarm behavior, target search via `laser_spot`, and the grenade tripwire. The items are not placed in any map, so testing is only possible via the console.
- **Runtime tests are missing** for many monsters. Only individual paths have been tested in the map RUST4A.
- **TODO/FIXME markers**: about 60 places in roughly 33 files in `dlls/` and `cl_dll/`.
- **Cut content** (`cutcontent_penta_battery.cpp`) is not registered.
- **FGD and map keyvalues** are not fully checked against all maps.

**Already done (per the audit of 05.09.2026):** `weapon_chemgun`, `weapon_dmlGrenade`, `weapon_beamgun` (`PrimaryAttack` corrected), `weapon_minigun` (spread and heat), `weapon_dml` (target lock and dual salvo), the Gunner family (was already implemented; the finding was corrected).

Please report bugs with the map name, the class, and the steps to reproduce.

## License

This project is based on the **Half-Life 1 SDK** by Valve Corp. and is subject to its license (see [`LICENSE`](LICENSE) and [`README_HLSDK.md`](README_HLSDK.md)). The license requires that distributions of the SDK, or substantial parts of it, include the copyright notice and the license text.

Gunman Chronicles is a separate game. The rights to its content (characters, maps, assets) belong to their respective rights holders and are not covered by this SDK's license.
