//=========================================================
// rust2a's ammo_beamgunclip (CBeamGunAmmo) and monster_critter
// (CCritter). weapon_beamgun, rust2a's third gap, is a player weapon
// and handled separately (see STATUS.md).
//
// Decompiled fresh from gunman.dll this session:
//   ammo_beamgunclip: LINK @0x10081370, vtable @0x100fa964, Spawn
//     @0x100813e0, Precache @0x10081410 -> shared CBasePlayerAmmo
//     spawn tail @0x100633c0 (same shared tail confirmed for
//     ammo_dmlsingle in rust1_misc.cpp - genuinely CBasePlayerAmmo,
//     not plain CItem despite findings/entity_review_list.csv's
//     "CItem-Ableitung" summary). Model "models/beamgunammo.mdl",
//     pickup sound "items/9mmclip1.wav".
//   monster_critter: LINK @0x100c2450, vtable @0x1010338c, Spawn
//     @0x100c3120, Precache @0x100c3200, Classify @0x100c24a0
//     (constant 0xd = 13 = CLASS_ALIEN_BIOWEAPON). Confirmed bbox
//     (-32,-32,0)/(32,32,64), SOLID_SLIDEBOX, view_ofs (0,0,30).
//     Precache confirms "sprites/tinyspit.spr" - a ranged spit-attack
//     projectile sprite, matching findings/entities/xenome_family.md's
//     Prima-Guide cross-reference of a "green slime" ranged attack for
//     this creature family.
//
// Event 2 is a CMaggot larva projectile, not an unknown xenomeshot. Its
// retail targeting utility includes an unlabelled lead calculation; the
// reconstruction uses the confirmed 800-unit direct-target fallback.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"

CBaseEntity* CreateCritterLarva(const Vector& vecOrigin, const Vector& vecVelocity,
	edict_t* pOwner, CBaseEntity* pEnemy);

//=========================================================
// ammo_beamgunclip - CBeamGunAmmo.
//=========================================================
class CBeamGunAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/beamgunammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PrecacheModel("models/beamgunammo.mdl");
		PrecacheSound("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(4, "battery", 40) != -1) // clip size 4 confirmed via GetItemInfo, see weapon_beamgun findings
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_beamgunclip, CBeamGunAmmo);

//=========================================================
// monster_critter - CCritter.
//=========================================================
class CCritter : public CBaseMonster
{
public:
	int MonsterProfile() override { return 17; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_BIOWEAPON; }
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
};
LINK_ENTITY_TO_CLASS(monster_critter, CCritter);

void CCritter::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/critter.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 64));

	pev->solid = SOLID_SLIDEBOX;
	pev->health = gSkillData.critterHealth;
	pev->view_ofs = Vector(0, 0, 30);
	m_afCapability |= bits_CAP_MELEE_ATTACK1 | bits_CAP_RANGE_ATTACK1;

	MonsterInit();
		// critter.mdl has no $eyeposition, so MonsterInit's SetEyePosition()
	// clears the confirmed spawn offset. Restore it for AI sight/path traces.
	pev->view_ofs = Vector(0, 0, 30);
}

void CCritter::Precache()
{
	PrecacheModel("models/critter.mdl");
	PrecacheModel("sprites/tinyspit.spr");
	PrecacheModel("sprites/gibxeno.spr");
	PrecacheModel("sprites/gorexeno.spr");
}

void CCritter::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	if (pEvent->event == 1)
	{
		// Retail HandleAnimEvent @ 0x100c2a10 passes the literal 15 here;
		// sk_critter_melee1_d belongs to another, still-unmapped path.
		CBaseEntity* pHurt = CheckTraceHullAttack(70.0f, 15.0f, DMG_SLASH);
		if (pHurt)
		{
			pHurt->pev->punchangle.x = RANDOM_FLOAT(-15.0f, 15.0f);
			pHurt->pev->punchangle.z = RANDOM_FLOAT(-15.0f, 15.0f);
			UTIL_MakeVectors(pev->angles);
			pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 25.0f + gpGlobals->v_up * 25.0f;
		}
		return;
	}

	if (pEvent->event == 2)
	{
		if (!m_hEnemy)
		{
			ALERT(at_console, "Critter spit without an enemy\n");
			return;
		}

		const Vector vecOrigin = pev->origin + Vector(0, 0, 17);
		const Vector vecDirection = (m_vecEnemyLKP - vecOrigin).Normalize();
		if (vecDirection != g_vecZero)
			CreateCritterLarva(vecOrigin, vecDirection * 800.0f, pev->owner, m_hEnemy);
		return;
	}

	CBaseMonster::HandleAnimEvent(pEvent);
}
