//=========================================================
// The DML customization-item pickups (cust_1DMLLaunch/cust_2DMLFlightpath/
// cust_3DMLDetonate/cust_4DMLPayload, cust_1DMLGrenDetonate/
// cust_2DMLGrenPayload) - the mechanism weapon_dml.md's "MULE Packs/MULE
// Launcher Trigger-type/Payload-type" matrix is gated behind, and the
// reason ~13 CDmlRocket methods (see dml.cpp) were previously undecompiled/
// unreproduced.
//
// CONFIRMED via fresh Ghidra decompile this session (per findings/entities/
// dll_reextraction_2026-09-01.md's own recommendation - the two vtables
// had been flagged "Ghidra: not a function" before force-decompilation was
// tried):
//   - Two vtables, `0x100fb250` (4-alias DML launcher chain) and
//     `0x100fb9d8` (2-alias DML grenade chain), share every slot from [1]
//     onward - only slot [0] (the ctor) differs between the two chains.
//     Force-decompiling both ctors (`FUN_10084b90`/`FUN_10089190`) shows
//     each one string-compares `pev->classname` against its own family's
//     alias names and stores a plain integer item-ID into `this+0x1b`
//     (9/10/11/12 for Launch/Flightpath/Detonate/Payload, 5/6 for
//     GrenDetonate/GrenPayload) - i.e. ALL of this is one shared class per
//     chain, distinguished only by which classname it was spawned as.
//   - Precache (`FUN_10081520`, shared) loads only `models/w_crowbar.mdl` -
//     confirms the findings doc's "generic placeholder model" note exactly.
//   - KeyValue (`FUN_100530f0`, shared) only recognizes the generic
//     `delay`/`killtarget` keys - nothing DML-specific.
//   - Save/Restore (`FUN_100623e0`/`FUN_10062410`) dispatch through a
//     table explicitly named `"CBasePlayerWeapon"` in the binary's own
//     string data - confirming these items are literally built on the
//     SAME base class as real weapons (matching the FGD's own
//     `base(Weapon, Targetx)` declaration), just never given a working
//     Deploy/PrimaryAttack override (none was found anywhere in either
//     vtable) - they are permanent, silent, non-wieldable inventory
//     entries. The bbox-setup slot (`FUN_10062440`) is likewise generic
//     pickup boilerplate.
//   - CONCLUSION: there is no DML-specific Touch/Use logic anywhere in
//     either item's own code. The actual mechanism is the standard
//     CBasePlayerItem/AddPlayerItem pickup pipeline (already implemented
//     unmodified in this SDK's player.cpp/weapons.cpp) - picking one of
//     these up permanently adds a never-deployed weapon-slot entry to the
//     player's inventory, and weapon_dml/weapon_dmlGrenade are expected to
//     later query for its presence (see dml.cpp/dmlgrenade.cpp: `CBasePlayer::
//     HasPlayerItemFromID()`, an existing unmodified stock SDK helper, is
//     the natural, already-present tool for exactly this query - no new
//     Read/Query mechanism needed).
//
// Reproduced here as a single class per chain (matching the confirmed
// shared-vtable structure 1:1), reading pev->classname in Spawn() exactly
// like the real ctor does. CanDeploy() is explicitly forced false (no
// override was found in either real vtable, so the base CBasePlayerWeapon
// default already matches - kept explicit here for clarity) so these can
// never be switched to or fired, matching the confirmed "silent flag"
// design.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

class CDmlCustomizeLauncher : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool CanDeploy() override { return false; }
};
LINK_ENTITY_TO_CLASS(cust_1DMLLaunch, CDmlCustomizeLauncher);
LINK_ENTITY_TO_CLASS(cust_2DMLFlightpath, CDmlCustomizeLauncher);
LINK_ENTITY_TO_CLASS(cust_3DMLDetonate, CDmlCustomizeLauncher);
LINK_ENTITY_TO_CLASS(cust_4DMLPayload, CDmlCustomizeLauncher);

void CDmlCustomizeLauncher::Precache()
{
	PrecacheModel("models/w_crowbar.mdl");
}

void CDmlCustomizeLauncher::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");

	// Confirmed via decompile: the ctor string-compares pev->classname
	// against each alias to pick the item-ID, in this exact order.
	if (FClassnameIs(pev, "cust_1DMLLaunch"))
		m_iId = WEAPON_CUST_DMLLAUNCH;
	else if (FClassnameIs(pev, "cust_2DMLFlightpath"))
		m_iId = WEAPON_CUST_DMLFLIGHTPATH;
	else if (FClassnameIs(pev, "cust_3DMLDetonate"))
		m_iId = WEAPON_CUST_DMLDETONATE;
	else
		m_iId = WEAPON_CUST_DMLPAYLOAD;

	FallInit();
}

bool CDmlCustomizeLauncher::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = nullptr;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 5; // same slot family as weapon_dml, see file header
	p->iPosition = 3;
	p->iId = m_iId;
	p->iWeight = 0;
	return true;
}

class CDmlCustomizeGrenade : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool CanDeploy() override { return false; }
};
LINK_ENTITY_TO_CLASS(cust_1DMLGrenDetonate, CDmlCustomizeGrenade);
LINK_ENTITY_TO_CLASS(cust_2DMLGrenPayload, CDmlCustomizeGrenade);

void CDmlCustomizeGrenade::Precache()
{
	PrecacheModel("models/w_crowbar.mdl");
}

void CDmlCustomizeGrenade::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");

	if (FClassnameIs(pev, "cust_1DMLGrenDetonate"))
		m_iId = WEAPON_CUST_DMLGRENDETONATE;
	else
		m_iId = WEAPON_CUST_DMLGRENPAYLOAD;

	FallInit();
}

bool CDmlCustomizeGrenade::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = nullptr;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 6; // same slot family as weapon_dmlGrenade, see file header
	p->iPosition = 3;
	p->iId = m_iId;
	p->iWeight = 0;
	return true;
}
