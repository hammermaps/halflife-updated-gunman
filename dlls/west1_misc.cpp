//=========================================================
// west1's remaining gaps: decore_eagle (CBaseBird-family ambient
// flying prop) and player_giveitems (confirmed dead cut-content stub,
// see gunman_custom_entities.cpp's CPlayerToggleHud for the identical
// pattern). weapon_dml (M.U.L.E. rocket launcher), also a gap on this
// map, is deferred - see [[project-weapon-dml-deferred]]-equivalent
// tracking in findings/weapons/weapon_dml.md; its PrimaryAttack was
// never located project-wide even before this session.
//
// Decompiled fresh from gunman.dll:
//   decore_eagle: LINK @0x10069ce0, vtable @0x100f6cf0, Spawn
//     @0x10069d90, Precache @0x10069f00, Classify (slot 8, @0x10069d70)
//     confirmed constant 0 (CLASS_NONE). Spawn's own body only sets the
//     model and calls a shared CBaseBird spawn-tail helper
//     @0x100689a0, the SAME helper already confirmed and documented in
//     mayan0a_fauna.cpp for decore_pteradon (CPteradon) -
//     bbox(-32,-32,-16)/(32,32,16) hardcoded floats in the helper
//     (0x42000000=32/0x41800000=16), FL_FLY+MOVETYPE_FLY(5)/SOLID_BBOX
//     (2), takedamage=DAMAGE_YES(0x40000000=2), health=10
//     (0x41200000), random start frame, and real hunting-flight AI via
//     CBaseBird::HuntThink/CBaseBird::FlyTouch (gunman.dll's own
//     internal base class, not decompiled in full this session - same
//     scope decision as CPteradon). Reproduced here with the exact
//     same CircleThink simplification already established for
//     CPteradon rather than re-deriving a third variant of the same
//     rationale.
//   player_giveitems: LINK @0x10043e70, vtable @0x100f2684, Spawn
//     (slot 0, @0x10052db0) - BYTE-IDENTICAL to player_togglehud's
//     Spawn (same address, same single `pev->solid = SOLID_NOT`
//     effect, same 0x20-byte object size with no class-owned
//     KeyValue/Touch/Use). Per
//     findings/entities/misc_gunman_custom_entities.md, both are
//     confirmed dead/cut-content stub classes in the retail build -
//     reproduced identically to CPlayerToggleHud in
//     gunman_custom_entities.cpp.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "basebird.h"
#include "player.h"
#include "gamerules.h"

//=========================================================
// player_giveitems - gives the player the items selected by spawnflags
// when used. Retail Use (slot 46 @0x10043ec0, re-decompiled 2026-10-31;
// the earlier "dead stub" rating was wrong - only Spawn was inspected):
// target = activator if it is a player, else the local player. Bits in
// spawnflags map to classnames: 0x1 weapon_fists, 0x2 weapon_gausspistol,
// 0x4 cust_2GaussPistolSniper, 0x8 weapon_shotgun, 0x10 weapon_minigun,
// 0x20 cust_2MinigunCooled, 0x40 weapon_beamgun, 0x80 weapon_dml,
// 0x100 weapon_SPchemicalgun, 0x200 weapon_aicore, 0x400 item_armor,
// 0x800/0x1000/0x2000/0x4000 item_healthkit. A retail global
// (DAT_10134d18) is set to 1 while giving; its purpose is unknown and
// it is not reproduced. The cust_* classes exist since customize_items.cpp.
//=========================================================
class CPlayerGiveItems : public CBaseEntity
{
public:
	void Spawn() override { pev->solid = SOLID_NOT; }
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
};
LINK_ENTITY_TO_CLASS(player_giveitems, CPlayerGiveItems);

void CPlayerGiveItems::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	static const char* const items[] = {
		"weapon_fists", "weapon_gausspistol", "cust_2GaussPistolSniper", "weapon_shotgun",
		"weapon_minigun", "cust_2MinigunCooled", "weapon_beamgun", "weapon_dml",
		"weapon_SPchemicalgun", "weapon_aicore", "item_armor", "item_healthkit",
		"item_healthkit", "item_healthkit", "item_healthkit"};

	CBasePlayer* pPlayer = nullptr;
	if (pActivator && pActivator->IsPlayer())
		pPlayer = static_cast<CBasePlayer*>(pActivator);
	else if (!g_pGameRules->IsMultiplayer())
		pPlayer = static_cast<CBasePlayer*>(UTIL_GetLocalPlayer());

	if (!pPlayer)
		return;

	for (int i = 0; i < 15; i++)
	{
		if ((pev->spawnflags & (1 << i)) != 0)
			pPlayer->GiveNamedItem(items[i]);
	}
}

//=========================================================
// decore_eagle - CEagle. Ambient killable flying prop, same
// CBaseBird-family CircleThink simplification as CPteradon.
//=========================================================
class CEagle : public CBaseBird
{
public:
	void Spawn() override { SpawnBird("models/eagle.mdl"); }
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_eagle, CEagle);

void CEagle::Precache()
{
	PrecacheModel("models/eagle.mdl");
	PrecacheSound("eagle/eagle_cry1.wav");
	PrecacheSound("eagle/eagle_cry2.wav");
	PrecacheSound("eagle/eagle_cry3.wav");
	PrecacheSound("eagle/eagle_flap1.wav");
	PrecacheSound("eagle/eagle_flap2.wav");
	PrecacheSound("eagle/eagle_flap3.wav");
}
