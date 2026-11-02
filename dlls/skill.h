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
// skill.h - skill level concerns
//=========================================================

#pragma once

struct skilldata_t
{

	int iSkillLevel; // game skill level

	// Monster Health & Damage
	float agruntHealth;
	float agruntDmgPunch;

	float apacheHealth;

	float barneyHealth;

	float bigmommaHealthFactor; // Multiply each node's health by this
	float bigmommaDmgSlash;		// melee attack damage
	float bigmommaDmgBlast;		// mortar attack damage
	float bigmommaRadiusBlast;	// mortar attack radius

	float bullsquidHealth;
	float bullsquidDmgBite;
	float bullsquidDmgWhip;
	float bullsquidDmgSpit;

	float gargantuaHealth;
	float gargantuaDmgSlash;
	float gargantuaDmgFire;
	float gargantuaDmgStomp;

	float hassassinHealth;

	float headcrabHealth;
	float headcrabDmgBite;

	float hgruntHealth;
	float hgruntDmgKick;
	float hgruntShotgunPellets;
	float hgruntGrenadeSpeed;

	float houndeyeHealth;
	float houndeyeDmgBlast;

	float slaveHealth;
	float slaveDmgClaw;
	float slaveDmgClawrake;
	float slaveDmgZap;

	float ichthyosaurHealth;
	float ichthyosaurDmgShake;

	float leechHealth;
	float leechDmgBite;

	float controllerHealth;
	float controllerDmgZap;
	float controllerSpeedBall;
	float controllerDmgBall;

	float nihilanthHealth;
	float nihilanthZap;

	float scientistHealth;

	float snarkHealth;
	float snarkDmgBite;
	float snarkDmgPop;

	float zombieHealth;
	float zombieDmgOneSlash;
	float zombieDmgBothSlash;

	float turretHealth;
	float miniturretHealth;
	float sentryHealth;

	// Nachtrag 2026-09-06 (RE-Nachtrag "Fauna/Cricket", monster_ourano):
	// echte Retail-Skill.cfg (Gunman-ENG-GER/rewolf/Skill.cfg) hat eigene
	// Gunman-Felder ohne SDK-Vorbild - ouranoHealth per Skill-Level
	// (sk_ourano_h1/h2/h3 = 70/70/80) sowie ein globaler
	// Zufallsvarianz-Prozentsatz (sk_percent_random_h1/h2/h3 = 6/12/18),
	// der laut Decompile von COurano::Spawn (FUN_100608d0 @0x100608d0)
	// auf die Basis-Health addiert wird: health = base +
	// RANDOM_FLOAT(0,base) * (percentRandomHealth * 0.01). Siehe
	// findings/entities/ourano_health_and_fauna_re.md.
	float ouranoHealth;
	float ouranoDamage;
	float percentRandomHealth;

	// Nachtrag 2026-09-06 (Cut-Content-Rekonstruktion, monster_penta):
	// keine Retail-Skill.cfg-Werte mehr vorhanden (Cvars dort entfernt) -
	// diese Werte stammen aus der E3-2000-Beta-Skill.cfg
	// (sk_penta_h1-3=300/400/500, sk_penta_bite_d1-3=30/40/50), siehe
	// findings/entities/cutcontent_penta_batterybot.md. monster_battery
	// (CBatteryBot) hat keine bekannten Skill-Werte - siehe dortiger
	// Klassenkopf-Kommentar fuer den verwendeten Platzhalter.
	float pentaHealth;
	float pentaBiteDamage;

	// Retail Gunman: monster_maggot uses sk_maggot_h<level>. The three
	// shipped values are all 10, but the value still belongs in Skill.cfg.
	float maggotHealth;

	// Retail Gunman: monster_critter uses its own health, bite and larva
	// projectile skill families (sk_critter_*), not stock alien values.
	float critterHealth;
	float critterSpitDamage;
	float critterMeleeDamage;

	// Retail Gunman: monster_tube uses sk_tube_h<level> (30/45/60).
	float tubeHealth;

	// Retail Gunman: monster_gator uses sk_gator_h<level> (40/50/60).
	float gatorHealth;
	float gatorBiteDamage;

	// Retail Gunman: monster_raptor/monster_rheptor and monster_scorpion.
	float raptorHealth;
	float raptorBiteDamage;
	float raptorJumpDamage;
	float smallScorpionHealth;
	float smallScorpionClawDamage;
	float largeScorpionHealth;
	float largeScorpionClawDamage;
	float largeScorpionStingDamage;
	float rustbitHealth;
	float rustbitShotDamage;
	float rustbitClawDamage;
	float rustbotHealth;
	float rustbotMeleeDamage;
	float rustbotLongDamage;
	float rustgunnerHealth;
	float rustgunnerRocketDamage;
	float rustgunnerMortarDamage;
	float beakHealth;
	float beakBiteDamage;
	float beakScrapeDamage;
	float hatchetFishHealth;
	float hatchetFishDamage;
	float xenomeHealth;
	float xenomeMeleeDamage;
	float xenomeDartDamage;

	// Retail Gunman: both human Bandit and Chopper Spawn routines load
	// dedicated short-form sk_*_h values rather than fixed health.
	float banditHealth;
	float banditBoltDamage;
	float banditFastDamage;

	// Retail Gauss-Pistole: sk_gaussshot_d (Rapid, 0x10136fc0), sk_gaussfast_d (Praezision, 0x10136fc4), sk_gaussslow_d (Charge, 0x10136fc8)
	float gaussShotDamage;
	float gaussFastDamage;
	float gaussSlowDamage;
	float chopperHealth;
	float demomanHealth;
	float demomanRocketDamage;
	// Retail-Skillwerte, die im SDK fehlten (Zuordnung Name -> Global aus der Retail-Skill-Initialisierung @0x1001e860)
	float humanKickDamage; // sk_human_kick_d (0x10136e88)
	float grenadeSpeed; // sk_gren_speed (0x10136e8c)
	float banditLimpHealth; // sk_bandit_limp_h (0x10136e94)
	float banditMeleeDamage; // sk_bandit_melee_d (0x10136e98)
	float banditMechDamage; // sk_bandit_mech_d (0x10136ea4)
	float smallScorpionStingDamage; // sk_smlscorp_sting_d (0x10136edc)
	float maggotDamage; // sk_maggot_d (0x10136f34)
	float chemgunDamage; // sk_chemgun_d (0x10136fd0)
	float handGrenadeDamage; // sk_hand_grenade_d (0x10136fcc)
	float twoHandPunchDamage; // sk_twohandpunch_d (0x10136fd8)
	float armorSuitValue; // sk_armorsuit (0x10136ff0)
	float demomanLimpHealth;	// sk_demoman_limp_h (Retail 0x10136eac): below this health the walk is a limp
	float demomanShotgunDamage; // sk_demoman_sg_d (Retail 0x10136eb4)
	float mineDamage;
	float dmlRocketDamage;


	// Player Weapons
	float plrDmgCrowbar;
	float gunmanOneHandPunchDamage;
	float gunmanKnifeDamage;
	float plrDmg9MM;
	float plrDmg357;
	float plrDmgMP5;
	float plrDmgM203Grenade;
	float plrDmgBuckshot;
	float gunmanBuckshotDamage;
	float plrDmgCrossbowClient;
	float plrDmgCrossbowMonster;
	float plrDmgRPG;
	float plrDmgGauss;
	float plrDmgEgonNarrow;
	float plrDmgEgonWide;
	float plrDmgHornet;
	float plrDmgHandGrenade;
	float plrDmgSatchel;
	float plrDmgTripmine;

	// weapons shared by monsters
	float monDmg9MM;
	float monDmgMP5;
	float monDmg12MM;
	float monDmgHornet;

	// health/suit charge
	float suitchargerCapacity;
	float batteryCapacity;
	float healthchargerCapacity;
	float healthkitCapacity;
	float scientistHeal;

	// monster damage adj
	float monHead;
	float monChest;
	float monStomach;
	float monLeg;
	float monArm;

	// player damage adj
	float plrHead;
	float plrChest;
	float plrStomach;
	float plrLeg;
	float plrArm;
};

inline DLL_GLOBAL skilldata_t gSkillData;
float GetSkillCvar(const char* pName);

inline DLL_GLOBAL int g_iSkillLevel;

#define SKILL_EASY 1
#define SKILL_MEDIUM 2
#define SKILL_HARD 3
