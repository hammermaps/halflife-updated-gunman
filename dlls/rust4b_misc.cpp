//=========================================================
// RUST4B's meteor_target used to be a minimal marker implementation in
// this translation unit. Retail analysis of CMeteorTarget::Use
// (@0x100a8210) established that it is an active, targeted meteor spawner.
// The complete shared implementation now lives in rust4a_misc.cpp beside
// CMeteorChunk/CMeteorGod, preventing a duplicate LINK registration.
//
// Decompiled fresh from gunman.dll: LINK @0x100a7f80, vtable
// @0x1010015c, Spawn @0x100a81a0, KeyValue @0x100a8030. Confirms
// findings/entity_review_list.csv's "Unsichtbarer Zielmarker fuer
// meteor_god" description: EF_NODRAW, SOLID_NOT, MOVETYPE_NONE,
// mapper-assigned model (like lava_god/meteor_god). Two named
// keyvalues confirmed: "pitch" (float) and "heightabove" (float, the
// documented fall-height offset); a third, unnamed keyvalue (an int
// field) was also found but its FGD name wasn't resolved this
// session - stored without a name-checked KeyValue branch.
//
// This file deliberately contains no entity code. It remains in the build
// list to preserve the existing RUST4B source grouping.
//=========================================================
