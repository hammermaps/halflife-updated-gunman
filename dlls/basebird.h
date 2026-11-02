//=========================================================
// CBaseBird - shared base of the retail "bird" family (decore_pteradon,
// decore_eagle; monster_human_chopper and monster_rustflier share the
// same retail base, not yet ported). Ports of retail CBaseBird init
// @0x100689a0, HuntThink @0x10068e60, flight steering @0x10069530,
// FlyTouch @0x10068d00 and Killed @0x10068b30; see
// findings/entities/cbasebird_huntthink_2026-10-31.md.
// Open points: idle/flap sound virtuals (+0x22c/+0x230) and the never
// written timestamp +0x2a8.
//=========================================================
#pragma once

class CBaseBird : public CBaseMonster
{
public:
	// Retail Spawn: precache, model, shared init helper @0x100689a0
	void SpawnBird(const char* pszModel);
	int Classify() override { return CLASS_NONE; }
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void EXPORT HuntThink();
	void EXPORT DyingThink();
	void EXPORT FlyTouch(CBaseEntity* pOther);
	void EXPORT CrashTouch(CBaseEntity* pOther);

private:
	void SteerFlight();
	void SetGoal(CBaseEntity* pGoal);

	float m_flThrust = 0;		 // +0x270
	Vector m_vecPrevGoal;		 // +0x274
	Vector m_vecDesiredDir;		 // +0x280
	Vector m_vecGoal;			 // +0x28c
	Vector m_vecGoalDir;		 // +0x298
	float m_flSpeedAccum = 0;	 // +0x2a4
	float m_flGoalTime = 0;		 // +0x2a8
	float m_flFlapTime = 0;		 // +0x2b0
	float m_flIdleTime = 0;		 // +0x2b4
	EHANDLE m_hGoal;			 // this+8
};
