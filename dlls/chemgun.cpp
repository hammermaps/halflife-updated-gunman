//=========================================================
// weapon_chemgun (Alias: weapon_SPchemicalgun) - CChemGun.
//
// Port aus dem frisch dekompilierten gunman.dll (2026-10-31): Vtable 0x100fddc4,
// Spawn @0x10097070, Precache @0x10097160, AddToPlayer @0x10097360, GetItemInfo @0x100971d0,
// Deploy @0x100974f0, PrimaryAttack @0x10097530, Einstellung (Vtable-Slot 68) @0x10097a60,
// CanDeploy @0x10097c70, WeaponIdle @0x10097cc0, Erzeuger-Auswahl @0x10097d40,
// Mischklasse @0x10097ea0, Schaden @0x10097fa0, Aufprallwert @0x10098110, Chemieart @0x10098170.
// Details, Konstanten und Vereinfachungen: findings/weapons/chemgun_port_2026-10-31.md
//
// Die Waffe verschiesst je nach Mischung (Saeure/Base/Neutralisierer, Druck 1-5) eine "chem_grenade"
// (CChemGrenade): Mischklasse 6 haftet (ShootSticky), 7 springt (ShootBouncing), 8 platzt beim ersten
// Treffer (ShootContact).
//
// Vereinfachungen / offene Punkte:
// - Die Retail-Oberflaeche (Regler im HUD) ist nicht nachgebaut; die Einstellung laeuft ueber den
//   client command "cust_<Option><Wert>" (Retail FUN_100405b0), die Reglerwerte gehen als "SliderX".
// - Die Anpassungs-Items (cust_1SPChemicalGunPressure ... cust_4ChemNuet) kommen aus customize_items.cpp.
// - iMaxAmmo1 liest Retail aus dem Skill-Global 0x10136f6c (zur Laufzeit gesetzt, statisch nicht lesbar);
//   hier 50 (Kapazitaet von ammo_chemical).
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "gamerules.h"
#include "soundent.h"
#include "chemgrenade.h"
#include "UserMessages.h"

class CChemGun : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	void AddToPlayer(CBasePlayer* pPlayer) override;
	bool Deploy() override;

	void PrimaryAttack() override;
	void WeaponIdle() override;
	bool ShouldWeaponIdle() override { return true; }
	void ApplyCustomSetting(char option, int value) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	void SendSliders();
	void ClassifyMix();
	void CalcDamage();
	void CalcBounce();
	void CalcType();
	void FireChemical(entvars_t* pevOwner, const Vector& vecSrc, const Vector& vecVelocity);

	int m_iPressure = 3;		   // Retail +0xa0
	float m_flLastMixChange = 0;   // Retail +0xa8
	float m_flShotSpeed = 850;	   // Retail +0xac ("m_iVelocity" in der Save-Tabelle, Typ Float)
	int m_iSplashSprite = 0;	   // Retail +0xb0
	Vector m_vecSpread = Vector(0.04362f, 0.04362f, 0.04362f); // Retail +0xb4
	int m_iAcid = 4;			   // Retail +0xc0
	int m_iBase = 3;			   // Retail +0xc4
	int m_iNeut = 2;			   // Retail +0xc8
	int m_iMixScore = 0;		   // Retail +0xcc
	int m_iDamage = 0;			   // Retail +0xd0
	float m_flBounce = 0;		   // Retail +0xd4
	int m_iField2ac = 0;		   // Retail +0xd8
	int m_iType = 0;			   // Retail +0xdc
	int m_iMix = 0;				   // Retail +0xe0 (6 haftend, 7 springend, 8 Kontakt)
	int m_iField2b8 = 0;		   // Retail +0xe4
	float m_flNextIdle = 0;		   // Retail +0xe8
};
LINK_ENTITY_TO_CLASS(weapon_chemgun, CChemGun);
LINK_ENTITY_TO_CLASS(weapon_SPchemicalgun, CChemGun);

// Retail-Save-Tabelle 0x10128dd8 (7 Felder)
TYPEDESCRIPTION CChemGun::m_SaveData[] =
	{
		DEFINE_FIELD(CChemGun, m_flShotSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CChemGun, m_iAcid, FIELD_INTEGER),
		DEFINE_FIELD(CChemGun, m_iBase, FIELD_INTEGER),
		DEFINE_FIELD(CChemGun, m_iNeut, FIELD_INTEGER),
		DEFINE_FIELD(CChemGun, m_iPressure, FIELD_INTEGER),
		DEFINE_FIELD(CChemGun, m_flNextIdle, FIELD_TIME),
		DEFINE_FIELD(CChemGun, m_vecSpread, FIELD_VECTOR),
};

IMPLEMENT_SAVERESTORE(CChemGun, CBasePlayerWeapon);

enum chemgun_e
{
	CHEMGUN_DRAW = 0,
	CHEMGUN_HOLSTER,
	CHEMGUN_IDLE,
	CHEMGUN_FIDGET,
	CHEMGUN_CHANGEMIX,
	CHEMGUN_SHOOT,
};

// Reglernummern der "SliderX"-Nachricht (= Weapon-IDs der cust_*-Items)
enum
{
	SLIDER_ACID = 0x18,
	SLIDER_NEUT = 0x19,
	SLIDER_BASE = 0x1a,
	SLIDER_PRESSURE = 0x1b,
};

void CChemGun::Spawn()
{
	pev->classname = MAKE_STRING("weapon_SPchemicalgun");
	Precache();
	SET_MODEL(ENT(pev), "models/w_chemgun.mdl");

	m_iId = WEAPON_CHEMGUN;
	m_flShotSpeed = 850;
	m_vecSpread = Vector(0.04362f, 0.04362f, 0.04362f);
	m_iPressure = 3;
	m_iAcid = 4;
	m_iBase = 3;
	m_iNeut = 2;
	m_iField2b8 = 0;
	m_iMix = 0;
	m_iType = 0;
	m_iDamage = 0;
	m_iMixScore = 0;
	m_flBounce = 0;
	m_flLastMixChange = 0;
	m_iDefaultAmmo = 10;

	FallInit();
	m_flNextIdle = 0;
}

void CChemGun::Precache()
{
	PRECACHE_MODEL("models/v_chemgun.mdl");
	PRECACHE_MODEL("models/w_chemgun.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("weapons/cg-fire.wav");
	PRECACHE_SOUND("weapons/empty.wav");
	UTIL_PrecacheOther("chem_grenade");
	PRECACHE_MODEL("models/p_hgun.mdl");
	m_iSplashSprite = PRECACHE_MODEL("sprites/greensplash.spr");
}

bool CChemGun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "ChemicalAcid";
	p->iMaxAmmo1 = 50; // Retail: (int)Skill-Global 0x10136f6c
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 7;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_CHEMGUN;
	p->iFlags = 0;
	p->iWeight = -10;

	return true;
}

// Retail @0x10097250: vier Reglerwerte. Der Druck wird aus der Geschwindigkeit zurueckgerechnet.
void CChemGun::SendSliders()
{
	const int iPressure = (int)((m_flShotSpeed - 400.0f) * 0.0066666668f);
	const int rgSlider[4][2] = {
		{SLIDER_PRESSURE, iPressure},
		{SLIDER_ACID, m_iAcid + 1},
		{SLIDER_BASE, m_iBase + 1},
		{SLIDER_NEUT, m_iNeut + 1},
	};

	for (const auto& slider : rgSlider)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgSliderX, nullptr, m_pPlayer->pev);
		WRITE_BYTE(slider[0]);
		WRITE_BYTE(slider[1]);
		MESSAGE_END();
	}
}

// Retail @0x10097360
void CChemGun::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	SendSliders();
	pPlayer->GiveNamedItem("cust_1SPChemicalGunPressure");
	pPlayer->GiveNamedItem("cust_2ChemAcid");
	pPlayer->GiveNamedItem("cust_3ChemBase");
	pPlayer->GiveNamedItem("cust_4ChemNuet");
}

// Retail @0x100974f0
bool CChemGun::Deploy()
{
	m_flNextIdle = gpGlobals->time + RANDOM_FLOAT(2, 5);
	return DefaultDeploy("models/v_chemgun.mdl", "models/p_hgun.mdl", CHEMGUN_DRAW, "hive");
}

// Retail @0x10097ea0: Mischklasse aus dem Verhaeltnis der Anteile.
void CChemGun::ClassifyMix()
{
	if (m_iBase == 0 && m_iAcid == 0 && m_iNeut == 0)
	{
		m_iMixScore = 0;
		m_iMix = 7;
		return;
	}

	if ((m_iAcid != 0 || (m_iBase != 0 && m_iNeut != 0)) && (m_iBase != 0 || m_iNeut != 0))
	{
		if (m_iAcid == 0 || m_iBase == 0)
			m_iMixScore = 100;
		else
			m_iMixScore = abs(m_iAcid - m_iBase) * -13 + 69;

		m_iMixScore += m_iNeut * -13;

		if (m_iMixScore > 44)
			m_iMix = m_iMixScore < 71 ? 6 : 8;
		else
			m_iMix = 7;
		return;
	}

	m_iMix = 8;
	m_iMixScore = 100;
}

// Retail @0x10097fa0: Schaden aus den Anteilen (Reihenfolge der Schritte wie im Dekompilat).
void CChemGun::CalcDamage()
{
	m_iDamage = 0;
	if (m_iBase == 0 && m_iAcid == 0 && m_iNeut == 0)
		return;

	if ((m_iAcid == 0 && (m_iBase == 0 || m_iNeut == 0)) || (m_iBase == 0 && m_iNeut == 0))
		m_iDamage = (m_iNeut + m_iAcid + m_iBase) * 23;

	if (m_iAcid != 0)
	{
		if (m_iBase != 0)
		{
			m_iDamage = (m_iAcid + m_iBase) * 10;
			if (m_iAcid == m_iBase)
				m_iDamage = (int)((double)m_iDamage * 1.5);
			m_iDamage = (int)((double)(m_iDamage - m_iNeut * 2) * 1.15);
			return;
		}
		if (m_iNeut != 0)
			m_iDamage = m_iAcid * 25 - m_iNeut * 2;
	}

	if (m_iBase != 0 && m_iNeut != 0)
		m_iDamage = m_iBase * 25 - m_iNeut * 2;

	if (m_iMixScore < 69)
		m_iDamage = (int)(m_flShotSpeed * 0.04f + (float)m_iDamage);

	if (m_iDamage < 10)
		m_iDamage = 25;

	m_iDamage = (int)((double)m_iDamage * 1.1);
}

// Retail @0x10098110: Aufprallbudget (zugleich Zuendzeit-Basis der Erzeuger)
void CChemGun::CalcBounce()
{
	m_flBounce = 55.0f;
	if (m_iAcid != 0 && m_iBase != 0)
		m_flBounce = (float)(abs(m_iAcid - m_iBase) * 10);
	m_flBounce += (float)(m_iNeut * 20);
}

// Retail @0x10098170: Chemieart (1 Saeure, 2 Base, 3 Sprengsatz, 4 Mischsatz, 5 Neutralisierer)
void CChemGun::CalcType()
{
	if (m_iBase == 0)
	{
		if (m_iAcid == 0 && m_iNeut == 0)
			return;
		if (m_iNeut == 0)
			m_iType = CHEM_ACID;
	}
	else if (m_iAcid != 0)
	{
		m_iType = (abs(m_iAcid - m_iBase) != 3) ? CHEM_EXPLOSIVE : CHEM_MIXED;
		return;
	}

	if (m_iAcid == 0)
	{
		if (m_iNeut == 0)
			m_iType = CHEM_BASE;
		if (m_iBase == 0)
			m_iType = CHEM_NEUTRAL;
	}
	else if (m_iNeut != 0)
	{
		m_iType = m_iAcid < m_iNeut ? CHEM_NEUTRAL : CHEM_ACID;
	}

	if (m_iBase != 0 && m_iNeut != 0)
		m_iType = m_iBase < m_iNeut ? CHEM_BASE : CHEM_ACID;
}

// Retail @0x10097d40
void CChemGun::FireChemical(entvars_t* pevOwner, const Vector& vecSrc, const Vector& vecVelocity)
{
	ClassifyMix();
	CalcDamage();
	CalcBounce();
	CalcType();

	if (m_iBase == 0 && m_iAcid == 0 && m_iNeut == 0)
		return;

	if (g_pGameRules && g_pGameRules->IsMultiplayer())
		m_iDamage = (int)((double)m_iDamage * 0.85);
	else if (m_flBounce < 3.0f)
		m_flBounce = 3.0f;

	ChemParams params;
	params.iAcid = m_iAcid;
	params.iBase = m_iBase;
	params.field29c = m_iNeut;
	params.iViscosity = m_iMixScore;
	params.iDamage = m_iDamage;
	params.flBounce = m_flBounce;
	params.field2ac = m_iField2ac;
	params.iType = m_iType;
	params.iMix = m_iMix;
	params.field2b8 = m_iField2b8;

	if (m_iMix == 6)
		CChemGrenade::ShootSticky(pevOwner, vecSrc, vecVelocity, params);
	if (m_iMix == 7)
		CChemGrenade::ShootBouncing(pevOwner, vecSrc, vecVelocity, params);
	if (m_iMix == 8)
		CChemGrenade::ShootContact(pevOwner, vecSrc, vecVelocity, params);
}

// Retail @0x10097530
void CChemGun::PrimaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0)
	{
		CalcDamage();
		CalcType();

		// Kosten je Schussstaerke (Schaden > 49 / > 85), Sprengsatz +1, danach -1, mindestens 1; ohne Chemie 0.
		int iCost = 1;
		if (m_iDamage > 49)
			iCost = 2;
		if (m_iDamage > 85)
			iCost = 3;
		if (m_iType == CHEM_EXPLOSIVE)
			iCost++;
		iCost--;
		if (iCost < 1)
			iCost = 1;
		if (m_iBase == 0 && m_iAcid == 0 && m_iNeut == 0)
			iCost = 0;

		if (iCost <= m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
			m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
			m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
			m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;

			SendWeaponAnim(CHEMGUN_SHOOT, 0);
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);

			// ohne Chemie nur ein leeres Klicken (Retail: "Sicherheitsschuss")
			const bool bEmpty = m_iAcid == 0 && m_iBase == 0 && m_iNeut == 0;
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, bEmpty ? "weapons/empty.wav" : "weapons/cg-fire.wav", 0.8, 0.8, 0, 100);

			UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
			Vector vecAimDir = gpGlobals->v_forward;

			// Streuung (Scheibenverfahren wie FireBulletsPlayer), nur wenn die Streuung nicht null ist
			if (m_vecSpread.x != 0 || m_vecSpread.y != 0 || m_vecSpread.z != 0)
			{
				float x, y, z;
				do
				{
					x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
					y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
					z = x * x + y * y;
				} while (z > 1);

				vecAimDir = vecAimDir + y * m_vecSpread.y * gpGlobals->v_up + x * m_vecSpread.x * gpGlobals->v_right;
			}

			// Rueckstoss: waagerechte Geschwindigkeit des Spielers sinkt um (Druck * 100 - 100) in Blickrichtung
			const float flRecoil = (float)(m_iPressure * 100 - 100);
			const float flVelZ = m_pPlayer->pev->velocity.z;
			m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * flRecoil;
			m_pPlayer->pev->velocity.z = flVelZ;

			const Vector vecVelocity = vecAimDir * m_flShotSpeed + gpGlobals->v_up * 100.0f;
			const Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16.0f;

			FireChemical(m_pPlayer->pev, vecSrc, vecVelocity);

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= iCost;

			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.0f;
			m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
			m_flNextIdle = UTIL_WeaponTimeBase() + 5.0f;

			m_pPlayer->pev->punchangle.x -= 10.0f;
			return;
		}
	}

	PlayEmptySound();
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
}

// Retail @0x10097a60 (Vtable-Slot 68): Regler. '1' Druck, '2' Saeure, '3' Base, '4' Neutralisierer.
void CChemGun::ApplyCustomSetting(char option, int value)
{
	switch (option)
	{
	case '1':
		m_flShotSpeed = (float)(value * 150 + 400);
		switch (value)
		{
		case 1:
			m_vecSpread = g_vecZero;
			break;
		case 2:
			m_vecSpread = Vector(0.02618f, 0.02618f, 0.02618f);
			break;
		case 3:
			m_vecSpread = Vector(0.06105f, 0.06105f, 0.06105f);
			break;
		case 4:
			m_vecSpread = Vector(0.08716f, 0.08716f, 0.08716f);
			break;
		case 5:
			m_vecSpread = Vector(0.13053f, 0.13053f, 0.13053f);
			break;
		}
		m_iPressure = value;
		break;
	case '2':
		m_iAcid = value - 1;
		break;
	case '3':
		m_iBase = value - 1;
		break;
	case '4':
		m_iNeut = value - 1;
		break;
	}

	if (3.0f < gpGlobals->time - m_flLastMixChange)
	{
		m_flLastMixChange = gpGlobals->time;
		SendWeaponAnim(CHEMGUN_CHANGEMIX, 0);
		m_flNextIdle = gpGlobals->time + 4.0f;
	}
}

// Retail @0x10097cc0 (die Leerlaufzeit liegt in +0xe8, nicht in m_flTimeWeaponIdle)
void CChemGun::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_flNextIdle <= gpGlobals->time)
	{
		const int iRand = RANDOM_LONG(0, 4);
		SendWeaponAnim(iRand != 0 ? CHEMGUN_IDLE : CHEMGUN_FIDGET, 0);
		m_flNextIdle = RANDOM_FLOAT(3, 8) + gpGlobals->time;
	}
}
