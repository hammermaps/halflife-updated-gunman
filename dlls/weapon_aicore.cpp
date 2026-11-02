//=========================================================
// weapon_aicore - CAiCore. A carried Gunman-only quest/story item
// implemented via the weapon system, not a combat weapon (see
// findings/weapons/weapon_aicore.md, cross-checked and confirmed by
// this session's own fresh decompile rather than trusted as-is).
//
// Decompiled fresh from gunman.dll: LINK @0x1007b1f0, vtable
// @0x100f9d50, Spawn @0x1007b2c0, Precache @0x1007b330. Confirms the
// doc's findings exactly: weapon ID 7 (re-mapped here to the new
// WEAPON_AICORE enum entry in cdll_dll.h rather than reusing the
// stock enum's own slot 7, which is WEAPON_SHOTGUN - Gunman's own
// internal weapon-ID numbering is unrelated to the stock HL enum),
// world model "models/w_aicore.mdl", view model "models/v_aicore.mdl",
// no ammo type (both pszAmmo1/2 stay null), no clip
// (iMaxClip = WEAPON_NOCLIP), pickup sound "buttons/button7.wav" -
// and confirms there is NOT a single firing/weapon sound precached
// anywhere, matching the doc's conclusion that this is a
// carry-and-use quest item (presumably interacted with at
// button_aiwallplug terminals, see rust7d_misc.cpp) rather than a
// combat weapon.
//
// The Retail primary action was resolved at 0x1007b457: it traces 32 units
// forward with autoaim and activates only a button_aiwallplug hit by that
// trace.  Remaining inherited weapon callbacks are audited separately.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

// Implemented by rust7_misc.cpp.  Retail calls CAiWallPlug through the
// AI-Core's player-use path rather than exposing it as a generic entity Use.
int ActivateAiWallPlug(CBaseEntity* pTarget, CBaseEntity* pActivator);
// Hull-Verfeinerung nach einem Hull-Treffer (Retail-Entsprechung 0x1008d680). Eigene Umsetzung, nicht aus dem
// Retail-Code uebernommen: Ein Linien-Trace zum doppelten Endpunkt, danach Linien-Traces zu den Ecken der
// Box, der jeweils naechste Treffer gewinnt. Ergebnis ist der Oberflaechenpunkt statt des Hull-Endpunkts.
static void RefineHullHit(const Vector& vecSrc, TraceResult& tr, const Vector& mins, const Vector& maxs, edict_t* pEntity)
{
	const Vector vecHullEnd = vecSrc + (tr.vecEndPos - vecSrc) * 2.0f;
	TraceResult tmp;
	UTIL_TraceLine(vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmp);
	if (tmp.flFraction < 1.0f)
	{
		tr = tmp;
		return;
	}

	float bestDistance = 1e6f;
	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			for (int k = 0; k < 2; k++)
			{
				const Vector corner((i ? maxs.x : mins.x), (j ? maxs.y : mins.y), (k ? maxs.z : mins.z));
				const Vector vecEnd = tr.vecEndPos + corner;
				UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmp);
				if (tmp.flFraction < 1.0f)
				{
					const float distance = (tmp.vecEndPos - vecSrc).Length();
					if (distance < bestDistance)
					{
						bestDistance = distance;
						tr = tmp;
					}
				}
			}
		}
	}
}
bool ConsumeAiWallPlugCompletion();

class CAiCore : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	bool GetItemInfo(ItemInfo* p) override;
	bool Deploy() override;
	void PrimaryAttack() override;
	void WeaponIdle() override;

private:
	int ActivateTargetInFront();
};
LINK_ENTITY_TO_CLASS(weapon_aicore, CAiCore);

void CAiCore::Spawn()
{
	pev->classname = MAKE_STRING("weapon_aicore");
	Precache();
	m_iId = WEAPON_AICORE;
	SET_MODEL(ENT(pev), "models/w_aicore.mdl");

	FallInit();
}

void CAiCore::Precache()
{
	PrecacheModel("models/v_aicore.mdl");
	PrecacheModel("models/w_aicore.mdl");
	PrecacheSound("buttons/button7.wav");
}

bool CAiCore::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = nullptr;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 5;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_AICORE;
	p->iWeight = 0;

	return true;
}

bool CAiCore::Deploy()
{
	return DefaultDeploy("models/v_aicore.mdl", "models/w_aicore.mdl", 0, "onehanded");
}

int CAiCore::ActivateTargetInFront()
{
	if (!m_pPlayer)
		return -1;

	const Vector vecStart = m_pPlayer->GetGunPosition();
	// Retail FUN_1007b457 scales the autoaim vector by the float at
	// 0x100ea884, whose raw value is exactly 32.0.
	const Vector vecEnd = vecStart + m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES) * 32.0f;
	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	// Retail (0x1007b450, Trace-Zweig): Trifft der Linien-Trace nichts (Anteil 1.0), folgt ein Hull-Trace mit
	// Hull-Index 3 (head_hull, Box -16/-16/-18 bis 16/16/18, dieselbe Box wie im Retail-Verfeinerungsaufruf
	// 0x1008d680). Das Ergebnis dieses Traces bestimmt das Ziel. Verfeinerung siehe RefineHullHit.
	if (tr.flFraction >= 1.0f)
	{
		UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, m_pPlayer->edict(), &tr);

		// Retail (Verfeinerung nach dem Hull-Treffer): Die Pruefung ueber Vtable-Slot 35 des Trefferobjekts
		// (0x100014a0) ist IsBSPModel() (solid == SOLID_BSP oder movetype == MOVETYPE_PUSHSTEP). Nur dann
		// wird RefineHullHit mit der Duck-Hull-Box ausgefuehrt (findings/weapons/weapon_aicore.md, Nachtrag 5).
		if (tr.flFraction < 1.0f && tr.pHit)
		{
			CBaseEntity* pHitEntity = CBaseEntity::Instance(tr.pHit);
			if (pHitEntity && pHitEntity->IsBSPModel())
				RefineHullHit(vecStart, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict());
		}
	}

	CBaseEntity* pTarget = tr.pHit ? CBaseEntity::Instance(tr.pHit) : nullptr;
	return ActivateAiWallPlug(pTarget, m_pPlayer);
}

void CAiCore::PrimaryAttack()
{
	const int result = ActivateTargetInFront();
	if (result < 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3f;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.3f;
		return;
	}

	if (result == 1)
	{
		EMIT_SOUND(m_pPlayer->edict(), CHAN_ITEM, "buttons/button7.wav", 1.0, ATTN_NORM);
		// Retail: bei Aktivierung Sequenz "coreplugin" (Index 1, v_aicore.mdl; Sendung im Original belegt).
		SendWeaponAnim(1);
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1f;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.3f;

	if (result == 2)
	{
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CBasePlayerItem::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1f;
	}
}

void CAiCore::WeaponIdle()
{
	// Retail FUN_1007b890 consumes the global flag written by
	// CAiWallPlug::AiWallPlugThink and immediately re-deploys the view model.
	if (ConsumeAiWallPlugCompletion())
	{
		Deploy();
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
		return;
	}

	// The same Retail idle callback probes the 32-unit interaction trace.  It
	// intentionally stays silent on a miss; only PrimaryAttack plays the empty
	// feedback sound.
	const int result = ActivateTargetInFront();
	if (result == 1)
		EMIT_SOUND(m_pPlayer->edict(), CHAN_ITEM, "buttons/button7.wav", 1.0, ATTN_NORM);
	else if (result == 2)
	{
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CBasePlayerItem::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1f;
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1f;
}
