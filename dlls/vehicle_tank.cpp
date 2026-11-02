//=========================================================
// The Gunman Chronicles drivable Battle Tank system - a genuinely new
// entity family with no stock SDK counterpart at all (vehicle_tank,
// vehicle_tank_body/turret/barrel, trigger_tank/trigger_tankoutofgas/
// trigger_tankshell/trigger_tankeject, plus the runtime-only
// vehicle_tank_shell/vehicle_tank_rocket projectiles).
//
// Decompiled from gunman.dll and cross-verified against
// findings/entities/tank_vehicle_system.md (8 prior sessions of
// research on this exact system) - re-verified this session rather
// than taken on faith: constructor @0x100989d0, BuildVehicleThink
// @0x10098ee0, UseVehicle @0x1009e520, WithoutPlayerThink @0x10099270,
// CVehicleTankBSP::TankBSPUse/WaitThink @0x1009f7e0/0x1009f860, the
// eject/dismount routine @0x1009e950 and its jump-safety gate
// @0x1009e850, and all 4 trigger_tank* classes' Spawn/Touch/Use
// (@0x1009fb50/0x1009fbf0, 0x1009fd00/0x1009fda0, 0x1009f940/0x1009f9e0,
// 0x1005b5a0/0x1005b5b0) - all freshly decompiled this session, not
// just copied from the findings doc.
//
// The 2026-09-06 pass additionally re-decompiled the complete mount/input/
// camera/model-control chain plus the corresponding client HUD handlers.
// Remaining approximations are called out at their implementation sites;
// in particular the SDK implementation uses a conservative hull trace in
// place of Retail's larger custom vehicle-physics helpers.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "gamerules.h"
#include "weapons.h"
#include "saverestore.h"
#include "effects.h"
#include "explode.h"
#include "shake.h"
#include "UserMessages.h"

#define SF_TANK_NO_TURRET_WEAPONS 1
#define SF_TANK_NO_MACHINEGUNS 2

// BUG FIX 2026-11-01 (Nutzerbericht: Geschosse explodieren im Panzer, wenn man mit der Hauptwaffe schiesst):
// Geschoss und Rakete starten am Rohr (m_pBarrel), also innerhalb des Panzerkoerpers. Ihre Touch-Funktionen
// trafen daher sofort den eigenen Panzer, seine Teile (vehicle_tank_body/turret/barrel) oder den Fahrer.
// Diese Hilfsfunktion ignoriert genau diese Ziele. Der Fahrer wird ueber den Abstand zum Panzer erkannt
// (Naeherung, nicht Retail-belegt). Andere Ziele bleiben unveraendert.
static bool IgnoreOwnTankTarget(CBaseEntity* pOther, entvars_t* pevProjectile)
{
	if (pOther == nullptr || pevProjectile == nullptr)
		return false;
	if (pevProjectile->owner != nullptr && pOther->edict() == pevProjectile->owner)
		return true;
	if (strncmp(STRING(pOther->pev->classname), "vehicle_tank", 12) == 0)
		return true;
	if (pOther->IsPlayer() && pevProjectile->owner != nullptr)
	{
		const Vector vecTank = VARS(pevProjectile->owner)->origin;
		if ((pOther->pev->origin - vecTank).Length() < 120.0f)
			return true;
	}
	return false;
}

class CVehicleTank;

//=========================================================
// vehicle_tank_body / vehicle_tank_turret / vehicle_tank_barrel -
// CVehicleTankBSP. The three physical brush-model pieces, each placed
// separately in the map and linked to their controller via a shared
// "vehicle_id". Use() forwards to the matching CVehicleTank::UseVehicle
// so a mapper (or the player) can trigger any of the three parts to
// mount the vehicle.
//=========================================================
class CVehicleTankBSP : public CBaseEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	// BUG FIX (2026-09-05, live gameplay report): "einsteigen
	// funktioniert nicht". CBaseEntity's default ObjectCaps() only
	// returns FCAP_ACROSS_TRANSITION - no use-capability bit at all.
	// CBasePlayer::PlayerUse() (player.cpp) gates its whole +use
	// traceline dispatch on
	// `pObject->ObjectCaps() & (FCAP_IMPULSE_USE|FCAP_CONTINUOUS_USE|FCAP_ONOFF_USE)`
	// before ever calling Use() - without this override that check
	// always failed, so the already-correct Use()-forwarding chain
	// below was simply never reached. Matches the same idiom already
	// used on CVehicleTank itself.
	int ObjectCaps() override { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
	void EXPORT WaitThink();
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	int m_iVehicleID = 1;
};
LINK_ENTITY_TO_CLASS(vehicle_tank_body, CVehicleTankBSP);
LINK_ENTITY_TO_CLASS(vehicle_tank_turret, CVehicleTankBSP);
LINK_ENTITY_TO_CLASS(vehicle_tank_barrel, CVehicleTankBSP);

TYPEDESCRIPTION CVehicleTankBSP::m_SaveData[] =
{
	DEFINE_FIELD(CVehicleTankBSP, m_iVehicleID, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CVehicleTankBSP, CBaseEntity);

bool CVehicleTankBSP::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "vehicle_id"))
	{
		m_iVehicleID = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CVehicleTankBSP::Spawn()
{
	SET_MODEL(ENT(pev), STRING(pev->model));
	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;

	// Retail intentionally maps all three BSP parts as additive/alpha zero.
	// They are moving collision hulls only; models/tank.mdl on the controller
	// supplies the complete visible and animated vehicle.

	UTIL_SetOrigin(pev, pev->origin);

	// DispatchUse reaches the overridden virtual Use() directly. Do not also
	// install it as a saved callback: Xash then expects a standalone EXPORT
	// symbol and reports "No EXPORT" while restoring the map entity.
	SetThink(&CVehicleTankBSP::WaitThink);
	pev->nextthink = pev->ltime + 1.0;
}

void CVehicleTankBSP::WaitThink()
{
	pev->nextthink = pev->ltime + 1.0;
}

// CVehicleTankBSP::Use is defined further down, after CVehicleTank is
// fully declared (it needs to call CVehicleTank::Use on the matching
// controller entity).

//=========================================================
// vehicle_tank - CVehicleTank. The controller owns the complete visible
// models/tank.mdl and its animation/bone controllers. The three linked BSP
// entities remain invisible and provide the moving collision geometry.
//=========================================================
class CVehicleTank : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int ObjectCaps() override { return (CBaseMonster::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
	bool OnControls(entvars_t* pevTest) override { return m_pDriver && pevTest == m_pDriver->pev; }
	// The trigger_tank*/TouchTank family gates on Classify()==2, which the
	// decompiled code only observed to be true for a currently-manned
	// tank - reproduced directly via the driver pointer rather than a
	// separate flag.
	int Classify() override { return m_pDriver ? 2 : CLASS_NONE; }

	void EXPORT BuildVehicleThink();
	void EXPORT WithPlayerThink();
	void EXPORT WithoutPlayerThink();

	void Dismount();
	void FireChassisWeapon();
	void FireRocket();
	void FireAutoCannon();
	void UpdateVehicleParts();
	void UpdateDriverCamera();
	void SendTankState(int state);
	void SendTankBodyState(int state);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	int m_iVehicleID = 1;
	float m_flVolume = 1.0;

	CBaseEntity* m_pBarrel = nullptr;
	CBaseEntity* m_pTurret = nullptr;
	CBaseEntity* m_pBody = nullptr;

	// BUG FIX (2026-09-06, live gameplay report: "sieht aus wie ein Block,
	// nicht als Panzer erkennbar"). vehicle_tank_body/_turret/_barrel are
	// three separately-shaped BSP brushes placed by the mapper; checked
	// against all 8 retail map placements (end1/city3a/west5b/west6b/
	// west6c/west6d/west6e), turret/barrel always share body's X/Y exactly
	// and are offset only in Z (~+60/+72 units) - the turret sits on top of
	// the hull, the barrel above that. WithoutPlayerThink previously
	// collapsed all three onto the SAME origin every tick
	// (m_pTurret->pev->origin = m_pBody->pev->origin), stacking the three
	// distinct brush shapes on top of each other into what looks like a
	// single indistinct block. WithPlayerThink didn't move turret/barrel at
	// all while driving, so they'd visibly detach and stay behind. Fixed by
	// caching each part's original offset from the body once (below) and
	// re-applying it every tick instead of snapping to an identical origin.
	Vector m_vecTurretOffset = g_vecZero;
	Vector m_vecBarrelOffset = g_vecZero;

	CBaseEntity* m_pDriver = nullptr;
	float m_flChassisSpeed = 0;
	float m_flChassisTurnRate = 0;
	float m_flNextChassisFire = 0;
	float m_flNextRocketFire = 0;
	float m_flNextJumpCheck = 0;
	float m_flNextAutoCannonFire = 0;
	float m_flNextBodyHud = 0;
	int m_iLastTurretHudFrame = -1;
};
LINK_ENTITY_TO_CLASS(vehicle_tank, CVehicleTank);

TYPEDESCRIPTION CVehicleTank::m_SaveData[] =
{
	DEFINE_FIELD(CVehicleTank, m_iVehicleID, FIELD_INTEGER),
	DEFINE_FIELD(CVehicleTank, m_flVolume, FIELD_FLOAT),
	DEFINE_FIELD(CVehicleTank, m_pBarrel, FIELD_CLASSPTR),
	DEFINE_FIELD(CVehicleTank, m_pTurret, FIELD_CLASSPTR),
	DEFINE_FIELD(CVehicleTank, m_pBody, FIELD_CLASSPTR),
	DEFINE_FIELD(CVehicleTank, m_vecTurretOffset, FIELD_VECTOR),
	DEFINE_FIELD(CVehicleTank, m_vecBarrelOffset, FIELD_VECTOR),
	DEFINE_FIELD(CVehicleTank, m_pDriver, FIELD_CLASSPTR),
	DEFINE_FIELD(CVehicleTank, m_flChassisSpeed, FIELD_FLOAT),
	DEFINE_FIELD(CVehicleTank, m_flChassisTurnRate, FIELD_FLOAT),
	DEFINE_FIELD(CVehicleTank, m_flNextChassisFire, FIELD_TIME),
	DEFINE_FIELD(CVehicleTank, m_flNextRocketFire, FIELD_TIME),
	DEFINE_FIELD(CVehicleTank, m_flNextJumpCheck, FIELD_TIME),
	DEFINE_FIELD(CVehicleTank, m_flNextAutoCannonFire, FIELD_TIME),
	DEFINE_FIELD(CVehicleTank, m_flNextBodyHud, FIELD_TIME),
	DEFINE_FIELD(CVehicleTank, m_iLastTurretHudFrame, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CVehicleTank, CBaseMonster);

bool CVehicleTank::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "vehicle_id"))
	{
		m_iVehicleID = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "vehicle_volume"))
	{
		m_flVolume = atof(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

void CVehicleTank::Precache()
{
	PrecacheModel("models/tank.mdl");
	PrecacheModel("models/rocket.mdl");
	PrecacheModel("models/artillary.mdl");
	PrecacheModel("models/dmlrocket.mdl");
	PrecacheModel("sprites/muzzleflash1.spr");
	PrecacheModel("sprites/smoke.spr");
	PrecacheSound("weapons/rocket1.wav");
	PrecacheSound("weapons/explode4.wav");
	PrecacheSound("tank/scrapewall.wav");
	PrecacheSound("tank/glug.wav");
	PrecacheSound("tank/startup.wav");
	PrecacheSound("tank/turn.wav");
	PrecacheSound("tank/ramwall.wav");
	PrecacheSound("tank/outofgas.wav");
	PrecacheSound("tank/powerdown.wav");
	PrecacheSound("tank/engineidle.wav");
	PrecacheSound("tank/firering.wav");
	PrecacheSound("ambience/flameburst1.wav");
}

void CVehicleTank::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_TRIGGER;
	pev->takedamage = DAMAGE_NO;
	pev->effects &= ~EF_NODRAW;
	SET_MODEL(ENT(pev), "models/tank.mdl");
	UTIL_SetSize(pev, Vector(-64, -64, 12), Vector(64, 64, 48));
	UTIL_SetOrigin(pev, pev->origin);
	pev->sequence = 0;
	pev->frame = 0;

	SetThink(&CVehicleTank::BuildVehicleThink);
	pev->nextthink = gpGlobals->time + 0.3;
}

void CVehicleTank::BuildVehicleThink()
{
	m_pBarrel = NULL;
	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityByClassname(pEnt, "vehicle_tank_barrel")) != NULL)
	{
		if (((CVehicleTankBSP*)pEnt)->m_iVehicleID == m_iVehicleID)
		{
			m_pBarrel = pEnt;
			break;
		}
	}
	pEnt = NULL;
	while ((pEnt = UTIL_FindEntityByClassname(pEnt, "vehicle_tank_turret")) != NULL)
	{
		if (((CVehicleTankBSP*)pEnt)->m_iVehicleID == m_iVehicleID)
		{
			m_pTurret = pEnt;
			break;
		}
	}
	pEnt = NULL;
	while ((pEnt = UTIL_FindEntityByClassname(pEnt, "vehicle_tank_body")) != NULL)
	{
		if (((CVehicleTankBSP*)pEnt)->m_iVehicleID == m_iVehicleID)
		{
			m_pBody = pEnt;
			break;
		}
	}

	if (!m_pBarrel || !m_pTurret || !m_pBody)
	{
		ALERT(at_error, "WARNING: VehicleID %d did NOT Link to all 3 parts\n", m_iVehicleID);
		pev->nextthink = gpGlobals->time + 1.0;
		return;
	}

	// Exact retail assembly distances. The mapped BSP origins use the same
	// vertical stack (body +0, turret +60, barrel +72), while the controller's
	// Studio model renders the complete tank.
	m_vecTurretOffset = Vector(0, 0, 60);
	m_vecBarrelOffset = Vector(0, 0, 72);

	SetThink(&CVehicleTank::WithoutPlayerThink);
	pev->nextthink = gpGlobals->time + 0.1;
	UTIL_SetOrigin(pev, m_pBody->pev->origin);
	UpdateVehicleParts();
}

void CVehicleTank::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		return;

	if (m_pDriver)
	{
		if (pActivator == m_pDriver && useType == USE_OFF)
			Dismount();
		return;
	}

	auto pPlayer = static_cast<CBasePlayer*>(pActivator);

	m_pDriver = pPlayer;
	pPlayer->m_pTank = this;

	// RE-bestaetigt (CVehicleTank::UseVehicle, 0x1009e520): der Fahrer wird
	// waehrend der Fahrt bewegungs-/kollisionslos und unsichtbar (die Sicht
	// laeuft ja ueber m_pBody), zusaetzlich immun gegen Lava-Schaden (der
	// Panzer schuetzt den Fahrer), und clientseitiges Waffen-/Kamera-Bob wird
	// deaktiviert, da die Sicht am Fahrzeug haengt statt am Spieler.
	pPlayer->pev->movetype = MOVETYPE_NOCLIP;
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->effects |= EF_NODRAW;
	pPlayer->pev->flags |= FL_IMMUNE_LAVA;
	pPlayer->m_afPhysicsFlags |= PFLAG_ONLADDER | PFLAG_DUCKING;
	if (pPlayer->m_pActiveItem)
		pPlayer->m_pActiveItem->Holster();
	// CBasePlayer::PostThink treats a non-zero weaponmodel while m_pTank
	// is set as having switched away from vehicle controls and immediately
	// sends USE_OFF. Mirror CFuncTank::StartControl by clearing both models
	// after holstering; Deploy() in Dismount restores the active weapon.
	pPlayer->pev->weaponmodel = 0;
	pPlayer->pev->viewmodel = 0;
	CLIENT_COMMAND(pPlayer->edict(), "cl_bob 0\n");
	CLIENT_COMMAND(pPlayer->edict(), "cl_viewbob 0\n");

	m_flChassisSpeed = 0;
	m_flChassisTurnRate = 0;
	m_flNextJumpCheck = gpGlobals->time + 0.5f;

	SetThink(&CVehicleTank::WithPlayerThink);
	pev->nextthink = gpGlobals->time;

	EMIT_SOUND_DYN(m_pBody->edict(), CHAN_STATIC, "tank/startup.wav", m_flVolume, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(m_pBody->edict(), CHAN_VOICE, "tank/engineidle.wav", 0, ATTN_NORM, 0, 100);
	SendTankState(1);
	UpdateDriverCamera();
}

void CVehicleTank::Dismount()
{
	if (!m_pDriver)
		return;

	auto pPlayer = static_cast<CBasePlayer*>(m_pDriver);
	pPlayer->m_pTank = nullptr;

	// Symmetrischer Rueckbau des Use()-Zustands, RE-bestaetigt (Dismount-
	// Routine @0x1009e950): movetype=3(MOVETYPE_WALK)/solid=3(SOLID_SLIDEBOX)
	// im Original, EF_NODRAW- und FL_IMMUNE_LAVA-Bits geloescht, cl_bob auf
	// den Originalwert "0.01" zurueckgesetzt.
	pPlayer->pev->movetype = MOVETYPE_WALK;
	pPlayer->pev->solid = SOLID_SLIDEBOX;
	pPlayer->pev->effects &= ~EF_NODRAW;
	pPlayer->pev->flags &= ~FL_IMMUNE_LAVA;
	pPlayer->m_afPhysicsFlags &= ~(PFLAG_ONLADDER | PFLAG_DUCKING);
	if (pPlayer->m_pActiveItem)
		pPlayer->m_pActiveItem->Deploy();
	CLIENT_COMMAND(pPlayer->edict(), "cl_bob 0.01\n");
	CLIENT_COMMAND(pPlayer->edict(), "cl_viewbob 1\n");

	UTIL_MakeVectors(pev->angles);
	Vector vecExit = pev->origin - gpGlobals->v_forward * 96 + Vector(0, 0, 36);
	UTIL_SetOrigin(pPlayer->pev, vecExit);

	EMIT_SOUND_DYN(m_pBody->edict(), CHAN_STATIC, "tank/powerdown.wav", m_flVolume, ATTN_NORM, 0, 100);
	EMIT_SOUND_DYN(m_pBody->edict(), CHAN_VOICE, "common/null.wav", 0, ATTN_NORM, 0, 100);
	SendTankState(0);

	m_pDriver = nullptr;
	m_flChassisSpeed = 0;
	m_flChassisTurnRate = 0;

	SetThink(&CVehicleTank::WithoutPlayerThink);
	pev->nextthink = gpGlobals->time;
}

void CVehicleTank::WithoutPlayerThink()
{
	pev->nextthink = gpGlobals->time + 0.02;
	if (!m_pBody)
	{
		SetThink(&CVehicleTank::BuildVehicleThink);
		return;
	}

	pev->sequence = 0;
	StudioFrameAdvance();
	UpdateVehicleParts();
}

void CVehicleTank::WithPlayerThink()
{
	pev->nextthink = gpGlobals->time + 0.02;

	if (!m_pDriver)
	{
		SetThink(&CVehicleTank::WithoutPlayerThink);
		return;
	}

	auto pPlayer = static_cast<CBasePlayer*>(m_pDriver);
	int buttons = pPlayer->pev->button;

	// Retail accelerates/decelerates rather than switching velocity instantly.
	if (FBitSet(buttons, IN_FORWARD))
		m_flChassisSpeed = V_min(240.0f, m_flChassisSpeed + 8.0f);
	else if (FBitSet(buttons, IN_BACK))
		m_flChassisSpeed = V_max(-160.0f, m_flChassisSpeed - 8.0f);
	else
		m_flChassisSpeed *= 0.88f;

	if (FBitSet(buttons, IN_MOVELEFT))
		m_flChassisTurnRate = V_max(-55.0f, m_flChassisTurnRate - 4.0f);
	else if (FBitSet(buttons, IN_MOVERIGHT))
		m_flChassisTurnRate = V_min(55.0f, m_flChassisTurnRate + 4.0f);
	else
		m_flChassisTurnRate *= 0.82f;

	pev->angles.y = UTIL_AngleMod(pev->angles.y + m_flChassisTurnRate * 0.02f);
	UTIL_MakeVectors(pev->angles);
	Vector vecWanted = pev->origin + gpGlobals->v_forward * (m_flChassisSpeed * 0.02f);

	// The trace starts inside the tank's own three BSP hulls. Temporarily
	// exclude just those linked parts, otherwise the trace is start-solid and
	// the vehicle can never move. The large SDK hull is the closest available
	// approximation to Retail's dedicated vehicle collision helpers.
	int iBodySolid = m_pBody->pev->solid;
	int iTurretSolid = m_pTurret->pev->solid;
	int iBarrelSolid = m_pBarrel->pev->solid;
	m_pBody->pev->solid = SOLID_NOT;
	m_pTurret->pev->solid = SOLID_NOT;
	m_pBarrel->pev->solid = SOLID_NOT;
	TraceResult tr;
	UTIL_TraceHull(pev->origin + Vector(0, 0, 24), vecWanted + Vector(0, 0, 24), dont_ignore_monsters, large_hull, edict(), &tr);
	m_pBody->pev->solid = iBodySolid;
	m_pTurret->pev->solid = iTurretSolid;
	m_pBarrel->pev->solid = iBarrelSolid;
	if (tr.flFraction < 1.0f && m_flNextBodyHud < gpGlobals->time)
	{
		// Retail's collision helper selects TankBody state 1/2 from the
		// impact-side dot product and rate-limits the flash to 2 seconds.
		// The SDK hull trace supplies the closest equivalent normal; this
		// sign mapping is an explicit approximation of Retail's custom trace.
		const float impactSide = DotProduct(vecWanted - pev->origin, tr.vecPlaneNormal);
		SendTankBodyState(impactSide <= 0.0f ? 2 : 1);
		m_flNextBodyHud = gpGlobals->time + 2.0f;
	}
	if (tr.flFraction > 0)
		UTIL_SetOrigin(pev, pev->origin + (vecWanted - pev->origin) * tr.flFraction);
	if (tr.flFraction < 1.0f)
		m_flChassisSpeed *= -0.2f;

	// Independent mouse aim: the BSP turret/barrel and the visible model's
	// four bone controllers all follow the driver's view, not chassis yaw.
	m_pTurret->pev->angles.y = pPlayer->pev->v_angle.y;
	m_pBarrel->pev->angles.y = pPlayer->pev->v_angle.y;
	m_pBarrel->pev->angles.x = V_max(-17.0f, V_min(90.0f, pPlayer->pev->v_angle.x));
	UpdateVehicleParts();
	UpdateDriverCamera();

	float flRelativeYaw = UTIL_AngleMod(m_pBarrel->pev->angles.y - pev->angles.y);
	SetBoneController(0, V_min(120.0f, flRelativeYaw));
	SetBoneController(1, V_min(120.0f, V_max(0.0f, flRelativeYaw - 120.0f)));
	SetBoneController(2, V_min(120.0f, V_max(0.0f, flRelativeYaw - 240.0f)));
	SetBoneController(3, m_pBarrel->pev->angles.x);

	int iHudFrame = ((int)(flRelativeYaw / 22.5f)) & 15;
	if (iHudFrame != m_iLastTurretHudFrame)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgTurretPos, NULL, pPlayer->pev);
		WRITE_SHORT(iHudFrame);
		MESSAGE_END();
		m_iLastTurretHudFrame = iHudFrame;
	}

	// Nachtrag 2026-09-06 (Live-Report "wackelt hin und her wie ein
	// laufender Motor"): models/tank.mdl's Sequenzen 0/1 heissen laut
	// mdl_inspect.py real "offline"/"engine" - nicht "steht"/"faehrt", wie
	// hier bisher angenommen. Solange ein Fahrer drin ist, laeuft der
	// Motor (engineidle.wav wird bereits bei Use() gestartet) - Sequenz 1
	// ("engine", das Leerlauf-Wackeln) gehoert daher durchgehend hierher,
	// nicht nur bei aktiver Bewegung. Der bisherige Code sprang bei jedem
	// Abbremsen/Anhalten (m_flChassisSpeed knapp unter/ueber der 1.0-
	// Schwelle) zwischen Sequenz 0 ("offline", starre Pose) und 1 hin und
	// her - genau das gemeldete Wackeln. Sequenz 0 bleibt fuer
	// WithoutPlayerThink() (echter Motor-aus-Zustand, kein Fahrer).
	pev->sequence = 1;
	StudioFrameAdvance();

	if (FBitSet(buttons, IN_ATTACK) && !FBitSet(pev->spawnflags, SF_TANK_NO_MACHINEGUNS))
		FireChassisWeapon();

	if (FBitSet(buttons, IN_ATTACK2) && !FBitSet(pev->spawnflags, SF_TANK_NO_TURRET_WEAPONS))
		FireRocket();

	// RE-bestaetigt (2026-09-06, FUN_1009f1f0 gefunden beim Verfolgen des
	// pev->dmg-Punkts): eine dritte, bisher komplett unbekannte Waffe -
	// ein automatisch zielender Hitscan-Kanonenschuss, ausgeloest ueber
	// IN_DUCK (nicht IN_ATTACK/IN_ATTACK2!) statt eines eigenen Tasten-
	// Inputs, solange der Panzer nicht im Wasser ist (FL_SWIM-Check im
	// Original). Vorher komplett nicht reproduziert.
	if (FBitSet(buttons, IN_DUCK) && (pev->flags & FL_SWIM) == 0)
		FireAutoCannon();

	// Jump = emergency dismount, same routine as the driver-facing Use.
	if (FBitSet(buttons, IN_JUMP) && m_flNextJumpCheck < gpGlobals->time)
	{
		Dismount();
	}
}

void CVehicleTank::UpdateVehicleParts()
{
	if (!m_pBody || !m_pTurret || !m_pBarrel)
		return;

	UTIL_MakeVectors(pev->angles);
	Vector vecUp = gpGlobals->v_up;
	m_pBody->pev->angles = pev->angles;
	UTIL_SetOrigin(m_pBody->pev, pev->origin);
	UTIL_SetOrigin(m_pTurret->pev, pev->origin + vecUp * 60.0f);
	UTIL_SetOrigin(m_pBarrel->pev, pev->origin + vecUp * 72.0f);
}

void CVehicleTank::UpdateDriverCamera()
{
	if (!m_pDriver)
		return;

	auto pPlayer = static_cast<CBasePlayer*>(m_pDriver);
	UTIL_MakeVectors(pPlayer->pev->v_angle);
	Vector vecFocus = pev->origin + Vector(0, 0, 72);
	Vector vecWanted = vecFocus - gpGlobals->v_forward * 150.0f + Vector(0, 0, 28);
	int iBodySolid = m_pBody ? m_pBody->pev->solid : SOLID_NOT;
	int iTurretSolid = m_pTurret ? m_pTurret->pev->solid : SOLID_NOT;
	int iBarrelSolid = m_pBarrel ? m_pBarrel->pev->solid : SOLID_NOT;
	if (m_pBody) m_pBody->pev->solid = SOLID_NOT;
	if (m_pTurret) m_pTurret->pev->solid = SOLID_NOT;
	if (m_pBarrel) m_pBarrel->pev->solid = SOLID_NOT;
	TraceResult tr;
	UTIL_TraceLine(vecFocus, vecWanted, ignore_monsters, edict(), &tr);
	if (m_pBody) m_pBody->pev->solid = iBodySolid;
	if (m_pTurret) m_pTurret->pev->solid = iTurretSolid;
	if (m_pBarrel) m_pBarrel->pev->solid = iBarrelSolid;
	UTIL_SetOrigin(pPlayer->pev, tr.vecEndPos);
	pPlayer->pev->velocity = g_vecZero;
}

void CVehicleTank::SendTankState(int state)
{
	CBaseEntity* pRecipient = m_pDriver;
	if (!pRecipient)
		return;
	MESSAGE_BEGIN(MSG_ONE, gmsgTank, NULL, pRecipient->pev);
	WRITE_SHORT(state);
	MESSAGE_END();
}

void CVehicleTank::SendTankBodyState(int state)
{
	if (!m_pDriver)
		return;
	MESSAGE_BEGIN(MSG_ONE, gmsgTankBody, NULL, m_pDriver->pev);
	WRITE_SHORT(state);
	MESSAGE_END();
}

void CVehicleTank::FireChassisWeapon()
{
	if (m_flNextChassisFire > gpGlobals->time)
		return;
	m_flNextChassisFire = gpGlobals->time + 0.15;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, m_pBarrel->pev->origin);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(m_pBarrel->pev->origin.x);
	WRITE_COORD(m_pBarrel->pev->origin.y);
	WRITE_COORD(m_pBarrel->pev->origin.z);
	WRITE_SHORT(PrecacheModel("sprites/muzzleflash1.spr"));
	WRITE_BYTE(5);
	WRITE_BYTE(200);
	MESSAGE_END();

	// RE-bestaetigt (2026-09-06, FUN_100a0170 komplett frisch decompiliert):
	// die Chassis-"MG" feuert tatsaechlich ein echtes, schnelles Projektil
	// (1800 u/s entlang der Rohrachse, exakter Retail-Konstantenwert) mit
	// CVehicleTankShell::ShellTouch als Touch-Callback - also mit voller
	// Explosions-Optik am Einschlagpunkt. Die fruehere Einstufung "rein
	// kosmetisch, kein Projektil" war falsch (basierte auf FUN_1009f000
	// allein, ohne die von dort aufgerufene FUN_100a0170 zu decompilieren).
	// Bestaetigt bleibt aber: dieser gepoolte Objektpfad ruft nie
	// CVehicleTankShell::Spawn() auf (nur den rohen Basis-Konstruktor plus
	// manuelle Feldzuweisungen), wodurch pev->dmg dort niemals auf 80
	// gesetzt wird - das Geschoss explodiert sichtbar, richtet aber mangels
	// initialisiertem pev->dmg keinen Schaden an. Reproduziert hier durch
	// ein echtes vehicle_tank_shell mit explizit auf 0 gesetztem pev->dmg.
	UTIL_MakeVectors(m_pBarrel->pev->angles);
	CBaseEntity* pRound = CBaseEntity::Create("vehicle_tank_shell", m_pBarrel->pev->origin, m_pBarrel->pev->angles, edict());
	if (pRound)
	{
		pRound->pev->movetype = MOVETYPE_FLY; // direct-fire round, not the lobbed main-cannon arc
		pRound->pev->gravity = 0;
		pRound->pev->velocity = gpGlobals->v_forward * 1800;
		pRound->pev->dmg = 0;
	}

	UTIL_ScreenShake(pev->origin, 30.0, 175.0, 0.75, 600.0);
}

void CVehicleTank::FireRocket()
{
	if (m_flNextRocketFire > gpGlobals->time)
		return;
	m_flNextRocketFire = gpGlobals->time + 1.5;

	UTIL_MakeVectors(m_pBarrel->pev->angles);
	Vector vecSrc = m_pBarrel->pev->origin + gpGlobals->v_forward * 32 + Vector(0, 0, 16);

	CBaseEntity* pRocket = CBaseEntity::Create("vehicle_tank_rocket", vecSrc, m_pBarrel->pev->angles, edict());
	if (pRocket)
		pRocket->pev->velocity = gpGlobals->v_forward * 500;
}

// RE-bestaetigt (FUN_1009f1f0, siehe findings/entities/tank_vehicle_system.md
// Nachtrag 2026-09-06): eine dritte Waffe - automatisch zielender
// Hitscan-Treffer entlang der Rohrachse, ausgeloest per IN_DUCK statt eines
// eigenen Feuerknopfs. Retail-Parameter: Reichweite 8192, Streuung ~2 Grad,
// Schaden 50 pro Treffer. Munitionstyp/Tracer-Frequenz-Feinheiten des
// Original-FireBullets-Aufrufs nicht bis zur letzten Konstante aufgeloest
// (Ghidra-Typinferenz unzuverlaessig bei diesem Aufruf, siehe Turret-Shoot()-
// Praezedenzfall) - BULLET_MONSTER_12MM/Cooldown 0.1s als plausible
// Naeherung uebernommen (deckt sich mit dem im Original beobachteten
// 0.1s-Wiederholungsintervall dieses Codezweigs).
void CVehicleTank::FireAutoCannon()
{
	if (m_flNextAutoCannonFire > gpGlobals->time)
		return;
	m_flNextAutoCannonFire = gpGlobals->time + 0.1;

	pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors(m_pBarrel->pev->angles);
	Vector vecSrc = m_pBarrel->pev->origin + gpGlobals->v_forward * 32;

	FireBullets(1, vecSrc, gpGlobals->v_forward, Vector(0.035, 0.035, 0.035), 8192, BULLET_MONSTER_12MM, 2, 50);
}

//=========================================================
// vehicle_tank_shell / vehicle_tank_rocket - the two projectile types,
// only ever created at runtime (never map-placed). Both re-verified
// against a fresh decompile of ShellTouch/RocketTouch/OrientThink/
// ShellSmokeThink/TankIgniteThink/RocketThink (2026-09-06).
//=========================================================
class CVehicleTankShell : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT ShellTouch(CBaseEntity* pOther);
	// RE-bestaetigt (OrientThink @0x100a0a30): waehrend des Flugs alle 0.1s
	// entlang der aktuellen Geschwindigkeit ausgerichtet (Wurfparabel-Nicken),
	// und selbst-entfernt wenn ausserhalb der Weltgrenzen (+-4096) - vorher
	// gar nicht reproduziert (Shell hatte kein Think).
	void EXPORT OrientThink();
	// RE-bestaetigt (ShellSmokeThink @0x100a0b00): nach dem Einschlag ein
	// animiertes Rauch-Sprite am Treffpunkt, danach entfernt sich die Shell
	// selbst - vorher entfernte sich die Shell sofort ohne Rauchpuff.
	void EXPORT ShellSmokeThink();
};
LINK_ENTITY_TO_CLASS(vehicle_tank_shell, CVehicleTankShell);

void CVehicleTankShell::Precache()
{
	PrecacheModel("models/artillary.mdl"); // sic - authentic typo in the original asset
	PrecacheModel("sprites/smoke.spr");
	PrecacheSound("weapons/rocket1.wav");
	PrecacheSound("tank/firering.wav");
}

void CVehicleTankShell::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("vehicle_tank_shell");
	SET_MODEL(ENT(pev), "models/artillary.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.6;
	// RE-bestaetigt (2026-09-06, FUN_1000e4d0/FUN_1000e140): der Schaden wird
	// in Retail aus dem projektil-eigenen pev->dmg gelesen, nicht aus einer
	// Literal-Konstante im Touch-Handler - hier als Spawn()-Default gesetzt.
	pev->dmg = 80;
	SetTouch(&CVehicleTankShell::ShellTouch);
	SetThink(&CVehicleTankShell::OrientThink);
	pev->nextthink = gpGlobals->time + 0.1;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "tank/firering.wav", 1.0, ATTN_NORM, 0, 100);
}

void CVehicleTankShell::OrientThink()
{
	// RE-bestaetigt: Weltgrenzen-Check (+-4096 je Achse), sonst Selbstentfernung.
	if (pev->origin.x < -4096 || pev->origin.x > 4096 ||
		pev->origin.y < -4096 || pev->origin.y > 4096 ||
		pev->origin.z < -4096 || pev->origin.z > 4096)
	{
		UTIL_Remove(this);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	pev->angles = UTIL_VecToAngles(pev->velocity);
}

void CVehicleTankShell::ShellTouch(CBaseEntity* pOther)
{
	if (IgnoreOwnTankTarget(pOther, pev))
		return;
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(30);
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects). Kein RE-Fund
	// - ergaenzt zusaetzlich zu den bereits sorgfaeltig RE-bestaetigten
	// Effekten unten, nicht anstelle davon.
	UTIL_ExplosionEffects(pev->origin, pev->dmg * 2.5f);

	if (pev->dmg > 0)
		// RE-bestaetigt: FUN_1000e4d0 ist bytegleich zu SDK-CBaseMonster::
		// RadiusDamage(pevInflictor, pevAttacker, flDamage, ...), das den
		// Radius intern als flDamage*2.5 berechnet (combat.cpp:1111) - kein
		// unabhaengiger Radius-Wert.
		RadiusDamage(pev->origin, pev, VARS(pev->owner), pev->dmg, pev->dmg * 2.5, CLASS_NONE, DMG_BLAST);

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->velocity = g_vecZero;
	SetTouch(nullptr);
	SetThink(&CVehicleTankShell::ShellSmokeThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CVehicleTankShell::ShellSmokeThink()
{
	CSprite* pSmoke = CSprite::SpriteCreate("sprites/smoke.spr", pev->origin, true);
	if (pSmoke)
	{
		pSmoke->SetScale(7.0);
		pSmoke->SetTransparency(kRenderTransAlpha, 255, 255, 255, 255, kRenderFxNoDissipation);
		pSmoke->AnimateAndDie(20.0);
		pSmoke->pev->origin.z += 16;
	}
	UTIL_Remove(this);
}

class CVehicleTankRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT TankIgniteThink();
	void EXPORT RocketThink();
	void EXPORT RocketTouch(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(vehicle_tank_rocket, CVehicleTankRocket);

void CVehicleTankRocket::Precache()
{
	PrecacheModel("models/dmlrocket.mdl");
	PrecacheModel("sprites/smoke.spr");
	PrecacheSound("tank/firering.wav");
	PrecacheSound("ambience/flameburst1.wav");
}

void CVehicleTankRocket::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("vehicle_tank_rocket");
	SET_MODEL(ENT(pev), "models/dmlrocket.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	// RE-bestaetigt (2026-09-06, FUN_1000e4d0/FUN_1000e140): siehe
	// CVehicleTankShell::Spawn's Kommentar - Schaden kommt aus pev->dmg.
	pev->dmg = 100;
	SetTouch(&CVehicleTankRocket::RocketTouch);
	// RE-bestaetigt (TankIgniteThink @0x100a1520): die Rakete startet mit
	// einem Zuend-/Feuerstoss-Effekt (TE-Nachricht + "ambience/
	// flameburst1.wav" + EF_LIGHT/0x40-Leuchtspur-Flag) und wechselt erst
	// danach in den eigentlichen Flug-/Lenk-Zustand RocketThink - vorher
	// startete die Reimplementierung direkt im Flugzustand ohne diesen
	// Zuend-Effekt.
	SetThink(&CVehicleTankRocket::TankIgniteThink);
	pev->nextthink = gpGlobals->time + 0.1;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "tank/firering.wav", 1.0, ATTN_NORM, 0, 100);
}

void CVehicleTankRocket::TankIgniteThink()
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_DLIGHT);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_BYTE(20);   // radius / 10
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(5); // life / 10
	WRITE_BYTE(0); // decay
	MESSAGE_END();

	pev->effects |= EF_LIGHT;
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "ambience/flameburst1.wav", 1.0, ATTN_NORM, 0, 100);

	SetThink(&CVehicleTankRocket::RocketThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CVehicleTankRocket::RocketThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	// RE-bestaetigt: Weltgrenzen-Check (+-4096 je Achse), sonst Selbstentfernung.
	if (pev->origin.x < -4096 || pev->origin.x > 4096 ||
		pev->origin.y < -4096 || pev->origin.y > 4096 ||
		pev->origin.z < -4096 || pev->origin.z > 4096)
	{
		UTIL_Remove(this);
		return;
	}

	// RE-bestaetigt (echte Zielsuche, nicht die bisher reproduzierte reine
	// Geradeausflug-Naeherung): CBaseMonster::Look(700) + BestVisibleEnemy()
	// zum (Neu-)Erwerb eines Ziels, dann ein geschwindigkeitsbegrenztes
	// Nachlenken (Kappung bei 20 Grad/Tick laut Retail-Konstante) statt
	// direktem Snap auf die Zielrichtung.
	if (!m_hEnemy || !m_hEnemy->IsAlive())
	{
		Look(700);
		m_hEnemy = BestVisibleEnemy();
	}

	if (m_hEnemy)
	{
		Vector vecToTarget = (m_hEnemy->BodyTarget(pev->origin) - pev->origin).Normalize();
		float flSpeed = pev->velocity.Length();
		Vector vecCurDir = pev->velocity.Normalize();

		float flDot = DotProduct(vecCurDir, vecToTarget);
		flDot = V_min(1.0f, V_max(-1.0f, flDot));
		float flAngleDiff = acos(flDot) * (180.0f / 3.14159265f);

		Vector vecNewDir;
		constexpr float MAX_TURN_DEGREES = 20.0f; // RE-bestaetigt: _DAT_100ebf30
		if (flAngleDiff > MAX_TURN_DEGREES)
		{
			float flFrac = MAX_TURN_DEGREES / flAngleDiff;
			vecNewDir = (vecCurDir * (1.0f - flFrac) + vecToTarget * flFrac).Normalize();
		}
		else
		{
			vecNewDir = vecToTarget;
		}

		pev->velocity = vecNewDir * flSpeed;
	}

	pev->angles = UTIL_VecToAngles(pev->velocity);

	// RE-bestaetigt (RocketSmokeThink @0x100a1ec0): eine kontinuierliche
	// Rauchfahne waehrend des Flugs (Retail: ein einzelnes, stetig
	// wachsendes Sprite, das bei Erreichen einer Groessenschwelle entfernt
	// und neu erzeugt wird). Vereinfacht hier auf eine Kette einzelner,
	// selbst-verblassender Rauchpuffs (CSprite::Expand()) pro Tick statt
	// eines einzelnen wachsend-zurueckgesetzten Objekts - der genaue
	// Dispatch-Mechanismus, der RocketSmokeThink neben RocketThink
	// anstoesst, wurde nicht aufgeloest (siehe Findings-Datei).
	CSprite* pPuff = CSprite::SpriteCreate("sprites/smoke.spr", pev->origin, false);
	if (pPuff)
	{
		pPuff->SetTransparency(kRenderTransAlpha, 200, 200, 200, 150, kRenderFxNoDissipation);
		pPuff->SetScale(1.0);
		pPuff->pev->movetype = MOVETYPE_NONE;
		pPuff->Expand(4.0, 30.0);
	}
}

void CVehicleTankRocket::RocketTouch(CBaseEntity* pOther)
{
	if (IgnoreOwnTankTarget(pOther, pev))
		return;
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(30);
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects).
	UTIL_ExplosionEffects(pev->origin, pev->dmg * 2.5f);

	// RE-bestaetigt: FUN_1000e4d0 ist bytegleich zu CBaseMonster::
	// RadiusDamage(pevInflictor, pevAttacker, flDamage, ...), das den
	// Radius intern als flDamage*2.5 berechnet - hier ueber die von
	// CGrenade/CBaseMonster geerbte 5-Parameter-Ueberladung genutzt statt
	// des freien ::RadiusDamage mit separatem Radius-Argument.
	CBaseMonster::RadiusDamage(pev, VARS(pev->owner), pev->dmg, CLASS_NONE, DMG_BLAST);
	UTIL_Remove(this);
}

//=========================================================
// trigger_tank / trigger_tankoutofgas / trigger_tankshell /
// trigger_tankeject - level-scripting triggers. All Touch-based except
// trigger_tankeject (Use-based). Spawn @0x1009fb50/0x1009fd00/
// 0x1009f940 share an identical pattern (solid=TRIGGER, non-solid to
// physics but touchable, model from the brush).
//=========================================================
class CVehicleTriggerTank : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT TouchTank(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(trigger_tank, CVehicleTriggerTank);

void CVehicleTriggerTank::Spawn()
{
	pev->classname = MAKE_STRING("trigger_tank");
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_PUSH;
	pev->effects |= EF_NODRAW;
	SET_MODEL(ENT(pev), STRING(pev->model));
	// Retail Spawn (frisch dekompiliert 2026-10-31): Groesse/Origin nach SET_MODEL neu setzen.
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	UTIL_SetOrigin(pev, pev->origin);
	SetTouch(&CVehicleTriggerTank::TouchTank);
}

void CVehicleTriggerTank::TouchTank(CBaseEntity* pOther)
{
	if (!FClassnameIs(pOther->pev, "vehicle_tank"))
		return;
	if (pOther->Classify() != 2) // manned tank only, see CVehicleTank::Classify()
		return;

	SUB_UseTargets(this, USE_TOGGLE, 0);
	UTIL_Remove(this);
}

class CVehicleTriggerTankGas : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT TouchTank(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(trigger_tankoutofgas, CVehicleTriggerTankGas);

void CVehicleTriggerTankGas::Spawn()
{
	pev->classname = MAKE_STRING("trigger_tankoutofgas");
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_PUSH;
	pev->effects |= EF_NODRAW;
	SET_MODEL(ENT(pev), STRING(pev->model));
	// Retail Spawn (frisch dekompiliert 2026-10-31): Groesse/Origin nach SET_MODEL neu setzen.
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	UTIL_SetOrigin(pev, pev->origin);
	SetTouch(&CVehicleTriggerTankGas::TouchTank);
}

void CVehicleTriggerTankGas::TouchTank(CBaseEntity* pOther)
{
	if (!FClassnameIs(pOther->pev, "vehicle_tank"))
		return;

	CVehicleTank* pTank = (CVehicleTank*)pOther;
	if (!pTank->m_pDriver)
		return;

	EMIT_SOUND_DYN(pOther->edict(), CHAN_STATIC, "tank/outofgas.wav", 1.0, ATTN_NORM, 0, 100);
	pTank->Dismount();

	SUB_UseTargets(this, USE_TOGGLE, 0);
	UTIL_Remove(this);
}

class CVehicleTriggerTankShell : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT TouchShell(CBaseEntity* pOther);
};
LINK_ENTITY_TO_CLASS(trigger_tankshell, CVehicleTriggerTankShell);

void CVehicleTriggerTankShell::Spawn()
{
	pev->classname = MAKE_STRING("trigger_tankshell");
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_PUSH;
	pev->effects |= EF_NODRAW;
	SET_MODEL(ENT(pev), STRING(pev->model));
	// Retail Spawn (frisch dekompiliert 2026-10-31): Groesse/Origin nach SET_MODEL neu setzen.
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	UTIL_SetOrigin(pev, pev->origin);
	SetTouch(&CVehicleTriggerTankShell::TouchShell);
}

void CVehicleTriggerTankShell::TouchShell(CBaseEntity* pOther)
{
	if (!FClassnameIs(pOther->pev, "vehicle_tank_shell") && !FClassnameIs(pOther->pev, "vehicle_tank_rocket"))
		return;

	SUB_UseTargets(this, USE_TOGGLE, 0);
	UTIL_Remove(this);
}

class CVehicleTriggerEject : public CBaseEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void EXPORT UseEject(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

private:
	int m_iszTarget = 0;
};
LINK_ENTITY_TO_CLASS(trigger_tankeject, CVehicleTriggerEject);

bool CVehicleTriggerEject::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "target"))
	{
		m_iszTarget = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CVehicleTriggerEject::Spawn()
{
	pev->classname = MAKE_STRING("trigger_tankeject");
	SetUse(&CVehicleTriggerEject::UseEject);
}

void CVehicleTriggerEject::UseEject(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (m_iszTarget == 0)
		return;

	CBaseEntity* pTarget = UTIL_FindEntityByTargetname(NULL, STRING(m_iszTarget));
	if (!pTarget || !FClassnameIs(pTarget->pev, "vehicle_tank"))
		return;

	((CVehicleTank*)pTarget)->Dismount();
}

void CVehicleTankBSP::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityByClassname(pEnt, "vehicle_tank")) != NULL)
	{
		auto pTank = static_cast<CVehicleTank*>(pEnt);
		if (pTank->m_iVehicleID == m_iVehicleID)
		{
			pTank->Use(pActivator, pCaller, useType, value);
			return;
		}
	}
}
