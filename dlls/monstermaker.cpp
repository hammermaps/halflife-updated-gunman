/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//=========================================================
// Monster Maker - this is an entity that creates monsters
// in the game.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"

// Monstermaker spawnflags
#define SF_MONSTERMAKER_START_ON 1	  // start active ( if has targetname )
#define SF_MONSTERMAKER_CYCLIC 4	  // drop one monster every time fired.
#define SF_MONSTERMAKER_MONSTERCLIP 8 // Children are blocked by monsterclip
// Gunman-Erweiterungen (RE-bestaetigt, findings/entities/game_rules_multi_monstermaker_sdk_diffs.md,
// Nachtrag 2026-09-06): zwei zusaetzliche FGD-Spawnflags gegenueber dem SDK.
#define SF_MONSTERMAKER_WARPIN 16			// Warp In Effect - Partikel-/Sprite-Vorlauf vor dem Spawn
#define SF_MONSTERMAKER_NOFALLTOGROUND 32 // Dont Fall to Ground - Kind wird nicht auf den Boden fallen gelassen

// Vereinfachte Naeherung der im Retail-Decompilat gemessenen ~1.5-2s
// Partikel-Vorlaufzeit von CMonsterMaker::MakerWarpThink (exakte
// Zeitkonstante nicht byte-genau rekonstruierbar, siehe Findings).
#define MONSTERMAKER_WARPIN_DELAY 1.5

//=========================================================
// MonsterMaker - this ent creates monsters during the game.
//=========================================================
class CMonsterMaker : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void EXPORT ToggleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT CyclicUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT MakerThink();
	// Gunman-Erweiterung (RE-bestaetigt): Warp-In-Vorlaufeffekt vor dem eigentlichen Spawn,
	// siehe SF_MONSTERMAKER_WARPIN. Kein SDK-Pendant.
	void EXPORT MakerWarpThink();
	void DeathNotice(entvars_t* pevChild) override; // monster maker children use this to tell the monster maker that they have died.
	void MakeMonster();

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	string_t m_iszMonsterClassname; // classname of the monster(s) that will be created.

	int m_cNumMonsters; // max number of monsters this ent can create


	int m_cLiveChildren;	// how many monsters made by this monster maker that are currently alive
	int m_iMaxLiveChildren; // max number of monsters that this maker may have out at one time.

	float m_flGround; // z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child

	bool m_fActive;
	bool m_fFadeChildren; // should we make the children fadeout?

	// Gunman-Erweiterungen (RE-bestaetigt, siehe Findings-Nachtrag 2026-09-06):
	int m_iSkin;					// "_skin" - wird auf pev->skin des gespawnten Kindes uebertragen (aktiv genutzt).
	int m_iFadeStatus;				// "fadestatus" - wird geparst/gespeichert, im Retail-Build aber nirgends
									// ausgelesen (bestaetigt toter Wert); hier nur zur FGD-Kompatibilitaet uebernommen.
	bool m_fWarpInPending;			// true waehrend der Warp-In-Vorlaufphase (siehe MakerWarpThink).
	float m_flWarpInFinished;		// Zeitpunkt, zu dem die Warp-In-Vorlaufphase endet und gespawnt wird.
};

LINK_ENTITY_TO_CLASS(monstermaker, CMonsterMaker);

TYPEDESCRIPTION CMonsterMaker::m_SaveData[] =
	{
		DEFINE_FIELD(CMonsterMaker, m_iszMonsterClassname, FIELD_STRING),
		DEFINE_FIELD(CMonsterMaker, m_cNumMonsters, FIELD_INTEGER),
		DEFINE_FIELD(CMonsterMaker, m_cLiveChildren, FIELD_INTEGER),
		DEFINE_FIELD(CMonsterMaker, m_flGround, FIELD_FLOAT),
		DEFINE_FIELD(CMonsterMaker, m_iMaxLiveChildren, FIELD_INTEGER),
		DEFINE_FIELD(CMonsterMaker, m_fActive, FIELD_BOOLEAN),
		DEFINE_FIELD(CMonsterMaker, m_fFadeChildren, FIELD_BOOLEAN),
		DEFINE_FIELD(CMonsterMaker, m_iSkin, FIELD_INTEGER),
		DEFINE_FIELD(CMonsterMaker, m_iFadeStatus, FIELD_INTEGER),
		DEFINE_FIELD(CMonsterMaker, m_fWarpInPending, FIELD_BOOLEAN),
		DEFINE_FIELD(CMonsterMaker, m_flWarpInFinished, FIELD_TIME),
};


IMPLEMENT_SAVERESTORE(CMonsterMaker, CBaseMonster);

bool CMonsterMaker::KeyValue(KeyValueData* pkvd)
{

	if (FStrEq(pkvd->szKeyName, "monstercount"))
	{
		m_cNumMonsters = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_imaxlivechildren"))
	{
		m_iMaxLiveChildren = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "monstertype"))
	{
		m_iszMonsterClassname = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "_skin"))
	{
		m_iSkin = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "fadestatus"))
	{
		// RE-bestaetigt toter Wert im Retail-Build, siehe m_iFadeStatus-Deklaration oben.
		m_iFadeStatus = atoi(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}


void CMonsterMaker::Spawn()
{
	pev->solid = SOLID_NOT;

	// Retail Spawn (monstermaker vtable slot 0, first 0x400 bytes): with spawnflags & 8 the capability
	// word (+0x220) gets bit 0x4 (bits_CAP_STRAFE). Same flag as SF_MONSTERMAKER_MONSTERCLIP.
	if ((pev->spawnflags & SF_MONSTERMAKER_MONSTERCLIP) != 0)
		m_afCapability |= bits_CAP_STRAFE;

	m_cLiveChildren = 0;
	Precache();
	if (!FStringNull(pev->targetname))
	{
		if ((pev->spawnflags & SF_MONSTERMAKER_CYCLIC) != 0)
		{
			SetUse(&CMonsterMaker::CyclicUse); // drop one monster each time we fire
		}
		else
		{
			SetUse(&CMonsterMaker::ToggleUse); // so can be turned on/off
		}

		if (FBitSet(pev->spawnflags, SF_MONSTERMAKER_START_ON))
		{ // start making monsters as soon as monstermaker spawns
			m_fActive = true;
			SetThink(&CMonsterMaker::MakerThink);
		}
		else
		{ // wait to be activated.
			m_fActive = false;
			SetThink(&CMonsterMaker::SUB_DoNothing);
		}
	}
	else
	{ // no targetname, just start.
		pev->nextthink = gpGlobals->time + m_flDelay;
		m_fActive = true;
		SetThink(&CMonsterMaker::MakerThink);
	}

	if (m_cNumMonsters == 1)
	{
		m_fFadeChildren = false;
	}
	else
	{
		m_fFadeChildren = true;
	}

	m_flGround = 0;
}

void CMonsterMaker::Precache()
{
	CBaseMonster::Precache();

	UTIL_PrecacheOther(STRING(m_iszMonsterClassname));

	if ((pev->spawnflags & SF_MONSTERMAKER_WARPIN) != 0)
		PrecacheSound("ambience/ammo_respawn.wav");
}

//=========================================================
// MakeMonster-  this is the code that drops the monster
//=========================================================
void CMonsterMaker::MakeMonster()
{
	edict_t* pent;
	entvars_t* pevCreate;

	if (m_iMaxLiveChildren > 0 && m_cLiveChildren >= m_iMaxLiveChildren)
	{ // not allowed to make a new one yet. Too many live ones out right now.
		return;
	}

	if (0 == m_flGround)
	{
		// set altitude. Now that I'm activated, any breakables, etc should be out from under me.
		TraceResult tr;

		UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 2048), ignore_monsters, ENT(pev), &tr);
		m_flGround = tr.vecEndPos.z;
	}

	Vector mins = pev->origin - Vector(34, 34, 0);
	Vector maxs = pev->origin + Vector(34, 34, 0);
	maxs.z = pev->origin.z;
	mins.z = m_flGround;

	CBaseEntity* pList[2];
	int count = UTIL_EntitiesInBox(pList, 2, mins, maxs, FL_CLIENT | FL_MONSTER);
	if (0 != count)
	{
		// don't build a stack of monsters!
		return;
	}

	pent = CREATE_NAMED_ENTITY(m_iszMonsterClassname);

	if (FNullEnt(pent))
	{
		ALERT(at_console, "NULL Ent in MonsterMaker!\n");
		return;
	}

	// If I have a target, fire!
	if (!FStringNull(pev->target))
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		FireTargets(STRING(pev->target), this, this, USE_TOGGLE, 0);
	}

	pevCreate = VARS(pent);
	pevCreate->origin = pev->origin;
	pevCreate->angles = pev->angles;
	// Gunman-Erweiterung (RE-bestaetigt): "_skin" wird vor dem Spawn direkt auf das Kind uebertragen.
	pevCreate->skin = m_iSkin;

	// Gunman-Erweiterung (RE-bestaetigt): SF_MONSTERMAKER_NOFALLTOGROUND unterdrueckt das sonst
	// immer gesetzte SF_MONSTER_FALL_TO_GROUND-Flag.
	if ((pev->spawnflags & SF_MONSTERMAKER_NOFALLTOGROUND) == 0)
		SetBits(pevCreate->spawnflags, SF_MONSTER_FALL_TO_GROUND);

	// Children hit monsterclip brushes
	if ((pev->spawnflags & SF_MONSTERMAKER_MONSTERCLIP) != 0)
		SetBits(pevCreate->spawnflags, SF_MONSTER_HITMONSTERCLIP);

	DispatchSpawn(ENT(pevCreate));
	pevCreate->owner = edict();

	// Gunman-Erweiterung (RE-bestaetigt): Warp-In-Effekt am eigentlichen Spawnpunkt -
	// Sound plus vereinfachter Funkeneffekt (Retail nutzt eine TE-Beam-Nachricht mit
	// mehreren Zufallsendpunkten, hier auf eine einzelne TE_SPARKS reduziert).
	if ((pev->spawnflags & SF_MONSTERMAKER_WARPIN) != 0)
	{
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "ambience/ammo_respawn.wav", 1, ATTN_NORM);

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SPARKS);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		MESSAGE_END();
	}

	if (!FStringNull(pev->netname))
	{
		// if I have a netname (overloaded), give the child monster that name as a targetname
		pevCreate->targetname = pev->netname;
	}

	m_cLiveChildren++; // count this monster
	m_cNumMonsters--;

	if (m_cNumMonsters == 0)
	{
		// Disable this forever.  Don't kill it because it still gets death notices
		SetThink(NULL);
		SetUse(NULL);
	}
}

//=========================================================
// CyclicUse - drops one monster from the monstermaker
// each time we call this.
//=========================================================
void CMonsterMaker::CyclicUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Gunman-Erweiterung (RE-bestaetigt): bei SF_MONSTERMAKER_WARPIN erst die
	// Warp-In-Vorlaufsequenz starten, statt sofort zu spawnen.
	if ((pev->spawnflags & SF_MONSTERMAKER_WARPIN) != 0)
	{
		m_fWarpInPending = false;
		SetThink(&CMonsterMaker::MakerWarpThink);
		pev->nextthink = gpGlobals->time;
		return;
	}

	MakeMonster();
}

//=========================================================
// ToggleUse - activates/deactivates the monster maker
//=========================================================
void CMonsterMaker::ToggleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!ShouldToggle(useType, m_fActive))
		return;

	if (m_fActive)
	{
		m_fActive = false;
		SetThink(NULL);
	}
	else
	{
		m_fActive = true;
		SetThink(&CMonsterMaker::MakerThink);
	}

	pev->nextthink = gpGlobals->time;
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CMonsterMaker::MakerThink()
{
	// Gunman-Erweiterung (RE-bestaetigt): bei SF_MONSTERMAKER_WARPIN erst die
	// Warp-In-Vorlaufsequenz starten, statt in diesem Zyklus sofort zu spawnen.
	if ((pev->spawnflags & SF_MONSTERMAKER_WARPIN) != 0)
	{
		m_fWarpInPending = false;
		SetThink(&CMonsterMaker::MakerWarpThink);
		pev->nextthink = gpGlobals->time;
		return;
	}

	pev->nextthink = gpGlobals->time + m_flDelay;

	MakeMonster();
}

//=========================================================
// MakerWarpThink - Gunman-Erweiterung (RE-bestaetigt, kein SDK-Pendant):
// mehrstufiger Partikel-/Sprite-Vorlauf vor dem eigentlichen MakeMonster()-Aufruf,
// wenn SF_MONSTERMAKER_WARPIN gesetzt ist. Das Retail-Decompilat baut dafuer ein
// animiertes CSprite plus vier zufaellig ausgerichtete Funken-/Blitzstrahlen auf;
// hier auf einen einzelnen wiederholten TE_SPARKS-Effekt am Spawnpunkt vereinfacht
// (siehe findings/entities/game_rules_multi_monstermaker_sdk_diffs.md).
//=========================================================
void CMonsterMaker::MakerWarpThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_fWarpInPending)
	{
		// erster Tick der Sequenz: Vorlaufeffekt starten.
		m_fWarpInPending = true;
		m_flWarpInFinished = gpGlobals->time + MONSTERMAKER_WARPIN_DELAY;

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SPARKS);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		MESSAGE_END();
		return;
	}

	if (gpGlobals->time < m_flWarpInFinished)
	{
		// weiterhin am Warten - periodisch Funken nachlegen.
		if (RANDOM_LONG(0, 3) == 0)
		{
			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
			WRITE_BYTE(TE_SPARKS);
			WRITE_COORD(pev->origin.x);
			WRITE_COORD(pev->origin.y);
			WRITE_COORD(pev->origin.z);
			MESSAGE_END();
		}
		return;
	}

	// Vorlaufzeit abgelaufen - jetzt tatsaechlich spawnen.
	m_fWarpInPending = false;
	MakeMonster();

	if ((pev->spawnflags & SF_MONSTERMAKER_CYCLIC) != 0)
	{
		// Cyclic: nur ein Kind pro Use-Aufruf, danach wieder inaktiv.
		SetThink(&CMonsterMaker::SUB_DoNothing);
	}
	else
	{
		SetThink(&CMonsterMaker::MakerThink);
		pev->nextthink = gpGlobals->time + m_flDelay;
	}
}


//=========================================================
//=========================================================
void CMonsterMaker::DeathNotice(entvars_t* pevChild)
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveChildren--;

	if (!m_fFadeChildren)
	{
		pevChild->owner = NULL;
	}
}
