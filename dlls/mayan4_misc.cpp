//=========================================================
// MAYAN4's remaining class with no SDK precedent: monster_renesaur
// (CRenesaur) - a stationary, non-damageable boss head+neck prop with
// no movement/AI, driven entirely by map-side scripted_sequence
// chains (per findings/entities/renesaur.md's thorough Session-49
// verification, cross-checked here by re-decompiling both addresses
// fresh rather than trusting the doc alone).
//
// Decompiled fresh from gunman.dll: LINK @0x10077830, vtable
// @0x100f8df4, Spawn @0x10077dc0. Confirms the doc's Spawn() summary
// exactly: SOLID_NOT, MOVETYPE_NONE (never moves/collides - all 10+
// bite* animation sequences are triggered externally by
// scripted_sequence entities targeting this by name, never by any
// internal attack logic), health field set to 8.0 (not the doc's
// flagged-as-mysterious literal 247, which - as independently found
// this session for monster_tank - turns out to sit at a
// class-private member offset rather than pev->health, and is most
// likely a default sequence/animation index, not combat-relevant),
// pev->takedamage explicitly cleared to DAMAGE_NO at the end of
// Spawn() (confirmed non-damageable, matching the doc's "keine reale
// Kampf-KI-relevanz" conclusion). No KeyValue override - falls
// through to the shared generic CBaseMonster::KeyValue.
//
// Not simplified relative to the original at all: the doc already
// established there IS no original C++ combat/schedule logic to
// simplify away - this class's entire "boss fight" lives in mayan4's
// own scripted_sequence/trigger_multiple entity chains, which need no
// C++ counterpart (the stock SDK's scripted_sequence/trigger_multiple
// already handle that generically).
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

class CRenesaur : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
	// BUG FIX (2026-09-05): monster_renesaur is driven entirely by 12
	// scripted_sequence entities (mayan4, all targeting it via
	// m_iszEntity="renhead") playing named bite sequences
	// ("bitedoorway"/"biteswitchleft" etc, all activity=0 per
	// tools/mdl_inspect.py on renesaurhead.mdl) - same architecture as
	// monster_generic's SAM turret and monster_furniture. Unlike
	// missleturret.mdl, renesaurhead.mdl's sequence 0 ("idle") DOES
	// carry a real activity=1 (ACT_IDLE) tag, so CBaseMonster's default
	// SetActivity(ACT_IDLE) resolves to a valid-looking idle pose
	// rather than an arbitrary sequence-0 fallback - but it still
	// stomps pev->sequence out from under an in-progress scripted bite
	// animation whenever the default AI schedule requests ACT_IDLE
	// between waypoints, matching the same root cause documented for
	// CFurniture/CGenericMonster - see
	// [[feedback-setactivity-act-idle-stomps-scripted-sequence]]. Same
	// fix: leave sequence selection entirely to
	// CCineMonster::StartSequence.
	void SetActivity(Activity NewActivity) override
	{
		if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && m_pCine != nullptr)
		{
			m_Activity = NewActivity;
			m_IdealActivity = NewActivity;
			return;
		}
		CBaseMonster::SetActivity(NewActivity);
	}
};
LINK_ENTITY_TO_CLASS(monster_renesaur, CRenesaur);

void CRenesaur::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/renesaurhead.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->health = 8;
	pev->takedamage = DAMAGE_NO;
	pev->yaw_speed = 1;
	m_bloodColor = 247;	  // Retail Spawn @0x10077dc0
	m_flFieldOfView = 0.0f; // Retail: param_1[0x49] = 0

	MonsterInit();

	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
}

void CRenesaur::Precache()
{
	PrecacheModel("models/renesaurhead.mdl");
}
