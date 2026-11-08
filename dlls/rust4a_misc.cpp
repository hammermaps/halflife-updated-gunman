//=========================================================
// RUST4A's remaining classes with no SDK precedent: monster_rustgunr
// (CGunner, + its "anime_rocket" runtime projectile) and meteor_god
// (CMeteorGod + its runtime meteor chunk). weapon_beamgun/
// weapon_minigun, RUST4A's other two gaps, are already-deferred
// player weapons.
//
// Decompiled fresh from gunman.dll this session:
//   monster_rustgunr: LINK @0x100bc320, vtable @0x1010265c - matches
//     findings/entities/gunner_family.md's independently-found vtable
//     value exactly, confirming this really is CGunner (the doc's
//     "true 1:1 identity across monster_gunner/monster_rustgnr/
//     monster_rustgunr" claim - all three share this one vtable).
//     Spawn @0x100bec80, Precache @0x100bee40, Classify @0x100bc230
//     (constant 8 = CLASS_ALIEN_PREY), CGunner::ToggleTakedamage
//     @0x100bec50 (named via the symbol table). Confirmed bbox
//     (-32,-32,0)/(32,32,150), health RANDOM(75,90) (matches the
//     doc's "RANDOM(0,15)+75" exactly), view_ofs (0,0,115).
//     ToggleTakedamage's real logic (re-derived fresh, more precise
//     than the doc's "0 or 2.0, vermutlich Schadensresistenz" guess):
//     it's pev->takedamage itself being toggled directly between
//     DAMAGE_NO(0) and 2.0 - a literal invulnerability on/off switch,
//     not a separate shield/resistance field.
//     findings/entities/gunner_family.md's exhaustive multi-session
//     search establishes that all 4 of CGunner's fire animation
//     events (6/7/8/9, confirmed via HandleAnimEvent @0x100bc7a0 -
//     not separately re-decompiled this session given how thoroughly
//     the doc already traced it across 6 independent search
//     approaches) spawn the same directly-constructed "anime_rocket"
//     projectile (CAnimeRocket) regardless of the animation's
//     "firegun"/"firerocket" name suggesting otherwise; the
//     separately-registered gunner_rocket/CGunnerRocket class is
//     documented as very likely genuine dead code (registered but
//     unreachable from any code path, confirmed in both retail and
//     the E3 beta) and is deliberately NOT implemented here.
//   meteor_god: LINK @0x100a5e50, ctor helper @0x100a5f00 (vtable
//     @0x100ffdb8), Spawn @0x100a62f0, Precache @0x100a5f70, KeyValue
//     @0x100a5ff0. Confirmed all 8 FGD-documented keyvalues
//     (typestreak/typelarge/typesmall/dropsizex/dropsizey/
//     mindropfreq/maxdropfreq/pitch) plus a "debris" boolean, and
//     their ctor defaults. Architecturally a near-twin of lava_god
//     (mayan3a_misc.cpp) - invisible (EF_NODRAW), non-solid
//     (SOLID_NOT/MOVETYPE_NONE) periodic spawner, paired with the
//     targeted meteor_target spawner below.
//
// Simplified relative to the original (documented per-case):
// - CGunner: no custom Schedule_t AI (same established
//   simplification as every other monster in this project). StartTask
//   reproduces the melee wind-up sound; other task handling delegates. The
//   gunr_eye sprite and its Retail slot-117 brightness update are
//   implemented; the separate random virtual call in slot 117 remains
//   unresolved.
// - CAnimeRocket (anime_rocket): findings/entities/gunner_family.md's
//   Session-93 full decompile documents genuine homing
//   (proportional-navigation AccelerateThink, ramping to 1000 u/s)
//   and RadiusDamage(1024, x3 multiplier) on impact - reproduced here
//   as a straight-flying (no homing) explosive with a plausible fixed
//   damage/radius instead, same simplification level already used for
//   vehicle_tank_rocket/monster_targetrocket_proj/monster_tank_rocket
//   elsewhere in this project.
// - CMeteorGod: the exact eruption math (per-type drop weighting,
//   dropsizex/y scaling, pitch) is approximated with a simple
//   randomized-offset periodic spawn within a fixed area, matching
//   CLavaGod's already-established simplification pattern rather than
//   reproducing byte-exact selection logic between the 3 configured
//   meteor types.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "animation.h"
#include "monsters.h"
#include "effects.h"
#include "explode.h"
#include "scriptevent.h"
#include "saverestore.h"
#include "weapons.h"
#include "decals.h"
#include "UserMessages.h"

//=========================================================
// The runtime-only homing-in-name-only rocket fired by CGunner's 4
// fire animation events (see file header re: homing simplification).
//=========================================================
class CAnimeRocket : public CGrenade
{
public:
	static CAnimeRocket* CreateForGunner(CBaseEntity* pOwner, CBaseEntity* pEnemy, const Vector& vecSpawnOrigin);
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT IgniteThink();
	void EXPORT AccelerateThink();
	void EXPORT RocketTouch(CBaseEntity* pOther);

private:
	CSprite* m_pFlame = nullptr;
	int m_iTrail = 0; // Sprite-Modellindex fuer TE_BEAMFOLLOW
};
LINK_ENTITY_TO_CLASS(anime_rocket, CAnimeRocket);

TYPEDESCRIPTION CAnimeRocket::m_SaveData[] =
	{
		DEFINE_FIELD(CAnimeRocket, m_pFlame, FIELD_CLASSPTR),
	};

IMPLEMENT_SAVERESTORE(CAnimeRocket, CGrenade);

void CAnimeRocket::Precache()
{
	PrecacheModel("models/rocket.mdl");
	m_iTrail = PrecacheModel("sprites/flame.spr");
	PrecacheSound("weapons/rocket1.wav");
	PrecacheSound("weapons/kaboom1.wav");
	PrecacheSound("weapons/kaboom2.wav");
	PrecacheSound("weapons/kaboom3.wav");
}

void CAnimeRocket::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("anime_rocket");
	SET_MODEL(ENT(pev), "models/rocket.mdl");
	UTIL_SetSize(pev, Vector(-2, -2, -2), Vector(2, 2, 2));
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->dmg = gSkillData.rustgunnerRocketDamage;
	if (m_hEnemy)
	{
		// Retail CAnimeRocket::Spawn builds a point in front of the enemy,
		// with independent lateral/up offsets, then aims the 500 u/s launch
		// vector at that point. CreateForGunner sets the attachment origin
		// before DispatchSpawn so this vector starts from the actual muzzle.
		UTIL_MakeVectors(m_hEnemy->pev->angles);
		const float flUpOffset = RANDOM_FLOAT(0.0f, 40.0f) - 20.0f;
		const float flRightOffset = RANDOM_FLOAT(0.0f, 30.0f) - 15.0f;
		const Vector vecAimPoint = m_hEnemy->pev->origin + gpGlobals->v_forward * 64.0f +
			gpGlobals->v_right * flRightOffset + gpGlobals->v_up * flUpOffset;
		pev->velocity = (vecAimPoint - pev->origin).Normalize() * 500.0f;
		pev->angles = UTIL_VecToAngles(pev->velocity);
	}
	SetTouch(&CAnimeRocket::RocketTouch);
	SetThink(&CAnimeRocket::IgniteThink);
	pev->nextthink = gpGlobals->time + 0.1f;
}

CAnimeRocket* CAnimeRocket::CreateForGunner(CBaseEntity* pOwner, CBaseEntity* pEnemy, const Vector& vecSpawnOrigin)
{
	if (!pOwner || !pEnemy)
		return nullptr;

	// Set all Spawn inputs, including the randomized attachment origin, before
	// DispatchSpawn: Spawn derives its initial velocity from pev->origin.
	CAnimeRocket* pRocket = GetClassPtr(static_cast<CAnimeRocket*>(nullptr));
	pRocket->pev->classname = MAKE_STRING("anime_rocket");
	pRocket->pev->owner = pOwner->edict();
	pRocket->m_hEnemy = pEnemy;
	UTIL_SetOrigin(pRocket->pev, vecSpawnOrigin);
	DispatchSpawn(pRocket->edict());
	return pRocket;
}


void CAnimeRocket::IgniteThink()
{
	// Nachtrag 2026-09-06 (Live-Report mit Retail-Referenzscreenshot: die
	// echte Rakete zieht einen duennen, hellen orangen Streif, keine graue
	// Rauchwolke): CAnimeRocket::IgniteThink (0x100ba320) frisch erneut
	// decompiliert - nutzt entgegen der vorherigen TE_BEAMFOLLOW-Naeherung
	// GAR KEIN Temp-Entity-Beam, sondern haengt ueber den gemeinsamen
	// Helfer FUN_10015400 eine echte "env_sprite"-Entity mit dem Modell
	// "sprites/flame.spr" (String direkt aus der Binary gelesen,
	// 0x10126a5c) an - additiv, Farbe/Alpha auf volle 255 gesetzt (4
	// aufeinanderfolgende float-Felder = 255.0). Mit `CSprite::
	// SetAttachment()` (MOVETYPE_FOLLOW + pev->aiment) nachgebildet, dem
	// nativen GoldSrc-Mechanismus fuer genau diesen Zweck, statt eines
	// TE_BEAMFOLLOW-Ersatzes.
	m_pFlame = CSprite::SpriteCreate("sprites/flame.spr", pev->origin, true);
	if (m_pFlame)
	{
		m_pFlame->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
		m_pFlame->SetScale(1.0);
		// models/rocket.mdl hat keine Attachment-Punkte -> der Sprite blieb am
		// Abschusspunkt stehen (kein Schweif). Stattdessen der Rakete folgen.
		m_pFlame->pev->aiment = edict();
		m_pFlame->pev->body = 0;
		m_pFlame->pev->skin = 0;
		m_pFlame->pev->movetype = MOVETYPE_FOLLOW;
		m_pFlame->pev->solid = SOLID_NOT;
	}

	// Sichtbarer Schweif wie bei CApacheHVR (apache.cpp:1063): der flame-Sprite
	// folgt der Rakete in Xash nicht sichtbar, TE_BEAMFOLLOW dagegen schon.
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(m_iTrail);
	WRITE_BYTE(15);  // life
	WRITE_BYTE(6);   // width
	WRITE_BYTE(224); // r
	WRITE_BYTE(224); // g
	WRITE_BYTE(255); // b
	WRITE_BYTE(200); // brightness
	MESSAGE_END();

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/rocket1.wav", 1.0, ATTN_NORM, 0, 100);
	SetThink(&CAnimeRocket::AccelerateThink);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CAnimeRocket::AccelerateThink()
{
	constexpr float WORLD_BOUNDS = 4096.0f;
	if (pev->origin.x < -WORLD_BOUNDS || pev->origin.x > WORLD_BOUNDS ||
		pev->origin.y < -WORLD_BOUNDS || pev->origin.y > WORLD_BOUNDS ||
		pev->origin.z < -WORLD_BOUNDS || pev->origin.z > WORLD_BOUNDS)
	{
		if (m_pFlame)
			UTIL_Remove(m_pFlame);
		UTIL_Remove(this);
		return;
	}

	// Retail 0x100ba4f0 uses the inherited enemy handle and a bounded
	// proportional-navigation correction. The exact clamp is not yet named,
	// but retaining speed while steering a small fraction per 0.1 s preserves
	// its confirmed target-following lifecycle without the former straight shot.
	if (m_hEnemy && m_hEnemy->IsAlive())
	{
		Vector vecToTarget = (m_hEnemy->BodyTarget(pev->origin) - pev->origin).Normalize();
		const float flSpeed = pev->velocity.Length();
		if (flSpeed > 0.0f)
			pev->velocity = (pev->velocity.Normalize() * 0.85f + vecToTarget * 0.15f).Normalize() * flSpeed;
	}

	const float flSpeed = pev->velocity.Length();
	if (flSpeed < 1000.0f && flSpeed > 0.0f)
		pev->velocity = pev->velocity.Normalize() * ((flSpeed + 100.0f < 1000.0f) ? flSpeed + 100.0f : 1000.0f);

	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CAnimeRocket::RocketTouch(CBaseEntity* pOther)
{
	// Retail ignores contacts with another rocket from the same runtime class.
	if (pOther && FClassnameIs(pOther->pev, "anime_rocket"))
		return;

	if (m_pFlame)
	{
		UTIL_Remove(m_pFlame);
		m_pFlame = nullptr;
	}

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/kaboom1.wav", 1.0, ATTN_NORM, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/kaboom2.wav", 1.0, ATTN_NORM, 0, 100);
		break;
	default:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/kaboom3.wav", 1.0, ATTN_NORM, 0, 100);
		break;
	}

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(40);
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects).
	UTIL_ExplosionEffects(pev->origin, 1024.0f);

	::RadiusDamage(pev->origin, pev, VARS(pev->owner), pev->dmg * 3.0f, 1024.0f, CLASS_NONE, DMG_BLAST);
	UTIL_Remove(this);
}

//=========================================================
// monster_rustgunr - CGunner. Also aliased (CORRECTION 2026-09-05:
// findings/entities/gunner_family.md's confirmed 1:1 vtable identity
// across all three classnames - see LINK addresses/vtable value in the
// file header above) as monster_gunner/monster_rustgnr, which never
// appear placed in any retail map (grep across all maps-src/*.ent
// finds only monster_rustgunr - RUST4A/rust6a/rust6c/rust7a), but are
// aliased here for classname completeness since they are genuinely the
// same class, not a documented gap.
//=========================================================
#define GUNNER_AE_FIRE_RIGHT_1 6
#define GUNNER_AE_FIRE_RIGHT_2 7
#define GUNNER_AE_FIRE_LEFT_1 8
#define GUNNER_AE_FIRE_LEFT_2 9

class CGunner : public CBaseMonster
{
public:
	int MonsterProfile() override { return 14; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_PREY; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	// Retail slot 101 @0x100bc700 (2026-10-31): 120 for idle/walk/run/melee 2, 180 for turns, 50 for rocket attacks.
	void SetYawSpeed() override;
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this. Retail slot 96 @0x100beb10 additionally picks the
	// ACT_RANGE_ATTACK1 sequence by arm ("firerocketsleft" / "firerocketright", side from CheckRangeAttack1).
	void SetActivity(Activity NewActivity) override;
	void AlertSound() override;
	void IdleSound() override;
	void PainSound() override;
	void PlayScriptedSentence(const char* pszSentence, float duration, float volume, float attenuation, bool bConcurrent, CBaseEntity* pListener) override;
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckRangeAttack2(float flDot, float flDist) override { return false; }
protected:
	// Both variants (gunner and friendly) share the muzzle line-of-sight test; returns the clear-sides mask
	// (1 = right forearm / attachment 2, 2 = left forearm / attachment 3).
	int TestMuzzleSides();
public:
	bool CheckMeleeAttack1(float flDot, float flDist) override { return flDist <= 100.0f && flDot >= 0.5f; }
	bool CheckMeleeAttack2(float flDot, float flDist) override { return flDist <= 100.0f && flDot >= 0.5f; }
	Schedule_t* GetSchedule() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void PrescheduleThink() override;
	// Retail slot 11 @0x100bcef0: damage from the gunner's own projectiles is ignored, a single hit is
	// capped at 30, shock/energy-beam damage (0x500) is halved, and every hit gibs (DMG_ALWAYSGIB).
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	// Retail slot 62 @0x100bd810: animation speed 1.5 on every AI tick (the footstep-effect branch is not ported).
	void RunAI() override;
	// Retail slot 85 plays the wind-up sound for either melee task, then delegates.
	void StartTask(Task_t* pTask) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	// Nachtrag 2026-09-06 (In-Game-Test-Meldung "Fleisch-Gibs statt
	// Metallteile"): models/gunnergibs.mdl war schon vorher precacht,
	// aber nie in GibMonster() verwendet - siehe CustomGibModel() in
	// basemonster.h.
	const char* CustomGibModel() override { return "models/gunnergibs.mdl"; }

public:
	// Testhilfe (`dbg_eye`): Auge dauerhaft auf Zielhelligkeit 200 zwingen.
	void DebugForceEye() { m_eyeBrightness = 200; if (m_pEyeGlow) { m_pEyeGlow->pev->renderamt = 200; m_pEyeGlow->pev->effects &= ~EF_NODRAW; } }
	void DebugEyeScale(float flScale) { if (m_pEyeGlow) { m_pEyeGlow->pev->scale = flScale; ALERT(at_aiconsole, "[eye] sprite entscale=%.1f origin=(%.0f %.0f %.0f) rendercolor=(%d %d %d)\n", flScale, m_pEyeGlow->pev->origin.x, m_pEyeGlow->pev->origin.y, m_pEyeGlow->pev->origin.z, (int)m_pEyeGlow->pev->rendercolor.x, (int)m_pEyeGlow->pev->rendercolor.y, (int)m_pEyeGlow->pev->rendercolor.z); } }

protected:
	void FireRocket(int iAttachment, bool bRunAISalvo = false);
	int m_fHackPitch = 75; // Retail +0x2c0; randomized in Spawn
	float m_fNextRangeAttack = 0; // Retail +0x2c4
	int m_iAttackSide = 0;		  // Retail +0x2cc: 1 = right arm sequence, 2 = left arm sequence
	// Shared with CGunnerFriendly, whose retail eye uses a different sprite
	// and brightness gate but the same fade/attachment update.
protected:
	int m_eyeBrightness = 0; // Retail +0x2bc; target brightness, 0 or 200
	int m_iEyeLastAmt = -1; // nur fuer die Debug-Ausgabe (Sichtbeleg)
	CSprite* m_pEyeGlow = nullptr; // Retail +0x2c8
	float m_flEyeTalkUntil = 0; // deliberate Rustgunner-only blue-eye deviation
	void UpdateEyeGlow();
private:
	float m_iNeverRun = 0; // Retail save field +0x2d0 (meaning unresolved)
	float m_fMeleeDistance = 0; // Retail +0x2d4: 0 until first attack, then 500 decaying by 0.995 each RunAI tick
	float m_fTimeSinceLastVolly = 0; // Retail +0x2d8
	int m_iVollyCount = 4; // Retail +0x2dc
};
// Testhilfe fuer `dbg_eye`: schaltet bei allen Gunnern der Map das Auge ein
// und gibt die Sprite-Position aus (Pruefung der Verankerung am Hals-Bone).
void DbgForceGunnerEyes()
{
	const char* rgClasses[] = { "monster_rustgunr", "monster_gunner", "monster_rustgnr", "monster_gunner_friendly" };

	for (int i = 0; i < 4; i++)
	{
		CBaseEntity* pEnt = NULL;
		while ((pEnt = UTIL_FindEntityByClassname(pEnt, rgClasses[i])) != NULL)
		{
			CGunner* pGunner = static_cast<CGunner*>(pEnt);
			pGunner->DebugForceEye();
			ALERT(at_aiconsole, "[eye] %s origin=(%.0f %.0f %.0f)\n",
				rgClasses[i], pEnt->pev->origin.x, pEnt->pev->origin.y, pEnt->pev->origin.z);
		}
	}
}

LINK_ENTITY_TO_CLASS(monster_rustgunr, CGunner);
LINK_ENTITY_TO_CLASS(monster_gunner, CGunner);
LINK_ENTITY_TO_CLASS(monster_rustgnr, CGunner);

TYPEDESCRIPTION CGunner::m_SaveData[] =
{
	DEFINE_FIELD(CGunner, m_pEyeGlow, FIELD_CLASSPTR),
	DEFINE_FIELD(CGunner, m_eyeBrightness, FIELD_INTEGER),
	DEFINE_FIELD(CGunner, m_flEyeTalkUntil, FIELD_TIME),
	DEFINE_FIELD(CGunner, m_fHackPitch, FIELD_INTEGER),
	DEFINE_FIELD(CGunner, m_fNextRangeAttack, FIELD_TIME),
	DEFINE_FIELD(CGunner, m_iNeverRun, FIELD_TIME),
	DEFINE_FIELD(CGunner, m_fMeleeDistance, FIELD_TIME),
	DEFINE_FIELD(CGunner, m_fTimeSinceLastVolly, FIELD_TIME),
	DEFINE_FIELD(CGunner, m_iVollyCount, FIELD_INTEGER),
};
IMPLEMENT_SAVERESTORE(CGunner, CBaseMonster);

//=========================================================
// monster_gunner_friendly - CGunnerFriendly : CGunner. Retail Spawn
// @0x100bf200 ruft CGunner::Spawn (@0x100bec80) und ueberschreibt danach
// Modell (aigunner), Gesundheit je Skill (75/50/35), Sichtfeld -1 und
// takedamage 1. Eigene Slots: Save/Restore, Classify (3), TakeDamage
// @0x100bf6b0, Killed @0x100bf7a0, IRelationship @0x100bf3d0.
// Friendly cameye/Save-Daten und Sound-Slots sind abgeglichen; offen bleibt
// die genaue Helligkeitsbedingung in Slot 117.
//=========================================================
class CGunnerFriendly : public CGunner
{
public:
	void Spawn() override;
	void Precache() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void AlertSound() override {}
	void IdleSound() override {}
	void PainSound() override;
	void PrescheduleThink() override;
	int Classify() override { return CLASS_HUMAN_PASSIVE; }
	int IRelationship(CBaseEntity* pTarget) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_flPainTime = 0; // Retail +0x2e0
	float m_flDistractedTime = 0; // Retail +0x2e4
	int m_iEndbossStatusUpdate = 7; // Retail +0x2e8
};
LINK_ENTITY_TO_CLASS(monster_gunner_friendly, CGunnerFriendly);

TYPEDESCRIPTION CGunnerFriendly::m_SaveData[] =
{
	DEFINE_FIELD(CGunnerFriendly, m_flPainTime, FIELD_TIME),
	DEFINE_FIELD(CGunnerFriendly, m_flDistractedTime, FIELD_TIME),
	DEFINE_FIELD(CGunnerFriendly, m_iEndbossStatusUpdate, FIELD_INTEGER),
};

bool CGunnerFriendly::Save(CSave& save)
{
	if (!CGunner::Save(save))
		return false;
	return save.WriteFields("CFriendlyGunner", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

bool CGunnerFriendly::Restore(CRestore& restore)
{
	if (!CGunner::Restore(restore))
		return false;
	return restore.ReadFields("CFriendlyGunner", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

void CGunnerFriendly::Spawn()
{
	CGunner::Spawn();
	if (m_pEyeGlow)
	{
		UTIL_Remove(m_pEyeGlow);
		m_pEyeGlow = nullptr;
	}
	m_eyeBrightness = 0;
	// Wie CGunner: animiertes Auge (sonst Frame 1, Meldung 2026-11-07).
	m_pEyeGlow = CSprite::SpriteCreate("sprites/cameye.spr", pev->origin, true);
	if (m_pEyeGlow)
	{
		// kRenderTransAdd statt kRenderGlow (kein Durchscheinen durchs Modell,
		// Meldung 2026-11-07).
		// Freundlicher Gunner = "normaler" Gunner: rotes Auge
		// (Spielerangabe 2026-11-07: monster_gunner rot, rust blau).
		m_pEyeGlow->SetTransparency(kRenderTransAdd, 255, 0, 0, 200, kRenderFxNoDissipation);
		// Kein SetAttachment: der gestagte aigunner.mdl hat keinen Kopf-Attachment
		// (nur Spine/Unterarme), dadurch lag das Sprite im Koerper und schien
		// durch das Modell. Position wird in UpdateEyeGlow am Kopf gesetzt.
		m_pEyeGlow->pev->framerate = 9.0f;
		m_pEyeGlow->SetScale(0.25f);
		m_pEyeGlow->TurnOn();
	}

	SET_MODEL(ENT(pev), "models/aigunner.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 150));
	switch (g_iSkillLevel)
	{
	case SKILL_MEDIUM:
		pev->health = 50;
		break;
	case SKILL_HARD:
		pev->health = 35;
		break;
	default:
		pev->health = 75;
		break;
	}
	pev->max_health = pev->health;
	m_flFieldOfView = -1.0f;
	pev->takedamage = DAMAGE_YES;
	m_flPainTime = gpGlobals->time;
	m_flDistractedTime = gpGlobals->time;
	m_iEndbossStatusUpdate = 7;
}

void CGunnerFriendly::Precache()
{
	CGunner::Precache();
	PrecacheModel("models/aigunner.mdl");
	PrecacheModel("sprites/cameye.spr");
	PrecacheSound("mainframe/end_7.wav");
	PrecacheSound("mainframe/end_8.wav");
	PrecacheSound("mainframe/end_9.wav");
	PrecacheSound("mainframe/mainend1.wav");
	PrecacheSound("mainframe/mainend2.wav");
	PrecacheSound("mainframe/mainend3.wav");
	PrecacheSound("mainframe/mainend4.wav");
	PrecacheSound("mainframe/mainend5.wav");
}

void CGunnerFriendly::PainSound()
{
	if (gpGlobals->time < m_flPainTime)
		return;

	static const char* const pLines[] = {
		"mainframe/mainend1.wav",
		"mainframe/mainend2.wav",
		"mainframe/mainend3.wav",
		"mainframe/mainend4.wav",
		"mainframe/mainend5.wav",
	};
	UTIL_EmitAmbientSound(edict(), pev->origin, pLines[RANDOM_LONG(0, 4)], 1.0f, 0.8f, 0, 100);
	m_flPainTime = gpGlobals->time + 3.0f;
}

void CGunnerFriendly::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	// Retail Friendly slot 139 is a RET stub. Its shared event handler calls
	// that virtual slot for event 1008, so Friendly emits no step sound/shake.
	if (pEvent->event == SCRIPT_EVENT_SOUND_VOICE)
		return;

	CGunner::HandleAnimEvent(pEvent);
}

void CGunnerFriendly::PrescheduleThink()
{
	// Retail helper 0x100bf560 advances the story voice at three endboss
	// health thresholds, gated by the boss's CBaseMonster::IsAlive slot.
	CBaseEntity* pBoss = UTIL_FindEntityByClassname(nullptr, "monster_endboss");
	if (pBoss && pBoss->IsAlive())
	{
		const float flHealth = pBoss->pev->health;
		const float flMaxHealth = pBoss->pev->max_health;
		const char* pszLine = nullptr;
		if (m_iEndbossStatusUpdate == 7 && flHealth < flMaxHealth * 0.7f)
		{
			pszLine = "mainframe/end_9.wav";
			m_iEndbossStatusUpdate = 5;
		}
		else if (m_iEndbossStatusUpdate == 5 && flHealth < flMaxHealth * 0.5f)
		{
			pszLine = "mainframe/end_7.wav";
			m_iEndbossStatusUpdate = 2;
		}
		else if (m_iEndbossStatusUpdate == 2 && flHealth < flMaxHealth * 0.2f)
		{
			pszLine = "mainframe/end_8.wav";
			m_iEndbossStatusUpdate = 10;
		}
		if (pszLine)
			UTIL_EmitAmbientSound(edict(), pev->origin, pszLine, 1.0f, 0.8f, 0, 100);
	}

	// Retail slot 117 @0x100bf430 checks player 1 against the gunner's
	// forward hemisphere. There is no distance gate: the normalized
	// player direction must have a positive dot product with v_forward.
	m_eyeBrightness = 0;
	CBaseEntity* pPlayer = UTIL_PlayerByIndex(1);
	if (pPlayer && pPlayer->pev)
	{
		Vector vecToPlayer = pPlayer->pev->origin - pev->origin;
		const float flDistance = vecToPlayer.Length();
		if (flDistance > 0.0f)
		{
			vecToPlayer = vecToPlayer * (1.0f / flDistance);
			UTIL_MakeVectors(pev->angles);
			if (DotProduct(gpGlobals->v_forward, vecToPlayer) > 0.0f)
				m_eyeBrightness = 200;
		}
	}
	UpdateEyeGlow();
}

int CGunnerFriendly::IRelationship(CBaseEntity* pTarget)
{
	const int iBase = CGunner::IRelationship(pTarget);
	if (!pTarget || gpGlobals->time >= m_flDistractedTime)
		return iBase;
	const int iClass = pTarget->Classify();
	if (iClass == CLASS_PLAYER_ALLY || iClass == CLASS_PLAYER_BIOWEAPON || iClass == CLASS_ALIEN_BIOWEAPON)
		return R_NO;
	return iBase;
}

bool CGunnerFriendly::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// Retail: Angriffe von monster_endboss und Spielern werden ignoriert;
	// alles andere richtet nur 1 Schaden an (ohne Gibs).
	const char* szAttacker = pevAttacker ? STRING(pevAttacker->classname) : "";
	if (FStrEq(szAttacker, "monster_endboss") || FStrEq(szAttacker, "player"))
		return false;

	if (RANDOM_LONG(0, 99) < 25)
		m_flDistractedTime = RANDOM_LONG(7, 15) + gpGlobals->time;
	return CGunner::TakeDamage(pevInflictor, pevAttacker, 1.0f, bitsDamageType | DMG_NEVERGIB);
}

// Retail @0x100bf7e0: no dot/min-range test; monster_beak enemies closer than 256 are not shot at;
// timer 2 s.
bool CGunnerFriendly::CheckRangeAttack1(float flDot, float flDist)
{
	if (m_fNextRangeAttack > gpGlobals->time)
		return false;
	if (m_hEnemy && FClassnameIs(m_hEnemy->pev, "monster_beak") && flDist < 256.0f)
		return false;
	if (!m_hEnemy)
		return false;

	const int iClear = TestMuzzleSides();
	if (iClear == 0)
		return false;
	if (iClear == 3)
		m_iAttackSide = RANDOM_LONG(0, 1) == 0 ? 2 : 1;
	else
		m_iAttackSide = iClear;

	m_fNextRangeAttack = gpGlobals->time + 2.0f;
	return true;
}

void CGunnerFriendly::Killed(entvars_t* pevAttacker, int iGib)
{
	FireTargets(STRING(pev->target), this, this, USE_TOGGLE, 0);
	CGunner::Killed(pevAttacker, iGib);
}

void CGunner::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/gunner.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 150));

	pev->solid = SOLID_SLIDEBOX;
	// Nachtrag 2026-09-06 (In-Game-Test via monster_zoo_test, Meldung
	// "Rust Gunner ist unverwundbar"): pev->movetype fehlte hier
	// komplett (bleibt sonst auf dem Edict-Default MOVETYPE_NONE) -
	// jede andere Monster-Klasse dieses Projekts setzt explizit
	// MOVETYPE_STEP. Explizit nachgetragen (matcht jede
	// Geschwisterklasse), auch wenn ein gezielter Debug-Test (trigger_
	// hurt + temporaerer TakeDamage()-Log-Override, siehe STATUS.md)
	// zeigte, dass pev->takedamage bereits korrekt DAMAGE_AIM(2.0) war
	// und Schaden sauber ankam - die urspruengliche "unverwundbar"-
	// Meldung war vermutlich ein Missverstaendnis (z.B. verfehlte
	// Treffer) oder ein anderes, noch ungeklaertes Problem, nicht dieses
	// hier. pev->movetype bleibt trotzdem als echte, unabhaengig
	// gefundene Abweichung von der Geschwisterklassen-Konvention gesetzt.
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	pev->health = gSkillData.rustgunnerHealth;
	pev->view_ofs = Vector(0, 0, 115);
	m_fHackPitch = 75 + RANDOM_LONG(0, 15);
	m_fNextRangeAttack = gpGlobals->time;
	m_fTimeSinceLastVolly = gpGlobals->time;
	m_iVollyCount = 4;
	m_eyeBrightness = 0;
	m_flEyeTalkUntil = 0;
	const bool bRustgunner = FStrEq(STRING(pev->classname), "monster_rustgunr");
	// `true` = sofort animieren (CSprite::TurnOn startet AnimateThink). Ohne
	// das blieb das Auge in Frame 1 stehen (Spielermeldung 2026-11-07).
	// Rust-Gunner: identisches Auge, aber in Blau (cameye.spr + (0,0,255)).
	// Retail CGunner::Spawn (0x100bec80) erzeugt "sprites/gunr_eye.spr".
	// Farbe je Klasse (Spielerangabe 2026-11-07): monster_gunner rot,
	// monster_rustgunr blau.
	m_pEyeGlow = CSprite::SpriteCreate("sprites/gunr_eye.spr", pev->origin, true);
	if (m_pEyeGlow)
	{
		// kRenderTransAdd statt kRenderGlow: der Glow-Modus rendert ohne
		// Tiefentest und schien durch das Gunner-Modell (Meldung 2026-11-07).
		const int iRed = bRustgunner ? 0 : 255;
		const int iBlue = bRustgunner ? 255 : 0;
		m_pEyeGlow->SetTransparency(kRenderTransAdd, iRed, 0, iBlue,
			200, kRenderFxNoDissipation);
		// Kein SetAttachment (siehe CGunnerFriendly): Position setzt UpdateEyeGlow.
		m_pEyeGlow->pev->framerate = 10.0f;
		// Skalierung: gunr_eye.spr ist 32x32 und deckt bei Scale 1 fast das
		// ganze Gesicht ab (Spielermeldung 2026-11-07: "viel zu gross").
		m_pEyeGlow->SetScale(0.25f);
		m_pEyeGlow->TurnOn();
	}

	MonsterInit();

	// Retail CGunner::Spawn @ 0x100bec80 restores this authored eye height
	// after the model-derived initialization step.
	pev->view_ofs = Vector(0, 0, 115);
	m_flDistTooFar = 4096.0f; // Retail +0x244 (frisch dekompiliert 2026-10-31)
}

void CGunner::Precache()
{
	PrecacheModel("models/gunner.mdl");
	PrecacheModel("models/gunnergibs.mdl");
	PrecacheModel("sprites/gunr_eye.spr");
	PrecacheModel("sprites/cameye.spr");
	PrecacheModel("sprites/botdeath.spr");
	PrecacheModel("sprites/gorebot.spr");
	PrecacheModel("sprites/gibbot.spr");

	PrecacheSound("gunner/gunner_boom1.wav");
	PrecacheSound("gunner/gunner_boom2.wav");
	PrecacheSound("gunner/gunner_idle.wav");
	PrecacheSound("gunner/gunner_idle2.wav");
	PrecacheSound("gunner/gunner_alert1.wav");
	PrecacheSound("gunner/gunner_alert2.wav");
	PrecacheSound("gunner/gunner_alert3.wav");
	PrecacheSound("gunner/gunner_flinch.wav");
	PrecacheSound("gunner/gunner_swing.wav");
	// Retail slot 58 shares a hit callback with a RenaSaur-like monster and
	// emits this voice-channel cue from a struck entity at a pitch per event.
	PrecacheSound("renesaur/ren_bite3.wav");
	PrecacheSound("gunner/gunner_run_step.wav");
	PrecacheSound("gunner/gunner_run_step1.wav");
	PrecacheSound("gunner/gunner_run_step2.wav");
	UTIL_PrecacheOther("anime_rocket");
}

void CGunner::StartTask(Task_t* pTask)
{
	if (pTask && (pTask->iTask == TASK_MELEE_ATTACK1 || pTask->iTask == TASK_MELEE_ATTACK2))
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "gunner/gunner_swing.wav", 1.0f, ATTN_NORM, 0, 100);

	CBaseMonster::StartTask(pTask);
}

void CGunner::AlertSound()
{
	static const char* const pSounds[] = {
		"gunner/gunner_alert1.wav",
		"gunner/gunner_alert2.wav",
		"gunner/gunner_alert3.wav",
	};
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pSounds[RANDOM_LONG(0, 2)], 1.0f, 0.8f, 0, m_fHackPitch);
}

void CGunner::IdleSound()
{
	// Retail vtable slot 135 @0x100bc530: random choice between the two
	// ambient idle lines, volume 1, attenuation 0.8, flags 8, pitch 100.
	const char* pszSound = RANDOM_LONG(0, 1) == 0 ? "gunner/gunner_idle.wav" : "gunner/gunner_idle2.wav";
	UTIL_EmitAmbientSound(edict(), pev->origin, pszSound, 1.0f, 0.8f, 8, 100);
}

void CGunner::PainSound()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "gunner/gunner_flinch.wav", 1.0f, 0.8f, 0, m_fHackPitch);
}

void CGunner::PlayScriptedSentence(const char* pszSentence, float duration, float volume, float attenuation,
	bool bConcurrent, CBaseEntity* pListener)
{
	CBaseMonster::PlayScriptedSentence(pszSentence, duration, volume, attenuation, bConcurrent, pListener);
	if (FStrEq(STRING(pev->classname), "monster_rustgunr"))
		m_flEyeTalkUntil = gpGlobals->time + duration;
}

bool CGunner::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (pevInflictor && pevInflictor->owner == edict())
		return false;
	if (flDamage > 30.0f)
		flDamage = 30.0f;
	if ((bitsDamageType & (DMG_SHOCK | DMG_ENERGYBEAM)) != 0)
		flDamage *= 0.5f;
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType | DMG_ALWAYSGIB);
}

void CGunner::SetYawSpeed()
{
	switch (m_Activity)
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		pev->yaw_speed = 180;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
		pev->yaw_speed = 50;
		break;
	default:
		pev->yaw_speed = 120;
		break;
	}
}

void CGunner::SetActivity(Activity NewActivity)
{
	if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && m_pCine != nullptr)
	{
		m_Activity = NewActivity;
		m_IdealActivity = NewActivity;
		return;
	}

	if (NewActivity != ACT_RANGE_ATTACK1)
	{
		CBaseMonster::SetActivity(NewActivity);
		return;
	}

	const int iSequence = LookupSequence(m_iAttackSide == 2 ? "firerocketsleft" : "firerocketright");
	m_Activity = NewActivity;
	if (iSequence > ACTIVITY_NOT_AVAILABLE)
	{
		if (pev->sequence != iSequence || !m_fSequenceLoops)
			pev->frame = 0;
		pev->sequence = iSequence;
		ResetSequenceInfo();
		SetYawSpeed();
	}
	else
	{
		pev->sequence = 0;
	}
}

// Retail slot 80 @0x100be8e0: farther than m_fMeleeDistance (0 first, then 500), facing (dot > 0.7), timer
// elapsed, and a clear line from at least one forearm attachment (1 = right, 2 = left) to the enemy.
bool CGunner::CheckRangeAttack1(float flDot, float flDist)
{
	if (flDist <= m_fMeleeDistance || flDot <= 0.7f || m_fNextRangeAttack > gpGlobals->time)
		return false;
	if (!m_hEnemy)
		return false;

	const int iClear = TestMuzzleSides();
	if (iClear == 0)
		return false;
	if (iClear == 3)
		m_iAttackSide = RANDOM_LONG(0, 1) == 0 ? 2 : 1;
	else
		m_iAttackSide = iClear;

	m_fMeleeDistance = 500.0f;
	m_fNextRangeAttack = RANDOM_FLOAT(3.0, 5.0) + gpGlobals->time;
	return true;
}

int CGunner::TestMuzzleSides()
{
	int iClear = 0;
	for (int i = 0; i < 2; i++)
	{
		Vector vecSrc, vecAngles;
		GetAttachment(i + 2, vecSrc, vecAngles);
		if (UTIL_PointContents(vecSrc) == CONTENTS_SOLID)
			continue;
		TraceResult tr;
		const Vector vecTarget = m_hEnemy->BodyTarget(vecSrc);
		UTIL_TraceLine(vecSrc, vecTarget, ignore_monsters, ignore_glass, edict(), &tr);
		if (tr.flFraction == 1.0f || tr.pHit == m_hEnemy->edict())
			iClear |= 1 << i;
	}
	return iClear;
}

Schedule_t* CGunner::GetSchedule()
{
	if (m_MonsterState == MONSTERSTATE_COMBAT && HasConditions(bits_COND_CAN_MELEE_ATTACK1) &&
		HasConditions(bits_COND_CAN_MELEE_ATTACK2))
	{
		return GetScheduleOfType(RANDOM_LONG(0, 99) < 90 ? SCHED_MELEE_ATTACK1 : SCHED_MELEE_ATTACK2);
	}
	return CBaseMonster::GetSchedule();
}

void CGunner::RunAI()
{
	pev->framerate = 1.5f;
	// Retail slot 62 multiplies +0x2d4 (m_fMeleeDistance) by 0.995 before base RunAI.
	m_fMeleeDistance *= 0.995;
	CBaseMonster::RunAI();

	// Retail slot 62 tail: while idle, walking or running with an enemy, a rocket is launched from the
	// left forearm attachment (3). Retail +0x2dc counts down from 4 after the 4-s pause,
	// producing four launches in consecutive AI ticks before the next pause.
	if ((m_Activity == ACT_RUN || m_Activity == ACT_WALK || m_Activity == ACT_IDLE) && m_hEnemy != nullptr &&
		(m_iVollyCount != 4 || gpGlobals->time - m_fTimeSinceLastVolly > 4.0f))
	{
		m_fTimeSinceLastVolly = gpGlobals->time;
		--m_iVollyCount;
		if (m_iVollyCount <= 0)
			m_iVollyCount = 4;
		FireRocket(3, true);
	}
}

void CGunner::PrescheduleThink()
{
	// Retail slot 117 @0x100beeb0: bits_COND_HEAR_SOUND (0x10) targets
	// eye brightness 200; otherwise it targets 0. Brightness approaches
	// the target by 26 per AI update, the dark sprite is hidden, and its
	// attachment origin is refreshed.
	// Retail-Slot 117: bits_COND_HEAR_SOUND -> 200, sonst 0. Zusaetzlich
	// (Abweichung auf Spielerwunsch, 2026-11-07): solange der Gunner ein Ziel
	// hat, bleibt das Auge an - sonst waere es im Spiel praktisch nie zu sehen
	// ("keiner der gunner hat eins"). Die Sprachzeile bleibt weiterer Ausloeser.
	m_eyeBrightness = (HasConditions(bits_COND_HEAR_SOUND) || m_hEnemy != nullptr ||
		gpGlobals->time < m_flEyeTalkUntil) ? 200 : 0;
	UpdateEyeGlow();

	// The same retail slot occasionally dispatches virtual slot 135 while
	// outside combat; Friendly has its own slot 117 and does not use this path.
	if (m_MonsterState != MONSTERSTATE_COMBAT && RANDOM_LONG(0, 60) == 0)
		IdleSound();
}

// Hals-Bone beider Gunner-Modelle (Bones 0..15, kein Kopf-Bone).
#define GUNNER_NECK_BONE 9

void CGunner::UpdateEyeGlow()
{
	if (!m_pEyeGlow)
		return;

	m_pEyeGlow->pev->renderamt = UTIL_Approach(m_eyeBrightness, m_pEyeGlow->pev->renderamt, 26);
	if ((int)m_pEyeGlow->pev->renderamt != m_iEyeLastAmt)
	{
		m_iEyeLastAmt = (int)m_pEyeGlow->pev->renderamt;
		ALERT(at_aiconsole, "[eye] %s amt=%d target=%d hear=%d\n",
			STRING(pev->classname), m_iEyeLastAmt, m_eyeBrightness,
			HasConditions(bits_COND_HEAR_SOUND) ? 1 : 0);
	}
	if (m_pEyeGlow->pev->renderamt == 0)
		m_pEyeGlow->pev->effects |= EF_NODRAW;
	else
		m_pEyeGlow->pev->effects &= ~EF_NODRAW;
	// Auge am Kopf. Die Modelle (gunner.mdl/aigunner.mdl) haben KEINEN
	// Kopf-Attachment-Punkt (nur Spine 0/1 und die beiden Unterarme 2/3) und
	// auch keinen Head-Bone: der "Kopf" ist Teil des Torsos. Deshalb wird das
	// Auge ueber den Hals-Bone (Index 9, "Bip01 Neck") verankert und von dort
	// leicht nach vorn geschoben; origin + view_ofs traf nur die Brust und
	// liess das Sprite neben dem Koerper schweben (Sichtpruefung 2026-11-07).
	Vector vecForward;
	AngleVectors(pev->angles, &vecForward, nullptr, nullptr);

	float vecBoneOrigin[3];
	float vecBoneAngles[3];
	g_engfuncs.pfnGetBonePosition(ENT(pev), GUNNER_NECK_BONE, vecBoneOrigin, vecBoneAngles);

	Vector vecEye = Vector(vecBoneOrigin) + vecForward * 6.0f;
	UTIL_SetOrigin(m_pEyeGlow->pev, vecEye);
}

void CGunner::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Confirmed via fresh decompile: a literal pev->takedamage on/off
	// toggle, not a graduated shield/resistance value (see file
	// header).
	pev->takedamage = (pev->takedamage != DAMAGE_NO) ? DAMAGE_NO : DAMAGE_AIM;
}

static void ApplyGunnerMeleeImpact(CBaseEntity* pVictim, const Vector& vecAttackerOrigin, const Vector& vecAttackerAngles);

void CGunner::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case SCRIPT_EVENT_SOUND_VOICE:
	{
		static const char* const pStepSounds[] = {
			"gunner/gunner_run_step.wav",
			"gunner/gunner_run_step1.wav",
			"gunner/gunner_run_step2.wav",
		};
		UTIL_EmitAmbientSound(edict(), pev->origin, pStepSounds[RANDOM_LONG(0, 2)], 1.0f, 0.8f, 0, 100);
		UTIL_ScreenShake(pev->origin, 25.0f, 150.0f, 0.75f, 2000.0f, false);
		break;
	}
	case 1:
	{
		// Retail gunner.mdl's swing sequence emits event 1 on its impact frame.
		// Slot 58 calls the shared hull-attack helper with (150, 100, DMG_SLASH).
		TraceResult traceResult;
		Vector vecAttackDir;
		CBaseEntity* pHurt = CheckTraceHullAttack(150.0f, 100, DMG_SLASH, &traceResult, &vecAttackDir);
		if (pHurt)
			pHurt->TraceBleed(100.0f, vecAttackDir, &traceResult, DMG_SLASH);
		// Retail checks the victim's IsPlayer virtual before playing this cue.
		if (pHurt && pHurt->pev && pHurt->IsPlayer())
			EMIT_SOUND_DYN(pHurt->edict(), CHAN_VOICE, "renesaur/ren_bite3.wav", 1.0f, ATTN_IDLE, 0, 130);
		ApplyGunnerMeleeImpact(pHurt, pev->origin, pev->angles);
		break;
	}
	case 12:
		// Retail event 12 calls the shared hull-attack helper with range 200,
		// damage 30, and DMG_SLASH. Its extra flags and hit TempEntity chain
		// remain under investigation.
	{
		TraceResult traceResult;
		Vector vecAttackDir;
		CBaseEntity* pHurt = CheckTraceHullAttack(200.0f, 30, DMG_SLASH, &traceResult, &vecAttackDir);
		if (pHurt)
			pHurt->TraceBleed(30.0f, vecAttackDir, &traceResult, DMG_SLASH);
		// Retail checks the victim's IsPlayer virtual before playing this cue.
		if (pHurt && pHurt->pev && pHurt->IsPlayer())
			EMIT_SOUND_DYN(pHurt->edict(), CHAN_VOICE, "renesaur/ren_bite3.wav", 1.0f, ATTN_IDLE, 0, 140);
		ApplyGunnerMeleeImpact(pHurt, pev->origin, pev->angles);
		break;
	}
	case GUNNER_AE_FIRE_RIGHT_1:
	case GUNNER_AE_FIRE_RIGHT_2:
		// Nachtrag 2026-09-06 (In-Game-Test-Meldung "Raketen kommen aus
		// dem Torso statt den Haenden"): models/gunner.mdl hat 4
		// Attachment-Punkte - [0]/[1] sitzen auf der Wirbelsaeule
		// (vermutlich Schulter-Referenzpunkte), [2]/[3] auf "Bip01 R/L
		// Forearm" (Unterarm), exakt passend zu den 4 Feuer-Events
		// (FIRE_RIGHT_1/2, FIRE_LEFT_1/2). FireRocket() nahm bislang
		// IMMER eine feste Position relativ zu pev->origin (Torso-Hoehe),
		// unabhaengig vom Event - jetzt der jeweils passende
		// Unterarm-Attachment-Punkt.
		FireRocket(2);
		break;
	case GUNNER_AE_FIRE_LEFT_1:
	case GUNNER_AE_FIRE_LEFT_2:
		FireRocket(3);
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

// Gunner melee has a strong server-side hit response in both attack variants.
// Retail's event-1 velocity writes are weak and fixed to world axes; use a
// directional impulse here so the hit reads clearly for players and NPCs.
static void ApplyGunnerMeleeImpact(CBaseEntity* pVictim, const Vector& vecAttackerOrigin, const Vector& vecAttackerAngles)
{
	if (!pVictim || !pVictim->pev || (pVictim->pev->flags & (FL_CLIENT | FL_MONSTER)) == 0)
		return;

	Vector vecKnockback = pVictim->pev->origin - vecAttackerOrigin;
	vecKnockback.z = 0.0f;
	const float flDistance = vecKnockback.Length();
	if (flDistance > 0.001f)
		vecKnockback = vecKnockback * (1.0f / flDistance);
	else
	{
		UTIL_MakeVectors(vecAttackerAngles);
		vecKnockback = gpGlobals->v_forward;
		vecKnockback.z = 0.0f;
		vecKnockback = vecKnockback.Normalize();
	}

	const bool bPlayer = pVictim->IsPlayer();
	const float flHorizontalImpulse = bPlayer ? 400.0f : 300.0f;
	const float flVerticalImpulse = bPlayer ? 180.0f : 140.0f;
	pVictim->pev->velocity = pVictim->pev->velocity + vecKnockback * flHorizontalImpulse + Vector(0, 0, flVerticalImpulse);
	pVictim->pev->flags &= ~FL_ONGROUND;
	pVictim->pev->groundentity = nullptr;

	if (bPlayer)
	{
		// View punch plus a short nearby shake makes a successful arm hit legible.
		pVictim->pev->punchangle.x += 12.0f;
		pVictim->pev->punchangle.y += RANDOM_LONG(0, 1) == 0 ? -8.0f : 8.0f;
		UTIL_ScreenShake(pVictim->pev->origin, 10.0f, 150.0f, 0.25f, 512.0f, false);
		return;
	}

	CBaseMonster* pMonster = pVictim->MyMonsterPointer();
	if (!pMonster || !pMonster->IsAlive())
		return;

	pMonster->SetConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE);
	if (pMonster->m_MonsterState == MONSTERSTATE_SCRIPT)
		return;

	if (pMonster->LookupActivity(ACT_BIG_FLINCH) != ACTIVITY_NOT_AVAILABLE)
		pMonster->SetActivity(ACT_BIG_FLINCH);
	else if (pMonster->LookupActivity(ACT_SMALL_FLINCH) != ACTIVITY_NOT_AVAILABLE)
		pMonster->SetActivity(ACT_SMALL_FLINCH);
}

void CGunner::FireRocket(int iAttachment, bool bRunAISalvo)
{
	if (!m_hEnemy)
		return;

	Vector vecSrc, vecAngles;
	GetAttachment(iAttachment, vecSrc, vecAngles);

	// Retail randomizes the launch point in the gunner's local right/up axes:
	// four choices at +/-10 units and one unchanged point.
	UTIL_MakeVectors(pev->angles);
	switch (RANDOM_LONG(0, 4) % 5)
	{
	case 0:
		vecSrc = vecSrc + gpGlobals->v_right * 10.0f;
		break;
	case 1:
		vecSrc = vecSrc - gpGlobals->v_right * 10.0f;
		break;
	case 2:
		vecSrc = vecSrc + gpGlobals->v_up * 10.0f;
		break;
	case 3:
		vecSrc = vecSrc - gpGlobals->v_up * 10.0f;
		break;
	}

	// RunAI's retail slot-140 path emits this launch cue before the muzzle
	// effect. The animation-event helper omits this sound.
	if (bRunAISalvo)
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/kaboom1.wav", 0.7f, 0.8f, 0, 100);

	// Both retail paths send this muzzle smoke payload at the attachment
	// point (TE_SMOKE, scale 10, rate 16).
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_SHORT(g_sModelIndexSmoke);
	WRITE_BYTE(10);
	WRITE_BYTE(16);
	MESSAGE_END();

	CAnimeRocket::CreateForGunner(this, m_hEnemy, vecSrc);
}

// Erschuetterung mit linearer Entfernungsabnahme. UTIL_ScreenShake liefert nur innerhalb
// eines harten Radius einen konstanten Ausschlag; die Meteoreinschlaege liegen 4000-8000
// Einheiten entfernt, deshalb mit weitem Radius und Abfall.
// Bekannte Grenze: waehrend einer Kamera-Animation (SET_VIEW auf trigger_camera) erreicht die
// Erschuetterung die Kamera nicht, weil die Anwendung clientseitig in HUD_CalcRefdef passiert.
static void MeteorScreenShake(const Vector& vecCenter, float flAmplitude, float flRadius)
{
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer)
			continue;

		const float flDist = (pPlayer->pev->origin - vecCenter).Length();
		if (flDist >= flRadius)
			continue;

		const float flLocal = flAmplitude * (1.0f - flDist / flRadius);
		if (flLocal < 0.3f)
			continue;

		// Der Engine-eigene ScreenShake wirkt nur im freien Blick, die eigene MeteorShake-
		// Nachricht wird vom Client nur in Kamera-Ansichten angewendet - beide senden.
		MESSAGE_BEGIN(MSG_ONE, gmsgShake, NULL, pPlayer->edict());
		WRITE_SHORT(static_cast<unsigned short>(flLocal * 4096.0f));
		WRITE_SHORT(static_cast<unsigned short>(1.5f * 4096.0f));
		WRITE_SHORT(static_cast<unsigned short>(4.0f * 256.0f));
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgCameraShake, NULL, pPlayer->edict());
		WRITE_SHORT(static_cast<unsigned short>(flLocal * 4096.0f));
		WRITE_SHORT(static_cast<unsigned short>(1.5f * 4096.0f));
		WRITE_SHORT(static_cast<unsigned short>(4.0f * 256.0f));
		MESSAGE_END();
	}
}

// Partikel-Budget: die Engine (GoldSrc/Xash3D) hat einen festen Partikelpool; Xash3D meldet
// "Particle overflow" bei 4096 gleichzeitigen Partikeln (Laufzeitbeleg:
// out/run/xash4d/... siehe findings/meteor_effect_suppression_2026-11-05.md).
// Wir beanspruchen hoechstens 55 % davon und verteilen das als Sekundenkontingent, damit
// gleichzeitige Einschlaege den Pool nicht mehr ueberlaufen.
#define METEOR_ENGINE_PARTICLE_LIMIT 4096
#define METEOR_PARTICLE_SHARE 0.45f
// Fensterlaenge: Rauchsprites leben rund 1-2 s, deshalb wird das Kontingent ueber ein
// laengeres Fenster vergeben (nicht pro Sekunde).
#define METEOR_PARTICLE_WINDOW 2.5f

static float s_flParticleQuotaReset = 0.0f;
static int s_iParticleQuotaUsed = 0;

static int MeteorParticleQuotaTotal()
{
	return static_cast<int>(METEOR_ENGINE_PARTICLE_LIMIT * METEOR_PARTICLE_SHARE);
}

// Fordert iWanted Partikel an; gibt die tatsaechlich freigegebene Anzahl zurueck (0 = nichts).
static int MeteorParticleBudget(int iWanted)
{
	if (iWanted <= 0)
		return 0;

	if (gpGlobals->time >= s_flParticleQuotaReset)
	{
		s_flParticleQuotaReset = gpGlobals->time + METEOR_PARTICLE_WINDOW;
		s_iParticleQuotaUsed = 0;
	}

	const int iFree = MeteorParticleQuotaTotal() - s_iParticleQuotaUsed;
	if (iFree <= 0)
		return 0;

	const int iGranted = iWanted < iFree ? iWanted : iFree;
	s_iParticleQuotaUsed += iGranted;
	return iGranted;
}

//=========================================================
// Kurzlebiger Rauch-Emitter fuer den Meteoreinschlag. Temp-Sprites (TE_SPRITE) nutzen die
// Alpha-Kanaele der SPR-Textur korrekt, CSprite-Entities rendern in diesem Build deckende
// Quads. Damit die Fahne stehen bleibt, wird ueber ~2,5 s nachgespuckt.
//=========================================================
class CMeteorSmoke : public CPointEntity
{
public:
	void Spawn() override { Precache(); SetThink(&CMeteorSmoke::SmokeThink); pev->nextthink = gpGlobals->time; }
	void Precache() override
	{
		m_iSmokeSprite = PrecacheModel("sprites/smoke.spr");
		m_iPuffSprite = PrecacheModel("sprites/smokepuff.spr");
		m_iFireSprite = PrecacheModel("sprites/fire.spr");
	}
	void EXPORT SmokeThink();

	Vector m_vecCenter = g_vecZero;
	float m_flEndTime = 0.0f;
	float m_flStartTime = 0.0f;
	int m_iSmokeSprite = 0;
	int m_iPuffSprite = 0;
	int m_iFireSprite = 0;
};

LINK_ENTITY_TO_CLASS(meteor_smoke, CMeteorSmoke);

void CMeteorSmoke::SmokeThink()
{
	const float flAge = gpGlobals->time - m_flStartTime;

	// Rauch in drei Groessenklassen, damit die Fahne unregelmaessig und dicht wirkt.
	const int iCount = flAge < 0.5f ? 2 : 1;
	for (int i = 0; i < iCount; ++i)
	{
		const Vector vecPos = m_vecCenter + Vector(RANDOM_FLOAT(-56.0f, 56.0f),
												RANDOM_FLOAT(-56.0f, 56.0f),
												flAge * 45.0f + RANDOM_FLOAT(0.0f, 36.0f));
		if (MeteorParticleBudget(1) == 0)
			break;

		const int iSize = RANDOM_LONG(0, 2);
		const int iScale = iSize == 0 ? RANDOM_LONG(50, 110) : (iSize == 1 ? RANDOM_LONG(110, 190) : RANDOM_LONG(190, 255));
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecPos);
		WRITE_BYTE((i % 3) ? TE_SMOKE : TE_SPRITE);
		WRITE_COORD(vecPos.x);
		WRITE_COORD(vecPos.y);
		WRITE_COORD(vecPos.z);
		WRITE_SHORT((i % 3) ? m_iSmokeSprite : m_iPuffSprite);
		WRITE_BYTE(iScale);
		WRITE_BYTE(RANDOM_LONG(7, 13));
		MESSAGE_END();
	}

	// Flackerndes Feuerlicht an der Brandstelle (kleine, unruhige dynamische Lichtquelle).
	static int s_iLightTick = 0;
	++s_iLightTick;
	if ((s_iLightTick % 3) == 0)
	{
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, m_vecCenter);
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(m_vecCenter.x + RANDOM_FLOAT(-16.0f, 16.0f));
		WRITE_COORD(m_vecCenter.y + RANDOM_FLOAT(-16.0f, 16.0f));
		WRITE_COORD(m_vecCenter.z + RANDOM_FLOAT(4.0f, 28.0f));
		WRITE_BYTE(RANDOM_LONG(12, 28)); // Radius in Zehntel-Einheiten
		WRITE_BYTE(255);
		WRITE_BYTE(RANDOM_LONG(130, 200));
		WRITE_BYTE(RANDOM_LONG(50, 110));
		WRITE_BYTE(RANDOM_LONG(3, 6)); // Lebensdauer
		WRITE_BYTE(RANDOM_LONG(6, 10));
		MESSAGE_END();
	}

	// Nachwuchsflammen: halten das Feuer an der Einschlagstelle am Leben.
	if (RANDOM_LONG(0, 1) != 0)
	{
		const Vector vecFlame = m_vecCenter + Vector(RANDOM_FLOAT(-48.0f, 48.0f),
											  RANDOM_FLOAT(-48.0f, 48.0f), RANDOM_FLOAT(0.0f, 8.0f));
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecFlame);
		WRITE_BYTE(TE_SPRITE);
		WRITE_COORD(vecFlame.x);
		WRITE_COORD(vecFlame.y);
		WRITE_COORD(vecFlame.z);
		WRITE_SHORT(m_iFireSprite);
		WRITE_BYTE(RANDOM_LONG(110, 240));
		WRITE_BYTE(RANDOM_LONG(6, 10));
		MESSAGE_END();
	}

	if (gpGlobals->time >= m_flEndTime)
	{
		UTIL_Remove(this);
		return;
	}
	pev->nextthink = gpGlobals->time + 0.35f;
}

//=========================================================
// The runtime-only meteor chunk spawned by meteor_god.
//=========================================================
class CMeteorChunk : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT ChunkTouch(CBaseEntity* pOther);
	void EXPORT SmokeThink();
	void EXPORT StreakerThink();
	void Configure(int iMeteorType, bool bDebris);

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	void EmitSmokeSprite(int iScale, float flHeight, int iSpread);
	void EmitChunkLight(int iRadius, int iLife);
	Vector GetImpactOrigin() const;
	void EmitRetailDebris(const Vector& vecImpact);
	void EmitEnhancedImpactEffects(const Vector& vecImpact, float flRadius);
	void EmitImpactFireAndSmoke(const Vector& vecImpact, float flRadius);

	int m_iMeteorType = 0;
	bool m_bDebris = false;
	float m_flRemoveTime = 0;
	int m_iSmokeSprite = 0;
	int m_iSmokePuffSprite = 0;
	int m_iBeamWidth = 0;
	int m_iImpactSprite = 0;
	int m_iDebrisModel = 0;
	int m_iSmallDebrisModel = 0;
};
LINK_ENTITY_TO_CLASS(meteor_god_chunk, CMeteorChunk);

TYPEDESCRIPTION CMeteorChunk::m_SaveData[] =
	{
		DEFINE_FIELD(CMeteorChunk, m_iMeteorType, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorChunk, m_bDebris, FIELD_BOOLEAN),
		DEFINE_FIELD(CMeteorChunk, m_flRemoveTime, FIELD_TIME),
		DEFINE_FIELD(CMeteorChunk, m_iSmokeSprite, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorChunk, m_iSmokePuffSprite, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorChunk, m_iBeamWidth, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorChunk, m_iImpactSprite, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorChunk, m_iDebrisModel, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorChunk, m_iSmallDebrisModel, FIELD_INTEGER),
	};

IMPLEMENT_SAVERESTORE(CMeteorChunk, CBaseEntity);

void CMeteorChunk::Precache()
{
	PrecacheModel("models/asteroid.mdl");
	m_iDebrisModel = PrecacheModel("models/rockgibsbig.mdl");
	m_iSmallDebrisModel = PrecacheModel("models/lavaball.mdl");
	PrecacheSound("meteor/meteor_hit1.wav");
	PrecacheSound("meteor/meteor_hit2.wav");
	PrecacheSound("meteor/meteor_hit3.wav");
	m_iImpactSprite = PrecacheModel("sprites/impact.spr");
	m_iSmokeSprite = PrecacheModel("sprites/smoke.spr");
	PrecacheModel("sprites/fire.spr");
	PrecacheModel("models/w_gibs_all.mdl"); // Steinbrocken (aus Hermes-Projekt w_gibs_all)
	m_iSmokePuffSprite = PrecacheModel("sprites/smokepuff.spr");
}

void CMeteorChunk::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("meteor_god_chunk");
	SET_MODEL(ENT(pev), "models/asteroid.mdl");
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	// Retail CMeteor::Spawn (0x100a6600): movetype (+0x108) = 9 (MOVETYPE_FLYMISSILE),
	// solid (+0x10c) = 1 (SOLID_TRIGGER) - der Meteor fliegt also als Trigger-Entitaet und
	// meldet den Aufprall ueber Touch, statt wie ein TOSS-Body an der Wand haengen zu bleiben.
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_FLYMISSILE;
	// Retail setzt zusaetzlich eine Zufalls-Drehung: avelocity (+0x5c..+0x64) je Achse
	// RANDOM_FLOAT(-150, 150) (0x100a6600).
	pev->avelocity = Vector(RANDOM_FLOAT(-150.0f, 150.0f), RANDOM_FLOAT(-150.0f, 150.0f),
		RANDOM_FLOAT(-150.0f, 150.0f));
	SetTouch(&CMeteorChunk::ChunkTouch);

	m_flRemoveTime = gpGlobals->time + 8.0f;
	pev->nextthink = m_flRemoveTime;
	SetThink(&CBaseEntity::SUB_Remove);
}

void CMeteorChunk::Configure(int iMeteorType, bool bDebris)
{
	m_iMeteorType = iMeteorType;
	m_bDebris = bDebris;

	// CMeteor has distinct streak, large and small paths in the retail DLL.
	// Keep the same visible families while retaining one safely bounded
	// runtime entity in the clean-room implementation.
	// Zufaellige Schweifbreite je Meteor, damit der Schwarm ungleichmaessig wirkt.
	m_iBeamWidth = m_iMeteorType == 2 ? RANDOM_LONG(38, 72) : (m_iMeteorType == 1 ? RANDOM_LONG(24, 48) : RANDOM_LONG(16, 34));

	if (m_iMeteorType == 0)
	{
		SET_MODEL(ENT(pev), "models/lavaball.mdl");
		UTIL_SetSize(pev, g_vecZero, g_vecZero);
	}
	else if (m_iMeteorType == 1)
	{
		SET_MODEL(ENT(pev), "models/asteroid.mdl");
		UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	}
	else
	{
		// CMeteor::Spawn uses asteroid.mdl for both smoking paths. The
		// rock-gib model belongs only to the optional break-model debris.
		SET_MODEL(ENT(pev), "models/asteroid.mdl");
		UTIL_SetSize(pev, Vector(-16, -16, -16), Vector(16, 16, 16));
	}

	// FUN_100a6840, shared by all three retail types, adds a separate
	// beam-follow trail. This coexists with the discrete Little/Big smoke
	// particles below; it is not a replacement for them.
	if (m_iMeteorType == 0)
		pev->effects |= EF_LIGHT;
	else
		pev->effects &= ~EF_LIGHT;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(m_iSmokeSprite);
	// Retail nutzt life = Typ+2 und Breite = type*2+1. Auf Wunsch deutlich laenger und
	// breiter, damit der Schweif stehen bleibt statt sofort zu verschwinden.
	// Bytes sind Zehntel-Einheiten. Lebensdauer 3 s (30), Breite bewusst zwischen Retail
	// (0.1-0.5) und der zu breiten Zwischenfassung: 4 / 6 / 8 Welteinheiten.
	WRITE_BYTE(30);
	WRITE_BYTE(m_iBeamWidth);
	WRITE_BYTE(224);
	WRITE_BYTE(224);
	WRITE_BYTE(255);
	WRITE_BYTE(100);
	MESSAGE_END();

	// Retail keeps StreakerThink free of Little/Big smoke. The two other
	// types emit three independent TE_SPRITE particles per think instead
	// of relying on the shared beam-follow trail alone.
	if (m_iMeteorType == 0)
	{
		// Retail setzt fuer den Streaker ebenfalls einen Think (StreakerThink,
		// 0x100a6600) mit nextthink +0.1; er raucht nicht, leuchtet aber.
		SetThink(&CMeteorChunk::StreakerThink);
		pev->nextthink = gpGlobals->time + 0.1f;
	}
	else
	{
		SetThink(&CMeteorChunk::SmokeThink);
		pev->nextthink = gpGlobals->time + 0.1f;
	}
}

void CMeteorChunk::EmitSmokeSprite(int iScale, float flHeight, int iSpread)
{
	// Auch die Schweifwolken zaehlen gegen das Partikelbudget der Engine.
	if (MeteorParticleBudget(1) == 0)
		return;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(pev->origin.x + RANDOM_LONG(-iSpread, iSpread));
	WRITE_COORD(pev->origin.y + RANDOM_LONG(-iSpread, iSpread));
	WRITE_COORD(pev->origin.z + flHeight);
	WRITE_SHORT(m_iSmokeSprite);
	WRITE_BYTE(iScale);
	WRITE_BYTE(20);
	MESSAGE_END();
}

void CMeteorChunk::EmitChunkLight(int iRadius, int iLife)
{
	// Kleines dynamisches Licht, das den fallenden Meteor und seinen Rauch beleuchtet.
	// Nicht aus dem Retail-Dump (dort nur renderfx GlowShell), sondern auf Wunsch fuer die
	// Sichtbarkeit des Schwarms ergaenzt.
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_DLIGHT);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_BYTE(iRadius / 10);
	WRITE_BYTE(255);
	WRITE_BYTE(140);
	WRITE_BYTE(60);
	WRITE_BYTE(iLife / 10);
	WRITE_BYTE(20);
	MESSAGE_END();
}

void CMeteorChunk::StreakerThink()
{
	constexpr float WORLD_BOUNDS = 4096.0f;
	if (pev->origin.x < -WORLD_BOUNDS || pev->origin.x > WORLD_BOUNDS ||
		pev->origin.y < -WORLD_BOUNDS || pev->origin.y > WORLD_BOUNDS ||
		pev->origin.z < -WORLD_BOUNDS || pev->origin.z > WORLD_BOUNDS)
	{
		UTIL_Remove(this);
		return;
	}

	EmitChunkLight(120, 2);
	pev->nextthink = gpGlobals->time + 0.05f;
}

void CMeteorChunk::SmokeThink()
{
	// LittleSmokingMeteorThink and BigSmokingMeteorThink remove the retail
	// meteor only after it leaves the fixed world cube, not after a timer.
	constexpr float WORLD_BOUNDS = 4096.0f;
	if (pev->origin.x < -WORLD_BOUNDS || pev->origin.x > WORLD_BOUNDS ||
		pev->origin.y < -WORLD_BOUNDS || pev->origin.y > WORLD_BOUNDS ||
		pev->origin.z < -WORLD_BOUNDS || pev->origin.z > WORLD_BOUNDS)
	{
		UTIL_Remove(this);
		return;
	}

	// Retail setzt in beiden Smoking-Thinks vor den Sprites den Glut-Schein:
	// renderfx (+0x15c) = 0x13 (kRenderFxGlowShell), renderamt (+0x14c) = 100.0,
	// rendercolor (+0x150..+0x158) = 255.0 / 130.0 / 30.0 (0x100a6fef bzw. 0x100a72af).
	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = 100.0f;
	pev->rendercolor = Vector(255.0f, 130.0f, 30.0f);

	EmitChunkLight(m_iMeteorType == 2 ? 200 : 150, 2);

	if (m_iMeteorType == 2)
	{
		// BigSmokingMeteorThink @0x100a7230: three larger smoke sprites, plus zwei
		// zusaetzliche dichtere Wolken fuer eine rauchigere Spur.
		EmitSmokeSprite(32, 32.0f, 8);
		EmitSmokeSprite(70, 256.0f, 16);
		EmitSmokeSprite(96, 512.0f, 32);
		EmitSmokeSprite(77, 96.0f, 12);
		EmitSmokeSprite(96, 192.0f, 24);
	}
	else
	{
		// LittleSmokingMeteorThink @0x100a6f70: three smaller sprites, plus zwei
		// zusaetzliche Wolken.
		EmitSmokeSprite(45, 32.0f, 8);
		EmitSmokeSprite(38, 128.0f, 16);
		EmitSmokeSprite(29, 256.0f, 32);
		EmitSmokeSprite(70, 64.0f, 10);
		EmitSmokeSprite(90, 160.0f, 22);
	}

	// Retail-Takt ist 100 ms; fuer eine dichtere, laenger sichtbare Rauchspur hier 50 ms.
	pev->nextthink = gpGlobals->time + 0.05f;
}

Vector CMeteorChunk::GetImpactOrigin() const
{
	if (m_iMeteorType == 0)
		return pev->origin;

	Vector vecForward = pev->velocity.Normalize();
	if (m_iMeteorType == 1)
		return pev->origin + vecForward * -50.0f;

	return pev->origin + vecForward * RANDOM_LONG(-156, -128);
}

void CMeteorChunk::EmitRetailDebris(const Vector& vecImpact)
{
	if (!m_bDebris || m_iMeteorType == 0)
		return;

	const int iShardCount = RANDOM_LONG(5, 10);
	const int iDebrisModel = m_iMeteorType == 1 ? m_iSmallDebrisModel : m_iDebrisModel;
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecImpact);
	WRITE_BYTE(TE_BREAKMODEL);
	WRITE_COORD(vecImpact.x);
	WRITE_COORD(vecImpact.y);
	WRITE_COORD(vecImpact.z);
	WRITE_COORD(10);
	WRITE_COORD(10);
	WRITE_COORD(10);
	WRITE_COORD(RANDOM_LONG(-200, 200));
	WRITE_COORD(RANDOM_LONG(-200, 200));
	WRITE_COORD(RANDOM_LONG(-300, -100));
	WRITE_BYTE(10);
	WRITE_SHORT(iDebrisModel);
	WRITE_BYTE(iShardCount);
	WRITE_BYTE(60 / iShardCount);
	WRITE_BYTE(TE_BOUNCE_SHELL);
	MESSAGE_END();
}

void CMeteorChunk::EmitEnhancedImpactEffects(const Vector& vecImpact, float flRadius)
{
	// Deliberate non-retail presentation layer. Keep it separate from the
	// debris/smoke paths above so future fidelity work can opt out cleanly.
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecImpact);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(vecImpact.x);
	WRITE_COORD(vecImpact.y);
	WRITE_COORD(vecImpact.z);
	WRITE_SHORT(m_iImpactSprite);
	WRITE_BYTE(m_iMeteorType == 2 ? 32 : 20);
	WRITE_BYTE(18);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects).
	UTIL_ExplosionEffects(vecImpact, flRadius);

	// Visible expanding shockwave, analogous to the SDK's blast-cylinder
	// effects.  This is cosmetic; RadiusDamage below is authoritative.
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecImpact);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(vecImpact.x);
	WRITE_COORD(vecImpact.y);
	WRITE_COORD(vecImpact.z + 8);
	WRITE_COORD(vecImpact.x);
	WRITE_COORD(vecImpact.y);
	WRITE_COORD(vecImpact.z + 8 + flRadius / 0.2f);
	WRITE_SHORT(m_iSmokeSprite);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(3);
	WRITE_BYTE(14);
	WRITE_BYTE(0);
	WRITE_BYTE(255);
	WRITE_BYTE(128);
	WRITE_BYTE(32);
	WRITE_BYTE(220);
	WRITE_BYTE(0);
	MESSAGE_END();

}

void CMeteorChunk::EmitImpactFireAndSmoke(const Vector& vecImpact, float flRadius)
{
	// Feuer-Animation in zwei Schichten: ein kurzer Aufflacker-Burst und darunter eine
	// laenger brennende Glut (kleinere Framerate => die Frames laufen laenger).
	const int iBurst = m_iMeteorType == 2 ? 4 : 3;
	for (int i = 0; i < iBurst; ++i)
	{
		const Vector vecPos = vecImpact + Vector(RANDOM_FLOAT(-32.0f, 32.0f),
												RANDOM_FLOAT(-32.0f, 32.0f),
												RANDOM_FLOAT(0.0f, 24.0f));
		CSprite* pFire = CSprite::SpriteCreate("sprites/fire.spr", vecPos, true);
		if (!pFire)
			continue;

		pFire->SetTransparency(kRenderTransAdd, 255, 190, 110, 255, kRenderFxNone);
		pFire->SetScale(RANDOM_FLOAT(0.8f, 3.2f));
		pFire->AnimateAndDie(RANDOM_FLOAT(8.0f, 12.0f));
	}

	// Brennende Stelle: groessere, langsamer ablaufende Flammen. Bei 4-6 fps laufen die
	// 15 Frames 2,5-3,7 s, der Brand bleibt also nach dem Einschlag sichtbar.
	const int iBurn = m_iMeteorType == 2 ? 2 : 1;
	for (int i = 0; i < iBurn; ++i)
	{
		const Vector vecPos = vecImpact + Vector(RANDOM_FLOAT(-40.0f, 40.0f),
												RANDOM_FLOAT(-40.0f, 40.0f),
												RANDOM_FLOAT(0.0f, 8.0f));
		CSprite* pBurn = CSprite::SpriteCreate("sprites/fire.spr", vecPos, true);
		if (!pBurn)
			continue;

		pBurn->SetTransparency(kRenderTransAdd, 255, 140, 60, 255, kRenderFxNone);
		pBurn->SetScale(RANDOM_FLOAT(1.8f, 4.2f));
		pBurn->AnimateAndDie(RANDOM_FLOAT(4.0f, 6.0f));
	}

	// Stehende, dichte Rauchfahne ueber einen eigenen Emitter (2,5 s Nachspucken).
	{
		CMeteorSmoke* pSmokeEmitter = static_cast<CMeteorSmoke*>(Create("meteor_smoke", vecImpact, g_vecZero));
		if (pSmokeEmitter)
		{
			pSmokeEmitter->m_vecCenter = vecImpact;
			pSmokeEmitter->m_flStartTime = gpGlobals->time;
			pSmokeEmitter->m_flEndTime = gpGlobals->time + RANDOM_FLOAT(1.8f, 2.4f);
		}
	}

	// Rauch: dichtere Fahne aus zwei Hoehenbaendern, groessere Wolken als zuvor.
	const int iSmoke = MeteorParticleBudget(m_iMeteorType == 2 ? 10 : 6);
	for (int i = 0; i < iSmoke; ++i)
	{
		const float flHeight = (i % 2) ? RANDOM_FLOAT(0.0f, 32.0f) : RANDOM_FLOAT(40.0f, 110.0f);
		const Vector vecPos = vecImpact + Vector(RANDOM_FLOAT(-56.0f, 56.0f),
												RANDOM_FLOAT(-56.0f, 56.0f), flHeight);
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecPos);
		WRITE_BYTE(TE_SPRITE);
		WRITE_COORD(vecPos.x);
		WRITE_COORD(vecPos.y);
		WRITE_COORD(vecPos.z);
		WRITE_SHORT((i % 3) ? m_iSmokeSprite : m_iSmokePuffSprite);
		WRITE_BYTE((i % 3) == 0 ? RANDOM_LONG(50, 110) : ((i % 3) == 1 ? RANDOM_LONG(110, 190)
																							: RANDOM_LONG(190, 255)));
		WRITE_BYTE(RANDOM_LONG(6, 12));
		MESSAGE_END();
	}

	// Zusaetzlich ein paar langsam aufsteigende TE_SMOKE-Wolken fuer eine stehende Fahne.
	const int iDrift = MeteorParticleBudget(4);
	for (int i = 0; i < iDrift; ++i)
	{
		const Vector vecPos = vecImpact + Vector(RANDOM_FLOAT(-40.0f, 40.0f),
												RANDOM_FLOAT(-40.0f, 40.0f), RANDOM_FLOAT(0.0f, 24.0f));
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecPos);
		WRITE_BYTE(TE_SMOKE);
		WRITE_COORD(vecPos.x);
		WRITE_COORD(vecPos.y);
		WRITE_COORD(vecPos.z);
		WRITE_SHORT(m_iSmokeSprite);
		WRITE_BYTE(RANDOM_LONG(120, 250));
		WRITE_BYTE(RANDOM_LONG(8, 14));
		MESSAGE_END();
	}
}

void CMeteorChunk::ChunkTouch(CBaseEntity* pOther)
{
	const Vector vecImpact = GetImpactOrigin();
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "meteor/meteor_hit1.wav", 1.0, ATTN_NORM, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "meteor/meteor_hit2.wav", 1.0, ATTN_NORM, 0, 100);
		break;
	default:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "meteor/meteor_hit3.wav", 1.0, ATTN_NORM, 0, 100);
		break;
	}

	// Ausnahme: Auf der schwarzen Canyon-Textur ("black") keine Effekte abspielen.
	// Die Textur wird ueber TRACE_TEXTURE bestimmt; Schaden, Sound und Erschuetterung bleiben.
	// Ausnahme: Auf der schwarzen Canyon-Textur ("black") keine Effekte abspielen.
	// Textur ueber TRACE_TEXTURE; Brush-Entities (func_wall & Co.) muessen als Tragee-Entity
	// uebergeben werden, sonst liefert der Trace dort keinen Namen und der Einschlag wuerde
	// faelschlich als "keine Oberflaeche" behandelt.
	bool bSuppressEffects = false;
	{
		// Sonde entgegen der Flugrichtung (deckt Wand- und Deckentreffer ab), ersatzweise
		// senkrecht nach unten.
		Vector vecProbe = -1.0f * pev->velocity;
		if (vecProbe.Length() < 1.0f)
			vecProbe = Vector(0, 0, -1);
		vecProbe.Normalize();

		float rgflStart[3] = {vecImpact.x + vecProbe.x * 4.0f, vecImpact.y + vecProbe.y * 4.0f,
			vecImpact.z + vecProbe.z * 4.0f};
		float rgflEnd[3] = {vecImpact.x + vecProbe.x * 48.0f, vecImpact.y + vecProbe.y * 48.0f,
			vecImpact.z + vecProbe.z * 48.0f};

		const bool bBrushHit = pOther && pOther->pev->solid == SOLID_BSP;
		// Welt-Treffer direkt gegen die Welt tracen; nur echte Brush-Entities separat.
		const bool bWorldHit = bBrushHit && pOther->edict() == INDEXENT(0);
		edict_t* pTraceEntity = (bBrushHit && !bWorldHit) ? ENT(pOther->pev) : INDEXENT(0);

		// Mehrere Distanzen und Richtungen probieren: bei Treffern innerhalb von Brush-
		// Geometrie liegt die Oberflaeche oft ausserhalb des kurzen 48-Einheiten-Fensters.
		const char* szTexture = nullptr;
		const float rgflDistances[3] = {48.0f, 128.0f, 320.0f};
		for (int iPass = 0; iPass < 2 && !szTexture; ++iPass)
		{
			const Vector vecDir = iPass == 0 ? vecProbe : Vector(0, 0, -1);

			for (int iDist = 0; iDist < 3 && !szTexture; ++iDist)
			{
				const float flDist = rgflDistances[iDist];
				float rgflS[3] = {vecImpact.x + vecDir.x * 4.0f, vecImpact.y + vecDir.y * 4.0f,
					vecImpact.z + vecDir.z * 4.0f};
				float rgflE[3] = {vecImpact.x + vecDir.x * flDist, vecImpact.y + vecDir.y * flDist,
					vecImpact.z + vecDir.z * flDist};

				szTexture = TRACE_TEXTURE(pTraceEntity, rgflS, rgflE);
				if (szTexture && szTexture[0] == '\0')
					szTexture = nullptr;

				if (!szTexture && pTraceEntity != INDEXENT(0))
				{
					szTexture = TRACE_TEXTURE(INDEXENT(0), rgflS, rgflE);
					if (szTexture && szTexture[0] == '\0')
						szTexture = nullptr;
				}
			}
		}

		if (szTexture)
		{
			const char* szName = szTexture;
			while (*szName == '{' || *szName == '+')
				++szName;
			// "black" = Canyon-Grund; "sky" = Himmel-/Skybrush-Flaechen (im Canyon ist der
			// Boden teilweise nicht als Brush vorhanden, der Meteor trifft dann die Skybox).
			bSuppressEffects = (stricmp(szName, "black") == 0) || (strnicmp(szName, "sky", 3) == 0);
		}
		else
		{
			// Kein Texturtreffer: Treffer auf eine bewegliche Entity oder ein in der Luft
			// zerstoerter Splitter. Brush-Treffer (func_wall u. a.) behalten die Effekte,
			// auch wenn ihre Textur nicht ermittelbar ist.
			bSuppressEffects = !bBrushHit;
		}



	}

	// Branddecal am Einschlagspunkt: kurzer Trace von knapp ueber dem Impact nach unten.
	// (Nutzerwunsch; im Retail-Dump nicht nachgewiesen, aber ohne Decal wirkt der Einschlag
	//  leer.)
	{
		TraceResult trDecal;
		UTIL_TraceLine(vecImpact + Vector(0, 0, 8), vecImpact - Vector(0, 0, 48), ignore_monsters,
			ENT(pev), &trDecal);
		if (trDecal.flFraction < 1.0f && !bSuppressEffects)
			UTIL_DecalTrace(&trDecal, DECAL_SCORCH1 + RANDOM_LONG(0, 1));
	}

	if (!bSuppressEffects)
		EmitChunkLight(m_iMeteorType == 2 ? 260 : 200, 3);

	// Schaden/Radius reduziert (Nutzerbefund: toetet Roboter in der Station zu leicht);
	// gesprengt wird vom Bodenpunkt aus, damit die Explosion nicht in Waenden steckt.
	const float flDamage = m_iMeteorType == 2 ? 55.0f : 30.0f;
	const float flRadius = m_iMeteorType == 2 ? 320.0f : 110.0f;
	// Bodenpunkt auch fuer den Schaden ermitteln (ausserhalb des Effekt-Blocks).
	Vector vecRockOrigin = vecImpact;
	{
		TraceResult trGround0;
		UTIL_TraceLine(vecImpact, vecImpact - Vector(0, 0, 512.0f), ignore_monsters, ENT(pev), &trGround0);
		if (trGround0.flFraction < 1.0f && !trGround0.fStartSolid)
			vecRockOrigin = trGround0.vecEndPos + Vector(0, 0, 8.0f);
	}

	// Fluessigkeiten gleich mit ausgewertet: unter Wasser/Lava/Schleim kein Feuer und kein Rauch
	// (Nutzerwunsch), und auch keine Steinbrocken.
	const int iContents = UTIL_PointContents(vecRockOrigin);
	const bool bLiquidHit = (iContents == CONTENTS_WATER) || (iContents == CONTENTS_LAVA) ||
							(iContents == CONTENTS_SLIME);

	if (!bSuppressEffects)
	{
		EmitRetailDebris(vecImpact);
		EmitEnhancedImpactEffects(vecImpact, flRadius);

		// Rauch und zusaetzliche Feuer-Animation am Einschlag (Nutzerwunsch) - nicht unter
		// Wasser/Lava/Schleim.
		if (!bLiquidHit)
			EmitImpactFireAndSmoke(vecImpact, flRadius);

		// Steinfragmente: Client erzeugt Temp-Modelle models/w_gibs_all.mdl (Skin 1 = GIB_ROCK),
		// portiert aus hl15we EV_Wallgib.
		// Spawnpunkt auf Bodenhoehe: vom Impact-Punkt senkrecht nach unten tasten und am
		// Trefferpunkt (leicht darueber) spawnen; ohne Treffer bleibt der alte Versatz.

		// Keine Steine bei Einschlaegen unter Wasser, Lava oder Schleim (Nutzerwunsch).
		if (!bLiquidHit)
		{
		MESSAGE_BEGIN(MSG_PAS, gmsgRockGib, vecRockOrigin);
		WRITE_COORD(vecRockOrigin.x);
		WRITE_COORD(vecRockOrigin.y);
		WRITE_COORD(vecRockOrigin.z);
		WRITE_BYTE(MeteorParticleBudget(m_iMeteorType == 2 ? RANDOM_LONG(8, 12) : RANDOM_LONG(4, 8)));
		WRITE_SHORT(m_iMeteorType == 2 ? RANDOM_LONG(600, 900) : RANDOM_LONG(350, 650));
		WRITE_BYTE(RANDOM_LONG(120, 220)); // Scale * 50 -> 2,4 .. 4,4 (deutlich groessere Brocken)
		WRITE_BYTE(1);                   // Skin 1 = GIB_ROCK
		MESSAGE_END();
		}

		// Erschuetterung nur bei Einschlaegen, die auch Effekte abspielen (Nutzerwunsch).
		// Radius deutlich groesser als der Schadensradius, damit auch weit entfernte Einschlaege
		// noch spuerbar sind (lineare Abnahme ueber den Radius).
		MeteorScreenShake(vecImpact, m_iMeteorType == 2 ? 6.0f : 3.0f,
			m_iMeteorType == 2 ? 16000.0f : 10000.0f);
	}

	// Erschuetterung beim Einschlag (Nutzerwunsch). Der Radius des ScreenShake ist bewusst
	// groesser als der Schadensradius, damit auch entfernte Einschlaege noch spuerbar sind;
	// im Retail gibt es an dieser Stelle nur RadiusDamage.
	// Both retail type-specific paths first move the meteor to their
	// calculated impact point and then apply the area damage from there.
	RadiusDamage(vecRockOrigin, pev, pev, flDamage, flRadius, CLASS_NONE, DMG_BLAST);

	UTIL_Remove(this);
}

//=========================================================
// meteor_target - CMeteorTarget. Triggered, targeted Meteor path.
// Retail Use @0x100a8210 projects the spawn origin by heightabove along
// pitch, traces that segment, then creates a CMeteor with the configured
// raw type and a downward start vector. This closes the map-facing path
// used by RUST4B/RUST5A/rust6b/rust6c/rust6d.
//=========================================================
class CMeteorTarget : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_flHeightAbove = 0.0f;
	int m_iMeteorType = 1;
};
LINK_ENTITY_TO_CLASS(meteor_target, CMeteorTarget);

TYPEDESCRIPTION CMeteorTarget::m_SaveData[] =
	{
		DEFINE_FIELD(CMeteorTarget, m_flHeightAbove, FIELD_FLOAT),
		DEFINE_FIELD(CMeteorTarget, m_iMeteorType, FIELD_INTEGER),
	};

IMPLEMENT_SAVERESTORE(CMeteorTarget, CBaseEntity);

void CMeteorTarget::Precache()
{
	PrecacheModel("models/asteroid.mdl");
	PrecacheModel("sprites/smoke.spr");
	PrecacheModel("sprites/impact.spr");
	PrecacheModel("sprites/chip.spr");
	UTIL_PrecacheOther("meteor_god_chunk");
}

void CMeteorTarget::Spawn()
{
	Precache();
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetOrigin(pev, pev->origin);
}

bool CMeteorTarget::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		pev->angles.x = atof(pkvd->szValue);
		return true;
	}
	if (FStrEq(pkvd->szKeyName, "heightabove"))
	{
		m_flHeightAbove = atof(pkvd->szValue);
		return true;
	}
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_iMeteorType = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CMeteorTarget::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	UTIL_MakeAimVectors(pev->angles);
	const Vector vecUp = gpGlobals->v_forward;
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + vecUp * m_flHeightAbove,
		ignore_monsters, edict(), &tr);

	CBaseEntity* pEntity = CBaseEntity::Create("meteor_god_chunk", tr.vecEndPos, g_vecZero, edict());
	if (pEntity)
	{
		auto pMeteor = static_cast<CMeteorChunk*>(pEntity);
		pMeteor->Configure(m_iMeteorType, false);
		pEntity->pev->velocity = vecUp * -1400.0f;
		pEntity->pev->angles = UTIL_VecToAngles(pEntity->pev->velocity);
	}
	// CMeteorTarget::Use removes the one-shot marker after creating its
	// meteor. This remains true even if entity allocation fails.
	UTIL_Remove(this);
}

//=========================================================
// meteor_god - CMeteorGod. Invisible ambient meteor-shower spawner
// (see file header re: eruption-math simplification).
//=========================================================
class CMeteorGod : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT ShowerThink();

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	float m_flDropSizeX = 1.0;
	float m_flDropSizeY = 5.0;
	float m_flMinDropFreq = 1.0;
	float m_flMaxDropFreq = 1.0;
	int m_iTypeStreak = 1;
	int m_iTypeLarge = 1;
	int m_iTypeSmall = 1;
	bool m_bDebris = false;
	bool m_bActive = true;
	Vector m_vecDropDirection = Vector(0, 0, 1);
};
LINK_ENTITY_TO_CLASS(meteor_god, CMeteorGod);

TYPEDESCRIPTION CMeteorGod::m_SaveData[] =
	{
		DEFINE_FIELD(CMeteorGod, m_flDropSizeX, FIELD_FLOAT),
		DEFINE_FIELD(CMeteorGod, m_flDropSizeY, FIELD_FLOAT),
		DEFINE_FIELD(CMeteorGod, m_flMinDropFreq, FIELD_FLOAT),
		DEFINE_FIELD(CMeteorGod, m_flMaxDropFreq, FIELD_FLOAT),
		DEFINE_FIELD(CMeteorGod, m_iTypeStreak, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorGod, m_iTypeLarge, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorGod, m_iTypeSmall, FIELD_INTEGER),
		DEFINE_FIELD(CMeteorGod, m_bDebris, FIELD_BOOLEAN),
		DEFINE_FIELD(CMeteorGod, m_bActive, FIELD_BOOLEAN),
		DEFINE_FIELD(CMeteorGod, m_vecDropDirection, FIELD_VECTOR),
	};

IMPLEMENT_SAVERESTORE(CMeteorGod, CBaseEntity);

bool CMeteorGod::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "dropsizex"))
	{
		m_flDropSizeX = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "dropsizey"))
	{
		m_flDropSizeY = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "mindropfreq"))
	{
		m_flMinDropFreq = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "maxdropfreq"))
	{
		m_flMaxDropFreq = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "typestreak"))
	{
		m_iTypeStreak = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "typelarge"))
	{
		m_iTypeLarge = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "typesmall"))
	{
		m_iTypeSmall = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "debris"))
	{
		// Retail stores the parsed map value inverted, then emits break-model
		// debris only when the stored field is non-zero.
		m_bDebris = atoi(pkvd->szValue) == 0;
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		pev->angles.x = atof(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CMeteorGod::Spawn()
{
	Precache();

	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetOrigin(pev, pev->origin);
	// Retail (Spawn 0x100a62f0, Richtung landet in this+0x40..0x48) rechnet die Fallrichtung
	// direkt aus pev->angles ohne die Pitch-Negation von UTIL_MakeAimVectors. Mit der
	// Negation zeigt v_forward bei pitch -90 nach unten, und velocity = dir * RANDOM(-1600,-1200)
	// wird dadurch nach OBEN gespiegelt - die Meteore stiegen statt zu fallen.
	{
		float rgflAngles[3] = {pev->angles.x, pev->angles.y, pev->angles.z};
		float rgflForward[3] = {0, 0, 0};
		float rgflRight[3] = {0, 0, 0};
		float rgflUp[3] = {0, 0, 0};
		g_engfuncs.pfnAngleVectors(rgflAngles, rgflForward, rgflRight, rgflUp);
		m_vecDropDirection = Vector(rgflForward[0], rgflForward[1], rgflForward[2]);
	}

	// Retail Spawn starts inactive when bit 1 is set; Use toggles this
	// state independently of the USE_TYPE argument.
	m_bActive = (pev->spawnflags & 1) == 0;

	SetThink(&CMeteorGod::ShowerThink);
	pev->nextthink = gpGlobals->time + 0.1; // Retail Spawn @0x100a62f0: +0.1 (frisch dekompiliert 2026-10-31)
}

void CMeteorGod::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_bActive = !m_bActive;
}

void CMeteorGod::Precache()
{
	PrecacheModel("models/rockgibsbig.mdl");
	PrecacheModel("models/lavaball.mdl");
	PrecacheModel("models/asteroid.mdl");
	PrecacheModel("sprites/smoke.spr");
	PrecacheModel("sprites/impact.spr");
	PrecacheModel("sprites/chip.spr");
	PrecacheSound("meteor/meteor_hit1.wav");
	PrecacheSound("meteor/meteor_hit2.wav");
	PrecacheSound("meteor/meteor_hit3.wav");
	UTIL_PrecacheOther("meteor_god_chunk");
}

void CMeteorGod::ShowerThink()
{
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(m_flMinDropFreq, m_flMaxDropFreq);
	if (!m_bActive)
		return;

	// CMeteorGod::Think samples the configured drop sizes as world units.
	Vector vecSpot = pev->origin + Vector(RANDOM_FLOAT(-m_flDropSizeX, m_flDropSizeX), RANDOM_FLOAT(-m_flDropSizeY, m_flDropSizeY), 0);

	CBaseEntity* pChunk = CBaseEntity::Create("meteor_god_chunk", vecSpot, g_vecZero, edict());
	if (pChunk)
	{
		// Retail (0x100a6488-0x100a64b9): RANDOM_LONG(0, streak+small+large) und dann
		// aufsteigend zugeordnet - <= streak -> Streaker(0), <= streak+small -> Small(1),
		// <= streak+small+large -> Large(2).
		const int iTotalWeight = m_iTypeStreak + m_iTypeSmall + m_iTypeLarge;
		const int iChoice = RANDOM_LONG(0, iTotalWeight > 0 ? iTotalWeight : 1);
		int iMeteorType = 0;
		if (iChoice > m_iTypeStreak && iChoice <= m_iTypeStreak + m_iTypeSmall)
			iMeteorType = 1;
		else if (iChoice > m_iTypeStreak + m_iTypeSmall)
			iMeteorType = 2;

		auto pMeteor = static_cast<CMeteorChunk*>(pChunk);
		pMeteor->Configure(iMeteorType, m_bDebris);
		// FUN_100a6500 jitters the first direction component by +/-0.1,
		// then applies a random scalar in the range -1600..-1200.
		Vector vecDropDirection = m_vecDropDirection;
		vecDropDirection.x += RANDOM_FLOAT(-0.1f, 0.1f);
		pChunk->pev->velocity = vecDropDirection * RANDOM_FLOAT(-1600.0f, -1200.0f);
		pChunk->pev->angles = UTIL_VecToAngles(pChunk->pev->velocity);
	}
}
