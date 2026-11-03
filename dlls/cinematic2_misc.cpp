//=========================================================
// cinematic2's targ_speaker (CTargSpeaker) - the third of Gunman's
// three sound-entity variants (alongside stock speaker/CSpeaker and
// this project's own player_speaker/random_speaker in
// gunman_custom_entities.cpp). FGD title "targeted Time Wav Player".
//
// Decompiled fresh from gunman.dll: LINK @0x10066910, ctor helper
// @0x100669c0 (vtable @0x100f6508), Spawn @0x10066a70,
// CTargSpeaker::Use @0x100669e0, KeyValue @0x10066a00. Single
// keyvalue "tsnoise" (the sound file to play). Spawn removes itself
// with an ALERT if no sound was assigned (confirmed error string
// "NO_SOUND_ASSIGNED_TO_TARG_SPEAKER" in the binary). Use() is a
// simple one-shot EMIT_SOUND, no toggle/loop state unlike stock
// CSpeaker.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "flightsteer.h"
#include "gunman_sphere_explosion.h"
#include "gamerules.h"
#include "monsters.h"
#include "effects.h"
#include "weapons.h"
#include "explode.h"
#include "soundent.h"
#include "talkmonster.h"
#include <cmath>

class CTargSpeaker : public CBaseEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_iszNoise = 0;
};
LINK_ENTITY_TO_CLASS(targ_speaker, CTargSpeaker);

TYPEDESCRIPTION CTargSpeaker::m_SaveData[] =
	{
		DEFINE_FIELD(CTargSpeaker, m_iszNoise, FIELD_STRING),
	};

IMPLEMENT_SAVERESTORE(CTargSpeaker, CBaseEntity);

bool CTargSpeaker::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "tsnoise"))
	{
		m_iszNoise = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CTargSpeaker::Spawn()
{
	if (FStringNull(m_iszNoise))
	{
		ALERT(at_error, "NO SOUND ASSIGNED TO TARG_SPEAKER at %f %f %f\n", pev->origin.x, pev->origin.y, pev->origin.z);
		UTIL_Remove(this);
		return;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
}

void CTargSpeaker::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, STRING(m_iszNoise), 1.0, ATTN_NORM);
}

//=========================================================
// monster_rustbot - CRustbot. A hostile combat robot with a melee arm
// swing and a continuous, tracking damage-over-time eye beam
// ("weapon_egon-like", per the user description that
// findings/entities/rustbot_combat.md confirmed against the
// decompile).
//
// Decompiled fresh from gunman.dll this session: LINK @0x100b7e30,
// vtable @0x10101d9c, Spawn @0x100b91c0, Precache @0x100b8010,
// Classify @0x100b7e90 (constant 7 = CLASS_ALIEN_MONSTER),
// HandleAnimEvent @0x100b8e50 (vtable slot 58, confirms
// findings/entities/rustbot_combat.md's event table exactly - this
// session independently re-derived event 1's melee traceline
// structure from the decompile rather than trusting the doc alone).
// Confirmed: model "models/rustbot.mdl", bbox (-32,-32,0)/(32,32,64),
// MOVETYPE_STEP, SOLID_SLIDEBOX, health from sk_rustbot_h with the
// shared percent-random-health helper, view_ofs=(0,0,40).
// The "borgRustbot" debug spawnflag/log (pev->effects & 0x40) is a
// cut-content developer flag with no findings-documented gameplay
// effect - not reproduced.
//
// Simplified relative to the original (documented per findings'
// still-open points and this session's own scope decision): the
// real eye beam (events 2/3) is a hand-built two-sprite +
// dynamic-light construction, live-repositioned and damage-ticked
// every frame via a dedicated per-tick function
// (FUN_100b8400, not fully decompiled this session). Reproduced here
// instead with the SDK's existing CBeam class (same approach
// egon.cpp already uses for a very similar continuous tracking
// laser) - a single beam entity, retargeted and damage-ticked each
// think, instead of the original's 2-sprite+dlight rig. Event 1's
// melee still uses the confirmed 128-unit traceline range and deals
// approximated (not byte-verified) damage. Spark-particle bursts on
// both events are approximated with a generic TE_SPARKS effect
// rather than the original's dedicated debris-spawner helper
// (FUN_100b9c40, shared with CGunner, not decompiled this session).
//=========================================================

#define RUSTBOT_AE_MELEE 1
#define RUSTBOT_AE_BEAM_OFF 2
#define RUSTBOT_AE_BEAM_ON 3

// Retail Rustbot range attack (schedule type 0x18, record 0x1012c610).
// Private tasks 0x64 (arm/power up) and 0x63 (plasma shot) are handled below.
#define TASK_RUSTBOT_PLASMA 0x63
#define TASK_RUSTBOT_POWERUP 0x64
#define TASK_RUSTBOT_SENTENCE 0x65

static Task_t tlRustbotRangeAttack[] =
{
	{TASK_FACE_ENEMY, 0},
	{TASK_SET_ACTIVITY, ACT_RANGE_ATTACK1},
	{TASK_RUSTBOT_POWERUP, 0},
	{TASK_RUSTBOT_PLASMA, 0},
	{TASK_SET_ACTIVITY, ACT_DISARM},
};
static Schedule_t slRustbotRangeAttack =
{
	tlRustbotRangeAttack, ARRAYSIZE(tlRustbotRangeAttack), 0, 0, "Rustbot Range Attack",
};

// Retail TakeCover record @0x1012c598. Task 0x65's sentence helper sets
// CTalkMonster's shared talk deadline before checking it, so its own gate
// rejects playback; preserve that observed side effect and complete the task.
static Task_t tlRustbotTakeCover[] =
{
	{TASK_STOP_MOVING, 0},
	{TASK_SET_FAIL_SCHEDULE, 50},
	{TASK_WAIT, 0.2f},
	{TASK_FIND_COVER_FROM_ENEMY, 0},
	{TASK_RUSTBOT_SENTENCE, 0},
	{TASK_RUN_PATH, 0},
	{TASK_WAIT_FOR_MOVEMENT, 0},
	{TASK_REMEMBER, 0},
	{TASK_SET_SCHEDULE, 49},
};
static Schedule_t slRustbotTakeCover =
{
	tlRustbotTakeCover, ARRAYSIZE(tlRustbotTakeCover), 0, 0, "TakeCover",
};

// Other Rustbot schedule records whose task streams use only SDK tasks.
// Cover and line-of-fire paths with unresolved private tasks remain deferred.
static Task_t tlRustbotCombatFace[] =
{
	{TASK_WAIT_FOR_MOVEMENT, 0}, {TASK_SET_ACTIVITY, ACT_IDLE}, {TASK_FACE_ENEMY, 0},
	{TASK_WAIT, 1.5f}, {TASK_SET_SCHEDULE, 48},
};
static Schedule_t slRustbotCombatFace =
{
	tlRustbotCombatFace, ARRAYSIZE(tlRustbotCombatFace), 0x110c00, 0, "Rustbot Combat Face",
};

static Task_t tlRustbotSweep[] =
{
	{TASK_TURN_LEFT, 179}, {TASK_WAIT, 1}, {TASK_TURN_LEFT, 179}, {TASK_WAIT, 1},
};
static Schedule_t slRustbotSweep =
{
	tlRustbotSweep, ARRAYSIZE(tlRustbotSweep), 0x30f10, 0x26, "Rustbot Sweep",
};

static Task_t tlRustbotWaitInCover[] =
{
	{TASK_WAIT_FOR_MOVEMENT, 0}, {TASK_SET_ACTIVITY, ACT_IDLE}, {TASK_WAIT_FACE_ENEMY, 0.5f},
};
static Schedule_t slRustbotWaitInCover =
{
	tlRustbotWaitInCover, ARRAYSIZE(tlRustbotWaitInCover), 0x30c00, 0x20, "Rustbot Wait In Cover",
};

// Retail records @0x1012c488/@0x1012c4c8. Both seek an enemy line of fire;
// their distinct failure schedules are 0x33 and SCHED_COMBAT_FACE.
static Task_t tlRustbotEstablishLineOfFire[] =
{
	{TASK_SET_FAIL_SCHEDULE, 0x33},
	{TASK_GET_PATH_TO_ENEMY, 0},
	{TASK_RUSTBOT_SENTENCE, 0},
	{TASK_RUN_PATH, 0},
	{TASK_WAIT_FOR_MOVEMENT, 0},
};
static Schedule_t slRustbotEstablishLineOfFire =
{
	tlRustbotEstablishLineOfFire, ARRAYSIZE(tlRustbotEstablishLineOfFire), 0x130c00, 0x20,
	"Rustbot Establish Line Of Fire",
};

static Task_t tlRustbotEstablishLineOfFireRetry[] =
{
	{TASK_SET_FAIL_SCHEDULE, SCHED_COMBAT_FACE},
	{TASK_GET_PATH_TO_ENEMY, 0},
	{TASK_RUSTBOT_SENTENCE, 0},
	{TASK_RUN_PATH, 0},
	{TASK_WAIT_FOR_MOVEMENT, 0},
};
static Schedule_t slRustbotEstablishLineOfFireRetry =
{
	tlRustbotEstablishLineOfFireRetry, ARRAYSIZE(tlRustbotEstablishLineOfFireRetry), 0x130c00, 0x20,
	"Rustbot Establish Line Of Fire",
};

static Task_t tlRustbotBorg[] =
{
	{TASK_WAIT_FOR_MOVEMENT, 0}, {TASK_SET_ACTIVITY, ACT_IDLE}, {TASK_SET_SCHEDULE, 52},
};
static Schedule_t slRustbotBorg =
{
	tlRustbotBorg, ARRAYSIZE(tlRustbotBorg), 0x300, 0, "Rustbot Borg",
};

static Task_t tlRustbotCombatFail[] =
{
	{TASK_WAIT_FOR_MOVEMENT, 0}, {TASK_SET_ACTIVITY, ACT_IDLE}, {TASK_WAIT_FACE_ENEMY, 0}, {TASK_SUGGEST_STATE, 0},
};
static Schedule_t slRustbotCombatFail =
{
	tlRustbotCombatFail, ARRAYSIZE(tlRustbotCombatFail), 0x10c00, 0, "Rustbot Combat Fail",
};

static Task_t tlRustbotFail[] =
{
	{TASK_WAIT_FOR_MOVEMENT, 0}, {TASK_SET_ACTIVITY, ACT_IDLE}, {TASK_WAIT, 2}, {TASK_SUGGEST_STATE, 0},
};
static Schedule_t slRustbotFail =
{
	tlRustbotFail, ARRAYSIZE(tlRustbotFail), 0xc00, 0, "Rustbot Fail",
};

static void CreateRustbotSparkShowers(entvars_t* pev, int count)
{
	for (int i = 0; i < count; ++i)
	{
		Vector vecOrigin = pev->origin + Vector(
			RANDOM_LONG(0, 89) - 45,
			RANDOM_LONG(0, 89) - 45,
			RANDOM_LONG(0, 29) + 24);
		CBaseEntity::Create("spark_shower", vecOrigin, pev->angles, nullptr);
	}
}

static void AdvanceRustbotSentenceCooldown()
{
	// Retail's sentence helper advances its shared deadline before checking the
	// gate, so these Rustbot schedule calls never emit the selected sentence.
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(1.5f, 2.0f);
}

class CRustbot : public CBaseMonster
{
public:
	int MonsterProfile() override { return 13; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	void StartTask(Task_t* pTask) override;
	void RunTask(Task_t* pTask) override;
	void PrescheduleThink() override;
	void AttackSound();
	Schedule_t* GetSchedule() override;
	Schedule_t* GetScheduleOfType(int Type) override;
	int IRelationship(CBaseEntity* pTarget) override
	{
		// User-requested gameplay exception: hostile Rustbots treat Xenomes as
		// enemies while they are invisible (class 13). Keep this scoped to
		// Xenomes; all other relationships use Retail data.
		if (pTarget && FClassnameIs(pTarget->pev, "monster_xenome"))
		{
			return R_DL;
		}
		return CBaseMonster::IRelationship(pTarget);
	}
	int Classify() override { return CLASS_ALIEN_MONSTER; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this - see the
	// feedback-setactivity-act-idle-stomps-scripted-sequence memory
	// note (same fix as CFriendlyGunman etc; covers CRustbotFriendly
	// too via inheritance).
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
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	void Melee();
	void BeamOn();
	void BeamOff();
	void EXPORT BeamThink();

	CBeam* m_pBeam = nullptr;
	CBeam* m_pBeamGlow = nullptr;
	CSprite* m_pBeamEnd = nullptr;
	bool m_bBorg = true; // Retail +0x2e8; initial Borg schedule until first damage.
	float m_flLastEnemyVisible = 0;
	float m_flFrustration = 0; // Retail m_fFrustration; drives beam-charge effects.
	float m_flChangeFrustration = 0; // Retail m_tChangeFrustration; initialized to gpGlobals->time.
	int m_iHeadTurn = 0; // Retail descriptor +0x2cc; Spawn initializes to zero.
};
LINK_ENTITY_TO_CLASS(monster_rustbot, CRustbot);

TYPEDESCRIPTION CRustbot::m_SaveData[] =
	{
		DEFINE_FIELD(CRustbot, m_pBeam, FIELD_CLASSPTR),
	DEFINE_FIELD(CRustbot, m_pBeamGlow, FIELD_CLASSPTR),
	DEFINE_FIELD(CRustbot, m_pBeamEnd, FIELD_CLASSPTR),
		DEFINE_FIELD(CRustbot, m_bBorg, FIELD_BOOLEAN),
		DEFINE_FIELD(CRustbot, m_flLastEnemyVisible, FIELD_TIME),
		DEFINE_FIELD(CRustbot, m_flFrustration, FIELD_FLOAT),
		DEFINE_FIELD(CRustbot, m_flChangeFrustration, FIELD_TIME),
		DEFINE_FIELD(CRustbot, m_iHeadTurn, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CRustbot, CBaseMonster);

Schedule_t* CRustbot::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_COMBAT_FACE:
		return &slRustbotCombatFace;
	case 0x30:
		return &slRustbotSweep;
	case 0x31:
		return &slRustbotWaitInCover;
	case 0x34:
		return &slRustbotBorg;
	case 0x12: // Retail TakeCover; all tasks are now mapped, including 0x65.
		return &slRustbotTakeCover;
	case 0x18:
		return &slRustbotRangeAttack;
	case 0x29:
		// Retail's failed-schedule record depends on whether the enemy handle
		// still resolves; this is the only custom fallback case.
		return m_hEnemy ? &slRustbotCombatFail : &slRustbotFail;
	case 0x32:
		// Retail retries the range attack when that condition is available,
		// otherwise it requests its failed-schedule case (0x29).
		return GetScheduleOfType(HasConditions(bits_COND_CAN_RANGE_ATTACK1) ? 0x18 : 0x29);
	case 0x2e:
		return &slRustbotEstablishLineOfFire;
	case 0x2f:
		return &slRustbotEstablishLineOfFireRetry;
	case 0x33:
		// Retail uses +0x29c as the last time an enemy was visible.
		return GetScheduleOfType(gpGlobals->time - m_flLastEnemyVisible > 10.0f ? 0x29 : 0x18);
	}

	// Retail delegates unhandled schedule types to CBaseMonster. Only 0x29
	// uses the enemy-handle-specific Rustbot failure records above.
	return CBaseMonster::GetScheduleOfType(Type);
}

Schedule_t* CRustbot::GetSchedule()
{
	// User-requested Xenome exception to Retail's initial Borg behavior: in a
	// free encounter a Xenome can select a nearby Rustbit while this Rustbot
	// remains in Borg with no enemy. Let a hostile Rustbot leave Borg when a
	// Xenome is actually visible, while preserving the Retail damage trigger for
	// every other target. Friendly Rustbots keep their authored passive behavior.
	if (m_bBorg && FStrEq(STRING(pev->classname), "monster_rustbot"))
	{
		CBaseEntity* pVisibleXenome = nullptr;
		float flNearestXenome = m_flDistLook;
		for (CBaseEntity* pTarget = UTIL_FindEntityByClassname(nullptr, "monster_xenome");
			pTarget; pTarget = UTIL_FindEntityByClassname(pTarget, "monster_xenome"))
		{
			if (!pTarget->IsAlive() || IRelationship(pTarget) <= R_NO ||
				!FInViewCone(pTarget) || !FVisible(pTarget))
				continue;

			const float flDistance = (pTarget->pev->origin - pev->origin).Length();
			if (flDistance <= flNearestXenome)
			{
				flNearestXenome = flDistance;
				pVisibleXenome = pTarget;
			}
		}

		if (pVisibleXenome)
		{
			m_bBorg = false;
			m_hEnemy = pVisibleXenome;
			m_vecEnemyLKP = pVisibleXenome->pev->origin;
			m_MonsterState = MONSTERSTATE_COMBAT;
			SetConditions(bits_COND_NEW_ENEMY | bits_COND_SEE_ENEMY);
		}
	}

	// Retail GetSchedule @0x100b9600 gives the Borg schedule priority until
	// either light or heavy damage clears the saved m_bBorg flag (+0x2e8).
	if (m_bBorg)
	{
		if (!HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
			return GetScheduleOfType(0x34);

		m_bBorg = false;
	}

	if (HasConditions(bits_COND_HEAR_SOUND))
	{
		CSound* pSound = PBestSound();
		if (pSound)
		{
			if (!HasConditions(bits_COND_SEE_ENEMY) &&
				(pSound->m_iType & (bits_SOUND_COMBAT | bits_SOUND_PLAYER)))
				MakeIdealYaw(pSound->m_vecOrigin);

			if (pSound->m_iType & bits_SOUND_DANGER)
			{
				AdvanceRustbotSentenceCooldown(); // RB_DANGER, group 11.
				return GetScheduleOfType(SCHED_TAKE_COVER_FROM_BEST_SOUND);
			}
		}
	}

	if (m_MonsterState == MONSTERSTATE_COMBAT && !HasConditions(bits_COND_ENEMY_DEAD))
	{
		if (HasConditions(bits_COND_NEW_ENEMY))
		{
			if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
				return GetScheduleOfType(SCHED_MELEE_ATTACK1);

			if (RANDOM_LONG(0, 1) != 0 && m_hEnemy)
			{
				int iEnemyClass = m_hEnemy->Classify();
				if (iEnemyClass != CLASS_MACHINE && iEnemyClass != CLASS_HUMAN_PASSIVE &&
					iEnemyClass != CLASS_HUMAN_MILITARY)
					AdvanceRustbotSentenceCooldown(); // RB_DANGER, group 11.
			}

			if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
				return GetScheduleOfType(SCHED_MELEE_ATTACK1);
			return GetScheduleOfType(0x2e);
		}

		if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
			return GetScheduleOfType(SCHED_MELEE_ATTACK1);
		if (HasConditions(bits_COND_LIGHT_DAMAGE))
		{
			AdvanceRustbotSentenceCooldown(); // RB_COVER, group 2.
			return GetScheduleOfType(0x12);
		}
		if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
			return GetScheduleOfType(0x18);
		if (HasConditions(bits_COND_ENEMY_OCCLUDED))
		{
			if (RANDOM_LONG(0, 1) != 0)
				AdvanceRustbotSentenceCooldown(); // RB_THROW, group 4.
			return GetScheduleOfType(0x2f);
		}
		if (HasConditions(bits_COND_SEE_ENEMY))
			return GetScheduleOfType(0x2e);
	}

	return CBaseMonster::GetSchedule();
}

void CRustbot::PrescheduleThink()
{
	// Retail slot 117 updates +0x29c while an enemy is visible and advances the
	// +0x2d0 m_fFrustration beam-charge effect while it is active.
	if (m_hEnemy && HasConditions(bits_COND_SEE_ENEMY))
		m_flLastEnemyVisible = gpGlobals->time;
	if (m_flFrustration < 0)
		m_flFrustration = 0;
	else if (m_flFrustration >= 10)
		m_flFrustration = 10;

	if (m_flFrustration >= 5)
	{
		float flRateAdjust = m_flFrustration * 0.1f;
		if (flRateAdjust > 0.7f)
			flRateAdjust = 0.7f;
		pev->framerate = 1.2f + flRateAdjust;
		if (RANDOM_LONG(0, 39) < 2)
			CreateRustbotSparkShowers(pev, 1);
	}

	CBaseMonster::PrescheduleThink();
}

void CRustbot::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/rustbot.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 64));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->health = gSkillData.rustbotHealth +
		RANDOM_FLOAT(0, gSkillData.rustbotHealth) * (gSkillData.percentRandomHealth * 0.01f);
	pev->view_ofs = Vector(0, 0, 40);
	m_bBorg = true;
	m_flChangeFrustration = gpGlobals->time;
	m_iHeadTurn = 0;

	MonsterInit();

	// Retail CRustbot::Spawn @ 0x100b91c0 restores the authored eye height
	// after initialization; rustbot.mdl has no $eyeposition for this lookup.
	pev->view_ofs = Vector(0, 0, 40);
}

void CRustbot::Precache()
{
	PrecacheModel("models/rustbot.mdl");
	PrecacheModel("sprites/xbeam1.spr");
	PrecacheModel("sprites/rustbeam_end.spr");
	PRECACHE_SOUND("rustbot/P-up.wav");
	PRECACHE_SOUND("rustbot/Plasma1.wav");
	PRECACHE_SOUND("rustbot_attack1.wav");
}

void CRustbot::AttackSound()
{
	// Retail FUN_100b8380 (AttackSound, vtable slot 131) inserts this danger
	// sound before playing the arm-strike cue.
	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 512, 0.2f);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "rustbot_attack1.wav", 0.6f, 0.8f, 0, PITCH_NORM);
}

void CRustbot::StartTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_RUSTBOT_POWERUP:
		TaskBegin();
		m_IdealActivity = ACT_ARM;
		// Retail FUN_100b82f0 inserts sound type 0x20 (danger), radius
		// 0x400 and 0.3 seconds before playing the P-up voice.
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 1024, 0.3f);
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "rustbot/P-up.wav", 0.6f, 0.8f, 0, PITCH_NORM);
		break;
	case TASK_RUSTBOT_PLASMA:
		TaskBegin();
		m_IdealActivity = ACT_RANGE_ATTACK1;
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "rustbot/Plasma1.wav", 0.6f, 0.8f, 0, PITCH_NORM);
		break;
	case TASK_RUSTBOT_SENTENCE:
		TaskBegin();
		// FUN_100b9370 calls the sentence helper, which advances the shared
		// deadline before its gate and therefore emits no phrase in Retail.
		AdvanceRustbotSentenceCooldown();
		if (!HasConditions(bits_COND_TASK_FAILED))
			TaskComplete();
		break;
	default:
		CBaseMonster::StartTask(pTask);
		break;
	}
}

void CRustbot::RunTask(Task_t* pTask)
{
	if (pTask->iTask == TASK_RUSTBOT_POWERUP || pTask->iTask == TASK_RUSTBOT_PLASMA)
	{
		// Retail completes both custom activity tasks after their sequence,
		// unless TASK_FAILED is set; otherwise the task keeps waiting.
		if (m_fSequenceFinished && !HasConditions(bits_COND_TASK_FAILED))
			TaskComplete();
		return;
	}

	CBaseMonster::RunTask(pTask);
}

void CRustbot::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case RUSTBOT_AE_MELEE:
		CreateRustbotSparkShowers(pev, RANDOM_LONG(1, 3));
		AttackSound();
		Melee();
		break;
	case RUSTBOT_AE_BEAM_OFF:
		BeamOff();
		break;
	case RUSTBOT_AE_BEAM_ON:
		BeamOn();
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CRustbot::Melee()
{
	if (!m_hEnemy)
		return;

	if ((m_hEnemy->pev->origin - pev->origin).Length() <= 128)
	{
		m_hEnemy->TakeDamage(pev, pev, gSkillData.rustbotMeleeDamage, DMG_CLUB);
	}
}

void CRustbot::BeamOn()
{
	BeamOff();
	m_flFrustration += 2;
	CreateRustbotSparkShowers(pev, 5);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPARKS);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 40);
	MESSAGE_END();

	m_pBeam = CBeam::BeamCreate("sprites/xbeam1.spr", 5);
	m_pBeamGlow = CBeam::BeamCreate("sprites/xbeam1.spr", 5);
	m_pBeamEnd = CSprite::SpriteCreate("sprites/rustbeam_end.spr", pev->origin, true);
	if (!m_pBeam || !m_pBeamGlow || !m_pBeamEnd)
	{
		BeamOff();
		return;
	}

	// Retail builds two point beams and a glowing impact sprite. The second
	// beam overlays the first; only the first carries the spark-start flag.
	m_pBeam->PointsInit(pev->origin, pev->origin);
	m_pBeam->SetColor(80, 120, 255);
	m_pBeam->SetBrightness(255);
	m_pBeam->SetFrame(10);
	m_pBeam->pev->owner = edict();
	m_pBeamGlow->PointsInit(pev->origin, pev->origin);
	m_pBeamGlow->SetColor(80, 120, 255);
	m_pBeamGlow->SetBrightness(255);
	m_pBeamGlow->SetFrame(10);
	m_pBeamGlow->pev->owner = edict();
	m_pBeam->SetFlags(SF_BEAM_SPARKSTART);
	m_pBeamEnd->SetScale(0.75f);
	m_pBeamEnd->SetTransparency(kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation);
	m_pBeamEnd->pev->owner = edict();

	SetThink(&CRustbot::BeamThink);
	pev->nextthink = gpGlobals->time;
}

void CRustbot::BeamOff()
{
	if (m_pBeamEnd)
	{
		m_pBeamEnd->Expand(10.0f, 500.0f);
		m_pBeamEnd = nullptr;
	}
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = nullptr;
	}
	if (m_pBeamGlow)
	{
		UTIL_Remove(m_pBeamGlow);
		m_pBeamGlow = nullptr;
	}

	// BeamOn temporarily replaces the regular monster think callback.  Always
	// restore it here; otherwise a finished beam leaves the rustbot running an
	// inert BeamThink every 100 ms and prevents its normal AI from resuming.
	SetThink(&CBaseMonster::CallMonsterThink);
	pev->nextthink = gpGlobals->time;
}

void CRustbot::BeamThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_pBeam || !m_pBeamGlow || !m_pBeamEnd || !m_hEnemy)
	{
		BeamOff();
		return;
	}

	// BeamOn replaces CBaseMonster::MonsterThink while the eye beam is
	// active. Keep advancing the attack sequence here so its later beam-off
	// event is dispatched instead of leaving the bot in an inert beam loop.
	float flInterval = StudioFrameAdvance();
	DispatchAnimEvents(flInterval);
	if (!m_pBeam || !m_hEnemy)
		return;

	CBaseMonster* pEnemy = m_hEnemy->MyMonsterPointer();
	if (!pEnemy)
	{
		BeamOff();
		return;
	}

	Vector vecSrc;
	Vector vecAttachmentAngles;
	GetAttachment(0, vecSrc, vecAttachmentAngles);

	Vector vecDirection;
	if (pEnemy->IsAlive() && pEnemy->m_hTargetEnt)
	{
		// Retail FUN_100b8400 uses FUN_1002d200 from attachment 0 and applies
		// bounded jitter only for a live target that has its own target handle.
		vecDirection = pEnemy->BodyTarget(vecSrc) - vecSrc;
		vecDirection.x += RANDOM_LONG(-32, 32);
		vecDirection.y += RANDOM_LONG(-32, 32);
		vecDirection.z += RANDOM_LONG(0, 64);
		vecDirection = vecDirection.Normalize();
	}
	else
	{
		// Otherwise Retail aims from the enemy's BodyTarget offset and the
		// Rustbot's last-known enemy position.
		const Vector vecBodyOffset = pEnemy->BodyTarget(vecSrc) - pEnemy->pev->origin;
		vecDirection = (m_vecEnemyLKP + vecBodyOffset - vecSrc).Normalize();
	}
	Vector vecEnd = vecSrc + vecDirection * 2048.0f;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	m_pBeam->SetStartPos(vecSrc);
	m_pBeam->SetEndPos(tr.vecEndPos);
	m_pBeamGlow->SetStartPos(vecSrc);
	m_pBeamGlow->SetEndPos(tr.vecEndPos);
	UTIL_SetOrigin(m_pBeamEnd->pev, tr.vecEndPos);
	// PointsInit relinks once at creation. The endpoints move every tick,
	// so update the beam bounds/PVS link as the stock beam paths do.
	m_pBeam->RelinkBeam();
	m_pBeamGlow->RelinkBeam();

	if (tr.flFraction < 1.0 && tr.pHit)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && pHit->pev->takedamage != DAMAGE_NO)
		{
			if (pHit->IsAlive())
				m_flFrustration -= 0.1f;
			pHit->TakeDamage(pev, pev, gSkillData.rustbotLongDamage, DMG_ENERGYBEAM);
		}

		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "buttons/spark6.wav", 0.5, ATTN_NORM, 0, 100);
	}
}

void CRustbot::Killed(entvars_t* pevAttacker, int iGib)
{
	BeamOff();
	CBaseMonster::Killed(pevAttacker, iGib);
}

//=========================================================
// monster_rustbot_friendly - CRustbotFriendly (rust7d+). Confirmed
// fresh this session (LINK @0x100b9d30, vtable @0x10101fd0): Spawn
// and Precache are the exact same addresses as monster_rustbot's own
// (0x100b91c0/0x100b8010). Classify (slot 8, @0x100b9db0) is the
// only override and returns the literal constant 3
// (CLASS_HUMAN_PASSIVE), matching monster_rustbit_friendly's
// identical pattern (see rust1_misc.cpp).
//=========================================================
class CRustbotFriendly : public CRustbot
{
public:
	int MonsterProfile() override { return 13; } // retail slot 59
	int Classify() override { return CLASS_HUMAN_PASSIVE; }
};
LINK_ENTITY_TO_CLASS(monster_rustbot_friendly, CRustbotFriendly);

//=========================================================
// lightning_bug - CLightningBug. Homing sub-projectile spawned
// periodically by ball_lightning's own MoveThink (see the
// ball_lightning section below for the full decompile evidence).
// Reuses this project's established CDmlRocket::TrackTarget-style
// simplified homing rather than the original's exact charge-tier
// damage table (see file header note below).
//=========================================================
class CLightningBug : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT TrackTargetBug();
	void EXPORT DieTouchBug(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(lightning_bug, CLightningBug);

void CLightningBug::Precache()
{
	// Retail Precache (0x10080990): the bug has its own spark sprite,
	// beam sprite and client event; it does not reuse ball_lightning's
	// flyball sprite.
	PrecacheModel("sprites/ballspark.spr");
	PrecacheModel("sprites/zbeam5.spr");
	PrecacheEvent(1, "events/balllightning.sc");
}

void CLightningBug::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("lightning_bug");
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->scale = 1.0;
	SET_MODEL(ENT(pev), "sprites/ballspark.spr");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	// Retail Spawn (0x10080740): multiplayer has its own 130-damage
	// branch; single-player uses 25/75/100 for easy/medium/hard and
	// increases only hard-mode flight speed.
	if (g_pGameRules && g_pGameRules->IsMultiplayer())
	{
		pev->dmg = 130.0f;
		pev->speed = 300.0f;
	}
	else if (g_iSkillLevel == SKILL_HARD)
	{
		pev->dmg = 100.0f;
		pev->speed = 400.0f;
	}
	else if (g_iSkillLevel == SKILL_MEDIUM)
	{
		pev->dmg = 75.0f;
		pev->speed = 300.0f;
	}
	else
	{
		pev->dmg = 25.0f;
		pev->speed = 300.0f;
	}

	SetTouch(&CLightningBug::DieTouchBug);
	SetThink(&CLightningBug::TrackTargetBug);
	pev->nextthink = gpGlobals->time + 0.1;

	pev->dmgtime = gpGlobals->time + 4.0; // self-expiry, no original timing decompiled
}

void CLightningBug::TrackTargetBug()
{
	if (gpGlobals->time >= pev->dmgtime)
	{
		UTIL_Remove(this);
		return;
	}
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_hEnemy || !m_hEnemy->IsAlive())
		return;

	Vector vecToTarget = (m_hEnemy->pev->origin - pev->origin).Normalize();
	float flSpeed = pev->velocity.Length();
	if (flSpeed < 1.0f)
		flSpeed = 400.0f;

	Vector vecNewDir = (pev->velocity.Normalize() * 0.85 + vecToTarget * 0.15).Normalize();
	pev->velocity = vecNewDir * flSpeed;
	pev->angles = UTIL_VecToAngles(pev->velocity);
}

void CLightningBug::DieTouchBug(CBaseEntity* pOther)
{
	if (pOther && pOther->pev->takedamage != DAMAGE_NO)
		pOther->TakeDamage(pev, pev, pev->dmg, DMG_SHOCK);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPARKS);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	MESSAGE_END();

	UTIL_Remove(this);
}

//=========================================================
// Shared target-picker for ball_lightning's lightning_bug spawns and
// weapon_beamgun's tracer volley (see both sections below and
// beamgun.cpp): if the firer is a CBaseMonster (e.g.
// monster_rustflier), reuse its own m_hEnemy; if it's the player
// (weapon_beamgun has no tracked "enemy" field in this SDK),
// approximate by picking the nearest living monster within range - a
// documented simplification, not decompiled. Not declared static
// since beamgun.cpp reuses it (extern-declared there).
//=========================================================
CBaseEntity* FindLightningBugTarget(CBaseEntity* pOwner, const Vector& vecOrigin)
{
	if (pOwner)
	{
		CBaseMonster* pMonster = pOwner->MyMonsterPointer();
		if (pMonster && pMonster->m_hEnemy && pMonster->m_hEnemy->IsAlive())
			return pMonster->m_hEnemy;
	}

	CBaseEntity* pList[8];
	int iCount = UTIL_MonstersInSphere(pList, 8, vecOrigin, 750.0f);
	CBaseEntity* pBest = nullptr;
	float flBestDist = 750.0f * 750.0f;
	for (int i = 0; i < iCount; i++)
	{
		if (pList[i] == pOwner || !pList[i]->IsAlive())
			continue;
		float flDist = (pList[i]->pev->origin - vecOrigin).LengthSquared();
		if (flDist < flBestDist)
		{
			flBestDist = flDist;
			pBest = pList[i];
		}
	}
	return pBest;
}

//=========================================================
// ball_lightning - CBallLightning. The runtime-only ranged attack
// projectile fired by monster_rustflier's HuntThink (per
// findings/entities/rustflier_family.md, also independently fired by
// the player weapon_beamgun's overheat mode - out of scope here, not
// yet implemented). No LINK-time map placement; spawned only via
// CBaseEntity::Create. Not individually re-decompiled this session
// (the doc's confirmation that CRustFlier::HuntThink calls a generic
// ball_lightning spawn helper was enough to justify a standard
// runtime-projectile implementation, same pattern as
// vehicle_tank_rocket/monster_targetrocket_proj elsewhere in this
// project) - straight-line flight, no homing.
//
// CORRECTION (2026-09-05): findings/entities/code_annahme_final_batch.md's
// "tracer/lightning_bug-Verhältnis" open point resolved by fresh
// decompile this session. `FUN_100812d0` (the "lightning_bug"
// constructor, vtable 0x100fa728) is called from
// **`CBallLightning::MoveThink`** (confirmed via xref) - i.e.
// ball_lightning periodically spawns homing "lightning_bug"
// sub-projectiles during its own flight, not a separate/unrelated
// system. `lightning_bug`'s own ctor (`FUN_10080740`) confirms
// Think=`CLightningBug::TrackTargetBug`, Touch=`CLightningBug::DieTouchBug`,
// and a 4-tier charge-scaled damage/speed table (param_1[0xa4] 1-4,
// confirmed float damage constants 25/75/100/~130 across the
// branches). Reproduced here as: `CBallLightning::MoveThink` (new,
// replaces the previous plain `SUB_Remove`-after-5s Think) fires one
// `CLightningBug` every ~0.5s toward the ball's current target (see
// `FindLightningBugTarget()` below) for as long as it has a valid
// target and hasn't yet hit anything; `CLightningBug` reuses this
// project's established `CDmlRocket::TrackTarget`-style simplified
// homing (steer a fraction toward the target's current position each
// tick, not the exact original lead/blend math) rather than the exact
// charge-tier table, which is approximated with a flat mid-tier
// damage/speed instead of the 4-way branch.
//=========================================================
class CBallLightning : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT BoltTouch(CBaseEntity* pOther);
	void EXPORT MoveThink();
	void EXPORT PopThink();

	unsigned short m_usEvent = 0;
};
LINK_ENTITY_TO_CLASS(ball_lightning, CBallLightning);

void CBallLightning::Precache()
{
	// Retail Precache (0x1007ff90): CBallLightning has its own primary
	// sprite, pop sprite, smoke sprite, flier sound set and event.
	PrecacheModel("sprites/ballpop.spr");
	PrecacheModel("sprites/ballight.spr");
	PrecacheModel("sprites/smoke.spr");
	PrecacheSound("weapons/ball-fly.wav");
	PrecacheSound("weapons/ball-die.wav");
	PrecacheSound("flier/flyball_fly1.wav");
	PrecacheSound("flier/flyball_fly2.wav");
	PrecacheSound("flier/flyball_fly3.wav");
	PrecacheSound("flier/flyball_hit1.wav");
	PrecacheSound("flier/flyball_hit2.wav");
	PrecacheSound("flier/flyball_hit3.wav");
	m_usEvent = PrecacheEvent(1, "events/balllightning.sc");
	UTIL_PrecacheOther("lightning_bug");
}

void CBallLightning::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("ball_lightning");
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	// Retail Spawn @0x1007fd50 (frisch dekompiliert 2026-10-31): Rendermodus 3
	// (Glow), Farbe 255/255/255, Renderfx 14, takedamage 0, dmg 0, frame 1.0,
	// erstes nextthink +0.1.
	pev->rendermode = kRenderGlow;
	pev->rendercolor = Vector(255, 255, 255);
	pev->renderamt = 255;
	pev->renderfx = kRenderFxNoDissipation;
	pev->takedamage = DAMAGE_NO;
	pev->dmg = 0;
	pev->frame = 1.0f;
	pev->scale = 1.0f;
	SET_MODEL(ENT(pev), "sprites/ballight.spr");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetTouch(&CBallLightning::BoltTouch);

	pev->dmgtime = gpGlobals->time + 5.0; // reused as this ball's own expiry deadline
	SetThink(&CBallLightning::MoveThink);
	pev->nextthink = gpGlobals->time + 0.1;

	// Retail CBallLightning::Spawn: medium skill uses the weapon sample;
	// other skill levels choose a Flier sample. Both use a random pitch 80..143.
	const char* pszFlySound = "weapons/ball-fly.wav";
	if (g_iSkillLevel != SKILL_MEDIUM)
	{
		static const char* const pFlierSounds[] = {
			"flier/flyball_fly1.wav", "flier/flyball_fly2.wav", "flier/flyball_fly3.wav"};
		pszFlySound = pFlierSounds[RANDOM_LONG(0, 2)];
	}
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pszFlySound, 1.0f, 0.8f, 0, RANDOM_LONG(0, 63) + 80);
}

void CBallLightning::BoltTouch(CBaseEntity* pOther)
{
	// Retail LightningTouch ignores its owner and other entities owned by the
	// same edict before playing the impact sound or applying the shock blast.
	if (pOther->edict() == pev->owner || pOther->pev->owner == pev->owner)
		return;

	// Retail uses the weapon impact sample at medium skill, otherwise one of
	// three Rust Flier samples. The sound pitch is independently randomized.
	const char* pszHitSound = "weapons/ball-die.wav";
	if (g_iSkillLevel != SKILL_MEDIUM)
	{
		static const char* const pFlierSounds[] = {
			"flier/flyball_hit1.wav", "flier/flyball_hit2.wav", "flier/flyball_hit3.wav"};
		pszHitSound = pFlierSounds[RANDOM_LONG(0, 2)];
	}
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pszHitSound, 1.0f, 0.8f, 0, RANDOM_LONG(0, 63) + 80);
	PLAYBACK_EVENT_FULL(0, edict(), m_usEvent, 0, g_vecZero, g_vecZero,
		0, 0, 0, 0, 0, 1);

	// Retail immediately removes a newly spawned ball that touches worldspawn;
	// the bounce path can otherwise retrigger the same collision repeatedly.
	const float flSpawnTime = pev->dmgtime - 5.0f;
	if (gpGlobals->time - flSpawnTime < 1.0f && FClassnameIs(pOther->pev, "worldspawn"))
	{
		pev->velocity = g_vecZero;
		pev->avelocity = g_vecZero;
		pev->origin = g_vecZero;
		pev->oldorigin = g_vecZero;
		UTIL_Remove(this);
		return;
	}

	// Retail CBallLightning::LightningTouch (0x10080040) freezes the projectile,
	// applies the radius blast, then hides and removes it after 0.3 seconds.
	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;

	// The Retail radius helper receives the ball as inflictor and its owner as
	// attacker. This is shared by Rust Flier and Beamgun.
	RadiusDamage(pev->origin, pev, VARS(pev->owner), 60.0f, 40.0f, CLASS_NONE, DMG_SHOCK);
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CBallLightning::PopThink);
	pev->nextthink = gpGlobals->time + 0.3f;
}

void CBallLightning::PopThink()
{
	UTIL_Remove(this);
}

void CBallLightning::MoveThink()
{
	if (gpGlobals->time >= pev->dmgtime)
	{
		UTIL_Remove(this);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.5;

	CBaseEntity* pOwner = pev->owner ? CBaseEntity::Instance(pev->owner) : nullptr;
	CBaseEntity* pTarget = FindLightningBugTarget(pOwner, pev->origin);
	if (!pTarget)
		return;

	CBaseEntity* pBug = CBaseEntity::Create("lightning_bug", pev->origin, UTIL_VecToAngles(pev->velocity), pev->owner);
	if (pBug)
	{
		pBug->pev->velocity = pev->velocity.Normalize() * pBug->pev->speed;
		static_cast<CLightningBug*>(pBug)->m_hEnemy = pTarget;
	}
}

//=========================================================
// monster_rustflier - CRustFlier. A flying, turbine-driven robot with
// a shield-buffer damage mechanic and a scripted crash-death sequence
// (see findings/entities/rustflier_family.md - a thorough, multi-
// session doc whose Spawn/Precache/Classify/KeyValue/TakeDamage/
// FlierUse/CrashTouch/FlyTouch claims were all independently
// re-confirmed via fresh decompile this session, not taken on faith).
//
// Decompiled fresh: LINK @0x100b2b50, ctor helper @0x100b2c00 (vtable
// @0x101012fc - matches the doc's independently-found vtable value
// exactly), Spawn @0x100b2fa0, Precache @0x100b30f0, Classify
// @0x100b2c80 (constant 8 = CLASS_ALIEN_PREY), KeyValue @0x100b2d70
// (confirms all 6 FGD fields: startshieldstate, targetdamage,
// deathpath, deathangle, timetillcrash, triggerondeath - and their
// FGD-documented defaults, independently reconstructed from the
// ctor's field-init values: shield on, targetdamage 10000, deathangle
// (0,0,0), timetillcrash 10), TakeDamage @0x100b5270 (vtable slot 10),
// FlierUse @0x100b4fe0, CrashTouch @0x100b3290, FlyTouch @0x100b31b0
// (all confirmed byte-for-byte matching the doc's descriptions).
//
// Confirmed bbox (-64,-64,0)/(64,64,64) (matches the FGD exactly -
// the doc's "nicht verifiziert" bbox open point is now resolved),
// MOVETYPE_FLY, SOLID_BBOX, pev->takedamage forced to DAMAGE_YES.
// TakeDamage's real logic (re-derived from the fresh decompile, not
// just summarized from the doc): pev->health itself doubles as the
// "still alive" gate, NOT a depleting shield counter - any single hit
// below the targetdamage threshold is pure cosmetic absorption (a
// random orange renderfx flash + one of 3 impact sounds, no HP loss
// at all), while a single hit AT OR ABOVE the targetdamage threshold
// (while pev->health > 0) immediately triggers the crash sequence.
// This is a "must land one huge hit" boss mechanic, not a drainable
// shield bar. FlierUse is confirmed as a two-press debug/cheat
// toggle (first press logs "shield down", second deletes the
// entity), not a real gameplay mechanic - reproduced faithfully
// anyway since it's cheap to.
//
// HuntThink steering, target acquisition, attack gates and crash travel
// follow the Retail implementation; CrashingThink's exact spark-particle
// timing remains simplified (see the entity findings).
//=========================================================

#define SF_RUSTFLIER_START_SHIELD_OFF 1 // not FGD-documented as a spawnflag; shield state comes from startshieldstate

class CRustFlier : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	int Classify() override { return CLASS_ALIEN_PREY; }
	// Retail BloodColor (slot 14, @0x100b2c90) returns DONT_BLEED (-1).
	int BloodColor() override { return DONT_BLEED; }
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT FlyTouch(CBaseEntity* pOther);
	void EXPORT CrashTouch(CBaseEntity* pOther);
	void EXPORT HuntThink();
	void EXPORT CrashingThink();
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

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	bool m_bShieldUp = true;			  // "startshieldstate"
	float m_flTargetDamage = 10000;	  // "targetdamage"
	int m_iszDeathPath = 0;			  // "deathpath"
	Vector m_vecDeathAngle = g_vecZero;  // "deathangle"
	float m_flTimeTillCrash = 10;		  // "timetillcrash"
	int m_iszTriggerOnDeath = 0;		  // "triggerondeath"

	float m_flNextAttack = 0;
	// Retail HuntThink fields (HuntThink @0x100b32c0, steering @0x100b4100)
	void HuntSteer();
	void ShieldHit();
	void StartCrash();
	void SetFlierGoal(CBaseEntity* pGoal);
	bool FireMissileDown();
	bool FireBolt();
	float m_flBombGate = 0;		   // +0x270 (gate for the bolt burst)
	float m_flMissileGate = 0;	   // +0x274 (next downward ball_lightning)
	int m_iSalvo = 0;			   // +0x278
	float m_flThrust = 0;		   // +0x27c
	Vector m_vecLastEnemyPos;	   // +0x280
	Vector m_vecDesiredDir;		   // +0x28c
	Vector m_vecGoal;			   // +0x298
	Vector m_vecGoalDir;		   // +0x2a4
	float m_flSpeedAccum = 0;	   // +0x2b0
	float m_flLastSeen = 0;		   // +0x2c0
	float m_flFirstSeen = 0;	   // +0x2c4
	int m_iRotorState = 0;		   // +0x2d4
	float m_flBoltGate = 0;		   // +0x2d8
	EHANDLE m_hGlow;			   // +0x2dc
	int m_iFlashCounter = 0;	   // +0x2e0
	bool m_bCrashing = false;
	Vector m_vecCrashGoal = g_vecZero;
	float m_flCrashStartTime = 0;
};
LINK_ENTITY_TO_CLASS(monster_rustflier, CRustFlier);

TYPEDESCRIPTION CRustFlier::m_SaveData[] =
	{
		DEFINE_FIELD(CRustFlier, m_bShieldUp, FIELD_BOOLEAN),
		DEFINE_FIELD(CRustFlier, m_flTargetDamage, FIELD_FLOAT),
		DEFINE_FIELD(CRustFlier, m_iszDeathPath, FIELD_STRING),
		DEFINE_FIELD(CRustFlier, m_vecDeathAngle, FIELD_VECTOR),
		DEFINE_FIELD(CRustFlier, m_flTimeTillCrash, FIELD_FLOAT),
		DEFINE_FIELD(CRustFlier, m_iszTriggerOnDeath, FIELD_STRING),
		DEFINE_FIELD(CRustFlier, m_flNextAttack, FIELD_TIME),
		DEFINE_FIELD(CRustFlier, m_flBombGate, FIELD_TIME),
		DEFINE_FIELD(CRustFlier, m_flMissileGate, FIELD_TIME),
		DEFINE_FIELD(CRustFlier, m_iSalvo, FIELD_INTEGER),
		DEFINE_FIELD(CRustFlier, m_flThrust, FIELD_FLOAT),
		DEFINE_FIELD(CRustFlier, m_vecLastEnemyPos, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CRustFlier, m_vecDesiredDir, FIELD_VECTOR),
		DEFINE_FIELD(CRustFlier, m_vecGoal, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CRustFlier, m_vecGoalDir, FIELD_VECTOR),
		DEFINE_FIELD(CRustFlier, m_flSpeedAccum, FIELD_FLOAT),
		DEFINE_FIELD(CRustFlier, m_flLastSeen, FIELD_TIME),
		DEFINE_FIELD(CRustFlier, m_flFirstSeen, FIELD_TIME),
		DEFINE_FIELD(CRustFlier, m_iRotorState, FIELD_INTEGER),
		DEFINE_FIELD(CRustFlier, m_flBoltGate, FIELD_TIME),
		DEFINE_FIELD(CRustFlier, m_hGlow, FIELD_EHANDLE),
		DEFINE_FIELD(CRustFlier, m_iFlashCounter, FIELD_INTEGER),
		DEFINE_FIELD(CRustFlier, m_bCrashing, FIELD_BOOLEAN),
		DEFINE_FIELD(CRustFlier, m_vecCrashGoal, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CRustFlier, m_flCrashStartTime, FIELD_TIME),
	};

IMPLEMENT_SAVERESTORE(CRustFlier, CBaseMonster);

bool CRustFlier::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "startshieldstate"))
	{
		m_bShieldUp = atoi(pkvd->szValue) != 0;
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "targetdamage"))
	{
		m_flTargetDamage = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "deathpath"))
	{
		m_iszDeathPath = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "deathangle"))
	{
		UTIL_StringToVector((float*)m_vecDeathAngle, pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "timetillcrash"))
	{
		m_flTimeTillCrash = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerondeath"))
	{
		m_iszTriggerOnDeath = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

void CRustFlier::Spawn()
{
	// Retail CRustFlier::Spawn @ 0x100b2fa0 (no MonsterInit): see
	// findings/entities/cbasebird_huntthink_2026-10-31.md Nachtrag 13.
	Precache();

	SET_MODEL(ENT(pev), "models/rustflyer.mdl");
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->effects = EF_INVLIGHT;
	pev->takedamage = DAMAGE_YES;
	pev->flags |= FL_FLY;
	pev->deadflag = DEAD_NO;
	pev->health = gSkillData.chopperHealth; // same skill global as the chopper (0x10136f44)
	UTIL_SetSize(pev, Vector(-64, -64, 0), Vector(64, 64, 64));
	UTIL_SetOrigin(pev, pev->origin);
	m_iFlashCounter = 0;
	m_flFieldOfView = -0.7f;
	pev->sequence = 0;
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);

	SetThink(&CRustFlier::HuntThink);
	SetTouch(&CRustFlier::FlyTouch);
	pev->nextthink = gpGlobals->time + 1.0;

	// retail FUN_100b4ab0
	m_flBoltGate = gpGlobals->time + 0.3f;
	m_iSalvo = 30;
	m_flBombGate = gpGlobals->time + 2.0f;
	m_hGlow = nullptr;
}

void CRustFlier::Precache()
{
	PrecacheModel("models/rustflyer.mdl");
	PrecacheModel("models/rustflyer_crashed.mdl");
	PrecacheModel("sprites/flyball.spr");
	PrecacheModel("sprites/white.spr");
	PrecacheModel("sprites/fexplo.spr");
	PrecacheModel("models/metalplategibs_green.mdl");
	UTIL_PrecacheOther("ball_lightning");

	PrecacheSound("flier/ap_rotor2.wav");
	PrecacheSound("flier/ap_whine1.wav");
	PrecacheSound("flier/flyer_sick.wav");
	PrecacheSound("flier/field_impact1.wav");
	PrecacheSound("flier/field_impact2.wav");
	PrecacheSound("flier/field_impact3.wav");
}

// Cosmetic shield reaction shared by retail TakeDamage (0x100b50f0) and
// TraceAttack (0x100b5270): counter reset, glow-shell flash, one of three
// field impact sounds (ambient, attenuation 0.8, pitch 80..120).
void CRustFlier::ShieldHit()
{
	if (!m_bShieldUp)
		return;

	m_iFlashCounter = 0;
	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = 255;
	pev->rendercolor = Vector(150, 0, 255);

	const char* pszSound = "flier/field_impact1.wav";
	switch (RANDOM_LONG(0, 2))
	{
	case 1: pszSound = "flier/field_impact2.wav"; break;
	case 2: pszSound = "flier/field_impact3.wav"; break;
	}
	EMIT_AMBIENT_SOUND(ENT(pev), pev->origin, pszSound, 1.0, 0.8, 0, 80 + RANDOM_LONG(0, 40));
}

bool CRustFlier::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (pevAttacker == pev)
		return false;

	if (flDamage > m_flTargetDamage && pev->health > 0)
	{
		StartCrash();
		return false;
	}

	ShieldHit();
	return false;
}

void CRustFlier::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (flDamage > m_flTargetDamage && pev->health > 0)
	{
		StartCrash();
		return;
	}

	if (m_bShieldUp)
	{
		ShieldHit();
		// retail FUN_10060080: TE_ARMOR_RICOCHET (111) at the end position, scale 2
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos);
		WRITE_BYTE(TE_ARMOR_RICOCHET);
		WRITE_COORD(ptr->vecEndPos.x);
		WRITE_COORD(ptr->vecEndPos.y);
		WRITE_COORD(ptr->vecEndPos.z);
		WRITE_BYTE(2);
		MESSAGE_END();
	}
}

// Retail FUN_100b3c00 creates a CSphereExplosion with parameters recovered
// from its constructor call; see cbasebird_huntthink_2026-10-31.md Nachtrag 14.
void CRustFlier::StartCrash()
{
	GunmanFireSphereExplosion(pev->origin, 200.0f, 8.0f, 255, 5, 1, 0, 0, 2.5f, 1.0f, 0.5f);

	CBaseEntity* pGlow = m_hGlow;
	if (pGlow)
		UTIL_Remove(pGlow);
	m_hGlow = nullptr;

	pev->health = -1;
	if (m_bShieldUp)
		m_bShieldUp = false; // retail calls FlierUse(USE_SET): first press = shield down

	if (m_iszTriggerOnDeath)
		FireTargets(STRING(m_iszTriggerOnDeath), this, this, USE_TOGGLE, 0);

	SetThink(&CRustFlier::CrashingThink);
	pev->nextthink = gpGlobals->time + 0.1;

	m_pGoalEnt = m_iszDeathPath ? UTIL_FindEntityByTargetname(nullptr, STRING(m_iszDeathPath)) : nullptr;
	if (m_pGoalEnt)
	{
		Vector vecToGoal = m_pGoalEnt->pev->origin - pev->origin;
		float flDist = vecToGoal.Length();
		pev->velocity = vecToGoal.Normalize() * flDist / m_flTimeTillCrash;

		pev->angles.x = RANDOM_LONG(0, 359);
		pev->angles.y = RANDOM_LONG(0, 359);
		pev->angles.z = RANDOM_LONG(0, 359);
		pev->avelocity.x = UTIL_AngleDiff(m_vecDeathAngle.x, pev->angles.x) / m_flTimeTillCrash;
		pev->avelocity.y = UTIL_AngleDiff(m_vecDeathAngle.y, pev->angles.y) / m_flTimeTillCrash;
		pev->avelocity.z = UTIL_AngleDiff(m_vecDeathAngle.z, pev->angles.z) / m_flTimeTillCrash;
		m_vecGoal = m_pGoalEnt->pev->origin;
	}
}

void CRustFlier::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Confirmed debug/cheat toggle, not a real gameplay mechanic (see
	// file header): first press drops the shield, second deletes the
	// flier outright.
	if (m_bShieldUp)
	{
		m_bShieldUp = false;
		ALERT(at_console, "Flier shield down!\n");
		return;
	}

	ALERT(at_console, "Deleting Flier!\n");
	UTIL_Remove(this);
}

void CRustFlier::FlyTouch(CBaseEntity* pOther)
{
	if (pOther && pOther->pev->movetype == MOVETYPE_FLY)
	{
		// Retail CRustFlier::FlyTouch @ 0x100b31b0 is byte-identical to
		// CChopper::FlyTouch: separate using the current collision plane,
		// not the other flier's origin.
		TraceResult tr = UTIL_GetGlobalTrace();
		pev->velocity = pev->velocity + tr.vecPlaneNormal * (pev->velocity.Length() + 200.0f);
	}
}

void CRustFlier::CrashTouch(CBaseEntity* pOther)
{
	if (pOther && pOther->pev->movetype == MOVETYPE_FLY)
	{
		// Retail CRustFlier::CrashTouch @ 0x100b3290 arms exactly one
		// immediate crash-think pass after colliding with a flying entity.
		SetTouch(NULL);
		pev->nextthink = gpGlobals->time;
	}
}

void CRustFlier::SetFlierGoal(CBaseEntity* pGoal)
{
	m_pGoalEnt = pGoal;
	if (!pGoal)
		return;
	m_vecGoal = pGoal->pev->origin;
	UTIL_MakeVectors(pGoal->pev->angles);
	m_vecGoalDir = gpGlobals->v_forward;
}

// Port of retail CRustFlier::HuntThink @ 0x100b32c0.
void CRustFlier::HuntThink()
{
	StudioFrameAdvance();
	if (m_fSequenceFinished)
		SetSequenceByName("idle");

	// glow sprite attached to attachment 2 (retail creates it on the first think)
	if (m_hGlow == nullptr)
	{
		CSprite* pGlow = CSprite::SpriteCreate("sprites/flyball.spr", pev->origin, true);
		if (pGlow)
		{
			pGlow->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
			pGlow->SetAttachment(edict(), 2);
			pGlow->SetScale(2.0f);
			pGlow->pev->framerate = 9.0f;
			pGlow->pev->spawnflags = 0;
			m_hGlow = pGlow;
		}
	}

	pev->nextthink = gpGlobals->time + 0.1;
	if (m_iFlashCounter == 20)
	{
		pev->renderfx = 0; // ends the TakeDamage flash
		m_iFlashCounter = 0;
	}
	else
	{
		m_iFlashCounter++;
	}

	if (!m_pGoalEnt && !FStringNull(pev->target))
	{
		SetFlierGoal(UTIL_FindEntityByTargetname(nullptr, STRING(pev->target)));
		if (m_pGoalEnt && !FStringNull(m_pGoalEnt->pev->message))
			FireTargets(STRING(m_pGoalEnt->pev->message), this, this, USE_TOGGLE, 0);
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
		SetFlierGoal(UTIL_FindEntityByTargetname(nullptr, STRING(m_pGoalEnt->pev->target)));
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

	HuntSteer();

	if ((pev->spawnflags & 2) != 0)
		return;

	if (m_iSalvo == 0)
	{
		// retail FUN_100b4ab0
		m_flBoltGate = gpGlobals->time + 0.3f;
		m_iSalvo = 30;
		m_flBombGate = gpGlobals->time + 2.0f;
		return;
	}

	if (m_flMissileGate < gpGlobals->time && FireMissileDown())
		m_flMissileGate = gpGlobals->time + 2.85f;

	if (gpGlobals->time >= m_flBombGate && gpGlobals->time >= m_flBoltGate &&
		gpGlobals->time < m_flLastSeen + 1.0 && m_flFirstSeen + 2.0 < gpGlobals->time && FireBolt())
	{
		if (m_flSpeedAccum > 75)
			m_flSpeedAccum = 75;
		m_iSalvo--;
		m_flBoltGate = gpGlobals->time + 0.1f;
	}
}

// Retail steering @ 0x100b4100 = flightsteer.cpp plus the flier sounds.
void CRustFlier::HuntSteer()
{
	flightsteer_t st;
	st.thrust = m_flThrust;
	st.desiredDir = m_vecDesiredDir;
	st.goal = m_vecGoal;
	st.speedAccum = m_flSpeedAccum;
	FlightSteer(pev, st);
	m_flThrust = st.thrust;

	if (m_iRotorState == 0 && (pev->spawnflags & 2) == 0)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "flier/ap_rotor2.wav", 1.0, 0.3, 0, 110);
		m_iRotorState = 0x80;
		return;
	}

	CBaseEntity* pPlayer = UTIL_FindEntityByClassname(nullptr, "player");
	if (pPlayer && (pev->spawnflags & 2) == 0)
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

		float flVol = m_flThrust * 0.01f + 0.1f;
		if (flVol > 1.0f)
			flVol = 1.0f;
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "flier/ap_rotor2.wav", 1.0, 0.3, SND_CHANGE_PITCH | SND_CHANGE_VOL, iPitch);
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "flier/ap_whine1.wav", flVol, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, iPitch);
		if (pev->health < 0)
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "flier/flyer_sick.wav", flVol, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, iPitch);
	}
}

// Retail FUN_100b4d70: when the enemy is far below (direction to the enemy
// against the flier's up vector < -0.8) launch a ball_lightning from
// attachment 1 at speed 600 and play the "attack" sequence.
bool CRustFlier::FireMissileDown()
{
	Vector vecSrc, vecAng;
	GetAttachment(1, vecSrc, vecAng);
	if (m_hEnemy == nullptr || !m_hEnemy->IsAlive())
		return false;

	Vector vecDir = (m_vecLastEnemyPos - vecSrc).Normalize();
	UTIL_MakeVectors(pev->angles);
	Vector vecUp = gpGlobals->v_up.Normalize();
	if (DotProduct(vecUp, vecDir) >= -0.8)
		return false;

	Vector vecAngles = UTIL_VecToAngles(vecDir);
	CBaseEntity* pBolt = CBaseEntity::Create("ball_lightning", vecSrc, vecAngles, edict());
	if (pBolt)
	{
		Vector vecVel;
		UTIL_MakeVectors(vecAngles);
		vecVel = vecDir * 600;
		pBolt->pev->velocity = vecVel;
	}
	SetSequenceByName("attack");
	return true;
}

// Retail FUN_100b4b20 + FUN_100b5870: one rustbitshot from attachment 0 when
// the flier faces the enemy (dot > 0.9), random cone 0.0262, speed 1000.
bool CRustFlier::FireBolt()
{
	Vector vecSrc, vecAng;
	GetAttachment(0, vecSrc, vecAng);
	if (m_hEnemy == nullptr || !m_hEnemy->IsAlive())
		return false;

	Vector vecDir = (m_vecLastEnemyPos - vecSrc).Normalize();
	UTIL_MakeVectors(pev->angles);
	if (DotProduct(gpGlobals->v_forward.Normalize(), vecDir) <= 0.9)
		return false;

	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;
	float x, y;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
	} while (x * x + y * y > 1.0);

	CBaseEntity* pShot = CBaseEntity::Create("rustbitshot", vecSrc, pev->angles, edict());
	if (pShot)
		pShot->pev->velocity = (vecDir + vecRight * (x * 0.0262f) + vecUp * (y * 0.0262f)) * 1000;
	return true;
}

// Retail CRustFlier::CrashingThink @ 0x100b3f50: only trails 2..4 smoke
// puffs behind the falling flier every 0.1 s (no end state in retail).
void CRustFlier::CrashingThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	Vector vecForward, vecRight;
	UTIL_MakeVectors(pev->angles);
	vecForward = gpGlobals->v_forward;
	vecRight = gpGlobals->v_right;

	for (int i = RANDOM_LONG(2, 4); i > 0; i--)
	{
		float flRight = RANDOM_LONG(0, 128);
		float flBack = RANDOM_LONG(0, 200);
		Vector vecPos = pev->origin - vecForward * flBack - vecRight * flRight;

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecPos);
		WRITE_BYTE(TE_SMOKE);
		WRITE_COORD(vecPos.x);
		WRITE_COORD(vecPos.y);
		WRITE_COORD(vecPos.z);
		WRITE_SHORT(g_sModelIndexSmoke);
		WRITE_BYTE(RANDOM_LONG(0, 9) + 20);
		WRITE_BYTE(RANDOM_LONG(8, 16));
		MESSAGE_END();
	}
}
