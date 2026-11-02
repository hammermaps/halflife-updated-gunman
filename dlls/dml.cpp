//=========================================================
// weapon_dml (Alias: weapon_mule) - CDml, plus its confirmed
// projectile class dmlRocket (CDmlRocket). Fifth of the six points
// deferred during the map-driven pass (see findings/weapons/
// weapon_dml.md and [[project-weapon-dml-deferred]] memory).
//
// Decompiled fresh from gunman.dll: LINK-equivalent constructor sets
// classname "weapon_dml" always (even spawned as "weapon_mule"),
// vtable @0x100faef8, Spawn @0x10082380, Precache @0x10082440,
// GetItemInfo @0x10082720, Deploy @0x100825b0, PrimaryAttack
// @0x100829e0, its two fire subfunctions FUN_10082ba0 (dual-rocket
// burst)/FUN_10082d90 (single guided shot), the shared projectile
// spawner FUN_10082f70, the target-lock search helper FUN_10083b60,
// and Reload (vtable slot 88) @0x100835a0. Confirms
// findings/weapons/weapon_dml.md exactly: weapon ID 0x10(16), world
// model "models/w_dml.mdl", ammo type "missiles", iMaxClip=6, reuses
// "models/p_crossbow.mdl" (stock HL's crossbow playermodel - rpg.cpp/
// crossbow.cpp remain untouched, only used as structural references).
//
// CORRECTION HISTORY (2026-09-05): an earlier same-day pass wrongly
// claimed "PrimaryAttack never found" (missing a pre-existing
// cross-weapon findings doc, findings/weapons/primaryattack_
// breakthrough.md) and shipped a simplified CGrenade::ShootTimed
// stand-in. This version replaces that with a real CDmlRocket class
// reproducing the confirmed mechanics:
//   - BOTH fire subfunctions spawn the SAME entity/classname
//     ("dmlRocket", confirmed via the shared spawner FUN_10082f70
//     using one vtable @0x100fbb4c for both call sites) - the earlier
//     assumption of two distinct entity types (rocket vs. cluster
//     grenade) was wrong; it's one configurable class with a mode
//     field.
//   - CDmlRocket::Spawn (0x10089600) confirmed: model
//     "models/dmlrocket.mdl", MOVETYPE_FLY, SOLID_BBOX, a skill-scaled
//     damage field, and a THREE-WAY Think dispatch on an internal mode
//     field: FollowThink (a laser-designator "laser_spot" entity
//     seeker - NOT reproduced, "laser_spot" doesn't exist anywhere in
//     this project and would be new out-of-scope content), TrackTarget
//     (real predictive-intercept homing toward a locked EHANDLE
//     target, extrapolating the target's velocity and blending a
//     lead-point estimate frame-to-frame - reproduced here as a
//     documented approximation: steer velocity a fraction toward the
//     target's current+velocity-extrapolated position each tick,
//     preserving speed, rather than the exact lead/blend math), and
//     RocketThink (straight flight with a confirmed cosmetic banked
//     roll via avelocity - reproduced as a plain spin for visual
//     parity, not the exact banking vector math).
//   - CDmlRocket::ExplodeThink (0x1008be20) confirmed: TE_EXPLOSION-
//     style tempentity + a temporary glow sprite, one of three random
//     "weapons/kabam1-3.wav" sounds, and a radius damage call scaled
//     from pev->dmg - reproduced via UTIL_ScreenShake-free
//     RadiusDamage using the confirmed skill-scaled damage field.
//   - PrimaryAttack's real internal trigger-type field (read from the
//     WEAPON, not the projectile) determines whether a given shot uses
//     the dual-burst or single-guided fire subfunction - defaulting to
//     0 (RocketThink/dual-burst path) absent the customization system.
//     Simplified here onto the two available player inputs instead:
//     PrimaryAttack fires the confirmed single GUIDED shot (with a
//     real lock-on search, reproducing the intent of FUN_10083b60 as
//     a crosshair traceline check against a valid living monster),
//     SecondaryAttack fires the confirmed DUAL-rocket burst (2 ammo,
//     banked left/right, no lock-on).
//
// CORRECTION (2026-09-05, later pass): the "~13 methods, none
// reproduced, customization system not implemented anywhere" claim
// above is now partially out of date. The DML customization-item
// system (dml_customization.cpp) and CDmlRocket's Detonate/Payload
// matrix have both now been freshly decompiled and reproduced - see
// findings/entities/dml_customization_decompile.md for the full raw
// decompile of all 12 named CDmlRocket methods
// (ShootClusterGrenades/BigThink/LittleThink/TumbleThink/PowerupThink/
// BeamBreakThink/DelayDeathThink/BounceSoundTouch/TrackTouch/Smoke/
// RemoveThink/FollowThink) and both customization-item vtables.
// Confirmed field layout: `this+0x280` is the Payload type
// (0=Explosive default, 2=Cluster -> routes to ShootClusterGrenades
// instead of the normal explode path) and `this+0x284` is the
// Detonate/Trigger-condition type (0=default immediate, 2=a
// countdown/lock-based "close enough" check in BigThink, 3=Timed via
// TumbleThink checking a fuse deadline field). Per
// findings/weapons/weapon_dml.md's own manual-text breakdown, this
// Payload/Detonate pair belongs to the FIRED "MULE Launcher" (weapon_dml
// itself, gated by cust_3DMLDetonate/cust_4DMLPayload) - the THROWN
// "MULE Packs" grenade's separate On-impact/Timed/Tripwire trigger
// matrix (gated by cust_1DMLGrenDetonate/cust_2DMLGrenPayload) is
// handled in dmlgrenade.cpp instead, since that's a structurally
// different (cook-and-throw, not fired-and-flies) weapon.
//
// Reproduced here as `CDmlRocket::m_iDetonateType`/`m_iPayloadType`
// (persisted for the lifetime of the in-flight rocket, no save/restore
// - matches every other runtime-only projectile in this project, e.g.
// CClusterGrenade, none of which survive save/load either):
//   - `ShootClusterGrenades` (0x1008a430, confirmed): spawns 4
//     dml_cluster/CClusterGrenade sub-munitions via the SAME shared
//     ctor helper (`FUN_10081a70`) already confirmed and reused for
//     `env_clusterExplosion` this session - reproduced by calling the
//     existing `Dml_ShootClusterGrenade()` wrapper
//     (mayan6_misc.cpp) rather than re-deriving a second cluster
//     class. Exact scatter-vector math not reproduced byte-exact
//     (approximated with a simple randomized-offset scatter, same
//     simplification level as CClusterExplosion's own scatter).
//   - `TumbleThink`/`BounceSoundTouch` (0x1008c7e0/0x1008a940,
//     confirmed): while the fuse (a `pev->dmgtime`-equivalent
//     deadline) hasn't expired, the rocket tumbles and bounces off
//     world geometry only (confirmed: the touched entity's classname
//     is checked against "worldspawn" specifically, not other
//     entities) with a cooldown between bounce sounds; once the fuse
//     expires, detonates via `ShootClusterGrenades` or the normal
//     explode path depending on Payload type. Reproduced faithfully
//     minus the confirmed-but-cosmetic mid-air deceleration
//     (`_DAT_100e9388`-scaled velocity damping once grounded) and the
//     "already on ground -> just remove" early-out, both skipped as
//     visual-only polish.
//   - The Proximity Detonate mode (`this+0x284==2`, confirmed part of
//     `BigThink`'s countdown-gated distance logic) is approximated as
//     a simple "explode once within 64 units of the locked enemy"
//     check added to `TrackTarget`, rather than reproducing
//     `BigThink`'s full countdown/multi-rocket flock-avoidance sweep
//     (which also drives the Flightpath "Spiral" mode via mutual
//     rocket repulsion) or `FollowThink`'s laser-designator seek
//     (needs a `laser_spot` target entity that doesn't exist anywhere
//     in this project - confirmed dead end, same as the already-
//     documented lock-on approximation). Consequently
//     `cust_1DMLLaunch`/`cust_2DMLFlightpath` (Trigger When-fired/
//     When-locked, Flightpath Guided/Homing/Spiral) are implemented as
//     real, pickupable inventory items (dml_customization.cpp) but
//     have NO distinct gameplay effect here - the existing lock-on-
//     based TrackTarget/RocketThink dispatch already covers "guided
//     when locked, straight otherwise" and isn't re-branched by them.
//   - `PowerupThink`/`BeamBreakThink`/`DelayDeathThink`/`TrackTouch`/
//     `Smoke`/`RemoveThink`/`LittleThink` are documented in
//     `dml_customization_decompile.md` but not wired into gameplay
//     here: `BeamBreakThink`'s tripwire behavior belongs to the
//     grenade side per the manual-text mapping above (not this
//     class's fired-rocket use), `PowerupThink`'s pre-launch charge
//     delay and `Smoke`'s cosmetic trail are cosmetic-only polish out
//     of scope for this pass, and `TrackTouch`/`RemoveThink` are
//     already covered by this class's existing `RocketTouch`/
//     `UTIL_Remove` calls.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "saverestore.h"
#include "gamerules.h"
#include "skill.h"
#include "UserMessages.h"

#define DML_ROCKET_SPEED_GUIDED 900 // confirmed (single guided shot)
#define DML_ROCKET_SPEED_BURST 600	// confirmed (dual burst)
#define DML_HOMING_TURN_FRACTION 0.15 // approximated - see file header
#define DML_PROXIMITY_RADIUS 64 // approximated, see file header (BigThink not reproduced byte-exact)

// Confirmed field values from this+0x280/0x284, see file header.
enum DmlPayloadType
{
	DML_PAYLOAD_EXPLOSIVE = 0,
	DML_PAYLOAD_CLUSTER = 2,
};
enum DmlDetonateType
{
	DML_DETONATE_ONIMPACT = 0,
	DML_DETONATE_PROXIMITY = 2,
	DML_DETONATE_TIMED = 3,
};

// Shared with mayan6_misc.cpp's dml_cluster/CClusterGrenade (built for
// env_clusterExplosion) - see file header, reused here for
// ShootClusterGrenades rather than a second cluster class.
extern CBaseEntity* Dml_ShootClusterGrenade(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float flDamage);

//=========================================================
// dmlRocket - CDmlRocket. Single confirmed projectile class for both
// weapon_dml fire modes, see file header.
//=========================================================
class CDmlRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT RocketThink();
	void EXPORT TrackTarget();
	void EXPORT RocketTouch(CBaseEntity* pOther);
	void EXPORT TumbleThink();
	void EXPORT BounceSoundTouch(CBaseEntity* pOther);
	void ShootClusterGrenades();
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	bool m_bGuided = false; // true = TrackTarget (single guided shot), false = RocketThink (dual burst)
	int m_iPayloadType = DML_PAYLOAD_EXPLOSIVE;
	int m_iDetonateType = DML_DETONATE_ONIMPACT;

	// Nachtrag 2026-09-06 (RE-Nachtrag, ausgeloest durch svencoop_gc_crossref.md's
	// "m_bReloadSwitch"-Frage): CDml's echte Save-Felder m_iSide/m_iFirstSide
	// (weapon_save_fields.md) waren bislang komplett ungenutzt. Frischer
	// Decompile von FUN_10082ba0 (dem Dual-Rocket-Feuerpfad,
	// CDml::SecondaryAttack's Original) zeigt: beim Doppelschuss werden
	// beide Raketen explizit als Geschwisterpaar verlinkt (jede kennt die
	// andere ueber einen Pointer) und mit einer Seiten-ID 1/2 markiert -
	// passt zu den ebenfalls bereits bestaetigten, aber bislang
	// ungenutzten m_iSpiralScalar_In/m_iSpiralScalar_Up-Konfigurationsfeldern
	// und der Handbuch-Option "Guided/Homing/Spiral" fuer WEAPON_CUST_
	// DMLFLIGHTPATH. Reproduziert als einfache Orbit-Versetzung um die
	// gemeinsame Flugachse (Seite 1/2 = Phasenversatz 180 Grad), nicht die
	// exakte Original-Physik (die echte In/Up-Skalierung der beiden
	// Config-Felder wurde nicht decompiliert).
	int m_iSide = 0; // 0=kein Paar, 1/2=Spiral-Partner
	EHANDLE m_hSibling;
	bool m_bSpiralFlightpath = false;

private:
	float m_flNextBounceSound = 0;
	float m_flSpiralPhase = 0;
	int m_iTrailSprite = 0;
};
LINK_ENTITY_TO_CLASS(dmlRocket, CDmlRocket);

TYPEDESCRIPTION CDmlRocket::m_SaveData[] =
	{
		DEFINE_FIELD(CDmlRocket, m_bGuided, FIELD_BOOLEAN),
		DEFINE_FIELD(CDmlRocket, m_iPayloadType, FIELD_INTEGER),
		DEFINE_FIELD(CDmlRocket, m_iDetonateType, FIELD_INTEGER),
		DEFINE_FIELD(CDmlRocket, m_iSide, FIELD_INTEGER),
		DEFINE_FIELD(CDmlRocket, m_hSibling, FIELD_EHANDLE),
		DEFINE_FIELD(CDmlRocket, m_bSpiralFlightpath, FIELD_BOOLEAN),
		DEFINE_FIELD(CDmlRocket, m_flNextBounceSound, FIELD_TIME),
		DEFINE_FIELD(CDmlRocket, m_flSpiralPhase, FIELD_FLOAT),
		DEFINE_FIELD(CDmlRocket, m_iTrailSprite, FIELD_INTEGER),
	};

IMPLEMENT_SAVERESTORE(CDmlRocket, CGrenade);

void CDmlRocket::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("dmlRocket");
	SET_MODEL(ENT(pev), "models/dmlrocket.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->health = 1;
	pev->dmg = gSkillData.dmlRocketDamage;

	pev->avelocity = Vector(0, 0, 360); // cosmetic banked-roll approximation, see file header

	// Nachtrag 2026-09-06 (Nutzer-Meldung "Raketen-Schweif fehlt auch bei
	// anderen Raketen"): kein Flug-Rauchschweif und kein Licht-Effekt fuer
	// die Treibstoffverbrennung vorhanden. Ergaenzt nach demselben, bereits
	// im Projekt etablierten Muster wie CRpgRocket::IgniteThink (rpg.cpp)
	// und CAnimeRocket (rust4a_misc.cpp). Nachtrag 2 (2026-09-06, Sprite-
	// Pruefung): Trail-Sprite ist "sprites/smoke.spr", nicht "flame.spr" -
	// visuell verglichen (tools/spr_tool.py unpack), flame.spr ist ein
	// heller radialer Glow-/Burst-Sprite (kein Rauchschweif), retail als
	// separate Flame-/Glow-Attachment-Sprite genutzt (siehe
	// fgd_gap_closure.md, monster_tank_rocket-Precache-Liste: flame.spr
	// UND smokering.spr getrennt precacht). smoke.spr ist die im Projekt
	// durchgaengig RE-belegte Schweif-Textur (rpg.cpp, apache.cpp,
	// vehicle_tank.cpp, rust4a_misc.cpp, rust6d_misc.cpp).
	UTIL_RocketTrail(this, m_iTrailSprite);

	if (m_iDetonateType == DML_DETONATE_TIMED)
	{
		// Confirmed (TumbleThink/BounceSoundTouch): tumbles/bounces off
		// world geometry until a fuse deadline, then detonates - see
		// file header.
		SetTouch(&CDmlRocket::BounceSoundTouch);
		SetThink(&CDmlRocket::TumbleThink);
		pev->dmgtime = gpGlobals->time + 3.0; // approximated fuse, exact constant not resolved
	}
	else
	{
		SetTouch(&CDmlRocket::RocketTouch);
		SetThink(m_bGuided ? &CDmlRocket::TrackTarget : &CDmlRocket::RocketThink);
	}
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDmlRocket::Precache()
{
	PrecacheModel("models/dmlrocket.mdl");
	m_iTrailSprite = PrecacheModel("sprites/smoke.spr");
	PrecacheModel("sprites/muz1.spr");
	PrecacheModel("sprites/firebeam.spr");
	PrecacheModel("sprites/part1.spr");
	PrecacheModel("sprites/part2.spr");
	PrecacheSound("weapons/rocket1.wav");
	PrecacheSound("weapons/kabam1.wav");
	PrecacheSound("weapons/kabam2.wav");
	PrecacheSound("weapons/kabam3.wav");
	PrecacheSound("weapons/grenade_hit1.wav");
	PrecacheSound("weapons/grenade_hit2.wav");
	PrecacheSound("weapons/grenade_hit3.wav");
	PrecacheSound("weapons/dml_fragment.wav");
	UTIL_PrecacheOther("dml_cluster");
}

void CDmlRocket::RocketThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_bSpiralFlightpath && m_iSide != 0)
	{
		// Orbit-Versetzung um die Flugachse, Seite 1/2 = 180 Grad
		// Phasenversatz - siehe Klassen-Kommentar.
		constexpr float SPIRAL_RATE = 6.0f;	// Radiant/s, Schaetzwert
		constexpr float SPIRAL_RADIUS = 12.0f; // Units, Schaetzwert
		m_flSpiralPhase += SPIRAL_RATE * 0.1f;
		float flPhase = m_flSpiralPhase + (m_iSide == 2 ? M_PI : 0.0f);

		Vector vecFwd = pev->velocity.Normalize();
		Vector vecUp = Vector(0, 0, 1);
		if (fabs(vecFwd.z) > 0.9f)
			vecUp = Vector(1, 0, 0);
		Vector vecRight = CrossProduct(vecFwd, vecUp).Normalize();
		vecUp = CrossProduct(vecRight, vecFwd).Normalize();

		Vector vecOffset = (vecRight * cos(flPhase) + vecUp * sin(flPhase)) * SPIRAL_RADIUS;
		float flSpeed = pev->velocity.Length();
		pev->velocity = (vecFwd * flSpeed) + vecOffset * SPIRAL_RATE;
		pev->angles = UTIL_VecToAngles(pev->velocity);
	}
}

void CDmlRocket::TrackTarget()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_hEnemy || !m_hEnemy->IsAlive())
	{
		SetThink(&CDmlRocket::RocketThink);
		return;
	}

	// Approximated Proximity detonate mode (this+0x284==2, part of the
	// unreproduced BigThink's countdown/distance logic) - see file
	// header.
	if (m_iDetonateType == DML_DETONATE_PROXIMITY &&
		(m_hEnemy->pev->origin - pev->origin).Length() <= DML_PROXIMITY_RADIUS)
	{
		RocketTouch(m_hEnemy);
		return;
	}

	// Approximated predictive intercept: lead the target by its current
	// velocity, then steer a fraction of the way there each tick,
	// preserving speed - see file header for the real (unreproduced)
	// lead/blend math.
	Vector vecTargetPos = m_hEnemy->pev->origin + m_hEnemy->pev->velocity * 0.2;
	Vector vecToTarget = (vecTargetPos - pev->origin).Normalize();

	float flSpeed = pev->velocity.Length();
	Vector vecNewDir = (pev->velocity.Normalize() * (1.0 - DML_HOMING_TURN_FRACTION) + vecToTarget * DML_HOMING_TURN_FRACTION).Normalize();
	pev->velocity = vecNewDir * flSpeed;
	pev->angles = UTIL_VecToAngles(pev->velocity);
}

void CDmlRocket::RocketTouch(CBaseEntity* pOther)
{
	if (m_iPayloadType == DML_PAYLOAD_CLUSTER)
	{
		ShootClusterGrenades();
		return;
	}

	const char* booms[] = {"weapons/kabam1.wav", "weapons/kabam2.wav", "weapons/kabam3.wav"};
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, booms[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(15);
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects).
	UTIL_ExplosionEffects(pev->origin, 250.0f);

	RadiusDamage(pev->origin, pev, VARS(pev->owner), pev->dmg, CLASS_NONE, DMG_BLAST);

	UTIL_Remove(this);
}

// Confirmed (0x1008a940): only plays a bounce sound when the touched
// entity is worldspawn specifically (not other entities), with a
// cooldown between plays - see file header.
void CDmlRocket::BounceSoundTouch(CBaseEntity* pOther)
{
	CGrenade::BounceTouch(pOther);

	if (!pOther || !FClassnameIs(pOther->pev, "worldspawn"))
		return;
	if (gpGlobals->time < m_flNextBounceSound)
		return;

	const char* sounds[] = {"weapons/grenade_hit1.wav", "weapons/grenade_hit2.wav", "weapons/grenade_hit3.wav"};
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, sounds[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM);
	m_flNextBounceSound = gpGlobals->time + 0.5; // approximated interval, exact constant not resolved
}

// Confirmed (0x1008c7e0): tumbles until pev->dmgtime (the fuse
// deadline), then detonates via the normal explode path or
// ShootClusterGrenades depending on Payload type - see file header
// (cosmetic mid-air deceleration/grounded-early-out not reproduced).
void CDmlRocket::TumbleThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (gpGlobals->time < pev->dmgtime)
		return;

	SetTouch(nullptr);
	SetThink(nullptr);

	if (m_iPayloadType == DML_PAYLOAD_CLUSTER)
		ShootClusterGrenades();
	else
		RocketTouch(nullptr);
}

// Confirmed (0x1008a430): spawns 4 dml_cluster sub-munitions via the
// same shared ctor helper used by env_clusterExplosion, then removes
// self - see file header for the scatter-math simplification.
void CDmlRocket::ShootClusterGrenades()
{
	for (int i = 0; i < 4; i++)
	{
		Vector vecScatter = pev->origin + Vector(RANDOM_FLOAT(-8, 8), RANDOM_FLOAT(-8, 8), RANDOM_FLOAT(-8, 8));
		Vector vecVelocity = Vector(RANDOM_FLOAT(-150, 150), RANDOM_FLOAT(-150, 150), RANDOM_FLOAT(50, 150));
		Dml_ShootClusterGrenade(pev->owner ? VARS(pev->owner) : pev, vecScatter, vecVelocity, pev->dmg * 0.4f);
	}

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/dml_fragment.wav", 1.0, ATTN_NORM);
	SetTouch(nullptr);
	SetThink(nullptr);
	UTIL_Remove(this);
}

//=========================================================
// weapon_dml - CDml.
//=========================================================
class CDml : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool Deploy() override;
	void Holster() override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void WeaponIdle() override;
	bool ShouldWeaponIdle() override { return true; }

private:
	CBaseEntity* FindLockOnTarget();
};
LINK_ENTITY_TO_CLASS(weapon_dml, CDml);
LINK_ENTITY_TO_CLASS(weapon_mule, CDml);

void CDml::Spawn()
{
	pev->classname = MAKE_STRING("weapon_dml");
	Precache();
	m_iId = WEAPON_DML;
	SET_MODEL(ENT(pev), "models/w_dml.mdl");

	FallInit();
}

void CDml::Precache()
{
	PrecacheModel("models/w_dml.mdl");
	PrecacheModel("models/v_dml.mdl");
	PrecacheModel("models/p_crossbow.mdl");
	PrecacheModel("sprites/dmllock.spr");
	PrecacheSound("weapons/dml_fire.wav");
	PrecacheSound("weapons/dml_reload.wav");
	PrecacheSound("weapons/dml_dualreload.wav");
	PrecacheSound("weapons/dml_customize.wav");
	PrecacheSound("weapons/dryfire.wav");
	PrecacheSound("weapons/dml_lock.wav");

	UTIL_PrecacheOther("dmlRocket");
}

bool CDml::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "missiles";
	p->iMaxAmmo1 = DML_MAX_CARRY;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 6;
	p->iSlot = 5;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_DML;
	p->iFlags = 5;
	p->iWeight = 10;

	return true;
}

bool CDml::Deploy()
{
	return DefaultDeploy("models/v_dml.mdl", "models/p_crossbow.mdl", 0, "crossbow");
}

// CORRECTION (2026-09-05): this class had no Holster() override at all
// - confirmed via client.dll's mirrored weapon-prediction class (Vtable
// slot 65, findings/client/client_weapons.md's open point) that the
// real CDml::Holster() exists and resets the lock-on sprite/sound state
// alongside the shared generic Holster tail. This SDK's PrimaryAttack
// only plays "dml_lock.wav" as a one-shot (not a looping channel), so
// there is no stuck sound to stop server-side; added for consistency
// with weapon_beamgun/weapon_gausspistol/weapon_minigun's identical
// m_flNextAttack reset pattern.
void CDml::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
}

// Reproduces the intent of FUN_10083b60: a crosshair traceline check
// against a living, damageable monster - see file header.
CBaseEntity* CDml::FindLockOnTarget()
{
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 4096;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	if (tr.flFraction >= 1.0)
		return nullptr;

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
	if (pEntity && pEntity->IsAlive() && 0 != pEntity->pev->takedamage)
		return pEntity;

	return nullptr;
}

void CDml::PrimaryAttack()
{
	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25;
		return;
	}

	m_iClip--;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 16 + gpGlobals->v_up * -8;

	CBaseEntity* pTarget = FindLockOnTarget();

	CDmlRocket* pRocket = GetClassPtr((CDmlRocket*)nullptr);
	pRocket->pev->angles = m_pPlayer->pev->v_angle;
	pRocket->pev->origin = vecSrc;
	pRocket->m_bGuided = pTarget != nullptr;
	// Read the DML customization items' persistent flags (see
	// dml_customization.cpp) - confirmed mechanism, see file header.
	if (m_pPlayer->HasPlayerItemFromID(WEAPON_CUST_DMLPAYLOAD))
		pRocket->m_iPayloadType = DML_PAYLOAD_CLUSTER;
	if (m_pPlayer->HasPlayerItemFromID(WEAPON_CUST_DMLDETONATE) && pTarget)
		pRocket->m_iDetonateType = DML_DETONATE_PROXIMITY;
	pRocket->Spawn();
	pRocket->pev->owner = m_pPlayer->edict();
	pRocket->pev->velocity = gpGlobals->v_forward * DML_ROCKET_SPEED_GUIDED;
	if (pTarget)
	{
		pRocket->m_hEnemy = pTarget;
		EMIT_SOUND(m_pPlayer->edict(), CHAN_ITEM, "weapons/dml_lock.wav", 1.0, ATTN_NORM);
	}

	EMIT_SOUND(m_pPlayer->edict(), CHAN_WEAPON, "weapons/dml_fire.wav", 1.0, ATTN_NORM);
	pev->effects |= EF_MUZZLEFLASH;
	// Retail: Schuss-Sequenz "fire" (Index 5, v_dml.mdl); Original-Sendung an PrimaryAttack belegt (findings/weapons/anim_index_abgleich_2026-11-01.md).
	SendWeaponAnim(5);

	m_pPlayer->pev->punchangle.x -= 3;

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;

	if (m_iClip == 0)
		EMIT_SOUND(m_pPlayer->edict(), CHAN_ITEM, "weapons/dml_reload.wav", 1.0, ATTN_NORM);
}

void CDml::SecondaryAttack()
{
	if (m_iClip < 2)
	{
		PlayEmptySound();
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
		return;
	}

	m_iClip -= 2;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();

	// Nachtrag 2026-09-06: echte Retail-Mechanik (FUN_10082ba0) verlinkt
	// beide Raketen des Doppelschusses als Geschwisterpaar mit
	// Seiten-ID 1/2 - siehe CDmlRocket-Klassenkommentar.
	bool bSpiral = m_pPlayer->HasPlayerItemFromID(WEAPON_CUST_DMLFLIGHTPATH);
	CDmlRocket* pRockets[2] = {nullptr, nullptr};
	int iIdx = 0;
	for (int i = -1; i <= 1; i += 2, iIdx++)
	{
		CDmlRocket* pRocket = GetClassPtr((CDmlRocket*)nullptr);
		pRocket->pev->angles = m_pPlayer->pev->v_angle;
		pRocket->pev->origin = vecSrc + gpGlobals->v_right * (i * 6);
		pRocket->m_bGuided = false;
		pRocket->m_iSide = iIdx + 1;
		pRocket->m_bSpiralFlightpath = bSpiral;
		if (m_pPlayer->HasPlayerItemFromID(WEAPON_CUST_DMLPAYLOAD))
			pRocket->m_iPayloadType = DML_PAYLOAD_CLUSTER;
		pRocket->Spawn();
		pRocket->pev->owner = m_pPlayer->edict();
		pRocket->pev->velocity = gpGlobals->v_forward * DML_ROCKET_SPEED_BURST + gpGlobals->v_right * (i * 20);
		pRockets[iIdx] = pRocket;
	}
	if (pRockets[0] && pRockets[1])
	{
		pRockets[0]->m_hSibling = pRockets[1];
		pRockets[1]->m_hSibling = pRockets[0];
	}

	EMIT_SOUND(m_pPlayer->edict(), CHAN_WEAPON, "weapons/dml_fire.wav", 1.0, ATTN_NORM);
	EMIT_SOUND(m_pPlayer->edict(), CHAN_ITEM, "weapons/dml_dualreload.wav", 1.0, ATTN_NORM);
	SendWeaponAnim(0);

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.5;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
}

void CDml::WeaponIdle()
{
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	SendWeaponAnim(0);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
}
