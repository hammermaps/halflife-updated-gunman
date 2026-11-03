
### (275) Profil-Blutspray und Gunner-TraceBleed portiert (2026-11-02)
- Gemeinsamer `CBaseMonster::TraceBleed`-Override sendet nun profilabhängiges `TE_SPRITE_SPRAY`; Spriteprecache für sechs Retailtypen wird über das jeweilige Profil ausgelöst. Gunner-Profil 14 → `gorebot.spr`.
- Beide Gunner-Nahkampfevents geben den Hull-Trace und Vorwärtsvektor an den Opfer-`TraceBleed`-Aufruf weiter. Spraymenge: `min(trunc(Schaden × 0,5), 50)`; der Retail-Helper setzt Speed/Noise auf 100 und ruft danach Basisdecals auf.
- Build erfolgreich; RUST4A ohne Xephyr lädt Rustgunner und Blut-Sprites, kein Host-/Systemfehler oder Crash. Kein gezielter Angriff/Sichttest. Die Herkunft des separaten Retail-Effektursprungs ist noch offen; aktuell `TraceResult::vecEndPos`.
- Details und REA-Evidence: `findings/entities/gunner_eye_2026-11-01.md` (Nachtrag Profil-Blutspray).
### (309) Scientist: clearvoice-Unterbrechung portiert (2026-11-02)
- Retail `CScientist::GetIdealState` (`0x100d8800`, REA Evidence `ev_36c2e771649c9fd019ab5cc27ec1cd491924f8c43946c5e74e154793b65f5940`) belegt einen einmaligen Interrupt bei Light-/Heavy-Damage oder gehörtem Sound, solange AI und Marker aktiv sind. Bei `clearvoice=1` wird `common/null.wav` über `CHAN_VOICE` abgespielt; danach wird der Marker gelöscht.
- Der SDK-Pfad sendet den stillen Sound direkt mit den Retail-Parametern (`CHAN_VOICE`, `common/null.wav`, Lautstärke 1, Attenuation 0.8) und setzt den Marker zurück. Den parallel gefeuerten Retail-Target-Callback (`FUN_100531f0`, Evidence `ev_88dee171373f50204b30c92b34278994eeb8bc193de817db040e6b8accc9f1cc`) nicht portiert: SDK-Feld und Keyvalue sind noch ungeklärt. Build erforderlich; gezielter Laufzeittest offen. Details: `findings/entities/scientist_family.md`.

### (310) Rustflier-Rotor-Pitch portiert; Scientist-Status korrigiert (2026-11-02)
- REA/Ghidra Retail `0x100b4100`: `round(dot(rustflierVelocity-playerVelocity, directionToPlayer) * 0.02 + 100)`, begrenzt auf 50..250. Startup-Pitch bleibt 110; Loop-Update-Tonhöhe wird an Rotor-, Whine- und Sick-Sounds gesendet.
- Umsetzung in `cinematic2_misc.cpp`; GCC-13-`hl`-Build erfolgreich. Laufzeit-Audiovergleich gegen Retail offen. Details/Evidence: `findings/entities/rustflier_rotor_pitch_2026-11-02.md`.
- Korrektur zu (309): Der Interrupt-Target-Callback ist in `CScientist::GetIdealState` portiert (`FireTargets` auf `m_iszTriggerTarget`); gezielter Laufzeittest bleibt offen.

### (311) CBaseBird-Cry-/Flap-Slots abgeglichen und Eagle-Sounds portiert (2026-11-02)
- Retail-CBaseBird ruft Vtable `+0x22c` nach dem Idle-Timer für `eagle_cry1-3.wav` und `+0x230` auf `glideflap` für `eagle_flap1-3.wav` auf. Parameter: `CHAN_VOICE`, Volume 1, Attenuation 0.3, Flags 0, Pitch 100. Die Pteradon-Ziele sind `RET`-Stubs.
- `CBaseBird` ruft nun `IdleSound`/`FlapSound`; nur `CEagle` implementiert diese Sounds. GCC-13-`hl`-Build erfolgreich. `west1` startete ohne Xephyr, lud `models/eagle.mdl` und lief bis zum 15-s-Limit; kein Host-/Systemfehler. Kein hörbarer A/B-Test.
- REA-Bundle und Details: `findings/entities/cbasebird_sound_virtuals_rea_2026-11-02.json`, `findings/entities/cbasebird_huntthink_2026-10-31.md` Nachtrag 16. `+0x2a8`, Eagle/Pteradon-Flug-A/B und getrennte Chopper-Audiopunkte bleiben offen.
