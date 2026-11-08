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
// skill.cpp - code for skill level concerns
//=========================================================
#include "extdll.h"
#include "util.h"
#include "skill.h"

//=========================================================
// Cvars, die es in der Gunman-Skill-Datei nicht gibt: HALF-LIFE-Stock-Groessen fuer
// Monster und Waffen, die Gunman nicht enthaelt (belegt: 62 distinkte Namen aus
// "GetSkillCVar Got a zero"-Warnungen ueber alle Maps, siehe
// findings/server_weapon_abgleich_2026-11-04.md, Abschnitt 2q). Fuer diese Namen wird die
// Warnung unterdrueckt, damit echte Nullwerte wieder auffallen; die Rueckgabe bleibt 0.
//=========================================================
static bool IsAbsentInGunman(const char* pName)
{
	static const char* kAbsent[] =
	{
		"sk_12mm_bullet", "sk_9mm_bullet", "sk_agrunt_dmg_punch", "sk_agrunt_health",
		"sk_apache_health", "sk_barney_health", "sk_bullsquid_dmg_bite",
		"sk_bullsquid_dmg_spit", "sk_bullsquid_dmg_whip", "sk_bullsquid_health",
		"sk_controller_dmgball", "sk_controller_dmgzap", "sk_controller_health",
		"sk_controller_speedball", "sk_gargantua_dmg_fire", "sk_gargantua_dmg_slash",
		"sk_gargantua_dmg_stomp", "sk_gargantua_health", "sk_hassassin_health",
		"sk_headcrab_dmg_bite", "sk_headcrab_health", "sk_hgrunt_gspeed", "sk_hgrunt_health",
		"sk_hgrunt_kick", "sk_hgrunt_pellets", "sk_hornet_dmg", "sk_houndeye_dmg_blast",
		"sk_houndeye_health", "sk_ichthyosaur_health", "sk_ichthyosaur_shake",
		"sk_islave_dmg_claw", "sk_islave_dmg_clawrake", "sk_islave_dmg_zap",
		"sk_islave_health", "sk_leech_dmg_bite", "sk_leech_health", "sk_miniturret_health",
		"sk_nihilanth_health", "sk_nihilanth_zap", "sk_plr_357_bullet",
		"sk_plr_9mmAR_bullet", "sk_plr_9mmAR_grenade", "sk_plr_9mm_bullet",
		"sk_plr_buckshot", "sk_plr_crowbar", "sk_plr_egon_narrow", "sk_plr_egon_wide",
		"sk_plr_gauss", "sk_plr_hand_grenade", "sk_plr_hornet_dmg", "sk_plr_rpg",
		"sk_plr_satchel", "sk_plr_tripmine", "sk_plr_xbow_bolt_client",
		"sk_plr_xbow_bolt_monster", "sk_snark_dmg_bite", "sk_snark_dmg_pop",
		"sk_snark_health", "sk_turret_health", "sk_zombie_dmg_both_slash",
		"sk_zombie_dmg_one_slash", "sk_zombie_health",
	};

	for (const char* psz : kAbsent)
	{
		if (!strcmp(psz, pName))
			return true;
	}

	return false;
}

//=========================================================
// take the name of a cvar, tack a digit for the skill level
// on, and return the value.of that Cvar
//=========================================================
float GetSkillCvar(const char* pName)
{
	int iCount;
	float flValue;
	char szBuffer[64];

	iCount = sprintf(szBuffer, "%s%d", pName, gSkillData.iSkillLevel);

	flValue = CVAR_GET_FLOAT(szBuffer);

	if (flValue <= 0 && !IsAbsentInGunman(pName))
	{
		ALERT(at_console, "\n\n** GetSkillCVar Got a zero for %s **\n\n", szBuffer);
	}

	return flValue;
}
