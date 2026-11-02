//=========================================================
// Anpassungs-Items der Waffen ("cust_*"), ausser den DML-Items (dml_customization.cpp): permanente, stumme Inventar-
// Eintraege in Slot 9, die das Retail-Menue "Customize weapon" (Regler) freischalten. Eigene Klasse je Item im
// Retail (Wrapper-Exporte 0x100814a0 ... 0x10098370, je eine Vtable); Spawn und GetItemInfo lesen sich identisch:
// Spawn setzt die Weapon-ID, Modell models/w_crowbar.mdl (Sniper: w_snipercase.mdl, Minigun gekuehlt: w_coolers.mdl),
// Clip -1, FallInit; GetItemInfo liefert Slot 9, die Position unten, Gewicht -100, keine Munition.
// Quelle: Spawn/GetItemInfo je Klasse (z. B. Sniper 0x10091cf0/0x10091e10, Chemie 0x100983c0/0x10098500),
// findings/weapons/customize_items_2026-11-01.md
//
// Die Waffen verteilen ihre Items selbst beim Aufnehmen (GiveNamedItem in AddToPlayer); Sniper-Kit und gekuehlter
// Minigun-Lauf liegen als Pickups in den Karten.
//
// Hinweis: Weapon-ID 0x20 (Shotgun-Munition) liegt ueber dem 32-Bit-Waffenfeld (1 << 32 ergibt auf x86 Bit 0, wie im
// Retail), ID 0x1f (Pistolen-Feuerart) ist zugleich WEAPON_SUIT (wie im Retail).
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

namespace
{
struct CustItem
{
	const char* classname;
	int id;
	int position;
	const char* model;
};

const CustItem g_CustItems[] = {
	{"cust_1fistsKnife", 8, 14, "models/w_crowbar.mdl"},
	{"cust_1ShotgunSpread", 0x14, 1, "models/w_crowbar.mdl"},
	{"cust_2ShotgunShells", 0x20, 0, "models/w_crowbar.mdl"},
	{"cust_1MinigunSpin", 0x1d, 2, "models/w_crowbar.mdl"},
	{"cust_2MinigunCooled", 0x17, 7, "models/w_coolers.mdl"},
	{"cust_1GaussPistolFireType", 0x1f, 13, "models/w_crowbar.mdl"},
	{"cust_2GaussPistolSniper", 0x12, 19, "models/w_snipercase.mdl"},
	{"cust_1BeamFrequency", 2, 8, "models/w_crowbar.mdl"},
	{"cust_2BeamCurrent", 3, 9, "models/w_crowbar.mdl"},
	{"cust_3BeamVoltage", 4, 10, "models/w_crowbar.mdl"},
	{"cust_1SPChemicalGunPressure", 0x1b, 6, "models/w_crowbar.mdl"},
	{"cust_2ChemAcid", 0x18, 3, "models/w_crowbar.mdl"},
	{"cust_3ChemBase", 0x1a, 5, "models/w_crowbar.mdl"},
	{"cust_4ChemNuet", 0x19, 4, "models/w_crowbar.mdl"},
};

const CustItem* FindCustItem(entvars_t* pev)
{
	for (const CustItem& item : g_CustItems)
	{
		if (FClassnameIs(pev, item.classname))
			return &item;
	}
	return nullptr;
}
}

class CCustomizeItem : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool CanDeploy() override { return false; }
	bool AddDuplicate(CBasePlayerItem* pItem) override { return true; } // Doppelte Items werden still verbraucht
};
LINK_ENTITY_TO_CLASS(cust_1fistsKnife, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_1ShotgunSpread, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_2ShotgunShells, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_1MinigunSpin, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_2MinigunCooled, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_1GaussPistolFireType, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_2GaussPistolSniper, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_1BeamFrequency, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_2BeamCurrent, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_3BeamVoltage, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_1SPChemicalGunPressure, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_2ChemAcid, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_3ChemBase, CCustomizeItem);
LINK_ENTITY_TO_CLASS(cust_4ChemNuet, CCustomizeItem);

void CCustomizeItem::Precache()
{
	PRECACHE_MODEL("models/w_crowbar.mdl");
	PRECACHE_MODEL("models/w_snipercase.mdl");
	PRECACHE_MODEL("models/w_coolers.mdl");
}

void CCustomizeItem::Spawn()
{
	Precache();

	const CustItem* pItem = FindCustItem(pev);
	if (pItem)
	{
		m_iId = pItem->id;
		SET_MODEL(ENT(pev), pItem->model);
	}
	else
	{
		SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	}

	m_iClip = -1;
	FallInit();
}

bool CCustomizeItem::GetItemInfo(ItemInfo* p)
{
	const CustItem* pItem = FindCustItem(pev);

	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = nullptr;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 9;
	p->iPosition = pItem ? pItem->position : 0;
	p->iId = m_iId = pItem ? pItem->id : m_iId;
	p->iFlags = 0;
	p->iWeight = -100;
	return true;
}
