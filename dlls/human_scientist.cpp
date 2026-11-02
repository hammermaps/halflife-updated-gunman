//=========================================================
// monster_human_scientist - CHumanScientist. Second gap of the
// REBAR2* block in the map-driven pass (after ammo_chemical, see
// rebar_misc.cpp). NOT the stock SDK's friendly CScientist/CTalkMonster
// - see findings/entities/scientist_family.md: this is a hostile enemy from
// the same "Human-Bandit" combat-NPC family as human_bandit.cpp's CBanditGrunt,
// reskinned as "Human Evil Scientist" (confirmed by the FGD's own literal
// display name). Its shared precache loads Gauss/shotgun assets but does not
// prove those attacks for this class; Retail Event 1 launches a chemical
// grenade, and the death path drops weapon_SPchemicalgun. It shares real
// vtable-slot code with CBanditGrunt (Classify()==5 and the same
// Spawn-tail FUN_100aa080 and Precache-tail FUN_100aa1d0 already
// reproduced in human_bandit.cpp/human_demoman.cpp).
//
// Decompiled fresh from gunman.dll: LINK @0x100b1d00, vtable
// @0x101010ac, Spawn @0x100b1e50, Classify (slot 8, @0x100b25d0,
// confirmed constant 5 = CLASS_ALIEN_MILITARY), HandleAnimEvent (slot
// 58, @0x100b2690). KeyValue (slot 2) is the generic shared stub - no
// custom FGD keyvalues beyond the engine's own generic "skin"
// mapping, matching the doc's finding that there is no weapon-choice
// keyvalue (weapon type is fixed in code, not mapper-selectable,
// unlike CBanditGrunt's MechaGunBandit).
//
// Spawn confirms: model "models/evil_scientist.mdl", bbox
// (-16,-16,0)/(16,16,72), SOLID_SLIDEBOX, health exactly double the
// sk_scientist_h-backed DAT_10136f14 global, view_ofs.z = 50
// (0x42480000), a
// fixed weapon-type bitmask field set to the constant 0x10 (one of
// the confirmed 1/2/4/0x10/0x40/0x80 values from the shared
// schedule-selection switch documented in
// findings/entities/human_bandit_base_class.md; the exact relationship
// between this weapon-mode bit and additional ranged attacks remains open).
//
// HandleAnimEvent: event 1 throws one of four chemical-grenade variants and
// resets its cooldown. Events 4/5 enter the shared
// short-range trace helper and ultimately use DAT_10136f4c; its exact
// parameter reconstruction remains open. All other events delegate to
// the generic monster handler. No other concrete ranged attack is inferred
// from the shared Precache tail.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "human_base.h"
#include "chemgrenade.h"

class CHumanScientist : public CHumanBase
{
public:
	int MonsterProfile() override { return 25; } // retail slot 59
	bool GetHeadshotGibBodygroup(int& group, int& value) override { group = 1; value = 1; return true; }
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_MILITARY; }
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	// Retail @0x100b25e0 (Slot 101)
	void SetYawSpeed() override
	{
		switch (m_Activity)
		{
		case ACT_IDLE:
			pev->yaw_speed = 120;
			break;
		case ACT_WALK:
			pev->yaw_speed = 180;
			break;
		case ACT_RUN:
			pev->yaw_speed = 150;
			break;
		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
			pev->yaw_speed = 140;
			break;
		default:
			pev->yaw_speed = 90;
			break;
		}
	}
	// BUG FIX (2026-09-05, live gameplay report): this NPC is commonly
	// puppeted by scripted_sequence "waiting"/idle-pose loops across
	// this project's maps (self-retriggering scripted_sequence chains
	// playing named, non-ACT_IDLE-tagged sequences). Without this
	// override, CBaseMonster's default SetActivity(ACT_IDLE) (called by
	// the default AI schedule between scripted waypoints, even for an
	// otherwise full combat monster) stomps pev->sequence mid-animation,
	// cutting the wait pose short and restarting it from frame 0 - same
	// root cause as CFurniture/CGenericMonster/CRenesaur/CAIGirl/
	// CFriendlyGunman. Normal combat activity switching is unaffected.
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
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	// Retail-Slots 80/82 (@0x100b2080 / @0x100b2050), 141 (@0x100b1de0: unbegrenzte Munition).
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckMeleeAttack1(float flDot, float flDist) override;
	bool InfiniteAmmo() override { return true; }
	bool CanFireRange2() override { return false; }
	// Retail virtual slot 126 returns attachment 0 as the scientist's gun
	// origin; the generic CBaseMonster fallback (origin + view_ofs) differs.
	Vector GetGunPosition() override;

private:
	void ThrowChemGrenade();
	void PainSound();
	bool m_fHasGun = true;		 // Retail +0x2f4: Chemgun noch am Koerper (wird beim Tod abgeworfen)
	float m_flNextPainTime = 0; // Retail +0x2f8
	float m_flNextTossCheck = 0; // Retail +0x300
	bool m_fCanToss = false;	 // Retail +0x304
	Vector m_vecToss;			 // Retail +0x308
	int m_iThrowType = 0;		 // Retail +0x314 (Wurfart 0..3 der Chemie-Granate)
};
LINK_ENTITY_TO_CLASS(monster_human_scientist, CHumanScientist);

// Retail @0x100b2390 (vtable slot 126): GET_ATTACHMENT(edict, 0), returning
// the attachment origin. The attachment angle output is intentionally unused.
Vector CHumanScientist::GetGunPosition()
{
	Vector vecOrigin, vecAngles;
	GetAttachment(0, vecOrigin, vecAngles);
	return vecOrigin;
}

// Retail CHostileScientist saves the gun state, pain timer, toss cooldown,
// toss-ready state and toss vector. m_iThrowType is deliberately not saved:
// the six retail descriptors contain no corresponding throw-variant field.
TYPEDESCRIPTION CHumanScientist::m_SaveData[] =
{
	DEFINE_FIELD(CHumanScientist, m_fHasGun, FIELD_BOOLEAN),
	DEFINE_FIELD(CHumanScientist, m_flNextPainTime, FIELD_TIME),
	DEFINE_FIELD(CHumanScientist, m_flNextTossCheck, FIELD_TIME),
	DEFINE_FIELD(CHumanScientist, m_fCanToss, FIELD_BOOLEAN),
	DEFINE_FIELD(CHumanScientist, m_vecToss, FIELD_VECTOR),
};
IMPLEMENT_SAVERESTORE(CHumanScientist, CHumanBase);

void CHumanScientist::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/evil_scientist.mdl");
	// Blutfarbe 247 kommt aus der Profiltabelle (MonsterProfile() = 25); Retail setzt +0x238 im Spawn nicht.
	// Retail setzt erst sk_scientist_h; MonsterInit kopiert das nach max_health.
	// Danach verdoppelt Retail nur health, max_health bleibt beim einfachen Skillwert.
	pev->health = gSkillData.scientistHealth;
	pev->weapons = 0x10;
	m_flNextGrenadeCheck = gpGlobals->time;
	m_flNextTossCheck = gpGlobals->time;
	m_fCanToss = false;
	m_iThrowType = RANDOM_LONG(0, 3);
	m_fHasGun = true;
	m_flNextPainTime = gpGlobals->time;

	// Retail ruft MonsterInit (Slot 67) vor dem Menschen-Init @0x100aa080.
	pev->view_ofs = Vector(0, 0, 50); // nur gegen die SDK-Warnung; Retail setzt es erst nach MonsterInit
	MonsterInit();
	// Retail FUN_100b1e50 verdoppelt health nach MonsterInit, nicht max_health.
	pev->health = gSkillData.scientistHealth * 2.0f;
	HumanInit();
	pev->view_ofs = Vector(0, 0, 50); // Retail Spawn @0x100b1e50: 0x42480000
}

// Retail @0x100b2080 (Slot 80): Wurfpruefung der Chemie-Granate. Bei nicht erledigter Wartezeit gilt das alte
// Ergebnis. Das Ziel muss am Boden oder im Wasser sein; Zielpunkt = Koerperziel des Feindes relativ zu seiner
// letzten bekannten Position; Wurfbahn per VecCheckToss; Wiederholung nach 0,3 s (Erfolg) bzw. 0,2 s (Fehlschlag).
// Das Retail-Statusbit this+0xbc & 0x10 entspricht bits_COND_SEE_ENEMY;
// in diesem Zustand korrigiert Retail den Granatenwurf um den Gegner-Vorhalt.
bool CHumanScientist::CheckRangeAttack1(float flDot, float flDist)
{
	if (m_flNextTossCheck > gpGlobals->time)
		return m_fCanToss;

	if (m_hEnemy == nullptr)
		return false;

	if ((m_hEnemy->pev->flags & FL_ONGROUND) == 0 && m_hEnemy->pev->waterlevel == 0)
	{
		m_fCanToss = false;
		return false;
	}

	Vector vecTarget = m_hEnemy->BodyTarget(pev->origin) - m_hEnemy->pev->origin + m_vecEnemyLKP;
	// Retail leads the grenade only while the SEE_ENEMY condition (0x10) is
	// set: enemy velocity times target distance / 900 units per second.
	if (HasConditions(bits_COND_SEE_ENEMY))
	{
		const float flLeadTime = (vecTarget - pev->origin).Length() / 900.0f;
		vecTarget = vecTarget + m_hEnemy->pev->velocity * flLeadTime;
	}
	const Vector vecToss = VecCheckToss(pev, GetGunPosition(), vecTarget, 0.5f);

	if (vecToss == g_vecZero)
	{
		m_fCanToss = false;
		m_flNextTossCheck = gpGlobals->time + 0.2f;
	}
	else
	{
		m_fCanToss = true;
		m_vecToss = vecToss;
		m_flNextTossCheck = gpGlobals->time + 0.3f;
	}
	return m_fCanToss;
}

// Retail @0x100b2050 (Slot 82): Feind vorhanden und naeher als 48.
bool CHumanScientist::CheckMeleeAttack1(float flDot, float flDist)
{
	return m_hEnemy != nullptr && flDist < 48.0f;
}

void CHumanScientist::Precache()
{
	PrecacheModel("models/evil_scientist.mdl");
	PrecacheModel("models/shell.mdl");
	PrecacheModel("models/shotgunshell.mdl");
	PrecacheModel("models/humanskull.mdl");
	PrecacheModel("models/hgibs.mdl");
	PrecacheModel("sprites/gibhuman.spr");
	PrecacheModel("sprites/gorebot.spr");
	PrecacheEvent(1, "events/monstershotgun.sc");

	PrecacheSound("weapons/gauss_fire1.wav");
	PrecacheSound("weapons/gauss_fire2.wav");
	PrecacheSound("weapons/gauss_charge.wav");
	PrecacheSound("debris/bustflesh1.wav");
	PrecacheSound("bandit/men_check_in.wav");
	PrecacheModel("sprites/gaussbeam1.spr");
	PrecacheModel("sprites/gausspark.spr");
	PrecacheModel("sprites/gorehuman.spr");
	PrecacheModel("sprites/gibhuman.spr");
	// Retail @0x100b1f90: Tod- und Schmerzgeraeusche
	static const char* const pSounds[] = {
		"gscientist/scientist_diebackwards.wav", "gscientist/scientist_dieforwards.wav", "gscientist/scientist_dieforwards2.wav",
		"gscientist/scientist_diesimple.wav", "gscientist/scientist_dieviolent.wav", "gscientist/scientist_dieheadshot.wav",
		"gscientist/hurtmask1.wav", "gscientist/hurtmask2.wav", "gscientist/hurtmask3.wav", "gscientist/hurtmask4.wav"};
	for (const char* pszSound : pSounds)
		PrecacheSound(pszSound);
	UTIL_PrecacheOther("weapon_SPchemicalgun");
	UTIL_PrecacheOther("chem_grenade");
}

// Retail @0x100b23d0: wirft je m_iThrowType eine von vier Chemie-Granaten (Schaden 20/30/40 nach Skill).
void CHumanScientist::ThrowChemGrenade()
{
	ChemParams params;
	params.iDamage = g_iSkillLevel == SKILL_MEDIUM ? 30 : (g_iSkillLevel == SKILL_HARD ? 40 : 20);
	params.iMix = 7;
	bool bContact = false;

	switch (m_iThrowType)
	{
	case 0:
		params.iType = CHEM_EXPLOSIVE;
		params.flBounce = 80;
		params.iViscosity = 4;
		break;
	case 1:
		params.iType = CHEM_EXPLOSIVE;
		params.flBounce = 30;
		params.iViscosity = 43;
		break;
	case 2:
		params.iType = CHEM_ACID;
		params.flBounce = 0;
		params.iViscosity = 100;
		bContact = true;
		break;
	default:
		params.iType = CHEM_EXPLOSIVE;
		params.flBounce = 20;
		params.iViscosity = 69;
		break;
	}

	if (RANDOM_LONG(0, 5) == 0)
		m_iThrowType = RANDOM_LONG(0, 3);

	CChemGrenade* pGrenade;
	if (bContact)
		pGrenade = CChemGrenade::ShootContact(pev, GetGunPosition(), m_vecToss, params);
	else
		pGrenade = CChemGrenade::ShootBouncing(pev, GetGunPosition(), m_vecToss, params); // Zuendzeit (int)(flBounce * 0.1) s im Erzeuger
	if (pGrenade)
		pGrenade->pev->gravity = 1.5f;
}

// Retail @0x100b2690 (Slot 58)
void CHumanScientist::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
		ThrowChemGrenade();
		m_fCanToss = false;
		m_flNextTossCheck = gpGlobals->time + RANDOM_FLOAT(2.0f, 5.0f);
		break;
	case 4:
	case 5:
	{
		// Retail: Nahkampf per CheckTraceHullAttack (64 Einheiten, Schaden 10, DMG_SLASH) mit zufaelligem Rueckschlag.
		CBaseEntity* pHurt = CheckTraceHullAttack(64, 10, DMG_SLASH);
		if (pHurt)
		{
			pHurt->pev->punchangle.x = (float)(RANDOM_LONG(0, 7) - 4);
			pHurt->pev->punchangle.z = (float)(RANDOM_LONG(0, 7) - 4);

			// Retail: bei Monstern Blutspur (Slot 15) und, wenn das Opfer lebt, Ton (Slot 137 @0x100b29d0).
			// Der Schaden fuer TraceBleed ist das Skill-Global 0x10136f4c = sk_xenomemelee_d (Init 0x1001ebc0).
			CBaseMonster* pMonster = pHurt->MyMonsterPointer();
			if (pMonster != nullptr)
			{
				TraceResult trBleed;
				UTIL_TraceLine(GetGunPosition(), pMonster->BodyTarget(GetGunPosition()), dont_ignore_monsters, edict(), &trBleed);
				pMonster->TraceBleed(gSkillData.xenomeMeleeDamage, g_vecZero, &trBleed, DMG_SLASH);
				if (pMonster->IsAlive())
					EMIT_SOUND_DYN(pMonster->edict(), CHAN_VOICE, "debris/bustflesh1.wav", 0.3f, 2.0f, 0, 110);
			}
		}
		break;
	}
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

// Retail @0x100b2890 (Slot 136): Schmerzgeraeusch (hurtmask1-4), hoechstens alle 0,5..0,75 s, nicht in den Zustaenden
// keiner/liegend/Skript/Totstellen.
void CHumanScientist::PainSound()
{
	if (m_flNextPainTime > gpGlobals->time || !IsAlive())
		return;
	if (m_MonsterState == MONSTERSTATE_NONE || m_MonsterState == MONSTERSTATE_PRONE || m_MonsterState == MONSTERSTATE_SCRIPT ||
		m_MonsterState == MONSTERSTATE_PLAYDEAD)
		return;

	m_flNextPainTime = gpGlobals->time + RANDOM_FLOAT(0.5f, 0.75f);
	static const char* const pPainSounds[] = {"gscientist/hurtmask1.wav", "gscientist/hurtmask2.wav", "gscientist/hurtmask3.wav", "gscientist/hurtmask4.wav"};
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pPainSounds[RANDOM_LONG(0, 3)], 1.0f, ATTN_NORM, 0, 100);
}

// Retail @0x100b2b20 (Slot 11): erst das Schmerzgeraeusch, dann der uebliche Schaden.
bool CHumanScientist::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	PainSound();
	return CHumanBase::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// Retail @0x100b2a00 (Slot 13): einmalig die Chemgun abwerfen (Koerpergruppe 3 -> 1), dann das uebliche Killed.
void CHumanScientist::Killed(entvars_t* pevAttacker, int iGib)
{
	if (m_fHasGun)
	{
		m_fHasGun = false;
		SetBodygroup(3, 1);
		Vector vecOrigin, vecAngles;
		GetAttachment(0, vecOrigin, vecAngles);
		CBaseEntity* pDrop = DropItem("weapon_SPchemicalgun", vecOrigin, vecAngles);
		if (pDrop != nullptr)
		{
			pDrop->pev->velocity = Vector(RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(200, 300));
			pDrop->pev->avelocity = Vector(0, RANDOM_FLOAT(200, 400), 0);
		}
	}
	CHumanBase::Killed(pevAttacker, iGib);
}
