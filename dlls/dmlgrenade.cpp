//=========================================================
// weapon_dmlGrenade - CDmlGrenade. Genuinely unimplemented gap
// identified during the 2026-09-05 findings audit
// (findings/open_items_audit_2026-09-05.md) - a fully separate
// inventory weapon (own slot, own weapon ID) from weapon_dml, even
// though both share the same "missiles" ammo pool (confirmed in
// findings/weapons/weapon_dmlGrenade.md's Session-73 Nachtrag: the
// "thrown MULE Packs" mode of the same M.U.L.E. family, occupying
// iSlot=6 next to weapon_dml's iSlot=5).
//
// Decompiled fresh from gunman.dll: vtable @0x100fb824, Spawn
// @0x10086230, Precache @0x100862d0, PrimaryAttack (confirmed via
// findings/weapons/primaryattack_breakthrough.md and a fresh
// decompile) @0x10086590. Confirms findings/weapons/weapon_dmlGrenade.md
// exactly: weapon ID 0xd(13), world model "models/w_grenade.mdl",
// ammo type "missiles" (same resource/pool as weapon_dml), iMaxClip=6.
// Precache confirms the SAME explosion sound set already reproduced
// in dml.cpp's CDmlRocket ("weapons/rocket1.wav", three
// "kabam1-3.wav", three "kaboom1-3.wav", "weapons/dml_fragment.wav")
// plus grenade-specific fragment models
// ("models/shelltip.mdl"/"shellengine.mdl"/"grenadecore.mdl") and two
// UTIL_PrecacheOther calls not individually resolved this session.
//
// PrimaryAttack confirms a genuine cook-then-throw state machine
// structurally identical in shape to the stock SDK's own
// CHandGrenade (dlls/handgrenade.cpp, left completely untouched, only
// used here as a reference for the hold/release pattern - not copied,
// see agents.md's rule against reusing decompiled original source
// verbatim): starts a "cook" timer and plays a pin-pull animation
// (SendWeaponAnim(4)) on first press while ammo is available, and
// dispatches to a distinct mode-3 branch (FUN_10087620, not
// decompiled this session - presumably a cluster-payload throw
// variant per the manual's Payload-type options) when an internal
// mode field is set to 3. The actual release/throw call (vtable slot
// 0x164) is triggered elsewhere (not conclusively located this
// session) once cook-state fields become nonzero.
//
// Simplified relative to the original: reproduces the confirmed
// cook-then-throw shape (pin-pull animation on press, then a
// timed-fuse toss on release) via the stock SDK's CGrenade::ShootTimed
// with a fixed 3-second fuse from the moment the pin is pulled -
// matching CHandGrenade's own confirmed "always explodes 3 seconds
// after the pin was pulled" constant, since the original's exact fuse
// duration constant was not resolved to a concrete value this
// session. The manual's Tripwire trigger-type option is NOT
// reproduced (no confirmed decompile of a grenade-side beam-tripwire
// mechanism was attempted this session, unlike CDmlRocket's
// BeamBreakThink - out of scope for this pass).
//
// CORRECTION (2026-09-05, later pass): the mode-3 cluster-payload
// branch (FUN_10087620) and the On-impact trigger option are now
// wired up, gated by the DML customization items
// (dml_customization.cpp, findings/entities/dml_customization_decompile.md):
// `cust_2DMLGrenPayload` (present -> Cluster payload) reuses the same
// `Dml_ShootClusterGrenade()` wrapper (mayan6_misc.cpp) already used
// by CDmlRocket::ShootClusterGrenades rather than re-deriving a
// separate cluster-throw path - approximated with 4 sub-munitions
// scattered around the throw point rather than the exact original
// count/spread. `cust_1DMLGrenDetonate` (present -> On-impact) swaps
// `CGrenade::ShootTimed` for the stock SDK's own `CGrenade::ShootContact`
// (already used elsewhere in this project, e.g. crossbow.cpp) instead
// of inventing a new immediate-detonate helper.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "skill.h"

// Shared with mayan6_misc.cpp's dml_cluster/CClusterGrenade and
// dml.cpp's CDmlRocket::ShootClusterGrenades - see file header.
extern CBaseEntity* Dml_ShootClusterGrenade(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float flDamage);

//=========================================================
// DmlHandGrenade - CDmlHandGrenade.
//=========================================================
class CDmlHandGrenade : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT DmlGrenExplodeTouch(CBaseEntity* pOther);
	void EXPORT DmlHandGrenTumbleThink();
	void EXPORT DmlHandGrenDetonate();

	static CDmlHandGrenade* Shoot(entvars_t* pevOwner, const Vector& vecStart,
		const Vector& vecVelocity, float flFuse, bool bTimed);

private:
	bool m_bTimed = false;
};
LINK_ENTITY_TO_CLASS(DmlHandGrenade, CDmlHandGrenade);

void CDmlHandGrenade::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/grenadecore.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->gravity = 0.5f;
	pev->friction = 0.8f;
	pev->dmg = 100.0f;
}

void CDmlHandGrenade::Precache()
{
	PrecacheModel("models/grenadecore.mdl");
	PrecacheModel("sprites/part1.spr");
	PrecacheModel("sprites/part2.spr");
	PrecacheSound("weapons/kabam1.wav");
	PrecacheSound("weapons/kabam2.wav");
	PrecacheSound("weapons/kabam3.wav");
	PrecacheSound("weapons/dml_fragment.wav");
}

CDmlHandGrenade* CDmlHandGrenade::Shoot(entvars_t* pevOwner, const Vector& vecStart,
	const Vector& vecVelocity, float flFuse, bool bTimed)
{
	auto* pGrenade = static_cast<CDmlHandGrenade*>(CBaseEntity::Create("DmlHandGrenade", vecStart,
		UTIL_VecToAngles(vecVelocity), ENT(pevOwner)));
	if (!pGrenade)
		return nullptr;

	pGrenade->pev->owner = ENT(pevOwner);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->m_bTimed = bTimed;
	pGrenade->pev->nextthink = gpGlobals->time + (bTimed ? 0.1f : flFuse);
	if (bTimed)
	{
		pGrenade->pev->dmgtime = gpGlobals->time + flFuse;
		pGrenade->SetTouch(&CDmlHandGrenade::BounceTouch);
		pGrenade->SetThink(&CDmlHandGrenade::DmlHandGrenTumbleThink);
	}
	else
	{
		pGrenade->SetTouch(&CDmlHandGrenade::DmlGrenExplodeTouch);
	}
	return pGrenade;
}

void CDmlHandGrenade::DmlGrenExplodeTouch(CBaseEntity* pOther)
{
	// Retail contact and fuse paths share the same DML detonation helper.
	CGrenade::ExplodeTouch(pOther);
}

void CDmlHandGrenade::DmlHandGrenTumbleThink()
{
	pev->nextthink = gpGlobals->time + 0.1f;
	if (pev->dmgtime <= gpGlobals->time)
	{
		SetThink(&CDmlHandGrenade::DmlHandGrenDetonate);
		pev->nextthink = gpGlobals->time;
	}
}

void CDmlHandGrenade::DmlHandGrenDetonate()
{
	CGrenade::Detonate();
}

class CDmlGrenade : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;

	void PrimaryAttack() override;
	void Reload() override;
	void WeaponIdle() override;
	bool ShouldWeaponIdle() override { return true; }
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	float m_flStartThrow = 0;
	float m_flReleaseThrow = -1;
};
LINK_ENTITY_TO_CLASS(weapon_dmlGrenade, CDmlGrenade);

TYPEDESCRIPTION CDmlGrenade::m_SaveData[] =
	{
		DEFINE_FIELD(CDmlGrenade, m_flStartThrow, FIELD_TIME),
		DEFINE_FIELD(CDmlGrenade, m_flReleaseThrow, FIELD_TIME),
	};

IMPLEMENT_SAVERESTORE(CDmlGrenade, CBasePlayerWeapon);

void CDmlGrenade::Spawn()
{
	Precache();
	m_iId = WEAPON_DMLGRENADE;
	SET_MODEL(ENT(pev), "models/w_grenade.mdl");
	// Retail Spawn writes DAT_10136fa8 to pev->dmg; its skill initializer
	// sets that constant to 75.0 (it is not a skill.cfg lookup).
	pev->dmg = 75.0f;

	FallInit();
}

void CDmlGrenade::Precache()
{
	PrecacheModel("models/w_grenade.mdl");
	PrecacheModel("models/v_grenade.mdl");
	PrecacheModel("models/p_grenade.mdl");
	PrecacheModel("models/shelltip.mdl");
	PrecacheModel("models/shellengine.mdl");
	PrecacheModel("models/grenadecore.mdl");
	PrecacheModel("sprites/part1.spr");
	PrecacheModel("sprites/part2.spr");
	PrecacheSound("weapons/rocket1.wav");
	PrecacheSound("weapons/kabam1.wav");
	PrecacheSound("weapons/kabam2.wav");
	PrecacheSound("weapons/kabam3.wav");
	PrecacheSound("weapons/kaboom1.wav");
	PrecacheSound("weapons/kaboom2.wav");
	PrecacheSound("weapons/kaboom3.wav");
	PrecacheSound("weapons/dml_fragment.wav");
	UTIL_PrecacheOther("dml_cluster");
	UTIL_PrecacheOther("DmlHandGrenade");
}

bool CDmlGrenade::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "missiles";
	// Retail CDmlGrenade::GetItemInfo @ 0x100863d0 reports six units
	// for both its shared missile reserve and clip, rather than the
	// launcher-wide carry constant.
	p->iMaxAmmo1 = 6;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 6; // confirmed
	p->iSlot = 6;
	p->iPosition = -1;
	p->iId = m_iId = WEAPON_DMLGRENADE;
	p->iFlags = 0;
	p->iWeight = -1;

	return true;
}

bool CDmlGrenade::Deploy()
{
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	return DefaultDeploy("models/v_grenade.mdl", "models/p_grenade.mdl", 0, "crowbar");
}

bool CDmlGrenade::CanHolster()
{
	return m_flStartThrow == 0;
}

void CDmlGrenade::Holster()
{
	// CORRECTION (2026-09-05): confirmed via client.dll's mirrored
	// weapon-prediction class (Vtable slot 65, findings/client/
	// client_weapons.md's open point) that the real Holster() resets
	// all three cook-throw state fields, not just the start-cook timer
	// - matches CanHolster()'s own guard (m_flStartThrow==0) but
	// m_flReleaseThrow was left stale.
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
}

void CDmlGrenade::PrimaryAttack()
{
	if (m_flStartThrow == 0 && m_iClip > 0)
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		SendWeaponAnim(4);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
}

void CDmlGrenade::Reload()
{
	DefaultReload(6, 1, 1.5);
}

void CDmlGrenade::WeaponIdle()
{
	if (m_flReleaseThrow == 0 && m_flStartThrow != 0)
		m_flReleaseThrow = gpGlobals->time;

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_flStartThrow != 0)
	{
		UTIL_MakeVectors(m_pPlayer->pev->v_angle);
		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;
		Vector vecThrow = gpGlobals->v_forward * 600 + m_pPlayer->pev->velocity;

		// confirmed shape (cook-then-throw); the real fuse constant was
		// not resolved, 3.0s matches CHandGrenade's own confirmed
		// timing - see file header
		float flFuse = m_flStartThrow - gpGlobals->time + 3.0;
		if (flFuse < 0)
			flFuse = 0;

		// Read the DML grenade customization items' persistent flags
		// (see dml_customization.cpp) - confirmed mechanism, see file
		// header.
		if (m_pPlayer->HasPlayerItemFromID(WEAPON_CUST_DMLGRENPAYLOAD))
		{
			for (int i = 0; i < 4; i++)
			{
				Vector vecScatter = vecSrc + Vector(RANDOM_FLOAT(-8, 8), RANDOM_FLOAT(-8, 8), RANDOM_FLOAT(-8, 8));
				Vector vecVelocity = vecThrow * 0.5 + Vector(RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(0, 100));
				Dml_ShootClusterGrenade(m_pPlayer->pev, vecScatter, vecVelocity, gSkillData.plrDmgM203Grenade * 0.4f);
			}
		}
	else if (m_pPlayer->HasPlayerItemFromID(WEAPON_CUST_DMLGRENDETONATE))
	{
		CDmlHandGrenade::Shoot(m_pPlayer->pev, vecSrc, vecThrow, 0.0f, false);
	}
	else
	{
		CDmlHandGrenade::Shoot(m_pPlayer->pev, vecSrc, vecThrow, flFuse, true);
	}

		SendWeaponAnim(5);
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;

		m_iClip--;

		if (m_iClip == 0)
			m_flTimeWeaponIdle = m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;

		return;
	}
	else if (m_flReleaseThrow > 0)
	{
		m_flStartThrow = 0;

		if (m_iClip != 0)
		{
			SendWeaponAnim(0);
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
		m_flReleaseThrow = -1;
		return;
	}

	if (m_iClip != 0)
	{
		SendWeaponAnim(0);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
	}
}
