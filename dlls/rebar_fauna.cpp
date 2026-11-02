//=========================================================
// Remaining REBAR2*/REBAR2K gaps: monster_tube_embryo (CTubeEmbryo),
// monster_xenome_embryo (CXenomeEmbryo), monster_aigirl (CAIGirl),
// entity_clustergod (CClusterGod). All four fresh-decompiled this
// session; entity_review_list.csv already had accurate one-line
// summaries for each, cross-checked against gunman.dll rather than
// trusted as-is.
//
// Decompiled fresh from gunman.dll:
//   monster_tube_embryo: LINK @0x100ce190, vtable @0x10106f7c, Spawn
//     @0x100ce1e0, Precache @0x100ce330, Classify (slot 8,
//     @0x100ce320) confirmed constant 0xd = 13 = CLASS_ALIEN_BIOWEAPON.
//     Confirmed model "models/tubryo.mdl", bbox (-6,-6,-6)/(6,6,12),
//     SOLID_SLIDEBOX, health 30, sets Think = CTubeEmbryo::SpawnSack -
//     a real method that dynamically spawns the "decore_sack" child
//     entity. The child construction, post-spawn bbox and ShockAnim
//     use callback are reproduced below; the child handle is persisted.
//   monster_xenome_embryo: LINK @0x100cd510, vtable @0x101068f4,
//     Spawn @0x100cd560, Precache @0x100cd6e0, Classify (slot 8,
//     @0x100cd6d0) confirmed constant 0xd = 13 =
//     CLASS_ALIEN_BIOWEAPON (same as CTubeEmbryo). Confirmed model
//     "models/xmbryo.mdl", same bbox as CTubeEmbryo, health 20,
//     takedamage=DAMAGE_YES, sets Use = CXenomeEmbryo::
//     XenomeEmbryoGibUse and Think = CXenomeEmbryo::IdleSoundThink
//     (initial random 0-6s interval; subsequent random 1-25s interval,
//     plays one of three confirmed idle sounds). Its Use callback logs
//     and always gibs the embryo.
//   monster_aigirl: LINK @0x10079260, vtable @0x100f9498, Spawn
//     @0x10079350, Precache @0x10079410, Classify (slot 8,
//     @0x10079310) confirmed constant 0 = CLASS_NONE - matches
//     findings/entity_review_list.csv's "rein dekorative
//     Hintergrundfigur ... nicht angreifbar/feindlich" exactly.
//     Confirmed model "models/aigirl.mdl", bbox (-16,-16,0)/
//     (16,16,72), SOLID_SLIDEBOX, MOVETYPE_FLY(4), health 100 - a
//     purely decorative, scripted_sequence/multi_manager-driven
//     background character per the doc, so no custom AI/Think is
//     needed for its confirmed usage (only in REBAR2I/rebar2l/REBAR3B,
//     always scripted).
//   entity_clustergod: LINK @0x100a2420, vtable @0x100fed78, Spawn
//     @0x100a2470, ShootClusterGrenades (Think) @0x100a2570 fresh-
//     decompiled. Confirmed: model "models/null.mdl" (invisible),
//     SOLID_NOT, MOVETYPE_NONE, no takedamage. ShootClusterGrenades
//     spawns exactly 4 submunition entities (model
//     "models/artillary.mdl") with randomized outward
//     position/velocity offsets around the spawner, then the spawner
//     removes itself via CBaseEntity::SUB_Remove - confirms
//     findings/entity_review_list.csv's "Laufzeit-Spawner fuer
//     Cluster-Sub-Munition". Simplified relative to the original: the
//     submunitions' own randomized-arc trajectory math is NOT
//     reproduced - reproduced instead via the stock SDK's
//     CGrenade::ShootTimed() (already used project-wide for other
//     timed-grenade spawns) fired in 4 randomized directions with a
//     short fuse, matching the confirmed "4 submunitions, then
//     self-remove" shape without re-deriving the exact ballistic arc
//     constants.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "saverestore.h"

// Shared DML cluster-submunition factory (defined in mayan6_misc.cpp).
extern CBaseEntity* Dml_ShootClusterGrenade(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float flDamage);

//=========================================================
// decore_sack - private runtime-only visual child of CTubeEmbryo.
//=========================================================
class CDecoreSack : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_BIOWEAPON; }
	void PlayShock();
};
LINK_ENTITY_TO_CLASS(decore_sack, CDecoreSack);

// Retail 0x100cdf60: the sack is a monster-family class (vtable with the
// full CBaseMonster slots, Classify 13 = CLASS_ALIEN_BIOWEAPON). Spawn sets
// model, classname, solid NOT, movetype NOCLIP (8), effects 0, takedamage 0,
// spawnflags |= 0x80000000 and then calls MonsterInit (vtable +0x10c), which
// makes it FL_MONSTER with takedamage DAMAGE_AIM and the monster Think/Use.
// Vereinfachung (offen): Retail GetScheduleOfType(0x2b) (Slot 88 -> 0x100ce000)
// liefert immer den Schedule "Locked - IdleStand" (TASK_STOP_MOVING, Task 95,
// TASK_WAIT 5.0 s, Interrupt-Maske 0x57). Task 95 existiert nicht im SDK
// (Retail-Enum ist groesser) und setzt nur ACT_IDLE und schliesst ab, sofern
// das Bit 0x40000000 am Monster nicht gesetzt ist. Hier bleibt der SDK-Standard-
// Schedule (SCHED_IDLE_STAND). Der Sack bewegt sich in beiden Faellen nicht.
void CDecoreSack::Spawn()
{
	pev->model = MAKE_STRING("models/sack.mdl");
	pev->classname = MAKE_STRING("decore_sack");
	SET_MODEL(ENT(pev), "models/sack.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->effects = 0;
	pev->takedamage = DAMAGE_NO;
	pev->spawnflags |= 0x80000000;
	MonsterInit();
}

void CDecoreSack::Precache()
{
	PrecacheModel("models/sack.mdl");
}

void CDecoreSack::PlayShock()
{
	const int iSequence = LookupSequence("shock");
	if (iSequence >= 0)
	{
		pev->sequence = iSequence;
		pev->frame = 0;
		ResetSequenceInfo();
	}
}

//=========================================================
// monster_tube_embryo - CTubeEmbryo.
//=========================================================
class CTubeEmbryo : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_BIOWEAPON; }
	void EXPORT SpawnSack();
	void EXPORT ShockAnim(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	EHANDLE m_hSack;
};
LINK_ENTITY_TO_CLASS(monster_tube_embryo, CTubeEmbryo);

TYPEDESCRIPTION CTubeEmbryo::m_SaveData[] =
	{
		DEFINE_FIELD(CTubeEmbryo, m_hSack, FIELD_EHANDLE),
	};

IMPLEMENT_SAVERESTORE(CTubeEmbryo, CBaseMonster);

void CTubeEmbryo::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/tubryo.mdl");
	UTIL_SetSize(pev, Vector(-6, -6, -6), Vector(6, 6, 12));

	pev->solid = SOLID_SLIDEBOX;
	pev->health = 30;
	m_bloodColor = 50;		 // Retail @0x100ce1e0
	m_flFieldOfView = -0.7f; // Retail: 0xbf333333
	pev->effects = 0;
	pev->spawnflags |= 0x80000000; // retail @0x100cdf60: entvars +0x1a0 (spawnflags), not effects

	MonsterInit();
	pev->view_ofs = Vector(0, 0, 6);
	SetThink(&CTubeEmbryo::SpawnSack);
	pev->nextthink = gpGlobals->time;
}

void CTubeEmbryo::Precache()
{
	PrecacheModel("models/tubryo.mdl");
	UTIL_PrecacheOther("decore_sack");
}

void CTubeEmbryo::SpawnSack()
{
	CBaseEntity* pSack = CBaseEntity::Create("decore_sack", pev->origin, pev->angles, nullptr);
	if (!pSack)
		return;

	m_hSack = pSack;
	pSack->pev->rendermode = kRenderTransTexture;
	pSack->pev->renderamt = 180.0f;
	// Retail grows the parent collision box after its visual sack exists.
	UTIL_SetSize(pev, Vector(-12, -12, -16), Vector(12, 12, 24));
	SetUse(&CTubeEmbryo::ShockAnim);
	SetThink(NULL);
}

void CTubeEmbryo::ShockAnim(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	SetSequenceByName("shock");
	CDecoreSack* pSack = dynamic_cast<CDecoreSack*>((CBaseEntity*)m_hSack);
	if (pSack)
		pSack->PlayShock();
	else
		ALERT(at_aiconsole, "no sack for shocking!\n");
}

//=========================================================
// monster_xenome_embryo - CXenomeEmbryo.
//=========================================================
class CXenomeEmbryo : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_BIOWEAPON; }
	void EXPORT IdleSoundThink();
	void EXPORT XenomeEmbryoGibUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
};
LINK_ENTITY_TO_CLASS(monster_xenome_embryo, CXenomeEmbryo);

void CXenomeEmbryo::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/xmbryo.mdl");
	UTIL_SetSize(pev, Vector(-6, -6, -6), Vector(6, 6, 12));

	pev->solid = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_YES;
	pev->health = 20;
	m_bloodColor = 100;		 // Retail @0x100cd560
	m_flFieldOfView = -0.7f;
	pev->spawnflags |= 0x80000000;

	MonsterInit();
	pev->view_ofs = Vector(0, 0, 6);
	SetUse(&CXenomeEmbryo::XenomeEmbryoGibUse);
	SetThink(&CXenomeEmbryo::IdleSoundThink);
	pev->nextthink = gpGlobals->time + RANDOM_LONG(0, 6);
}

void CXenomeEmbryo::Precache()
{
	PrecacheModel("models/xmbryo.mdl");
	PrecacheModel("models/xenomegibs.mdl");
	PrecacheSound("xenome/xmbryo_idle1.wav");
	PrecacheSound("xenome/xmbryo_idle2.wav");
	PrecacheSound("xenome/xmbryo_idle3.wav");
}

void CXenomeEmbryo::IdleSoundThink()
{
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(1.0f, 25.0f);

	const char* sounds[] = {"xenome/xmbryo_idle1.wav", "xenome/xmbryo_idle2.wav", "xenome/xmbryo_idle3.wav"};
	EMIT_SOUND(ENT(pev), CHAN_VOICE, sounds[RANDOM_LONG(0, 2)], 1.0, ATTN_IDLE);
}

void CXenomeEmbryo::XenomeEmbryoGibUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	ALERT(at_aiconsole, "Using Embryo\n");
	Killed(pev, GIB_ALWAYS);
}

//=========================================================
// monster_aigirl - CAIGirl. Purely decorative, scripted-sequence
// driven background character, see file header.
//=========================================================
class CAIGirl : public CBaseMonster
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_NONE; }
	// BUG FIX (2026-09-05): monster_aigirl is purely decorative,
	// scripted_sequence/multi_manager-driven (see file header) - same
	// architecture as monster_furniture/monster_generic/monster_renesaur.
	// aigirl.mdl's "Idle" sequence carries a real activity=1 (ACT_IDLE)
	// tag, but its cutscene sequences ("gesture"/"slap"/"choke#idle"/
	// "chokestop"/"point"/"jump"/"jump2"/"typing#idle") are all
	// activity=0 (named-only, per tools/mdl_inspect.py). Without this
	// override, CBaseMonster's default SetActivity(ACT_IDLE) (called by
	// the default AI schedule between scripted waypoints) snaps her back
	// to the idle stance mid-sequence - see
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
	// CORRECTION (2026-09-05 pattern, applied here 2026-09-06): missing
	// SetYawSpeed() override - same root cause/fix as CFriendlyGunman.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_WALK) ? 90 : 70; }
	// Nachtrag 2026-09-06 (In-Game-Test-Meldung "haengt in der Luft,
	// bitte einfache KI die sich nur bewegt", KORRIGIERT nach
	// Nutzer-Rueckmeldung: KEIN freies autonomes Wandern per Default -
	// sie soll sich nur bewegen, wenn sie vom Spieler beruehrt oder
	// beschossen wird. Freies Wandern nur, wenn der neue Spawnflag
	// SF_AIGIRL_WANDER gesetzt ist; Default = aus).
	// Einfaches Bewegungsmuster nach dem etablierten CRoach/CCricket-
	// Muster dieses Projekts (direktes WALK_MOVE zu einem Zielpunkt,
	// kein voller Schedule_t/Task_t-Scheduler). aigirl.mdl hat echte
	// "Idle" (ACT_IDLE) und "walk"/"walk2" (ACT_WALK) Sequenzen mit
	// korrekten Activity-Tags (per tools/mdl_inspect.py bestaetigt) -
	// beide werden genutzt. Greift NICHT ein, waehrend eine
	// scripted_sequence die Figur steuert (m_pCine != nullptr, gleiche
	// Bedingung wie in SetActivity() oben).
	void EXPORT AIGirlThink();
	void EXPORT AIGirlTouch(CBaseEntity* pOther);
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override
	{
		StartReactiveMove(pevAttacker ? CBaseEntity::Instance(pevAttacker) : nullptr);
		CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
	}

private:
	void StartReactiveMove(CBaseEntity* pInstigator);

	Vector m_vecWanderDest = g_vecZero;
};
LINK_ENTITY_TO_CLASS(monster_aigirl, CAIGirl);

TYPEDESCRIPTION CAIGirl::m_SaveData[] =
{
	DEFINE_FIELD(CAIGirl, m_vecWanderDest, FIELD_POSITION_VECTOR),
};
IMPLEMENT_SAVERESTORE(CAIGirl, CBaseMonster);

// Spawnflag-Bit fuer freies autonomes Wandern (Default aus, siehe
// Klassenkommentar oben). In die rekonstruierte FGD uebernommen
// (findings/fgd-diff/gunman-reconstructed-additions.fgd).
#define SF_AIGIRL_WANDER (1 << 0)

void CAIGirl::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/aigirl.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));

	pev->solid = SOLID_SLIDEBOX;
	// Nachtrag 2026-09-06 (In-Game-Test-Meldung "haengt in der Luft"):
	// MOVETYPE_FLY hielt sie permanent auf Spawn-Hoehe fest, ohne von
	// der Schwerkraft auf den Boden gezogen zu werden - jede andere
	// laufende Figur dieses Projekts nutzt MOVETYPE_STEP.
	pev->movetype = MOVETYPE_STEP;
	pev->health = 100;

	MonsterInit();
	SetActivity(ACT_IDLE);
	SetTouch(&CAIGirl::AIGirlTouch);
	// Retail uses this as a scripted background actor and leaves the normal
	// MonsterInitThink -> StartMonster -> CallMonsterThink transition alive.
	// AIGirlThink is installed only for the optional, local reaction below.
}

void CAIGirl::Precache()
{
	PrecacheModel("models/aigirl.mdl");
}

// Nachtrag 2026-09-06 (Nutzer-Korrektur): ausgeloest von AIGirlTouch()
// (Spielerberuehrung) und TraceAttack() (Beschuss/Angriff jeder Art,
// nicht nur Spielerwaffen) - laeuft weg vom Ausloeser statt in eine
// zufaellige Richtung, sofern ein Ausloeser bekannt ist.
void CAIGirl::StartReactiveMove(CBaseEntity* pInstigator)
{
	if (m_pCine != nullptr || m_Activity == ACT_WALK)
		return;

	Vector vecDir;
	if (pInstigator != nullptr)
		vecDir = (pev->origin - pInstigator->pev->origin).Normalize();
	else
		vecDir = Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), 0).Normalize();

	float flDist = 128 + RANDOM_LONG(0, 255);
	m_vecWanderDest = pev->origin + vecDir * flDist;
	SetActivity(ACT_WALK);
	SetThink(&CAIGirl::AIGirlThink);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CAIGirl::AIGirlTouch(CBaseEntity* pOther)
{
	if (pOther->IsPlayer())
		StartReactiveMove(pOther);
}

void CAIGirl::AIGirlThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	// scripted_sequence hat Vorrang - siehe SetActivity()-Kommentar oben.
	if (m_pCine != nullptr)
	{
		SetThink(&CBaseMonster::CallMonsterThink);
		return;
	}

	if (m_Activity != ACT_WALK)
	{
		// Freies autonomes Wandern NUR mit gesetztem SF_AIGIRL_WANDER-
		// Spawnflag (Default aus) - sonst bewegt sie sich ausschliesslich
		// reaktiv (siehe AIGirlTouch()/TraceAttack()).
		if (FBitSet(pev->spawnflags, SF_AIGIRL_WANDER) && RANDOM_LONG(0, 49) == 1)
		{
			StartReactiveMove(nullptr);
		}
		else
		{
			SetThink(&CBaseMonster::CallMonsterThink);
		}
		return;
	}

	float flDist = (m_vecWanderDest - pev->origin).Length2D();
	if (flDist <= m_flGroundSpeed * 0.1f)
	{
		SetActivity(ACT_IDLE);
		SetThink(&CBaseMonster::CallMonsterThink);
		return;
	}

	MakeIdealYaw(m_vecWanderDest);
	ChangeYaw(pev->yaw_speed);
	if (!WALK_MOVE(ENT(pev), pev->ideal_yaw, m_flGroundSpeed * 0.1f, WALKMOVE_NORMAL))
	{
		// blockiert - aufgeben und wieder warten statt haengenzubleiben.
		SetActivity(ACT_IDLE);
		SetThink(&CBaseMonster::CallMonsterThink);
	}
}

//=========================================================
// entity_clustergod - CClusterGod.
//=========================================================
class CClusterGod : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT ShootClusterGrenades();
};
LINK_ENTITY_TO_CLASS(entity_clustergod, CClusterGod);

void CClusterGod::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SET_MODEL(ENT(pev), "models/null.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	// Retail FUN_100a2470 (0x100a2470): bbox mins (-16,-16,0), maxs (16,16,16),
	// takedamage 0, pev+0x160 (health) = 20.
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
	pev->takedamage = DAMAGE_NO;
	pev->health = 20.0f;

	SetThink(&CClusterGod::ShootClusterGrenades);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CClusterGod::Precache()
{
	PrecacheModel("models/artillary.mdl");
	UTIL_PrecacheOther("dml_cluster");
	PrecacheModel("models/null.mdl");
}

// Retail 0x100a2570. Verified: random pitch -90..-70 and yaw -90..90,
// MakeVectors, a trace of 2048 units from the origin (owner ignored, monsters
// not ignored); on a hit with a non-conveyor entity/world the axes are rebuilt
// from the hit plane normal (VecToAngles + MakeVectors); then four cluster
// grenades with the owner of the god, a RANDOM_LONG(-200,200) sideways offset,
// RANDOM_LONG(50,250) launch speed along the forward axis and a small
// RANDOM_LONG(-15,15)/(5,20) sideways/up scatter, model artillary.mdl; finally
// SUB_Remove after 5.0 s.
// Simplification: the exact composition of the offset/velocity vectors from
// those random terms was not recoverable from the decompile (stack-garbled);
// the terms above are combined in the most direct way. Damage is not passed in
// retail (value 20 is the unproven stand-in).
void CClusterGod::ShootClusterGrenades()
{
	pev->angles.x = RANDOM_FLOAT(-90, -70);
	pev->angles.y = RANDOM_FLOAT(-90, 90);
	pev->angles.z = 0;
	UTIL_MakeVectors(pev->angles);

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 2048, dont_ignore_monsters, pev->owner, &tr);

	if (tr.flFraction < 1.0f)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && !FBitSet(pHit->pev->flags, FL_CONVEYOR))
			UTIL_MakeVectors(UTIL_VecToAngles(tr.vecPlaneNormal));
	}

	entvars_t* pevOwner = pev->owner ? VARS(pev->owner) : pev;
	for (int i = 0; i < 4; i++)
	{
		Vector vecOffset = gpGlobals->v_right * RANDOM_LONG(-200, 200);
		Vector vecVel = gpGlobals->v_forward * RANDOM_LONG(50, 250) + gpGlobals->v_right * RANDOM_LONG(-15, 15) + gpGlobals->v_up * RANDOM_LONG(5, 20);
		CBaseEntity* pCluster = Dml_ShootClusterGrenade(pevOwner, pev->origin + vecOffset, vecVel, 20.0f);
		if (pCluster)
			SET_MODEL(ENT(pCluster->pev), "models/artillary.mdl");
	}

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 5.0;
}
