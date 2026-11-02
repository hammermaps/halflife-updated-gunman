//=========================================================
// CChemGrenade ("chem_grenade") - Chemie-Granate/-Klecks von weapon_chemgun und monster_human_scientist.
// Retail: Vtable 0x100fd0dc, Spawn @0x10091f30, Precache @0x10091f70, Erzeuger @0x10093b50 (springend, zeitgezuendet)
// und @0x10093d90 (Kontakt), Touch/Think @0x10092040..0x10094110. Details und Vereinfachungen:
// findings/entities/chemgrenade_port_2026-10-31.md
//=========================================================

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"

// Werte der Chemie-Art (m_iType, Retail +0x2b0): 1 Saeure, 2 Base, 3 und 4 Sprengsatz (3 explodiert, 4 mischt), 5 Neutralisierer/Zaehfluessig.
enum
{
	CHEM_ACID = 1,
	CHEM_BASE = 2,
	CHEM_EXPLOSIVE = 3,
	CHEM_MIXED = 4,
	CHEM_NEUTRAL = 5,
};

// Retail-Parameterblock (10 Dwords ab +0x294); Felder ohne bekannte Bedeutung heissen field*.
struct ChemParams
{
	int iAcid = 0;		   // +0x294 (Saeureanteil; ChemicalExplode vergleicht mit iBase)
	int iBase = 0;		   // +0x298
	int field29c = 0;	   // +0x29c
	int iViscosity = 0;	   // +0x2a0 (Daempfung beim Aufprall: (30 - Wert) / 15)
	int iDamage = 0;	   // +0x2a4
	float flBounce = 0;	   // +0x2a8 (Aufprallbudget)
	int field2ac = 0;	   // +0x2ac
	int iType = CHEM_ACID; // +0x2b0
	int iMix = 0;		   // +0x2b4 (Mischklasse 6/7/8 der Chemgun)
	int field2b8 = 0;	   // +0x2b8
};

class CChemGrenade : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	// Retail @0x10092c20: haftende Granate (Mischklasse 6 der Chemgun); Zuendzeit = flBounce * 0.1 s.
	static CChemGrenade* ShootSticky(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, const ChemParams& params);
	// Retail @0x10093b50: springende Granate; Zuendzeit = (int)(flBounce * 0.1) s (Ganzzahl, Retail FUN_100d9378 = _ftol).
	static CChemGrenade* ShootBouncing(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, const ChemParams& params);
	// Retail @0x10093d90: Kontaktgranate (zerplatzt beim ersten Treffer).
	static CChemGrenade* ShootContact(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, const ChemParams& params);

	void EXPORT SplatterTouch(CBaseEntity* pOther);
	void EXPORT StickyChemicalTouch(CBaseEntity* pOther);
	void EXPORT BouncyChemicalTouch(CBaseEntity* pOther);
	void EXPORT TimedChemicalThink();
	void EXPORT ChemicalTrail();
	void EXPORT ChemicalSmoke();
	void EXPORT ChemicalSmokeThink();

private:
	void Splatter(TraceResult* pTrace);
	void ChemicalExplode();
	void Setup(entvars_t* pevOwner, const Vector& vecStart, const Vector& vecVelocity, const ChemParams& params);
	void ChemRadiusDamage(const Vector& vecSrc, entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, float flRadius, int iClassIgnore, int iType);

	int m_fBounced = 0;			 // +0x270
	float m_flSplatterTime = 0;	 // +0x274
	int m_iSprGreen = 0;		 // +0x278
	int m_iSprRed = 0;			 // +0x27c
	int m_iSprYellow = 0;		 // +0x280
	int m_iSprThrower = 0;		 // +0x284
	unsigned short m_usSpray = 0; // +0x288
	int m_iSmokeSprite = 0;
	int m_fSticky = 0;			 // +0x28c
	ChemParams m_Params;		 // +0x294
};
