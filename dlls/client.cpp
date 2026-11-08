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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include <algorithm>

// Debug switch for decore_cam tracking (dbg_camtrack), default off.
bool g_bDbgCamTrack = false;

#include <string>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "filesystem_utils.h"
#include "cbase.h"
#include "com_model.h"
#include "saverestore.h"
#include "player.h"
#include "monsters.h"
#include "spectator.h"
#include "dbg_log.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "customentity.h"
#include "weapons.h"
#include "weaponinfo.h"
#include "schedule.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "UserMessages.h"

DLL_GLOBAL unsigned int g_ulFrameCount;

extern void CopyToBodyQue(entvars_t* pev);
extern bool g_bDbgRouteProbe;
extern bool g_bDbgRouteExpandedOnly;

void LinkUserMessages();

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame(entvars_t* pev)
{
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

	//	pev->frame		= $deatha11;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->deadflag = DEAD_DEAD;
	pev->nextthink = -1;
}


/*
===========
ClientConnect

called when a player connects to a server
============
*/
qboolean ClientConnect(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	return static_cast<qboolean>(g_pGameRules->ClientConnected(pEntity, pszName, pszAddress, szRejectReason));

	// a client connecting during an intermission can cause problems
	//	if (intermission_running)
	//		ExitIntermission ();
}


/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
void ClientDisconnect(edict_t* pEntity)
{
	if (g_fGameOver)
		return;

	char text[256] = "";
	if (!FStringNull(pEntity->v.netname))
		snprintf(text, sizeof(text), "- %s has left the game\n", STRING(pEntity->v.netname));
	text[sizeof(text) - 1] = 0;
	MESSAGE_BEGIN(MSG_ALL, gmsgSayText, NULL);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	CSound* pSound;
	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(pEntity));
	{
		// since this client isn't around to think anymore, reset their sound.
		if (pSound)
		{
			pSound->Reset();
		}
	}

	// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.takedamage = DAMAGE_NO; // don't attract autoaim
	pEntity->v.solid = SOLID_NOT;	   // nonsolid
	UTIL_SetOrigin(&pEntity->v, pEntity->v.origin);

	auto pPlayer = reinterpret_cast<CBasePlayer*>(GET_PRIVATE(pEntity));

	if (pPlayer)
	{
		if (pPlayer->m_pTank != NULL)
		{
			pPlayer->m_pTank->Use(pPlayer, pPlayer, USE_OFF, 0);
			pPlayer->m_pTank = NULL;
		}
	}

	g_pGameRules->ClientDisconnected(pEntity);
}


// called by ClientKill and DeadThink
void respawn(entvars_t* pev, bool fCopyCorpse)
{
	if (0 != gpGlobals->coop || 0 != gpGlobals->deathmatch)
	{
		if (fCopyCorpse)
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue(pev);
		}

		// respawn player
		GetClassPtr((CBasePlayer*)pev)->Spawn();
	}
	else
	{ // restart the entire server
		SERVER_COMMAND("reload\n");
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;

	CBasePlayer* pl = (CBasePlayer*)CBasePlayer::Instance(pev);

	if (pl->m_fNextSuicideTime > gpGlobals->time)
		return; // prevent suiciding too ofter

	pl->m_fNextSuicideTime = gpGlobals->time + 1; // don't let them suicide for 5 seconds after suiciding

	// have the player kill themself
	pev->health = 0;
	pl->Killed(pev, GIB_NEVER);

	//	pev->modelindex = g_ulModelIndexPlayer;
	//	pev->frags -= 2;		// extra penalty
	//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer(edict_t* pEntity)
{
	CBasePlayer* pPlayer;

	entvars_t* pev = &pEntity->v;

	pPlayer = GetClassPtr((CBasePlayer*)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn();

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;

	pPlayer->pev->iuser1 = 0; // disable any spec modes
	pPlayer->pev->iuser2 = 0;
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;



#if defined(_MSC_VER) || defined(WIN32)
typedef wchar_t uchar16;
typedef unsigned int uchar32;
#else
typedef unsigned short uchar16;
typedef wchar_t uchar32;
#endif

//-----------------------------------------------------------------------------
// Purpose: determine if a uchar32 represents a valid Unicode code point
//-----------------------------------------------------------------------------
bool Q_IsValidUChar32(uchar32 uVal)
{
	// Values > 0x10FFFF are explicitly invalid; ditto for UTF-16 surrogate halves,
	// values ending in FFFE or FFFF, or values in the 0x00FDD0-0x00FDEF reserved range
	return (uVal < 0x110000u) && ((uVal - 0x00D800u) > 0x7FFu) && ((uVal & 0xFFFFu) < 0xFFFEu) && ((uVal - 0x00FDD0u) > 0x1Fu);
}


// Decode one character from a UTF-8 encoded string. Treats 6-byte CESU-8 sequences
// as a single character, as if they were a correctly-encoded 4-byte UTF-8 sequence.
int Q_UTF8ToUChar32(const char* pUTF8_, uchar32& uValueOut, bool& bErrorOut)
{
	const uint8* pUTF8 = (const uint8*)pUTF8_;

	int nBytes = 1;
	uint32 uValue = pUTF8[0];
	uint32 uMinValue = 0;

	// 0....... single byte
	if (uValue < 0x80)
		goto decodeFinishedNoCheck;

	// Expecting at least a two-byte sequence with 0xC0 <= first <= 0xF7 (110...... and 11110...)
	if ((uValue - 0xC0u) > 0x37u || (pUTF8[1] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0xC0 << 6) + pUTF8[1] - 0x80;
	nBytes = 2;
	uMinValue = 0x80;

	// 110..... two-byte lead byte
	if ((uValue & (0x20 << 6)) == 0)
		goto decodeFinished;

	// Expecting at least a three-byte sequence
	if ((pUTF8[2] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0x20 << 12) + pUTF8[2] - 0x80;
	nBytes = 3;
	uMinValue = 0x800;

	// 1110.... three-byte lead byte
	if ((uValue & (0x10 << 12)) == 0)
		goto decodeFinishedMaybeCESU8;

	// Expecting a four-byte sequence, longest permissible in UTF-8
	if ((pUTF8[3] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0x10 << 18) + pUTF8[3] - 0x80;
	nBytes = 4;
	uMinValue = 0x10000;

	// 11110... four-byte lead byte. fall through to finished.

decodeFinished:
	if (uValue >= uMinValue && Q_IsValidUChar32(uValue))
	{
	decodeFinishedNoCheck:
		uValueOut = uValue;
		bErrorOut = false;
		return nBytes;
	}
decodeError:
	uValueOut = '?';
	bErrorOut = true;
	return nBytes;

decodeFinishedMaybeCESU8:
	// Do we have a full UTF-16 surrogate pair that's been UTF-8 encoded afterwards?
	// That is, do we have 0xD800-0xDBFF followed by 0xDC00-0xDFFF? If so, decode it all.
	if ((uValue - 0xD800u) < 0x400u && pUTF8[3] == 0xED && (uint8)(pUTF8[4] - 0xB0) < 0x10 && (pUTF8[5] & 0xC0) == 0x80)
	{
		uValue = 0x10000 + ((uValue - 0xD800u) << 10) + ((uint8)(pUTF8[4] - 0xB0) << 6) + pUTF8[5] - 0x80;
		nBytes = 6;
		uMinValue = 0x10000;
	}
	goto decodeFinished;
}



//-----------------------------------------------------------------------------
// Purpose: Returns true if UTF-8 string contains invalid sequences.
//-----------------------------------------------------------------------------
bool Q_UnicodeValidate(const char* pUTF8)
{
	bool bError = false;
	while ('\0' != *pUTF8)
	{
		uchar32 uVal;
		// Our UTF-8 decoder silently fixes up 6-byte CESU-8 (improperly re-encoded UTF-16) sequences.
		// However, these are technically not valid UTF-8. So if we eat 6 bytes at once, it's an error.
		int nCharSize = Q_UTF8ToUChar32(pUTF8, uVal, bError);
		if (bError || nCharSize == 6)
			return false;
		pUTF8 += nCharSize;
	}
	return true;
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say(edict_t* pEntity, bool teamonly)
{
	CBasePlayer* client;
	int j;
	char* p;
	char text[128];
	char szTemp[256];
	const char* cpSay = "say";
	const char* cpSayTeam = "say_team";
	const char* pcmd = CMD_ARGV(0);

	// We can get a raw string now, without the "say " prepended
	if (CMD_ARGC() == 0)
		return;

	entvars_t* pev = &pEntity->v;
	CBasePlayer* player = GetClassPtr((CBasePlayer*)pev);

	//Not yet.
	if (player->m_flNextChatTime > gpGlobals->time)
		return;

	if (!stricmp(pcmd, cpSay) || !stricmp(pcmd, cpSayTeam))
	{
		if (CMD_ARGC() >= 2)
		{
			p = (char*)CMD_ARGS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else // Raw text, need to prepend argv[0]
	{
		if (CMD_ARGC() >= 2)
		{
			sprintf(szTemp, "%s %s", (char*)pcmd, (char*)CMD_ARGS());
		}
		else
		{
			// Just a one word command, use the first word...sigh
			sprintf(szTemp, "%s", (char*)pcmd);
		}
		p = szTemp;
	}

	// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

	// make sure the text has content

	if (!p || '\0' == p[0] || !Q_UnicodeValidate(p))
		return; // no character found, so say nothing

	// Gaming-Test-Marker: jede Chat-Zeile landet mit Position im Server-Log
	// (`-condebug`), damit ein gemeldeter Bug spaeter genau dort wiedergefunden
	// wird. Rein lesend, keine Zustandsaenderung.
	ALERT(at_console, "[say] %s (%.0f %.0f %.0f): %s\n",
		STRING(pev->netname), pev->origin.x, pev->origin.y, pev->origin.z, p);

	// turn on color set 2  (color on,  no sound)
	// turn on color set 2  (color on,  no sound)
	if (player->IsObserver() && (teamonly))
		sprintf(text, "%c(SPEC) %s: ", 2, STRING(pEntity->v.netname));
	else if (teamonly)
		sprintf(text, "%c(TEAM) %s: ", 2, STRING(pEntity->v.netname));
	else
		sprintf(text, "%c%s: ", 2, STRING(pEntity->v.netname));

	j = sizeof(text) - 2 - strlen(text); // -2 for /n and null terminator
	if ((int)strlen(p) > j)
		p[j] = 0;

	strcat(text, p);
	strcat(text, "\n");


	player->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL;

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while (((client = (CBasePlayer*)UTIL_FindEntityByClassname(client, "player")) != NULL) && (!FNullEnt(client->edict())))
	{
		if (!client->pev)
			continue;

		if (client->edict() == pEntity)
			continue;

		if (!(client->IsNetClient())) // Not a client ? (should never be true)
			continue;

		// can the receiver hear the sender? or has he muted him?
		if (g_VoiceGameMgr.PlayerHasBlockedPlayer(client, player))
			continue;

		if (!player->IsObserver() && teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE)
			continue;

		// Spectators can only talk to other specs
		if (player->IsObserver() && teamonly)
			if (!client->IsObserver())
				continue;

		MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, client->pev);
		WRITE_BYTE(ENTINDEX(pEntity));
		WRITE_STRING(text);
		MESSAGE_END();
	}

	// print to the sending client
	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, &pEntity->v);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint(text);

	const char* temp;
	if (teamonly)
		temp = "say_team";
	else
		temp = "say";

	// team match?
	if (g_teamplay)
	{
		UTIL_LogPrintf("\"%s<%i><%s><%s>\" %s \"%s\"\n",
			STRING(pEntity->v.netname),
			GETPLAYERUSERID(pEntity),
			GETPLAYERAUTHID(pEntity),
			g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pEntity), "model"),
			temp,
			p);
	}
	else
	{
		UTIL_LogPrintf("\"%s<%i><%s><%i>\" %s \"%s\"\n",
			STRING(pEntity->v.netname),
			GETPLAYERUSERID(pEntity),
			GETPLAYERAUTHID(pEntity),
			GETPLAYERUSERID(pEntity),
			temp,
			p);
	}
}


/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/

//=========================================================
// Benannte Teleport-Punkte (Debug, nicht Teil des Retail-Spiels).
//
// Ablage: <gamedir>/waypoints.txt, eine Zeile je Punkt:
//     map|name|x|y|z|yaw|pitch
// Damit ueberleben die Punkte Kartenwechsel und Neustarts, und die Datei kann
// auch von Hand (oder aus dem Chat uebernommen) ergaenzt werden.
// Befehle: tp_add <name>, tp_list, tp_del <name>, tp <name> | tp <x> <y> <z> [yaw] [pitch]
//=========================================================
struct DbgWaypoint
{
	char szMap[32];
	char szName[64];
	Vector vecOrigin;
	float flYaw;
	float flPitch;
};

static void DbgWaypointPath(char* pszOut, size_t uiSize)
{
	char szGameDir[256];
	szGameDir[0] = '\0';
	GET_GAME_DIR(szGameDir);
	snprintf(pszOut, uiSize, "%s/waypoints.txt", szGameDir);
}

static int DbgWaypointLoad(const char* pszPath, DbgWaypoint* pOut, int iMax)
{
	FILE* f = fopen(pszPath, "r");
	if (f == nullptr)
		return 0;

	int iCount = 0;
	char szLine[512];

	while (iCount < iMax && fgets(szLine, sizeof(szLine), f) != nullptr)
	{
		if (szLine[0] == '#' || szLine[0] == '\n' || szLine[0] == '\r')
			continue;

		DbgWaypoint& wp = pOut[iCount];
		if (sscanf(szLine, "%31[^|]|%63[^|]|%f|%f|%f|%f|%f",
				wp.szMap, wp.szName,
				&wp.vecOrigin.x, &wp.vecOrigin.y, &wp.vecOrigin.z,
				&wp.flYaw, &wp.flPitch) == 7)
		{
			++iCount;
		}
	}

	fclose(f);
	return iCount;
}

static bool DbgWaypointSave(const char* pszPath, const DbgWaypoint* pIn, int iCount)
{
	FILE* f = fopen(pszPath, "w");
	if (f == nullptr)
		return false;

	fprintf(f, "# map|name|x|y|z|yaw|pitch  (Debug-Teleportpunkte)\n");
	for (int i = 0; i < iCount; ++i)
	{
		fprintf(f, "%s|%s|%.1f|%.1f|%.1f|%.1f|%.1f\n",
			pIn[i].szMap, pIn[i].szName,
			pIn[i].vecOrigin.x, pIn[i].vecOrigin.y, pIn[i].vecOrigin.z,
			pIn[i].flYaw, pIn[i].flPitch);
	}

	fclose(f);
	return true;
}

// Sucht einen Punkt der aktuellen Karte; -1 = nicht vorhanden.
static int DbgWaypointFind(const DbgWaypoint* pList, int iCount, const char* pszMap, const char* pszName)
{
	for (int i = 0; i < iCount; ++i)
	{
		if (FStrEq(pList[i].szMap, pszMap) && FStrEq(pList[i].szName, pszName))
			return i;
	}
	return -1;
}

// Debug helper for tp / dbg_focus: raises target in 16-unit steps (max 16)
// until a human-hull trace from it is free (monsters block). Returns false
// when no free position was found.
static bool DbgFindFreeSpot(edict_t* pEntity, Vector& target)
{
	TraceResult tr;
	for (int steps = 0; steps <= 16; steps++)
	{
		UTIL_TraceHull(target, target, dont_ignore_monsters, human_hull, pEntity, &tr);
		if (!tr.fStartSolid && !tr.fAllSolid)
			return true;
		target.z += 16;
	}
	return false;
}

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand(edict_t* pEntity)
{
	const char* pcmd = CMD_ARGV(0);
	const char* pstr;

	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;

	entvars_t* pev = &pEntity->v;

	auto player = GetClassPtr<CBasePlayer>(reinterpret_cast<CBasePlayer*>(&pEntity->v));

	if (FStrEq(pcmd, "say"))
	{
		Host_Say(pEntity, false);
	}
	else if (FStrEq(pcmd, "say_team"))
	{
		Host_Say(pEntity, true);
	}
	else if (FStrEq(pcmd, "fullupdate"))
	{
		player->ForceClientDllUpdate();
	}
	else if (FStrEq(pcmd, "give"))
	{
		if (0 != g_psv_cheats->value)
		{
			int iszItem = ALLOC_STRING(CMD_ARGV(1)); // Make a copy of the classname
			player->GiveNamedItem(STRING(iszItem));
		}
	}

	else if (FStrEq(pcmd, "drop"))
	{
		// player is dropping an item.
		player->DropPlayerItem((char*)CMD_ARGV(1));
	}
	else if (FStrEq(pcmd, "fov"))
	{
		if (0 != g_psv_cheats->value && CMD_ARGC() > 1)
		{
			player->m_iFOV = atoi(CMD_ARGV(1));
		}
		else
		{
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("\"fov\" is \"%d\"\n", (int)player->m_iFOV));
		}
	}
	else if (FStrEq(pcmd, "use"))
	{
		player->SelectItem((char*)CMD_ARGV(1));
	}
	else if (((pstr = strstr(pcmd, "weapon_")) != NULL) && (pstr == pcmd))
	{
		player->SelectItem(pcmd);
	}
	else if (FStrEq(pcmd, "lastinv"))
	{
		player->SelectLastItem();
	}
	else if (FStrEq(pcmd, "custupdate")) // "+custom" (MOUSE2, "Customize weapon")
	{
		player->PlayerCustomization();
	}
	else if (((pstr = strstr(pcmd, "cust_")) != NULL) && (pstr == pcmd))
	{
		// Retail FUN_100405b0: "cust_<Option><Wert>" -> aktive Waffe, Vtable-Slot 68 (Option ist das Zeichen hinter dem ersten '_')
		const char* pUnderscore = strchr(pcmd, '_');
		const char cOption = pUnderscore[1];
		const int iValue = atoi(pUnderscore + 2);
		if (cOption < '1')
			ALERT(at_console, "Illegal option number\n");
		else if (player->m_pActiveItem)
			player->m_pActiveItem->ApplyCustomSetting(cOption, iValue);
	}
	else if (FStrEq(pcmd, "spectate")) // clients wants to become a spectator
	{
		// always allow proxies to become a spectator
		if ((pev->flags & FL_PROXY) != 0 || 0 != allow_spectators.value)
		{
			edict_t* pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot(player);
			player->StartObserver(pev->origin, VARS(pentSpawnSpot)->angles);

			// notify other clients of player switching to spectator mode
			UTIL_ClientPrintAll(HUD_PRINTNOTIFY, UTIL_VarArgs("%s switched to spectator mode\n",
													 (!FStringNull(pev->netname) && STRING(pev->netname)[0] != 0) ? STRING(pev->netname) : "unconnected"));
		}
		else
			ClientPrint(pev, HUD_PRINTCONSOLE, "Spectator mode is disabled.\n");
	}
	else if (FStrEq(pcmd, "specmode")) // new spectator mode
	{
		if (player->IsObserver())
			player->Observer_SetMode(atoi(CMD_ARGV(1)));
	}
	else if (FStrEq(pcmd, "dbg_camtrack"))
	{
		// Debug only (sv_cheats, not part of the retail game): dbg_camtrack [1|0]
		// Lets decore_cam turn its yaw toward player 1 (see gunman_custom_entities.cpp, CDecoreCam::TrackThink).
		// Retail has no camera pan/tilt Think; this switch is off by default.
		if (0 == g_psv_cheats->value)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: dbg_camtrack [1|0] (needs sv_cheats 1)\n");
			return;
		}
		g_bDbgCamTrack = CMD_ARGC() < 2 || atoi(CMD_ARGV(1)) != 0;
		ClientPrint(pev, HUD_PRINTCONSOLE, g_bDbgCamTrack ? "dbg_camtrack: on\n" : "dbg_camtrack: off\n");
	}
	else if (FStrEq(pcmd, "dbg_god"))
	{
		// Debug god mode (sv_cheats only, not part of the retail game): dbg_god [1|0]
		// Sets FL_GODMODE on the player (combat.cpp ignores damage for it).
		// debug.cfg sets it after the map has loaded (see mod-src/xash3d/debug.cfg).
		if (0 == g_psv_cheats->value)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: dbg_god [1|0] (needs sv_cheats 1)\n");
			return;
		}
		const bool on = CMD_ARGC() < 2 || atoi(CMD_ARGV(1)) != 0;
		if (on)
			pev->flags |= FL_GODMODE;
		else
			pev->flags &= ~FL_GODMODE;
		ClientPrint(pev, HUD_PRINTCONSOLE, on ? "dbg_god: on\n" : "dbg_god: off\n");
	}
	else if (FStrEq(pcmd, "dbg_danger"))
	{
		// One-shot AI diagnostic (sv_cheats only): insert a DANGER sound at a
		// selected entity, e.g. dbg_danger monster_beak 1024 1.5. This exercises
		// real alert/schedule selection without making an explosive or damage.
		if (0 == g_psv_cheats->value || CMD_ARGC() < 2)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: dbg_danger <classname> [radius] [duration] (needs sv_cheats 1)\n");
			return;
		}

		CBaseEntity* pTarget = nullptr;
		for (int i = 1; i < gpGlobals->maxEntities; ++i)
		{
			edict_t* pEdict = INDEXENT(i);
			if (!pEdict->free && FClassnameIs(&pEdict->v, CMD_ARGV(1)))
			{
				pTarget = CBaseEntity::Instance(pEdict);
				break;
			}
		}
		if (!pTarget)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("dbg_danger: no entity '%s'\n", CMD_ARGV(1)));
			return;
		}

		const int iRadius = CMD_ARGC() >= 3 ? atoi(CMD_ARGV(2)) : 1024;
		const float flDuration = CMD_ARGC() >= 4 ? static_cast<float>(atof(CMD_ARGV(3))) : 1.5f;
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pTarget->pev->origin, iRadius, flDuration);
		ClientPrint(pev, HUD_PRINTCONSOLE,
			UTIL_VarArgs("dbg_danger: inserted at %s#%d radius=%d duration=%.1f\n",
				STRING(pTarget->pev->classname), ENTINDEX(pTarget->edict()), iRadius, flDuration));
	}
	else if (FStrEq(pcmd, "tp"))
	{
		// Debug teleport (sv_cheats only, not part of the retail game):
		//   tp <x> <y> <z> [yaw] [pitch]   freie Koordinaten
		//   tp <name>                      gespeicherter Punkt (waypoints.txt)
		// Freie Koordinaten werden mit einem Human-Hull-Trace geprueft und in
		// 16-Einheiten-Schritten angehoben, bis die Stelle frei ist.
		if (0 == g_psv_cheats->value || CMD_ARGC() < 2)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE,
				"usage: tp <x> <y> <z> [yaw] [pitch] | tp <name> | tp_list (needs sv_cheats 1)\n");
			return;
		}

		const char* pszMapNow = STRING(gpGlobals->mapname);
		Vector target;
		float flYawArg = 0.0f;
		float flPitchArg = 0.0f;
		bool bHaveAngles = false;
		bool bNamed = (CMD_ARGC() == 2);

		if (bNamed)
		{
			// Name: Punkt der aktuellen Karte suchen.
			char szPath[320];
			DbgWaypointPath(szPath, sizeof(szPath));

			static DbgWaypoint s_Waypoints[256];
			const int iCount = DbgWaypointLoad(szPath, s_Waypoints, 256);
			const int iFound = DbgWaypointFind(s_Waypoints, iCount, pszMapNow, CMD_ARGV(1));

			if (iFound < 0)
			{
				ClientPrint(pev, HUD_PRINTCONSOLE,
					UTIL_VarArgs("tp: '%s' ist auf Karte %s nicht gespeichert (tp_list zeigt alle)\n",
						CMD_ARGV(1), pszMapNow));
				return;
			}

			target = s_Waypoints[iFound].vecOrigin;
			flYawArg = s_Waypoints[iFound].flYaw;
			flPitchArg = s_Waypoints[iFound].flPitch;
			bHaveAngles = true;
		}
		else
		{
			if (CMD_ARGC() < 4)
			{
				ClientPrint(pev, HUD_PRINTCONSOLE,
					"usage: tp <x> <y> <z> [yaw] [pitch] | tp <name>\n");
				return;
			}

			target = Vector(atof(CMD_ARGV(1)), atof(CMD_ARGV(2)), atof(CMD_ARGV(3)));

			if (CMD_ARGC() >= 5)
			{
				flYawArg = atof(CMD_ARGV(4));
				bHaveAngles = true;

				if (CMD_ARGC() >= 6)
					flPitchArg = atof(CMD_ARGV(5));
			}
		}

		const Vector requested = target;
		if (!DbgFindFreeSpot(pEntity, target))
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "tp: no free position above the requested coordinates\n");
			return;
		}

		UTIL_SetOrigin(pev, target);
		pev->velocity = g_vecZero;
		pev->basevelocity = g_vecZero;

		// Blickrichtung setzen (aus Koordinaten oder aus dem gespeicherten
		// Punkt). yaw 0..360, pitch -90..90 wie in den [pos]-Zeilen.
		if (bHaveAngles)
		{
			pev->v_angle.y = flYawArg;
			pev->angles.y = flYawArg;
			pev->fixangle = 1;

			float flPitch = flPitchArg;
			if (flPitch > 90.0f)
				flPitch = 90.0f;
			else if (flPitch < -90.0f)
				flPitch = -90.0f;

			pev->v_angle.x = flPitch;
			pev->angles.x = flPitch;
		}

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"tp: requested (%.0f %.0f %.0f), placed at (%.0f %.0f %.0f), offset z +%.0f%s%s\n",
			requested.x, requested.y, requested.z, target.x, target.y, target.z,
			target.z - requested.z, bNamed ? ", point " : "", bNamed ? CMD_ARGV(1) : ""));
		DBG_LOG("[tp] map=%s target=%.1f %.1f %.1f yaw=%.1f pitch=%.1f%s%s",
			pszMapNow, target.x, target.y, target.z, flYawArg, flPitchArg,
			bNamed ? " point=" : "", bNamed ? CMD_ARGV(1) : "");
	}
	else if (FStrEq(pcmd, "tp_npc"))
	{
		// Spieler vor einen NPC setzen und ihn ansehen. Die Blickrichtung des
		// NPC kommt aus seinen Kartenwinkeln (pev->angles), damit die Kamera
		// nicht hinter dem Modell landet.
		// usage: tp_npc <classname|targetname> [abstand]
		if (0 == g_psv_cheats->value || CMD_ARGC() < 2)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE,
				"usage: tp_npc <classname|targetname> [distance] (needs sv_cheats 1)\n");
			return;
		}

		const char* pszWant = CMD_ARGV(1);
		const float flDist = CMD_ARGC() >= 3 ? (float)atof(CMD_ARGV(2)) : 220.0f;

		CBaseEntity* pNpc = UTIL_FindEntityByClassname(nullptr, pszWant);
		if (pNpc == nullptr)
			pNpc = UTIL_FindEntityByTargetname(nullptr, pszWant);
		if (pNpc == nullptr)
			pNpc = UTIL_FindEntityByClassname(nullptr, UTIL_VarArgs("monster_%s", pszWant));

		if (pNpc == nullptr)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE,
				UTIL_VarArgs("tp_npc: kein NPC '%s' auf dieser Karte\n", pszWant));
			return;
		}

		Vector vecNpcForward;
		AngleVectors(pNpc->pev->angles, &vecNpcForward, nullptr, nullptr);

		const Vector vecNpcEye = pNpc->pev->origin + Vector(0, 0, pNpc->pev->view_ofs.z);

		// Distanzleiter: auf engen Karten (REBAR2B/2C) gibt es bei der
		// Wunschdistanz keine freie Stelle. Statt abzubrechen, schrittweise
		// naeher herangehen und die benutzte Distanz mitloggen.
		Vector vecSpot = vecNpcEye;
		float flUsedDist = 0.0f;

		// Erst Distanz verkleinern, dann einen Kegel von +/-60 Grad um die
		// Blickachse abtasten: auf engen Karten steht der NPC oft direkt vor
		// einer Wand, waehrend seitlich Platz ist (Spieler bleibt in Sicht).
		static const float kFlYawOffsets[] = {0.0f, 20.0f, -20.0f, 40.0f, -40.0f, 60.0f, -60.0f};

		for (const float flScale : {1.0f, 0.75f, 0.5f, 0.35f, 0.25f})
		{
			const float flTry = flDist * flScale;

			if (flTry < 64.0f)
				continue;

			for (const float flYaw : kFlYawOffsets)
			{
				Vector vecDir;
				AngleVectors(Vector(0.0f, pNpc->pev->angles.y + flYaw, 0.0f), &vecDir, nullptr, nullptr);

				vecSpot = vecNpcEye + vecDir * flTry;
				vecSpot.z = pNpc->pev->origin.z + 32.0f;

				if (DbgFindFreeSpot(pEntity, vecSpot))
				{
					flUsedDist = flTry;
					break;
				}
			}

			if (flUsedDist > 0.0f)
				break;
		}

		if (flUsedDist <= 0.0f)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE,
				UTIL_VarArgs("tp_npc: keine freie Stelle zwischen 64 und %.0f Einheiten vor '%s'\n",
					flDist, pszWant));
			return;
		}

		if (flUsedDist < flDist)
			ClientPrint(pev, HUD_PRINTCONSOLE,
				UTIL_VarArgs("tp_npc: Wunschdistanz %.0f nicht frei, nutze %.0f\n", flDist, flUsedDist));

		UTIL_SetOrigin(pev, vecSpot);
		pev->velocity = g_vecZero;
		pev->basevelocity = g_vecZero;

		// Blick zur NPC-Mitte (Augenhoehe), Nickwinkel daraus.
		const Vector vecLook = vecNpcEye - vecSpot;
		Vector vecAngles = UTIL_VecToAngles(vecLook);
		// UTIL_VecToAngles liefert den Nickwinkel umgekehrt zu v_angle
		// (positiv = nach oben). v_angle.x ist in GoldSrc positiv = nach unten,
		// deshalb negieren - sonst schaut die Kamera auf die Fuesse.
		vecAngles.x = -vecAngles.x;
		pev->v_angle = vecAngles;
		pev->angles = vecAngles;
		pev->fixangle = 1;

		// Der NPC schaut oft in eine Wand (Kartenwinkel) und sieht den Spieler
		// dann nie. Fuer den Test den NPC zum Spieler drehen und das Ergebnis
		// pruefen, statt es zu vermuten.
		// Volle Ausrichtung (Yaw **und** Pitch): nur die Yaw zu drehen liess
		// den Pitch der Kartenwinkel stehen, und der Bandit schoss senkrecht
		// (Spielerbeobachtung 2026-11-08).
		const Vector vecPlayerEye = vecSpot + Vector(0, 0, 28.0f);
		Vector vecToPlayer = vecPlayerEye - (pNpc->pev->origin + Vector(0, 0, pNpc->pev->view_ofs.z));

		if (vecToPlayer.Length() > 1.0f)
		{
			Vector vecNpcAngles = UTIL_VecToAngles(vecToPlayer);
			// UTIL_VecToAngles liefert den Nickwinkel invertiert (positiv = nach
			// oben), v_angle.x ist positiv = nach unten.
			vecNpcAngles.x = -vecNpcAngles.x;
			vecNpcAngles.z = 0.0f;
			pNpc->pev->angles = vecNpcAngles;
			pNpc->pev->v_angle = vecNpcAngles;
			pNpc->pev->ideal_yaw = vecNpcAngles.y;
		}

		CBaseEntity* pPlayerEnt = CBaseEntity::Instance(pev);
		const bool bSees = pPlayerEnt != nullptr && pNpc->FVisible(pPlayerEnt) != 0;

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"tp_npc: %s bei (%.0f %.0f %.0f) -> Spieler (%.0f %.0f %.0f) yaw=%.0f pitch=%.0f npc_yaw=%.0f visible=%d\n",
			STRING(pNpc->pev->classname), pNpc->pev->origin.x, pNpc->pev->origin.y, pNpc->pev->origin.z,
			vecSpot.x, vecSpot.y, vecSpot.z, vecAngles.y, vecAngles.x,
			pNpc->pev->angles.y, bSees ? 1 : 0));
		DBG_LOG("[tp_npc] map=%s npc=%s npcorg=%.1f %.1f %.1f spot=%.1f %.1f %.1f yaw=%.1f pitch=%.1f",
			STRING(gpGlobals->mapname), STRING(pNpc->pev->classname),
			pNpc->pev->origin.x, pNpc->pev->origin.y, pNpc->pev->origin.z,
			vecSpot.x, vecSpot.y, vecSpot.z, vecAngles.y, vecAngles.x);
	}
	else if (FStrEq(pcmd, "tp_add"))
	{
		// Aktuelle Position als benannten Punkt der laufenden Karte speichern
		// (oder ueberschreiben). Ablage: <gamedir>/waypoints.txt.
		if (0 == g_psv_cheats->value || CMD_ARGC() < 2)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: tp_add <name> (needs sv_cheats 1)\n");
			return;
		}

		char szPath[320];
		DbgWaypointPath(szPath, sizeof(szPath));

		static DbgWaypoint s_List[256];
		int iCount = DbgWaypointLoad(szPath, s_List, 256);
		const char* pszMapNow = STRING(gpGlobals->mapname);
		const char* pszName = CMD_ARGV(1);

		int iFound = DbgWaypointFind(s_List, iCount, pszMapNow, pszName);
		if (iFound < 0)
		{
			if (iCount >= 256)
			{
				ClientPrint(pev, HUD_PRINTCONSOLE, "tp_add: list is full (256)\n");
				return;
			}
			iFound = iCount++;
			strncpy(s_List[iFound].szMap, pszMapNow, sizeof(s_List[iFound].szMap) - 1);
			s_List[iFound].szMap[sizeof(s_List[iFound].szMap) - 1] = '\0';
			strncpy(s_List[iFound].szName, pszName, sizeof(s_List[iFound].szName) - 1);
			s_List[iFound].szName[sizeof(s_List[iFound].szName) - 1] = '\0';
		}

		s_List[iFound].vecOrigin = pev->origin;
		s_List[iFound].flYaw = pev->v_angle.y;
		s_List[iFound].flPitch = pev->v_angle.x;

		if (!DbgWaypointSave(szPath, s_List, iCount))
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("tp_add: could not write %s\n", szPath));
			return;
		}

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"tp_add: %s auf %s = (%.1f %.1f %.1f) yaw=%.1f pitch=%.1f\n",
			pszName, pszMapNow, pev->origin.x, pev->origin.y, pev->origin.z,
			pev->v_angle.y, pev->v_angle.x));
		DBG_LOG("[tp] add map=%s point=%s %.1f %.1f %.1f yaw=%.1f pitch=%.1f",
			pszMapNow, pszName, pev->origin.x, pev->origin.y, pev->origin.z,
			pev->v_angle.y, pev->v_angle.x);
	}
	else if (FStrEq(pcmd, "tp_list"))
	{
		char szPath[320];
		DbgWaypointPath(szPath, sizeof(szPath));

		static DbgWaypoint s_List[256];
		const int iCount = DbgWaypointLoad(szPath, s_List, 256);
		const char* pszMapNow = STRING(gpGlobals->mapname);

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"tp_list: %d Punkt(e), Datei %s (aktuelle Karte %s)\n", iCount, szPath, pszMapNow));

		for (int i = 0; i < iCount; ++i)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
				"  %s%-16s %8.1f %8.1f %8.1f  yaw=%6.1f pitch=%5.1f\n",
				FStrEq(s_List[i].szMap, pszMapNow) ? "* " : "  ",
				s_List[i].szName, s_List[i].vecOrigin.x, s_List[i].vecOrigin.y,
				s_List[i].vecOrigin.z, s_List[i].flYaw, s_List[i].flPitch));
		}
	}
	else if (FStrEq(pcmd, "tp_del"))
	{
		if (0 == g_psv_cheats->value || CMD_ARGC() < 2)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: tp_del <name> (needs sv_cheats 1)\n");
			return;
		}

		char szPath[320];
		DbgWaypointPath(szPath, sizeof(szPath));

		static DbgWaypoint s_List[256];
		int iCount = DbgWaypointLoad(szPath, s_List, 256);
		const char* pszMapNow = STRING(gpGlobals->mapname);
		const int iFound = DbgWaypointFind(s_List, iCount, pszMapNow, CMD_ARGV(1));

		if (iFound < 0)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("tp_del: '%s' nicht auf %s\n", CMD_ARGV(1), pszMapNow));
			return;
		}

		for (int i = iFound; i < iCount - 1; ++i)
			s_List[i] = s_List[i + 1];
		--iCount;

		DbgWaypointSave(szPath, s_List, iCount);
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("tp_del: '%s' entfernt (%d verbleiben)\n", CMD_ARGV(1), iCount));
	}
	else if (FStrEq(pcmd, "dbg_eye"))
	{
		// Testhilfe: schaltet bei allen Gunnern das rote Auge ein (die
		// Helligkeit ist im Spiel sonst nur bei Sichtkontakt/Aerger 200).
		extern void DbgForceGunnerEyes();
		DbgForceGunnerEyes();
		ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_eye: Gunner-Augen aktiviert\n");
	}
	else if (FStrEq(pcmd, "dbg_scene"))
	{
		// Gaming-Test: Zustand von Spieler + Umgebung als Marker ins Log.
		// Reiner Lesezugriff, aendert nichts. Radius optional, Standard 512.
		if (0 == g_psv_cheats->value)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_scene: sv_cheats 1 erforderlich\n");
			return;
		}

		float flRadius = (CMD_ARGC() >= 2) ? (float)atof(CMD_ARGV(1)) : 512.0f;
		if (flRadius < 64.0f)
			flRadius = 64.0f;
		if (flRadius > 4096.0f)
			flRadius = 4096.0f;

		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(pEntity));

		ALERT(at_console, "[scene] ================ MARKER ================\n");
		ALERT(at_console, "[scene] map=%s skill=%d time=%.1f\n",
			STRING(gpGlobals->mapname), g_iSkillLevel, gpGlobals->time);

		// Spielerzustand
		ALERT(at_console, "[scene] player origin=(%.0f %.0f %.0f) v_angle=(%.1f %.1f %.1f)\n",
			pev->origin.x, pev->origin.y, pev->origin.z,
			pev->v_angle.x, pev->v_angle.y, pev->v_angle.z);
		ALERT(at_console, "[scene] player health=%.0f armor=%.0f vel=(%.0f %.0f %.0f) movetype=%d water=%d\n",
			pev->health, pev->armorvalue, pev->velocity.x, pev->velocity.y, pev->velocity.z,
			(int)pev->movetype, (int)pev->waterlevel);
		ALERT(at_console, "[scene] player flags=0x%x god=%d notarget=%d noclip=%d onground=%d\n",
			(int)pev->flags, (pev->flags & FL_GODMODE) ? 1 : 0,
			(pev->flags & FL_NOTARGET) ? 1 : 0, (pev->flags & FL_FLY) ? 1 : 0,
			(pev->flags & FL_ONGROUND) ? 1 : 0);
		if (pPlayer)
		{
			ALERT(at_console, "[scene] player weapon=%s\n",
				(pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->pev->classname)
					? STRING(pPlayer->m_pActiveItem->pev->classname) : "none");
		}

		// Blickfokus: Trace entlang der Blickrichtung
		Vector vecEye = pev->origin + pev->view_ofs;
		Vector vecAngles = pev->v_angle;
		Vector vecForward;
		AngleVectors(vecAngles, &vecForward, nullptr, nullptr);
		TraceResult trFocus;
		UTIL_TraceLine(vecEye, vecEye + vecForward * 8192.0f, dont_ignore_monsters, pEntity, &trFocus);
		CBaseEntity* pFocus = trFocus.pHit ? CBaseEntity::Instance(trFocus.pHit) : nullptr;
		ALERT(at_console, "[scene] view hit=%s targetname=%s at=(%.0f %.0f %.0f) dist=%.0f\n",
			(pFocus && pFocus->pev->classname) ? STRING(pFocus->pev->classname) : "(none)",
			(pFocus && pFocus->pev->targetname) ? STRING(pFocus->pev->targetname) : "",
			trFocus.vecEndPos.x, trFocus.vecEndPos.y, trFocus.vecEndPos.z,
			(trFocus.vecEndPos - vecEye).Length());

		// Umgebung
		ALERT(at_console, "[scene] entities within %.0f units:\n", flRadius);
		CBaseEntity* pEnt = NULL;
		int iIndex = 0;
		int iPrinted = 0;
		while ((pEnt = UTIL_FindEntityInSphere(pEnt, pev->origin, flRadius)) != NULL)
		{
			iIndex++;
			if (iPrinted >= 120)
				continue;
			iPrinted++;

			const char* pszClass = (pEnt->pev->classname) ? STRING(pEnt->pev->classname) : "?";
			const char* pszName = (pEnt->pev->targetname) ? STRING(pEnt->pev->targetname) : "";
			// Brush-Entities haben origin 0; fuer den Abstand den Hull-Mittelpunkt
			// nehmen, sonst zeigt die Liste unsinnige Tausenderwerte.
			Vector vecCenter = pEnt->Center();
			float flDist = (vecCenter - pev->origin).Length();
			if (flDist > flRadius)
				continue; // Brush-Entities mit origin 0 sonst mit Riesenabstand

			ALERT(at_console, "[scene]  #%d ent=%d %s name=\"%s\" center=(%.0f %.0f %.0f) dist=%.0f hp=%.0f\n",
				iPrinted, ENTINDEX(pEnt->edict()), pszClass, pszName,
				vecCenter.x, vecCenter.y, vecCenter.z, flDist, pEnt->pev->health);

			CBaseMonster* pMon = pEnt->MyMonsterPointer();
			if (pMon)
			{
				ALERT(at_console, "[scene]       state=%d enemy=%s seq=%d frame=%.0f schedule=%d\n",
					(int)pMon->m_MonsterState,
					(pMon->m_hEnemy && pMon->m_hEnemy->pev->classname) ? STRING(pMon->m_hEnemy->pev->classname) : "none",
					(int)pEnt->pev->sequence, pEnt->pev->frame, pMon->m_iScheduleIndex);
			}
		}
		ALERT(at_console, "[scene] entities total=%d (printed %d)\n", iIndex, iPrinted);
		ALERT(at_console, "[scene] ============== MARKER END ==============\n");

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("dbg_scene: %d Entities im Radius %.0f geloggt\n", iIndex, flRadius));
	}
	else if (FStrEq(pcmd, "dbg_bossinfo"))
	{
		CBaseEntity* pBoss = UTIL_FindEntityByClassname(NULL, "monster_endboss");
		if (!pBoss)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_bossinfo: kein monster_endboss\n");
			return;
		}
		extern void DbgEndBossInfo(CBaseEntity* pBossEnt, CBaseEntity* pPlayer);
		DbgEndBossInfo(pBoss, CBaseEntity::Instance(pEntity));
	}
	else if (FStrEq(pcmd, "dbg_kata"))
	{
		CBaseEntity* pBoss = UTIL_FindEntityByClassname(NULL, "monster_endboss");
		if (!pBoss)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_kata: kein monster_endboss auf dieser Map\n");
			return;
		}

		extern void DbgEndBossKata(CBaseEntity* pBossEnt, CBaseEntity* pPlayer);
		DbgEndBossKata(pBoss, CBaseEntity::Instance(pEntity));
		ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_kata: abgefeuert\n");
	}
	else if (FStrEq(pcmd, "dbg_stomp"))
	{
		CBaseEntity* pBoss = UTIL_FindEntityByClassname(NULL, "monster_endboss");
		if (!pBoss)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_stomp: kein monster_endboss auf dieser Map\n");
			return;
		}

		extern void DbgEndBossStomp(CBaseEntity* pBossEnt, CBaseEntity* pPlayer);
		DbgEndBossStomp(pBoss, CBaseEntity::Instance(pEntity));
		ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_stomp: ausgeloest\n");
	}
	else if (FStrEq(pcmd, "dbg_activate"))
	{
		// Testhilfe: holt den Endboss aus der Warte-Sequenz in den Kampfzustand
		// (Gegner = Spieler, Cooldowns zurueck). Vorher mit `dbg_focus
		// monster_endboss <dist>` hinstellen.
		CBaseEntity* pBoss = UTIL_FindEntityByClassname(NULL, "monster_endboss");

		if (!pBoss)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_activate: kein monster_endboss auf dieser Map\n");
			return;
		}

		extern void DbgEndBossActivate(CBaseEntity* pBossEnt, CBaseEntity* pPlayer);
		DbgEndBossActivate(pBoss, CBaseEntity::Instance(pEntity));

		ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_activate: Boss in Kampfzustand gesetzt\n");
	}
	else if (FStrEq(pcmd, "dbg_armswipe"))
	{
		// Testhilfe: setzt den Endboss als Gegner des Spielers und loest den
		// Arm-Schlag aus (Retail-Events 6/7, spielt events/tankmguns.sc).
		CBaseEntity* pBoss = UTIL_FindEntityByClassname(NULL, "monster_endboss");

		if (!pBoss)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_armswipe: kein monster_endboss auf dieser Map\n");
			return;
		}

		extern void DbgEndBossArmSwipe(CBaseEntity* pBossEnt, CBaseEntity* pPlayer);
		DbgEndBossArmSwipe(pBoss, CBaseEntity::Instance(pEntity));

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"dbg_armswipe: Boss bei (%.0f %.0f %.0f) ausgeloest\n",
			pBoss->pev->origin.x, pBoss->pev->origin.y, pBoss->pev->origin.z));
	}
	else if (FStrEq(pcmd, "dbg_tank"))
	{
		// Testhilfe: stellt den Spieler vor den naechsten vehicle_tank-Controller
		// und blickt auf ihn, damit der Use-Trace das Fahrzeug trifft.
		CBaseEntity* pTank = UTIL_FindEntityByClassname(NULL, "vehicle_tank");

		if (!pTank)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_tank: kein vehicle_tank auf dieser Map\n");
			return;
		}

		UTIL_MakeVectors(pTank->pev->angles);
		Vector vecSpot = pTank->pev->origin - gpGlobals->v_forward * 96.0f + Vector(0, 0, 24);

		UTIL_SetOrigin(pev, vecSpot);
		pev->v_angle = pTank->pev->angles;
		pev->angles = pev->v_angle;
		pev->fixangle = 1;

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"dbg_tank: Tank bei (%.0f %.0f %.0f), Spieler bei (%.0f %.0f %.0f)\n",
			pTank->pev->origin.x, pTank->pev->origin.y, pTank->pev->origin.z,
			vecSpot.x, vecSpot.y, vecSpot.z));
	}
	else if (FStrEq(pcmd, "dbg_sniper"))
	{
		// Testhilfe: spielt events/snipershot.sc vom Spieler aus ab, damit die
		// Sniper-Effektkette (gaussring-Kette + gausspoof) reproduzierbar ist.
		extern int g_iDbgSniperEvent;

		if (0 == g_psv_cheats->value || 0 == g_iDbgSniperEvent)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE,
				"dbg_sniper: nicht verfuegbar (sv_cheats 1 bzw. Event-Precache)\n");
			return;
		}

		Vector vecSrc = pev->origin + pev->view_ofs;
		PLAYBACK_EVENT_FULL(FEV_RELIABLE, pEntity, (unsigned short)g_iDbgSniperEvent, 0.0f,
			vecSrc, pev->v_angle, 0.0f, 0.0f, 1, 0, 0, 1);

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"dbg_sniper: Event %d bei (%.0f %.0f %.0f) yaw=%.0f\n",
			g_iDbgSniperEvent, vecSrc.x, vecSrc.y, vecSrc.z, pev->v_angle.y));
	}
	else if (FStrEq(pcmd, "dbg_eyemode"))
	{
		// Debug: Rendermodus der Gunner-Augen-Sprites in der Naehe setzen,
		// um das Durchscheinen (Glow ohne Tiefentest) zu pruefen.
		// usage: dbg_eyemode <0..5> [radius]
		if (0 == g_psv_cheats->value || CMD_ARGC() < 2)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: dbg_eyemode <0..5> [radius] (needs sv_cheats 1)\n");
			return;
		}

		const int iMode = atoi(CMD_ARGV(1));
		const float flRadius = CMD_ARGC() >= 3 ? (float)atof(CMD_ARGV(2)) : 512.0f;

		int iCount = 0;
		for (CBaseEntity* pEnt = UTIL_FindEntityInSphere(nullptr, pev->origin, flRadius);
			 pEnt != nullptr;
			 pEnt = UTIL_FindEntityInSphere(pEnt, pev->origin, flRadius))
		{
			if (!FClassnameIs(pEnt->pev, "env_sprite"))
				continue;

			pEnt->pev->rendermode = iMode;
			if (iMode == kRenderTransAdd)
				pEnt->pev->renderamt = 200;
			++iCount;
		}

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"dbg_eyemode: %d Sprite(s) auf rendermode %d gesetzt (Radius %.0f)\n",
			iCount, iMode, flRadius));
	}
	else if (FStrEq(pcmd, "dbg_kata"))
	{
		// Debug: einen Endboss-Kataball vor dem Spieler abwerfen (Test fuer
		// events/kataimpact.sc, Retail 0x100d2aa0 CEndBossKataball::KataTouch).
		if (0 == g_psv_cheats->value)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: dbg_kata (needs sv_cheats 1)\n");
			return;
		}

		UTIL_MakeVectors(pev->v_angle);

		// Variante "self": direkt am Spieler erzeugen, damit der Ball ihn sofort
		// beruehrt (Zweig "lebendes Ziel" -> katahitplayer-Kette der Karte).
		const bool bSelf = (CMD_ARGC() >= 2 && FStrEq(CMD_ARGV(1), "self"));
		const Vector vecSrc = bSelf
			? pev->origin + pev->view_ofs * 0.5f
			: pev->origin + pev->view_ofs + gpGlobals->v_forward * 32.0f;

		CBaseEntity* pBall = CBaseEntity::Create("endboss_kataball", vecSrc, pev->angles, pEntity);
		if (!pBall)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_kata: could not create endboss_kataball\n");
			return;
		}

		pBall->pev->velocity = bSelf ? g_vecZero : gpGlobals->v_forward * 900.0f;

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"dbg_kata: spawned at (%.0f %.0f %.0f), velocity (%.0f %.0f %.0f)\n",
			vecSrc.x, vecSrc.y, vecSrc.z,
			pBall->pev->velocity.x, pBall->pev->velocity.y, pBall->pev->velocity.z));
		DBG_LOG("[dbg] kataball spawned at %.1f %.1f %.1f",
			vecSrc.x, vecSrc.y, vecSrc.z);
	}
	else if (FStrEq(pcmd, "dbg_pos"))
	{
		// Debug: aktuelle Position als fertige Teleport-Zeile ausgeben
		// (gleiche Form wie die [pos]-Zeilen aus dbg_poslog).
		const Vector vOrigin = pev->origin;
		const char* pszMap = STRING(gpGlobals->mapname);
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs(
			"[pos] map=%s %.1f %.1f %.1f yaw=%.1f pitch=%.1f\n"
			"tp %.0f %.0f %.0f %.0f %.0f\n",
			pszMap, vOrigin.x, vOrigin.y, vOrigin.z, pev->v_angle.y, pev->v_angle.x,
			vOrigin.x, vOrigin.y, vOrigin.z, pev->v_angle.y, pev->v_angle.x));
		DBG_LOG("[pos] map=%s %.1f %.1f %.1f yaw=%.1f pitch=%.1f (dbg_pos)",
			pszMap, vOrigin.x, vOrigin.y, vOrigin.z, pev->v_angle.y, pev->v_angle.x);
	}
	else if (FStrEq(pcmd, "dbg_route") || FStrEq(pcmd, "dbg_route_expanded"))
	{
		// Single local-route probe for separating direct movement, triangulation,
		// and graph fallback from whatever schedule the selected monster runs.
		if (0 == g_psv_cheats->value || CMD_ARGC() < 5)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: dbg_route[_expanded] <classname> <x> <y> <z> [target-classname] (needs sv_cheats 1)\n");
			return;
		}

		CBaseMonster* pMonster = nullptr;
		edict_t* pEdict = INDEXENT(1);
		for (int i = 1; i < gpGlobals->maxEntities; ++i, ++pEdict)
		{
			if (pEdict->free || (pEdict->v.flags & FL_MONSTER) == 0 ||
				!FStrEq(STRING(pEdict->v.classname), CMD_ARGV(1)))
				continue;

			CBaseEntity* pEntity = CBaseEntity::Instance(pEdict);
			pMonster = pEntity ? pEntity->MyMonsterPointer() : nullptr;
			if (pMonster != nullptr)
				break;
		}

		if (pMonster == nullptr)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("dbg_route: no monster '%s'\n", CMD_ARGV(1)));
			return;
		}

		const Vector goal(atof(CMD_ARGV(2)), atof(CMD_ARGV(3)), atof(CMD_ARGV(4)));
		CBaseEntity* pProbeTarget = pMonster->m_hEnemy;
		if (CMD_ARGC() >= 6)
		{
			pProbeTarget = nullptr;
			float nearestDistance = 128.0f * 128.0f;
			for (CBaseEntity* pCandidate = UTIL_FindEntityInSphere(nullptr, goal, 128.0f);
				pCandidate; pCandidate = UTIL_FindEntityInSphere(pCandidate, goal, 128.0f))
			{
				if (!FStrEq(STRING(pCandidate->pev->classname), CMD_ARGV(5)))
					continue;
				const float candidateDistance = (pCandidate->pev->origin - goal).LengthSquared();
				if (candidateDistance < nearestDistance)
				{
					nearestDistance = candidateDistance;
					pProbeTarget = pCandidate;
				}
			}
		}
		float distanceToBlock = 0.0f;
		const int localMove = pMonster->CheckLocalMove(pMonster->pev->origin, goal, pProbeTarget, &distanceToBlock);
		Vector apex = g_vecZero;
		const int routeIndex = pMonster->m_iRouteIndex;
		if (routeIndex < 0 || routeIndex >= ROUTE_SIZE)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_route: monster route index is invalid\n");
			return;
		}
		const Vector oldFarSide = pMonster->m_Route[routeIndex].vecLocation;
		pMonster->m_Route[routeIndex].vecLocation = goal;
		g_bDbgRouteProbe = true;
		g_bDbgRouteExpandedOnly = FStrEq(pcmd, "dbg_route_expanded");
		const bool triangulated = localMove != LOCALMOVE_VALID && localMove != LOCALMOVE_INVALID_DONT_TRIANGULATE &&
			pMonster->FTriangulate(pMonster->pev->origin, goal, distanceToBlock, pProbeTarget, &apex);
		g_bDbgRouteExpandedOnly = false;
		g_bDbgRouteProbe = false;
		pMonster->m_Route[routeIndex].vecLocation = oldFarSide;
		ClientPrint(pev, HUD_PRINTCONSOLE,
			UTIL_VarArgs("%s local=%d dist=%.0f target=%s triangulated=%d apex=(%.0f %.0f %.0f)\n",
				pcmd, localMove, distanceToBlock, pProbeTarget ? STRING(pProbeTarget->pev->classname) : "none",
				triangulated, apex.x, apex.y, apex.z));
	}
	else if (FStrEq(pcmd, "dbg_monsters"))
	{
		// Diagnostic snapshot for controlled NPC tests (sv_cheats only).
		if (0 == g_psv_cheats->value)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_monsters needs sv_cheats 1\n");
			return;
		}

		int count = 0;
		edict_t* pEdict = INDEXENT(1);
		for (int i = 1; i < gpGlobals->maxEntities; ++i, ++pEdict)
		{
			if (pEdict->free || (pEdict->v.flags & FL_MONSTER) == 0)
				continue;

			CBaseEntity* pEntity = CBaseEntity::Instance(pEdict);
			CBaseMonster* pMonster = pEntity ? pEntity->MyMonsterPointer() : nullptr;
			if (pMonster == nullptr)
				continue;

			CBaseEntity* pEnemy = pMonster->m_hEnemy;
			const char* pszSchedule = pMonster->m_pSchedule && pMonster->m_pSchedule->pName
				? pMonster->m_pSchedule->pName
				: "<none>";
			ClientPrint(pev, HUD_PRINTCONSOLE,
				UTIL_VarArgs("dbg_monsters #%d %s class=%d alpha=%.0f enemy=%s#%d cine=%d state=%d activity=%d framerate=%.2f schedule=%s task=%d health=%.0f origin=(%.0f %.0f %.0f) route=%d waypoint=(%.0f %.0f %.0f)\n",
					ENTINDEX(pMonster->edict()), STRING(pMonster->pev->classname), pMonster->Classify(),
					pMonster->pev->renderamt,
					pEnemy ? STRING(pEnemy->pev->classname) : "<none>",
					pEnemy ? ENTINDEX(pEnemy->edict()) : 0,
					pMonster->m_pCine != nullptr, pMonster->m_MonsterState, pMonster->m_Activity, pMonster->pev->framerate, pszSchedule,
					pMonster->m_iScheduleIndex, pMonster->pev->health,
					pMonster->pev->origin.x, pMonster->pev->origin.y, pMonster->pev->origin.z,
					pMonster->m_iRouteIndex,
					pMonster->m_iRouteIndex >= 0 && pMonster->m_iRouteIndex < ROUTE_SIZE ? pMonster->m_Route[pMonster->m_iRouteIndex].vecLocation.x : 0.0f,
					pMonster->m_iRouteIndex >= 0 && pMonster->m_iRouteIndex < ROUTE_SIZE ? pMonster->m_Route[pMonster->m_iRouteIndex].vecLocation.y : 0.0f,
					pMonster->m_iRouteIndex >= 0 && pMonster->m_iRouteIndex < ROUTE_SIZE ? pMonster->m_Route[pMonster->m_iRouteIndex].vecLocation.z : 0.0f));
			if (FClassnameIs(pMonster->pev, "monster_rustbit"))
			{
				edict_t* pTargetEdict = INDEXENT(1);
				for (int targetIndex = 1; targetIndex < gpGlobals->maxEntities; ++targetIndex, ++pTargetEdict)
				{
					if (pTargetEdict->free || !FClassnameIs(&pTargetEdict->v, "monster_xenome"))
						continue;

					CBaseEntity* pTarget = CBaseEntity::Instance(pTargetEdict);
					if (!pTarget)
						continue;

					ClientPrint(pev, HUD_PRINTCONSOLE,
						UTIL_VarArgs("dbg_monsters pair rustbit#%d xenome#%d relation=%d in_view=%d visible=%d distance=%.0f yaw=%.0f fov=%.2f\n",
							ENTINDEX(pMonster->edict()), ENTINDEX(pTarget->edict()), pMonster->IRelationship(pTarget),
							pMonster->FInViewCone(pTarget), pMonster->FVisible(pTarget),
							(pTarget->pev->origin - pMonster->pev->origin).Length(), pMonster->pev->angles.y,
							pMonster->m_flFieldOfView));
				}
			}
			++count;
		}
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("dbg_monsters: %d monster(s)\n", count));
		g_flDbgMonsterStateTraceUntil = gpGlobals->time + 12.0f;
	}
	else if (FStrEq(pcmd, "dbg_focus"))
	{
		// Debug focus (sv_cheats only): dbg_focus <classname> [distance]
		// Finds the nearest entity with that classname, places the player at
		// 'distance' units (default 160) from it on the side the player comes
		// from (free-spot check as in tp), and turns the view to its center.
		// Afterwards dbg_anim / dbg_sched act on it through the crosshair.
		if (0 == g_psv_cheats->value || CMD_ARGC() < 2)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: dbg_focus <classname> [distance] | dbg_focus off (needs sv_cheats 1)\n");
			return;
		}

		if (FStrEq(CMD_ARGV(1), "off"))
		{
			player->m_hDbgFocus = nullptr;
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_focus: view unlocked\n");
			return;
		}

		const float distance = CMD_ARGC() >= 3 ? atof(CMD_ARGV(2)) : 160.0f;
		CBaseEntity* pTarget = nullptr;
		float bestDist = 1e9f;
		for (CBaseEntity* pEnt = UTIL_FindEntityByClassname(nullptr, CMD_ARGV(1)); pEnt; pEnt = UTIL_FindEntityByClassname(pEnt, CMD_ARGV(1)))
		{
			const float d = (pEnt->pev->origin - pev->origin).Length();
			if (d < bestDist)
			{
				bestDist = d;
				pTarget = pEnt;
			}
		}
		if (pTarget == nullptr)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("dbg_focus: no entity '%s' on this map\n", CMD_ARGV(1)));
			return;
		}

		// Stand on the player's side of the target, horizontally.
		Vector side = pev->origin - pTarget->pev->origin;
		side.z = 0;
		if (side.Length() < 1)
			side = Vector(-1, 0, 0);
		side = side.Normalize() * distance;

		// Floor correction: drop from the target's height down to the first solid
		// surface at the spot, so the player stands at the target's level and not on
		// a ledge above it (a ledge gave bandits "GetPathToEnemy failed" on END1).
		// Floor level under the target (the origin can be above the floor, e.g. the
		// endboss origin is 200 units above its floor on END1).
		Vector spot = pTarget->pev->origin + side;
		{
			TraceResult ground;
			const Vector targetTop(pTarget->pev->origin.x, pTarget->pev->origin.y, pTarget->pev->origin.z);
			const Vector targetBottom(pTarget->pev->origin.x, pTarget->pev->origin.y, pTarget->pev->origin.z - 1024);
			UTIL_TraceLine(targetTop, targetBottom, ignore_monsters, pTarget->edict(), &ground);
			const float floorZ = ground.flFraction < 1.0f ? ground.vecEndPos.z : pTarget->pev->origin.z;

			TraceResult spotGround;
			const Vector spotTop(spot.x, spot.y, floorZ + 36);
			const Vector spotBottom(spot.x, spot.y, floorZ - 128);
			UTIL_TraceHull(spotTop, spotBottom, dont_ignore_monsters, human_hull, pEntity, &spotGround);
			if (!spotGround.fStartSolid && spotGround.flFraction < 1.0f)
				spot = spotGround.vecEndPos;
			else
				spot.z = floorZ;
		}
		if (!DbgFindFreeSpot(pEntity, spot))
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg_focus: no free spot next to the target\n");
			return;
		}

		UTIL_SetOrigin(pev, spot);
		pev->velocity = g_vecZero;
		pev->basevelocity = g_vecZero;

		// View toward the target's center, applied by the engine (fixangle).
		const Vector eye = pev->origin + pev->view_ofs;
		Vector ang = UTIL_VecToAngles(pTarget->Center() - eye);
		ang.x = -ang.x; // VEC_TO_ANGLES liefert den Pitch invertiert (Blick nach oben = positiv); Spiel: negativ = oben
		pev->v_angle = ang;
		pev->angles = ang;
		pev->fixangle = 1;
		// Lock the view on the target's center from now on (CBasePlayer::PreThink).
		player->m_hDbgFocus = pTarget;

		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("dbg_focus (locked): %s at (%.0f %.0f %.0f), player at (%.0f %.0f %.0f), view %.0f %.0f\n", STRING(pTarget->pev->classname), pTarget->pev->origin.x, pTarget->pev->origin.y, pTarget->pev->origin.z, spot.x, spot.y, spot.z, ang.x, ang.y));
	}
	else if (FStrEq(pcmd, "dbg_anim") || FStrEq(pcmd, "dbg_sched"))
	{
		// Debug hook (sv_cheats only, not part of the retail game): drives the
		// explicitly focused monster, or falls back to the monster under the crosshair.
		//   dbg_anim <activity number | sequence name>  -> SetActivity / SetSequenceByName
		//   dbg_sched <schedule type number>            -> ChangeSchedule(GetScheduleOfType(n))
		// Use with a map that contains the monster, e.g. END1 for monster_endboss.
		if (0 == g_psv_cheats->value || CMD_ARGC() < 2)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "usage: dbg_anim <activity|sequence> | dbg_sched <type> (needs sv_cheats 1, aim at a monster)\n");
			return;
		}

		CBaseMonster* pMonster = player->m_hDbgFocus ? player->m_hDbgFocus->MyMonsterPointer() : nullptr;
		if (pMonster == nullptr)
		{
			UTIL_MakeVectors(pev->v_angle);
			const Vector vecSrc = pev->origin + pev->view_ofs;
			TraceResult tr;
			UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 2048, dont_ignore_monsters, pEntity, &tr);

			CBaseEntity* pHit = tr.pHit ? CBaseEntity::Instance(tr.pHit) : nullptr;
			pMonster = pHit ? pHit->MyMonsterPointer() : nullptr;
		}
		if (pMonster == nullptr)
		{
			ClientPrint(pev, HUD_PRINTCONSOLE, "dbg: no monster in the crosshair\n");
			return;
		}

		if (FStrEq(pcmd, "dbg_anim"))
		{
			const char* arg = CMD_ARGV(1);
			// A pure number is an activity id (ACT_*), anything else a sequence name.
			const bool isNumber = arg[0] != '\0' && (arg[0] >= '0' && arg[0] <= '9');
			if (isNumber)
			{
				pMonster->SetActivity(static_cast<Activity>(atoi(arg)));
			}
			else
			{
				pMonster->SetSequenceByName(arg);
			}
		}
		else
		{
			pMonster->ChangeSchedule(pMonster->GetScheduleOfType(atoi(CMD_ARGV(1))));
		}
	}
	else if (FStrEq(pcmd, "closemenus"))
	{
		// just ignore it
	}
	else if (FStrEq(pcmd, "follownext")) // follow next player
	{
		if (player->IsObserver())
			player->Observer_FindNextPlayer(atoi(CMD_ARGV(1)) != 0);
	}
	else if (g_pGameRules->ClientCommand(player, pcmd))
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy(command, pcmd, 127);
		command[127] = '\0';
		// First parse the name and remove any %'s
		for (char* pApersand = command; pApersand != NULL && *pApersand != 0; pApersand++)
		{
			// Replace it with a space
			if (*pApersand == '%')
				*pApersand = ' ';
		}

		// tell the user they entered an unknown command
		ClientPrint(&pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs("Unknown command: %s\n", command));
	}
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged(edict_t* pEntity, char* infobuffer)
{
	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if (!FStringNull(pEntity->v.netname) && STRING(pEntity->v.netname)[0] != 0 && !FStrEq(STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue(infobuffer, "name")))
	{
		char sName[256];
		char* pName = g_engfuncs.pfnInfoKeyValue(infobuffer, "name");
		strncpy(sName, pName, sizeof(sName) - 1);
		sName[sizeof(sName) - 1] = '\0';

		// First parse the name and remove any %'s
		for (char* pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++)
		{
			// Replace it with a space
			if (*pApersand == '%')
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pEntity), infobuffer, "name", sName);

		if (gpGlobals->maxClients > 1)
		{
			char text[256];
			sprintf(text, "* %s changed name to %s\n", STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
			MESSAGE_BEGIN(MSG_ALL, gmsgSayText, NULL);
			WRITE_BYTE(ENTINDEX(pEntity));
			WRITE_STRING(text);
			MESSAGE_END();
		}

		// team match?
		if (g_teamplay)
		{
			UTIL_LogPrintf("\"%s<%i><%s><%s>\" changed name to \"%s\"\n",
				STRING(pEntity->v.netname),
				GETPLAYERUSERID(pEntity),
				GETPLAYERAUTHID(pEntity),
				g_engfuncs.pfnInfoKeyValue(infobuffer, "model"),
				g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
		}
		else
		{
			UTIL_LogPrintf("\"%s<%i><%s><%i>\" changed name to \"%s\"\n",
				STRING(pEntity->v.netname),
				GETPLAYERUSERID(pEntity),
				GETPLAYERAUTHID(pEntity),
				GETPLAYERUSERID(pEntity),
				g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
		}
	}

	g_pGameRules->ClientUserInfoChanged(GetClassPtr((CBasePlayer*)&pEntity->v), infobuffer);
}

static int g_serveractive = 0;

void ServerDeactivate()
{
	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate
	if (g_serveractive != 1)
	{
		return;
	}

	g_serveractive = 0;

	// Peform any shutdown operations here...
	//
}

void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	int i;
	CBaseEntity* pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;

	// Clients have not been initialized yet
	for (i = 0; i < edictCount; i++)
	{
		if (0 != pEdictList[i].free)
			continue;

		// Clients aren't necessarily initialized until ClientPutInServer()
		if ((i > 0 && i <= clientMax) || !pEdictList[i].pvPrivateData)
			continue;

		pClass = CBaseEntity::Instance(&pEdictList[i]);
		// Activate this entity if it's got a class & isn't dormant
		if (pClass && (pClass->pev->flags & FL_DORMANT) == 0)
		{
			pClass->Activate();
		}
		else
		{
			ALERT(at_console, "Can't instance %s\n", STRING(pEdictList[i].v.classname));
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();
}


/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PreThink();
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PostThink();
}



void ParmsNewLevel()
{
}


void ParmsChangeLevel()
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA* pSaveData = (SAVERESTOREDATA*)gpGlobals->pSaveData;

	if (pSaveData)
		pSaveData->connectionCount = BuildChangeList(pSaveData->levelList, MAX_LEVEL_CONNECTIONS);
}

static std::vector<std::string> g_MapsToLoad;

static void LoadNextMap()
{
	const std::string mapName = std::move(g_MapsToLoad.back());
	g_MapsToLoad.pop_back();

	pmove->Con_Printf("Loading map \"%s\" automatically (%d left)\n",
		mapName.c_str(), static_cast<int>(g_MapsToLoad.size() + 1));

	if (g_MapsToLoad.empty())
	{
		pmove->Con_Printf("Loading last map\n");
		g_MapsToLoad.shrink_to_fit();
	}

	SERVER_COMMAND(UTIL_VarArgs("map \"%s\"\n", mapName.c_str()));
}

static void LoadAllMaps()
{
	if (!g_MapsToLoad.empty())
	{
		pmove->Con_Printf("Already loading all maps (%d remaining)\nUse sv_stop_loading_all_maps to stop\n",
			static_cast<int>(g_MapsToLoad.size()));
		return;
	}

	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;

	const char* fileName = g_pFileSystem->FindFirst("maps/*.bsp", &handle);

	if (fileName != nullptr)
	{
		do
		{
			std::string mapName = fileName;
			mapName.resize(mapName.size() - 4);

			if (std::find_if(g_MapsToLoad.begin(), g_MapsToLoad.end(), [=](const auto& candidate)
					{ return 0 == stricmp(candidate.c_str(), mapName.c_str()); }) == g_MapsToLoad.end())
			{
				g_MapsToLoad.push_back(std::move(mapName));
			}
		} while ((fileName = g_pFileSystem->FindNext(handle)) != nullptr);

		g_pFileSystem->FindClose(handle);

		// Sort in reverse order so the first map in alphabetic order is loaded first.
		std::sort(g_MapsToLoad.begin(), g_MapsToLoad.end(), [](const auto& lhs, const auto& rhs)
			{ return rhs < lhs; });
	}

	if (!g_MapsToLoad.empty())
	{
		if (CMD_ARGC() == 2)
		{
			const char* firstMapToLoad = CMD_ARGV(1);

			// Clear out all maps that would have been loaded before this one.
			if (auto it = std::find(g_MapsToLoad.begin(), g_MapsToLoad.end(), firstMapToLoad);
				it != g_MapsToLoad.end())
			{
				const std::size_t numberOfMapsToSkip = g_MapsToLoad.size() - (it - g_MapsToLoad.begin());

				g_MapsToLoad.erase(it + 1, g_MapsToLoad.end());

				pmove->Con_Printf("Skipping %d maps to start with \"%s\"\n",
					static_cast<int>(numberOfMapsToSkip), g_MapsToLoad.back().c_str());
			}
			else
			{
				pmove->Con_Printf("Unknown map \"%s\", starting from beginning\n", firstMapToLoad);
			}
		}

		pmove->Con_Printf("Loading %d maps one at a time to generate files\n", static_cast<int>(g_MapsToLoad.size()));

		// Load the first map right now.
		LoadNextMap();
	}
	else
	{
		pmove->Con_Printf("No maps to load\n");
	}
}

void InitMapLoadingUtils()
{
	g_engfuncs.pfnAddServerCommand("sv_load_all_maps", &LoadAllMaps);
	// Escape hatch in case the command is executed in error.
	g_engfuncs.pfnAddServerCommand("sv_stop_loading_all_maps", []()
		{ g_MapsToLoad.clear(); });
}

static bool g_LastAllowBunnyHoppingState = false;

//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame()
{
	if (g_pGameRules)
		g_pGameRules->Think();

	if (g_fGameOver)
		return;

	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;

	const bool allowBunnyHopping = sv_allowbunnyhopping.value != 0;

	if (allowBunnyHopping != g_LastAllowBunnyHoppingState)
	{
		g_LastAllowBunnyHoppingState = allowBunnyHopping;

		for (int i = 1; i <= gpGlobals->maxClients; ++i)
		{
			auto player = UTIL_PlayerByIndex(i);

			if (!player)
			{
				continue;
			}

			g_engfuncs.pfnSetPhysicsKeyValue(player->edict(), "bj", UTIL_dtos1(allowBunnyHopping ? 1 : 0));
		}
	}

	// If we're loading all maps then change maps after 3 seconds (time starts at 1)
	// to give the game time to generate files.
	if (!g_MapsToLoad.empty() && gpGlobals->time > 4)
	{
		LoadNextMap();
	}
}


void ClientPrecache()
{
	// setup precaches always needed
	PrecacheSound("player/sprayer.wav"); // spray paint sound for PreAlpha

	// PrecacheSound("player/pl_jumpland2.wav");		// UNDONE: play 2x step sound

	PrecacheSound("player/pl_fallpain2.wav");
	PrecacheSound("player/pl_fallpain3.wav");

	PrecacheSound("player/pl_step1.wav"); // walk on concrete
	PrecacheSound("player/pl_step2.wav");
	PrecacheSound("player/pl_step3.wav");
	PrecacheSound("player/pl_step4.wav");

	PrecacheSound("common/npc_step1.wav"); // NPC walk on concrete
	PrecacheSound("common/npc_step2.wav");
	PrecacheSound("common/npc_step3.wav");
	PrecacheSound("common/npc_step4.wav");

	PrecacheSound("player/pl_metal1.wav"); // walk on metal
	PrecacheSound("player/pl_metal2.wav");
	PrecacheSound("player/pl_metal3.wav");
	PrecacheSound("player/pl_metal4.wav");

	PrecacheSound("player/pl_dirt1.wav"); // walk on dirt
	PrecacheSound("player/pl_dirt2.wav");
	PrecacheSound("player/pl_dirt3.wav");
	PrecacheSound("player/pl_dirt4.wav");

	PrecacheSound("player/pl_duct1.wav"); // walk in duct
	PrecacheSound("player/pl_duct2.wav");
	PrecacheSound("player/pl_duct3.wav");
	PrecacheSound("player/pl_duct4.wav");

	PrecacheSound("player/pl_grate1.wav"); // walk on grate
	PrecacheSound("player/pl_grate2.wav");
	PrecacheSound("player/pl_grate3.wav");
	PrecacheSound("player/pl_grate4.wav");

	PrecacheSound("player/pl_slosh1.wav"); // walk in shallow water
	PrecacheSound("player/pl_slosh2.wav");
	PrecacheSound("player/pl_slosh3.wav");
	PrecacheSound("player/pl_slosh4.wav");

	PrecacheSound("player/pl_tile1.wav"); // walk on tile
	PrecacheSound("player/pl_tile2.wav");
	PrecacheSound("player/pl_tile3.wav");
	PrecacheSound("player/pl_tile4.wav");
	PrecacheSound("player/pl_tile5.wav");

	PrecacheSound("player/pl_swim1.wav"); // breathe bubbles
	PrecacheSound("player/pl_swim2.wav");
	PrecacheSound("player/pl_swim3.wav");
	PrecacheSound("player/pl_swim4.wav");

	PrecacheSound("player/pl_ladder1.wav"); // climb ladder rung
	PrecacheSound("player/pl_ladder2.wav");
	PrecacheSound("player/pl_ladder3.wav");
	PrecacheSound("player/pl_ladder4.wav");

	PrecacheSound("player/pl_wade1.wav"); // wade in water
	PrecacheSound("player/pl_wade2.wav");
	PrecacheSound("player/pl_wade3.wav");
	PrecacheSound("player/pl_wade4.wav");

	PrecacheSound("debris/wood1.wav"); // hit wood texture
	PrecacheSound("debris/wood2.wav");
	PrecacheSound("debris/wood3.wav");

	PrecacheSound("plats/train_use1.wav"); // use a train
	PrecacheSound("plats/vehicle_ignition.wav");

	PrecacheSound("buttons/spark5.wav"); // hit computer texture
	PrecacheSound("buttons/spark6.wav");
	PrecacheSound("debris/glass1.wav");
	PrecacheSound("debris/glass2.wav");
	PrecacheSound("debris/glass3.wav");

	PrecacheSound(SOUND_FLASHLIGHT_ON);
	PrecacheSound(SOUND_FLASHLIGHT_OFF);

	// player gib sounds
	PrecacheSound("common/bodysplat.wav");

	// player pain sounds
	PrecacheSound("player/pl_pain2.wav");
	PrecacheSound("player/pl_pain4.wav");
	PrecacheSound("player/pl_pain5.wav");
	PrecacheSound("player/pl_pain6.wav");
	PrecacheSound("player/pl_pain7.wav");

	PrecacheModel("models/player.mdl");

	// hud sounds

	PrecacheSound("common/wpn_hudoff.wav");
	PrecacheSound("common/wpn_hudon.wav");
	PrecacheSound("common/wpn_moveselect.wav");
	PrecacheSound("common/wpn_select.wav");
	PrecacheSound("common/wpn_denyselect.wav");


	// geiger sounds

	PrecacheSound("player/geiger6.wav");
	PrecacheSound("player/geiger5.wav");
	PrecacheSound("player/geiger4.wav");
	PrecacheSound("player/geiger3.wav");
	PrecacheSound("player/geiger2.wav");
	PrecacheSound("player/geiger1.wav");

	if (giPrecacheGrunt)
		UTIL_PrecacheOther("monster_human_grunt");
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char* GetGameDescription()
{
	if (g_pGameRules) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Half-Life";
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
void Sys_Error(const char* error_string)
{
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
void PlayerCustomization(edict_t* pEntity, customization_t* pCust)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (!pPlayer)
	{
		ALERT(at_console, "PlayerCustomization:  Couldn't get player!\n");
		return;
	}

	if (!pCust)
	{
		ALERT(at_console, "PlayerCustomization:  NULL customization!\n");
		return;
	}

	switch (pCust->resource.type)
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames(pCust->nUserData2); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT(at_console, "PlayerCustomization:  Unknown customization type!\n");
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
void SpectatorConnect(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect();
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
void SpectatorDisconnect(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect();
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
void SpectatorThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink();
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
void SetupVisibility(edict_t* pViewEntity, edict_t* pClient, unsigned char** pvs, unsigned char** pas)
{
	Vector org;
	edict_t* pView = pClient;

	// Find the client's PVS
	if (pViewEntity)
	{
		pView = pViewEntity;
	}

	if ((pClient->v.flags & FL_PROXY) != 0)
	{
		*pvs = NULL; // the spectator proxy sees
		*pas = NULL; // and hears everything
		return;
	}

	org = pView->v.origin + pView->v.view_ofs;
	if ((pView->v.flags & FL_DUCKING) != 0)
	{
		org = org + (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);
	}

	*pvs = ENGINE_SET_PVS((float*)&org);
	*pas = ENGINE_SET_PAS((float*)&org);
}

#include "entity_state.h"

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise

state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
e and ent are the entity that is being added to the update, if 1 is returned
host is the player's edict of the player whom we are sending the update to
player is 1 if the ent/e is a player and 0 otherwise
pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
int AddToFullPack(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, int player, unsigned char* pSet)
{
	// Entities with an index greater than this will corrupt the client's heap because 
	// the index is sent with only 11 bits of precision (2^11 == 2048).
	// So we don't send them, just like having too many entities would result
	// in the entity not being sent.
	if (e >= MAX_EDICTS)
	{
		return 0;
	}

	int i;

	auto entity = reinterpret_cast<CBaseEntity*>(GET_PRIVATE(ent));

	// don't send if flagged for NODRAW and it's not the host getting the message
	if ((ent->v.effects & EF_NODRAW) != 0 &&
		(ent != host))
		return 0;

	// Ignore ents without valid / visible models
	if (0 == ent->v.modelindex || !STRING(ent->v.model))
		return 0;

	// Don't send spectators to other players
	if ((ent->v.flags & FL_SPECTATOR) != 0 && (ent != host))
	{
		return 0;
	}

	// Ignore if not the host and not touching a PVS/PAS leaf
	// If pSet is NULL, then the test will always succeed and the entity will be added to the update
	if (ent != host)
	{
		if (!ENGINE_CHECK_VISIBILITY((const struct edict_s*)ent, pSet))
		{
			return 0;
		}
	}


	// Don't send entity to local client if the client says it's predicting the entity itself.
	if ((ent->v.flags & FL_SKIPLOCALHOST) != 0)
	{
		if ((hostflags & 1) != 0 && (ent->v.owner == host))
			return 0;
	}

	if (0 != host->v.groupinfo)
	{
		UTIL_SetGroupTrace(host->v.groupinfo, GROUP_OP_AND);

		// Should always be set, of course
		if (0 != ent->v.groupinfo)
		{
			if (g_groupop == GROUP_OP_AND)
			{
				if ((ent->v.groupinfo & host->v.groupinfo) == 0)
					return 0;
			}
			else if (g_groupop == GROUP_OP_NAND)
			{
				if ((ent->v.groupinfo & host->v.groupinfo) != 0)
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset(state, 0, sizeof(*state));

	// Assign index so we can track this entity from frame to frame and
	//  delta from it.
	state->number = e;
	state->entityType = ENTITY_NORMAL;

	// Flag custom entities.
	if ((ent->v.flags & FL_CUSTOMENTITY) != 0)
	{
		state->entityType = ENTITY_BEAM;
	}

	//
	// Copy state data
	//

	// Round animtime to nearest millisecond
	state->animtime = (int)(1000.0 * ent->v.animtime) / 1000.0;

	memcpy(state->origin, ent->v.origin, 3 * sizeof(float));
	memcpy(state->angles, ent->v.angles, 3 * sizeof(float));
	memcpy(state->mins, ent->v.mins, 3 * sizeof(float));
	memcpy(state->maxs, ent->v.maxs, 3 * sizeof(float));

	memcpy(state->startpos, ent->v.startpos, 3 * sizeof(float));
	memcpy(state->endpos, ent->v.endpos, 3 * sizeof(float));

	state->impacttime = ent->v.impacttime;
	state->starttime = ent->v.starttime;

	state->modelindex = ent->v.modelindex;

	state->frame = ent->v.frame;

	state->skin = ent->v.skin;
	state->effects = ent->v.effects;

	// This non-player entity is being moved by the game .dll and not the physics simulation system
	//  make sure that we interpolate it's position on the client if it moves
	/*
	if (0 == player &&
		0 != ent->v.animtime &&
		ent->v.velocity[0] == 0 &&
		ent->v.velocity[1] == 0 &&
		ent->v.velocity[2] == 0)
	{
		state->eflags |= EFLAG_SLERP;
	}
	*/

	if ((ent->v.flags & FL_FLY) != 0)
	{
		state->eflags |= EFLAG_SLERP;
	}
	else
	{
		state->eflags &= ~EFLAG_SLERP;
	}

	state->eflags |= entity->m_EFlags;

	state->scale = ent->v.scale;
	state->solid = ent->v.solid;
	state->colormap = ent->v.colormap;

	state->movetype = ent->v.movetype;
	state->sequence = ent->v.sequence;
	state->framerate = ent->v.framerate;
	state->body = ent->v.body;

	for (i = 0; i < 4; i++)
	{
		state->controller[i] = ent->v.controller[i];
	}

	for (i = 0; i < 2; i++)
	{
		state->blending[i] = ent->v.blending[i];
	}

	state->rendermode = ent->v.rendermode;
	state->renderamt = ent->v.renderamt;
	state->renderfx = ent->v.renderfx;
	state->rendercolor.r = ent->v.rendercolor.x;
	state->rendercolor.g = ent->v.rendercolor.y;
	state->rendercolor.b = ent->v.rendercolor.z;

	state->aiment = 0;
	if (ent->v.aiment)
	{
		state->aiment = ENTINDEX(ent->v.aiment);
	}

	state->owner = 0;
	if (ent->v.owner)
	{
		int owner = ENTINDEX(ent->v.owner);

		// Only care if owned by a player
		if (owner >= 1 && owner <= gpGlobals->maxClients)
		{
			state->owner = owner;
		}
	}

	// HACK:  Somewhat...
	// Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
	if (0 == player)
	{
		state->playerclass = ent->v.playerclass;
	}

	// Special stuff for players only
	if (0 != player)
	{
		memcpy(state->basevelocity, ent->v.basevelocity, 3 * sizeof(float));

		state->weaponmodel = MODEL_INDEX(STRING(ent->v.weaponmodel));
		state->gaitsequence = ent->v.gaitsequence;
		state->spectator = ent->v.flags & FL_SPECTATOR;
		state->friction = ent->v.friction;

		state->gravity = ent->v.gravity;
		//		state->team			= ent->v.team;
		//
		state->usehull = (ent->v.flags & FL_DUCKING) != 0 ? 1 : 0;
		state->health = ent->v.health;
	}

	CBaseEntity* pEntity = static_cast<CBaseEntity*>(GET_PRIVATE(ent));
	if (pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
		state->eflags |= EFLAG_FLESH_SOUND;
	else
		state->eflags &= ~EFLAG_FLESH_SOUND;

	return 1;
}

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
void CreateBaseline(int player, int eindex, struct entity_state_s* baseline, struct edict_s* entity, int playermodelindex, Vector* player_mins, Vector* player_maxs)
{
	baseline->origin = entity->v.origin;
	baseline->angles = entity->v.angles;
	baseline->frame = entity->v.frame;
	baseline->skin = (short)entity->v.skin;

	// render information
	baseline->rendermode = (byte)entity->v.rendermode;
	baseline->renderamt = (byte)entity->v.renderamt;
	baseline->rendercolor.r = (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g = (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b = (byte)entity->v.rendercolor.z;
	baseline->renderfx = (byte)entity->v.renderfx;

	if (0 != player)
	{
		baseline->mins = *player_mins;
		baseline->maxs = *player_maxs;

		baseline->colormap = eindex;
		baseline->modelindex = playermodelindex;
		baseline->friction = 1.0;
		baseline->movetype = MOVETYPE_WALK;

		baseline->scale = entity->v.scale;
		baseline->solid = SOLID_SLIDEBOX;
		baseline->framerate = 1.0;
		baseline->gravity = 1.0;
	}
	else
	{
		baseline->mins = entity->v.mins;
		baseline->maxs = entity->v.maxs;

		baseline->colormap = 0;
		baseline->modelindex = entity->v.modelindex; //SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype = entity->v.movetype;

		baseline->scale = entity->v.scale;
		baseline->solid = entity->v.solid;
		baseline->framerate = entity->v.framerate;
		baseline->gravity = entity->v.gravity;
	}
}

typedef struct
{
	char name[32];
	int field;
} entity_field_alias_t;

#define FIELD_ORIGIN0 0
#define FIELD_ORIGIN1 1
#define FIELD_ORIGIN2 2
#define FIELD_ANGLES0 3
#define FIELD_ANGLES1 4
#define FIELD_ANGLES2 5

static entity_field_alias_t entity_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
		{"angles[0]", 0},
		{"angles[1]", 0},
		{"angles[2]", 0},
};

void Entity_FieldInit(struct delta_s* pFields)
{
	entity_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN0].name);
	entity_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN1].name);
	entity_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN2].name);
	entity_field_alias[FIELD_ANGLES0].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES0].name);
	entity_field_alias[FIELD_ANGLES1].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES1].name);
	entity_field_alias[FIELD_ANGLES2].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES2].name);
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
FIXME:  Move to script
==================
*/
void Entity_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	static bool initialized = false;

	if (!initialized)
	{
		Entity_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	const bool localplayer = (t->number - 1) == ENGINE_CURRENT_PLAYER();
	if (localplayer)
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}

	if ((t->impacttime != 0) && (t->starttime != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);

		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES2].field);
	}

	if ((t->movetype == MOVETYPE_FOLLOW) &&
		(t->aiment != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
	else if (t->aiment != f->aiment)
	{
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
}

static entity_field_alias_t player_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
};

void Player_FieldInit(struct delta_s* pFields)
{
	player_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN0].name);
	player_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN1].name);
	player_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN2].name);
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
void Player_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	static bool initialized = false;

	if (!initialized)
	{
		Player_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	const bool localplayer = (t->number - 1) == ENGINE_CURRENT_PLAYER();
	if (localplayer)
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}

	if ((t->movetype == MOVETYPE_FOLLOW) &&
		(t->aiment != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
	else if (t->aiment != f->aiment)
	{
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
}

#define CUSTOMFIELD_ORIGIN0 0
#define CUSTOMFIELD_ORIGIN1 1
#define CUSTOMFIELD_ORIGIN2 2
#define CUSTOMFIELD_ANGLES0 3
#define CUSTOMFIELD_ANGLES1 4
#define CUSTOMFIELD_ANGLES2 5
#define CUSTOMFIELD_SKIN 6
#define CUSTOMFIELD_SEQUENCE 7
#define CUSTOMFIELD_ANIMTIME 8

entity_field_alias_t custom_entity_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
		{"angles[0]", 0},
		{"angles[1]", 0},
		{"angles[2]", 0},
		{"skin", 0},
		{"sequence", 0},
		{"animtime", 0},
};

void Custom_Entity_FieldInit(struct delta_s* pFields)
{
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].name);
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].name);
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].name);
	custom_entity_field_alias[CUSTOMFIELD_SKIN].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].name);
	custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].name);
	custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].name);
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
void Custom_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	int beamType;
	static bool initialized = false;

	if (!initialized)
	{
		Custom_Entity_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	beamType = t->rendermode & 0x0f;

	if (beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field);
	}

	if (beamType != BEAM_POINTS)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field);
	}

	if (beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field);
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ((int)f->animtime == (int)t->animtime)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field);
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders()
{
	DELTA_ADDENCODER("Entity_Encode", Entity_Encode);
	DELTA_ADDENCODER("Custom_Encode", Custom_Encode);
	DELTA_ADDENCODER("Player_Encode", Player_Encode);
}

int GetWeaponData(struct edict_s* player, struct weapon_data_s* info)
{
	memset(info, 0, MAX_WEAPONS * sizeof(weapon_data_t));

#if defined(CLIENT_WEAPONS)
	int i;
	weapon_data_t* item;
	entvars_t* pev = &player->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));
	CBasePlayerWeapon* gun;

	ItemInfo II;

	if (!pl)
		return 1;

	// go through all of the weapons and make a list of the ones to pack
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (pl->m_rgpPlayerItems[i])
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem* pPlayerItem = pl->m_rgpPlayerItems[i];

			while (pPlayerItem)
			{
				gun = pPlayerItem->GetWeaponPtr();
				if (gun && gun->UseDecrement())
				{
					// Get The ID.
					memset(&II, 0, sizeof(II));
					gun->GetItemInfo(&II);

					if (II.iId >= 0 && II.iId < MAX_WEAPONS)
					{
						item = &info[II.iId];

						item->m_iId = II.iId;
						item->m_iClip = gun->m_iClip;

						item->m_flTimeWeaponIdle = V_max(gun->m_flTimeWeaponIdle, -0.001f);
						item->m_flNextPrimaryAttack = V_max(gun->m_flNextPrimaryAttack, -0.001f);
						item->m_flNextSecondaryAttack = V_max(gun->m_flNextSecondaryAttack, -0.001f);
						item->m_fInReload = static_cast<int>(gun->m_fInReload);
						item->m_fInSpecialReload = gun->m_fInSpecialReload;
						item->fuser1 = V_max(gun->pev->fuser1, -0.001f);
						item->fuser2 = gun->m_flStartThrow;
						item->fuser3 = gun->m_flReleaseThrow;
						item->iuser1 = gun->m_chargeReady;
						item->iuser2 = gun->m_fInAttack;
						item->iuser3 = gun->m_fireState;

						gun->GetWeaponData(*item);

						//						item->m_flPumpTime				= V_max( gun->m_flPumpTime, -0.001f );
					}
				}
				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}
#endif
	return 1;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
void UpdateClientData(const edict_t* ent, int sendweapons, struct clientdata_s* cd)
{
	if (!ent || !ent->pvPrivateData)
		return;
	entvars_t* pev = (entvars_t*)&ent->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));
	entvars_t* pevOrg = NULL;

	// if user is spectating different player in First person, override some vars
	if (pl && pl->pev->iuser1 == OBS_IN_EYE)
	{
		if (pl->m_hObserverTarget)
		{
			pevOrg = pev;
			pev = pl->m_hObserverTarget->pev;
			pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));
		}
	}

	cd->flags = pev->flags;
	cd->health = pev->health;

	cd->viewmodel = MODEL_INDEX(STRING(pev->viewmodel));

	cd->waterlevel = pev->waterlevel;
	cd->watertype = pev->watertype;
	cd->weapons = pev->weapons;

	// Vectors
	cd->origin = pev->origin;
	cd->velocity = pev->velocity;
	cd->view_ofs = pev->view_ofs;
	cd->punchangle = pev->punchangle;

	cd->bInDuck = pev->bInDuck;
	cd->flTimeStepSound = pev->flTimeStepSound;
	cd->flDuckTime = pev->flDuckTime;
	cd->flSwimTime = pev->flSwimTime;
	cd->waterjumptime = pev->teleport_time;

	strcpy(cd->physinfo, ENGINE_GETPHYSINFO(ent));

	cd->maxspeed = pev->maxspeed;
	cd->fov = pl->m_iFOV;
	cd->weaponanim = pev->weaponanim;

	cd->pushmsec = pev->pushmsec;

	//Spectator mode
	if (pevOrg != NULL)
	{
		// don't use spec vars from chased player
		cd->iuser1 = pevOrg->iuser1;
		cd->iuser2 = pevOrg->iuser2;
	}
	else
	{
		cd->iuser1 = pev->iuser1;
		cd->iuser2 = pev->iuser2;
	}



#if defined(CLIENT_WEAPONS)
	if (0 != sendweapons)
	{
		if (pl)
		{
			cd->m_flNextAttack = pl->m_flNextAttack;
			cd->fuser2 = pl->m_flNextAmmoBurn;
			cd->fuser3 = pl->m_flAmmoStartCharge;
			cd->vuser1.x = pl->ammo_9mm;
			cd->vuser1.y = pl->ammo_357;
			cd->vuser1.z = pl->ammo_argrens;
			cd->ammo_nails = pl->ammo_bolts;
			cd->ammo_shells = pl->ammo_buckshot;
			cd->ammo_rockets = pl->ammo_rockets;
			cd->ammo_cells = pl->ammo_uranium;
			cd->vuser2.x = pl->ammo_hornets;


			if (pl->m_pActiveItem)
			{
				CBasePlayerWeapon* gun = pl->m_pActiveItem->GetWeaponPtr();
				if (gun && gun->UseDecrement())
				{
					ItemInfo II;
					memset(&II, 0, sizeof(II));
					gun->GetItemInfo(&II);

					cd->m_iId = II.iId;

					cd->vuser3.z = gun->m_iSecondaryAmmoType;
					cd->vuser4.x = gun->m_iPrimaryAmmoType;
					cd->vuser4.y = pl->m_rgAmmo[gun->m_iPrimaryAmmoType];
					cd->vuser4.z = pl->m_rgAmmo[gun->m_iSecondaryAmmoType];

					if (pl->m_pActiveItem->m_iId == WEAPON_RPG)
					{
						cd->vuser2.y = static_cast<vec_t>(((CRpg*)pl->m_pActiveItem)->m_fSpotActive);
						cd->vuser2.z = ((CRpg*)pl->m_pActiveItem)->m_cActiveRockets;
					}
				}
			}
		}
	}
#endif
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
void CmdStart(const edict_t* player, const struct usercmd_s* cmd, unsigned int random_seed)
{
	entvars_t* pev = (entvars_t*)&player->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));

	if (!pl)
		return;

	if (pl->pev->groupinfo != 0)
	{
		UTIL_SetGroupTrace(pl->pev->groupinfo, GROUP_OP_AND);
	}

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd(const edict_t* player)
{
	entvars_t* pev = (entvars_t*)&player->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));

	if (!pl)
		return;
	if (pl->pev->groupinfo != 0)
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int ConnectionlessPacket(const struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size)
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	return static_cast<int>(PM_GetHullBounds(hullnumber, mins, maxs));
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
void CreateInstancedBaselines()
{
	int iret = 0;
	entity_state_t state;

	memset(&state, 0, sizeof(state));

	// Create any additional baselines here for things like grendates, etc.
	// iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

	// Destroy objects.
	//UTIL_Remove( pc );
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int InconsistentFile(const edict_t* player, const char* filename, char* disconnect_message)
{
	// Server doesn't care?
	if (CVAR_GET_FLOAT("mp_consistency") != 1)
		return 0;

	// Default behavior is to kick the player
	sprintf(disconnect_message, "Server is enforcing file consistency for %s\n", filename);

	// Kick now with specified disconnect message.
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
int AllowLagCompensation()
{
	return 1;
}
