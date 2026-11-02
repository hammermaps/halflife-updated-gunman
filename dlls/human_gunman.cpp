//=========================================================
// monster_human_gunman - CFriendlyGunman. A friendly, talking, armed
// human ally ("Human Gunman Friendly" per the FGD) - Gunman
// Chronicles' equivalent of stock HL's CBarney, built the same way:
// a CTalkMonster subclass using the SDK's existing generic sentence-
// group system (TLK_ANSWER/TLK_QUESTION/etc., see talkmonster.h),
// except Gunman exposes all 19 sentence-group names as map keyvalues
// ("gn_*") instead of hardcoding them per monster type, plus two
// extra groups (gn_kill/gn_attack) the stock TLK_ enum has no slot
// for.
//
// Decompiled from gunman.dll: constructor @0x100ae360 (vtable
// 0x101009f0), Spawn @0x100ae1d0, Precache @0x100ae300, KeyValue
// @0x100ae750 (the field-name-to-offset mapping below was verified
// against the FGD's exact 21-keyvalue list, all present and in
// order), Save @0x100add00 (class name literal "CFriendlyGunman"),
// Classify @0x100adda0 (constant 3 = CLASS_HUMAN_PASSIVE, matching
// the FGD title "Human Gunman Friendly").
//
// Retail comparison coverage includes the custom idle/follow schedules,
// range-attack cache, hit/damage reactions, gun-drop, speech hooks and the
// monstershotgun.sc playback event. The Gunman-only gn_kill and gn_attack
// sentence groups are consumed by GetSchedule and AlertSound respectively.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "talkmonster.h"
#include "schedule.h"
#include "defaultai.h"
#include "weapons.h"
#include "soundent.h"

#define GUNMAN_AE_SHOOT 1

// Retail schedules from CFriendlyGunman::GetScheduleOfType @ 0x100af360:
// SCHED_TARGET_CHASE is the two-task Follow schedule; the custom idle
// schedule is selected only when CTalkMonster returned slIdleStand.
Task_t tlFriendlyGunmanFollow[] =
	{
		{TASK_MOVE_TO_TARGET_RANGE, 128.0f},
		{TASK_SET_SCHEDULE, (float)SCHED_TARGET_FACE},
	};

Schedule_t slFriendlyGunmanFollow[] =
	{
		{tlFriendlyGunmanFollow,
			ARRAYSIZE(tlFriendlyGunmanFollow),
			bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE |
				bits_COND_HEAR_SOUND | bits_COND_PROVOKED,
			bits_SOUND_DANGER,
			"Follow"},
	};

Task_t tlFriendlyGunmanIdleStand[] =
	{
		{TASK_STOP_MOVING, 0.0f},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
		{TASK_WAIT, 2.0f},
		{TASK_TLK_HEADRESET, 0.0f},
	};

Schedule_t slFriendlyGunmanIdleStand[] =
	{
		{tlFriendlyGunmanIdleStand,
			ARRAYSIZE(tlFriendlyGunmanIdleStand),
			bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE |
				bits_COND_HEAR_SOUND | bits_COND_SMELL | bits_COND_PROVOKED,
			bits_SOUND_COMBAT | bits_SOUND_DANGER | bits_SOUND_MEAT |
				bits_SOUND_CARCASS | bits_SOUND_GARBAGE,
			"IdleStand"},
	};

class CFriendlyGunman : public CTalkMonster
{
public:
	int MonsterProfile() override { return 23; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_HUMAN_PASSIVE; }
	int ISoundMask() override;
	void AlertSound() override;
	void SetYawSpeed() override;
	void SetActivity(Activity NewActivity) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool CheckRangeAttack1(float flDot, float flDist) override;
	Schedule_t* GetScheduleOfType(int Type) override;
	Schedule_t* GetSchedule() override;
	void PainSound() override;
	void DeathSound() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void FollowerUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void DeclineFollowing() override;
	void TalkInit();
	void Shoot();

	// Retail ObjectCaps (vtable slot 5, 0x100ae400) returns 0x0a.
	int ObjectCaps() override
	{
		return FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE;
	}

	bool KeyValue(KeyValueData* pkvd) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	CUSTOM_SCHEDULES;

private:
	int m_iHealthValue = 80;
	int m_iGunState = 1; // 1 = Drawn, 2 = None (per FGD; no "Holstered" choice for this variant)

	int m_iszAnswer = 0;
	int m_iszQuestion = 0;
	int m_iszIdle = 0;
	int m_iszStare = 0;
	int m_iszUseSentence = 0;
	int m_iszUnUseSentence = 0;
	int m_iszStop = 0;
	int m_iszNoShoot = 0;
	int m_iszHello = 0;
	int m_iszPlHurt1 = 0;
	int m_iszPlHurt2 = 0;
	int m_iszPlHurt3 = 0;
	int m_iszPHello = 0;
	int m_iszPIdle = 0;
	int m_iszPQuestion = 0;
	int m_iszSmell = 0;
	int m_iszWound = 0;
	int m_iszMortal = 0;
	int m_iszKill = 0;	 // Gunman-only, no stock TLK_ slot - see file header comment
	int m_iszAttack = 0; // Gunman-only, no stock TLK_ slot - see file header comment
	unsigned short m_usShotEvent = 0;
	bool m_bHasAmmo = true;
	float m_flPainTime = 0.0f;
	float m_flCheckAttackTime = 0.0f;
	bool m_bLastAttackCheck = false;
};
LINK_ENTITY_TO_CLASS(monster_human_gunman, CFriendlyGunman);

// Retail 0x100add60 registers exactly these two custom schedules for
// ScheduleFromName().  This is required when an active schedule is restored
// from a savegame by name.
DEFINE_CUSTOM_SCHEDULES(CFriendlyGunman)
{
	slFriendlyGunmanFollow,
	slFriendlyGunmanIdleStand,
};
IMPLEMENT_CUSTOM_SCHEDULES(CFriendlyGunman, CTalkMonster);

TYPEDESCRIPTION CFriendlyGunman::m_SaveData[] =
		{
			DEFINE_FIELD(CFriendlyGunman, m_flPainTime, FIELD_TIME),
			DEFINE_FIELD(CFriendlyGunman, m_flCheckAttackTime, FIELD_TIME),
			DEFINE_FIELD(CFriendlyGunman, m_bLastAttackCheck, FIELD_BOOLEAN),
			DEFINE_FIELD(CFriendlyGunman, m_bHasAmmo, FIELD_BOOLEAN),
			DEFINE_FIELD(CFriendlyGunman, m_iGunState, FIELD_INTEGER),
			DEFINE_FIELD(CFriendlyGunman, m_iszAnswer, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszQuestion, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszIdle, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszStare, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszUseSentence, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszUnUseSentence, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszStop, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszNoShoot, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszHello, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszPlHurt1, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszPlHurt2, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszPlHurt3, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszPHello, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszPIdle, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszPQuestion, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszSmell, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszWound, FIELD_STRING),
		DEFINE_FIELD(CFriendlyGunman, m_iszMortal, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszKill, FIELD_STRING),
			DEFINE_FIELD(CFriendlyGunman, m_iszAttack, FIELD_STRING),
	};

IMPLEMENT_SAVERESTORE(CFriendlyGunman, CTalkMonster);

bool CFriendlyGunman::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
		m_iHealthValue = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gunstate"))
	{
		m_iGunState = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_answer"))
	{
		m_iszAnswer = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_question"))
	{
		m_iszQuestion = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_idle"))
	{
		m_iszIdle = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_stare"))
	{
		m_iszStare = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_use"))
	{
		m_iszUseSentence = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_unuse"))
	{
		m_iszUnUseSentence = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_stop"))
	{
		m_iszStop = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_noshoot"))
	{
		m_iszNoShoot = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_hello"))
	{
		m_iszHello = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_plhurt1"))
	{
		m_iszPlHurt1 = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_plhurt2"))
	{
		m_iszPlHurt2 = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_plhurt3"))
	{
		m_iszPlHurt3 = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_phello"))
	{
		m_iszPHello = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_pidle"))
	{
		m_iszPIdle = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_pquestion"))
	{
		m_iszPQuestion = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_smell"))
	{
		m_iszSmell = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_wound"))
	{
		m_iszWound = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_mortal"))
	{
		m_iszMortal = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_kill"))
	{
		m_iszKill = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_attack"))
	{
		m_iszAttack = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CTalkMonster::KeyValue(pkvd);
}

void CFriendlyGunman::TalkInit()
{
	CTalkMonster::TalkInit();

	// Retail TalkInit @ 0x100ae420 makes the two Gunman ally classes the
	// preferred conversation partners. The third entry intentionally remains
	// untouched, as it does in retail's process-wide CTalkMonster table.
	m_szFriends[0] = "monster_human_gunman";
	m_szFriends[1] = "monster_human_unarmed";

	m_szGrp[TLK_ANSWER] = STRING(m_iszAnswer);
	m_szGrp[TLK_QUESTION] = STRING(m_iszQuestion);
	m_szGrp[TLK_IDLE] = STRING(m_iszIdle);
	m_szGrp[TLK_STARE] = STRING(m_iszStare);
	m_szGrp[TLK_USE] = STRING(m_iszUseSentence);
	m_szGrp[TLK_UNUSE] = STRING(m_iszUnUseSentence);
	m_szGrp[TLK_STOP] = STRING(m_iszStop);
	m_szGrp[TLK_NOSHOOT] = STRING(m_iszNoShoot);
	m_szGrp[TLK_HELLO] = STRING(m_iszHello);
	m_szGrp[TLK_PLHURT1] = STRING(m_iszPlHurt1);
	m_szGrp[TLK_PLHURT2] = STRING(m_iszPlHurt2);
	m_szGrp[TLK_PLHURT3] = STRING(m_iszPlHurt3);
	m_szGrp[TLK_PHELLO] = STRING(m_iszPHello);
	m_szGrp[TLK_PIDLE] = STRING(m_iszPIdle);
	m_szGrp[TLK_PQUESTION] = STRING(m_iszPQuestion);
	m_szGrp[TLK_SMELL] = STRING(m_iszSmell);
	m_szGrp[TLK_WOUND] = STRING(m_iszWound);
	m_szGrp[TLK_MORTAL] = STRING(m_iszMortal);
	m_voicePitch = 100;
}

void CFriendlyGunman::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/gunmantrooper.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->health = (float)m_iHealthValue;
	pev->view_ofs = Vector(0, 0, 50);
	m_flFieldOfView = VIEW_FIELD_WIDE;
	m_MonsterState = MONSTERSTATE_NONE;

	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_RANGE_ATTACK1;

	MonsterInit();
	// Retail CFriendlyGunman::Spawn @ 0x100ae1d0 writes its eye height after
	// the model-derived initialization, preserving it for models without
	// $eyeposition.
	pev->view_ofs = Vector(0, 0, 50);
	SetUse(&CFriendlyGunman::FollowerUse);

	// gunstate 2 = "None" (no visible weapon), body 1; otherwise drawn, body 0.
	SetBodygroup(1, m_iGunState == 2 ? 1 : 0);
	m_bHasAmmo = m_iGunState != 2;
}

void CFriendlyGunman::SetYawSpeed()
{
	// CBaseMonster deliberately provides a no-op default. Without a class
	// override the scripted route updates ideal_yaw, but ChangeYaw() cannot
	// rotate the Gunman before it moves.
	pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70;
}

void CFriendlyGunman::FollowerUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Retail CFriendlyGunman::FollowerUse @ 0x100af550 does not enter the
	// generic StartFollowing/StopFollowing path. After the normal use-time
	// gate it dispatches its virtual DeclineFollowing slot.
	if (m_useTime <= gpGlobals->time)
		DeclineFollowing();
}

void CFriendlyGunman::DeclineFollowing()
{
	// Retail virtual slot +0x22c @ 0x100af570 plays the configured use group.
	PlaySentence(m_szGrp[TLK_USE], 2.0f, VOL_NORM, ATTN_NORM);
}

int CFriendlyGunman::ISoundMask()
{
	// Retail vtable slot 111 (0x100add90) returns 0x7f exactly: this NPC
	// listens to every category the GoldSrc sound system exposes.
	return bits_SOUND_COMBAT | bits_SOUND_WORLD | bits_SOUND_PLAYER |
		bits_SOUND_CARCASS | bits_SOUND_MEAT | bits_SOUND_DANGER | bits_SOUND_GARBAGE;
}

void CFriendlyGunman::AlertSound()
{
	// Retail 0x100addb0 plays the map-configured gn_attack group whenever
	// this ally obtains an enemy and is permitted to speak.
	if (m_hEnemy != nullptr && FOkToSpeak())
		PlaySentence(STRING(m_iszAttack), RANDOM_FLOAT(2.8f, 3.2f), VOL_NORM, ATTN_IDLE);
}

// BUG FIX (2026-09-05, live gameplay report): "die gunman ... in einer
// wartenden Animation ... beenden diese und beginnen von vorne" - this
// NPC is commonly puppeted by scripted_sequence "waiting"/idle-pose
// loops across this project's maps (e.g. mayan1's self-retriggering
// "fart1" scripted_sequence, m_iszPlay="onground#idle", a named,
// non-ACT_IDLE-tagged sequence). Without this override, CBaseMonster's
// default SetActivity(ACT_IDLE) (called by the default AI schedule
// between scripted waypoints, even for an otherwise full combat/talk
// monster) stomps pev->sequence mid-animation, cutting the wait pose
// short and restarting it from frame 0 - exactly the reported symptom.
// Same root cause and fix as CFurniture/CGenericMonster/CRenesaur/
// CAIGirl (see findings/entities and the
// feedback-setactivity-act-idle-stomps-scripted-sequence memory note).
// Normal combat/talk activity switching outside scripted segments is
// unaffected.
//
// Nachtrag 2026-09-06 (Live-Report "gunman haben wieder das gleiche
// Animationsproblem"): mayan1's "fart1" scripted_sequence retriggert
// sich selbst (target==targetname). CCineMonster::Use() (scripted.cpp)
// startet eine bereits bekannte Zielentity dabei NIE sofort neu, sondern
// immer mit 0.05s Verzoegerung (Stock-SDK-Verhalten, unveraendert) - in
// dieser kurzen Luecke ist m_pCine bereits null, obwohl die Figur
// eigentlich weiter "im Skript" wartet. Der reine m_pCine-Check greift
// dort nicht mehr durch, der Sprung war zurueck. Anders als bei
// CFurniture (siehe [[feedback-setactivity-guard-per-class-only]]) darf
// der Guard hier NICHT unbedingt gemacht werden - CFriendlyGunman ist
// ein echtes, dauerhaft eigenstaendig laufendes KI-/Talk-Monster
// (Follower, Kampf, echte ACT_IDLE-Sequenzen), ein unbedingtes Abfangen
// wuerde jede normale Idle-Aktivitaetsumschaltung ausserhalb von Cines
// kaputt machen. Stattdessen ueberbrueckt m_flCineGraceEnd
// (basemonster.h, von CineCleanup() gesetzt) genau die 0.05s-Luecke,
// ohne echtes freies Verhalten zu beeinflussen.
void CFriendlyGunman::SetActivity(Activity NewActivity)
{
	const bool scriptOwnsSequence = m_pCine != nullptr && (NewActivity == ACT_IDLE || NewActivity == ACT_RESET);
	const bool scriptRetriggerPending = m_pCine == nullptr && gpGlobals->time < m_flCineGraceEnd;
	if (scriptOwnsSequence || scriptRetriggerPending)
	{
		m_Activity = NewActivity;
		m_IdealActivity = NewActivity;
		return;
	}
	CTalkMonster::SetActivity(NewActivity);
}

void CFriendlyGunman::Precache()
{
	PrecacheModel("models/gunmantrooper.mdl");
	m_usShotEvent = PrecacheEvent(1, "events/monstershotgun.sc");
	PrecacheModel("sprites/gorehuman.spr");
	PrecacheModel("sprites/gibhuman.spr");

	PrecacheSound("barney/ba_pain1.wav");
	PrecacheSound("barney/ba_pain2.wav");
	PrecacheSound("barney/ba_pain3.wav");
	PrecacheSound("barney/ba_die1.wav");
	PrecacheSound("barney/ba_die2.wav");
	PrecacheSound("barney/ba_die3.wav");
	PrecacheSound("weapons/sbarrel1.wav");

	TalkInit();
	CTalkMonster::Precache();
}

void CFriendlyGunman::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case GUNMAN_AE_SHOOT:
		// Retail 0x100adfe0 emits the report from the animation event, before
		// calling Shoot().  Keeping it here also prevents direct Shoot() uses
		// from producing an unintended second report.
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/sbarrel1.wav", 1.0f, ATTN_NORM, 0, 100);
		Shoot();
		break;
	default:
		CTalkMonster::HandleAnimEvent(pEvent);
		break;
	}
}

bool CFriendlyGunman::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// Retail CFriendlyGunman::TakeDamage @ 0x100aeea0 has the same
	// player-friendly-fire escalation as Barney, except it does not retain
	// Barney's unused m_flPlayerDamage counter.
	const bool result = CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	if (!IsAlive() || pev->deadflag == DEAD_DYING)
		return result;

	if (m_MonsterState != MONSTERSTATE_PRONE && (pevAttacker->flags & FL_CLIENT) != 0)
	{
		if (m_hEnemy == nullptr)
		{
			if (HasMemory(bits_MEMORY_SUSPICIOUS) || IsFacing(pevAttacker, pev->origin))
			{
				PlaySentence(m_szGrp[TLK_NOSHOOT], 4.0f, VOL_NORM, ATTN_NORM);
				Remember(bits_MEMORY_PROVOKED);
				StopFollowing(true);
			}
			else
			{
				PlaySentence(m_szGrp[TLK_NOSHOOT], 4.0f, VOL_NORM, ATTN_NORM);
				Remember(bits_MEMORY_SUSPICIOUS);
			}
		}
		else if (!m_hEnemy->IsPlayer() && pev->deadflag == DEAD_NO)
		{
			PlaySentence(m_szGrp[TLK_NOSHOOT], 4.0f, VOL_NORM, ATTN_NORM);
		}
	}

	return result;
}

void CFriendlyGunman::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	// Retail CFriendlyGunman::TraceAttack @ 0x100af1a0: the torso takes
	// half bullet/slash/blast damage; hitgroup 10 is armour which absorbs
	// 20 bullet/slash/club damage and is then reported as a head hit.
	switch (ptr->iHitgroup)
	{
	case HITGROUP_CHEST:
	case HITGROUP_STOMACH:
		if ((bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST)) != 0)
			flDamage *= 0.5f;
		break;

	case 10:
		if ((bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB)) != 0)
		{
			flDamage -= 20.0f;
			if (flDamage <= 0.0f)
			{
				UTIL_Ricochet(ptr->vecEndPos, 1.0f);
				flDamage = 0.01f;
			}
		}
		ptr->iHitgroup = HITGROUP_HEAD;
		break;
	}

	CTalkMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

void CFriendlyGunman::PainSound()
{
	// Retail vtable slot 136 (0x100af000): a 0.5--0.75 second throttle
	// followed by one of the three precached Barney pain sounds.
	if (gpGlobals->time < m_flPainTime)
		return;

	m_flPainTime = gpGlobals->time + RANDOM_FLOAT(0.5f, 0.75f);
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "barney/ba_pain1.wav", 1.0f, ATTN_NORM, 0, GetVoicePitch());
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "barney/ba_pain2.wav", 1.0f, ATTN_NORM, 0, GetVoicePitch());
		break;
	case 2:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "barney/ba_pain3.wav", 1.0f, ATTN_NORM, 0, GetVoicePitch());
		break;
	}
}

void CFriendlyGunman::DeathSound()
{
	// Retail vtable slot 133 (0x100af0f0).
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "barney/ba_die1.wav", 1.0f, ATTN_NORM, 0, GetVoicePitch());
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "barney/ba_die2.wav", 1.0f, ATTN_NORM, 0, GetVoicePitch());
		break;
	case 2:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "barney/ba_die3.wav", 1.0f, ATTN_NORM, 0, GetVoicePitch());
		break;
	}
}

void CFriendlyGunman::Killed(entvars_t* pevAttacker, int iGib)
{
	// Retail vtable slot 13 (0x100af240) drops the shotgun only while its
	// weapon is visible, hides the bodygroup first, and applies these exact
	// randomized linear/angular velocities to the dropped entity.
	if (m_bHasAmmo)
	{
		m_bHasAmmo = false;
		SetBodygroup(1, 1);

		Vector vecGunPos;
		Vector vecGunAngles;
		GetAttachment(0, vecGunPos, vecGunAngles);

		if (CBaseEntity* pGun = DropItem("weapon_shotgun", vecGunPos, vecGunAngles))
		{
			pGun->pev->velocity = Vector(
				RANDOM_FLOAT(-48.0f, 48.0f), RANDOM_FLOAT(-48.0f, 48.0f), RANDOM_FLOAT(100.0f, 200.0f));
			pGun->pev->avelocity = Vector(0.0f, RANDOM_FLOAT(100.0f, 200.0f), 0.0f);
		}
	}

	SetUse(nullptr);
	CTalkMonster::Killed(pevAttacker, iGib);
}

bool CFriendlyGunman::CheckRangeAttack1(float flDot, float flDist)
{
	// Retail CFriendlyGunman::CheckRangeAttack1 @ 0x100ade50 gates the
	// shotgun schedule on a cached line-of-sight result.  The base SDK
	// check has no cache, so its condition remains true every AI tick and
	// causes a permanent range-attack loop.
	if (!m_hEnemy || flDist > 1024.0f || flDot < 0.5f)
		return false;

	if (gpGlobals->time > m_flCheckAttackTime)
	{
		const Vector vecShootOrigin = pev->origin + Vector(0, 0, 55);
		const Vector vecShootTarget =
			(m_hEnemy->BodyTarget(vecShootOrigin) - m_hEnemy->pev->origin) + m_vecEnemyLKP;

		TraceResult tr;
		UTIL_TraceLine(vecShootOrigin, vecShootTarget, dont_ignore_monsters, ENT(pev), &tr);
		m_bLastAttackCheck = tr.flFraction == 1.0f ||
			(tr.pHit != nullptr && CBaseEntity::Instance(tr.pHit) == m_hEnemy);
		m_flCheckAttackTime = gpGlobals->time + 1.5f;
	}

	return m_bLastAttackCheck;
}

Schedule_t* CFriendlyGunman::GetScheduleOfType(int Type)
{
	if (Type == SCHED_IDLE_STAND)
	{
		Schedule_t* pSchedule = CTalkMonster::GetScheduleOfType(Type);
		return pSchedule == slIdleStand ? slFriendlyGunmanIdleStand : pSchedule;
	}

	if (Type == SCHED_TARGET_CHASE)
		return slFriendlyGunmanFollow;

	return CTalkMonster::GetScheduleOfType(Type);
}

Schedule_t* CFriendlyGunman::GetSchedule()
{
	// Retail CFriendlyGunman::GetSchedule @ 0x100af3b0 is the ally/talk
	// state machine: danger interrupts first, then damage and follow state
	// are resolved before falling back to CTalkMonster.
	if (HasConditions(bits_COND_HEAR_SOUND))
	{
		CSound* pSound = PBestSound();
		if (pSound && (pSound->m_iType & bits_SOUND_DANGER))
			return GetScheduleOfType(SCHED_TAKE_COVER_FROM_BEST_SOUND);
	}

	if (HasConditions(bits_COND_ENEMY_DEAD) && FOkToSpeak() && !FStringNull(m_iszKill))
		PlaySentence(STRING(m_iszKill), 4.0f, VOL_NORM, ATTN_NORM);

	switch (m_MonsterState)
	{
	case MONSTERSTATE_COMBAT:
		if (HasConditions(bits_COND_ENEMY_DEAD))
			return CBaseMonster::GetSchedule();
		if (HasConditions(bits_COND_HEAVY_DAMAGE))
			return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ENEMY);
		break;

	case MONSTERSTATE_ALERT:
		// Retail 0x100af3b0 only flinches while alert when a new enemy and
		// light damage arrive together. Follow handling remains an idle-only
		// path, so alert state falls through to CTalkMonster otherwise.
		if (!HasConditions(bits_COND_ENEMY_DEAD))
		{
			if (HasAllConditions(bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE))
				return GetScheduleOfType(SCHED_SMALL_FLINCH);
			if (HasConditions(bits_COND_CLIENT_PUSH))
				return GetScheduleOfType(SCHED_MOVE_AWAY);
		}
		break;

	case MONSTERSTATE_IDLE:
		if (HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
			return GetScheduleOfType(SCHED_SMALL_FLINCH);

		if (m_hEnemy == nullptr && IsFollowing())
		{
			if (m_hTargetEnt == nullptr || !m_hTargetEnt->IsAlive())
			{
				StopFollowing(false);
				break;
			}
			if (HasConditions(bits_COND_CLIENT_PUSH))
				return GetScheduleOfType(SCHED_MOVE_AWAY_FOLLOW);
			return GetScheduleOfType(SCHED_TARGET_FACE);
		}

		if (HasConditions(bits_COND_CLIENT_PUSH))
			return GetScheduleOfType(SCHED_MOVE_AWAY);

		TrySmellTalk();
		break;
	}

	return CTalkMonster::GetSchedule();
}

void CFriendlyGunman::Shoot()
{
	if (!m_hEnemy)
		return;

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy(vecShootOrigin);
	// Retail 0x100ae030 plays the registered NPC-shotgun event reliably
	// before applying the server-side pellets. Without this, clients never
	// receive monstershotgun.sc's muzzle/shell effect.
	PLAYBACK_EVENT_FULL(FEV_RELIABLE, edict(), m_usShotEvent, 0.0f,
		pev->origin, pev->angles, 0.0f, 0.0f, 0, 0, 0, 0);
	// Retail FUN_100ae030: 3 pellets, 0.13053-radian spread and 4096 range.
	FireBullets(3, vecShootOrigin, vecShootDir, Vector(0.13053f, 0.13053f, 0), 4096,
		BULLET_PLAYER_BUCKSHOT, 0, gSkillData.gunmanBuckshotDamage);
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;

	// Retail updates blending controller zero from the final shot direction.
	Vector angDir = UTIL_VecToAngles(vecShootDir);
	SetBlending(0, angDir.x);
}
