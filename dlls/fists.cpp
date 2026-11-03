//=========================================================
// weapon_fists - CFists. Gunman's start melee weapon: a fist/knife
// hybrid that reuses stock Half-Life's crowbar assets for the fist
// mode (models/w_crowbar.mdl, models/p_crowbar.mdl,
// weapons/cbar_miss1.wav) alongside its own v_hands.mdl view model
// and knife sounds/animations. See findings/weapons/weapon_fists.md
// for the RE history; this session additionally re-decompiled
// FUN_1008d8c0 (PrimaryAttack) and FUN_1008d930 (Swing) fresh from
// gunman.dll to confirm/refine that doc before writing this file.
//
// Confirmed by the fresh decompile:
// - Trace/hit-detection (FUN_1008d930) is structurally identical to
//   CCrowbar::Swing: a 32-unit line trace from GetGunPosition(),
//   falling back to a head_hull UTIL_TraceHull and then this
//   weapon's own FindHullIntersection-equivalent (FUN_1008d680) with
//   the duck hull mins/maxs on a miss. There is NO separate
//   underwater/water-level branch anywhere in this function -
//   correcting an earlier (Session 101) memory note that speculated
//   about a distinct underwater hull-trace path. The eGuide's "useful
//   at grates / in water fish fights" flavor text most likely just
//   describes melee weapons working underwater unlike guns, not a
//   distinct code path; waterripple.spr/.sc are precached but not
//   referenced from this swing logic (likely a splash effect wired
//   elsewhere, e.g. via WEAPON_NOCLIP hands touching water on the
//   view model - not reproduced here, purely cosmetic).
// - param_1[0x38] is the fist(0)/knife(1) mode field, confirmed
//   Spawn() sets it to 1 (starts in knife mode).
// - Miss sounds: fist mode always plays weapons/cbar_miss1.wav
//   (regardless of the param_1[0x28] parity check - both parity
//   branches route to the same sound). Knife mode miss alternates
//   KnifeAttack1.wav / KnifeAttack1b.wav by parity.
// - Hit sounds: fist mode picks among RightPunch/RightPunch2/
//   RightPunch3.wav via a RANDOM_LONG(0,2)-style call - confirmed
//   this happens identically regardless of the param_1[0x28] parity
//   branch taken, i.e. LeftPunch1-3.wav are precached but this
//   function never plays them. Knife mode hit alternates
//   KnifeAttack2.wav / KnifeAttack2b.wav by parity.
// - Damage type/amount remain uncertain. The fresh Swing decompile
//   does recover the primary-attack timers in param_1[0x1e]; these
//   are applied per mode/parity below. The separate 8-13s value written
//   to param_1[0x37] by PrimaryAttack is not mapped to an SDK field.
//   Damage is still simplified here:
//   damage reuses gSkillData.plrDmgCrowbar like the stock crowbar,
//   DMG_CLUB for fist mode and DMG_SLASH for knife mode (a reasoned
//   choice matching the mode's real-world analogue, not decompiled).
// - LeftPunch1-3.wav are precached (per the original findings doc)
//   but confirmed unused in this function; kept in Precache() only
//   because they are genuinely precached in retail, not because
//   they're played here.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "saverestore.h"
#include "player.h"
#include "gamerules.h"

#define FISTS_BODYHIT_VOLUME 128
#define FISTS_WALLHIT_VOLUME 512

// v_hands.mdl sequence indices, from models-src/v_hands/v_hands.qc.
enum fists_e
{
	FISTS_IDLE = 0,
	FISTS_IDLEJUDO,
	FISTS_IDLEKICKASS,
	FISTS_RIGHTPUNCH,
	FISTS_LEFTPUNCH,
	FISTS_DOUBLEPUNCH,
	FISTS_READY,
	FISTS_HOLSTER,
	FISTS_KNIFEDRAW,
	FISTS_KNIFEHOLSTER,
	FISTS_IDLEKNIFE,
	FISTS_IDLEKNIFEINSPECT,
	FISTS_KNIFEATTACK1,
	FISTS_KNIFEATTACK2,
	FISTS_PUSHBUTTON
};

// Local Gunman implementation; the stock crowbar translation unit is not
// part of the game DLL anymore.
static void FindFistsHullIntersection(const Vector& vecSrc, TraceResult& tr, const Vector& mins, const Vector& maxs, edict_t* pEntity)
{
	Vector vecHullEnd = vecSrc + ((tr.vecEndPos - vecSrc) * 2);
	TraceResult tmpTrace;
	float flDistance = 1000000.0f;

	UTIL_TraceLine(vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace);
	if (tmpTrace.flFraction < 1.0)
	{
		tr = tmpTrace;
		return;
	}

	for (int i = 0; i < 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 2; ++k)
			{
				Vector vecEnd = vecHullEnd + Vector(i ? maxs.x : mins.x, j ? maxs.y : mins.y, k ? maxs.z : mins.z);
				UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace);
				if (tmpTrace.flFraction < 1.0)
				{
					float flThisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if (flThisDistance < flDistance)
					{
						tr = tmpTrace;
						flDistance = flThisDistance;
					}
				}
			}
		}
	}
}

class CFists : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool Deploy() override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	void Holster() override;
	void PrimaryAttack() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT SwingAgain();
	void EXPORT Smack();
	bool Swing(bool fFirst);

	// Fist/knife mode switch, confirmed at param_1[0x38] in the
	// retail weapon instance. 0 = fist mode, 1 = knife mode.
	int m_iMode;

private:
	int m_iSwing;
	TraceResult m_trHit;
};
LINK_ENTITY_TO_CLASS(weapon_fists, CFists);

TYPEDESCRIPTION CFists::m_SaveData[] =
{
	DEFINE_FIELD(CFists, m_iMode, FIELD_INTEGER),
	DEFINE_FIELD(CFists, m_iSwing, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CFists, CBasePlayerWeapon);

void CFists::Spawn()
{
	Precache();
	m_iId = WEAPON_FISTS;
	// Spawn() confirmed setting the mode field to 1 (knife mode) -
	// the weapon starts drawn as the knife, not bare fists.
	m_iMode = 1;
	SET_MODEL(ENT(pev), "models/w_knife.mdl");
	m_iClip = WEAPON_NOCLIP;

	// BUG FIX (2026-09-05, live gameplay report): v_hands.mdl has TWO
	// bodyparts, not one as the reconstructed .qc suggested -
	// confirmed via tools/mdl_inspect.py --bodyparts directly on the
	// compiled model: [0] "body" (nummodels=1, always the bare-hand
	// mesh) and [1] "knife" (nummodels=2 - submodel 0 = no visible
	// blade, submodel 1 = the knife blade attached to the hand). Since
	// bodypart 0 has only 1 submodel it never contributes to the
	// encoded body value, so pev->body itself IS the "knife" bodypart's
	// submodel index here. Without this, the view/player models always
	// rendered the hidden-blade submodel regardless of animation, which
	// is why knife mode showed bare fists even while playing the
	// correct knife draw/attack/idle sequences. SendWeaponAnim() (see
	// weapons.cpp) replicates this weapon entity's own pev->body to the
	// client for both the viewmodel and player-visible weaponmodel, so
	// setting it here (rather than on the player or per-call) is
	// sufficient - SET_MODEL above already reset it to 0.
	pev->body = m_iMode;

	FallInit();
}

void CFists::Precache()
{
	PrecacheModel("models/v_hands.mdl");
	PrecacheModel("models/w_crowbar.mdl");
	PrecacheModel("models/p_crowbar.mdl");
	PrecacheModel("models/w_knife.mdl");

	PrecacheSound("weapons/cbar_miss1.wav");
	PrecacheSound("weapons/Hands_IdleKickAss_F0.wav");
	PrecacheSound("weapons/LeftPunch.wav");
	PrecacheSound("weapons/LeftPunch2.wav");
	PrecacheSound("weapons/LeftPunch3.wav");
	PrecacheSound("weapons/RightPunch.wav");
	PrecacheSound("weapons/RightPunch2.wav");
	PrecacheSound("weapons/RightPunch3.wav");
	PrecacheSound("weapons/KnifeAttack1.wav");
	PrecacheSound("weapons/KnifeAttack1b.wav");
	PrecacheSound("weapons/KnifeAttack2.wav");
	PrecacheSound("weapons/KnifeAttack2b.wav");
	PrecacheSound("weapons/KnifeDraw.wav");
	PrecacheSound("weapons/KnifeHolster.wav");

	// Precached in retail but not referenced from the swing logic
	// decompiled this session; kept for parity, not wired up further.
	PrecacheModel("sprites/waterripple.spr");
}

bool CFists::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = nullptr;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = WEAPON_FISTS;
	p->iWeight = 0;
	return true;
}

bool CFists::Deploy()
{
	if (m_iMode == 1)
		return DefaultDeploy("models/v_hands.mdl", "models/p_crowbar.mdl", FISTS_KNIFEDRAW, "onehanded", m_iMode);

	return DefaultDeploy("models/v_hands.mdl", "models/p_crowbar.mdl", FISTS_READY, "onehanded", m_iMode);
}

void CFists::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(m_iMode == 1 ? FISTS_KNIFEHOLSTER : FISTS_HOLSTER, m_iMode);
}

void CFists::PrimaryAttack()
{
	if (!Swing(true))
	{
		SetThink(&CFists::SwingAgain);
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CFists::Smack()
{
	DecalGunshot(&m_trHit, BULLET_PLAYER_CROWBAR);
}

void CFists::SwingAgain()
{
	Swing(false);
}

bool CFists::Swing(bool fFirst)
{
	bool fDidHit = false;

	TraceResult tr;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

#ifndef CLIENT_DLL
	if (tr.flFraction >= 1.0)
	{
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(m_pPlayer->pev), &tr);
		if (tr.flFraction < 1.0)
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (!pHit || pHit->IsBSPModel())
				FindFistsHullIntersection(vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict());
			vecEnd = tr.vecEndPos;
		}
	}
#endif

	// No PRECACHE_EVENT call for a swing event was found in the
	// decompiled Precache() (only events/waterripple.sc, unrelated to
	// swinging) - unlike CCrowbar, this weapon does not appear to
	// need PLAYBACK_EVENT_FULL prediction wiring for its swing.

	bool fKnife = m_iMode == 1;
	int iVariant = (m_iSwing++) % 2;

	if (tr.flFraction >= 1.0)
	{
		if (fFirst)
		{
			// Miss - confirmed identical for both parity variants:
			// fist mode always plays cbar_miss1.wav, knife mode
			// alternates KnifeAttack1/1b.wav by parity.
			const float flAttackDelay = fKnife ? (iVariant == 0 ? 0.65f : 0.5f) : 0.4f;
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + flAttackDelay;

			SendWeaponAnim(fKnife ? FISTS_KNIFEATTACK1 : FISTS_RIGHTPUNCH, m_iMode);
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifndef CLIENT_DLL
			if (fKnife)
			{
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON,
					iVariant == 0 ? "weapons/KnifeAttack1.wav" : "weapons/KnifeAttack1b.wav",
					1, ATTN_NORM);
			}
			else
			{
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM);
			}
#endif
		}
	}
	else
	{
		SendWeaponAnim(fKnife ? FISTS_KNIFEATTACK2 : (iVariant == 0 ? FISTS_RIGHTPUNCH : FISTS_LEFTPUNCH), m_iMode);

		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifndef CLIENT_DLL
		fDidHit = true;
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

		ClearMultiDamage();

		float flDamage = fKnife ? gSkillData.gunmanKnifeDamage : gSkillData.gunmanOneHandPunchDamage;
		if ((m_flNextPrimaryAttack + 1.0f > UTIL_WeaponTimeBase()) && !g_pGameRules->IsMultiplayer())
			flDamage *= 0.5f;

		pEntity->TraceAttack(m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, fKnife ? DMG_SLASH : DMG_CLUB);
		ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

#endif

		const float flAttackDelay = fKnife ? (iVariant == 0 ? 0.5f : 0.45f) : 0.2f;
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + flAttackDelay;

#ifndef CLIENT_DLL
		if (fKnife)
		{
			// Confirmed: knife hit alternates KnifeAttack2/2b.wav by
			// parity, regardless of what/who was hit.
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON,
				iVariant == 0 ? "weapons/KnifeAttack2.wav" : "weapons/KnifeAttack2b.wav",
				1, ATTN_NORM);

			m_pPlayer->m_iWeaponVolume = FISTS_BODYHIT_VOLUME;
		}
		else
		{
			// Confirmed: fist hit is a 3-way random pick among
			// RightPunch/RightPunch2/RightPunch3.wav, independent of
			// the parity variant used for the swing animation.
			switch (RANDOM_LONG(0, 2))
			{
			case 0:
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch.wav", 1, ATTN_NORM);
				break;
			case 1:
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch2.wav", 1, ATTN_NORM);
				break;
			case 2:
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/RightPunch3.wav", 1, ATTN_NORM);
				break;
			}

			m_pPlayer->m_iWeaponVolume = FISTS_BODYHIT_VOLUME;
		}

		if (pEntity && !pEntity->IsAlive())
			return true;

		m_trHit = tr;
#endif
		SetThink(&CFists::Smack);
		pev->nextthink = gpGlobals->time + 0.2;
	}
	return fDidHit;
}

// Retail: beim Aufnehmen die Anpassungs-Items verteilen (customize_items.cpp)
void CFists::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	pPlayer->GiveNamedItem("cust_1fistsKnife");
}
