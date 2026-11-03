#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "basebird.h"

void CBaseBird::SpawnBird(const char* pszModel)
{
	Precache();

	SET_MODEL(ENT(pev), pszModel);
	UTIL_SetSize(pev, Vector(-32, -32, -16), Vector(32, 32, 16));
	UTIL_SetOrigin(pev, pev->origin);

	// Retail CBaseBird init @ 0x100689a0 (no MonsterInit)
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->flags |= FL_FLY;
	// entvars +0x16c is takedamage (not framerate): 2.0 = DAMAGE_AIM
	pev->takedamage = DAMAGE_AIM;
	pev->health = 10;
	m_flFieldOfView = -0.7f; // member +0x124 (assumed CBaseMonster::m_flFieldOfView)
	pev->sequence = 0;
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();

	SetThink(&CBaseBird::HuntThink);
	SetTouch(&CBaseBird::FlyTouch);
	pev->nextthink = gpGlobals->time + 1.0f;
	m_flFlapTime = m_flIdleTime = gpGlobals->time;
}

void CBaseBird::SetGoal(CBaseEntity* pGoal)
{
	m_hGoal = pGoal;
	m_vecGoal = pGoal->pev->origin;
	UTIL_MakeVectors(pGoal->pev->angles);
	m_vecGoalDir = gpGlobals->v_forward;
}

// Port of retail CBaseBird::HuntThink @ 0x10068e60. Eagle overrides the
// idle and flap sounds; Pteradon inherits the Retail no-op flap hook.
void CBaseBird::HuntThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_flSpeedAccum < 600)
		m_flSpeedAccum += 10;

	if (m_flIdleTime < gpGlobals->time)
	{
		m_flIdleTime = gpGlobals->time + RANDOM_LONG(20, 30);
		IdleSound();
	}

	if (m_fSequenceFinished)
	{
		if (gpGlobals->time <= m_flFlapTime || RANDOM_LONG(0, 99) >= 5)
		{
			SetSequenceByName("glide");
		}
		else
		{
			m_flFlapTime = gpGlobals->time + RANDOM_LONG(3, 4);
			SetSequenceByName("glideflap");
			FlapSound();
		}
	}

	CBaseEntity* pGoal = m_hGoal;
	if (!pGoal && !FStringNull(pev->target))
	{
		pGoal = UTIL_FindEntityByTargetname(nullptr, STRING(pev->target));
		if (pGoal)
			SetGoal(pGoal);
	}

	if (pGoal)
	{
		if ((pev->origin - m_vecGoal).Length() < 100 && !FStringNull(pGoal->pev->target))
		{
			CBaseEntity* pNext = UTIL_FindEntityByTargetname(nullptr, STRING(pGoal->pev->target));
			if (pNext)
			{
				m_vecPrevGoal = m_vecGoal;
				SetGoal(pNext);
			}
		}

		if ((pev->origin - m_vecGoal).Length() <= 100)
		{
			m_vecDesiredDir = m_vecGoalDir;
		}
		else
		{
			Vector toGoal = (m_vecGoal - pev->origin).Normalize();
			m_vecDesiredDir = toGoal;
			if (gpGlobals->time < m_flGoalTime + 90)
			{
				Vector toPrev = (m_vecPrevGoal - pev->origin).Normalize();
				if (DotProduct(toGoal, toPrev) > 0.25)
					m_vecDesiredDir = toPrev;
			}
		}
	}

	SteerFlight();
}

// Port of retail flight steering @ 0x10069530. Thrust update compares W.z
// with the goal Z, the final pitch cases the signed speed (frame slots
// resolved, cbasebird_huntthink_2026-10-31.md Nachtrag 7).
void CBaseBird::SteerFlight()
{
	// yaw rate
	Vector ang = pev->angles + pev->avelocity * 2;
	ang.x += 8;
	UTIL_MakeVectors(ang);
	float side = DotProduct(m_vecDesiredDir, gpGlobals->v_right);
	if (side < 0 && pev->avelocity.y < 60)
		pev->avelocity.y += 8;
	else if (side >= 0 && pev->avelocity.y > -60)
		pev->avelocity.y -= 8;

	// predicted point
	ang = pev->angles + pev->avelocity;
	ang.x += 8;
	UTIL_MakeVectors(ang);
	Vector vecPred = pev->origin + pev->velocity * 2 + gpGlobals->v_up * (m_flThrust * 20);
	Vector vecW(vecPred.x, vecPred.y, vecPred.z - 768);

	// thrust
	UTIL_MakeVectors(Vector(pev->angles.x + 8, pev->angles.y, pev->angles.z));
	pev->velocity = pev->velocity + gpGlobals->v_up * m_flThrust;
	pev->velocity.z -= 38.4;
	float speed = pev->velocity.Length();

	if (speed < 400 && m_flFlapTime < gpGlobals->time)
	{
		m_flFlapTime = gpGlobals->time + RANDOM_LONG(3, 4);
		SetSequenceByName("glideflap");
		FlapSound();
	}

	if (pev->velocity.x * gpGlobals->v_forward.x + pev->velocity.y * gpGlobals->v_forward.y < 0)
		speed = -speed;

	Vector G = m_vecGoal - vecW;
	float gForward = DotProduct(G, gpGlobals->v_forward);
	float gRight = -DotProduct(G, gpGlobals->v_right);

	// roll rate
	if (gRight <= 0)
	{
		if (pev->angles.z >= 30 || pev->avelocity.z >= 15)
			pev->avelocity.z -= 2;
		else
			pev->avelocity.z += 4;
	}
	else
	{
		if (pev->angles.z <= -30 || pev->avelocity.z <= -15)
			pev->avelocity.z += 2;
		else
			pev->avelocity.z -= 4;
	}

	// damping
	pev->velocity.x *= 1 - 0.05 * fabs(gpGlobals->v_right.x);
	pev->velocity.y *= 1 - 0.05 * fabs(gpGlobals->v_right.y);
	pev->velocity.z *= 1 - 0.05 * fabs(gpGlobals->v_right.z);
	pev->velocity = pev->velocity * 0.995f;

	// thrust update
	if (m_flThrust < 80 && vecW.z < m_vecGoal.z)
		m_flThrust += 12;
	else if (m_flThrust > 30 && vecW.z > m_vecGoal.z)
		m_flThrust -= 8;

	// pitch rate
	if (gForward > 0 && speed < m_flSpeedAccum && pev->angles.x + pev->avelocity.x > -40)
	{
		pev->avelocity.x -= 12;
		return;
	}
	if (gForward < 0 && speed > -50 && pev->angles.x + pev->avelocity.x < 20)
	{
		pev->avelocity.x += 12;
		return;
	}
	float pitch = pev->angles.x + pev->avelocity.x;
	if (pitch > 0)
		pev->avelocity.x -= 4;
	else if (pitch < 0)
		pev->avelocity.x += 4;
}

void CBaseBird::FlyTouch(CBaseEntity* pOther)
{
	// Retail CBaseBird::FlyTouch (0x10068d00), shared with decore_eagle.
	if (pOther != nullptr && pOther->pev->movetype == MOVETYPE_FLY)
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		pev->velocity = pev->velocity + tr.vecPlaneNormal * (pev->velocity.Length() + 200.0f);
	}
}

void CBaseBird::Killed(entvars_t* pevAttacker, int iGib)
{
	// Retail CBaseBird::Killed @ 0x10068b30.
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.5f;
	SetSequenceByName("falling");
	SetThink(&CBaseBird::DyingThink);
	SetTouch(&CBaseBird::CrashTouch);
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->health = 0;
	pev->max_health = 0;
	pev->solid = SOLID_TRIGGER;
}

void CBaseBird::DyingThink()
{
	StudioFrameAdvance();
	if (m_fSequenceFinished)
		SetSequenceByName("falling");

	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	pev->avelocity = pev->avelocity * 1.03f;
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CBaseBird::CrashTouch(CBaseEntity* pOther)
{
	SetSequenceByName("dead");
	if (pOther && pOther->pev->movetype == MOVETYPE_FLY)
	{
		SetTouch(NULL);
		SetThink(NULL);
		pev->nextthink = gpGlobals->time;
		pev->velocity = g_vecZero;
		pev->movetype = MOVETYPE_NONE;
		pev->solid = SOLID_TRIGGER;
		UTIL_SetOrigin(pev, pev->origin);
	}
}
