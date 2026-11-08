/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
//
//  hud_msg.cpp
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"

#include "particleman.h"
#include "pmtrace.h"
#include "ev_hldm.h"
extern IParticleMan* g_pParticleMan;

extern BEAM* pBeam;
extern BEAM* pBeam2;


/// USER-DEFINED SERVER MESSAGE HANDLERS

float g_flClientShakeStart = -1000.0f;
float g_flClientShakeAmplitude = 0.0f;
float g_flClientShakeDuration = 0.0f;
float g_flClientShakeFrequency = 0.0f;

bool CHud::MsgFunc_RockGib(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// Nutzdaten des Servers: Origin, Anzahl, Geschwindigkeit, Scale (x50), Skin.
	const float flX = READ_COORD();
	const float flY = READ_COORD();
	const float flZ = READ_COORD();
	const int iCount = READ_BYTE();
	const int iSpeed = READ_SHORT();
	const float flScale = static_cast<float>(READ_BYTE()) / 50.0f;
	const int iSkin = READ_BYTE();

	static bool s_bProbe = false;
	if (!s_bProbe)
	{
		s_bProbe = true;
		gEngfuncs.Con_Printf("PARTLIMIT max_particles=%.0f cl_maxparticles=%.0f r_maxparticles=%.0f\n",
			CVAR_GET_FLOAT("max_particles"), CVAR_GET_FLOAT("cl_maxparticles"),
			CVAR_GET_FLOAT("r_maxparticles"));
	}

	EV_RetailRockShards(Vector(flX, flY, flZ), static_cast<float>(iSpeed), flScale, iCount, iSkin,
		FTENT_SMOKETRAIL | FTENT_ROTATE);
	return true;
}

bool CHud::MsgFunc_CameraShake(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// Protokoll wie UTIL_ScreenShake: Amplitude und Dauer in 4.12-Fixkomma, Frequenz in 8.8.
	const float flAmplitude = static_cast<float>(READ_SHORT()) / 4096.0f;
	const float flDuration = static_cast<float>(READ_SHORT()) / 4096.0f;
	const float flFrequency = static_cast<float>(READ_SHORT()) / 256.0f;

	g_flClientShakeStart = gEngfuncs.GetClientTime();
	g_flClientShakeAmplitude = flAmplitude;
	g_flClientShakeDuration = flDuration;
	g_flClientShakeFrequency = flFrequency;
	return true;
}

bool CHud::MsgFunc_ResetHUD(const char* pszName, int iSize, void* pbuf)
{
	ASSERT(iSize == 0);

	// clear all hud data
	HUDLIST* pList = m_pHudList;

	while (pList)
	{
		if (pList->p)
			pList->p->Reset();
		pList = pList->pNext;
	}

	//Reset weapon bits.
	m_iWeaponBits = 0ULL;

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	return true;
}

void CAM_ToFirstPerson();

void CHud::MsgFunc_ViewMode(const char* pszName, int iSize, void* pbuf)
{
	CAM_ToFirstPerson();
}

void CHud::MsgFunc_InitHUD(const char* pszName, int iSize, void* pbuf)
{
	// prepare all hud data
	HUDLIST* pList = m_pHudList;

	while (pList)
	{
		if (pList->p)
			pList->p->InitHUDData();
		pList = pList->pNext;
	}


	//TODO: needs to be called on every map change, not just when starting a new game
	if (g_pParticleMan)
		g_pParticleMan->ResetParticles();

	//Probably not a good place to put this.
	pBeam = pBeam2 = NULL;
}


bool CHud::MsgFunc_GameMode(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	//Note: this user message could be updated to include multiple gamemodes, so make sure this checks for game mode 1
	//See CHalfLifeTeamplay::UpdateGameMode
	//TODO: define game mode constants
	m_Teamplay = READ_BYTE() == 1;

#ifdef STEAM_RICH_PRESENCE
	if (m_Teamplay)
		gEngfuncs.pfnClientCmd("richpresence_gamemode Teamplay\n");
	else
		gEngfuncs.pfnClientCmd("richpresence_gamemode\n"); // reset

	gEngfuncs.pfnClientCmd("richpresence_update\n");
#endif

	return true;
}


bool CHud::MsgFunc_Damage(const char* pszName, int iSize, void* pbuf)
{
	int armor, blood;
	Vector from;
	int i;
	float count;

	BEGIN_READ(pbuf, iSize);
	armor = READ_BYTE();
	blood = READ_BYTE();

	for (i = 0; i < 3; i++)
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);

	if (count < 10)
		count = 10;

	// TODO: kick viewangles,  show damage visually

	return true;
}

bool CHud::MsgFunc_Concuss(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iConcussionEffect = READ_BYTE();
	if (0 != m_iConcussionEffect)
	{
		int r, g, b;
		UnpackRGB(r, g, b, RGB_YELLOWISH);
		this->m_StatusIcons.EnableIcon("dmg_concuss", r, g, b);
	}
	else
		this->m_StatusIcons.DisableIcon("dmg_concuss");
	return true;
}

bool CHud::MsgFunc_Weapons(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	const std::uint64_t lowerBits = READ_LONG();
	const std::uint64_t upperBits = READ_LONG();

	m_iWeaponBits = (lowerBits & 0XFFFFFFFF) | ((upperBits & 0XFFFFFFFF) << 32ULL);

	return true;
}
