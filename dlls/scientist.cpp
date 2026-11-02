/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// human scientist (passive lab worker)
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "talkmonster.h"
#include "schedule.h"
#include "defaultai.h"
#include "scripted.h"
#include "animation.h"
#include "soundent.h"


#define NUM_SCIENTIST_HEADS 4 // four heads available for scientist model
enum
{
	HEAD_GLASSES = 0,
	HEAD_EINSTEIN = 1,
	HEAD_LUTHER = 2,
	HEAD_SLICK = 3
};

enum
{
	SCHED_HIDE = LAST_TALKMONSTER_SCHEDULE + 1,
	SCHED_FEAR,
	SCHED_PANIC,
	SCHED_STARTLE,
	SCHED_TARGET_CHASE_SCARED,
	SCHED_TARGET_FACE_SCARED,
};

enum
{
	TASK_SAY_HEAL = LAST_TALKMONSTER_TASK + 1,
	TASK_HEAL,
	TASK_SAY_FEAR,
	TASK_RUN_PATH_SCARED,
	TASK_SCREAM,
	TASK_RANDOM_SCREAM,
	TASK_MOVE_TO_TARGET_RANGE_SCARED,
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define SCIENTIST_AE_HEAL (1)
#define SCIENTIST_AE_NEEDLEON (2)
#define SCIENTIST_AE_NEEDLEOFF (3)

//=======================================================
// Scientist
//=======================================================

class CScientist : public CTalkMonster
{
public:
	int MonsterProfile() override { return 20; } // retail slot 59
	void Spawn() override;
	void Precache() override;

	void SetYawSpeed() override;
	int Classify() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	bool CheckMeleeAttack1(float flDot, float flDist) override;
	void RunTask(Task_t* pTask) override;
	void StartTask(Task_t* pTask) override;
	int ObjectCaps() override { return CTalkMonster::ObjectCaps() | FCAP_IMPULSE_USE; }
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	int FriendNumber(int arrayNumber) override;
	void SetActivity(Activity newActivity) override;
	Activity GetStoppedActivity() override;
	int ISoundMask() override;
	void DeclineFollowing() override;

	float CoverRadius() override { return 1200; } // Need more room for cover because scientists want to get far away!
	bool DisregardEnemy(CBaseEntity* pEnemy) { return !pEnemy->IsAlive() || (gpGlobals->time - m_fearTime) > 15; }

	bool CanHeal();
	void Heal();
	void Scream();

	// Override these to set behavior
	Schedule_t* GetScheduleOfType(int Type) override;
	Schedule_t* GetSchedule() override;
	MONSTERSTATE GetIdealState() override;

	void DeathSound() override;
	void PainSound() override;

	void TalkInit();
	bool KeyValue(KeyValueData* pkvd) override;

	char* GetScientistModel() const;

	void Killed(entvars_t* pevAttacker, int iGib) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

private:
	float m_painTime;
	float m_healTime;
	float m_fearTime;
	// Retail CScientist::KeyValue stores the 20 sc_* strings. Eighteen have
	// direct CTalkMonster sentence-group slots; sc_kill/sc_attack are retained
	// for save compatibility until their Retail consumers are mapped.
	string_t m_iszSentenceOverrides[20] = {};
	// Retail +0x2ec: initialised to 1; CScientist::KeyValue("IsScared") can set it to 0.
	int m_iClassifyBias = 1;
	// Retail save table fields +0x2f0/+0x2f4/+0x2f8. Only m_iAIActive's
	// schedule/state effect is fully mapped; the other values are retained.
	int m_iAIActive = 1;
	int m_iShouldClearVoiceChannel = 0;
	int m_iBlockSound = 0;
};

LINK_ENTITY_TO_CLASS(monster_scientist, CScientist);

TYPEDESCRIPTION CScientist::m_SaveData[] =
	{
		DEFINE_FIELD(CScientist, m_painTime, FIELD_TIME),
		DEFINE_FIELD(CScientist, m_healTime, FIELD_TIME),
		DEFINE_FIELD(CScientist, m_fearTime, FIELD_TIME),
		DEFINE_ARRAY(CScientist, m_iszSentenceOverrides, FIELD_STRING, 20),
		DEFINE_FIELD(CScientist, m_iClassifyBias, FIELD_INTEGER),
		DEFINE_FIELD(CScientist, m_iAIActive, FIELD_INTEGER),
		DEFINE_FIELD(CScientist, m_iShouldClearVoiceChannel, FIELD_INTEGER),
		DEFINE_FIELD(CScientist, m_iBlockSound, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CScientist, CTalkMonster);

char* CScientist::GetScientistModel() const
{
	char* pszOverride = (char*)CVAR_GET_STRING("_sv_override_scientist_mdl");
	if (pszOverride && strlen(pszOverride) > 5) // at least requires ".mdl"
	{
		return pszOverride;
	}

	return (char *)"models/scientist.mdl";
}

bool CScientist::KeyValue(KeyValueData* pkvd)
{
	struct SentenceOverride_t
	{
		const char* pszKey;
		int iTalkGroup;
	};
	static const SentenceOverride_t overrides[] = {
		{"sc_answer", TLK_ANSWER}, {"sc_question", TLK_QUESTION}, {"sc_idle", TLK_IDLE},
		{"sc_stare", TLK_STARE}, {"sc_use", TLK_USE}, {"sc_unuse", TLK_UNUSE},
		{"sc_stop", TLK_STOP}, {"sc_noshoot", TLK_NOSHOOT}, {"sc_hello", TLK_HELLO},
		{"sc_plhurt1", TLK_PLHURT1}, {"sc_plhurt2", TLK_PLHURT2}, {"sc_plhurt3", TLK_PLHURT3},
		{"sc_phello", TLK_PHELLO}, {"sc_pidle", TLK_PIDLE}, {"sc_pquestion", TLK_PQUESTION},
		{"sc_smell", TLK_SMELL}, {"sc_wound", TLK_WOUND}, {"sc_mortal", TLK_MORTAL},
		{"sc_kill", -1}, {"sc_attack", -1},
	};

	for (int i = 0; i < (int)ARRAYSIZE(overrides); ++i)
	{
		if (FStrEq(pkvd->szKeyName, overrides[i].pszKey))
		{
			m_iszSentenceOverrides[i] = ALLOC_STRING(pkvd->szValue);
			return true;
		}
	}

	if (FStrEq(pkvd->szKeyName, "IsScared"))
	{
		m_iClassifyBias = 0;
		return true;
	}
	if (FStrEq(pkvd->szKeyName, "knife"))
	{
		if (atoi(pkvd->szValue) != 0)
			pev->weapons |= 0x20;
		else
			pev->weapons &= ~0x20;
		return true;
	}
	if (FStrEq(pkvd->szKeyName, "DefautAIState"))
	{
		// Retail handles the literal "0" by disabling its Scientist-specific
		// state/schedule path; other values leave the constructor default.
		if (FStrEq(pkvd->szValue, "0"))
			m_iAIActive = 0;
		return true;
	}
	if (FStrEq(pkvd->szKeyName, "clearvoice"))
	{
		m_iShouldClearVoiceChannel = atoi(pkvd->szValue) != 0;
		return true;
	}
	if (FStrEq(pkvd->szKeyName, "blocksound"))
	{
		m_iBlockSound = atoi(pkvd->szValue) != 0;
		return true;
	}

	return CTalkMonster::KeyValue(pkvd);
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t tlFollow[] =
	{
		{TASK_SET_FAIL_SCHEDULE, (float)SCHED_CANT_FOLLOW}, // If you fail, bail out of follow
		{TASK_MOVE_TO_TARGET_RANGE, (float)128},			// Move within 128 of target ent (client)
															//	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t slFollow[] =
	{
		{tlFollow,
			ARRAYSIZE(tlFollow),
			bits_COND_NEW_ENEMY |
				bits_COND_LIGHT_DAMAGE |
				bits_COND_HEAVY_DAMAGE |
				bits_COND_HEAR_SOUND,
			bits_SOUND_COMBAT |
				bits_SOUND_DANGER,
			"Follow"},
};

Task_t tlFollowScared[] =
	{
		{TASK_SET_FAIL_SCHEDULE, (float)SCHED_TARGET_CHASE}, // If you fail, follow normally
		{TASK_MOVE_TO_TARGET_RANGE_SCARED, (float)128},		 // Move within 128 of target ent (client)
															 //	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE_SCARED },
};

Schedule_t slFollowScared[] =
	{
		{tlFollowScared,
			ARRAYSIZE(tlFollowScared),
			bits_COND_NEW_ENEMY |
				bits_COND_HEAR_SOUND |
				bits_COND_LIGHT_DAMAGE |
				bits_COND_HEAVY_DAMAGE,
			bits_SOUND_DANGER,
			"FollowScared"},
};

Task_t tlFaceTargetScared[] =
	{
		{TASK_FACE_TARGET, (float)0},
		{TASK_SET_ACTIVITY, (float)ACT_CROUCHIDLE},
		{TASK_SET_SCHEDULE, (float)SCHED_TARGET_CHASE_SCARED},
};

Schedule_t slFaceTargetScared[] =
	{
		{tlFaceTargetScared,
			ARRAYSIZE(tlFaceTargetScared),
			bits_COND_HEAR_SOUND |
				bits_COND_NEW_ENEMY,
			bits_SOUND_DANGER,
			"FaceTargetScared"},
};

Task_t tlStopFollowing[] =
	{
		{TASK_CANT_FOLLOW, (float)0},
};

Schedule_t slStopFollowing[] =
	{
		{tlStopFollowing,
			ARRAYSIZE(tlStopFollowing),
			0,
			0,
			"StopFollowing"},
};


Task_t tlHeal[] =
	{
		{TASK_MOVE_TO_TARGET_RANGE, (float)50},				 // Move within 60 of target ent (client)
		{TASK_SET_FAIL_SCHEDULE, (float)SCHED_TARGET_CHASE}, // If you fail, catch up with that guy! (change this to put syringe away and then chase)
		{TASK_FACE_IDEAL, (float)0},
		{TASK_SAY_HEAL, (float)0},
		{TASK_PLAY_SEQUENCE_FACE_TARGET, (float)ACT_ARM},	 // Whip out the needle
		{TASK_HEAL, (float)0},								 // Put it in the player
		{TASK_PLAY_SEQUENCE_FACE_TARGET, (float)ACT_DISARM}, // Put away the needle
};

Schedule_t slHeal[] =
	{
		{tlHeal,
			ARRAYSIZE(tlHeal),
			0, // Don't interrupt or he'll end up running around with a needle all the time
			0,
			"Heal"},
};


Task_t tlFaceTarget[] =
	{
		{TASK_STOP_MOVING, (float)0},
		{TASK_FACE_TARGET, (float)0},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
		{TASK_SET_SCHEDULE, (float)SCHED_TARGET_CHASE},
};

Schedule_t slFaceTarget[] =
	{
		{tlFaceTarget,
			ARRAYSIZE(tlFaceTarget),
			bits_COND_CLIENT_PUSH |
				bits_COND_NEW_ENEMY |
				bits_COND_HEAR_SOUND,
			bits_SOUND_COMBAT |
				bits_SOUND_DANGER,
			"FaceTarget"},
};


Task_t tlSciPanic[] =
	{
		{TASK_STOP_MOVING, (float)0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_SCREAM, (float)0},
		{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_EXCITED}, // This is really fear-stricken excitement
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
};

Schedule_t slSciPanic[] =
	{
		{tlSciPanic,
			ARRAYSIZE(tlSciPanic),
			0,
			0,
			"SciPanic"},
};


Task_t tlIdleSciStand[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
		{TASK_WAIT, (float)2},			// repick IDLESTAND every two seconds.
		{TASK_TLK_HEADRESET, (float)0}, // reset head position
};

Schedule_t slIdleSciStand[] =
	{
		{tlIdleSciStand,
			ARRAYSIZE(tlIdleSciStand),
			bits_COND_NEW_ENEMY |
				bits_COND_LIGHT_DAMAGE |
				bits_COND_HEAVY_DAMAGE |
				bits_COND_HEAR_SOUND |
				bits_COND_SMELL |
				bits_COND_CLIENT_PUSH |
				bits_COND_PROVOKED,

			bits_SOUND_COMBAT | // sound flags
				//bits_SOUND_PLAYER		|
				//bits_SOUND_WORLD		|
				bits_SOUND_DANGER |
				bits_SOUND_MEAT | // scents
				bits_SOUND_CARCASS |
				bits_SOUND_GARBAGE,
			"IdleSciStand"

		},
};


Task_t tlScientistCover[] =
	{
		{TASK_SET_FAIL_SCHEDULE, (float)SCHED_PANIC}, // If you fail, just panic!
		{TASK_STOP_MOVING, (float)0},
		{TASK_FIND_COVER_FROM_ENEMY, (float)0},
		{TASK_RUN_PATH_SCARED, (float)0},
		{TASK_TURN_LEFT, (float)179},
		{TASK_SET_SCHEDULE, (float)SCHED_HIDE},
};

Schedule_t slScientistCover[] =
	{
		{tlScientistCover,
			ARRAYSIZE(tlScientistCover),
			bits_COND_NEW_ENEMY,
			0,
			"ScientistCover"},
};



Task_t tlScientistHide[] =
	{
		{TASK_SET_FAIL_SCHEDULE, (float)SCHED_PANIC}, // If you fail, just panic!
		{TASK_STOP_MOVING, (float)0},
		{TASK_PLAY_SEQUENCE, (float)ACT_CROUCH},
		{TASK_SET_ACTIVITY, (float)ACT_CROUCHIDLE}, // FIXME: This looks lame
		{TASK_WAIT_RANDOM, (float)10.0},
};

Schedule_t slScientistHide[] =
	{
		{tlScientistHide,
			ARRAYSIZE(tlScientistHide),
			bits_COND_NEW_ENEMY |
				bits_COND_HEAR_SOUND |
				bits_COND_SEE_ENEMY |
				bits_COND_SEE_HATE |
				bits_COND_SEE_FEAR |
				bits_COND_SEE_DISLIKE,
			bits_SOUND_DANGER,
			"ScientistHide"},
};


Task_t tlScientistStartle[] =
	{
		{TASK_SET_FAIL_SCHEDULE, (float)SCHED_PANIC}, // If you fail, just panic!
		{TASK_RANDOM_SCREAM, (float)0.3},			  // Scream 30% of the time
		{TASK_STOP_MOVING, (float)0},
		{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCH},
		{TASK_RANDOM_SCREAM, (float)0.1}, // Scream again 10% of the time
		{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCHIDLE},
		{TASK_WAIT_RANDOM, (float)1.0},
};

Schedule_t slScientistStartle[] =
	{
		{tlScientistStartle,
			ARRAYSIZE(tlScientistStartle),
			bits_COND_NEW_ENEMY |
				bits_COND_SEE_ENEMY |
				bits_COND_SEE_HATE |
				bits_COND_SEE_FEAR |
				bits_COND_SEE_DISLIKE,
			0,
			"ScientistStartle"},
};


// Marphy Fact Files Fix - Restore fear display animation
Task_t tlFear[] =
	{
		{TASK_STOP_MOVING, (float)0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_SAY_FEAR, (float)0},
		{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_FEAR_DISPLAY},
};

Schedule_t slFear[] =
	{
		{tlFear,
			ARRAYSIZE(tlFear),
			bits_COND_NEW_ENEMY,
			0,
			"Fear"},
};


DEFINE_CUSTOM_SCHEDULES(CScientist){
	slFollow,
	slFaceTarget,
	slIdleSciStand,
	slFear,
	slScientistCover,
	slScientistHide,
	slScientistStartle,
	slHeal,
	slStopFollowing,
	slSciPanic,
	slFollowScared,
	slFaceTargetScared,
};


IMPLEMENT_CUSTOM_SCHEDULES(CScientist, CTalkMonster);


void CScientist::DeclineFollowing()
{
	Talk(10);
	m_hTalkTarget = m_hEnemy;
	PlaySentence("SC_POK", 2, VOL_NORM, ATTN_NORM);
}


void CScientist::Scream()
{
	// Marphy Fact Files Fix - This speech check always fails during combat, so removing
	//if ( FOkToSpeak() )
	//{
	Talk(10);
	m_hTalkTarget = m_hEnemy;
	PlaySentence("SC_SCREAM", RANDOM_FLOAT(3, 6), VOL_NORM, ATTN_NORM);
	//}
}


Activity CScientist::GetStoppedActivity()
{
	if (m_hEnemy != NULL)
		return ACT_EXCITED;
	return CTalkMonster::GetStoppedActivity();
}


void CScientist::StartTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_SAY_HEAL:
		//		if ( FOkToSpeak() )
		Talk(2);
		m_hTalkTarget = m_hTargetEnt;
		PlaySentence("SC_HEAL", 2, VOL_NORM, ATTN_IDLE);

		TaskComplete();
		break;

	case TASK_SCREAM:
		Scream();
		TaskComplete();
		break;

	case TASK_RANDOM_SCREAM:
		if (RANDOM_FLOAT(0, 1) < pTask->flData)
			Scream();
		TaskComplete();
		break;

	case TASK_SAY_FEAR:
		// Marphy Fact FIles Fix - This speech check always fails during combat, so removing
		//if ( FOkToSpeak() )
		if (m_hEnemy)
		{
			Talk(2);
			m_hTalkTarget = m_hEnemy;
			if (m_hEnemy->IsPlayer())
				PlaySentence("SC_PLFEAR", 5, VOL_NORM, ATTN_NORM);
			else
				PlaySentence("SC_FEAR", 5, VOL_NORM, ATTN_NORM);
		}
		TaskComplete();
		break;

	case TASK_HEAL:
		m_IdealActivity = ACT_MELEE_ATTACK1;
		break;

	case TASK_RUN_PATH_SCARED:
		m_movementActivity = ACT_RUN_SCARED;
		break;

	case TASK_MOVE_TO_TARGET_RANGE_SCARED:
	{
		if ((m_hTargetEnt->pev->origin - pev->origin).Length() < 1)
			TaskComplete();
		else
		{
			m_vecMoveGoal = m_hTargetEnt->pev->origin;
			if (!MoveToTarget(ACT_WALK_SCARED, 0.5))
				TaskFail();
		}
	}
	break;

	default:
		CTalkMonster::StartTask(pTask);
		break;
	}
}

void CScientist::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_RUN_PATH_SCARED:
		if (MovementIsComplete())
			TaskComplete();

		// Marphy Fact Files Fix - Reducing scream (which didn't work before) chance significantly
		//if ( RANDOM_LONG(0,31) < 8 )
		if (RANDOM_LONG(0, 63) < 1)
			Scream();
		break;

	case TASK_MOVE_TO_TARGET_RANGE_SCARED:
	{
		// Marphy Fact Files Fix - Removing redundant scream
		//if ( RANDOM_LONG(0,63)< 8 )
		//Scream();

		if (m_hEnemy == NULL)
		{
			TaskFail();
		}
		else
		{
			float distance;

			distance = (m_vecMoveGoal - pev->origin).Length2D();
			// Re-evaluate when you think your finished, or the target has moved too far
			if ((distance < pTask->flData) || (m_vecMoveGoal - m_hTargetEnt->pev->origin).Length() > pTask->flData * 0.5)
			{
				m_vecMoveGoal = m_hTargetEnt->pev->origin;
				distance = (m_vecMoveGoal - pev->origin).Length2D();
				FRefreshRoute();
			}

			// Set the appropriate activity based on an overlapping range
			// overlap the range to prevent oscillation
			if (distance < pTask->flData)
			{
				TaskComplete();
				RouteClear(); // Stop moving
			}
			else if (distance < 190 && m_movementActivity != ACT_WALK_SCARED)
				m_movementActivity = ACT_WALK_SCARED;
			else if (distance >= 270 && m_movementActivity != ACT_RUN_SCARED)
				m_movementActivity = ACT_RUN_SCARED;
		}
	}
	break;

	case TASK_HEAL:
		if (m_fSequenceFinished)
		{
			TaskComplete();
		}
		else
		{
			if (TargetDistance() > 90)
				TaskComplete();
			pev->ideal_yaw = UTIL_VecToYaw(m_hTargetEnt->pev->origin - pev->origin);
			ChangeYaw(pev->yaw_speed);
		}
		break;
	default:
		CTalkMonster::RunTask(pTask);
		break;
	}
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CScientist::Classify()
{
	// Retail @0x100d7d20 (re-decompiled 2026-10-31): never CLASS_HUMAN_PASSIVE.
	// Retail returns 5 - (+0x2ec != 0) and only falls back to 4 in the German /
	// violence_germany build; that censorship branch is deliberately not adopted
	// (the SDK stays uncut). +0x2ec defaults to 1, so the result is 4.
	return CLASS_ALIEN_MILITARY - (m_iClassifyBias != 0 ? 1 : 0);
}


//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CScientist::SetYawSpeed()
{
	int ys;

	ys = 90;

	switch (m_Activity)
	{
	case ACT_IDLE:
		ys = 120;
		break;
	case ACT_WALK:
		ys = 180;
		break;
	case ACT_RUN:
		ys = 150;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 120;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CScientist::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 4:
	case 5:
	{
		CBaseEntity* pHurt = CheckTraceHullAttack(64, 15, DMG_SLASH);
		if (pHurt)
		{
			// Retail supplies a random punch on the victim and bleeds monster
			// victims with sk_xenomemelee_d (global 0x10136f4c).
			pHurt->pev->punchangle.x = (float)(RANDOM_LONG(0, 7) - 4);
			pHurt->pev->punchangle.z = (float)(RANDOM_LONG(0, 7) - 4);
			if (CBaseMonster* pMonster = pHurt->MyMonsterPointer())
			{
				TraceResult trBleed;
				UTIL_TraceLine(GetGunPosition(), pMonster->BodyTarget(GetGunPosition()), dont_ignore_monsters, edict(), &trBleed);
				pMonster->TraceBleed(gSkillData.xenomeMeleeDamage, g_vecZero, &trBleed, DMG_SLASH);
			}
		}
		break;
	}
	case SCIENTIST_AE_HEAL: // Heal my target (if within range)
		Heal();
		break;
	case SCIENTIST_AE_NEEDLEON:
	{
		int oldBody = pev->body;
		pev->body = (oldBody % NUM_SCIENTIST_HEADS) + NUM_SCIENTIST_HEADS * 1;
	}
	break;
	case SCIENTIST_AE_NEEDLEOFF:
	{
		int oldBody = pev->body;
		pev->body = (oldBody % NUM_SCIENTIST_HEADS) + NUM_SCIENTIST_HEADS * 0;
	}
	break;

	default:
		CTalkMonster::HandleAnimEvent(pEvent);
	}
}

// Retail slot 82: knife-only, 64-unit / 0.7-dot melee check. The original
// also rejects classify values 18 and 19; those Gunman-only classes do not
// have SDK names, so keep the observed numeric exclusions local here.
bool CScientist::CheckMeleeAttack1(float flDot, float flDist)
{
	if (pev->weapons != 0x20 || flDist > 64.0f || flDot < 0.7f || m_hEnemy == NULL)
		return false;

	int enemyClass = m_hEnemy->Classify();
	return enemyClass != 18 && enemyClass != 19;
}

//=========================================================
// Spawn
//=========================================================
void CScientist::Spawn()
{
	if (pev->body == -1)
	{														 // -1 chooses a random head
		pev->body = RANDOM_LONG(0, NUM_SCIENTIST_HEADS - 1); // pick a head, any head
	}

	Precache();

	SET_MODEL(ENT(pev), GetScientistModel());
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->health = gSkillData.scientistHealth;
	pev->view_ofs = Vector(0, 0, 50);  // position of the eyes relative to monster's origin.
	m_flFieldOfView = VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so scientists will notice player and say hello
	m_MonsterState = MONSTERSTATE_NONE;

	//	m_flDistTooFar		= 256.0;

	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS | bits_CAP_USE;

	// White hands
	pev->skin = 0;

	// Luther is black, make his hands black
	if (pev->body == HEAD_LUTHER)
		pev->skin = 1;

	MonsterInit();
	if (pev->weapons == 0x20)
		m_afCapability |= bits_CAP_MELEE_ATTACK1;
	SetUse(&CScientist::FollowerUse);
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CScientist::Precache()
{
	PrecacheModel(GetScientistModel());
	PrecacheSound("gscientist/hurt1.wav");
	PrecacheSound("gscientist/hurt5.wav");
	PrecacheSound("gscientist/hurt6.wav");

	// every new scientist must call this, otherwise
	// when a level is loaded, nobody will talk (time is reset to 0)
	TalkInit();

	CTalkMonster::Precache();

	// Apply explicit Gunman sentence groups after the generic UseSentence /
	// UnUseSentence handling. Retail stores the sc_* fields independently.
	static const int talkGroups[18] = {
		TLK_ANSWER, TLK_QUESTION, TLK_IDLE, TLK_STARE, TLK_USE, TLK_UNUSE,
		TLK_STOP, TLK_NOSHOOT, TLK_HELLO, TLK_PLHURT1, TLK_PLHURT2, TLK_PLHURT3,
		TLK_PHELLO, TLK_PIDLE, TLK_PQUESTION, TLK_SMELL, TLK_WOUND, TLK_MORTAL};
	for (int i = 0; i < (int)ARRAYSIZE(talkGroups); ++i)
	{
		if (!FStringNull(m_iszSentenceOverrides[i]))
			m_szGrp[talkGroups[i]] = STRING(m_iszSentenceOverrides[i]);
	}
}

// Init talk data
void CScientist::TalkInit()
{

	CTalkMonster::TalkInit();

	// scientist will try to talk to friends in this order:

	m_szFriends[0] = "monster_scientist";
	m_szFriends[1] = "monster_sitting_scientist";
	m_szFriends[2] = "monster_barney";

	// scientists speach group names (group names are in sentences.txt)

	m_szGrp[TLK_ANSWER] = "SC_ANSWER";
	m_szGrp[TLK_QUESTION] = "SC_QUESTION";
	m_szGrp[TLK_IDLE] = "SC_IDLE";
	m_szGrp[TLK_STARE] = "SC_STARE";
	m_szGrp[TLK_USE] = "SC_OK";
	m_szGrp[TLK_UNUSE] = "SC_WAIT";
	m_szGrp[TLK_STOP] = "SC_STOP";
	m_szGrp[TLK_NOSHOOT] = "SC_SCARED";
	m_szGrp[TLK_HELLO] = "SC_HELLO";

	m_szGrp[TLK_PLHURT1] = "!SC_CUREA";
	m_szGrp[TLK_PLHURT2] = "!SC_CUREB";
	m_szGrp[TLK_PLHURT3] = "!SC_CUREC";

	m_szGrp[TLK_PHELLO] = "SC_PHELLO";
	m_szGrp[TLK_PIDLE] = "SC_PIDLE";
	m_szGrp[TLK_PQUESTION] = "SC_PQUEST";
	m_szGrp[TLK_SMELL] = "SC_SMELL";

	m_szGrp[TLK_WOUND] = "SC_WOUND";
	m_szGrp[TLK_MORTAL] = "SC_MORTAL";

	// get voice for head
	switch (pev->body % NUM_SCIENTIST_HEADS)
	{
	default:
	case HEAD_GLASSES:
		m_voicePitch = 105;
		break; //glasses
	case HEAD_EINSTEIN:
		m_voicePitch = 100;
		break; //einstein
	case HEAD_LUTHER:
		m_voicePitch = 95;
		break; //luther
	case HEAD_SLICK:
		m_voicePitch = 100;
		break; //slick
	}
}

bool CScientist::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// Retail calls PainSound first and forwards damage with DMG_SLASH set.
	PainSound();
	return CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_SLASH);
}


//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. In the base class implementation,
// monsters care about all sounds, but no scents.
//=========================================================
int CScientist::ISoundMask()
{
	// Retail FUN_100d87a0 returns a fixed mask while Scientist AI is disabled.
	if (!m_iAIActive)
		return 0x30310;

	int iSoundMask = CTalkMonster::ISoundMask();
	// Retail adds this undocumented sound category when `blocksound` is set.
	if (m_iBlockSound)
		iSoundMask |= 0x20000;

	return iSoundMask;
}

//=========================================================
// PainSound
//=========================================================
void CScientist::PainSound()
{
	// Retail slot 136 checks IsAlive and ignores NONE, SCRIPT, DEAD and the
	// custom 7 state. Its integer-rounded elapsed-time check gates at ~0.5 s.
	if (!IsAlive() || fabsf(gpGlobals->time - m_painTime) <= 0.5f)
		return;

	if (m_MonsterState == MONSTERSTATE_NONE || m_MonsterState == MONSTERSTATE_SCRIPT ||
		m_MonsterState == MONSTERSTATE_DEAD || m_MonsterState == 7)
		return;

	m_painTime = gpGlobals->time;

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "gscientist/hurt1.wav", 1, ATTN_NORM, 0, GetVoicePitch());
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "gscientist/hurt5.wav", 1, ATTN_NORM, 0, GetVoicePitch());
		break;
	default:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "gscientist/hurt6.wav", 1, ATTN_NORM, 0, GetVoicePitch());
		break;
	}
}

//=========================================================
// DeathSound
//=========================================================
void CScientist::DeathSound()
{
	PainSound();
}


void CScientist::Killed(entvars_t* pevAttacker, int iGib)
{
	SetUse(NULL);
	CTalkMonster::Killed(pevAttacker, iGib);
}


void CScientist::SetActivity(Activity newActivity)
{
	// BUG FIX (2026-09-05, Gunman-specific): monster_scientist is
	// puppeted by scripted_sequence "waiting"/idle-pose loops in this
	// project's maps just like the other Gunman NPCs (named,
	// non-ACT_IDLE-tagged sequences). Without this guard,
	// CTalkMonster::SetActivity(ACT_IDLE) (reached below, itself
	// forwarding to CBaseMonster::SetActivity) stomps pev->sequence
	// mid-animation, cutting the wait pose short and restarting it
	// from frame 0 - same root cause as CFriendlyGunman/CGenericMonster/
	// etc, see the feedback-setactivity-act-idle-stomps-scripted-sequence
	// memory note. This is a deliberate deviation from the otherwise
	// untouched stock CScientist implementation, needed because this
	// project drives it via scripted_sequence in ways the stock SDK
	// never anticipated.
	if ((newActivity == ACT_IDLE || newActivity == ACT_RESET) && m_pCine != nullptr)
	{
		m_Activity = newActivity;
		m_IdealActivity = newActivity;
		return;
	}

	int iSequence;

	iSequence = LookupActivity(newActivity);

	// Set to the desired anim, or default anim if the desired is not present
	if (iSequence == ACTIVITY_NOT_AVAILABLE)
		newActivity = ACT_IDLE;
	CTalkMonster::SetActivity(newActivity);
}


Schedule_t* CScientist::GetScheduleOfType(int Type)
{
	Schedule_t* psched;

	switch (Type)
	{
	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		// call base class default so that scientist will talk
		// when 'used'
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
			return slFaceTarget; // override this for different target face behavior
		else
			return psched;

	case SCHED_TARGET_CHASE:
		return slFollow;

	case SCHED_CANT_FOLLOW:
		return slStopFollowing;

	case SCHED_PANIC:
		return slSciPanic;

	case SCHED_TARGET_CHASE_SCARED:
		return slFollowScared;

	case SCHED_TARGET_FACE_SCARED:
		return slFaceTargetScared;

	case SCHED_IDLE_STAND:
		// call base class default so that scientist will talk
		// when standing during idle
		psched = CTalkMonster::GetScheduleOfType(Type);

		if (psched == slIdleStand)
			return slIdleSciStand;
		else
			return psched;

	case SCHED_HIDE:
		return slScientistHide;

	case SCHED_STARTLE:
		return slScientistStartle;

	case SCHED_FEAR:
		return slFear;
	}

	return CTalkMonster::GetScheduleOfType(Type);
}

Schedule_t* CScientist::GetSchedule()
{
	// Retail delegates to base scheduling when DefautAIState disables the
	// Scientist-specific AI path.
	if (!m_iAIActive)
		return CBaseMonster::GetSchedule();

	// so we don't keep calling through the EHANDLE stuff
	CBaseEntity* pEnemy = m_hEnemy;

	if (HasConditions(bits_COND_HEAR_SOUND))
	{
		CSound* pSound;
		pSound = PBestSound();

		ASSERT(pSound != NULL);
		if (pSound && (pSound->m_iType & bits_SOUND_DANGER) != 0)
			return GetScheduleOfType(SCHED_TAKE_COVER_FROM_BEST_SOUND);
	}

	switch (m_MonsterState)
	{
	case MONSTERSTATE_ALERT:
	case MONSTERSTATE_IDLE:
		if (pEnemy)
		{
			if (HasConditions(bits_COND_SEE_ENEMY))
				m_fearTime = gpGlobals->time;
			else if (DisregardEnemy(pEnemy)) // After 15 seconds of being hidden, return to alert
			{
				m_hEnemy = NULL;
				pEnemy = NULL;

				// Marphy Fact Files Fix - Fix scientists not disregarding enemy after hiding
				m_fearTime = gpGlobals->time;
			}
		}

		if (HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
		{
			// flinch if hurt
			return GetScheduleOfType(SCHED_SMALL_FLINCH);
		}

		// Cower when you hear something scary
		if (HasConditions(bits_COND_HEAR_SOUND))
		{
			CSound* pSound;
			pSound = PBestSound();

			ASSERT(pSound != NULL);
			if (pSound)
			{
				if ((pSound->m_iType & (bits_SOUND_DANGER | bits_SOUND_COMBAT)) != 0)
				{
					if (gpGlobals->time - m_fearTime > 3) // Only cower every 3 seconds or so
					{
						m_fearTime = gpGlobals->time;			 // Update last fear
						return GetScheduleOfType(SCHED_STARTLE); // This will just duck for a second
					}
				}
			}
		}

		// Behavior for following the player
		if (IsFollowing())
		{
			if (!m_hTargetEnt->IsAlive())
			{
				// UNDONE: Comment about the recently dead player here?
				StopFollowing(false);
				break;
			}

			int relationship = R_NO;

			// Nothing scary, just me and the player
			if (pEnemy != NULL)
				relationship = IRelationship(pEnemy);

			// UNDONE: Model fear properly, fix R_FR and add multiple levels of fear
			if (relationship != R_DL && relationship != R_HT)
			{
				// If I'm already close enough to my target
				if (TargetDistance() <= 128)
				{
					if (CanHeal()) // Heal opportunistically
						return slHeal;
					if (HasConditions(bits_COND_CLIENT_PUSH)) // Player wants me to move
						return GetScheduleOfType(SCHED_MOVE_AWAY_FOLLOW);
				}
				return GetScheduleOfType(SCHED_TARGET_FACE); // Just face and follow.
			}
			else // UNDONE: When afraid, scientist won't move out of your way.  Keep This?  If not, write move away scared
			{
				if (HasConditions(bits_COND_NEW_ENEMY))				// I just saw something new and scary, react
					return GetScheduleOfType(SCHED_FEAR);			// React to something scary
				return GetScheduleOfType(SCHED_TARGET_FACE_SCARED); // face and follow, but I'm scared!
			}
		}

		if (HasConditions(bits_COND_CLIENT_PUSH)) // Player wants me to move
			return GetScheduleOfType(SCHED_MOVE_AWAY);

		// try to say something about smells
		TrySmellTalk();
		break;
	case MONSTERSTATE_COMBAT:
		// Retail routes knife-equipped scientists through the generic combat
		// scheduler; unarmed scientists keep their fearful cover behavior.
		if (pev->weapons == 0x20)
			return CBaseMonster::GetSchedule();

		if (HasConditions(bits_COND_NEW_ENEMY))
			return slFear; // Point and scream!

		if (HasConditions(bits_COND_SEE_ENEMY))
		{
			// Marphy Fact Files Fix - Fix scientists not disregarding enemy after hiding
			m_fearTime = gpGlobals->time;
			return slScientistCover; // Take Cover
		}

		if (HasConditions(bits_COND_HEAR_SOUND))
			return slTakeCoverFromBestSound; // Cower and panic from the scary sound!

		// Marphy Fact Files Fix - Fix scientists not disregarding enemy after hiding
		if (pEnemy)
		{
			if (HasConditions(bits_COND_SEE_ENEMY))
				m_fearTime = gpGlobals->time;
			else if (DisregardEnemy(pEnemy)) // After 15 seconds of being hidden, return to alert
			{
				m_hEnemy = NULL;
				pEnemy = NULL;

				m_fearTime = gpGlobals->time;

				if (IsFollowing())
				{
					return slScientistStartle;
				}

				return slScientistHide; // Hide after disregard
			}
		}

		return slScientistCover; // Run & Cower
		break;
	}

	return CTalkMonster::GetSchedule();
}

MONSTERSTATE CScientist::GetIdealState()
{
	// Match Retail's inactive-AI fallback before Scientist fear/follow logic.
	if (!m_iAIActive)
		return CBaseMonster::GetIdealState();

	// Retail clears the one-shot Scientist voice channel on the first damage
	// or interesting-sound interruption. IsScared disables this gate.
	if (m_iClassifyBias && HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND))
	{
		if (m_iShouldClearVoiceChannel)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM);
		m_iClassifyBias = 0;
		// Retail @0x100d8800 fires TriggerTarget on this one-shot interruption.
		// FUN_100531f0 resolves targetname matches, skips FL_KILLME entities,
		// and calls Use with this as activator and caller.
		FireTargets(STRING(m_iszTriggerTarget), this, this, USE_TOGGLE, 0);
	}

	switch (m_MonsterState)
	{
	case MONSTERSTATE_ALERT:
	case MONSTERSTATE_IDLE:
		if (HasConditions(bits_COND_NEW_ENEMY))
		{
			if (IsFollowing())
			{
				int relationship = IRelationship(m_hEnemy);
				if (relationship != R_FR || relationship != R_HT && !HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
				{
					// Don't go to combat if you're following the player
					m_IdealMonsterState = MONSTERSTATE_ALERT;
					return m_IdealMonsterState;
				}
				StopFollowing(true);
			}
		}
		else if (HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
		{
			// Stop following if you take damage
			if (IsFollowing())
				StopFollowing(true);
		}
		break;

	case MONSTERSTATE_COMBAT:
	{
		CBaseEntity* pEnemy = m_hEnemy;
		if (pEnemy != NULL)
		{
			if (DisregardEnemy(pEnemy)) // After 15 seconds of being hidden, return to alert
			{
				// Strip enemy when going to alert
				m_IdealMonsterState = MONSTERSTATE_ALERT;
				m_hEnemy = NULL;

				// Marphy Fact Files Fix - Fix scientists not disregarding enemy after hiding
				m_fearTime = gpGlobals->time;
				return m_IdealMonsterState;
			}
			// Follow if only scared a little
			if (m_hTargetEnt != NULL)
			{
				m_IdealMonsterState = MONSTERSTATE_ALERT;
				return m_IdealMonsterState;
			}

			if (HasConditions(bits_COND_SEE_ENEMY))
			{
				m_fearTime = gpGlobals->time;
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
				return m_IdealMonsterState;
			}
		}
	}
	break;
	}

	return CTalkMonster::GetIdealState();
}


bool CScientist::CanHeal()
{
	if ((m_healTime > gpGlobals->time) || (m_hTargetEnt == NULL) || (m_hTargetEnt->pev->health > (m_hTargetEnt->pev->max_health * 0.5)))
		return false;

	return true;
}

void CScientist::Heal()
{
	if (!CanHeal())
		return;

	Vector target = m_hTargetEnt->pev->origin - pev->origin;
	if (target.Length() > 100)
		return;

	m_hTargetEnt->TakeHealth(gSkillData.scientistHeal, DMG_GENERIC);
	// Don't heal again for 1 minute
	m_healTime = gpGlobals->time + 60;
}

int CScientist::FriendNumber(int arrayNumber)
{
	static int array[3] = {1, 2, 0};
	if (arrayNumber < 3)
		return array[arrayNumber];
	return arrayNumber;
}


//=========================================================
// Dead Scientist PROP
//=========================================================
class CDeadScientist : public CBaseMonster
{
public:
	void Spawn() override;
	// Retail monster_scientist_dead Classify @0x100d89d0 returns 4 (stock HL: HUMAN_PASSIVE).
	int Classify() override { return CLASS_HUMAN_MILITARY; }

	// passed into Precache which is non-const
	char* GetScientistModel() const;

	bool KeyValue(KeyValueData* pkvd) override;
	int m_iPose; // which sequence to display
	static const char* m_szPoses[7];
};
const char* CDeadScientist::m_szPoses[] = {"lying_on_back", "lying_on_stomach", "dead_sitting", "dead_hang", "dead_table1", "dead_table2", "dead_table3"};

char* CDeadScientist::GetScientistModel() const
{
	char* pszOverride = (char*)CVAR_GET_STRING("_sv_override_scientist_mdl");
	if (pszOverride && strlen(pszOverride) > 5) // at least requires ".mdl"
	{
		return pszOverride;
	}

	return (char *)"models/scientist.mdl";
}

bool CDeadScientist::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}
LINK_ENTITY_TO_CLASS(monster_scientist_dead, CDeadScientist);

//
// ********** DeadScientist SPAWN **********
//
void CDeadScientist::Spawn()
{
	PrecacheModel(GetScientistModel());
	SET_MODEL(ENT(pev), GetScientistModel());

	pev->effects = 0;
	pev->sequence = 0;
	// Corpses have less health
	pev->health = 8; //gSkillData.scientistHealth;

	m_bloodColor = BLOOD_COLOR_RED;

	if (pev->body == -1)
	{														 // -1 chooses a random head
		// Retail CDeadScientist::Spawn randomizes only body variants 0..2.
		pev->body = RANDOM_LONG(0, 2);
	}

	pev->sequence = LookupSequence(m_szPoses[m_iPose]);
	if (pev->sequence == -1)
	{
		ALERT(at_console, "Dead scientist with bad pose\n");
	}

	//	pev->skin += 2; // use bloody skin -- UNDONE: Turn this back on when we have a bloody skin again!
	MonsterInitDead();
}


//=========================================================
// Sitting Scientist PROP
//=========================================================

class CSittingScientist : public CScientist // kdb: changed from public CBaseMonster so he can speak
{
public:
	int MonsterProfile() override { return 20; } // retail slot 59
	void Spawn() override;
	void Precache() override;

	void EXPORT SittingThink();
	int Classify() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void SetAnswerQuestion(CTalkMonster* pSpeaker) override;
	int FriendNumber(int arrayNumber) override;

	bool FIdleSpeak();
	int m_baseSequence;
	int m_headTurn;
	float m_flResponseDelay;
};

LINK_ENTITY_TO_CLASS(monster_sitting_scientist, CSittingScientist);
TYPEDESCRIPTION CSittingScientist::m_SaveData[] =
	{
		// Don't need to save/restore m_baseSequence (recalced)
		DEFINE_FIELD(CSittingScientist, m_headTurn, FIELD_INTEGER),
		DEFINE_FIELD(CSittingScientist, m_flResponseDelay, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CSittingScientist, CScientist);

// animation sequence aliases
typedef enum
{
	SITTING_ANIM_sitlookleft,
	SITTING_ANIM_sitlookright,
	SITTING_ANIM_sitscared,
	SITTING_ANIM_sitting2,
	SITTING_ANIM_sitting3
} SITTING_ANIM;


//
// ********** Scientist SPAWN **********
//
void CSittingScientist::Spawn()
{
	PrecacheModel(GetScientistModel());
	SET_MODEL(ENT(pev), GetScientistModel());
	Precache();
	InitBoneControllers();

	UTIL_SetSize(pev, Vector(-14, -14, 0), Vector(14, 14, 36));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = 50;

	m_bloodColor = BLOOD_COLOR_RED;
	m_flFieldOfView = VIEW_FIELD_WIDE; // indicates the width of this monster's forward view cone ( as a dotproduct result )

	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD;

	SetBits(pev->spawnflags, SF_MONSTER_PREDISASTER); // predisaster only!

	if (pev->body == -1)
	{														 // -1 chooses a random head
		// Retail CSittingScientist::Spawn randomizes only body variants 0..2.
		pev->body = RANDOM_LONG(0, 2);
	}
	m_baseSequence = LookupSequence("sitlookleft");
	pev->sequence = m_baseSequence + RANDOM_LONG(0, 4);
	ResetSequenceInfo();

	SetThink(&CSittingScientist::SittingThink);
	pev->nextthink = gpGlobals->time + 0.1;

	DROP_TO_FLOOR(ENT(pev));
}

void CSittingScientist::Precache()
{
	m_baseSequence = LookupSequence("sitlookleft");
	TalkInit();
}

//=========================================================
// ID as a passive human
//=========================================================
int CSittingScientist::Classify()
{
	// Retail monster_sitting_scientist Classify @0x100d8df0 returns 4 (stock HL: HUMAN_PASSIVE).
	return CLASS_HUMAN_MILITARY;
}


int CSittingScientist::FriendNumber(int arrayNumber)
{
	static int array[3] = {2, 1, 0};
	if (arrayNumber < 3)
		return array[arrayNumber];
	return arrayNumber;
}



//=========================================================
// sit, do stuff
//=========================================================
void CSittingScientist::SittingThink()
{
	CBaseEntity* pent;

	StudioFrameAdvance();

	// try to greet player
	if (FIdleHello())
	{
		pent = FindNearestFriend(true);
		if (pent)
		{
			float yaw = VecToYaw(pent->pev->origin - pev->origin) - pev->angles.y;

			if (yaw > 180)
				yaw -= 360;
			if (yaw < -180)
				yaw += 360;

			if (yaw > 0)
				pev->sequence = m_baseSequence + SITTING_ANIM_sitlookleft;
			else
				pev->sequence = m_baseSequence + SITTING_ANIM_sitlookright;

			ResetSequenceInfo();
			pev->frame = 0;
			SetBoneController(0, 0);
		}
	}
	else if (m_fSequenceFinished)
	{
		int i = RANDOM_LONG(0, 99);
		m_headTurn = 0;

		if (0 != m_flResponseDelay && gpGlobals->time > m_flResponseDelay)
		{
			// respond to question
			IdleRespond();
			pev->sequence = m_baseSequence + SITTING_ANIM_sitscared;
			m_flResponseDelay = 0;
		}
		else if (i < 30)
		{
			pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;

			// turn towards player or nearest friend and speak

			if (!FBitSet(m_bitsSaid, bit_saidHelloPlayer))
				pent = FindNearestFriend(true);
			else
				pent = FindNearestFriend(false);

			if (!FIdleSpeak() || !pent)
			{
				m_headTurn = RANDOM_LONG(0, 8) * 10 - 40;
				pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;
			}
			else
			{
				// only turn head if we spoke
				float yaw = VecToYaw(pent->pev->origin - pev->origin) - pev->angles.y;

				if (yaw > 180)
					yaw -= 360;
				if (yaw < -180)
					yaw += 360;

				if (yaw > 0)
					pev->sequence = m_baseSequence + SITTING_ANIM_sitlookleft;
				else
					pev->sequence = m_baseSequence + SITTING_ANIM_sitlookright;

				//ALERT(at_console, "sitting speak\n");
			}
		}
		else if (i < 60)
		{
			pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;
			m_headTurn = RANDOM_LONG(0, 8) * 10 - 40;
			if (RANDOM_LONG(0, 99) < 5)
			{
				//ALERT(at_console, "sitting speak2\n");
				FIdleSpeak();
			}
		}
		else if (i < 80)
		{
			pev->sequence = m_baseSequence + SITTING_ANIM_sitting2;
		}
		else if (i < 100)
		{
			pev->sequence = m_baseSequence + SITTING_ANIM_sitscared;
		}

		ResetSequenceInfo();
		pev->frame = 0;
		SetBoneController(0, m_headTurn);
	}
	pev->nextthink = gpGlobals->time + 0.1;
}

// prepare sitting scientist to answer a question
void CSittingScientist::SetAnswerQuestion(CTalkMonster* pSpeaker)
{
	m_flResponseDelay = gpGlobals->time + RANDOM_FLOAT(3, 4);
	m_hTalkTarget = (CBaseMonster*)pSpeaker;
}


//=========================================================
// FIdleSpeak
// ask question of nearby friend, or make statement
//=========================================================
bool CSittingScientist::FIdleSpeak()
{
	// try to start a conversation, or make statement
	int pitch;

	if (!FOkToSpeak())
		return false;

	// set global min delay for next conversation
	CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(4.8, 5.2);

	pitch = GetVoicePitch();

	// if there is a friend nearby to speak to, play sentence, set friend's response time, return

	// try to talk to any standing or sitting scientists nearby
	CBaseEntity* pentFriend = FindNearestFriend(false);

	if (pentFriend && RANDOM_LONG(0, 1))
	{
		CTalkMonster* pTalkMonster = GetClassPtr((CTalkMonster*)pentFriend->pev);
		pTalkMonster->SetAnswerQuestion(this);

		IdleHeadTurn(pentFriend->pev->origin);
		SENTENCEG_PlayRndSz(ENT(pev), m_szGrp[TLK_PQUESTION], 1.0, ATTN_IDLE, 0, pitch);
		// set global min delay for next conversation
		CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(4.8, 5.2);
		return true;
	}

	// otherwise, play an idle statement
	if (RANDOM_LONG(0, 1))
	{
		SENTENCEG_PlayRndSz(ENT(pev), m_szGrp[TLK_PIDLE], 1.0, ATTN_IDLE, 0, pitch);
		// set global min delay for next conversation
		CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT(4.8, 5.2);
		return true;
	}

	// never spoke
	CTalkMonster::g_talkWaitTime = 0;
	return false;
}
