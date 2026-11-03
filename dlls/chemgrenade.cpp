//=========================================================
// CChemGrenade ("chem_grenade"), siehe chemgrenade.h.
//
// Frisch dekompiliert aus gunman.dll (2026-10-31): SplatterTouch @0x10092040, Splatter @0x100922c0, ChemicalSmoke
// @0x10092ad0, ChemicalSmokeThink @0x10092af0, StickyChemicalTouch @0x10092e70, BouncyChemicalTouch @0x10093120,
// TimedChemicalThink @0x10093780, ChemicalExplode @0x10093f40, ChemicalTrail @0x10094110, die Chemie-Flaechenschaden-
// Funktion @0x10094880 und die Explosion @0x10094160 (identisch zu CGrenade::Explode, daher dort aufgerufen).
//
// Vereinfachungen (siehe Findings): die Wurfzuendzeit ist im Dekompilat verdeckt (hier vom Aufrufer uebergeben); die
// Blutsprite-Bedingung (Retail Vtable-Aufruf auf ein Globalobjekt) wird als "an" angenommen; die Splatter-Strahlen
// verwenden die Ebenennormale als Basisrichtung (Retail liest dort globale Werte); Save/Restore speichert die eigenen
// Felder, nicht die unbekannte Retail-Tabelle @0x10128688.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "gamerules.h"
#include "decals.h"
#include "chemgrenade.h"
#include "dbg_log.h"

LINK_ENTITY_TO_CLASS(chem_grenade, CChemGrenade);

TYPEDESCRIPTION CChemGrenade::m_SaveData[] =
	{
		DEFINE_FIELD(CChemGrenade, m_fBounced, FIELD_INTEGER),
		DEFINE_FIELD(CChemGrenade, m_flSplatterTime, FIELD_TIME),
		DEFINE_FIELD(CChemGrenade, m_fSticky, FIELD_INTEGER),
		DEFINE_ARRAY(CChemGrenade, m_Params, FIELD_INTEGER, 10),
};

IMPLEMENT_SAVERESTORE(CChemGrenade, CGrenade);

// Retail-Dekalnamen {cg_grn (0x43), {cg_yel (0x44), {cg_orn (0x45), {cg_red (0x46): Saeure gruen, Base rot, sonst gelb.
static int ChemDecal(int iType)
{
	if (iType == CHEM_BASE)
		return DECAL_CG_RED;
	if (iType == CHEM_ACID)
		return DECAL_CG_GRN;
	return DECAL_CG_YEL;
}

static int ChemBloodColor(int iType)
{
	if (iType == CHEM_ACID)
		return 0x35;
	if (iType == CHEM_BASE)
		return 0xf7;
	return 0xc3;
}

void CChemGrenade::Precache()
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("models/tubeball.mdl");
	PRECACHE_MODEL("models/stickygib.mdl");
	m_iSmokeSprite = PRECACHE_MODEL("sprites/smoker_gray.spr");
	PRECACHE_MODEL("sprites/orangesplash.spr");
	PRECACHE_SOUND("weapons/fizzy1.wav");
	PRECACHE_SOUND("weapons/sprog1.wav");
	PRECACHE_SOUND("weapons/sprog2.wav");
	PRECACHE_SOUND("weapons/cgbounce.wav");
	PRECACHE_SOUND("weapons/debris1.wav");
	PRECACHE_SOUND("weapons/debris2.wav");
	PRECACHE_SOUND("weapons/debris3.wav");
	m_iSprGreen = PRECACHE_MODEL("sprites/greensplash.spr");
	m_iSprRed = PRECACHE_MODEL("sprites/redsplash.spr");
	m_iSprYellow = PRECACHE_MODEL("sprites/yellowsplash.spr");
	m_iSprThrower = PRECACHE_MODEL("sprites/fthrower.spr");
	m_usSpray = PRECACHE_EVENT(1, "events/ChemicalSpray.sc");
}

// Retail @0x10091f30 / @0x100204f0
void CChemGrenade::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = MAKE_STRING("chem_grenade");
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), "models/grenade.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->dmg = 100;
	m_fBounced = 0;
	m_flSplatterTime = 0;
}

static int ChemSkin(int iType)
{
	if (iType == CHEM_ACID)
		return 0;
	if (iType == CHEM_BASE)
		return 3;
	if (iType == CHEM_NEUTRAL)
		return 1;
	return 2;
}

// Gemeinsamer Teil von @0x10093b50 / @0x10093d90
void CChemGrenade::Setup(entvars_t* pevOwner, const Vector& vecStart, const Vector& vecVelocity, const ChemParams& params)
{
	m_Params = params;
	pev->gravity = 0.5f;
	UTIL_SetOrigin(pev, vecStart);
	pev->velocity = vecVelocity;
	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->owner = ENT(pevOwner);
	pev->avelocity.x = RANDOM_FLOAT(-100, -500);
	pev->dmg = (float)params.iDamage;
	SET_MODEL(ENT(pev), "models/tubeball.mdl");
	pev->skin = ChemSkin(params.iType);
}

CChemGrenade* CChemGrenade::ShootSticky(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, const ChemParams& params)
{
	const float flFuse = params.flBounce * 0.1f;

	CChemGrenade* pGrenade = GetClassPtr((CChemGrenade*)nullptr);
	pGrenade->Spawn();
	pGrenade->Setup(pevOwner, vecStart, vecVelocity, params);
	pGrenade->pev->dmgtime = gpGlobals->time + flFuse;
	pGrenade->SetThink(&CChemGrenade::TimedChemicalThink);
	pGrenade->pev->nextthink = gpGlobals->time + 0.1f;
	if (flFuse < 0.1f)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = g_vecZero;
	}
	pGrenade->SetTouch(&CChemGrenade::StickyChemicalTouch);
	return pGrenade;
}

CChemGrenade* CChemGrenade::ShootBouncing(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, const ChemParams& params)
{
	const int iFuse = (int)(params.flBounce * 0.1f);

	CChemGrenade* pGrenade = GetClassPtr((CChemGrenade*)nullptr);
	pGrenade->Spawn();
	pGrenade->Setup(pevOwner, vecStart, vecVelocity, params);
	pGrenade->pev->dmgtime = gpGlobals->time + (float)iFuse;
	pGrenade->SetThink(&CChemGrenade::TimedChemicalThink);
	pGrenade->pev->nextthink = gpGlobals->time + 0.1f;
	if ((double)iFuse < 0.1)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = g_vecZero;
	}
	pGrenade->SetTouch(&CChemGrenade::BouncyChemicalTouch);
	return pGrenade;
}

CChemGrenade* CChemGrenade::ShootContact(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, const ChemParams& params)
{
	CChemGrenade* pGrenade = GetClassPtr((CChemGrenade*)nullptr);
	pGrenade->Spawn();
	pGrenade->Setup(pevOwner, vecStart, vecVelocity, params);
	pGrenade->SetThink(&CChemGrenade::ChemicalTrail);
	pGrenade->pev->nextthink = gpGlobals->time + 0.2f;
	pGrenade->SetTouch(&CChemGrenade::SplatterTouch);
	return pGrenade;
}

// Retail @0x10094110: Spurereignis, danach die Gefahrenmeldung der Granate (CGrenade::DangerSoundThink).
void CChemGrenade::ChemicalTrail()
{
	PLAYBACK_EVENT_FULL(FEV_RELIABLE, ENT(pev), m_usSpray, 0, g_vecZero, g_vecZero, 0, 0, pev->skin, 0, 0, 0);
	DangerSoundThink();
}

// Retail @0x10092040: Treffer -> Himmel entfernt die Granate; sonst Flaeche suchen, zerplatzen, entfernen.
void CChemGrenade::SplatterTouch(CBaseEntity* pOther)
{
	DBG_LOG("CHEMGRENADE %d SplatterTouch other=%s origin=(%.1f %.1f %.1f) velocity=(%.1f %.1f %.1f)",
		ENTINDEX(edict()), pOther ? STRING(pOther->pev->classname) : "world",
		pev->origin.x, pev->origin.y, pev->origin.z,
		pev->velocity.x, pev->velocity.y, pev->velocity.z);
	if (UTIL_PointContents(pev->origin) == CONTENTS_SKY)
	{
		UTIL_Remove(this);
		return;
	}

	pev->enemy = pOther->edict();

	const Vector vecDir = pev->velocity.Length() == 0 ? Vector(0, 0, 1) : pev->velocity.Normalize();
	const Vector vecStart = pev->origin - vecDir * 32;
	const Vector vecEnd = vecStart + vecDir * 64;

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev), &tr);
	SetTouch(nullptr);
	Splatter(&tr);
	UTIL_Remove(this);
}

// Retail @0x100922c0
void CChemGrenade::Splatter(TraceResult* pTrace)
{
	pev->modelindex = 0;
	pev->model = 0;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;

	Vector vecBlood;
	if (pTrace->flFraction == 1.0f)
	{
		vecBlood = pev->origin;
	}
	else
	{
		const float flAdj = pev->dmg - 24.0f;
		vecBlood = pTrace->vecEndPos + pTrace->vecPlaneNormal * (flAdj * 0.6f);
	}

	UTIL_PointContents(pev->origin);
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + pev->velocity * 10.0f, dont_ignore_monsters, ENT(pev), &tr);

	// Spritespray in der Chemiefarbe
	int iSprite = m_Params.iType == CHEM_ACID ? m_iSprGreen : (m_Params.iType == CHEM_BASE ? m_iSprRed : m_iSprYellow);
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos);
	WRITE_BYTE(TE_SPRITE_SPRAY);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_COORD(tr.vecPlaneNormal.x);
	WRITE_COORD(tr.vecPlaneNormal.y);
	WRITE_COORD(tr.vecPlaneNormal.z);
	WRITE_SHORT(iSprite);
	WRITE_BYTE(25);
	WRITE_BYTE(50);
	WRITE_BYTE(200);
	MESSAGE_END();

	// Blutsprites nur im Einzelspieler (Retail: g_pGameRules->IsMultiplayer() == 0, Vtable-Slot 6)
	const int iColor = ChemBloodColor(m_Params.iType);
	static const float kOffset[3] = {0, 5, 9};
	static const int kScale[3] = {16, 12, 30};
	for (int i = 0; i < 3 && !g_pGameRules->IsMultiplayer(); i++)
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecBlood);
		WRITE_BYTE(TE_BLOODSPRITE);
		WRITE_COORD(vecBlood.x + kOffset[i]);
		WRITE_COORD(vecBlood.y + kOffset[i]);
		WRITE_COORD(vecBlood.z + kOffset[i]);
		WRITE_SHORT(g_sModelIndexBloodSpray);
		WRITE_SHORT(g_sModelIndexBloodDrop);
		WRITE_BYTE(iColor);
		WRITE_BYTE(kScale[i]);
		MESSAGE_END();
	}

	// 12 Strahlen (Streuung +-2 plus Ebenennormale, Reichweite 20 und 40) setzen Dekale in der Umgebung.
	const int iDecal = ChemDecal(m_Params.iType);
	for (int i = 0; i < 12; i++)
	{
		const float flR1 = RANDOM_FLOAT(-2, 2), flR2 = RANDOM_FLOAT(-2, 2), flR3 = RANDOM_FLOAT(-2, 2);
		for (int iRange = 20; iRange <= 40; iRange += 20)
		{
			TraceResult trDecal;
			const Vector vecEnd = pTrace->vecEndPos + Vector(flR1 + pTrace->vecPlaneNormal.x, flR2 + pTrace->vecPlaneNormal.y, flR3 + pTrace->vecPlaneNormal.z) * (float)iRange;
			UTIL_TraceLine(pTrace->vecEndPos, vecEnd, ignore_monsters, ENT(pev), &trDecal);
			if (trDecal.flFraction != 1.0f)
				UTIL_DecalTrace(&trDecal, iDecal);
		}
	}

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0f);

	entvars_t* pevOwner = pev->owner ? VARS(pev->owner) : nullptr;
	pev->owner = nullptr;
	ChemRadiusDamage(pev->origin, pev, pevOwner, pev->dmg, pev->dmg * 2.5f, 0, m_Params.iType);
	UTIL_DecalTrace(pTrace, iDecal);

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/fizzy1.wav", 0.25f, 0.8f, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/sprog1.wav", 0.25f, 0.8f, 0, 100);
		break;
	default:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/sprog2.wav", 0.25f, 0.8f, 0, 100);
		break;
	}

	pev->effects |= EF_NODRAW;
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 1.0f;
	m_flSplatterTime = gpGlobals->time;
}

// Retail @0x10092ad0: bleibt 10 s als Giftwolke liegen.
void CChemGrenade::ChemicalSmoke()
{
	SetThink(&CChemGrenade::ChemicalSmokeThink);
	pev->nextthink = gpGlobals->time;
}

void CChemGrenade::ChemicalSmokeThink()
{
	if (gpGlobals->time - m_flSplatterTime > 10.0f)
	{
		UTIL_Remove(this);
		return;
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITE_SPRAY);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_COORD(1.0f);
	WRITE_SHORT(m_iSmokeSprite);
	WRITE_BYTE(3);
	WRITE_BYTE(50);
	WRITE_BYTE(50);
	MESSAGE_END();

	// Retail: RadiusDamage(Ursprung, pev, Angreifer 0, Schaden 2, Radius pev->dmg, Klasse 0, DMG_RADIATION)
	::RadiusDamage(pev->origin, pev, pev, 2.0f, pev->dmg, 0, DMG_RADIATION);
	pev->nextthink = gpGlobals->time + 0.2f;
}

// Retail @0x10092e70: bleibt an der Flaeche kleben.
void CChemGrenade::StickyChemicalTouch(CBaseEntity* pOther)
{
	if (UTIL_PointContents(pev->origin) == CONTENTS_SKY)
	{
		UTIL_Remove(this);
		return;
	}

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + pev->velocity * 32.0f, dont_ignore_monsters, ENT(pev), &tr);
	m_fSticky = 1;

	int iDecal;
	if (m_Params.iType == CHEM_ACID)
		iDecal = DECAL_CG_GRN;
	else if (m_Params.iType == CHEM_NEUTRAL)
		iDecal = DECAL_CG_YEL;
	else
		iDecal = DECAL_CG_RED;
	UTIL_DecalTrace(&tr, iDecal);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/cgbounce.wav", 0.55f, 0.8f, 0, 100);

	pev->velocity = tr.vecPlaneNormal * -1.0f;
	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;

	// Retail vergleicht den Klassennamen des Getroffenen mit einer Tabelle (@0x10115174): bei Treffer Movetype NONE,
	// sonst Solid NOT und Movetype FOLLOW (Zahl 6 in Retail = TOSS wird hier nicht unterschieden).
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;

	if (m_Params.iType != CHEM_EXPLOSIVE && m_Params.iType != CHEM_MIXED && m_Params.iType != CHEM_NEUTRAL)
	{
		SetThink(&CChemGrenade::ChemicalSmoke);
		pev->velocity = g_vecZero;
		pev->nextthink = gpGlobals->time + 0.1f;
		SetTouch(nullptr);
		m_flSplatterTime = gpGlobals->time;
	}
}

// Retail @0x10093120
void CChemGrenade::BouncyChemicalTouch(CBaseEntity* pOther)
{
	DBG_LOG("CHEMGRENADE %d BouncyTouch other=%s origin=(%.1f %.1f %.1f) velocity=(%.1f %.1f %.1f)",
		ENTINDEX(edict()), pOther ? STRING(pOther->pev->classname) : "world",
		pev->origin.x, pev->origin.y, pev->origin.z,
		pev->velocity.x, pev->velocity.y, pev->velocity.z);
	if (UTIL_PointContents(pev->origin) != CONTENTS_SKY)
	{
		if (pOther->edict() == pev->owner)
			return;

		m_Params.flBounce -= pev->velocity.Length() * 0.06666667f;

		if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100.0f)
		{
			entvars_t* pevOwner = pev->owner ? VARS(pev->owner) : nullptr;
			if (pevOwner)
			{
				TraceResult tr = UTIL_GetGlobalTrace();
				ClearMultiDamage();
				pOther->TraceAttack(pevOwner, 1.0f, gpGlobals->v_forward, &tr, DMG_CLUB);
				ApplyMultiDamage(pev, pevOwner);
			}
			m_flNextAttack = gpGlobals->time + 1.0f;
		}

		const float vz = pev->velocity.z * 0.45f;
		if (m_Params.flBounce >= 0 && (m_fBounced != 0 || sqrt(pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y + vz * vz) > 100.0f))
		{
			if ((pev->flags & FL_ONGROUND) == 0)
			{
				int iDiv = (30 - m_Params.iViscosity) / 15;
				if (iDiv < 1)
					iDiv = 1;
				pev->velocity = pev->velocity / (float)iDiv;
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/cgbounce.wav", 0.55f, 0.8f, 0, 100);
			}
			else
			{
				pev->velocity = pev->velocity * 0.8f;
				pev->sequence = RANDOM_LONG(1, 1);
			}

			pev->framerate = pev->velocity.Length() * 0.005f;
			if (pev->framerate > 1.0f)
				pev->framerate = 1.0f;
			else if (pev->framerate < 0.5f)
				pev->framerate = 0;
			return;
		}

		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 200, 0.3f);
		m_fBounced = 1;

		if (m_Params.iType == CHEM_EXPLOSIVE || m_Params.iType == CHEM_MIXED)
		{
			ChemicalExplode();
		}
		else
		{
			const Vector vecDir = pev->velocity.Length() == 0 ? Vector(0, 0, 1) : pev->velocity.Normalize();
			TraceResult tr;
			UTIL_TraceLine(pev->origin - vecDir * 32, pev->origin - vecDir * 32 + vecDir * 64, ignore_monsters, ENT(pev), &tr);
			Splatter(&tr);
		}
	}
	UTIL_Remove(this);
}

// Retail @0x10093780
void CChemGrenade::TimedChemicalThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	if (m_fSticky == 0)
		PLAYBACK_EVENT_FULL(FEV_RELIABLE, ENT(pev), m_usSpray, 0, g_vecZero, g_vecZero, 0, 0, pev->skin, 0, 0, 0);

	DangerSoundThink();
	pev->nextthink = gpGlobals->time + 0.1f;

	if (pev->dmgtime - 1.0f < gpGlobals->time)
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), (int)pev->velocity.Length(), 0.1f);

	if (pev->dmgtime <= gpGlobals->time)
	{
		if (m_Params.iType != CHEM_EXPLOSIVE && m_Params.iType != CHEM_MIXED)
		{
			const Vector vecDir = pev->velocity.Length() == 0 ? Vector(0, 0, 1) : pev->velocity.Normalize();
			TraceResult tr;
			UTIL_TraceLine(pev->origin - vecDir * 32, pev->origin - vecDir * 32 + vecDir * 64, ignore_monsters, ENT(pev), &tr);
			Splatter(&tr);
			UTIL_Remove(this);
			return;
		}
		ChemicalExplode();
		UTIL_Remove(this);
		return;
	}

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5f;
		pev->framerate = 0.2f;
	}
}

// Retail @0x10093f40
void CChemGrenade::ChemicalExplode()
{
	TraceResult tr;
	const Vector vecStart = pev->origin + Vector(0, 0, 8.0f);
	UTIL_TraceLine(vecStart, vecStart - Vector(0, 0, 40.0f), ignore_monsters, ENT(pev), &tr);

	pev->dmg = (float)m_Params.iDamage;
	Explode(&tr, DMG_BLAST);

	if (m_Params.iType == CHEM_MIXED)
	{
		pev->owner = nullptr;
		m_Params.iType = m_Params.iAcid < m_Params.iBase ? CHEM_ACID : CHEM_BASE;
		const int iColor = m_Params.iType == CHEM_ACID ? 0x35 : 0xf7;
		for (int i = 0; i < 2; i++)
		{
			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
			WRITE_BYTE(TE_BLOODSPRITE);
			WRITE_COORD(pev->origin.x);
			WRITE_COORD(pev->origin.y);
			WRITE_COORD(pev->origin.z);
			WRITE_SHORT(g_sModelIndexBloodSpray);
			WRITE_SHORT(g_sModelIndexBloodDrop);
			WRITE_BYTE(iColor);
			WRITE_BYTE(16);
			MESSAGE_END();
		}
		ChemRadiusDamage(pev->origin, pev, nullptr, pev->dmg, pev->dmg * 2.5f, 0, m_Params.iType);
	}
}

// Retail @0x10094880: Flaechenschaden mit Chemie-Art. Saeure trifft Lebewesen voll, Base Maschinen, sonst halber/ viertel Schaden.
void CChemGrenade::ChemRadiusDamage(const Vector& vecSrc, entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, float flRadius, int iClassIgnore, int iType)
{
	const float flFalloff = flRadius == 0 ? 1.0f : flDamage / flRadius;
	const bool bInWater = UTIL_PointContents(vecSrc) == CONTENTS_WATER;
	if (!pevAttacker)
		pevAttacker = pevInflictor;

	CBaseEntity* pEntity = nullptr;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, flRadius)) != nullptr)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO)
			continue;
		if (iClassIgnore != 0 && pEntity->Classify() == iClassIgnore)
			continue;
		if (bInWater ? pEntity->pev->waterlevel == 0 : pEntity->pev->waterlevel == 3)
			continue;

		const Vector vecSpot = pEntity->BodyTarget(vecSrc);
		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSpot, dont_ignore_monsters, ENT(pevInflictor), &tr);

		if (tr.flFraction != 1.0f && tr.pHit != pEntity->edict())
			continue;

		if (tr.fStartSolid)
		{
			tr.vecEndPos = vecSrc;
			tr.flFraction = 0.0f;
		}

		float flAdjusted = flDamage - (vecSrc - tr.vecEndPos).Length() * flFalloff;
		if (flAdjusted < 0)
			flAdjusted = 0;

		const int iClass = pEntity->Classify();
		const bool bMachine = iClass == 11 || iClass == 12;
		const bool bLiving = iClass == 2 || iClass == 4 || iClass == 5;
		int iBits;
		if (iType == CHEM_ACID)
		{
			if (!bLiving)
				flAdjusted *= 0.5f;
			iBits = DMG_ACID | DMG_NEVERGIB;
		}
		else if (iType == CHEM_BASE)
		{
			if (!bMachine)
				flAdjusted *= 0.5f;
			iBits = DMG_BURN | DMG_NEVERGIB;
		}
		else if (iType == CHEM_NEUTRAL)
		{
			flAdjusted *= 0.25f;
			iBits = DMG_BLAST | DMG_NEVERGIB;
		}
		else
		{
			flAdjusted *= 0.8f;
			iBits = DMG_BLAST;
		}

		if (tr.flFraction != 1.0f)
		{
			DBG_LOG("CHEMGRENADE %d RadiusTrace target=%s damage=%.1f type=%d",
				ENTINDEX(ENT(pevInflictor)), STRING(pEntity->pev->classname), flAdjusted, iType);
			ClearMultiDamage();
			pEntity->TraceAttack(pevInflictor, flAdjusted, (tr.vecEndPos - vecSrc).Normalize(), &tr, iBits);
			ApplyMultiDamage(pevInflictor, pevAttacker);
		}
		else
		{
			DBG_LOG("CHEMGRENADE %d RadiusDamage target=%s damage=%.1f type=%d",
				ENTINDEX(ENT(pevInflictor)), STRING(pEntity->pev->classname), flAdjusted, iType);
			pEntity->TakeDamage(pevInflictor, pevAttacker, flAdjusted, iBits);
		}
	}
}
