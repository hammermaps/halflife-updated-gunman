//=========================================================
// CHumanBase - gemeinsame Kampf-KI von monster_human_bandit (= monster_human_grunt),
// monster_human_demoman und monster_human_scientist.
//
// Retail: eine abgewandelte Kopie der Half-Life-Grunt-KI ueber CSquadMonster; gemeinsame Slots
// StartTask @0x100aa2f0, RunTask @0x100aa6d0, GetScheduleOfType @0x100ab400, GetSchedule @0x100aa940,
// Schedule-Daten @0x10129c90..0x1012a4a8. Details und Vereinfachungen:
// findings/entities/human_base_ki_2026-10-31.md und human_base_stufe1-3_2026-10-31.md.
//=========================================================

#pragma once

#include "squadmonster.h"

// Retail-spezifische Aufgaben (gemeinsame Aufgaben 0x5a..0x5f gehen ueber das SDK hinaus, Menschen-Aufgaben ab 0x61).
enum
{
	TASK_HB_IDLE_ACTIVITY = 0x5f, // Idealaktivitaet ACT_IDLE und Aufgabe beenden (ersetzt TASK_SET_ACTIVITY ACT_IDLE)
	TASK_HB_FACE_TOSS_DIR = 0x61,
	TASK_HB_SPEAK_SENTENCE = 0x62,
	TASK_HB_CHECK_FIRE = 0x63,
	TASK_HB_ROLL_RIGHT = 0x64,
	TASK_HB_ROLL_LEFT = 0x65,
};

// Retail-Schedule-Typen der Menschen-Basis (Zahlenwerte wie in Retail; die Werte bis 0x29 sind die SDK-Schedules).
enum
{
	HB_SCHED_SUPPRESS = 0x2e,
	HB_SCHED_ESTABLISH_LINE_OF_FIRE = 0x2f,
	HB_SCHED_COVER_AND_RELOAD = 0x30,
	HB_SCHED_SWEEP = 0x31,
	HB_SCHED_FOUND_ENEMY = 0x32,
	HB_SCHED_REPEL = 0x33,
	HB_SCHED_REPEL_ATTACK = 0x34,
	HB_SCHED_REPEL_LAND = 0x35,
	HB_SCHED_WAIT_FACE_ENEMY = 0x36,
	HB_SCHED_TAKECOVER_FAILED = 0x37,
	HB_SCHED_ELOF_FAIL = 0x38,
	HB_SCHED_RANGE_MACHINEGUN = 0x39,
	HB_SCHED_RANGE_ROCKET = 0x3a,
	HB_SCHED_RANGE_SHOTGUN = 0x3b,
	HB_SCHED_DRAW = 0x3c,
	HB_SCHED_ROLL_LEFT = 0x3d,	// Retail @0x1012a458 (Ausweichrolle aus GetSchedule)
	HB_SCHED_ROLL_RIGHT = 0x3e, // Retail @0x1012a490
};

#define bits_COND_HB_NOFIRE (bits_COND_SPECIAL1)

// Satztabelle @0x10129c40 (Index = m_iSentence)
enum HB_SENTENCE
{
	HB_SENT_NONE = -1,
	HB_SENT_GREN = 0,
	HB_SENT_ALERT,
	HB_SENT_MONSTER,
	HB_SENT_COVER,
	HB_SENT_THROW,
	HB_SENT_CHARGE,
	HB_SENT_TAUNT,
	HB_SENT_IDLE,
	HB_SENT_QUEST,
};

class CHumanBase : public CSquadMonster
{
public:
	void StartTask(Task_t* pTask) override;
	void RunTask(Task_t* pTask) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	Activity GetDeathActivity() override;
	Schedule_t* ScheduleFromName(const char* pName) override;
	Schedule_t* GetSchedule() override;
	Schedule_t* GetScheduleOfType(int Type) override;
	int ISoundMask() override;
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckRangeAttack2(float flDot, float flDist) override;
	bool FCanCheckAttacks() override;
	void CheckAmmo() override;
	void PrescheduleThink() override;
	void IdleSound() override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;

	// Retail @0x100aa080 (von den Spawn-Funktionen der Unterklassen vor MonsterInit aufgerufen): Box, Movetype,
	// Squad-Faehigkeiten, Zeitfelder, Stimmlage, Sichtweite. Die Unterklasse setzt danach m_cClipSize.
	void HumanInit();

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	// Retail @0x100a8780: Sprechen nur nach Wartezeit, nicht bei SF_MONSTER_GAG ausserhalb des Kampfes und nur im Squad.
	bool FOkToSpeak();
	// Retail @0x100a87d0: naechste Sprechpause 2..3,5 s, Satzmerker loeschen.
	void JustSpoke();
	// Retail @0x100a8710: den vorgemerkten Satz sprechen.
	void SpeakSentence();

	// Retail-Slot 139 @0x100b1a70 (Bandit): geduckte Haltung (Augenhoehe 32).
	virtual void Crouch();
	// Retail-Slot 142 @0x100b1cf0 (Bandit: false): Klasse kann Granaten/Raketen als Zweitangriff nutzen.
	virtual bool CanFireRange2() { return false; }
	// Retail-Slot 141 @0x100b1b10 (Bandit: false): unbegrenzte Munition, kein Nachladen.
	virtual bool InfiniteAmmo() { return false; }
	// Retail vtable+0x244 returns {enabled, bodygroup, value} for headshot gibs.
	virtual bool GetHeadshotGibBodygroup(int& group, int& value) { (void)group; (void)value; return false; }
	// Retail-Slot 143 @0x100b1a60 (Bandit: true): darf sich zum Schiessen ducken.
	virtual bool CanCrouch() { return true; }
	// Retail Slot 144 (0x240): Rollseite fuer die Ausweichrolle (1 rechts, 2 links, 0 = rollt nie); nur der Bandit liefert +0x300.
	virtual int GetRollSide() { return 0; }

protected:
	Schedule_t* GetDodgeRollSchedule(); // Retail @0x100aa940: Ausweichrolle im Zweig CAN_RANGE_ATTACK1

public:

	float m_flNextGrenadeCheck = 0;
	float m_flNextRollCheck = 0;
	float m_flNextPainTime = 0; // Retail +0x2b4
	Vector m_vecTossVelocity;
	bool m_fThrowGrenade = false;
	bool m_fStanding = true;
	bool m_fFirstEncounter = true;
	int m_cClipSize = 0;
	int m_voicePitch = 100;
	int m_iSentence = HB_SENT_NONE;
	int m_iRollSide = 0; // Retail +0x300

protected:
	// Muendungsposition fuer den Sichtlinientest (Retail @0x100b0800, aus dem Dekompilat rekonstruiert):
	// Modus 0 rechts (+10), Modus 1 links (-10), jeweils 54 ueber dem Ursprung; Mecha nutzt immer Modus 0.
	Vector MuzzlePosition(int iMode);
	bool MuzzleHasLineOfFire(const Vector& vecSrc);

	static float s_flTalkWaitTime;
	static int s_iQuestionStage; // Retail-Global @0x10142cc4 (Leerlauf-Frage/Antwort) // Retail-Global @0x101414e0
};
