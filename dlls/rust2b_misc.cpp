//=========================================================
// rust2b's remaining classes with no SDK precedent: ammo_dmlclip
// (CDMLClipAmmo), monster_maggot (CMaggot) and monster_tube (CTube).
// func_tanklaserrust is implemented in func_tank.cpp instead,
// alongside its stock CFuncTankLaser base. weapon_beamgun and
// weapon_minigun, rust2b's other two gaps, are already-deferred
// player weapons (see STATUS.md/memory).
//
// Decompiled fresh from gunman.dll this session:
//   ammo_dmlclip: LINK @0x10084900, vtable @0x100fb160, Spawn
//     @0x10084960, Precache @0x10084990 -> same shared
//     CBasePlayerAmmo tail as ammo_dmlsingle/ammo_beamgunclip. Model
//     "models/dmlammo.mdl" (the multi-round clip, vs.
//     ammo_dmlsingle's "models/singlerocket.mdl" single round),
//     pickup sound "items/9mmclip1.wav".
//   monster_maggot: LINK @0x100c39d0, vtable @0x101037e4, Spawn
//     @0x100c3ec0, Precache @0x100c3e90, Classify @0x100c4e70
//     (constant 0xd = 13 = CLASS_ALIEN_BIOWEAPON, same as
//     monster_critter). Confirmed bbox (-12,-12,0)/(12,12,8),
//     SOLID_SLIDEBOX, `CMaggot::SquashTouch` wired directly in Spawn
//     (named via the vtable-adjacent literal `param_1[5] =
//     CMaggot::SquashTouch`) - same squash-to-death pattern as
//     CCricket/CScorpion elsewhere in this project. Precache also
//     loads "models/larva.mdl" (matches
//     findings/entity_review_list.csv's "Zwei-Stadien-Kreatur"
//     description), confirming the two-stage larva/maggot life cycle,
//     though the stage-transition logic itself wasn't decompiled this
//     session (see Simplified section).
//   monster_tube: LINK @0x100c4e90, vtable @0x10103a20, Spawn
//     @0x100c5a90, Precache @0x100c5c70, Classify @0x100c6420
//     (constant 0xc = 12 = CLASS_PLAYER_BIOWEAPON). Confirmed bbox
//     (-32,-32,0)/(32,32,64), SOLID_SLIDEBOX. Confirms
//     findings/entities/xenome_family.md's "Queen-Kind"-flag finding
//     exactly: Spawn compares pev->owner's classname against the
//     literal string "monster_tubequeen" and, on a match, sets
//     pev->spawnflags bit 0x80000000 - reproduced faithfully here.
//
// Simplified relative to the original (documented per-case): CTube's actual
// homing-projectile-launcher attack behavior (per findings, "Tube
// Launcher" fires homing biological projectiles) was not decompiled in this
// pass. It relies on default CBaseMonster AI beyond the confirmed
// Spawn/Precache/Classify/owner-flag behavior.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

//=========================================================
// ammo_dmlclip - CDMLClipAmmo.
//=========================================================
class CDMLClipAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/dmlammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PrecacheModel("models/dmlammo.mdl");
		PrecacheSound("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		// Retail FUN_100849b0 mirrors the single pickup's companion-weapon
		// grant and fills the six-unit shared missiles pool in one pickup.
		if (pOther->IsPlayer())
		{
			auto* pPlayer = static_cast<CBasePlayer*>(pOther);
			if (!pPlayer->HasNamedPlayerItem("weapon_dmlGrenade"))
				pPlayer->GiveNamedItem("weapon_dmlGrenade");
		}

		if (pOther->GiveAmmo(6, "missiles", DML_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_dmlclip, CDMLClipAmmo);

//=========================================================
// monster_maggot - CMaggot.
//=========================================================
class CMaggot : public CBaseMonster
{
public:
	int MonsterProfile() override { return 21; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_BIOWEAPON; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
	void StartTask(Task_t* pTask) override;
	void EXPORT SquashTouch(CBaseEntity* pOther);
	void EXPORT LaunchedTouch(CBaseEntity* pOther);
	void EXPORT LeapTouch(CBaseEntity* pOther);
	void LaunchFromCritter(CBaseEntity* pEnemy, const Vector& vecVelocity);
	void StartLeap();
	void PlayAngrySound();
	void PlaySplatSound();

};
LINK_ENTITY_TO_CLASS(monster_maggot, CMaggot);

void CMaggot::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/maggot.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 8));

	pev->solid = SOLID_SLIDEBOX;
	pev->health = gSkillData.maggotHealth;
	pev->spawnflags |= 0x80000000; // Retail @0x100c3ec0
	pev->flags |= FL_MONSTER;

	MonsterInit();
		// Retail FUN_100c3ec0 assigns this after the virtual MonsterInit call.
	pev->view_ofs = Vector(0, 0, 20);
	// Retail sets capability bit 0x2000 (= bits_CAP_MELEE_ATTACK1) right here
	// (`or ah,0x20` on m_afCapability). maggot.mdl has no ACT_MELEE_ATTACK1, so
	// StartMonster would not set it; the leap's melee-task trigger needs it.
	m_afCapability |= bits_CAP_MELEE_ATTACK1;
	SetTouch(&CMaggot::SquashTouch);
}

void CMaggot::Precache()
{
	PrecacheModel("models/maggot.mdl");
	PrecacheModel("models/larva.mdl");
	PrecacheModel("sprites/greensplash.spr");

	PrecacheSound("maggot/maggot_angry1.wav");
	PrecacheSound("maggot/maggot_angry2.wav");
	PrecacheSound("maggot/maggot_flinch1.wav");
	PrecacheSound("maggot/maggot_flinch2.wav");
	PrecacheSound("maggot/maggot_idle1.wav");
	PrecacheSound("maggot/maggot_idle2.wav");
	PrecacheSound("maggot/maggot_idle3.wav");
	PrecacheSound("maggot/maggot_run1.wav");
	PrecacheSound("maggot/maggot_walk1.wav");
	PrecacheSound("debris/bustflesh2.wav");
}

void CMaggot::SquashTouch(CBaseEntity* pOther)
{
	if (pOther && pOther->IsPlayer() && FBitSet(pev->flags, FL_ONGROUND))
	{
		TakeDamage(pOther->pev, pOther->pev, 100, DMG_CRUSH);
	}
}

//---------------------------------------------------------------------
// Leap attack. Retail StartTask (vtable slot 85, 0x100c4af0) handles the
// two melee task ids 36/37 (TASK_MELEE_ATTACK1/2 in this SDK's enum) and
// passes every other task to the base. Verified from the decompile:
//   - activity ACT_LEAP (8); touch = LeapTouch; FL_ONGROUND cleared; origin
//     raised by 1; MakeVectors(angles).
//   - enemy present: target = enemy origin + enemy view_ofs (eye);
//     height = (eye z) - own origin z, minimum 8; gravity = sv_gravity, minimum
//     1; vz = sqrt(2 * g * h); flight time t = vz / g; horizontal velocity
//     = (target - origin).xy / t; vertical = vz; speed clamped to 350.
//   - m_flNextAttack (retail +0x224) = time + 2.0.
// LeapTouch (0x100c4960): if the touched entity takes damage and has another
// Classify() while the maggot is in the air, it takes sk_maggot_d (DMG_SLASH).
// Either way the touch falls back to SquashTouch if the classes differ.
// Simplified / open (see findings/entities/maggot_leap_2026-11-01.md):
//   - no-enemy branch: velocity = v_forward * 350 (gpGlobals component mapping
//     not unambiguous in the decompile);
//   - RANDOM_LONG(0,2) != 0 calls vtable slot 134 - not reproduced;
//   - the blood/trace effect branch of LeapTouch - not reproduced;
//   - the trigger (retail CheckMeleeAttack1 of the maggot) - not checked, so the
//     leap is not yet selected by any schedule in-game.
//---------------------------------------------------------------------
void CMaggot::StartTask(Task_t* pTask)
{
	if (pTask->iTask == TASK_MELEE_ATTACK1 || pTask->iTask == TASK_MELEE_ATTACK2)
	{
		StartLeap();
		return;
	}

	CBaseMonster::StartTask(pTask);
}

void CMaggot::StartLeap()
{
	m_iTaskStatus = TASKSTATUS_RUNNING;
	m_IdealActivity = ACT_LEAP;
	SetTouch(&CMaggot::LeapTouch);

	pev->flags &= ~FL_ONGROUND;
	UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
	UTIL_MakeVectors(pev->angles);

	CBaseEntity* pEnemy = m_hEnemy;
	if (!pEnemy)
	{
		// Retail no-enemy branch: velocity = (v_forward.x, v_forward.y, v_up.z) * 350
		// (component indices 10, 11, 15 of gpGlobals, as in the decompile).
		pev->velocity = Vector(gpGlobals->v_forward.x * 350.0f, gpGlobals->v_forward.y * 350.0f, gpGlobals->v_up.z * 350.0f);
	}
	else
	{
		float flGravity = CVAR_GET_FLOAT("sv_gravity");
		if (flGravity <= 1.0f)
			flGravity = 1.0f;

		float flHeight = (pEnemy->pev->view_ofs.z + pEnemy->pev->origin.z) - pev->origin.z;
		if (flHeight < 8.0f)
			flHeight = 8.0f;

		const float vzLaunch = sqrtf(2.0f * flGravity * flHeight);
		const float flFlightTime = vzLaunch / flGravity;

		const Vector vecTarget = pEnemy->pev->origin + pEnemy->pev->view_ofs;
		const Vector vecDelta = vecTarget - pev->origin;
		Vector vecVelocity(vecDelta.x / flFlightTime, vecDelta.y / flFlightTime, vzLaunch);

		const float flSpeed = vecVelocity.Length();
		if (flSpeed > 350.0f)
			vecVelocity = vecVelocity * (350.0f / flSpeed);

		pev->velocity = vecVelocity;
	}

	// Retail: RANDOM_LONG(0,2) != 0 -> angry call (vtable slot 134).
	if (RANDOM_LONG(0, 2) != 0)
		PlayAngrySound();

	// Retail +0x224 is CBaseMonster::m_flNextAttack (save table entry at 0x224, verified).
	m_flNextAttack = gpGlobals->time + 2.0f;
}

void CMaggot::PlayAngrySound()
{
	// Retail slot 134 (0x100c3b90): RANDOM_LONG(0,1) picks angry1 or angry2 on CHAN_VOICE, volume 1.0, pitch 100.
	const char* pszSound = (RANDOM_LONG(0, 1) == 0) ? "maggot/maggot_angry1.wav" : "maggot/maggot_angry2.wav";
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pszSound, 1.0f, 2.0f, 0, 100);
}

void CMaggot::PlaySplatSound()
{
	// Retail slot 137 (0x100c3d10): debris/bustflesh2.wav on CHAN_VOICE, volume 0.4, pitch 120.
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustflesh2.wav", 0.4f, 2.0f, 0, 120);
}

void CMaggot::LeapTouch(CBaseEntity* pOther)
{
	if (pOther && pOther->pev->takedamage != DAMAGE_NO && pOther->Classify() != Classify())
	{
		if (!FBitSet(pev->flags, FL_ONGROUND))
		{
			pOther->TakeDamage(pev, pev, gSkillData.maggotDamage, DMG_SLASH);
			// Retail: the effect branch runs only if victim vtable slot 17 returns a
			// non-null object (not identified by name); MyMonsterPointer() is used as
			// the SDK equivalent (open, see maggot_leap_2026-11-01.md).
			CBaseMonster* pVictim = pOther->MyMonsterPointer();
			if (pVictim)
			{
				// Retail effect path: BodyTarget (vtable slot 54) of victim and maggot,
				// TraceLine between them, normalized direction (helpers 0x1000e400 /
				// 0x1000e440), then the victim's TraceBleed (vtable slot 15, blood stream).
				// Assumed: the BodyTarget source argument is the maggot's origin and the
				// trace runs body-target to body-target.
				const Vector vecVictim = pOther->BodyTarget(pev->origin);
				const Vector vecSelf = BodyTarget(pOther->pev->origin);
				TraceResult tr;
				UTIL_TraceLine(vecSelf, vecVictim, dont_ignore_monsters, ENT(pev), &tr);
				const Vector vecDir = (vecVictim - vecSelf).Normalize();
				pVictim->TraceBleed(gSkillData.maggotDamage, vecDir, &tr, DMG_SLASH);

				if (pVictim->IsAlive())
					PlaySplatSound();
			}
		}
		SetTouch(&CMaggot::SquashTouch);
	}
}

void CMaggot::LaunchFromCritter(CBaseEntity* pEnemy, const Vector& vecVelocity)
{
	// Retail CCritter event 2 creates monster_maggot and immediately switches
	// it to this separate larva projectile state (FUN_100c45e0).
	SET_MODEL(ENT(pev), "models/larva.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 8));
	pev->movetype = MOVETYPE_FLY;
	pev->gravity = 1.0f;
	pev->velocity = vecVelocity;
	m_hEnemy = pEnemy;
	SetTouch(&CMaggot::LaunchedTouch);
}

void CMaggot::LaunchedTouch(CBaseEntity* pOther)
{
	// Retail ignores its source Critter and removes the larva after the first
	// other contact. The original uses a more involved trace/effect branch;
	// damage, collision scope and one-hit lifetime are the confirmed gameplay
	// contract reproduced here.
	if (pOther && !FClassnameIs(pOther->pev, "monster_critter"))
	{
		if (pOther->pev->takedamage != DAMAGE_NO)
			pOther->TakeDamage(pev, pev, gSkillData.critterSpitDamage, DMG_ACID);
		UTIL_Remove(this);
	}
}

CBaseEntity* CreateCritterLarva(const Vector& vecOrigin, const Vector& vecVelocity,
	edict_t* pOwner, CBaseEntity* pEnemy)
{
	CBaseEntity* pEntity = CBaseEntity::Create("monster_maggot", vecOrigin, g_vecZero, pOwner);
	if (pEntity)
		static_cast<CMaggot*>(pEntity)->LaunchFromCritter(pEnemy, vecVelocity);
	return pEntity;
}

//=========================================================
// monster_tube - CTube.
//=========================================================
class CTube : public CBaseMonster
{
public:
	int MonsterProfile() override { return 18; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int Classify() override { return CLASS_PLAYER_BIOWEAPON; }
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this - see the
	// feedback-setactivity-act-idle-stomps-scripted-sequence memory
	// note (same fix as CFriendlyGunman etc).
	void SetActivity(Activity NewActivity) override
	{
		if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && m_pCine != nullptr)
		{
			m_Activity = NewActivity;
			m_IdealActivity = NewActivity;
			return;
		}
		CBaseMonster::SetActivity(NewActivity);
	}

private:
	void LaunchRocket();
};
LINK_ENTITY_TO_CLASS(monster_tube, CTube);

void CTube::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/tube.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 64));

	pev->solid = SOLID_SLIDEBOX;
	pev->health = gSkillData.tubeHealth;
	pev->flags |= FL_MONSTER;

	// Confirmed: a tube spawned by a monster_tubequeen owner gets a
	// "queen-child" spawnflag bit set, matching
	// findings/entities/xenome_family.md's Tube-Launcher/Queen
	// relationship finding exactly.
	if (pev->owner && FClassnameIs(pev->owner, "monster_tubequeen"))
		pev->spawnflags |= 0x80000000;

	MonsterInit();
	// Retail (Capability-Schreibzugriff +0x220 nach MonsterInit): gleiche Bitoperation wie im Dekompilat.
	m_afCapability |= bits_CAP_MELEE_ATTACK1;
	// Retail FUN_100c5a90 assigns this after the virtual MonsterInit call.
	pev->view_ofs = Vector(0, 0, 40);
}

void CTube::Precache()
{
	PrecacheModel("models/tube.mdl");
	PrecacheModel("sprites/gibtube.spr");
	PrecacheModel("sprites/goretube.spr");
	PrecacheModel("sprites/tubeguts.spr");
	// CTube's retail firing helper creates tuberocket at runtime. It must
	// be precached during map load, never from an animation/task callback.
	UTIL_PrecacheOther("tuberocket");
	PrecacheSound("tube/tube_mourn1.wav");
	PrecacheSound("tube/tube_mourn2.wav");
	PrecacheSound("tube/tube_mourn3.wav");
}

void CTube::LaunchRocket()
{
	// Retail CTube::FireRocket @ 0x100c54e0 first creates a tuberocket
	// from origin + (0, 0, 48), then the animation callback places it at
	// attachment 0.  The initial speed literal is 500 units/s.
	const Vector vecAimOrigin = pev->origin + Vector(0, 0, 48);
	const Vector vecDirection = (m_vecEnemyLKP - vecAimOrigin).Normalize();
	const Vector vecAngles = UTIL_VecToAngles(vecDirection);

	CBaseEntity* pRocket = CBaseEntity::Create("tuberocket", vecAimOrigin, vecAngles, edict());
	if (!pRocket)
		return;

	Vector vecAttachment;
	Vector vecAttachmentAngles;
	GetAttachment(0, vecAttachment, vecAttachmentAngles);
	UTIL_SetOrigin(pRocket->pev, vecAttachment);
	pRocket->pev->velocity = vecDirection * 500.0f;
	pRocket->pev->owner = edict();
}

void CTube::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
	case 2:
	case 3:
		// All three retail fire events use the same attachment/factory path.
		LaunchRocket();
		return;
	case 4:
	{
		const char* const mournSounds[] = {
			"tube/tube_mourn1.wav",
			"tube/tube_mourn2.wav",
			"tube/tube_mourn3.wav",
		};
		EMIT_SOUND(ENT(pev), CHAN_BODY, mournSounds[RANDOM_LONG(0, ARRAYSIZE(mournSounds) - 1)], 1.0f, 0.8f);
		return;
	}
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		return;
	}
}
