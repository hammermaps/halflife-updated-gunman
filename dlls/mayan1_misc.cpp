//=========================================================
// MAYAN1's remaining classes with no SDK precedent: monster_raptor
// (CRaptor) and monster_darttrap (CDartTrap + its runtime-only
// CDart projectile).
//
// Decompiled fresh from gunman.dll this session:
//   monster_raptor: LINK @0x10075fa0, vtable @0x100f8bc8, Spawn
//     @0x100776e0, Precache @0x10077430, Classify @0x10077470
//     (returns 15 normally, 5 == CLASS_ALIEN_MILITARY when
//     pev->skin==1 - see CLASS_ALIEN_PREDATOR_RAPTOR in cbase.h),
//     bbox-recompute override @0x10076000 (custom SetObjectCollisionBox
//     - not reproduced, see below), CRaptor::JumpTouch @0x100766a0
//     (named via the binary's symbol table). Cross-checked against
//     findings/entity_review_list.csv's prior notes - confirms the
//     "precacht tatsaechlich Rheptor.mdl" naming trap and the
//     skin-driven Classify(5|15) split, both independently re-derived
//     from the fresh decompile rather than assumed from the doc.
//     monster_rheptor (same CRaptor class, per findings a pure
//     model/recolor alias) and monster_microraptor (a separate
//     CMicroRaptor class per findings) are NOT linked here - MAYAN1
//     only requires monster_raptor; the others get added, per the
//     project's map-driven rule, only once an actual map needs them.
//   monster_darttrap: LINK @0x100a3600, vtable @0x100ff2b8, Spawn
//     @0x100a37f0, Precache @0x100a37c0, KeyValue @0x100a36b0,
//     CDartTrap::OnUse @0x100a3870, CDartTrap::DartShootThink
//     @0x100a3890 (all named via the symbol table), plus the
//     runtime-only dart-fire helper @0x100a31b0 and CDart::ShotTouch
//     @0x100a32e0. Cross-checked against
//     findings/entity_review_list.csv's "FGD-Keyvalues 1:1 bestaetigt"
//     note - re-verified this session: dartcount (int, -1 = infinite,
//     decremented each shot, 0 removes the trap), spread (int),
//     timebetweenshots (float, also the OnUse arm delay).
//
// Simplified relative to the original (documented per-case):
// - CRaptor: the custom bbox-recompute override (slot 7,
//   FUN_10076000) - a hand-tuned collision box distinct from the
//   normal UTIL_SetSize hull - is not reproduced; the engine's default
//   axis-aligned box from UTIL_SetSize is used instead. No custom
//   TakeDamage/Killed override reproduced (matches
//   entity_review_list.csv's note that monster_rheptor, sharing this
//   same class, has "keine eigene KI/Kampflogik"). JumpTouch's exact
//   vector-cross knockback math is approximated as a simple
//   fixed-damage bite-on-touch instead of the byte-exact push vector.
//   The exact skill-cvar health lookup (FUN_100608d0(DAT_10136ec8))
//   was not traced to a specific gSkillData field - plausible fixed
//   health used instead.
// - CDartTrap: DartShootThink spread and projectile speed are now ported
//   from the retail rejection-sampled cone and firing helper. CRaptor's
//   simplified combat details remain listed above.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "effects.h"
#include "explode.h"
#include "weapons.h"

//=========================================================
// The runtime-only dart projectile fired by monster_darttrap. No
// LINK_ENTITY_TO_CLASS under a map-facing name since it's never
// map-placed, spawned only via CBaseEntity::Create.
//=========================================================
class CDart : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT ShotTouch(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(monster_darttrap_dart, CDart);

void CDart::Precache()
{
	PrecacheModel("models/dart.mdl");
	PrecacheSound("darts/blowdart2.wav");
	PrecacheSound("darts/blowdart3.wav");
	PrecacheSound("darts/blowdartimpact1.wav");
	PrecacheSound("darts/blowdartimpact2.wav");
	PrecacheSound("darts/blowdartimpact3.wav");
	PrecacheSound("debris/flesh1.wav");
}

void CDart::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("monster_dart");
	SET_MODEL(ENT(pev), "models/dart.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	UTIL_MakeVectors(pev->angles); // retail @0x100a30e0 builds the basis from the creator's angles; it does not zero them
	pev->avelocity.z = RANDOM_LONG(-200, 200);
	SetTouch(&CDart::ShotTouch);

	// Retail FUN_100608b0(0x55) selects blowdart3 on 85 of 100 outcomes.
	const char* pszFlightSound = RANDOM_LONG(0, 99) < 85 ? "darts/blowdart3.wav" : "darts/blowdart2.wav";
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pszFlightSound, 1.0, ATTN_NORM, 0, 100);
}

void CDart::ShotTouch(CBaseEntity* pOther)
{
	const Vector vecImpactVelocity = pev->velocity;

	// Retail clears flight immediately and schedules SUB_Remove at the
	// current time for every impact. The worldspawn branch below may still
	// adjust the one-frame embedded presentation first.
	SetThink(&CDart::SUB_Remove);
	pev->nextthink = gpGlobals->time;
	pev->velocity = g_vecZero;

	if (pOther && pOther->pev->takedamage != DAMAGE_NO)
	{
		pOther->TakeDamage(pev, pev, 8, DMG_BULLET);
		// Retail ShotTouch @ 0x100a32e0 plays the flesh sound only for a player.
		if (pOther->IsPlayer())
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/flesh1.wav", 1.0, ATTN_NORM, 0, 110);
	}
	else if (pOther && FClassnameIs(pOther->pev, "worldspawn"))
	{
		// Retail embeds the dart 12 units along its travel direction before
		// the already scheduled immediate remove, preserving the impact frame.
		const Vector vecDirection = vecImpactVelocity.Normalize();
		UTIL_SetOrigin(pev, pev->origin - vecDirection * 12.0f);
		pev->angles = UTIL_VecToAngles(vecDirection);
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_FLY;

		switch (RANDOM_LONG(0, 2))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "darts/blowdartimpact1.wav", 0.7, ATTN_NORM, 0, 100);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "darts/blowdartimpact2.wav", 0.7, ATTN_NORM, 0, 100);
			break;
		default:
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "darts/blowdartimpact3.wav", 0.7, ATTN_NORM, 0, 100);
			break;
		}
	}
}

//=========================================================
// monster_darttrap - CDartTrap. An invisible, Use-triggered dart
// launcher script point (see file header re: spread-cone
// simplification).
//=========================================================
class CDartTrap : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT OnUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT DartShootThink();

	int m_iDartCount = -1;
	int m_iSpread = 0;
	float m_flTimeBetweenShots = 2.0;
};
LINK_ENTITY_TO_CLASS(monster_darttrap, CDartTrap);

TYPEDESCRIPTION CDartTrap::m_SaveData[] =
	{
		DEFINE_FIELD(CDartTrap, m_iDartCount, FIELD_INTEGER),
		DEFINE_FIELD(CDartTrap, m_iSpread, FIELD_INTEGER),
		DEFINE_FIELD(CDartTrap, m_flTimeBetweenShots, FIELD_FLOAT),
	};

IMPLEMENT_SAVERESTORE(CDartTrap, CBaseEntity);

bool CDartTrap::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "dartcount"))
	{
		m_iDartCount = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "timebetweenshots"))
	{
		m_flTimeBetweenShots = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spread"))
	{
		m_iSpread = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CDartTrap::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/null.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	pev->takedamage = DAMAGE_NO;

	SetUse(&CDartTrap::OnUse);
	SetThink(&CDartTrap::DartShootThink);
}

void CDartTrap::Precache()
{
	PrecacheModel("models/null.mdl");
	PrecacheModel("models/dart.mdl");
	UTIL_PrecacheOther("monster_darttrap_dart");
}

void CDartTrap::OnUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	pev->nextthink = gpGlobals->time + m_flTimeBetweenShots;
}

void CDartTrap::DartShootThink()
{
	if (m_iDartCount != -1)
	{
		if (m_iDartCount == 0)
		{
			UTIL_Remove(this);
			return;
		}
		m_iDartCount--;
	}

	const float flSpreadRadians = m_iSpread * 0.00873f;
	float flRightOffset;
	float flUpOffset;
	do
	{
		// Retail samples each disk coordinate as the sum of two independent
		// RANDOM_FLOAT(-0.5, 0.5) values, then rejects points outside the unit disk.
		flRightOffset = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
		flUpOffset = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
	} while (flRightOffset * flRightOffset + flUpOffset * flUpOffset > 1.0f);

	UTIL_MakeVectors(pev->angles);
	Vector vecDir = gpGlobals->v_forward + gpGlobals->v_right * (flSpreadRadians * flRightOffset) + gpGlobals->v_up * (flSpreadRadians * flUpOffset);
	vecDir = vecDir.Normalize();

	CBaseEntity* pDart = CBaseEntity::Create("monster_darttrap_dart", pev->origin, UTIL_VecToAngles(vecDir), edict());
	if (pDart)
		pDart->pev->velocity = vecDir * 800.0f;

	pev->nextthink = gpGlobals->time + m_flTimeBetweenShots;
}

//=========================================================
// monster_raptor - CRaptor. Also used (per findings, unverified this
// session since MAYAN1 doesn't require it) under the alias
// "monster_rheptor".
//=========================================================
class CRaptor : public CBaseMonster
{
public:
	int MonsterProfile() override { return 8; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	void SetObjectCollisionBox() override;
	int Classify() override { return pev->skin == 1 ? CLASS_ALIEN_MILITARY : CLASS_ALIEN_PREDATOR_RAPTOR; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this - see the
	// feedback-setactivity-act-idle-stomps-scripted-sequence memory
	// note (same fix as CFriendlyGunman etc; covers monster_rheptor
	// too via classname alias).
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
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void EXPORT JumpTouch(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(monster_raptor, CRaptor);
// CORRECTION (2026-09-05): findings/entities/dinosaur_family.md confirms
// monster_rheptor is a pure classname alias of monster_raptor (identical
// vtable @0x100f8bc8) - added here for completeness, same pattern as
// monster_gunner/monster_rustgnr's alias fix, see
// findings/open_items_audit_2026-09-05.md.
LINK_ENTITY_TO_CLASS(monster_rheptor, CRaptor);

void CRaptor::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/rheptor.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 40));

	pev->movetype = MOVETYPE_STEP;
	pev->solid = SOLID_SLIDEBOX;
	// Retail @0x100776e0: Health per FUN_100608d0 (Skill-Basis + Zufallsanteil).
	pev->health = gSkillData.raptorHealth + RANDOM_FLOAT(0, gSkillData.raptorHealth) * (gSkillData.percentRandomHealth * 0.01f);
	pev->view_ofs = Vector(0, 0, 48);
	pev->flags |= FL_MONSTER;

	if (pev->skin == 1)
		SetBodygroup(2, 1);

	MonsterInit();
	// Retail (Capability-Schreibzugriff +0x220 nach MonsterInit): gleiche Bitoperation wie im Dekompilat.
	m_afCapability |= bits_CAP_MELEE_ATTACK2;
	
	// rheptor.mdl has no $eyeposition.  MonsterInit() therefore clears this
	// offset; retail writes its authored eye height after initialization.
	pev->view_ofs = Vector(0, 0, 48);
}

void CRaptor::Precache()
{
	PrecacheModel("models/rheptor.mdl");
	PrecacheModel("sprites/gorehuman.spr");
	PrecacheModel("sprites/gibhuman.spr");
	PrecacheSound("rheptor/diefight.wav");
	PrecacheSound("rheptor/diesimple.wav");
	PrecacheSound("rheptor/smallflinch.wav");
	PrecacheSound("rheptor/bigflinch.wav");
	PrecacheSound("rheptor/bitechunk.wav");
	PrecacheSound("lg_scorp/lg_scorp_attackb.wav");
}

void CRaptor::SetObjectCollisionBox()
{
	// Retail FUN_10076000 uses a larger, asymmetric-to-the-model absolute
	// box for the adult Rheptor. It deliberately does not overwrite the
	// spawn-time pev->mins/maxs collision hull.
	pev->absmin = pev->origin + Vector(-48, -48, 0);
	pev->absmax = pev->origin + Vector(48, 48, 96);
}

void CRaptor::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
		// Retail FUN_10076870: event 1 performs a 70-unit bite with
		// DAT_10136ecc = sk_raptor_bite_d.
		CheckTraceHullAttack(70.0f, gSkillData.raptorBiteDamage, DMG_SLASH);
		break;

	case 2:
		// Retail uses DAT_10136ee4 here, which RefreshSkillData maps to
		// sk_lrgscorp_sting_d. Keep this unusual cross-family slot intact.
		CheckTraceHullAttack(70.0f, gSkillData.largeScorpionStingDamage, DMG_SLASH);
		break;

	case 3:
		// Retail FUN_10076870 launches the raptor only for a valid ballistic
		// arc to its current enemy, and enables JumpTouch for that flight.
		if (m_hEnemy)
		{
			const Vector vecVelocity = VecCheckThrow(pev, pev->origin, m_hEnemy->Center(), 600.0f, 1.0f);
			if (vecVelocity != g_vecZero)
			{
				pev->velocity = vecVelocity;
				pev->flags &= ~FL_ONGROUND;
				SetTouch(&CRaptor::JumpTouch);
			}
		}
		break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CRaptor::JumpTouch(CBaseEntity* pOther)
{
	// Retail applies the jump impact only while airborne. Landing marks the
	// raptor as grounded before the target check, and every touch ends the
	// temporary flight callback.
	if (!FBitSet(pev->flags, FL_ONGROUND))
	{
		pev->flags |= FL_ONGROUND;
		if (pOther && pOther->pev->takedamage != DAMAGE_NO && pOther->Classify() != Classify())
		{
			if (pOther->TakeDamage(pev, pev, gSkillData.raptorJumpDamage, DMG_SLASH))
			{
				EMIT_SOUND_DYN(pOther->edict(), CHAN_VOICE, "lg_scorp/lg_scorp_attackb.wav", 0.5f, ATTN_IDLE, 0, 50);
			}
		}
	}

	SetTouch(NULL);
}
