//=========================================================
// MAYAN0A's fauna/script-point classes with no SDK precedent:
// monster_cricket (CCricket), monster_dragonfly (CDragonfly),
// monster_ourano (COurano), monster_targetrocket
// (CTargetRocketLauncher + its runtime-only CTargetRocket
// projectile), decore_pteradon (CPteradon), decore_butterflyflock
// (CButterflyFlock), monster_butterfly (CButterfly).
//
// Decompiled fresh from gunman.dll this session (LINK wrappers +
// vtable dumps + named-method decompiles, cross-checked against but
// not substituted by findings/entity_review_list.csv's prior notes):
//   monster_cricket:  LINK @0x1006e260, vtable @0x100f7800,
//     Spawn @0x1006e5c0, Precache @0x1006e5a0, Classify @0x1006e2b0
//     (returns 17, see CLASS_INSECT_CRICKET in cbase.h), TakeDamage
//     @0x1006e580, CCricket::SquashTouch @0x1006ec80.
//   monster_dragonfly: LINK @0x1006ee30, vtable @0x100f7a38,
//     Spawn @0x1006eee0, Precache @0x1006eff0, CDragonfly::FallHack
//     @0x1006f0b0, CDragonfly::IdleThink @0x1006f210,
//     CDragonfly::Start @0x1006f270, CDragonfly::FlockLeaderThink
//     @0x1006f700.
//   monster_ourano:    LINK @0x10074d70, vtable @0x100f876c,
//     Spawn @0x10075950, Precache @0x10075910, Classify @0x10074fa0
//     (returns 3 = CLASS_HUMAN_PASSIVE - "friedlicher Dinosaurier"
//     per findings, harmless even though large).
//   monster_targetrocket: LINK @0x100a42d0, vtable @0x100ff718,
//     Spawn @0x100a43a0, Precache @0x100a4380,
//     CTargetRocketLauncher::OnUse @0x100a4410,
//     CTargetRocketLauncher::RocketShootThink @0x100a4430,
//     CTargetRocket::RocketExplodeTouch @0x100a3dd0,
//     CTargetRocket::IgniteThink @0x100a3e80,
//     CTargetRocket::AccelerateThink @0x100a4000.
//
// Also implemented this session (previously deferred twice, per
// gunman_custom_entities.cpp's file header: "CBaseBird-family flying
// fauna ... not attempted in this pass") - kept complete for MAYAN0A
// rather than deferred a third time:
//   decore_pteradon:  LINK @0x10069f60, vtable @0x100f6f24, Spawn
//     @0x10069ff0 -> shared CBaseBird spawn tail @0x100689a0 (also
//     used by decore_eagle per findings/entity_review_list.csv -
//     confirms decore_eagle/CChopper/CRustFlier's full CBaseBird base
//     with HuntThink/FlyTouch is a separate, still-undone task; only
//     Spawn/Precache stats are reproduced here, not HuntThink/FlyTouch
//     themselves).
//   decore_butterflyflock: LINK @0x1006fb70, ctor helper @0x1006fce0
//     (vtable @0x100f7c74, confirms findings' "Vtable 0x100f7c74"),
//     flock-spawn body @0x1006fd40 (loops iFlockSize times, each
//     iteration allocating a CButterfly at a random offset within
//     flFlockRadius of the flock point, then the flock entity itself
//     is removed - reproduced faithfully).
//   monster_butterfly: LINK @0x1006ff50, vtable @0x100f7ea0, Spawn
//     @0x10070000 -> shared init @0x10070200, CButterfly::IdleThink
//     @0x100703b0, ::Start @0x10070410, ::FormFlock @0x10070470,
//     ::FlockLeaderThink @0x10070dd0, ::FlockFollowerThink
//     @0x100712e0, ::FallHack @0x10070140 (architecture identical to
//     CDragonfly's IdleThink/Start/FlockLeaderThink/FallHack pattern -
//     same simplification applies, see CDragonfly's entry above).
//
// - CPteradon: only Spawn's concrete stats (bbox, MOVETYPE_FLY,
//   DAMAGE_AIM takedamage, health 10, random start frame) are
//   reproduced; CBaseBird::HuntThink/FlyTouch (real hunting flight AI
//   and player-collision push/crash behavior, shared with
//   decore_eagle/monster_human_chopper/monster_rustflier per
//   findings) are NOT reproduced - replaced with the same simplified
//   wandering-circular-flight Think already used for CDragonfly, and
//   damage uses the default CBaseMonster TakeDamage/Killed path
//   (still killable, matching the class being CLASS_ALIEN_PREY-ish
//   huntable fauna, just not via the exact original combat logic).
// - CButterfly: same simplification as CDragonfly (wandering flight
//   instead of true boids flock steering) - CButterflyFlock's own
//   spawn-loop-and-scatter behavior IS reproduced faithfully, since
//   that part is simple and fully decompiled.
//
// Simplified relative to the original (documented per-case):
// - CCricket: the decompiled SquashTouch (stomp-to-death when a
//   grounded player touches it: 100 DMG_CRUSH self-damage) is
//   reproduced exactly, but findings/entity_review_list.csv documents
//   a custom Schedule_t/Task_t jump-attack AI (StartTask 0x24/0x25,
//   turn-to-player vs. wander schedule selection) that was NOT
//   re-decompiled (out of scope for an ambient/minor creature).
//   Nachtrag 2026-09-06 (Nutzerwunsch: "die grillen koennten von den
//   monstern auch zertreten werden wie kakalarken oder dem spieler,
//   zudem dem monster zufaellige bewegungen wie bei der kakalarke
//   einbauen"): zwei Punkte ergaenzt, beide als dokumentierte
//   Vereinfachung anstelle der nicht decompilierten echten
//   Schedule_t/Task_t-KI: (1) SquashTouch reagiert jetzt zusaetzlich
//   auf jedes FL_MONSTER-Entity mit Bewegung, nicht nur den Spieler -
//   analog zu CRoach::Touch (roach.cpp), das ebenfalls per Kontakt
//   sofort stirbt. (2) CricketThink()/PickNewDest()/Move() sind 1:1
//   nach dem Vorbild von CRoach::MonsterThink/PickNewDest/Move
//   (roach.cpp) nachgebaut - zufaellige Ziel-Punkte im 128-511-Unit-
//   Radius, Wechsel zwischen ACT_IDLE und ACT_WALK, WALK_MOVE mit
//   Stuck-Erkennung. Kein Fluchtverhalten/keine Sprungangriffe
//   (jump-Sequenz existiert im Modell, aber die reale
//   Trigger-Bedingung dafuer ist nicht decompiliert) - reines
//   Wander-Verhalten.
// - CDragonfly: Spawn/Precache/the IdleThink-to-Start visibility gate
//   are reproduced closely, aber FlockLeaderThink's realer Koerper
//   ist ein Trace-Line-basiertes Ausweich-/Kreisflug-Verhalten (per
//   Decompile 2026-09-06 verifiziert, 0x1006f700, ~140 Zeilen):
//   Geschwindigkeits-Rampe bis max. 200 u/s (DAT_100ebce4, bestaetigt
//   und konsistent mit CTargetRocket::AccelerateThink's Boost-
//   Konstante), TraceLine-Vergleich (FUN_1005f470) zur Wahl der
//   Umlaufrichtung (im/gegen Uhrzeigersinn) um einen Punkt, sowie
//   zwei nicht weiter aufgeloeste Helferfunktionen (FUN_1006f170/
//   FUN_1006f2c0, vermutlich Landepunkt-Suche/-Pruefung). Wegen der
//   verbleibenden Unklarheit dieser beiden Helfer wurde auf eine
//   vollstaendige Nachbildung verzichtet (waere sonst nur unsichere
//   Vermutung als RE-Ergebnis ausgegeben) - weiterhin ersetzt durch
//   eine einfache langsame Kreisflug-Wanderung, rein kosmetische
//   Ambient-Fauna laut Findings ("kein Kampfverhalten", takedamage=0
//   confirmed in Spawn). FallHack's original purpose (per findings:
//   "eingestandener Workaround fuer das Absturzverhalten bei Tod")
//   doesn't apply since this reimplementation never lets it fall in
//   the first place (MOVETYPE_FLY, SOLID_NOT throughout) - omitted.
// - COurano: Spawn's exact skill-cvar health lookup
//   (FUN_100608d0(DAT_10136ec4)) was not traced to a specific
//   gSkillData field this session; a plausible fixed health is used
//   instead, documented as an open question. The custom TakeDamage
//   (@0x10075080) and Killed/gib-check overrides
//   (@0x10074dd0/0x10074e00/0x100520c0) are complex and not
//   reproduced - COurano uses default CBaseMonster TakeDamage/Killed,
//   consistent with findings' "kann getoetet werden" (still killable,
//   just via the generic path instead of the exact original one). No
//   custom wander/eat schedule (mentioned in findings) is
//   reproduced - default CBaseMonster idle stands in for it.
// - CTargetRocket: IgniteThink creates its confirmed attached flame
//   sprite and TE_BEAMFOLLOW trail. AccelerateThink reproduces the
//   target steering: normalized target direction times the retail
//   0.7 turn-rate constant plus the confirmed 200-unit minimum-speed
//   boost along the original launch vector. The custom 0x17/0x16
//   effect itself remains represented by the standard GoldSrc trail.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "effects.h"
#include "explode.h"
#include "weapons.h"
#include "saverestore.h"
#include "basebird.h"

namespace
{
int g_iTargetRocketMuzzleFlash = 0;

void PrecacheTargetRocketAssets()
{
	g_iTargetRocketMuzzleFlash = PrecacheModel("sprites/muzzleflash1.spr");
}
}

//=========================================================
// monster_cricket - CCricket
//=========================================================
class CCricket : public CBaseMonster
{
public:
	int MonsterProfile() override { return 5; } // retail slot 59
	void Spawn() override;
	void Precache() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int Classify() override { return CLASS_INSECT_CRICKET; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
	void EXPORT SquashTouch(CBaseEntity* pOther);
	// Nachtrag 2026-09-06: einfache Wander-KI, siehe Datei-Kopf-Kommentar.
	void EXPORT CricketThink();
	void PickNewDest();
	void Move(float flInterval);
	// Nachtrag 2026-09-06 (RE-Nachtrag "genauer decompilieren"): Flucht-Sprung,
	// siehe Datei-Kopf-Kommentar fuer die Decompile-Belege.
	void JumpAway(CBaseEntity* pThreat);
	float m_flNextJumpTime;
};
LINK_ENTITY_TO_CLASS(monster_cricket, CCricket);

void CCricket::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/cricket.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));

	pev->solid = SOLID_SLIDEBOX; // retail @0x1006e5c0: solid 3
	pev->movetype = MOVETYPE_STEP;
	pev->health = 1;
	pev->view_ofs = Vector(0, 0, 20);
	pev->flags |= FL_MONSTER;

	MonsterInit();
	// Retail (Capability-Schreibzugriff +0x220 nach MonsterInit): gleiche Bitoperation wie im Dekompilat.
	m_afCapability |= bits_CAP_MELEE_ATTACK1;
	SetTouch(&CCricket::SquashTouch);
	// Retail Spawn leaves MonsterInit's MonsterInitThink -> StartMonster ->
	// CallMonsterThink chain intact.  The replacement CricketThink callback
	// skipped that initialization and could leave the cricket without normal
	// AI, animation events or scripted_sequence control.
}

void CCricket::Precache()
{
	PrecacheModel("models/cricket.mdl");

	PrecacheSound("cricket/cricket_angry.wav");
	PrecacheSound("cricket/cricket_angry2.wav");
	PrecacheSound("cricket/cricket_angry3.wav");
	PrecacheSound("cricket/cricket_flinch.wav");
	PrecacheSound("cricket/cricket_idle1.wav");
	PrecacheSound("cricket/cricket_idle2.wav");
	PrecacheSound("cricket/cricket_idle3.wav");
	PrecacheSound("cricket/cricket_jump1.wav");
	PrecacheSound("cricket/cricket_jump2.wav");
	PrecacheSound("cricket/cricket_jump3.wav");
	PrecacheSound("cricket/cricket_splat1.wav");
	PrecacheSound("cricket/cricket_splat2.wav");
	PrecacheSound("cricket/cricket_splat3.wav");
}

void CCricket::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	// The class declares this virtual slot but has no custom event body in
	// the reconstructed SDK implementation.  Preserve the base monster
	// handling rather than leaving the vtable entry undefined at link time.
	CBaseMonster::HandleAnimEvent(pEvent);
}

void CCricket::SquashTouch(CBaseEntity* pOther)
{
	if (!pOther)
		return;

	if (pOther->IsPlayer() && FBitSet(pOther->pev->flags, FL_ONGROUND))
	{
		TakeDamage(pOther->pev, pOther->pev, 100, DMG_CRUSH);
		return;
	}

	// Nachtrag 2026-09-06 (Nutzerwunsch): auch andere Monster koennen
	// die Grille zertreten, analog zu CRoach::Touch (roach.cpp), das
	// bei jedem sich bewegenden Nicht-Spieler-Kontakt ebenfalls sofort
	// ausloest.
	if (FBitSet(pOther->pev->flags, FL_MONSTER) && pOther->pev->velocity != g_vecZero)
	{
		TakeDamage(pOther->pev, pOther->pev, 100, DMG_CRUSH);
	}
}

void CCricket::CricketThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	float flInterval = StudioFrameAdvance();

	// Nachtrag 2026-09-06 (RE-Nachtrag): Flucht-Sprung, wenn sich etwas auf
	// CheckMeleeAttack1-Reichweite (256 Units, DAT_100e93c0) naehert -
	// siehe Datei-Kopf-Kommentar. Hat Vorrang vor dem normalen Wandern.
	if (gpGlobals->time >= m_flNextJumpTime)
	{
		CBaseEntity* pPlayer = UTIL_FindEntityByClassname(nullptr, "player");
		if (pPlayer && (pPlayer->pev->origin - pev->origin).Length() <= 256.0f)
		{
			JumpAway(pPlayer);
			return;
		}
	}

	// Waehrend des Fluchtsprungs (kein FL_ONGROUND) ueberlaesst der Think
	// die Bewegung der normalen MOVETYPE_STEP-Physik statt WALK_MOVE zu
	// rufen, sonst wuerde die ballistische Sprunggeschwindigkeit sofort
	// wieder von der Wander-Logik ueberschrieben.
	if (!FBitSet(pev->flags, FL_ONGROUND))
	{
		// nichts zu tun, Schwerkraft/Physik traegt die Grille
	}
	else if (m_Activity == ACT_HOP)
	{
		// gelandet - zurueck in den Normalzustand, sonst bleibt die
		// Sprunganimation eingefroren stehen.
		SetActivity(ACT_IDLE);
	}
	else if (m_flGroundSpeed != 0)
	{
		Move(flInterval);
	}
	else if (RANDOM_LONG(0, 49) == 1)
	{
		// gelegentlich aus dem Stand losgehen (Langeweile), analog zu
		// CRoach::MonsterThink's ROACH_BORED-Fall.
		PickNewDest();
		SetActivity(ACT_WALK);
	}
}

void CCricket::JumpAway(CBaseEntity* pThreat)
{
	// Nachtrag 2026-09-06 (RE-Nachtrag "genauer decompilieren"): CCricket
	// hat in gunman.dll eine eigene CheckMeleeAttack1 (Vtable-Slot 82,
	// FUN_1006ec40 @0x1006ec40: Distanz <= DAT_100e93c0=256.0 UND ein
	// Winkel-/Cooldown-Flag-Bit 0x200), eine eigene ScheduleFromName
	// (Slot 84, 0x1006e2d0, 2 eigene Schedule-Namen), StartTask/RunTask
	// fuer Task-IDs 0x24/0x25 (Slot 85/86, 0x1006e9f0/0x1006e970) sowie
	// eine eigene GetScheduleOfType (Slot 87, 0x1006ecc0). StartTask
	// spielt einen von drei "cricket_jumpN.wav"-Sounds ab, loescht das
	// 0x200-Cooldown-Flag und setzt pev->velocity direkt auf eine
	// ballistische Flugbahn (Skalierung DAT_100f2094=350.0), RunTask
	// wartet auf Sequenzende. Die Grille hat laut der neu decompilierten
	// 21x21-IRelationship-Tabelle (siehe cricket_irelationship_and_wander.md)
	// fast durchgehend R_FR (Furcht) zu allen anderen Klassen - das
	// vollstaendige Schedule_t/Task_t-Wiring (Task-Arrays, Interrupt-
	// Masken der beiden eigenen Schedules) wurde NICHT reproduziert
	// (grosser Zusatzaufwand fuer eine Nebenkreatur), stattdessen wird
	// hier direkt (ohne AI-Scheduler) ein Fluchtsprung WEG vom
	// Ausloeser ausgefuehrt, sobald dieser in Reichweite kommt - mit den
	// beiden konkret decompilierten Zahlenwerten (256 Units Ausloese-
	// Reichweite, 2.0s Cooldown ueber DAT_100e9d74) sowie der
	// Sprunggeschwindigkeit (350) uebernommen.
	constexpr float JUMP_COOLDOWN = 2.0f;
	constexpr float JUMP_SPEED = 350.0f;

	m_flNextJumpTime = gpGlobals->time + JUMP_COOLDOWN;

	Vector vecAway = pev->origin - pThreat->pev->origin;
	vecAway.z = 0;
	if (vecAway.Length() < 1.0f)
		vecAway = Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), 0);
	vecAway = vecAway.Normalize();

	pev->velocity.x = vecAway.x * JUMP_SPEED;
	pev->velocity.y = vecAway.y * JUMP_SPEED;
	pev->velocity.z = JUMP_SPEED * 0.6f;

	MakeIdealYaw(pev->origin + vecAway * -64.0f); // Blick in Sprungrichtung
	SetActivity(ACT_HOP);

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "cricket/cricket_jump1.wav", 1, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "cricket/cricket_jump2.wav", 1, ATTN_NORM);
		break;
	default:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "cricket/cricket_jump3.wav", 1, ATTN_NORM);
		break;
	}
}

void CCricket::PickNewDest()
{
	Vector vecNewDir;
	Vector vecDest;

	do
	{
		vecNewDir.x = RANDOM_FLOAT(-1, 1);
		vecNewDir.y = RANDOM_FLOAT(-1, 1);
		float flDist = 128 + RANDOM_LONG(0, 383);
		vecDest = pev->origin + vecNewDir * flDist;
	} while ((vecDest - pev->origin).Length2D() < 64);

	m_Route[0].vecLocation.x = vecDest.x;
	m_Route[0].vecLocation.y = vecDest.y;
	m_Route[0].vecLocation.z = pev->origin.z;
	m_Route[0].iType = bits_MF_TO_LOCATION;
	m_movementGoal = RouteClassify(m_Route[0].iType);
}

void CCricket::Move(float flInterval)
{
	float flWaypointDist = (m_Route[m_iRouteIndex].vecLocation - pev->origin).Length2D();
	MakeIdealYaw(m_Route[m_iRouteIndex].vecLocation);
	ChangeYaw(pev->yaw_speed);

	if (RANDOM_LONG(0, 7) == 1)
	{
		// gelegentliche Blockade-Pruefung, wie bei CRoach::Move.
		if (!WALK_MOVE(ENT(pev), pev->ideal_yaw, 4, WALKMOVE_NORMAL))
		{
			PickNewDest();
			return;
		}
	}

	WALK_MOVE(ENT(pev), pev->ideal_yaw, m_flGroundSpeed * flInterval, WALKMOVE_NORMAL);

	if (flWaypointDist <= m_flGroundSpeed * flInterval)
	{
		SetActivity(ACT_IDLE);
	}
}

//=========================================================
// monster_dragonfly - CDragonfly. Ambient, harmless, non-damageable
// flying fauna (see file header re: FlockLeaderThink simplification).
//=========================================================
class CDragonfly : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT IdleThink();
	void EXPORT Start();
	void EXPORT FlockLeaderThink();

	Vector m_vecWanderDir;
	// Nachtrag 2026-09-06 (RE-Vertiefung): geglaettete Geschwindigkeit fuer
	// den Pitch/Banking-Effekt, siehe FlockLeaderThink-Kommentar.
	float m_flSmoothedSpeed;
};
LINK_ENTITY_TO_CLASS(monster_dragonfly, CDragonfly);

void CDragonfly::Spawn()
{
	Precache();

	pev->view_ofs = g_vecZero;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->skin = RANDOM_LONG(0, 2); // retail @0x1006eee0: entvars +0x110 is skin, not body
	pev->spawnflags |= 4;
	m_flFieldOfView = 0.2f; // member +0x124

	SET_MODEL(ENT(pev), "models/dragonfly.mdl");
	UTIL_SetSize(pev, Vector(-5, -5, 0), Vector(5, 5, 2));
	pev->frame = 0;

	SetThink(&CDragonfly::IdleThink);
	pev->nextthink = gpGlobals->time + 0.1; // retail: time + 0.1 (constant 0x100e99a0)
}

void CDragonfly::Precache()
{
	PrecacheModel("models/dragonfly.mdl");
}

void CDragonfly::IdleThink()
{
	pev->nextthink = gpGlobals->time + 0.3;
	float flInterval = StudioFrameAdvance();
	DispatchAnimEvents(flInterval);

	// Original checks whether the local player can see this entity
	// (FindClientInPVS-style) before switching to active flight;
	// approximated here with a simple distance check against the
	// nearest player.
	CBaseEntity* pPlayer = UTIL_FindEntityByClassname(nullptr, "player");
	if (pPlayer && (pPlayer->pev->origin - pev->origin).Length() < 1024)
	{
		SetThink(&CDragonfly::Start);
		pev->nextthink = gpGlobals->time;
	}
}

void CDragonfly::Start()
{
	pev->nextthink = gpGlobals->time;
	SetThink(&CDragonfly::FlockLeaderThink);

	m_vecWanderDir = UTIL_RandomBloodVector(); // reused as a cheap unit random direction helper
	m_vecWanderDir.z = 0;
	m_vecWanderDir = m_vecWanderDir.Normalize();
	m_flSmoothedSpeed = 40.0f;
}

void CDragonfly::FlockLeaderThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	float flInterval = StudioFrameAdvance();
	DispatchAnimEvents(flInterval);

	// Nachtrag 2026-09-06 (RE-Vertiefung "CDragonfly/CButterfly Boids-
	// Flugsteuerung vertiefen"): CDragonfly::FlockLeaderThink
	// (0x1006f700) ruft zu Beginn FUN_1006f170 (Pitch/Banking-Effekt)
	// und FUN_1006f2c0 (Hindernis-/Landeplatz-Pruefung per TraceLine)
	// auf, deren Koerper jetzt beide frisch decompiliert wurden.
	//
	// FUN_1006f170 ist eindeutig rekonstruierbar: exponentiell
	// geglaettete Geschwindigkeit (smoothed = smoothed*A + speed*B),
	// die Differenz zur aktuellen Geschwindigkeit wird geclampt und
	// als Pitch-Drehrate (avelocity.x) entgegen der aktuellen
	// Blickneigung gesetzt - ein rein kosmetischer "die Nase senkt/
	// hebt sich beim Beschleunigen/Abbremsen"-Effekt. Reproduziert
	// unten mit plausiblen (nicht decompilierten, da die konkreten
	// DAT_-Konstanten denselben Ghidra-Referenzauflösungs-Bug wie
	// mehrfach zuvor in diesem Projekt zeigten) Glaettungs-/Clamp-
	// Werten.
	//
	// FUN_1006f2c0 ist ein TraceLine von der aktuellen Position entlang
	// v_forward, Reichweite 125.0 (DAT_100f7c70, bestaetigt). Nachtrag
	// 2026-09-06 (Nutzerwunsch "die noch unaufgeloesten Funktionen bitte
	// auch nachgehen"): FUN_10008210/FUN_10008250/FUN_1000e400 sind
	// KEINE spezielle Landeplatz-Logik, sondern schlicht die drei
	// primitiven Vektor-Operatoren (Addition/Skalarmultiplikation/
	// Subtraktion) - nach deren Aufloesung folgen im Original bei
	// erfolgreichem ersten Trace zwei WEITERE, seitlich um v_right*12.0
	// (DAT_100e9380, bestaetigt) versetzte Kandidaten-Traces, deren
	// Distanz verglichen wird, um zur freieren Seite zu steuern
	// (Kern-Mechanismus jetzt klar, siehe CButterfly::FlockLeaderThink
	// unten fuer dieselbe Logik in lesbarerer Form). Die genaue
	// Verzweigungs-Bedingung (welcher tr.flFraction-Vergleichswert -
	// DAT_100e93e0 liest sich als 0.0, was fuer eine "Pfad frei"-
	// Bedingung ungewoehnlich waere) bleibt trotz aufgeloester
	// Bausteine unklar, da Ghidra fuer diese grosse Funktion mehrere
	// Stack-Slots erkennbar wiederverwendet/falsch zuordnet (u.a. ein
	// Zweig liest lokale Variablen lokal_14/10/c aus, die im
	// decompilierten Text nirgends zuvor beschrieben werden - klassisches
	// Ghidra-Artefakt bei Funktionen dieser Groesse). Ohne interaktive
	// Ghidra-GUI-Sitzung mit echtem Datenfluss-Tracing nicht weiter
	// zuverlaessig aufloesbar (siehe findings/entities/
	// mayan0a_fauna_re_followup_2026-09-06.md fuer Details). Reproduziert
	// wird daher weiterhin nur der eindeutig belegte AEUSSERE Teil:
	// vorausschauendes TraceLine (jetzt mit der bestaetigten Reichweite
	// 125.0), bei Blockade Ausweichen (neue Zufallsrichtung statt der
	// unklaren Zwei-Kandidaten-Feinauswahl), sonst normale
	// Kreisflug-Wanderung.
	constexpr float SMOOTH_A = 0.8f;
	constexpr float SMOOTH_B = 0.2f;
	constexpr float MAX_PITCH_RATE = 15.0f;
	constexpr float PITCH_SCALE = 0.15f;

	float flCurSpeed = pev->velocity.Length();
	float flSpeedDelta = flCurSpeed - m_flSmoothedSpeed;
	m_flSmoothedSpeed = m_flSmoothedSpeed * SMOOTH_A + flCurSpeed * SMOOTH_B;
	float flPitchRate = V_min(fabs(flSpeedDelta) * PITCH_SCALE, MAX_PITCH_RATE);
	pev->avelocity.x = -(flPitchRate + pev->angles.x);

	UTIL_MakeVectors(pev->angles);

	TraceResult tr;
	constexpr float LOOKAHEAD = 125.0f; // DAT_100f7c70, bestaetigt
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * LOOKAHEAD, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0f)
	{
		// Hindernis voraus (bestaetigter aeusserer Ast von FUN_1006f2c0) -
		// ausweichen statt weiter geradeaus zu fliegen.
		m_vecWanderDir = UTIL_RandomBloodVector();
		m_vecWanderDir.z = 0;
		m_vecWanderDir = m_vecWanderDir.Normalize();
		pev->angles.y = UTIL_VecToYaw(m_vecWanderDir);
	}
	else
	{
		// Simplified slow wandering circular flight - not the original's
		// full boids-style flock steering (see file header).
		pev->angles.y += 4.0;
		if (pev->angles.y > 360)
			pev->angles.y -= 360;
	}

	UTIL_MakeVectors(pev->angles);
	pev->velocity = gpGlobals->v_forward * 40;
}

//=========================================================
// monster_ourano - COurano. Harmless wandering dinosaur (see file
// header re: TakeDamage/skill-health simplification).
//=========================================================
class COurano : public CBaseMonster
{
public:
	int MonsterProfile() override { return 7; } // retail slot 59
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void Spawn() override;
	void Precache() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int Classify() override { return CLASS_HUMAN_PASSIVE; }
	// CORRECTION (2026-09-05): missing SetYawSpeed() override, same
	// root cause/fix as CFriendlyGunman (human_gunman.cpp) - see
	// findings/open_items_audit_2026-09-05.md.
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
	// BUG FIX (2026-09-05): scripted_sequence-driven "waiting" poses
	// get stomped mid-animation without this - see the
	// feedback-setactivity-act-idle-stomps-scripted-sequence memory
	// note (same fix as CFriendlyGunman etc).
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
	// Nachtrag 2026-09-06 (RE-Nachtrag, ausgeloest durch die offizielle
	// Website-Warnung "try to be nice to the Ouranos!",
	// official_manual_and_website.md): TraceAttack (0x10075080) frisch
	// decompiliert - vergleicht den Klassennamen des Angreifers
	// (pevAttacker) mit dem Literal-String "monster_raptor". Ist der
	// Angreifer KEIN Raptor, wird der eingehende Schaden mit 3.0
	// (DAT_100ea888, bestaetigt) multipliziert; Raptoren als natuerliche
	// Fressfeinde verursachen unveraendert (1x) Schaden. Erklaert die
	// Website-Warnung direkt: Ouranos sind gegen den Spieler (und alles
	// ausser Raptoren) extrem zerbrechlich - man toetet sie leicht aus
	// Versehen. Der zusaetzliche 10%-Chance-Zweig (Blutspritzer-Decal
	// Index 0xf7) ist rein kosmetisch, nicht reproduziert.
	//
	// Nachtrag 2 2026-09-06 (Nutzer-Meldung "reagiert nicht auf notarget"):
	// vollstaendige Vtable von COurano (0x100f876c, 150 Slots via
	// read_vtable_slots.py) frisch geprueft - entgegen der bisherigen
	// Annahme "komplett passiv, keine eigene KI" hat Ourano ein echtes,
	// eigenes GetSchedule()/GetScheduleOfType()/StartTask()/RunTask()-
	// System (Slots 85-88, @0x10075470/0x10075560/0x10075800/0x100756a0).
	// GetSchedule() (0x100756a0) verzweigt im ALERT-Zustand ausschliesslich
	// ueber bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE (Maske 0x300) -
	// die Flucht-/Flinch-Reaktion ist also NICHT sicht-/Ziel-basiert
	// (Look()/IRelationship), sondern reiner SCHADENS-Trigger. Das erklaert
	// die Nutzer-Beobachtung direkt und korrekt: der Cheat "notarget"
	// (FL_NOTARGET) unterdrueckt in CBaseMonster::Look() nur das
	// sicht-/ziel-basierte Erkennen (monsters.cpp:328) - er hat im
	// Original-Spiel NIE Einfluss auf eine reine Schadensreaktion, exakt
	// wie in Stock-Half-Life ("notarget" verhindert, dass Monster den
	// Spieler als Ziel waehlen, nicht dass sie auf eingehenden Schaden
	// reagieren). Kein Bug in FL_NOTARGET-Handling - stattdessen eine
	// bisher fehlende, echte RE-belegte Verhaltensschicht: bis zu diesem
	// Nachtrag hatte COurano ueberhaupt keine eigene Schedule/Task-Logik
	// (nur Classify/TraceAttack/SetYawSpeed/SetActivity), lief also komplett
	// ueber Stock-CBaseMonster-Fallback-Verhalten - das erklaert vermutlich
	// auch den fruehreren offenen Punkt "Triceratops stoesst Spieler nicht
	// zurueck": es gibt in Gunman gar keinen Rueckstoss-Angriff, sondern
	// eine Schadens-Flucht-/Flinch-Reaktion.
	//
	// Die exakten Schedule_t/Task_t-Tabellen (Gunman-eigene, ueber den
	// Stock-SDK-Bereich hinaus erweiterte SCHED_-Konstanten wie 0x2c/0x2e/
	// 0x2f/0x31, siehe GetScheduleOfType @0x10075800) sind reine
	// Datentabellen (keine Funktionen) und wurden NICHT bit-genau
	// nachgebaut - stattdessen eine dokumentierte, vereinfachte
	// Reproduktion des bestaetigten Kernverhaltens (Schaden -> Flinch-Sound
	// + Flucht vom Angreifer weg), nach demselben bereits etablierten
	// Reaktions-Muster wie CAIGirl::StartReactiveMove (rebar_fauna.cpp).
	// SmallFLinch/BigFLinch-Sequenzen (ACT_SMALL_FLINCH=26, beide im .mdl
	// mit derselben Activity-Nummer getaggt, per mdl_inspect.py bestaetigt)
	// und "run" (ACT_RUN, echte eigene Sequenz) genutzt.
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override
	{
		if (!pevAttacker || !FClassnameIs(pevAttacker, "monster_raptor"))
			flDamage *= 3.0f;
		StartFlee(pevAttacker ? CBaseEntity::Instance(pevAttacker) : nullptr, flDamage);
		CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
	}

	void EXPORT OuranoThink();

private:
	void StartFlee(CBaseEntity* pInstigator, float flDamage);

	Vector m_vecFleeDest = g_vecZero;
};
LINK_ENTITY_TO_CLASS(monster_ourano, COurano);

TYPEDESCRIPTION COurano::m_SaveData[] =
{
	DEFINE_FIELD(COurano, m_vecFleeDest, FIELD_POSITION_VECTOR),
};
IMPLEMENT_SAVERESTORE(COurano, CBaseMonster);

void COurano::StartFlee(CBaseEntity* pInstigator, float flDamage)
{
	if (m_pCine != nullptr)
		return;

	const char* pszFlinchSound = (flDamage >= 20.0f) ? "ourano/Ourano_BigFlinch_F0.wav" : "ourano/Ourano_SmallFlinch_F0.wav";
	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszFlinchSound, 1.0, ATTN_NORM);
	SetActivity(ACT_SMALL_FLINCH);

	Vector vecDir;
	if (pInstigator != nullptr)
		vecDir = (pev->origin - pInstigator->pev->origin).Normalize();
	else
		vecDir = Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), 0).Normalize();

	float flDist = 200 + RANDOM_LONG(0, 300);
	m_vecFleeDest = pev->origin + vecDir * flDist;

	// Keep the reduced custom path only for the damage reaction. Retail Spawn
	// otherwise leaves MonsterInit's normal CallMonsterThink lifecycle intact.
	SetThink(&COurano::OuranoThink);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void COurano::OuranoThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if (m_pCine != nullptr)
	{
		SetThink(&CBaseMonster::CallMonsterThink);
		return;
	}

	if (m_Activity == ACT_SMALL_FLINCH)
	{
		if (m_fSequenceFinished)
			SetActivity(ACT_RUN);
		return;
	}

	if (m_Activity != ACT_RUN)
	{
		SetThink(&CBaseMonster::CallMonsterThink);
		return;
	}

	float flDist = (m_vecFleeDest - pev->origin).Length2D();
	if (flDist <= m_flGroundSpeed * 0.1f)
	{
		SetActivity(ACT_IDLE);
		SetThink(&CBaseMonster::CallMonsterThink);
		return;
	}

	MakeIdealYaw(m_vecFleeDest);
	ChangeYaw(pev->yaw_speed);
	if (!WALK_MOVE(ENT(pev), pev->ideal_yaw, m_flGroundSpeed * 0.1f, WALKMOVE_NORMAL))
	{
		SetActivity(ACT_IDLE);
		SetThink(&CBaseMonster::CallMonsterThink);
	}
}

void COurano::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/ourano.mdl");
	UTIL_SetSize(pev, Vector(-40, -40, 0), Vector(40, 40, 60));

	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->flags |= FL_MONSTER;
	// Nachtrag 2026-09-06 (RE-Nachtrag "genauer decompilieren", Fauna/Cricket):
	// COurano::Spawn (0x10075950) ruft FUN_100608d0(DAT_10136ec4) auf, um
	// pev->health zu setzen. DAT_10136ec4 ist zur Compile-Zeit 0 (ein zur
	// Laufzeit von Skill.cfg befuellter globaler Basiswert, nicht statisch
	// rekonstruierbar), FUN_100608d0 selbst decompiliert zu
	// "base + RANDOM_FLOAT(0,base) * (skillGlobal * ratio)" - eine
	// zufaellige Gesundheitsvarianz. Die echte Retail-Skill.cfg
	// (Gunman-ENG-GER/rewolf/Skill.cfg, jetzt auch in mod-src/base/)
	// bestaetigt beide Cvar-Werte: "sk_ourano_h1/h2/h3"=70/70/80 (Basis)
	// und "sk_percent_random_h1/h2/h3"=6/12/18 (Skill-globale
	// Zufallsvarianz in Prozent, gilt vermutlich fuer mehrere
	// FUN_100608d0-Aufrufer, nicht nur Ourano). Der Skalierungsfaktor
	// (Prozent -> Bruch, 0.01) ist nicht direkt decompiliert (derselbe
	// Ghidra-Referenzauflösungs-Bug wie beim Panzer-Rakete-Radius liefert
	// fuer die rohe Ratio-Konstante einen unplausiblen Wert), aber
	// zwingend naheliegend angesichts der Cvar-Werte als ganze Prozentzahlen.
	pev->health = gSkillData.ouranoHealth + RANDOM_FLOAT(0, gSkillData.ouranoHealth) * (gSkillData.percentRandomHealth * 0.01f);
	m_flFieldOfView = -0.7f; // Retail @0x10075950
	pev->view_ofs = Vector(0, 0, 40);

	// Retail leaves MonsterInit's MonsterInitThink -> CallMonsterThink
	// lifecycle in place. Replacing it here starves all normal schedules,
	// animation advancement and scripted_sequence hand-off.
	MonsterInit();
	// Retail (Capability-Schreibzugriff +0x220 nach MonsterInit): gleiche Bitoperation wie im Dekompilat.
	m_afCapability |= bits_CAP_SQUAD;
}

void COurano::Precache()
{
	PrecacheModel("models/ourano.mdl");

	PrecacheSound("ourano/Ourano_BigFlinch_F0.wav");
	PrecacheSound("ourano/Ourano_SmallFlinch_F0.wav");
	PrecacheSound("ourano/Ourano_SeePlayer.wav");
	PrecacheSound("ourano/Ourano_Roar_F0.wav");
	PrecacheSound("ourano/Ourano_QuickAttack_F0.wav");
	PrecacheSound("ourano/Ourano_Idle_F0.wav");
	PrecacheSound("ourano/Ourano_Idle_2_F0.wav");
	PrecacheSound("ourano/Ourano_FootStep1.wav");
	PrecacheSound("ourano/Ourano_FootStep2.wav");
	PrecacheSound("ourano/Ourano_FootStep3.wav");
	PrecacheSound("ourano/Ourano_FootStep4.wav");
	PrecacheSound("ourano/Ourano_DieSimple_F0.wav");
	PrecacheSound("ourano/Ourano_DieDrastic_F0.wav");
	PrecacheSound("ourano/Ourano_Chew_F0.wav");
	PrecacheSound("ourano/Ourano_Bite_F0.wav");
	PrecacheSound("ourano/Ourano_MoveAttack_Vocals.wav");
	PrecacheSound("ourano/Ourano_MoveAttack_FirstStep.wav");
	PrecacheSound("ourano/Ourano_MoveAttack_SecondStep.wav");
	PrecacheModel("sprites/gorehuman.spr");
	PrecacheModel("sprites/gibhuman.spr");
}

void COurano::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
	case 2:
		// Retail FUN_10075190: both QuickAttack events use the same
		// 96-unit close-range attack and DAT_10136ec0 (sk_ourano_d).
		CheckTraceHullAttack(96.0f, gSkillData.ouranoDamage, DMG_SLASH);
		break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// monster_targetrocket - CTargetRocketLauncher: an invisible,
// solid-less script point. Use() arms it to fire exactly one
// CTargetRocket at its "target" keyvalue entity on the next think,
// then removes itself (matches decompiled RocketShootThink setting
// Think = CBaseEntity::SUB_Remove at the end).
//=========================================================
// Forward-deklariert, Definition nach CTargetRocket unten - vermeidet eine
// grosse Umsortierung der Klassen nur wegen dieser einen Zuweisung.
void TargetRocket_InitHoming(CBaseEntity* pRocket, CBaseEntity* pTarget);

class CTargetRocketLauncher : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT RocketShootThink();
};
LINK_ENTITY_TO_CLASS(monster_targetrocket, CTargetRocketLauncher);

void CTargetRocketLauncher::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/null.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	SetUse(&CTargetRocketLauncher::Use);
	SetThink(&CTargetRocketLauncher::RocketShootThink);
}

void CTargetRocketLauncher::Precache()
{
	PrecacheModel("models/null.mdl");
	UTIL_PrecacheOther("target_rocket");
	PrecacheTargetRocketAssets();
}

void CTargetRocketLauncher::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	pev->nextthink = gpGlobals->time;
}

void CTargetRocketLauncher::RocketShootThink()
{
	CBaseEntity* pTarget = UTIL_FindEntityByTargetname(nullptr, STRING(pev->target));
	if (!pTarget)
	{
		UTIL_Remove(this);
		return;
	}

	UTIL_MakeVectors(pev->angles);
	CBaseEntity* pRocket = CBaseEntity::Create("target_rocket", pev->origin, pev->angles, edict());
	if (pRocket)
	{
		pRocket->pev->target = pev->target;
		UTIL_SetOrigin(pRocket->pev, pev->origin);
		pRocket->pev->velocity = gpGlobals->v_forward * 100.0f;
		TargetRocket_InitHoming(pRocket, pTarget);
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_iTargetRocketMuzzleFlash);
	WRITE_BYTE(5);
	WRITE_BYTE(12);
	MESSAGE_END();

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 1.0;
}

//=========================================================
// The runtime-only rocket projectile fired by monster_targetrocket.
// Retail registers it as target_rocket and derives it from CGrenade.
// It starts along the launcher's forward vector, then steers toward the
// target with a 0.7 velocity factor while retaining the launch direction
// as a 200-unit minimum-speed boost.
class CTargetRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT IgniteThink();
	void EXPORT RocketExplodeTouch(CBaseEntity* pOther);
	void EXPORT AccelerateThink();

	int m_iFireBeam = 0;
	Vector m_vecLaunchDir = g_vecZero;
	CSprite* m_pFlame = nullptr;
	CBaseEntity* m_pTarget = nullptr;
};
LINK_ENTITY_TO_CLASS(target_rocket, CTargetRocket);

TYPEDESCRIPTION CTargetRocket::m_SaveData[] =
	{
		DEFINE_FIELD(CTargetRocket, m_iFireBeam, FIELD_INTEGER),
		DEFINE_FIELD(CTargetRocket, m_vecLaunchDir, FIELD_VECTOR),
		DEFINE_FIELD(CTargetRocket, m_pFlame, FIELD_CLASSPTR),
		DEFINE_FIELD(CTargetRocket, m_pTarget, FIELD_CLASSPTR),
	};

IMPLEMENT_SAVERESTORE(CTargetRocket, CGrenade);

void TargetRocket_InitHoming(CBaseEntity* pRocket, CBaseEntity* pTarget)
{
	auto* pRealRocket = static_cast<CTargetRocket*>(pRocket);
	pRealRocket->m_pTarget = pTarget;
}

void CTargetRocket::Precache()
{
	PrecacheModel("models/rocket.mdl");
	m_iFireBeam = PrecacheModel("sprites/firebeam.spr");
	PrecacheModel("sprites/flame.spr");
	PrecacheSound("weapons/rocket1.wav");
	PrecacheSound("gunner/gunner_boom1.wav");
	PrecacheSound("gunner/gunner_boom2.wav");
	PrecacheSound("gunner/gunner_boom3.wav");
}

void CTargetRocket::Spawn()
{
	Precache();

	pev->effects = 5;
	SET_MODEL(ENT(pev), "models/rocket.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	UTIL_MakeVectors(pev->angles);
	m_vecLaunchDir = gpGlobals->v_forward;
	pev->dmg = 80.0f;
	pev->gravity = 2.0f; // Retail Spawn @0x100a3cd0: +0x11c = 2.0
	SetTouch(&CTargetRocket::RocketExplodeTouch);
	SetThink(&CTargetRocket::IgniteThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CTargetRocket::IgniteThink()
{
	m_pFlame = CSprite::SpriteCreate("sprites/flame.spr", pev->origin, true);
	if (m_pFlame)
	{
		m_pFlame->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
		m_pFlame->SetAttachment(edict(), 1);
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/rocket1.wav", 0.5, 0.5, 0, 100);
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(m_iFireBeam);
	WRITE_BYTE(15);
	WRITE_BYTE(5);
	WRITE_BYTE(224);
	WRITE_BYTE(224);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	MESSAGE_END();

	SetThink(&CTargetRocket::AccelerateThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CTargetRocket::AccelerateThink()
{
	constexpr float WORLD_BOUNDS = 4096.0f;
	if (pev->origin.x < -WORLD_BOUNDS || pev->origin.x > WORLD_BOUNDS ||
		pev->origin.y < -WORLD_BOUNDS || pev->origin.y > WORLD_BOUNDS ||
		pev->origin.z < -WORLD_BOUNDS || pev->origin.z > WORLD_BOUNDS)
	{
		if (m_pFlame)
			UTIL_Remove(m_pFlame);
		UTIL_Remove(this);
		return;
	}

	if (m_pTarget)
	{
		Vector vecToTarget = (m_pTarget->pev->origin - pev->origin).Normalize();
		pev->velocity = pev->velocity + vecToTarget * 0.7f;
	}

	constexpr float MIN_SPEED = 1800.0f;
	constexpr float BOOST_SCALE = 200.0f;
	if (pev->velocity.Length() < MIN_SPEED)
	{
		pev->velocity = pev->velocity + m_vecLaunchDir * BOOST_SCALE;
	}

	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CTargetRocket::RocketExplodeTouch(CBaseEntity* pOther)
{
	if (m_pFlame)
		UTIL_Remove(m_pFlame);

	const char* pszBoom;
	switch (RANDOM_LONG(1, 3))
	{
	case 1:
		pszBoom = "gunner/gunner_boom1.wav";
		break;
	case 2:
		pszBoom = "gunner/gunner_boom2.wav";
		break;
	default:
		pszBoom = "gunner/gunner_boom3.wav";
		break;
	}
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pszBoom, 1.0, ATTN_NORM, 0, 100);
	CGrenade::ExplodeTouch(pOther);
}

//=========================================================
// decore_pteradon - CPteradon. A large flying, killable creature
// (confirmed pev->takedamage=DAMAGE_AIM in Spawn, unlike the
// non-damageable dragonfly/butterfly ambient fauna). Shares its real
// hunting-flight AI (CBaseBird::HuntThink/FlyTouch) with decore_eagle
// and monster_human_chopper/monster_rustflier - not reproduced here,
// see file header.
//=========================================================
class CPteradon : public CBaseBird
{
public:
	void Spawn() override { SpawnBird("models/pteradon2.mdl"); }
	void Precache() override { PrecacheModel("models/pteradon2.mdl"); }
};
LINK_ENTITY_TO_CLASS(decore_pteradon, CPteradon);

//=========================================================
// monster_butterfly - CButterfly. Ambient, harmless, non-damageable
// flying fauna, spawned in bulk by decore_butterflyflock below. Same
// simplified-flight rationale as CDragonfly (see file header).
//=========================================================
class CButterfly : public CBaseMonster
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	void Spawn() override;
	void Precache() override;
	void EXPORT IdleThink();
	void ConfigureFlockMember(CButterfly* pLeader);

	static TYPEDESCRIPTION m_SaveData[];

private:
	// Retail's FlockInit @0x10071820 links every member to the first
	// butterfly and pushes it onto that leader's follower chain.
	EHANDLE m_hFlockLeader;
	EHANDLE m_hNextFlockmate;
};
LINK_ENTITY_TO_CLASS(monster_butterfly, CButterfly);

TYPEDESCRIPTION CButterfly::m_SaveData[] =
	{
		DEFINE_FIELD(CButterfly, m_hFlockLeader, FIELD_EHANDLE),
		DEFINE_FIELD(CButterfly, m_hNextFlockmate, FIELD_EHANDLE),
	};

IMPLEMENT_SAVERESTORE(CButterfly, CBaseMonster);

void CButterfly::Spawn()
{
	Precache();

	pev->view_ofs = g_vecZero;
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->skin = RANDOM_LONG(0, 9);
	pev->flags |= FL_FLY;

	SET_MODEL(ENT(pev), "models/butterfly.mdl");
	UTIL_SetSize(pev, Vector(-5, -5, -5), Vector(5, 5, 5));
	UTIL_SetOrigin(pev, pev->origin);

	SetThink(&CButterfly::IdleThink);
	// Retail CButterfly::Spawn @ 0x10070000 schedules the first idle
	// check after 0.1 seconds (DAT_100e99a0), not a random delay.
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CButterfly::ConfigureFlockMember(CButterfly* pLeader)
{
	if (pLeader == nullptr)
	{
		m_hFlockLeader = this;
		m_hNextFlockmate = nullptr;
	}
	else
	{
		m_hFlockLeader = pLeader;
		m_hNextFlockmate = pLeader->m_hNextFlockmate;
		pLeader->m_hNextFlockmate = this;
	}

	// The retail flock-spawn helper clears motion and begins its common
	// IdleThink path after a fixed 0.2-second delay (DAT_100ea858).
	pev->velocity = g_vecZero;
	SetThink(&CButterfly::IdleThink);
	pev->nextthink = gpGlobals->time + 0.2f;
}

void CButterfly::Precache()
{
	PrecacheModel("models/butterfly.mdl");
}

void CButterfly::IdleThink()
{
	// Retail CButterfly::IdleThink @ 0x100703b0 uses the same 0.2-second
	// cadence as the flock-spawn helper.
	pev->nextthink = gpGlobals->time + 0.2f;
	float flInterval = StudioFrameAdvance();
	DispatchAnimEvents(flInterval);

	// Nachtrag 2026-09-06 (RE-Vertiefung, dann "unaufgeloeste Funktionen
	// nachgehen"): CButterfly::FlockLeaderThink (0x10070dd0, frisch
	// decompiliert) nutzt denselben Ausweich-Mechanismus wie
	// CDragonfly::FlockLeaderThink/FUN_1006f2c0 - zwei seitlich um
	// v_right*192.0 (DAT_100f80cc, bestaetigt - der seitliche Versatz,
	// nicht die Vorausschau-Reichweite) versetzte Kandidaten-Traces,
	// Vergleich der jeweiligen Blockade-Distanz, Steuerung Richtung der
	// freieren Seite (bei Gleichstand zufaellig per RANDOM_LONG(0,1),
	// direkt im Funktionskoerper sichtbar - kein separater Helferaufruf
	// wie bei Dragonfly). Fuer eine eigene, direkte Vorausschau-
	// Reichweite (Dragonflys Aequivalent zu DAT_100f7c70=125.0) wurde
	// bei Butterfly keine eigene Konstante identifiziert; hier
	// weiterhin ein plausibler Schaetzwert. Die Retail-Spawnverkettung
	// (Leader + Follower-Kette) wird seit 2026-09-08 gespeichert, die
	// nachfolgende IdleThink->Start->FlockLeaderThink-Kette bleibt aber
	// unreproduziert (Datei-Kopf-Kommentar) - hier nur der gemeinsame Ausweich-
	// Mechanismus uebernommen, analog zu CDragonfly::FlockLeaderThink.
	UTIL_MakeVectors(pev->angles);
	TraceResult tr;
	constexpr float LOOKAHEAD = 64.0f; // Schaetzwert, siehe Kommentar oben
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * LOOKAHEAD, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0f)
	{
		pev->angles.y += RANDOM_FLOAT(60.0f, 150.0f) * (RANDOM_LONG(0, 1) != 0 ? 1.0f : -1.0f);
	}
	else
	{
		pev->angles.y += RANDOM_FLOAT(-8.0, 8.0);
	}

	UTIL_MakeVectors(pev->angles);
	pev->velocity = gpGlobals->v_forward * 20;
}

//=========================================================
// decore_butterflyflock - CButterflyFlock. Not a real entity in the
// running level - Spawn() scatters iFlockSize CButterfly instances
// within flFlockRadius of its own origin, then removes itself.
// Reproduced faithfully (the loop-and-scatter logic is simple and
// fully decompiled, unlike CButterfly's own flight AI - see header).
//=========================================================
class CButterflyFlock : public CBaseEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;

	int m_iFlockSize = 15;
	float m_flFlockRadius = 100.0;
};
LINK_ENTITY_TO_CLASS(decore_butterflyflock, CButterflyFlock);

bool CButterflyFlock::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "iFlockSize"))
	{
		m_iFlockSize = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "flFlockRadius"))
	{
		m_flFlockRadius = atof(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CButterflyFlock::Spawn()
{
	CButterfly* pLeader = nullptr;
	for (int i = 0; i < m_iFlockSize; i++)
	{
		Vector vecOffset(
			RANDOM_FLOAT(-m_flFlockRadius, m_flFlockRadius),
			RANDOM_FLOAT(-m_flFlockRadius, m_flFlockRadius),
			RANDOM_FLOAT(0, 16)); // Retail @0x1006fd40: 0x41800000

		CButterfly* pButterfly = static_cast<CButterfly*>(CBaseEntity::Create("monster_butterfly", pev->origin + vecOffset, pev->angles, edict()));
		if (pButterfly)
		{
			pButterfly->pev->angles = pev->angles;
			pButterfly->ConfigureFlockMember(pLeader);
			if (pLeader == nullptr)
				pLeader = pButterfly;
		}
	}

	UTIL_Remove(this);
}
