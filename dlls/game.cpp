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
#include "eiface.h"
#include "util.h"
#include "client.h"
#include "game.h"
#include "filesystem_utils.h"

cvar_t displaysoundlist = {"displaysoundlist", "0"};

// multiplayer server rules
cvar_t fragsleft = {"mp_fragsleft", "0", FCVAR_SERVER | FCVAR_UNLOGGED}; // Don't spam console/log files/users with this changing
cvar_t timeleft = {"mp_timeleft", "0", FCVAR_SERVER | FCVAR_UNLOGGED};	 // "      "

// multiplayer server rules
cvar_t teamplay = {"mp_teamplay", "0", FCVAR_SERVER};
cvar_t fraglimit = {"mp_fraglimit", "0", FCVAR_SERVER};
cvar_t timelimit = {"mp_timelimit", "0", FCVAR_SERVER};
cvar_t friendlyfire = {"mp_friendlyfire", "0", FCVAR_SERVER};
cvar_t falldamage = {"mp_falldamage", "0", FCVAR_SERVER};
cvar_t weaponstay = {"mp_weaponstay", "0", FCVAR_SERVER};
cvar_t forcerespawn = {"mp_forcerespawn", "1", FCVAR_SERVER};
cvar_t flashlight = {"mp_flashlight", "0", FCVAR_SERVER};
cvar_t aimcrosshair = {"mp_autocrosshair", "1", FCVAR_SERVER};
cvar_t decalfrequency = {"decalfrequency", "30", FCVAR_SERVER};
cvar_t teamlist = {"mp_teamlist", "hgrunt;scientist", FCVAR_SERVER};
cvar_t teamoverride = {"mp_teamoverride", "1"};
cvar_t defaultteam = {"mp_defaultteam", "0"};
cvar_t allowmonsters = {"mp_allowmonsters", "0", FCVAR_SERVER};

cvar_t allow_spectators = {"allow_spectators", "0.0", FCVAR_SERVER}; // 0 prevents players from being spectators

cvar_t mp_chattime = {"mp_chattime", "10", FCVAR_SERVER};

cvar_t sv_allowbunnyhopping = {"sv_allowbunnyhopping", "0", FCVAR_SERVER};

//CVARS FOR SKILL LEVEL SETTINGS
// Agrunt
cvar_t sk_agrunt_health1 = {"sk_agrunt_health1", "0"};
cvar_t sk_agrunt_health2 = {"sk_agrunt_health2", "0"};
cvar_t sk_agrunt_health3 = {"sk_agrunt_health3", "0"};

cvar_t sk_agrunt_dmg_punch1 = {"sk_agrunt_dmg_punch1", "0"};
cvar_t sk_agrunt_dmg_punch2 = {"sk_agrunt_dmg_punch2", "0"};
cvar_t sk_agrunt_dmg_punch3 = {"sk_agrunt_dmg_punch3", "0"};

// Apache
cvar_t sk_apache_health1 = {"sk_apache_health1", "0"};
cvar_t sk_apache_health2 = {"sk_apache_health2", "0"};
cvar_t sk_apache_health3 = {"sk_apache_health3", "0"};

// Barney
cvar_t sk_barney_health1 = {"sk_barney_health1", "0"};
cvar_t sk_barney_health2 = {"sk_barney_health2", "0"};
cvar_t sk_barney_health3 = {"sk_barney_health3", "0"};

// Bullsquid
cvar_t sk_bullsquid_health1 = {"sk_bullsquid_health1", "0"};
cvar_t sk_bullsquid_health2 = {"sk_bullsquid_health2", "0"};
cvar_t sk_bullsquid_health3 = {"sk_bullsquid_health3", "0"};

cvar_t sk_bullsquid_dmg_bite1 = {"sk_bullsquid_dmg_bite1", "0"};
cvar_t sk_bullsquid_dmg_bite2 = {"sk_bullsquid_dmg_bite2", "0"};
cvar_t sk_bullsquid_dmg_bite3 = {"sk_bullsquid_dmg_bite3", "0"};

cvar_t sk_bullsquid_dmg_whip1 = {"sk_bullsquid_dmg_whip1", "0"};
cvar_t sk_bullsquid_dmg_whip2 = {"sk_bullsquid_dmg_whip2", "0"};
cvar_t sk_bullsquid_dmg_whip3 = {"sk_bullsquid_dmg_whip3", "0"};

cvar_t sk_bullsquid_dmg_spit1 = {"sk_bullsquid_dmg_spit1", "0"};
cvar_t sk_bullsquid_dmg_spit2 = {"sk_bullsquid_dmg_spit2", "0"};
cvar_t sk_bullsquid_dmg_spit3 = {"sk_bullsquid_dmg_spit3", "0"};


// Big Momma
cvar_t sk_bigmomma_health_factor1 = {"sk_bigmomma_health_factor1", "1.0"};
cvar_t sk_bigmomma_health_factor2 = {"sk_bigmomma_health_factor2", "1.0"};
cvar_t sk_bigmomma_health_factor3 = {"sk_bigmomma_health_factor3", "1.0"};

cvar_t sk_bigmomma_dmg_slash1 = {"sk_bigmomma_dmg_slash1", "50"};
cvar_t sk_bigmomma_dmg_slash2 = {"sk_bigmomma_dmg_slash2", "50"};
cvar_t sk_bigmomma_dmg_slash3 = {"sk_bigmomma_dmg_slash3", "50"};

cvar_t sk_bigmomma_dmg_blast1 = {"sk_bigmomma_dmg_blast1", "100"};
cvar_t sk_bigmomma_dmg_blast2 = {"sk_bigmomma_dmg_blast2", "100"};
cvar_t sk_bigmomma_dmg_blast3 = {"sk_bigmomma_dmg_blast3", "100"};

cvar_t sk_bigmomma_radius_blast1 = {"sk_bigmomma_radius_blast1", "250"};
cvar_t sk_bigmomma_radius_blast2 = {"sk_bigmomma_radius_blast2", "250"};
cvar_t sk_bigmomma_radius_blast3 = {"sk_bigmomma_radius_blast3", "250"};

// Gargantua
cvar_t sk_gargantua_health1 = {"sk_gargantua_health1", "0"};
cvar_t sk_gargantua_health2 = {"sk_gargantua_health2", "0"};
cvar_t sk_gargantua_health3 = {"sk_gargantua_health3", "0"};

cvar_t sk_gargantua_dmg_slash1 = {"sk_gargantua_dmg_slash1", "0"};
cvar_t sk_gargantua_dmg_slash2 = {"sk_gargantua_dmg_slash2", "0"};
cvar_t sk_gargantua_dmg_slash3 = {"sk_gargantua_dmg_slash3", "0"};

cvar_t sk_gargantua_dmg_fire1 = {"sk_gargantua_dmg_fire1", "0"};
cvar_t sk_gargantua_dmg_fire2 = {"sk_gargantua_dmg_fire2", "0"};
cvar_t sk_gargantua_dmg_fire3 = {"sk_gargantua_dmg_fire3", "0"};

cvar_t sk_gargantua_dmg_stomp1 = {"sk_gargantua_dmg_stomp1", "0"};
cvar_t sk_gargantua_dmg_stomp2 = {"sk_gargantua_dmg_stomp2", "0"};
cvar_t sk_gargantua_dmg_stomp3 = {"sk_gargantua_dmg_stomp3", "0"};


// Hassassin
cvar_t sk_hassassin_health1 = {"sk_hassassin_health1", "0"};
cvar_t sk_hassassin_health2 = {"sk_hassassin_health2", "0"};
cvar_t sk_hassassin_health3 = {"sk_hassassin_health3", "0"};


// Headcrab
cvar_t sk_headcrab_health1 = {"sk_headcrab_health1", "0"};
cvar_t sk_headcrab_health2 = {"sk_headcrab_health2", "0"};
cvar_t sk_headcrab_health3 = {"sk_headcrab_health3", "0"};

cvar_t sk_headcrab_dmg_bite1 = {"sk_headcrab_dmg_bite1", "0"};
cvar_t sk_headcrab_dmg_bite2 = {"sk_headcrab_dmg_bite2", "0"};
cvar_t sk_headcrab_dmg_bite3 = {"sk_headcrab_dmg_bite3", "0"};


// Hgrunt
cvar_t sk_hgrunt_health1 = {"sk_hgrunt_health1", "0"};
cvar_t sk_hgrunt_health2 = {"sk_hgrunt_health2", "0"};
cvar_t sk_hgrunt_health3 = {"sk_hgrunt_health3", "0"};

cvar_t sk_hgrunt_kick1 = {"sk_hgrunt_kick1", "0"};
cvar_t sk_hgrunt_kick2 = {"sk_hgrunt_kick2", "0"};
cvar_t sk_hgrunt_kick3 = {"sk_hgrunt_kick3", "0"};

cvar_t sk_hgrunt_pellets1 = {"sk_hgrunt_pellets1", "0"};
cvar_t sk_hgrunt_pellets2 = {"sk_hgrunt_pellets2", "0"};
cvar_t sk_hgrunt_pellets3 = {"sk_hgrunt_pellets3", "0"};

cvar_t sk_hgrunt_gspeed1 = {"sk_hgrunt_gspeed1", "0"};
cvar_t sk_hgrunt_gspeed2 = {"sk_hgrunt_gspeed2", "0"};
cvar_t sk_hgrunt_gspeed3 = {"sk_hgrunt_gspeed3", "0"};

// Houndeye
cvar_t sk_houndeye_health1 = {"sk_houndeye_health1", "0"};
cvar_t sk_houndeye_health2 = {"sk_houndeye_health2", "0"};
cvar_t sk_houndeye_health3 = {"sk_houndeye_health3", "0"};

cvar_t sk_houndeye_dmg_blast1 = {"sk_houndeye_dmg_blast1", "0"};
cvar_t sk_houndeye_dmg_blast2 = {"sk_houndeye_dmg_blast2", "0"};
cvar_t sk_houndeye_dmg_blast3 = {"sk_houndeye_dmg_blast3", "0"};


// ISlave
cvar_t sk_islave_health1 = {"sk_islave_health1", "0"};
cvar_t sk_islave_health2 = {"sk_islave_health2", "0"};
cvar_t sk_islave_health3 = {"sk_islave_health3", "0"};

cvar_t sk_islave_dmg_claw1 = {"sk_islave_dmg_claw1", "0"};
cvar_t sk_islave_dmg_claw2 = {"sk_islave_dmg_claw2", "0"};
cvar_t sk_islave_dmg_claw3 = {"sk_islave_dmg_claw3", "0"};

cvar_t sk_islave_dmg_clawrake1 = {"sk_islave_dmg_clawrake1", "0"};
cvar_t sk_islave_dmg_clawrake2 = {"sk_islave_dmg_clawrake2", "0"};
cvar_t sk_islave_dmg_clawrake3 = {"sk_islave_dmg_clawrake3", "0"};

cvar_t sk_islave_dmg_zap1 = {"sk_islave_dmg_zap1", "0"};
cvar_t sk_islave_dmg_zap2 = {"sk_islave_dmg_zap2", "0"};
cvar_t sk_islave_dmg_zap3 = {"sk_islave_dmg_zap3", "0"};


// Icthyosaur
cvar_t sk_ichthyosaur_health1 = {"sk_ichthyosaur_health1", "0"};
cvar_t sk_ichthyosaur_health2 = {"sk_ichthyosaur_health2", "0"};
cvar_t sk_ichthyosaur_health3 = {"sk_ichthyosaur_health3", "0"};

cvar_t sk_ichthyosaur_shake1 = {"sk_ichthyosaur_shake1", "0"};
cvar_t sk_ichthyosaur_shake2 = {"sk_ichthyosaur_shake2", "0"};
cvar_t sk_ichthyosaur_shake3 = {"sk_ichthyosaur_shake3", "0"};


// Leech
cvar_t sk_leech_health1 = {"sk_leech_health1", "0"};
cvar_t sk_leech_health2 = {"sk_leech_health2", "0"};
cvar_t sk_leech_health3 = {"sk_leech_health3", "0"};

cvar_t sk_leech_dmg_bite1 = {"sk_leech_dmg_bite1", "0"};
cvar_t sk_leech_dmg_bite2 = {"sk_leech_dmg_bite2", "0"};
cvar_t sk_leech_dmg_bite3 = {"sk_leech_dmg_bite3", "0"};

// Controller
cvar_t sk_controller_health1 = {"sk_controller_health1", "0"};
cvar_t sk_controller_health2 = {"sk_controller_health2", "0"};
cvar_t sk_controller_health3 = {"sk_controller_health3", "0"};

cvar_t sk_controller_dmgzap1 = {"sk_controller_dmgzap1", "0"};
cvar_t sk_controller_dmgzap2 = {"sk_controller_dmgzap2", "0"};
cvar_t sk_controller_dmgzap3 = {"sk_controller_dmgzap3", "0"};

cvar_t sk_controller_speedball1 = {"sk_controller_speedball1", "0"};
cvar_t sk_controller_speedball2 = {"sk_controller_speedball2", "0"};
cvar_t sk_controller_speedball3 = {"sk_controller_speedball3", "0"};

cvar_t sk_controller_dmgball1 = {"sk_controller_dmgball1", "0"};
cvar_t sk_controller_dmgball2 = {"sk_controller_dmgball2", "0"};
cvar_t sk_controller_dmgball3 = {"sk_controller_dmgball3", "0"};

// Nihilanth
cvar_t sk_nihilanth_health1 = {"sk_nihilanth_health1", "0"};
cvar_t sk_nihilanth_health2 = {"sk_nihilanth_health2", "0"};
cvar_t sk_nihilanth_health3 = {"sk_nihilanth_health3", "0"};

cvar_t sk_nihilanth_zap1 = {"sk_nihilanth_zap1", "0"};
cvar_t sk_nihilanth_zap2 = {"sk_nihilanth_zap2", "0"};
cvar_t sk_nihilanth_zap3 = {"sk_nihilanth_zap3", "0"};

// Scientist
cvar_t sk_scientist_health1 = {"sk_scientist_health1", "0"};
cvar_t sk_scientist_health2 = {"sk_scientist_health2", "0"};
cvar_t sk_scientist_health3 = {"sk_scientist_health3", "0"};

// Nachtrag 2026-09-06 (CVAR_REGISTER-Audit gegen retail gunman.dll
// String-Dump): "sk_scientist_health<N>" ist Stock-SDK-Ballast, existiert
// NICHT im retail-Binary. Der echte, retail-verwendete Cvar-Name ist
// "sk_scientist_h<N>" (bestaetigt: String-Dump + echte Skill.cfg,
// 45/55/65) - monster_scientist (CScientist, scientist.cpp) hatte dadurch
// bislang IMMER Health 0, obwohl es mit 24 Platzierungen die
// meistgenutzte Klasse ist, die noch das alte SDK-Cvar-Muster nutzte.
// Siehe gamerules.cpp fuer die korrigierte GetSkillCvar-Zeile.
cvar_t sk_scientist_h1 = {"sk_scientist_h1", "45"};
cvar_t sk_scientist_h2 = {"sk_scientist_h2", "55"};
cvar_t sk_scientist_h3 = {"sk_scientist_h3", "65"};


// Snark
cvar_t sk_snark_health1 = {"sk_snark_health1", "0"};
cvar_t sk_snark_health2 = {"sk_snark_health2", "0"};
cvar_t sk_snark_health3 = {"sk_snark_health3", "0"};

cvar_t sk_snark_dmg_bite1 = {"sk_snark_dmg_bite1", "0"};
cvar_t sk_snark_dmg_bite2 = {"sk_snark_dmg_bite2", "0"};
cvar_t sk_snark_dmg_bite3 = {"sk_snark_dmg_bite3", "0"};

cvar_t sk_snark_dmg_pop1 = {"sk_snark_dmg_pop1", "0"};
cvar_t sk_snark_dmg_pop2 = {"sk_snark_dmg_pop2", "0"};
cvar_t sk_snark_dmg_pop3 = {"sk_snark_dmg_pop3", "0"};



// Zombie
cvar_t sk_zombie_health1 = {"sk_zombie_health1", "0"};
cvar_t sk_zombie_health2 = {"sk_zombie_health2", "0"};
cvar_t sk_zombie_health3 = {"sk_zombie_health3", "0"};

cvar_t sk_zombie_dmg_one_slash1 = {"sk_zombie_dmg_one_slash1", "0"};
cvar_t sk_zombie_dmg_one_slash2 = {"sk_zombie_dmg_one_slash2", "0"};
cvar_t sk_zombie_dmg_one_slash3 = {"sk_zombie_dmg_one_slash3", "0"};

cvar_t sk_zombie_dmg_both_slash1 = {"sk_zombie_dmg_both_slash1", "0"};
cvar_t sk_zombie_dmg_both_slash2 = {"sk_zombie_dmg_both_slash2", "0"};
cvar_t sk_zombie_dmg_both_slash3 = {"sk_zombie_dmg_both_slash3", "0"};


//Turret
cvar_t sk_turret_health1 = {"sk_turret_health1", "0"};
cvar_t sk_turret_health2 = {"sk_turret_health2", "0"};
cvar_t sk_turret_health3 = {"sk_turret_health3", "0"};


// MiniTurret
cvar_t sk_miniturret_health1 = {"sk_miniturret_health1", "0"};
cvar_t sk_miniturret_health2 = {"sk_miniturret_health2", "0"};
cvar_t sk_miniturret_health3 = {"sk_miniturret_health3", "0"};


// Sentry Turret
cvar_t sk_sentry_health1 = {"sk_sentry_health1", "0"};
cvar_t sk_sentry_health2 = {"sk_sentry_health2", "0"};
cvar_t sk_sentry_health3 = {"sk_sentry_health3", "0"};


// PLAYER WEAPONS

// Crowbar whack
cvar_t sk_plr_crowbar1 = {"sk_plr_crowbar1", "0"};
cvar_t sk_plr_crowbar2 = {"sk_plr_crowbar2", "0"};
cvar_t sk_plr_crowbar3 = {"sk_plr_crowbar3", "0"};

// Glock Round
cvar_t sk_plr_9mm_bullet1 = {"sk_plr_9mm_bullet1", "0"};
cvar_t sk_plr_9mm_bullet2 = {"sk_plr_9mm_bullet2", "0"};
cvar_t sk_plr_9mm_bullet3 = {"sk_plr_9mm_bullet3", "0"};

// 357 Round
cvar_t sk_plr_357_bullet1 = {"sk_plr_357_bullet1", "0"};
cvar_t sk_plr_357_bullet2 = {"sk_plr_357_bullet2", "0"};
cvar_t sk_plr_357_bullet3 = {"sk_plr_357_bullet3", "0"};

// MP5 Round
cvar_t sk_plr_9mmAR_bullet1 = {"sk_plr_9mmAR_bullet1", "0"};
cvar_t sk_plr_9mmAR_bullet2 = {"sk_plr_9mmAR_bullet2", "0"};
cvar_t sk_plr_9mmAR_bullet3 = {"sk_plr_9mmAR_bullet3", "0"};


// M203 grenade
cvar_t sk_plr_9mmAR_grenade1 = {"sk_plr_9mmAR_grenade1", "0"};
cvar_t sk_plr_9mmAR_grenade2 = {"sk_plr_9mmAR_grenade2", "0"};
cvar_t sk_plr_9mmAR_grenade3 = {"sk_plr_9mmAR_grenade3", "0"};


// Shotgun buckshot
cvar_t sk_plr_buckshot1 = {"sk_plr_buckshot1", "0"};
cvar_t sk_plr_buckshot2 = {"sk_plr_buckshot2", "0"};
cvar_t sk_plr_buckshot3 = {"sk_plr_buckshot3", "0"};
// Retail Gunman skill.cfg uses this non-player family for the friendly
// Gunman's shotgun event (three pellets, eight damage each).
cvar_t sk_buckshot1 = {"sk_buckshot1", "8"};
cvar_t sk_buckshot2 = {"sk_buckshot2", "8"};
cvar_t sk_buckshot3 = {"sk_buckshot3", "8"};


// Crossbow
cvar_t sk_plr_xbow_bolt_client1 = {"sk_plr_xbow_bolt_client1", "0"};
cvar_t sk_plr_xbow_bolt_client2 = {"sk_plr_xbow_bolt_client2", "0"};
cvar_t sk_plr_xbow_bolt_client3 = {"sk_plr_xbow_bolt_client3", "0"};

cvar_t sk_plr_xbow_bolt_monster1 = {"sk_plr_xbow_bolt_monster1", "0"};
cvar_t sk_plr_xbow_bolt_monster2 = {"sk_plr_xbow_bolt_monster2", "0"};
cvar_t sk_plr_xbow_bolt_monster3 = {"sk_plr_xbow_bolt_monster3", "0"};


// RPG
cvar_t sk_plr_rpg1 = {"sk_plr_rpg1", "0"};
cvar_t sk_plr_rpg2 = {"sk_plr_rpg2", "0"};
cvar_t sk_plr_rpg3 = {"sk_plr_rpg3", "0"};


// Zero Point Generator
cvar_t sk_plr_gauss1 = {"sk_plr_gauss1", "0"};
cvar_t sk_plr_gauss2 = {"sk_plr_gauss2", "0"};
cvar_t sk_plr_gauss3 = {"sk_plr_gauss3", "0"};


// Tau Cannon
cvar_t sk_plr_egon_narrow1 = {"sk_plr_egon_narrow1", "0"};
cvar_t sk_plr_egon_narrow2 = {"sk_plr_egon_narrow2", "0"};
cvar_t sk_plr_egon_narrow3 = {"sk_plr_egon_narrow3", "0"};

cvar_t sk_plr_egon_wide1 = {"sk_plr_egon_wide1", "0"};
cvar_t sk_plr_egon_wide2 = {"sk_plr_egon_wide2", "0"};
cvar_t sk_plr_egon_wide3 = {"sk_plr_egon_wide3", "0"};


// Hand Grendade
cvar_t sk_plr_hand_grenade1 = {"sk_plr_hand_grenade1", "0"};
cvar_t sk_plr_hand_grenade2 = {"sk_plr_hand_grenade2", "0"};
cvar_t sk_plr_hand_grenade3 = {"sk_plr_hand_grenade3", "0"};

// Gunman melee weapons
cvar_t sk_onehandpunch_d1 = {"sk_onehandpunch_d1", "10"};
cvar_t sk_onehandpunch_d2 = {"sk_onehandpunch_d2", "10"};
cvar_t sk_onehandpunch_d3 = {"sk_onehandpunch_d3", "10"};
cvar_t sk_knife1_d1 = {"sk_knife1_d1", "35"};
cvar_t sk_knife1_d2 = {"sk_knife1_d2", "35"};
cvar_t sk_knife1_d3 = {"sk_knife1_d3", "35"};


// Satchel Charge
cvar_t sk_plr_satchel1 = {"sk_plr_satchel1", "0"};
cvar_t sk_plr_satchel2 = {"sk_plr_satchel2", "0"};
cvar_t sk_plr_satchel3 = {"sk_plr_satchel3", "0"};


// Tripmine
cvar_t sk_plr_tripmine1 = {"sk_plr_tripmine1", "0"};
cvar_t sk_plr_tripmine2 = {"sk_plr_tripmine2", "0"};
cvar_t sk_plr_tripmine3 = {"sk_plr_tripmine3", "0"};

// HORNET
cvar_t sk_plr_hornet_dmg1 = {"sk_plr_hornet_dmg1", "0"};
cvar_t sk_plr_hornet_dmg2 = {"sk_plr_hornet_dmg2", "0"};
cvar_t sk_plr_hornet_dmg3 = {"sk_plr_hornet_dmg3", "0"};


// WORLD WEAPONS
cvar_t sk_12mm_bullet1 = {"sk_12mm_bullet1", "0"};
cvar_t sk_12mm_bullet2 = {"sk_12mm_bullet2", "0"};
cvar_t sk_12mm_bullet3 = {"sk_12mm_bullet3", "0"};

cvar_t sk_9mmAR_bullet1 = {"sk_9mmAR_bullet1", "0"};
cvar_t sk_9mmAR_bullet2 = {"sk_9mmAR_bullet2", "0"};
cvar_t sk_9mmAR_bullet3 = {"sk_9mmAR_bullet3", "0"};

cvar_t sk_9mm_bullet1 = {"sk_9mm_bullet1", "0"};
cvar_t sk_9mm_bullet2 = {"sk_9mm_bullet2", "0"};
cvar_t sk_9mm_bullet3 = {"sk_9mm_bullet3", "0"};


// HORNET
cvar_t sk_hornet_dmg1 = {"sk_hornet_dmg1", "0"};
cvar_t sk_hornet_dmg2 = {"sk_hornet_dmg2", "0"};
cvar_t sk_hornet_dmg3 = {"sk_hornet_dmg3", "0"};

// HEALTH/CHARGE
cvar_t sk_suitcharger1 = {"sk_suitcharger1", "0"};
cvar_t sk_suitcharger2 = {"sk_suitcharger2", "0"};
cvar_t sk_suitcharger3 = {"sk_suitcharger3", "0"};

cvar_t sk_battery1 = {"sk_battery1", "0"};
cvar_t sk_battery2 = {"sk_battery2", "0"};
cvar_t sk_battery3 = {"sk_battery3", "0"};

cvar_t sk_healthcharger1 = {"sk_healthcharger1", "0"};
cvar_t sk_healthcharger2 = {"sk_healthcharger2", "0"};
cvar_t sk_healthcharger3 = {"sk_healthcharger3", "0"};

cvar_t sk_healthkit1 = {"sk_healthkit1", "0"};
cvar_t sk_healthkit2 = {"sk_healthkit2", "0"};
cvar_t sk_healthkit3 = {"sk_healthkit3", "0"};

cvar_t sk_scientist_heal1 = {"sk_scientist_heal1", "0"};
cvar_t sk_scientist_heal2 = {"sk_scientist_heal2", "0"};
cvar_t sk_scientist_heal3 = {"sk_scientist_heal3", "0"};

// Nachtrag 2026-09-06 (RE-Fund via In-Game-Smoke-Test der Cut-Content-
// Rekonstruktion monster_penta): gSkillData.ouranoHealth/
// percentRandomHealth/pentaHealth/pentaBiteDamage (gamerules.cpp,
// RefreshSkillData) lasen seit ihrer Einfuehrung IMMER 0, weil fuer
// diese Gunman-eigenen Cvars (anders als fuer alle SDK-Stock-Cvars
// oben) nie ein CVAR_REGISTER(...) existierte - "exec skill.cfg" kann
// einen noch nie registrierten Cvar-Namen nicht anlegen (nur "Unknown
// command"-Warnung, Wert geht verloren), siehe GetSkillCVar in
// skill.cpp. Betraf COurano::Spawn() (Session "Fauna/Cricket") UND
// CPenta::Spawn() (Cut-Content-Rekonstruktion) UNBEMERKT, bis der
// erste echte In-Game-Test (map cutcontent_test) beide auf "GetSkillCVar
// Got a zero" zeigte. Default-Werte hier bewusst identisch zur echten
// Skill.cfg (siehe mod-src/base/skill.cfg) gesetzt, damit auch ohne
// erfolgreichen skill.cfg-Exec ein sinnvoller Wert vorliegt.
cvar_t sk_ourano_h1 = {"sk_ourano_h1", "70"};
cvar_t sk_ourano_h2 = {"sk_ourano_h2", "70"};
cvar_t sk_ourano_h3 = {"sk_ourano_h3", "80"};
cvar_t sk_ourano_d1 = {"sk_ourano_d1", "30"};
cvar_t sk_ourano_d2 = {"sk_ourano_d2", "35"};
cvar_t sk_ourano_d3 = {"sk_ourano_d3", "40"};

cvar_t sk_percent_random_h1 = {"sk_percent_random_h1", "6"};
cvar_t sk_percent_random_h2 = {"sk_percent_random_h2", "12"};
cvar_t sk_percent_random_h3 = {"sk_percent_random_h3", "18"};

cvar_t sk_penta_h1 = {"sk_penta_h1", "300"};
cvar_t sk_penta_h2 = {"sk_penta_h2", "400"};
cvar_t sk_penta_h3 = {"sk_penta_h3", "500"};

cvar_t sk_penta_bite_d1 = {"sk_penta_bite_d1", "30"};
cvar_t sk_penta_bite_d2 = {"sk_penta_bite_d2", "40"};
cvar_t sk_penta_bite_d3 = {"sk_penta_bite_d3", "50"};

// monster_maggot: exact defaults from the retail Skill.cfg.
cvar_t sk_maggot_h1 = {"sk_maggot_h1", "10"};
cvar_t sk_maggot_h2 = {"sk_maggot_h2", "10"};
cvar_t sk_maggot_h3 = {"sk_maggot_h3", "10"};

// monster_human_bandit and monster_human_chopper: exact defaults from
// the retail Skill.cfg and their respective Retail Spawn functions.
cvar_t sk_bandit_h1 = {"sk_bandit_h1", "100"};
cvar_t sk_bandit_h2 = {"sk_bandit_h2", "110"};
cvar_t sk_bandit_h3 = {"sk_bandit_h3", "125"};
cvar_t sk_bandit_bolt_d1 = {"sk_bandit_bolt_d1", "3"};
cvar_t sk_bandit_bolt_d2 = {"sk_bandit_bolt_d2", "4"};
cvar_t sk_bandit_bolt_d3 = {"sk_bandit_bolt_d3", "6"};
cvar_t sk_bandit_fast_d1 = {"sk_bandit_fast_d1", "3"};
cvar_t sk_bandit_fast_d2 = {"sk_bandit_fast_d2", "6"};
cvar_t sk_bandit_fast_d3 = {"sk_bandit_fast_d3", "12"};
cvar_t sk_gaussshot_d1 = {"sk_gaussshot_d1", "9.5"};
cvar_t sk_gaussshot_d2 = {"sk_gaussshot_d2", "9.5"};
cvar_t sk_gaussshot_d3 = {"sk_gaussshot_d3", "8"};
cvar_t sk_gaussfast_d1 = {"sk_gaussfast_d1", "13"};
cvar_t sk_gaussfast_d2 = {"sk_gaussfast_d2", "13"};
cvar_t sk_gaussfast_d3 = {"sk_gaussfast_d3", "10"};
cvar_t sk_gaussslow_d1 = {"sk_gaussslow_d1", "50"};
cvar_t sk_gaussslow_d2 = {"sk_gaussslow_d2", "50"};
cvar_t sk_gaussslow_d3 = {"sk_gaussslow_d3", "50"};
cvar_t sk_chopper_h1 = {"sk_chopper_h1", "200"};
cvar_t sk_chopper_h2 = {"sk_chopper_h2", "250"};
cvar_t sk_chopper_h3 = {"sk_chopper_h3", "300"};
cvar_t sk_demoman_h1 = {"sk_demoman_h1", "125"};
cvar_t sk_demoman_h2 = {"sk_demoman_h2", "135"};
cvar_t sk_demoman_h3 = {"sk_demoman_h3", "145"};
cvar_t sk_human_kick_d1 = {"sk_human_kick_d1", "20"};
cvar_t sk_human_kick_d2 = {"sk_human_kick_d2", "20"};
cvar_t sk_human_kick_d3 = {"sk_human_kick_d3", "20"};
cvar_t sk_gren_speed1 = {"sk_gren_speed1", "50"};
cvar_t sk_gren_speed2 = {"sk_gren_speed2", "75"};
cvar_t sk_gren_speed3 = {"sk_gren_speed3", "100"};
cvar_t sk_bandit_limp_h1 = {"sk_bandit_limp_h1", "40"};
cvar_t sk_bandit_limp_h2 = {"sk_bandit_limp_h2", "30"};
cvar_t sk_bandit_limp_h3 = {"sk_bandit_limp_h3", "20"};
cvar_t sk_bandit_melee_d1 = {"sk_bandit_melee_d1", "5"};
cvar_t sk_bandit_melee_d2 = {"sk_bandit_melee_d2", "7"};
cvar_t sk_bandit_melee_d3 = {"sk_bandit_melee_d3", "11"};
cvar_t sk_bandit_mech_d1 = {"sk_bandit_mech_d1", "6"};
cvar_t sk_bandit_mech_d2 = {"sk_bandit_mech_d2", "8"};
cvar_t sk_bandit_mech_d3 = {"sk_bandit_mech_d3", "12"};
cvar_t sk_smlscorp_sting_d1 = {"sk_smlscorp_sting_d1", "1"};
cvar_t sk_smlscorp_sting_d2 = {"sk_smlscorp_sting_d2", "2"};
cvar_t sk_smlscorp_sting_d3 = {"sk_smlscorp_sting_d3", "3"};
cvar_t sk_maggot_d1 = {"sk_maggot_d1", "3"};
cvar_t sk_maggot_d2 = {"sk_maggot_d2", "6"};
cvar_t sk_maggot_d3 = {"sk_maggot_d3", "12"};
cvar_t sk_chemgun_d1 = {"sk_chemgun_d1", "85"};
cvar_t sk_chemgun_d2 = {"sk_chemgun_d2", "85"};
cvar_t sk_chemgun_d3 = {"sk_chemgun_d3", "85"};
cvar_t sk_hand_grenade_d1 = {"sk_hand_grenade_d1", "90"};
cvar_t sk_hand_grenade_d2 = {"sk_hand_grenade_d2", "90"};
cvar_t sk_hand_grenade_d3 = {"sk_hand_grenade_d3", "90"};
cvar_t sk_twohandpunch_d1 = {"sk_twohandpunch_d1", "10"};
cvar_t sk_twohandpunch_d2 = {"sk_twohandpunch_d2", "10"};
cvar_t sk_twohandpunch_d3 = {"sk_twohandpunch_d3", "10"};
cvar_t sk_armorsuit1 = {"sk_armorsuit1", "100"};
cvar_t sk_armorsuit2 = {"sk_armorsuit2", "100"};
cvar_t sk_armorsuit3 = {"sk_armorsuit3", "100"};
cvar_t sk_demoman_limp_h1 = {"sk_demoman_limp_h1", "40"};
cvar_t sk_demoman_limp_h2 = {"sk_demoman_limp_h2", "20"};
cvar_t sk_demoman_limp_h3 = {"sk_demoman_limp_h3", "15"};
cvar_t sk_demoman_sg_d1 = {"sk_demoman_sg_d1", "3"};
cvar_t sk_demoman_sg_d2 = {"sk_demoman_sg_d2", "6"};
cvar_t sk_demoman_sg_d3 = {"sk_demoman_sg_d3", "9"};
cvar_t sk_demoman_rocket_d1 = {"sk_demoman_rocket_d1", "50"};
cvar_t sk_demoman_rocket_d2 = {"sk_demoman_rocket_d2", "70"};
cvar_t sk_demoman_rocket_d3 = {"sk_demoman_rocket_d3", "90"};
cvar_t sk_mine_d1 = {"sk_mine_d1", "75"};
cvar_t sk_mine_d2 = {"sk_mine_d2", "75"};
cvar_t sk_mine_d3 = {"sk_mine_d3", "75"};
cvar_t sk_dml_rocket_d1 = {"sk_dml_rocket_d1", "100"};
cvar_t sk_dml_rocket_d2 = {"sk_dml_rocket_d2", "100"};
cvar_t sk_dml_rocket_d3 = {"sk_dml_rocket_d3", "100"};

// monster_critter: exact defaults from the retail Skill.cfg.
cvar_t sk_critter_h1 = {"sk_critter_h1", "75"};
cvar_t sk_critter_h2 = {"sk_critter_h2", "100"};
cvar_t sk_critter_h3 = {"sk_critter_h3", "120"};
cvar_t sk_critter_spit_d1 = {"sk_critter_spit_d1", "8"};
cvar_t sk_critter_spit_d2 = {"sk_critter_spit_d2", "12"};
cvar_t sk_critter_spit_d3 = {"sk_critter_spit_d3", "16"};
cvar_t sk_critter_melee1_d1 = {"sk_critter_melee1_d1", "12"};
cvar_t sk_critter_melee1_d2 = {"sk_critter_melee1_d2", "16"};
cvar_t sk_critter_melee1_d3 = {"sk_critter_melee1_d3", "27"};

// monster_tube: exact defaults from the retail Skill.cfg.
cvar_t sk_tube_h1 = {"sk_tube_h1", "30"};
cvar_t sk_tube_h2 = {"sk_tube_h2", "45"};
cvar_t sk_tube_h3 = {"sk_tube_h3", "60"};

// monster_gator: exact defaults from the retail Skill.cfg.
cvar_t sk_gator_h1 = {"sk_gator_h1", "40"};
cvar_t sk_gator_h2 = {"sk_gator_h2", "50"};
cvar_t sk_gator_h3 = {"sk_gator_h3", "60"};
cvar_t sk_gator_bite_d1 = {"sk_gator_bite_d1", "3"};
cvar_t sk_gator_bite_d2 = {"sk_gator_bite_d2", "6"};
cvar_t sk_gator_bite_d3 = {"sk_gator_bite_d3", "12"};

// Maya fauna: exact defaults from the retail Skill.cfg.
cvar_t sk_raptor_h1 = {"sk_raptor_h1", "75"};
cvar_t sk_raptor_h2 = {"sk_raptor_h2", "90"};
cvar_t sk_raptor_h3 = {"sk_raptor_h3", "110"};
cvar_t sk_raptor_bite_d1 = {"sk_raptor_bite_d1", "5"};
cvar_t sk_raptor_bite_d2 = {"sk_raptor_bite_d2", "7"};
cvar_t sk_raptor_bite_d3 = {"sk_raptor_bite_d3", "15"};
cvar_t sk_raptor_jump_d1 = {"sk_raptor_jump_d1", "5"};
cvar_t sk_raptor_jump_d2 = {"sk_raptor_jump_d2", "7"};
cvar_t sk_raptor_jump_d3 = {"sk_raptor_jump_d3", "17"};
cvar_t sk_smlscorp_h1 = {"sk_smlscorp_h1", "3"};
cvar_t sk_smlscorp_h2 = {"sk_smlscorp_h2", "5"};
cvar_t sk_smlscorp_h3 = {"sk_smlscorp_h3", "7"};
cvar_t sk_smlscorp_claw_d1 = {"sk_smlscorp_claw_d1", "1"};
cvar_t sk_smlscorp_claw_d2 = {"sk_smlscorp_claw_d2", "2"};
cvar_t sk_smlscorp_claw_d3 = {"sk_smlscorp_claw_d3", "4"};
cvar_t sk_lrgscorp_h1 = {"sk_lrgscorp_h1", "45"};
cvar_t sk_lrgscorp_h2 = {"sk_lrgscorp_h2", "55"};
cvar_t sk_lrgscorp_h3 = {"sk_lrgscorp_h3", "65"};
cvar_t sk_lrgscorp_claw_d1 = {"sk_lrgscorp_claw_d1", "5"};
cvar_t sk_lrgscorp_claw_d2 = {"sk_lrgscorp_claw_d2", "10"};
cvar_t sk_lrgscorp_claw_d3 = {"sk_lrgscorp_claw_d3", "15"};
cvar_t sk_lrgscorp_sting_d1 = {"sk_lrgscorp_sting_d1", "7"};
cvar_t sk_lrgscorp_sting_d2 = {"sk_lrgscorp_sting_d2", "14"};
cvar_t sk_lrgscorp_sting_d3 = {"sk_lrgscorp_sting_d3", "20"};
cvar_t sk_rustbit_h1 = {"sk_rustbit_h1", "30"};
cvar_t sk_rustbit_h2 = {"sk_rustbit_h2", "40"};
cvar_t sk_rustbit_h3 = {"sk_rustbit_h3", "55"};
cvar_t sk_rustbit_shot_d1 = {"sk_rustbit_shot_d1", "5"};
cvar_t sk_rustbit_shot_d2 = {"sk_rustbit_shot_d2", "7"};
cvar_t sk_rustbit_shot_d3 = {"sk_rustbit_shot_d3", "9"};
cvar_t sk_rustbit_claw_d1 = {"sk_rustbit_claw_d1", "20"};
cvar_t sk_rustbit_claw_d2 = {"sk_rustbit_claw_d2", "25"};
cvar_t sk_rustbit_claw_d3 = {"sk_rustbit_claw_d3", "35"};
cvar_t sk_rustbot_h1 = {"sk_rustbot_h1", "60"};
cvar_t sk_rustbot_h2 = {"sk_rustbot_h2", "75"};
cvar_t sk_rustbot_h3 = {"sk_rustbot_h3", "120"};
cvar_t sk_rustbot_melee_d1 = {"sk_rustbot_melee_d1", "50"};
cvar_t sk_rustbot_melee_d2 = {"sk_rustbot_melee_d2", "65"};
cvar_t sk_rustbot_melee_d3 = {"sk_rustbot_melee_d3", "80"};
cvar_t sk_rustbot_long_d1 = {"sk_rustbot_long_d1", "3"};
cvar_t sk_rustbot_long_d2 = {"sk_rustbot_long_d2", "4"};
cvar_t sk_rustbot_long_d3 = {"sk_rustbot_long_d3", "6"};
cvar_t sk_rustgunner_h1 = {"sk_rustgunner_h1", "400"};
cvar_t sk_rustgunner_h2 = {"sk_rustgunner_h2", "500"};
cvar_t sk_rustgunner_h3 = {"sk_rustgunner_h3", "700"};
cvar_t sk_rustgunner_r_d1 = {"sk_rustgunner_r_d1", "35"};
cvar_t sk_rustgunner_r_d2 = {"sk_rustgunner_r_d2", "50"};
cvar_t sk_rustgunner_r_d3 = {"sk_rustgunner_r_d3", "60"};
cvar_t sk_rustgunner_mr_d1 = {"sk_rustgunner_mr_d1", "10"};
cvar_t sk_rustgunner_mr_d2 = {"sk_rustgunner_mr_d2", "20"};
cvar_t sk_rustgunner_mr_d3 = {"sk_rustgunner_mr_d3", "35"};
// monster_beak: exact defaults from the retail Skill.cfg.
cvar_t sk_beak_h1 = {"sk_beak_h1", "30"};
cvar_t sk_beak_h2 = {"sk_beak_h2", "40"};
cvar_t sk_beak_h3 = {"sk_beak_h3", "50"};
cvar_t sk_beak_bite_d1 = {"sk_beak_bite_d1", "4"};
cvar_t sk_beak_bite_d2 = {"sk_beak_bite_d2", "7"};
cvar_t sk_beak_bite_d3 = {"sk_beak_bite_d3", "14"};
cvar_t sk_beak_scrape_d1 = {"sk_beak_scrape_d1", "6"};
cvar_t sk_beak_scrape_d2 = {"sk_beak_scrape_d2", "7"};
cvar_t sk_beak_scrape_d3 = {"sk_beak_scrape_d3", "11"};
// monster_hatchetfish: exact defaults from the retail Skill.cfg.
cvar_t sk_hatchetfish_h1 = {"sk_hatchetfish_h1", "6"};
cvar_t sk_hatchetfish_h2 = {"sk_hatchetfish_h2", "6"};
cvar_t sk_hatchetfish_h3 = {"sk_hatchetfish_h3", "6"};
cvar_t sk_hatchetfish_d1 = {"sk_hatchetfish_d1", "2"};
cvar_t sk_hatchetfish_d2 = {"sk_hatchetfish_d2", "2"};
cvar_t sk_hatchetfish_d3 = {"sk_hatchetfish_d3", "2"};
// monster_xenome: exact defaults from the retail Skill.cfg.
cvar_t sk_xenome_h1 = {"sk_xenome_h1", "60"};
cvar_t sk_xenome_h2 = {"sk_xenome_h2", "90"};
cvar_t sk_xenome_h3 = {"sk_xenome_h3", "120"};
cvar_t sk_xenomemelee_d1 = {"sk_xenomemelee_d1", "5"};
cvar_t sk_xenomemelee_d2 = {"sk_xenomemelee_d2", "11"};
cvar_t sk_xenomemelee_d3 = {"sk_xenomemelee_d3", "17"};
cvar_t sk_xenomedart_d1 = {"sk_xenomedart_d1", "3"};
cvar_t sk_xenomedart_d2 = {"sk_xenomedart_d2", "7"};
cvar_t sk_xenomedart_d3 = {"sk_xenomedart_d3", "12"};


// monster damage adjusters
cvar_t sk_monster_head1 = {"sk_monster_head1", "2"};
cvar_t sk_monster_head2 = {"sk_monster_head2", "2"};
cvar_t sk_monster_head3 = {"sk_monster_head3", "2"};

cvar_t sk_monster_chest1 = {"sk_monster_chest1", "1"};
cvar_t sk_monster_chest2 = {"sk_monster_chest2", "1"};
cvar_t sk_monster_chest3 = {"sk_monster_chest3", "1"};

cvar_t sk_monster_stomach1 = {"sk_monster_stomach1", "1"};
cvar_t sk_monster_stomach2 = {"sk_monster_stomach2", "1"};
cvar_t sk_monster_stomach3 = {"sk_monster_stomach3", "1"};

cvar_t sk_monster_arm1 = {"sk_monster_arm1", "1"};
cvar_t sk_monster_arm2 = {"sk_monster_arm2", "1"};
cvar_t sk_monster_arm3 = {"sk_monster_arm3", "1"};

cvar_t sk_monster_leg1 = {"sk_monster_leg1", "1"};
cvar_t sk_monster_leg2 = {"sk_monster_leg2", "1"};
cvar_t sk_monster_leg3 = {"sk_monster_leg3", "1"};

// player damage adjusters
cvar_t sk_player_head1 = {"sk_player_head1", "2"};
cvar_t sk_player_head2 = {"sk_player_head2", "2"};
cvar_t sk_player_head3 = {"sk_player_head3", "2"};

cvar_t sk_player_chest1 = {"sk_player_chest1", "1"};
cvar_t sk_player_chest2 = {"sk_player_chest2", "1"};
cvar_t sk_player_chest3 = {"sk_player_chest3", "1"};

cvar_t sk_player_stomach1 = {"sk_player_stomach1", "1"};
cvar_t sk_player_stomach2 = {"sk_player_stomach2", "1"};
cvar_t sk_player_stomach3 = {"sk_player_stomach3", "1"};

cvar_t sk_player_arm1 = {"sk_player_arm1", "1"};
cvar_t sk_player_arm2 = {"sk_player_arm2", "1"};
cvar_t sk_player_arm3 = {"sk_player_arm3", "1"};

cvar_t sk_player_leg1 = {"sk_player_leg1", "1"};
cvar_t sk_player_leg2 = {"sk_player_leg2", "1"};
cvar_t sk_player_leg3 = {"sk_player_leg3", "1"};

// END Cvars for Skill Level settings

cvar_t sv_pushable_fixed_tick_fudge = {"sv_pushable_fixed_tick_fudge", "15"};

cvar_t sv_busters = {"sv_busters", "0", FCVAR_SERVER};

static bool SV_InitServer()
{
	if (!FileSystem_LoadFileSystem())
	{
		return false;
	}

	return true;
}

// Register your console variables here
// This gets called one time when the game is initialied
void GameDLLInit()
{
	// Register cvars here:

	g_psv_gravity = CVAR_GET_POINTER("sv_gravity");
	g_psv_aim = CVAR_GET_POINTER("sv_aim");
	g_psv_allow_autoaim = CVAR_GET_POINTER("sv_allow_autoaim");
	g_footsteps = CVAR_GET_POINTER("mp_footsteps");
	g_psv_cheats = CVAR_GET_POINTER("sv_cheats");

	if (!SV_InitServer())
	{
		g_engfuncs.pfnServerPrint("Error initializing server\n");
		//Shut the game down as soon as possible.
		SERVER_COMMAND("quit\n");
		return;
	}

	CVAR_REGISTER(&displaysoundlist);
	CVAR_REGISTER(&allow_spectators);

	CVAR_REGISTER(&teamplay);
	CVAR_REGISTER(&fraglimit);
	CVAR_REGISTER(&timelimit);

	CVAR_REGISTER(&fragsleft);
	CVAR_REGISTER(&timeleft);

	CVAR_REGISTER(&friendlyfire);
	CVAR_REGISTER(&falldamage);
	CVAR_REGISTER(&weaponstay);
	CVAR_REGISTER(&forcerespawn);
	CVAR_REGISTER(&flashlight);
	CVAR_REGISTER(&aimcrosshair);
	CVAR_REGISTER(&decalfrequency);
	CVAR_REGISTER(&teamlist);
	CVAR_REGISTER(&teamoverride);
	CVAR_REGISTER(&defaultteam);
	CVAR_REGISTER(&allowmonsters);

	CVAR_REGISTER(&mp_chattime);

	CVAR_REGISTER(&sv_busters);

	CVAR_REGISTER(&sv_allowbunnyhopping);

	// REGISTER CVARS FOR SKILL LEVEL STUFF
	// Agrunt
	CVAR_REGISTER(&sk_agrunt_health1); // {"sk_agrunt_health1","0"};
	CVAR_REGISTER(&sk_agrunt_health2); // {"sk_agrunt_health2","0"};
	CVAR_REGISTER(&sk_agrunt_health3); // {"sk_agrunt_health3","0"};

	CVAR_REGISTER(&sk_agrunt_dmg_punch1); // {"sk_agrunt_dmg_punch1","0"};
	CVAR_REGISTER(&sk_agrunt_dmg_punch2); // {"sk_agrunt_dmg_punch2","0"};
	CVAR_REGISTER(&sk_agrunt_dmg_punch3); // {"sk_agrunt_dmg_punch3","0"};

	// Apache
	CVAR_REGISTER(&sk_apache_health1); // {"sk_apache_health1","0"};
	CVAR_REGISTER(&sk_apache_health2); // {"sk_apache_health2","0"};
	CVAR_REGISTER(&sk_apache_health3); // {"sk_apache_health3","0"};

	// Barney
	CVAR_REGISTER(&sk_barney_health1); // {"sk_barney_health1","0"};
	CVAR_REGISTER(&sk_barney_health2); // {"sk_barney_health2","0"};
	CVAR_REGISTER(&sk_barney_health3); // {"sk_barney_health3","0"};

	// Bullsquid
	CVAR_REGISTER(&sk_bullsquid_health1); // {"sk_bullsquid_health1","0"};
	CVAR_REGISTER(&sk_bullsquid_health2); // {"sk_bullsquid_health2","0"};
	CVAR_REGISTER(&sk_bullsquid_health3); // {"sk_bullsquid_health3","0"};

	CVAR_REGISTER(&sk_bullsquid_dmg_bite1); // {"sk_bullsquid_dmg_bite1","0"};
	CVAR_REGISTER(&sk_bullsquid_dmg_bite2); // {"sk_bullsquid_dmg_bite2","0"};
	CVAR_REGISTER(&sk_bullsquid_dmg_bite3); // {"sk_bullsquid_dmg_bite3","0"};

	CVAR_REGISTER(&sk_bullsquid_dmg_whip1); // {"sk_bullsquid_dmg_whip1","0"};
	CVAR_REGISTER(&sk_bullsquid_dmg_whip2); // {"sk_bullsquid_dmg_whip2","0"};
	CVAR_REGISTER(&sk_bullsquid_dmg_whip3); // {"sk_bullsquid_dmg_whip3","0"};

	CVAR_REGISTER(&sk_bullsquid_dmg_spit1); // {"sk_bullsquid_dmg_spit1","0"};
	CVAR_REGISTER(&sk_bullsquid_dmg_spit2); // {"sk_bullsquid_dmg_spit2","0"};
	CVAR_REGISTER(&sk_bullsquid_dmg_spit3); // {"sk_bullsquid_dmg_spit3","0"};


	CVAR_REGISTER(&sk_bigmomma_health_factor1); // {"sk_bigmomma_health_factor1","1.0"};
	CVAR_REGISTER(&sk_bigmomma_health_factor2); // {"sk_bigmomma_health_factor2","1.0"};
	CVAR_REGISTER(&sk_bigmomma_health_factor3); // {"sk_bigmomma_health_factor3","1.0"};

	CVAR_REGISTER(&sk_bigmomma_dmg_slash1); // {"sk_bigmomma_dmg_slash1","50"};
	CVAR_REGISTER(&sk_bigmomma_dmg_slash2); // {"sk_bigmomma_dmg_slash2","50"};
	CVAR_REGISTER(&sk_bigmomma_dmg_slash3); // {"sk_bigmomma_dmg_slash3","50"};

	CVAR_REGISTER(&sk_bigmomma_dmg_blast1); // {"sk_bigmomma_dmg_blast1","100"};
	CVAR_REGISTER(&sk_bigmomma_dmg_blast2); // {"sk_bigmomma_dmg_blast2","100"};
	CVAR_REGISTER(&sk_bigmomma_dmg_blast3); // {"sk_bigmomma_dmg_blast3","100"};

	CVAR_REGISTER(&sk_bigmomma_radius_blast1); // {"sk_bigmomma_radius_blast1","250"};
	CVAR_REGISTER(&sk_bigmomma_radius_blast2); // {"sk_bigmomma_radius_blast2","250"};
	CVAR_REGISTER(&sk_bigmomma_radius_blast3); // {"sk_bigmomma_radius_blast3","250"};

	// Gargantua
	CVAR_REGISTER(&sk_gargantua_health1); // {"sk_gargantua_health1","0"};
	CVAR_REGISTER(&sk_gargantua_health2); // {"sk_gargantua_health2","0"};
	CVAR_REGISTER(&sk_gargantua_health3); // {"sk_gargantua_health3","0"};

	CVAR_REGISTER(&sk_gargantua_dmg_slash1); // {"sk_gargantua_dmg_slash1","0"};
	CVAR_REGISTER(&sk_gargantua_dmg_slash2); // {"sk_gargantua_dmg_slash2","0"};
	CVAR_REGISTER(&sk_gargantua_dmg_slash3); // {"sk_gargantua_dmg_slash3","0"};

	CVAR_REGISTER(&sk_gargantua_dmg_fire1); // {"sk_gargantua_dmg_fire1","0"};
	CVAR_REGISTER(&sk_gargantua_dmg_fire2); // {"sk_gargantua_dmg_fire2","0"};
	CVAR_REGISTER(&sk_gargantua_dmg_fire3); // {"sk_gargantua_dmg_fire3","0"};

	CVAR_REGISTER(&sk_gargantua_dmg_stomp1); // {"sk_gargantua_dmg_stomp1","0"};
	CVAR_REGISTER(&sk_gargantua_dmg_stomp2); // {"sk_gargantua_dmg_stomp2","0"};
	CVAR_REGISTER(&sk_gargantua_dmg_stomp3); // {"sk_gargantua_dmg_stomp3","0"};


	// Hassassin
	CVAR_REGISTER(&sk_hassassin_health1); // {"sk_hassassin_health1","0"};
	CVAR_REGISTER(&sk_hassassin_health2); // {"sk_hassassin_health2","0"};
	CVAR_REGISTER(&sk_hassassin_health3); // {"sk_hassassin_health3","0"};


	// Headcrab
	CVAR_REGISTER(&sk_headcrab_health1); // {"sk_headcrab_health1","0"};
	CVAR_REGISTER(&sk_headcrab_health2); // {"sk_headcrab_health2","0"};
	CVAR_REGISTER(&sk_headcrab_health3); // {"sk_headcrab_health3","0"};

	CVAR_REGISTER(&sk_headcrab_dmg_bite1); // {"sk_headcrab_dmg_bite1","0"};
	CVAR_REGISTER(&sk_headcrab_dmg_bite2); // {"sk_headcrab_dmg_bite2","0"};
	CVAR_REGISTER(&sk_headcrab_dmg_bite3); // {"sk_headcrab_dmg_bite3","0"};


	// Hgrunt
	CVAR_REGISTER(&sk_hgrunt_health1); // {"sk_hgrunt_health1","0"};
	CVAR_REGISTER(&sk_hgrunt_health2); // {"sk_hgrunt_health2","0"};
	CVAR_REGISTER(&sk_hgrunt_health3); // {"sk_hgrunt_health3","0"};

	CVAR_REGISTER(&sk_hgrunt_kick1); // {"sk_hgrunt_kick1","0"};
	CVAR_REGISTER(&sk_hgrunt_kick2); // {"sk_hgrunt_kick2","0"};
	CVAR_REGISTER(&sk_hgrunt_kick3); // {"sk_hgrunt_kick3","0"};

	CVAR_REGISTER(&sk_hgrunt_pellets1);
	CVAR_REGISTER(&sk_hgrunt_pellets2);
	CVAR_REGISTER(&sk_hgrunt_pellets3);

	CVAR_REGISTER(&sk_hgrunt_gspeed1);
	CVAR_REGISTER(&sk_hgrunt_gspeed2);
	CVAR_REGISTER(&sk_hgrunt_gspeed3);

	// Houndeye
	CVAR_REGISTER(&sk_houndeye_health1); // {"sk_houndeye_health1","0"};
	CVAR_REGISTER(&sk_houndeye_health2); // {"sk_houndeye_health2","0"};
	CVAR_REGISTER(&sk_houndeye_health3); // {"sk_houndeye_health3","0"};

	CVAR_REGISTER(&sk_houndeye_dmg_blast1); // {"sk_houndeye_dmg_blast1","0"};
	CVAR_REGISTER(&sk_houndeye_dmg_blast2); // {"sk_houndeye_dmg_blast2","0"};
	CVAR_REGISTER(&sk_houndeye_dmg_blast3); // {"sk_houndeye_dmg_blast3","0"};


	// ISlave
	CVAR_REGISTER(&sk_islave_health1); // {"sk_islave_health1","0"};
	CVAR_REGISTER(&sk_islave_health2); // {"sk_islave_health2","0"};
	CVAR_REGISTER(&sk_islave_health3); // {"sk_islave_health3","0"};

	CVAR_REGISTER(&sk_islave_dmg_claw1); // {"sk_islave_dmg_claw1","0"};
	CVAR_REGISTER(&sk_islave_dmg_claw2); // {"sk_islave_dmg_claw2","0"};
	CVAR_REGISTER(&sk_islave_dmg_claw3); // {"sk_islave_dmg_claw3","0"};

	CVAR_REGISTER(&sk_islave_dmg_clawrake1); // {"sk_islave_dmg_clawrake1","0"};
	CVAR_REGISTER(&sk_islave_dmg_clawrake2); // {"sk_islave_dmg_clawrake2","0"};
	CVAR_REGISTER(&sk_islave_dmg_clawrake3); // {"sk_islave_dmg_clawrake3","0"};

	CVAR_REGISTER(&sk_islave_dmg_zap1); // {"sk_islave_dmg_zap1","0"};
	CVAR_REGISTER(&sk_islave_dmg_zap2); // {"sk_islave_dmg_zap2","0"};
	CVAR_REGISTER(&sk_islave_dmg_zap3); // {"sk_islave_dmg_zap3","0"};


	// Icthyosaur
	CVAR_REGISTER(&sk_ichthyosaur_health1); // {"sk_ichthyosaur_health1","0"};
	CVAR_REGISTER(&sk_ichthyosaur_health2); // {"sk_ichthyosaur_health2","0"};
	CVAR_REGISTER(&sk_ichthyosaur_health3); // {"sk_ichthyosaur_health3","0"};

	CVAR_REGISTER(&sk_ichthyosaur_shake1); // {"sk_ichthyosaur_health3","0"};
	CVAR_REGISTER(&sk_ichthyosaur_shake2); // {"sk_ichthyosaur_health3","0"};
	CVAR_REGISTER(&sk_ichthyosaur_shake3); // {"sk_ichthyosaur_health3","0"};



	// Leech
	CVAR_REGISTER(&sk_leech_health1); // {"sk_leech_health1","0"};
	CVAR_REGISTER(&sk_leech_health2); // {"sk_leech_health2","0"};
	CVAR_REGISTER(&sk_leech_health3); // {"sk_leech_health3","0"};

	CVAR_REGISTER(&sk_leech_dmg_bite1); // {"sk_leech_dmg_bite1","0"};
	CVAR_REGISTER(&sk_leech_dmg_bite2); // {"sk_leech_dmg_bite2","0"};
	CVAR_REGISTER(&sk_leech_dmg_bite3); // {"sk_leech_dmg_bite3","0"};


	// Controller
	CVAR_REGISTER(&sk_controller_health1);
	CVAR_REGISTER(&sk_controller_health2);
	CVAR_REGISTER(&sk_controller_health3);

	CVAR_REGISTER(&sk_controller_dmgzap1);
	CVAR_REGISTER(&sk_controller_dmgzap2);
	CVAR_REGISTER(&sk_controller_dmgzap3);

	CVAR_REGISTER(&sk_controller_speedball1);
	CVAR_REGISTER(&sk_controller_speedball2);
	CVAR_REGISTER(&sk_controller_speedball3);

	CVAR_REGISTER(&sk_controller_dmgball1);
	CVAR_REGISTER(&sk_controller_dmgball2);
	CVAR_REGISTER(&sk_controller_dmgball3);

	// Nihilanth
	CVAR_REGISTER(&sk_nihilanth_health1); // {"sk_nihilanth_health1","0"};
	CVAR_REGISTER(&sk_nihilanth_health2); // {"sk_nihilanth_health2","0"};
	CVAR_REGISTER(&sk_nihilanth_health3); // {"sk_nihilanth_health3","0"};

	CVAR_REGISTER(&sk_nihilanth_zap1);
	CVAR_REGISTER(&sk_nihilanth_zap2);
	CVAR_REGISTER(&sk_nihilanth_zap3);

	// Scientist
	CVAR_REGISTER(&sk_scientist_health1); // {"sk_scientist_health1","0"};
	CVAR_REGISTER(&sk_scientist_health2); // {"sk_scientist_health2","0"};
	CVAR_REGISTER(&sk_scientist_health3); // {"sk_scientist_health3","0"};
	// Nachtrag 2026-09-06: siehe Deklarations-Kommentar oben - der echte
	// retail-verwendete Cvar fuer monster_scientist.
	CVAR_REGISTER(&sk_scientist_h1);
	CVAR_REGISTER(&sk_scientist_h2);
	CVAR_REGISTER(&sk_scientist_h3);


	// Snark
	CVAR_REGISTER(&sk_snark_health1); // {"sk_snark_health1","0"};
	CVAR_REGISTER(&sk_snark_health2); // {"sk_snark_health2","0"};
	CVAR_REGISTER(&sk_snark_health3); // {"sk_snark_health3","0"};

	CVAR_REGISTER(&sk_snark_dmg_bite1); // {"sk_snark_dmg_bite1","0"};
	CVAR_REGISTER(&sk_snark_dmg_bite2); // {"sk_snark_dmg_bite2","0"};
	CVAR_REGISTER(&sk_snark_dmg_bite3); // {"sk_snark_dmg_bite3","0"};

	CVAR_REGISTER(&sk_snark_dmg_pop1); // {"sk_snark_dmg_pop1","0"};
	CVAR_REGISTER(&sk_snark_dmg_pop2); // {"sk_snark_dmg_pop2","0"};
	CVAR_REGISTER(&sk_snark_dmg_pop3); // {"sk_snark_dmg_pop3","0"};



	// Zombie
	CVAR_REGISTER(&sk_zombie_health1); // {"sk_zombie_health1","0"};
	CVAR_REGISTER(&sk_zombie_health2); // {"sk_zombie_health3","0"};
	CVAR_REGISTER(&sk_zombie_health3); // {"sk_zombie_health3","0"};

	CVAR_REGISTER(&sk_zombie_dmg_one_slash1); // {"sk_zombie_dmg_one_slash1","0"};
	CVAR_REGISTER(&sk_zombie_dmg_one_slash2); // {"sk_zombie_dmg_one_slash2","0"};
	CVAR_REGISTER(&sk_zombie_dmg_one_slash3); // {"sk_zombie_dmg_one_slash3","0"};

	CVAR_REGISTER(&sk_zombie_dmg_both_slash1); // {"sk_zombie_dmg_both_slash1","0"};
	CVAR_REGISTER(&sk_zombie_dmg_both_slash2); // {"sk_zombie_dmg_both_slash2","0"};
	CVAR_REGISTER(&sk_zombie_dmg_both_slash3); // {"sk_zombie_dmg_both_slash3","0"};


	//Turret
	CVAR_REGISTER(&sk_turret_health1); // {"sk_turret_health1","0"};
	CVAR_REGISTER(&sk_turret_health2); // {"sk_turret_health2","0"};
	CVAR_REGISTER(&sk_turret_health3); // {"sk_turret_health3","0"};


	// MiniTurret
	CVAR_REGISTER(&sk_miniturret_health1); // {"sk_miniturret_health1","0"};
	CVAR_REGISTER(&sk_miniturret_health2); // {"sk_miniturret_health2","0"};
	CVAR_REGISTER(&sk_miniturret_health3); // {"sk_miniturret_health3","0"};


	// Sentry Turret
	CVAR_REGISTER(&sk_sentry_health1); // {"sk_sentry_health1","0"};
	CVAR_REGISTER(&sk_sentry_health2); // {"sk_sentry_health2","0"};
	CVAR_REGISTER(&sk_sentry_health3); // {"sk_sentry_health3","0"};


	// PLAYER WEAPONS

	// Crowbar whack
	CVAR_REGISTER(&sk_plr_crowbar1); // {"sk_plr_crowbar1","0"};
	CVAR_REGISTER(&sk_plr_crowbar2); // {"sk_plr_crowbar2","0"};
	CVAR_REGISTER(&sk_plr_crowbar3); // {"sk_plr_crowbar3","0"};

	// Glock Round
	CVAR_REGISTER(&sk_plr_9mm_bullet1); // {"sk_plr_9mm_bullet1","0"};
	CVAR_REGISTER(&sk_plr_9mm_bullet2); // {"sk_plr_9mm_bullet2","0"};
	CVAR_REGISTER(&sk_plr_9mm_bullet3); // {"sk_plr_9mm_bullet3","0"};

	// 357 Round
	CVAR_REGISTER(&sk_plr_357_bullet1); // {"sk_plr_357_bullet1","0"};
	CVAR_REGISTER(&sk_plr_357_bullet2); // {"sk_plr_357_bullet2","0"};
	CVAR_REGISTER(&sk_plr_357_bullet3); // {"sk_plr_357_bullet3","0"};

	// MP5 Round
	CVAR_REGISTER(&sk_plr_9mmAR_bullet1); // {"sk_plr_9mmAR_bullet1","0"};
	CVAR_REGISTER(&sk_plr_9mmAR_bullet2); // {"sk_plr_9mmAR_bullet2","0"};
	CVAR_REGISTER(&sk_plr_9mmAR_bullet3); // {"sk_plr_9mmAR_bullet3","0"};


	// M203 grenade
	CVAR_REGISTER(&sk_plr_9mmAR_grenade1); // {"sk_plr_9mmAR_grenade1","0"};
	CVAR_REGISTER(&sk_plr_9mmAR_grenade2); // {"sk_plr_9mmAR_grenade2","0"};
	CVAR_REGISTER(&sk_plr_9mmAR_grenade3); // {"sk_plr_9mmAR_grenade3","0"};


	// Shotgun buckshot
	CVAR_REGISTER(&sk_plr_buckshot1); // {"sk_plr_buckshot1","0"};
	CVAR_REGISTER(&sk_plr_buckshot2); // {"sk_plr_buckshot2","0"};
	CVAR_REGISTER(&sk_plr_buckshot3); // {"sk_plr_buckshot3","0"};
	CVAR_REGISTER(&sk_buckshot1);
	CVAR_REGISTER(&sk_buckshot2);
	CVAR_REGISTER(&sk_buckshot3);


	// Crossbow
	CVAR_REGISTER(&sk_plr_xbow_bolt_monster1); // {"sk_plr_xbow_bolt1","0"};
	CVAR_REGISTER(&sk_plr_xbow_bolt_monster2); // {"sk_plr_xbow_bolt2","0"};
	CVAR_REGISTER(&sk_plr_xbow_bolt_monster3); // {"sk_plr_xbow_bolt3","0"};

	CVAR_REGISTER(&sk_plr_xbow_bolt_client1); // {"sk_plr_xbow_bolt1","0"};
	CVAR_REGISTER(&sk_plr_xbow_bolt_client2); // {"sk_plr_xbow_bolt2","0"};
	CVAR_REGISTER(&sk_plr_xbow_bolt_client3); // {"sk_plr_xbow_bolt3","0"};


	// RPG
	CVAR_REGISTER(&sk_plr_rpg1); // {"sk_plr_rpg1","0"};
	CVAR_REGISTER(&sk_plr_rpg2); // {"sk_plr_rpg2","0"};
	CVAR_REGISTER(&sk_plr_rpg3); // {"sk_plr_rpg3","0"};


	// Gauss Gun
	CVAR_REGISTER(&sk_plr_gauss1); // {"sk_plr_gauss1","0"};
	CVAR_REGISTER(&sk_plr_gauss2); // {"sk_plr_gauss2","0"};
	CVAR_REGISTER(&sk_plr_gauss3); // {"sk_plr_gauss3","0"};


	// Egon Gun
	CVAR_REGISTER(&sk_plr_egon_narrow1); // {"sk_plr_egon_narrow1","0"};
	CVAR_REGISTER(&sk_plr_egon_narrow2); // {"sk_plr_egon_narrow2","0"};
	CVAR_REGISTER(&sk_plr_egon_narrow3); // {"sk_plr_egon_narrow3","0"};

	CVAR_REGISTER(&sk_plr_egon_wide1); // {"sk_plr_egon_wide1","0"};
	CVAR_REGISTER(&sk_plr_egon_wide2); // {"sk_plr_egon_wide2","0"};
	CVAR_REGISTER(&sk_plr_egon_wide3); // {"sk_plr_egon_wide3","0"};


	// Hand Grendade
	CVAR_REGISTER(&sk_plr_hand_grenade1); // {"sk_plr_hand_grenade1","0"};
	CVAR_REGISTER(&sk_plr_hand_grenade2); // {"sk_plr_hand_grenade2","0"};
	CVAR_REGISTER(&sk_plr_hand_grenade3); // {"sk_plr_hand_grenade3","0"};
	CVAR_REGISTER(&sk_onehandpunch_d1);
	CVAR_REGISTER(&sk_onehandpunch_d2);
	CVAR_REGISTER(&sk_onehandpunch_d3);
	CVAR_REGISTER(&sk_knife1_d1);
	CVAR_REGISTER(&sk_knife1_d2);
	CVAR_REGISTER(&sk_knife1_d3);


	// Satchel Charge
	CVAR_REGISTER(&sk_plr_satchel1); // {"sk_plr_satchel1","0"};
	CVAR_REGISTER(&sk_plr_satchel2); // {"sk_plr_satchel2","0"};
	CVAR_REGISTER(&sk_plr_satchel3); // {"sk_plr_satchel3","0"};


	// Tripmine
	CVAR_REGISTER(&sk_plr_tripmine1); // {"sk_plr_tripmine1","0"};
	CVAR_REGISTER(&sk_plr_tripmine2); // {"sk_plr_tripmine2","0"};
	CVAR_REGISTER(&sk_plr_tripmine3); // {"sk_plr_tripmine3","0"};

	// HORNET
	CVAR_REGISTER(&sk_plr_hornet_dmg1); // {"sk_plr_hornet_dmg1","0"};
	CVAR_REGISTER(&sk_plr_hornet_dmg2); // {"sk_plr_hornet_dmg2","0"};
	CVAR_REGISTER(&sk_plr_hornet_dmg3); // {"sk_plr_hornet_dmg3","0"};


	// WORLD WEAPONS
	CVAR_REGISTER(&sk_12mm_bullet1); // {"sk_12mm_bullet1","0"};
	CVAR_REGISTER(&sk_12mm_bullet2); // {"sk_12mm_bullet2","0"};
	CVAR_REGISTER(&sk_12mm_bullet3); // {"sk_12mm_bullet3","0"};

	CVAR_REGISTER(&sk_9mmAR_bullet1); // {"sk_9mm_bullet1","0"};
	CVAR_REGISTER(&sk_9mmAR_bullet2); // {"sk_9mm_bullet1","0"};
	CVAR_REGISTER(&sk_9mmAR_bullet3); // {"sk_9mm_bullet1","0"};

	CVAR_REGISTER(&sk_9mm_bullet1); // {"sk_9mm_bullet1","0"};
	CVAR_REGISTER(&sk_9mm_bullet2); // {"sk_9mm_bullet2","0"};
	CVAR_REGISTER(&sk_9mm_bullet3); // {"sk_9mm_bullet3","0"};


	// HORNET
	CVAR_REGISTER(&sk_hornet_dmg1); // {"sk_hornet_dmg1","0"};
	CVAR_REGISTER(&sk_hornet_dmg2); // {"sk_hornet_dmg2","0"};
	CVAR_REGISTER(&sk_hornet_dmg3); // {"sk_hornet_dmg3","0"};

	// HEALTH/SUIT CHARGE DISTRIBUTION
	CVAR_REGISTER(&sk_suitcharger1);
	CVAR_REGISTER(&sk_suitcharger2);
	CVAR_REGISTER(&sk_suitcharger3);

	CVAR_REGISTER(&sk_battery1);
	CVAR_REGISTER(&sk_battery2);
	CVAR_REGISTER(&sk_battery3);

	CVAR_REGISTER(&sk_healthcharger1);
	CVAR_REGISTER(&sk_healthcharger2);
	CVAR_REGISTER(&sk_healthcharger3);

	CVAR_REGISTER(&sk_healthkit1);
	CVAR_REGISTER(&sk_healthkit2);
	CVAR_REGISTER(&sk_healthkit3);

	CVAR_REGISTER(&sk_scientist_heal1);
	CVAR_REGISTER(&sk_scientist_heal2);
	CVAR_REGISTER(&sk_scientist_heal3);

	// Nachtrag 2026-09-06: siehe Deklarations-Kommentar oben - ohne
	// diese Registrierung liest gSkillData.ouranoHealth/
	// percentRandomHealth/pentaHealth/pentaBiteDamage immer 0.
	CVAR_REGISTER(&sk_ourano_h1);
	CVAR_REGISTER(&sk_ourano_h2);
	CVAR_REGISTER(&sk_ourano_h3);
	CVAR_REGISTER(&sk_ourano_d1);
	CVAR_REGISTER(&sk_ourano_d2);
	CVAR_REGISTER(&sk_ourano_d3);

	CVAR_REGISTER(&sk_percent_random_h1);
	CVAR_REGISTER(&sk_percent_random_h2);
	CVAR_REGISTER(&sk_percent_random_h3);

	CVAR_REGISTER(&sk_penta_h1);
	CVAR_REGISTER(&sk_penta_h2);
	CVAR_REGISTER(&sk_penta_h3);

	CVAR_REGISTER(&sk_penta_bite_d1);
	CVAR_REGISTER(&sk_penta_bite_d2);
	CVAR_REGISTER(&sk_penta_bite_d3);

	CVAR_REGISTER(&sk_maggot_h1);
	CVAR_REGISTER(&sk_maggot_h2);
	CVAR_REGISTER(&sk_maggot_h3);

	CVAR_REGISTER(&sk_bandit_h1);
	CVAR_REGISTER(&sk_bandit_h2);
	CVAR_REGISTER(&sk_bandit_h3);
	CVAR_REGISTER(&sk_bandit_bolt_d1);
	CVAR_REGISTER(&sk_bandit_bolt_d2);
	CVAR_REGISTER(&sk_bandit_bolt_d3);
	CVAR_REGISTER(&sk_bandit_fast_d1);
	CVAR_REGISTER(&sk_bandit_fast_d2);
	CVAR_REGISTER(&sk_bandit_fast_d3);
	CVAR_REGISTER(&sk_gaussshot_d1);
	CVAR_REGISTER(&sk_gaussshot_d2);
	CVAR_REGISTER(&sk_gaussshot_d3);
	CVAR_REGISTER(&sk_gaussfast_d1);
	CVAR_REGISTER(&sk_gaussfast_d2);
	CVAR_REGISTER(&sk_gaussfast_d3);
	CVAR_REGISTER(&sk_gaussslow_d1);
	CVAR_REGISTER(&sk_gaussslow_d2);
	CVAR_REGISTER(&sk_gaussslow_d3);
	CVAR_REGISTER(&sk_chopper_h1);
	CVAR_REGISTER(&sk_chopper_h2);
	CVAR_REGISTER(&sk_chopper_h3);
	CVAR_REGISTER(&sk_demoman_h1);
	CVAR_REGISTER(&sk_demoman_h2);
	CVAR_REGISTER(&sk_demoman_h3);
	CVAR_REGISTER(&sk_human_kick_d1);
	CVAR_REGISTER(&sk_human_kick_d2);
	CVAR_REGISTER(&sk_human_kick_d3);
	CVAR_REGISTER(&sk_gren_speed1);
	CVAR_REGISTER(&sk_gren_speed2);
	CVAR_REGISTER(&sk_gren_speed3);
	CVAR_REGISTER(&sk_bandit_limp_h1);
	CVAR_REGISTER(&sk_bandit_limp_h2);
	CVAR_REGISTER(&sk_bandit_limp_h3);
	CVAR_REGISTER(&sk_bandit_melee_d1);
	CVAR_REGISTER(&sk_bandit_melee_d2);
	CVAR_REGISTER(&sk_bandit_melee_d3);
	CVAR_REGISTER(&sk_bandit_mech_d1);
	CVAR_REGISTER(&sk_bandit_mech_d2);
	CVAR_REGISTER(&sk_bandit_mech_d3);
	CVAR_REGISTER(&sk_smlscorp_sting_d1);
	CVAR_REGISTER(&sk_smlscorp_sting_d2);
	CVAR_REGISTER(&sk_smlscorp_sting_d3);
	CVAR_REGISTER(&sk_maggot_d1);
	CVAR_REGISTER(&sk_maggot_d2);
	CVAR_REGISTER(&sk_maggot_d3);
	CVAR_REGISTER(&sk_chemgun_d1);
	CVAR_REGISTER(&sk_chemgun_d2);
	CVAR_REGISTER(&sk_chemgun_d3);
	CVAR_REGISTER(&sk_hand_grenade_d1);
	CVAR_REGISTER(&sk_hand_grenade_d2);
	CVAR_REGISTER(&sk_hand_grenade_d3);
	CVAR_REGISTER(&sk_twohandpunch_d1);
	CVAR_REGISTER(&sk_twohandpunch_d2);
	CVAR_REGISTER(&sk_twohandpunch_d3);
	CVAR_REGISTER(&sk_armorsuit1);
	CVAR_REGISTER(&sk_armorsuit2);
	CVAR_REGISTER(&sk_armorsuit3);
	CVAR_REGISTER(&sk_demoman_limp_h1);
	CVAR_REGISTER(&sk_demoman_limp_h2);
	CVAR_REGISTER(&sk_demoman_limp_h3);
	CVAR_REGISTER(&sk_demoman_sg_d1);
	CVAR_REGISTER(&sk_demoman_sg_d2);
	CVAR_REGISTER(&sk_demoman_sg_d3);
	CVAR_REGISTER(&sk_demoman_rocket_d1);
	CVAR_REGISTER(&sk_demoman_rocket_d2);
	CVAR_REGISTER(&sk_demoman_rocket_d3);
	CVAR_REGISTER(&sk_mine_d1);
	CVAR_REGISTER(&sk_mine_d2);
	CVAR_REGISTER(&sk_mine_d3);
	CVAR_REGISTER(&sk_dml_rocket_d1);
	CVAR_REGISTER(&sk_dml_rocket_d2);
	CVAR_REGISTER(&sk_dml_rocket_d3);

	CVAR_REGISTER(&sk_critter_h1);
	CVAR_REGISTER(&sk_critter_h2);
	CVAR_REGISTER(&sk_critter_h3);
	CVAR_REGISTER(&sk_critter_spit_d1);
	CVAR_REGISTER(&sk_critter_spit_d2);
	CVAR_REGISTER(&sk_critter_spit_d3);
	CVAR_REGISTER(&sk_critter_melee1_d1);
	CVAR_REGISTER(&sk_critter_melee1_d2);
	CVAR_REGISTER(&sk_critter_melee1_d3);

	CVAR_REGISTER(&sk_tube_h1);
	CVAR_REGISTER(&sk_tube_h2);
	CVAR_REGISTER(&sk_tube_h3);

	CVAR_REGISTER(&sk_gator_h1);
	CVAR_REGISTER(&sk_gator_h2);
	CVAR_REGISTER(&sk_gator_h3);
	CVAR_REGISTER(&sk_gator_bite_d1);
	CVAR_REGISTER(&sk_gator_bite_d2);
	CVAR_REGISTER(&sk_gator_bite_d3);

	CVAR_REGISTER(&sk_raptor_h1);
	CVAR_REGISTER(&sk_raptor_h2);
	CVAR_REGISTER(&sk_raptor_h3);
	CVAR_REGISTER(&sk_raptor_bite_d1);
	CVAR_REGISTER(&sk_raptor_bite_d2);
	CVAR_REGISTER(&sk_raptor_bite_d3);
	CVAR_REGISTER(&sk_raptor_jump_d1);
	CVAR_REGISTER(&sk_raptor_jump_d2);
	CVAR_REGISTER(&sk_raptor_jump_d3);
	CVAR_REGISTER(&sk_smlscorp_h1);
	CVAR_REGISTER(&sk_smlscorp_h2);
	CVAR_REGISTER(&sk_smlscorp_h3);
	CVAR_REGISTER(&sk_smlscorp_claw_d1);
	CVAR_REGISTER(&sk_smlscorp_claw_d2);
	CVAR_REGISTER(&sk_smlscorp_claw_d3);
	CVAR_REGISTER(&sk_lrgscorp_h1);
	CVAR_REGISTER(&sk_lrgscorp_h2);
	CVAR_REGISTER(&sk_lrgscorp_h3);
	CVAR_REGISTER(&sk_lrgscorp_claw_d1);
	CVAR_REGISTER(&sk_lrgscorp_claw_d2);
	CVAR_REGISTER(&sk_lrgscorp_claw_d3);
	CVAR_REGISTER(&sk_lrgscorp_sting_d1);
	CVAR_REGISTER(&sk_lrgscorp_sting_d2);
	CVAR_REGISTER(&sk_lrgscorp_sting_d3);
	CVAR_REGISTER(&sk_rustbit_h1);
	CVAR_REGISTER(&sk_rustbit_h2);
	CVAR_REGISTER(&sk_rustbit_h3);
	CVAR_REGISTER(&sk_rustbit_shot_d1);
	CVAR_REGISTER(&sk_rustbit_shot_d2);
	CVAR_REGISTER(&sk_rustbit_shot_d3);
	CVAR_REGISTER(&sk_rustbit_claw_d1);
	CVAR_REGISTER(&sk_rustbit_claw_d2);
	CVAR_REGISTER(&sk_rustbit_claw_d3);
	CVAR_REGISTER(&sk_rustbot_h1);
	CVAR_REGISTER(&sk_rustbot_h2);
	CVAR_REGISTER(&sk_rustbot_h3);
	CVAR_REGISTER(&sk_rustbot_melee_d1);
	CVAR_REGISTER(&sk_rustbot_melee_d2);
	CVAR_REGISTER(&sk_rustbot_melee_d3);
	CVAR_REGISTER(&sk_rustbot_long_d1);
	CVAR_REGISTER(&sk_rustbot_long_d2);
	CVAR_REGISTER(&sk_rustbot_long_d3);
	CVAR_REGISTER(&sk_rustgunner_h1);
	CVAR_REGISTER(&sk_rustgunner_h2);
	CVAR_REGISTER(&sk_rustgunner_h3);
	CVAR_REGISTER(&sk_rustgunner_r_d1);
	CVAR_REGISTER(&sk_rustgunner_r_d2);
	CVAR_REGISTER(&sk_rustgunner_r_d3);
	CVAR_REGISTER(&sk_rustgunner_mr_d1);
	CVAR_REGISTER(&sk_rustgunner_mr_d2);
	CVAR_REGISTER(&sk_rustgunner_mr_d3);
	CVAR_REGISTER(&sk_beak_h1);
	CVAR_REGISTER(&sk_beak_h2);
	CVAR_REGISTER(&sk_beak_h3);
	CVAR_REGISTER(&sk_beak_bite_d1);
	CVAR_REGISTER(&sk_beak_bite_d2);
	CVAR_REGISTER(&sk_beak_bite_d3);
	CVAR_REGISTER(&sk_beak_scrape_d1);
	CVAR_REGISTER(&sk_beak_scrape_d2);
	CVAR_REGISTER(&sk_beak_scrape_d3);
	CVAR_REGISTER(&sk_hatchetfish_h1);
	CVAR_REGISTER(&sk_hatchetfish_h2);
	CVAR_REGISTER(&sk_hatchetfish_h3);
	CVAR_REGISTER(&sk_hatchetfish_d1);
	CVAR_REGISTER(&sk_hatchetfish_d2);
	CVAR_REGISTER(&sk_hatchetfish_d3);
	CVAR_REGISTER(&sk_xenome_h1);
	CVAR_REGISTER(&sk_xenome_h2);
	CVAR_REGISTER(&sk_xenome_h3);
	CVAR_REGISTER(&sk_xenomemelee_d1);
	CVAR_REGISTER(&sk_xenomemelee_d2);
	CVAR_REGISTER(&sk_xenomemelee_d3);
	CVAR_REGISTER(&sk_xenomedart_d1);
	CVAR_REGISTER(&sk_xenomedart_d2);
	CVAR_REGISTER(&sk_xenomedart_d3);

	// monster damage adjusters
	CVAR_REGISTER(&sk_monster_head1);
	CVAR_REGISTER(&sk_monster_head2);
	CVAR_REGISTER(&sk_monster_head3);

	CVAR_REGISTER(&sk_monster_chest1);
	CVAR_REGISTER(&sk_monster_chest2);
	CVAR_REGISTER(&sk_monster_chest3);

	CVAR_REGISTER(&sk_monster_stomach1);
	CVAR_REGISTER(&sk_monster_stomach2);
	CVAR_REGISTER(&sk_monster_stomach3);

	CVAR_REGISTER(&sk_monster_arm1);
	CVAR_REGISTER(&sk_monster_arm2);
	CVAR_REGISTER(&sk_monster_arm3);

	CVAR_REGISTER(&sk_monster_leg1);
	CVAR_REGISTER(&sk_monster_leg2);
	CVAR_REGISTER(&sk_monster_leg3);

	// player damage adjusters
	CVAR_REGISTER(&sk_player_head1);
	CVAR_REGISTER(&sk_player_head2);
	CVAR_REGISTER(&sk_player_head3);

	CVAR_REGISTER(&sk_player_chest1);
	CVAR_REGISTER(&sk_player_chest2);
	CVAR_REGISTER(&sk_player_chest3);

	CVAR_REGISTER(&sk_player_stomach1);
	CVAR_REGISTER(&sk_player_stomach2);
	CVAR_REGISTER(&sk_player_stomach3);

	CVAR_REGISTER(&sk_player_arm1);
	CVAR_REGISTER(&sk_player_arm2);
	CVAR_REGISTER(&sk_player_arm3);

	CVAR_REGISTER(&sk_player_leg1);
	CVAR_REGISTER(&sk_player_leg2);
	CVAR_REGISTER(&sk_player_leg3);
	// END REGISTER CVARS FOR SKILL LEVEL STUFF

	CVAR_REGISTER(&sv_pushable_fixed_tick_fudge);

	InitMapLoadingUtils();

	SERVER_COMMAND("exec skill.cfg\n");
}

void GameDLLShutdown()
{
	FileSystem_FreeFileSystem();
}
