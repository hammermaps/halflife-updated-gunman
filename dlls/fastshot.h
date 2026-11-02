//=========================================================
// CFastShot ("gauss_charged") - Laser-/Gauss-Projektil. Retail: Spawn @0x1008ece0, Precache @0x1008f0e0,
// Animate @0x1008ee80, FastShotTouch @0x1008eee0, Erzeugung durch die Bandit-Ereignisse 5/6 @0x1008e9c0.
// Details: findings/entities/bandit_laser_2026-10-31.md
//=========================================================

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CFastShot : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	void EXPORT FastShotTouch(CBaseEntity* pOther);
	void EXPORT Animate();
	// Lebensdauer fuer verfehlte Projektile (Abweichung von Retail, siehe fastshot.cpp).
	void EXPORT Expire();

	// Erzeugt ein Projektil am Ursprung mit der Geschwindigkeit; pev->dmg = Schaden, Besitzer = pevOwner.
	static CFastShot* Create(entvars_t* pevOwner, const Vector& vecOrigin, const Vector& vecVelocity, float flDamage);

private:
	float m_flMaxFrame = 0;
	int m_iTrail = 0;
	int m_iGlow = 0;
	unsigned short m_usImpact = 0;
};
