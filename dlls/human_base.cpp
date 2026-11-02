//=========================================================
// CHumanBase - Implementierung (Stufen 1-3: Schedule-Tabellen, StartTask/RunTask, GetScheduleOfType/GetSchedule).
// Die Schedule-Tabellen sind aus den Retail-Daten @0x10129c90..0x1012a4a8 uebernommen (Aufgaben, Interrupt-
// und Sound-Masken, Namen); Vereinfachungen siehe findings/entities/human_base_stufe1-3_2026-10-31.md.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "animation.h"
#include "squadmonster.h"
#include "soundent.h"
#include "saverestore.h"
#include "talkmonster.h"
#include "human_base.h"
#include "weapons.h"
#include "skill.h"
#include "gamerules.h"

#define HB_SENTENCE_VOLUME 0.25f // Retail: 0x3e800000
#define HB_SENTENCE_ATTN 0.8f	 // Retail: 0x3f4ccccd

float CHumanBase::s_flTalkWaitTime = 0;
int CHumanBase::s_iQuestionStage = 0;

// Retail Schedule_t @0x10129c90
static Task_t tlHBGruntFail_9c90[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_IDLE_ACTIVITY, (float)0},
	{TASK_WAIT, (float)2},
	{TASK_WAIT_PVS, (float)0},
};
static Schedule_t slHBGruntFail_9c90[] = {{tlHBGruntFail_9c90, ARRAYSIZE(tlHBGruntFail_9c90),
	bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2 | bits_COND_CAN_MELEE_ATTACK2,
	0,
	"Grunt Fail"}};

// Retail Schedule_t @0x10129cc8
static Task_t tlHBGruntCombatFail_9cc8[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_IDLE_ACTIVITY, (float)0},
	{TASK_WAIT_FACE_ENEMY, (float)2},
	{TASK_WAIT_PVS, (float)0},
};
static Schedule_t slHBGruntCombatFail_9cc8[] = {{tlHBGruntCombatFail_9cc8, ARRAYSIZE(tlHBGruntCombatFail_9cc8),
	bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2,
	0,
	"Grunt Combat Fail"}};

// Retail Schedule_t @0x10129d20
static Task_t tlHBGruntvictorydance_9d20[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_WAIT, 1.5f},
	{TASK_GET_PATH_TO_ENEMY_CORPSE, (float)0},
	{TASK_WALK_PATH, (float)0},
	{TASK_WAIT_FOR_MOVEMENT, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
};
static Schedule_t slHBGruntvictorydance_9d20[] = {{tlHBGruntvictorydance_9d20, ARRAYSIZE(tlHBGruntvictorydance_9d20),
	bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY,
	0,
	"GruntVictoryDance"}};

// Retail Schedule_t @0x10129d60
static Task_t tlHBGruntestablishlineoffire_9d60[] = {
	{TASK_SET_FAIL_SCHEDULE, (float)HB_SCHED_ELOF_FAIL},
	{TASK_GET_PATH_TO_ENEMY, (float)0},
	{TASK_HB_SPEAK_SENTENCE, (float)0},
	{TASK_RUN_PATH, (float)0},
	{TASK_WAIT_FOR_MOVEMENT, (float)0},
};
static Schedule_t slHBGruntestablishlineoffire_9d60[] = {{tlHBGruntestablishlineoffire_9d60, ARRAYSIZE(tlHBGruntestablishlineoffire_9d60),
	bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2 | bits_COND_CAN_MELEE_ATTACK2 | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND | bits_COND_ENEMY_DEAD,
	bits_SOUND_DANGER,
	"GruntEstablishLineOfFire"}};

// Retail Schedule_t @0x10129d90
static Task_t tlHBGruntfoundenemy_9d90[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_WAIT_RANDOM, 0.3f},
};
static Schedule_t slHBGruntfoundenemy_9d90[] = {{tlHBGruntfoundenemy_9d90, ARRAYSIZE(tlHBGruntfoundenemy_9d90),
	bits_COND_HEAR_SOUND,
	bits_SOUND_DANGER,
	"GruntFoundEnemy"}};

// Retail Schedule_t @0x10129dd0
static Task_t tlHBCombatFace_9dd0[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_IDLE_ACTIVITY, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_WAIT, 1.5f},
	{TASK_SET_SCHEDULE, (float)HB_SCHED_SWEEP},
};
static Schedule_t slHBCombatFace_9dd0[] = {{tlHBCombatFace_9dd0, ARRAYSIZE(tlHBCombatFace_9dd0),
	bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2 | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD,
	0,
	"Combat Face"}};

// Retail Schedule_t @0x10129e80
static Task_t tlHBSignalsuppress_9e80[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_FACE_IDEAL, (float)0},
	{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_COMBAT_IDLE},
	{TASK_WAIT_RANDOM, 0.2f},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
};
static Schedule_t slHBSignalsuppress_9e80[] = {{tlHBSignalsuppress_9e80, ARRAYSIZE(tlHBSignalsuppress_9e80),
	bits_COND_NO_AMMO_LOADED | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND | bits_COND_ENEMY_DEAD | bits_COND_HB_NOFIRE,
	bits_SOUND_DANGER,
	"SignalSuppress"}};

// Retail Schedule_t @0x10129f18
static Task_t tlHBSuppress_9f18[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
};
static Schedule_t slHBSuppress_9f18[] = {{tlHBSuppress_9f18, ARRAYSIZE(tlHBSuppress_9f18),
	bits_COND_NO_AMMO_LOADED | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND | bits_COND_ENEMY_DEAD | bits_COND_HB_NOFIRE,
	bits_SOUND_DANGER,
	"Suppress"}};

// Retail Schedule_t @0x10129f48
static Task_t tlHBGruntwaitincover_9f48[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_IDLE_ACTIVITY, (float)0},
	{TASK_WAIT_FACE_ENEMY, (float)1},
};
static Schedule_t slHBGruntwaitincover_9f48[] = {{tlHBGruntwaitincover_9f48, ARRAYSIZE(tlHBGruntwaitincover_9f48),
	bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2 | bits_COND_CAN_MELEE_ATTACK2 | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND,
	bits_SOUND_DANGER,
	"GruntWaitInCover"}};

// Retail Schedule_t @0x10129fa8
static Task_t tlHBTakecover_9fa8[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_SET_FAIL_SCHEDULE, (float)HB_SCHED_TAKECOVER_FAILED},
	{TASK_WAIT, 0.2f},
	{TASK_FIND_COVER_FROM_ENEMY, (float)0},
	{TASK_HB_SPEAK_SENTENCE, (float)0},
	{TASK_RUN_PATH, (float)0},
	{TASK_WAIT_FOR_MOVEMENT, (float)0},
	{TASK_REMEMBER, (float)bits_MEMORY_INCOVER},
	{TASK_SET_SCHEDULE, (float)HB_SCHED_WAIT_FACE_ENEMY},
};
static Schedule_t slHBTakecover_9fa8[] = {{tlHBTakecover_9fa8, ARRAYSIZE(tlHBTakecover_9fa8),
	0,
	0,
	"TakeCover"}};

// Retail Schedule_t @0x1012a000
static Task_t tlHBGrenadecover_a000[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_FIND_COVER_FROM_ENEMY, (float)99},
	{TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, (float)384},
	{TASK_PLAY_SEQUENCE, (float)ACT_SPECIAL_ATTACK1},
	{TASK_CLEAR_MOVE_WAIT, (float)0},
	{TASK_RUN_PATH, (float)0},
	{TASK_WAIT_FOR_MOVEMENT, (float)0},
	{TASK_SET_SCHEDULE, (float)HB_SCHED_WAIT_FACE_ENEMY},
};
static Schedule_t slHBGrenadecover_a000[] = {{tlHBGrenadecover_a000, ARRAYSIZE(tlHBGrenadecover_a000),
	0,
	0,
	"GrenadeCover"}};

// Retail Schedule_t @0x1012a030
static Task_t tlHBTossgrenadecover_a030[] = {
	{TASK_FACE_ENEMY, (float)0},
	{TASK_RANGE_ATTACK2, (float)0},
	{TASK_SET_SCHEDULE, (float)SCHED_TAKE_COVER_FROM_ENEMY},
};
static Schedule_t slHBTossgrenadecover_a030[] = {{tlHBTossgrenadecover_a030, ARRAYSIZE(tlHBTossgrenadecover_a030),
	0,
	0,
	"TossGrenadeCover"}};

// Retail Schedule_t @0x1012a080
static Task_t tlHBGrunttakecoverfrombestsound_a080[] = {
	{TASK_SET_FAIL_SCHEDULE, (float)SCHED_COWER},
	{TASK_STOP_MOVING, (float)0},
	{TASK_FIND_COVER_FROM_BEST_SOUND, (float)0},
	{TASK_RUN_PATH, (float)0},
	{TASK_WAIT_FOR_MOVEMENT, (float)0},
	{TASK_REMEMBER, (float)bits_MEMORY_INCOVER},
	{TASK_TURN_LEFT, (float)179},
};
static Schedule_t slHBGrunttakecoverfrombestsound_a080[] = {{tlHBGrunttakecoverfrombestsound_a080, ARRAYSIZE(tlHBGrunttakecoverfrombestsound_a080),
	0,
	0,
	"GruntTakeCoverFromBestSound"}};

// Retail Schedule_t @0x1012a0d8
static Task_t tlHBGrunthidereload_a0d8[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_SET_FAIL_SCHEDULE, (float)SCHED_RELOAD},
	{TASK_FIND_COVER_FROM_ENEMY, (float)0},
	{TASK_RUN_PATH, (float)0},
	{TASK_RELOAD, (float)0},
	{TASK_WAIT_FOR_MOVEMENT, (float)0},
	{TASK_REMEMBER, (float)bits_MEMORY_INCOVER},
	{TASK_FACE_ENEMY, (float)0},
};
static Schedule_t slHBGrunthidereload_a0d8[] = {{tlHBGrunthidereload_a0d8, ARRAYSIZE(tlHBGrunthidereload_a0d8),
	0,
	0,
	"GruntHideReload"}};

// Retail Schedule_t @0x1012a108
static Task_t tlHBGruntreload_a108[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_RELOAD, (float)0},
	{TASK_FACE_ENEMY, (float)0},
};
static Schedule_t slHBGruntreload_a108[] = {{tlHBGruntreload_a108, ARRAYSIZE(tlHBGruntreload_a108),
	0,
	0,
	"GruntReload"}};

// Retail Schedule_t @0x1012a140
static Task_t tlHBGruntSweep_a140[] = {
	{TASK_TURN_LEFT, (float)179},
	{TASK_WAIT, (float)1},
	{TASK_TURN_LEFT, (float)179},
	{TASK_WAIT, (float)1},
};
static Schedule_t slHBGruntSweep_a140[] = {{tlHBGruntSweep_a140, ARRAYSIZE(tlHBGruntSweep_a140),
	bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2 | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND,
	bits_SOUND_WORLD | bits_SOUND_PLAYER | bits_SOUND_DANGER,
	"Grunt Sweep"}};

// Retail Schedule_t @0x1012a178
static Task_t tlHBRangeattack2_a178[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_FACE_TOSS_DIR, (float)0},
	{TASK_PLAY_SEQUENCE, (float)ACT_RANGE_ATTACK2},
	{TASK_SET_SCHEDULE, (float)HB_SCHED_WAIT_FACE_ENEMY},
};
static Schedule_t slHBRangeattack2_a178[] = {{tlHBRangeattack2_a178, ARRAYSIZE(tlHBRangeattack2_a178),
	0,
	0,
	"RangeAttack2"}};

// Retail Schedule_t @0x1012a1a8
static Task_t tlHBRepel_a1a8[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_FACE_IDEAL, (float)0},
	{TASK_PLAY_SEQUENCE, (float)ACT_GLIDE},
};
static Schedule_t slHBRepel_a1a8[] = {{tlHBRepel_a1a8, ARRAYSIZE(tlHBRepel_a1a8),
	bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND,
	bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER,
	"Repel"}};

// Retail Schedule_t @0x1012a1d8
static Task_t tlHBRepelAttack_a1d8[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_PLAY_SEQUENCE, (float)ACT_FLY},
};
static Schedule_t slHBRepelAttack_a1d8[] = {{tlHBRepelAttack_a1d8, ARRAYSIZE(tlHBRepelAttack_a1d8),
	bits_COND_ENEMY_OCCLUDED,
	0,
	"Repel Attack"}};

// Retail Schedule_t @0x1012a220
static Task_t tlHBRepelLand_a220[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_PLAY_SEQUENCE, (float)ACT_LAND},
	{TASK_GET_PATH_TO_LASTPOSITION, (float)0},
	{TASK_RUN_PATH, (float)0},
	{TASK_WAIT_FOR_MOVEMENT, (float)0},
	{TASK_CLEAR_LASTPOSITION, (float)0},
};
static Schedule_t slHBRepelLand_a220[] = {{tlHBRepelLand_a220, ARRAYSIZE(tlHBRepelLand_a220),
	bits_COND_SEE_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND,
	bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER,
	"Repel Land"}};

// Retail Schedule_t @0x1012a258
static Task_t tlHBMachinegunRangeStand_a258[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
};
static Schedule_t slHBMachinegunRangeStand_a258[] = {{tlHBMachinegunRangeStand_a258, ARRAYSIZE(tlHBMachinegunRangeStand_a258),
	bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD | bits_COND_HB_NOFIRE,
	0,
	"Machinegun range stand"}};

// Retail Schedule_t @0x1012a298
static Task_t tlHBRangeMachinegunCrouch_a298[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCH},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FACE_ENEMY, (float)0},
};
static Schedule_t slHBRangeMachinegunCrouch_a298[] = {{tlHBRangeMachinegunCrouch_a298, ARRAYSIZE(tlHBRangeMachinegunCrouch_a298),
	bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD | bits_COND_HB_NOFIRE,
	0,
	"Range machinegun crouch"}};

// Retail Schedule_t @0x1012a2d0
static Task_t tlHBRocketRangeStand_a2d0[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FIND_COVER_FROM_ORIGIN, (float)0},
};
static Schedule_t slHBRocketRangeStand_a2d0[] = {{tlHBRocketRangeStand_a2d0, ARRAYSIZE(tlHBRocketRangeStand_a2d0),
	bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_HB_NOFIRE,
	0,
	"Rocket range stand"}};

// Retail Schedule_t @0x1012a310
static Task_t tlHBRocketlauncherCrouch_a310[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCH},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
	{TASK_FIND_COVER_FROM_ORIGIN, (float)0},
};
static Schedule_t slHBRocketlauncherCrouch_a310[] = {{tlHBRocketlauncherCrouch_a310, ARRAYSIZE(tlHBRocketlauncherCrouch_a310),
	bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD | bits_COND_HB_NOFIRE,
	0,
	"rocketlauncher crouch"}};

// Retail Schedule_t @0x1012a348
static Task_t tlHBShotgunRangeStand_a348[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_FACE_ENEMY, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
};
static Schedule_t slHBShotgunRangeStand_a348[] = {{tlHBShotgunRangeStand_a348, ARRAYSIZE(tlHBShotgunRangeStand_a348),
	bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_HB_NOFIRE,
	0,
	"Shotgun range stand"}};

// Retail Schedule_t @0x1012a380
static Task_t tlHBShotgunCrouch_a380[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCH},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
};
static Schedule_t slHBShotgunCrouch_a380[] = {{tlHBShotgunCrouch_a380, ARRAYSIZE(tlHBShotgunCrouch_a380),
	bits_COND_NO_AMMO_LOADED | bits_COND_ENEMY_OCCLUDED | bits_COND_HEAVY_DAMAGE | bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD | bits_COND_HB_NOFIRE,
	0,
	"shotgun crouch"}};

// Retail Schedule_t @0x1012a3b0
static Task_t tlHBBanditDraw_a3b0[] = {
	{TASK_STOP_MOVING, (float)0},
	{TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_ARM},
	{TASK_FACE_ENEMY, (float)0},
};
static Schedule_t slHBBanditDraw_a3b0[] = {{tlHBBanditDraw_a3b0, ARRAYSIZE(tlHBBanditDraw_a3b0),
	0,
	0,
	"Bandit Draw"}};

// Retail Schedule_t @0x1012a3e8
static Task_t tlHBBanditRollLeft_a3e8[] = {
	{TASK_WAIT, 0.2f},
	{TASK_HB_ROLL_LEFT, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
};
static Schedule_t slHBBanditRollLeft_a3e8[] = {{tlHBBanditRollLeft_a3e8, ARRAYSIZE(tlHBBanditRollLeft_a3e8),
	0,
	0,
	"Bandit Roll Left"}};

// Retail Schedule_t @0x1012a420
static Task_t tlHBBanditRollRight_a420[] = {
	{TASK_WAIT, 0.2f},
	{TASK_HB_ROLL_RIGHT, (float)0},
	{TASK_HB_CHECK_FIRE, (float)0},
	{TASK_RANGE_ATTACK1, (float)0},
};
static Schedule_t slHBBanditRollRight_a420[] = {{tlHBBanditRollRight_a420, ARRAYSIZE(tlHBBanditRollRight_a420),
	0,
	0,
	"Bandit Roll Right"}};

// Retail Schedule_t @0x1012a458
static Task_t tlHBBanditRollLeft_a458[] = {
	{TASK_SET_FAIL_SCHEDULE, (float)SCHED_RANGE_ATTACK1},
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_ROLL_LEFT, (float)0},
	{TASK_FACE_ENEMY, (float)0},
};
static Schedule_t slHBBanditRollLeft_a458[] = {{tlHBBanditRollLeft_a458, ARRAYSIZE(tlHBBanditRollLeft_a458),
	0,
	0,
	"Bandit Roll Left"}};

// Retail Schedule_t @0x1012a490
static Task_t tlHBBanditRollRight_a490[] = {
	{TASK_SET_FAIL_SCHEDULE, (float)SCHED_RANGE_ATTACK1},
	{TASK_STOP_MOVING, (float)0},
	{TASK_HB_ROLL_RIGHT, (float)0},
	{TASK_FACE_ENEMY, (float)0},
};
static Schedule_t slHBBanditRollRight_a490[] = {{tlHBBanditRollRight_a490, ARRAYSIZE(tlHBBanditRollRight_a490),
	0,
	0,
	"Bandit Roll Right"}};

// Retail ScheduleFromName @0x100aa910 searches these 31 human schedules, then falls back to the base list.
static Schedule_t* slHumanBaseSchedules[] = {
	slHBGruntFail_9c90,
	slHBGruntCombatFail_9cc8,
	slHBGruntvictorydance_9d20,
	slHBGruntestablishlineoffire_9d60,
	slHBGruntfoundenemy_9d90,
	slHBCombatFace_9dd0,
	slHBSignalsuppress_9e80,
	slHBSuppress_9f18,
	slHBGruntwaitincover_9f48,
	slHBTakecover_9fa8,
	slHBGrenadecover_a000,
	slHBTossgrenadecover_a030,
	slHBGrunttakecoverfrombestsound_a080,
	slHBGrunthidereload_a0d8,
	slHBGruntreload_a108,
	slHBGruntSweep_a140,
	slHBRangeattack2_a178,
	slHBRepel_a1a8,
	slHBRepelAttack_a1d8,
	slHBRepelLand_a220,
	slHBMachinegunRangeStand_a258,
	slHBRangeMachinegunCrouch_a298,
	slHBRocketRangeStand_a2d0,
	slHBRocketlauncherCrouch_a310,
	slHBShotgunRangeStand_a348,
	slHBShotgunCrouch_a380,
	slHBBanditDraw_a3b0,
	slHBBanditRollLeft_a3e8,
	slHBBanditRollRight_a420,
	slHBBanditRollLeft_a458,
	slHBBanditRollRight_a490,
};

Schedule_t* CHumanBase::ScheduleFromName(const char* pName)
{
	Schedule_t* pSchedule = ScheduleInList(pName, slHumanBaseSchedules, ARRAYSIZE(slHumanBaseSchedules));
	return pSchedule ? pSchedule : CBaseMonster::ScheduleFromName(pName);
}

// Retail Slot 111 @0x100a8770 returns 0x27: world, combat, player and danger sounds.
int CHumanBase::ISoundMask()
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER;
}


//=========================================================
// Saverestore (Retail speichert die Squad-Felder der Basis; hier die eigenen Zustandsfelder)
//=========================================================
TYPEDESCRIPTION CHumanBase::m_SaveData[] =
	{
		DEFINE_FIELD(CHumanBase, m_flNextGrenadeCheck, FIELD_TIME),
		DEFINE_FIELD(CHumanBase, m_flNextRollCheck, FIELD_TIME),
		DEFINE_FIELD(CHumanBase, m_vecTossVelocity, FIELD_VECTOR),
		DEFINE_FIELD(CHumanBase, m_fThrowGrenade, FIELD_BOOLEAN),
		DEFINE_FIELD(CHumanBase, m_fStanding, FIELD_BOOLEAN),
		DEFINE_FIELD(CHumanBase, m_fFirstEncounter, FIELD_BOOLEAN),
		DEFINE_FIELD(CHumanBase, m_cClipSize, FIELD_INTEGER),
		DEFINE_FIELD(CHumanBase, m_voicePitch, FIELD_INTEGER),
		DEFINE_FIELD(CHumanBase, m_iSentence, FIELD_INTEGER),
		DEFINE_FIELD(CHumanBase, m_iRollSide, FIELD_INTEGER),
};
IMPLEMENT_SAVERESTORE(CHumanBase, CSquadMonster);

//=========================================================
// Sprechen (Satztabelle @0x10129c40)
//=========================================================
static const char* const pHBSentences[] = {"HG_GREN", "HG_ALERT", "HG_MONST", "HG_COVER", "HG_THROW", "HG_CHARGE", "HG_TAUNT", "HG_IDLE", "HG_QUEST"};

bool CHumanBase::FOkToSpeak()
{
	if (gpGlobals->time <= s_flTalkWaitTime)
		return false;
	if ((pev->spawnflags & SF_MONSTER_GAG) != 0 && m_MonsterState != MONSTERSTATE_COMBAT)
		return false;
	return InSquad();
}

void CHumanBase::JustSpoke()
{
	s_flTalkWaitTime = RANDOM_FLOAT(2.0, 3.5) + gpGlobals->time;
	m_iSentence = HB_SENT_NONE;
}

void CHumanBase::SpeakSentence()
{
	if (m_iSentence == HB_SENT_NONE || m_iSentence >= (int)ARRAYSIZE(pHBSentences))
		return;
	if (FOkToSpeak())
	{
		SENTENCEG_PlayRndSz(ENT(pev), pHBSentences[m_iSentence], HB_SENTENCE_VOLUME, HB_SENTENCE_ATTN, 0, m_voicePitch);
		JustSpoke();
	}
}

void CHumanBase::Crouch()
{
	m_fStanding = false;
	pev->view_ofs = Vector(0, 0, 32);
}

//=========================================================
// StartTask  (Retail @0x100aa2f0)
//=========================================================
void CHumanBase::StartTask(Task_t* pTask)
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch (pTask->iTask)
	{
	case TASK_RUN_PATH:
	case TASK_WALK_PATH:
		// Wer sich bewegt, gilt nicht mehr als gedeckt.
		Forget(bits_MEMORY_INCOVER);
		CSquadMonster::StartTask(pTask);
		return;

	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		CSquadMonster::StartTask(pTask);
		if (pev->movetype == MOVETYPE_FLY)
			m_IdealActivity = ACT_GLIDE;
		return;

	case TASK_RELOAD:
		if (LookupActivity(ACT_RELOAD) != ACTIVITY_NOT_AVAILABLE)
		{
			m_IdealActivity = ACT_RELOAD;
			return;
		}
		if (!HasConditions(bits_COND_TASK_FAILED))
			TaskComplete();
		return;

	case TASK_PLAY_SEQUENCE:
	case TASK_PLAY_SEQUENCE_FACE_ENEMY:
	case TASK_PLAY_SEQUENCE_FACE_TARGET:
	{
		const Activity act = (Activity)(int)pTask->flData;
		if (LookupActivity(act) != ACTIVITY_NOT_AVAILABLE)
		{
			CSquadMonster::StartTask(pTask);
			return;
		}
		// Retail meldet fehlende Aktivitaeten und weicht aus, statt das Modell zu stoeren.
		switch (act)
		{
		case ACT_FLY:
		case ACT_LAND:
		case (Activity)0x11:
		case ACT_RELOAD:
		case ACT_ARM:
		case ACT_GLIDE:
		case (Activity)0x3c:
		case (Activity)0x41:
			ALERT(at_console, "no act: %d for: %s, defaulting\n", (int)act, STRING(pev->classname));
			if (!HasConditions(bits_COND_TASK_FAILED))
			{
				m_IdealActivity = act;
				TaskComplete();
				return;
			}
			break;
		case (Activity)0x2d:
		case (Activity)0x2e:
		case (Activity)0x2f:
		case (Activity)0x3e:
			ALERT(at_console, "no act: %d for: %s, defaulting\n", (int)act, STRING(pev->classname));
			m_IdealActivity = ACT_IDLE;
			return;
		case (Activity)0x30:
			ALERT(at_console, "no act: %d for: %s, defaulting\n", (int)act, STRING(pev->classname));
			m_IdealActivity = ACT_WALK;
			return;
		default:
			break;
		}
		m_IdealActivity = act;
		return;
	}

	case TASK_HB_IDLE_ACTIVITY: // Retail-Basisaufgabe 0x5f
		m_IdealActivity = ACT_IDLE;
		if (!HasConditions(bits_COND_TASK_FAILED))
			TaskComplete();
		return;

	case TASK_HB_FACE_TOSS_DIR:
		return; // laeuft in RunTask

	case TASK_HB_SPEAK_SENTENCE:
		SpeakSentence();
		if (!HasConditions(bits_COND_TASK_FAILED))
			TaskComplete();
		return;

	case TASK_HB_CHECK_FIRE:
		if (!NoFriendlyFire())
			SetConditions(bits_COND_HB_NOFIRE);
		if (!HasConditions(bits_COND_TASK_FAILED))
			TaskComplete();
		return;

	case TASK_HB_ROLL_RIGHT:
	case TASK_HB_ROLL_LEFT:
	{
		const bool bRight = pTask->iTask == TASK_HB_ROLL_RIGHT;
		UTIL_MakeVectors(pev->angles);
		const Vector vecDest = pev->origin + gpGlobals->v_right * (bRight ? 40.0f : -40.0f);
		if (CheckLocalMove(pev->origin, vecDest, nullptr, nullptr) == LOCALMOVE_VALID &&
			MoveToLocation(bRight ? ACT_ROLL_RIGHT : ACT_ROLL_LEFT, 0, vecDest))
			return;
		TaskFail();
		return;
	}

	default:
		CSquadMonster::StartTask(pTask);
		return;
	}
}

//=========================================================
// RunTask  (Retail @0x100aa6d0)
//=========================================================
void CHumanBase::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_RELOAD:
		if (m_fSequenceFinished)
		{
			m_cAmmoLoaded = m_cClipSize;
			if (!HasConditions(bits_COND_TASK_FAILED))
				TaskComplete();
		}
		return;

	case TASK_HB_ROLL_RIGHT:
	case TASK_HB_ROLL_LEFT:
	{
		const Vector vecGoal = m_Route[m_iRouteIndex].vecLocation;
		const float flDx = vecGoal.x - pev->origin.x;
		const float flDy = vecGoal.y - pev->origin.y;
		if (sqrt(flDx * flDx + flDy * flDy) < 1.0f || m_fSequenceFinished)
		{
			Crouch();
			RouteClear();
			if (!HasConditions(bits_COND_TASK_FAILED))
				TaskComplete();
		}
		return;
	}

	case TASK_HB_FACE_TOSS_DIR:
		// Einen Punkt entlang des Wurfvektors anpeilen und dorthin drehen.
		MakeIdealYaw(pev->origin + m_vecTossVelocity * 64);
		ChangeYaw(pev->yaw_speed);
		if (FacingIdeal())
			m_iTaskStatus = TASKSTATUS_COMPLETE;
		return;

	default:
		CSquadMonster::RunTask(pTask);
		return;
	}
}

//=========================================================
// GetScheduleOfType  (Retail @0x100ab400)
//=========================================================
Schedule_t* CHumanBase::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_COMBAT_FACE:
		return slHBCombatFace_9dd0;

	case SCHED_VICTORY_DANCE:
		if (InSquad() && !IsLeader())
			return slHBGruntFail_9c90;
		return slHBGruntvictorydance_9d20;

	case SCHED_TAKE_COVER_FROM_ENEMY:
		if (!InSquad())
		{
			if (RANDOM_LONG(0, 99) < 75 && CanFireRange2())
				return slHBGrenadecover_a000;
		}
		else if (g_iSkillLevel == SKILL_HARD && HasConditions(bits_COND_CAN_RANGE_ATTACK2) &&
				 OccupySlot(bits_SLOTS_HGRUNT_GRENADE) && CanFireRange2())
		{
			if (FOkToSpeak())
			{
				m_iSentence = HB_SENT_THROW;
				SpeakSentence();
			}
			return slHBTossgrenadecover_a030;
		}
		return slHBTakecover_9fa8;

	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		return slHBGrunttakecoverfrombestsound_a080;

	case SCHED_COWER:
		// Retail: gegen Spieler im Squad lieber schiessen als ducken.
		if (InSquad() && m_hEnemy != nullptr && FClassnameIs(m_hEnemy->pev, "player"))
			return GetScheduleOfType(SCHED_RANGE_ATTACK1);
		return CSquadMonster::GetScheduleOfType(Type);

	case SCHED_RANGE_ATTACK1:
		switch (pev->weapons)
		{
		case 1:
		case 0x40:
		case 0x80:
			return GetScheduleOfType(HB_SCHED_RANGE_MACHINEGUN);
		case 2:
		case 0x10:
			return GetScheduleOfType(HB_SCHED_RANGE_ROCKET);
		case 4:
			return GetScheduleOfType(HB_SCHED_RANGE_SHOTGUN);
		default:
			ALERT(at_console, "No firing for this type of weapon\n");
			return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ENEMY);
		}

	case SCHED_RANGE_ATTACK2:
		return slHBRangeattack2_a178;

	case SCHED_RELOAD:
		return slHBGruntreload_a108;

	case SCHED_FAIL:
		if (m_hEnemy != nullptr)
			return slHBGruntCombatFail_9cc8;
		return slHBGruntFail_9c90;

	case HB_SCHED_SUPPRESS:
		if (m_hEnemy != nullptr && m_hEnemy->IsPlayer() && m_fFirstEncounter)
		{
			m_fFirstEncounter = false; // nur beim ersten Zusammentreffen des Squads
			return slHBSignalsuppress_9e80;
		}
		return slHBSuppress_9f18;

	case HB_SCHED_ESTABLISH_LINE_OF_FIRE:
		return slHBGruntestablishlineoffire_9d60;

	case HB_SCHED_COVER_AND_RELOAD:
		return slHBGrunthidereload_a0d8;

	case HB_SCHED_SWEEP:
		return slHBGruntSweep_a140;

	case HB_SCHED_FOUND_ENEMY:
		return slHBGruntfoundenemy_9d90;

	case HB_SCHED_REPEL:
		if (pev->velocity.z > -128.0f)
			pev->velocity.z -= 32.0f;
		return slHBRepel_a1a8;

	case HB_SCHED_REPEL_ATTACK:
		if (pev->velocity.z > -128.0f)
			pev->velocity.z -= 32.0f;
		return slHBRepelAttack_a1d8;

	case HB_SCHED_REPEL_LAND:
		return slHBRepelLand_a220;

	case HB_SCHED_WAIT_FACE_ENEMY:
		return slHBGruntwaitincover_9f48;

	case HB_SCHED_TAKECOVER_FAILED:
		if (HasConditions(bits_COND_CAN_RANGE_ATTACK1) && !HasConditions(bits_COND_ENEMY_OCCLUDED) &&
			OccupySlot(bits_SLOTS_HGRUNT_ENGAGE))
			return GetScheduleOfType(SCHED_RANGE_ATTACK1);
		return GetScheduleOfType(SCHED_FAIL);

	case HB_SCHED_ELOF_FAIL:
		return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ENEMY);

	case HB_SCHED_RANGE_MACHINEGUN:
		return m_fStanding ? slHBMachinegunRangeStand_a258 : slHBRangeMachinegunCrouch_a298;

	case HB_SCHED_RANGE_ROCKET:
		return m_fStanding ? slHBRocketRangeStand_a2d0 : slHBRocketlauncherCrouch_a310;

	case HB_SCHED_RANGE_SHOTGUN:
		return m_fStanding ? slHBShotgunRangeStand_a348 : slHBShotgunCrouch_a380;

	case HB_SCHED_ROLL_LEFT:
		return slHBBanditRollLeft_a458;
	case HB_SCHED_ROLL_RIGHT:
		return slHBBanditRollRight_a490;
	case HB_SCHED_DRAW:
		return slHBBanditDraw_a3b0;

	default:
		return CSquadMonster::GetScheduleOfType(Type);
	}
}

//=========================================================
// GetSchedule  (Retail @0x100aa940)
//
// Die Ausweichrolle steht in GetDodgeRollSchedule (2026-11-01). Die "Wer bin ich"-Abfragen (Slots 0x234/0x238/0x240) sind als virtuelle
// Haken der Klasse abgebildet (CanFireRange2()).
//=========================================================
// Retail @0x100aa940, Zweig CAN_RANGE_ATTACK1: Ausweichrolle des Banditen (GetRollSide() != 0), Schedules
// "Bandit Roll Right/Left" (@0x1012a490 / @0x1012a458 beziehungsweise a3e8/a420 als Tabellen im SDK).
// (A) Feind verdeckt, im Squad, 50 %: rollt, wenn der Feind ungefaehr vor einem steht (Skalarprodukt <= -0,9).
// (B) Feind schaut zum Bandit, Wartezeit abgelaufen: rollt zur Seite, die den Winkel zum Feind besser verlaesst
// (Skalarprodukt <= -0,98), danach 6..18 s Pause; sonst naechster Versuch in 1 s.
Schedule_t* CHumanBase::GetDodgeRollSchedule()
{
	if (GetRollSide() == 0 || m_hEnemy == nullptr)
		return nullptr;

	UTIL_MakeVectors(pev->angles);
	const Vector vecRight = gpGlobals->v_right;
	const Vector vecRightPos = pev->origin + vecRight * 40.0f;
	const Vector vecLeftPos = pev->origin + vecRight * -40.0f;

	if (HasConditions(bits_COND_ENEMY_OCCLUDED) && InSquad() && RANDOM_LONG(0, 99) < 50)
	{
		const Vector vecAway = (pev->origin - m_hEnemy->pev->origin).Normalize();
		if (DotProduct(vecAway, gpGlobals->v_forward) <= -0.9f)
		{
			if (GetRollSide() == 1)
			{
				if (CheckLocalMove(pev->origin, vecRightPos, nullptr, nullptr) == LOCALMOVE_VALID)
					return GetScheduleOfType(HB_SCHED_ROLL_RIGHT);
			}
			else if (GetRollSide() == 2)
			{
				if (CheckLocalMove(pev->origin, vecLeftPos, nullptr, nullptr) == LOCALMOVE_VALID)
					return GetScheduleOfType(HB_SCHED_ROLL_LEFT);
			}
		}
	}

	if (m_flNextRollCheck < gpGlobals->time && HasConditions(bits_COND_ENEMY_FACING_ME))
	{
		const float flRight = DotProduct((vecRightPos - m_hEnemy->pev->origin).Normalize(), gpGlobals->v_forward);
		const float flLeft = DotProduct((vecLeftPos - m_hEnemy->pev->origin).Normalize(), gpGlobals->v_forward);

		if (flRight <= -0.98f && flRight < flLeft && CheckLocalMove(pev->origin, vecRightPos, nullptr, nullptr) == LOCALMOVE_VALID)
		{
			m_flNextRollCheck = gpGlobals->time + RANDOM_LONG(6, 18);
			return GetScheduleOfType(HB_SCHED_ROLL_RIGHT);
		}
		if (flLeft <= -0.98f && flLeft < flRight && CheckLocalMove(pev->origin, vecLeftPos, nullptr, nullptr) == LOCALMOVE_VALID)
		{
			m_flNextRollCheck = gpGlobals->time + RANDOM_LONG(6, 18);
			return GetScheduleOfType(HB_SCHED_ROLL_LEFT);
		}
		m_flNextRollCheck = gpGlobals->time + 1.0f;
	}
	return nullptr;
}

Schedule_t* CHumanBase::GetSchedule()
{
	m_iSentence = HB_SENT_NONE;

	// Abseilen: PRONE heisst Barnacle, sonst Rope.
	if (pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE)
	{
		if ((pev->flags & FL_ONGROUND) != 0)
		{
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType(HB_SCHED_REPEL_LAND);
		}
		return GetScheduleOfType(m_MonsterState != MONSTERSTATE_COMBAT ? HB_SCHED_REPEL : HB_SCHED_REPEL_ATTACK);
	}

	// Gefahrengeraeusche haben Vorrang.
	if (HasConditions(bits_COND_HEAR_SOUND))
	{
		CSound* pSound = PBestSound();
		if (pSound)
		{
			if ((pSound->m_iType & bits_SOUND_DANGER) != 0)
			{
				if (FOkToSpeak())
				{
					m_iSentence = HB_SENT_GREN;
					SpeakSentence();
				}
				return GetScheduleOfType(SCHED_TAKE_COVER_FROM_BEST_SOUND);
			}
			if (!HasConditions(bits_COND_SEE_ENEMY) && (pSound->m_iType & (bits_SOUND_COMBAT | bits_SOUND_PLAYER)) != 0)
				MakeIdealYaw(pSound->m_vecOrigin);
		}
	}

	if (m_MonsterState == MONSTERSTATE_COMBAT && !HasConditions(bits_COND_ENEMY_DEAD))
	{
		if (!HasConditions(bits_COND_NEW_ENEMY))
		{
			if (HasConditions(bits_COND_NO_AMMO_LOADED))
				return GetScheduleOfType(HB_SCHED_COVER_AND_RELOAD);

			if (!HasConditions(bits_COND_LIGHT_DAMAGE))
			{
				if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
					return GetScheduleOfType(SCHED_MELEE_ATTACK1);

				if (CanFireRange2() && HasConditions(bits_COND_CAN_RANGE_ATTACK2))
					return GetScheduleOfType(SCHED_RANGE_ATTACK2);

				if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
				{
					if (InSquad())
					{
						CSquadMonster* pLeader = MySquadLeader();
						if (pLeader->m_fEnemyEluded && !HasConditions(bits_COND_ENEMY_FACING_ME))
						{
							pLeader->m_fEnemyEluded = false;
							return GetScheduleOfType(HB_SCHED_FOUND_ENEMY);
						}
					}
					if (Schedule_t* pRoll = GetDodgeRollSchedule())
						return pRoll;
					if (OccupySlot(bits_SLOTS_HGRUNT_ENGAGE))
						return GetScheduleOfType(SCHED_RANGE_ATTACK1);
					if (HasConditions(bits_COND_CAN_RANGE_ATTACK2) && OccupySlot(bits_SLOTS_HGRUNT_GRENADE) && CanFireRange2())
						return GetScheduleOfType(SCHED_RANGE_ATTACK2);
					return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ENEMY);
				}

				if (HasConditions(bits_COND_ENEMY_OCCLUDED))
				{
					if (HasConditions(bits_COND_CAN_RANGE_ATTACK2) && OccupySlot(bits_SLOTS_HGRUNT_GRENADE))
					{
						if (FOkToSpeak())
						{
							m_iSentence = HB_SENT_THROW;
							SpeakSentence();
						}
						return GetScheduleOfType(SCHED_RANGE_ATTACK2);
					}
					if (OccupySlot(bits_SLOTS_HGRUNT_ENGAGE))
					{
						if (FOkToSpeak() && RANDOM_LONG(0, 99) < 50)
							m_iSentence = HB_SENT_CHARGE;
						return GetScheduleOfType(HB_SCHED_ESTABLISH_LINE_OF_FIRE);
					}
					if (FOkToSpeak() && RANDOM_LONG(0, 1))
					{
						m_iSentence = HB_SENT_TAUNT;
						SpeakSentence();
					}
					return GetScheduleOfType(SCHED_STANDOFF);
				}
			}
			else
			{
				// Erster Treffer: zusammenzucken; danach in 50 % der Faelle Deckung im Squad.
				if ((m_afMemory & bits_MEMORY_FLINCHED) == 0)
				{
					Remember(bits_MEMORY_FLINCHED);
					return GetScheduleOfType(SCHED_SMALL_FLINCH);
				}
				if (RANDOM_LONG(0, 99) < 50 && InSquad() && FOkToSpeak())
				{
					m_iSentence = HB_SENT_COVER;
					JustSpoke();
					return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ENEMY);
				}
			}
		}
		else if (InSquad())
		{
			// Neuer Feind: ein Squad-Mitglied sucht Deckung, der Anführer greift an.
			CSquadMonster* pLeader = MySquadLeader();
			pLeader->m_fEnemyEluded = false;
			if (pLeader != this)
				return GetScheduleOfType(SCHED_TAKE_COVER_FROM_ENEMY);

			if (FOkToSpeak())
			{
				if (m_hEnemy != nullptr && m_hEnemy->IsPlayer())
					m_iSentence = HB_SENT_ALERT;
				else if (m_hEnemy != nullptr && m_hEnemy->Classify() != CLASS_PLAYER_ALLY &&
						 m_hEnemy->Classify() != CLASS_PLAYER_BIOWEAPON && m_hEnemy->Classify() != CLASS_ALIEN_BIOWEAPON)
					m_iSentence = HB_SENT_MONSTER;
				SpeakSentence();
			}
			if (HasConditions(bits_COND_CAN_RANGE_ATTACK1) && pev->weapons != 2)
				return GetScheduleOfType(HB_SCHED_SUPPRESS);
			return GetScheduleOfType(HB_SCHED_ESTABLISH_LINE_OF_FIRE);
		}

		// Sieht den Feind, kann aber nicht schiessen: Schussposition suchen.
		if (HasConditions(bits_COND_SEE_ENEMY) && !HasConditions(bits_COND_CAN_RANGE_ATTACK1))
			return GetScheduleOfType(HB_SCHED_ESTABLISH_LINE_OF_FIRE);
	}

	return CSquadMonster::GetSchedule();
}

//=========================================================
// Stufe 4
//=========================================================

// Retail @0x100aa080
void CHumanBase::HumanInit()
{
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	m_flFieldOfView = 0.2f; // von der Profiltabelle (-0,7) ueberschrieben, wenn MonsterInit sie anwendet

	m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
	m_flNextPainTime = gpGlobals->time;
	m_iSentence = HB_SENT_NONE;
	m_fStanding = true;
	m_fFirstEncounter = true;
	m_iRollSide = 0;
	m_flNextRollCheck = gpGlobals->time;
	m_voicePitch = RANDOM_LONG(0, 1) == 0 ? 90 : 90 + RANDOM_LONG(0, 7);
	s_flTalkWaitTime = 0;
	m_flDistTooFar = 4096.0f;

	// Retail-Capability 0x7c8 (SQUAD, USE, HEAR, AUTO_DOORS, OPEN_DOORS, TURN_HEAD); die Profiltabelle ersetzt sie
	// in Retail durch 0x1af. Das SDK ergaenzt die Angriffsbits fuer sein CheckAttacks.
	m_afCapability = bits_CAP_SQUAD | bits_CAP_USE | bits_CAP_HEAR | bits_CAP_AUTO_DOORS | bits_CAP_OPEN_DOORS |
					 bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK1 | bits_CAP_MELEE_ATTACK1;
	if (CanFireRange2())
		m_afCapability |= bits_CAP_RANGE_ATTACK2;
	m_cAmmoLoaded = InfiniteAmmo() ? 1 : m_cClipSize;
}

// Retail-Slot 107 @0x100a90d0
void CHumanBase::CheckAmmo()
{
	if (!InfiniteAmmo() && m_cAmmoLoaded <= 0)
		SetConditions(bits_COND_NO_AMMO_LOADED);
}

// Retail-Slot 106: only ENEMY_TOOFAR gates the shared squad-position update.
bool CHumanBase::FCanCheckAttacks()
{
	return !HasConditions(bits_COND_ENEMY_TOOFAR);
}

// Retail-Slot 117 @0x100a8800: Squad-Anfuehrer merkt sich, wann der Feind zuletzt gesehen wurde.
void CHumanBase::PrescheduleThink()
{
	if (InSquad() && m_hEnemy != nullptr)
	{
		CSquadMonster* pLeader = MySquadLeader();
		if (HasConditions(bits_COND_SEE_ENEMY))
			pLeader->m_flLastEnemySightTime = gpGlobals->time;
		else if (gpGlobals->time - pLeader->m_flLastEnemySightTime > 5.0f)
			pLeader->m_fEnemyEluded = true;
	}
}

// Retail-Slot 135 @0x100a8f60: abwechselnd Frage und Antwort im Leerlauf.
void CHumanBase::IdleSound()
{
	if (!FOkToSpeak())
		return;

	const char* pszGroup = nullptr;
	if (s_iQuestionStage == 0)
	{
		if (RANDOM_LONG(0, 1) == 0)
			return;
		switch (RANDOM_LONG(0, 2))
		{
		case 0:
			pszGroup = "HG_CHECK";
			s_iQuestionStage = 1;
			break;
		case 1:
			pszGroup = "HG_QUEST";
			s_iQuestionStage = 2;
			break;
		default:
			pszGroup = "HG_IDLE";
			break;
		}
	}
	else if (s_iQuestionStage == 1)
	{
		pszGroup = "HG_CLEAR";
		s_iQuestionStage = 0;
	}
	else
	{
		pszGroup = "HG_ANSWER";
		s_iQuestionStage = 0;
	}
	SENTENCEG_PlayRndSz(ENT(pev), pszGroup, HB_SENTENCE_VOLUME, HB_SENTENCE_ATTN, 0, m_voicePitch);
	JustSpoke();
}

// Retail-Slot 10 @0x100a8d90: Treffer in Trefferzone 11 zaehlen als Kopftreffer.
void CHumanBase::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (ptr->iHitgroup == 11 || ptr->iHitgroup == HITGROUP_HEAD)
		ptr->iHitgroup = HITGROUP_HEAD;
	CSquadMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

// Retail Slot 118 @0x100a8de0: combat headshots detach a skull and one human gib,
// then continue with the shared death-activity selection. Retail's gib velocity,
// lifetime and spawn offsets are not reproduced by SpawnCustomGibs yet.
Activity CHumanBase::GetDeathActivity()
{
	int group = 0;
	int value = 0;
	if (m_LastHitGroup == HITGROUP_HEAD && m_MonsterState == MONSTERSTATE_COMBAT &&
		CVAR_GET_FLOAT("violence_hgibs") != 0.0f && GetHeadshotGibBodygroup(group, value))
	{
		SetBodygroup(group, value);
		CGib::SpawnCustomGibs(pev, 1, "models/humanskull.mdl", 0, 3.0f, true, 2);
		CGib::SpawnCustomGibs(pev, 1, "models/hgibs.mdl", 8, 1.0f, true, 2);
	}
	return CBaseMonster::GetDeathActivity();
}

// Retail-Slot 81 @0x100a8ae0: Handgranate (Waffenbit 8); Pruefung wie im Half-Life-Grunt, ohne Granatwerfer.
bool CHumanBase::CheckRangeAttack2(float flDot, float flDist)
{
	if ((pev->weapons & 8) == 0)
		return false;

	// Wer sich bewegt, wirft nicht.
	if (m_flGroundSpeed != 0)
	{
		m_fThrowGrenade = false;
		return false;
	}

	// Seit der letzten Pruefung hat sich wenig geaendert.
	if (gpGlobals->time < m_flNextGrenadeCheck)
		return m_fThrowGrenade;

	if (m_hEnemy == nullptr)
		return false;

	if ((m_hEnemy->pev->flags & FL_ONGROUND) == 0 && m_hEnemy->pev->waterlevel == 0 && m_vecEnemyLKP.z > pev->absmax.z)
	{
		// Wer in der Luft ist, wird nicht mit Granaten beworfen.
		m_fThrowGrenade = false;
		return false;
	}

	Vector vecTarget;
	if (RANDOM_LONG(0, 1) != 0)
		vecTarget = Vector(m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z); // Fuesse
	else
		vecTarget = m_vecEnemyLKP; // wo er zuletzt gesehen wurde

	// Keine Granate, wenn Squad-Mitglieder in der Naehe des Einschlags sind.
	if (InSquad() && SquadMemberInRange(vecTarget, 256))
	{
		m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
		m_fThrowGrenade = false;
	}

	if ((vecTarget - pev->origin).Length2D() <= 256)
	{
		m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
		m_fThrowGrenade = false;
		return false;
	}

	const Vector vecToss = VecCheckToss(pev, GetGunPosition(), vecTarget, 0.5f);
	if (vecToss != g_vecZero)
	{
		m_vecTossVelocity = vecToss;
		m_fThrowGrenade = true;
		m_flNextGrenadeCheck = gpGlobals->time;
	}
	else
	{
		m_fThrowGrenade = false;
		m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
	}
	return m_fThrowGrenade;
}

Vector CHumanBase::MuzzlePosition(int iMode)
{
	UTIL_MakeVectors(pev->angles);
	const float flSide = (pev->weapons == 0x80 || iMode == 0) ? 10.0f : -10.0f;
	return pev->origin + gpGlobals->v_right * flSide + gpGlobals->v_up * 54.0f;
}

bool CHumanBase::MuzzleHasLineOfFire(const Vector& vecSrc)
{
	TraceResult tr;
	UTIL_TraceLine(vecSrc, m_hEnemy->BodyTarget(vecSrc), ignore_monsters, ignore_glass, edict(), &tr);
	return tr.flFraction == 1.0f || tr.pHit == m_hEnemy->edict();
}

// Retail-Slot 80 @0x100b0b30 (Bandit): Stehend/Geduckt und Waffenmodus ergeben sich aus freien Sichtlinien der
// Muendungen. Die Muendungspositionen sind aus dem Dekompilat rekonstruiert (siehe MuzzlePosition).
bool CHumanBase::CheckRangeAttack1(float flDot, float flDist)
{
	if (!InfiniteAmmo() && HasConditions(bits_COND_NO_AMMO_LOADED))
		return false;
	if (flDist >= 4096.0f || flDot <= 0.1f)
		return false;
	if (!NoFriendlyFire())
		return false;
	if (m_hEnemy == nullptr)
		return false;

	bool bOneClear = false;
	TraceResult tr;

	if (CanCrouch() && !(flDist <= 512.0f && m_fStanding))
	{
		// Geduckt: Muendungen 20 Einheiten tiefer.
		Vector vecSrc = MuzzlePosition(0);
		vecSrc.z -= 20.0f;
		if (MuzzleHasLineOfFire(vecSrc))
		{
			m_iRollSide = 1;
			bOneClear = true;
		}
		if (pev->weapons != 0x80)
		{
			vecSrc = MuzzlePosition(1);
			vecSrc.z -= 20.0f;
			if (MuzzleHasLineOfFire(vecSrc))
			{
				m_iRollSide = 2;
				if (bOneClear)
				{
					m_fStanding = false;
					return true;
				}
			}
		}
	}

	// Stehend.
	m_fStanding = true;
	bOneClear = false;
	if (MuzzleHasLineOfFire(MuzzlePosition(0)))
	{
		m_iRollSide = 1;
		bOneClear = true;
	}
	if (pev->weapons == 0x80)
		return bOneClear;

	const Vector vecLeft = MuzzlePosition(1);
	UTIL_TraceLine(vecLeft, m_hEnemy->BodyTarget(vecLeft), ignore_monsters, ignore_glass, edict(), &tr);
	if (tr.flFraction != 1.0f && tr.pHit != m_hEnemy->edict())
	{
		if (!bOneClear)
			return false;
		pev->weapons = 1;
		return true;
	}

	m_iRollSide = 1;
	if (!bOneClear)
		return false;
	// Beide Seiten frei: meist einhaendig (0x40), selten beidhaendig (1).
	pev->weapons = (tr.flFraction <= 512.0f && RANDOM_LONG(0, 99) >= 10) ? 0x40 : 1;
	return true;
}

// Retail @0x100a9cb0 (Basis-Ereignisse): 2 = Granate werfen (Wurfgeschwindigkeit aus CheckRangeAttack2), 3 = Granate fallen
// lassen, 4 = Tritt. Das Retail-Dekompilat laeuft bei Ereignis 3 ohne break in den Tritt weiter (Original-Fehler, hier
// nachgebildet). Andere Ereignisse gehen an CBaseMonster. Die Nachbehandlung des Tritts (Slot 15 TraceBleed) ist vereinfacht.
void CHumanBase::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 2:
	{
		UTIL_MakeVectors(pev->angles);
		CGrenade::ShootTimed(pev, pev->origin + gpGlobals->v_forward * 34 + Vector(0, 0, 32), m_vecTossVelocity, 3.5f);
		m_fThrowGrenade = false;
		m_flNextGrenadeCheck = gpGlobals->time + 6;
		break;
	}
	case 3:
	case 4:
	{
		if (pEvent->event == 3)
		{
			UTIL_MakeVectors(pev->angles);
			CGrenade::ShootTimed(pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3);
		}

		// Retail @0x100a9110: Kurzspur 70 Einheiten vor den Fuessen
		UTIL_MakeVectors(pev->angles);
		TraceResult trKick;
		const Vector vecStart = Vector(pev->origin.x, pev->origin.y, pev->origin.z + pev->size.z * 0.5f);
		UTIL_TraceHull(vecStart, vecStart + gpGlobals->v_forward * 70, dont_ignore_monsters, head_hull, ENT(pev), &trKick);
		CBaseEntity* pHurt = CBaseEntity::Instance(trKick.pHit);
		if (pHurt != nullptr)
		{
			UTIL_MakeVectors(pev->angles);
			pHurt->pev->punchangle.x = 15;
			pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50;
			pHurt->TakeDamage(pev, pev, gSkillData.humanKickDamage, DMG_CLUB);

			CBaseMonster* pMonster = pHurt->MyMonsterPointer();
			if (pMonster != nullptr)
			{
				TraceResult trBleed;
				UTIL_TraceLine(GetGunPosition(), pMonster->BodyTarget(GetGunPosition()), dont_ignore_monsters, edict(), &trBleed);
				pMonster->TraceBleed(gSkillData.humanKickDamage, g_vecZero, &trBleed, DMG_SLASH);
				if (pMonster->IsAlive())
					EMIT_SOUND_DYN(pMonster->edict(), CHAN_VOICE, "debris/bustflesh1.wav", 0.4f, 2.0f, 0, 120);
			}
		}
		break;
	}
	default:
		CSquadMonster::HandleAnimEvent(pEvent);
		break;
	}
}
