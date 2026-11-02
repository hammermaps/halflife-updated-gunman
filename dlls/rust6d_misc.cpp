//=========================================================
// rust6d's monster_tubequeen (CTubeQueen, the "Xenome Birther Queen"
// boss). weapon_minigun, rust6c/rust6d's other gap, is an
// already-deferred player weapon.
//
// Decompiled fresh from gunman.dll this session: LINK @0x100c6fb0,
// vtable @0x10103e78, Spawn @0x100c7a90, Precache @0x100c7bd0,
// Classify @0x100c72d0 (constant 0xc = 12 = CLASS_PLAYER_BIOWEAPON,
// same as monster_tube). Confirmed a genuine skill-scaled health
// table (health 800/1200/900 for skill 1/2/3, read from the skill
// cvar - matches this project's g_iSkillLevel), a small bbox
// (-12,-12,0)/(12,12,24) despite being a boss (consistent with
// findings/entities/xenome_family.md's description of a
// wall-embedded creature - MOVETYPE_NONE, SOLID_BBOX, confirmed here
// too).
//
// CORRECTION (2026-09-05, see findings/entities/tubequeen_boss.md and
// findings/open_items_audit_2026-09-05.md): this file previously
// stubbed the entire boss to a no-op Use(); the audit's own listed
// "Byte-Detailfragen"-only open item was itself stale for this class -
// tubequeen_boss.md (Session 52) already fully decompiled and
// documented the boss's real combat, so it is reproduced here:
//
//   - CTubeQueen::UseQueen (0x100c6e80): three map trigger_multiple
//     volumes (clawcenter/clawleft/clawright, all target=tubequeen,
//     confirmed via rust6d.ent) dispatch three claw-swing sounds/
//     animations. Reproduced via pCaller->pev->targetname dispatch +
//     an immediate skill-agnostic melee TakeDamage check (the
//     original's exact damage-application point inside the animation
//     wasn't found by the findings doc either - only sounds were
//     confirmed per anim event - so this uses the same
//     distance-check+TakeDamage simplification already established for
//     CBanditGrunt::Melee()).
//   - HandleAnimEvent (0x100c7310): events 9/0x14/10 are the confirmed
//     "hand into hole" grab sequence (bodygroup 1 = 1 -> grab sound
//     chain -> spawn monster_tube at a fixed (563,147,-580) offset ->
//     bodygroup 1 = 0); event 1 fires UTIL_ScreenShake(amp 7, freq 0.5,
//     radius 2400); events 0x11/0x12/0x13 are the confirmed claw-hit
//     sounds, reproduced here directly from UseQueen since this file
//     doesn't reproduce a custom Schedule_t/Task_t sequence player
//     (see Simplified section).
//   - Ranged mortar attack (CTubeQueen's own FUN_100c7fc0 + CTubeMortar/
//     tuberocket, per tubequeen_boss.md and xenome_family.md: model
//     models/tubemortar.mdl, sounds tq_mortarhit1/2.wav, skill-scaled
//     damage/radius 80/70, 140/90, 180/100 for skill 1/2/3). On impact,
//     a non-submunition tuberocket scatters RANDOM_LONG(8,9) further
//     tuberocket submissiles (exact original RANDOM_LONG bounds not
//     resolved to a literal by the findings doc either - approximated
//     to match the user's own observed 8-9 range).
//   - Retreat: TakeDamage triggers a one-time ClimbAway reaction once
//     health drops to 1/3 of max_health (the findings doc confirms the
//     threshold-compare/GetSchedule/StartTask(ClimbAway) chain exists
//     but not the exact fraction constant - approximated as 1/3) -
//     plays tq_climbaway.wav plus a stronger UTIL_ScreenShake (amp 7,
//     freq 8, radius 3600) and permanently stops further attacks.
//
// Simplified relative to the original: no custom Schedule_t/Task_t AI
// (same established simplification as every other monster in this
// project) - the claw/grab/mortar attack cycle here runs from a
// straightforward MonsterThink()-driven cooldown timer instead of the
// original's animation-sequence-driven state machine (idle1/idle2 gating,
// exact clawattack/clawattackleft/clawattackright/climbaway sequence
// playback, the anti-kiting 6x-repeat RadiusDamage failsafe, and the
// "blutiger werdender Bauch" skin/bodygroup escalation) are not
// reproduced. The mortar's real parabolic MortarThink and lead-
// calculating aim-prediction are approximated with a single computed
// launch velocity (constant-gravity toss to the target's current
// position) instead of a per-tick trajectory/prediction recompute.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "skill.h"
#include "weapons.h"
#include "effects.h"

//=========================================================
// tuberocket - CTubeRocket.  Retail has a distinct constructor for this
// projectile (0x100c8390) and separate StartTrack/TrackTarget/TrackTouch
// methods.  It must not share CTubeMortar's classname.
//=========================================================
class CTubeRocket : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT TrackTouch(CBaseEntity* pOther);
	void DieTouch(CBaseEntity* pOther);
	void TrackTarget();
	void SetLifetime(float flSeconds);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_flExpireTime = 0.0f;
};
LINK_ENTITY_TO_CLASS(tuberocket, CTubeRocket);

TYPEDESCRIPTION CTubeRocket::m_SaveData[] =
	{
		DEFINE_FIELD(CTubeRocket, m_flExpireTime, FIELD_TIME),
	};

IMPLEMENT_SAVERESTORE(CTubeRocket, CBaseEntity);

void CTubeRocket::Precache()
{
	PrecacheModel("models/tubeprojectile.mdl");
	PrecacheModel("sprites/gsplat.spr");
	PrecacheSound("tube/TubeCritter_Hit1.wav");
	PrecacheSound("tube/TubeCritter_Hit3.wav");
}

void CTubeRocket::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/tubeprojectile.mdl");
	UTIL_SetSize(pev, Vector(-2, -2, -2), Vector(2, 2, 2));
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->health = 1;
	pev->takedamage = DAMAGE_NO;
	pev->dmg = 8;
	m_flExpireTime = gpGlobals->time + 10.0f;
	SetTouch(&CTubeRocket::TrackTouch);
	SetThink(&CTubeRocket::TrackTarget);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CTubeRocket::SetLifetime(float flSeconds)
{
	// Used by decore_sittingtubemortar's gibs (retail 0x100cd8d0 writes +0x224).
	m_flExpireTime = gpGlobals->time + flSeconds;
}

// Set the lifetime of a tuberocket created elsewhere (checks the classname
// before the cast, so the class is not confused with another entity).
void TubeRocketSetLifetime(CBaseEntity* pEnt, float flSeconds)
{
	if (pEnt != nullptr && FStrEq(STRING(pEnt->pev->classname), "tuberocket"))
		static_cast<CTubeRocket*>(pEnt)->SetLifetime(flSeconds);
}

void CTubeRocket::TrackTarget()
{
	// Retail CTubeRocket actively steers and removes itself at bounds/lifetime
	// limits.  Keep its velocity intact when no target has been assigned by a
	// launcher (for the Queen's scattered submunitions).
	if (pev->owner)
	{
		CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
		if (pOwner && pOwner->MyMonsterPointer() && pOwner->MyMonsterPointer()->m_hEnemy)
		{
			Vector direction = (pOwner->MyMonsterPointer()->m_hEnemy->Center() - pev->origin).Normalize();
			pev->velocity = direction * 300.0f;
		}
	}

	if (gpGlobals->time >= m_flExpireTime || pev->origin.x < -8192 || pev->origin.x > 8192 || pev->origin.y < -8192 ||
		pev->origin.z < -8192 || pev->origin.z > 8192)
	{
		UTIL_Remove(this);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
}

void CTubeRocket::TrackTouch(CBaseEntity* pOther)
{
	// Retail only reacts to a different classification; the owner filter is
	// the safe equivalent for the SDK's currently untyped target relation.
	if (!pOther || pOther == CBaseEntity::Instance(pev->owner))
		return;

	if (pOther->pev->takedamage != DAMAGE_NO)
		pOther->TakeDamage(pev, pev, pev->dmg, DMG_SLASH);

	DieTouch(pOther);
}

void CTubeRocket::DieTouch(CBaseEntity* pOther)
{
	// Retail DieTouch @ 0x100c92f0 creates this one-shot impact sprite.
	CSprite* pImpact = CSprite::SpriteCreate("sprites/gsplat.spr", pev->origin, true);
	if (pImpact)
	{
		pImpact->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
		pImpact->SetScale(10.0f);
		pImpact->AnimateAndDie(10.0f);
	}

	if (pOther && pOther->IsPlayer())
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tube/TubeCritter_Hit3.wav", 1.0f, ATTN_NORM);
	else
		EMIT_SOUND(ENT(pev), CHAN_WEAPON,
			"tube/TubeCritter_Hit1.wav", 0.7f, ATTN_NORM);

	pev->velocity = g_vecZero;
	pev->gravity = 0.0f;
	SetTouch(NULL);
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1f;
}

//=========================================================
// tubequeen_mortar - CTubeMortar. The Queen's arcing shell; its impact
// scatters the separate CTubeRocket projectile above.
//=========================================================
class CTubeMortar : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT MortarTouch(CBaseEntity* pOther);
	void Configure(bool bSubmunition, float flDamage, float flRadius);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	bool m_bSubmunition = false;
	float m_flDamage = 40.0f;
	float m_flRadius = 100.0f;
	int m_iTrailSprite = 0;
};
LINK_ENTITY_TO_CLASS(tubequeen_mortar, CTubeMortar);

TYPEDESCRIPTION CTubeMortar::m_SaveData[] =
	{
		DEFINE_FIELD(CTubeMortar, m_bSubmunition, FIELD_BOOLEAN),
		DEFINE_FIELD(CTubeMortar, m_flDamage, FIELD_FLOAT),
		DEFINE_FIELD(CTubeMortar, m_flRadius, FIELD_FLOAT),
	};

bool CTubeMortar::Save(CSave& save)
{
	if (!CBaseEntity::Save(save))
		return false;

	return save.WriteFields("CTubeMortar", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

bool CTubeMortar::Restore(CRestore& restore)
{
	if (!CBaseEntity::Restore(restore))
		return false;

	const bool status = restore.ReadFields("CTubeMortar", this, m_SaveData, ARRAYSIZE(m_SaveData));
	if (status)
	{
		// TE_BEAMFOLLOW is client-side only, so restore the visual trail after a load.
		UTIL_RocketTrail(this, MODEL_INDEX("sprites/smoke.spr"), false);
	}

	return status;
}

void CTubeMortar::Precache()
{
	PrecacheModel("models/tubemortar.mdl");
	PrecacheSound("tubequeen/tq_mortarhit1.wav");
	PrecacheSound("tubequeen/tq_mortarhit2.wav");
	m_iTrailSprite = PrecacheModel("sprites/smoke.spr");
}

void CTubeMortar::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("tubequeen_mortar");
	SET_MODEL(ENT(pev), "models/tubemortar.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 1.0;

	// Nachtrag 2026-09-06 (Nutzer-Meldung "Schweif fehlt auch bei
	// Moerser-Granaten"): rein ballistisch (kein Triebwerk) - Schweif
	// ohne Treibstoff-Lichteffekt, siehe UTIL_RocketTrail (util.cpp).
	UTIL_RocketTrail(this, m_iTrailSprite, false);

	SetTouch(&CTubeMortar::MortarTouch);
	pev->nextthink = gpGlobals->time + 6.0;
	SetThink(&CBaseEntity::SUB_Remove);
}

void CTubeMortar::Configure(bool bSubmunition, float flDamage, float flRadius)
{
	m_bSubmunition = bSubmunition;
	m_flDamage = flDamage;
	m_flRadius = flRadius;

	if (bSubmunition)
		pev->scale = 0.5f; // visually smaller sub-munition, same model
}

void CTubeMortar::MortarTouch(CBaseEntity* pOther)
{
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, RANDOM_LONG(0, 1) == 0 ? "tubequeen/tq_mortarhit1.wav" : "tubequeen/tq_mortarhit2.wav", 1.0, ATTN_NORM);

	RadiusDamage(pev->origin, pev, VARS(pev->owner), m_flDamage, m_flRadius, CLASS_NONE, DMG_BLAST);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(m_bSubmunition ? 10 : 25);
	WRITE_BYTE(10);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects).
	UTIL_ExplosionEffects(pev->origin, m_flRadius);

	if (!m_bSubmunition)
	{
		// RANDOM_LONG bounds approximated as 8-9, see file header.
		int iCount = RANDOM_LONG(8, 9);
		for (int i = 0; i < iCount; i++)
		{
			CBaseEntity* pSub = CBaseEntity::Create("tuberocket", pev->origin, g_vecZero, edict());
			if (pSub)
			{
				pSub->pev->velocity = Vector(RANDOM_FLOAT(-200, 200), RANDOM_FLOAT(-200, 200), RANDOM_FLOAT(100, 300));
				pSub->pev->owner = pev->owner;
			}
		}
	}

	UTIL_Remove(this);
}

//=========================================================
// monster_tubequeen - CTubeQueen.
//=========================================================
#define TQ_AE_SHAKE 1
#define TQ_AE_GRAB_HANDIN 9
#define TQ_AE_GRAB_SPAWNTUBE 10
#define TQ_AE_INSPECTBELLY 0x10
#define TQ_AE_CLAW_CENTER_HIT 0x11
#define TQ_AE_CLAW_LEFT_HIT 0x12
#define TQ_AE_CLAW_RIGHT_HIT 0x13
#define TQ_AE_GRAB_SOUNDCHAIN 0x14

class CTubeQueen : public CBaseMonster
{
public:
	int MonsterProfile() override { return 24; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_PLAYER_BIOWEAPON; }
	// Retail ObjectCaps (slot 5, @0x100c7000) returns 130 = ACROSS_TRANSITION | FORCE_TRANSITION.
	int ObjectCaps() override { return FCAP_ACROSS_TRANSITION | FCAP_FORCE_TRANSITION; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md. CTubeQueen doesn't walk
	// (MOVETYPE_NONE), but the override is harmless and kept for
	// consistency with the project-wide fix.
	void SetYawSpeed() override { pev->yaw_speed = 90; }
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
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void MonsterThink() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	void SpawnTube();
	void ClawDamage();
	void FireMortar(CBaseEntity* pTarget);

	bool m_bRetreated = false;
	float m_flGrabSpawnTime = 0;
	float m_flNextMortar = 0;
	float m_flNextGrab = 0;
};
LINK_ENTITY_TO_CLASS(monster_tubequeen, CTubeQueen);

TYPEDESCRIPTION CTubeQueen::m_SaveData[] =
	{
		DEFINE_FIELD(CTubeQueen, m_bRetreated, FIELD_BOOLEAN),
		DEFINE_FIELD(CTubeQueen, m_flGrabSpawnTime, FIELD_TIME),
		DEFINE_FIELD(CTubeQueen, m_flNextMortar, FIELD_TIME),
		DEFINE_FIELD(CTubeQueen, m_flNextGrab, FIELD_TIME),
	};

IMPLEMENT_SAVERESTORE(CTubeQueen, CBaseMonster);

void CTubeQueen::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/tubequeen.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_YES;

	switch (g_iSkillLevel)
	{
	case SKILL_MEDIUM:
		pev->health = 1200;
		break;
	case SKILL_HARD:
		pev->health = 900;
		break;
	default:
		pev->health = 800;
		break;
	}
	pev->max_health = pev->health;
	m_bloodColor = 247;		// Retail @0x100c7a90
	m_flFieldOfView = 0.0f; // Retail: param_1[0x49] = 0
	UTIL_SetOrigin(pev, pev->origin);

	MonsterInit();
	// Retail (Capability-Schreibzugriff +0x220 nach MonsterInit): gleiche Bitoperation wie im Dekompilat.
	m_afCapability |= bits_CAP_RANGE_ATTACK1;
}

void CTubeQueen::Precache()
{
	PrecacheModel("models/tubequeen.mdl");

	PrecacheSound("tubequeen/tq_bigflinch.wav");
	PrecacheSound("tubequeen/tq_bigflinch2.wav");
	PrecacheSound("tubequeen/tq_clawattack.wav");
	PrecacheSound("tubequeen/tq_clawattack_left.wav");
	PrecacheSound("tubequeen/tq_clawattack_right.wav");
	PrecacheSound("tubequeen/tq_climbaway.wav");
	PrecacheSound("tubequeen/tq_flinchviolent.wav");
	PrecacheSound("tubequeen/tq_grabtube1.wav");
	PrecacheSound("tubequeen/tq_grabtube2.wav");
	PrecacheSound("tubequeen/tq_grabtube3.wav");
	PrecacheSound("tubequeen/tq_idle1.wav");
	PrecacheSound("tubequeen/tq_idle1a.wav");
	PrecacheSound("tubequeen/tq_idle1b.wav");
	PrecacheSound("tubequeen/tq_idle2.wav");
	PrecacheSound("tubequeen/tq_inspectbelly.wav");
	PrecacheSound("tubequeen/tq_smallflinch.wav");
	PrecacheSound("tubequeen/tq_smallflinch2.wav");
	PrecacheSound("tubequeen/tq_mortarfire1.wav");
	PrecacheSound("tubequeen/tq_mortarfire2.wav");
	PrecacheSound("tubequeen/tq_mortarfire3.wav");
	PrecacheModel("sprites/gorebirth.spr");
	PrecacheModel("sprites/gibbirth.spr");

	UTIL_PrecacheOther("monster_tube");
	UTIL_PrecacheOther("tubequeen_mortar");
	UTIL_PrecacheOther("tuberocket");
}

void CTubeQueen::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (m_bRetreated || pev->deadflag != DEAD_NO)
		return;

	// Confirmed via findings/entities/tubequeen_boss.md: the three
	// trigger_multiple volumes in front of the boss are named
	// clawcenter/clawleft/clawright and all target=tubequeen.
	const char* callerName = pCaller ? STRING(pCaller->pev->targetname) : "";

	if (FStrEq(callerName, "clawleft"))
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tubequeen/tq_clawattack_left.wav", 1.0, ATTN_NORM);
	else if (FStrEq(callerName, "clawright"))
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tubequeen/tq_clawattack_right.wav", 1.0, ATTN_NORM);
	else
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tubequeen/tq_clawattack.wav", 1.0, ATTN_NORM);

	ClawDamage();
}

void CTubeQueen::ClawDamage()
{
	// Simplified distance-check+TakeDamage melee, same pattern already
	// established for CBanditGrunt::Melee() - the original's exact
	// damage-application point wasn't found either, see file header.
	CBaseEntity* pPlayer = UTIL_PlayerByIndex(1);
	if (pPlayer && (pPlayer->pev->origin - pev->origin).Length() <= 200)
		pPlayer->TakeDamage(pev, pev, 20, DMG_SLASH);
}

void CTubeQueen::SpawnTube()
{
	// Confirmed fixed right-side spawn offset (563,147,-580 relative to
	// the queen), see file header/findings/entities/tubequeen_boss.md.
	UTIL_MakeVectors(pev->angles);
	Vector vecSpawn = pev->origin + gpGlobals->v_right * 563 + gpGlobals->v_forward * 147 + Vector(0, 0, -580);

	CBaseEntity* pTube = CBaseEntity::Create("monster_tube", vecSpawn, pev->angles, edict());
	if (pTube)
	{
		pTube->pev->owner = edict();
		pTube->pev->team = pev->team;
	}

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSpawn);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(vecSpawn.x);
	WRITE_COORD(vecSpawn.y);
	WRITE_COORD(vecSpawn.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(20);
	WRITE_BYTE(10);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects).
	UTIL_ExplosionEffects(vecSpawn, 150.0f);
}

void CTubeQueen::FireMortar(CBaseEntity* pTarget)
{
	const char* sounds[] = {"tubequeen/tq_mortarfire1.wav", "tubequeen/tq_mortarfire2.wav", "tubequeen/tq_mortarfire3.wav"};
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, sounds[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM);

	Vector vecSrc = pev->origin + Vector(0, 0, 100);
	Vector vecTargetPos = pTarget->pev->origin;
	float flDist = (vecTargetPos - vecSrc).Length();
	float flTime = V_max(flDist / 500.0f, 0.5f);

	// Constant-gravity toss to the target's current position - the
	// original's per-tick MortarThink parabola/lead-prediction is not
	// reproduced, see file header.
	Vector vecVelocity = (vecTargetPos - vecSrc) / flTime;
	vecVelocity.z += 0.5f * 380.0f * flTime;

	float flDamage;
	float flRadius;
	switch (g_iSkillLevel)
	{
	case SKILL_MEDIUM:
		flDamage = 140;
		flRadius = 90;
		break;
	case SKILL_HARD:
		flDamage = 180;
		flRadius = 100;
		break;
	default:
		flDamage = 80;
		flRadius = 70;
		break;
	}

	CBaseEntity* pMortar = CBaseEntity::Create("tubequeen_mortar", vecSrc, g_vecZero, edict());
	if (pMortar)
	{
		pMortar->pev->velocity = vecVelocity;
		pMortar->pev->owner = edict();
		static_cast<CTubeMortar*>(pMortar)->Configure(false, flDamage, flRadius);
	}
}

void CTubeQueen::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case TQ_AE_SHAKE:
		UTIL_ScreenShake(pev->origin, 7.0, 0.5, 1.0, 2400.0);
		break;
	case TQ_AE_GRAB_HANDIN:
		SetBodygroup(1, 1);
		break;
	case TQ_AE_GRAB_SOUNDCHAIN:
	{
		const char* grabs[] = {"tubequeen/tq_grabtube1.wav", "tubequeen/tq_grabtube2.wav", "tubequeen/tq_grabtube3.wav"};
		EMIT_SOUND(ENT(pev), CHAN_VOICE, grabs[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM);
		break;
	}
	case TQ_AE_GRAB_SPAWNTUBE:
		SpawnTube();
		SetBodygroup(1, 0);
		break;
	case TQ_AE_INSPECTBELLY:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "tubequeen/tq_inspectbelly.wav", 1.0, ATTN_NORM);
		break;
	case TQ_AE_CLAW_CENTER_HIT:
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tubequeen/tq_clawattack.wav", 1.0, ATTN_NORM);
		break;
	case TQ_AE_CLAW_LEFT_HIT:
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tubequeen/tq_clawattack_left.wav", 1.0, ATTN_NORM);
		break;
	case TQ_AE_CLAW_RIGHT_HIT:
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tubequeen/tq_clawattack_right.wav", 1.0, ATTN_NORM);
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CTubeQueen::MonsterThink()
{
	CBaseMonster::MonsterThink();

	if (pev->deadflag != DEAD_NO)
		return;

	// Pending grab-attack tube spawn, delayed to follow the hand-in
	// sound chain (see file header - no custom sequence player here).
	if (m_flGrabSpawnTime != 0 && gpGlobals->time >= m_flGrabSpawnTime)
	{
		SpawnTube();
		SetBodygroup(1, 0);
		m_flGrabSpawnTime = 0;
	}

	if (m_bRetreated)
		return;

	CBaseEntity* pPlayer = UTIL_PlayerByIndex(1);
	if (!pPlayer || !FVisible(pPlayer))
		return;

	if (gpGlobals->time >= m_flNextMortar)
	{
		m_flNextMortar = gpGlobals->time + RANDOM_FLOAT(4.0, 7.0);
		FireMortar(pPlayer);
	}

	if (m_flGrabSpawnTime == 0 && gpGlobals->time >= m_flNextGrab)
	{
		m_flNextGrab = gpGlobals->time + RANDOM_FLOAT(12.0, 20.0);

		SetBodygroup(1, 1);
		const char* grabs[] = {"tubequeen/tq_grabtube1.wav", "tubequeen/tq_grabtube2.wav", "tubequeen/tq_grabtube3.wav"};
		EMIT_SOUND(ENT(pev), CHAN_VOICE, grabs[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM);
		m_flGrabSpawnTime = gpGlobals->time + 1.2;
	}
}

bool CTubeQueen::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (!CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType))
		return false;

	// Retreat threshold approximated as 1/3 max_health - the original's
	// exact fraction constant wasn't resolved, see file header.
	if (!m_bRetreated && pev->health > 0 && pev->health <= pev->max_health / 3.0f)
	{
		m_bRetreated = true;
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "tubequeen/tq_climbaway.wav", 1.0, ATTN_NORM);
		UTIL_ScreenShake(pev->origin, 7.0, 8.0, 2.0, 3600.0);
	}

	return true;
}
