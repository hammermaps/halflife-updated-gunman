//=========================================================
// monster_endboss - CEndBoss, the final boss (end1), plus its three
// companion classes endboss_rocket (CEndBossRocket), endboss_kataball
// (CEndBossKataball), and endboss_gib (CEndBossGib). The sixth and
// last of the six points deferred during the map-driven pass (see
// [[project-monster-endboss-deferred]] memory) - after this, every
// classname gap on every scanned map in maps-src/ is closed.
//
// Decompiled fresh from gunman.dll this session: LINK @0x100d4780,
// vtable @0x10108da8, Spawn @0x100d64d0, Precache @0x100d66a0,
// Classify (slot 8, @0x100d47d0) confirmed constant 0x14=20 - a
// previously-unseen class value, added as CLASS_ALIEN_BOSS in
// cbase.h. HandleAnimEvent (slot 58, @0x100d49e0) fully decompiled:
//   event 3 -> spawns endboss_rocket (vtable 0x1010871c) at an
//     attachment point, occasionally (50% chance) triggers a
//     screen-shake message sequence.
//   event 4 -> spawns endboss_kataball (vtable 0x101084e8) at an
//     attachment point, plays "endboss/kata_cannon1.wav", increments
//     an internal kataball counter.
//   events 6/7 -> a pair of long-range (4096 unit) claw-swipe attacks
//     via the shared multi-pellet traceline helper FUN_1000f090 (the
//     same helper already reproduced as FireBullets() calls for
//     CBanditGrunt::MechaVolley/CChopper elsewhere in this project) -
//     confirmed as two mirrored arm attacks (left/right), reproduced
//     here as a single ArmSwipe() used by both events.
//   event 10 -> a stomp attack: skill-scaled radius (130/150/200 for
//     skill 1/2/3), a ground-effect helper call (FUN_1005eb80) and a
//     "spawn a beam-arc effect" helper (FUN_1000e140, the same helper
//     seen in CDemomanRocket's AccelerateThink), plays
//     "endboss/end_stomp2.wav".
//   default -> falls through to the shared generic
//     footstep/miscellaneous animation-event handler (cosmetic only,
//     same pattern used by every other monster_human_*/boss class in
//     this project).
// Spawn confirms: model "models/endboss.mdl", bbox
// (-48,-48,0)/(48,48,300) (a very tall boss hull), SOLID_BBOX,
// health=200, view_ofs.z=300, an internal 4096-unit "attack range"
// field (matches the confirmed 4096.0 constant used by both
// ArmSwipe's FUN_1000f090 calls). Precache confirms 5
// UTIL_PrecacheOther calls: endboss_kataball, endboss_rocket,
// "antirocketflare" (per findings/entities/dll_reextraction_2026-09-01.md
// this is already fully decompiled elsewhere as a trivial, invisible
// prop with Think=SUB_Remove - not reimplemented here since no
// decompiled HandleAnimEvent branch in THIS class spawns it; only
// referenced by the AccelerateThink homing math this session
// simplifies away, so precached for completeness but never invoked),
// endboss_gib, and "sphere_explosion" (ALREADY implemented in
// gunman_custom_entities.cpp for CITY2B - not invoked here either,
// since its Use-trigger-based API doesn't fit a direct in-code
// death-explosion call; a plain TE_EXPLOSION tempentity is used
// instead, same pattern as CDummyBot::Explode()).
//
// endboss_rocket: LINK @0x100d3310, vtable @0x1010871c, Spawn
// @0x100d3430, Precache @0x100d33d0, IgniteThink @0x100d38d0,
// AccelerateThink @0x100d3a40, RocketTouch @0x100d3ed0. Confirmed
// model "models/rocket.mdl", MOVETYPE_FLY, SOLID_BBOX, a short
// randomized ignite delay (0.4-0.8s) before launch, and 4 confirmed
// explosion sounds ("endboss/boss_boom1-4.wav"). AccelerateThink's
// real homing-steering math (aim-toward-target velocity blending) is
// NOT reproduced - same simplification already established for
// human_demoman.cpp's CDemomanRocket and rebar_fauna.cpp's
// CClusterGod submunitions: straight-line flight at a fixed speed
// from the confirmed launch trajectory, exploding via CGrenade on
// touch.
//
// endboss_kataball: LINK @0x100d27f0, vtable @0x101084e8, Spawn
// @0x100d2930, Precache @0x100d28b0, IgniteThink @0x100d2d70,
// AccelerateThink @0x100d2db0. Confirmed sprite "sprites/kata.spr",
// kRenderGlow-style rendermode, and 3 confirmed impact sounds
// ("endboss/imp1-3_gs.wav"). Same AccelerateThink homing
// simplification as endboss_rocket.
//
// endboss_gib: LINK @0x100d4650, vtable @0x10108b7c, Spawn
// @0x100d46c0. Reuses the boss's OWN model ("models/endboss.mdl") as
// a non-solid bounce fragment. It starts the "death" sequence with
// bodygroup 1 set to 2, then CEndBossGib::FlopAnim @0x100d4600 advances
// it until completion and freezes the result; it is not a timed
// self-removing tossed prop.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "animation.h"
#include "weapons.h"
#include "soundent.h"
#include "effects.h"
#include "gamerules.h"
#include "skill.h"
#include "dbg_log.h"

#define ENDBOSS_AE_ROCKET 3
#define ENDBOSS_AE_KATABALL 4
#define ENDBOSS_AE_ARMSWIPE_L 6
#define ENDBOSS_AE_ARMSWIPE_R 7
#define ENDBOSS_AE_STOMP 10

//=========================================================
// endboss_rocket - CEndBossRocket.
//=========================================================
class CEndBossRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void SetTarget(CBaseEntity* pTarget) { m_hEnemy = pTarget; }
	void EXPORT IgniteThink();
	void EXPORT RocketTouch(CBaseEntity* pOther);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_iTrailSprite = 0;
	float m_flExpireTime = 0.0f;
};
LINK_ENTITY_TO_CLASS(endboss_rocket, CEndBossRocket);

TYPEDESCRIPTION CEndBossRocket::m_SaveData[] =
{
	DEFINE_FIELD(CEndBossRocket, m_flExpireTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CEndBossRocket, CGrenade);

void CEndBossRocket::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("endboss_rocket");
	SET_MODEL(ENT(pev), "models/rocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	UTIL_MakeVectors(pev->angles);
	// Retail starts at 500 and ignites after a random 0.4-0.8 seconds;
	// IgniteThink then raises the straight-flight approximation to 700.
	pev->velocity = gpGlobals->v_forward * 500.0f;

	// Nachtrag 2026-09-06 (Nutzer-Meldung "Raketen-Schweif fehlt auch bei
	// anderen Raketen"): siehe UTIL_RocketTrail (util.cpp) - gleiches
	// Muster wie CRpgRocket/CAnimeRocket/CDmlRocket.
	UTIL_RocketTrail(this, m_iTrailSprite);

	SetTouch(&CEndBossRocket::RocketTouch);
	m_flExpireTime = gpGlobals->time + 8.0f;
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.4f, 0.8f);
	SetThink(&CEndBossRocket::IgniteThink);
}

void CEndBossRocket::IgniteThink()
{
	// Retail CEndBossRocket::IgniteThink @ 0x100d38d0 normalizes the
	// launch vector and applies the confirmed 700-unit cruise speed.
	if (pev->velocity != g_vecZero)
		pev->velocity = pev->velocity.Normalize() * 700.0f;

	SetThink(&CEndBossRocket::SUB_Remove);
	pev->nextthink = m_flExpireTime;
}

void CEndBossRocket::Precache()
{
	PrecacheModel("models/rocket.mdl");
	PrecacheModel("sprites/firebeam.spr");
	// Sprite-Pruefung 2026-09-06: smoke.spr statt flame.spr, siehe dml.cpp
	// Nachtrag 2 fuer die Begruendung (flame.spr = Glow-Burst, kein Schweif).
	m_iTrailSprite = PrecacheModel("sprites/smoke.spr");
	PrecacheSound("endboss/boss_boom1.wav");
	PrecacheSound("endboss/boss_boom2.wav");
	PrecacheSound("endboss/boss_boom3.wav");
	PrecacheSound("endboss/boss_boom4.wav");
}

void CEndBossRocket::RocketTouch(CBaseEntity* pOther)
{
	const char* booms[] = {"endboss/boss_boom1.wav", "endboss/boss_boom2.wav", "endboss/boss_boom3.wav", "endboss/boss_boom4.wav"};
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, booms[RANDOM_LONG(0, 3)], 1.0, ATTN_NORM);
	CGrenade::ExplodeTouch(pOther);
}

//=========================================================
// endboss_kataball - CEndBossKataball.
//=========================================================
class CEndBossKataball : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void SetTarget(CBaseEntity* pTarget) { m_hEnemy = pTarget; }
	void EXPORT IgniteThink();
	void EXPORT KataTouch(CBaseEntity* pOther);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_flExpireTime = 0.0f;
};
LINK_ENTITY_TO_CLASS(endboss_kataball, CEndBossKataball);

TYPEDESCRIPTION CEndBossKataball::m_SaveData[] =
{
	DEFINE_FIELD(CEndBossKataball, m_flExpireTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CEndBossKataball, CGrenade);

void CEndBossKataball::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("endboss_kataball");
	SET_MODEL(ENT(pev), "sprites/kata.spr");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderGlow;
	pev->renderamt = 255;
	pev->framerate = 15.0;

	UTIL_MakeVectors(pev->angles);
	pev->velocity = gpGlobals->v_forward * 400.0f;

	// Retail Spawn @ 0x100d2930 arms the Kataball one 0.1-second think
	// later; before that it deliberately has no collision callback.
	SetTouch(NULL);
	m_flExpireTime = gpGlobals->time + 8.0f;
	pev->nextthink = gpGlobals->time + 0.1f;
	SetThink(&CEndBossKataball::IgniteThink);
}

void CEndBossKataball::IgniteThink()
{
	// Retail CEndBossKataball::IgniteThink @ 0x100d2d70 installs the
	// impact handler and switches to its steering Think. The latter remains
	// intentionally unreconstructed; retain the established straight flight.
	SetTouch(&CEndBossKataball::KataTouch);
	SetThink(&CEndBossKataball::SUB_Remove);
	pev->nextthink = m_flExpireTime;
}

void CEndBossKataball::Precache()
{
	PrecacheModel("sprites/kata.spr");
	PrecacheModel("sprites/firebeam.spr");
	PrecacheModel("sprites/flame.spr");
	PrecacheModel("sprites/kataimpact.spr");
	PrecacheSound("endboss/imp1_gs.wav");
	PrecacheSound("endboss/imp2_gs.wav");
	PrecacheSound("endboss/imp3_gs.wav");
	PrecacheEvent(1, "events/kataimpact.sc");
}

void CEndBossKataball::KataTouch(CBaseEntity* pOther)
{
	const char* impacts[] = {"endboss/imp1_gs.wav", "endboss/imp2_gs.wav", "endboss/imp3_gs.wav"};
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, impacts[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM);
	CGrenade::ExplodeTouch(pOther);
}

//=========================================================
// endboss_gib - CEndBossGib. A tossed death-gib reusing the boss's
// own model, see file header.
//=========================================================
class CEndBossGib : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT FlopAnim();
};
LINK_ENTITY_TO_CLASS(endboss_gib, CEndBossGib);

void CEndBossGib::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("endboss_gib");
	SET_MODEL(ENT(pev), "models/endboss.mdl");

	// Retail CEndBossGib::Spawn @ 0x100d46c0 is a short, animated
	// bounce fragment; its movement is supplied by the boss death event.
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->takedamage = DAMAGE_NO;
	SetSequenceByName("death");
	SetBodygroup(1, 2);
	SetThink(&CEndBossGib::FlopAnim);
	pev->nextthink = gpGlobals->time;
}

void CEndBossGib::Precache()
{
	PrecacheModel("models/endboss.mdl");
}

void CEndBossGib::FlopAnim()
{
	// Retail CEndBossGib::FlopAnim @ 0x100d4600 advances the death
	// sequence every 0.1 seconds, then freezes the completed fragment.
	StudioFrameAdvance();
	if (m_fSequenceFinished)
	{
		SetThink(NULL);
		pev->framerate = 0;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
}

//=========================================================
// monster_endboss - CEndBoss.
//=========================================================
class CEndBoss : public CBaseMonster
{
public:
	int MonsterProfile() override { return 22; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_BOSS; }
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this - see the
	// feedback-setactivity-act-idle-stomps-scripted-sequence memory
	// note (same fix as CFriendlyGunman etc).
	// Retail CEndBoss::SetActivity (vtable slot 96 @0x100d6430), disassembled
	// 2026-11-01 (findings/entities/endboss_slot96_2026-11-01.md):
	//  - ACT_RUN is looked up as ACT_WALK (the endboss model has no run
	//    sequence to use for it), Retail @0x100d6440.
	//  - pev->frame is reset when the sequence changes or the sequence does
	//    not loop; there is no walk/run exemption (the SDK base has one,
	//    but its oldActivity is set from NewActivity, so it is effectively
	//    inverted - see monsters.cpp CBaseMonster::SetActivity).
	//  - Missing sequence: ALERT and sequence 0, as in the base class.
	// The SDK fix for scripted_sequence (m_pCine) is kept as before; it is
	// not in the retail function.
	void SetActivity(Activity NewActivity) override
	{
		if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && m_pCine != nullptr)
		{
			m_Activity = NewActivity;
			m_IdealActivity = NewActivity;
			return;
		}
		m_Activity = NewActivity;
		m_IdealActivity = NewActivity;

		const int iSequence = LookupActivity(NewActivity == ACT_RUN ? ACT_WALK : NewActivity);
		if (iSequence > ACTIVITY_NOT_AVAILABLE)
		{
			if (pev->sequence != iSequence || !m_fSequenceLoops)
				pev->frame = 0;
			pev->sequence = iSequence;
			ResetSequenceInfo();
			SetYawSpeed();
		}
		else
		{
			ALERT(at_aiconsole, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity);
			pev->sequence = 0;
		}
	}
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	// Retail slots 80/81/82/83 (@0x100d5e80/@0x100d5ee0/@0x100d5f80/@0x100d5f30), 87 (@0x100d5510),
	// 88 (@0x100d53d0) and 101 (@0x100d4920).
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckRangeAttack2(float flDot, float flDist) override;
	bool CheckMeleeAttack1(float flDot, float flDist) override;
	bool CheckMeleeAttack2(float flDot, float flDist) override;
	Schedule_t* GetScheduleOfType(int Type) override;
	Schedule_t* GetSchedule() override;
	void SetYawSpeed() override;

private:
	float m_flNextKata = 0;	   // Retail +0x270
	float m_flNextRocket = 0;  // Retail +0x274
	float m_flNextSpecial = 0; // Retail +0x278
	bool m_bRocketsEnabled = true; // Retail +0x284 (set to 1 in Spawn; no writer found yet)
	void LaunchRocket();
	void LaunchKataball();
	void ArmSwipe();
	void Stomp();

	int m_iRocketCount = 0;
};
LINK_ENTITY_TO_CLASS(monster_endboss, CEndBoss);

void CEndBoss::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/endboss.mdl");
	UTIL_SetSize(pev, Vector(-48, -48, 0), Vector(48, 48, 300));

	pev->solid = SOLID_BBOX;
	// Blutfarbe 156 kommt aus der Profiltabelle (MonsterProfile() = 22); Retail setzt +0x238 im Spawn nicht.
	pev->health = 200; // confirmed fixed value, see file header - no skill scaling found
	pev->view_ofs = Vector(0, 0, 300);

	// Retail's capability word (+0x220) is not mappable; the retail attack checks (CheckAttacks, slot 98)
	// run range1/range2 under their capability bits and melee1/melee2 unconditionally, so all four are set.
	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK1 | bits_CAP_RANGE_ATTACK2 | bits_CAP_MELEE_ATTACK1 | bits_CAP_MELEE_ATTACK2;

	MonsterInit();

	// endboss.mdl has no $eyeposition. Retail Spawn @ 0x100d64d0 restores
	// its authored 300-unit eye height after the model-derived init step.
	pev->view_ofs = Vector(0, 0, 300);
	// Retail Spawn @0x100d64d0 setzt nach MonsterInit das Sichtfeld auf -1.0
	// (Rundumsicht, 0xbf800000). Das Retail-Basisfeld +0x244 entspricht
	// m_flDistTooFar (Gunner-Abgleich, findings/entities/gunner_friendly_2026-10-31.md).
	m_flFieldOfView = -1.0f;
	m_flNextKata = m_flNextRocket = m_flNextSpecial = gpGlobals->time;
	m_flDistTooFar = 4096.0f; // Retail Spawn setzt das gemeinsame Basisfeld +0x244.
}

// Retail custom schedule type 0x2e, table @0x1012f9b0 "Special Machinegun Attack".
#define SCHED_ENDBOSS_SPECIAL 0x2e

static Task_t tlEndBossSpecial[] =
{
	{TASK_STOP_MOVING, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_PLAY_SEQUENCE, (float)ACT_SPECIAL_ATTACK2},
	{TASK_FACE_ENEMY, 0},
};

static Schedule_t slEndBossSpecial[] =
{
	{tlEndBossSpecial, ARRAYSIZE(tlEndBossSpecial), 0, 0, "Special Machinegun Attack"},
};

bool CEndBoss::CheckRangeAttack1(float flDot, float flDist)
{
	// Rocket volley: pure cooldown, 2-5 s.
	if (gpGlobals->time < m_flNextRocket || !m_bRocketsEnabled)
		return false;
	m_flNextRocket = gpGlobals->time + RANDOM_FLOAT(2, 5);
	return true;
}

bool CEndBoss::CheckRangeAttack2(float flDot, float flDist)
{
	// Kataball: pure cooldown, 5-8 s.
	if (gpGlobals->time < m_flNextKata)
		return false;
	m_flNextKata = gpGlobals->time + RANDOM_FLOAT(5, 8);
	return true;
}

bool CEndBoss::CheckMeleeAttack1(float flDot, float flDist)
{
	return flDot > 0.7f && flDist < 256.0f;
}

bool CEndBoss::CheckMeleeAttack2(float flDot, float flDist)
{
	// Special machinegun attack: pure cooldown, 5-8 s.
	if (gpGlobals->time < m_flNextSpecial)
		return false;
	m_flNextSpecial = gpGlobals->time + RANDOM_FLOAT(5, 8);
	return true;
}

Schedule_t* CEndBoss::GetScheduleOfType(int Type)
{
	if (Type == SCHED_ENDBOSS_SPECIAL)
		return &slEndBossSpecial[0];
	return CBaseMonster::GetScheduleOfType(Type);
}

Schedule_t* CEndBoss::GetSchedule()
{
	// Retail @0x100d53d0. Outside combat, and for the dead-enemy handling, the stock logic applies.
	if (m_MonsterState != MONSTERSTATE_COMBAT || HasConditions(bits_COND_ENEMY_DEAD))
		return CBaseMonster::GetSchedule();

	if (HasConditions(bits_COND_NEW_ENEMY))
		return GetScheduleOfType(SCHED_WAKE_ANGRY);

	if (!HasConditions(bits_COND_SEE_ENEMY))
	{
		if (!HasConditions(bits_COND_ENEMY_OCCLUDED))
			return GetScheduleOfType(SCHED_COMBAT_FACE);
	}
	else
	{
		if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_AUTO, "endboss/end_stomp.wav", 1.0, ATTN_NONE, 0, 100);
			return GetScheduleOfType(SCHED_MELEE_ATTACK1);
		}
		if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
			return GetScheduleOfType(SCHED_RANGE_ATTACK1);
		if (HasConditions(bits_COND_CAN_RANGE_ATTACK2))
			return GetScheduleOfType(SCHED_RANGE_ATTACK2);
		if (HasConditions(bits_COND_CAN_MELEE_ATTACK2))
			return GetScheduleOfType(SCHED_ENDBOSS_SPECIAL);
	}
	return GetScheduleOfType(SCHED_CHASE_ENEMY);
}

void CEndBoss::SetYawSpeed()
{
	switch (m_Activity)
	{
	case ACT_IDLE:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		pev->yaw_speed = 40;
		break;
	case ACT_WALK:
	case ACT_RUN:
		pev->yaw_speed = 60;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
		pev->yaw_speed = 50;
		break;
	case ACT_MELEE_ATTACK2:
		pev->yaw_speed = 70;
		break;
	default:
		pev->yaw_speed = 45;
		break;
	}
}

void CEndBoss::Precache()
{
	PrecacheModel("models/endboss.mdl");
	PrecacheModel("sprites/fexplo.spr");
	PrecacheModel("models/metalplategibs_green.mdl");
	PrecacheSound("endboss/end_stomp.wav");
	PrecacheSound("endboss/end_stomp2.wav");
	for (int i = 1; i <= 8; i++)
	{
		char snd[32];
		sprintf(snd, "endboss/endboss_stomp%d.wav", i);
		PrecacheSound(snd);
	}
	PrecacheEvent(1, "events/tankmguns.sc");

	UTIL_PrecacheOther("endboss_kataball");
	UTIL_PrecacheOther("endboss_rocket");
	UTIL_PrecacheOther("endboss_gib");
	UTIL_PrecacheOther("sphere_explosion");
	UTIL_PrecacheOther("antirocketflare");
}

void CEndBoss::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case ENDBOSS_AE_ROCKET:
		LaunchRocket();
		break;
	case ENDBOSS_AE_KATABALL:
		LaunchKataball();
		break;
	case ENDBOSS_AE_ARMSWIPE_L:
	case ENDBOSS_AE_ARMSWIPE_R:
		ArmSwipe();
		break;
	case ENDBOSS_AE_STOMP:
		Stomp();
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CEndBoss::LaunchRocket()
{
	Vector vecOrigin, vecAngles;
	// Retail event 3 (helper 0x100d5bd0, mode 2): attachment 2, no z offset.
	GetAttachment(2, vecOrigin, vecAngles);

	CBaseEntity* pRocket = CBaseEntity::Create("endboss_rocket", vecOrigin, pev->angles);
	if (pRocket)
	{
		pRocket->pev->owner = edict();
		// Retail event 3 copies the boss's current enemy handle to the new
		// projectile before Spawn().  CBaseMonster persists that EHANDLE.
		static_cast<CEndBossRocket*>(pRocket)->SetTarget(m_hEnemy);
	}

	m_iRocketCount++;
}

void CEndBoss::LaunchKataball()
{
	Vector vecOrigin, vecAngles;
	// Retail event 4 (helper 0x100d5bd0, mode 3): attachment 3, no z offset.
	GetAttachment(3, vecOrigin, vecAngles);

	CBaseEntity* pBall = CBaseEntity::Create("endboss_kataball", vecOrigin, pev->angles);
	if (pBall)
	{
		pBall->pev->owner = edict();
		// Retail event 4 uses the same target propagation as the rocket.
		static_cast<CEndBossKataball*>(pBall)->SetTarget(m_hEnemy);
	}

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "endboss/kata_cannon1.wav", 1.0, ATTN_NORM);

	m_iRocketCount++;
}

//=========================================================
// ArmSwipe (Retail-Events 6 und 7, Helfer 0x100d5bd0 Modus 1, Pellet-Schuss 0x1000f090).
// Belegt (Dekompilat 2026-11-01, findings/entities/endboss_armswipe_2026-11-01.md):
//  - Ohne Gegner kein Schlag.
//  - Ursprung: Attachment 1 mit z +8 (Helfer Modus 1).
//  - Richtung: auf den Gegner (Retail-Ausrichtung, hier CBaseMonster::ShootAtEnemy).
//  - Drei Pellets, Reichweite 4096 (Aufrufargumente 3 und 4096.0).
//  - Streuung: je Achse Summe zweier Zufallswerte in -0,5..0,5, Verwerfen ausserhalb
//    des Einheitskreises (x*x + y*y > 1), wie in FUN_1000f090.
//  - Schaden je Treffer (DMG_BULLET): Cvar sk_9mmAR_bullet<Skill> (Retail-Name ohne Stufe, Helfer haengt die Stufe an; skill.cfg: 10).
// VEREINFACHUNGEN / OFFEN (nicht belegt, daher nicht nachgebaut):
//  - Streuungsfaktor: Retail multipliziert die Streuung mit einem Argument des Helfers (nicht belegt).
//    Hier VECTOR_CONE_4DEGREES (Apache-Geschuetz, apache.cpp), auf Wunsch des Nutzers als Naeherung.
//  - Retail: Der Schaden geht an das Trefferziel (Vtable-Slot 0x28 = TraceAttack, Schadenstyp 2 = DMG_BULLET).
//    Der Zaehler 0x1013228c (Modulo 3) steuert nur den Effekt und den Lebend-Check, nicht den Schaden.
//  - Effekt: TE_TRACER (6) von Start nach Trefferpunkt, MSG_PAS, jeder dritte Treffer (Retail-Zaehler).
//    Retail prueft vor dem Effekt pev->flags (Offset 0x1a4) & FL_CLIENT (0x8) des Angreifers; der Angreifer ist hier der Endboss selbst (kein Client), die Pruefung wuerde nie greifen.
//  - Retail-Zweig "Abstand unter Schwelle" (0x100e9384) nicht nachgebaut.
//  - Die fruehere 150-Einheiten-Pruefung entfaellt: Retail hat keine feste Distanzpruefung, die Reichweite ist 4096.
//=========================================================
// Zaehler wie Retail-Global 0x1013228c (nur Effekt-Gating), gemeinsam fuer alle Schlaege.
static int s_iArmSwipeCounter = 0;

void CEndBoss::ArmSwipe()
{
	if (!m_hEnemy)
		return;

	// Retail: GET_ATTACHMENT(1), Helfer Modus 1 (z +8).
	Vector vecOrigin, vecAngles;
	GetAttachment(1, vecOrigin, vecAngles);
	vecOrigin.z += 8;

	const Vector vecAim = ShootAtEnemy(vecOrigin);
	UTIL_MakeAimVectors(UTIL_VecToAngles(vecAim));
	const Vector vecRight = gpGlobals->v_right;
	const Vector vecUp = gpGlobals->v_up;

	// Streuung: Wert des Apache-Geschuetzes (VECTOR_CONE_4DEGREES, apache.cpp), wie vom Nutzer vorgegeben.
	// Der Retail-Faktor ist nicht belegt; diese Naeherung ersetzt ihn.
	const Vector vecSpread = VECTOR_CONE_4DEGREES;
	const float flDamage = GetSkillCvar("sk_9mmAR_bullet");

	ClearMultiDamage();
	for (int i = 0; i < 3; i++)
	{
		// Retail-Streuung (Summe zweier Zufallswerte je Achse, Kreisrejektion).
		float x, y;
		do
		{
			x = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
			y = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
		} while (x * x + y * y > 1.0f);

		const Vector vecDir = (vecAim + vecRight * (x * vecSpread.x) + vecUp * (y * vecSpread.y)).Normalize();
		const Vector vecEnd = vecOrigin + vecDir * 4096;

		TraceResult tr;
		UTIL_TraceLine(vecOrigin, vecEnd, dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction < 1.0f && tr.pHit)
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			DBG_LOG("ARMSWIPE pellet %d origin=(%.0f %.0f %.0f) end=(%.0f %.0f %.0f) frac=%.3f hit=%s dmg=%.1f",
				i, vecOrigin.x, vecOrigin.y, vecOrigin.z, tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z,
				tr.flFraction, pHit ? STRING(pHit->pev->classname) : "(none)", flDamage);
			// Retail (0x1000f3d8..f3f1): Effekt nur bei jedem dritten Treffer (globaler Zaehler).
			// Der virtuelle Aufruf bei vtable+0x9c ist CBaseEntity::IsPlayer (bei diesem
			// Endboss false), kein Lebend-Check. Beide Zweige senden TE_TRACER (6),
			// MSG_PAS (5), SVC_TEMPENTITY (23); der Zähler beeinflusst den Schaden nicht.
			// Retail-Pruefung [pev+0x1a4] & FL_CLIENT (0x8) entfaellt hier: Angreifer ist der Endboss selbst (kein Client).
			if (pHit && (s_iArmSwipeCounter++ % 3) == 0)
			{
				MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecOrigin);
				WRITE_BYTE(TE_TRACER);
				WRITE_COORD(vecOrigin.x);
				WRITE_COORD(vecOrigin.y);
				WRITE_COORD(vecOrigin.z);
				WRITE_COORD(tr.vecEndPos.x);
				WRITE_COORD(tr.vecEndPos.y);
				WRITE_COORD(tr.vecEndPos.z);
				MESSAGE_END();
			}
			if (pHit && pHit->pev->takedamage != DAMAGE_NO)
				pHit->TraceAttack(pev, flDamage, vecDir, &tr, DMG_BULLET); // Retail: Schadenstyp 2 (DMG_BULLET), siehe findings/entities/endboss_armswipe_2026-11-01.md
		}
		else
			DBG_LOG("ARMSWIPE pellet %d origin=(%.0f %.0f %.0f) no hit", i, vecOrigin.x, vecOrigin.y, vecOrigin.z);
	}
	ApplyMultiDamage(pev, pev);
}

void CEndBoss::Stomp()
{
	float radius = 130;
	if (g_iSkillLevel == SKILL_MEDIUM)
		radius = 150;
	else if (g_iSkillLevel == SKILL_HARD)
		radius = 200;

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "endboss/end_stomp2.wav", 1.0, ATTN_NORM);
	UTIL_ScreenShake(pev->origin, 12.0, 3.0, 1.0, radius * 2);
	RadiusDamage(pev->origin, pev, pev, 40, CLASS_NONE, DMG_CLUB);
}
