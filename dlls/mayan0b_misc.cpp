//=========================================================
// MAYAN0B's remaining classes with no SDK precedent: random_trigger
// (CRand_Trigger), monster_hatchetfish (CHatchetFish), monster_tank
// (CScriptedTank) + its runtime-only monster_tank_rocket projectile
// (CScriptedTankRocket). (monster_sentry_mini went into turret.cpp
// as CMiniSentry, alongside its CBaseTurret/CSentry relatives.)
//
// Decompiled fresh from gunman.dll this session:
//   random_trigger: LINK @0x10066b20, ctor helper @0x10066bd0
//     (vtable @0x100f65f0), Spawn @0x10066d70, KeyValue @0x10066c00,
//     CRand_Trigger::Rand_TriggerUse @0x10066dc0,
//     CRand_Trigger::Rand_TriggerThink @0x10066de0. Cross-checked
//     against findings/entity_review_list.csv's prior summary
//     (matches exactly: delay/random_min/random_max/start_state
//     keyvalues, Rand_TriggerThink/Use names) but re-verified by
//     decompiling all 4 functions again rather than trusting the doc.
//   monster_hatchetfish: LINK @0x10072200, vtable @0x100f82fc, Spawn
//     @0x10072410, Precache @0x10072800, Classify @0x100723a0
//     (returns 17 = CLASS_INSECT_CRICKET, matches
//     entity_review_list.csv's cross-reference to monster_cricket),
//     BloodColor @0x10072390 (returns -1 = DONT_BLEED),
//     CHatchetFish::SwimThink @0x10073380 (named via symbol table).
//   monster_tank: LINK @0x100a5a10, vtable @0x100ffb80, Spawn
//     @0x100a5a60, Precache @0x100a5be0. No demangled method names in
//     the binary for this class (Ghidra never resolved a class name
//     for it - matches findings' "Literal nicht 100%" caveat).
//     Confirmed: model "models/script_tank.mdl", bbox
//     (-16,-16,0)/(16,16,74), SOLID_SLIDEBOX, MOVETYPE_STEP, health
//     8.0, pev->takedamage explicitly cleared to DAMAGE_NO at the end
//     of Spawn (this tank is invulnerable - a scripted/cutscene prop,
//     not a real combat target, consistent with it being cheap
//     8.0 "health" that's never actually checked). The FGD/findings
//     doc's claimed "247 HP" does not match pev->health in the
//     decompile; the literal 247 (0xf7) does appear in Spawn, but at
//     a CScriptedTank-private member offset, not pev->health - likely
//     a default sequence/animation index instead, not reproduced
//     here (open question, not chased further).
//
// Simplified relative to the original (documented per-case):
// - CHatchetFish: Spawn bounds/FOV, 0.1-s SwimThink cadence, the
//   facing-gated bite event and the 25-unit TraceAttack knockback are
//   reproduced. The exact skill-cvar health and the complex DeadThink
//   schedule branch remain open.
// - CScriptedTank: event IDs 1/2 now launch the rocket exactly where
//   the animation requests it; no autonomous fire timer remains. The
//   rocket uses its retail 700-unit homing loop. The proprietary fire
//   event's additional area-effect payload remains undocumented.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "effects.h"
#include "explode.h"
#include "weapons.h"
#include "skill.h"

//=========================================================
// random_trigger - CRand_Trigger. A trigger_relay equivalent with
// randomized retrigger timing. Use() toggles the enabled/disabled
// state; while enabled, Think fires its targets on a
// wait + RANDOM(random_min, random_max) cycle.
//=========================================================
class CRand_Trigger : public CBaseEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT Rand_TriggerUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT Rand_TriggerThink();

	bool m_bState = false;
	float m_fBaseTime = 5.0;
	float m_fMinAddTime = 5.0;
	float m_fMaxAddTime = 10.0;
};
LINK_ENTITY_TO_CLASS(random_trigger, CRand_Trigger);

TYPEDESCRIPTION CRand_Trigger::m_SaveData[] =
	{
		DEFINE_FIELD(CRand_Trigger, m_fMaxAddTime, FIELD_FLOAT),
		DEFINE_FIELD(CRand_Trigger, m_fMinAddTime, FIELD_FLOAT),
		DEFINE_FIELD(CRand_Trigger, m_fBaseTime, FIELD_FLOAT),
		DEFINE_FIELD(CRand_Trigger, m_bState, FIELD_BOOLEAN),
	};

IMPLEMENT_SAVERESTORE(CRand_Trigger, CBaseEntity);

bool CRand_Trigger::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "start_state"))
	{
		m_bState = atoi(pkvd->szValue) != 0;
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_fBaseTime = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "random_max"))
	{
		m_fMaxAddTime = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "random_min"))
	{
		m_fMinAddTime = atof(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CRand_Trigger::Spawn()
{
	SetUse(&CRand_Trigger::Rand_TriggerUse);
	SetThink(&CRand_Trigger::Rand_TriggerThink);
	pev->solid = SOLID_NOT;

	pev->nextthink = gpGlobals->time + m_fBaseTime + RANDOM_FLOAT(m_fMinAddTime, m_fMaxAddTime);
}

void CRand_Trigger::Rand_TriggerUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_bState = !m_bState;
}

void CRand_Trigger::Rand_TriggerThink()
{
	if (!m_bState)
	{
		pev->nextthink = gpGlobals->time + 1.0;
		return;
	}

	FireTargets(STRING(pev->target), this, this, USE_TOGGLE, 0);
	pev->nextthink = gpGlobals->time + m_fBaseTime + RANDOM_FLOAT(m_fMinAddTime, m_fMaxAddTime);
}

//=========================================================
// monster_hatchetfish - CHatchetFish. Frisch dekompiliert 2026-10-31
// (Spawn @0x10072410, Activate @0x100725d0/100725e0, SwimThink
// @0x10073380, Zustandswechsel @0x10072710, Hindernisprobe @0x10072bc0,
// Bewegung/Animation @0x10073110, Killed @0x10073a30, DeadThink
// @0x10072fe0, Biss @0x10072990, TakeDamage @0x10072860, Touch
// @0x10072250). Das Zeitfeld +0x298 ersetzt nextthink fuer den
// Zustandswechsel; nextthink bleibt 0,1 s.
//=========================================================
class CHatchetFish : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void Activate() override;
	void SetObjectCollisionBox() override;
	int Classify() override { return CLASS_INSECT_CRICKET; }
	int BloodColor() override { return DONT_BLEED; }
	int IRelationship(CBaseEntity* pTarget) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void Touch(CBaseEntity* pOther) override;
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT SwimThink();
	void EXPORT DeadThink();

private:
	void ComputeDepthRange();
	void ChooseState();
	float CheckObstacle(CBaseEntity* pEnemy);
	void UpdateMotion();

	float m_flTurnOffset;	 // +0x270
	int m_bObstructed;		 // +0x274
	float m_flSmoothSpeed;	 // +0x278
	float m_flSpeedFactor;	 // +0x27c
	float m_flMaxZ;			 // +0x280
	float m_flMinZ;			 // +0x284
	float m_flTargetZ;		 // +0x288
	float m_flNextDepthTime; // +0x28c
	float m_flProbeTime;	 // +0x290
	float m_flNextTargetTime; // +0x294
	float m_flNextStateTime; // +0x298
};
LINK_ENTITY_TO_CLASS(monster_hatchetfish, CHatchetFish);

TYPEDESCRIPTION CHatchetFish::m_SaveData[] =
	{
		DEFINE_FIELD(CHatchetFish, m_flTurnOffset, FIELD_FLOAT),
		DEFINE_FIELD(CHatchetFish, m_bObstructed, FIELD_INTEGER),
		DEFINE_FIELD(CHatchetFish, m_flSmoothSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CHatchetFish, m_flSpeedFactor, FIELD_FLOAT),
		DEFINE_FIELD(CHatchetFish, m_flMaxZ, FIELD_FLOAT),
		DEFINE_FIELD(CHatchetFish, m_flMinZ, FIELD_FLOAT),
		DEFINE_FIELD(CHatchetFish, m_flTargetZ, FIELD_FLOAT),
		DEFINE_FIELD(CHatchetFish, m_flNextDepthTime, FIELD_TIME),
		DEFINE_FIELD(CHatchetFish, m_flProbeTime, FIELD_TIME),
		DEFINE_FIELD(CHatchetFish, m_flNextTargetTime, FIELD_TIME),
		DEFINE_FIELD(CHatchetFish, m_flNextStateTime, FIELD_TIME),
};
IMPLEMENT_SAVERESTORE(CHatchetFish, CBaseMonster);

void CHatchetFish::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/hatchet.mdl");
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->flags |= FL_SWIM;
	pev->health = gSkillData.hatchetFishHealth;
	UTIL_SetSize(pev, Vector(-1, -1, 0), Vector(1, 1, 2));
	m_flFieldOfView = -8.0f;
	m_flDistLook = 750.0f;

	MonsterInit();

	// Retail ersetzt nach MonsterInit Think, Touch und Use: MonsterInitThink
	// laeuft nie, der Fisch denkt nur ueber SwimThink.
	SetThink(&CHatchetFish::SwimThink);
	SetTouch(nullptr);
	SetUse(nullptr);
	pev->view_ofs = g_vecZero;
	m_flTurnOffset = 0;
	m_bObstructed = 0;
	SetActivity(ACT_SWIM);
	SetState(MONSTERSTATE_IDLE);
	m_flNextStateTime = RANDOM_FLOAT(1.0, 5.0) + gpGlobals->time;
}

void CHatchetFish::Precache()
{
	PrecacheModel("models/hatchet.mdl");

	PrecacheSound("hatchet/hatchet_bite1.wav");
	PrecacheSound("hatchet/hatchet_bite2.wav");
	PrecacheSound("hatchet/hatchet_splash1.wav");
	PrecacheSound("hatchet/hatchet_splash2.wav");
	PrecacheSound("hatchet/hatchet_sight1.wav");
	PrecacheSound("hatchet/hatchet_sight2.wav");
}

void CHatchetFish::SetObjectCollisionBox()
{
	pev->absmin = Vector(pev->origin.x - 8, pev->origin.y - 8, pev->origin.z);
	pev->absmax = Vector(pev->origin.x + 8, pev->origin.y + 8, pev->origin.z + 2);
}

int CHatchetFish::IRelationship(CBaseEntity* pTarget)
{
	// Retail Slot 66: Spieler gelten als R_DL, sonst Basistabelle.
	if (pTarget->IsPlayer())
		return R_DL;
	return CBaseMonster::IRelationship(pTarget);
}

void CHatchetFish::Touch(CBaseEntity* pOther)
{
	if (pOther->IsPlayer() && gpGlobals->trace_ent && gpGlobals->trace_ent == edict())
	{
		pev->basevelocity = pOther->pev->velocity * 2;
		pev->flags |= FL_BASEVELOCITY;
	}
}

// Tiefenband aus Boden (Trace 400 nach unten) und Wasseroberflaeche.
void CHatchetFish::Activate()
{
	ComputeDepthRange();
}

void CHatchetFish::ComputeDepthRange()
{
	TraceResult tr;
	Vector vecEnd = Vector(pev->origin.x, pev->origin.y, pev->origin.z - 400);
	UTIL_TraceLine(pev->origin, vecEnd, missile, edict(), &tr);

	if (tr.flFraction == 1.0f)
		m_flMinZ = vecEnd.z;
	else
		m_flMinZ = tr.vecEndPos.z + 1.0f;

	const float flSurface = UTIL_WaterLevel(pev->origin, pev->origin.z, pev->origin.z + 400) - 1.0f;
	const float flFloor = m_flMinZ;
	m_flMinZ = flFloor * 0.8f + flSurface * 0.2f;
	m_flMaxZ = flFloor * 0.2f + flSurface * 0.8f;
	m_flTargetZ = RANDOM_FLOAT(m_flMinZ, m_flMaxZ);
	m_flNextDepthTime = RANDOM_FLOAT(5.0, 7.0) + gpGlobals->time;
}

// Retail @0x10072710: Zustandswechsel auf dem eigenen Zeitfeld +0x298.
void CHatchetFish::ChooseState()
{
	m_flNextStateTime = RANDOM_FLOAT(3.0, 6.0) + gpGlobals->time;

	if (m_MonsterState == MONSTERSTATE_COMBAT)
	{
		m_hEnemy = nullptr;
		SetState(MONSTERSTATE_IDLE);
		m_flProbeTime = 0;
		return;
	}

	Look((int)m_flDistLook);
	CBaseEntity* pEnemy = BestVisibleEnemy();
	if (pEnemy && pEnemy->pev->waterlevel != 0)
	{
		m_hEnemy = pEnemy;
		SetState(MONSTERSTATE_COMBAT);
		m_flNextStateTime = RANDOM_FLOAT(18.0, 25.0) + gpGlobals->time;
	}
}

// Retail @0x10072bc0: Vorwaertstrace 45 Einheiten; liefert den Bruchteil.
// Die Seitenproben liegen 45 Einheiten voraus und je 20 Einheiten seitlich.
float CHatchetFish::CheckObstacle(CBaseEntity* pEnemy)
{
	TraceResult tr;
	UTIL_MakeVectors(Vector(-pev->angles.x, pev->angles.y, pev->angles.z));
	const Vector vecFwd = gpGlobals->v_forward * 45.0f;
	const Vector vecRight = gpGlobals->v_right * 10.0f;

	UTIL_TraceLine(pev->origin, pev->origin + vecFwd, missile, edict(), &tr);
	if (tr.fAllSolid)
		pev->speed = -150.0f;

	if (tr.flFraction != 1.0f)
	{
		if (!pEnemy || tr.pHit != pEnemy->edict())
			return tr.flFraction;
		if (fabs(m_flTargetZ - pev->origin.z) > 10.0f)
			return tr.flFraction;
	}

	if (m_flProbeTime < gpGlobals->time)
	{
		// Retail checks a point 45 units ahead and 20 units to either side.
		UTIL_TraceLine(pev->origin, pev->origin + vecFwd + vecRight * 2, missile, edict(), &tr);
		if (tr.flFraction != 1.0f)
			return tr.flFraction;
		UTIL_TraceLine(pev->origin, pev->origin + vecFwd - vecRight * 2, missile, edict(), &tr);
		if (tr.flFraction != 1.0f)
			return tr.flFraction;
		m_flProbeTime = RANDOM_FLOAT(0.5, 1.0) + gpGlobals->time;
	}
	return 1.0f;
}

void CHatchetFish::SwimThink()
{
	float flTurn = 0;
	float flSpeedTarget = 75.0f;
	CBaseEntity* pEnemy = nullptr;

	edict_t* pClient = FIND_CLIENT_IN_PVS(edict());
	if (FNullEnt(pClient))
	{
		// Ohne Spieler in der PVS ruht der Fisch.
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(1.0, 1.5);
		pev->velocity = g_vecZero;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	if (m_flNextDepthTime < gpGlobals->time)
		ComputeDepthRange();
	if (m_flNextStateTime < gpGlobals->time)
		ChooseState();

	ClearConditions(bits_COND_CAN_MELEE_ATTACK1);

	if (m_MonsterState == MONSTERSTATE_COMBAT)
	{
		pEnemy = m_hEnemy;
		if (!pEnemy)
		{
			ChooseState();
		}
		else
		{
			m_flTargetZ = pEnemy->pev->view_ofs.z + pEnemy->pev->origin.z - 5.0f;
			if (m_flTargetZ < m_flMinZ)
				m_flTargetZ = m_flMinZ;
			else if (m_flTargetZ > m_flMaxZ)
				m_flTargetZ = m_flMaxZ;

			Vector vecToEnemy = pEnemy->pev->origin - pev->origin;
			vecToEnemy.z += pEnemy->pev->view_ofs.z;
			if (vecToEnemy.Length() < 40.0f)
				SetConditions(bits_COND_CAN_MELEE_ATTACK1);

			const float flDiff = UTIL_AngleDiff(UTIL_VecToYaw(vecToEnemy), UTIL_AngleMod(pev->angles.y));
			flTurn = flDiff;
			if (flDiff < -67.5f)
				flTurn = -67.5f;
			else if (flDiff <= 67.5f)
				flSpeedTarget = 150.0f;
			else
				flTurn = 67.5f;
		}
	}
	else
	{
		if (m_flNextTargetTime < gpGlobals->time)
		{
			m_flTargetZ = (RANDOM_FLOAT(m_flMinZ, m_flMaxZ) + m_flTargetZ) * 0.5f;
			m_flNextTargetTime = RANDOM_FLOAT(1.0, 4.0) + gpGlobals->time;
		}
		if (RANDOM_LONG(0, 100) < 10)
			flTurn = RANDOM_LONG(-30, 30);

		const Vector vecMoved = pev->origin - pev->oldorigin;
		if (vecMoved.Length() < 1.0f)
			m_flProbeTime = 0;
	}

	m_flSpeedFactor = CheckObstacle(pEnemy);
	pev->oldorigin = pev->origin;
	if (m_flSpeedFactor < 0.1f)
		m_flSpeedFactor = 0.1f;

	if (m_flSpeedFactor == 1.0f)
	{
		if (m_flTurnOffset != 0)
			m_flTurnOffset = 0;
		m_bObstructed = 0;
		pev->speed = UTIL_Approach(flSpeedTarget, pev->speed, 8.0f);
		pev->velocity = gpGlobals->v_forward * pev->speed;
	}
	else
	{
		m_bObstructed = 1;
		m_flSpeedFactor = 1.0f / m_flSpeedFactor;
		if (m_flTurnOffset == 0)
		{
			TraceResult tr;
			const Vector vecFwd = gpGlobals->v_forward * 45.0f;
			UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_right * 10.0f + vecFwd, missile, edict(), &tr);
			const float flRight = tr.flFraction;
			UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_right * -10.0f + vecFwd, missile, edict(), &tr);
			const float flDiff = flRight - tr.flFraction;
			if (flDiff > 0.1f)
				m_flTurnOffset = -90.0f;
			else if (flDiff <= -0.1f || RANDOM_LONG(0, 100) >= 50)
				m_flTurnOffset = 90.0f;
			else
				m_flTurnOffset = -90.0f;
		}
		pev->speed = UTIL_Approach(-150.0f, pev->speed, m_flSpeedFactor);
		pev->velocity = gpGlobals->v_forward * pev->speed;
	}

	pev->ideal_yaw = flTurn + m_flTurnOffset;
	UpdateMotion();
}

// Retail @0x10073110: Drehrate, Rollen, Neigung und Animation.
void CHatchetFish::UpdateMotion()
{
	float flDelta = (pev->speed - m_flSmoothSpeed) * 0.1f;
	m_flSmoothSpeed = m_flSmoothSpeed * 0.8f + pev->speed * 0.2f;
	if (flDelta < 0)
		flDelta = -flDelta;
	float flRate = flDelta + 1.0f;
	if (flRate < 0.5f)
		flRate = 0.5f;
	if (flRate > 1.9f)
		flRate = 1.9f;
	pev->framerate = flRate;

	pev->avelocity.y = m_bObstructed ? pev->ideal_yaw * m_flSpeedFactor : pev->ideal_yaw;

	if (pev->avelocity.y > 150.0f)
		m_IdealActivity = ACT_TURN_LEFT;
	else if (pev->avelocity.y >= -150.0f)
		m_IdealActivity = ACT_SWIM;
	else
		m_IdealActivity = ACT_TURN_RIGHT;

	const float flDz = m_flTargetZ - pev->origin.z;
	float flPitch;
	if (flDz < -10.0f)
		flPitch = -30.0f;
	else
		flPitch = (flDz > 10.0f) ? 30.0f : 0.0f;
	pev->angles.x = UTIL_Approach(flPitch, pev->angles.x, 6.0f);
	pev->avelocity.z = -(pev->avelocity.y * 0.25f + pev->angles.z);

	if (m_MonsterState == MONSTERSTATE_COMBAT && HasConditions(bits_COND_CAN_MELEE_ATTACK1))
		m_IdealActivity = ACT_MELEE_ATTACK1;

	if (pev->waterlevel == 0)
	{
		// Ausserhalb des Wassers zappelt der Fisch am Boden.
		pev->movetype = MOVETYPE_TOSS;
		m_IdealActivity = ACT_TWITCH;
		pev->velocity = g_vecZero;
		pev->angles.z = 0;
		pev->angles.x = 0;
		if (pev->framerate < 1.0f)
			pev->framerate = 1.0f;
	}
	else if (pev->movetype == MOVETYPE_TOSS)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->flags &= ~FL_ONGROUND;
		ComputeDepthRange();
		m_flNextDepthTime = gpGlobals->time + 2.0f;
	}

	if (m_Activity != m_IdealActivity)
		SetActivity(m_IdealActivity);

	float flInterval = StudioFrameAdvance(0);
	DispatchAnimEvents(flInterval);
}

void CHatchetFish::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	if (pEvent->event == 1)
	{
		CBaseEntity* pEnemy = m_hEnemy;
		if (pEnemy)
		{
			UTIL_MakeVectors(pev->angles);
			Vector vecForward = gpGlobals->v_forward;
			vecForward.z = 0;
			Vector vecToEnemy = pEnemy->pev->origin - pev->origin;
			vecToEnemy.z = 0;
			// Retail: beide Vektoren normalisiert, Skalarprodukt > 0,9.
			if (DotProduct(vecForward.Normalize(), vecToEnemy.Normalize()) > 0.9)
				pEnemy->TakeDamage(pev, pev, gSkillData.hatchetFishDamage, DMG_SLASH);
		}
		m_flNextStateTime -= 2.0f;
	}
	else if (pEvent->event != 2)
	{
		CBaseMonster::HandleAnimEvent(pEvent);
	}
}

bool CHatchetFish::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	pev->velocity = g_vecZero;
	if (pevInflictor)
	{
		// Retail @0x10072860: Rueckstoss 25 Einheiten vom Verursacher weg.
		Vector vecAway = pev->origin - pevInflictor->origin;
		if (vecAway.Length() == 0)
			vecAway = Vector(0, 0, 1);
		else
			vecAway = vecAway.Normalize();
		pev->velocity = vecAway * 25.0f;
	}
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CHatchetFish::Killed(entvars_t* pevAttacker, int iGib)
{
	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	if (pOwner)
		pOwner->DeathNotice(pev);

	if (pev->waterlevel == 0)
	{
		SetActivity(ACT_DIEFORWARD);
	}
	else
	{
		pev->angles.z = 0;
		pev->angles.x = 0;
		pev->origin.z += 1.0f;
		pev->avelocity = g_vecZero;
		if (RANDOM_LONG(0, 99) < 70)
			pev->avelocity.y = RANDOM_LONG(-720, 720);
		pev->gravity = 0.02f;
		pev->flags &= ~FL_ONGROUND;
		SetActivity(ACT_DIESIMPLE);
	}
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_NO;
	SetThink(&CHatchetFish::DeadThink);
}

void CHatchetFish::DeadThink()
{
	if (m_fSequenceFinished)
	{
		if (m_Activity == ACT_DIEFORWARD)
		{
			SetThink(nullptr);
			pev->framerate = 0;
			return;
		}
		if ((pev->flags & FL_ONGROUND) != 0)
		{
			pev->solid = SOLID_NOT;
			SetActivity(ACT_DIEFORWARD);
		}
	}

	StudioFrameAdvance(0);
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->velocity.x != 0 || pev->velocity.y != 0)
	{
		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin + pev->velocity * 0.5f, missile, edict(), &tr);
		if (tr.flFraction != 1.0f)
		{
			pev->velocity.x = 0;
			pev->velocity.y = 0;
		}
	}
}

//=========================================================
// monster_tank - CScriptedTank. An invulnerable scripted/cutscene tank;
// its fire animation dispatches the rocket through events 1 and 2.
//=========================================================
class CScriptedTank : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this - see the
	// feedback-setactivity-act-idle-stomps-scripted-sequence memory
	// note (same fix as CFriendlyGunman etc).
	void SetActivity(Activity NewActivity) override
	{
		if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && m_pCine != nullptr)
		{
			m_Activity = NewActivity;
			m_IdealActivity = NewActivity;
			return;
		}
		CBaseMonster::SetActivity(NewActivity);
	}

private:
	void FireRocket();
	int m_iLastAttachment = 2;
};
LINK_ENTITY_TO_CLASS(monster_tank, CScriptedTank);

void CScriptedTank::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/script_tank.mdl");
	// Retail Spawn @0x100a5a60 (frisch dekompiliert 2026-10-31): Box z = 72
	// (SDK hatte 74), Blutfarbe 247, Sichtfeld 0.5; takedamage wird NACH
	// MonsterInit auf 0 gesetzt. spawnflags 4 -> SOLID_NOT; 32 -> kein
	// Kollisionskoerper, MOVETYPE_FLY und Nullbox.
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = 247;
	pev->health = 8;
	m_flFieldOfView = 0.5f;
	pev->takedamage = DAMAGE_NO;

	MonsterInit();

	if ((pev->spawnflags & 4) != 0)
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
	}
	if ((pev->spawnflags & 0x20) != 0)
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
		pev->movetype = MOVETYPE_FLY;
		UTIL_SetSize(pev, g_vecZero, g_vecZero);
		UTIL_SetOrigin(pev, pev->origin);
	}
	pev->takedamage = DAMAGE_NO;
	UTIL_SetOrigin(pev, pev->origin);
}

void CScriptedTank::Precache()
{
	PrecacheModel("models/script_tank.mdl");
	PrecacheSound("weapons/explode4.wav");
	// Nachtrag 2026-09-06 (In-Game-Test via monster_zoo_test): der
	// vorherige Pfad "tank/tank_engineidle.wav" existiert nicht -
	// echter retail-Pfad ist "Tank/engineidle.wav" (Ordner
	// gross geschrieben, kein "tank_"-Praefix beim Dateinamen).
	PrecacheSound("Tank/engineidle.wav");
}

void CScriptedTank::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	if (pEvent->event == 1 || pEvent->event == 2)
	{
		FireRocket();
		return;
	}
	CBaseMonster::HandleAnimEvent(pEvent);
}

void CScriptedTank::FireRocket()
{
	// The model owns the fire timing. Alternate its two verified cannon
	// attachments to preserve the safe, elevated launch origin.
	Vector vecSrc, vecAnglesUnused;
	GetAttachment(m_iLastAttachment, vecSrc, vecAnglesUnused);
	m_iLastAttachment = (m_iLastAttachment == 2) ? 3 : 2;

	CBaseEntity* pRocket = CBaseEntity::Create("monster_tank_rocket", vecSrc, pev->angles, edict());
	if (pRocket)
	{
		UTIL_MakeVectors(pev->angles);
		Vector vecDir = gpGlobals->v_forward;
		pRocket->pev->velocity = vecDir * 400;
		pRocket->pev->angles = UTIL_VecToAngles(vecDir);
	}
}

//=========================================================
// The runtime-only homing rocket fired by monster_tank. No LINK-time map
// placement; spawned only from the tank's fire animation events.
//=========================================================
class CScriptedTankRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	// Retail Classify (vtable slot 8, @0x100a4680) returns 19.
	int Classify() override { return 19; }
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT RocketThink();
	void EXPORT RocketTouch(CBaseEntity* pOther);

private:
	CSprite* m_pFlame = nullptr;
	int m_iFireBeam = 0; // Retail +0x274 (sprites/firebeam.spr)
	int m_iWhiteBeam = 0; // Retail +0x27c (sprites/white2.spr)
};
LINK_ENTITY_TO_CLASS(monster_tank_rocket, CScriptedTankRocket);

TYPEDESCRIPTION CScriptedTankRocket::m_SaveData[] =
	{
		DEFINE_FIELD(CScriptedTankRocket, m_pFlame, FIELD_CLASSPTR),
	};

IMPLEMENT_SAVERESTORE(CScriptedTankRocket, CGrenade);

void CScriptedTankRocket::Precache()
{
	PrecacheModel("models/dmlrocket.mdl");
	m_iFireBeam = PrecacheModel("sprites/firebeam.spr");
	m_iWhiteBeam = PrecacheModel("sprites/white2.spr");
	PrecacheModel("sprites/part2.spr");
	PrecacheModel("sprites/explosion4.spr");
	PrecacheModel("sprites/smokering.spr");
	PrecacheSound("weapons/explode4.wav");
	PrecacheSound("Tank/firering.wav");
	PrecacheSound("ambience/flameburst1.wav");
	// Nachtrag 2026-09-06 (Live-Report mit Retail-Referenzscreenshot,
	// siehe CAnimeRocket::Spawn() in rust4a_misc.cpp fuer die volle
	// Begruendung): monster_tank_rocket's reale Precache-Liste
	// (fgd_gap_closure.md) fuehrt "sprites/flame.spr" bereits explizit
	// auf, zusammen mit demselben FUN_10015400-Anhaenge-Helfer wie
	// CAnimeRocket - echter Mechanismus ist eine angehaengte additive
	// env_sprite, kein TE_BEAMFOLLOW-Rauchschweif.
	PrecacheModel("sprites/flame.spr");
}

void CScriptedTankRocket::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("monster_tank_rocket");
	SET_MODEL(ENT(pev), "models/dmlrocket.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->solid = SOLID_BBOX;
	pev->gravity = 0.75f;
	pev->movetype = MOVETYPE_FLY;
	SetTouch(&CScriptedTankRocket::RocketTouch);
	SetThink(&CScriptedTankRocket::RocketThink);
	pev->nextthink = gpGlobals->time + 0.1f;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "Tank/firering.wav", 1.0f, 0.8f, 0, 100);

	m_pFlame = CSprite::SpriteCreate("sprites/flame.spr", pev->origin, true);
	if (m_pFlame)
	{
		m_pFlame->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation);
		m_pFlame->SetScale(1.0);
		m_pFlame->pev->framerate = 15.0f;
		m_pFlame->SetAttachment(edict(), 0);
	}
	// Retail Spawn @0x100a47e0 (frisch dekompiliert 2026-10-31): zwei
	// TE_BEAMFOLLOW-Schweife (firebeam: Leben 20, Breite 5, Farbe 224/224/100,
	// Helligkeit 255; white2: Leben 9, Breite 1, weiss, Helligkeit 150) zusaetzlich
	// zum angehaengten Flammen-Sprite; takedamage 0.
	pev->takedamage = DAMAGE_NO;
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(m_iFireBeam);
	WRITE_BYTE(20);
	WRITE_BYTE(5);
	WRITE_BYTE(224);
	WRITE_BYTE(224);
	WRITE_BYTE(100);
	WRITE_BYTE(255);
	MESSAGE_END();
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(m_iWhiteBeam);
	WRITE_BYTE(9);
	WRITE_BYTE(1);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(150);
	MESSAGE_END();
	pev->effects |= EF_LIGHT;
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "ambience/flameburst1.wav", 1.0f, 0.5f, 0, 100);
}

void CScriptedTankRocket::RocketThink()
{
	pev->nextthink = gpGlobals->time + 0.1f;
	if (pev->origin.x < -4096 || pev->origin.x > 4096 ||
		pev->origin.y < -4096 || pev->origin.y > 4096 ||
		pev->origin.z < -4096 || pev->origin.z > 4096)
	{
		UTIL_Remove(this);
		return;
	}

	if (!m_hEnemy || !m_hEnemy->IsAlive())
	{
		Look(700);
		m_hEnemy = BestVisibleEnemy();
	}

	if (!m_hEnemy)
		return;

	Vector targetDirection = (m_hEnemy->BodyTarget(pev->origin) - pev->origin).Normalize();
	float speed = pev->velocity.Length();
	Vector currentDirection = pev->velocity.Normalize();
	float dot = V_min(1.0f, V_max(-1.0f, DotProduct(currentDirection, targetDirection)));
	float angle = acos(dot) * (180.0f / 3.14159265f);
	if (angle > 20.0f)
		targetDirection = (currentDirection * (1.0f - 20.0f / angle) + targetDirection * (20.0f / angle)).Normalize();
	pev->velocity = targetDirection * speed;
	pev->angles = UTIL_VecToAngles(pev->velocity);
}

void CScriptedTankRocket::RocketTouch(CBaseEntity* pOther)
{
	if (m_pFlame)
	{
		UTIL_Remove(m_pFlame);
		m_pFlame = nullptr;
	}

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(30);
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects).
	UTIL_ExplosionEffects(pev->origin, 250.0f);

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/explode4.wav", 1.0, ATTN_NORM, 0, 100);
	::RadiusDamage(pev->origin, pev, VARS(pev->owner), 60, 250, CLASS_NONE, DMG_BLAST);
	UTIL_Remove(this);
}
