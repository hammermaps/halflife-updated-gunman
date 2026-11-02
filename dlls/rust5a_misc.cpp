//=========================================================
// RUST5A's remaining classes with no SDK precedent: trigger_kill
// (CGunmanTriggerKill) and monster_beakbirther (CBeakBirther, the
// "Beak Queen"). weapon_beamgun, RUST5A's third gap, is an
// already-deferred player weapon.
//
// Decompiled fresh from gunman.dll this session:
//   trigger_kill: LINK @0x1005a050, ctor helper @0x1005a100 (vtable
//     @0x100f4c34), Spawn @0x1005a190 (just wires
//     SetUse(&KillUse) - Think/Touch untouched), KeyValue
//     @0x1005a110, CGunmanTriggerKill::KillUse @0x1005a1a0 (found via
//     the binary's own demangled symbol table after an initial wrong
//     guess at "vtable slot 58" turned out to read past the real
//     vtable's end into unrelated neighboring data - a reminder that
//     slot-position assumptions need per-class verification, not
//     copy-paste from a similarly-sized class). Confirms
//     findings/entity_review_list.csv's description exactly: Use-
//     triggered (no Touch), finds its target by the stock "target"
//     keyvalue, checks the target isn't already dead
//     (pev->takedamage != DAMAGE_NO), then calls TakeDamage on it
//     with damage = the target's own current health + 1 (guaranteed
//     lethal regardless of the target's actual health) and
//     bitsDamageType = DMG_CRUSH | (shouldgib ? DMG_ALWAYSGIB : 0) -
//     the confirmed "shouldgib" keyvalue maps directly onto the
//     stock DMG_ALWAYSGIB bit (0x2000), not a Gunman-custom flag.
//   monster_beakbirther: LINK @0x100c1d80, vtable @0x10102f34, Spawn
//     @0x100c21c0, Precache @0x100c1ed0, Classify @0x100c21b0
//     (constant 5 = CLASS_ALIEN_MILITARY). Confirmed bbox
//     (-10,-10,-10)/(10,10,10), MOVETYPE_FLY, SOLID_SLIDEBOX, health
//     150. Matches findings/entity_review_list.csv's "Nicht in FGD
//     platzierbar, aber in 3 Retail-Maps hart verdrahtet" note (no
//     custom KeyValue found here either - falls through to the
//     shared generic CBaseMonster::KeyValue).
//
// Simplified relative to the original: CBeakBirther's actual
// Beak-spawning behavior (per findings, the in-game "Hopper Cocoon"
// that spawns monster_beak instances) was not decompiled this
// session - out of scope for a secondary/setpiece creature at this
// map-driven pass (same simplification level as CTube's undecompiled
// homing-launcher attack elsewhere in this project); relies on
// default CBaseMonster AI beyond the confirmed Spawn/Precache/
// Classify behavior.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "soundent.h"

//=========================================================
// trigger_kill - CGunmanTriggerKill.
//=========================================================
#define SF_TRIGGERKILL_SHOULDGIB DMG_ALWAYSGIB

class CGunmanTriggerKill : public CBaseEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void EXPORT KillUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
};
LINK_ENTITY_TO_CLASS(trigger_kill, CGunmanTriggerKill);

bool CGunmanTriggerKill::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "shouldgib"))
	{
		if (atoi(pkvd->szValue) != 0)
			pev->spawnflags |= SF_TRIGGERKILL_SHOULDGIB;
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CGunmanTriggerKill::Spawn()
{
	SetUse(&CGunmanTriggerKill::KillUse);
}

void CGunmanTriggerKill::KillUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBaseEntity* pTarget = UTIL_FindEntityByTargetname(nullptr, STRING(pev->target));
	if (!pTarget || pTarget->pev->takedamage == DAMAGE_NO)
		return;

	CBaseEntity* pAttacker = pActivator ? pActivator : this;
	// Confirmed via decompile: damage = target's own current health + 1
	// (guaranteed lethal regardless of magnitude), bitsDamageType =
	// DMG_CRUSH | the confirmed shouldgib->DMG_ALWAYSGIB mapping.
	pTarget->TakeDamage(pAttacker->pev, pAttacker->pev, pTarget->pev->health + 1, DMG_CRUSH | (pev->spawnflags & SF_TRIGGERKILL_SHOULDGIB));
}

//=========================================================
// monster_beakbirther - CBeakBirther ("Beak Queen").
//=========================================================
class CBeakBirther : public CBaseMonster
{
public:
	int MonsterProfile() override { return 16; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void GibMonster() override;
	// Retail Slot 11 @0x100c2190: bitsDamageType |= 0x2000 (DMG_ALWAYSGIB).
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override
	{
		return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_ALWAYSGIB);
	}
	// Retail Slot 7 @0x100c1de0: Box +-140 in X/Y, z von origin bis +300.
	void SetObjectCollisionBox() override
	{
		pev->absmin = Vector(pev->origin.x - 140, pev->origin.y - 140, pev->origin.z);
		pev->absmax = Vector(pev->origin.x + 140, pev->origin.y + 140, pev->origin.z + 300);
	}
	// Retail Slot 88 @0x100c2290 (GetScheduleOfType, slot 87, forwards to the stock table @0x1000fff0).
	Schedule_t* GetSchedule() override;
	int Classify() override { return CLASS_ALIEN_MILITARY; }
	int ISoundMask() override { return bits_SOUND_DANGER; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
};
LINK_ENTITY_TO_CLASS(monster_beakbirther, CBeakBirther);

Schedule_t* CBeakBirther::GetSchedule()
{
	// Retail @0x100c2290: damage -> small flinch (6), heard sound -> standoff (0x1c), else idle stand (1).
	if (HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
		return GetScheduleOfType(SCHED_ALERT_SMALL_FLINCH);
	if (HasConditions(bits_COND_HEAR_SOUND))
		return GetScheduleOfType(SCHED_STANDOFF);
	return GetScheduleOfType(SCHED_IDLE_STAND);
}

// Retail CBeakBirtherGib::Spawn @0x100c1cd0 and FlopAnim @0x100c1c20.
// This is the large, stationary animated fragment accompanying the smaller
// beakbryo gibs created by CBeakBirther::GibMonster @0x100c2010.
class CBeakBirtherGib : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int BloodColor() override { return 12; }
	void EXPORT FlopAnim();
};
LINK_ENTITY_TO_CLASS(beakbirther_gib, CBeakBirtherGib);

void CBeakBirtherGib::Precache()
{
	PrecacheModel("models/beakqueengib.mdl");
}

void CBeakBirtherGib::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("beakbirther_gib");
	SET_MODEL(ENT(pev), "models/beakqueengib.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->takedamage = DAMAGE_NO;
	SetThink(&CBeakBirtherGib::FlopAnim);
	SetSequenceByName("bodygib#start");
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time;
}

void CBeakBirtherGib::FlopAnim()
{
	StudioFrameAdvance();
	if (m_fSequenceFinished)
	{
		SetSequenceByName("bodygib#idle");
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
}

void CBeakBirther::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/beakqueen.mdl");
	UTIL_SetSize(pev, Vector(-10, -10, -10), Vector(10, 10, 10));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	m_bloodColor = 247; // retail Spawn @0x100c21c0 (+0x238)
	pev->effects = 0;
	pev->health = 150;
	m_flFieldOfView = -0.7f;

	MonsterInit();
}

void CBeakBirther::Precache()
{
	PrecacheModel("models/beakqueen.mdl");
	PrecacheModel("models/beakqueengib.mdl");
	PrecacheModel("models/beakbryo.mdl");
	PrecacheModel("sprites/gorebirth.spr");
	PrecacheModel("sprites/gibbirth.spr");

	PrecacheSound("beak/qbroar1.wav");
	PrecacheSound("beak/qbroar2.wav");
	PrecacheSound("beak/qbroar3.wav");
	// Nachtrag 2026-09-06 (In-Game-Test via monster_zoo_test): "beak/
	// qbdie.wav" existiert nicht im retail-Sound-Ordner - vorhanden
	// sind nur die generischen "beak/die1.wav"/"die2.wav" (auch von
	// monster_beak genutzt), kein eigener "qb"-Todessound fuer die
	// Beak-Birther/-Queen. Auf die naechstliegende echte Datei
	// umgestellt (dokumentierte Annahme, kein RE-Fund).
	PrecacheSound("beak/die1.wav");
}

void CBeakBirther::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	if (pEvent->event == 1)
	{
		// Retail FUN_100c1f60 selects one of the three queen-roar sounds
		// for animation event 1 before running the shared event handler.
		const char* const roars[] = {
			"beak/qbroar1.wav",
			"beak/qbroar2.wav",
			"beak/qbroar3.wav",
		};
		EMIT_SOUND(ENT(pev), CHAN_VOICE, roars[RANDOM_LONG(0, ARRAYSIZE(roars) - 1)], 1.0f, ATTN_NORM);
	}

	CBaseMonster::HandleAnimEvent(pEvent);
}

void CBeakBirther::GibMonster()
{
	// Retail emits a Gunman-specific birth-sprite message here. Its client
	// handler is not present in this SDK, but the server-side fragment count,
	// model, target firing and removal are fully recovered from 0x100c2010.
	if (CVAR_GET_FLOAT("violence_agibs") != 0)
		CGib::SpawnCustomGibs(pev, RANDOM_LONG(4, 8), "models/beakbryo.mdl");

	CBaseEntity::Create("beakbirther_gib", pev->origin, pev->angles, edict());
	SUB_UseTargets(this, USE_TOGGLE, 0);
	UTIL_Remove(this);
}
