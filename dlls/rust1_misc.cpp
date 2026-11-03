//=========================================================
// rust1's remaining classes with no SDK precedent: ammo_dmlsingle
// (CDMLSingleAmmo), monster_rustbit (CRustbit) and monster_beak
// (CBeak). Also adds rustbitshot (CRustbitShot, monster_rustbit's
// long-open fireball projectile, added 2026-09-05 during the
// open-items audit follow-up - see its own header comment below for
// full details).
//
// Decompiled fresh from gunman.dll this session:
//   ammo_dmlsingle: LINK @0x100847b0, vtable @0x100fb070, Spawn
//     @0x10084810, Precache @0x10084840 -> shared CBasePlayerAmmo
//     spawn tail @0x100633c0 (confirms it's actually a
//     CBasePlayerAmmo, not a plain CItem as
//     findings/entity_review_list.csv's summary said - uses
//     CBasePlayerAmmo::GlowThink/DefaultTouch directly). Model
//     "models/singlerocket.mdl", pickup sound "items/9mmclip1.wav".
//   monster_rustbit: LINK @0x100b6a10, vtable @0x10101934 (matches
//     findings/entities/rustbot_family.md's independently-found
//     vtable value exactly), Spawn @0x100b7750, Precache @0x100b7920,
//     Classify @0x100b6a70 (constant 8 = CLASS_ALIEN_PREY),
//     HandleAnimEvent @0x100b71c0 (vtable slot 58). Confirmed bbox
//     (-16,-16,0)/(16,16,42), SOLID_SLIDEBOX, cooldown field
//     RANDOM(-10,10)+80 (matches the doc's "Zufalls-Cooldown-Feld"
//     note). HandleAnimEvent's real structure, freshly decompiled
//     rather than assumed from the doc: events 1/3 and 2/4 maintain a
//     persistent hand-mounted dynamic light + glow sprite (left/right
//     hand respectively) - NOT a ranged fireball throw as an older
//     findings summary line suggested; events 5/6 are in fact an
//     80-unit alternating left/right melee claw traceline with random
//     +-10 unit jitter and direct TakeDamage - this session's fresh
//     decompile corrects that specific point in the older doc rather
//     than reproducing it.
//   monster_beak: LINK @0x100c04a0, vtable @0x10102ad4, Spawn
//     @0x100c1af0, Precache @0x100c1a20, Classify @0x100c04f0
//     (constant 0xb = 11 = CLASS_PLAYER_ALLY - confirmed literally by
//     the decompile and kept as-is per "Code ist Wahrheit", even
//     though findings/entity_review_list.csv describes it purely as
//     a leaping melee attacker with no mention of being allied).
//     Confirmed bbox (-16,-16,0)/(16,16,40), SOLID_SLIDEBOX, matches
//     the FGD's documented 32x32x40 footprint.
//
// Simplified relative to the original (documented per-case):
// - CRustbit: events 1-4 fire rustbitshot through the recovered Retail
//   helper, play RapidFire audio, and maintain their corresponding hand
//   glow sprite. Save/Restore links and death cleanup are reproduced.
//   The confirmed melee claw attack (events 5/6) uses the retail 80-unit
//   hull trace, but its exact skill-scaled damage remains unresolved.
//   findings/entities/rustbot_family.md's "GetSchedule/GetScheduleOfType
//   waehlen ueber Distanz-/Zustandsfelder zwischen Melee-/
//   Fernkampf-Schedules" custom AI is not reproduced - default
//   CBaseMonster scheduling is used instead (same simplification
//   level as every other monster in this project).
// - CBeak: findings/entity_review_list.csv documents a full custom
//   Schedule_t/Task_t leap-attack system (StartTask case 100 = a
//   gravity-compensated jump-to-enemy + LeapTouch, shared logic with
//   CXenome::LeapTouch) across 20 decompiled functions - not
//   reproduced this session (same "no custom schedule tables"
//   simplification used throughout this project); relies on default
//   CBaseMonster melee capability/scheduling instead.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "effects.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"

//=========================================================
// ammo_dmlsingle - CDMLSingleAmmo.
//=========================================================
class CDMLSingleAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/singlerocket.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PrecacheModel("models/singlerocket.mdl");
		PrecacheSound("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		// Retail FUN_10084860 first makes the companion MULE-pack weapon
		// available, then grants one unit from the shared missiles pool.
		if (pOther->IsPlayer())
		{
			auto* pPlayer = static_cast<CBasePlayer*>(pOther);
			if (!pPlayer->HasNamedPlayerItem("weapon_dmlGrenade"))
				pPlayer->GiveNamedItem("weapon_dmlGrenade");
		}

		if (pOther->GiveAmmo(1, "missiles", DML_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_dmlsingle, CDMLSingleAmmo);

//=========================================================
// rustbitshot - CRustbitShot. A confirmed but previously
// unimplemented hitscan-style projectile for monster_rustbit/
// monster_rustbot (findings/entities/rustbot_family.md's
// Session-37/38 find, ShotTouch decompiled in Session 39, but its
// real spawn/calling site was never located across multiple prior
// sessions - see findings/open_items_audit_2026-09-05.md).
//
// Decompiled fresh this session: LINK @0x100b5680, vtable
// @0x10101530, Spawn @0x100b5760, Precache @0x100b5730, ShotTouch
// @0x100b5af0 (already named in the binary's own symbol table).
// Confirmed: sprite "sprites/bitboltsmall.spr", bbox
// (-1,-1,-1)/(1,1,1), MOVETYPE_FLY, SOLID_BBOX, kRenderGlow-style
// rendermode, takedamage=DAMAGE_NO. ShotTouch confirmed: ignores
// same-team/self hits, plays a world-impact sound
// ("RustBit_Projectile2.wav") plus a bullet-hole decal on a static
// hit, or deals skill-scaled damage via TakeDamage on a living target
// (playing "RustBit_Projectile1.wav" on a successful hit,
// "_Projectile2.wav" otherwise) - standard hitscan-projectile
// behavior, no special effects, matching the doc's own conclusion
// exactly.
//
// Where the shot is launched was re-checked against the Retail event
// handler: events 1/3 call FUN_100b6ff0 with attachment index 0, events
// 2/4 with index 1. That helper calls FUN_100b5870, which creates and
// spawns the CRustbitShot vtable (0x10101530). Thus each of the four
// animation events launches a shot; the SDK now follows that event timing.
// The Retail speed and skill-dependent spread progression are now ported.
// The additional random aim offset from FUN_1002d200 remains a documented
// difference; current targeting uses BodyTarget plus the firing spread.
//=========================================================
class CRustbitShot : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT ShotTouch(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(rustbitshot, CRustbitShot);

void CRustbitShot::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("rustbitshot");
	SET_MODEL(ENT(pev), "sprites/bitboltsmall.spr");
	UTIL_SetSize(pev, Vector(-1, -1, -1), Vector(1, 1, 1));

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderGlow;
	pev->renderamt = 255;
	pev->takedamage = DAMAGE_NO;

	SetTouch(&CRustbitShot::ShotTouch);

	pev->nextthink = gpGlobals->time + 3.0;
	SetThink(&CRustbitShot::SUB_Remove);
}

void CRustbitShot::Precache()
{
	PrecacheModel("sprites/bitboltsmall.spr");
	PrecacheSound("rustbit/RustBit_Projectile1.wav");
	PrecacheSound("rustbit/RustBit_Projectile2.wav");
}

void CRustbitShot::ShotTouch(CBaseEntity* pOther)
{
	if (pOther->pev->owner == pev->owner)
		return;

	if (0 != pOther->pev->takedamage)
	{
		pOther->TakeDamage(pev, VARS(pev->owner), gSkillData.rustbitShotDamage, DMG_BULLET);
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "rustbit/RustBit_Projectile1.wav", 1.0, ATTN_NORM);
	}
	else
	{
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "rustbit/RustBit_Projectile2.wav", 1.0, ATTN_NORM);
	}

	UTIL_Remove(this);
}

//=========================================================
// monster_rustbit - CRustbit.
//=========================================================
#define RUSTBIT_AE_FIRE_RIGHT_1 1
#define RUSTBIT_AE_FIRE_LEFT_1 2
#define RUSTBIT_AE_FIRE_RIGHT_2 3
#define RUSTBIT_AE_FIRE_LEFT_2 4
#define RUSTBIT_AE_CLAW_LEFT 5
#define RUSTBIT_AE_CLAW_RIGHT 6

// Retail CRustbit schedule (table entry at 0x1012bf90).
// Task IDs 0x08/0x12/0x59 map to GET_PATH_TO_ENEMY, RUN_PATH, and
// WAIT_FOR_MOVEMENT in the shared GoldSrc task list.
static Task_t tlRustbitHunt[] =
{
	{TASK_GET_PATH_TO_ENEMY, 0},
	{TASK_RUN_PATH, 0},
	{TASK_WAIT_FOR_MOVEMENT, 0},
};

static Schedule_t slRustbitHunt =
{
	tlRustbitHunt,
	ARRAYSIZE(tlRustbitHunt),
	bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND,
	bits_SOUND_DANGER,
	"RustbitHunt",
};

// Retail CRustbit schedules (table entries at 0x1012bfd8/0x1012c050).
// Their arrays use shared GoldSrc tasks.
static Task_t tlRustbitRange1[] =
{
	{TASK_WAIT_FOR_MOVEMENT, 0},
	{TASK_FACE_ROUTE, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_RANGE_ATTACK2, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_RANGE_ATTACK2, 0},
};

static Schedule_t slRustbitRange1 =
{
	tlRustbitRange1,
	ARRAYSIZE(tlRustbitRange1),
	bits_COND_NEW_ENEMY | bits_COND_CAN_MELEE_ATTACK1,
	0,
	"Rustbit Range1",
};

static Task_t tlRustbitRange2[] =
{
	{TASK_WAIT_FOR_MOVEMENT, 0},
	{TASK_FACE_ROUTE, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_RANGE_ATTACK2, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_RANGE_ATTACK2, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_RANGE_ATTACK2, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_RANGE_ATTACK2, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_RANGE_ATTACK1, 0},
};

static Schedule_t slRustbitRange2 =
{
	tlRustbitRange2,
	ARRAYSIZE(tlRustbitRange2),
	bits_COND_NEW_ENEMY | bits_COND_CAN_MELEE_ATTACK1,
	0,
	"Rustbit Range2",
};

// Retail RustbitExposed (type 0x2e, record 0x1012be18): stop, then
// return to the standard take-cover-from-enemy schedule.
static Task_t tlRustbitExposed[] =
{
	{TASK_STOP_MOVING, 0},
	{TASK_SET_SCHEDULE, SCHED_TAKE_COVER_FROM_ENEMY},
};

static Schedule_t slRustbitExposed =
{
	tlRustbitExposed,
	ARRAYSIZE(tlRustbitExposed),
	bits_COND_SEE_DISLIKE | bits_COND_SEE_ENEMY,
	0,
	"RustbitExposed",
};

// Retail RustbitHide (type 0x08, combat state only).
static Task_t tlRustbitHide[] =
{
	{TASK_STOP_MOVING, 0},
	{0x5f, 0}, // Retail task: set ACT_IDLE and complete.
	{TASK_WAIT, 2},
	{TASK_SET_SCHEDULE, SCHED_CHASE_ENEMY},
};

static Schedule_t slRustbitHide =
{
	tlRustbitHide,
	ARRAYSIZE(tlRustbitHide),
	bits_COND_SEE_FEAR | bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE | bits_COND_PROVOKED | bits_COND_NEW_ENEMY |
		bits_COND_HEAR_SOUND,
	bits_SOUND_DANGER,
	"RustbitHide",
};

// Retail RustbitFail (type 0x29, combat state only).
static Task_t tlRustbitFail[] =
{
	{TASK_STOP_MOVING, 0},
	{0x5f, 0},
	{TASK_WAIT_FACE_ENEMY, 2},
	{TASK_SET_SCHEDULE, SCHED_CHASE_ENEMY},
};

static Schedule_t slRustbitFail =
{
	tlRustbitFail,
	ARRAYSIZE(tlRustbitFail),
	bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE |
		bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_PROVOKED,
	bits_SOUND_DANGER | bits_SOUND_PLAYER,
	"RustbitFail",
};

// Retail health-selected SCHED_TAKE_COVER_FROM_ENEMY variants.
static Task_t tlRustbitTakeCoverHealthy[] =
{
	{TASK_STOP_MOVING, 0},
	{TASK_WAIT, 0.2f},
	{TASK_SET_FAIL_SCHEDULE, 47},
	{TASK_FIND_COVER_FROM_ENEMY, 0},
	{TASK_RUN_PATH, 0},
	{TASK_WAIT_FOR_MOVEMENT, 0},
	{TASK_REMEMBER, 2},
	{TASK_FACE_ROUTE, 0},
};

static Schedule_t slRustbitTakeCoverHealthy =
{
	tlRustbitTakeCoverHealthy,
	ARRAYSIZE(tlRustbitTakeCoverHealthy),
	bits_COND_NEW_ENEMY | bits_COND_CAN_MELEE_ATTACK1,
	0,
	"RustbitTakeCoverFromEnemy",
};

static Task_t tlRustbitTakeCoverWounded[] =
{
	{TASK_STOP_MOVING, 0},
	{TASK_WAIT, 0.2f},
	{TASK_FACE_ROUTE, 0},
	{TASK_RANGE_ATTACK2, 0},
	{TASK_SET_FAIL_SCHEDULE, 47},
	{TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, 384},
	{TASK_RUN_PATH, 0},
	{TASK_WAIT_FOR_MOVEMENT, 0},
	{TASK_REMEMBER, 2},
	{TASK_FACE_ROUTE, 0},
};

static Schedule_t slRustbitTakeCoverWounded =
{
	tlRustbitTakeCoverWounded,
	ARRAYSIZE(tlRustbitTakeCoverWounded),
	bits_COND_NEW_ENEMY | bits_COND_CAN_MELEE_ATTACK2,
	0,
	"RustbitTakeCoverFromEnemy2",
};

static Task_t tlRustbitTakeCoverFromBestSound[] =
{
	{TASK_SET_FAIL_SCHEDULE, 47},
	{TASK_STOP_MOVING, 0},
	{TASK_FIND_COVER_FROM_BEST_SOUND, 0},
	{TASK_RUN_PATH, 0},
	{TASK_WAIT_FOR_MOVEMENT, 0},
	{TASK_REMEMBER, 2},
	{TASK_TURN_RIGHT, 179},
};

static Schedule_t slRustbitTakeCoverFromBestSound =
{
	tlRustbitTakeCoverFromBestSound,
	ARRAYSIZE(tlRustbitTakeCoverFromBestSound),
	bits_COND_LIGHT_DAMAGE,
	0,
	"RustbitTakeCoverFromBestSound",
};

class CRustbit : public CBaseMonster
{
public:
	int MonsterProfile() override { return 12; } // retail slot 59
	int IRelationship(CBaseEntity* pTarget) override
	{
		// User-requested gameplay exception: Retail ignores class-13 Xenomes
		// while they spawn invisible, so Rustbits never acquire them. Keep the
		// exception scoped to Xenomes; all other relationships use Retail data.
		if (pTarget && FClassnameIs(pTarget->pev, "monster_xenome"))
			return R_DL;
		return CBaseMonster::IRelationship(pTarget);
	}
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void Spawn() override;
	void Precache() override;
	void PrescheduleThink() override;
	Schedule_t* GetSchedule() override;
	Schedule_t* GetScheduleOfType(int Type) override;
	int Classify() override { return CLASS_ALIEN_PREY; }
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void GibMonster() override;
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this - see
	// findings/entities and the
	// feedback-setactivity-act-idle-stomps-scripted-sequence memory
	// note (same fix as CFriendlyGunman etc; covers CRustbitFriendly
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
	void StartTask(Task_t* pTask) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;

private:
	void Claw();
	void PlayRapidFireSound();
	void UpdateShotDeviation();
	void LaunchShot(int attachment);
	void EnsureHandGlow(bool rightHand);
	void RemoveHandGlows();
	CSprite* m_pRightGlow = nullptr;
	CSprite* m_pLeftGlow = nullptr;
	float m_flLastEnemyVisible = 0; // Retail +0x29c, updated while SEE_ENEMY.
	float m_flLastShot = 0; // Retail +0x2bc.
	float m_flDiviation = 0; // Retail spelling/field +0x2c0.
	int m_iScheduleCoverCount = 0; // Retail +0x2c8.
	int m_iVoicePitch = 80; // Retail +0x2c4; randomized to 70..90 in Spawn.
};
LINK_ENTITY_TO_CLASS(monster_rustbit, CRustbit);

TYPEDESCRIPTION CRustbit::m_SaveData[] =
{
	DEFINE_FIELD(CRustbit, m_pRightGlow, FIELD_CLASSPTR),
	DEFINE_FIELD(CRustbit, m_pLeftGlow, FIELD_CLASSPTR),
	DEFINE_FIELD(CRustbit, m_flLastEnemyVisible, FIELD_TIME),
	DEFINE_FIELD(CRustbit, m_flLastShot, FIELD_TIME),
	DEFINE_FIELD(CRustbit, m_flDiviation, FIELD_FLOAT),
	DEFINE_FIELD(CRustbit, m_iVoicePitch, FIELD_INTEGER),
};
IMPLEMENT_SAVERESTORE(CRustbit, CBaseMonster);

Schedule_t* CRustbit::GetSchedule()
{
	if (m_MonsterState == MONSTERSTATE_COMBAT)
	{
		if (HasConditions(bits_COND_ENEMY_DEAD))
			return CBaseMonster::GetSchedule();

		if (HasConditions(bits_COND_NEW_ENEMY))
			return GetScheduleOfType(SCHED_WAKE_ANGRY);

		if (HasConditions(bits_COND_HEAR_SOUND))
		{
			CSound* pSound = PBestSound();
			if (pSound && (pSound->m_iType & bits_SOUND_DANGER))
			{
				++m_iScheduleCoverCount;
				return GetScheduleOfType(SCHED_TAKE_COVER_FROM_BEST_SOUND);
			}
		}

		if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
			return GetScheduleOfType(SCHED_MELEE_ATTACK1);

		if (HasConditions(bits_COND_LIGHT_DAMAGE) &&
			(m_afMemory & bits_MEMORY_FLINCHED) == 0)
		{
			m_afMemory |= bits_MEMORY_FLINCHED;
			++m_iScheduleCoverCount;
			return GetScheduleOfType(SCHED_SMALL_FLINCH);
		}

		if (!m_hTargetEnt &&
			HasConditions(bits_COND_SEE_ENEMY | bits_COND_ENEMY_FACING_ME) &&
			m_iScheduleCoverCount < 3)
		{
			++m_iScheduleCoverCount;
			return GetScheduleOfType(0x2e); // Retail RustbitExposed.
		}

		if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
		{
			if (m_iScheduleCoverCount > 3)
			{
				m_iScheduleCoverCount = 0;
				return GetScheduleOfType(SCHED_RANGE_ATTACK1);
			}

			// With no mapper target, Retail FUN_10051fc0 returns true. Its
			// target-bound memory-mask behavior remains below as an open item.
			if (!m_hTargetEnt)
			{
				m_iScheduleCoverCount = 0;
				return GetScheduleOfType(SCHED_RANGE_ATTACK1);
			}
		}

		if (HasConditions(bits_COND_SEE_ENEMY))
			return GetScheduleOfType(SCHED_COMBAT_FACE);

		// With no target entity, Retail FUN_10051fc0(this, 0x18) succeeds.
		if (!m_hTargetEnt)
			return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ENEMY);
	}

	if (m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT)
	{
		if (HasConditions(bits_COND_HEAR_SOUND))
		{
			CSound* pSound = PBestSound();
			if (pSound)
			{
				if (pSound->m_iType & bits_SOUND_DANGER)
					return GetScheduleOfType(SCHED_TAKE_COVER_FROM_BEST_SOUND);

				if (pSound->m_iType & bits_SOUND_COMBAT)
					return GetScheduleOfType(SCHED_INVESTIGATE_SOUND);
			}
		}
	}

	return CBaseMonster::GetSchedule();
}

Schedule_t* CRustbit::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case 0x08: // Retail RustbitHide, only in combat state.
		if (m_MonsterState == MONSTERSTATE_COMBAT)
			return &slRustbitHide;
		break;
	case SCHED_CHASE_ENEMY:
		return &slRustbitHunt;
	case SCHED_TAKE_COVER_FROM_ENEMY:
		return pev->health > 30.0f ? &slRustbitTakeCoverHealthy : &slRustbitTakeCoverWounded;
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		return &slRustbitTakeCoverFromBestSound;
	case SCHED_RANGE_ATTACK1:
		return &slRustbitRange1;
	case SCHED_RANGE_ATTACK2:
		return &slRustbitRange2;
	case 0x2e: // Retail RustbitExposed
		return &slRustbitExposed;
	case 0x2f: // Retail fail target: recent sight -> RustbitFail, else Range1.
		if (gpGlobals->time - m_flLastEnemyVisible <= 10.0f)
			return &slRustbitFail;
		return &slRustbitRange1;
	case 0x29: // Retail RustbitFail, only in combat state.
		if (m_MonsterState == MONSTERSTATE_COMBAT)
			return &slRustbitFail;
		break;
	default:
		return CBaseMonster::GetScheduleOfType(Type);
	}
	return CBaseMonster::GetScheduleOfType(Type);
}

void CRustbit::PrescheduleThink()
{
	// Retail PrescheduleThink @0x100b7950 writes +0x29c whenever an enemy is
	// currently visible. That timestamp selects the cover failure schedule.
	if (m_hEnemy && HasConditions(bits_COND_SEE_ENEMY))
		m_flLastEnemyVisible = gpGlobals->time;

	// Retail also fades the hand glows by four alpha units per call and hides
	// them once fully transparent. Animation events restore brightness to 200.
	auto FadeGlow = [](CSprite* pGlow)
	{
		if (!pGlow)
			return;

		if (pGlow->pev->renderamt > 4.0f)
			pGlow->pev->renderamt -= 4.0f;
		else
			pGlow->pev->renderamt = 0.0f;
		if (pGlow->pev->renderamt == 0.0f)
			pGlow->pev->effects |= EF_NODRAW;
		else
			pGlow->pev->effects &= ~EF_NODRAW;
	};
	FadeGlow(m_pRightGlow);
	FadeGlow(m_pLeftGlow);

	CBaseMonster::PrescheduleThink();
}

void CRustbit::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/rustbit.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 42));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	// Retail Spawn @0x100b7750: Health per FUN_100608d0 (Skill-Basis + Zufallsanteil).
	pev->health = gSkillData.rustbitHealth + RANDOM_FLOAT(0, gSkillData.rustbitHealth) * (gSkillData.percentRandomHealth * 0.01f);
	pev->view_ofs = Vector(0, 0, 40);
	m_iVoicePitch = RANDOM_LONG(-10, 10) + 80;

	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK1 | bits_CAP_MELEE_ATTACK1;

	MonsterInit();
		// Retail CRustbit::Spawn @ 0x100b7750 restores this after MonsterInit().
	pev->view_ofs = Vector(0, 0, 40);
}

void CRustbit::Precache()
{
	PrecacheModel("models/rustbit.mdl");
	PrecacheModel("sprites/rustbitglow.spr");
	PrecacheSound("rustbit/Rustbit_RapidFire_FirstShot_F0.wav");
	PrecacheSound("rustbit/Rustbit_RapidFire_SecondShot_F1.wav");

	UTIL_PrecacheOther("rustbitshot");
}

void CRustbit::StartTask(Task_t* pTask)
{
	// The Retail Range1/Range2 schedules put FACE_ROUTE immediately before
	// firing. On an isolated Rustbit with a visible enemy but no existing
	// movement route, Retail's base task fails here and the attack never starts.
		// Preserve route-facing when a path exists; otherwise face the known enemy
		// and let the base task complete the turn. This is an intentional gameplay fix.
	if (pTask->iTask == TASK_FACE_ROUTE && FRouteClear() && m_hEnemy)
	{
		MakeIdealYaw(m_vecEnemyLKP);
		SetTurnActivity();
		return;
	}

	CBaseMonster::StartTask(pTask);
}

void CRustbit::LaunchShot(int attachment)
{
	CBaseMonster* pEnemy = m_hEnemy ? m_hEnemy->MyMonsterPointer() : nullptr;
	if (!pEnemy)
		return;

	Vector vecSource;
	Vector vecAttachmentAngles;
	GetAttachment(attachment, vecSource, vecAttachmentAngles);

	Vector vecDirection;
	if (pEnemy->IsAlive() && pEnemy->m_hTargetEnt)
	{
		// Retail FUN_100b6ff0 calls FUN_1002d200 before applying deviation.
		// For a live enemy with a target entity, that helper aims from the
		// attachment at BodyTarget and adds its bounded per-axis jitter.
		vecDirection = pEnemy->BodyTarget(vecSource) - vecSource;
		vecDirection.x += RANDOM_LONG(-32, 32);
		vecDirection.y += RANDOM_LONG(-32, 32);
		vecDirection.z += RANDOM_LONG(0, 64);
		vecDirection = vecDirection.Normalize();
	}
	else
	{
		// Without a live target-entity handle, Retail aims using the target's
		// BodyTarget offset from its origin plus this monster's enemy LKP.
		const Vector vecBodyOffset = pEnemy->BodyTarget(vecSource) - pEnemy->pev->origin;
		vecDirection = (m_vecEnemyLKP + vecBodyOffset - vecSource).Normalize();
	}
	UTIL_MakeVectors(pev->angles);
	float x;
	float y;
	do
	{
		x = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
		y = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
	} while (x * x + y * y > 1.0f);
	vecDirection = vecDirection + gpGlobals->v_right * (x * m_flDiviation);
	vecDirection = vecDirection + gpGlobals->v_forward * (y * m_flDiviation * 0.8333333f);
	CBaseEntity* pShot = CBaseEntity::Create("rustbitshot", vecSource, UTIL_VecToAngles(vecDirection), edict());
	if (!pShot)
		return;

	// Retail passes the 1000-unit range constant to the CRustbitShot factory.
	pShot->pev->velocity = vecDirection * 1000.0f;
	pShot->pev->owner = edict();
}

void CRustbit::UpdateShotDeviation()
{
	const bool firedRecently = gpGlobals->time <= m_flLastShot + 0.5f;
	if (firedRecently)
	{
		if (g_iSkillLevel == SKILL_MEDIUM)
			m_flDiviation -= 0.02f;
		else if (g_iSkillLevel == SKILL_HARD)
			m_flDiviation -= 0.03f;
		else
			m_flDiviation -= 0.01f;
	}
	else
	{
		if (g_iSkillLevel == SKILL_MEDIUM)
			m_flDiviation += 0.05f;
		else if (g_iSkillLevel == SKILL_HARD)
			m_flDiviation += 0.03f;
		else
			m_flDiviation += 0.1f;
	}

	m_flDiviation = V_max(0.01f, V_min(0.15f, m_flDiviation));
	m_flLastShot = gpGlobals->time;
}

void CRustbit::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case RUSTBIT_AE_CLAW_LEFT:
	case RUSTBIT_AE_CLAW_RIGHT:
		Claw();
		break;
	case RUSTBIT_AE_FIRE_RIGHT_1:
	case RUSTBIT_AE_FIRE_RIGHT_2:
		if (m_hEnemy)
		{
			UpdateShotDeviation();
			LaunchShot(1); // Retail attachment index 0; SDK GetAttachment is 1-based.
			PlayRapidFireSound();
		}
		EnsureHandGlow(true); // Retail writes the right-hand pointer at +0x2d4.
		break;
	case RUSTBIT_AE_FIRE_LEFT_1:
	case RUSTBIT_AE_FIRE_LEFT_2:
		if (m_hEnemy)
		{
			UpdateShotDeviation();
			LaunchShot(2); // Retail attachment index 1; SDK GetAttachment is 1-based.
			PlayRapidFireSound();
		}
		EnsureHandGlow(false);
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CRustbit::PlayRapidFireSound()
{
	const char* sample = RANDOM_LONG(0, 1) == 0
		? "rustbit/Rustbit_RapidFire_SecondShot_F1.wav"
		: "rustbit/Rustbit_RapidFire_FirstShot_F0.wav";
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, sample, 0.7f, 0.8f, 0, m_iVoicePitch);
}

void CRustbit::EnsureHandGlow(bool rightHand)
{
	CSprite*& pGlow = rightHand ? m_pRightGlow : m_pLeftGlow;
	if (!pGlow)
	{
		pGlow = CSprite::SpriteCreate("sprites/rustbitglow.spr", pev->origin, true);
		pGlow->SetTransparency(kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation);
		pGlow->SetScale(0.5f);
		// Retail binds the right hand to attachment 1 and the left to 2.
		pGlow->SetAttachment(edict(), rightHand ? 1 : 2);
	}
	else
	{
		pGlow->SetBrightness(200);
	}
	// PrescheduleThink hides faded glows with EF_NODRAW. A new animation
	// event reactivates that existing sprite and must make it visible again.
	pGlow->pev->effects &= ~EF_NODRAW;
}

void CRustbit::Claw()
{
	CBaseEntity* pHurt = CheckTraceHullAttack(80.0f, gSkillData.rustbitClawDamage, DMG_SLASH);
	if (pHurt)
	{
		pHurt->pev->punchangle.x = RANDOM_LONG(-10, 10);
		pHurt->pev->punchangle.z = RANDOM_LONG(-10, 10);
		UTIL_MakeVectors(pev->angles);
		pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_forward * 100.0f;
	}
}

void CRustbit::RemoveHandGlows()
{
	if (m_pRightGlow)
		UTIL_Remove(m_pRightGlow);
	if (m_pLeftGlow)
		UTIL_Remove(m_pLeftGlow);
	m_pRightGlow = nullptr;
	m_pLeftGlow = nullptr;
}

void CRustbit::Killed(entvars_t* pevAttacker, int iGib)
{
	RemoveHandGlows();
	CBaseMonster::Killed(pevAttacker, iGib);
}

void CRustbit::GibMonster()
{
	RemoveHandGlows();
	CBaseMonster::GibMonster();
}

//=========================================================
// monster_rustbit_friendly - CRustbitFriendly (RUST7B+). Confirmed
// fresh this session (LINK @0x100b7da0, vtable @0x10101b70): Spawn
// and Precache are the exact same addresses as monster_rustbit's own
// (0x100b7750/0x100b7920) - a true shared-vtable variant, not a
// separate implementation. Classify (slot 8, @0x100b7e20) is the
// only override and returns the literal constant 3
// (CLASS_HUMAN_PASSIVE) - confirms
// findings/entities/gunner_family.md's "einziger Unterschied ist
// Classify()-Override" finding exactly.
//=========================================================
class CRustbitFriendly : public CRustbit
{
public:
	int MonsterProfile() override { return 12; } // retail slot 59
	int Classify() override { return CLASS_HUMAN_PASSIVE; }
};
LINK_ENTITY_TO_CLASS(monster_rustbit_friendly, CRustbitFriendly);

//=========================================================
// monster_beak - CBeak.
//=========================================================
class CBeak : public CBaseMonster
{
public:
	int MonsterProfile() override { return 15; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int Classify() override { return CLASS_PLAYER_ALLY; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	// Retail slot 101 (@0x100c0730): 90 for idle/walk/turns, 120 for run, 30 for leap, 0 for crouch-idle, else 120.
	void SetYawSpeed() override;
	// Retail TakeDamage filter @0x100c07e0: damage from other monster_beak is ignored.
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	// Retail @0x100c0c20 / @0x100c0c80.
	bool CheckMeleeAttack1(float flDot, float flDist) override;
	bool CheckMeleeAttack2(float flDot, float flDist) override;
	// Retail @0x100c0ba0 (events 4..7, 9 -> @0x100c09c0) and CBeak::LeapTouch @0x100c0860.
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void EXPORT LeapTouch(CBaseEntity* pOther);
	// Retail StartTask @0x100c0d70, RunTask @0x100c1520, GetScheduleOfType @0x100c18d0, GetSchedule @0x100c1750.
	void StartTask(Task_t* pTask) override;
	void RunTask(Task_t* pTask) override;
	Schedule_t* GetScheduleOfType(int Type) override;
	Schedule_t* GetSchedule() override;

private:
	float m_flLeapReady = 0; // Retail +0x2b4; set to "now" in Spawn, time + 3 after a leap
	float m_flVoiceTime = 0; // Retail +0x2b0, roar/eat sound cooldown
	bool m_bHungry = true;	 // Retail +0x2ac
	int m_iVoicePitch = 60;	 // Retail +0x2a8 = 60 - (int)bite damage
	EHANDLE m_hFood;		 // Retail +0x270 (no writer found; stays empty)
	void PlayVoice(int iSound);
	void Melee(float flReach, float flDamage);

public:
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
};
LINK_ENTITY_TO_CLASS(monster_beak, CBeak);

// Retail's three confirmed Beak save descriptors map to the existing timers
// and voice-pitch state. The other three Retail descriptors remain unmapped.
TYPEDESCRIPTION CBeak::m_SaveData[] = {
	DEFINE_FIELD(CBeak, m_flLeapReady, FIELD_TIME),
	DEFINE_FIELD(CBeak, m_flVoiceTime, FIELD_TIME),
	DEFINE_FIELD(CBeak, m_iVoicePitch, FIELD_INTEGER),
};
IMPLEMENT_SAVERESTORE(CBeak, CBaseMonster);

void CBeak::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/beak.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 40));

	pev->solid = SOLID_SLIDEBOX;
	pev->health = gSkillData.beakHealth + RANDOM_FLOAT(0, gSkillData.beakHealth) * (gSkillData.percentRandomHealth * 0.01f);
	m_flLeapReady = gpGlobals->time;
	m_flVoiceTime = gpGlobals->time;
	m_bHungry = true;
	m_iVoicePitch = 60 - (int)gSkillData.beakBiteDamage;
	pev->view_ofs = Vector(0, 0, 40);

	m_afCapability |= bits_CAP_MELEE_ATTACK1;

	MonsterInit();
	// Retail CBeak::Spawn @ 0x100c1af0 restores this after MonsterInit().
	pev->view_ofs = Vector(0, 0, 40);
}

void CBeak::SetYawSpeed()
{
	switch (m_Activity)
	{
	case ACT_IDLE:
	case ACT_WALK:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		pev->yaw_speed = 90;
		break;
	case ACT_LEAP:
		pev->yaw_speed = 30;
		break;
	case ACT_CROUCHIDLE:
		pev->yaw_speed = 0;
		break;
	default: // includes ACT_RUN
		pev->yaw_speed = 120;
		break;
	}
}

bool CBeak::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (pevAttacker && FStrEq(STRING(pevAttacker->classname), "monster_beak"))
		return false;
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

bool CBeak::CheckMeleeAttack1(float flDot, float flDist)
{
	// The enemy must not be standing on the beak.
	if (m_IdealActivity != ACT_MELEE_ATTACK2 && m_hEnemy != nullptr &&
		m_hEnemy->pev->groundentity != edict() && flDist <= 48)
		return true;
	return false;
}

bool CBeak::CheckMeleeAttack2(float flDot, float flDist)
{
	return m_flLeapReady <= gpGlobals->time && flDist <= 240 && m_hEnemy != nullptr;
}

void CBeak::Melee(float flReach, float flDamage)
{
	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 1024, 0.2f);

	TraceResult trace;
	CBaseEntity* pHurt = CheckTraceHullAttack(flReach, flDamage, DMG_SLASH, &trace);
	if (pHurt)
	{
		// Retail applies a random view punch of +-5 degrees.
		pHurt->pev->punchangle.z = RANDOM_FLOAT(-5, 5);
		pHurt->pev->punchangle.x = RANDOM_FLOAT(-5, 5);

		// Retail calls victim virtual slot 15 (TraceBleed) for monster victims,
		// using the vector between the attacker/victim BodyTarget points.
		if (CBaseMonster* pVictimMonster = pHurt->MyMonsterPointer())
		{
			const Vector vecSource = BodyTarget(pev->origin);
			const Vector vecTarget = pHurt->BodyTarget(pHurt->pev->origin);
			const Vector vecAttackDir = (vecTarget - vecSource).Normalize();
			pVictimMonster->TraceBleed(flDamage, vecAttackDir, &trace, DMG_SLASH);
		}

		// Retail's attacker-vtable +0x224 callback plays the impact vocalization
		// on players/monsters on two of three random outcomes.
		if ((pHurt->pev->flags & (FL_CLIENT | FL_MONSTER)) != 0 && RANDOM_LONG(0, 2) < 2)
		{
			EMIT_SOUND_DYN(ENT(pHurt->pev), CHAN_VOICE, "lg_scorp/lg_scorp_attackb.wav", 0.25f, 2.0f, 0, 60);
		}
	}
}

void CBeak::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	// Events 4, 5, 6 bite (reach 60 / 70 / 70), 7 and 9 scrape (reach 60) and arm the leap touch.
	switch (pEvent->event)
	{
	case 4:
		Melee(60, gSkillData.beakBiteDamage);
		break;
	case 5:
	case 6:
		Melee(70, gSkillData.beakBiteDamage);
		break;
	case 7:
	case 9:
		SetTouch(&CBeak::LeapTouch);
		Melee(60, gSkillData.beakScrapeDamage);
		// Simplification: retail sets this timer (time + 3) when the leap task starts
		// (@0x100c0d70, task 100), whose velocity code is not ported.
		m_flLeapReady = gpGlobals->time + 3.0f;
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CBeak::LeapTouch(CBaseEntity* pOther)
{
	if (pOther->pev->takedamage == DAMAGE_NO)
		return;
	if (pOther->Classify() == Classify() || FBitSet(pev->flags, FL_ONGROUND))
		return;

	pOther->TakeDamage(pev, pev, gSkillData.beakScrapeDamage, DMG_SLASH);
	SetTouch(nullptr);
}


//=========================================================
// Beak AI (retail @0x100c0d70 / 0x100c1520 / 0x100c18d0 / 0x100c1750)
//=========================================================
#define TASK_BEAK_EAT 0x61
#define TASK_BEAK_ROAR 0x62
#define TASK_BEAK_JUMP 0x64
#define TASK_BEAK_GOTO_FOOD 0x65
#define TASK_BEAK_URGENT 0x66
#define SCHED_BEAK_EAT 0x2e
#define SCHED_BEAK_PRETEND_EAT 0x2f

static Task_t tlBeakLookAround[] = {
	{TASK_STOP_MOVING, 0},
	{TASK_PLAY_SEQUENCE, (float)ACT_SPECIAL_ATTACK1}, // retail data 60
	{TASK_FACE_ENEMY, 0},
};
static Task_t tlBeakRoar[] = {
	{TASK_STOP_MOVING, 0},
	{TASK_BEAK_ROAR, 0},
	{TASK_SET_SCHEDULE, (float)SCHED_CHASE_ENEMY},
};
static Task_t tlBeakJumpMelee[] = {
	{TASK_SET_FAIL_SCHEDULE, (float)SCHED_CHASE_ENEMY_FAILED},
	{TASK_FACE_ENEMY, 0},
	{TASK_STOP_MOVING, 0},
	{TASK_BEAK_JUMP, 0},
	{TASK_SET_SCHEDULE, (float)SCHED_CHASE_ENEMY},
};
static Task_t tlBeakFailPvs[] = {
	{TASK_STOP_MOVING, 0},
	{TASK_BEAK_ROAR, 0},
	{TASK_WAIT_FACE_ENEMY, 1},
	{TASK_WAIT_PVS, 0},
};
static Task_t tlBeakFailEnemy[] = {
	{TASK_STOP_MOVING, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_BEAK_ROAR, 0},
	{TASK_FACE_ENEMY, 0},
};
static Task_t tlBeakEat[] = {
	{TASK_SET_FAIL_SCHEDULE, (float)SCHED_BEAK_PRETEND_EAT},
	{TASK_STOP_MOVING, 0},
	{TASK_STORE_LASTPOSITION, 0},
	{TASK_GET_PATH_TO_BESTSCENT, 0},
	{TASK_WALK_PATH, 0},
	{TASK_WAIT_FOR_MOVEMENT, 0},
	{TASK_BEAK_EAT, 0},
};
static Task_t tlBeakPretendEat[] = {
	{TASK_SET_FAIL_SCHEDULE, (float)SCHED_IDLE_STAND},
	{TASK_STOP_MOVING, 0},
	{TASK_STORE_LASTPOSITION, 0},
	{TASK_BEAK_GOTO_FOOD, 50},
	{TASK_BEAK_EAT, 0},
};
static Task_t tlBeakUrgent[] = {
	{TASK_SET_FAIL_SCHEDULE, (float)SCHED_CHASE_ENEMY_FAILED},
	{TASK_BEAK_URGENT, 0},
	{TASK_SET_SCHEDULE, (float)SCHED_TAKE_COVER_FROM_ORIGIN},
};

static Schedule_t slBeakLookAround[] = {{tlBeakLookAround, ARRAYSIZE(tlBeakLookAround), 0x38300, 0x25, "Looking Around"}};
static Schedule_t slBeakRoar[] = {{tlBeakRoar, ARRAYSIZE(tlBeakRoar), 0x300, 0, "Beak Roaring"}};
static Schedule_t slBeakJumpMelee[] = {{tlBeakJumpMelee, ARRAYSIZE(tlBeakJumpMelee), 0, 0, "Beak Secondary Jump-Melee"}};
static Schedule_t slBeakFailPvs[] = {{tlBeakFailPvs, ARRAYSIZE(tlBeakFailPvs), 0x32b00, 0x24, "Failed with PVS"}};
static Schedule_t slBeakFailEnemy[] = {{tlBeakFailEnemy, ARRAYSIZE(tlBeakFailEnemy), 0x12a00, 0x24, "Failed with PVS"}};
static Schedule_t slBeakEat[] = {{tlBeakEat, ARRAYSIZE(tlBeakEat), 0x20300, 0x39, "Eating"}};
static Schedule_t slBeakPretendEat[] = {{tlBeakPretendEat, ARRAYSIZE(tlBeakPretendEat), 0x20300, 0x39, "Pretending to Eat"}};
static Schedule_t slBeakUrgent[] = {{tlBeakUrgent, ARRAYSIZE(tlBeakUrgent), 0, 0, "Urgent"}};

void CBeak::PlayVoice(int iSound)
{
	// Retail @0x100c06f0: table @0x1012d254 = { eating.wav, roar.wav }, channel 2, vol 1.0, attn 0.8.
	static const char* const sounds[] = {"beak/eating.wav", "beak/roar.wav"};
	EMIT_SOUND_DYN(edict(), CHAN_VOICE, sounds[iSound], 1.0f, 0.8f, 0, m_iVoicePitch);
}

void CBeak::StartTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_MELEE_ATTACK1:
		m_iTaskStatus = TASKSTATUS_RUNNING;
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "beak/attack1.wav", 1.0f, 0.8f, 0, m_iVoicePitch);
		CBaseMonster::StartTask(pTask);
		break;

	case TASK_BEAK_EAT:
		m_iTaskStatus = TASKSTATUS_RUNNING;
		MakeIdealYaw(m_vecEnemyLKP);
		ChangeYaw((int)pev->yaw_speed);
		SetActivity(ACT_EAT);
		break;

	case TASK_BEAK_ROAR:
		m_iTaskStatus = TASKSTATUS_RUNNING;
		if (m_flVoiceTime < gpGlobals->time)
		{
			m_IdealActivity = ACT_THREAT_DISPLAY;
			m_flVoiceTime = gpGlobals->time + 9.0f;
			PlayVoice(1);
		}
		else
			TaskFail();
		break;

	case TASK_BEAK_JUMP:
	{
		m_iTaskStatus = TASKSTATUS_RUNNING;
		m_IdealActivity = ACT_MELEE_ATTACK2;
		m_flLeapReady = gpGlobals->time + 3.0f;
		ClearBits(pev->flags, FL_ONGROUND);
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "beak/attack2.wav", 0.7f, 0.8f, 0, m_iVoicePitch);

		const float flGravity = CVAR_GET_FLOAT("sv_gravity");
		if (m_hEnemy != nullptr)
		{
			// Retail aims at the enemy's body target with a lead along the enemy velocity, speed 2.5 x
			// distance capped at 500, plus gravity * 0.625 * 0.3 on z. Simplification: the lead is
			// distance / 500 seconds (the retail float arithmetic of the lead term is not unambiguous).
			const Vector vecTarget = m_hEnemy->BodyTarget(pev->origin);
			const float flDist = (vecTarget - pev->origin).Length();
			Vector vecAim = vecTarget;
			const Vector vecToEnemy = (vecTarget - pev->origin).Normalize();
			const Vector vecEnemyVel = m_hEnemy->pev->velocity;
			const Vector vecVelDir = vecEnemyVel.Length() > 0 ? vecEnemyVel.Normalize() : Vector(1, 0, 0);
			float flLead = flDist / 500.0f;
			if (DotProduct(vecToEnemy, vecVelDir) < 0.4f && flDist > 100)
				flLead = 0;
			vecAim = vecAim + vecEnemyVel * flLead;
			float flSpeed = (vecAim - pev->origin).Length() * 2.5f;
			if (flSpeed > 500.0f)
				flSpeed = 500.0f;
			pev->velocity = (vecAim - pev->origin).Normalize() * flSpeed;
			pev->velocity.z += flGravity * 0.625f * 0.3f;
		}
		else
		{
			UTIL_MakeVectors(pev->angles);
			pev->velocity = gpGlobals->v_forward * 500.0f;
			pev->velocity.z += flGravity * 0.625f * 0.4f;
		}
		break;
	}

	case TASK_BEAK_GOTO_FOOD:
	{
		m_iTaskStatus = TASKSTATUS_RUNNING;
		CBaseEntity* pFood = m_hFood;
		m_hTargetEnt = pFood ? pFood : this;
		break;
	}

	case TASK_BEAK_URGENT:
		m_iTaskStatus = TASKSTATUS_RUNNING;
		pev->ideal_yaw += RANDOM_FLOAT(-20, 20);
		if (HasConditions(bits_COND_SEE_ENEMY))
			TaskFail();
		m_IdealActivity = ACT_LEAP;
		ClearBits(pev->flags, FL_ONGROUND);
		{
			// Retail: random hop along the facing direction (x100) with small random side/up jitter
			// scaled by sv_gravity; exact scaling terms approximated.
			UTIL_MakeVectors(pev->angles);
			const float flGravity = CVAR_GET_FLOAT("sv_gravity");
			Vector vecDir = gpGlobals->v_forward * 100.0f * RANDOM_FLOAT(-1.0f, 1.0f);
			pev->velocity = vecDir;
			pev->velocity.z += RANDOM_FLOAT(0.1f, 0.4f) * flGravity * 0.5f;
		}
		TaskFail(); // retail sets bits_COND_TASK_FAILED on the way out (see 0x100c1329 tail)
		break;

	default:
		CBaseMonster::StartTask(pTask);
		break;
	}
}

void CBeak::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_BEAK_EAT:
		// Never completes by itself; loops the eating sound every 9 s once the sequence finished.
		if (!m_fSequenceFinished)
			return;
		if (gpGlobals->time <= m_flVoiceTime)
			return;
		m_flVoiceTime = gpGlobals->time + 9.0f;
		PlayVoice(0);
		return;

	case TASK_BEAK_ROAR:
		if (!m_fSequenceFinished)
			return;
		break;

	case TASK_BEAK_JUMP:
		// Still airborne (not on the ground and moving): keep running.
		if (!FBitSet(pev->flags, FL_ONGROUND) && pev->velocity.Length() != 0)
			return;
		break;

	case TASK_BEAK_GOTO_FOOD:
		if (m_hTargetEnt == nullptr)
		{
			TaskFail();
			return;
		}
		if (MoveToTarget(ACT_WALK, 2.0f))
			return;
		TaskFail();
		return;

	case TASK_BEAK_URGENT:
		ChangeYaw((int)pev->yaw_speed);
		if (FBitSet(pev->flags, FL_ONGROUND) && !HasConditions(bits_COND_TASK_FAILED))
			m_iTaskStatus = TASKSTATUS_COMPLETE;
		if (!m_fSequenceFinished || HasConditions(bits_COND_TASK_FAILED))
			return;
		m_iTaskStatus = TASKSTATUS_COMPLETE;
		return;

	default:
		CBaseMonster::RunTask(pTask);
		return;
	}

	if (!HasConditions(bits_COND_TASK_FAILED))
		TaskComplete();
}

Schedule_t* CBeak::GetScheduleOfType(int Type)
{
	CBaseEntity* pFood = m_hFood;
	const bool bPretending = pFood == this; // retail compares the +0x270 handle with `this`

	switch (Type)
	{
	case SCHED_IDLE_STAND:
		if (!bPretending)
			break;
		return &slBeakLookAround[0];
	case SCHED_CHASE_ENEMY_FAILED:
		return &slBeakLookAround[0];
	case SCHED_MELEE_ATTACK1:
		if (bPretending && RANDOM_LONG(0, 99) < 5)
			return &slBeakRoar[0];
		break;
	case SCHED_MELEE_ATTACK2:
		return &slBeakJumpMelee[0];
	case SCHED_FAIL:
		if (m_MonsterState != MONSTERSTATE_COMBAT)
			return &slBeakFailPvs[0];
		if (FIND_CLIENT_IN_PVS(edict()) != nullptr)
			return &slBeakFailEnemy[0];
		break;
	case SCHED_BEAK_EAT:
		return &slBeakEat[0];
	case SCHED_BEAK_PRETEND_EAT:
		return &slBeakPretendEat[0];
	}
	return CBaseMonster::GetScheduleOfType(Type);
}

Schedule_t* CBeak::GetSchedule()
{
	if (m_MonsterState == MONSTERSTATE_COMBAT)
	{
		if (!HasConditions(bits_COND_ENEMY_DEAD))
		{
			if (HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
			{
				m_bHungry = false;
				if (RANDOM_LONG(0, 99) < 10)
				{
					if (RANDOM_LONG(0, 99) < 30)
						return GetScheduleOfType(SCHED_SMALL_FLINCH);
					return GetScheduleOfType(SCHED_SMALL_FLINCH); // retail type 0x2c shares the table
				}
			}
			if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
				return GetScheduleOfType(SCHED_MELEE_ATTACK1);
			if (HasConditions(bits_COND_CAN_MELEE_ATTACK2))
				return GetScheduleOfType(SCHED_MELEE_ATTACK2);
		}
		else
			m_bHungry = true;
	}
	else if (m_MonsterState == MONSTERSTATE_ALERT)
	{
		if (HasConditions(bits_COND_SEE_ENEMY | bits_COND_NEW_ENEMY))
			m_IdealMonsterState = MONSTERSTATE_COMBAT;

		if (HasConditions(bits_COND_HEAR_SOUND))
		{
			CSound* pSound = PBestSound();
			if (pSound && (pSound->m_iType & 0x21) != 0)
			{
				m_bHungry = false;
				MakeIdealYaw(pSound->m_vecOrigin);
				return &slBeakUrgent[0];
			}
		}
		if (HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
		{
			m_bHungry = false;
			if (RANDOM_LONG(0, 99) >= 10)
				return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ORIGIN);
			return GetScheduleOfType(SCHED_SMALL_FLINCH); // retail: 30% type 0x11, else type 0x2c (same table)
		}
		if (HasConditions(bits_COND_SMELL_FOOD) && m_bHungry)
			return GetScheduleOfType(SCHED_BEAK_EAT);
	}
	return CBaseMonster::GetSchedule();
}

void CBeak::Precache()
{
	PrecacheModel("models/beak.mdl");

	PrecacheSound("beak/attack1.wav");
	PrecacheSound("beak/attack2.wav");
	PrecacheSound("beak/suffer.wav");
	PrecacheSound("beak/alert1.wav");
	PrecacheSound("beak/pain1.wav");
	PrecacheSound("beak/pain2.wav");
	PrecacheSound("beak/die1.wav");
	PrecacheSound("beak/die2.wav");
	PrecacheSound("beak/eating.wav");
	PrecacheSound("beak/roar.wav");
	PrecacheSound("lg_scorp/lg_scorp_attackb.wav");
	PrecacheModel("sprites/gibbeak.spr");
	PrecacheModel("sprites/gorebeak.spr");
}
