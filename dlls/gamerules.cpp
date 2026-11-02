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
// GameRules.cpp
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "skill.h"
#include "game.h"
#include "UserMessages.h"

extern edict_t* EntSelectSpawnPoint(CBaseEntity* pPlayer);

CBasePlayerItem* CGameRules::FindNextBestWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pCurrentWeapon)
{
	if (pCurrentWeapon != nullptr && !pCurrentWeapon->CanHolster())
	{
		// can't put this gun away right now, so can't switch.
		return nullptr;
	}

	const int currentWeight = pCurrentWeapon != nullptr ? pCurrentWeapon->iWeight() : -1;

	CBasePlayerItem* pBest = nullptr; // this will be used in the event that we don't find a weapon in the same category.

	int iBestWeight = -1; // no weapon lower than -1 can be autoswitched to

	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		for (auto pCheck = pPlayer->m_rgpPlayerItems[i]; pCheck; pCheck = pCheck->m_pNext)
		{
			// don't reselect the weapon we're trying to get rid of
			if (pCheck == pCurrentWeapon)
			{
				continue;
			}

			if ((pCheck->iFlags() & ITEM_FLAG_NOAUTOSWITCHTO) != 0)
			{
				continue;
			}

			if (pCheck->iWeight() > -1 && pCheck->iWeight() == currentWeight)
			{
				// this weapon is from the same category.
				if (pCheck->CanDeploy())
				{
					if (pPlayer->SwitchWeapon(pCheck))
					{
						return pCheck;
					}
				}
			}
			else if (pCheck->iWeight() > iBestWeight)
			{
				//ALERT ( at_console, "Considering %s\n", STRING( pCheck->pev->classname ) );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted
				// weapon.
				if (pCheck->CanDeploy())
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}
		}
	}

	// if we make it here, we've checked all the weapons and found no useable
	// weapon in the same catagory as the current weapon.

	// if pBest is nullptr, we didn't find ANYTHING. Shouldn't be possible- should always
	// at least get the crowbar, but ya never know.

	return pBest;
}

bool CGameRules::GetNextBestWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pCurrentWeapon, bool alwaysSearch)
{
	if (auto pBest = FindNextBestWeapon(pPlayer, pCurrentWeapon); pBest != nullptr)
	{
		pPlayer->SwitchWeapon(pBest);
		return true;
	}

	return false;
}

//=========================================================
//=========================================================
bool CGameRules::CanHaveAmmo(CBasePlayer* pPlayer, const char* pszAmmoName, int iMaxCarry)
{
	int iAmmoIndex;

	if (pszAmmoName)
	{
		iAmmoIndex = pPlayer->GetAmmoIndex(pszAmmoName);

		if (iAmmoIndex > -1)
		{
			if (pPlayer->AmmoInventory(iAmmoIndex) < iMaxCarry)
			{
				// player has room for more of this type of ammo
				return true;
			}
		}
	}

	return false;
}

//=========================================================
//=========================================================
edict_t* CGameRules::GetPlayerSpawnSpot(CBasePlayer* pPlayer)
{
	edict_t* pentSpawnSpot = EntSelectSpawnPoint(pPlayer);

	pPlayer->pev->origin = VARS(pentSpawnSpot)->origin + Vector(0, 0, 1);
	pPlayer->pev->v_angle = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = VARS(pentSpawnSpot)->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = 1;

	return pentSpawnSpot;
}

//=========================================================
//=========================================================
bool CGameRules::CanHavePlayerItem(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
	// only living players can have items
	if (pPlayer->pev->deadflag != DEAD_NO)
		return false;

	if (pWeapon->pszAmmo1())
	{
		if (!CanHaveAmmo(pPlayer, pWeapon->pszAmmo1(), pWeapon->iMaxAmmo1()))
		{
			// we can't carry anymore ammo for this gun. We can only
			// have the gun if we aren't already carrying one of this type
			if (pPlayer->HasPlayerItem(pWeapon))
			{
				return false;
			}
		}
	}
	else
	{
		// weapon doesn't use ammo, don't take another if you already have it.
		if (pPlayer->HasPlayerItem(pWeapon))
		{
			return false;
		}
	}

	// note: will fall through to here if GetItemInfo doesn't fill the struct!
	return true;
}

//=========================================================
// load the SkillData struct with the proper values based on the skill level.
//=========================================================
void CGameRules::RefreshSkillData()
{
	int iSkill;

	iSkill = (int)CVAR_GET_FLOAT("skill");
	g_iSkillLevel = iSkill;

	if (iSkill < 1)
	{
		iSkill = 1;
	}
	else if (iSkill > 3)
	{
		iSkill = 3;
	}

	gSkillData.iSkillLevel = iSkill;

	ALERT(at_console, "\nGAME SKILL LEVEL:%d\n", iSkill);

	//Agrunt
	gSkillData.agruntHealth = GetSkillCvar("sk_agrunt_health");
	gSkillData.agruntDmgPunch = GetSkillCvar("sk_agrunt_dmg_punch");

	// Apache
	gSkillData.apacheHealth = GetSkillCvar("sk_apache_health");

	// Barney
	gSkillData.barneyHealth = GetSkillCvar("sk_barney_health");

	// Big Momma
	gSkillData.bigmommaHealthFactor = GetSkillCvar("sk_bigmomma_health_factor");
	gSkillData.bigmommaDmgSlash = GetSkillCvar("sk_bigmomma_dmg_slash");
	gSkillData.bigmommaDmgBlast = GetSkillCvar("sk_bigmomma_dmg_blast");
	gSkillData.bigmommaRadiusBlast = GetSkillCvar("sk_bigmomma_radius_blast");

	// Bullsquid
	gSkillData.bullsquidHealth = GetSkillCvar("sk_bullsquid_health");
	gSkillData.bullsquidDmgBite = GetSkillCvar("sk_bullsquid_dmg_bite");
	gSkillData.bullsquidDmgWhip = GetSkillCvar("sk_bullsquid_dmg_whip");
	gSkillData.bullsquidDmgSpit = GetSkillCvar("sk_bullsquid_dmg_spit");

	// Gargantua
	gSkillData.gargantuaHealth = GetSkillCvar("sk_gargantua_health");
	gSkillData.gargantuaDmgSlash = GetSkillCvar("sk_gargantua_dmg_slash");
	gSkillData.gargantuaDmgFire = GetSkillCvar("sk_gargantua_dmg_fire");
	gSkillData.gargantuaDmgStomp = GetSkillCvar("sk_gargantua_dmg_stomp");

	// Hassassin
	gSkillData.hassassinHealth = GetSkillCvar("sk_hassassin_health");

	// Headcrab
	gSkillData.headcrabHealth = GetSkillCvar("sk_headcrab_health");
	gSkillData.headcrabDmgBite = GetSkillCvar("sk_headcrab_dmg_bite");

	// Hgrunt
	gSkillData.hgruntHealth = GetSkillCvar("sk_hgrunt_health");
	gSkillData.hgruntDmgKick = GetSkillCvar("sk_hgrunt_kick");
	gSkillData.hgruntShotgunPellets = GetSkillCvar("sk_hgrunt_pellets");
	gSkillData.hgruntGrenadeSpeed = GetSkillCvar("sk_hgrunt_gspeed");

	// Houndeye
	gSkillData.houndeyeHealth = GetSkillCvar("sk_houndeye_health");
	gSkillData.houndeyeDmgBlast = GetSkillCvar("sk_houndeye_dmg_blast");

	// ISlave
	gSkillData.slaveHealth = GetSkillCvar("sk_islave_health");
	gSkillData.slaveDmgClaw = GetSkillCvar("sk_islave_dmg_claw");
	gSkillData.slaveDmgClawrake = GetSkillCvar("sk_islave_dmg_clawrake");
	gSkillData.slaveDmgZap = GetSkillCvar("sk_islave_dmg_zap");

	// Icthyosaur
	gSkillData.ichthyosaurHealth = GetSkillCvar("sk_ichthyosaur_health");
	gSkillData.ichthyosaurDmgShake = GetSkillCvar("sk_ichthyosaur_shake");

	// Leech
	gSkillData.leechHealth = GetSkillCvar("sk_leech_health");

	gSkillData.leechDmgBite = GetSkillCvar("sk_leech_dmg_bite");

	// Controller
	gSkillData.controllerHealth = GetSkillCvar("sk_controller_health");
	gSkillData.controllerDmgZap = GetSkillCvar("sk_controller_dmgzap");
	gSkillData.controllerSpeedBall = GetSkillCvar("sk_controller_speedball");
	gSkillData.controllerDmgBall = GetSkillCvar("sk_controller_dmgball");

	// Nihilanth
	gSkillData.nihilanthHealth = GetSkillCvar("sk_nihilanth_health");
	gSkillData.nihilanthZap = GetSkillCvar("sk_nihilanth_zap");

	// Scientist (Nachtrag 2026-09-06, CVAR_REGISTER-Audit): das Stock-SDK-
	// Cvar-Muster "sk_scientist_health<level>" existiert NICHT im retail
	// gunman.dll (String-Suche: 0 Treffer). Der real registrierte Cvar
	// heisst "sk_scientist_h<level>" (bestaetigt per String-Dump gegen
	// gunman.dll UND per Eintrag in der echten Skill.cfg, 45/55/65) -
	// dasselbe Kurzform-Namensmuster wie sk_bandit_h/sk_ourano_h/
	// sk_raptor_h usw. monster_scientist (CScientist, stock-SDK-Klasse)
	// ist mit 24 Platzierungen die meistgenutzte Klasse dieser Datei
	// (per direktem grep -a gegen alle retail-.bsp) - hatte also bislang
	// IMMER Health 0. Siehe game.cpp fuer die neue CVAR_REGISTER-Zeile.
	gSkillData.scientistHealth = GetSkillCvar("sk_scientist_h");

	// Snark
	gSkillData.snarkHealth = GetSkillCvar("sk_snark_health");
	gSkillData.snarkDmgBite = GetSkillCvar("sk_snark_dmg_bite");
	gSkillData.snarkDmgPop = GetSkillCvar("sk_snark_dmg_pop");

	// Zombie
	gSkillData.zombieHealth = GetSkillCvar("sk_zombie_health");
	gSkillData.zombieDmgOneSlash = GetSkillCvar("sk_zombie_dmg_one_slash");
	gSkillData.zombieDmgBothSlash = GetSkillCvar("sk_zombie_dmg_both_slash");

	//Turret
	gSkillData.turretHealth = GetSkillCvar("sk_turret_health");

	// MiniTurret
	gSkillData.miniturretHealth = GetSkillCvar("sk_miniturret_health");

	// Sentry Turret
	// Nachtrag 2026-10-30 (RE-bestaetigt, frisch dekompiliert aus der Retail-
	// gunman.dll): die Retail-RefreshSkillData (0x1001e860) liest hier keinen
	// Cvar, sondern setzt das Skill-Feld konstant auf 1000.0 (0x447a0000,
	// Schreibzugriff @0x1001ebe4, gelesen von CSentry::Spawn @0x1005d7c0 nach
	// pev->health). Ein "sk_sentry_health"-String existiert in Retail nicht;
	// der fruehere Cvar-Lookup lieferte darum immer 0 (Sentry mit Health 0).
	// Gilt fuer monster_sentry; monster_sentry_mini setzt 50 selbst im Spawn.
	gSkillData.sentryHealth = 1000.0f;

	// Ourano (Nachtrag 2026-09-06, RE-bestaetigt gegen die echte Retail-
	// Skill.cfg - eigene Gunman-Cvar-Namen "sk_ourano_h<level>", nicht
	// das SDK-uebliche "sk_ourano_health<level>"-Muster).
	gSkillData.ouranoHealth = GetSkillCvar("sk_ourano_h");
	gSkillData.ouranoDamage = GetSkillCvar("sk_ourano_d");
	gSkillData.percentRandomHealth = GetSkillCvar("sk_percent_random_h");

	// Penta (Nachtrag 2026-09-06, Cut-Content-Rekonstruktion - Cvar-Werte
	// aus der E3-2000-Beta-Skill.cfg uebernommen, in Retail entfernt).
	gSkillData.pentaHealth = GetSkillCvar("sk_penta_h");
	gSkillData.pentaBiteDamage = GetSkillCvar("sk_penta_bite_d");

	// Maggot: retail Skill.cfg uses sk_maggot_h<level> (10/10/10).
	gSkillData.maggotHealth = GetSkillCvar("sk_maggot_h");

	// Retail Bandit Spawn and events 1/2 and 5/6 load DAT_10136e90,
	// DAT_10136e9c and DAT_10136ea0, initialized from sk_bandit_h,
	// sk_bandit_bolt_d and sk_bandit_fast_d respectively.
	gSkillData.banditHealth = GetSkillCvar("sk_bandit_h");
	gSkillData.banditBoltDamage = GetSkillCvar("sk_bandit_bolt_d");
	gSkillData.banditFastDamage = GetSkillCvar("sk_bandit_fast_d");
	gSkillData.gaussShotDamage = GetSkillCvar("sk_gaussshot_d");
	gSkillData.gaussFastDamage = GetSkillCvar("sk_gaussfast_d");
	gSkillData.gaussSlowDamage = GetSkillCvar("sk_gaussslow_d");
	gSkillData.chopperHealth = GetSkillCvar("sk_chopper_h");
	gSkillData.demomanHealth = GetSkillCvar("sk_demoman_h");
	gSkillData.demomanRocketDamage = GetSkillCvar("sk_demoman_rocket_d");
	gSkillData.humanKickDamage = GetSkillCvar("sk_human_kick_d");
	gSkillData.grenadeSpeed = GetSkillCvar("sk_gren_speed");
	gSkillData.banditLimpHealth = GetSkillCvar("sk_bandit_limp_h");
	gSkillData.banditMeleeDamage = GetSkillCvar("sk_bandit_melee_d");
	gSkillData.banditMechDamage = GetSkillCvar("sk_bandit_mech_d");
	gSkillData.smallScorpionStingDamage = GetSkillCvar("sk_smlscorp_sting_d");
	gSkillData.maggotDamage = GetSkillCvar("sk_maggot_d");
	gSkillData.chemgunDamage = GetSkillCvar("sk_chemgun_d");
	gSkillData.handGrenadeDamage = GetSkillCvar("sk_hand_grenade_d");
	gSkillData.twoHandPunchDamage = GetSkillCvar("sk_twohandpunch_d");
	gSkillData.armorSuitValue = GetSkillCvar("sk_armorsuit");
	gSkillData.demomanLimpHealth = GetSkillCvar("sk_demoman_limp_h");
	gSkillData.demomanShotgunDamage = GetSkillCvar("sk_demoman_sg_d");
	gSkillData.mineDamage = GetSkillCvar("sk_mine_d");
	gSkillData.dmlRocketDamage = GetSkillCvar("sk_dml_rocket_d");

	// Critter: retail Skill.cfg uses dedicated health, larva-spit and bite
	// families. Do not silently fall back to stock alien damage values.
	gSkillData.critterHealth = GetSkillCvar("sk_critter_h");
	gSkillData.critterSpitDamage = GetSkillCvar("sk_critter_spit_d");
	gSkillData.critterMeleeDamage = GetSkillCvar("sk_critter_melee1_d");

	// Tube: retail Skill.cfg uses sk_tube_h<level> (30/45/60).
	gSkillData.tubeHealth = GetSkillCvar("sk_tube_h");

	// Gator: retail Skill.cfg uses sk_gator_h<level> (40/50/60).
	gSkillData.gatorHealth = GetSkillCvar("sk_gator_h");
	gSkillData.gatorBiteDamage = GetSkillCvar("sk_gator_bite_d");

	// Maya fauna: direct Retail Spawn/animation-handler references.
	gSkillData.raptorHealth = GetSkillCvar("sk_raptor_h");
	gSkillData.raptorBiteDamage = GetSkillCvar("sk_raptor_bite_d");
	gSkillData.raptorJumpDamage = GetSkillCvar("sk_raptor_jump_d");
	gSkillData.smallScorpionHealth = GetSkillCvar("sk_smlscorp_h");
	gSkillData.smallScorpionClawDamage = GetSkillCvar("sk_smlscorp_claw_d");
	gSkillData.largeScorpionHealth = GetSkillCvar("sk_lrgscorp_h");
	gSkillData.largeScorpionClawDamage = GetSkillCvar("sk_lrgscorp_claw_d");
	gSkillData.largeScorpionStingDamage = GetSkillCvar("sk_lrgscorp_sting_d");
	gSkillData.rustbitHealth = GetSkillCvar("sk_rustbit_h");
	gSkillData.rustbitShotDamage = GetSkillCvar("sk_rustbit_shot_d");
	gSkillData.rustbitClawDamage = GetSkillCvar("sk_rustbit_claw_d");
	gSkillData.rustbotHealth = GetSkillCvar("sk_rustbot_h");
	gSkillData.rustbotMeleeDamage = GetSkillCvar("sk_rustbot_melee_d");
	gSkillData.rustbotLongDamage = GetSkillCvar("sk_rustbot_long_d");
	gSkillData.rustgunnerHealth = GetSkillCvar("sk_rustgunner_h");
	gSkillData.rustgunnerRocketDamage = GetSkillCvar("sk_rustgunner_r_d");
	gSkillData.rustgunnerMortarDamage = GetSkillCvar("sk_rustgunner_mr_d");
	gSkillData.beakHealth = GetSkillCvar("sk_beak_h");
	gSkillData.beakBiteDamage = GetSkillCvar("sk_beak_bite_d");
	gSkillData.beakScrapeDamage = GetSkillCvar("sk_beak_scrape_d");
	gSkillData.hatchetFishHealth = GetSkillCvar("sk_hatchetfish_h");
	gSkillData.hatchetFishDamage = GetSkillCvar("sk_hatchetfish_d");
	gSkillData.xenomeHealth = GetSkillCvar("sk_xenome_h");
	gSkillData.xenomeMeleeDamage = GetSkillCvar("sk_xenomemelee_d");
	gSkillData.xenomeDartDamage = GetSkillCvar("sk_xenomedart_d");

	// PLAYER WEAPONS

	// Crowbar whack
	gSkillData.plrDmgCrowbar = GetSkillCvar("sk_plr_crowbar");

	// Glock Round
	gSkillData.plrDmg9MM = GetSkillCvar("sk_plr_9mm_bullet");

	// 357 Round
	gSkillData.plrDmg357 = GetSkillCvar("sk_plr_357_bullet");

	// MP5 Round
	gSkillData.plrDmgMP5 = GetSkillCvar("sk_plr_9mmAR_bullet");

	// M203 grenade
	gSkillData.plrDmgM203Grenade = GetSkillCvar("sk_plr_9mmAR_grenade");

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = GetSkillCvar("sk_plr_buckshot");
	gSkillData.gunmanBuckshotDamage = GetSkillCvar("sk_buckshot");

	// Crossbow
	gSkillData.plrDmgCrossbowClient = GetSkillCvar("sk_plr_xbow_bolt_client");
	gSkillData.plrDmgCrossbowMonster = GetSkillCvar("sk_plr_xbow_bolt_monster");

	// RPG
	gSkillData.plrDmgRPG = GetSkillCvar("sk_plr_rpg");

	// Gauss gun
	gSkillData.plrDmgGauss = GetSkillCvar("sk_plr_gauss");

	// Egon Gun
	gSkillData.plrDmgEgonNarrow = GetSkillCvar("sk_plr_egon_narrow");
	gSkillData.plrDmgEgonWide = GetSkillCvar("sk_plr_egon_wide");

	// Hand Grendade
	gSkillData.plrDmgHandGrenade = GetSkillCvar("sk_plr_hand_grenade");
	gSkillData.gunmanOneHandPunchDamage = GetSkillCvar("sk_onehandpunch_d");
	gSkillData.gunmanKnifeDamage = GetSkillCvar("sk_knife1_d");

	// Satchel Charge
	gSkillData.plrDmgSatchel = GetSkillCvar("sk_plr_satchel");

	// Tripmine
	gSkillData.plrDmgTripmine = GetSkillCvar("sk_plr_tripmine");

	// MONSTER WEAPONS
	gSkillData.monDmg12MM = GetSkillCvar("sk_12mm_bullet");
	gSkillData.monDmgMP5 = GetSkillCvar("sk_9mmAR_bullet");
	gSkillData.monDmg9MM = GetSkillCvar("sk_9mm_bullet");

	// MONSTER HORNET
	gSkillData.monDmgHornet = GetSkillCvar("sk_hornet_dmg");

	// PLAYER HORNET
	gSkillData.plrDmgHornet = GetSkillCvar("sk_plr_hornet_dmg");


	// HEALTH/CHARGE
	gSkillData.suitchargerCapacity = GetSkillCvar("sk_suitcharger");
	gSkillData.batteryCapacity = GetSkillCvar("sk_battery");
	gSkillData.healthchargerCapacity = GetSkillCvar("sk_healthcharger");
	gSkillData.healthkitCapacity = GetSkillCvar("sk_healthkit");
	gSkillData.scientistHeal = GetSkillCvar("sk_scientist_heal");

	// monster damage adj
	gSkillData.monHead = GetSkillCvar("sk_monster_head");
	gSkillData.monChest = GetSkillCvar("sk_monster_chest");
	gSkillData.monStomach = GetSkillCvar("sk_monster_stomach");
	gSkillData.monLeg = GetSkillCvar("sk_monster_leg");
	gSkillData.monArm = GetSkillCvar("sk_monster_arm");

	// player damage adj
	gSkillData.plrHead = GetSkillCvar("sk_player_head");
	gSkillData.plrChest = GetSkillCvar("sk_player_chest");
	gSkillData.plrStomach = GetSkillCvar("sk_player_stomach");
	gSkillData.plrLeg = GetSkillCvar("sk_player_leg");
	gSkillData.plrArm = GetSkillCvar("sk_player_arm");
}

//=========================================================
// instantiate the proper game rules object
//=========================================================

CGameRules* InstallGameRules()
{
	SERVER_COMMAND("exec game.cfg\n");
	SERVER_EXECUTE();

	if (1 == sv_busters.value)
	{
		g_teamplay = false;
		return new CMultiplayBusters;
	}
	else if (0 == gpGlobals->deathmatch)
	{
		// generic half-life
		g_teamplay = false;
		return new CHalfLifeRules;
	}
	else
	{
		if (teamplay.value > 0)
		{
			// teamplay

			g_teamplay = true;
			return new CHalfLifeTeamplay;
		}
		if ((int)gpGlobals->deathmatch == 1)
		{
			// vanilla deathmatch
			g_teamplay = false;
			return new CHalfLifeMultiplay;
		}
		else
		{
			// vanilla deathmatch??
			g_teamplay = false;
			return new CHalfLifeMultiplay;
		}
	}
}
