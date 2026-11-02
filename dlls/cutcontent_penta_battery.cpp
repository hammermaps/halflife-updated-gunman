//=========================================================
// Cut-Content-Rekonstruktion: monster_penta (CPenta) und
// monster_battery (CBatteryBot) - siehe findings/entities/
// cutcontent_penta_batterybot.md fuer die vollstaendige RE-Herleitung.
//
// Beide existierten laut frisch decompilierter Export-Tabelle einer
// bisher unbekannten Pre-Release-DLL (E3-2000-Demo, 08.05.2000,
// valvearchive.com) als vollstaendiger, kompilierter Code
// (monster_penta@0x10090da0, monster_battery@0x1005f400), fehlen aber
// in der Retail-Export-Tabelle - echter, vor Release entfernter
// Inhalt, KEIN retail-verifiziertes Gameplay. Beide Namen waren bereits
// zuvor unabhaengig als FGD-only-Eintraege bekannt
// (findings/fgd-diff/fgd_gunman_only.txt: "monster_penta"/
// "monster_rustbattery") - dieser Fund bestaetigt, dass es sich um
// echten, funktionsfaehigen Code handelte, nicht nur um tote
// Editor-Eintraege.
//
// WICHTIG: die eigentlichen gameplay-relevanten Funktionen
// (CPenta::StartTask/RunTask/GetScheduleOfType, CBatteryBot::Spawn/
// Precache/Classify) wurden NICHT decompiliert (keine EXPORT-markierten
// Symbole fuer CPenta gefunden; CBatteryBots einzige gefundene Exports
// sind FlyTouch@0x1005f660 und HuntThink@0x1005f760, siehe
// findings-Dokument fuer den vollen Decompile-Text). Diese
// Implementierung ist daher eine DOKUMENTIERTE REKONSTRUKTION auf
// Basis von: (a) den beiden real decompilierten CBatteryBot-Methoden,
// (b) den echten, retail-shipped .mdl-Dateien (Bones/Sequenzen/Events
// per tools/mdl_inspect.py verifiziert) und Sounds, (c) den
// Skill.cfg-Balancing-Werten aus der E3-Beta (siehe skill.h/
// gamerules.cpp), und (d) etablierten Mustern aus bereits
// re-verifizierten Monstern dieses Projekts (Chase+Melee-Zustands-
// maschine analog zu monster_cricket/human_chopper). Testbar per
// maps-src/cutcontent_test/ (mod-src/base/maps/cutcontent_test.bsp,
// "map cutcontent_test" in der Konsole).
//
// Nachtrag 2026-09-06 (In-Game-Smoke-Test via cutcontent_test.bsp):
// beide Klassen spawnen fehlerfrei. "%s has no view_ofs!"-Alert im
// Log ist bei beiden Modellen erwartet/harmlos (CBaseMonster::
// SetEyePosition, in MonsterInit() aufgerufen, liest die $eyeposition
// aus dem .mdl und ueberschreibt pev->view_ofs damit - beide retail-
// shipped, aber nie fertiggestellten Modelle haben dort keinen Wert
// gesetzt (0,0,0), wie es fuer ungenutzten Cut-Content zu erwarten
// ist). Der Smoke-Test deckte einen ECHTEN Bug auf: gSkillData.
// ouranoHealth/percentRandomHealth/pentaHealth/pentaBiteDamage lasen
// bislang IMMER 0 (fehlendes CVAR_REGISTER in game.cpp, siehe dortiger
// Kommentar) - jetzt behoben.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "weapons.h"
#include "soundent.h"

//=========================================================
// monster_penta - CPenta. Vollstaendig geriggtes Quadruped-Modell
// (29 Knochen inkl. Schwanz, 14 Sequenzen: idle_normal/idle_wary,
// vier idle_eat-Varianten, walk/run mit Schrittgeraeusch-Events,
// turn_left/right, attack_close mit 2 Angriffs-Events bei Frame
// 15/33, flinch_big/small, death_simple) - alle Sequenzen mit
// Standard-Stock-Activity-IDs getaggt (idle=ACT_IDLE, walk=ACT_WALK,
// run=ACT_RUN, attack_close=ACT_MELEE_ATTACK1, turn=ACT_TURN_LEFT/
// RIGHT, flinch=ACT_SMALL_FLINCH/BIG_FLINCH, death=ACT_DIESIMPLE),
// verifiziert per tools/mdl_inspect.py gegen die echte, retail-
// shipped models/penta.mdl. Health/Bissschaden aus der E3-Beta-
// Skill.cfg (gSkillData.pentaHealth/pentaBiteDamage, siehe
// gamerules.cpp) - in Retail entfernt, hier als historischer
// Balancing-Wert uebernommen.
//
// Reine Chase-und-Beiss-KI (kein Stock-AI-Scheduler-Wiring, dieselbe
// Vereinfachung wie bei anderen direkt-Think-basierten Monstern
// dieser Session, z.B. monster_cricket): waehrend der Spieler
// ausserhalb der Beiss-Reichweite ist, laeuft Penta darauf zu; in
// Reichweite wird die attack_close-Sequenz abgespielt und beim
// zweiten Angriffs-Event (Frame 33, Biss-Kontakt) Schaden
// zugefuegt.
//=========================================================
class CPenta : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_MONSTER; }
	void SetYawSpeed() override { pev->yaw_speed = (m_Activity == ACT_RUN) ? 90 : 70; }
	void EXPORT PentaThink();
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	// Nachtrag 2026-09-06: models/pentagibs.mdl war schon vorher
	// precacht, aber nie in GibMonster() verwendet - siehe
	// CBaseMonster::CustomGibModel() in basemonster.h.
	const char* CustomGibModel() override { return "models/pentagibs.mdl"; }

private:
	float m_flNextAttack = 0;
	bool m_bAttacking = false;
};
LINK_ENTITY_TO_CLASS(monster_penta, CPenta);

TYPEDESCRIPTION CPenta::m_SaveData[] =
{
	DEFINE_FIELD(CPenta, m_flNextAttack, FIELD_TIME),
	DEFINE_FIELD(CPenta, m_bAttacking, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CPenta, CBaseMonster);

void CPenta::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/penta.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 48));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->health = gSkillData.pentaHealth;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_RED;

	MonsterInit();
	SetActivity(ACT_IDLE);
	SetThink(&CPenta::PentaThink);
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.1, 0.5);
}

void CPenta::Precache()
{
	PrecacheModel("models/penta.mdl");
	PrecacheModel("models/pentagibs.mdl");

	PrecacheSound("penta/penta_attack1.wav");
	PrecacheSound("penta/penta_attack2.wav");
	PrecacheSound("penta/penta_death1.wav");
	PrecacheSound("penta/penta_foot1.wav");
	PrecacheSound("penta/penta_foot2.wav");
	PrecacheSound("penta/penta_foot3.wav");
	PrecacheSound("penta/penta_foot4.wav");
	PrecacheSound("penta/penta_idle1.wav");
	PrecacheSound("penta/penta_idle2.wav");
	PrecacheSound("penta/penta_idle3.wav");
	PrecacheSound("penta/penta_pain1.wav");
	PrecacheSound("penta/penta_pain2.wav");
	PrecacheSound("penta/penta_sight1.wav");
	PrecacheSound("penta/penta_sight2.wav");
}

void CPenta::PentaThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if (m_bAttacking)
	{
		if (m_fSequenceFinished)
		{
			m_bAttacking = false;
			SetActivity(ACT_IDLE);
		}
		return;
	}

	CBaseEntity* pPlayer = UTIL_FindEntityByClassname(nullptr, "player");
	if (!pPlayer || !pPlayer->IsAlive())
	{
		if (m_Activity != ACT_IDLE)
			SetActivity(ACT_IDLE);
		return;
	}

	float flDist = (pPlayer->pev->origin - pev->origin).Length();
	constexpr float MELEE_RANGE = 80.0f;
	constexpr float SIGHT_RANGE = 600.0f;

	if (flDist <= MELEE_RANGE)
	{
		if (gpGlobals->time >= m_flNextAttack)
		{
			MakeIdealYaw(pPlayer->pev->origin);
			ChangeYaw(pev->yaw_speed);
			m_bAttacking = true;
			SetActivity(ACT_MELEE_ATTACK1);
			m_flNextAttack = gpGlobals->time + 2.0f;
		}
		return;
	}

	if (flDist > SIGHT_RANGE)
	{
		if (m_Activity != ACT_IDLE)
			SetActivity(ACT_IDLE);
		return;
	}

	MakeIdealYaw(pPlayer->pev->origin);
	ChangeYaw(pev->yaw_speed);

	if (m_Activity != ACT_RUN)
		SetActivity(ACT_RUN);

	UTIL_MakeVectors(pev->angles);
	if (!WALK_MOVE(ENT(pev), pev->ideal_yaw, m_flGroundSpeed * 0.1f, WALKMOVE_NORMAL))
	{
		// Blockiert - kleine zufaellige Ausweichdrehung statt haengenzubleiben.
		pev->ideal_yaw = UTIL_AngleMod(pev->ideal_yaw + RANDOM_FLOAT(-90, 90));
	}
}

void CPenta::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	// Bestaetigt per mdl_inspect.py: attack_close hat zwei Events (Frame
	// 15 und 33, beide ID-lose Marker im Original) - hier als
	// Ankuendigung (1) und tatsaechlicher Bisskontakt (2) interpretiert,
	// da das kein exakter RE-Fund ist (keine CPenta-Methode
	// decompiliert), sondern eine plausible, dokumentierte Annahme.
	switch (pEvent->event)
	{
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, RANDOM_LONG(0, 1) ? "penta/penta_attack1.wav" : "penta/penta_attack2.wav", 1.0, ATTN_NORM);
		break;
	case 2:
	{
		CBaseEntity* pPlayer = UTIL_FindEntityByClassname(nullptr, "player");
		if (pPlayer && (pPlayer->pev->origin - pev->origin).Length() <= 96.0f)
		{
			pPlayer->TakeDamage(pev, pev, gSkillData.pentaBiteDamage, DMG_SLASH);
		}
		break;
	}
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// monster_battery - CBatteryBot. Fliegende Kreatur, vollstaendig
// geriggt (15 Knochen in 3 symmetrischen Gliedmassen-Ketten, 4
// Attachment-Punkte, 9 Sequenzen: idle1/2, run, drei verschiedene
// Angriffsanimationen attack1/2/3, flinch, turnleft/right) - per
// tools/mdl_inspect.py gegen die echte, retail-shipped
// models/battbot.mdl verifiziert (die attack-Sequenzen sind dort mit
// Stock-ACT_RANGE_ATTACK1 getaggt, obwohl kein Fernkampf-Feuercode
// gefunden wurde - vermutlich ein wiederverwendeter Platzhalter-Tag,
// nicht woertlich zu nehmen, siehe Findings-Dokument).
//
// FlyTouch (0x1005f660) und HuntThink (0x1005f760) wurden aus der
// E3-Pre-Release-DLL FRISCH DECOMPILIERT (siehe Findings-Dokument fuer
// den vollen Text): HuntThink implementiert eine echte Verfolgungs-/
// Sichtlinien-/Ausweich-KI mit Wegpunkt-Steuerung (strukturell
// identisch zum bereits bekannten CBaseBird-Familienmuster,
// CChopper/CRustFlier - vermutlich ein viertes, cut CBaseBird-
// Geschwister). FlyTouch prueft einen Zustandswert am beruehrten
// Entity und wendet bei Treffer eine geschwindigkeitsskalierte
// Rueckstoss-Physik an - reproduziert hier als direkter
// Kollisions-/Rammschaden am Spieler, da kein Fernkampf-Feuercode
// gefunden wurde. Kein bekannter Skill-Health-Wert (siehe
// Klassenkopf oben) - Platzhalter 40, dieselbe Groessenordnung wie
// das bereits als "plausibler Default" dokumentierte CChopper.
//=========================================================
class CBatteryBot : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_MONSTER; }
	void EXPORT HuntThink();
	void EXPORT FlyTouch(CBaseEntity* pOther);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	// Nachtrag 2026-09-06: models/battgib.mdl war schon vorher precacht,
	// aber nie in GibMonster() verwendet - siehe CBaseMonster::
	// CustomGibModel() in basemonster.h.
	const char* CustomGibModel() override { return "models/battgib.mdl"; }

private:
	float m_flNextAttack = 0;
};
LINK_ENTITY_TO_CLASS(monster_battery, CBatteryBot);

TYPEDESCRIPTION CBatteryBot::m_SaveData[] =
{
	DEFINE_FIELD(CBatteryBot, m_flNextAttack, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CBatteryBot, CBaseMonster);

void CBatteryBot::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/battbot.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, -16), Vector(16, 16, 16));

	pev->flags |= FL_FLY;
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	pev->health = 40; // kein bekannter Skill-Wert, siehe Klassenkopf-Kommentar
	m_bloodColor = DONT_BLEED;

	MonsterInit();
	SetActivity(ACT_IDLE);
	SetThink(&CBatteryBot::HuntThink);
	SetTouch(&CBatteryBot::FlyTouch);
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.1, 0.5);
}

void CBatteryBot::Precache()
{
	PrecacheModel("models/battbot.mdl");
	PrecacheModel("models/battgib.mdl");
}

void CBatteryBot::HuntThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	CBaseEntity* pPlayer = UTIL_FindEntityByClassname(nullptr, "player");
	if (!pPlayer || !pPlayer->IsAlive())
	{
		if (m_Activity != ACT_IDLE)
			SetActivity(ACT_IDLE);
		return;
	}

	Vector vecToPlayer = pPlayer->pev->origin + Vector(0, 0, 32) - pev->origin;
	float flDist = vecToPlayer.Length();

	if (flDist > 900.0f)
	{
		if (m_Activity != ACT_IDLE)
			SetActivity(ACT_IDLE);
		pev->velocity = pev->velocity * 0.9f;
		return;
	}

	if (m_Activity != ACT_RUN)
		SetActivity(ACT_RUN);

	Vector vecDir = vecToPlayer.Normalize();
	pev->velocity = pev->velocity * 0.8f + vecDir * 200.0f * 0.2f;
	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->angles.x = -pev->angles.x; // Studio-Modelle nutzen invertiertes Pitch

	if (flDist <= 128.0f && gpGlobals->time >= m_flNextAttack && m_fSequenceFinished)
	{
		switch (RANDOM_LONG(0, 2))
		{
		case 0:
			SetActivity((Activity)28); // attack1, siehe Klassenkopf-Kommentar zum ACT_RANGE_ATTACK1-Tag
			break;
		case 1:
			pev->sequence = LookupSequence("attack2");
			ResetSequenceInfo();
			break;
		default:
			pev->sequence = LookupSequence("attack3");
			ResetSequenceInfo();
			break;
		}
		m_flNextAttack = gpGlobals->time + 1.5f;
	}
}

void CBatteryBot::FlyTouch(CBaseEntity* pOther)
{
	// Nachtrag: reproduziert FlyTouch@0x1005f660 (frisch decompiliert) -
	// dort wird bei Kontakt eine geschwindigkeitsskalierte
	// Rueckstoss-Physik angewendet, kein direkter Schaden gefunden.
	// Hier zusaetzlich mit echtem Rammschaden am Spieler ergaenzt
	// (dokumentierte Annahme, kein RE-Fund), da eine reine Physik-
	// Kollision ohne Konsequenz fuer eine "Kreatur" spielerisch
	// wirkungslos waere.
	if (!pOther->IsPlayer())
		return;

	if (gpGlobals->time < m_flNextAttack)
		return;

	Vector vecPush = (pOther->pev->origin - pev->origin).Normalize();
	pOther->pev->velocity = pOther->pev->velocity + vecPush * 150.0f;
	pOther->TakeDamage(pev, pev, 10, DMG_CRUSH);
	m_flNextAttack = gpGlobals->time + 1.0f;
}
