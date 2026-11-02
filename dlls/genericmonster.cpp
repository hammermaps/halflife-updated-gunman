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
// Generic Monster - purely for scripted sequence work.
//=========================================================
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "soundent.h"

// For holograms, make them not solid so the player can walk through them
#define SF_GENERICMONSTER_NOTSOLID 4

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CGenericMonster : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override;
	int Classify() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int ISoundMask() override;
	void SetActivity(Activity NewActivity) override;
};
LINK_ENTITY_TO_CLASS(monster_generic, CGenericMonster);

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CGenericMonster::Classify()
{
	// Retail Classify (vtable @0x100ecd60 slot 8, @0x1001efb0) is `xor eax,eax; ret`
	// = CLASS_NONE. The stock SDK value CLASS_PLAYER_ALLY made every Gunman
	// (CLASS_HUMAN_PASSIVE) hate scripted props such as the dropship (R_HT) and
	// shoot at them endlessly.
	return CLASS_NONE;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGenericMonster::SetYawSpeed()
{
	int ys;

	switch (m_Activity)
	{
	case ACT_IDLE:
	default:
		ys = 90;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGenericMonster::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 0:
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// ISoundMask - generic monster can't hear.
//=========================================================
int CGenericMonster::ISoundMask()
{
	return bits_SOUND_NONE;
}

// BUG FIX (2026-09-05, live gameplay report): monster_generic is the
// stock "any model, driven purely by scripted_sequence" prop class -
// used throughout this project for Gunman's custom scripted props
// (e.g. mayan1's "blaster" SAM site turret, models/missleturret.mdl,
// controlled entirely by a scripted_sequence/trigger_changetarget
// chain). Its Gunman-specific models commonly expose only named
// sequences (e.g. "mayan#idle"/"mayan#shootdown" via m_iszPlay), not
// an ACT_IDLE-tagged sequence. CBaseMonster's default SetActivity()
// (monsters.cpp) calls LookupActivity(ACT_IDLE), which falls back to
// sequence 0 when no match exists, and writes that straight into
// pev->sequence - stomping whatever CCineMonster::StartSequence had
// just set for the in-progress scripted sequence every time the
// default AI schedule briefly requests ACT_IDLE between
// scripted_sequence waypoints. Symptom (reported live): the prop
// appears stuck in its sequence-0 pose (e.g. the SAM turret looks
// permanently "extended") and repeats the idle-pair waypoints
// indefinitely; even after a trigger_changetarget correctly redirects
// the chain onward (e.g. to the "shoot"/wall-break sequences), those
// keep getting stomped and re-looped the same way.
//
// Same root cause and fix already applied to CFurniture/
// monster_furniture (scripted.cpp, Session 102) after a customized
// AnimateThink attempt broke its scripted-sequence playback - restore
// stock AI/think behavior everywhere else, but stop SetActivity()
// from touching pev->sequence for ACT_IDLE/ACT_RESET specifically,
// leaving sequence selection entirely to CCineMonster::StartSequence.
//
// VERIFIED AGAINST gunman.dll (2026-09-05): re-checked this fix isn't
// masking a real, differently-behaving retail deviation before
// shipping it a second time. monster_generic's LINK (0x1001ef50) and
// vtable (0x100ecd60) were freshly resolved and fully dumped (75
// slots). There is no separately-named "SetActivity" export anywhere
// in the whole binary, and - importantly - byte-offset vtable slot
// numbers are NOT comparable between this modern SDK fork and the
// 2000-era retail binary's own (differently ordered/sized) C++ ABI;
// an earlier attempt to identify the slot this way (guessing from
// findings/entities/renesaur.md's own speculative "vtable slot
// 0x10c, vermutlich ACT_IDLE" comment) turned out wrong on
// cross-check - that exact slot is confirmed IDENTICAL between
// CGenericMonster and CFurniture in the retail vtable (0x1002af50)
// and decompiles to an AI-init tail routine (sets
// Think=MonsterInitThink/Use=MonsterUse), not SetActivity at all.
// Conclusion: there is no reliable way to identify or rule out a
// distinct retail CGenericMonster::SetActivity via vtable-slot
// comparison against this fork, so this remains what it was always
// labeled as - a necessary engine-integration fix for how this
// project's modern AI-schedule timing interacts with
// CCineMonster::StartSequence, not a confirmed decompiled retail
// behavior - see [[feedback-setactivity-act-idle-stomps-scripted-sequence]].
//
// Nachtrag 2026-09-06 (Live-Report "gunman in city2b haben noch
// Probleme mit der Animation" - CITY2B's "trooper4", ein
// monster_generic mit models/gunmantrooper_ng.mdl): trooper4 wird
// ueber eine ZWEI-Entity-Ping-Pong-Kette bespielt (scripted_sequence
// "aflyout1" -> target "aflyout2" -> target "aflyout1", je ohne vom
// Mapper gesetzte Verzoegerung). Wie bei CFriendlyGunman/
// CFriendlyUnarmed (siehe deren SetActivity()-Kommentare und
// [[feedback-setactivity-guard-per-class-only]]) verzoegert
// CCineMonster::Use() (scripted.cpp, Stock-SDK) JEDEN Neustart einer
// bereits bekannten Zielentity trotzdem immer um 0.05s - in dieser
// Luecke ist m_pCine kurz null, obwohl die Figur weiter "im Skript"
// wartet. Anders als CFurniture darf der Guard hier NICHT unbedingt
// gemacht werden, da CGenericMonster (MOVETYPE_STEP/SOLID_SLIDEBOX)
// projektweit auch fuer potenziell frei stehende/laufende
// Platzierungen genutzt wird - stattdessen ueberbrueckt
// m_flCineGraceEnd (basemonster.h, von CineCleanup() gesetzt) die
// 0.05s-Luecke gezielt.
void CGenericMonster::SetActivity(Activity NewActivity)
{
	if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && (m_pCine != nullptr || gpGlobals->time < m_flCineGraceEnd))
	{
		m_Activity = NewActivity;
		m_IdealActivity = NewActivity;
		return;
	}

	CBaseMonster::SetActivity(NewActivity);
}

//=========================================================
// Spawn
//=========================================================
void CGenericMonster::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), STRING(pev->model));

	/*
	if ( FStrEq( STRING(pev->model), "models/player.mdl" ) )
		UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
*/

	if (FStrEq(STRING(pev->model), "models/player.mdl") || FStrEq(STRING(pev->model), "models/holo.mdl"))
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->health = 8;
	m_flFieldOfView = 0.5; // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();

	if ((pev->spawnflags & SF_GENERICMONSTER_NOTSOLID) != 0)
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGenericMonster::Precache()
{
	PrecacheModel((char*)STRING(pev->model));
}

//=========================================================
// monster_flashlight - Gunman Chronicles extension of CGenericMonster:
// identical placeholder/scripted-sequence prop, with a flashlight beam
// attachment enabled via bodygroup 2, submodel 1.
//
// Decompiled from gunman.dll: Spawn @0x1001f2f0 is exactly
// CGenericMonster::Spawn() (@0x1001eff0, matches field-for-field) plus
// one extra SetBodygroup(2, 1) call. Precache/Classify are unmodified
// (Precache @0x1001f220 precaches STRING(pev->model), same as
// CGenericMonster::Precache(); Classify @0x1001efb0 returns 0 =
// CLASS_NONE, the CGenericMonster default).
//=========================================================
class CFlashlightMonster : public CGenericMonster
{
public:
	void Spawn() override
	{
		CGenericMonster::Spawn();
		SetBodygroup(2, 1);
	}
};
LINK_ENTITY_TO_CLASS(monster_flashlight, CFlashlightMonster);

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
