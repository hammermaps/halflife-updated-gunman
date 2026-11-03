//=========================================================
// MAYAN6's monster classes with no SDK precedent: monster_gator
// (CGator) and monster_microraptor (CMicroRaptor), plus MAYAN6's one
// env_clusterExplosion placement (CClusterExplosion + its dml_cluster/
// CClusterGrenade sub-munition). weapon_minigun, MAYAN6's fourth gap,
// is a player weapon and out of scope for this pass (see STATUS.md).
//
// Decompiled fresh from gunman.dll this session:
//   monster_gator: LINK @0x10071920, vtable @0x100f80d0, Spawn
//     @0x10071f10, Precache @0x10071ef0, Classify @0x10071d90
//     (returns 5 = CLASS_ALIEN_MILITARY). Matches
//     findings/entity_review_list.csv's "keine Verhaltenslogik
//     decompiliert" - no custom Schedule_t/Task_t table found here
//     either, this session's decompile only covers Spawn/Precache/
//     Classify (consistent with the doc, not a gap introduced by
//     this pass).
//   monster_microraptor: LINK @0x10073b50, vtable @0x100f8540, Spawn
//     @0x10074c70, Precache @0x10074c30, Classify @0x10073bb0
//     (returns 15 = CLASS_ALIEN_PREDATOR_RAPTOR, same constant
//     confirmed independently for monster_raptor's hostile variant
//     and monster_largescorpion). Confirms
//     findings/entity_review_list.csv's "Modell 'Raptor.mdl' gehoert
//     zu DIESER Klasse, NICHT zu 'monster_raptor'" naming-trap note -
//     this is the small, fast, 12 HP raptor from the user's original
//     screenshot, distinct from monster_raptor/monster_rheptor's
//     larger models/rheptor.mdl.
//
// Combat status: CMicroRaptor's melee checks, primary and jump-fly-land
// schedules, attack events, and jump touch have been reconstructed and
// ported. Retail GetSchedule (slot 88) dispatches to schedule 0x2a for
// CAN_MELEE_ATTACK2; the remaining Eating/Urgent schedules contain task
// IDs 97/101 that Retail CBaseMonster::StartTask does not handle. The
// actual leap attack still needs a focused runtime check. CGator's
// remaining combat/schedule behavior is outside this finding and must be
// checked separately.
//
// env_clusterExplosion (CClusterExplosion) - reviewed this session via
// the already-fully-decompiled ghidra/logs/cexp_ctor.txt (ctor helper
// FUN_10081a70 @0x10081a70, constructing "dml_cluster") and
// cexp_combat.txt (CClusterExplosion::ClusterExplode @0x100a2c20,
// vtable @0x100fefa4, findings/entities/env_clusterexplosion.md,
// Session 64) - confirmed against the actual decompile output, not
// just the findings prose. Confirmed: invisible models/null.mdl point
// entity, KeyValue-parsed clusterDamage(int,default 40)/
// numGrenades(int,default 4), spawnflag bit 2 = Repeatable (self-
// removes after firing when unset, matches
// `(*(byte*)(...+0x1a0) & 2) == 0 -> FUN_100606b0` = UTIL_Remove),
// Use=ClusterExplode spawning `numGrenades` "dml_cluster" entities in
// a loop via the same raw-vtable-construct pattern used elsewhere in
// this project (see CDmlRocket/CChemBall), each given a randomized
// scatter position/velocity and pev->dmg = clusterDamage directly
// (offset 0x1e0, matches this SDK's pev->dmg usage in
// CGrenade::Explode). The previously approximated dml_cluster lifecycle
// is now fully reconstructed from Spawn/Precache/TumbleClusterThink/
// ClusterDetonate/ClusterSmokeThink; see
// findings/entities/cluster_grenade_retail_batch_2026-09-09.md.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "skill.h"
#include "saverestore.h"

//=========================================================
// monster_gator - CGator.
//=========================================================
class CGator : public CBaseMonster
{
public:
	int MonsterProfile() override { return 6; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int Classify() override { return CLASS_ALIEN_MILITARY; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }

private:
	void AttackSound();
};
LINK_ENTITY_TO_CLASS(monster_gator, CGator);

void CGator::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/gator.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 25));

	pev->solid = SOLID_SLIDEBOX;
	// Retail @0x10071f10: Health per FUN_100608d0 (Skill-Basis + Zufallsanteil, wie Ourano), FOV 0.7.
	pev->health = gSkillData.gatorHealth + RANDOM_FLOAT(0, gSkillData.gatorHealth) * (gSkillData.percentRandomHealth * 0.01f);
	m_flFieldOfView = 0.7f;
	pev->view_ofs = Vector(0, 0, 20);

	MonsterInit();
	// Retail CGator::Spawn @ 0x10071f10 restores its authored eye height
	// after the model-derived initialization.
	pev->view_ofs = Vector(0, 0, 20);
}

void CGator::Precache()
{
	PrecacheModel("models/gator.mdl");
	PrecacheSound("gator/Gator_BiteAttack_1.wav");
	PrecacheSound("gator/Gator_BiteAttack.wav");
	PrecacheSound("gator/Gator_BiteAttack_2_f0.wav");
	PrecacheSound("gator/Gator_SeePlayer_f0.wav");
	PrecacheSound("gator/Gator_IdleNormal_F0.wav");
	PrecacheSound("gator/Gator_IdleRoarLow_F0.wav");
	PrecacheSound("gator/Gator_Sflinch_f0.wav");
	PrecacheSound("gator/Gator_Footstep1.wav");
	PrecacheSound("gator/Gator_Footstep2.wav");
	PrecacheSound("gator/Gator_DieSimple.wav");
	PrecacheSound("gator/die1.wav");
}

void CGator::AttackSound()
{
	const char* const sound = RANDOM_LONG(0, 1) == 0
		? "gator/Gator_BiteAttack_1.wav"
		: "gator/Gator_BiteAttack.wav";
	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, sound, 0.7f, ATTN_NORM, 0, 100);
}

void CGator::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 3:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "gator/Gator_Footstep1.wav", 1.0f, ATTN_NORM, 0, 100);
		break;

	case 4:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "gator/Gator_Footstep2.wav", 1.0f, ATTN_NORM, 0, 100);
		break;

	case 7:
		// Retail FUN_10071e10 intentionally falls through: its bite sound
		// and the 72-unit slash are both emitted for animation event 7.
		AttackSound();
		[[fallthrough]];

	case 8:
		CheckTraceHullAttack(72.0f, gSkillData.gatorBiteDamage, DMG_SLASH);
		break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// monster_microraptor - CMicroRaptor.
//=========================================================
class CMicroRaptor : public CBaseMonster
{
public:
	int MonsterProfile() override { return 9; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_PREDATOR_RAPTOR; }
	bool CheckMeleeAttack2(float flDot, float flDist) override;
	Schedule_t* GetSchedule() override;
	Schedule_t* GetScheduleOfType(int Type) override;
	void StartTask(Task_t* pTask) override;
	void RunTask(Task_t* pTask) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void EXPORT JumpTouch(CBaseEntity* pOther);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }

private:
	// Retail Save/Restore fields (+0x2a8/+0x2ac/+0x2b0).
	int m_iRelaxed = 0;
	float m_tNextSoundTime = 0;
	float m_tNextMeleeTime = 0;
	// Retail +0x2c4: second melee/leap cooldown. Its Retail save descriptor
	// does not include this field, so it intentionally is not saved here.
	float m_flNextLeapTime = 0;
};
LINK_ENTITY_TO_CLASS(monster_microraptor, CMicroRaptor);

TYPEDESCRIPTION CMicroRaptor::m_SaveData[] = {
	DEFINE_FIELD(CMicroRaptor, m_iRelaxed, FIELD_INTEGER),
	DEFINE_FIELD(CMicroRaptor, m_tNextSoundTime, FIELD_TIME),
	DEFINE_FIELD(CMicroRaptor, m_tNextMeleeTime, FIELD_TIME),
};
IMPLEMENT_SAVERESTORE(CMicroRaptor, CBaseMonster);

// Retail schedule type 0x16: "Raptor Primary Melee Attack".
static Task_t tlMicroRaptorMeleeAttack1[] = {
	{TASK_STOP_MOVING, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_MELEE_ATTACK1, 0},
};

static Schedule_t slMicroRaptorMeleeAttack1 = {
	tlMicroRaptorMeleeAttack1,
	ARRAYSIZE(tlMicroRaptorMeleeAttack1),
	bits_COND_ENEMY_OCCLUDED | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD,
	0,
	"Raptor Primary Melee Attack",
};

// Retail schedule type 0x2a: "Jump-fly-land(Jump_Air)". The task IDs
// 90..92 are Gunman CBaseMonster extensions (ACT_HOP/FLY/LAND).
static constexpr int TASK_MICRORAPTOR_HOP = 90;
static constexpr int TASK_MICRORAPTOR_FLY = 91;
static constexpr int TASK_MICRORAPTOR_LAND = 92;

static Task_t tlMicroRaptorJumpFlyLand[] = {
	{TASK_STOP_MOVING, 0},
	{TASK_MICRORAPTOR_HOP, 0},
	{TASK_MICRORAPTOR_FLY, 0},
	{TASK_STOP_MOVING, 0},
	{TASK_MICRORAPTOR_LAND, 0},
	{TASK_FACE_ENEMY, 0},
};

static Schedule_t slMicroRaptorJumpFlyLand = {
	tlMicroRaptorJumpFlyLand,
	ARRAYSIZE(tlMicroRaptorJumpFlyLand),
	bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE,
	0,
	"Jump-fly-land(Jump_Air)",
};

void CMicroRaptor::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/raptor.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 32));

	pev->solid = SOLID_SLIDEBOX;
	pev->health = 12;
	pev->view_ofs = Vector(0, 0, 24);
	pev->flags |= FL_MONSTER;
	m_afCapability |= bits_CAP_MELEE_ATTACK2;

	MonsterInit();
	// Retail CMicroRaptor::Spawn @ 0x10074c70 writes this after MonsterInit().
	pev->view_ofs = Vector(0, 0, 24);
}

void CMicroRaptor::Precache()
{
	PrecacheModel("models/raptor.mdl");
	PrecacheModel("sprites/gorehuman.spr");
	PrecacheModel("sprites/gibhuman.spr");
	PrecacheSound("raptor/Raptor_IdleNormal_F0.wav");
	PrecacheSound("raptor/Raptor_IdleChitter_F0.wav");
	PrecacheSound("raptor/Raptor_BiteChunk_F0.wav");
	PrecacheSound("raptor/Raptor_ChewChunk_F0.wav");
	PrecacheSound("raptor/Raptor_RoarAngry_F0.wav");
	PrecacheSound("raptor/Raptor_FootStep1.wav");
	PrecacheSound("raptor/Raptor_Footstep2.wav");
	PrecacheSound("raptor/Raptor_LeapStartMiddleEnd_F0.wav");
	PrecacheSound("raptor/Raptor_SmallFlinch_F0.wav");
	PrecacheSound("raptor/Raptor_DieSimple.wav");
	PrecacheSound("raptor/Raptor_DieFight.wav");
}

// Retail slot 83 (@0x10073ed0): second melee attack is gated by its cooldown,
// a 64..256 unit range, a forward dot above 0.9 and a valid enemy.
bool CMicroRaptor::CheckMeleeAttack2(float flDot, float flDist)
{
	return m_flNextLeapTime <= gpGlobals->time && flDist > 64.0f && flDist <= 256.0f && flDot > 0.9f && m_hEnemy != nullptr;
}

// Retail slot 88 selects schedule type 0x2a for the second melee condition.
Schedule_t* CMicroRaptor::GetSchedule()
{
	if (m_MonsterState == MONSTERSTATE_COMBAT && HasConditions(bits_COND_CAN_MELEE_ATTACK2))
		return GetScheduleOfType(0x2a);

	return CBaseMonster::GetSchedule();
}

// Retail slot 87 maps schedule type 0x16 to this three-task table.
Schedule_t* CMicroRaptor::GetScheduleOfType(int Type)
{
	if (Type == SCHED_MELEE_ATTACK1)
		return &slMicroRaptorMeleeAttack1;
	if (Type == 0x2a)
		return &slMicroRaptorJumpFlyLand;

	return CBaseMonster::GetScheduleOfType(Type);
}

void CMicroRaptor::StartTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_MICRORAPTOR_HOP:
		m_IdealActivity = ACT_HOP;
		break;
	case TASK_MICRORAPTOR_FLY:
		m_IdealActivity = ACT_FLY;
		break;
	case TASK_MICRORAPTOR_LAND:
		m_IdealActivity = ACT_LAND;
		break;
	default:
		CBaseMonster::StartTask(pTask);
		break;
	}
}

void CMicroRaptor::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_MICRORAPTOR_HOP:
	case TASK_MICRORAPTOR_LAND:
		if (m_fSequenceFinished)
			TaskComplete();
		break;
	case TASK_MICRORAPTOR_FLY:
		if (FBitSet(pev->flags, FL_ONGROUND))
			TaskComplete();
		break;
	default:
		CBaseMonster::RunTask(pTask);
		break;
	}
}

void CMicroRaptor::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
	case 2:
		// Retail FUN_10074180: both melee events use the shared 70-unit
		// attack helper with a fixed 5 damage.
		CheckTraceHullAttack(70.0f, 5, DMG_SLASH);
		break;

	case 3:
	{
		// Retail launches toward the current enemy at 600 units/s and
		// enables the existing JumpTouch callback only for a valid arc.
		// Retail event 3 also starts the 5..7 second second-attack cooldown.
		m_flNextLeapTime = gpGlobals->time + RANDOM_LONG(5, 7);
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "raptor/Raptor_LeapStartMiddleEnd_F0.wav", 1.0f, ATTN_NORM, 0, 100);
		if (m_hEnemy)
		{
			const Vector vecVelocity = VecCheckThrow(pev, pev->origin, m_hEnemy->Center(), 600.0f, 1.0f);
			if (vecVelocity != g_vecZero)
			{
				pev->velocity = vecVelocity;
				pev->flags &= ~FL_ONGROUND;
				SetTouch(&CMicroRaptor::JumpTouch);
			}
		}
		break;
	}

	case 4:
		// Retail explicitly consumes LeapEnd without delegating to the base.
		break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CMicroRaptor::JumpTouch(CBaseEntity* pOther)
{
	if (!FBitSet(pev->flags, FL_ONGROUND) && pOther && pOther->pev->takedamage != DAMAGE_NO && pOther->Classify() != Classify())
	{
		pev->framerate = 4.0f;
		pOther->TakeDamage(pev, pev, 3.0f, DMG_SLASH);
	}

	SetTouch(NULL);
}

//=========================================================
// dml_cluster - CClusterGrenade. Shared cluster sub-munition (also
// used by weapon_dml and monster_manta). It is not a stock timed grenade:
// retail has its own randomized fuse, smoke trail, detonation and expanding
// smoke-ring lifecycle. See findings/entities/cluster_grenade_retail_batch_2026-09-09.md.
//=========================================================
class CClusterGrenade : public CGrenade
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void Spawn() override;
	void Precache() override;
	void EXPORT TumbleClusterThink();
	void EXPORT ClusterDetonate();
	void EXPORT ClusterSmokeThink();

	static CClusterGrenade* Shoot(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float flDamage, bool bOverrideDamage = true);

private:
	int m_iSmokeSprite = 0;
	int m_iShockSprite = 0;
	unsigned short m_usClusterExplosion = 0;
	EHANDLE m_hSmokeRing;
	bool m_bTrailStarted = false;
};
LINK_ENTITY_TO_CLASS(dml_cluster, CClusterGrenade);

TYPEDESCRIPTION CClusterGrenade::m_SaveData[] =
{
	DEFINE_FIELD(CClusterGrenade, m_iSmokeSprite, FIELD_INTEGER),
	DEFINE_FIELD(CClusterGrenade, m_iShockSprite, FIELD_INTEGER),
	DEFINE_FIELD(CClusterGrenade, m_usClusterExplosion, FIELD_SHORT),
	DEFINE_FIELD(CClusterGrenade, m_hSmokeRing, FIELD_EHANDLE),
	DEFINE_FIELD(CClusterGrenade, m_bTrailStarted, FIELD_BOOLEAN),
};
IMPLEMENT_SAVERESTORE(CClusterGrenade, CGrenade);

void CClusterGrenade::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_NO;
	pev->gravity = 0.5f;
	pev->friction = 0.6f;
	pev->classname = MAKE_STRING("dml_cluster");
	// Retail CClusterGrenade::Spawn sets pev->dmg from sk_dml_rocket_d.
	// Callers may override it after Spawn; entity_clustergod keeps this value.
	pev->dmg = gSkillData.dmlRocketDamage * 0.47619048f;
	SET_MODEL(ENT(pev), "models/dmlcluster.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetTouch(&CGrenade::BounceTouch);
	SetThink(&CClusterGrenade::TumbleClusterThink);
	pev->dmgtime = gpGlobals->time + RANDOM_FLOAT(0.8f, 4.0f);
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->framerate = 1.0f;
	m_bTrailStarted = false;
	m_hSmokeRing = nullptr;
}

void CClusterGrenade::Precache()
{
	PrecacheModel("models/dmlcluster.mdl");
	m_iSmokeSprite = PrecacheModel("sprites/smoke.spr");
	PrecacheModel("sprites/kaboom.spr");
	m_usClusterExplosion = PrecacheEvent(1, "events/ClusterExplosion.sc");
	m_iShockSprite = PrecacheModel("sprites/clustershock.spr");
	PrecacheModel("sprites/smokering.spr");
	PrecacheSound("manta/carpetbomb1.wav");
	PrecacheSound("manta/carpetbomb2.wav");
	PrecacheSound("manta/carpetbomb3.wav");
}

CClusterGrenade* CClusterGrenade::Shoot(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float flDamage, bool bOverrideDamage)
{
	CClusterGrenade* pGrenade = GetClassPtr((CClusterGrenade*)nullptr);
	pGrenade->Spawn();

	UTIL_SetOrigin(pGrenade->pev, vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = ENT(pevOwner);
	if (bOverrideDamage)
		pGrenade->pev->dmg = flDamage;

	pGrenade->pev->sequence = RANDOM_LONG(3, 6);
	pGrenade->pev->animtime = gpGlobals->time;

	return pGrenade;
}

void CClusterGrenade::TumbleClusterThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	if (!m_bTrailStarted)
	{
		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(TE_BEAMFOLLOW);
		WRITE_SHORT(entindex());
		WRITE_SHORT(m_iSmokeSprite);
		WRITE_BYTE(20);
		WRITE_BYTE(5);
		WRITE_BYTE(224);
		WRITE_BYTE(224);
		WRITE_BYTE(255);
		WRITE_BYTE(255);
		MESSAGE_END();
		m_bTrailStarted = true;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
	if (pev->dmgtime - 1.0f < gpGlobals->time)
		UTIL_DynamicLight(pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400.0f, 255, 255, 255, 0.1f, 0.0f);
	if (pev->dmgtime <= gpGlobals->time)
		SetThink(&CClusterGrenade::ClusterDetonate);

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.8f;
		pev->framerate = 0.2f;
	}
}

void CClusterGrenade::ClusterDetonate()
{
	TraceResult tr;
	Vector vecStart = pev->origin + Vector(0, 0, 8);
	Vector vecEnd = pev->origin - Vector(0, 0, 64);
	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, edict(), &tr);
	pev->origin = tr.vecEndPos;

	PLAYBACK_EVENT_FULL(FEV_GLOBAL, edict(), m_usClusterExplosion, 0.0f, pev->origin, g_vecZero, 0.0f, 0.0f, 0, 0, 0, 0);
	UTIL_ExplosionEffects(pev->origin, 400.0f);
	::RadiusDamage(pev->origin, pev, pev->owner ? VARS(pev->owner) : pev, pev->dmg, pev->dmg * 2.5f, CLASS_NONE, DMG_BLAST);

	pev->effects |= EF_NODRAW;
	pev->velocity = g_vecZero;
	SetThink(&CClusterGrenade::ClusterSmokeThink);
	pev->nextthink = gpGlobals->time;
}

void CClusterGrenade::ClusterSmokeThink()
{
	CSprite* pSmoke = static_cast<CSprite*>((CBaseEntity*)m_hSmokeRing);
	if (!pSmoke)
	{
		pSmoke = CSprite::SpriteCreate("sprites/smokering.spr", pev->origin, false);
		if (!pSmoke)
		{
			UTIL_Remove(this);
			return;
		}
		m_hSmokeRing = pSmoke;
		pSmoke->SetScale(0.2f);
		pSmoke->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation);
	}
	else
	{
		pSmoke->SetScale(pSmoke->pev->scale + 0.16f);
	}

	pev->nextthink = gpGlobals->time + 0.1f;
	if (pSmoke->pev->scale > 1.25f)
	{
		UTIL_Remove(pSmoke);
		m_hSmokeRing = nullptr;
		UTIL_Remove(this);
	}
}

// Thin wrapper so other translation units (dml.cpp/dmlgrenade.cpp - the
// confirmed real dml_cluster/CClusterGrenade users besides
// env_clusterExplosion, see findings/weapons/weapon_dml.md) can spawn a
// dml_cluster sub-munition without redeclaring the CClusterGrenade class
// itself (which would violate ODR since it's only partially declared
// elsewhere).
CBaseEntity* Dml_ShootClusterGrenade(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float flDamage)
{
	return CClusterGrenade::Shoot(pevOwner, vecStart, vecVelocity, flDamage);
}

CBaseEntity* Dml_ShootClusterGrenadeDefaultDamage(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity)
{
	return CClusterGrenade::Shoot(pevOwner, vecStart, vecVelocity, 0.0f, false);
}

//=========================================================
// env_clusterExplosion - CClusterExplosion. Level-scripting trigger,
// see file header for the full decompile-verified behavior.
//=========================================================
class CClusterExplosion : public CBaseEntity
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void EXPORT ClusterExplode(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

private:
	int m_iClusterDamage = 40;
	int m_iNumGrenades = 4;
};
LINK_ENTITY_TO_CLASS(env_clusterExplosion, CClusterExplosion);

TYPEDESCRIPTION CClusterExplosion::m_SaveData[] =
{
	DEFINE_FIELD(CClusterExplosion, m_iClusterDamage, FIELD_INTEGER),
	DEFINE_FIELD(CClusterExplosion, m_iNumGrenades, FIELD_INTEGER),
};
IMPLEMENT_SAVERESTORE(CClusterExplosion, CBaseEntity);

bool CClusterExplosion::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "clusterDamage"))
	{
		m_iClusterDamage = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "numGrenades"))
	{
		m_iNumGrenades = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CClusterExplosion::Spawn()
{
	Precache();

	// Retail Spawn @0x100a2b20 (frisch dekompiliert 2026-10-31): movetype FLY,
	// effects 0 (unsichtbar nur ueber models/null.mdl), takedamage 0,
	// health 60, Nullgeschwindigkeit, Nullbox.
	SET_MODEL(ENT(pev), "models/null.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = 60;
	pev->velocity = g_vecZero;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetTouch(nullptr);
	SetThink(nullptr);
	SetUse(&CClusterExplosion::ClusterExplode);
	UTIL_SetOrigin(pev, pev->origin);
}

void CClusterExplosion::Precache()
{
	PrecacheModel("models/null.mdl");
	UTIL_PrecacheOther("dml_cluster");
}

void CClusterExplosion::ClusterExplode(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	for (int i = 0; i < m_iNumGrenades; i++)
	{
		Vector vecScatter = pev->origin + Vector(RANDOM_FLOAT(-15, 15), RANDOM_FLOAT(-15, 15), RANDOM_FLOAT(-15, 15));
		Vector vecVelocity = Vector(RANDOM_FLOAT(-250, 250), RANDOM_FLOAT(-250, 250), RANDOM_FLOAT(50, 250));

		CClusterGrenade::Shoot(pev, vecScatter, vecVelocity, (float)m_iClusterDamage);
	}

	// SF_CLUSTEREXPLOSION_REPEATABLE (bit 2): if unset, the trigger is
	// single-use and removes itself after firing - confirmed via
	// decompile, see file header.
	if ((pev->spawnflags & 2) == 0)
		UTIL_Remove(this);
}
