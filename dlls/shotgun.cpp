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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "UserMessages.h"

// special deathmatch shotgun spreads
#define VECTOR_CONE_DM_SHOTGUN Vector(0.08716, 0.04362, 0.00)		// 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN Vector(0.17365, 0.04362, 0.00) // 20 degrees by 5 degrees

LINK_ENTITY_TO_CLASS(weapon_shotgun, CShotgun);

void CShotgun::Spawn()
{
	Precache();
	m_iId = WEAPON_SHOTGUN_GUNMAN;
	SET_MODEL(ENT(pev), "models/w_shotgun.mdl");

	m_iDefaultAmmo = SHOTGUN_DEFAULT_GIVE;
	m_iClip = WEAPON_NOCLIP;
	m_vecSpread = Vector(0.08716f, 0.08716f, 0.08716f);
	m_iSlidePos = Vector(2, 0, 2);
	m_iNumShells = 2;
	m_flNextCustomize = 0;

	FallInit(); // get ready to fall
}


void CShotgun::Precache()
{
	// Retail-Beleg: gunman.dll FUN_10096100 (CShotgun::Precache) precacht genau
	// models/v_shotgun.mdl, models/w_shotgun.mdl, models/p_shotgun.mdl,
	// models/shotgunshell.mdl (-> 0xa8 = m_iShell), die Sounds items/9mmclip1.wav,
	// weapons/sbarrel1.wav, weapons/shotgun_cock_heavy.wav, weapons/357_cock1.wav
	// und das Event events/gw_shotgun.sc (-> 0xcc = m_usFire).
	// Die frueheren Stock-HL-Sounds (dbarrel1, reload1, reload3, scock1) existieren in
	// Gunman nicht und werden deshalb nicht mehr precacht.
	PrecacheModel("models/v_shotgun.mdl");
	PrecacheModel("models/w_shotgun.mdl");
	PrecacheModel("models/p_shotgun.mdl");

	m_iShell = PrecacheModel("models/shotgunshell.mdl"); // shotgun shell

	PrecacheSound("items/9mmclip1.wav");

	PrecacheSound("weapons/sbarrel1.wav");			// shotgun fire
	PrecacheSound("weapons/shotgun_cock_heavy.wav");	// shotgun pump
	PrecacheSound("weapons/357_cock1.wav");			// gun empty sound

	m_usFire = PrecacheEvent(1, "events/gw_shotgun.sc");
}

bool CShotgun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP; // Retail GetItemInfo sets ItemInfo +0x1c to -1.
	p->iSlot = 2;
	p->iPosition = 0; // Retail GetItemInfo sets ItemInfo +0x4 to 0.
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SHOTGUN_GUNMAN;
	p->iWeight = SHOTGUN_WEIGHT;

	return true;
}



bool CShotgun::Deploy()
{
	// Retail CShotgun::Deploy passes animation index 0 to the common deploy helper.
	// In v_shotgun.mdl sequence 0 is "draw"; SHOTGUN_DRAW is the stock-HL index 6.
	return DefaultDeploy("models/v_shotgun.mdl", "models/p_shotgun.mdl", 0, "shotgun");
}

void CShotgun::ApplyCustomSetting(char option, int value)
{
	if (option == '1')
	{
		m_iSlidePos.x = (float)value;
		switch (value)
		{
		case 1: m_vecSpread = Vector(0.05234f, 0.05234f, 0.05234f); break;
		case 2: m_vecSpread = Vector(0.08716f, 0.08716f, 0.08716f); break;
		case 3: m_vecSpread = Vector(0.13053f, 0.13053f, 0.13053f); break;
		case 4: m_vecSpread = Vector(0.17365f, 0.17365f, 0.17365f); break;
		default: m_vecSpread = Vector(0.02618f, 0.02618f, 0.02618f); break;
		}
	}
	else if (option == '2')
	{
		m_iNumShells = value;
		m_iSlidePos.z = (float)value;
		switch (value)
		{
		case 1: m_iSlidePos.y = 4; break;
		case 2: m_iSlidePos.y = 8; break;
		case 3: m_iSlidePos.y = 11; break;
		case 4: m_iSlidePos.y = 13; break;
		}
	}

	const float flTime = UTIL_WeaponTimeBase();
	if (m_flNextCustomize < flTime)
	{
		SendWeaponAnim(7);
		m_flNextCustomize = flTime;
		m_flTimeWeaponIdle = flTime + RANDOM_FLOAT(1.0f, 10.0f);
	}
}

void CShotgun::PrimaryAttack()
{
	// Retail uses a player ammo-pool count and m_iNumShells, not a
	// magazine clip. Its three spread values are refreshed per shot.
	switch ((int)m_iSlidePos.x)
	{
	case 1:
		m_vecSpread = Vector(0.05234f, 0.05234f, 0.05234f);
		break;
	case 2:
		m_vecSpread = Vector(0.10906f, 0.10906f, 0.10906f);
		break;
	case 3:
	default:
		m_vecSpread = Vector(0.17365f, 0.17365f, 0.17365f);
		break;
	}

	const int iAmmoAvailable = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];
	if (iAmmoAvailable <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase();
		return;
	}

	int iShellsFired = m_iNumShells;
	int iPellets = (int)m_iSlidePos.y;
	if (iAmmoAvailable < m_iNumShells)
	{
		iShellsFired = iAmmoAvailable;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;
		switch (iShellsFired)
		{
		case 1: iPellets = 4; break;
		case 2: iPellets = 7; break;
		case 3: iPellets = 9; break;
		case 4: iPellets = 11; break;
		default: iPellets = 0; break;
		}
	}
	else
	{
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= m_iNumShells;
		switch (m_iNumShells)
		{
		case 1: iPellets = 4; break;
		case 2: iPellets = 7; break;
		case 3: iPellets = 9; break;
		case 4: iPellets = 11; break;
		default: iPellets = 0; break;
		}

		if (g_pGameRules->IsMultiplayer())
		{
			if (m_iNumShells == 2) iPellets = 8;
			else if (m_iNumShells == 3) iPellets = 12;
			else if (m_iNumShells == 4) iPellets = 16;
		}
	}
	m_iSlidePos.y = (float)iPellets;

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	int flags;
#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif


	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	// Retail (0x10096933-0x10096974): FireBullets bekommt den Schaden explizit, nicht
	// ueber die Bullet-Typ-Tabelle. Basis 8.0 (0x41000000), im Mehrspieler 9.8
	// (0x411ccccd); skill-skaliert ueber 0x100d9378 - die Skill-Datei fuehrt
	// sk_buckshot1..3 = 8, daher 8 in Einzelspieler.
	const float flRetailDamage = g_pGameRules->IsMultiplayer() ? 9.8f : gSkillData.gunmanBuckshotDamage;
	const int iRetailDamage = (int)flRetailDamage;

	m_pPlayer->FireBulletsPlayer(iPellets, vecSrc, vecAiming, m_vecSpread,
		2048, BULLET_PLAYER_BUCKSHOT, 0, iRetailDamage, m_pPlayer->pev,
		m_pPlayer->random_seed);

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usFire, 0.0,
		g_vecZero, g_vecZero, 0, 0, (int)m_iSlidePos.x, m_iNumShells, 0, 0);

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	// Schuss-Sequenz des Modells ist 3 ("shoot"), nicht der SDK-Enum-Wert
	// (findings/weapons/anim_index_abgleich_2026-11-01.md: 0 draw, 1 idle,
	// 2 idleinspect, 3 shoot, 4 shoot2, 5 shoot3, 6 shoot4, 7 customize).
	// Bisher sendete PrimaryAttack gar keine Animation - daher "keine
	// Schuss-Animation".
	SendWeaponAnim(3);

	// Stock-HL-Zeiten (shotgun.cpp): Nachladen der Kammer 0.5 s, Feuertakt
	// 0.75 s. Mit UTIL_WeaponTimeBase() ohne Zuschlag feuerte die Flinte in
	// jedem Frame - daher der "Maschinengewehr"-Effekt.
	m_flPumpTime = UTIL_WeaponTimeBase() + 0.5f;

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.75f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.75f;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5f;
}


void CShotgun::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
	{
		if (RANDOM_FLOAT(0, 1) > 0)
		{
			SendWeaponAnim(2);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_LONG(7, 13);
		}
		else
		{
			SendWeaponAnim(1);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_LONG(2, 5);
		}
	}
}

void CShotgun::ItemPostFrame()
{
	if (0 != m_flPumpTime && m_flPumpTime < gpGlobals->time)
	{
		// Retail (CShotgun::Precache, FUN_10096100) precacht
		// weapons/shotgun_cock_heavy.wav als Pump-Sound; der frueher hier
		// benutzte Stock-HL-Sound weapons/scock1.wav existiert in Gunman nicht
		// (Smoke-Log: "Could not load sound sound/weapons/scock1.wav").
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/shotgun_cock_heavy.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0, 0x1f));
		m_flPumpTime = 0;
	}

	CBasePlayerWeapon::ItemPostFrame();
}


class CShotgunAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/shotgunammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PrecacheModel("models/shotgunammo.mdl");
		PrecacheSound("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(AMMO_BUCKSHOTBOX_GIVE, "buckshot", BUCKSHOT_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_buckshot, CShotgunAmmo);

#ifndef CLIENT_DLL
// Retail: beim Aufnehmen die Anpassungs-Items verteilen (customize_items.cpp)
void CShotgun::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	pPlayer->GiveNamedItem("cust_1ShotgunSpread");
	pPlayer->GiveNamedItem("cust_2ShotgunShells");
}
#endif
