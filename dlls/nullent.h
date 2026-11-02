// entity_null: lingering laser "glow" after the gauss pistol sniper shot (retail vtable @0x100fc088,
// creator @0x1008e8f0). Shared between gunman_custom_entities.cpp and gausspistol.cpp.
#pragma once

class CNullEnt : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT WaitThink();
	void EXPORT LazerThink();
	void EXPORT RemoveThink();

	// Retail creator @0x1008e8f0: origin, beam end, owner, wide flag
	static CNullEnt* Create(const Vector& vecOrigin, const Vector& vecEnd, entvars_t* pevOwner, bool bWide);

	Vector m_vecBeamEnd;		// +0x270
	int m_iBrightness = 90;		// +0x27c
	bool m_bStartLaser = false; // +0x280
	bool m_bWideBeam = false;	// +0x284
	bool m_bHitWorld = false;	// +0x288 (set for non-damageable targets; no reader found in retail)
	EHANDLE m_hHelper;			// +0x28c (its renderamt follows the brightness)

private:
	int m_iBeamSprite = 0;
};
