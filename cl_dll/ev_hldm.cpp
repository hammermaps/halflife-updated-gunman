/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
#include "hud.h"
#include "cl_util.h"

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"

#include "com_weapons.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"

#include "eventscripts.h"
#include "ev_hldm.h"

#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"

#include <string.h>

#include "r_studioint.h"
#include "com_model.h"

extern engine_studio_api_t IEngineStudio;

static int tracerCount[MAX_PLAYERS];

#include "pm_shared.h"
#include "triangleapi.h"

static void EV_TempSprite(const Vector& vecOrigin, const Vector& vecDir, float flScale, int iModelIndex,
	int iRenderMode, int iRenderFx, float flAlpha, float flLife, int iFlags);

void V_PunchAxis(int axis, float punch);
void VectorAngles(const float* forward, float* angles);

extern cvar_t* cl_lw;
extern cvar_t* r_decals;

static inline bool EV_HLDM_IsBSPModel(physent_t* pe)
{
	return pe != nullptr && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP);
}

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound(int idx, pmtrace_t* ptr, float* vecSrc, float* vecEnd, int iBulletType)
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = CHAR_TEX_CONCRETE;
	cl_entity_t* cl_entity = NULL;
	float fvol;
	float fvolbar;
	const char* rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;
	int entity;
	char* pTextureName;
	char texname[64];
	char szbuffer[64];

	entity = gEngfuncs.pEventAPI->EV_IndexFromTrace(ptr);

	// FIXME check if playtexture sounds movevar is set
	//

	chTextureType = 0;

	// Player
	if (entity == 0)
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char*)gEngfuncs.pEventAPI->EV_TraceTexture(ptr->ent, vecSrc, vecEnd);

		if (pTextureName)
		{
			strcpy(texname, pTextureName);
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
			{
				pTextureName += 2;
			}

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
			{
				pTextureName++;
			}

			// '}}'
			strcpy(szbuffer, pTextureName);
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;

			// get texture type
			chTextureType = PM_FindTextureType(szbuffer);
		}
	}
	else
	{
		// JoshA: Look up the entity and find the EFLAG_FLESH_SOUND flag.
		// This broke at some point then TF:C added prediction.
		//
		// It used to use Classify of pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE
		// to determine what sound to play, but that's server side and isn't available on the client
		// and got lost in the translation to that.
		// Now the server will replicate that state via an eflag.
		cl_entity = gEngfuncs.GetEntityByIndex(entity);

		if (cl_entity && !!(cl_entity->curstate.eflags & EFLAG_FLESH_SOUND))
			chTextureType = CHAR_TEX_FLESH;
	}

	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE:
		fvol = 0.9;
		fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL:
		fvol = 0.9;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:
		fvol = 0.9;
		fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:
		fvol = 0.5;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE:
		fvol = 0.9;
		fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:
		fvol = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH:
		fvol = 0.9;
		fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD:
		fvol = 0.9;
		fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if (iBulletType == BULLET_PLAYER_CROWBAR)
			return 0.0; // crowbar already makes this sound
		fvol = 1.0;
		fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	}

	// play material hit sound
	gEngfuncs.pEventAPI->EV_PlaySound(0, ptr->endpos, CHAN_STATIC, rgsz[gEngfuncs.pfnRandomLong(0, cnt - 1)], fvol, fattn, 0, 96 + gEngfuncs.pfnRandomLong(0, 0xf));
	return fvolbar;
}

char* EV_HLDM_DamageDecal(physent_t* pe)
{
	static char decalname[32];
	int idx;

	if (pe->rendermode == kRenderTransAlpha)
		return nullptr;

	if (pe->classnumber == 1)
	{
		idx = gEngfuncs.pfnRandomLong(0, 2);
		sprintf(decalname, "{break%i", idx + 1);
	}
	else if (pe->rendermode != kRenderNormal)
	{
		// Retail nennt hier "{bproof1" - dieser Lump fehlt in Gunmans
		// decals.wad; der Engine-Lookup faellt dann auf Index 0 ({AICORE1)
		// zurueck. Naechster vorhandener Gunman-Lump: "{shield".
		sprintf(decalname, "{shield");
	}
	else
	{
		idx = gEngfuncs.pfnRandomLong(0, 4);
		sprintf(decalname, "{shot%i", idx + 1);
	}
	return decalname;
}

void EV_HLDM_GunshotDecalTrace(pmtrace_t* pTrace, char* decalName)
{
	int iRand;

	gEngfuncs.pEfxAPI->R_BulletImpactParticles(pTrace->endpos);

	iRand = gEngfuncs.pfnRandomLong(0, 0x7FFF);
	if (iRand < (0x7fff / 2)) // not every bullet makes a sound.
	{
		switch (iRand % 5)
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 4:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
	}

	// Only decal brush models such as the world etc.
	if (decalName && '\0' != decalName[0] && r_decals->value > 0)
	{
		gEngfuncs.pEfxAPI->R_DecalShoot(
			gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(decalName)),
			gEngfuncs.pEventAPI->EV_IndexFromTrace(pTrace), 0, pTrace->endpos, 0);
	}
}

void EV_HLDM_DecalGunshot(pmtrace_t* pTrace, int iBulletType)
{
	physent_t* pe;

	pe = gEngfuncs.pEventAPI->EV_GetPhysent(pTrace->ent);

	if (EV_HLDM_IsBSPModel(pe))
	{
		switch (iBulletType)
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_357:
		default:
			// smoke and decal
			EV_HLDM_GunshotDecalTrace(pTrace, EV_HLDM_DamageDecal(pe));
			break;
		}
	}
}

void EV_HLDM_CheckTracer(int idx, float* vecSrc, float* end, float* forward, float* right, int iBulletType, int iTracerFreq, int* tracerCount)
{
	int i;
	bool player = idx >= 1 && idx <= gEngfuncs.GetMaxClients();

	if (iTracerFreq != 0 && ((*tracerCount)++ % iTracerFreq) == 0)
	{
		Vector vecTracerSrc;

		if (player)
		{
			Vector offset(0, 0, -4);

			// adjust tracer position for player
			for (i = 0; i < 3; i++)
			{
				vecTracerSrc[i] = vecSrc[i] + offset[i] + right[i] * 2 + forward[i] * 16;
			}
		}
		else
		{
			VectorCopy(vecSrc, vecTracerSrc);
		}

		switch (iBulletType)
		{
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_MONSTER_9MM:
		case BULLET_MONSTER_12MM:
		default:
			EV_CreateTracer(vecTracerSrc, end);
			break;
		}
	}
}


/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void EV_HLDM_FireBullets(int idx, float* forward, float* right, float* up, int cShots, float* vecSrc, float* vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int* tracerCount, float flSpreadX, float flSpreadY)
{
	int i;
	pmtrace_t tr;
	int iShot;

	for (iShot = 1; iShot <= cShots; iShot++)
	{
		Vector vecDir, vecEnd;

		float x, y, z;
		//We randomize for the Shotgun.
		if (iBulletType == BULLET_PLAYER_BUCKSHOT)
		{
			do
			{
				x = gEngfuncs.pfnRandomFloat(-0.5, 0.5) + gEngfuncs.pfnRandomFloat(-0.5, 0.5);
				y = gEngfuncs.pfnRandomFloat(-0.5, 0.5) + gEngfuncs.pfnRandomFloat(-0.5, 0.5);
				z = x * x + y * y;
			} while (z > 1);

			for (i = 0; i < 3; i++)
			{
				vecDir[i] = vecDirShooting[i] + x * flSpreadX * right[i] + y * flSpreadY * up[i];
				vecEnd[i] = vecSrc[i] + flDistance * vecDir[i];
			}
		} //But other guns already have their spread randomized in the synched spread.
		else
		{

			for (i = 0; i < 3; i++)
			{
				vecDir[i] = vecDirShooting[i] + flSpreadX * right[i] + flSpreadY * up[i];
				vecEnd[i] = vecSrc[i] + flDistance * vecDir[i];
			}
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		// JoshA: Changed from PM_STUDIO_BOX to PM_NORMAL in prediction code as otherwise if you hit an NPC or player's
		// bounding box but not one of their hitboxes, the shot won't hit on the server but it will
		// play a hit sound on the client and not make a decal (as if it hit the NPC/player).
		// We should mirror the way the server does the test here as close as possible.
		//
		// I initially thought I was just fixing some stupid Half-Life bug but no,
		// this is *the* root cause of all the ghost shot bad prediction bugs in Half-Life Deathmatch!
		//
		// Also... CStrike was always using PM_NORMAL for all of these so it didn't have the problem.
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_NORMAL, -1, &tr);

		EV_HLDM_CheckTracer(idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount);

		// do damage, paint decals
		if (tr.fraction != 1.0)
		{
			switch (iBulletType)
			{
			default:
			case BULLET_PLAYER_9MM:

				EV_HLDM_PlayTextureSound(idx, &tr, vecSrc, vecEnd, iBulletType);
				EV_HLDM_DecalGunshot(&tr, iBulletType);

				break;
			case BULLET_PLAYER_MP5:

				EV_HLDM_PlayTextureSound(idx, &tr, vecSrc, vecEnd, iBulletType);
				EV_HLDM_DecalGunshot(&tr, iBulletType);
				break;
			case BULLET_PLAYER_BUCKSHOT:

				EV_HLDM_DecalGunshot(&tr, iBulletType);

				break;
			case BULLET_PLAYER_357:

				EV_HLDM_PlayTextureSound(idx, &tr, vecSrc, vecEnd, iBulletType);
				EV_HLDM_DecalGunshot(&tr, iBulletType);

				break;
			}
		}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

//======================
//	    GLOCK START
//======================
void EV_FireGlock1(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireGlock1\n");
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	bool empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = 0 != args->bparam1;
	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 0);

		V_PunchAxis(0, -2.0);
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, &tracerCount[idx - 1], args->fparam1, args->fparam2);
}

void EV_FireGlock2(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireGlock2\n");
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	bool empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = 0 != args->bparam1;
	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 0);

		V_PunchAxis(0, -2.0);
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, &tracerCount[idx - 1], args->fparam1, args->fparam2);
}
//======================
//	   GLOCK END
//======================

//======================
//	  SHOTGUN START
//======================
void EV_FireShotGunDouble(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireShotGunDouble\n");
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	int j;
	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(SHOTGUN_FIRE2, 0);
		V_PunchAxis(0, -10.0);
	}

	for (j = 0; j < 2; j++)
	{
		EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6);

		EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/dbarrel1.wav", gEngfuncs.pfnRandomFloat(0.98, 1.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong(0, 0x1f));

	EV_GetGunPosition(args, vecSrc, origin);
	VectorCopy(forward, vecAiming);

	if (gEngfuncs.GetMaxClients() > 1)
	{
		EV_HLDM_FireBullets(idx, forward, right, up, 8, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx - 1], 0.17365, 0.04362);
	}
	else
	{
		EV_HLDM_FireBullets(idx, forward, right, up, 12, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx - 1], 0.08716, 0.08716);
	}
}

void EV_FireShotGunSingle(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireShotGunSingle\n");
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(SHOTGUN_FIRE, 0);

		V_PunchAxis(0, -5.0);
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/sbarrel1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0x1f));

	EV_GetGunPosition(args, vecSrc, origin);
	VectorCopy(forward, vecAiming);

	if (gEngfuncs.GetMaxClients() > 1)
	{
		EV_HLDM_FireBullets(idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx - 1], 0.08716, 0.04362);
	}
	else
	{
		EV_HLDM_FireBullets(idx, forward, right, up, 6, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx - 1], 0.08716, 0.08716);
	}
}
//======================
//	   SHOTGUN END
//======================

//======================
//	    MP5 START
//======================
void EV_FireMP5(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireMP5\n");
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(MP5_FIRE1 + gEngfuncs.pfnRandomLong(0, 2), 0);

		V_PunchAxis(0, gEngfuncs.pfnRandomFloat(-2, 2));
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));
		break;
	}

	EV_GetGunPosition(args, vecSrc, origin);
	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx - 1], args->fparam1, args->fparam2);
}

// We only predict the animation and sound
// The grenade is still launched from the server.
void EV_FireMP52(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireMP52\n");
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(MP5_LAUNCH, 0);
		V_PunchAxis(0, -10);
	}

	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/glauncher.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/glauncher2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));
		break;
	}
}
//======================
//		 MP5 END
//======================

//======================
//	   PHYTON START
//	     ( .357 )
//======================
void EV_FirePython(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FirePython\n");
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	if (EV_IsLocal(idx))
	{
		// Python uses different body in multiplayer versus single player
		bool multiplayer = gEngfuncs.GetMaxClients() != 1;

		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(PYTHON_FIRE1, multiplayer ? 1 : 0);

		V_PunchAxis(0, -10.0);
	}

	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/357_shot1.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM);
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/357_shot2.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM);
		break;
	}

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, &tracerCount[idx - 1], args->fparam1, args->fparam2);
}
//======================
//	    PHYTON END
//	     ( .357 )
//======================

//======================
//	   GAUSS START
//======================
void EV_SpinGauss(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_SpinGauss\n");
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	int iSoundState = 0;

	int pitch;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	pitch = args->iparam1;

	iSoundState = 0 != args->bparam1 ? SND_CHANGE_PITCH : 0;

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/pulsemachine.wav", 1.0, ATTN_NORM, iSoundState, pitch);
}

/*
==============================
EV_StopPreviousGauss

==============================
*/
void EV_StopPreviousGauss(int idx)
{
	// Make sure we don't have a gauss spin event in the queue for this guy
	gEngfuncs.pEventAPI->EV_KillEvents(idx, "events/gaussspin.sc");
	gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_WEAPON, "ambience/pulsemachine.wav");
}

extern float g_flApplyVel;

void EV_FireGauss(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireGauss\n");
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	float flDamage = args->fparam1;

	bool m_fPrimaryFire = 0 != args->bparam1;
	Vector vecSrc;
	Vector vecDest;
	edict_t* pentIgnore;
	pmtrace_t tr, beam_tr;
	float flMaxFrac = 1.0;
	bool fHasPunched = false;
	bool fFirstBeam = true;
	int nMaxHits = 10;
	physent_t* pEntity;
	int m_iBeam, m_iGlow, m_iBalls;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	if (0 != args->bparam2)
	{
		EV_StopPreviousGauss(idx);
		return;
	}

	//	Con_Printf( "Firing gauss with %f\n", flDamage );
	EV_GetGunPosition(args, vecSrc, origin);

	m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/smoke.spr");
	m_iBalls = m_iGlow = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/hotglow.spr");

	AngleVectors(angles, forward, right, up);

	VectorMA(vecSrc, 8192, forward, vecDest);

	if (EV_IsLocal(idx))
	{
		V_PunchAxis(0, -2.0);
		gEngfuncs.pEventAPI->EV_WeaponAnimation(GAUSS_FIRE2, 0);

		if (!m_fPrimaryFire)
			g_flApplyVel = flDamage;
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/gauss2.wav", 0.5 + flDamage * (1.0 / 400.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong(0, 0x1f));

	while (flDamage > 10 && nMaxHits > 0)
	{
		nMaxHits--;

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecDest, PM_NORMAL, -1, &tr);

		gEngfuncs.pEventAPI->EV_PopPMStates();

		if (0 != tr.allsolid)
			break;

		if (fFirstBeam)
		{
			if (EV_IsLocal(idx))
			{
				// Add muzzle flash to current weapon model
				EV_MuzzleFlash();
			}
			fFirstBeam = false;

			gEngfuncs.pEfxAPI->R_BeamEntPoint(
				idx | 0x1000,
				tr.endpos,
				m_iBeam,
				0.1,
				m_fPrimaryFire ? 1.0 : 2.5,
				0.0,
				(m_fPrimaryFire ? 128.0 : flDamage) / 255.0,
				0,
				0,
				0,
				(m_fPrimaryFire ? 255 : 255) / 255.0,
				(m_fPrimaryFire ? 128 : 255) / 255.0,
				(m_fPrimaryFire ? 0 : 255) / 255.0);
		}
		else
		{
			gEngfuncs.pEfxAPI->R_BeamPoints(vecSrc,
				tr.endpos,
				m_iBeam,
				0.1,
				m_fPrimaryFire ? 1.0 : 2.5,
				0.0,
				(m_fPrimaryFire ? 128.0 : flDamage) / 255.0,
				0,
				0,
				0,
				(m_fPrimaryFire ? 255 : 255) / 255.0,
				(m_fPrimaryFire ? 128 : 255) / 255.0,
				(m_fPrimaryFire ? 0 : 255) / 255.0);
		}

		pEntity = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pEntity == NULL)
			break;

		if (EV_HLDM_IsBSPModel(pEntity))
		{
			float n;

			pentIgnore = NULL;

			n = -DotProduct(tr.plane.normal, forward);

			if (n < 0.5) // 60 degrees
			{
				// ALERT( at_console, "reflect %f\n", n );
				// reflect
				Vector r;

				VectorMA(forward, 2.0 * n, tr.plane.normal, r);

				flMaxFrac = flMaxFrac - tr.fraction;

				VectorCopy(r, forward);

				VectorMA(tr.endpos, 8.0, forward, vecSrc);
				VectorMA(vecSrc, 8192.0, forward, vecDest);

				gEngfuncs.pEfxAPI->R_TempSprite(tr.endpos, vec3_origin, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage * n / 255.0, flDamage * n * 0.5 * 0.1, FTENT_FADEOUT);

				Vector fwd;
				VectorAdd(tr.endpos, tr.plane.normal, fwd);

				gEngfuncs.pEfxAPI->R_Sprite_Trail(TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 3, 0.1, gEngfuncs.pfnRandomFloat(10, 20) / 100.0, 100,
					255, 100);

				// lose energy
				if (n == 0)
				{
					n = 0.1;
				}

				flDamage = flDamage * (1 - n);
			}
			else
			{
				// tunnel
				EV_HLDM_DecalGunshot(&tr, BULLET_MONSTER_12MM);

				gEngfuncs.pEfxAPI->R_TempSprite(tr.endpos, vec3_origin, 1.0, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0, 6.0, FTENT_FADEOUT);

				// limit it to one hole punch
				if (fHasPunched)
				{
					break;
				}
				fHasPunched = true;

				// try punching through wall if secondary attack (primary is incapable of breaking through)
				if (!m_fPrimaryFire)
				{
					Vector start;

					VectorMA(tr.endpos, 8.0, forward, start);

					// Store off the old count
					gEngfuncs.pEventAPI->EV_PushPMStates();

					// Now add in all of the players.
					gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

					gEngfuncs.pEventAPI->EV_SetTraceHull(2);
					gEngfuncs.pEventAPI->EV_PlayerTrace(start, vecDest, PM_NORMAL, -1, &beam_tr);

					if (0 == beam_tr.allsolid)
					{
						Vector delta;
						float n;

						// trace backwards to find exit point

						gEngfuncs.pEventAPI->EV_PlayerTrace(beam_tr.endpos, tr.endpos, PM_NORMAL, -1, &beam_tr);

						VectorSubtract(beam_tr.endpos, tr.endpos, delta);

						n = Length(delta);

						if (n < flDamage)
						{
							if (n == 0)
								n = 1;
							flDamage -= n;

							// absorption balls
							{
								Vector fwd;
								VectorSubtract(tr.endpos, forward, fwd);
								gEngfuncs.pEfxAPI->R_Sprite_Trail(TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 3, 0.1, gEngfuncs.pfnRandomFloat(10, 20) / 100.0, 100,
									255, 100);
							}

							//////////////////////////////////// WHAT TO DO HERE
							// CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );

							EV_HLDM_DecalGunshot(&beam_tr, BULLET_MONSTER_12MM);

							gEngfuncs.pEfxAPI->R_TempSprite(beam_tr.endpos, vec3_origin, 0.1, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0, 6.0, FTENT_FADEOUT);

							// balls
							{
								Vector fwd;
								VectorSubtract(beam_tr.endpos, forward, fwd);
								gEngfuncs.pEfxAPI->R_Sprite_Trail(TE_SPRITETRAIL, beam_tr.endpos, fwd, m_iBalls, (int)(flDamage * 0.3), 0.1, gEngfuncs.pfnRandomFloat(10, 20) / 100.0, 200,
									255, 40);
							}

							VectorAdd(beam_tr.endpos, forward, vecSrc);
						}
					}
					else
					{
						flDamage = 0;
					}

					gEngfuncs.pEventAPI->EV_PopPMStates();
				}
				else
				{
					if (m_fPrimaryFire)
					{
						// slug doesn't punch through ever with primary
						// fire, so leave a little glowy bit and make some balls
						gEngfuncs.pEfxAPI->R_TempSprite(tr.endpos, vec3_origin, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, 200.0 / 255.0, 0.3, FTENT_FADEOUT);

						{
							Vector fwd;
							VectorAdd(tr.endpos, tr.plane.normal, fwd);
							gEngfuncs.pEfxAPI->R_Sprite_Trail(TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 8, 0.6, gEngfuncs.pfnRandomFloat(10, 20) / 100.0, 100,
								255, 200);
						}
					}

					flDamage = 0;
				}
			}
		}
		else
		{
			VectorAdd(tr.endpos, forward, vecSrc);
		}
	}
}
//======================
//	   GAUSS END
//======================

//======================
//	   CROWBAR START
//======================
int g_iSwing;

//Only predict the miss sounds, hit sounds are still played
//server side, so players don't get the wrong idea.
void EV_Crowbar(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_Crowbar\n");
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if (EV_IsLocal(idx))
	{
		switch ((g_iSwing++) % 3)
		{
		case 0:
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK1MISS, 0);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK2MISS, 0);
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK3MISS, 0);
			break;
		}
	}
}
//======================
//	   CROWBAR END
//======================

//======================
//	  CROSSBOW START
//======================
//=====================
// EV_BoltCallback
// This function is used to correct the origin and angles
// of the bolt, so it looks like it's stuck on the wall.
//=====================
void EV_BoltCallback(struct tempent_s* ent, float frametime, float currenttime)
{
	ent->entity.origin = ent->entity.baseline.vuser1;
	ent->entity.angles = ent->entity.baseline.vuser2;
}

void EV_FireCrossbow2(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireCrossbow2\n");
	Vector vecSrc, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;

	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	EV_GetGunPosition(args, vecSrc, origin);

	VectorMA(vecSrc, 8192, forward, vecEnd);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));

	if (EV_IsLocal(idx))
	{
		if (0 != args->iparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 0);
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 0);
	}

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_NORMAL, -1, &tr);

	//We hit something
	if (tr.fraction < 1.0)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

		//Not the world, let's assume we hit something organic ( dog, cat, uncle joe, etc ).
		if (!EV_HLDM_IsBSPModel(pe))
		{
			switch (gEngfuncs.pfnRandomLong(0, 1))
			{
			case 0:
				gEngfuncs.pEventAPI->EV_PlaySound(idx, tr.endpos, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_PlaySound(idx, tr.endpos, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			}
		}
		//Stick to world but don't stick to glass, it might break and leave the bolt floating. It can still stick to other non-transparent breakables though.
		else if (pe->rendermode == kRenderNormal)
		{
			gEngfuncs.pEventAPI->EV_PlaySound(0, tr.endpos, CHAN_BODY, "weapons/xbow_hit1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, PITCH_NORM);

			//Not underwater, do some sparks...
			if (gEngfuncs.PM_PointContents(tr.endpos, NULL) != CONTENTS_WATER)
				gEngfuncs.pEfxAPI->R_SparkShower(tr.endpos);

			Vector vBoltAngles;
			int iModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("models/crossbow_bolt.mdl");

			VectorAngles(forward, vBoltAngles);

			TEMPENTITY* bolt = gEngfuncs.pEfxAPI->R_TempModel(tr.endpos - forward * 10, Vector(0, 0, 0), vBoltAngles, 5, iModelIndex, TE_BOUNCE_NULL);

			if (bolt)
			{
				bolt->flags |= (FTENT_CLIENTCUSTOM);					 //So it calls the callback function.
				bolt->entity.baseline.vuser1 = tr.endpos - forward * 10; // Pull out a little bit
				bolt->entity.baseline.vuser2 = vBoltAngles;				 //Look forward!
				bolt->callback = EV_BoltCallback;						 //So we can set the angles and origin back. (Stick the bolt to the wall)
			}
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

//TODO: Fully predict the fliying bolt.
void EV_FireCrossbow(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireCrossbow\n");
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));

	//Only play the weapon anims if I shot it.
	if (EV_IsLocal(idx))
	{
		if (0 != args->iparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 0);
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 0);

		V_PunchAxis(0, -2.0);
	}
}
//======================
//	   CROSSBOW END
//======================

//======================
//	    RPG START
//======================
void EV_FireRpg(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_FireRpg\n");
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM);
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM);

	//Only play the weapon anims if I shot it.
	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(RPG_FIRE2, 0);

		V_PunchAxis(0, -5.0);
	}
}
//======================
//	     RPG END
//======================

//======================
//	    EGON END
//======================
int g_fireAnims1[] = {EGON_FIRE1, EGON_FIRE2, EGON_FIRE3, EGON_FIRE4};
int g_fireAnims2[] = {EGON_ALTFIRECYCLE};

BEAM* pBeam;
BEAM* pBeam2;

void EV_EgonFire(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_EgonFire\n");
	int idx, iFireMode;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	iFireMode = args->iparam2;
	bool iStartup = 0 != args->bparam1;


	if (iStartup)
	{
		if (iFireMode == FIRE_WIDE)
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.98, ATTN_NORM, 0, 125);
		else
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.9, ATTN_NORM, 0, 100);
	}
	else
	{
		//If there is any sound playing already, kill it.
		//This is necessary because multiple sounds can play on the same channel at the same time.
		//In some cases, more than 1 run sound plays when the egon stops firing, in which case only the earliest entry in the list is stopped.
		//This ensures no more than 1 of those is ever active at the same time.
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, EGON_SOUND_RUN);

		if (iFireMode == FIRE_WIDE)
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.98, ATTN_NORM, 0, 125);
		else
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.9, ATTN_NORM, 0, 100);
	}

	//Only play the weapon anims if I shot it.
	if (EV_IsLocal(idx))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(g_fireAnims1[gEngfuncs.pfnRandomLong(0, 3)], 0);

	if (iStartup && EV_IsLocal(idx) && !pBeam && !pBeam2 && 0 != cl_lw->value) //Adrian: Added the cl_lw check for those lital people that hate weapon prediction.
	{
		Vector vecSrc, vecEnd, angles, forward, right, up;
		pmtrace_t tr;

		cl_entity_t* pl = gEngfuncs.GetEntityByIndex(idx);

		if (pl)
		{
			VectorCopy(gHUD.m_vecAngles, angles);

			AngleVectors(angles, forward, right, up);

			EV_GetGunPosition(args, vecSrc, pl->origin);

			VectorMA(vecSrc, 2048, forward, vecEnd);

			gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);

			// Store off the old count
			gEngfuncs.pEventAPI->EV_PushPMStates();

			// Now add in all of the players.
			gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

			gEngfuncs.pEventAPI->EV_SetTraceHull(2);
			gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_NORMAL, -1, &tr);

			gEngfuncs.pEventAPI->EV_PopPMStates();

			int iBeamModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex(EGON_BEAM_SPRITE);

			float r = 50.0f;
			float g = 50.0f;
			float b = 125.0f;

			//if ( IEngineStudio.IsHardware() )
			{
				r /= 255.0f;
				g /= 255.0f;
				b /= 255.0f;
			}


			pBeam = gEngfuncs.pEfxAPI->R_BeamEntPoint(idx | 0x1000, tr.endpos, iBeamModelIndex, 99999, 3.5, 0.2, 0.7, 55, 0, 0, r, g, b);

			if (pBeam)
				pBeam->flags |= (FBEAM_SINENOISE);

			pBeam2 = gEngfuncs.pEfxAPI->R_BeamEntPoint(idx | 0x1000, tr.endpos, iBeamModelIndex, 99999, 5.0, 0.08, 0.7, 25, 0, 0, r, g, b);
		}
	}
}

void EV_EgonStop(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_EgonStop\n");
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, EGON_SOUND_RUN);

	//Only stop the sound if the event was sent by the same source as the owner of the egon.
	//If the local player owns the egon then only the local event should play this sound.
	//If another player owns it, only the server event should play it.
	if (0 != args->iparam1)
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, EGON_SOUND_OFF, 0.98, ATTN_NORM, 0, 100);

	if (EV_IsLocal(idx))
	{
		if (pBeam)
		{
			pBeam->die = 0.0;
			pBeam = NULL;
		}


		if (pBeam2)
		{
			pBeam2->die = 0.0;
			pBeam2 = NULL;
		}

		// HACK: only reset animation if the Egon is still equipped.
		if (g_CurrentWeaponId == WEAPON_EGON)
		{
			gEngfuncs.pEventAPI->EV_WeaponAnimation(EGON_IDLE1, 0);
		}
	}
}
//======================
//	    EGON END
//======================

//======================
//	   HORNET START
//======================
void EV_HornetGunFire(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_HornetGunFire\n");
	int idx;
	Vector origin, angles;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	//Only play the weapon anims if I shot it.
	if (EV_IsLocal(idx))
	{
		V_PunchAxis(0, gEngfuncs.pfnRandomLong(0, 2));
		gEngfuncs.pEventAPI->EV_WeaponAnimation(HGUN_SHOOT, 0);
	}

	switch (gEngfuncs.pfnRandomLong(0, 2))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "agrunt/ag_fire1.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "agrunt/ag_fire2.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 2:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "agrunt/ag_fire3.wav", 1, ATTN_NORM, 0, 100);
		break;
	}
}
//======================
//	   HORNET END
//======================

//======================
//	   TRIPMINE START
//======================
//We only check if it's possible to put a trip mine
//and if it is, then we play the animation. Server still places it.
void EV_TripmineFire(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_TripmineFire\n");
	int idx;
	Vector vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, vecSrc);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, NULL, NULL);

	if (!EV_IsLocal(idx))
		return;

	// Grab predicted result for local player
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);

	vecSrc = vecSrc + view_ofs;

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecSrc + forward * 128, PM_NORMAL, -1, &tr);

	//Hit something solid
	if (tr.fraction < 1.0)
		gEngfuncs.pEventAPI->EV_WeaponAnimation(TRIPMINE_DRAW, 0);

	gEngfuncs.pEventAPI->EV_PopPMStates();
}
//======================
//	   TRIPMINE END
//======================

//======================
//	   SQUEAK START
//======================
void EV_SnarkFire(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_SnarkFire\n");
	int idx;
	Vector vecSrc, angles, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, vecSrc);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, NULL, NULL);

	if (!EV_IsLocal(idx))
		return;

	if (0 != args->ducking)
		vecSrc = vecSrc - (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc + forward * 20, vecSrc + forward * 64, PM_NORMAL, -1, &tr);

	//Find space to drop the thing.
	if (tr.allsolid == 0 && tr.startsolid == 0 && tr.fraction > 0.25)
		gEngfuncs.pEventAPI->EV_WeaponAnimation(SQUEAK_THROW, 0);

	gEngfuncs.pEventAPI->EV_PopPMStates();
}
//======================
//	   SQUEAK END
//======================

void EV_TrainPitchAdjust(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_TrainPitchAdjust\n");
	int idx;
	Vector origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	bool stop;

	char sz[256];

	idx = args->entindex;

	VectorCopy(args->origin, origin);

	us_params = (unsigned short)args->iparam1;
	stop = 0 != args->bparam1;

	m_flVolume = (float)(us_params & 0x003f) / 40.0;
	noise = (int)(((us_params) >> 12) & 0x0007);
	pitch = (int)(10.0 * (float)((us_params >> 6) & 0x003f));

	switch (noise)
	{
	case 1:
		strcpy(sz, "plats/ttrain1.wav");
		break;
	case 2:
		strcpy(sz, "plats/ttrain2.wav");
		break;
	case 3:
		strcpy(sz, "plats/ttrain3.wav");
		break;
	case 4:
		strcpy(sz, "plats/ttrain4.wav");
		break;
	case 5:
		strcpy(sz, "plats/ttrain6.wav");
		break;
	case 6:
		strcpy(sz, "plats/ttrain7.wav");
		break;
	default:
		// no sound
		strcpy(sz, "");
		return;
	}

	if (stop)
	{
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, sz);
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch);
	}
}

void EV_VehiclePitchAdjust(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_VehiclePitchAdjust\n");
	int idx;
	Vector origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	bool stop;

	char sz[256];

	idx = args->entindex;

	VectorCopy(args->origin, origin);

	us_params = (unsigned short)args->iparam1;
	stop = 0 != args->bparam1;

	m_flVolume = (float)(us_params & 0x003f) / 40.0;
	noise = (int)(((us_params) >> 12) & 0x0007);
	pitch = (int)(10.0 * (float)((us_params >> 6) & 0x003f));

	switch (noise)
	{
	case 1:
		strcpy(sz, "plats/vehicle1.wav");
		break;
	case 2:
		strcpy(sz, "plats/vehicle2.wav");
		break;
	case 3:
		strcpy(sz, "plats/vehicle3.wav");
		break;
	case 4:
		strcpy(sz, "plats/vehicle4.wav");
		break;
	case 5:
		strcpy(sz, "plats/vehicle6.wav");
		break;
	case 6:
		strcpy(sz, "plats/vehicle7.wav");
		break;
	default:
		// no sound
		strcpy(sz, "");
		return;
	}

	if (stop)
	{
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, sz);
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch);
	}
}

bool EV_TFC_IsAllyTeam(int iTeam1, int iTeam2)
{
	return false;
}


//======================
//  WIEDERHERGESTELLT 2026-11-06 (aus Sitzungsdump)
//======================
// 72-byte effect structure (matches retail FUN_10002810 / DAT_10092658 list,
// linked continuation at +0x48; consumer FUN_100396c0 for radius/alpha).
struct cluster_explosion_effect_t
{
	Vector origin;        // +0x00 (maps to event_args origin)
	float startTime;      // +0x0c (client time at spawn)
	float padding[2];     // +0x10..+0x17 reserved / angle/velocity area
	int typeValue;        // +0x18 (+0x28 in retail offset; 1 = ClusterExplosion branch)
	int subType;          // +0x1c (+0x2c; 2/3/4 handled by FUN_100392e0)
	float velocity[3];    // +0x20..+0x28 (speed component)
	float radiusRate;     // +0x30 (retail uses rate 3 from FUN_100396c0)
	float alphaRate;      // +0x34 (retail decay 10 per tick)
	float maxRadius;      // +0x38 (90.0 per finding)
	int linkedNext;       // +0x3c placeholder (+0x48 linked continuation)
	char soundGroup;      // +0x40 (+0x40 selects sound group)
	float padding2[2];    // +0x44..+0x4b padding to 72-byte size
};

// Retail registrator precache function - mirrors retail FUN_10020860, FUN_1001a2f0, FUN_1001af90, FUN_1001dff0, FUN_100156a0, FUN_1000b630
// Called during client initialization to precache all event resources
void EV_Precache()
{
	// Main registrar (FUN_10020860) - 10 events: ClusterExplosion, Glock1, MP5, Python, SniperShot, RocketSmoke, ChemicalSpray, gw_shotgun, Turret, Sentry
	gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/kaboom.spr");
	gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/fthrower.spr");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ClusterExplosion.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/glock1.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/glock2.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/mp5.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/mp52.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/python.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gauss.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gaussspin.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/snipershot.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/RapidImpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ChargedImpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ClusterExplosion.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ChemicalSpray.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/train.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/vehicle.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/crowbar.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/crossbow1.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/crossbow2.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/rpg.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/egon_fire.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/egon_stop.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/firehornet.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/tripfire.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/snarkfire.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/turretbullets.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstersentry.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gauss.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gaussspin.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/shotgun1.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/shotgun2.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/mp5.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/mp52.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/python.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_shotgun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_minigun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_minigunHeat.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ChemicalSpray.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/lightning.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/RocketSmoke.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/volumeexplosion.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ClusterExplosion.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ChargedImpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/sphereexplode.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/rapidimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/tankmguns.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstershotgun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstermechagun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/snipershot.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/cylinder.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstersentry.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/turretbullets.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/waterripple.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/batbotbeam.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/chopperfire.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/balllightning.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/rustbitimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/kataimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/cleanup.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/spurtblood.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_shotgun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_minigun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_minigunHeat.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ChemicalSpray.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/lightning.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/RocketSmoke.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/volumeexplosion.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ClusterExplosion.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ChargedImpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/sphereexplode.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/rapidimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/tankmguns.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstershotgun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstermechagun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/snipershot.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/cylinder.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstersentry.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/turretbullets.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/waterripple.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/batbotbeam.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/chopperfire.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/balllightning.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/rustbitimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/kataimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/cleanup.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/spurtblood.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_shotgun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_minigun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/gw_minigunHeat.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ChemicalSpray.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/lightning.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/RocketSmoke.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/volumeexplosion.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ClusterExplosion.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/ChargedImpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/sphereexplode.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/rapidimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/tankmguns.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstershotgun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstermechagun.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/snipershot.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/cylinder.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/monstersentry.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/turretbullets.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/waterripple.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/batbotbeam.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/chopperfire.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/balllightning.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/rustbitimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/kataimpact.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/cleanup.sc");
	gEngfuncs.pEventAPI->EV_PrecacheEvent(0, "events/spurtblood.sc");
}

//======================
//\t  CHEMICAL SPRAY END
//======================

//======================
//\t  TURRET BULLETS START
//======================
// Retail events/turretbullets.sc -> client.dll FUN_10020860 (main registrar).
// Turret fires bullet projectiles with muzzle flash and impact effects.
void EV_TurretBullets(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_TurretBullets\n");
	const int idx = args->entindex;
	Vector forward, right, up;
	pmtrace_t tr;

	AngleVectors(args->angles, forward, right, up);

	const int iMuzzleFlash = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/muzzleflash3.spr");
	const int iSpark = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gausspuff.spr");

	Vector vecEnd = Vector(args->origin) + forward * 8192;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(args->origin, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0f)
	{
		Vector vecPos = Vector(tr.endpos) + Vector(tr.plane.normal) * 5.0f;

		gEngfuncs.pEventAPI->EV_PlaySound(idx, vecPos, CHAN_AUTO, "weapons/turret_hit.wav", 1.0f, 0.8f, 0,
			85 + gEngfuncs.pfnRandomLong(0, 31));




		EV_TempSprite(vecPos, vec3_origin, 0.3f, iSpark, kRenderTransAdd, kRenderFxNone, 1.0f, 0.5f, FTENT_FADEOUT);

		if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0.0f)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP))
			{
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{shot5")),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}
		}
	}

	// Muzzle flash at turret origin
	EV_TempSprite(args->origin, vec3_origin, 0.2f, gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/muzzleflash3.spr"), kRenderTransAdd, kRenderFxNone, 1.0f, 0.5f, FTENT_FADEOUT);
}
//======================
//\t  TURRET BULLETS END
//======================

//======================
//\t  MONSTER SENTRY START
//======================
// Retail events/monstersentry.sc -> client.dll FUN_10020860 (main registrar).
// Sentry gun fires at targets with muzzle flash and impact effects.
void EV_MonsterSentry(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_MonsterSentry\n");
	const int idx = args->entindex;
	Vector forward, right, up;
	pmtrace_t tr;

	AngleVectors(args->angles, forward, right, up);

	const int iMuzzleFlash = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/muzzleflash3.spr");
	const int iSpark = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gausspuff.spr");

	Vector vecEnd = Vector(args->origin) + forward * 8192;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(args->origin, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0f)
	{
		Vector vecPos = Vector(tr.endpos) + Vector(tr.plane.normal) * 5.0f;

		gEngfuncs.pEventAPI->EV_PlaySound(idx, vecPos, CHAN_AUTO, "weapons/sentry_hit.wav", 1.0f, 0.8f, 0,
			85 + gEngfuncs.pfnRandomLong(0, 31));

		EV_TempSprite(vecPos, vec3_origin, 0.3f, gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gausspuff.spr"), kRenderTransAdd, kRenderFxNone, 1.0f, 0.5f, FTENT_FADEOUT);

		if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0.0f)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP))
			{
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{shot5")),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}
		}
	}

	// Muzzle flash at sentry origin
	EV_TempSprite(args->origin, vec3_origin, 0.2f, gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/muzzleflash3.spr"), kRenderTransAdd, kRenderFxNone, 1.0f, 0.5f, FTENT_FADEOUT);
}
//======================
//\t  MONSTER SENTRY END
//======================

// Retail registrator precache function - mirrors retail FUN_10020860, FUN_1001a2f0, FUN_1001af90, FUN_1001dff0, FUN_100156a0, FUN_1000b630
// Called during client initialization to precache all event resources


//======================
//  OFFENE WIEDERHERSTELLUNG (2026-11-06)
//======================
// Diese Handler gingen bei einem fehlerhaften Datei-Schreibvorgang verloren.
// Sie sind hier als Platzhalter vorhanden, damit der Client wieder linkt; die
// Retail-Implementierungen liegen als Pseudocode in
// re-project/out/re/client_pseudo_2026-11-06/functions/ und muessen aus den
// jeweiligen Registrator-/Handler-Adressen nachgezogen werden.

// Retail 0x100083b0: Trace 2048 mit Hull 2; bei bparam2 != 0 zusaetzlich ein
// DLight (Radius 130, RGB 180/0/255, Decay 80) und der Decal "{balburn",
// sonst "{pulse". Am Trefferpunkt erscheint sprites/ballpop.spr.
void EV_BallLightning(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_BallLightning\n");
	const int idx = args->entindex;
	Vector forward;
	AngleVectors(args->angles, forward, nullptr, nullptr);

	const Vector vecSrc(args->origin);
	const Vector vecEnd = vecSrc + forward * 2048.0f;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (args->bparam2 != 0)
	{
		dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0);

		if (dl)
		{
			dl->origin = tr.endpos;
			dl->radius = 130.0f;
			dl->color.r = 180;
			dl->color.g = 0;
			dl->color.b = 255;
			dl->die = gEngfuncs.GetClientTime() + 0.1f;
			dl->decay = 80.0f;
		}
	}

	const int iPop = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/ballpop.spr");

	if (iPop > 0)
	{
		gEngfuncs.pEfxAPI->R_TempSprite((float*)&tr.endpos, vec3_origin, 0.3f, iPop,
			kRenderGlow, kRenderFxNone, 1.0f, 0.4f, FTENT_FADEOUT);
	}

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	if (!pe || !((pe->solid == SOLID_BSP) || (pe->movetype == MOVETYPE_PUSHSTEP)))
		return;

	if (gEngfuncs.pfnGetCvarFloat("r_decals") == 0.0f)
		return;

	const char* pszDecal = (args->bparam2 != 0) ? "{balburn" : "{pulse";

	gEngfuncs.pEfxAPI->R_DecalShoot(
		gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
		gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
}

// Effektlisten-Eintraege (Definitionen weiter unten).
void EV_AddEffectEntityTube(int iEntIndex, const Vector& vecDir, float flRate,
	float flAlphaStart, float flAlphaRate, int iSprite);

// Retail 0x10008020 (Event "events/batbotbeam.sc").
//
// Belegt aus Decompilat + Disassembly + Engine-Slot-Tabelle:
//   * Sprite: "sprites/hugeball.spr" (0x10076544) ueber pfnSPR_Load (idx 0)
//     und GetSpritePointer (idx 62); GetClientTime (idx 54) liefert den
//     Startzeitpunkt.
//   * Richtung: VectorSet(this, 0, 0, -1) -> (0, 0, -1), per VectorCopy
//     zweimal in den 18-Dword-Effektknoten kopiert.
//   * Konstruktor 0x10002870(this, 4.0) legt +0x00 = 1.0 (Alpha), +0x1c = 4.0
//     (Skalierung) sowie die uebrigen Felder aus dem Stack fest; der Helfer
//     0x10002ae0 haengt den Knoten an die zyklische Effektliste (76-Byte-
//     Knoten, Verkettung +0x48, Ringpuffer bei 64000).
//   * Stackreste an der Aufrufstelle: 6.0, 240.0, 7, 0.1 — in Reihenfolge
//     Lebensdauer/Geschwindigkeit/Anzahl/Alpha-Rate (Deutung, nicht belegt).
//
// Abweichung: Retail legt einen Eintrag in seine Effektliste und rendert ihn
// spaeter mit dem Sprite aus dem Knoten. Diese Liste hat unser Port nicht in
// dieser Form, deshalb wird der Ball direkt als Temp-Sprite erzeugt. Sichtbar
// ist derselbe Sprite am selben Ort; die Wiederverwendung des Listeneintrags
// (mehrere Baelle je Strahl) ist nicht nachgebildet.
void EV_BatBotBeam(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_BatBotBeam\n");

	// Typ-7-Eintrag: am Ursprung der Schuetzen-Entity verankert, Richtung
	// (0,0,-1) wie im Retail-Konstruktor, Alpha 240, Abnahme 6, Sprite
	// sprites/hugeball.spr, Zeichenradius 4.5, Laenge 9.0 (FUN_10038810).
	static int iHugeBall = 0;

	if (!iHugeBall)
		iHugeBall = gEngfuncs.pfnSPR_Load("sprites/hugeball.spr");

	if (iHugeBall <= 0)
		return;

	EV_AddEffectEntityTube(args->entindex, Vector(0.0f, 0.0f, -1.0f),
		6.0f, 240.0f, 6.0f, iHugeBall);
}

void EV_ChargedImpact(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_ChargedImpact\n");
	const int idx = args->entindex;
	Vector forward;
	AngleVectors(args->angles, forward, nullptr, nullptr);

	const Vector vecSrc(args->origin);
	const Vector vecEnd = vecSrc + forward * 2048.0f;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	// Retail 0x10009300: Sound (Pitch 85), Puff-Sprite mit Alpha 150/255, Decal "{charge".
	gEngfuncs.pEventAPI->EV_PlaySound(idx, tr.endpos, CHAN_STATIC, "weapons/gauss_spritebig.wav",
		1.0f, ATTN_NORM, 0, 85);

	if (!pe || pe->solid != SOLID_BSP)
		return;

	const int iPuff = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gausspuff.spr");

	if (iPuff > 0)
	{
		gEngfuncs.pEfxAPI->R_TempSprite((float*)&tr.endpos, vec3_origin, 0.3f, iPuff,
			kRenderGlow, kRenderFxNone, 150.0f / 255.0f, 0.5f, FTENT_FADEOUT);
	}

	if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0.0f)
	{
		gEngfuncs.pEfxAPI->R_DecalShoot(
			gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{charge")),
			gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
	}
}

// Retail 0x10009cd0: Sprite nach iparam1 (0 = gruen, 2 = orange, 3 = rot,
// sonst gelb), Trace entlang args->velocity mit Hull 2; trifft der Trace die
// Welt, spritzt R_Sprite_Spray(origin, dir, sprite, 10, 50, 100).
void EV_ChemicalSpray(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_ChemicalSpray\n");
	const char* pszSprite = "sprites/yellowsplash.spr";

	switch (args->iparam1)
	{
	case 0: pszSprite = "sprites/greensplash.spr"; break;
	case 2: pszSprite = "sprites/orangesplash.spr"; break;
	case 3: pszSprite = "sprites/redsplash.spr"; break;
	default: break;
	}

	const int iModel = gEngfuncs.pEventAPI->EV_FindModelIndex(pszSprite);

	if (iModel <= 0)
		return;

	Vector vecDir(args->velocity);
	const float flSpeed = VectorNormalize(vecDir);
	const float flDistance = (flSpeed > 1.0f) ? flSpeed : 512.0f;

	const Vector vecSrc(args->origin);
	const Vector vecEnd = vecSrc + vecDir * flDistance;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(args->entindex - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.ent != 0)
		return;

	// Retail-Parameter (count 10, speed 50, size 100) unveraendert uebernommen.
	// Hinweis: 100 ist in diesem Build sehr gross (siehe entity-effects-tuning).
	gEngfuncs.pEfxAPI->R_Sprite_Spray((float*)&vecSrc, (float*)&vecDir, iModel, 10, 50, 100);
}

// Retail 0x10005c10: Push-Sequenz vor `call 0x100021d0` ergibt
// cShots = 2, Distanz 2048.0 (0x45000000), Kugeltyp 2
// (= BULLET_PLAYER_MP5), Tracer-Frequenz 1. Huelse models/shell.mdl.
void EV_ChopperFire(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_ChopperFire\n");
	const int idx = args->entindex;
	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl");

	Vector vecSrc(args->origin);
	int iTracer = 0;

	EV_HLDM_FireBullets(idx, forward, right, up, 2, (float*)&vecSrc, forward, 2048.0f,
		BULLET_PLAYER_MP5, 1, &iTracer, args->fparam1, args->fparam2);
}

// Retail 0x10007fd0. Teil 1 ist belegt und wird umgesetzt: das Viewmodel
// verliert seinen Heat-Zustand (renderfx zurueck auf 0). Teil 2 (Walk ueber
// [global+0x830] und Aufruf von Vtable+0x24(0) auf allen nicht-leeren
// Panel-Zellen) bleibt offen: das globale Objekt ist nicht identifiziert.
void EV_Cleanup(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_Cleanup\n");
	// func_0x10028a70(args->entindex) != 0 -> Entitaet existiert
	if (args->entindex == 0)
		return;

	cl_entity_t* pViewModel = gEngfuncs.GetViewModel();
	if (!pViewModel)
		return;

	// +0x304 = curstate.renderfx (belegt ueber den Struct-Abgleich:
	// rendermode +0x2F8, renderamt +0x2FC, rendercolor +0x300, renderfx +0x304)
	pViewModel->curstate.renderfx = 0;
}

void EV_AddClusterEffect(const Vector& vecOrigin, int iType, float flMaxRadius);

// Effektliste Typ 5 (Rohr/Ringband) und Typ 6 (additives Quad). Retail:
// Konstruktor 0x10002810, Anhang 0x10002ae0, Consumer FUN_10038ab0 bzw.
// FUN_10038c80. Details in findings/client/client_event_table_2026-11-04.md
// (Ergebnisse 7/16/17) und references/effect-list-layout.md.
void EV_AddEffectTube(const Vector& vecOrigin, const Vector& vecDir, float flRate,
	float flAlphaStart, float flAlphaRate, int iSprite);
void EV_AddEffectQuad(const Vector& vecOrigin, const Vector& vecDir, float flWidth,
	float flAlphaRate, int iSprite);
void EV_AddEffectEntityTube(int iEntIndex, const Vector& vecDir, float flRate,
	float flAlphaStart, float flAlphaRate, int iSprite);

// Gemeinsame ShellInfo aus Retail 0x10028b70 (siehe EV_ShotgunFire):
//   pos = origin + view_ofs + flForward*forward + flRight*right - 12*up
//   vel = velocity + 25*forward + rand(50,70)*right + rand(100,150)*up
// Der Up-Versatz ist in beiden Retail-Aufrufern (Schrotflinte 32/6,
// Minigun 20/4) konstant -12.0; view_ofs 28 bzw. geduckt 12.
void EV_GetShellInfo(const event_args_t* args, float flForward, float flRight,
	Vector& vecPos, Vector& vecVel);

// Retail 0x10009e10: R_Explosion(origin, sprites/kaboom.spr, 3.0, 15.0, 12),
// danach ein Sound aus der Gruppe, die `bparam1` (Retail +0x40) waehlt:
// 0 -> weapons/kaboom1..3.wav (Pitch 85 + Random(0,31)), sonst
// manta/carpetbomb1..3.wav (Pitch normal). Auswahl per RandomLong(0,2).
void EV_ClusterExplosion(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_ClusterExplosion\n");
	static const char* const kszKaboom[] = {"weapons/kaboom1.wav", "weapons/kaboom2.wav", "weapons/kaboom3.wav"};
	static const char* const kszCarpet[] = {"manta/carpetbomb1.wav", "manta/carpetbomb2.wav", "manta/carpetbomb3.wav"};

	const int idx = args->entindex;
	const int iKaboom = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/kaboom.spr");

	if (iKaboom > 0)
		gEngfuncs.pEfxAPI->R_Explosion((float*)&args->origin, iKaboom, 3.0f, 15.0f, 12);

	const int iRand = gEngfuncs.pfnRandomLong(0, 2);

	if (args->bparam1 == 0)
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, (float*)&args->origin, CHAN_STATIC, kszKaboom[iRand],
			1.0f, 0.8f, 0, 85 + gEngfuncs.pfnRandomLong(0, 31));
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, (float*)&args->origin, CHAN_STATIC, kszCarpet[iRand],
			1.0f, 0.8f, 0, PITCH_NORM);
	}

	// Retail haengt zusaetzlich einen Effektlisteneintrag mit 90.0 an (Typ 1).
	EV_AddClusterEffect(Vector(args->origin), 1, 90.0f);
}

// Retail 0x1000a790: AngleVectors(args->angles, fwd/right/up), Sprite
// sprites/smoke.spr aufloesen, dann ein Effektlisteneintrag Typ 5:
// Rate 2.4, Alpha-Start 220.0, Alpha-Abnahme 4.0 (Push-Folge `2.4, 5, 220.0,
// 4.0`), Laengsgrenze 10.0 (erstes Argument, im Typ-5-Consumer ungenutzt).
void EV_Cylinder(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_Cylinder\n");
	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	static int iSmoke = 0;

	if (!iSmoke)
		iSmoke = gEngfuncs.pfnSPR_Load("sprites/smoke.spr");

	if (iSmoke <= 0)
		return;

	EV_AddEffectTube(Vector(args->origin), forward, 2.4f, 220.0f, 4.0f, iSmoke);
}

// Retail 0x10007940: Sprites sprites/gaussbeam2.spr (0x1007652c),
// sprites/gausspark.spr (0x10076514) und Modell models/w_gauss.mdl
// (0x10076500) aufloesen; AngleVectors + Trace 8192 (0x46000000); Waffen-
// animation ueber Slot +0x40; im Alternativzweig (iparam1 == 1) Sound
// weapons/gauss_fire4.wav (0x100764e8).
// Trefferpfad: Sprite sprites/gaussimpact.spr (0x100764d0) plus
// Effektlisteneintrag 8.0 (0x41000000) bei Brush-Treffern, sonst 15.0
// (0x41f00000) bei lebenden Zielen; zum Schluss Decal "{pulse2" (0x100764c8).
void EV_GaussPistol(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_GaussPistol\n");
	const int idx = args->entindex;
	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	const int iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gaussbeam2.spr");
	const int iSpark = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gausspark.spr");
	const int iImpact = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gaussimpact.spr");

	(void)iBeam;

	if (args->iparam1 == 1)
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, (float*)&args->origin, CHAN_WEAPON, "weapons/gauss_fire4.wav",
			1.0f, 0.8f, 0, 85 + gEngfuncs.pfnRandomLong(0, 31));
	}

	const Vector vecSrc(args->origin);
	const Vector vecEnd = vecSrc + forward * 8192.0f;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	if (iSpark > 0)
	{
		// Ohne FTENT_SPRANIMATE: Xash entfernt animierte Temp-Sprites erst, wenn
		// die Animation durchlaeuft; bei framerate 0 bleibt der Funke sonst
		// dauerhaft an der Wand (Spielerbeobachtung 2026-11-07).
		gEngfuncs.pEfxAPI->R_TempSprite((float*)&tr.endpos, vec3_origin, 0.3f, iSpark,
			kRenderGlow, kRenderFxNone, 1.0f, 0.3f, FTENT_FADEOUT);
	}

	if (pe && pe->solid == SOLID_BSP)
	{
		if (iImpact > 0)
		{
			gEngfuncs.pEfxAPI->R_TempSprite((float*)&tr.endpos, vec3_origin, 0.4f, iImpact,
				kRenderGlow, kRenderFxNone, 1.0f, 0.5f, FTENT_FADEOUT);
		}

		EV_AddClusterEffect(Vector(tr.endpos), 1, 8.0f);
	}
	else
	{
		EV_AddClusterEffect(Vector(tr.endpos), 1, 15.0f);
	}

	if (!pe || !((pe->solid == SOLID_BSP) || (pe->movetype == MOVETYPE_PUSHSTEP)))
		return;

	if (gEngfuncs.pfnGetCvarFloat("r_decals") == 0.0f)
		return;

	gEngfuncs.pEfxAPI->R_DecalShoot(
		gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{pulse2")),
		gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
}

void EV_KataImpact(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_KataImpact\n");
	const int idx = args->entindex;
	Vector forward;
	AngleVectors(args->angles, forward, nullptr, nullptr);

	const Vector vecSrc(args->origin);
	const Vector vecEnd = vecSrc + forward * 2048.0f;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	// Retail 0x10008810: DLight (Radius 130, RGB 0/255/255, Decay 80) am Einschlag,
	// danach Decal "{scorch1" auf Brush-Treffern.
	dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0);

	if (dl)
	{
		dl->origin = tr.endpos;
		dl->radius = 130.0f;
		dl->color.r = 0;
		dl->color.g = 255;
		dl->color.b = 255;
		dl->die = gEngfuncs.GetClientTime() + 0.1f;
		dl->decay = 80.0f;
	}

	// Retail 0x10008bd8: Effektlisteneintrag Typ 6 (Consumer FUN_10038c80),
	// Breite 130.0, Alpha-Abnahme 12, Sprite sprites/kataimpact.spr. Der
	// Eintrag entsteht vor den Decal-Fruehausstiegen, damit er auch bei
	// r_decals 0 bzw. ohne Brush-Treffer erscheint (wie im frueheren Port).
	// Richtung = Einschlagnormale: das gepushte Richtungsargument an der
	// Konstruktorstelle ist nicht eindeutig zugeordnet (Ergebnis 17).
	static int iKataSprite = 0;

	if (!iKataSprite)
		iKataSprite = gEngfuncs.pfnSPR_Load("sprites/kataimpact.spr");

	if (iKataSprite > 0)
		EV_AddEffectQuad(Vector(tr.endpos), Vector(tr.plane.normal), 130.0f, 12.0f, iKataSprite);

	if (!pe || !((pe->solid == SOLID_BSP) || (pe->movetype == MOVETYPE_PUSHSTEP)))
		return;

	if (gEngfuncs.pfnGetCvarFloat("r_decals") == 0.0f)
		return;

	gEngfuncs.pEfxAPI->R_DecalShoot(
		gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{scorch1")),
		gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
}

// Retail 0x1000a4f0: Beam mit sprites/zbeam5.spr (0x1007673c). Aus der
// Push-Sequenz vor dem Beam-Aufruf: Leben 0.1 (0x3dcccccd), Breite 2.5
// (0x40200000), Amplitude 0.25 (0x3e800000), Helligkeit 200.0 (0x43480000),
// Start-Radius 0, End-Radius 10.0 (0x41200000), Farbe 1/1/1; Trace-Laenge
// 8192 (0x46000000).
void EV_Lightning(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_Lightning\n");
	const int iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/zbeam5.spr");

	if (iBeam <= 0)
		return;

	Vector forward;
	AngleVectors(args->angles, forward, nullptr, nullptr);

	const Vector vecSrc(args->origin);
	const Vector vecEnd = vecSrc + forward * 8192.0f;

	gEngfuncs.pEfxAPI->R_BeamPoints((float*)&vecSrc, (float*)&vecEnd, iBeam, 0.1f, 2.5f, 0.25f,
		200.0f, 0.0f, 0.0f, 10.0f, 1.0f, 1.0f, 1.0f);
}

// Retail 0x10006550: Waffenanimation 2 (bzw. 4 bei fparam1 < fparam2),
// Sound weapons/hks1..3.wav (Volume 0.65, Atten 0.8, zufaelliger Pitch),
// Huelse models/shell.mdl ueber die gemeinsame ShellInfo 0x10028b70 mit
// Achsen-Skalen 20.0/-12.0/4.0 (Pushes 0x100066ef..0x100066fd) und
// soundtype 1 = TE_BOUNCE_SHELL (0x100068cf). Dazu ruft Retail 0x10028ca0,
// das beim lokalen Spieler Bit 2 in `+0x2ec` setzt — Feld unbelegt, daher
// nicht portiert.
void EV_MinigunFire(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_MinigunFire\n");
	static int iShellModel = 0;
	static const char* const kszFire[] = {"weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav"};

	if (!iShellModel)
		iShellModel = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl");

	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	if (args->iparam2 == 0)
		gEngfuncs.pEventAPI->EV_WeaponAnimation(2, args->bparam1);
	else if (args->fparam1 < args->fparam2)
		gEngfuncs.pEventAPI->EV_WeaponAnimation(4, args->bparam1);

	const int iSound = gEngfuncs.pfnRandomLong(0, 2);

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, (float*)&args->origin, CHAN_WEAPON, kszFire[iSound],
		0.65f, 0.8f, 0, 98 + gEngfuncs.pfnRandomLong(0, 15));

	// Eine Huelse je Schuss (Retail ruft die Huelle ausserhalb einer Schleife).
	if (iShellModel <= 0)
		return;

	Vector vecPos, vecVel;
	EV_GetShellInfo(args, 20.0f, 4.0f, vecPos, vecVel);

	Vector vecAngles(0.0f, 0.0f, args->angles[2]);

	TEMPENTITY* pShell = gEngfuncs.pEfxAPI->R_TempModel((float*)&vecPos, (float*)&vecVel,
		(float*)&vecAngles, 2.5f, iShellModel, TE_BOUNCE_SHELL);

	gEngfuncs.Con_DPrintf("[ev] minigun shell pos=%.1f/%.1f/%.1f vel=%.1f/%.1f/%.1f flags=0x%x\n",
		vecPos.x, vecPos.y, vecPos.z, vecVel.x, vecVel.y, vecVel.z,
		pShell ? pShell->flags : 0);
}

// Retail 0x10007410. Vollstaendig aus dem Decompilat (2026-11-07 erneut):
//   iVar1 = *(int *)(args + 0x40) = **bparam1**, iVar2 = *(int *)(args + 0x44)
//   = **bparam2**. Die frueher hier notierten iparam1/iparam2 (+0x38/+0x3c)
//   waren falsch — event_args_t: iparam1 +0x38, iparam2 +0x3c, bparam1 +0x40,
//   bparam2 +0x44.
//   Ziel ist das Viewmodel (Engine-Slot +0xD0 = pfnGetViewModel,
//   bestaetigt ueber Tabellenbasis 0x1008a3e0: Slot 0x1008a4b0 = idx 52).
//   bparam1 != 0:  renderfx = 0x13, renderamt = 0x3c
//                  bparam2 == 0 -> rendercolor = (255,100,0)  [orange Hitze]
//                  sonst        -> rendercolor = (100,100,255) [blaue Hitze]
//   bparam1 == 0:  renderfx = 0
// Offsets gegen entity_state_t geprueft: rendermode +0x2F8, renderamt +0x2FC,
// rendercolor +0x300, renderfx +0x304 (curstate beginnt bei +0x2B0).
void EV_MinigunHeat(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_MinigunHeat\n");
	if (args->entindex == 0)
		return;

	cl_entity_t* pViewModel = gEngfuncs.GetViewModel();
	if (!pViewModel)
		return;

	if (args->bparam1)
	{
		pViewModel->curstate.renderfx = 19;
		pViewModel->curstate.renderamt = 60;

		if (args->bparam2 == 0)
		{
			pViewModel->curstate.rendercolor.r = 255;
			pViewModel->curstate.rendercolor.g = 100;
			pViewModel->curstate.rendercolor.b = 0;
		}
		else
		{
			pViewModel->curstate.rendercolor.r = 100;
			pViewModel->curstate.rendercolor.g = 100;
			pViewModel->curstate.rendercolor.b = 255;
		}
	}
	else
	{
		pViewModel->curstate.renderfx = 0;
	}

	gEngfuncs.Con_DPrintf("[ev] heat b1=%d b2=%d vz=%p fx=%d amt=%d rgb=%d/%d/%d\n",
		args->bparam1, args->bparam2, (void*)pViewModel,
		pViewModel->curstate.renderfx, pViewModel->curstate.renderamt,
		pViewModel->curstate.rendercolor.r, pViewModel->curstate.rendercolor.g,
		pViewModel->curstate.rendercolor.b);
}

// Retail 0x100055b0: Push-Sequenz vor `call 0x100021d0` ergibt
// cShots = 1, Distanz 2048.0 (0x45000000), Kugeltyp 2
// (= BULLET_PLAYER_MP5), Tracer-Frequenz 1.
void EV_MonsterMechagun(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_MonsterMechagun\n");
	const int idx = args->entindex;
	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	Vector vecSrc(args->origin);
	int iTracer = 0;

	EV_HLDM_FireBullets(idx, forward, right, up, 1, (float*)&vecSrc, forward, 2048.0f,
		BULLET_PLAYER_MP5, 1, &iTracer, args->fparam1, args->fparam2);
}

// Retail 0x10004c90: aus der Push-Sequenz vor dem Aufruf von
// EV_HLDM_FireBullets (0x100021d0) belegt: cShots = 8, Distanz 2048.0
// (0x45000000), Kugeltyp 4 (= BULLET_PLAYER_BUCKSHOT), Tracer-Frequenz 0.
// Huelse models/shotgunshell.mdl; Animation/Soundteil bleibt offen.
void EV_MonsterShotgun(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_MonsterShotgun\n");
	const int idx = args->entindex;
	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl");

	Vector vecSrc(args->origin);
	int iTracer = 0;

	EV_HLDM_FireBullets(idx, forward, right, up, 8, (float*)&vecSrc, forward, 2048.0f,
		BULLET_PLAYER_BUCKSHOT, 0, &iTracer, args->fparam1, args->fparam2);
}

void EV_NoOpEvent(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_NoOpEvent\n");
	(void)args; // bewusst leer (Retail-Handler ohne Wirkung)
}

void EV_RapidImpact(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_RapidImpact\n");
	const int idx = args->entindex;
	Vector forward;
	AngleVectors(args->angles, forward, nullptr, nullptr);

	const Vector vecSrc(args->origin);
	const Vector vecEnd = vecSrc + forward * 2048.0f;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	// Retail 0x10009860: Sound (Pitch 85), Puff-Sprite am Brush-Treffer, Decal "{shot5".
	gEngfuncs.pEventAPI->EV_PlaySound(idx, tr.endpos, CHAN_STATIC, "weapons/gauss_spritesmall.wav",
		1.0f, ATTN_NORM, 0, 85);

	if (!pe || pe->solid != SOLID_BSP)
		return;

	const int iPuff = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gausspuff.spr");

	if (iPuff > 0)
	{
		gEngfuncs.pEfxAPI->R_TempSprite((float*)&tr.endpos, vec3_origin, 0.3f, iPuff,
			kRenderGlow, kRenderFxNone, 1.0f, 0.5f, FTENT_FADEOUT);
	}

	if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0.0f)
	{
		gEngfuncs.pEfxAPI->R_DecalShoot(
			gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{shot5")),
			gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
	}
}

// Retail 0x1000a660: AngleVectors(args->angles), Sprite sprites/firebeam.spr
// aufloesen, danach ein Effektlisteneintrag Typ 5: Rate 2.4, Alpha-Start
// 255.0, Alpha-Abnahme 15.0, Laengsgrenze 16.0 (erstes Argument, im
// Typ-5-Consumer ungenutzt). Consumer FUN_10038ab0 zeichnet daraus ein
// additives Ringband (32 Segmente, Radius 8.0, Laenge 1 + age*rate*30 + 7).
void EV_RocketSmoke(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_RocketSmoke\n");
	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	// HSPRITE aus pfnSPR_Load (Retail: pfnSPR_Load + GetSpritePointer); ein
	// Modellindex aus EV_FindModelIndex liefert bei GetSpritePointer NULL.
	static int iBeam = 0;

	if (!iBeam)
		iBeam = gEngfuncs.pfnSPR_Load("sprites/firebeam.spr");

	if (iBeam <= 0)
		return;

	EV_AddEffectTube(Vector(args->origin), forward, 2.4f, 255.0f, 15.0f, iBeam);
}

void EV_RustbitImpact(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_RustbitImpact\n");
	const int idx = args->entindex;
	Vector forward;
	AngleVectors(args->angles, forward, nullptr, nullptr);

	const Vector vecSrc(args->origin);
	const Vector vecEnd = vecSrc + forward * 2048.0f;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	// Retail 0x10008150: nur Decal "{pulse" auf Brush-Treffern.
	if (pe && !((pe->solid == SOLID_BSP) || (pe->movetype == MOVETYPE_PUSHSTEP)))
		return;

	if (gEngfuncs.pfnGetCvarFloat("r_decals") == 0.0f)
		return;

	gEngfuncs.pEfxAPI->R_DecalShoot(
		gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{pulse")),
		gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
}

// Retail 0x10002e80 (Event "events/gw_shotgun.sc").
//
// Belegt aus Decompilat + Disassembly + Engine-Slot-Tabelle:
//   * pEventAPI+0xc = EV_FindModelIndex("models/shotgunshell.mdl") (0x10076428)
//   * args->iparam1 (+0x38) waehlt per Sprungtabelle 0x100031d0 einen
//     Streuwinkel: Fall 1 = 0.05234f (0x3d566278), Fall 2 = 0.10906f
//     (0x3ddf5ad9), Fall 3/4/Default = 0.17365f (0x3e31d14e)
//   * args->iparam2 (+0x3c) = Anzahl der ausgeworfenen Huelsen (Schleife jl)
//   * pro Huelse: pEfxAPI+0xc0 = R_TempModel(pos, dir, angles, 2.5f,
//     modelIndex, soundtype) — sechs Argumente, Leben 2.5 (0x40200000)
//     aufgerufen ueber die Huelle 0x10028b20 (setzt zwei Winkel-Komponenten
//     auf 0 und ruft dann *0xc0(pEfxAPI)); der Aufrufer schiebt als sechstes
//     Argument die Konstante **2** = TE_BOUNCE_SHOTSHELL (0x100030a6).
//     Das ist der Grund fuer den fehlenden Tumble: die Engine setzt
//     FTENT_ROTATE nur fuer TE_BOUNCE_SHELL/TE_BOUNCE_SHOTSHELL.
//   * am Ende "weapons/sbarrel1.wav" (0x10076410) mit Pitch
//     93 + pfnRandomLong(0, 0x1f)
//
// 1:1 nach 0x10028b70 (ShellInfo; Decompilat + Stackzuordnung im Aufrufer):
//   view_ofs = (0,0,28); ist der Schuetze lokal -> EV_LocalPlayerViewheight
//   (pEventAPI+0x18), sonst args->ducking == 1 -> view_ofs.z = 12
//   je Huelse: fR = pfnRandomFloat(50,70), fU = pfnRandomFloat(100,150)
//   shellvelocity = args->velocity + 25*forward + fR*right + fU*up
//   shellorigin   = args->origin + view_ofs + 32*forward + 6*right - 12*up
//   Winkelargument von R_TempModel ist (0,0,args->angles[2]) — die Huelle
//   0x10028b20 nullt die ersten beiden Komponenten; der Spin kommt aus
//   FTENT_ROTATE (soundtype 2), nicht aus diesem Vektor.
void EV_ShotgunFire(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_ShotgunFire\n");
	static int iShellModel = 0;

	if (!iShellModel)
		iShellModel = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl");

	if (args->iparam2 <= 0)
		return;

	// Entwickler-Ausgabe: belegt zur Laufzeit, dass der Handler laeuft
	// (Xash3D protokolliert Event-Ausfuehrung selbst nicht).
	gEngfuncs.Con_DPrintf("[ev] gw_shotgun: shells=%d spread-case=%d\n",
		args->iparam2, args->iparam1);

	// Streuwinkel je Fall (Sprungtabelle 0x100031d0). Retail nutzt den Wert
	// im Aufrufer fuer die Schrotgarbe; der Huelsenpfad braucht ihn nicht.
	float flSpread;
	switch (args->iparam1)
	{
	case 1:  flSpread = 0.05234f; break;
	case 2:  flSpread = 0.10906f; break;
	default: flSpread = 0.17365f; break;
	}
	(void)flSpread;

	for (int i = 0; i < args->iparam2; i++)
	{
		// Retail zieht fR/fU je Huelse (in 0x10028b70), deshalb hier im Loop.
		Vector vecPos, vecVel;
		EV_GetShellInfo(args, 32.0f, 6.0f, vecPos, vecVel);

		Vector vecAngles(0.0f, 0.0f, args->angles[2]);

		gEngfuncs.Con_DPrintf("[ev] shell %d pos=%.1f/%.1f/%.1f vel=%.1f/%.1f/%.1f model=%d\n",
			i, vecPos.x, vecPos.y, vecPos.z, vecVel.x, vecVel.y, vecVel.z, iShellModel);

		TEMPENTITY* pShell = gEngfuncs.pEfxAPI->R_TempModel((float*)&vecPos, (float*)&vecVel,
			(float*)&vecAngles, 2.5f, iShellModel, TE_BOUNCE_SHOTSHELL);

		// Belegt den Tumble zur Laufzeit: R_TempModel setzt FTENT_ROTATE nur
		// fuer TE_BOUNCE_SHELL/SHOTSHELL und traegt dann eine zufaellige
		// Winkelgeschwindigkeit in baseline.angles ein; HUD_TempEntUpdate
		// (entity.cpp) addiert sie pro Frame auf entity.angles.
		if (pShell)
			gEngfuncs.Con_DPrintf("[ev] shell %d flags=0x%x spin=%.1f/%.1f/%.1f\n",
				i, pShell->flags,
				pShell->entity.baseline.angles[0],
				pShell->entity.baseline.angles[1],
				pShell->entity.baseline.angles[2]);
	}

	gEngfuncs.pfnPlaySoundByNameAtPitch("weapons/sbarrel1.wav", 1.0f,
		93 + gEngfuncs.pfnRandomLong(0, 0x1f));
}

// Retail 0x10008c60: Trace 8192 (0x46000000) von der Waffenposition,
// V_PunchAxis(0, -2.2) fuer den lokalen Spieler (0xc00ccccd), DLight am
// Trefferpunkt (Radius 80 = 0x42a00000, RGB 255/255/0) und die Sprites
// sprites/gaussring.spr (0x100765c0) und sprites/gausspoof.spr (0x100765a8);
// zum Schluss Decal "{pulse2" (0x100764c8) bei Brush-Treffern.
// Naeherung: Retail baut zusaetzlich zwei Effektlisteneintraege (0x41500000,
// 90.0), die hier noch fehlen.
void EV_SniperShot(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_SniperShot\n");
	const int idx = args->entindex;
	Vector vecSrc, forward, right, up;

	AngleVectors(args->angles, forward, right, up);
	VectorCopy(args->origin, vecSrc);

	if (EV_IsLocal(idx))
		V_PunchAxis(0, -2.2f);

	const Vector vecEnd = vecSrc + forward * 8192.0f;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	const int iRing = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gaussring.spr");
	const int iPuff = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/gausspoof.spr");

	// Retail 0x10008c60: kein einzelner Ring am Treffer, sondern eine **Kette**
	// von gaussring-Knoten entlang der Schusslinie - Startversatz 80, Schritt 80,
	// solange der Versatz kleiner als die Trefferdistanz ist; jeder Knoten mit
	// Radius 12.0f (0x41500000). Danach ein gausspoof-Knoten am Treffer.
	// Umsetzung mit R_TempSprite (gleiche Optik, keine Effektlisten-Erweiterung);
	// die Knotenzahl ist auf 24 begrenzt, damit lange Schuesse nicht hunderte
	// Sprites erzeugen.
	// Retail 0x10008c60: Kette von gaussring-Effektknoten entlang der Schusslinie
	// (Startversatz 80, Schritt 80, Radius 12.0). gaussring.spr ist ein
	// Typ-2-Sprite (64x128) — als Billboard gezeichnet steht es senkrecht
	// (Spielerbeobachtung 2026-11-08: "vertikaler Schuss-Trace"). Retail
	// zeichnet die Knoten als Ribbon entlang der Richtung; dieser Renderer
	// fehlt im Port, deshalb wird die Spur bis dahin als Beam mit demselben
	// Sprite und der Retail-Breite 12.0 gezogen (dokumentierte Abweichung).
	if (iRing > 0)
	{
		gEngfuncs.pEfxAPI->R_BeamPoints((float*)&vecSrc, (float*)&tr.endpos, iRing,
			0.6f, 6.0f, 0.0f, 220.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	}

	if (iPuff > 0)
	{
		gEngfuncs.pEfxAPI->R_TempSprite((float*)&tr.endpos, (float*)&forward, 0.3f, iPuff,
			kRenderGlow, kRenderFxNone, 1.0f, 0.3f, FTENT_FADEOUT);
	}

	dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0);

	if (dl)
	{
		dl->origin = tr.endpos;
		dl->radius = 80.0f;
		// Retail setzt 255/0/0 (Byte +0x11 = 0xff, +0x12 = 0), nicht gelb.
		dl->color.r = 255;
		dl->color.g = 0;
		dl->color.b = 0;
		dl->die = gEngfuncs.GetClientTime() + 0.1f;
		dl->decay = 80.0f;
	}

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	if (!pe || !((pe->solid == SOLID_BSP) || (pe->movetype == MOVETYPE_PUSHSTEP)))
		return;

	if (gEngfuncs.pfnGetCvarFloat("r_decals") == 0.0f)
		return;

	gEngfuncs.pEfxAPI->R_DecalShoot(
		gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{pulse2")),
		gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
}

// Retail 0x1000a170: Effektlisteneintrag am Ursprung, danach drei
// Zufallspunkte im Bereich +/-150 (0xc4160000 / 0x44160000) mit Sprites.
// Umgesetzt ist der Effekteintrag (Radiusgrenze 150.0); die drei
// Zufallssprites fehlen noch.
void EV_SphereExplode(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_SphereExplode\n");
	EV_AddClusterEffect(Vector(args->origin), 4, 150.0f);
}

// Retail 0x100097f0: berechnet nur Winkel und ruft danach
// CSprite::AnimateUntilDead (0x10001ea0) - in Retail ein leerer Stub. Der
// Handler hat damit keine sichtbare Wirkung; bewusst leer.
void EV_SpurtBlood(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_SpurtBlood\n");
	(void)args;
}

// Retail 0x10003630: Sound weapons/hks1..3.wav (Pitch 100), dann
// EV_HLDM_FireBullets mit 2 Schuessen, Distanz 8192 (0x46000000),
// Kugeltyp 2 (= BULLET_PLAYER_MP5), Tracer-Frequenz 3.
void EV_TankMGuns(event_args_t* args)
{
	gEngfuncs.Con_DPrintf("[ev] EV_TankMGuns\n");
	static const char* const kszFire[] = {"weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav"};

	const int idx = args->entindex;
	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	const int iRand = gEngfuncs.pfnRandomLong(0, 2);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, (float*)&args->origin, CHAN_WEAPON, kszFire[iRand],
		1.0f, 0.8f, 0, 100);

	Vector vecSrc(args->origin);
	int iTracer = 0;

	EV_HLDM_FireBullets(idx, forward, right, up, 2, (float*)&vecSrc, forward, 8192.0f,
		BULLET_PLAYER_MP5, 3, &iTracer, 6.0f, 6.0f);
}

// HINWEIS (2026-11-07): Alle in diesen Effekten benutzten SPR-Modelle
// (muzzleflash3, gausspuff, gaussring, gausspoof, ballpop, hugeball) tragen im
// Kopf `framerate = 0.0` (geprueft in out/mod/xash3d/rewolf/sprites/). Xash3D
// entfernt einen Temp-Sprite mit FTENT_SPRANIMATE erst am Ende der
// Frame-Animation - bei framerate 0 endet sie nie, der Sprite bleibt dauerhaft
// an der Wand stehen (Spielerbeobachtung 2026-11-07). Deshalb ueberall nur
// FTENT_FADEOUT: der Effekt verschwindet zuverlaessig, laeuft aber nicht durch
// seine Einzelbilder (bewusste Abweichung).

// Duenne Huelle um R_TempSprite (in der verlorenen Fassung vorhanden).
static void EV_TempSprite(const Vector& vecOrigin, const Vector& vecDir, float flScale, int iModelIndex,
	int iRenderMode, int iRenderFx, float flAlpha, float flLife, int iFlags)
{
	gEngfuncs.pEfxAPI->R_TempSprite((float*)&vecOrigin, (float*)&vecDir, flScale, iModelIndex,
		iRenderMode, iRenderFx, flAlpha, flLife, iFlags);
}

//======================
//  OFFENE WIEDERHERSTELLUNG - Hilfsfunktionen (2026-11-06)
//======================
// Diese Helfer gingen beim Dateiunfall verloren; sie sind hier leer, damit der
// Client linkt. Retail-Pseudocode: re-project/out/re/client_pseudo_2026-11-06/.
// ---- Kugel-Effektliste (Retail: Konstruktor 0x10002810, Liste ab DAT_10092658,
// Renderer 0x10037850 mit sprites/fthrower.spr, 202 Vertices / 400 Dreiecke,
// Ringwinkel pi/11 und pi/10; Radius waechst, Alpha faellt.)
struct cluster_effect_t
{
	Vector origin;
	float startTime;
	float maxRadius = 90.0f;
	float life = 1.5f;
	int type = 1;
};

static const int CLUSTER_EFFECT_MAX = 32;
static cluster_effect_t g_rgClusterEffects[CLUSTER_EFFECT_MAX];
static int g_iClusterEffectCount = 0;

// ---- Effektliste, Retail-Knoten (Konstruktor 0x10002810/0x10002870,
// Anhang 0x10002ae0, Liste ab 0x10092658, Knoten 76 Byte / 18 Dwords).
// Portiert sind die Consumer, deren Zeichenrezept vollstaendig dekompiliert
// ist: Typ 5 (FUN_10038ab0, Ringband) und Typ 6 (FUN_10038c80, additives
// Quad). Die Feldnummern entsprechen dem Retail-Knoten:
//   [0] Basislaenge 1.0 | [1..3] Ursprung | [4..6] Richtung | [7] Breite/Radius
//   [8] Rate | [0xb] Alpha-Start | [0xc] Alpha-Abnahme | [0xd] Zeitbasis
//   [0xe] Sprite | [0xf]/[0x10] Frame-Schritt/Frame-Anzahl (nur Typ 6)
// Abweichung: Retail fuehrt EINE zyklische Liste (Ringpuffer ab 64000
// Eintraegen) und dispatcht beim Update ueber [0xa]; hier liegen Typ 5 und
// Typ 6 in getrennten, festen Feldern, weil nur diese beiden Consumer
// portiert sind. Verhalten je Eintrag ist identisch.
struct gm_effect_entry_t
{
	Vector	vecOrigin;
	Vector	vecDir;
	float	flWidth;
	float	flRate;
	float	flAlphaStart;
	float	flAlphaRate;
	float	flTimeBase;
	int	iSprite;
	int	iFrameStep;
	int	iFrameCount;
	// Typ 7 (FUN_10038810): Eintrag haengt an einer Entity ([0xf]); deren
	// Ursprung wird beim Update gelesen, verschwindet sie, faellt der Eintrag weg.
	int	iAnchorEntity;
	float	flDrawRadius;
	float	flDrawLengthBase;
};

static const int GM_EFFECT_TUBE_MAX = 64;
static const int GM_EFFECT_QUAD_MAX = 64;
static gm_effect_entry_t g_rgEffectTubes[GM_EFFECT_TUBE_MAX];
static gm_effect_entry_t g_rgEffectQuads[GM_EFFECT_QUAD_MAX];
static int g_iEffectTubeCount = 0;
static int g_iEffectQuadCount = 0;

// Retail 0x10028b70 (ShellInfo), von Schrotflinte (32/6) und Minigun (20/4)
// gemeinsam genutzt. Siehe Prototyp oben; Beweis im Findings-Dokument
// huelsen_bewegung_2026_11_06.md (Decompilat + Stackzuordnung).
void EV_GetShellInfo(const event_args_t* args, float flForward, float flRight,
	Vector& vecPos, Vector& vecVel)
{
	Vector view_ofs(0.0f, 0.0f, 28.0f);

	if (gEngfuncs.pEventAPI->EV_IsLocal(args->entindex))
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	else if (args->ducking == 1)
		view_ofs.z = 12.0f;

	Vector forward, right, up;
	AngleVectors(args->angles, forward, right, up);

	const float fR = gEngfuncs.pfnRandomFloat(50.0f, 70.0f);
	const float fU = gEngfuncs.pfnRandomFloat(100.0f, 150.0f);

	vecPos = Vector(args->origin) + view_ofs
		+ forward * flForward
		+ right * flRight
		- up * 12.0f;

	vecVel = Vector(args->velocity)
		+ forward * 25.0f
		+ right * fR
		+ up * fU;
}

// Retail: Konstruktor 0x10002810 + Anhang 0x10002ae0.
void EV_AddClusterEffect(const Vector& vecOrigin, int iType, float flMaxRadius)
{
	if (g_iClusterEffectCount >= CLUSTER_EFFECT_MAX)
		return;

	cluster_effect_t& e = g_rgClusterEffects[g_iClusterEffectCount++];
	e.origin = vecOrigin;
	e.startTime = gEngfuncs.GetClientTime();
	e.maxRadius = flMaxRadius > 0.0f ? flMaxRadius : 90.0f;
	e.life = 1.5f;
	e.type = iType;
}

void EV_ResetClusterExplosionEffects()
{
	g_iClusterEffectCount = 0;
	g_iEffectTubeCount = 0;
	g_iEffectQuadCount = 0;
}

void EV_UpdateClusterExplosionEffects()
{
	if (g_iClusterEffectCount <= 0)
		return;

	const float flTime = gEngfuncs.GetClientTime();

	// WICHTIG: GetSpritePointer nimmt ein HSPRITE (Handle aus pfnSPR_Load),
	// keinen Modellindex. Mit EV_FindModelIndex kam hier immer NULL heraus,
	// d. h. der Kugel-Effekt hat nie gezeichnet (Fix 2026-11-08).
	static int iSprite = 0;

	if (!iSprite)
		iSprite = gEngfuncs.pfnSPR_Load("sprites/fthrower.spr");

	if (iSprite <= 0)
	{
		g_iClusterEffectCount = 0;
		return;
	}

	const int iRings = 10;
	const int iSegments = 20;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(iSprite), 0);
	gEngfuncs.pTriAPI->Begin(TRI_TRIANGLES);

	int iAlive = 0;

	for (int i = 0; i < g_iClusterEffectCount; i++)
	{
		cluster_effect_t& e = g_rgClusterEffects[i];
		const float flAge = flTime - e.startTime;

		if (flAge >= e.life)
			continue;

		g_rgClusterEffects[iAlive++] = e;

		const float flFrac = flAge / e.life;
		const float flRadius = e.maxRadius * flFrac;
		const float flAlpha = 1.0f - flFrac;

		gEngfuncs.pTriAPI->Color4f(0.4f, 0.6f, 1.0f, flAlpha);

		// Pfadbeleg wie bei Typ 5/6: ersten Zeichenaufruf einmalig protokollieren.
		static int s_iClusterDrawLog = 0;

		if (s_iClusterDrawLog < 4)
		{
			gEngfuncs.Con_DPrintf("[fx] cluster draw age=%.2f alpha=%.2f radius=%.1f n=%d\n",
				flAge, flAlpha, flRadius, g_iClusterEffectCount);
			s_iClusterDrawLog++;
		}

		for (int r = 0; r < iRings; r++)
		{
			const float flA0 = M_PI * (float)r / (float)(iRings + 1);
			const float flA1 = M_PI * (float)(r + 1) / (float)(iRings + 1);

			for (int s = 0; s < iSegments; s++)
			{
				const float flT0 = 2.0f * M_PI * (float)s / (float)iSegments;
				const float flT1 = 2.0f * M_PI * (float)(s + 1) / (float)iSegments;

				Vector v[4];
				v[0] = e.origin + Vector(sinf(flA0) * cosf(flT0), sinf(flA0) * sinf(flT0), cosf(flA0)) * flRadius;
				v[1] = e.origin + Vector(sinf(flA1) * cosf(flT0), sinf(flA1) * sinf(flT0), cosf(flA1)) * flRadius;
				v[2] = e.origin + Vector(sinf(flA1) * cosf(flT1), sinf(flA1) * sinf(flT1), cosf(flA1)) * flRadius;
				v[3] = e.origin + Vector(sinf(flA0) * cosf(flT1), sinf(flA0) * sinf(flT1), cosf(flA0)) * flRadius;

				gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
				gEngfuncs.pTriAPI->Vertex3fv((float*)&v[0]);
				gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f);
				gEngfuncs.pTriAPI->Vertex3fv((float*)&v[1]);
				gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
				gEngfuncs.pTriAPI->Vertex3fv((float*)&v[2]);

				gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
				gEngfuncs.pTriAPI->Vertex3fv((float*)&v[2]);
				gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f);
				gEngfuncs.pTriAPI->Vertex3fv((float*)&v[3]);
				gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
				gEngfuncs.pTriAPI->Vertex3fv((float*)&v[0]);
			}
		}
	}

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

	g_iClusterEffectCount = iAlive;
}

// ---- Typ 5: Ringband ---------------------------------------------------
// Retail FUN_100379f0(start, dir, laenge, radius=8.0, segmente=32, alpha):
// RenderMode(5) additiv, CullFace(TRI_NONE), Begin(TRI_QUAD_STRIP),
// Color4f(1,1,1, alpha/255); je Schritt i = 0..segmente:
//   Rollwinkel = i/segmente * 360 Grad auf die aus `dir` gewonnenen
//   (pitch,yaw)-Winkel, dann AngleVectors -> right/up;
//   A = start + right*radius - up*laenge, B = start - up*laenge;
//   TexCoord2f(0, i/n) A, TexCoord2f(1, i/n) B.
// Am Ende End() und RenderMode(0). Beleg: 0x100379f0 (Disassembly bis
// 0x10037bc3), Rollwinkel aus `fmod(i/n*360, 360)` an 0x10037a89.
static void EV_DrawEffectTube(const Vector& vecStart, const Vector& vecDir,
	float flLength, float flRadius, int iSegments, float flAlpha)
{
	if (iSegments <= 2)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	gEngfuncs.pTriAPI->Begin(TRI_QUAD_STRIP);
	gEngfuncs.pTriAPI->Color4f(1.0f, 1.0f, 1.0f, flAlpha * (1.0f / 255.0f));

	float vecAngles[3];
	VectorAngles((const float*)&vecDir, vecAngles);

	for (int i = 0; i <= iSegments; i++)
	{
		const float flFrac = (float)i / (float)iSegments;
		vecAngles[2] = flFrac * 360.0f;

		Vector vecForward, vecRight, vecUp;
		AngleVectors(vecAngles, vecForward, vecRight, vecUp);

		const Vector vecInner = vecStart - vecUp * flLength;
		const Vector vecOuter = vecInner + vecRight * flRadius;

		gEngfuncs.pTriAPI->TexCoord2f(0.0f, flFrac);
		gEngfuncs.pTriAPI->Vertex3fv((float*)&vecOuter);
		gEngfuncs.pTriAPI->TexCoord2f(1.0f, flFrac);
		gEngfuncs.pTriAPI->Vertex3fv((float*)&vecInner);
	}

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

void EV_AddEffectTube(const Vector& vecOrigin, const Vector& vecDir, float flRate,
	float flAlphaStart, float flAlphaRate, int iSprite)
{
	if (g_iEffectTubeCount >= GM_EFFECT_TUBE_MAX)
		return;

	gm_effect_entry_t& e = g_rgEffectTubes[g_iEffectTubeCount++];
	e.vecOrigin = vecOrigin;
	e.vecDir = vecDir;
	e.flWidth = 0.0f;		// [7] wird von Typ 5 nicht gelesen
	e.flRate = flRate;
	e.flAlphaStart = flAlphaStart;
	e.flAlphaRate = flAlphaRate;
	e.flTimeBase = gEngfuncs.GetClientTime();
	e.iSprite = iSprite;
	e.iFrameStep = 0;
	e.iFrameCount = 0;
	e.iAnchorEntity = -1;
	e.flDrawRadius = 8.0f;
	e.flDrawLengthBase = 1.0f + 7.0f;
}

// Retail 0x10038810 (Typ 7): Ringband wie Typ 5, aber am Ursprung der Entity
// aus [0xf] verankert, Radius 4.5, Laenge [0] + 8.0 (konstant), Startpunkt um
// dir * (Alter * [8] * 30) versetzt. Beleg: Decompilat FUN_10038810.
void EV_AddEffectEntityTube(int iEntIndex, const Vector& vecDir, float flRate,
	float flAlphaStart, float flAlphaRate, int iSprite)
{
	if (g_iEffectTubeCount >= GM_EFFECT_TUBE_MAX)
		return;

	gm_effect_entry_t& e = g_rgEffectTubes[g_iEffectTubeCount++];
	e.vecOrigin = vec3_origin;
	e.vecDir = vecDir;
	e.flWidth = 4.0f;			// [7] Konstruktorargument (batbotbeam: 4.0)
	e.flRate = flRate;
	e.flAlphaStart = flAlphaStart;
	e.flAlphaRate = flAlphaRate;
	e.flTimeBase = gEngfuncs.GetClientTime();
	e.iSprite = iSprite;
	e.iFrameStep = 0;
	e.iFrameCount = 0;
	e.iAnchorEntity = iEntIndex;
	e.flDrawRadius = 4.5f;
	e.flDrawLengthBase = 1.0f + 8.0f;
}

void EV_UpdateEffectTubes()
{
	if (g_iEffectTubeCount <= 0)
		return;

	const float flTime = gEngfuncs.GetClientTime();
	int iAlive = 0;

	for (int i = 0; i < g_iEffectTubeCount; i++)
	{
		gm_effect_entry_t e = g_rgEffectTubes[i];
		const float flAge = flTime - e.flTimeBase;

		// Retail: alpha = [0xb] - age*[0xc]*30 (UNK_10061248 = 30.0);
		// Abbau, sobald alpha < 1.0 bzw. alpha < 5.0 (FUN_10038ab0/0x10038810).
		const float flAlpha = e.flAlphaStart - flAge * e.flAlphaRate * 30.0f;

		if (flAlpha < 5.0f)
			continue;

		// Typ 7: Startpunkt = Ursprung der verankerten Entity + dir*age*rate*30;
		// fehlt die Entity, wird der Eintrag entfernt (Retail prueft das Feld
		// [0xf] ueber CL_GetEntityByIndex und baut den Eintrag sonst ab).
		Vector vecStart = e.vecOrigin;
		float flLength = flAge * e.flRate * 30.0f + e.flDrawLengthBase;

		if (e.iAnchorEntity >= 0)
		{
			cl_entity_t* pAnchor = gEngfuncs.GetEntityByIndex(e.iAnchorEntity);

			if (!pAnchor)
				continue;

			vecStart = pAnchor->origin + e.vecDir * (flAge * e.flRate * 30.0f);
			flLength = e.flDrawLengthBase;
		}

		g_rgEffectTubes[iAlive++] = e;

		// Retail bindet das Sprite aus [0xe] mit Frame 0 und zeichnet nur,
		// wenn das Feld gesetzt ist; der Eintrag lebt dann weiter.
		const struct model_s* pSprite = gEngfuncs.GetSpritePointer(e.iSprite);

		if (!pSprite)
		{
			static bool s_bWarned = false;

			if (!s_bWarned)
			{
				gEngfuncs.Con_DPrintf("[fx] tube: kein Sprite fuer Handle %d\n", e.iSprite);
				s_bWarned = true;
			}

			continue;
		}

		gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)pSprite, 0);

		// Pfadbeleg: die ersten Zeichenaufrufe einmalig protokollieren, damit
		// "Event gelaufen" von "Renderer hat gezeichnet" unterscheidbar ist.
		static int s_iDrawLog = 0;

		if (s_iDrawLog < 8)
		{
			gEngfuncs.Con_DPrintf("[fx] tube draw age=%.2f alpha=%.0f len=%.1f radius=%.1f anchor=%d n=%d\n",
				flAge, flAlpha, flLength, e.flDrawRadius, e.iAnchorEntity, g_iEffectTubeCount);
			s_iDrawLog++;
		}

		EV_DrawEffectTube(vecStart, e.vecDir, flLength, e.flDrawRadius, 32, flAlpha);
	}

	g_iEffectTubeCount = iAlive;
}

// ---- Typ 6: additives Quad --------------------------------------------
// Retail FUN_10038c80 ab `if ([0xa] == 6)`:
//   alpha = 255.0 - age*[0xc]*30 (UNK_10061258/UNK_10061248), mind. 1.0,
//   Abbau bei alpha < 4.0 (UNK_1005d198);
//   Breite = [7], solange [8] == 0, sonst age*[8]*30 + [0xb] + 1.0;
//   Frame = (int)fmod(age*[0xf], [0x10]); Abbau zusaetzlich, wenn [0x11]
//   gesetzt und age*[0xf] > [0x11];
//   Basis Right/Up aus AngleVectors(VectorAngles(dir)), Quad der Kantenlaenge
//   `Breite` um origin + dir; RenderMode(5), CullFace(TRI_FRONT),
//   Begin(TRI_QUADS), Color4f(1,1,1, alpha/255), TexCoords
//   (0,0) (0,1) (1,1) (1,0).
void EV_AddEffectQuad(const Vector& vecOrigin, const Vector& vecDir, float flWidth,
	float flAlphaRate, int iSprite)
{
	if (g_iEffectQuadCount >= GM_EFFECT_QUAD_MAX)
		return;

	gm_effect_entry_t& e = g_rgEffectQuads[g_iEffectQuadCount++];
	e.vecOrigin = vecOrigin;
	e.vecDir = vecDir;
	e.flWidth = flWidth;
	e.flRate = 0.0f;		// [8] = 0: Breite bleibt [7]
	e.flAlphaStart = 3.0f;	// [0xb], nur im Wachstumszweig relevant
	e.flAlphaRate = flAlphaRate;
	e.flTimeBase = gEngfuncs.GetClientTime();
	e.iSprite = iSprite;
	e.iFrameStep = 18;		// [0xf] (kataimpact: 18)
	e.iFrameCount = 18;		// [0x10] (kataimpact: 18)
}

void EV_UpdateEffectQuads()
{
	if (g_iEffectQuadCount <= 0)
		return;

	const float flTime = gEngfuncs.GetClientTime();
	int iAlive = 0;

	for (int i = 0; i < g_iEffectQuadCount; i++)
	{
		gm_effect_entry_t e = g_rgEffectQuads[i];
		const float flAge = flTime - e.flTimeBase;

		float flAlpha = 255.0f - flAge * e.flAlphaRate * 30.0f;

		if (flAlpha < 1.0f)
			flAlpha = 1.0f;

		if (flAlpha < 4.0f)
			continue;

		g_rgEffectQuads[iAlive++] = e;

		const struct model_s* pSprite = gEngfuncs.GetSpritePointer(e.iSprite);

		if (!pSprite)
			continue;

		float flWidth = e.flWidth;

		if (e.flRate != 0.0f)
			flWidth = flAge * e.flRate * 30.0f + e.flAlphaStart + 1.0f;

		float flFrame = flAge * (float)e.iFrameStep;

		if (e.iFrameCount > 0)
			flFrame = fmodf(flFrame, (float)e.iFrameCount);

		gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)pSprite, (int)flFrame);

		// Pfadbeleg wie bei Typ 5: erste Zeichenaufrufe einmalig protokollieren.
		static int s_iQuadDrawLog = 0;

		if (s_iQuadDrawLog < 8)
		{
			gEngfuncs.Con_DPrintf("[fx] quad draw age=%.2f alpha=%.0f width=%.1f frame=%d n=%d\n",
				flAge, flAlpha, flWidth, (int)flFrame, g_iEffectQuadCount);
			s_iQuadDrawLog++;
		}

		float vecAngles[3];
		VectorAngles((const float*)&e.vecDir, vecAngles);

		Vector vecForward, vecRight, vecUp;
		AngleVectors(vecAngles, vecForward, vecRight, vecUp);

		// Retail baut das Quad von `origin + dir - right*Breite/2` auf und
		// laeuft +right*Breite; Ergebnis ist ein Quad der Kantenlaenge
		// `Breite` um `origin + dir`.
		const Vector vecCenter = e.vecOrigin + e.vecDir - vecRight * (flWidth * 0.5f);

		const Vector v0 = vecCenter + vecUp * (flWidth * 0.5f);
		const Vector v1 = v0 + vecRight * flWidth;
		const Vector v2 = vecCenter + vecRight * flWidth - vecUp * (flWidth * 0.5f);
		const Vector v3 = vecCenter - vecUp * (flWidth * 0.5f);

		gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
		gEngfuncs.pTriAPI->CullFace(TRI_FRONT);
		gEngfuncs.pTriAPI->Begin(TRI_QUADS);
		gEngfuncs.pTriAPI->Color4f(1.0f, 1.0f, 1.0f, flAlpha * (1.0f / 255.0f));

		gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
		gEngfuncs.pTriAPI->Vertex3fv((float*)&v0);
		gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f);
		gEngfuncs.pTriAPI->Vertex3fv((float*)&v1);
		gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
		gEngfuncs.pTriAPI->Vertex3fv((float*)&v2);
		gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f);
		gEngfuncs.pTriAPI->Vertex3fv((float*)&v3);

		gEngfuncs.pTriAPI->End();
		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	}

	g_iEffectQuadCount = iAlive;
}

void EV_RetailRockShards(const Vector& vecOrigin, float flSpeed, float flLife, int iCount, int iModel, int iFlags) {}
