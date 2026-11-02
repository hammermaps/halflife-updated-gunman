//=========================================================
// weapon_gausspistol (Aliases: weapon_pistol) - CGaussPistol. Fourth
// of the six points deferred during the map-driven pass (see
// findings/weapons/weapon_gausspistol.md and
// [[project-weapon-gausspistol-deferred]] memory) to be picked up -
// the most thoroughly pre-decompiled of the five deferred weapons
// (all four in-game-manual fire modes were already traced to concrete
// addresses across earlier sessions).
//
// Decompiled fresh from gunman.dll this session: LINK-equivalent
// constructor sets classname "weapon_gausspistol" always (even
// spawned as "weapon_pistol"), vtable @0x100fcb88, Spawn @0x100901f0,
// Precache @0x10090270, PrimaryAttack dispatch @0x100907a0. Confirms
// findings/weapons/weapon_gausspistol.md exactly: weapon ID 0x1e(30),
// world model "models/w_gauss.mdl", ammo type "uranium" (its own
// resource, not shared 9mm), iMaxClip=1. Precache confirms
// "models/p_357.mdl" (stock HL's Python playermodel, reused unchanged
// - dlls/python.cpp is untouched, only used as a reference for
// hitscan-pistol structure) and the confirmed sniper-zoom sound set
// (gsnipe_zoom.wav/sniperzoom.wav/sniperunzoom.wav), plus 4
// UTIL_PrecacheOther calls matching the doc's four companion classes
// (gauss_charged/guass_bolt/gauss_ball/gauss_glow - none of which are
// placed by any classname-gap map scanned so far, so none are
// reimplemented here).
//
// PrimaryAttack's dispatch on the internal fire-mode field
// (param_1[0x2d]) is confirmed as: field==4 -> sniper-scope toggle
// (FUN_10090c70), field==2 -> the "Charge" mode (FUN_10090ac0, 10
// ammo/shot, confirmed exactly matching the manual), field==3 -> a
// single precision shot (FUN_10090940, 1 ammo/shot), any other value
// (the field's default/initial state, param_1[0x2f]=0 in Spawn) ->
// "Rapid" automatic fire through the shared, project-wide generic
// fire helper FUN_100912f0 with a 3-shot burst counter. This project
// has no per-mapper or per-map keyvalue confirmed for selecting
// between these internal field values (the manual's "Sniper Kit"/
// "Fire Type" upgrade items - cust_2GaussPistolSniper/
// cust_1GaussPistolFireType, see the findings doc's Session-37/38
// note - are the in-game mechanism, not reimplemented here; no gap
// scan required them). Simplified relative to the original: only two
// of the four confirmed modes are wired to the two available
// CBasePlayerWeapon attack inputs - PrimaryAttack reproduces the
// confirmed single-shot mode (FUN_10090940: 1 ammo, "gauss_fire1.wav"),
// SecondaryAttack reproduces the confirmed sniper-scope toggle mode
// (FUN_10090c70: gated on >=20 ammo to enter, "gsnipe_zoom.wav" on
// enter / "sniperunzoom.wav" on exit, toggles the player's FOV -
// same m_iFOV toggle pattern already used unmodified in dlls/
// crossbow.cpp, only used here as a reference). The "Charge" (10
// ammo/shot) and "Rapid" (3-shot burst) modes are NOT reachable
// through a control input here - their confirmed ammo costs and
// sounds are documented above for a future pass that adds a
// mode-cycle input or reproduces the Fire-Type upgrade item.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "dbg_log.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "gamerules.h"
#include "soundent.h"
#include "skill.h"
#include "decals.h"
#include "fastshot.h"
#include "UserMessages.h"
#include "nullent.h"

//=========================================================
// Neufassung 2026-10-31 (Dekompilat-Stand: findings/weapons/gausspistol_decode_2026-10-31.md,
// Port: findings/weapons/gausspistol_port_2026-10-31.md). Alle vier Modi sind nach Retail nachgebaut:
// Rapid (Standard, Ereignis events/gauss.sc + Treffer), Praezision (Modus 3, CFastShot), Charge (Modus 2, "gauss_bolt"),
// Sniper (Modus 4, Zoom + Schuss beim Loslassen, Ereignis events/snipershot.sc).
// Vereinfachungen: Die Anpassungs-Items cust_1GaussPistolFireType/cust_2GaussPistolSniper fehlen, der Modus wird nur ueber
// "cust_<Option><Wert>" gesetzt (Wert 4 = Sniper nur mit vorhandenem Sniper-Item, siehe ApplyCustomSetting); Schaden des Sniper-Schusses
// ist im Dekompilat verdeckt (hier sk_gaussslow_d geladen / sk_gaussshot_d normal). Die Folge-Entity 0x1008e8f0
// (Glow, CNullEnt::Create) und das Zoom-Overlay (CHudZoom im Client) sind nachgebaut.
//=========================================================

// Retail-Moduswerte (+0xb4)
enum
{
	GP_MODE_RAPID = 0,
	GP_MODE_CHARGE = 2,
	GP_MODE_PRECISION = 3,
	GP_MODE_SNIPER = 4,
};

enum gausspistol_e
{
	GP_IDLE = 0,
	GP_IDLE2,
	GP_UNUSED2,
	GP_FIRE_PRECISION,
	GP_FIRE_CHARGE,
	GP_DRAW,
	GP_TO_SNIPER,
	GP_SNIPER_IDLE,
	GP_SNIPER_IDLE2,
	GP_SNIPER_IDLE3,
	GP_SNIPER_FIRE,
	GP_SNIPER_DRAW,
	GP_FROM_SNIPER,
};

#define GAUSSPISTOL_SCOPE_FOV 20
#define GAUSSPISTOL_SCOPE_MIN_AMMO 20

//=========================================================
// gauss_bolt - CBoltShot (Ladekugel der Charge-Stufe). Retail: Spawn @0x1008f2a0, Precache @0x1008f750,
// ThrowSparks @0x1008f430, BoltTouch @0x1008f570, Erzeuger @0x1008f130. Der Schaden kommt aus sk_gaussslow_d
// (0x10136fc8).
//=========================================================
class CBoltShot : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	void EXPORT BoltTouch(CBaseEntity* pOther);
	void EXPORT ThrowSparks();

	static CBoltShot* BoltCreate(const Vector& vecOrigin, const Vector& vecAngles, entvars_t* pevOwner);

private:
	int m_iTrail = 0;
	int m_iSparkSprite = 0;
	unsigned short m_usImpact = 0;
};
LINK_ENTITY_TO_CLASS(gauss_bolt, CBoltShot);

void CBoltShot::Precache()
{
	PRECACHE_MODEL("sprites/gausswomp.spr");
	PRECACHE_MODEL("sprites/smoke.spr");
	m_iTrail = PRECACHE_MODEL("sprites/gaussbeam2.spr");
	m_iSparkSprite = PRECACHE_MODEL("sprites/gausspark.spr");
	UTIL_PrecacheOther("dmlRocket");
	m_usImpact = PRECACHE_EVENT(1, "events/ChargedImpact.sc");
}

void CBoltShot::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING("gauss_bolt");
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	SET_MODEL(ENT(pev), "sprites/gausswomp.spr");
	SetTouch(&CBoltShot::BoltTouch);
	SetThink(&CBoltShot::ThrowSparks);
	pev->frame = 0;
	pev->scale = 1.0f;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(m_iTrail);
	WRITE_BYTE(4);
	WRITE_BYTE(5);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	MESSAGE_END();

	pev->nextthink = gpGlobals->time + 0.1;
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
}

// Retail @0x1008f130
CBoltShot* CBoltShot::BoltCreate(const Vector& vecOrigin, const Vector& vecAngles, entvars_t* pevOwner)
{
	CBoltShot* pBolt = GetClassPtr((CBoltShot*)nullptr);
	pBolt->Spawn();

	UTIL_MakeVectors(vecAngles);
	UTIL_SetOrigin(pBolt->pev, vecOrigin + gpGlobals->v_right * 5.0f);
	pBolt->pev->angles = vecAngles;
	pBolt->pev->velocity = gpGlobals->v_forward * 1450.0f;
	pBolt->pev->owner = ENT(pevOwner);
	return pBolt;
}

// Retail @0x1008f430
void CBoltShot::ThrowSparks()
{
	if (!g_pGameRules->IsMultiplayer())
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SPRITETRAIL);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(pev->origin.x + gpGlobals->v_forward.x);
		WRITE_COORD(pev->origin.y + gpGlobals->v_forward.y);
		WRITE_COORD(pev->origin.z + gpGlobals->v_forward.z);
		WRITE_SHORT(m_iSparkSprite);
		WRITE_BYTE(3);
		WRITE_BYTE(10);
		WRITE_BYTE(RANDOM_LONG(1, 2));
		WRITE_BYTE(10);
		WRITE_BYTE(20);
		MESSAGE_END();
	}

	pev->frame += 1.0f;
	if (pev->frame > 7.0f)
		pev->frame = 0;

	pev->nextthink = gpGlobals->time + 0.1;
}

// Retail @0x1008f570
void CBoltShot::BoltTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner || pev->owner == pOther->pev->owner)
		return;

	entvars_t* pevOwner = VARS(pev->owner);
	if (pOther->pev->takedamage == DAMAGE_NO)
	{
		::RadiusDamage(pev->origin, pev, pevOwner, gSkillData.gaussSlowDamage * 0.05875f, gSkillData.gaussSlowDamage * 0.05875f, CLASS_NONE, DMG_BULLET | DMG_NEVERGIB);
	}
	else
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		ClearMultiDamage();
		pOther->TraceAttack(pevOwner, gSkillData.gaussSlowDamage, pev->velocity.Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB);
		ApplyMultiDamage(pev, pevOwner);
		pev->velocity = g_vecZero;
	}

	PLAYBACK_EVENT_FULL(0, edict(), m_usImpact, 0, pev->origin, pev->angles, 0, 0, 0, 0, 0, 0);
	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 1000, 2.0f);
	UTIL_Remove(this);
}

//=========================================================
// weapon_gausspistol (Alias weapon_pistol) - CGaussPistol. Retail: Spawn @0x100901f0, Precache @0x10090270, AddToPlayer
// @0x100903d0, GetItemInfo @0x10090480, Deploy @0x100905f0, Holster @0x10090640, Einstellung @0x10090500,
// SecondaryAttack @0x10090700, PrimaryAttack @0x100907a0, Praezision @0x10090940, Charge @0x10090ac0, Zoom @0x10090c70,
// Sniper-Schuss @0x10090e00, Rapid @0x100912f0/@0x10091410, WeaponIdle @0x100916c0.
//=========================================================
class CGaussPistol : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	bool Deploy() override;
	void Holster() override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void WeaponIdle() override;
	bool ShouldWeaponIdle() override { return true; }
	void ApplyCustomSetting(char option, int value) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	void FireRapid();
	void FirePrecision();
	void FireCharge();
	void ZoomHold();
	void ZoomCancel();
	void SniperShot();
	void SendZoom(int iState);
	void SendSlider();

	int m_iFireMode = GP_MODE_RAPID; // Retail +0xb4
	int m_iBurst = 0;				 // +0xbc
	int m_iSlider = 1;				 // +0xc0
	float m_flSniperSwitch = 0;		 // +0xc4
	float m_flZoomStart = 0;		 // +0xc8
	int m_fZoomed = 0;				 // +0xcc
	int m_fCharged = 0;				 // +0xd0
	int m_fAnimStarted = 0;			 // +0xd4
	int m_fHaveSniper = 0;			 // +0xd8
	int m_fHolstered = 0;			 // +0xdc
	unsigned short m_usGauss = 0;	 // Retail events/gauss.sc
	unsigned short m_usSniper = 0;	 // +0xe0, events/snipershot.sc
};
LINK_ENTITY_TO_CLASS(weapon_gausspistol, CGaussPistol);
LINK_ENTITY_TO_CLASS(weapon_pistol, CGaussPistol);

TYPEDESCRIPTION CGaussPistol::m_SaveData[] =
	{
		DEFINE_FIELD(CGaussPistol, m_iFireMode, FIELD_INTEGER),
		DEFINE_FIELD(CGaussPistol, m_iSlider, FIELD_INTEGER),
		DEFINE_FIELD(CGaussPistol, m_fHaveSniper, FIELD_INTEGER),
		DEFINE_FIELD(CGaussPistol, m_fHolstered, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CGaussPistol, CBasePlayerWeapon);

void CGaussPistol::Spawn()
{
	pev->classname = MAKE_STRING("weapon_gausspistol");
	Precache();
	m_iId = WEAPON_GAUSSPISTOL;
	SET_MODEL(ENT(pev), "models/w_gauss.mdl");

	m_iBurst = 0;
	m_iDefaultAmmo = 35;
	FallInit();
	m_iSlider = 1;
	m_fHolstered = 0;
}

void CGaussPistol::Precache()
{
	PRECACHE_MODEL("sprites/gaussbig.spr");
	PRECACHE_MODEL("sprites/gausspark.spr");
	PRECACHE_MODEL("sprites/gaussbeam2.spr");
	PRECACHE_MODEL("sprites/gaussfade.spr");
	PRECACHE_MODEL("sprites/gausspoof.spr");
	PRECACHE_MODEL("sprites/gausspuff.spr");
	PRECACHE_MODEL("models/w_gauss.mdl");
	PRECACHE_MODEL("models/v_guasspistol.mdl"); // Original-Tippfehler
	PRECACHE_MODEL("models/p_357.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");
	UTIL_PrecacheOther("gauss_charged");
	UTIL_PrecacheOther("gauss_bolt");
	PRECACHE_SOUND("weapons/gauss_charge.wav");
	PRECACHE_SOUND("weapons/gauss_fire1.wav");
	PRECACHE_SOUND("weapons/gauss_fire2.wav");
	PRECACHE_SOUND("weapons/gauss_fire3.wav");
	PRECACHE_SOUND("weapons/gauss_fire4.wav");
	PRECACHE_MODEL("sprites/white.spr");
	PRECACHE_SOUND("weapons/gsnipe_zoom.wav");
	PRECACHE_SOUND("weapons/sniperzoom.wav");
	PRECACHE_SOUND("weapons/sniperunzoom.wav");
	m_usGauss = PRECACHE_EVENT(1, "events/gauss.sc");
	m_usSniper = PRECACHE_EVENT(1, "events/snipershot.sc");
}

bool CGaussPistol::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_GAUSSPISTOL;
	p->iFlags = 0;
	p->iWeight = -1;

	return true;
}

void CGaussPistol::SendSlider()
{
	MESSAGE_BEGIN(MSG_ONE, gmsgSliderX, nullptr, m_pPlayer->pev);
	WRITE_BYTE(0x1f);
	WRITE_BYTE(m_iSlider);
	MESSAGE_END();
}

void CGaussPistol::SendZoom(int iState)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgZoom, nullptr, m_pPlayer->pev);
	WRITE_BYTE(iState);
	MESSAGE_END();
	// Das Sichtfeld und das Scope-Overlay setzt der Client (CHudZoom, cl_dll/hud_zoom.cpp), wie im Retail.
}

// Retail @0x100903d0
void CGaussPistol::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	SendSlider();
	pPlayer->GiveNamedItem("cust_1GaussPistolFireType");
}

// Retail @0x100905f0
bool CGaussPistol::Deploy()
{
	m_fHolstered = 0;
	return DefaultDeploy("models/v_guasspistol.mdl", "models/p_357.mdl", pev->body != 0 ? GP_SNIPER_DRAW : GP_DRAW, "python", pev->body);
}

// Retail @0x10090640
void CGaussPistol::Holster()
{
	m_fHolstered = 1;
	m_pPlayer->m_flNextAttack = gpGlobals->time + 2.0f;
	m_iBurst = 0;
	SendZoom(0);
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, 0.8, 0, 100);
	m_fZoomed = 0;
	m_fCharged = 0;
	m_flZoomStart = 0;
	CBasePlayerWeapon::Holster();
}

// Retail @0x10090500 (Vtable-Slot 68): Modus setzen; Sniper (4) nur mit dem Item cust_2GaussPistolSniper.
void CGaussPistol::ApplyCustomSetting(char option, int value)
{
	if (value == 4 && !m_fHaveSniper)
	{
		for (int i = 0; i < MAX_ITEM_TYPES && !m_fHaveSniper; i++)
		{
			for (CBasePlayerItem* pItem = m_pPlayer->m_rgpPlayerItems[i]; pItem; pItem = pItem->m_pNext)
			{
				if (FStrEq(STRING(pItem->pev->classname), "cust_2GaussPistolSniper"))
				{
					m_fHaveSniper = 1;
					break;
				}
			}
		}
		if (!m_fHaveSniper)
			return;
	}

	m_iFireMode = value;
	m_iSlider = value;
}

// Retail @0x10090700: Zoom abbrechen
void CGaussPistol::SecondaryAttack()
{
	if (m_iFireMode == GP_MODE_SNIPER)
		ZoomCancel();
}

void CGaussPistol::ZoomCancel()
{
	m_fZoomed = 0;
	m_pPlayer->m_flNextAttack = gpGlobals->time + 2.0f;
	SendZoom(0);
	m_fCharged = 0;
	m_flZoomStart = 0;
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, 0.8, 0, 100);
}

// Retail @0x100907a0
void CGaussPistol::PrimaryAttack()
{
	if (m_fHolstered)
		return;

	if (m_iFireMode == GP_MODE_SNIPER)
	{
		if (pev->body != 0)
		{
			ZoomHold();
			return;
		}
	}
	else if (pev->body != 1)
	{
		m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 600, 0.3f);

		if (m_iFireMode == GP_MODE_PRECISION)
		{
			FirePrecision();
			return;
		}
		if (m_iFireMode != GP_MODE_CHARGE)
		{
			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
			{
				m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
				FireRapid();
				m_flTimeWeaponIdle = gpGlobals->time + 1.0f;
				m_pPlayer->m_flNextAttack = gpGlobals->time + 0.15f;
				if (g_pGameRules->IsMultiplayer())
					m_pPlayer->m_flNextAttack = gpGlobals->time + 0.1f;

				if (++m_iBurst > 2)
				{
					m_iBurst = 0;
					m_pPlayer->m_flNextAttack = gpGlobals->time + 0.45f;
					if (g_pGameRules->IsMultiplayer())
						m_pPlayer->m_flNextAttack = gpGlobals->time + 0.25f;
				}
				m_pPlayer->pev->punchangle.x -= 1.0f;
				return;
			}
			PlayEmptySound();
			return;
		}
		FireCharge();
		return;
	}

	WeaponIdle();
}

// Retail @0x100912f0 + Treffer-Hilfe @0x10091410: Ereignis, dann Treffer mit sk_gaussshot_d (0x10136fc0).
void CGaussPistol::FireRapid()
{
	PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usGauss, 0, m_pPlayer->pev->origin, m_pPlayer->pev->angles, 0, 0, 1, 0, 0, 0);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	const Vector vecSrc = m_pPlayer->GetGunPosition();
	const Vector vecEnd = vecSrc + gpGlobals->v_forward * 8192;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	if (tr.fAllSolid == 0)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit)
		{
			ClearMultiDamage();
			pHit->TraceAttack(m_pPlayer->pev, gSkillData.gaussShotDamage, gpGlobals->v_forward, &tr, DMG_ENERGYBEAM);
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
		}
	}

	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
}

// Retail @0x10090940
void CGaussPistol::FirePrecision()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 0)
	{
		PlayEmptySound();
		return;
	}

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_flNextPrimaryAttack = gpGlobals->time + 0.1f;
	m_flTimeWeaponIdle = gpGlobals->time + 1.0f;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	const Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAim = m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	// Streuung im Scheibenverfahren (Faktor 0,06105), Offset 5 nach rechts, Geschwindigkeit 2000 (Retail @0x1008e9c0)
	float x, y, z;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		z = x * x + y * y;
	} while (z > 1);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	vecAim = vecAim + (y * 0.06105f) * gpGlobals->v_up + (x * 0.06105f) * gpGlobals->v_right;
	CFastShot::Create(m_pPlayer->pev, vecSrc + gpGlobals->v_right * 5.0f, vecAim * 2000.0f, gSkillData.gaussFastDamage);

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	SendWeaponAnim(GP_FIRE_PRECISION, pev->body);
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss_fire1.wav", 1.0, 0.8, 0, 100);
}

// Retail @0x10090ac0
void CGaussPistol::FireCharge()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 10)
	{
		PlayEmptySound();
		return;
	}

	SendWeaponAnim(GP_FIRE_CHARGE, pev->body);
	m_flNextPrimaryAttack = gpGlobals->time + 1.5f;
	m_flTimeWeaponIdle = gpGlobals->time + 2.0f;

	const Vector vecAngles = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors(vecAngles);
	CBoltShot::BoltCreate(m_pPlayer->GetGunPosition(), m_pPlayer->pev->v_angle, m_pPlayer->pev);

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 10;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->pev->punchangle.x -= 10.0f;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss_fire2.wav", 1.0, 0.8, 0, 100);
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/gauss_charge.wav", 1.0, 0.8, 0, 100);
}

// Retail @0x10090c70: Zoom halten; ab 0,5 s ist der Schuss geladen. Schuss beim Loslassen (WeaponIdle).
void CGaussPistol::ZoomHold()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < GAUSSPISTOL_SCOPE_MIN_AMMO)
	{
		PlayEmptySound();
		return;
	}

	const bool bFirst = m_flZoomStart == 0;
	m_fZoomed = 1;
	if (bFirst)
	{
		m_flZoomStart = gpGlobals->time;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gsnipe_zoom.wav", 1.0, 0.8, 0, 100);
		SendZoom(1);
	}

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.01f;

	if ((m_pPlayer->pev->button & IN_JUMP) != 0)
	{
		m_fZoomed = 0;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 3.0f;
		SendZoom(0);
		m_fCharged = 0;
		m_flZoomStart = 0;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, 0.8, 0, 100);
		return;
	}

	if (0.5f < gpGlobals->time - m_flZoomStart && m_flZoomStart != 0)
		m_fCharged = 1;
}

// Retail @0x10090e00
void CGaussPistol::SniperShot()
{
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	const Vector vecSrc = m_pPlayer->GetGunPosition();
	const Vector vecEnd = vecSrc + gpGlobals->v_forward * 8192;

	SendWeaponAnim(GP_SNIPER_FIRE, pev->body);
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	if (!m_fCharged)
	{
		SendZoom(0);
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 8;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/sniperunzoom.wav", 1.0, 0.8, 0, 100);
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.65f;
	}
	else
	{
		SendZoom(3);
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 20;
		float flPitch = (gpGlobals->time - m_flZoomStart) * 100.0f;
		if (flPitch > 133.0f)
			flPitch = 133.0f;
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.8f;
		m_flZoomStart = gpGlobals->time + 0.5f;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/sniperzoom.wav", 1.0, 0.8, 0, (int)flPitch);
	}

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	if (tr.fAllSolid == 0)
	{
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, tr.vecEndPos, 600, 0.3f);
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit)
		{
			if (pHit->pev->takedamage == DAMAGE_NO)
			{
				UTIL_DecalTrace(&tr, DECAL_GUNSHOT1 + RANDOM_LONG(0, 4));
				if (g_pGameRules->IsMultiplayer())
					::RadiusDamage(tr.vecEndPos, m_pPlayer->pev, m_pPlayer->pev, gSkillData.gaussSlowDamage * 0.5f, 128, CLASS_NONE, DMG_BULLET | DMG_NEVERGIB);
			}
			else
			{
				ClearMultiDamage();
				pHit->TraceAttack(m_pPlayer->pev, m_fCharged ? gSkillData.gaussSlowDamage : gSkillData.gaussShotDamage, gpGlobals->v_forward, &tr, DMG_BULLET | DMG_NEVERGIB);
				ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
			}
			m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
			PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usSniper, 0, vecSrc, m_pPlayer->pev->angles, 0, 0, 1, 0, m_fCharged, 0);
		}

		// Retail @0x1008e8f0: lingering laser from the muzzle (gun position + up*-1 + forward*11 + right*1) to the impact
		const Vector vecMuzzle = vecSrc + gpGlobals->v_up * -1.0f + gpGlobals->v_forward * 11.0f + gpGlobals->v_right * 1.0f;
		CNullEnt* pGlow = CNullEnt::Create(vecMuzzle, tr.vecEndPos, m_pPlayer->pev, m_fCharged != 0);
		pGlow->m_bStartLaser = true;
		if (pHit && pHit->pev->takedamage == DAMAGE_NO)
			pGlow->m_bHitWorld = true;
	}
	m_fCharged = 0;
}

// Retail @0x100916c0
void CGaussPistol::WeaponIdle()
{
	if (!m_fAnimStarted)
	{
		SendWeaponAnim(pev->body == 0 ? GP_IDLE : GP_SNIPER_IDLE, pev->body);
		m_fAnimStarted = 1;
	}

	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_fZoomed)
	{
		SniperShot();
		m_fZoomed = 0;
	}
	m_flZoomStart = 0;
	ResetEmptySound();
	m_iBurst = 0;

	if (m_iFireMode == GP_MODE_SNIPER)
	{
		if (pev->body == 0)
		{
			m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0f;
			m_flTimeWeaponIdle = gpGlobals->time + 2.0f;
			if (m_flSniperSwitch != 0)
			{
				if (gpGlobals->time <= m_flSniperSwitch)
					return;
				m_flSniperSwitch = 0;
				pev->body = 1;
				SendWeaponAnim(GP_SNIPER_DRAW, pev->body);
				m_flTimeWeaponIdle = gpGlobals->time;
				return;
			}
			SendWeaponAnim(GP_TO_SNIPER, pev->body);
			m_flSniperSwitch = gpGlobals->time + 0.25f;
			return;
		}
	}
	else if (pev->body == 1)
	{
		m_pPlayer->m_flNextAttack = gpGlobals->time + 2.0f;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5f;
		if (m_flSniperSwitch != 0)
		{
			if (gpGlobals->time <= m_flSniperSwitch)
				return;
			m_flSniperSwitch = 0;
			pev->body = 0;
			SendWeaponAnim(GP_DRAW, pev->body);
			m_flTimeWeaponIdle = gpGlobals->time + 0.25f;
			m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0f;
			return;
		}
		SendWeaponAnim(GP_FROM_SNIPER, pev->body);
		m_flSniperSwitch = gpGlobals->time + 0.25f;
		return;
	}

	if (m_flTimeWeaponIdle <= gpGlobals->time)
	{
		if (RANDOM_LONG(0, 99) < 96)
		{
			if (RANDOM_LONG(0, 99) >= 75)
				SendWeaponAnim(m_iFireMode == GP_MODE_SNIPER ? GP_SNIPER_IDLE2 : GP_IDLE2, pev->body);
			else
				SendWeaponAnim(m_iFireMode == GP_MODE_SNIPER ? GP_SNIPER_IDLE : GP_IDLE, pev->body);
			m_flTimeWeaponIdle = RANDOM_FLOAT(3, 10) + gpGlobals->time;
			return;
		}
		SendWeaponAnim(m_iFireMode == GP_MODE_SNIPER ? GP_SNIPER_IDLE3 : GP_IDLE, pev->body);
		m_flTimeWeaponIdle = gpGlobals->time + 3.0f;
	}
}


// Munitionsentitaet ammo_gaussclip (Map-Entity). Die Klasse stammt aus gauss.cpp, das nicht im hl-Target liegt;
// ohne diese Registrierung hatte die Karte REBAR2A "No spawn function" fuer ammo_gaussclip (Smoke-Test 2026-11-02).
class CGaussAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/guassammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PrecacheModel("models/guassammo.mdl");
		PrecacheSound("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_gaussclip, CGaussAmmo);
