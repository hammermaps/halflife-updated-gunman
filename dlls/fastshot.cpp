//=========================================================
// CFastShot ("gauss_charged"), siehe fastshot.h.
//
// Retail-Befunde (frisch dekompiliert 2026-10-31): Movetype FLY (5), Solid TRIGGER (2), Modell
// sprites/gausswomp.spr, Rendermode additiv (5) mit Amt 255, Skalierung 0,25, Groesse 0. Beim Erzeugen wird ein
// TE_BEAMFOLLOW (Lebensdauer 2, Breite 2, weiss) mit sprites/muzzleflash3.spr an das Projektil gehaengt.
// Beim Aufprall: Schaden DMG_BULLET|DMG_NEVERGIB ueber TraceAttack mit pev->dmg, Geschwindigkeit 0, gauss_fire3
// (Lautstaerke 0,35) auf lebenden Zielen, Aufprall-Ereignis events/RapidImpact.sc, InsertSound(COMBAT, 600, 0,1),
// danach unsichtbar und nach 0,1 s entfernt.
//
// Vereinfachungen: Die Retail-Abfrage "Ziel lebt" (Vtable-Slot 0x9c) ist als IsAlive()
// angenommen. Spawn setzt in Retail kein nextthink: Animate laeuft erst, wenn jemand es plant, und wird hier
// ebenfalls nicht gestartet.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "soundent.h"
#include "fastshot.h"

LINK_ENTITY_TO_CLASS(gauss_charged, CFastShot);

void CFastShot::Precache()
{
	m_iTrail = PRECACHE_MODEL("sprites/muzzleflash3.spr");
	m_iGlow = PRECACHE_MODEL("sprites/gaussglow2.spr");
	PRECACHE_MODEL("sprites/gausswomp.spr");
	PRECACHE_MODEL("sprites/gausspuff.spr");
	PRECACHE_SOUND("weapons/gauss_fire3.wav");
	PRECACHE_SOUND("weapons/gauss_spritesmall.wav");
	m_usImpact = PRECACHE_EVENT(1, "events/RapidImpact.sc");
}

void CFastShot::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING("gauss_charged");
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	SET_MODEL(ENT(pev), "sprites/gausswomp.spr");
	SetTouch(&CFastShot::FastShotTouch);
	SetThink(&CFastShot::Animate);
	pev->frame = 0;
	pev->scale = 0.25f;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	m_flMaxFrame = (float)MODEL_FRAMES(pev->modelindex) - 1;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(m_iTrail);
	WRITE_BYTE(2);
	WRITE_BYTE(2);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	MESSAGE_END();
}

void CFastShot::Animate()
{
	pev->nextthink = gpGlobals->time + 0.1f;
	const bool bWasAnimating = pev->frame != 0;
	pev->frame += 1;
	if (bWasAnimating && m_flMaxFrame < pev->frame)
		pev->frame = 0;
}

void CFastShot::FastShotTouch(CBaseEntity* pOther)
{
	const Vector vecFlight = pev->velocity;
	if (pOther && pOther->pev->takedamage != DAMAGE_NO)
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		entvars_t* pevOwner = pev->owner ? VARS(pev->owner) : pev;

		ClearMultiDamage();
		pOther->TraceAttack(pevOwner, pev->dmg, vecFlight.Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB);
		ApplyMultiDamage(pev, pevOwner);
		pev->velocity = g_vecZero;

		if (pOther->IsAlive())
			EMIT_SOUND_DYN(ENT(pOther->pev), CHAN_WEAPON, "weapons/gauss_fire3.wav", 0.35f, ATTN_NORM, 0, 70);
	}

	// Retail uebergibt Nullvektoren (die Engine setzt dann Ursprung/Winkel des Projektils); hier explizit, Winkel in
	// der AngleVectors-Konvention (Nick invertiert).
	Vector vecAngles = UTIL_VecToAngles(vecFlight);
	vecAngles.x = -vecAngles.x;
	PLAYBACK_EVENT_FULL(0, ENT(pev), m_usImpact, 0, pev->origin, vecAngles, 0, 0, 0, 0, 0, 0);
	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 600, 0.1f);

	SetTouch(nullptr);
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->effects |= EF_NODRAW;
}

void CFastShot::Expire()
{
	UTIL_Remove(this);
}

CFastShot* CFastShot::Create(entvars_t* pevOwner, const Vector& vecOrigin, const Vector& vecVelocity, float flDamage)
{
	CFastShot* pShot = GetClassPtr((CFastShot*)nullptr);
	UTIL_SetOrigin(pShot->pev, vecOrigin);
	pShot->Spawn();
	pShot->pev->dmg = flDamage;
	pShot->pev->velocity = vecVelocity;
	pShot->pev->angles = UTIL_VecToAngles(vecVelocity);
	pShot->pev->owner = ENT(pevOwner);
	// Abweichung von Retail (Bugfix 2026-11-01): Ein verfehltes Projektil (kein
	// FastShotTouch) blieb in der SDK-Version dauerhaft als Sprite in der Luft
	// stehen, weil Spawn kein nextthink setzt und nichts es entfernt. Nach 3 s
	// wird es entfernt. Treffer entfernen es weiterhin sofort (FastShotTouch).
	// Retail-Verhalten fuer verfehlte Schuesse ist nicht belegt.
	pShot->SetThink(&CFastShot::Expire);
	pShot->pev->nextthink = gpGlobals->time + 3.0f;
	return pShot;
}
