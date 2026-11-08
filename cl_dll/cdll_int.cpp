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
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include <SDL2/SDL_messagebox.h>

#include "hud.h"
#include "cl_util.h"
#include "netadr.h"
#include "interface.h"
#include "gunman_engine_target.h"
//#include "vgui_schememanager.h"

#include "pm_shared.h"

#include <string.h>
#include "vgui_int.h"

#include "Platform.h"
#include "Exports.h"

#include "tri.h"
#include "vgui_TeamFortressViewport.h"
#include "filesystem_utils.h"

cl_enginefunc_t gEngfuncs;
CHud gHUD;
TeamFortressViewport* gViewPort = NULL;


#include "particleman.h"
IParticleMan* g_pParticleMan = nullptr;

void CL_LoadParticleMan();
void CL_UnloadParticleMan();

void InitInput();
void EV_HookEvents();
void EV_UpdateClusterExplosionEffects();
void EV_UpdateEffectTubes();
void EV_UpdateEffectQuads();
void EV_ResetClusterExplosionEffects();
void IN_Commands();

/*
================================
HUD_GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int DLLEXPORT HUD_GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	return static_cast<int>(PM_GetHullBounds(hullnumber, mins, maxs));
}

/*
================================
HUD_ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int DLLEXPORT HUD_ConnectionlessPacket(const struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size)
{
	//	RecClConnectionlessPacket(net_from, args, response_buffer, response_buffer_size);

	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

void DLLEXPORT HUD_PlayerMoveInit(struct playermove_s* ppmove)
{
	//	RecClClientMoveInit(ppmove);

	PM_Init(ppmove);
}

char DLLEXPORT HUD_PlayerMoveTexture(char* name)
{
	//	RecClClientTextureType(name);

	return PM_FindTextureType(name);
}

void DLLEXPORT HUD_PlayerMove(struct playermove_s* ppmove, int server)
{
	//	RecClClientMove(ppmove, server);

	PM_Move(ppmove, server);
}

static bool CL_InitClient()
{
	EV_HookEvents();
	CL_LoadParticleMan();

	if (!FileSystem_LoadFileSystem())
	{
		return false;
	}

	// get tracker interface, if any
	return true;
}

int DLLEXPORT Initialize(cl_enginefunc_t* pEnginefuncs, int iVersion)
{
	gEngfuncs = *pEnginefuncs;

	//	RecClInitialize(pEnginefuncs, iVersion);

	if (iVersion != CLDLL_INTERFACE_VERSION)
		return 0;

	memcpy(&gEngfuncs, pEnginefuncs, sizeof(cl_enginefunc_t));

	const bool result = CL_InitClient();

	if (!result)
	{
		gEngfuncs.Con_DPrintf("Error initializing client\n");
		gEngfuncs.pfnClientCmd("quit\n");
		return 0;
	}

	return 1;
}


/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/

int DLLEXPORT HUD_VidInit()
{
	//	RecClHudVidInit();
	gHUD.VidInit();

	VGui_Startup();

	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all 
the hud variables.
==========================
*/

void DLLEXPORT HUD_Init()
{
	//	RecClHudInit();
	InitInput();
	gHUD.Init();
	Scheme_Init();
}


/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/

int DLLEXPORT HUD_Redraw(float time, int intermission)
{
	//	RecClHudRedraw(time, intermission);
	EV_UpdateClusterExplosionEffects();
	EV_UpdateEffectTubes();
	EV_UpdateEffectQuads();

	gHUD.Redraw(time, 0 != intermission);

	return 1;
}


/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int DLLEXPORT HUD_UpdateClientData(client_data_t* pcldata, float flTime)
{
	//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();

	return static_cast<int>(gHUD.UpdateClientData(pcldata, flTime));
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

void DLLEXPORT HUD_Reset()
{
	//	RecClHudReset();
	EV_ResetClusterExplosionEffects();

	gHUD.VidInit();
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/

void DLLEXPORT HUD_Frame(double time)
{
	//	RecClHudFrame(time);

	static int s_iTestFrame = 0;
	static int s_iTestHold = 0;
	static int s_iTestPhase = 0;
	static bool s_bTestCvarReady = false;

	GetClientVoiceMgr()->Frame(time);

	// Selbsttest-Treiber: `gm_test_fire 1` laesst den Client selbst schiessen,
	// damit Event-Handler ohne Tastatureingabe pruefbar sind (Xash3D
	// protokolliert Event-Ausfuehrung nicht). Ablauf: Waffe waehlen, Angriff
	// ueber mehrere Frames halten, loslassen — ein Impuls im selben Frame
	// reicht nicht, weil die Engine den Tastenzustand erst im Folgeframe liest.
	if (!s_bTestCvarReady)
	{
		gEngfuncs.pfnRegisterVariable("gm_test_fire", "0", FCVAR_ARCHIVE);
		gEngfuncs.pfnRegisterVariable("gm_test_weapon", "weapon_shotgun", FCVAR_ARCHIVE);
	// Haltedauer je Durchlauf in Frames (Standard 26). Laenger halten, wenn ein
	// Effekt erst nach Sekunden Dauerfeuer kommt (z. B. Beamgun-Hitze ab 80).
	gEngfuncs.pfnRegisterVariable("gm_test_hold", "26", FCVAR_ARCHIVE);
	// 1 = Testtreiber drueckt die Sekundaertaste statt der Primaeren; noetig
	// fuer den Sniper-Modus der Gauss-Pistole (Zoom halten, beim Loslassen
	// feuert SecondaryAttack -> events/snipershot.sc).
	gEngfuncs.pfnRegisterVariable("gm_test_attack2", "0", FCVAR_ARCHIVE);
	// Anlaufzeit in Sekunden vor dem ersten Schuss (0 = kein Anlauf).
	gEngfuncs.pfnRegisterVariable("gm_test_walk", "0", FCVAR_ARCHIVE);
	// Wirft nach dem Anlauf einen Endboss-Kataball ab (Test fuer kataimpact.sc).
	gEngfuncs.pfnRegisterVariable("gm_test_kata", "0", FCVAR_ARCHIVE);
		s_bTestCvarReady = true;
	}

	s_iTestFrame++;

	// Screenshot-Takt fuer den visuellen Abgleich: `gm_shot <sekunden>` nimmt
	// in diesem Abstand ein Bild auf (0 = aus). Die Bilder landen im
	// Spielverzeichnis und lassen sich danach mit den Retail-Effekten
	// vergleichen.
	{
		static const char* s_szShotCvar = "gm_shot";
		static float s_flNextShot = 0.0f;
		static bool s_bShotCvar = false;

		if (!s_bShotCvar)
		{
			gEngfuncs.pfnRegisterVariable(s_szShotCvar, "0", FCVAR_ARCHIVE);
			s_bShotCvar = true;
		}

		const float flShotInterval = gEngfuncs.pfnGetCvarFloat(s_szShotCvar);

		// Frame-basiert, weil `time` in HUD_Frame nicht zuverlaessig
		// fortlaufend ist (empirisch: nur ein Impuls pro Lauf).
		const int iShotEvery = (flShotInterval > 0.0f)
			? (int)(flShotInterval * 72.0f) : 0;

		if (iShotEvery > 0 && (s_iTestFrame % iShotEvery) == 0)
		{
			gEngfuncs.pfnClientCmd("screenshot\n");
			gEngfuncs.Con_Printf("[shot] screenshot at frame %d\n", s_iTestFrame);
		}
	}

	// Auswahl der Waffen-Modifikation automatisieren: `gm_test_custom 1` oeffnet
	// mit +custom das Auswahlmenue und bestaetigt per Linksklick (Retail-Pfad
	// CHudAmmo::Think). Nur so laesst sich `cust=` in der Serverzeile pruefen.
	{
		static bool s_bCustomCvar = false;
		static int s_iCustomStep = 0;
		static int s_iCustomTimer = 0;

		if (!s_bCustomCvar)
		{
			gEngfuncs.pfnRegisterVariable("gm_test_custom", "0", FCVAR_ARCHIVE);
			s_bCustomCvar = true;
		}

		if (gEngfuncs.pfnGetCvarFloat("gm_test_custom") > 0.0f)
		{
			if (s_iCustomTimer < s_iTestFrame)
			{
				switch (s_iCustomStep)
				{
				case 0: gEngfuncs.pfnClientCmd("+custom\n"); s_iCustomTimer = s_iTestFrame + 6; s_iCustomStep = 1; break;
				case 1: gEngfuncs.pfnClientCmd("+attack\n");  s_iCustomTimer = s_iTestFrame + 4; s_iCustomStep = 2; break;
				case 2: gEngfuncs.pfnClientCmd("-attack\n");
					gEngfuncs.Con_Printf("[test] custom click (frame %d)\n", s_iTestFrame);
					s_iCustomTimer = s_iTestFrame + 4; s_iCustomStep = 3; break;
				default: gEngfuncs.pfnClientCmd("-custom\n"); s_iCustomTimer = s_iTestFrame + 120; s_iCustomStep = 0; break;
				}
			}
		}
	}

	const float flTestMode = gEngfuncs.pfnGetCvarFloat("gm_test_fire");
	if (flTestMode <= 0.0f)
		return;

	// Optionaler Anlauf: `gm_test_walk <Sekunden>` haelt +forward, damit der
	// Spieler vor einer Wand steht, bevor geschossen wird (Nahkampf-Trace
	// reicht nur 32 Einheiten).
	{
		static bool s_bWalkDone = false;
		static int s_iWalkStartFrame = -1;
		const float flWalk = gEngfuncs.pfnGetCvarFloat("gm_test_walk");

		if (!s_bWalkDone && flWalk > 0.0f)
		{
			if (s_iWalkStartFrame < 0)
			{
				gEngfuncs.pfnClientCmd("+forward\n");
				s_iWalkStartFrame = s_iTestFrame;
				gEngfuncs.Con_Printf("[test] walk start (%.1f s, frame %d)\n",
					flWalk, s_iTestFrame);
			}
			else if ((s_iTestFrame - s_iWalkStartFrame) < (int)(flWalk * 72.0f))
			{
				return;
			}
			else
			{
				gEngfuncs.pfnClientCmd("-forward\n");
				gEngfuncs.Con_Printf("[test] walk stop (frame %d)\n", s_iTestFrame);
				s_bWalkDone = true;

				// Optional: nach dem Anlauf einen Test-Kataball abwerfen
				// (gm_test_kata 1), damit der Wandtreffer ohne KI entsteht.
				if (gEngfuncs.pfnGetCvarFloat("gm_test_kata") > 0.0f)
				{
					// gm_test_kata 2 = Variante "self" (Ball am Spieler, loest
					// den katahitplayer-Zweig aus).
					gEngfuncs.pfnClientCmd(
						gEngfuncs.pfnGetCvarFloat("gm_test_kata") >= 2.0f
							? "dbg_kata self\n" : "dbg_kata\n");
				}
			}
		}
	}

	if (s_iTestPhase == 0)
	{
		if ((s_iTestFrame % 90) == 0)
		{
			// Waffe per Cvar waehlbar (Default wie bisher), damit auch
			// Minigun, Gauss usw. ohne Tastatureingabe geprueft werden koennen.
			const char* szWeapon = gEngfuncs.pfnGetCvarString("gm_test_weapon");

			if (!szWeapon || !szWeapon[0])
				szWeapon = "weapon_shotgun";

			char szCmd[128];
			snprintf(szCmd, sizeof(szCmd), "use %s\n", szWeapon);
			gEngfuncs.pfnClientCmd(szCmd);
			s_iTestPhase = 1;
			s_iTestHold = 0;
		}
		return;
	}

	// Phase 1: Angriff halten, nach 8 Frames loslassen.
	if (s_iTestHold == 0)
	{
		if (gEngfuncs.pfnGetCvarFloat("gm_test_attack2") > 0.0f)
			gEngfuncs.pfnClientCmd("+attack2\n");
		else
			gEngfuncs.pfnClientCmd("+attack\n");
		gEngfuncs.Con_Printf("[test] press attack (frame %d, mode %.1f)\n", s_iTestFrame, flTestMode);
	}

	// Aufnahme direkt nach dem Schuss: die Huelsen leben 2,5 s, ein
	// Intervall-Takt trifft sie nur zufaellig.
	// Sechs Aufnahmen je Schuss (Frames 2/4/6/12/18/24 ≈ 0,03-0,33 s), damit
	// die Flugbahn UND die Rotation (Huelse dreht sich) in der Bildserie
	// sichtbar werden — ein Einzelbild sagt nichts ueber die Bewegungskurve.
	if ((s_iTestHold == 2 || s_iTestHold == 4 || s_iTestHold == 6
		|| s_iTestHold == 12 || s_iTestHold == 18 || s_iTestHold == 24)
		&& gEngfuncs.pfnGetCvarFloat("gm_shot") > 0.0f)
	{
		gEngfuncs.pfnClientCmd("screenshot\n");
		gEngfuncs.Con_Printf("[shot] after-shot capture hold=%d frame=%d\n",
			s_iTestHold, s_iTestFrame);
	}

	int iTestHoldMax = (int)gEngfuncs.pfnGetCvarFloat("gm_test_hold");
	if (iTestHoldMax < 2)
		iTestHoldMax = 26;

	if (++s_iTestHold >= iTestHoldMax)
	{
		if (gEngfuncs.pfnGetCvarFloat("gm_test_attack2") > 0.0f)
			gEngfuncs.pfnClientCmd("-attack2\n");
		else
			gEngfuncs.pfnClientCmd("-attack\n");
		// Alternative Waffe fuer den naechsten Durchlauf.
		s_iTestPhase = 0;
	}
}


/*
==========================
HUD_VoiceStatus

Called when a player starts or stops talking.
==========================
*/

void DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
	////	RecClVoiceStatus(entindex, bTalking);

	GetClientVoiceMgr()->UpdateSpeakerStatus(entindex, 0 != bTalking);
}

/*
==========================
HUD_DirectorMessage

Called when a director event message was received
==========================
*/

void DLLEXPORT HUD_DirectorMessage(int iSize, void* pbuf)
{
	//	RecClDirectorMessage(iSize, pbuf);

	gHUD.m_Spectator.DirectorMessage(iSize, pbuf);
}

void CL_UnloadParticleMan()
{
	g_pParticleMan = nullptr;
}

void CL_LoadParticleMan()
{
	//Now implemented in the client library.
	auto particleManFactory = Sys_GetFactoryThis();

	g_pParticleMan = (IParticleMan*)particleManFactory(PARTICLEMAN_INTERFACE, nullptr);

	if (g_pParticleMan)
	{
		g_pParticleMan->SetUp(&gEngfuncs);
	}
}

extern "C" void DLLEXPORT F(void* pv)
{
	cldll_func_t* pcldll_func = (cldll_func_t*)pv;

	cldll_func_t cldll_func =
		{
			Initialize,
			HUD_Init,
			HUD_VidInit,
			HUD_Redraw,
			HUD_UpdateClientData,
			HUD_Reset,
			HUD_PlayerMove,
			HUD_PlayerMoveInit,
			HUD_PlayerMoveTexture,
			IN_ActivateMouse,
			IN_DeactivateMouse,
			IN_MouseEvent,
			IN_ClearStates,
			IN_Accumulate,
			CL_CreateMove,
			CL_IsThirdPerson,
			CL_CameraOffset,
			KB_Find,
			CAM_Think,
			V_CalcRefdef,
			HUD_AddEntity,
			HUD_CreateEntities,
			HUD_DrawNormalTriangles,
			HUD_DrawTransparentTriangles,
			HUD_StudioEvent,
			HUD_PostRunCmd,
			HUD_Shutdown,
			HUD_TxferLocalOverrides,
			HUD_ProcessPlayerState,
			HUD_TxferPredictionData,
			Demo_ReadBuffer,
			HUD_ConnectionlessPacket,
			HUD_GetHullBounds,
			HUD_Frame,
			HUD_Key_Event,
			HUD_TempEntUpdate,
			HUD_GetUserEntity,
			HUD_VoiceStatus,
			HUD_DirectorMessage,
			HUD_GetStudioModelInterface,
			HUD_ChatInputPosition,
		};

	*pcldll_func = cldll_func;
}

#include "cl_dll/IGameClientExports.h"

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	// returns the name of the server the user is connected to, if any
	const char* GetServerHostName() override
	{
		/*if (gViewPortInterface)
		{
			return gViewPortInterface->GetServerName();
		}*/
		return "";
	}

	// ingame voice manipulation
	bool IsPlayerGameVoiceMuted(int playerIndex) override
	{
		if (GetClientVoiceMgr())
			return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);
		return false;
	}

	void MutePlayerGameVoice(int playerIndex) override
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, true);
		}
	}

	void UnmutePlayerGameVoice(int playerIndex) override
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, false);
		}
	}
};

EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION);
