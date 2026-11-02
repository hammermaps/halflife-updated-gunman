//=========================================================
// monster_human_unarmed - CFriendlyUnarmed. A friendly, talking human
// NPC without a combat role ("Unarmed Gunman" per the FGD) - the
// unarmed sibling of monster_human_gunman (see human_gunman.cpp for
// the shared CTalkMonster/generic-sentence-group background; this
// class follows the exact same pattern).
//
// Decompiled from gunman.dll: constructor @0x100afe10 (vtable
// 0x10100c28), Spawn @0x100af6a0, Precache @0x100af7d0, KeyValue
// @0x100af800 (all 21 field offsets re-verified fresh this session,
// not assumed identical to CFriendlyGunman's layout even though the
// keyvalue set and semantics turned out to match), Save @0x100af5f0
// (class name literal "CFriendlyUnarmed"), Classify @0x100af650
// (constant 3 = CLASS_HUMAN_PASSIVE, same as CFriendlyGunman).
//
// Confirmed differences from CFriendlyGunman found by re-decompiling
// rather than assuming symmetry:
// - Model is "models/gunmantrooper_ng.mdl" ("no gun" variant) instead
//   of "models/gunmantrooper.mdl".
// - No "healthvalue" keyvalue at all - pev->health is a hardcoded
//   80.0 constant in Spawn().
// - gunstate is a 3-way FGD choice here (0=Holstered/1=Drawn/2=None,
//   vs. CFriendlyGunman's 2-way 1=Drawn/2=None) but the compiled Spawn
//   only branches on ==2: bodygroup 1 becomes 2 when gunstate==2,
//   0 otherwise - Holstered(0) and Drawn(1) compile to the *same*
//   body index (weapon visibility is apparently handled by animation/
//   sequence choice, not a body swap, for those two states).
// - Two extra SetBodygroup(0, 0) / SetBodygroup(2, 0) calls not
//   present in CFriendlyGunman's Spawn.
//
// Simplified relative to the original, same rationale as
// CFriendlyGunman (see human_gunman.cpp's file header): no custom
// Schedule_t/Task_t tables, no ranged-attack implementation (this
// variant's FGD title and "unarmed" naming both suggest no combat
// role, and no ranged-attack evidence was found in the decompiled
// Spawn - unlike CFriendlyGunman it does not set
// bits_CAP_RANGE_ATTACK1), gn_kill/gn_attack parsed and stored but not
// wired into playback logic.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "talkmonster.h"
#include "schedule.h"
#include "weapons.h"
#include "soundent.h"

class CFriendlyUnarmed : public CTalkMonster
{
public:
	void Spawn() override;
	void Precache() override;
	// Retail CFriendlyUnarmed::ObjectCaps @ 0x100afea0 returns 0x0a.
	int ObjectCaps() override { return FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE; }
	int Classify() override { return CLASS_HUMAN_PASSIVE; }
	void SetYawSpeed() override;
	void SetActivity(Activity NewActivity) override;
	void FollowerUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void DeclineFollowing() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void TalkInit();

	bool KeyValue(KeyValueData* pkvd) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_iGunState = 0; // 0 = Holstered, 1 = Drawn, 2 = None

	int m_iszAnswer = 0;
	int m_iszQuestion = 0;
	int m_iszIdle = 0;
	int m_iszStare = 0;
	int m_iszUseSentence = 0;
	int m_iszUnUseSentence = 0;
	int m_iszStop = 0;
	int m_iszNoShoot = 0;
	int m_iszHello = 0;
	int m_iszPlHurt1 = 0;
	int m_iszPlHurt2 = 0;
	int m_iszPlHurt3 = 0;
	int m_iszPHello = 0;
	int m_iszPIdle = 0;
	int m_iszPQuestion = 0;
	int m_iszSmell = 0;
	int m_iszWound = 0;
	int m_iszMortal = 0;
	int m_iszKill = 0;	 // Gunman-only, no stock TLK_ slot - see human_gunman.cpp
	int m_iszAttack = 0; // Gunman-only, no stock TLK_ slot - see human_gunman.cpp
};
LINK_ENTITY_TO_CLASS(monster_human_unarmed, CFriendlyUnarmed);

TYPEDESCRIPTION CFriendlyUnarmed::m_SaveData[] =
		{
			DEFINE_FIELD(CFriendlyUnarmed, m_iGunState, FIELD_INTEGER),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszAnswer, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszQuestion, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszIdle, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszStare, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszUseSentence, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszUnUseSentence, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszStop, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszNoShoot, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszHello, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszPlHurt1, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszPlHurt2, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszPlHurt3, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszPHello, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszPIdle, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszPQuestion, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszSmell, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszWound, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszMortal, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszKill, FIELD_STRING),
			DEFINE_FIELD(CFriendlyUnarmed, m_iszAttack, FIELD_STRING),
		};

IMPLEMENT_SAVERESTORE(CFriendlyUnarmed, CTalkMonster);

// Preserve scripted animations while the cinematic owns the monster and
// across CCineMonster's short self-retrigger gap. During the gap m_pCine is
// already null, so every AI activity (including turn/walk) must be ignored;
// once the grace period expires, normal talk/follower AI is unaffected.
void CFriendlyUnarmed::SetActivity(Activity NewActivity)
{
	const bool scriptOwnsSequence = m_pCine != nullptr && (NewActivity == ACT_IDLE || NewActivity == ACT_RESET);
	const bool scriptRetriggerPending = m_pCine == nullptr && gpGlobals->time < m_flCineGraceEnd;
	if (scriptOwnsSequence || scriptRetriggerPending)
	{
		m_Activity = NewActivity;
		m_IdealActivity = NewActivity;
		return;
	}

	CTalkMonster::SetActivity(NewActivity);
}

bool CFriendlyUnarmed::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "gunstate"))
	{
		m_iGunState = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_answer"))
	{
		m_iszAnswer = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_question"))
	{
		m_iszQuestion = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_idle"))
	{
		m_iszIdle = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_stare"))
	{
		m_iszStare = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_use"))
	{
		m_iszUseSentence = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_unuse"))
	{
		m_iszUnUseSentence = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_stop"))
	{
		m_iszStop = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_noshoot"))
	{
		m_iszNoShoot = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_hello"))
	{
		m_iszHello = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_plhurt1"))
	{
		m_iszPlHurt1 = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_plhurt2"))
	{
		m_iszPlHurt2 = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_plhurt3"))
	{
		m_iszPlHurt3 = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_phello"))
	{
		m_iszPHello = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_pidle"))
	{
		m_iszPIdle = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_pquestion"))
	{
		m_iszPQuestion = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_smell"))
	{
		m_iszSmell = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_wound"))
	{
		m_iszWound = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_mortal"))
	{
		m_iszMortal = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_kill"))
	{
		m_iszKill = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "gn_attack"))
	{
		m_iszAttack = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CTalkMonster::KeyValue(pkvd);
}

void CFriendlyUnarmed::TalkInit()
{
	CTalkMonster::TalkInit();

	// Retail TalkInit @ 0x100afeb0 aliases this pair for the shared
	// TalkMonster implementation and uses the fixed human voice pitch.
	m_szFriends[0] = "monster_human_unarmed";
	m_szFriends[1] = "monster_human_gunman";

	m_szGrp[TLK_ANSWER] = STRING(m_iszAnswer);
	m_szGrp[TLK_QUESTION] = STRING(m_iszQuestion);
	m_szGrp[TLK_IDLE] = STRING(m_iszIdle);
	m_szGrp[TLK_STARE] = STRING(m_iszStare);
	m_szGrp[TLK_USE] = STRING(m_iszUseSentence);
	m_szGrp[TLK_UNUSE] = STRING(m_iszUnUseSentence);
	m_szGrp[TLK_STOP] = STRING(m_iszStop);
	m_szGrp[TLK_NOSHOOT] = STRING(m_iszNoShoot);
	m_szGrp[TLK_HELLO] = STRING(m_iszHello);
	m_szGrp[TLK_PLHURT1] = STRING(m_iszPlHurt1);
	m_szGrp[TLK_PLHURT2] = STRING(m_iszPlHurt2);
	m_szGrp[TLK_PLHURT3] = STRING(m_iszPlHurt3);
	m_szGrp[TLK_PHELLO] = STRING(m_iszPHello);
	m_szGrp[TLK_PIDLE] = STRING(m_iszPIdle);
	m_szGrp[TLK_PQUESTION] = STRING(m_iszPQuestion);
	m_szGrp[TLK_SMELL] = STRING(m_iszSmell);
	m_szGrp[TLK_WOUND] = STRING(m_iszWound);
	m_szGrp[TLK_MORTAL] = STRING(m_iszMortal);
	m_voicePitch = 100;
}

void CFriendlyUnarmed::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/gunmantrooper_ng.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->health = 80;
	pev->view_ofs = Vector(0, 0, 50);
	m_flFieldOfView = VIEW_FIELD_WIDE;
	m_MonsterState = MONSTERSTATE_NONE;

	m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	MonsterInit();
	// Retail CFriendlyUnarmed::Spawn @ 0x100af6a0 restores the authored eye
	// height after MonsterInit()'s model-derived lookup.
	pev->view_ofs = Vector(0, 0, 50);
	SetUse(&CFriendlyUnarmed::FollowerUse);

	SetBodygroup(0, 0);
	// Holstered(0) and Drawn(1) share body index 0 in the original -
	// weapon visibility for those two states is handled elsewhere
	// (animation/sequence), not by this bodygroup.
	SetBodygroup(1, m_iGunState == 2 ? 2 : 0);
	SetBodygroup(2, 0);
}

void CFriendlyUnarmed::SetYawSpeed()
{
	// See CFriendlyGunman: the base implementation is a no-op, so scripted
	// movement needs an explicit turn rate to face its route.
	pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70;
}

void CFriendlyUnarmed::FollowerUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Retail @ 0x100b0200 only observes the scripted-sentence use lock; it
	// never enters CTalkMonster's generic follow/unfollow implementation.
	if (m_useTime <= gpGlobals->time)
		DeclineFollowing();
}

void CFriendlyUnarmed::DeclineFollowing()
{
	// Retail virtual slot +0x22c @ 0x100b0220. This is deliberately the
	// configured use group, not the stock decline/follow response.
	PlaySentence(m_szGrp[TLK_USE], 2.0f, VOL_NORM, ATTN_NORM);
}

void CFriendlyUnarmed::Killed(entvars_t* pevAttacker, int iGib)
{
	// Retail slot 13 @ 0x100b01e0 clears the touch callback before invoking
	// CTalkMonster::Killed. This prevents a dead scripted civilian from
	// retaining an authored touch handler during the base death transition.
	SetTouch(NULL);
	CTalkMonster::Killed(pevAttacker, iGib);
}

void CFriendlyUnarmed::Precache()
{
	PrecacheModel("models/gunmantrooper_ng.mdl");

	TalkInit();
	CTalkMonster::Precache();
}
