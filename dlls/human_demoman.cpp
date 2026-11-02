//=========================================================
// monster_human_demoman (CDemoman, real C++ class name confirmed via
// a raw string literal "CDemoman" in gunman.dll) plus its two
// projectile/ordnance classes demoman_mine (CDemomanMine) and
// demoman_rocket (CDemomanRocket). First gap of west3b/west4a/west5b/
// west6a/west6b/west6c/west6d in the map-driven pass. NOT part of the
// CTalkMonster family (same "hostile CBaseMonster, own combat NPC"
// shape as human_bandit.cpp's CBanditGrunt) - see
// [[project-talking-human-npc-pattern]].
//
// Decompiled fresh from gunman.dll:
//   monster_human_demoman: LINK @0x100ac9a0, vtable @0x101007a8, Spawn
//     @0x100acd50, Precache @0x100acd10 (calls a shared
//     "Human-Bandit"-family base precache FUN_100aa1d0, confirmed via
//     findings/entities/monster_human_demoman.md's xref analysis to
//     also be used by CBanditGrunt/CFriendlyGunman/monster_human_
//     scientist - reproduced here with CBanditGrunt's own confirmed
//     precache list rather than a separate helper class, matching
//     this project's existing pattern of flattening shared C++ base
//     precache into each leaf class), Classify (slot 8, @0x100ad540,
//     confirmed constant 5 = CLASS_ALIEN_MILITARY, same as
//     CBanditGrunt), HandleAnimEvent (slot 58, @0x100ad880). KeyValue
//     (slot 2) is the generic shared stub - unlike CBanditGrunt, this
//     class has NO custom FGD keyvalues.
//   Spawn confirms: bbox implied by model (not separately set - the
//     default CBaseMonster hull from the SDK is used, no
//     UTIL_SetSize call found in the decompile, matching an
//     unusually short Spawn body), pev->skin randomized 30%-vs-random
//     split, health loaded from the sk_demoman_h-backed DAT_10136ea8
//     global, view_ofs.z =
//     68 (0x42880000). Skill-cvar-scaled field at param_1[0xb5]
//     (3/4/6 for skill 1/2/3) copied into param_1[0x87] - exact
//     semantic meaning (likely an internal ammo-count/rocket-reserve
//     field for HandleAnimEvent's rocket branch) not resolved; not
//     reproduced since HandleAnimEvent's rocket firing here has no
//     ammo gate of its own (matches CBanditGrunt's precedent of
//     omitting internal ammo bookkeeping for monster attacks).
//   HandleAnimEvent's 4 real events (byte-confirmed, matches
//     findings' Session-91 table exactly): event 1 = rocket launch
//     (spawns demoman_rocket), event 3 = mine drop (spawns
//     demoman_mine). Both use attachment 0 only when its origin is empty,
//     event 6 = shotgun blast (weapons/sbarrel1.wav +
//     BULLET_MONSTER_BUCKSHOT spread, same shared multi-pellet
//     pattern as CBanditGrunt's MechaVolley/weapon_shotgun), event
//     0xb = CORRECTED (2026-09-05): confirmed via a direct decompile
//     of FUN_10001f90 to be a plain SetBodygroup(2, 2) call - a raw
//     bodygroup-array walk over the model's studio-header
//     numbodyparts/bodypart-offset fields, writing pev->body. There is
//     NO melee damage/traceline in the original for this event at
//     all (the earlier claim of "128-unit melee traceline" here was a
//     fabrication, not a reproduction - see Melee()'s own comment and
//     findings/open_items_audit_2026-09-05.md). CBanditGrunt's event
//     0xb (human_bandit.cpp) is unrelated real combat code, not shared
//     with this class despite the matching event number.
//
//   demoman_mine: LINK @0x100ab950, vtable @0x10100344, Spawn
//     @0x100aba40, Precache @0x100aba10, WaitToExplode (Think)
//     @0x100abd20 - confirmed derived from stock SDK CGrenade
//     (calls CGrenade::Detonate() directly at the end of
//     WaitToExplode). Confirmed: model "models/demomine.mdl",
//     MOVETYPE_TOSS(6), bbox (-4,-4,-8)/(4,4,8), SOLID_BBOX,
//     takedamage=DAMAGE_YES, a sk_mine_d-backed damage field
//     (DAT_10136fe0), and a blinking warning-light child sprite
//     ("sprites/camled.spr") plus a "weapons/dml_lock.wav" tick sound
//     played once armed. WaitToExplode first scans a 50-unit radius;
//     arming then creates the 0.4-scale camled at origin.z+4 and
//     toggles its EF_NODRAW bit for the warning blink. The retail fuse
//     duration is skill-dependent, but its three exact constants have
//     not yet been recovered; the former 3-second fuse remains the
//     explicit compatibility fallback after activation.
//
//   demoman_rocket: LINK @0x100abf40, vtable @0x10100574,
//     RocketExplodeTouch @0x100ac130 (removes the attached flame child,
//     then calls stock CGrenade::ExplodeTouch, confirming
//     CDemomanRocket : CGrenade), IgniteThink @0x100ac160 (spawns a
//     trailing flame-sprite child,
//     plays weapons/rocket1.wav, sends two temp-entity sprite
//     messages for muzzle flash/glow, then switches Think to
//     AccelerateThink), AccelerateThink @0x100ac390 - a full
//     nearby-target homing/steering algorithm (scans entities within
//     a ~1024-unit radius, steers velocity toward the nearest valid
//     enemy, clamps speed between 1200-1400 units/s). The current
//     reconstruction preserves the verified cadence, range and speed
//     interval, using a conservative nearest-live-target steering
//     approximation; its retail target-validity predicate remains open.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "gamerules.h"
#include "effects.h"
#include "human_base.h"

#define DEMOMAN_AE_ROCKET 1
#define DEMOMAN_AE_MINE 3
#define DEMOMAN_AE_SHOTGUN 6
#define DEMOMAN_AE_MELEE 0xb

class CDemomanMine : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT WaitToExplode();

private:
	EHANDLE m_hWarningLight;
	bool m_bArmed = false;
	float m_flDetonateTime = 0.0f;
	float m_flNextBlink = 0.0f;
};
LINK_ENTITY_TO_CLASS(demoman_mine, CDemomanMine);

TYPEDESCRIPTION CDemomanMine::m_SaveData[] =
{
	DEFINE_FIELD(CDemomanMine, m_hWarningLight, FIELD_EHANDLE),
	DEFINE_FIELD(CDemomanMine, m_bArmed, FIELD_BOOLEAN),
	DEFINE_FIELD(CDemomanMine, m_flDetonateTime, FIELD_TIME),
	DEFINE_FIELD(CDemomanMine, m_flNextBlink, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CDemomanMine, CGrenade);

void CDemomanMine::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("demoman_mine");
	SET_MODEL(ENT(pev), "models/demomine.mdl");
	// Retail Spawn @0x100aba40: Box (-4,-4,0)-(4,4,8), takedamage 2.0 (DAMAGE_AIM).
	UTIL_SetSize(pev, Vector(-4, -4, 0), Vector(4, 4, 8));

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	pev->dmg = gSkillData.mineDamage;

	SetThink(&CDemomanMine::WaitToExplode);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CDemomanMine::Precache()
{
	PrecacheModel("models/demomine.mdl");
	PrecacheModel("sprites/camled.spr");
	PrecacheSound("weapons/dml_lock.wav");
}

void CDemomanMine::WaitToExplode()
{
	if (!m_bArmed)
	{
		// Retail calls its custom 50-unit proximity search every Think until
		// a valid target is found. Its exact relationship/visibility predicate
		// is still unresolved; retaining live, non-owner entities gives the
		// mine the verified proximity-triggered lifecycle without treating its
		// owner as a target.
		for (CBaseEntity* pCandidate = nullptr;
			(pCandidate = UTIL_FindEntityInSphere(pCandidate, pev->origin + Vector(0, 0, 4), 50.0f)) != nullptr;)
		{
			if (pCandidate != this && pCandidate->edict() != pev->owner && pCandidate->IsAlive())
			{
				m_bArmed = true;
				m_flDetonateTime = gpGlobals->time + 3.0f;
				m_flNextBlink = gpGlobals->time;
				CSprite* pLight = CSprite::SpriteCreate("sprites/camled.spr", pev->origin + Vector(0, 0, 4), true);
				if (pLight)
				{
					pLight->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxNone);
					pLight->SetScale(0.4f);
					m_hWarningLight = pLight;
				}
				break;
			}
		}
	}

	if (m_bArmed && gpGlobals->time >= m_flNextBlink)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/dml_lock.wav", 1.0f, 0.8f, 0, 100);
		if (CBaseEntity* pLight = m_hWarningLight)
			pLight->pev->effects ^= EF_NODRAW;
		m_flNextBlink = gpGlobals->time + (m_flDetonateTime - gpGlobals->time) * 0.25f;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
	if (m_bArmed && m_flDetonateTime <= gpGlobals->time)
	{
		if (CBaseEntity* pLight = m_hWarningLight)
			UTIL_Remove(pLight);
		m_hWarningLight = nullptr;
		CGrenade::Detonate();
	}
}

class CDemomanRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT RocketExplodeTouch(CBaseEntity* pOther);
	void EXPORT IgniteThink();
	void EXPORT AccelerateThink();

private:
	CSprite* m_pFlame = nullptr;
};
LINK_ENTITY_TO_CLASS(demoman_rocket, CDemomanRocket);

TYPEDESCRIPTION CDemomanRocket::m_SaveData[] =
{
	DEFINE_FIELD(CDemomanRocket, m_pFlame, FIELD_CLASSPTR),
};

IMPLEMENT_SAVERESTORE(CDemomanRocket, CGrenade);

void CDemomanRocket::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("demoman_rocket");
	SET_MODEL(ENT(pev), "models/dmlrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->dmg = gSkillData.demomanRocketDamage;
	SetTouch(&CDemomanRocket::RocketExplodeTouch);

	UTIL_MakeVectors(pev->angles);
	pev->velocity = gpGlobals->v_forward * 1200; // fixed speed, see file header (homing not reproduced)

	// Retail Spawn arms IgniteThink after 0.1 s; the sound, trail and
	// guided flight do not begin at entity creation.
	pev->nextthink = gpGlobals->time + 0.1f;
	SetThink(&CDemomanRocket::IgniteThink);
}

void CDemomanRocket::Precache()
{
	PrecacheModel("models/dmlrocket.mdl");
	PrecacheSound("weapons/rocket1.wav");
	PrecacheModel("sprites/flame.spr");
}

void CDemomanRocket::RocketExplodeTouch(CBaseEntity* pOther)
{
	if (m_pFlame)
	{
		UTIL_Remove(m_pFlame);
		m_pFlame = nullptr;
	}
	CGrenade::ExplodeTouch(pOther);
}

void CDemomanRocket::IgniteThink()
{
	// Retail creates a kRenderGlow flame child, attaches it at attachment 0,
	// and removes it on impact or once the rocket leaves the world.
	m_pFlame = CSprite::SpriteCreate("sprites/flame.spr", pev->origin, true);
	if (m_pFlame)
	{
		m_pFlame->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxNone);
		m_pFlame->SetAttachment(edict(), 0);
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/rocket1.wav", 0.5f, 0.5f, 0, 100);
	SetThink(&CDemomanRocket::AccelerateThink);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CDemomanRocket::AccelerateThink()
{
	if (!IsInWorld())
	{
		if (m_pFlame)
			UTIL_Remove(m_pFlame);
		m_pFlame = nullptr;
		UTIL_Remove(this);
		return;
	}

	CBaseEntity* pTarget = nullptr;
	float flBestDistance = 1024.0f;
	for (CBaseEntity* pCandidate = nullptr;
		(pCandidate = UTIL_FindEntityInSphere(pCandidate, pev->origin, 1024.0f)) != nullptr;)
	{
		if (pCandidate == this || pCandidate->edict() == pev->owner || !pCandidate->IsAlive())
			continue;
		const float flDistance = (pCandidate->Center() - pev->origin).Length();
		if (flDistance < flBestDistance)
		{
			pTarget = pCandidate;
			flBestDistance = flDistance;
		}
	}

	if (pTarget)
	{
		const Vector vecDesired = (pTarget->Center() - pev->origin).Normalize();
		pev->velocity = (pev->velocity.Normalize() * 0.9f + vecDesired * 0.1f).Normalize() * pev->velocity.Length();
	}

	const float flSpeed = pev->velocity.Length();
	if (flSpeed > 1400.0f)
		pev->velocity = pev->velocity.Normalize() * 1400.0f;
	else if (flSpeed < 1200.0f)
		pev->velocity = pev->velocity.Normalize() * 1200.0f;

	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->nextthink = gpGlobals->time + 0.1f;
}

class CDemoman : public CHumanBase
{
public:
	int MonsterProfile() override { return 1; } // retail slot 59
	bool GetHeadshotGibBodygroup(int& group, int& value) override { group = 1; value = 3; return true; }
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_MILITARY; }
	bool FVisible(CBaseEntity* pEntity) override; // retail vtable slot 57 (0x100acb90)
	// CORRECTION (2026-09-05): missing SetYawSpeed() override left
	// pev->yaw_speed at 0 (CBaseMonster's default is a no-op) - the
	// monster couldn't turn toward its enemy at all. Same fix as
	// CFriendlyGunman (human_gunman.cpp); see
	// findings/open_items_audit_2026-09-05.md for the full story.
	// Retail @0x100ad480 (Slot 101): Gehen/Drehen 130, Fliegen/Gleiten 20, Angriffe 80, sonst 100.
	void SetYawSpeed() override
	{
		switch (m_Activity)
		{
		case ACT_WALK:
		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
			pev->yaw_speed = 130;
			break;
		case ACT_FLY:
		case ACT_GLIDE:
			pev->yaw_speed = 20;
			break;
		case ACT_RANGE_ATTACK1:
		case ACT_RANGE_ATTACK2:
		case ACT_MELEE_ATTACK1:
		case ACT_MELEE_ATTACK2:
			pev->yaw_speed = 80;
			break;
		default:
			pev->yaw_speed = 100;
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
	// Retail @0x100adb90 (Slot 96): Aktivitaeten werden vor der Sequenzsuche ersetzt: Idle im Kampf -> ACT_IDLE_ANGRY (47),
	// Gehen bei Gesundheit <= sk_demoman_limp_h -> ACT_WALK_HURT (48), ACT_RANGE_ATTACK1 -> Sequenz "shootshotgun"
	// (pev->weapons == 4) oder "launchrocket".
	void SetActivity(Activity NewActivity) override
	{
		if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && m_pCine != nullptr)
		{
			m_Activity = NewActivity;
			m_IdealActivity = NewActivity;
			return;
		}

		int iSequence;
		if (NewActivity == ACT_IDLE)
		{
			if (m_MonsterState == MONSTERSTATE_COMBAT)
				NewActivity = ACT_IDLE_ANGRY;
			iSequence = LookupActivity(NewActivity);
		}
		else if (NewActivity == ACT_WALK)
		{
			if (gSkillData.demomanLimpHealth >= pev->health)
				NewActivity = ACT_WALK_HURT;
			iSequence = LookupActivity(NewActivity);
		}
		else if (NewActivity == ACT_RANGE_ATTACK1)
		{
			iSequence = LookupSequence(pev->weapons == 4 ? "shootshotgun" : "launchrocket");
		}
		else
		{
			iSequence = LookupActivity(NewActivity);
		}

		m_Activity = NewActivity;
		if (iSequence < 0)
		{
			ALERT(at_console, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity);
			pev->sequence = 0;
			return;
		}

		if (pev->sequence != iSequence || !m_fSequenceLoops)
			pev->frame = 0;
		pev->sequence = iSequence;
		ResetSequenceInfo();
		SetYawSpeed();
	}
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;

	// Retail-Slots 80/81/82 (@0x100acff0 / @0x100ad130 / @0x100acfb0) und 87 (@0x100ada00).
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckRangeAttack2(float flDot, float flDist) override;
	bool CheckMeleeAttack1(float flDot, float flDist) override;
	Schedule_t* GetScheduleOfType(int Type) override;
	// Retail-Slots 139/141/142/143 (@0x100adc80 / @0x100aca30 / @0x100aca40 / @0x100adca0).
	void Crouch() override {}
	bool CanFireRange2() override { return true; }
	bool CanCrouch() override { return false; }

private:
	float m_flNextRocketTime = 0; // Retail +0x2fc
	void LaunchRocket();
	void DropMine();
	void Shotgun();
	unsigned short m_usShotEvent = 0; // Retail +0x2f0, events/monstershotgun.sc
	void Melee();
};
LINK_ENTITY_TO_CLASS(monster_human_demoman, CDemoman);

// Retail slot 57 (0x100acb90, FVisible(entity)): same NOTARGET and water-level
// checks as the base, but the sight line ends at the target's Center() (retail
// vtable slot 52), not at its EyePosition() as in CBaseEntity::FVisible. The
// Vector variant (slot 56) matches the base.
bool CDemoman::FVisible(CBaseEntity* pEntity)
{
	if (FBitSet(pEntity->pev->flags, FL_NOTARGET))
		return false;

	// don't look through water (same condition as the base)
	if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3) || (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
		return false;

	TraceResult tr;
	UTIL_TraceLine(EyePosition(), pEntity->Center(), ignore_monsters, ignore_glass, ENT(pev), &tr);
	return tr.flFraction == 1.0;
}

void CDemoman::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/demolitionman.mdl");
	// Blutfarbe 247 kommt aus der Profiltabelle (MonsterProfile() = 1); Retail setzt +0x238 im Spawn nicht.
	pev->health = gSkillData.demomanHealth;
	m_flNextGrenadeCheck = gpGlobals->time;
	m_flNextRocketTime = gpGlobals->time;

	// Retail @0x100acd50: 70 % Skin 0 mit zufaelligem Kopf (Body 0..2), sonst Skin 1.
	if (RANDOM_LONG(0, 99) < 70)
	{
		pev->skin = 0;
		pev->body = RANDOM_LONG(0, 2);
	}
	else
	{
		pev->skin = 1;
	}

	// Magazin je Schwierigkeitsgrad: leicht/normal 3 (Skill 2: 4, Skill 3: 6), Retail @0x100acd50.
	m_cClipSize = g_iSkillLevel == SKILL_MEDIUM ? 4 : (g_iSkillLevel == SKILL_HARD ? 6 : 3);
	// Retail @0x100acd50 ruft MonsterInit (Slot 67) VOR dem Menschen-Init @0x100aa080; dessen Faehigkeiten
	// (0x7c8) und Sichtfeld 0,2 ersetzen danach die Profilwerte.
	pev->view_ofs = Vector(0, 0, 68); // nur gegen die SDK-Warnung; Retail setzt es erst nach MonsterInit
	MonsterInit();
		HumanInit();
	m_iRollSide = 1;
	m_cAmmoLoaded = m_cClipSize;
	// Some Gunman models do not carry $eyeposition. Restore the confirmed
	// gameplay eye height after MonsterInit's model-derived lookup.
	pev->view_ofs = Vector(0, 0, 68);
}

// Retail @0x100acff0 (Slot 80): Entfernung < 4096, Winkel > 0,5, Ziel > 64 entfernt, freie Schusslinie; ist das
// Ziel naeher als 256, wird der Raketentimer auf +5 s gesetzt. Retail testet zusaetzlich ein Statusbit (this+0xbc & 1)
// bei begrenzter Munition; nicht zugeordnet, daher nicht nachgebaut.
bool CDemoman::CheckRangeAttack1(float flDot, float flDist)
{
	if (flDist >= 4096.0f || flDot <= 0.5f || m_hEnemy == nullptr || flDist <= 64.0f)
		return false;

	if (flDist < 256.0f)
		m_flNextRocketTime = gpGlobals->time + 5.0f;

	TraceResult tr;
	UTIL_TraceLine(GetGunPosition(), m_hEnemy->BodyTarget(GetGunPosition()), dont_ignore_monsters, edict(), &tr);
	if (tr.flFraction != 1.0f && tr.pHit != m_hEnemy->edict())
		return false;
	return true;
}

// Retail @0x100ad130 (Slot 81): Granatenwurf-Pruefung wie CHGrunt, aber ohne Waffenbit-/Bewegungstest, bis 1024
// Entfernung, Wurfparabel zu 80 % ueber VecCheckToss sonst VecCheckThrow, naechste Pruefung nach 0,3 s.
bool CDemoman::CheckRangeAttack2(float flDot, float flDist)
{
	if (flDist > 1024.0f)
	{
		m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
		return false;
	}

	if (gpGlobals->time < m_flNextGrenadeCheck)
		return m_fThrowGrenade;

	if (m_hEnemy == nullptr)
		return false;

	if ((m_hEnemy->pev->flags & FL_ONGROUND) == 0 && m_hEnemy->pev->waterlevel == 0 && m_vecEnemyLKP.z > pev->absmax.z)
	{
		m_fThrowGrenade = false;
		return false;
	}

	Vector vecTarget;
	if (RANDOM_LONG(0, 1) != 0)
		vecTarget = Vector(m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z);
	else
		vecTarget = m_vecEnemyLKP;

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

	Vector vecToss;
	if (RANDOM_LONG(0, 10) < 8)
		vecToss = VecCheckToss(pev, GetGunPosition(), vecTarget, 0.5f);
	else
		vecToss = VecCheckThrow(pev, GetGunPosition(), vecTarget, gSkillData.grenadeSpeed, 0.5f); // Retail @0x100ad3a0: sk_gren_speed

	if (vecToss != g_vecZero)
	{
		m_vecTossVelocity = vecToss;
		m_fThrowGrenade = true;
		m_flNextGrenadeCheck = gpGlobals->time + 0.3f;
	}
	else
	{
		m_fThrowGrenade = false;
		m_flNextGrenadeCheck = gpGlobals->time + 1.0f;
	}
	return m_fThrowGrenade;
}

// Retail @0x100acfb0 (Slot 82)
bool CDemoman::CheckMeleeAttack1(float flDot, float flDist)
{
	return m_hEnemy != nullptr && flDist <= 64.0f && flDot >= 0.7f;
}

// Retail @0x100ada00 (Slot 87): Zweitangriff = Rakete (Ziel weiter als 128 und Raketentimer abgelaufen, danach
// 5..8 s Pause) oder Schrotflinte.
Schedule_t* CDemoman::GetScheduleOfType(int Type)
{
	if (Type != SCHED_RANGE_ATTACK2)
		return CHumanBase::GetScheduleOfType(Type);

	if (m_hEnemy != nullptr && (m_hEnemy->pev->origin - pev->origin).Length() > 128.0f && m_flNextRocketTime < gpGlobals->time)
	{
		m_flNextRocketTime = gpGlobals->time + RANDOM_LONG(5, 8);
		pev->weapons = 2;
		return CHumanBase::GetScheduleOfType(HB_SCHED_RANGE_ROCKET);
	}

	pev->weapons = 4;
	return CHumanBase::GetScheduleOfType(HB_SCHED_RANGE_SHOTGUN);
}

void CDemoman::Precache()
{
	PrecacheModel("models/demolitionman.mdl");
	PrecacheModel("models/shell.mdl");
	PrecacheModel("models/shotgunshell.mdl");
	PrecacheModel("models/humanskull.mdl");
	PrecacheModel("models/hgibs.mdl");
	PrecacheModel("sprites/zbeam5.spr");
	PrecacheModel("sprites/gaussbeam1.spr");
	PrecacheModel("sprites/gausspark.spr");
	PrecacheModel("sprites/gorehuman.spr");
	PrecacheModel("sprites/gibhuman.spr");
	PrecacheModel("sprites/gorebot.spr");
	m_usShotEvent = PrecacheEvent(1, "events/monstershotgun.sc");

	PrecacheSound("weapons/gauss_fire1.wav");
	PrecacheSound("weapons/gauss_fire2.wav");
	PrecacheSound("weapons/gauss_charge.wav");
	PrecacheSound("debris/bustflesh1.wav");
	PrecacheSound("bandit/men_check_in.wav");
	PrecacheSound("weapons/sbarrel1.wav");
	// Retail @0x100acaf0 (Tabelle @0x1012a958): alle Demoman-Toene
	static const char* const pDemomanSounds[] = {
		"demoman/demo_flinch1.wav", "demoman/demo_flinch2.wav", "demoman/demo_flinch3.wav", "demoman/demo_kick.wav",
		"demoman/demo_diebackwards.wav", "demoman/demo_dieforwards.wav", "demoman/demo_dieheadshot.wav",
		"demoman/demo_diesimple.wav", "demoman/demo_dieviolent.wav", "demoman/demo_dropmine.wav",
		"demoman/demo_launchrocket.wav", "demoman/demo_run_footstep1.wav", "demoman/demo_run_footstep2.wav",
		"demoman/demo_throwgrenade.wav", "demoman/demo_victorydance.wav", "demoman/demo_walk_footstep1.wav",
		"demoman/demo_walk_footstep2.wav", "demoman/demo_idlelong.wav", "weapons/sbarrel1.wav"};
	for (const char* pszSound : pDemomanSounds)
		PrecacheSound(pszSound);

	UTIL_PrecacheOther("demoman_mine");
	UTIL_PrecacheOther("demoman_rocket");
}

void CDemoman::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case DEMOMAN_AE_ROCKET:
		LaunchRocket();
		break;
	case DEMOMAN_AE_MINE:
		DropMine();
		break;
	case DEMOMAN_AE_SHOTGUN:
		Shotgun();
		break;
	case DEMOMAN_AE_MELEE:
		Melee();
		break;
	default:
		CHumanBase::HandleAnimEvent(pEvent);
		break;
	}
}

void CDemoman::LaunchRocket()
{
	Vector vecOrigin, vecAngles;
	GetAttachment(0, vecOrigin, vecAngles);
	if (UTIL_PointContents(vecOrigin) != CONTENTS_EMPTY)
		return;

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "demoman/demo_launchrocket.wav", 1.0, ATTN_NORM);
	CBaseEntity::Create("demoman_rocket", vecOrigin, vecAngles, edict());
}

void CDemoman::DropMine()
{
	Vector vecOrigin, vecAngles;
	GetAttachment(0, vecOrigin, vecAngles);
	if (UTIL_PointContents(vecOrigin) != CONTENTS_EMPTY)
		return;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "demoman/demo_dropmine.wav", 1.0f, 0.8f, 0, 100);
	CBaseEntity::Create("demoman_mine", vecOrigin, vecAngles, edict());
}

// Retail @0x100a9b00 (Basis-Schrotflintenschuss): Ton, Muendungsfeuer, 6 Kugeln mit 15-Grad-Streuung auf 4096 Einheiten und
// Schaden sk_demoman_sg_d; Munition -1, Zielneigung ueber SetBlending. Vereinfachung: Ereignisparameter (Ursprung/Winkel)
// aus Muendung und Schussrichtung; Kugeltyp 3 (Retail-Wert) mit festem Schaden.
void CDemoman::Shotgun()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/sbarrel1.wav", 1.0f, ATTN_NORM, 0, 100);

	if (!m_hEnemy)
		return;

	const Vector vecShootOrigin = GetGunPosition();
	const Vector vecShootDir = ShootAtEnemy(vecShootOrigin);
	const Vector angDir = UTIL_VecToAngles(vecShootDir);

	UTIL_MakeVectors(pev->angles);
	PLAYBACK_EVENT_FULL(FEV_RELIABLE, edict(), m_usShotEvent, 0, vecShootOrigin, angDir, 0, 0, 0, 0, 0, 0);
	FireBullets(6, vecShootOrigin, vecShootDir, Vector(0.13053f, 0.13053f, 0.13053f), 4096, BULLET_PLAYER_357, 0, (int)gSkillData.demomanShotgunDamage);

	pev->effects |= EF_MUZZLEFLASH;
	if (!InfiniteAmmo())
		m_cAmmoLoaded--;
	SetBlending(0, angDir.x);
}

// Retail @0x100ace70 (Slot 13): mit 30 % Wahrscheinlichkeit faellt eine DML-Munition (ammo_dmlsingle) aus dem Koerper
// (Flag +0x300 = m_iRollSide, im Spawn auf 1 gesetzt), dann das uebliche Killed.
void CDemoman::Killed(entvars_t* pevAttacker, int iGib)
{
	if (m_iRollSide != 0)
	{
		m_iRollSide = 0;
		if (RANDOM_LONG(0, 99) < 30)
		{
			Vector vecOrigin, vecAngles;
			GetAttachment(0, vecOrigin, vecAngles);
			CBaseEntity* pDrop = DropItem("ammo_dmlsingle", vecOrigin, vecAngles);
			if (pDrop != nullptr)
			{
				pDrop->pev->velocity = Vector(RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(200, 300));
				pDrop->pev->avelocity = Vector(0, RANDOM_FLOAT(200, 400), 0);
			}
		}
	}
	CHumanBase::Killed(pevAttacker, iGib);
}

void CDemoman::Melee()
{
	// CORRECTION (2026-09-05): a fresh re-decompile as part of resolving
	// the FUN_10001f90 interpretation contradiction flagged in
	// findings/open_items_audit_2026-09-05.md shows the original event
	// 0xb body is JUST `SetBodygroup(2, 2)` (FUN_10001f90 is a raw
	// SetBodygroup-equivalent helper - confirmed by decompiling it
	// directly: it walks the model's bodypart array via the numbodyparts/
	// bodypart-offset studio-header fields and writes pev->body,
	// nowhere touching combat/traceline code). There is NO melee damage
	// call at all in the original for this event - the previous version
	// of this function (a 128-unit distance check + TakeDamage) was a
	// fabrication, not a reproduction of confirmed behavior. Bandit's
	// own event 0xb (human_bandit.cpp) IS a real, more elaborate
	// grab-then-traceline attack - it does not share code with this
	// class's event 0xb despite the same event number and a similar
	// name in this project's convention.
	SetBodygroup(2, 2);
}
