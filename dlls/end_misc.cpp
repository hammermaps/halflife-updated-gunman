//=========================================================
// monster_gunner_friendly - end1/end2's only non-weapon gap besides
// monster_endboss (tracked separately, see STATUS.md). NOT the same
// class as monster_human_gunman/CFriendlyGunman (human_gunman.cpp) -
// different vtable address, different model, different C++ class
// despite the similar name.
//
// Decompiled fresh from gunman.dll: LINK @0x100bef60, vtable
// @0x101028a0, Spawn @0x100bf200, Precache @0x100bf080, Classify
// (slot 8, @0x100beff0) confirmed constant 3 = CLASS_HUMAN_PASSIVE
// (friendly). Confirmed model "models/aigunner.mdl" - an AI/robot
// gunner turret-style ally, not a human NPC - bbox
// (-32,-32,0)/(32,32,150), skill-scaled health (~50/35/75 for
// skill 1/2/3 from the decompiled float constants, an unusual
// non-monotonic table - reproduced as-is), spawns a child glow-sprite
// ("sprites/cameye.spr", a camera-eye effect) attached as a
// trailing/glowing effect. Precache's sound list is entirely
// "mainframe/end_*"/"mainframe/mainend*" story/cutscene voice lines -
// confirms this is endgame-specific mainframe-AI content, matching
// its exclusive placement in end1/end2.
//
// Simplified relative to the original: the child cameye sprite
// (spawned via a generic CSprite-creation helper, attached with
// kRenderGlow-style transparency per the decompiled field values) and
// any HandleAnimEvent-driven attack/voice-line playback were NOT
// decompiled in full this session - out of scope for an endgame-only
// ally with no located combat anim events. Relies on default
// CBaseMonster AI/scheduling with no custom attack.
//=========================================================

// Aktualisierung 2026-10-31: monster_gunner_friendly ist in Retail eine
// Unterklasse des Gunners (Spawn @0x100bf200 ruft CGunner::Spawn
// @0x100bec80). Die Klasse steht deshalb jetzt in rust4a_misc.cpp
// (CGunnerFriendly : CGunner); siehe findings/entities/gunner_friendly_2026-10-31.md.
