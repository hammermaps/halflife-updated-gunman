//=========================================================
// monster_human_bandit (alias monster_human_grunt) - CBanditGrunt (real
// C++ class name confirmed via a raw string literal in gunman.dll,
// right next to the "MechaGunBandit"/"StartDrawn" keyvalue strings).
// A hostile combat NPC, NOT part of the CTalkMonster "Talk/Ally"
// family that monster_human_gunman/monster_human_unarmed belong to -
// see findings/entities/human_bandit_base_class.md for the full
// two-family breakdown (this session's decompile confirms every
// address the doc already listed, cross-checked rather than assumed).
//
// Decompiled from gunman.dll: constructor @0x100b19c0 (vtable
// 0x10100e64), Spawn @0x100b1b80, Precache @0x100b1b20, KeyValue
// @0x100b0410, Classify @0x100b19e0 (constant 5 =
// CLASS_ALIEN_MILITARY), HandleAnimEvent @0x100b10e0 (vtable slot
// 58).
//
// KeyValues confirmed against the retail FGD's monster_human_bandit
// block: "StartDrawn" (Choices, default 1=Yes/drawn, 2=No), "body"
// (Choices, FGD title "Head" - 0=Random/1=Original/2=Bandana/3=Tail/
// 4=Pilot - re-parsed defensively by the DLL's own KeyValue() even
// though the engine already auto-maps "body" onto pev->body directly;
// not the "Head" bone-name string that also happens to sit nearby in
// the binary's string table, which is unrelated), "MechaGunBandit"
// (Choices 0=No/1=Yes - selects the "Mecha Gun Bandit" weapon mode).
//
// HandleAnimEvent's real event IDs (all confirmed via the fresh
// decompile, matching findings/entities/human_bandit_base_class.md's
// table exactly):
//   1/2 -> FUN_100a9290 (single gauss-pistol-style shot)
//   3   -> weapon-drawn bodygroup state
//   4   -> weapon-holstered bodygroup state
//   5/6 -> FUN_100a9890 (single shot, second variant)
//   8   -> Mecha-mode volley via FUN_1000f090 (the shared multi-pellet
//          traceline helper also used by weapon_shotgun/CFriendlyGunman
//          /CChopper), confirmed spread angle 0.02618 rad, plays
//          weapons/hks1.wav or hks2.wav
//   0xb -> a real, more elaborate melee than a plain traceline:
//          searches nearby entities within 128 units (FUN_1000e570),
//          applies a small random positional shove to the found
//          target, THEN does an LOS traceline (FUN_1005f470) and, if
//          clear, deals skill-scaled damage via a virtual call. NOTE
//          (2026-09-05 correction, see findings/open_items_audit_
//          2026-09-05.md): earlier revisions of this comment and
//          CBanditGrunt::Melee() attributed this event's damage call
//          to `FUN_10001f90` - that function was independently
//          decompiled and is actually an unrelated SetBodygroup(group,
//          value) helper, confirmed also used (correctly) for this
//          same class's events 3/4 below. The real event-0xb damage
//          path uses different helper functions entirely
//          (FUN_1000e570/FUN_1005f470), not FUN_10001f90.
//   default -> footstep/misc fallback (FUN_100a9cb0), cosmetic only
//
// Simplified relative to the original (documented per-case): events
// 1/2/5/6 all fire through a single generic FireBullets() call here
// instead of reproducing FUN_100a9290/FUN_100a9890's separate,
// byte-exact aim/spread math (same simplification level already used
// for CFriendlyGunman's Shoot()). Event 8's real FUN_1000f090 pellet
// count/damage-per-pellet parameters were not fully decoded from the
// decompile's register-heavy calling convention - approximated as a
// 6-pellet FireBullets spray at the confirmed 0.02618 rad spread
// angle. Event 0xb's melee is a simple forward-distance+TakeDamage
// check instead of the original's shove-then-LOS-gated damage
// sequence described above. No custom Schedule_t/Task_t AI tables (same
// established simplification as every other monster_human_* class in
// this project) - relies on default CBaseMonster scheduling.
// monster_human_grunt is linked to this same class below, matching
// Retail's shared Vtable. The unrelated SDK CHGrunt implementation in
// hgrunt.cpp has no entity registration, so it cannot claim the classname.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "schedule.h"
#include "weapons.h"
#include "soundent.h"
#include "human_base.h"
#include "fastshot.h"
#include "dbg_log.h"

#define BANDIT_AE_SHOOT1 1
#define BANDIT_AE_SHOOT2 2
#define BANDIT_AE_DRAW 3
#define BANDIT_AE_HOLSTER 4
#define BANDIT_AE_SHOOT3 5
#define BANDIT_AE_SHOOT4 6
#define BANDIT_AE_MECHA_VOLLEY 8
#define BANDIT_AE_MELEE 0xb

class CBanditGrunt : public CHumanBase
{
public:
	int MonsterProfile() override { return 3; } // retail slot 59
	bool GetHeadshotGibBodygroup(int& group, int& value) override { group = 2; value = 4; return true; }
	// Retail CheckRangeAttack1 calls the Bandit vtable entry at +0x23c
	// before its crouched muzzle tests; that target is `xor eax,eax; ret`,
	// so the crouched branch is unreachable for this class.
	bool CanCrouch() override { return false; }
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_MILITARY; }
	int GetRollSide() override { return m_iRollSide; } // Retail Slot 144 @0x100b1a50
	void SetYawSpeed() override;
	// BUG FIX (2026-09-05, live gameplay report): this NPC is commonly
	// puppeted by scripted_sequence "waiting"/idle-pose loops across
	// this project's maps (self-retriggering scripted_sequence chains
	// playing named, non-ACT_IDLE-tagged sequences). Without this
	// override, CBaseMonster's default SetActivity(ACT_IDLE) (called by
	// the default AI schedule between scripted waypoints, even for an
	// otherwise full combat monster) stomps pev->sequence mid-animation,
	// cutting the wait pose short and restarting it from frame 0 - same
	// root cause as CFurniture/CGenericMonster/CRenesaur/CAIGirl/
	// CFriendlyGunman. Normal combat activity switching is unaffected.
	void SetActivity(Activity NewActivity) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	bool KeyValue(KeyValueData* pkvd) override;
	Schedule_t* GetSchedule() override;
	bool CheckMeleeAttack1(float flDot, float flDist) override;
	// Retail slot 126 @0x100a91f0: muzzle origin is origin + 60 (standing) or + 48 (crouching).
	Vector GetGunPosition() override { return pev->origin + Vector(0, 0, m_fStanding ? 60.0f : 48.0f); }
	// Retail slot 62 @0x100b1040: animation speed 1.5 while rolling, otherwise 1.0.
	void RunAI() override
	{
		pev->framerate = (m_Activity == ACT_ROLL_LEFT || m_Activity == ACT_ROLL_RIGHT) ? 1.5f : 1.0f;
		CBaseMonster::RunAI();
	}

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	bool SetActivitySequence(Activity activity, const char* sequenceName);
	void ShootBeam(float damage, int attachment);
	void ShootBolt(float damage, int attachment);
	bool PrepareShot(Vector& vecSrc, Vector& vecDir, float flSpread, int attachment);
	void FinishShot();
	unsigned short m_usSniperShot = 0;
	void MechaVolley();
	void Melee();

	int m_iBody = 0;		 // "body" keyvalue (FGD title "Head") - 0=Random, 1-4 = fixed head variants
	int m_iStartDrawn = 1; // "StartDrawn" keyvalue - 1=Yes (default), 2=No
	bool m_bHasAmmo = true;
	int m_iGunState = 1; // 1 = gauss pistols, 0x80 = MechaGunBandit
};
LINK_ENTITY_TO_CLASS(monster_human_bandit, CBanditGrunt);
// Retail: monster_human_grunt is the same class (identical LINK function and vtable 0x10100e64).
LINK_ENTITY_TO_CLASS(monster_human_grunt, CBanditGrunt);

TYPEDESCRIPTION CBanditGrunt::m_SaveData[] =
	{
		// Retail TYPEDESCRIPTION @ 0x1012b3a0 contains exactly these two
		// runtime fields. Mapper keyvalues are intentionally not persisted.
		DEFINE_FIELD(CBanditGrunt, m_bHasAmmo, FIELD_BOOLEAN),
		DEFINE_FIELD(CBanditGrunt, m_iGunState, FIELD_INTEGER),
	};

IMPLEMENT_SAVERESTORE(CBanditGrunt, CHumanBase);

bool CBanditGrunt::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "body"))
	{
		m_iBody = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "StartDrawn"))
	{
		m_iStartDrawn = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "MechaGunBandit"))
	{
		if (atoi(pkvd->szValue) != 0)
			m_iGunState = 0x80;
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

void CBanditGrunt::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/bandit.mdl");
	// Blutfarbe 247 kommt aus der Profiltabelle (MonsterProfile() = 3); Retail setzt +0x238 im Spawn nicht.
	pev->health = gSkillData.banditHealth;
	pev->view_ofs = Vector(0, 0, 60);
	m_flFieldOfView = 0.2f; // Retail Spawn @0x100b1b80: 0x3e4ccccd

	// Retail: Magazingroesse 18 (Mecha 24); HumanInit @0x100aa080 setzt Box, Movetype, Squad-Faehigkeiten, Zeitfelder.
	m_cClipSize = m_iGunState == 0x80 ? 24 : 18;
	HumanInit();

	MonsterInit();
	// Retail (Capability-Schreibzugriff +0x220 nach MonsterInit): gleiche Bitoperation wie im Dekompilat.
	m_afCapability &= ~bits_CAP_RANGE_ATTACK2;
	// Retail CBanditGrunt::Spawn @ 0x100b1b80 restores its 60-unit eye
	// height after the model-derived MonsterInit() lookup.
	pev->view_ofs = Vector(0, 0, 60);
	// Retail MonsterInit overwrites the 0.2 set above with the profile table's -0.7, and Spawn does not
	// restore it (frisch dekompiliert 2026-10-31, @0x100b1b80 / @0x1002af50).
	m_flFieldOfView = -0.7f;

	// Retail: random head 0..2 (RANDOM_LONG(0,2)); skin 0 or 1 with equal chance.
	SetBodygroup(2, m_iBody == 0 ? RANDOM_LONG(0, 2) : m_iBody - 1);
	pev->skin = RANDOM_LONG(0, 99) < 50 ? 0 : 1;

	pev->weapons = m_iGunState;
	m_bHasAmmo = true;
	if (m_iGunState == 0x80)
	{
		SetBodygroup(1, 2);
		SetBodygroup(3, 1);
	}
	else
	{
		SetBodygroup(1, m_iStartDrawn != 1 ? 1 : 0);
	}
}

void CBanditGrunt::Precache()
{
	PrecacheModel("models/bandit.mdl");
	PrecacheModel("models/shell.mdl");
	PrecacheModel("models/shotgunshell.mdl");
	PrecacheModel("models/humanskull.mdl");
	PrecacheModel("models/hgibs.mdl");
	PrecacheModel("sprites/zbeam5.spr");
	PrecacheModel("sprites/gaussbeam1.spr");
	PrecacheModel("sprites/gausspark.spr");
	PrecacheModel("sprites/gorehuman.spr");
	PrecacheModel("sprites/gibhuman.spr");
	PrecacheModel("sprites/gorebot.spr");
	m_usSniperShot = PrecacheEvent(1, "events/snipershot.sc");
	PrecacheModel("sprites/gaussbeam2.spr");
	PrecacheModel("sprites/gaussring.spr");
	PrecacheModel("sprites/gausspoof.spr");
	PrecacheEvent(1, "events/monstermechagun.sc");
	PrecacheEvent(1, "events/monstershotgun.sc");

	PrecacheSound("weapons/gauss_fire1.wav");
	PrecacheSound("weapons/gauss_fire2.wav");
	PrecacheSound("weapons/gauss_charge.wav");
	PrecacheSound("weapons/hks1.wav");
	PrecacheSound("weapons/hks2.wav");
	PrecacheSound("debris/bustflesh1.wav");
	PrecacheSound("bandit/men_check_in.wav");

	// Retail CBanditGrunt::Precache @ 0x100b1b20 delegates these ammo
	// classes through UTIL_PrecacheOther. ammo_minigunClip has no matching
	// SDK entity yet, so it cannot be registered safely here.
	UTIL_PrecacheOther("ammo_gaussclip");
	UTIL_PrecacheOther("gauss_charged");
}

void CBanditGrunt::SetYawSpeed()
{
	// Retail CBanditGrunt::SetYawSpeed @ 0x100b0f60 uses authored turn
	// rates per activity rather than the SDK's generic 70/90 fallback.
	switch (m_Activity)
	{
	case ACT_IDLE:
	case ACT_RUN:
		pev->yaw_speed = 100;
		break;
	case ACT_WALK:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		pev->yaw_speed = 130;
		break;
	case ACT_FLY:
	case ACT_GLIDE:
		pev->yaw_speed = 20;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
	case ACT_MELEE_ATTACK1:
	case ACT_MELEE_ATTACK2:
		pev->yaw_speed = 80;
		break;
	default:
		pev->yaw_speed = 120;
		break;
	}
}

bool CBanditGrunt::SetActivitySequence(Activity activity, const char* sequenceName)
{
	const int sequence = LookupSequence(sequenceName);
	if (sequence <= ACTIVITY_NOT_AVAILABLE)
		return false;

	m_Activity = activity;
	if (pev->sequence != sequence || !m_fSequenceLoops)
		pev->frame = 0;
	pev->sequence = sequence;
	ResetSequenceInfo();
	SetYawSpeed();
	return true;
}

void CBanditGrunt::SetActivity(Activity NewActivity)
{
	if ((NewActivity == ACT_IDLE || NewActivity == ACT_RESET) && m_pCine != nullptr)
	{
		m_Activity = NewActivity;
		m_IdealActivity = NewActivity;
		return;
	}

	// Retail CBanditGrunt::SetActivity @ 0x100b16f0 explicitly selects
	// these Mecha sequences because bandit.mdl leaves them untagged
	// (ACT_RESET). The crouch/standing attack and reload variants depend
	// on the still-unreconstructed shared firing-position state and fall
	// through to the SDK until that state is available.
	if (pev->weapons == 0x80)
	{
		const char* sequenceName = nullptr;
		switch (NewActivity)
		{
		case ACT_IDLE:
			sequenceName = RANDOM_LONG(0, 14) == 0 ? "mechaidle" : "mechaidleactive";
			break;
		case ACT_WALK:
			sequenceName = "mechawalk";
			break;
		case ACT_RUN:
			sequenceName = "mecharun";
			break;
		case ACT_CROUCHIDLE:
			sequenceName = "mcrouch_mechacrouching";
			break;
		case ACT_STAND:
			sequenceName = "mechastanding";
			break;
		case ACT_MELEE_ATTACK1:
			sequenceName = "mechapunch";
			break;
		default:
			break;
		}

		if (sequenceName != nullptr && SetActivitySequence(NewActivity, sequenceName))
			return;
	}

	// Retail @0x100b16f0 (Nicht-Mecha-Zweig): Gehen/Rennen bei Gesundheit <= sk_bandit_limp_h (0x10136e94) als
	// ACT_WALK_HURT/ACT_RUN_HURT, Angriffs-, Nachlade- und Idle-Sequenzen je nach Haltung (stehend/geduckt).
	if (pev->weapons != 0x80)
	{
		const char* sequenceName = nullptr;
		bool bError = false;
		switch (NewActivity)
		{
		case ACT_IDLE:
			if (!m_fStanding)
				sequenceName = "crouch#idle";
			break;
		case ACT_WALK:
		case ACT_RUN:
			if (pev->health <= gSkillData.banditLimpHealth)
			{
				const int sequence = LookupActivity(NewActivity == ACT_WALK ? ACT_WALK_HURT : ACT_RUN_HURT);
				if (sequence > ACTIVITY_NOT_AVAILABLE)
				{
					m_Activity = NewActivity;
					if (pev->sequence != sequence || !m_fSequenceLoops)
						pev->frame = 0;
					pev->sequence = sequence;
					ResetSequenceInfo();
					SetYawSpeed();
					return;
				}
			}
			break;
		case ACT_RANGE_ATTACK1:
			if (pev->weapons == 1)
				sequenceName = m_fStanding ? "shooting" : "crouchfire";
			else if (pev->weapons == 0x40)
				sequenceName = m_fStanding ? "rapidfire" : "crouchrapidfire";
			else
				bError = true;
			break;
		case ACT_RELOAD:
			sequenceName = m_fStanding ? "reload" : "crouchreload";
			break;
		default:
			break;
		}

		if (bError)
		{
			m_Activity = NewActivity;
			ALERT(at_console, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity);
			pev->sequence = 0;
			return;
		}
		if (sequenceName != nullptr && SetActivitySequence(NewActivity, sequenceName))
			return;
	}

	CBaseMonster::SetActivity(NewActivity);
}

Schedule_t* CBanditGrunt::GetSchedule()
{
	// Retail CBanditGrunt::GetSchedule @ 0x100b1080 first corrects a
	// mapper default that was left visually holstered, then draws a
	// deliberately holstered bandit as soon as it reaches alert/combat.
	if (m_iStartDrawn == 1 && GetBodygroup(1) == 1)
		SetBodygroup(1, 0);

	if ((m_MonsterState == MONSTERSTATE_ALERT || m_MonsterState == MONSTERSTATE_COMBAT) && m_iStartDrawn == 2)
		return GetScheduleOfType(HB_SCHED_DRAW);

	return CHumanBase::GetSchedule();
}

bool CBanditGrunt::CheckMeleeAttack1(float flDot, float flDist)
{
	// Retail slot 82 is FUN_100a88a0, byte-identical in behavior to
	// CHGrunt::CheckMeleeAttack1: reject non-monster targets and the two
	// bio-weapon classes, without inheriting CBaseMonster's FL_ONGROUND
	// restriction.
	CBaseMonster* pEnemy = m_hEnemy != nullptr ? m_hEnemy->MyMonsterPointer() : nullptr;
	if (!pEnemy)
		return false;

	// Retail @0x100a88a0 compares the enemy class with the literals 19 and 18 (not the bio-weapon
	// classes 12/13 used by the stock SDK).
	return flDist <= 64.0f && flDot >= 0.7f && pEnemy->Classify() != 19 && pEnemy->Classify() != 18;
}

void CBanditGrunt::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	DBG_LOG("BANDIT %d HandleAnimEvent event=%d options='%s' frame=%.1f seq=%d enemy=%s", ENTINDEX(edict()), pEvent->event, pEvent->options, pev->frame, pev->sequence, m_hEnemy ? STRING(m_hEnemy->pev->classname) : "none");
	switch (pEvent->event)
	{
	case BANDIT_AE_SHOOT1:
	case BANDIT_AE_SHOOT2:
		ShootBeam(gSkillData.banditBoltDamage, pEvent->event == BANDIT_AE_SHOOT1 ? 0 : 1);
		break;
	case BANDIT_AE_SHOOT3:
	case BANDIT_AE_SHOOT4:
		ShootBolt(gSkillData.banditFastDamage, pEvent->event == BANDIT_AE_SHOOT3 ? 0 : 1);
		break;
	case BANDIT_AE_DRAW:
		SetBodygroup(1, 0);
		break;
	case BANDIT_AE_HOLSTER:
		SetBodygroup(1, 1);
		break;
	case BANDIT_AE_MECHA_VOLLEY:
		if (m_iGunState == 0x80)
			MechaVolley();
		break;
	case BANDIT_AE_MELEE:
		Melee();
		break;
	default:
		break;
	}
}

void CBanditGrunt::Killed(entvars_t* pevAttacker, int iGib)
{
	// Retail CBanditGrunt::Killed @ 0x100b0520 only drops a weapon once.
	if (m_bHasAmmo)
	{
		m_bHasAmmo = false;
		SetBodygroup(1, 3);

		auto dropWeapon = [this](const char* weaponName, int attachment, float zMin, float zMax)
		{
			Vector vecPos;
			Vector vecAngles;
			GetAttachment(attachment, vecPos, vecAngles);
			if (CBaseEntity* pWeapon = DropItem(weaponName, vecPos, vecAngles))
			{
				pWeapon->pev->velocity = Vector(RANDOM_FLOAT(-48.0f, 48.0f), RANDOM_FLOAT(-48.0f, 48.0f), RANDOM_FLOAT(zMin, zMax));
				pWeapon->pev->avelocity = Vector(0.0f, RANDOM_FLOAT(zMin, zMax), 0.0f);
			}
		};

		if (m_iGunState == 0x80)
		{
			dropWeapon("weapon_minigun", 1, 100.0f, 200.0f);
		}
		else
		{
			dropWeapon("weapon_gausspistol", 0, 200.0f, 400.0f);
			dropWeapon("weapon_gausspistol", 1, 200.0f, 400.0f);
		}
	}

	CBaseMonster::Killed(pevAttacker, iGib);
}

// Gemeinsamer Anfang der Retail-Funktionen @0x100a9290 / @0x100a9890: Zielrichtung zum Feind, gleichverteilte
// Streuung (Summe zweier Zufallswerte je Achse, im Kreis verworfen) mal flSpread auf Hoch/Rechts, Muendung
// 3 rechts und 8 unter der Waffenposition.
bool CBanditGrunt::PrepareShot(Vector& vecSrc, Vector& vecDir, float flSpread, int attachment)
{
	if (!m_hEnemy)
		return false;

	// Retail event 1 (@0x100b1105) takes its shot origin from attachment 0 (weapon hand), not from
	// the monster's center or head: GetGunPosition() put the shots at head height. Attachment 1 is
	// used by retail event 2 (not yet mapped to SHOOT2/SHOOT4 here, see TODO in findings/entities).
	Vector vecAttachment, vecAttachAngles;
	GetAttachment(attachment, vecAttachment, vecAttachAngles);
	vecSrc = vecAttachment;
	DBG_LOG("BANDIT %d PrepareShot attachment=%d src=(%.1f %.1f %.1f) spread=%.4f", ENTINDEX(edict()), attachment, vecSrc.x, vecSrc.y, vecSrc.z, flSpread);
	vecDir = ShootAtEnemy(vecSrc);

	Vector vecAngles = UTIL_VecToAngles(vecDir);
	UTIL_MakeVectors(vecAngles);

	float x, y;
	do
	{
		x = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
		y = RANDOM_FLOAT(-0.5f, 0.5f) + RANDOM_FLOAT(-0.5f, 0.5f);
	} while (x * x + y * y > 1.0f);

	vecDir = (vecDir + gpGlobals->v_right * (x * flSpread) + gpGlobals->v_up * (y * flSpread)).Normalize();
	vecSrc = vecSrc + gpGlobals->v_right * 3.0f - gpGlobals->v_up * 8.0f;
	return true;
}

void CBanditGrunt::FinishShot()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/gauss_fire1.wav", 1.0, 0.8, 0, 100);
	pev->effects |= EF_MUZZLEFLASH;
	if (!InfiniteAmmo())
		m_cAmmoLoaded--;
}

// Ereignisse 1/2 (Retail @0x100a9290 -> @0x100a9520): Strahl 8192 Einheiten mit 4 Grad Streukegel, Schaden
// banditBoltDamage per TraceAttack, gauss_fire3 beim Treffer lebender Ziele. Dargestellt ueber events/snipershot.sc
// (Client-Handler EV_SniperShot).
void CBanditGrunt::ShootBeam(float damage, int attachment)
{
	Vector vecSrc, vecDir;
	if (!PrepareShot(vecSrc, vecDir, 0.06976f, attachment))
		return;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192.0f, dont_ignore_monsters, edict(), &tr);
	DBG_LOG("BANDIT %d ShootBeam trace frac=%.3f end=(%.1f %.1f %.1f) hit=%s", ENTINDEX(edict()), tr.flFraction, tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z, tr.pHit ? STRING(tr.pHit->v.classname) : "none");

	if (tr.pHit)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && pHit->pev->takedamage != DAMAGE_NO)
		{
			ClearMultiDamage();
			pHit->TraceAttack(pev, damage, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB);
			ApplyMultiDamage(pev, pev);
			if (pHit->IsAlive())
				EMIT_SOUND_DYN(ENT(pHit->pev), CHAN_WEAPON, "weapons/gauss_fire3.wav", 0.25f, ATTN_NORM, 0, 120);
		}
	}

	// Retail: events/snipershot.sc, Flags 2 (FEV_RELIABLE), Ursprung = Muendung, Winkel = Schussrichtung
	// (AngleVectors-Konvention), iparam1 1, bparam2 1.
	Vector vecAngles = UTIL_VecToAngles(vecDir);
	vecAngles.x = -vecAngles.x;
	PLAYBACK_EVENT_FULL(FEV_RELIABLE, edict(), m_usSniperShot, 0, vecSrc, vecAngles, 0, 0, 1, 0, 0, 1);

	FinishShot();
}

// Ereignisse 5/6 (Retail @0x100a9890 -> @0x1008e9c0): ein CFastShot-Projektil mit Geschwindigkeit 2000, Streukegel
// 3,5 Grad, Schaden banditFastDamage.
void CBanditGrunt::ShootBolt(float damage, int attachment)
{
	Vector vecSrc, vecDir;
	if (!PrepareShot(vecSrc, vecDir, 0.06105f, attachment))
		return;

	DBG_LOG("BANDIT %d ShootBolt src=(%.1f %.1f %.1f) dir=(%.3f %.3f %.3f) damage=%.1f", ENTINDEX(edict()), vecSrc.x, vecSrc.y, vecSrc.z, vecDir.x, vecDir.y, vecDir.z, damage);
	CFastShot::Create(pev, vecSrc, vecDir * 2000.0f, damage);
	FinishShot();
}

// Retail @0x100b10e0 Ereignis 8: 6 Kugeln, Streuung 0,02618, Reichweite 4096, Kugeltyp 1, Leuchtspur alle 4, Schaden
// sk_bandit_mech_d (0x10136ea4); Ton hks1/hks2 mit Lautstaerke 0,5 und Daempfung 0,6, Muendungsfeuer.
void CBanditGrunt::MechaVolley()
{
	if (!m_hEnemy)
		return;

	// Retail event 8 (@0x100b1233) starts the volley at attachment 0 (bandit.mdl: Bip01 R Hand,
	// +15 forward), not at the monster's center. GetGunPosition() put the shots in the middle of the model.
	Vector vecShootOrigin, vecShootAngles;
	GetAttachment(0, vecShootOrigin, vecShootAngles);
	Vector vecShootDir = ShootAtEnemy(vecShootOrigin);
	DBG_LOG("BANDIT %d MechaVolley src=(%.1f %.1f %.1f) dir=(%.3f %.3f %.3f)", ENTINDEX(edict()), vecShootOrigin.x, vecShootOrigin.y, vecShootOrigin.z, vecShootDir.x, vecShootDir.y, vecShootDir.z);
	FireBullets(6, vecShootOrigin, vecShootDir, Vector(0.02618, 0.02618, 0.02618), 4096, BULLET_PLAYER_9MM, 4, (int)gSkillData.banditMechDamage);
	m_cAmmoLoaded--;
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, RANDOM_LONG(0, 1) ? "weapons/hks2.wav" : "weapons/hks1.wav", 0.5f, 0.6f, 0, 100);
	pev->effects |= EF_MUZZLEFLASH;
}

// Retail @0x100b10e0 Ereignis 0xb: Nahkampf 128 Einheiten, Schaden sk_bandit_melee_d (0x10136e98, DMG_SLASH);
// das Opfer bekommt einen Zufallsschlag (+-15 Grad) und wird 30 Einheiten zurueck- und hochgestossen; bei Monstern
// Blutspur (Schaden sk_rustbit_claw_d, 0x10136ef4) und, falls es lebt, der Treffer-Ton (Slot 137).
void CBanditGrunt::Melee()
{
	CBaseEntity* pHurt = CheckTraceHullAttack(128, gSkillData.banditMeleeDamage, DMG_SLASH);
	if (pHurt == nullptr)
		return;

	pHurt->pev->punchangle.z = (float)(RANDOM_LONG(0, 29) - 15);
	pHurt->pev->punchangle.x = (float)(RANDOM_LONG(0, 29) - 15);
	UTIL_MakeVectors(pev->angles);
	pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_forward * 30.0f + gpGlobals->v_up * 30.0f;

	CBaseMonster* pMonster = pHurt->MyMonsterPointer();
	if (pMonster != nullptr)
	{
		TraceResult trBleed;
		UTIL_TraceLine(GetGunPosition(), pMonster->BodyTarget(GetGunPosition()), dont_ignore_monsters, edict(), &trBleed);
		pMonster->TraceBleed(gSkillData.rustbitClawDamage, g_vecZero, &trBleed, DMG_SLASH);
		if (pMonster->IsAlive())
			EMIT_SOUND_DYN(pMonster->edict(), CHAN_VOICE, "debris/bustflesh1.wav", 0.3f, 2.0f, 0, 110);
	}
}
