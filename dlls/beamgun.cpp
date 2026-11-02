//=========================================================
// weapon_beamgun (Alias: weapon_polarisblade) - CBeamGun. First of the
// six points deferred during the map-driven pass (see
// findings/weapons/weapon_beamgun.md and
// [[project-weapon-beamgun-deferred]] memory) to be picked up.
//
// Decompiled fresh from gunman.dll this session: LINK-equivalent
// constructor sets classname "weapon_beamgun" always (even spawned as
// "weapon_polarisblade"), vtable @0x100fa100, Spawn @0x1007c040,
// Precache @0x1007c0c0, GetItemInfo @0x1007c3a0, Deploy @0x1007c1b0.
// Confirmed: weapon ID 0xe(14), world model "models/w_beam.mdl", ammo
// type "battery" (same resource as item_battery), iMaxClip=4,
// iWeight=10, no secondary ammo type. Precache confirms
// "models/p_egon.mdl" and several "egon_*"-named sounds are reused
// UNCHANGED from stock Half-Life's Gluon Gun - this is a
// reskinned/renamed Egon Gun, matching this project's existing CEgon
// (dlls/egon.cpp, an unmodified stock-SDK class kept only as a
// reference template here, not linked to any Gunman classname and NOT
// altered by this file).
//
// CORRECTION (2026-09-05, later same-day pass): the initial version of
// this file claimed "PrimaryAttack never located" and put the
// ball_lightning spawn in SecondaryAttack. Both claims were wrong - a
// pre-existing findings doc (findings/weapons/primaryattack_
// breakthrough.md, Session 79/95, missed during the first pass because
// it lives outside weapon_beamgun.md) had already located and fully
// decompiled the real functions. Now fresh-decompiled again directly
// from gunman.dll to confirm:
//   CBeamGun::PrimaryAttack (0x1007c440): a real charge-up/continuous-
//     beam state machine - idle state plays "egon_windup2.wav" and
//     transitions to firing state; firing state calls a per-tick
//     helper (0x1007e4d0) every frame that both fires the beam damage
//     AND accumulates a 0-100 overheat counter (this+0xe8, difficulty-
//     scaled increment rate). At counter==100 it calls the confirmed
//     ball_lightning spawner FUN_1007fc40 (see findings' Nachtrag 4 -
//     the exact same CBallLightning class already implemented in
//     cinematic2_misc.cpp for monster_rustflier, confirmed shared
//     between both callers), plays "weapons/electro4.wav", and arms a
//     self-damage timer on the player (matches the official manual's
//     documented text: "overheating this volatile weapon sends a
//     surge through your body... deploy an energy-wasting round of
//     ball lightning").
//   CBeamGun::SecondaryAttack (0x1007c430) is NOT a ball_lightning
//     spawn - its body is a single call into the beam's own Slot-89
//     function (0x1007dc60), a purely cosmetic 7-way random spark-
//     discharge temp-entity burst (no CBaseEntity::Create/projectile
//     spawn at all), consistent with the precached "electro4.wav"/
//     "sprites/XSpark1.spr" assets.
// Reproduced here accordingly: PrimaryAttack keeps the previously-
// written continuous-beam state machine (independently written,
// modeled on CEgon::Attack/Fire's confirmed stock-SDK pattern per
// agents.md's "keine dekompilierten Originalquellen als Implementierung
// übernehmen" rule - CEgon's own file remains untouched) but now
// accumulates heat every tick and spawns ball_lightning plus
// self-damage on overheat, exactly where the decompile puts it.
// SecondaryAttack now fires the cosmetic spark-discharge effect
// instead. The exact heat-increment/difficulty-scaling formulas and
// the random pre-100 escalation chance in 0x1007e4d0 are NOT
// reproduced byte-exact - approximated as a flat per-tick increment,
// same simplification level used throughout this project for
// documented-but-not-fully-traced numeric constants. The "Long/Medium/
// Short range"/"Power" configuration keyvalues and the "Chain"
// lightning-form remain out of scope, unrelated to this correction.
//
// CORRECTION (2026-09-05, later pass): the "SecondaryAttack is purely
// cosmetic" claim above was itself incomplete. Fresh decompile of the
// spark-discharge call chain (`FUN_1007c7c0` -> `FUN_1007d110`) shows
// `FUN_1007d110` does the cosmetic beam-draw tempentity AND, gated by
// a singleplayer-only condition (`FUN_1007bfc0`: true only if a
// specific pev->effects bit is set on the beamgun AND a float field
// exceeds a threshold - approximated here as "player has a living
// enemy in front of them", the project's established lock-on
// simplification pattern), fires **three "tracer" entities** in a
// loop (`FUN_1007e090`, confirmed via string xref to spawn classname
// "tracer" = `CLightningTracer`, implemented below) with a cooldown
// timer. This resolves findings/entities/code_annahme_final_batch.md's
// open "tracer"-vs-"lightning_bug" relationship question: they are
// two separate homing sub-projectile families - "tracer" from
// weapon_beamgun's SecondaryAttack, "lightning_bug" from
// ball_lightning's own flight (see cinematic2_misc.cpp). Reproduced
// here as: SecondaryAttack keeps the cosmetic spark burst always, and
// additionally fires 3 CLightningTracer bolts toward the nearest
// living enemy when one exists in range (shared
// FindLightningBugTarget() helper from cinematic2_misc.cpp), on a
// cooldown - the exact original cooldown/condition constants were not
// cleanly resolved from the decompile and are approximated.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "saverestore.h"
#include "customentity.h"
#include "gamerules.h"
#include "UserMessages.h"

#define BEAMGUN_BEAM_SPRITE "sprites/xbeam1.spr"
#define BEAMGUN_FLARE_SPRITE "sprites/XSpark1.spr"

// Shared with cinematic2_misc.cpp's ball_lightning/lightning_bug
// implementation - see its definition there for the simplification
// rationale.
extern CBaseEntity* FindLightningBugTarget(CBaseEntity* pOwner, const Vector& vecOrigin);

//=========================================================
// tracer - CLightningTracer. Homing bolt fired 3x by weapon_beamgun's
// SecondaryAttack (see file header). Reuses the same simplified
// homing pattern as CLightningBug/CDmlRocket::TrackTarget.
//=========================================================
class CLightningTracer : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT TrackTarget();
	void EXPORT TrackTouch(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(tracer, CLightningTracer);

void CLightningTracer::Precache()
{
	PrecacheModel(BEAMGUN_FLARE_SPRITE);
}

void CLightningTracer::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("tracer");
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 160;
	pev->scale = 0.3;
	SET_MODEL(ENT(pev), BEAMGUN_FLARE_SPRITE);
	UTIL_SetSize(pev, Vector(-1, -1, -1), Vector(1, 1, 1));
	pev->dmg = 10; // approximated, exact value not decompiled

	SetTouch(&CLightningTracer::TrackTouch);
	SetThink(&CLightningTracer::TrackTarget);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = gpGlobals->time + 3.0; // self-expiry, no original timing decompiled
}

void CLightningTracer::TrackTarget()
{
	if (gpGlobals->time >= pev->dmgtime)
	{
		UTIL_Remove(this);
		return;
	}
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_hEnemy || !m_hEnemy->IsAlive())
		return;

	Vector vecToTarget = (m_hEnemy->pev->origin - pev->origin).Normalize();
	float flSpeed = pev->velocity.Length();
	if (flSpeed < 1.0f)
		flSpeed = 500.0f;

	Vector vecNewDir = (pev->velocity.Normalize() * 0.8 + vecToTarget * 0.2).Normalize();
	pev->velocity = vecNewDir * flSpeed;
	pev->angles = UTIL_VecToAngles(pev->velocity);
}

void CLightningTracer::TrackTouch(CBaseEntity* pOther)
{
	if (pOther && pOther->pev->takedamage != DAMAGE_NO)
		pOther->TakeDamage(pev, pev, pev->dmg, DMG_SHOCK);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPARKS);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	MESSAGE_END();

	UTIL_Remove(this);
}

//=========================================================
// weapon_beamgun - CBeamGun.
//=========================================================
class CBeamGun : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool Deploy() override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	void Holster() override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void WeaponIdle() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	bool ShouldWeaponIdle() override { return true; }

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

private:
	void EndAttack();
	void SparkDischarge();

	CBeam* m_pBeam = nullptr;
	float m_flAmmoUseTime = 0;
	float m_flHeat = 0; // confirmed 0-100 overheat counter, see file header
	float m_flNextTracerVolley = 0;
};
LINK_ENTITY_TO_CLASS(weapon_beamgun, CBeamGun);
LINK_ENTITY_TO_CLASS(weapon_polarisblade, CBeamGun);

TYPEDESCRIPTION CBeamGun::m_SaveData[] =
	{
		DEFINE_FIELD(CBeamGun, m_pBeam, FIELD_CLASSPTR),
		DEFINE_FIELD(CBeamGun, m_flAmmoUseTime, FIELD_TIME),
		DEFINE_FIELD(CBeamGun, m_flHeat, FIELD_FLOAT),
		DEFINE_FIELD(CBeamGun, m_flNextTracerVolley, FIELD_TIME),
	};

IMPLEMENT_SAVERESTORE(CBeamGun, CBasePlayerWeapon);

void CBeamGun::Spawn()
{
	pev->classname = MAKE_STRING("weapon_beamgun");
	Precache();
	m_iId = WEAPON_BEAMGUN;
	SET_MODEL(ENT(pev), "models/w_beam.mdl");

	FallInit();
}

void CBeamGun::Precache()
{
	PrecacheModel("sprites/beamlight2.spr");
	PrecacheModel("models/w_beam.mdl");
	PrecacheModel("models/v_beam.mdl");
	PrecacheModel("models/p_egon.mdl");
	PrecacheSound("items/9mmclip1.wav");
	PrecacheSound("weapons/egon_off1.wav");
	PrecacheSound("weapons/egon_run3.wav");
	PrecacheSound("weapons/egon_windup2.wav");
	PrecacheSound("weapons/electro4.wav");
	PrecacheSound("weapons/overheat.wav");
	PrecacheModel(BEAMGUN_BEAM_SPRITE);
	PrecacheModel(BEAMGUN_FLARE_SPRITE);
	PrecacheModel("sprites/gorebot.spr");

	UTIL_PrecacheOther("ball_lightning");
	UTIL_PrecacheOther("tracer");
}

bool CBeamGun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "battery";
	p->iMaxAmmo1 = BATTERY_MAX_CARRY;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 4;
	p->iSlot = 3;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_BEAMGUN;
	p->iFlags = 0;
	p->iWeight = 10;

	return true;
}

bool CBeamGun::Deploy()
{
	return DefaultDeploy("models/v_beam.mdl", "models/p_egon.mdl", 0, "egon");
}

void CBeamGun::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	EndAttack();
}

void CBeamGun::EndAttack()
{
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = nullptr;
	}
	STOP_SOUND(m_pPlayer->edict(), CHAN_WEAPON, "weapons/egon_run3.wav");
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
	m_flHeat = V_max(m_flHeat - 20, 0.0);
}

void CBeamGun::PrimaryAttack()
{
	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25;
		return;
	}

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecEnd = vecSrc + vecAiming * 1024;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	if (!m_pBeam)
	{
		m_pBeam = CBeam::BeamCreate(BEAMGUN_BEAM_SPRITE, 40);
		m_pBeam->PointEntInit(tr.vecEndPos, entindex());
		m_pBeam->SetEndAttachment(1);
		m_pBeam->SetColor(96, 128, 255);
		m_pBeam->SetBrightness(200);
		m_pBeam->SetNoise(20);
		// Retail: Schuss-Sequenz "fire" (Index 4, v_beam.mdl) beim Start des Strahls (findings/weapons/anim_index_abgleich_2026-11-01.md).
		SendWeaponAnim(4);
		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/egon_windup2.wav", 1.0, ATTN_NORM, 0, 100);
		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_STATIC, "weapons/egon_run3.wav", 1.0, ATTN_NORM, SND_CHANGE_VOL, 100);
	}
	else
	{
		m_pBeam->PointEntInit(tr.vecEndPos, entindex());
	}

	if (gpGlobals->time >= m_flAmmoUseTime)
	{
		m_iClip--;
		m_flAmmoUseTime = gpGlobals->time + 0.2;
	}

	if (tr.flFraction < 1.0)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity && 0 != pEntity->pev->takedamage)
		{
			ClearMultiDamage();
			pEntity->TraceAttack(m_pPlayer->pev, 4, vecAiming, &tr, DMG_ENERGYBEAM | DMG_SHOCK);
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
		}
	}

	// Confirmed 0-100 overheat counter (0x1007e4d0) - increment rate
	// approximated, see file header. At 100 the original spawns
	// ball_lightning and damages the player (matches the official
	// manual's documented overheat text).
	m_flHeat += 4;
	if (m_flHeat >= 100)
	{
		m_flHeat = 60; // stays hot, doesn't fully reset - matches confirmed "stays escalated" shape

		Vector vecBallSrc = vecSrc + vecAiming * 32;
		CBaseEntity* pBall = CBaseEntity::Create("ball_lightning", vecBallSrc, m_pPlayer->pev->v_angle);
		pBall->pev->owner = m_pPlayer->edict();
		pBall->pev->velocity = vecAiming * 300;

		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 100);
		m_pPlayer->TakeDamage(pev, pev, 5, DMG_SHOCK);
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;

	if (m_iClip <= 0)
		EndAttack();
}

void CBeamGun::SecondaryAttack()
{
	SparkDischarge();

	// CORRECTION (2026-09-05): SecondaryAttack also fires a 3-tracer
	// volley toward a nearby living enemy when one exists (see file
	// header) - the original's exact lock-on condition/cooldown was
	// not cleanly resolved, approximated here as "has a nearby target"
	// gated on this weapon's own cooldown timer.
	if (gpGlobals->time >= m_flNextTracerVolley)
	{
		UTIL_MakeVectors(m_pPlayer->pev->v_angle);
		Vector vecSrc = m_pPlayer->GetGunPosition();
		CBaseEntity* pTarget = FindLightningBugTarget(m_pPlayer, vecSrc);
		if (pTarget)
		{
			for (int i = 0; i < 3; i++)
			{
				CBaseEntity* pTracer = CBaseEntity::Create("tracer", vecSrc, m_pPlayer->pev->v_angle, m_pPlayer->edict());
				if (pTracer)
				{
					pTracer->pev->velocity = gpGlobals->v_forward * 500 + Vector(RANDOM_FLOAT(-40, 40), RANDOM_FLOAT(-40, 40), RANDOM_FLOAT(-40, 40));
					static_cast<CGrenade*>(pTracer)->m_hEnemy = pTarget;
				}
			}
			m_flNextTracerVolley = gpGlobals->time + 1.0;
		}
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

void CBeamGun::SparkDischarge()
{
	// Confirmed (0x1007dc60): a purely cosmetic 7-way random spark
	// temp-entity burst around the muzzle, NOT a ball_lightning spawn
	// - see file header correction.
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();

	for (int i = 0; i < 7; i++)
	{
		Vector vecSpark = vecSrc + gpGlobals->v_forward * RANDOM_FLOAT(8, 32) + Vector(RANDOM_FLOAT(-16, 16), RANDOM_FLOAT(-16, 16), RANDOM_FLOAT(-16, 16));

		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSpark);
		WRITE_BYTE(TE_SPARKS);
		WRITE_COORD(vecSpark.x);
		WRITE_COORD(vecSpark.y);
		WRITE_COORD(vecSpark.z);
		MESSAGE_END();
	}

	EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 100);
}

void CBeamGun::WeaponIdle()
{
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_pBeam)
		EndAttack();

	SendWeaponAnim(0);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
}

// Retail: beim Aufnehmen die Anpassungs-Items verteilen (customize_items.cpp)
void CBeamGun::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	pPlayer->GiveNamedItem("cust_1BeamFrequency");
	pPlayer->GiveNamedItem("cust_2BeamCurrent");
	pPlayer->GiveNamedItem("cust_3BeamVoltage");
}
