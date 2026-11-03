//=========================================================
// weapon_minigun (Alias: weapon_mechagun) - CMinigun. Third of the
// six points deferred during the map-driven pass (see
// findings/weapons/weapon_minigun.md and
// [[project-weapon-minigun-deferred]] memory) to be picked up.
//
// Decompiled fresh from gunman.dll this session: LINK-equivalent
// constructor sets classname "weapon_minigun" always (even spawned as
// "weapon_mechagun"), vtable @0x100fd30c, Spawn @0x10094c70, Precache
// @0x10094d30. Confirms findings/weapons/weapon_minigun.md exactly:
// weapon ID 0x1c(28), world model "models/w_mechagun.mdl", ammo type
// "9mm" (generic, shared with weapon_mp5/weapon_glock - confirmed via
// the Session-72 GetItemInfo decompile), iMaxClip=200, iWeight=-10.
// Precache confirms model "models/p_9mmAR.mdl" (stock Half-Life's MP5
// playermodel, reused unchanged - dlls/mp5.cpp is left completely
// untouched, only used here as a reference for standard hitscan
// weapon structure), muzzle-flash sprite "sprites/muzlblue.spr", shell
// casing model "models/shell.mdl", three machine-gun fire sounds
// (hks1/2/3.wav, the same "hks" sounds already reused for several
// monster_human_*/CChopper attacks elsewhere in this project), and
// the two confirmed spin sounds "weapons/MechaSpinUp.wav"/
// "weapons/MechaSpinDown.wav". Two network events registered:
// "events/gw_minigun.sc" and "events/gw_minigunHeat.sc" (the latter
// shared with weapon_beamgun, per the findings doc - not literally
// shared code between the two weapons here, just the same event
// name/heat-bar HUD convention).
//
// Spawn's `DAT_101322d8` check is CGameRules::IsMultiplayer(): in the
// Retail multiplayer path it sets a pev flag and an internal byte. The
// exact downstream use of those two writes remains unresolved, so they
// are not reproduced speculatively.
//
// CORRECTION (2026-09-05, later same-day pass): the initial version of
// this file guessed a 1-second "spin-up before firing" gate. Fresh
// decompile of the real PrimaryAttack (0x10094f80, address was known
// but unread at the time - findings/weapons/primaryattack_
// breakthrough.md flagged it as "not yet decompiled") shows there is
// NO windup delay - every call fires immediately. Confirmed instead:
//   - Normal mode fires 1 pellet per call at a confirmed 0.02618 rad
//     spread (matches this project's VECTOR_CONE_3DEGREES exactly),
//     0.2s between shots, heat +1 per shot.
//   - Retail has a runtime customization toggle in vtable slot 68
//     (ApplyCustomSetting), separate from SecondaryAttack. Its transition
//     state machine is documented in findings/weapons/weapon_minigun.md;
//     it is not implemented here yet. The resulting Barrel Spin mode fires
//     2 pellets per call at 0.08716 rad spread
//     (0.05234 in multiplayer), 0.1s between shots, same 1-ammo cost
//     per call despite the double pellet count. The `cust_1MinigunSpin`
//     inventory item enables the customization UI; its exact client-side
//     selection command and the server transition path are still open.
//   - Heat counter caps at 100 and forces an overheat lockout (state
//     field transitions match a "must cool down" branch) until it
//     drops back below threshold - reproduced with the same shape,
//     cool-down rate approximated (not decompiled to the exact
//     value).
// The precached MechaSpinUp.wav/MechaSpinDown.wav sounds are real but
// their exact trigger site was not conclusively located in
// PrimaryAttack itself - kept here as cosmetic transition sounds
// (played once when firing resumes after an idle period / once when
// forced into overheat lockout) rather than a per-shot windup gate.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "saverestore.h"
#include "effects.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "dbg_log.h"

#define MINIGUN_HEAT_PER_SHOT 1.0 // confirmed (normal mode; Barrel Spin mode's 2/shot not reproduced, see file header
#define MINIGUN_HEAT_MAX 100.0	  // confirmed
#define MINIGUN_HEAT_COOL_RATE 25.0 // per second while not firing - rate approximated, see file header
#define MINIGUN_FIRE_DELAY 0.2	  // confirmed

class CMinigun : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool Deploy() override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	void Holster() override;

	void PrimaryAttack() override;
	void WeaponIdle() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	bool ShouldWeaponIdle() override { return true; }

private:
	void SpinDown();
	void AmmoHudUpdate();

	bool m_bSpinning = false;
	float m_flHeat = 0;
	int m_iLastHudHeat = -1;
};
LINK_ENTITY_TO_CLASS(weapon_minigun, CMinigun);
LINK_ENTITY_TO_CLASS(weapon_mechagun, CMinigun);

TYPEDESCRIPTION CMinigun::m_SaveData[] =
{
	DEFINE_FIELD(CMinigun, m_bSpinning, FIELD_BOOLEAN),
	DEFINE_FIELD(CMinigun, m_flHeat, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CMinigun, CBasePlayerWeapon);

void CMinigun::Spawn()
{
	pev->classname = MAKE_STRING("weapon_minigun");
	Precache();
	m_iId = WEAPON_MINIGUN;
	SET_MODEL(ENT(pev), "models/w_mechagun.mdl");

	FallInit();
}

void CMinigun::Precache()
{
	PrecacheModel("models/v_mechagun.mdl");
	PrecacheModel("models/w_mechagun.mdl");
	PrecacheModel("models/p_9mmAR.mdl");
	PrecacheModel("models/shell.mdl");
	PrecacheModel("sprites/muzlblue.spr");
	PrecacheSound("items/9mmclip1.wav");
	PrecacheSound("weapons/electro4.wav");
	PrecacheSound("weapons/hks1.wav");
	PrecacheSound("weapons/hks2.wav");
	PrecacheSound("weapons/hks3.wav");
	PrecacheSound("weapons/MechaSpinUp.wav");
	PrecacheSound("weapons/MechaSpinDown.wav");
	PrecacheEvent(1, "events/gw_minigun.sc");
	PrecacheEvent(1, "events/gw_minigunHeat.sc");
}

bool CMinigun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 200;
	p->iSlot = 4;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_MINIGUN;
	p->iFlags = 0;
	p->iWeight = -10;

	return true;
}

bool CMinigun::Deploy()
{
	m_bSpinning = false;
	m_flHeat = 0;
	m_iClip = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]; // Anzeige: Pool als Magazinwert
	return DefaultDeploy("models/v_mechagun.mdl", "models/p_9mmAR.mdl", 0, "mp5");
}

void CMinigun::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SpinDown();
}

void CMinigun::SpinDown()
{
	if (m_bSpinning)
	{
		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/MechaSpinDown.wav", 1.0, ATTN_NORM, 0, 100);
		// Retail: Ausklang-Sequenz "spindown" (Index 5, v_mechagun.mdl; Sendung im Original belegt).
		SendWeaponAnim(5);
		m_bSpinning = false;
	}
}

void CMinigun::AmmoHudUpdate()
{
	// TempX überträgt Waffen-ID und Hitze. Nur geänderte ganzzahlige Werte
	// senden; Retail aktualisiert den Wert ebenfalls nur bei Änderungen.
	const int iHeat = (int)m_flHeat;
	if (iHeat == m_iLastHudHeat)
		return;
	m_iLastHudHeat = iHeat;

	MESSAGE_BEGIN(MSG_ONE, gmsgTempX, nullptr, m_pPlayer->pev);
	WRITE_BYTE(m_iId);
	WRITE_BYTE(iHeat);
	MESSAGE_END();
}

void CMinigun::PrimaryAttack()
{
	// Retail (Befund 2026-11-01, weapon_minigun.md Nachtrag 2/5): Munition kommt aus dem Spielerpool, nicht aus einem Magazin.
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.2;
		SpinDown();
		return;
	}

	if (m_flHeat >= MINIGUN_HEAT_MAX)
	{
		SpinDown();
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.5;
		return;
	}

	if (!m_bSpinning)
	{
		m_bSpinning = true;
		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/MechaSpinUp.wav", 1.0, ATTN_NORM, 0, 100);
	}

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_iClip = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]; // Entscheidung 2026-11-01: Magazinanzeige zeigt den Pool (HUD liest m_iClip, siehe weapon_minigun.md Nachtrag 7)
	m_flHeat = V_min(m_flHeat + MINIGUN_HEAT_PER_SHOT, MINIGUN_HEAT_MAX);
	AmmoHudUpdate();

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	FireBullets(1, vecSrc, vecAiming, VECTOR_CONE_3DEGREES, 2048, BULLET_PLAYER_9MM);

	const char* fireSounds[] = {"weapons/hks1.wav", "weapons/hks2.wav", "weapons/hks3.wav"};
	EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, fireSounds[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM, 0, RANDOM_LONG(94, 106));

	pev->effects |= EF_MUZZLEFLASH;
	SendWeaponAnim(0);

	m_pPlayer->pev->punchangle.x -= 1;

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + MINIGUN_FIRE_DELAY;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + MINIGUN_FIRE_DELAY;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		SpinDown();
}

void CMinigun::WeaponIdle()
{
	// Abkuehlung nur bei losgelassenem Feuerknopf und zeitabhaengig (Frametime). Vorher: fester Abzug von 2,5 je
	// Aufruf, also je Frame; dadurch blieb die Hitze bei jedem Schuss bei 0 (Nutzerbericht, TEMP PA-Log: heat=0.0).
	if (m_flHeat > 0 && (m_pPlayer->pev->button & IN_ATTACK) == 0)
		m_flHeat = V_max(m_flHeat - MINIGUN_HEAT_COOL_RATE * gpGlobals->frametime, 0.0f);
	AmmoHudUpdate();

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	SpinDown();
	SendWeaponAnim(0);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
}


//=========================================================
// ammo_mechgunClip / ammo_minigunClip - Retail teilt sich eine Klasse
// (Vtable 0x100fd788): Modell "models/mechammo.mdl", AddAmmo gibt 20 "9mm"
// (Maximum 200), Aufnahme-Sound "items/9mmclip1.wav".
// Quelle: Frischdekompilat Spawn @0x10095e90, Precache @0x10095ec0,
// AddAmmo @0x10095ee0 (2026-10-31). In Phase 1 fehlte die Klasse im SDK.
//=========================================================
class CMechgunAmmo : public CBasePlayerAmmo
{
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/mechammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override
	{
		PrecacheModel("models/mechammo.mdl");
		PrecacheSound("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(20, "9mm", 200) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_mechgunClip, CMechgunAmmo);
LINK_ENTITY_TO_CLASS(ammo_minigunClip, CMechgunAmmo);

// Retail: beim Aufnehmen die Anpassungs-Items verteilen (customize_items.cpp)
void CMinigun::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	pPlayer->GiveNamedItem("cust_1MinigunSpin");
}
