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
// type "battery" (same resource as item_battery), iMaxAmmo1=100,
// iMaxClip=WEAPON_NOCLIP, slot 4 / position 0.
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
//     helper (0x1007e4d0) every frame that fires the beam and advances
//     the 0-100 heat counter (+0xe8) on a deadline. Retail Spawn defaults
//     to +0xa4=3, +0xa8=2, +0xac=2; the default counter gains 2 per
//     0.1 s (single-player) or 0.16 s (multiplayer). At 100, Retail
//     enters an overheat state, plays "weapons/electro4.wav", and sets
//     the player's next-attack deadline to 5 s. The ball_lightning
//     branch is conditional on the high-voltage setting (+0xac>3) and
//     a random gate; it is not the default threshold behavior. Exact
//     self-damage call path remains unresolved.
//   CBeamGun::SecondaryAttack (0x1007c430) is NOT a ball_lightning
//     spawn - its body is a single call into the beam's own Slot-89
//     function (0x1007dc60), a purely cosmetic 7-way random spark-
//     discharge temp-entity burst (no CBaseEntity::Create/projectile
//     spawn at all), consistent with the precached "electro4.wav"/
//     "sprites/XSpark1.spr" assets.
// The server's default mode now follows these verified timers, heat
// increments, holster cooling, and five-second recovery. The custom
// selector values, exact delayed self-damage consumer, and conditional
// high-voltage ball_lightning branch remain open. The continuous beam
// damage trace and the SecondaryAttack tracer targeting still contain
// separate approximations. The "Long/Medium/Short range"/"Power"
// configuration keyvalues and the "Chain" lightning-form remain
// unresolved.
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
	bool UpdateClientData(CBasePlayer* pPlayer) override;
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
	void SendHeatHud(CBasePlayer* pPlayer);

	CBeam* m_pBeam = nullptr;
	float m_flAmmoUseTime = 0;
	float m_flHeat = 1; // Retail Spawn initializes +0xe8 to 1.
	float m_flNextHeatTick = 0; // Retail +0xec: active heat-increase deadline.
	float m_flNextHeatCool = 0; // Retail +0xf8: inventory-update cooling deadline.
	float m_flOverheatStart = 0; // Retail +0xf4.
	bool m_bOverheated = false; // Retail +0xf0.
	float m_flNextTracerVolley = 0;
};
LINK_ENTITY_TO_CLASS(weapon_beamgun, CBeamGun);
LINK_ENTITY_TO_CLASS(weapon_polarisblade, CBeamGun);

TYPEDESCRIPTION CBeamGun::m_SaveData[] =
	{
		DEFINE_FIELD(CBeamGun, m_pBeam, FIELD_CLASSPTR),
		DEFINE_FIELD(CBeamGun, m_flAmmoUseTime, FIELD_TIME),
		DEFINE_FIELD(CBeamGun, m_flHeat, FIELD_FLOAT),
		DEFINE_FIELD(CBeamGun, m_flNextHeatTick, FIELD_TIME),
		DEFINE_FIELD(CBeamGun, m_flNextHeatCool, FIELD_TIME),
		DEFINE_FIELD(CBeamGun, m_flOverheatStart, FIELD_TIME),
		DEFINE_FIELD(CBeamGun, m_bOverheated, FIELD_BOOLEAN),
		DEFINE_FIELD(CBeamGun, m_flNextTracerVolley, FIELD_TIME),
	};

IMPLEMENT_SAVERESTORE(CBeamGun, CBasePlayerWeapon);

void CBeamGun::Spawn()
{
	pev->classname = MAKE_STRING("weapon_beamgun");
	m_flHeat = 1;
	m_flNextHeatTick = 0;
	m_flNextHeatCool = 0;
	m_flOverheatStart = 0;
	m_bOverheated = false;
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
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
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
}

void CBeamGun::SendHeatHud(CBasePlayer* pPlayer)
{
	if (!pPlayer)
		return;

	MESSAGE_BEGIN(MSG_ONE, gmsgSliderX, nullptr, pPlayer->pev);
	WRITE_BYTE(WEAPON_BEAMGUN);
	WRITE_BYTE((int)m_flHeat);
	MESSAGE_END();
}

bool CBeamGun::UpdateClientData(CBasePlayer* pPlayer)
{
	// Retail slot 67 (FUN_1007db50) runs once for every inventory item.
	// While overheated, it holds the state for five seconds, then resumes
	// at heat 70. Otherwise it cools one point every 0.2 seconds, to 1.
	if (m_bOverheated)
	{
		if (gpGlobals->time - m_flOverheatStart >= 5.0f)
		{
			m_bOverheated = false;
			m_flHeat = 70;
		}
		SendHeatHud(pPlayer);
	}
	else if (gpGlobals->time > m_flNextHeatCool)
	{
		if (m_flHeat > 1.0f)
			m_flHeat = V_max(m_flHeat - 1.0f, 1.0f);
		m_flNextHeatCool = gpGlobals->time + 0.2f;

		if (pPlayer && pPlayer->m_pActiveItem == this)
			SendHeatHud(pPlayer);
	}

	return CBasePlayerWeapon::UpdateClientData(pPlayer);
}

void CBeamGun::PrimaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
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
		// Retail checks the player's ammo pool and debits one battery from it
		// on the discharge timer; the beamgun has no local clip (iMaxClip=-1).
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
		// Retail Spawn defaults (+0xa4=3, +0xa8=2), selecting 0.12 s.
		m_flAmmoUseTime = gpGlobals->time + 0.12f;
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

	// Retail +0xa8 defaults to 2: two heat points per due tick. Retail
	// schedules ticks at 0.1 s (single-player) or 0.16 s (multiplayer).
	if (gpGlobals->time > m_flNextHeatTick)
	{
		m_flHeat = V_min(m_flHeat + 2.0f, 100.0f);
		m_flNextHeatTick = gpGlobals->time + (g_pGameRules->IsMultiplayer() ? 0.16f : 0.1f);
		SendHeatHud(m_pPlayer);

		if (m_flHeat >= 100.0f && !m_bOverheated)
		{
			m_bOverheated = true;
			m_flOverheatStart = gpGlobals->time;
			m_pPlayer->m_flNextAttack = gpGlobals->time + 5.0f;
			m_flTimeWeaponIdle = gpGlobals->time + 0.2f;
			EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 100);
		}
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
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
