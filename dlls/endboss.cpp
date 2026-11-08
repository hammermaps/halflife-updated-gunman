//=========================================================
// monster_endboss - CEndBoss, the final boss (end1), plus its three
// companion classes endboss_rocket (CEndBossRocket), endboss_kataball
// (CEndBossKataball), and endboss_gib (CEndBossGib). The sixth and
// last of the six points deferred during the map-driven pass (see
// [[project-monster-endboss-deferred]] memory) - after this, every
// classname gap on every scanned map in maps-src/ is closed.
//
// Decompiled fresh from gunman.dll this session: LINK @0x100d4780,
// vtable @0x10108da8, Spawn @0x100d64d0, Precache @0x100d66a0,
// Classify (slot 8, @0x100d47d0) confirmed constant 0x14=20 - a
// previously-unseen class value, added as CLASS_ALIEN_BOSS in
// cbase.h. HandleAnimEvent (slot 58, @0x100d49e0) fully decompiled:
//   event 3 -> spawns endboss_rocket (vtable 0x1010871c) at an
//     attachment point, occasionally (50% chance) triggers a
//     screen-shake message sequence.
//   event 4 -> spawns endboss_kataball (vtable 0x101084e8) at an
//     attachment point, plays "endboss/kata_cannon1.wav", increments
//     an internal kataball counter.
//   events 6/7 -> a pair of long-range (4096 unit) claw-swipe attacks
//     via the shared multi-pellet traceline helper FUN_1000f090 (the
//     same helper already reproduced as FireBullets() calls for
//     CBanditGrunt::MechaVolley/CChopper elsewhere in this project) -
//     confirmed as two mirrored arm attacks (left/right), reproduced
//     here as a single ArmSwipe() used by both events.
//   event 10 -> a stomp attack: skill-scaled damage (130/150/200) at
//     origin + forward*64 in radius 300, ScreenShake (12, 3, 0.5, 600)
//     from the same point, and ambient "endboss/end_stomp2.wav".
//   default -> falls through to the shared generic
//     footstep/miscellaneous animation-event handler (cosmetic only,
//     same pattern used by every other monster_human_*/boss class in
//     this project).
// Spawn confirms: model "models/endboss.mdl", bbox
// (-48,-48,0)/(48,48,300) (a very tall boss hull), SOLID_BBOX,
// health=200, view_ofs.z=300, an internal 4096-unit "attack range"
// field (matches the confirmed 4096.0 constant used by both
// ArmSwipe's FUN_1000f090 calls). Precache confirms 5
// UTIL_PrecacheOther calls: endboss_kataball, endboss_rocket,
// "antirocketflare" (Spawn is a trivial invisible, three-second
// CBaseEntity; EndBoss RunAI's slot-62 path creates five decoys when a
// nearby vehicle_tank_rocket targets the boss. Attachment origin, random
// launch velocity, owner, and rocket-handle retargeting are ported),
// endboss_gib, and "sphere_explosion" (ALREADY implemented in
// gunman_custom_entities.cpp for CITY2B - not invoked here either,
// since its Use-trigger-based API doesn't fit a direct in-code
// death-explosion call; a plain TE_EXPLOSION tempentity is used
// instead, same pattern as CDummyBot::Explode()).
//
// endboss_rocket: LINK @0x100d3310, vtable @0x1010871c, Spawn
// @0x100d3430, Precache @0x100d33d0, IgniteThink @0x100d38d0,
// AccelerateThink @0x100d3a40, RocketTouch @0x100d3ed0. Confirmed
// model "models/rocket.mdl", MOVETYPE_FLY, SOLID_BBOX, a short
// randomized ignite delay (0.4-0.8s) before launch, and 4 confirmed
// explosion sounds ("endboss/boss_boom1-4.wav"). AccelerateThink's
// real homing-steering math (aim-toward-target velocity blending) is
// NOT reproduced - same simplification already established for
// human_demoman.cpp's CDemomanRocket and rebar_fauna.cpp's
// CClusterGod submunitions: straight-line flight at a fixed speed
// from the confirmed launch trajectory, exploding via CGrenade on
// touch.
//
// endboss_kataball: LINK @0x100d27f0, vtable @0x101084e8, Spawn
// @0x100d2930, Precache @0x100d28b0, IgniteThink @0x100d2d70,
// AccelerateThink @0x100d2db0. Confirmed sprite "sprites/kata.spr",
// kRenderGlow-style rendermode, and 3 confirmed impact sounds
// ("endboss/imp1-3_gs.wav"). Same AccelerateThink homing
// simplification as endboss_rocket.
//
// endboss_gib: LINK @0x100d4650, vtable @0x10108b7c, Spawn
// @0x100d46c0. Reuses the boss's OWN model ("models/endboss.mdl") as
// a non-solid bounce fragment. It starts the "death" sequence with
// bodygroup 1 set to 2, then CEndBossGib::FlopAnim @0x100d4600 advances
// it until completion and freezes the result; it is not a timed
// self-removing tossed prop.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "animation.h"
#include "weapons.h"
#include "soundent.h"
#include "effects.h"
#include "gamerules.h"
#include "skill.h"
#include "dbg_log.h"
#include "scripted.h"

#define ENDBOSS_AE_ROCKET 3
#define ENDBOSS_AE_KATABALL 4
#define ENDBOSS_AE_ARMSWIPE_L 6
#define ENDBOSS_AE_ARMSWIPE_R 7
#define ENDBOSS_AE_STOMP 10

//=========================================================
// endboss_rocket - CEndBossRocket.
//=========================================================
class CEndBossRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void SetTarget(CBaseEntity* pTarget) { m_hEnemy = pTarget; }
	void EXPORT IgniteThink();
	void EXPORT RocketTouch(CBaseEntity* pOther);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_iTrailSprite = 0;
	float m_flExpireTime = 0.0f;
};
LINK_ENTITY_TO_CLASS(endboss_rocket, CEndBossRocket);

TYPEDESCRIPTION CEndBossRocket::m_SaveData[] =
{
	DEFINE_FIELD(CEndBossRocket, m_flExpireTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CEndBossRocket, CGrenade);

void CEndBossRocket::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("endboss_rocket");
	SET_MODEL(ENT(pev), "models/rocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	UTIL_MakeVectors(pev->angles);
	// Retail starts at 500 and ignites after a random 0.4-0.8 seconds;
	// IgniteThink then raises the straight-flight approximation to 700.
	pev->velocity = gpGlobals->v_forward * 500.0f;

	// Nachtrag 2026-09-06 (Nutzer-Meldung "Raketen-Schweif fehlt auch bei
	// anderen Raketen"): siehe UTIL_RocketTrail (util.cpp) - gleiches
	// Muster wie CRpgRocket/CAnimeRocket/CDmlRocket.
	UTIL_RocketTrail(this, m_iTrailSprite);

	SetTouch(&CEndBossRocket::RocketTouch);
	m_flExpireTime = gpGlobals->time + 8.0f;
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.4f, 0.8f);
	SetThink(&CEndBossRocket::IgniteThink);
}

void CEndBossRocket::IgniteThink()
{
	// Retail CEndBossRocket::IgniteThink @ 0x100d38d0 normalizes the
	// launch vector and applies the confirmed 700-unit cruise speed.
	if (pev->velocity != g_vecZero)
		pev->velocity = pev->velocity.Normalize() * 700.0f;

	SetThink(&CEndBossRocket::SUB_Remove);
	pev->nextthink = m_flExpireTime;
}

void CEndBossRocket::Precache()
{
	PrecacheModel("models/rocket.mdl");
	PrecacheModel("sprites/firebeam.spr");
	// Sprite-Pruefung 2026-09-06: smoke.spr statt flame.spr, siehe dml.cpp
	// Nachtrag 2 fuer die Begruendung (flame.spr = Glow-Burst, kein Schweif).
	m_iTrailSprite = PrecacheModel("sprites/smoke.spr");
	PrecacheSound("endboss/boss_boom1.wav");
	PrecacheSound("endboss/boss_boom2.wav");
	PrecacheSound("endboss/boss_boom3.wav");
	PrecacheSound("endboss/boss_boom4.wav");
}

void CEndBossRocket::RocketTouch(CBaseEntity* pOther)
{
	const char* booms[] = {"endboss/boss_boom1.wav", "endboss/boss_boom2.wav", "endboss/boss_boom3.wav", "endboss/boss_boom4.wav"};
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, booms[RANDOM_LONG(0, 3)], 1.0, ATTN_NORM);
	CGrenade::ExplodeTouch(pOther);
}

//=========================================================
// endboss_kataball - CEndBossKataball.
//=========================================================
class CEndBossKataball : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void SetTarget(CBaseEntity* pTarget) { m_hEnemy = pTarget; }
	void EXPORT IgniteThink();
	void EXPORT BallThink();
	void EXPORT KataTouch(CBaseEntity* pOther);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_flExpireTime = 0.0f;
	// Retail +0x27c: Handle fuer events/kataimpact.sc (Precache 0x100d2902).
	unsigned short m_usKataImpact = 0;
};
LINK_ENTITY_TO_CLASS(endboss_kataball, CEndBossKataball);

TYPEDESCRIPTION CEndBossKataball::m_SaveData[] =
{
	DEFINE_FIELD(CEndBossKataball, m_flExpireTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CEndBossKataball, CGrenade);

void CEndBossKataball::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("endboss_kataball");
	SET_MODEL(ENT(pev), "sprites/kata.spr");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	// Retail Spawn 0x100d2930: rendermode (+0x148) = 5 (kRenderTransAdd),
	// renderamt (+0x14c) = 255.0, renderfx (+0x15c) = 0, framerate (+0x138) = 15.0.
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->renderfx = kRenderFxNone;
	pev->framerate = 15.0;

	UTIL_MakeVectors(pev->angles);
	pev->velocity = gpGlobals->v_forward * 400.0f;

	// Retail Spawn @ 0x100d2930 arms the Kataball one 0.1-second think
	// later; before that it deliberately has no collision callback.
	SetTouch(NULL);
	m_flExpireTime = gpGlobals->time + 8.0f;
	pev->nextthink = gpGlobals->time + 0.1f;
	SetThink(&CEndBossKataball::IgniteThink);
}

void CEndBossKataball::IgniteThink()
{
	// Retail CEndBossKataball::IgniteThink @ 0x100d2d70: Touch = KataTouch,
	// Think = AccelerateThink (0x100d2db0), nextthink +0.1, movetype 5 (FLY).
	SetTouch(&CEndBossKataball::KataTouch);
	SetThink(&CEndBossKataball::BallThink);
	pev->movetype = MOVETYPE_FLY;
	pev->nextthink = gpGlobals->time + 0.1f;
}

// Retail CEndBossKataball::AccelerateThink (0x100d2db0), Animations- und
// Beschleunigungsteil. Belegt:
//   nextthink = time + 0.1
//   frame = (int)(frame + 1) % 9           <- 10 Hz, 9 Bilder von kata.spr
//   Geschwindigkeit waechst je Skill: +7 bis 750 (mittel), +10 bis 1000
//   (schwer), sonst +5 bis 500.
// Steering-Teil ebenfalls aus 0x100d2db0:
//   Ziel = BodyTarget des Gegners (sonst aktuelle Flugrichtung * 1024)
//   Richtung = normalize(Ziel - origin), z-Anteil je Skill *1.5 / *1.25 / *1.0
//   neue Richtung = normalize(normalize(velocity) * 128 + Richtung * Gewicht)
//   Gewicht = 8 (leicht) / 12 (mittel) / 17 (schwer)
//   Geschwindigkeit = neue Richtung * (gewachsene) Geschwindigkeit
void CEndBossKataball::BallThink()
{
	if (gpGlobals->time >= m_flExpireTime)
	{
		SetThink(&CEndBossKataball::SUB_Remove);
		pev->nextthink = gpGlobals->time;
		return;
	}

	// Sprite-Animation wie Retail: 9 Bilder bei 10 Hz.
	pev->frame = (float)(((int)pev->frame + 1) % 9);

	float flSpeed = pev->velocity.Length();
	float flMaxSpeed, flAccelStep, flSteerWeight, flVertical;
	if (g_iSkillLevel == SKILL_HARD)
	{
		flMaxSpeed = 1000.0f;
		flAccelStep = 10.0f;
		flSteerWeight = 17.0f;
		flVertical = 1.0f;
	}
	else if (g_iSkillLevel == SKILL_MEDIUM)
	{
		flMaxSpeed = 750.0f;
		flAccelStep = 7.0f;
		flSteerWeight = 12.0f;
		flVertical = 1.25f;
	}
	else
	{
		flMaxSpeed = 500.0f;
		flAccelStep = 5.0f;
		flSteerWeight = 8.0f;
		flVertical = 1.5f;
	}

	if (flSpeed <= 0.0f)
	{
		pev->nextthink = gpGlobals->time + 0.1f;
		return;
	}

	if (flSpeed < flMaxSpeed)
	{
		flSpeed += flAccelStep;
		if (flSpeed > flMaxSpeed)
			flSpeed = flMaxSpeed;
	}

	// Zielrichtung: Gegner-BodyTarget, sonst geradeaus weiter.
	Vector vecTargetDir = pev->velocity.Normalize();
	if (m_hEnemy != nullptr)
	{
		Vector vecToTarget = m_hEnemy->BodyTarget(pev->origin) - pev->origin;
		if (vecToTarget.Length() > 0.0f)
			vecTargetDir = vecToTarget.Normalize();
	}

	vecTargetDir.z *= flVertical;

	Vector vecNewDir = (pev->velocity.Normalize() * 128.0f + vecTargetDir * flSteerWeight);
	if (vecNewDir.Length() > 0.0f)
		vecNewDir = vecNewDir.Normalize();
	else
		vecNewDir = pev->velocity.Normalize();

	pev->velocity = vecNewDir * flSpeed;

	if ((int)pev->frame == 0)
		ALERT(at_aiconsole, "[kata] t=%.1f speed=%.0f dist=%.0f dir=(%.2f %.2f %.2f)\n",
			gpGlobals->time, flSpeed,
			m_hEnemy ? (m_hEnemy->pev->origin - pev->origin).Length() : -1.0f,
			vecNewDir.x, vecNewDir.y, vecNewDir.z);

	pev->nextthink = gpGlobals->time + 0.1f;
}

void CEndBossKataball::Precache()
{
	PrecacheModel("sprites/kata.spr");
	PrecacheModel("sprites/firebeam.spr");
	PrecacheModel("sprites/flame.spr");
	PrecacheModel("sprites/kataimpact.spr");
	PrecacheSound("endboss/imp1_gs.wav");
	PrecacheSound("endboss/imp2_gs.wav");
	PrecacheSound("endboss/imp3_gs.wav");
	// Retail legt den Rueckgabewert in +0x27c ab (0x100d290c); ohne das Feld
	// ist keine Wiedergabe moeglich.
	m_usKataImpact = (unsigned short)PrecacheEvent(1, "events/kataimpact.sc");
}

// Retail 0x100d2aa0 (CEndBossKataball::KataTouch), nach `rea function` und
// vollstaendigem Decompilat:
//   1) lebt das getroffene Objekt -> Sound "katahitplayer", Explode, KEIN Event
//   2) Klassenabgleich vehicle_tank_barrel / vehicle_tank_turret / vehicle_tank
//      -> katahitwithvehicle bzw. katahitwithoutvehicle (Schleife ueber alle
//      vehicle_tank mit gleichem Besitzerfeld +0x274)
//   3) monster_gunner_friendly -> Sound "katahitgunner", Explode, KEIN Event
//   4) sonst: PlaybackEvent(0, edict, +0x27c, 0, g_vecZero, g_vecZero, 0,0,0,0,0)
//      danach Zufallssound endboss/imp1..3_gs.wav und Ausblenden (SUB_Remove,
//      nextthink +0.2, EF_NODRAW).
// Der Event laeuft also nur im Zweig "Getroffenes lebt nicht".
void CEndBossKataball::KataTouch(CBaseEntity* pOther)
{
	const char* impacts[] = {"endboss/imp1_gs.wav", "endboss/imp2_gs.wav", "endboss/imp3_gs.wav"};

	// `katahitplayer` usw. sind KEINE Sounds, sondern Targetnames: Retail ruft
	// FUN_100531f0 (Dispatcher, identisch zu FireTargets in subs.cpp) und feuert
	// die so benannten Map-Entitaeten mit Use(activator=caller=Kataball,
	// USE_TOGGLE, 0). Alle vier Namen existieren in `end1.bsp`.
	const char* pszClass = (pOther && pOther->pev->classname) ? STRING(pOther->pev->classname) : nullptr;

	if (pOther && pOther->IsAlive())
	{
		ALERT(at_aiconsole, "[kata] Treffer auf %s (frame=%.0f speed=%.0f)\n",
			pszClass ? pszClass : "?", pev->frame, pev->velocity.Length());
		// Zweig 1: lebendes Ziel -> katahitplayer feuern, dann explodieren.
		FireTargets("katahitplayer", this, this, USE_TOGGLE, 0);
		CGrenade::ExplodeTouch(pOther);
		return;
	}

	// Zweig 2: Fahrzeugteile. Retail waehlt je Fahrzeug zwischen
	// katahitwithvehicle / katahitwithoutvehicle anhand der Felder +0x270/+0x274
	// (Besitzer/Verkettung), die im Port nicht modelliert sind -> hier nur die
	// sichere Variante "mit Fahrzeug".
	if (pszClass && (FStrEq(pszClass, "vehicle_tank_barrel") ||
			FStrEq(pszClass, "vehicle_tank_turret") ||
			FStrEq(pszClass, "vehicle_tank_body") ||
			FStrEq(pszClass, "vehicle_tank")))
	{
		FireTargets("katahitwithvehicle", this, this, USE_TOGGLE, 0);
	}

	// Zweig 3: verbuendeter Gunner -> katahitgunner feuern, dann explodieren.
	if (pszClass && FStrEq(pszClass, "monster_gunner_friendly"))
	{
		FireTargets("katahitgunner", this, this, USE_TOGGLE, 0);
		CGrenade::ExplodeTouch(pOther);
		return;
	}

	// Zweig 4: Aufprall auf ein nicht lebendes Ziel.
	PLAYBACK_EVENT_FULL(0, edict(), m_usKataImpact, 0, g_vecZero, g_vecZero,
		0.0f, 0.0f, 0, 0, 0, 0);
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, impacts[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM);
	CGrenade::ExplodeTouch(pOther);
}

//=========================================================
// endboss_gib - CEndBossGib. A tossed death-gib reusing the boss's
// own model, see file header.
//=========================================================
class CEndBossGib : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT FlopAnim();
};
LINK_ENTITY_TO_CLASS(endboss_gib, CEndBossGib);

void CEndBossGib::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("endboss_gib");
	SET_MODEL(ENT(pev), "models/endboss.mdl");

	// Retail CEndBossGib::Spawn @ 0x100d46c0 is a short, animated
	// bounce fragment; its movement is supplied by the boss death event.
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->takedamage = DAMAGE_NO;
	SetSequenceByName("death");
	SetBodygroup(1, 2);
	SetThink(&CEndBossGib::FlopAnim);
	pev->nextthink = gpGlobals->time;
}

void CEndBossGib::Precache()
{
	PrecacheModel("models/endboss.mdl");
}

void CEndBossGib::FlopAnim()
{
	// Retail CEndBossGib::FlopAnim @ 0x100d4600 advances the death
	// sequence every 0.1 seconds, then freezes the completed fragment.
	StudioFrameAdvance();
	if (m_fSequenceFinished)
	{
		SetThink(NULL);
		pev->framerate = 0;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
}

//=========================================================
// antirocketflare - invisible, short-lived countermeasure decoy.
// Retail Spawn @0x100d4540, Precache @0x100d4520. EndBoss RunAI
// creates five when a nearby vehicle_tank_rocket targets it. The rocket's
// enemy EHANDLE is retargeted to each flare in turn. Attachment and launch
// vector calculations are ported from the Retail slot-62 disassembly.
//=========================================================
class CAntiRocketFlare : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(antirocketflare, CAntiRocketFlare);

void CAntiRocketFlare::Precache()
{
	PrecacheModel("models/null.mdl");
}

void CAntiRocketFlare::Spawn()
{
	pev->classname = MAKE_STRING("antirocketflare");
	SET_MODEL(ENT(pev), "models/null.mdl");
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->effects = EF_NODRAW;
	pev->framerate = 1.0f;
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 3.0f;
}

//=========================================================
// monster_endboss - CEndBoss.
//=========================================================
class CEndBoss : public CBaseMonster
{
public:
	int MonsterProfile() override { return 22; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int Classify() override { return CLASS_ALIEN_BOSS; }
	// Mechanischer Endboss: Metallgibs statt Stock-Fleischgibs. Die Klasse precacht
	// models/metalplategibs_green.mdl bereits.
	const char* CustomGibModel() override { return "models/metalplategibs_green.mdl"; }
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this - see the
	// feedback-setactivity-act-idle-stomps-scripted-sequence memory
	// note (same fix as CFriendlyGunman etc).
	// Retail CEndBoss::SetActivity (vtable slot 96 @0x100d6430), disassembled
	// 2026-11-01 (findings/entities/endboss_slot96_2026-11-01.md):
	//  - ACT_RUN is looked up as ACT_WALK (the endboss model has no run
	//    sequence to use for it), Retail @0x100d6440.
	//  - pev->frame is reset when the sequence changes or the sequence does
	//    not loop; there is no walk/run exemption (the SDK base has one,
	//    but its oldActivity is set from NewActivity, so it is effectively
	//    inverted - see monsters.cpp CBaseMonster::SetActivity).
	//  - Missing sequence: ALERT and sequence 0, as in the base class.
	// The SDK fix for scripted_sequence (m_pCine) is kept as before; it is
	// not in the retail function.
	void SetActivity(Activity NewActivity) override
	{
		if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && m_pCine != nullptr)
		{
			m_Activity = NewActivity;
			m_IdealActivity = NewActivity;
			return;
		}
		m_Activity = NewActivity;
		m_IdealActivity = NewActivity;

		const int iSequence = LookupActivity(NewActivity == ACT_RUN ? ACT_WALK : NewActivity);
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
			ALERT(at_aiconsole, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity);
			pev->sequence = 0;
		}
	}
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	// Retail slots 80/81/82/83 (@0x100d5e80/@0x100d5ee0/@0x100d5f80/@0x100d5f30), 87 (@0x100d5510),
	// 88 (@0x100d53d0) and 101 (@0x100d4920).
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckRangeAttack2(float flDot, float flDist) override;
	bool CheckMeleeAttack1(float flDot, float flDist) override;
	bool CheckMeleeAttack2(float flDot, float flDist) override;
	// Retail vtable slot 62: countermeasure flares for rockets targeting this boss.
	void RunAI() override;
	Schedule_t* GetScheduleOfType(int Type) override;
	Schedule_t* GetSchedule() override;
	void SetYawSpeed() override;

private:
	float m_flNextRocket = 0; // Retail +0x270, m_flNextRocketAttack
	float m_flNextKata = 0; // Retail +0x274, m_fNextKataAttack
	float m_flNextMachinegun = 0; // Retail +0x278, m_fNextMachinegunAttack
	float m_flLostKatagunTime = 0; // Retail +0x27c
	unsigned short m_usTankMGuns = 0; // Retail +0x298, events/tankmguns.sc
	float m_flNextSpeak = 0; // Retail +0x280
	bool m_bHasKatagun = true; // Retail +0x284
	int m_iKataFiring = 0; // Retail +0x28c
	int m_iEnemyHitMe = 0; // Retail +0x288
public:
	// Testhilfen fuer `dbg_armswipe` / `dbg_activate` (Client-Debugbefehle).
	void DebugArmSwipe() { ArmSwipe(); }
	void DebugStomp() { Stomp(); }
	void DebugKata() { LaunchKataball(); }
	void DebugInfo();
	void DebugSetEnemy(CBaseEntity* pEnemy) { m_hEnemy = pEnemy; }
	void DebugActivate(CBaseEntity* pEnemy);
private:
	void LaunchRocket();
	void LaunchKataball();
	void ArmSwipe();
	void Stomp();
};
LINK_ENTITY_TO_CLASS(monster_endboss, CEndBoss);

// Testhilfe fuer den Client-Debugbefehl `dbg_armswipe` (siehe client.cpp):
// setzt den Spieler als Gegner und loest den Arm-Schlag aus.
// Testhilfe fuer `dbg_activate`: holt den Boss aus der Warte-Sequenz in den
// Kampfzustand und setzt alle Angriffs-Cooldowns auf jetzt.
void CEndBoss::DebugActivate(CBaseEntity* pEnemy)
{
	m_hEnemy = pEnemy;

	// WICHTIG: Die Map-Sequenz (`changeboss` auf end1) haelt den Boss in
	// MONSTERSTATE_SCRIPT (6); in diesem Zustand waehlt er nie einen
	// Angriffsschedule. Sequenz daher abbrechen, sonst bleibt dbg_activate
	// wirkungslos (Messung 2026-11-07: 1 Kata in 50 s).
	if (m_pCine)
	{
		m_pCine->CancelScript();
		m_pCine = NULL;
	}

	m_IdealMonsterState = MONSTERSTATE_COMBAT;
	SetState(MONSTERSTATE_COMBAT);
	SetConditions(bits_COND_SEE_ENEMY | bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE);
	m_flNextKata = m_flNextRocket = m_flNextMachinegun = gpGlobals->time;
	m_flNextSpeak = gpGlobals->time;
	ALERT(at_aiconsole, "[boss] aktiviert: state=%d enemy=%s origin=(%.0f %.0f %.0f)\n",
		(int)m_MonsterState, (m_hEnemy && m_hEnemy->pev) ? STRING(m_hEnemy->pev->classname) : "none",
		pev->origin.x, pev->origin.y, pev->origin.z);
}

// Testhilfe fuer `dbg_activate` (Client-Debugbefehl): Boss in den Kampf holen.
void DbgEndBossActivate(CBaseEntity* pBossEnt, CBaseEntity* pPlayer)
{
	static_cast<CEndBoss*>(pBossEnt)->DebugActivate(pPlayer);
}

// Testhilfe fuer `dbg_bossinfo`: Abstand und Sichtlinie zum aktuellen Gegner.
void CEndBoss::DebugInfo()
{
	CBaseEntity* pEnemy = m_hEnemy;
	float flDist = pEnemy ? (pEnemy->pev->origin - pev->origin).Length() : -1.0f;
	int iVisible = (pEnemy && FVisible(pEnemy)) ? 1 : 0;
	ALERT(at_aiconsole, "[bossinfo] state=%d dist=%.0f visible=%d boss=(%.0f %.0f %.0f) player=(%.0f %.0f %.0f)\n",
		(int)m_MonsterState, flDist, iVisible, pev->origin.x, pev->origin.y, pev->origin.z,
		pEnemy ? pEnemy->pev->origin.x : 0.0f, pEnemy ? pEnemy->pev->origin.y : 0.0f,
		pEnemy ? pEnemy->pev->origin.z : 0.0f);
}

// Testhilfe fuer `dbg_kata`.
void DbgEndBossKata(CBaseEntity* pBossEnt, CBaseEntity* pPlayer)
{
	CEndBoss* pBoss = static_cast<CEndBoss*>(pBossEnt);
	pBoss->DebugSetEnemy(pPlayer);
	pBoss->DebugKata();
}

// Testhilfe fuer `dbg_bossinfo`.
void DbgEndBossInfo(CBaseEntity* pBossEnt, CBaseEntity* pPlayer)
{
	CEndBoss* pBoss = static_cast<CEndBoss*>(pBossEnt);
	pBoss->DebugSetEnemy(pPlayer);
	pBoss->DebugInfo();
}

// Testhilfe fuer `dbg_stomp`.
void DbgEndBossStomp(CBaseEntity* pBossEnt, CBaseEntity* pPlayer)
{
	CEndBoss* pBoss = static_cast<CEndBoss*>(pBossEnt);
	pBoss->DebugSetEnemy(pPlayer);
	pBoss->DebugStomp();
}

void DbgEndBossArmSwipe(CBaseEntity* pBossEnt, CBaseEntity* pPlayer)
{
	CEndBoss* pBoss = static_cast<CEndBoss*>(pBossEnt);
	pBoss->DebugSetEnemy(pPlayer);
	pBoss->DebugArmSwipe();
}

TYPEDESCRIPTION CEndBoss::m_SaveData[] =
{
	DEFINE_FIELD(CEndBoss, m_flNextSpeak, FIELD_TIME),
	DEFINE_FIELD(CEndBoss, m_flNextKata, FIELD_TIME),
	DEFINE_FIELD(CEndBoss, m_flNextRocket, FIELD_TIME),
	DEFINE_FIELD(CEndBoss, m_flNextMachinegun, FIELD_TIME),
	DEFINE_FIELD(CEndBoss, m_flLostKatagunTime, FIELD_TIME),
	DEFINE_FIELD(CEndBoss, m_bHasKatagun, FIELD_BOOLEAN),
	DEFINE_FIELD(CEndBoss, m_iKataFiring, FIELD_INTEGER),
	DEFINE_FIELD(CEndBoss, m_iEnemyHitMe, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CEndBoss, CBaseMonster);

void CEndBoss::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/endboss.mdl");
	UTIL_SetSize(pev, Vector(-48, -48, 0), Vector(48, 48, 300));

	pev->solid = SOLID_BBOX;
	// Blutfarbe 156 kommt aus der Profiltabelle (MonsterProfile() = 22); Retail setzt +0x238 im Spawn nicht.
	pev->health = 200; // confirmed fixed value, see file header - no skill scaling found
	pev->view_ofs = Vector(0, 0, 300);

	// Retail's capability word (+0x220) is not mappable; the retail attack checks (CheckAttacks, slot 98)
	// run range1/range2 under their capability bits and melee1/melee2 unconditionally, so all four are set.
	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK1 | bits_CAP_RANGE_ATTACK2 | bits_CAP_MELEE_ATTACK1 | bits_CAP_MELEE_ATTACK2;

	MonsterInit();

	// endboss.mdl has no $eyeposition. Retail Spawn @ 0x100d64d0 restores
	// its authored 300-unit eye height after the model-derived init step.
	pev->view_ofs = Vector(0, 0, 300);
	// Retail Spawn @0x100d64d0 setzt nach MonsterInit das Sichtfeld auf -1.0
	// (Rundumsicht, 0xbf800000). Das Retail-Basisfeld +0x244 entspricht
	// m_flDistTooFar (Gunner-Abgleich, findings/entities/gunner_friendly_2026-10-31.md).
	m_flFieldOfView = -1.0f;
	m_flNextKata = m_flNextRocket = m_flNextMachinegun = m_flNextSpeak = gpGlobals->time;
	m_bHasKatagun = true;
	m_flDistTooFar = 4096.0f; // Retail Spawn setzt das gemeinsame Basisfeld +0x244.
}

// Retail custom schedule type 0x2e, table @0x1012f9b0 "Special Machinegun Attack".
#define SCHED_ENDBOSS_SPECIAL 0x2e

static Task_t tlEndBossSpecial[] =
{
	{TASK_STOP_MOVING, 0},
	{TASK_FACE_ENEMY, 0},
	{TASK_PLAY_SEQUENCE, (float)ACT_SPECIAL_ATTACK2},
	{TASK_FACE_ENEMY, 0},
};

static Schedule_t slEndBossSpecial[] =
{
	{tlEndBossSpecial, ARRAYSIZE(tlEndBossSpecial), 0, 0, "Special Machinegun Attack"},
};

// Debug: Angriffspruefungen protokollieren (nur bei Ergebniswechsel, max. alle
// 0.5 s). Zeigt, ob Cooldowns oder Distanz/Dot blockieren.
static void DbgAttackCheck(const char* pszName, int iIndex, bool bResult, float flDot, float flDist)
{
	static bool s_bLast[4] = { false, false, false, false };
	static float s_flLastLog[4] = { 0, 0, 0, 0 };

	if (s_bLast[iIndex] == bResult && gpGlobals->time - s_flLastLog[iIndex] < 2.0f)
		return;
	if (gpGlobals->time - s_flLastLog[iIndex] < 0.5f)
		return;

	s_bLast[iIndex] = bResult;
	s_flLastLog[iIndex] = gpGlobals->time;
	ALERT(at_aiconsole, "[atk] %s res=%d dot=%.2f dist=%.0f\n", pszName, bResult ? 1 : 0, flDot, flDist);
}

bool CEndBoss::CheckRangeAttack1(float flDot, float flDist)
{
	// Katagun: Retail +0x274 cooldown and +0x284 ownership flag.
	bool bResult = (gpGlobals->time >= m_flNextKata && m_bHasKatagun);
	if (bResult)
		m_flNextKata = gpGlobals->time + RANDOM_FLOAT(2, 5);

	DbgAttackCheck("range1(kata)", 0, bResult, flDot, flDist);
	return bResult;
}

bool CEndBoss::CheckRangeAttack2(float flDot, float flDist)
{
	// Rockets: Retail +0x270 cooldown, 5-8 s.
	bool bResult = (gpGlobals->time >= m_flNextRocket);
	if (bResult)
		m_flNextRocket = gpGlobals->time + RANDOM_FLOAT(5, 8);

	DbgAttackCheck("range2(rocket)", 1, bResult, flDot, flDist);
	return bResult;
}

bool CEndBoss::CheckMeleeAttack1(float flDot, float flDist)
{
	bool bResult = (flDot > 0.7f && flDist < 256.0f);
	DbgAttackCheck("melee1(stomp)", 2, bResult, flDot, flDist);
	return bResult;
}

bool CEndBoss::CheckMeleeAttack2(float flDot, float flDist)
{
	// Special machinegun attack: pure cooldown, 5-8 s.
	bool bResult = (gpGlobals->time >= m_flNextMachinegun);
	if (bResult)
		m_flNextMachinegun = gpGlobals->time + RANDOM_FLOAT(5, 8);

	DbgAttackCheck("melee2(mg)", 3, bResult, flDot, flDist);
	return bResult;
}

Schedule_t* CEndBoss::GetScheduleOfType(int Type)
{
	if (Type == SCHED_ENDBOSS_SPECIAL)
		return &slEndBossSpecial[0];
	return CBaseMonster::GetScheduleOfType(Type);
}

Schedule_t* CEndBoss::GetSchedule()
{
	// Retail @0x100d53d0. Outside combat, and for the dead-enemy handling, the stock logic applies.
	if (m_MonsterState != MONSTERSTATE_COMBAT || HasConditions(bits_COND_ENEMY_DEAD))
		return CBaseMonster::GetSchedule();

	if (HasConditions(bits_COND_NEW_ENEMY))
		return GetScheduleOfType(SCHED_WAKE_ANGRY);

	if (!HasConditions(bits_COND_SEE_ENEMY))
	{
		if (!HasConditions(bits_COND_ENEMY_OCCLUDED))
			return GetScheduleOfType(SCHED_COMBAT_FACE);
	}
	else
	{
		if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_AUTO, "endboss/end_stomp.wav", 1.0, ATTN_NONE, 0, 100);
			return GetScheduleOfType(SCHED_MELEE_ATTACK1);
		}
		if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
			return GetScheduleOfType(SCHED_RANGE_ATTACK1);
		if (HasConditions(bits_COND_CAN_RANGE_ATTACK2))
			return GetScheduleOfType(SCHED_RANGE_ATTACK2);
		if (HasConditions(bits_COND_CAN_MELEE_ATTACK2))
			return GetScheduleOfType(SCHED_ENDBOSS_SPECIAL);
	}
	return GetScheduleOfType(SCHED_CHASE_ENEMY);
}

void CEndBoss::SetYawSpeed()
{
	switch (m_Activity)
	{
	case ACT_IDLE:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		pev->yaw_speed = 40;
		break;
	case ACT_WALK:
	case ACT_RUN:
		pev->yaw_speed = 60;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
		pev->yaw_speed = 50;
		break;
	case ACT_MELEE_ATTACK2:
		pev->yaw_speed = 70;
		break;
	default:
		pev->yaw_speed = 45;
		break;
	}
}

void CEndBoss::RunAI()
{
	CBaseMonster::RunAI();

	for (CBaseEntity* pEntity = UTIL_FindEntityInSphere(nullptr, pev->origin, 800.0f);
		pEntity;
		pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, 800.0f))
	{
		if (!FClassnameIs(pEntity->pev, "vehicle_tank_rocket"))
			continue;

		// Retail treats vehicle_tank_rocket as CGrenade and checks its EHANDLE.
		CGrenade* pRocket = static_cast<CGrenade*>(pEntity);
		if (pRocket->m_hEnemy != this)
			continue;

		Vector vecAttachmentOrigin;
		Vector vecAttachmentAngles;
		GetAttachment(3, vecAttachmentOrigin, vecAttachmentAngles);

		for (int i = 0; i < 5; ++i)
		{
			edict_t* pentFlare = CREATE_NAMED_ENTITY(MAKE_STRING("antirocketflare"));
			if (FNullEnt(pentFlare))
				continue;

			CBaseEntity* pFlare = CBaseEntity::Instance(pentFlare);
			if (!pFlare)
			{
				REMOVE_ENTITY(pentFlare);
				continue;
			}

			// Retail relinks the entity at attachment 3 + v_forward before Spawn.
			UTIL_SetOrigin(pFlare->pev, vecAttachmentOrigin + gpGlobals->v_forward);
			pFlare->Spawn();

			// Retail assigns owner/angles and launch velocity after Spawn.
			pFlare->pev->owner = pev->pContainingEntity;
			pFlare->pev->angles = pev->angles;
			const int iRightOffset = RANDOM_LONG(-128, 128);
			const int iUpOffset = RANDOM_LONG(-128, 128);
			pFlare->pev->velocity = gpGlobals->v_forward * 512.0f +
				gpGlobals->v_right * iRightOffset + gpGlobals->v_up * iUpOffset;
			pRocket->m_hEnemy = pFlare;
		}
	}
}

void CEndBoss::Precache()
{
	PrecacheModel("models/endboss.mdl");
	PrecacheModel("sprites/fexplo.spr");
	PrecacheModel("models/metalplategibs_green.mdl");
	PrecacheSound("endboss/end_stomp.wav");
	PrecacheSound("endboss/end_stomp2.wav");
	for (int i = 1; i <= 8; i++)
	{
		char snd[32];
		sprintf(snd, "endboss/endboss_stomp%d.wav", i);
		PrecacheSound(snd);
	}
	// Retail +0x298: der Rueckgabewert wird gespeichert (nicht verworfen) und
	// in den Animationsereignissen 6/7 (Arm-Schlag, 0x100d4e07/0x100d50ea)
	// abgespielt.
	m_usTankMGuns = (unsigned short)PrecacheEvent(1, "events/tankmguns.sc");

	UTIL_PrecacheOther("endboss_kataball");
	UTIL_PrecacheOther("endboss_rocket");
	UTIL_PrecacheOther("endboss_gib");
	UTIL_PrecacheOther("sphere_explosion");
	UTIL_PrecacheOther("antirocketflare");
}

void CEndBoss::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case ENDBOSS_AE_ROCKET:
		LaunchRocket();
		break;
	case ENDBOSS_AE_KATABALL:
		LaunchKataball();
		break;
	case ENDBOSS_AE_ARMSWIPE_L:
	case ENDBOSS_AE_ARMSWIPE_R:
		ArmSwipe();
		break;
	case ENDBOSS_AE_STOMP:
		Stomp();
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CEndBoss::LaunchRocket()
{
	Vector vecOrigin, vecAngles;
	// Retail event 3 (helper 0x100d5bd0, mode 2): attachment 2, no z offset.
	GetAttachment(2, vecOrigin, vecAngles);

	CBaseEntity* pRocket = CBaseEntity::Create("endboss_rocket", vecOrigin, pev->angles);
	if (pRocket)
	{
		pRocket->pev->owner = edict();
		// Retail event 3 copies the boss's current enemy handle to the new
		// projectile before Spawn().  CBaseMonster persists that EHANDLE.
		static_cast<CEndBossRocket*>(pRocket)->SetTarget(m_hEnemy);
	}

}

void CEndBoss::LaunchKataball()
{
	Vector vecOrigin, vecAngles;
	// Retail event 4 (helper 0x100d5bd0, mode 3): attachment 3, no z offset.
	GetAttachment(3, vecOrigin, vecAngles);

	// Retail CEndBossKataball::Spawn (0x100d2930) setzt die Geschwindigkeit aus
	// gpGlobals->v_forward * 400 — also aus der **Blickrichtung, die beim
	// Erzeugen gesetzt war**, nicht aus den Koerperwinkeln des Bosses. Der
	// Aufrufer muss daher auf den Gegner zielen.
	Vector vecAimAngles = vecAngles;
	if (m_hEnemy)
	{
		Vector vecDir = m_hEnemy->BodyTarget(vecOrigin) - vecOrigin;
		if (vecDir.Length() > 0.0f)
		{
			vecAimAngles = UTIL_VecToAngles(vecDir.Normalize());

			// WICHTIG: UTIL_VecToAngles geht ueber die Engine (VEC_TO_ANGLES =
			// pfnVecToAngles) und liefert den Pitch mit umgekehrtem Vorzeichen
			// (Blick nach oben = positiv), waehrend UTIL_MakeVectors/AngleVectors
			// im Spiel negativen Pitch als "nach oben" interpretiert. Ohne diese
			// Korrektur fliegt die Kata-Kugel spiegelverkehrt (nach oben statt
			// zum Spieler). Gleiche Korrektur wie in client.cpp (dbg_focus).
			vecAimAngles.x = -vecAimAngles.x;
			if (vecAimAngles.x > 180.0f)
				vecAimAngles.x -= 360.0f;
			else if (vecAimAngles.x < -180.0f)
				vecAimAngles.x += 360.0f;
		}
	}

	CBaseEntity* pBall = CBaseEntity::Create("endboss_kataball", vecOrigin, vecAimAngles);
	if (pBall)
	{
		pBall->pev->owner = edict();
		// Retail event 4 uses the same target propagation as the rocket.
		static_cast<CEndBossKataball*>(pBall)->SetTarget(m_hEnemy);

		ALERT(at_aiconsole, "[kata] origin=(%.0f %.0f %.0f) angles=(%.1f %.1f %.1f) vel=(%.0f %.0f %.0f) enemy=(%.0f %.0f %.0f)\n",
			vecOrigin.x, vecOrigin.y, vecOrigin.z,
			pBall->pev->angles.x, pBall->pev->angles.y, pBall->pev->angles.z,
			pBall->pev->velocity.x, pBall->pev->velocity.y, pBall->pev->velocity.z,
			m_hEnemy ? m_hEnemy->pev->origin.x : 0.0f,
			m_hEnemy ? m_hEnemy->pev->origin.y : 0.0f,
			m_hEnemy ? m_hEnemy->pev->origin.z : 0.0f);
	}

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "endboss/kata_cannon1.wav", 1.0, ATTN_NORM);

}

//=========================================================
// ArmSwipe (Retail-Events 6 und 7, Helfer 0x100d5bd0 Modus 1, Pellet-Schuss 0x1000f090).
// Belegt (Dekompilat 2026-11-01, findings/entities/endboss_armswipe_2026-11-01.md):
//  - Ohne Gegner kein Schlag.
//  - Ursprung: Attachment 1 mit z +8 (Helfer Modus 1).
//  - Richtung: Retail FUN_1002d200 (BodyTarget/Enemy-LKP; Player mit m_hTargetEnt erhält Zufallsversatz).
//  - Drei Pellets, Reichweite 4096 (Aufrufargumente 3 und 4096.0).
//  - Streuung: dynamischer Retail-Vektor aus Attachment-Achse und Aim-z; Einzelheiten
//    stehen in findings/entities/endboss_armswipe_2026-11-01.md.
//  - Schaden je Treffer (DMG_BULLET): Cvar sk_9mmAR_bullet<Skill> (Retail-Name ohne Stufe, Helfer haengt die Stufe an; skill.cfg: 10).
// VEREINFACHUNGEN / OFFEN:
//  - Keine statische Streuungsnäherung mehr: der belegte Retail-Vektor wird berechnet.
//  - Retail: Der Schaden geht an das Trefferziel (Vtable-Slot 0x28 = TraceAttack, Schadenstyp 2 = DMG_BULLET).
//    Der Zaehler 0x1013228c (Modulo 3) steuert nur den Effekt und den Lebend-Check, nicht den Schaden.
//  - Effekt: TE_TRACER (6) von Start nach Trefferpunkt, MSG_PAS, jeder dritte Treffer (Retail-Zaehler).
//    Retail prueft vor dem Effekt pev->flags (Offset 0x1a4) & FL_CLIENT (0x8) des Angreifers; der Angreifer ist hier der Endboss selbst (kein Client), die Pruefung wuerde nie greifen.
//  - Retail-Zweig "Abstand unter Schwelle" (0x100e9384) nicht nachgebaut.
//  - Die fruehere 150-Einheiten-Pruefung entfaellt: Retail hat keine feste Distanzpruefung, die Reichweite ist 4096.
//=========================================================
// Zaehler wie Retail-Global 0x1013228c (nur Effekt-Gating), gemeinsam fuer alle Schlaege.
static int s_iArmSwipeCounter = 0;

// Abweichung vom Retail-Original (Nutzerwunsch, 2026-11-07): Der Nahkampf soll
// den Spieler ein gutes Stueck zurueckschleudern. Die Decompilate der
// Animationsereignisse 6/7 (0x100d4b60 / 0x100d4e34) schreiben keine
// Geschwindigkeit, der Rueckstoss ist also bewusst zusaetzlich eingebaut.
// Nur Nahkampf: wirkt bis 256 Einheiten, Kraft faellt mit dem Abstand.
static void ApplyArmSwipeKnockback(CBaseEntity* pTarget, const Vector& vecFrom, const Vector& vecDir)
{
	if (!pTarget || !pTarget->IsPlayer())
		return;

	Vector vecPush = pTarget->pev->origin - vecFrom;
	vecPush.z = 0.0f;
	float flDist = vecPush.Length();
	if (flDist < 1.0f || flDist > 256.0f)
		return;

	vecPush = vecPush.Normalize();

	// Basisrichtung: vom Boss weg (nicht die Schussrichtung, damit der Spieler
	// auch bei Streifschuessen nach hinten fliegt). 700 -> 350 ueber 256 Einheiten.
	const float flScale = 700.0f - (flDist / 256.0f) * 350.0f;
	pTarget->pev->velocity = vecPush * flScale + Vector(0, 0, 260.0f);
	pTarget->pev->flags &= ~FL_ONGROUND;
	pTarget->pev->punchangle.x = -8.0f;

	ALERT(at_aiconsole, "[boss] knockback dist=%.0f vel=%.0f %.0f %.0f\n",
		flDist, pTarget->pev->velocity.x, pTarget->pev->velocity.y, pTarget->pev->velocity.z);
}

void CEndBoss::ArmSwipe()
{
	if (!m_hEnemy)
		return;

	// Retail: GET_ATTACHMENT(1), Helfer Modus 1 (z +8).
	Vector vecOrigin, vecAngles;
	GetAttachment(1, vecOrigin, vecAngles);
	Vector vecAttachment0;
	GetAttachment(0, vecAttachment0, vecAngles);
	const Vector vecArmAxis = vecOrigin - vecAttachment0;
	vecOrigin.z += 8;

	// Retail 0x100d4e07 (Event 6) und 0x100d50ea (Event 7): PlaybackEvent mit
	// Flags 2 am Arm-Ursprung, fparam1 = 1.0, danach Kampfschall
	// (InsertSound, Radius 2048, 0.1 s).
	PLAYBACK_EVENT_FULL(2, edict(), m_usTankMGuns, 0.0f, vecOrigin, vecAngles,
		1.0f, 0.0f, 0, 0, 0, 0);
	CSoundEnt::InsertSound(bits_SOUND_COMBAT, vecOrigin, 2048, 0.1f);

	Vector vecAim;
	CBaseEntity* pEnemy = m_hEnemy;
	CBaseMonster* pEnemyMonster = (pEnemy && !pEnemy->IsPlayer()) ? pEnemy->MyMonsterPointer() : nullptr;
	// Retail FUN_1002d200 hat zwei Zweige: BodyTarget plus begrenzter
	// Zufallsversatz, wenn der Gegner ein gueltiges Ziel-Handle hat, sonst den
	// LKP-Fallback (ShootAtEnemy). Das Ziel-Handle eines Spielers ist im SDK
	// nicht modelliert (m_hTargetEnt gibt es nur bei Monstern), deshalb bleibt
	// es beim Retail-Zweig fuer Monster und beim LKP-Pfad fuer Spieler.
	// Messung 2026-11-07: der BodyTarget-Zweig fuer Spieler traf SCHLECHTER
	// (0 von 9 Pellets) als der LKP-Pfad (1 von 9) — der Zufallsversatz von
	// bis zu 64 Einheiten nach oben lenkt bei ~250 Einheiten Abstand vorbei.
	if (pEnemy && pEnemy->IsPlayer() && pEnemyMonster && pEnemyMonster->m_hTargetEnt)
	{
		vecAim = pEnemy->BodyTarget(vecOrigin) - vecOrigin;
		vecAim.x += RANDOM_LONG(-32, 32);
		vecAim.y += RANDOM_LONG(-32, 32);
		vecAim.z += RANDOM_LONG(0, 64);
		vecAim = vecAim.Normalize();
	}
	else
		vecAim = ShootAtEnemy(vecOrigin); // Retail fallback: BodyTarget offset + m_vecEnemyLKP.
	// Retail events 6/7: spread vector starts as Attachment(1)-Attachment(0).
	// Its z component is clamped around the arm axis by 10% of that vector's length.
	const float flArmSpreadRange = static_cast<float>(vecArmAxis.Length() * 0.1);
	const float flSpreadZMin = vecArmAxis.z - flArmSpreadRange;
	const float flSpreadZMax = vecArmAxis.z + flArmSpreadRange;
	float flSpreadZ = vecAim.z;
	if (flSpreadZ < flSpreadZMin)
		flSpreadZ = flSpreadZMin;
	if (flSpreadZ > flSpreadZMax)
		flSpreadZ = flSpreadZMax;
	Vector vecSpread(vecArmAxis.x, vecArmAxis.y, flSpreadZ);
	const float flSpreadLength = vecSpread.Length();
	// Retail's x87 compare tests the equality flag: exactly 12.0 selects (0,0,1);
	// all other finite lengths take the reciprocal-length normalization path.
	if (flSpreadLength == 12.0f)
		vecSpread = Vector(0, 0, 1);
	else
		vecSpread = vecSpread.Normalize();

	// The pellet helper consumes gpGlobals->v_right/up directly. Retail does not
	// recompute those basis vectors in either animation-event block.
	const Vector vecRight = gpGlobals->v_right;
	const Vector vecUp = gpGlobals->v_up;
	const float flDamage = GetSkillCvar("sk_9mmAR_bullet");

	ClearMultiDamage();
	for (int i = 0; i < 3; i++)
	{
		// Retail-Streuung (Summe zweier Zufallswerte je Achse, Kreisrejektion).
		float x, y;
		do
		{
			x = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
			y = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
		} while (x * x + y * y > 1.0f);

		const Vector vecDir = (vecAim + vecRight * (x * vecSpread.x) + vecUp * (y * vecSpread.y)).Normalize();
		const Vector vecEnd = vecOrigin + vecDir * 4096;

		TraceResult tr;
		UTIL_TraceLine(vecOrigin, vecEnd, dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction < 1.0f && tr.pHit)
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			DBG_LOG("ARMSWIPE pellet %d origin=(%.0f %.0f %.0f) end=(%.0f %.0f %.0f) frac=%.3f hit=%s dmg=%.1f",
				i, vecOrigin.x, vecOrigin.y, vecOrigin.z, tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z,
				tr.flFraction, pHit ? STRING(pHit->pev->classname) : "(none)", flDamage);
			// Retail (0x1000f3d8..f3f1): Effekt nur bei jedem dritten Treffer (globaler Zaehler).
			// Der virtuelle Aufruf bei vtable+0x9c ist CBaseEntity::IsPlayer (bei diesem
			// Endboss false), kein Lebend-Check. Beide Zweige senden TE_TRACER (6),
			// MSG_PAS (5), SVC_TEMPENTITY (23); der Zähler beeinflusst den Schaden nicht.
			// Retail-Pruefung [pev+0x1a4] & FL_CLIENT (0x8) entfaellt hier: Angreifer ist der Endboss selbst (kein Client).
			if (pHit && (s_iArmSwipeCounter++ % 3) == 0)
			{
				MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecOrigin);
				WRITE_BYTE(TE_TRACER);
				WRITE_COORD(vecOrigin.x);
				WRITE_COORD(vecOrigin.y);
				WRITE_COORD(vecOrigin.z);
				WRITE_COORD(tr.vecEndPos.x);
				WRITE_COORD(tr.vecEndPos.y);
				WRITE_COORD(tr.vecEndPos.z);
				MESSAGE_END();
			}
			if (pHit && pHit->pev->takedamage != DAMAGE_NO)
				pHit->TraceAttack(pev, flDamage, vecDir, &tr, DMG_BULLET); // Retail: Schadenstyp 2 (DMG_BULLET), siehe findings/entities/endboss_armswipe_2026-11-01.md

			// Bewusste Abweichung (Nutzerwunsch): Nahkampf schleudert den Spieler
			// zurueck, auch wenn Retail keine Geschwindigkeit setzt.
			ALERT(at_aiconsole, "[armswipe] pellet %d hit=%s dist=%.0f\n", i,
				pHit && pHit->pev->classname ? STRING(pHit->pev->classname) : "?",
				pHit ? (pHit->pev->origin - vecOrigin).Length() : -1.0f);
			ApplyArmSwipeKnockback(pHit, vecOrigin, vecDir);

			// Retail 0x1000f090, case 2: nach dem Treffer Einschlag-Dekal, aber nur
			// auf Brush-/Weltgeometrie (movetype == MOVETYPE_PUSH).
			// Kette: 0x10061d60 -> 0x10061d40(ent, 2) -> 0x1005ffa0, das ein
			// TE_DECAL (109) sendet. Das Argument 2 ist DMG_BULLET (1 << 1) und
			// 0x80 im Zweig mode 4 ist DMG_CLUB (1 << 7): 0x10061d40 ruft also
			// CBaseEntity::DamageDecal(bitsDamageType) (Vtable-Slot +0x70), das
			// je rendermode ein Dekal liefert (kRenderTransAlpha -> -1,
			// sonst DECAL_BPROOF1, normal DECAL_GUNSHOT1 + RANDOM_LONG(0, 4)).
			if (pHit && pHit->pev->movetype == MOVETYPE_PUSH)
			{
				const int iDecal = pHit->DamageDecal(DMG_BULLET);
				ALERT(at_aiconsole, "[decal] pellet %d hit %s decal=%d\n", i,
					STRING(pHit->pev->classname), iDecal);
				if (iDecal > 0)
					UTIL_DecalTrace(&tr, iDecal);
			}
		}
		else
			DBG_LOG("ARMSWIPE pellet %d origin=(%.0f %.0f %.0f) no hit", i, vecOrigin.x, vecOrigin.y, vecOrigin.z);
	}
	ApplyMultiDamage(pev, pev);
}

void CEndBoss::Stomp()
{
	float damage = 130;
	if (g_iSkillLevel == SKILL_MEDIUM)
		damage = 150;
	else if (g_iSkillLevel == SKILL_HARD)
		damage = 200;

	UTIL_MakeVectors(pev->angles);
	const Vector stompOrigin = pev->origin + gpGlobals->v_forward * 64.0f;
	::RadiusDamage(stompOrigin, pev, pev, damage, 300.0f, CLASS_NONE, DMG_CLUB);

	// Bewusste Abweichung (Nutzerwunsch, 2026-11-07): Der Nahkampf-Stomp soll
	// den Spieler deutlich zurueckschleudern. Retail setzt nur RadiusDamage +
	// ScreenShake (Decompilat Event 10, 0x100d528c..0x100d52e1).
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer || !pPlayer->IsAlive())
			continue;

		Vector vecPush = pPlayer->pev->origin - pev->origin;
		vecPush.z = 0.0f;
		float flDist = vecPush.Length();
		if (flDist < 1.0f || flDist > 300.0f)
			continue;

		vecPush = vecPush.Normalize();
		const float flScale = 800.0f - (flDist / 300.0f) * 400.0f;
		pPlayer->pev->velocity = vecPush * flScale + Vector(0, 0, 320.0f);
		pPlayer->pev->flags &= ~FL_ONGROUND;
		pPlayer->pev->punchangle.x = -12.0f;

		ALERT(at_aiconsole, "[boss] stomp knockback dist=%.0f vel=%.0f %.0f %.0f\n",
			flDist, pPlayer->pev->velocity.x, pPlayer->pev->velocity.y, pPlayer->pev->velocity.z);
	}
	UTIL_ScreenShake(stompOrigin, 12.0f, 3.0f, 0.5f, 600.0f);
	UTIL_EmitAmbientSound(ENT(pev), pev->origin, "endboss/end_stomp2.wav",
		1.0f, ATTN_NONE, 0, 100);
}
