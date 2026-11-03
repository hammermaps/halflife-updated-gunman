//=========================================================
// rust7c/rust7d's remaining classes with no SDK precedent:
// monster_xenome (CXenome, the "Emperor Xenome") and
// button_aiwallplug (CAiWallPlug). weapon_beamgun/weapon_gausspistol/
// weapon_minigun, this pass's other gaps, are already-deferred player
// weapons.
//
// Decompiled fresh from gunman.dll this session:
//   monster_xenome: LINK @0x100c9920, vtable @0x101043c8, Spawn
//     @0x100cad30, Precache @0x100cace0, Classify @0x100c9bd0.
//     Confirmed bbox (-16,-16,0)/(16,16,48), SOLID_SLIDEBOX, health
//     from a skill cvar. Spawn sets pev->rendermode = kRenderTransColor
//     with pev->renderamt = 0 - a genuinely, code-confirmed invisible
//     spawn state, matching findings/entity_review_list.csv's
//     "teilweise unsichtbar" ("Emperor Xenome") note exactly rather
//     than being a rendering bug. Classify itself is dynamic: it
//     compares the entity's CURRENT pev->renderamt against a float
//     threshold and returns CLASS_PLAYER_BIOWEAPON(12) once visible
//     enough, CLASS_ALIEN_BIOWEAPON(13) otherwise - a
//     visibility-linked hostility switch uses the exact retail alpha
//     threshold of 55.0.
//   button_aiwallplug: LINK @0x1007ad00, vtable @0x100f9b24, Spawn
//     @0x1007adc0, Precache @0x1007aed0, ObjectCaps (slot 14,
//     @0x1007ad50, confirmed literal return -1 - matches
//     findings/entity_review_list.csv's "ObjectCaps=FCAP_CONTINUOUS_USE-
//     artig" note, reproduced faithfully). Confirmed class name
//     "CAiWallPlug" (Save-string literal, via
//     @0x1007ad60's embedded save-field helper). Confirmed bbox
//     (-16,-16,0)/(16,16,16), MOVETYPE_FLY, SOLID_BBOX,
//     takedamage=DAMAGE_NO, body index 1, and both confirmed sounds
//     ("mainframe/aiplug_activate_gs.wav"/
//     "mainframe/aiplug_deactivate_gs.wav") plus a shared sound-group
//     precache call not individually decompiled this session (a
//     sentence group, per findings - matches this project's
//     established random_speaker/player_speaker sentence-group
//     idiom).
//
// Current limits (2026-11-02): CXenome's target-direction branch and leap
// vector are now matched to Retail, but some Touch follow-up remains open.
// Its broader AI schedule is also not fully ported. CAiWallPlug still uses a
// simplified toggle-and-fire-targets Use() pending its exact sentence-group
// and state transition mapping.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "weapons.h"
#include "skill.h"

//=========================================================
// xenomeshot - CXenomeShot. Retail CXenome::HandleAnimEvent events 2/3
// call FUN_100ca010, which reaches factory FUN_100c95d0. The factory
// creates this point-sized flying projectile and assigns these callbacks.
//=========================================================
class CXenomeShot : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT XenomeShotThink();
	void EXPORT TouchShot(CBaseEntity* pOther);
	static CXenomeShot* Shoot(entvars_t* pevOwner, const Vector& vecStart, const Vector& vecVelocity);
};
LINK_ENTITY_TO_CLASS(xenomeshot, CXenomeShot);

void CXenomeShot::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("xenomeshot");
	SET_MODEL(ENT(pev), "models/spike.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
}

void CXenomeShot::Precache()
{
	PrecacheModel("models/spike.mdl");
}

CXenomeShot* CXenomeShot::Shoot(entvars_t* pevOwner, const Vector& vecStart, const Vector& vecVelocity)
{
	CXenomeShot* pShot = GetClassPtr((CXenomeShot*)nullptr);
	pShot->Spawn();
	UTIL_SetOrigin(pShot->pev, vecStart);
	pShot->pev->owner = ENT(pevOwner);
	pShot->pev->velocity = vecVelocity;
	pShot->pev->angles = UTIL_VecToAngles(vecVelocity);
	pShot->SetThink(&CXenomeShot::XenomeShotThink);
	pShot->SetTouch(&CXenomeShot::TouchShot);
	pShot->pev->nextthink = gpGlobals->time;
	return pShot;
}

void CXenomeShot::XenomeShotThink()
{
	// Retail FUN_100c96b0 keeps the projectile active by writing the current
	// server time to nextthink on every callback.
	pev->nextthink = gpGlobals->time;
}

void CXenomeShot::TouchShot(CBaseEntity* pOther)
{
	if (!pOther)
		return;

	// Retail installs SUB_Remove before distinguishing world from a damageable
	// entity, so every successful collision ends this projectile's lifetime.
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time;

	if (FClassnameIs(pOther->pev, "worldspawn"))
	{
		// Retail clears collision and schedules removal after world contact.
		// Keep this one-way world-impact lifecycle without adding an unverified
		// permanent embedded-prop duration.
		pev->movetype = MOVETYPE_NONE;
		pev->solid = SOLID_NOT;
		pev->velocity = g_vecZero;
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1f;
		return;
	}

	if (pOther->pev->takedamage != DAMAGE_NO)
		pOther->TakeDamage(pev, pev->owner ? VARS(pev->owner) : pev, gSkillData.xenomeDartDamage, DMG_GENERIC);
}

//=========================================================
// monster_xenome - CXenome ("Emperor Xenome").
//=========================================================
class CXenome : public CBaseMonster
{
public:
	int MonsterProfile() override { return 19; } // retail slot 59
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void Spawn() override;
	void Precache() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void EXPORT LeapTouch(CBaseEntity* pOther);
	int Classify() override
	{
		// Retail FUN_100c9bd0 compares the current render alpha against
		// its exact 55.0 threshold before changing allegiance.
		return pev->renderamt >= 55.0f ? CLASS_PLAYER_BIOWEAPON : CLASS_ALIEN_BIOWEAPON;
	}
	// Retail CXenome slot 82: do not start melee attack 1 while the ideal
	// activity is melee attack 2; then require a valid enemy within 48 units.
	bool CheckMeleeAttack1(float flDot, float flDist) override
	{
		(void)flDot;
		return m_IdealActivity != ACT_MELEE_ATTACK2 && flDist <= 48.0f && m_hEnemy != nullptr;
	}
	// Retail slot 83: the jump melee is available after its saved cooldown,
	// while the Xenome sees its enemy and that enemy faces it.
	bool CheckMeleeAttack2(float flDot, float flDist) override
	{
		(void)flDot;
		(void)flDist;
		return gpGlobals->time >= m_flNextJump &&
			HasAllConditions(bits_COND_SEE_ENEMY | bits_COND_ENEMY_FACING_ME);
	}
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
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
	void ShootAtEnemy(int iSide);
	void LeapAttack();
	float m_flNextJump = 0;
	int m_iVoicePitch = PITCH_NORM;
};
LINK_ENTITY_TO_CLASS(monster_xenome, CXenome);

TYPEDESCRIPTION CXenome::m_SaveData[] =
{
	DEFINE_FIELD(CXenome, m_flNextJump, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CXenome, CBaseMonster);

void CXenome::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/xenome.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 48));

	pev->solid = SOLID_SLIDEBOX;
	pev->health = gSkillData.xenomeHealth;
	pev->rendermode = kRenderTransTexture; // Retail @0x100cad30: +0x148 = 2
	m_flFieldOfView = -0.7f;
	pev->renderamt = 0; // confirmed spawn-invisible, see file header
	m_flNextJump = gpGlobals->time; // Retail +0x2b4 / m_flNextJump.
	m_iVoicePitch = RANDOM_LONG(-10, 10) + 90; // Retail +0x2a8; range 80..100.

	MonsterInit();
	// Retail (Capability-Schreibzugriff +0x220 nach MonsterInit): gleiche Bitoperation wie im Dekompilat.
	m_afCapability |= bits_CAP_SQUAD;
	m_afCapability |= bits_CAP_MELEE_ATTACK2;
	// Retail FUN_100cad30 assigns this after the virtual MonsterInit call.
	pev->view_ofs = Vector(0, 0, 40);
}

void CXenome::Precache()
{
	PrecacheModel("models/xenome.mdl");
	PrecacheModel("sprites/gibxeno.spr");
	PrecacheModel("sprites/gorexeno.spr");
	PrecacheSound("xenome/xenome_shoot.wav");
	PrecacheSound("xenome/xenome_clawattack_1.wav");
	PrecacheSound("xenome/xenome_clawattack_2.wav");
	PrecacheSound("lg_scorp/lg_scorp_attackb.wav");
	UTIL_PrecacheOther("xenomeshot");
}

static void XenomeHitSound(CBaseEntity* pOther)
{
	if (pOther)
		EMIT_SOUND_DYN(ENT(pOther->pev), CHAN_VOICE, "lg_scorp/lg_scorp_attackb.wav", 1.0f, 2.0f, 0, 140);
}

void CXenome::ShootAtEnemy(int iSide)
{
	CBaseMonster* pEnemy = m_hEnemy ? m_hEnemy->MyMonsterPointer() : nullptr;
	if (!pEnemy)
		return;

	// Retail FUN_100c9e10 uses origin + up*20 and offsets event 2/3
	// by right*-25/+25 respectively.
	const Vector vecStart = pev->origin + gpGlobals->v_up * 20.0f +
		gpGlobals->v_right * (iSide != 0 ? 25.0f : -25.0f);
	Vector vecDirection;
	if (pEnemy->IsAlive() && pEnemy->m_hTargetEnt)
	{
		// Retail uses this perturbed BodyTarget branch only for a live enemy
		// with a target entity handle.
		vecDirection = pEnemy->BodyTarget(vecStart) - vecStart;
		vecDirection.x += RANDOM_LONG(-32, 32);
		vecDirection.y += RANDOM_LONG(-32, 32);
		vecDirection.z += RANDOM_LONG(0, 64);
		vecDirection = vecDirection.Normalize();
	}
	else
	{
		// Retail uses BodyTarget offset plus the Xenome's last-known enemy
		// point when the enemy is dead or has no target entity handle.
		const Vector vecBodyOffset = pEnemy->BodyTarget(vecStart) - pEnemy->pev->origin;
		vecDirection = (m_vecEnemyLKP + vecBodyOffset - vecStart).Normalize();
	}

	CXenomeShot::Shoot(pev, vecStart, vecDirection * 2000.0f);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "xenome/xenome_shoot.wav", 0.5f, 0.8f, 0, m_iVoicePitch);
}

void CXenome::LeapAttack()
{
	// Retail vtable slot +0x20c selects one claw attack sound before the hull trace.
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON,
		RANDOM_LONG(0, 1) ? "xenome/xenome_clawattack_2.wav" : "xenome/xenome_clawattack_1.wav",
		1.0f, 0.8f, 0, 80);

	TraceResult tr;
	CBaseEntity* pHurt = CheckTraceHullAttack(64.0f, gSkillData.xenomeMeleeDamage, DMG_SLASH, &tr);
	if (!pHurt)
		return;

	// Retail assigns integer view punch values from [-10, 10] on both axes.
	pHurt->pev->punchangle.z = RANDOM_LONG(-10, 10);
	pHurt->pev->punchangle.x = RANDOM_LONG(-10, 10);
	if (CBaseMonster* pVictim = pHurt->MyMonsterPointer())
	{
		const Vector vecSource = BodyTarget(g_vecZero);
		const Vector vecTarget = pHurt->BodyTarget(g_vecZero);
		const Vector vecAttackDir = (vecTarget - vecSource).Normalize();
		pVictim->TraceBleed(gSkillData.xenomeMeleeDamage, vecAttackDir, &tr, DMG_SLASH);
	}
	// Retail calls the hit callback only while the damaged target remains alive.
	if (pHurt->IsAlive())
		XenomeHitSound(pHurt);

	// Retail launches after resolving the melee hit and arms the contact callback.
	UTIL_MakeVectors(pev->angles);
	pev->flags &= ~FL_ONGROUND;
	pev->velocity = gpGlobals->v_forward * 500.0f;
	pev->velocity.z += CVAR_GET_FLOAT("sv_gravity") * 0.625f * 0.7f;
	SetTouch(&CXenome::LeapTouch);
	m_flNextJump = gpGlobals->time + 3.0f;
}

void CXenome::LeapTouch(CBaseEntity* pOther)
{
	// Retail tests entvars +0x16c (takedamage), not the target's vertical velocity.
	if (!pOther || pOther->pev->takedamage == DAMAGE_NO ||
		pOther->Classify() == Classify() || FBitSet(pev->flags, FL_ONGROUND))
		return;

	pOther->TakeDamage(pev, pev, gSkillData.xenomeMeleeDamage, DMG_SLASH);
	if (CBaseMonster* pVictim = pOther->MyMonsterPointer())
	{
		const Vector vecSource = BodyTarget(g_vecZero);
		const Vector vecTarget = pOther->BodyTarget(g_vecZero);
		const Vector vecAttackDir = (vecTarget - vecSource).Normalize();
		TraceResult tr;
		// Retail TraceLine uses pev->pContainingEntity (+0x208) as the
		// skipped edict, in addition to tracing through other monsters.
		UTIL_TraceLine(vecSource, vecTarget, dont_ignore_monsters, pev->pContainingEntity, &tr);
		pVictim->TraceBleed(gSkillData.xenomeMeleeDamage, vecAttackDir, &tr, DMG_SLASH);
		// Retail calls slot +0x224 only for monster victims and only when
		// their entvars flags pass TEST 0x28.
		if ((pOther->pev->flags & 0x28) != 0)
			XenomeHitSound(pOther);
	}
	// Retail clears m_pfnTouch after a successful leap contact.
	SetTouch(nullptr);
}

void CXenome::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 2:
		ShootAtEnemy(0);
		break;
	case 3:
		ShootAtEnemy(1);
		break;
	case 6:
	case 7:
		LeapAttack();
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// entity_aicoreTemp - CAicoreTemp.
//=========================================================
static bool g_bAiWallPlugCompleted = false;

bool ConsumeAiWallPlugCompletion()
{
	const bool completed = g_bAiWallPlugCompleted;
	g_bAiWallPlugCompleted = false;
	return completed;
}

class CAicoreTemp : public CBaseAnimating
{
public:
	void Spawn() override;
	void Precache() override;
	void DestroyTouch(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(entity_aicoreTemp, CAicoreTemp);

void CAicoreTemp::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_aicore.mdl");
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetThink(nullptr);
	SetTouch(&CAicoreTemp::DestroyTouch);
	// Retail Spawn (0x1007bd60) ends with the sequence reset (0x10001c50 = ResetSequenceInfo);
	// the class is animated, so the base is CBaseAnimating.
	ResetSequenceInfo();
}

void CAicoreTemp::Precache()
{
	PrecacheModel("models/w_aicore.mdl");
}

void CAicoreTemp::DestroyTouch(CBaseEntity* pOther)
{
	// Retail compares the touching entity's owner to this prop's owner.
	if (pOther && pOther->pev->owner != pev->owner)
		SUB_Remove();
}

//=========================================================
// button_aiwallplug - CAiWallPlug.
//=========================================================
class CAiWallPlug : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int ObjectCaps() override { return -1; } // confirmed literal, see file header
	int Activate(CBaseEntity* pActivator);
	void AiWallPlugThink();
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	void SetSequence(const char* name);

	int m_iState = 0;
	EHANDLE m_hActivator;
};
LINK_ENTITY_TO_CLASS(button_aiwallplug, CAiWallPlug);

// The retail player-use path invokes CAiWallPlug directly rather than its
// virtual CBaseEntity::Use callback.  Keep that boundary explicit so ordinary
// map I/O cannot accidentally turn the terminal into a generic toggle.
int ActivateAiWallPlug(CBaseEntity* pTarget, CBaseEntity* pActivator)
{
	if (!pTarget || !FClassnameIs(pTarget->pev, "button_aiwallplug"))
		return -1;

	return static_cast<CAiWallPlug*>(pTarget)->Activate(pActivator);
}

TYPEDESCRIPTION CAiWallPlug::m_SaveData[] =
	{
		DEFINE_FIELD(CAiWallPlug, m_iState, FIELD_INTEGER),
		DEFINE_FIELD(CAiWallPlug, m_hActivator, FIELD_EHANDLE),
	};

IMPLEMENT_SAVERESTORE(CAiWallPlug, CBaseMonster);

void CAiWallPlug::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/aiwallplug.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_NO;
	pev->body = 1;
	m_iState = 0;
	m_hActivator = nullptr;
	SetSequence("inactive");

	UTIL_SetOrigin(pev, pev->origin);
}

void CAiWallPlug::Precache()
{
	PrecacheModel("models/aiwallplug.mdl");
	PrecacheSound("mainframe/aiplug_activate_gs.wav");
	PrecacheSound("mainframe/aiplug_deactivate_gs.wav");
}

void CAiWallPlug::SetSequence(const char* name)
{
	const int sequence = LookupSequence(name);
	if (sequence >= 0)
	{
		pev->sequence = sequence;
		pev->frame = 0;
		ResetSequenceInfo();
	}
}

int CAiWallPlug::Activate(CBaseEntity* pActivator)
{
	if (m_iState != 0)
		return 1;

	m_iState = 1;
	m_hActivator = pActivator;
	SetSequence("activating");
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "mainframe/aiplug_activate_gs.wav", 1.0, ATTN_NORM);

	// Retail checks spawnflag 0x100 immediately after activating.  That path
	// stays active, fires its targets and consumes the AI Core.
	if (pev->spawnflags & 0x100)
	{
		m_iState = 2;
		SetSequence("active");
		FireTargets(STRING(pev->target), pActivator, this, USE_TOGGLE, 0);
		return 2;
	}

	SetSequence("deactivating");
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "mainframe/aiplug_deactivate_gs.wav", 1.0, ATTN_NORM);
	SetThink(&CAiWallPlug::AiWallPlugThink);
	pev->nextthink = gpGlobals->time + 0.1f;
	return 0;
}

void CAiWallPlug::AiWallPlugThink()
{
	if (m_iState == 2 || !m_hActivator)
		return;

	m_iState = 2;
	FireTargets(STRING(pev->target), m_hActivator, this, USE_TOGGLE, 0);
	SetSequence("inactive");

	// Retail spawns a short-lived physical AI-Core prop at the terminal and
	// launches it along its forward axis at the exact 75-unit speed constant.
	UTIL_MakeVectors(pev->angles);
	CBaseEntity* pCore = CBaseEntity::Create("entity_aicoreTemp", pev->origin, pev->angles, pev->owner);
	if (pCore)
		pCore->pev->velocity = gpGlobals->v_forward * 75.0f;

	m_hActivator = nullptr;
	g_bAiWallPlugCompleted = true;
}
