//=========================================================
// monster_human_chopper - CChopper. First gap of west6a/west6c in the
// map-driven pass (west3b's monster_human_chopper resolves to the
// same class). CBaseBird-family flying combat boss - see
// findings/entities/CChopper.md and mayan0a_fauna.cpp's CPteradon
// (same family/simplification rationale).
//
// Decompiled fresh from gunman.dll: LINK @0x1006a230, vtable
// @0x100f7388, Spawn @0x1006a320, Precache @0x1006a470. Confirmed
// model "models/chopper.mdl", bbox (-96,-96,0)/(96,96,96) (corrected 2026-10-31: mins.z is 0 in the Spawn decompile),
// MOVETYPE_FLY, SOLID_BBOX, FL_FLY, takedamage=DAMAGE_AIM, health
// from the sk_chopper_h skill global (DAT_10136f44), random start frame
// and EF_INVLIGHT.
// Sets Think=HuntThink/Touch=FlyTouch directly after sequence/bone setup.
// Its retail death override switches to FallingThink/CrashTouch.
//
// Fire logic (FUN_1006be80) fresh-decompiled: gates on yaw/pitch aim
// tolerance toward the last known enemy position, then fires via the
// shared multi-pellet traceline helper FUN_1000f090 (same helper
// already reproduced as a FireBullets() call for CBanditGrunt's
// MechaVolley and weapon_shotgun) at a confirmed 0.06976 rad spread,
// plays "weapons/hks2.wav" - the same MP5-style weapon sound already
// confirmed for CSentry::Shoot() in the stock SDK base.
//
// Simplified relative to the original: HuntThink's full waypoint/banking
// solver (FUN_1006b560) and its exact yaw/pitch firing gate remain open.
// The previous independent CircleThink and missing crash state were replaced
// by retail-backed target, impact and falling lifecycle behavior.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "saverestore.h"
#include "soundent.h"
#include "flightsteer.h"
#include <cmath>

// Shared with mayan6_misc.cpp's dml_cluster/CClusterGrenade.
extern CBaseEntity* Dml_ShootClusterGrenade(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float flDamage);

class CChopper : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_MILITARY; }
	int BloodColor() override { return DONT_BLEED; }
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void GibMonster() override;
	void EXPORT HuntThink();
	void EXPORT FlyTouch(CBaseEntity* pOther);
	void EXPORT FallingThink();
	void EXPORT CrashTouch(CBaseEntity* pOther);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	// Nachtrag 2026-09-06: models/choppergibs.mdl war schon vorher
	// precacht, aber nie in GibMonster() verwendet - siehe
	// CBaseMonster::CustomGibModel() in basemonster.h. (Das ebenfalls
	// precachte models/metalplategibs_green.mdl ist ein von mehreren
	// mechanischen Klassen geteiltes generisches Metalltruemmer-Modell,
	// nicht Chopper-spezifisch - hier bewusst nicht verwendet.)
	const char* CustomGibModel() override { return "models/choppergibs.mdl"; }

private:
	void SteerFlight();
	void SmokeThink();
	bool AimAndFire();
	void ResetFireSalvo();
	void SetGoal(CBaseEntity* pGoal);
	int m_iExplosionSprite = 0;
	int m_iMetalGibs = 0;
	int m_iSalvoShots = 0;		   // +0x274
	float m_flNextFireTime = 0.0f; // +0x270
	// Retail fields (HuntThink @0x1006ae20, steering @0x1006b560)
	float m_flThrust = 0.0f;	   // +0x278
	Vector m_vecLastEnemyPos;	   // +0x27c
	Vector m_vecDesiredDir;		   // +0x288
	Vector m_vecGoal;			   // +0x294
	Vector m_vecGoalDir;		   // +0x2a0
	float m_flSpeedAccum = 0.0f;   // +0x2ac
	float m_flAimPitch = 0.0f;	   // +0x2b0 (bone controller 1)
	float m_flAimYaw = 0.0f;	   // +0x2b4 (bone controller 0)
	float m_flLastSeen = 0.0f;	   // +0x2bc
	float m_flFirstSeen = 0.0f;	   // +0x2c0
	int m_iRotorState = 0;		   // +0x2d0
	int m_iSmokeCounter = 0;	   // +0x2d4
};
LINK_ENTITY_TO_CLASS(monster_human_chopper, CChopper);
LINK_ENTITY_TO_CLASS(monster_apache, CChopper); // Retail: gleiche Vtable 0x100f7388

TYPEDESCRIPTION CChopper::m_SaveData[] =
	{
		DEFINE_FIELD(CChopper, m_iExplosionSprite, FIELD_INTEGER),
		DEFINE_FIELD(CChopper, m_iMetalGibs, FIELD_INTEGER),
		DEFINE_FIELD(CChopper, m_iSalvoShots, FIELD_INTEGER),
		DEFINE_FIELD(CChopper, m_flNextFireTime, FIELD_TIME),
		DEFINE_FIELD(CChopper, m_flThrust, FIELD_FLOAT),
		DEFINE_FIELD(CChopper, m_vecLastEnemyPos, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CChopper, m_vecDesiredDir, FIELD_VECTOR),
		DEFINE_FIELD(CChopper, m_vecGoal, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CChopper, m_vecGoalDir, FIELD_VECTOR),
		DEFINE_FIELD(CChopper, m_flSpeedAccum, FIELD_FLOAT),
		DEFINE_FIELD(CChopper, m_flAimPitch, FIELD_FLOAT),
		DEFINE_FIELD(CChopper, m_flAimYaw, FIELD_FLOAT),
		DEFINE_FIELD(CChopper, m_flLastSeen, FIELD_TIME),
		DEFINE_FIELD(CChopper, m_flFirstSeen, FIELD_TIME),
		DEFINE_FIELD(CChopper, m_iRotorState, FIELD_INTEGER),
		DEFINE_FIELD(CChopper, m_iSmokeCounter, FIELD_INTEGER),
	};

IMPLEMENT_SAVERESTORE(CChopper, CBaseMonster);

void CChopper::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/chopper.mdl");
	UTIL_SetSize(pev, Vector(-96, -96, 0), Vector(96, 96, 96)); // retail Spawn @0x1006a320: mins.z = 0
	UTIL_SetOrigin(pev, pev->origin);

	pev->flags |= FL_FLY;
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM; // retail Spawn: entvars +0x16c = 2.0
	pev->effects |= EF_INVLIGHT;
	pev->health = gSkillData.chopperHealth;
	pev->frame = RANDOM_LONG(0, 255);
	m_flFieldOfView = -0.7f; // retail Spawn @0x1006a320: 0xbf333333
	ResetSequenceInfo();
	InitBoneControllers();
	SetThink(&CChopper::HuntThink);
	SetTouch(&CChopper::FlyTouch);
	pev->nextthink = gpGlobals->time + 1.0;
	ResetFireSalvo();
}

void CChopper::Precache()
{
	PrecacheModel("models/chopper.mdl");
	PrecacheModel("models/chopperblade.mdl");
	PrecacheModel("models/choppergibs.mdl");
	m_iMetalGibs = PrecacheModel("models/metalplategibs_green.mdl");
	PrecacheModel("sprites/white.spr");
	m_iExplosionSprite = PrecacheModel("sprites/fexplo.spr");
	PrecacheSound("turret/tu_fire1.wav");
	PrecacheSound("apache/ap_rotor2.wav");
	PrecacheSound("weapons/explode5.wav");
	PrecacheSound("weapons/hks2.wav");
	PrecacheEvent(1, "events/chopperfire.sc");
}

void CChopper::SetGoal(CBaseEntity* pGoal)
{
	m_pGoalEnt = pGoal;
	if (!pGoal)
		return;
	m_vecGoal = pGoal->pev->origin;
	UTIL_MakeVectors(pGoal->pev->angles);
	m_vecGoalDir = gpGlobals->v_forward;
}

// Retail FUN_1006c270: damaged-chopper smoke puff (TE_SMOKE) when a random
// roll exceeds the remaining health, or while the counter +0x2d4 is positive.
void CChopper::SmokeThink()
{
	if (m_iSmokeCounter >= 1 || (float)RANDOM_LONG(0, 99) > pev->health)
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SMOKE);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z - 32);
		WRITE_SHORT(g_sModelIndexSmoke);
		WRITE_BYTE(RANDOM_LONG(0, 9) + 10);
		WRITE_BYTE(12);
		MESSAGE_END();
	}
	if (m_iSmokeCounter > 0)
		m_iSmokeCounter--;
}

// Port of retail CChopper::HuntThink @ 0x1006ae20.
void CChopper::HuntThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	SmokeThink();

	if (!m_pGoalEnt && !FStringNull(pev->target))
	{
		SetGoal(UTIL_FindEntityByTargetname(nullptr, STRING(pev->target)));
	}

	Look(4092);
	m_hEnemy = BestVisibleEnemy();

	if (m_flSpeedAccum < 600)
		m_flSpeedAccum += 10;

	if (m_hEnemy != nullptr)
	{
		if (!FVisible(m_hEnemy))
		{
			m_hEnemy = nullptr;
		}
		else
		{
			if (m_flLastSeen < gpGlobals->time - 5)
				m_flFirstSeen = gpGlobals->time;
			m_flLastSeen = gpGlobals->time;
			m_vecLastEnemyPos = m_hEnemy->BodyTarget(pev->origin);
		}
	}

	float flDist = (pev->origin - m_vecGoal).Length();
	if (!m_pGoalEnt)
	{
		m_vecGoal = pev->origin;
	}
	else if (flDist < 128)
	{
		SetGoal(UTIL_FindEntityByTargetname(nullptr, STRING(m_pGoalEnt->pev->target)));
		if (m_pGoalEnt)
			flDist = (pev->origin - m_vecGoal).Length();
	}

	if (flDist <= 250)
	{
		m_vecDesiredDir = m_vecGoalDir;
	}
	else
	{
		bool bToEnemy = false;
		if (gpGlobals->time < m_flLastSeen + 90)
		{
			Vector toGoal = (m_vecGoal - pev->origin).Normalize();
			Vector toEnemy = (m_vecLastEnemyPos - pev->origin).Normalize();
			if (DotProduct(toEnemy, toGoal) > 0.25)
			{
				m_vecDesiredDir = toEnemy;
				bToEnemy = true;
			}
		}
		if (!bToEnemy)
			m_vecDesiredDir = (m_vecGoal - pev->origin).Normalize();
	}

	SteerFlight();

	if (m_iSalvoShots != 0)
	{
		if (gpGlobals->time >= m_flNextFireTime && gpGlobals->time < m_flLastSeen + 1.0 &&
			m_flFirstSeen + 2.0 < gpGlobals->time && AimAndFire() && m_flSpeedAccum > 150)
		{
			m_flSpeedAccum = 150;
		}
		return;
	}
	ResetFireSalvo();
}

// Retail flight steering @ 0x1006b560 lives in flightsteer.cpp (shared with
// the rustflier); the chopper only adds the rotor sound below.
void CChopper::SteerFlight()
{
	flightsteer_t st;
	st.thrust = m_flThrust;
	st.desiredDir = m_vecDesiredDir;
	st.goal = m_vecGoal;
	st.speedAccum = m_flSpeedAccum;
	FlightSteer(pev, st);
	m_flThrust = st.thrust;

	// Retail starts the looping sound once (state 0x80) at pitch 110. Later
	// updates use round(dot(chopperVelocity - playerVelocity,
	// directionToPlayer) * 0.02 + 100), clamped to [50, 250].
	if (m_iRotorState == 0)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav", 1.0, 0.3, 0, 110);
		m_iRotorState = 0x80;
	}
	else
	{
		CBaseEntity* pPlayer = UTIL_FindEntityByClassname(nullptr, "player");
		if (pPlayer)
		{
			Vector vecToPlayer = pPlayer->pev->origin - pev->origin;
			if (vecToPlayer.Length() != 0.0f)
				vecToPlayer = vecToPlayer.Normalize();
			else
				vecToPlayer = Vector(0, 0, 1); // Retail zero-length fallback.

			const Vector vecRelativeVelocity = pev->velocity - pPlayer->pev->velocity;
			const float flPitch = DotProduct(vecRelativeVelocity, vecToPlayer) * 0.02f + 100.0f;
			int iPitch = static_cast<int>(std::nearbyint(flPitch));
			if (iPitch < 50)
				iPitch = 50;
			else if (iPitch > 250)
				iPitch = 250;
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav", 1.0, 0.3, SND_CHANGE_PITCH | SND_CHANGE_VOL, iPitch);
		}
	}
}

// Port of retail FUN_1006be80: slews the two turret bone controllers (max 18
// per call) towards the last known enemy position and fires one bullet from
// attachment 1 once both have arrived.
bool CChopper::AimAndFire()
{
	UTIL_MakeVectors(pev->angles);
	Vector vecSrc, vecAng;
	GetAttachment(1, vecSrc, vecAng);

	Vector vecDir = (m_vecLastEnemyPos - vecSrc).Normalize();
	Vector local(DotProduct(vecDir, gpGlobals->v_forward), -DotProduct(vecDir, gpGlobals->v_right), DotProduct(vecDir, gpGlobals->v_up));
	Vector vecAimAngles = UTIL_VecToAngles(local);
	float flYaw = vecAimAngles.y;
	float flPitch = -vecAimAngles.x;
	if (flYaw > 180) flYaw -= 360;
	if (flYaw < -180) flYaw += 360;
	if (flPitch > 180) flPitch -= 360;
	if (flPitch < -180) flPitch += 360;

	auto slew = [](float cur, float target, bool& reached) {
		float next = target;
		if (cur < target && cur + 18 < target)
			next = cur + 18;
		else if (target < cur && cur - 18 > target)
			next = cur - 18;
		reached = (next == target);
		return next;
	};

	bool bYaw, bPitch;
	m_flAimYaw = SetBoneController(0, slew(m_flAimYaw, flYaw, bYaw));
	m_flAimPitch = SetBoneController(1, slew(m_flAimPitch, flPitch, bPitch));

	if (!bYaw || !bPitch)
		return false;

	FireBullets(1, vecSrc, vecDir, Vector(0.06976, 0.06976, 0.06976), 8192, BULLET_MONSTER_MP5, 0);
	m_iSalvoShots--;
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/hks2.wav", 1.0, 0.8, 0, 100);
	return true;
}

void CChopper::ResetFireSalvo()
{
	// Retail FUN_1006be40: twelve shots, followed by a three-second gate.
	m_iSalvoShots = 12;
	m_flNextFireTime = gpGlobals->time + 3.0f;
}

void CChopper::FlyTouch(CBaseEntity* pOther)
{
	// Retail CChopper::FlyTouch @ 0x1006a8a0 only deflects on contact
	// with another MOVETYPE_FLY entity. The global trace supplies the
	// collision-plane normal used for the separation impulse.
	if (pOther && pOther->pev->movetype == MOVETYPE_FLY)
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		pev->velocity = pev->velocity + tr.vecPlaneNormal * (pev->velocity.Length() + 200.0f);
	}
}

void CChopper::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.3f;
	STOP_SOUND(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav");
	UTIL_SetSize(pev, Vector(-32, -32, -64), Vector(32, 32, 0));
	SetThink(&CChopper::FallingThink);
	SetTouch(&CChopper::CrashTouch);
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
}

void CChopper::GibMonster()
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav");
}

void CChopper::FallingThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->avelocity = pev->avelocity * 1.02f;

	Vector spot = pev->origin + Vector(RANDOM_FLOAT(-30, 30), RANDOM_FLOAT(-30, 30), RANDOM_FLOAT(-30, 30));
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, spot);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(spot.x);
	WRITE_COORD(spot.y);
	WRITE_COORD(spot.z);
	WRITE_SHORT(m_iExplosionSprite);
	WRITE_BYTE(50);
	WRITE_BYTE(10);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	pev->flags &= ~FL_ONGROUND;
}

void CChopper::CrashTouch(CBaseEntity* pOther)
{
	if (!pOther || pOther->pev->solid != SOLID_BSP)
		return;

	SetTouch(NULL);
	Vector spot = pev->origin + (pev->mins + pev->maxs) * 0.5f;
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, spot);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(spot.x);
	WRITE_COORD(spot.y);
	WRITE_COORD(spot.z);
	WRITE_SHORT(m_iExplosionSprite);
	WRITE_BYTE(108);
	WRITE_BYTE(30);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, spot);
	WRITE_BYTE(TE_BREAKMODEL);
	WRITE_COORD(spot.x);
	WRITE_COORD(spot.y);
	WRITE_COORD(spot.z);
	WRITE_COORD(100);
	WRITE_COORD(100);
	WRITE_COORD(132);
	WRITE_COORD(pev->velocity.x);
	WRITE_COORD(pev->velocity.y);
	WRITE_COORD(pev->velocity.z);
	WRITE_BYTE(30);
	WRITE_SHORT(m_iMetalGibs);
	WRITE_BYTE(30);
	WRITE_BYTE(20);
	WRITE_BYTE(BREAK_METAL);
	MESSAGE_END();
	EMIT_SOUND(ENT(pev), CHAN_STATIC, "weapons/explode5.wav", 1.0f, ATTN_NORM);
	SetThink(&CChopper::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1f;
}

//=========================================================
// monster_manta / grenade_mantabomb. The retail CMantaShip derives from
// CChopper, replaces its model and Think/Use callbacks, and emits up to three
// bombs after DropBomb. See findings/entities/manta_retail_batch_2026-09-11.md.
//=========================================================
class CMantaBomb : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT MantaBombTouch(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(grenade_mantabomb, CMantaBomb);

void CMantaBomb::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/chopperbomb.mdl");
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_NO;
	pev->gravity = 1.0f;
	pev->dmg = 240.0f;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetThink(&CGrenade::TumbleThink);
	SetTouch(&CMantaBomb::MantaBombTouch);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CMantaBomb::Precache()
{
	PrecacheModel("models/chopperbomb.mdl");
	PrecacheSound("manta/manta_bomb_falls1.wav");
	PrecacheSound("manta/manta_bomb_falls2.wav");
	PrecacheSound("manta/manta_bomb_falls3.wav");
	UTIL_PrecacheOther("dml_cluster");
}

void CMantaBomb::MantaBombTouch(CBaseEntity* pOther)
{
	Vector forward;
	UTIL_MakeVectors(pev->angles);
	forward = gpGlobals->v_forward;

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "manta/manta_bomb_falls1.wav", 1.0f, ATTN_NORM);
	UTIL_ScreenShake(pev->origin, 8.0f, 3.0f, 1.0f, 2048.0f);
	::RadiusDamage(pev->origin, pev, pev->owner ? VARS(pev->owner) : pev, pev->dmg, 1024.0f, CLASS_NONE, DMG_BLAST);

	// Retail CMantaBomb::MantaBombTouch creates exactly three dml_cluster
	// projectiles and assigns each 200 damage. The exact random-vector helper
	// is not typed by Ghidra; preserve the confirmed count, damage and forward
	// scatter envelope without inventing a second projectile class.
	for (int i = 0; i < 3; ++i)
	{
		Vector velocity = forward * RANDOM_FLOAT(200.0f, 400.0f) +
			Vector(RANDOM_FLOAT(-400.0f, 400.0f), RANDOM_FLOAT(-400.0f, 400.0f), RANDOM_FLOAT(-15.0f, 15.0f));
		Dml_ShootClusterGrenade(pev->owner ? VARS(pev->owner) : pev,
			pev->origin + Vector(RANDOM_FLOAT(-20.0f, 20.0f), RANDOM_FLOAT(-20.0f, 20.0f), RANDOM_FLOAT(5.0f, 20.0f)),
			velocity, 200.0f);
	}

	UTIL_Remove(this);
}

class CMantaShip : public CChopper
{
public:
	// Retail Classify (vtable slot 8 of monster_manta, @0x1006c460) returns 0 = CLASS_NONE,
	// unlike the apache (CLASS_ALIEN_MILITARY).
	int Classify() override { return CLASS_NONE; }
	void Spawn() override;
	void Precache() override;
	void EXPORT MantaThink();
	void EXPORT DropBomb(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_iBombsRemaining = 0;
	float m_flNextBomb = 0.0f;
};
LINK_ENTITY_TO_CLASS(monster_manta, CMantaShip);

TYPEDESCRIPTION CMantaShip::m_SaveData[] =
{
	DEFINE_FIELD(CMantaShip, m_iBombsRemaining, FIELD_INTEGER),
	DEFINE_FIELD(CMantaShip, m_flNextBomb, FIELD_TIME),
};
IMPLEMENT_SAVERESTORE(CMantaShip, CChopper);

void CMantaShip::Spawn()
{
	CChopper::Spawn();
	SET_MODEL(ENT(pev), "models/mantadock.mdl");
	SetThink(&CMantaShip::MantaThink);
	SetUse(&CMantaShip::DropBomb);
	// Retail Spawn @0x1006c470 (frisch dekompiliert 2026-10-31): takedamage 0,
	// Box (-96,-96,0)-(96,96,96) wie Chopper, nextthink bleibt auf dem
	// Chopper-Wert (+1.0); Anflug: Ziel per targetname suchen, Geschwindigkeit
	// 400 in Richtung Ziel, Blickrichtung (Gier) vom Ziel weg + 180.
	pev->takedamage = DAMAGE_NO;
	UTIL_SetSize(pev, Vector(-96, -96, 0), Vector(96, 96, 96));
	UTIL_SetOrigin(pev, pev->origin);
	m_iBombsRemaining = 0;
	m_flNextBomb = 0.0f;
	m_pGoalEnt = UTIL_FindEntityByTargetname(nullptr, STRING(pev->target));
	if (m_pGoalEnt)
	{
		Vector vecToGoal = m_pGoalEnt->pev->origin - pev->origin;
		pev->velocity = vecToGoal.Normalize() * 400;
		Vector vecAway = pev->origin - m_pGoalEnt->pev->origin;
		pev->angles.y = UTIL_VecToAngles(vecAway).y + 180;
	}
}

void CMantaShip::Precache()
{
	CChopper::Precache();
	PrecacheModel("models/mantadock.mdl");
	PrecacheSound("manta/manta_idle2.wav");
	UTIL_PrecacheOther("grenade_mantabomb");
}

void CMantaShip::DropBomb(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_iBombsRemaining = 3;
	m_flNextBomb = gpGlobals->time;
}

void CMantaShip::MantaThink()
{
	// Retain the proven CChopper flight lifecycle; the retail Manta has a
	// separate waypoint/banking solver, but shares the base class and its
	// damage/death behavior. The use-triggered carpet-bomb state is separate.
	CChopper::HuntThink();
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "manta/manta_idle2.wav", 1.0f, 0.3f, 0, 100);

	if (m_iBombsRemaining > 0 && gpGlobals->time >= m_flNextBomb)
	{
		CBaseEntity* pBomb = CBaseEntity::Create("grenade_mantabomb", pev->origin, pev->angles, edict());
		if (pBomb)
		{
			pBomb->pev->owner = edict();
			pBomb->pev->velocity = pev->velocity;
		}
		--m_iBombsRemaining;
		m_flNextBomb = gpGlobals->time + 0.25f;
	}
}
