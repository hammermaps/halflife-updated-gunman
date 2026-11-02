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
/*

===== h_export.cpp ========================================================

  Entity classes exported by Halflife.

*/

#include <stdarg.h>
#include "extdll.h"
#include "util.h"

#include "cbase.h"
#include "dbg_log.h"

#undef DLLEXPORT
#ifdef WIN32
#define DLLEXPORT __stdcall
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif

//=========================================================
// Debug event log (sv_cheats-independent cvar dbg_eventlog, set to 1 by debug.cfg).
// Wraps the engine's PrecacheEvent / PlaybackEvent so every server-side event is recorded
// in one file, dbg_events.log in the engine's working directory: what (event name), who
// (invoker entity index and classname), where (origin) and the parameters.
// Client-side playback is logged by the engine itself (cl_showevents 1, same debug.cfg).
// Debug only, not part of the retail game.
//=========================================================
static unsigned short (*s_pfnPrecacheEventOrig)(int type, const char* psz) = nullptr;
static void (*s_pfnPlaybackEventOrig)(int flags, const edict_t* pInvoker, unsigned short eventindex, float delay, const float* origin, const float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2) = nullptr;

static char s_eventNames[1024][64];

// The cvar must exist before debug.cfg runs "dbg_eventlog 1" (otherwise: Unknown command).
static cvar_t s_dbgEventLogCvar = { (char*)"dbg_eventlog", (char*)"0", 0, 0.0f, nullptr };

static bool DbgEventLogEnabled()
{
	return g_engfuncs.pfnCVarGetFloat != nullptr && g_engfuncs.pfnCVarGetFloat("dbg_eventlog") != 0.0f;
}

// Public helper, see dbg_log.h (DBG_LOG macro). Writes one line when dbg_eventlog is 1.
void DbgLogWrite(const char* fmt, ...)
{
	if (!DbgEventLogEnabled())
		return;
	FILE* f = fopen("dbg_events.log", "a");
	if (f == nullptr)
		return;
	fprintf(f, "%8.2f ", gpGlobals->time);
	va_list args;
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);
	fprintf(f, "\n");
	fclose(f);
}

static void DbgEventLogLine(const char* text)
{
	FILE* f = fopen("dbg_events.log", "a");
	if (f == nullptr)
		return;
	fprintf(f, "%8.2f %s\n", gpGlobals->time, text);
	fclose(f);
}

static unsigned short DbgPrecacheEvent(int type, const char* psz)
{
	const unsigned short index = s_pfnPrecacheEventOrig(type, psz);
	if (index < 1024 && psz != nullptr)
	{
		strncpy(s_eventNames[index], psz, sizeof(s_eventNames[index]) - 1);
		s_eventNames[index][sizeof(s_eventNames[index]) - 1] = '\0';
	}
	if (DbgEventLogEnabled())
	{
		char line[256];
		snprintf(line, sizeof(line), "PRECACHE index=%u name=%s", (unsigned)index, psz ? psz : "(null)");
		DbgEventLogLine(line);
	}
	return index;
}

static void DbgPlaybackEvent(int flags, const edict_t* pInvoker, unsigned short eventindex, float delay, const float* origin, const float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
{
	if (DbgEventLogEnabled())
	{
		const char* name = eventindex < 1024 ? s_eventNames[eventindex] : "?";
		const int who = pInvoker ? g_engfuncs.pfnIndexOfEdict(pInvoker) : -1;
		const char* cls = (pInvoker && pInvoker->v.classname) ? STRING(pInvoker->v.classname) : "(none)";
		const float* o = origin ? origin : vec3_origin;
		char line[512];
		snprintf(line, sizeof(line),
			"PLAYBACK %s index=%u flags=%d who=%d(%s) origin=(%.1f %.1f %.1f) angles=(%.1f %.1f %.1f) delay=%.2f f=(%.2f %.2f) i=(%d %d) b=(%d %d)",
			name, (unsigned)eventindex, flags, who, cls, o[0], o[1], o[2],
			angles ? angles[0] : 0.0f, angles ? angles[1] : 0.0f, angles ? angles[2] : 0.0f,
			delay, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
		DbgEventLogLine(line);
	}
	s_pfnPlaybackEventOrig(flags, pInvoker, eventindex, delay, origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
}

extern "C" void DLLEXPORT GiveFnptrsToDll(enginefuncs_t* pengfuncsFromEngine, globalvars_t* pGlobals)
{
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
	gpGlobals = pGlobals;

	// Install the event-log wrappers (see above).
	s_pfnPrecacheEventOrig = g_engfuncs.pfnPrecacheEvent;
	s_pfnPlaybackEventOrig = g_engfuncs.pfnPlaybackEvent;
	g_engfuncs.pfnPrecacheEvent = DbgPrecacheEvent;
	g_engfuncs.pfnPlaybackEvent = DbgPlaybackEvent;
	g_engfuncs.pfnCVarRegister(&s_dbgEventLogCvar);
}
