//=========================================================
// Gunman Chronicles custom entities that have no stock Half-Life SDK
// counterpart at all (not overrides of an existing SDK class - genuinely
// new LINK_ENTITY_TO_CLASS names). Added incrementally, one class at a
// time, in the order the retail campaign's maps (maps-src/, in story
// order) actually require them, each one freshly decompiled from
// gunman.dll and build-tested before moving to the next - see STATUS.md
// for the per-class session log.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "effects.h"
#include "explode.h"
#include "weapons.h"
#include "gamerules.h"
#include "player.h"
#include "shake.h"
#include "items.h"
#include "soundent.h"
#include "customentity.h" // BEAM_F* Render-Flags (nicht die SF_BEAM_*-Spawnflags aus effects.h)
#include "gunman_sphere_explosion.h"

//=========================================================
// decore_asteroid - a spinning decorative rock prop. Used in CITY1A (and
// elsewhere) as pure background scenery.
//
// Decompiled from gunman.dll (Spawn @0x100ce6d0, KeyValue @0x100ce650,
// vtable @0x101073d8, slot 0/2 - same CBaseMonster-derived vtable shape
// as CFurniture/CGunmanCycler). No FGD model field - the model is
// hardcoded; only keyvalue is "asteroidsize" (Choices 0/1/2 = big/
// medium/small, default 1), directly confirmed against
// Gunman-ENG-GER/rewolf/halflife.fgd.
//
// ABWEICHENDE ÄNDERUNG VOM ORIGINAL (2026-09-05, auf Nutzerwunsch):
// zusätzlich zum bestätigten reinen Eigenspin (avelocity) driftet der
// Asteroid jetzt langsam und begrenzt um seine Ausgangsposition, um ein
// natürlicheres, "schwebendes" Verhalten zu erzielen - das ist KEINE
// dekompilierte Retail-Eigenschaft, sondern eine bewusste, hier
// dokumentierte Erweiterung. Konfigurierbar über zwei neue, in der FGD
// ergänzte Keyvalues ("driftradius"/"driftspeed"); da bestehende
// Retail-Karten diese Felder nicht setzen, greift ein sinnvoller
// Standardwert (24 Units Radius, 6 Units/s), sodass das Verhalten auch
// unverändert platzierter Karten sofort sichtbar, aber dezent bleibt.
// `driftradius 0` deaktiviert das Driften vollständig (reines
// Spawn-Punkt-Verhalten wie zuvor).
//=========================================================
class CDecoreAsteroid : public CBaseMonster
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT DriftThink();

private:
	int m_iAsteroidSize = 1;
	// Gunman-eigene Erweiterung, siehe Datei-Header - keine Retail-Werte.
	float m_flDriftRadius = 24.0f;
	float m_flDriftSpeed = 6.0f;
	Vector m_vecStartOrigin;
};
LINK_ENTITY_TO_CLASS(decore_asteroid, CDecoreAsteroid);

TYPEDESCRIPTION CDecoreAsteroid::m_SaveData[] =
{
	DEFINE_FIELD(CDecoreAsteroid, m_flDriftRadius, FIELD_FLOAT),
	DEFINE_FIELD(CDecoreAsteroid, m_flDriftSpeed, FIELD_FLOAT),
	DEFINE_FIELD(CDecoreAsteroid, m_vecStartOrigin, FIELD_POSITION_VECTOR),
};

IMPLEMENT_SAVERESTORE(CDecoreAsteroid, CBaseMonster);

bool CDecoreAsteroid::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "asteroidsize"))
	{
		m_iAsteroidSize = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "driftradius"))
	{
		// Gunman-eigene Erweiterung, siehe Datei-Header.
		m_flDriftRadius = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "driftspeed"))
	{
		// Gunman-eigene Erweiterung, siehe Datei-Header.
		m_flDriftSpeed = atof(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

void CDecoreAsteroid::Spawn()
{
	pev->classname = MAKE_STRING("decore_asteroid");
	PrecacheModel("models/rockspin.mdl");
	SET_MODEL(ENT(pev), "models/rockspin.mdl");

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY; // no gravity, avelocity spin is engine-integrated
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = 60;
	// asteroidsize picks the model's body submodel (0/1/2 = big/medium/small)
	pev->body = m_iAsteroidSize;
	pev->velocity = g_vecZero;

	// Random tumble: wider spin range for bigger asteroids.
	float flSpinRange = (float)((m_iAsteroidSize + 1) * 10);
	pev->avelocity.x = RANDOM_FLOAT(-flSpinRange, flSpinRange);
	pev->avelocity.y = RANDOM_FLOAT(-flSpinRange, flSpinRange);
	pev->avelocity.z = RANDOM_FLOAT(-flSpinRange, flSpinRange);

	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	// Gunman-eigene Erweiterung (siehe Datei-Header): begrenztes Driften
	// um die Ausgangsposition, sofern nicht per Keyvalue deaktiviert.
	m_vecStartOrigin = pev->origin;
	if (m_flDriftRadius > 0.0f)
	{
		SetThink(&CDecoreAsteroid::DriftThink);
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.5, 2.0);
	}
}

// Gunman-eigene Erweiterung (siehe Datei-Header, keine Retail-Funktion):
// wählt periodisch einen neuen Zufallspunkt innerhalb von m_flDriftRadius
// um die Ausgangsposition und driftet sanft dorthin - rein kosmetisch,
// erzeugt ein natürliches "Schweben" ohne die Requisite je weit vom
// vorgesehenen Platz wegtreiben zu lassen.
void CDecoreAsteroid::DriftThink()
{
	Vector vecTarget = m_vecStartOrigin + Vector(
											 RANDOM_FLOAT(-m_flDriftRadius, m_flDriftRadius),
											 RANDOM_FLOAT(-m_flDriftRadius, m_flDriftRadius),
											 RANDOM_FLOAT(-m_flDriftRadius, m_flDriftRadius));

	Vector vecDir = vecTarget - pev->origin;
	float flDist = vecDir.Length();
	pev->velocity = (flDist > 0.1f) ? (vecDir / flDist) * m_flDriftSpeed : g_vecZero;

	// Nächsten Wegpunkt in ein paar Sekunden neu wählen - lang genug für
	// eine ruhige, gleichmäßige Drift, kurz genug um erkennbar zu bleiben.
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(3.0, 6.0);
}


// The trainingbot is declared before CSphereExplosion below, so keep its
// Retail-only paired sphere-effect setup behind this small helper.
static void FireTrainingBotSphereExplosions(CBaseEntity* pBot);
static void SpawnTrainingBotGib(CBaseMonster* pBot, int body);

//=========================================================
// monster_trainingbot - CDummyBot. A floating target/practice drone: three
// legs, each with a permanent decorative beam converging on a center
// sprite, that hovers between waypoints (chained via pev->target/
// GetNextTarget(), same idiom as path_corner). On death it doesn't die
// outright: it goes limp and falls (MOVETYPE_BOUNCE) while smoking, then
// explodes and gibs on ground/wall contact.
//
// Decompiled from gunman.dll: Spawn @0x100cefc0, Precache @0x100cf2f0,
// Save/Restore @0x100cef60/0x100cef90 (class name literal "CDummyBot"),
// Classify @0x100cef50 (constant 7 = CLASS_ALIEN_MONSTER - kept as-is,
// not "corrected" to CLASS_MACHINE, since that's what the binary
// actually returns), TraceAttack wrapper @0x100cf7a0, pain/death handler
// @0x100cf7c0, FloatThink/BotSparkThink/SmokeFallThink/BotExplodeTouch
// (named via the binary's own demangled symbol table), and the beam/
// sprite setup helper @0x100d0550 (matches CBeam::EntsInit/CSprite::
// SetAttachment field-for-field - see findings/entities/misc_monsters.md
// and STATUS.md for the full verification).
//
// Approximated rather than byte-reproduced: the exact "distortion ring"
// (a custom PLAYBACK_EVENT the retail client.dll renders, which we have
// no client-side handler for) and the spark/smoke temp-entity messages
// use a custom Gunman message ID (0x17) our client doesn't understand
// either - substituted with standard, working TE_SPRITE/TE_SMOKE/
// TE_EXPLOSION effects that convey the same three-stage "hover -> smoke
// while falling -> explode+gib" sequence instead of silently doing
// nothing. The hover-speed profile is the sole retained gameplay deviation:
// its exact Retail values are known, but a slower profile was explicitly kept
// after live testing. All timing and lifecycle values otherwise use Retail.
//=========================================================
class CDummyBot : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_ALIEN_MONSTER; }
	// Retail vtable slot 14 (`FUN_100cef40`) is BloodColor (all other monsters
	// return m_bloodColor there) and returns the literal 156 (0x9c); ObjectCaps
	// (slot 5) is the default. Earlier code mistook the literal for ObjectCaps.
	int BloodColor() override { return 156; }
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT FloatThink();
	void EXPORT SmokeFallThink();
	void EXPORT BotExplodeTouch(CBaseEntity* pOther);

private:
	void CreateDecoration();
	void RemoveDecoration();
	void Explode();

	// The names and field types below deliberately follow the ten-entry
	// retail Save/Restore table.  In particular, the four visual children are
	// separate CLASSPTR fields, not one synthetic array entry.
	CBaseEntity* m_pGoalEnt = nullptr;
	Vector m_vGoalPos;
	float m_fSpeed = 0;
	float m_flSlowDown = 0;
	float m_flNextRingTime = 0;
	float m_flNextLightTime = 0;

	int m_iExplosion = 0;
	int m_iSpriteSmoke = 0;
	unsigned short m_usDistortRing = 0;

	CBeam* m_pAttackBeam3 = nullptr;
	CBeam* m_pAttackBeam2 = nullptr;
	CBeam* m_pAttackBeam1 = nullptr;
	CSprite* m_pSprite = nullptr;
};
LINK_ENTITY_TO_CLASS(monster_trainingbot, CDummyBot);

TYPEDESCRIPTION CDummyBot::m_SaveData[] =
	{
		DEFINE_FIELD(CDummyBot, m_pGoalEnt, FIELD_CLASSPTR),
		DEFINE_FIELD(CDummyBot, m_pAttackBeam3, FIELD_CLASSPTR),
		DEFINE_FIELD(CDummyBot, m_pAttackBeam2, FIELD_CLASSPTR),
		DEFINE_FIELD(CDummyBot, m_pAttackBeam1, FIELD_CLASSPTR),
		DEFINE_FIELD(CDummyBot, m_pSprite, FIELD_CLASSPTR),
		DEFINE_FIELD(CDummyBot, m_vGoalPos, FIELD_VECTOR),
		DEFINE_FIELD(CDummyBot, m_iExplosion, FIELD_INTEGER),
		DEFINE_FIELD(CDummyBot, m_fSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CDummyBot, m_flSlowDown, FIELD_TIME),
		DEFINE_FIELD(CDummyBot, m_flNextRingTime, FIELD_TIME),
	};

IMPLEMENT_SAVERESTORE(CDummyBot, CBaseMonster);

void CDummyBot::Precache()
{
	PrecacheModel("models/batterybot.mdl");
	PrecacheModel("sprites/xbeam1.spr");
	PrecacheModel("sprites/hugeball.spr");
	// The final Retail helper creates two sphere_explosion effects at death.
	// Precache their compatible ring sprite during map load, never on touch.
	PrecacheModel("sprites/clustershock.spr");
	m_iExplosion = PrecacheModel("sprites/explosion4.spr");
	m_iSpriteSmoke = PrecacheModel("sprites/dmlsmoke.spr");
	PrecacheModel("models/battgib.mdl");
	PrecacheSound("drone/drone_idle.wav");
	PrecacheSound("drone/drone_flinch1.wav");
	PrecacheSound("drone/drone_flinch2.wav");
	m_usDistortRing = PrecacheEvent(1, "events/batbotbeam.sc");
}

void CDummyBot::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("monster_trainingbot");
	SET_MODEL(ENT(pev), "models/batterybot.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, -32), Vector(32, 32, 32));
	UTIL_SetOrigin(pev, pev->origin);

	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_AIM;
	pev->effects = 0;
	pev->health = 60;
	pev->velocity = g_vecZero;

	RemoveDecoration();

	// Initial waypoint from the stock "target" keyvalue (no custom
	// KeyValue() needed - target/targetname are parsed by the engine
	// itself), falling back to a hardcoded default anchor name if unset.
	if (FStringNull(pev->target))
		pev->target = MAKE_STRING("bot_anchor");

	CBaseEntity* pWaypoint = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));
	m_pGoalEnt = pWaypoint;
	if (pWaypoint)
		m_vGoalPos = pWaypoint->pev->origin;

	// Intentional gameplay deviation: Retail starts at 150-350 ups and halves
	// that speed after four seconds. A live report found that too fast for an
	// ambient target, so the reconstruction keeps its calmer 40-90 ups and
	// one later 1.3x boost. The saved field name still matches Retail.
	m_fSpeed = RANDOM_FLOAT(40.0, 90.0);
	m_flSlowDown = gpGlobals->time + 3.0;
	if (pWaypoint)
	{
		Vector vecDirection = m_vGoalPos - pev->origin;
		if (vecDirection.Length() > 0.001f)
			pev->velocity = vecDirection.Normalize() * m_fSpeed;
	}

	CreateDecoration();

	SetThink(&CDummyBot::FloatThink);
	pev->nextthink = gpGlobals->time + 0.1;

	ResetSequenceInfo();
	pev->frame = 0;

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "drone/drone_idle.wav", 0.65, 0.5, SND_SPAWNING, 100);
}

void CDummyBot::CreateDecoration()
{
	// Drei dauerhafte Deko-Strahlen, je einer von einem Bein-Attachment zur
	// Mitte, plus ein Glow-Sprite im Zentrum - einmalig beim Spawn erzeugt,
	// rein kosmetisch (kein Angriff).
	//
	// WICHTIG (Fix Session 102): Die Attachment-Indizes in CBeam/CSprite sind
	// EINS-BASIERT - 0 bedeutet "kein Attachment, nimm den Entity-Ursprung",
	// 1..n meint Modell-Attachment 0..n-1. Der frisch decompilierte Original-
	// Helfer (FUN_100d0550) setzt entsprechend Ende=1 (=Modell-Attachment 0,
	// die Mitte) und Start=2/3/4 (=Modell-Attachments 1/2/3, die drei Arme);
	// das Modell hat exakt diese 4 Attachments. Unsere vorherige 0-basierte
	// Fassung (Start 1/2/3, Ende 0) liess damit einen Strahl von der Mitte zum
	// Ursprung laufen - also unsichtbar - und den dritten Arm voellig ohne
	// Strahl: exakt der gemeldete "fehlt ein Energiestrahl an einem Arm".
	for (int i = 0; i < 3; i++)
	{
		CBeam* pBeam = CBeam::BeamCreate("sprites/xbeam1.spr", 40);
		pBeam->EntsInit(entindex(), entindex());
		pBeam->SetFlags(BEAM_FSHADEOUT);
		pBeam->SetScrollRate(5);
		pBeam->SetNoise(50);
		pBeam->SetBrightness(255);
		pBeam->SetWidth(25);
		pBeam->SetColor(80, 120, 255);
		pBeam->SetEndAttachment(1);		  // Modell-Attachment 0 = Mitte
		pBeam->SetStartAttachment(i + 2); // Modell-Attachments 1/2/3 = Arme
		pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
		if (i == 0)
			m_pAttackBeam3 = pBeam;
		else if (i == 1)
			m_pAttackBeam2 = pBeam;
		else
			m_pAttackBeam1 = pBeam;
	}

	m_pSprite = CSprite::SpriteCreate("sprites/hugeball.spr", pev->origin, true);
	// kRenderGlow intentionally bypasses Z-buffer testing, which made this
	// central blue sprite visible through the batterybot and nearby geometry.
	// Keep its additive glow, but let depth testing occlude it correctly.
	m_pSprite->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation);
	m_pSprite->SetScale(0.2);
	m_pSprite->pev->framerate = 10;
	m_pSprite->pev->frame = 0;
	m_pSprite->TurnOn();
	m_pSprite->SetAttachment(edict(), 1); // ebenfalls 1-basiert -> Mitte
}

void CDummyBot::RemoveDecoration()
{
	CBeam* pBeams[] = {m_pAttackBeam3, m_pAttackBeam2, m_pAttackBeam1};
	for (CBeam* pBeam : pBeams)
		if (pBeam)
			UTIL_Remove(pBeam);
	m_pAttackBeam3 = nullptr;
	m_pAttackBeam2 = nullptr;
	m_pAttackBeam1 = nullptr;
	if (m_pSprite)
	{
		UTIL_Remove(m_pSprite);
		m_pSprite = nullptr;
	}
}

void CDummyBot::FloatThink()
{
	// One-time speed boost after settling in.
	if (m_flSlowDown != 0 && m_flSlowDown < gpGlobals->time)
	{
		m_fSpeed *= 1.3;
		m_flSlowDown = 0;
	}

	StudioFrameAdvance();

	if (!m_pGoalEnt)
	{
		m_pGoalEnt = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));
	}

	CBaseEntity* pWaypoint = m_pGoalEnt;
	if (pWaypoint)
	{
		m_vGoalPos = pWaypoint->pev->origin;
		pev->velocity = (m_vGoalPos - pev->origin).Normalize() * m_fSpeed;
	}

	// Close enough to the current waypoint - advance to the next one in
	// the chain (same "target" chaining idiom as path_corner).
	if (pWaypoint && (pev->origin - m_vGoalPos).Length() < 40.0)
	{
		CBaseEntity* pNext = pWaypoint->GetNextTarget();
		if (pNext)
		{
			m_pGoalEnt = pNext;
			m_vGoalPos = pNext->pev->origin;
			pev->velocity = (m_vGoalPos - pev->origin).Normalize() * m_fSpeed;
		}
	}

	// Retail replays the distortion-ring event on every 0.1 s think, unless
	// the mapper disabled it. The proprietary client handler remains absent,
	// but the server-side event cadence is retained.
	if (m_flNextRingTime < gpGlobals->time && !FBitSet(pev->spawnflags, 1024))
	{
		PLAYBACK_EVENT_FULL(0, edict(), m_usDistortRing, 0, (float*)&pev->origin, (float*)&pev->angles, 0, 0, 0, 0, 0, 0);
		m_flNextRingTime = gpGlobals->time + 0.1;
	}

	// The blue centre sprite is attached to model attachment 0. Use the
	// attachment-keyed light path so its small glow stays on the batterybot
	// instead of colouring unrelated nearby models like a world dlight.
	//
	// BUG FIX (2026-09-05, live gameplay report): "leuchten manchmal nur
	// auf ... hier fehlt das regelmäßige Update, damit das Licht bestehen
	// bleibt". The ELIGHT's encoded life (0.35s, truncated to 3 tenths =
	// 0.3s by UTIL_AttachedDynamicLight's tenths-of-a-second encoding)
	// left only a ~0.05s margin over the 0.25s refresh interval - and
	// since this check only runs when FloatThink itself ticks (every
	// 0.1s), the actual gap between refreshes can drift up to ~0.35s in
	// the worst case, occasionally exceeding the light's real lifetime
	// and letting it expire and flash back on instead of staying lit.
	// Widened the margin (0.5s life vs the same 0.25s refresh - always at
	// least ~0.15s of overlap even with think-interval drift) and
	// shrunk the radius per the second part of the report ("den
	// Lichtschein weiter begrenzen nur im Bereich der Sprite") to match
	// the small 0.2-scale center sprite more tightly.
	if (m_flNextLightTime < gpGlobals->time)
	{
		Vector vecLightOrigin, vecLightAngles;
		GetAttachment(0, vecLightOrigin, vecLightAngles);
			// This is deliberately model-bound and tiny: it is a blue indicator
			// on the bot, not area lighting for every nearby model.
			UTIL_AttachedDynamicLight(this, 0, vecLightOrigin, 2.0f, 80, 120, 255, 0.5f);
		m_flNextLightTime = gpGlobals->time + 0.25f;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

void CDummyBot::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	// Same base TraceAttack, with an extra damage-type bit forced on
	// (decompiled: FUN_1000d820(this, ..., flags | 2)) - matches
	// DMG_CRUSH being added regardless of the actual hit, so every hit
	// registers as guaranteed damage for training purposes.
	CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType | DMG_CRUSH);
}

void CDummyBot::Killed(entvars_t* pevAttacker, int iGib)
{
	// Retail resolves and fires this fixed death relay before selecting the
	// explosion/fall branch. It deliberately replaces the waypoint target.
	CBaseEntity* pDeathRelay = UTIL_FindEntityByTargetname(NULL, "bradybunch");
	if (pDeathRelay)
		pDeathRelay->Use(this, this, USE_ON, 0);

	RemoveDecoration();
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "drone/drone_idle.wav", 0, 0, SND_STOP, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE,
		RANDOM_LONG(0, 1) == 0 ? "drone/drone_flinch1.wav" : "drone/drone_flinch2.wav",
		0.65, 0.5, 0, 100);

	// Retail only takes the immediate-explosion path for a >=30 point
	// overkill, and does so with probability 6/7 (RANDOM_LONG(0, 6) != 0).
	if (pev->health <= -30.0 && RANDOM_LONG(0, 6) != 0)
	{
		Explode();
		CBaseMonster::Killed(pevAttacker, GIB_ALWAYS);
		return;
	}

	// Otherwise Retail keeps the bot alive as a bouncing, smoking wreck.
	pev->health = 1.0;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->velocity = Vector(RANDOM_FLOAT(-150.0, 150.0), RANDOM_FLOAT(-150.0, 150.0), RANDOM_FLOAT(-150.0, 150.0));
	SetThink(&CDummyBot::SmokeFallThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDummyBot::SmokeFallThink()
{
	SetTouch(&CDummyBot::BotExplodeTouch);

	// Retail's custom message has exactly the stock sprite-spray payload:
	// one dmlsmoke particle, rising along +Z, speed 100, noise 20.
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITE_SPRAY);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_COORD(1);
	WRITE_SHORT(m_iSpriteSmoke);
	WRITE_BYTE(1);
	WRITE_BYTE(100);
	WRITE_BYTE(20);
	MESSAGE_END();

	pev->nextthink = gpGlobals->time + 0.1;
}

void CDummyBot::BotExplodeTouch(CBaseEntity* pOther)
{
	Explode();

	// Retail resolves the owner (falling back to the world entity) and calls
	// Vtable slot 9 with our entvars. In the SDK this is DeathNotice(), which
	// lets a monstermaker or other controller account for the destroyed bot.
	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner ? pev->owner : UTIL_GetEntityList());
	if (pOwner)
		pOwner->DeathNotice(pev);

	UTIL_Remove(this);
}

void CDummyBot::Explode()
{
	RemoveDecoration();
	// Retail FUN_100d03d0 first fires two short sphere_explosion instances.
	// Their proprietary client payload cannot be reproduced byte-for-byte,
	// but CSphereExplosion supplies the compatible stock ring/tracer path.
	FireTrainingBotSphereExplosions(this);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(m_iExplosion);
	WRITE_BYTE(12); // Retail scale * 10
	WRITE_BYTE(15); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();
	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects). Der getrennte
	// Helfer bleibt eine bewusste Gameplay-Feel-Ergaenzung; der frische
	// Retail-Abgleich korrigierte den direkten Feuerball oben auf Flag 0.
	UTIL_ExplosionEffects(pev->origin, 200.0f);

	// Four battgib.mdl chunks (body 0-3), matching the decompiled
	// FUN_1000ca60(this, "models/battgib.mdl", 1.0, N) calls.
	for (int i = 0; i < 4; i++)
		SpawnTrainingBotGib(this, i);
}


//=========================================================
// entity_spritegod - CSpriteGod. A generic, highly configurable particle/
// sprite spray emitter for level designers - Gunman's rough equivalent of
// a parametrizable env_shooter. Used 79 times across 27 maps.
//
// Decompiled from gunman.dll: constructor helper @0x10068010 (vtable
// 0x100f69ac, default field values), Spawn @0x100680d0, Precache
// @0x10068090, KeyValue @0x10068190, Save/Restore @0x10067fb0/0x10067fe0
// (class name literal "CSpriteGod"), and UseToggle/SpewEvent - both
// found directly named via the binary's own symbol table
// (@0x10068480/@0x100684e0). See findings/entities/god_spawner_family.md
// (Nachtrag Session 65) for the original summary; offsets/behavior
// re-verified against the actual decompiled code here (Session 101),
// which corrected one detail: the third temp-entity byte parameter is
// "spritenoise", not "spritestartstate" as the earlier summary assumed.
//
// Not a CBaseMonster (object size 0x4c/76 bytes is far too small) - a
// plain CBaseEntity/CPointEntity-style entity, no monster AI overhead.
//
// SpewEvent's Retail payload starts with 0x6e, which is the SDK's standard
// TE_SPRITE_SPRAY identifier. Origin, direction, sprite model, count, speed
// and noise therefore map directly to the normal GoldSrc message format.
//=========================================================
class CSpriteGod : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT SpewEvent();

private:
	int m_iszSpriteName = 0;
	int m_iszTargetEnt = 0;
	int m_iSpriteIndex = 0;
	int m_iSpriteNoise = 50;
	int m_iSpriteSpeed = 0;
	int m_iActive = 0; // also doubles as the "spritestartstate" initial value
	int m_iSpriteCount = 1;
	int m_iSpriteFreq = 1; // tenths of a second between spews
	float m_flSpriteX = 0;
	float m_flSpriteY = 0;
};
LINK_ENTITY_TO_CLASS(entity_spritegod, CSpriteGod);

TYPEDESCRIPTION CSpriteGod::m_SaveData[] =
	{
		DEFINE_FIELD(CSpriteGod, m_iszSpriteName, FIELD_STRING),
		DEFINE_FIELD(CSpriteGod, m_iszTargetEnt, FIELD_STRING),
		DEFINE_FIELD(CSpriteGod, m_iSpriteNoise, FIELD_INTEGER),
		DEFINE_FIELD(CSpriteGod, m_iSpriteSpeed, FIELD_INTEGER),
		DEFINE_FIELD(CSpriteGod, m_iActive, FIELD_INTEGER),
		DEFINE_FIELD(CSpriteGod, m_iSpriteCount, FIELD_INTEGER),
		DEFINE_FIELD(CSpriteGod, m_iSpriteFreq, FIELD_INTEGER),
		DEFINE_FIELD(CSpriteGod, m_flSpriteX, FIELD_FLOAT),
	};

IMPLEMENT_SAVERESTORE(CSpriteGod, CBaseEntity);

void CSpriteGod::Precache()
{
	PrecacheModel("models/null.mdl");
	if (m_iszSpriteName != 0)
		m_iSpriteIndex = PrecacheModel((char*)STRING(m_iszSpriteName));
}

void CSpriteGod::Spawn()
{
	if (m_iszSpriteName == 0)
		m_iszSpriteName = ALLOC_STRING("sprites/gibbeak.spr");

	Precache();

	SET_MODEL(ENT(pev), "models/null.mdl");
	pev->takedamage = DAMAGE_NO;
	pev->effects |= EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin); // Retail Spawn @0x100680d0

	SetThink(&CSpriteGod::SpewEvent);
	pev->nextthink = gpGlobals->time + 0.1;
}

bool CSpriteGod::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "spritename"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spritenoise"))
	{
		m_iSpriteNoise = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spritespeed"))
	{
		m_iSpriteSpeed = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spritestartstate"))
	{
		m_iActive = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spritecount"))
	{
		m_iSpriteCount = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spritefreq"))
	{
		m_iSpriteFreq = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spritex"))
	{
		m_flSpriteX = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "spritey"))
	{
		m_flSpriteY = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "targetent"))
	{
		m_iszTargetEnt = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CSpriteGod::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (m_iActive == 0)
	{
		m_iActive = 1;
		SetThink(&CSpriteGod::SpewEvent);
		pev->nextthink = gpGlobals->time + (float)(m_iSpriteFreq / 10);
	}
	else
	{
		m_iActive = 0;
		SetThink(nullptr);
	}
}

void CSpriteGod::SpewEvent()
{
	if (m_iActive == 0)
	{
		SetThink(nullptr);
		return;
	}

	Vector vecDir;
	if (m_iszTargetEnt == 0)
	{
		// Fixed target point in world space (not actually "relative to
		// self" despite the FGD description - decompiled evidence is a
		// plain point-minus-origin subtraction). spritez has no keyvalue
		// (dead/unreachable code in the original KeyValue - the third
		// coordinate check compares against "spritex" a second time
		// instead of "spritez"), so it's always treated as world Z=0.
		vecDir = (Vector(m_flSpriteX, m_flSpriteY, 0) - pev->origin).Normalize();
	}
	else
	{
		CBaseEntity* pTarget = UTIL_FindEntityByTargetname(NULL, STRING(m_iszTargetEnt));
		if (pTarget)
			vecDir = (pTarget->pev->origin - pev->origin).Normalize();
		else
			vecDir = Vector(0, 0, 1);
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITE_SPRAY);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(vecDir.x);
	WRITE_COORD(vecDir.y);
	WRITE_COORD(vecDir.z);
	WRITE_SHORT(m_iSpriteIndex);
	WRITE_BYTE(m_iSpriteCount);
	WRITE_BYTE(m_iSpriteSpeed);
	WRITE_BYTE(m_iSpriteNoise);
	MESSAGE_END();

	pev->nextthink = gpGlobals->time + (float)(m_iSpriteFreq / 10);
}


//=========================================================
// entity_volcanospew - CVolcanoSpew. A short-lived, invisible monster
// carrier used by the Mayan/Rust volcano makers. Retail emits six fixed
// lava sprite sprays, then removes the carrier on the following think.
//=========================================================
class CVolcanoSpew : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT SprayLava();

private:
	int m_iLavaSprite = 0;
	int m_iSprayCount = 0;
};
LINK_ENTITY_TO_CLASS(entity_volcanospew, CVolcanoSpew);

TYPEDESCRIPTION CVolcanoSpew::m_SaveData[] =
{
	DEFINE_FIELD(CVolcanoSpew, m_iLavaSprite, FIELD_INTEGER),
	DEFINE_FIELD(CVolcanoSpew, m_iSprayCount, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CVolcanoSpew, CBaseMonster);

void CVolcanoSpew::Precache()
{
	PrecacheModel("models/null.mdl");
	m_iLavaSprite = PrecacheModel("sprites/lavaslop.spr");
}

void CVolcanoSpew::Spawn()
{
	Precache();

	pev->takedamage = DAMAGE_NO;
	SET_MODEL(ENT(pev), "models/null.mdl");
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetTouch(nullptr);
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink(&CVolcanoSpew::SprayLava);
	m_iSprayCount = 0;
	ResetSequenceInfo(); // retail @0x10068790 (not MonsterInit, which would replace the SprayLava think)
}

void CVolcanoSpew::SprayLava()
{
	++m_iSprayCount;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITE_SPRAY);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_COORD(1);
	WRITE_SHORT(m_iLavaSprite);
	WRITE_BYTE(10);
	WRITE_BYTE(100);
	WRITE_BYTE(150);
	MESSAGE_END();

	if (m_iSprayCount > 5)
		SetThink(&CBaseEntity::SUB_Remove);

	pev->nextthink = gpGlobals->time + 0.1;
}


//=========================================================
// player_speaker - CDirectSentence. A trigger-fired narration/voice-over
// speaker: on Use(), plays a sentence directly at the local player
// (not positional audio from the entity's own origin). Removes itself
// immediately in multiplayer, since these are single-player narrative
// beats.
//
// Decompiled from gunman.dll: Spawn @0x10066fc0, KeyValue @0x10066f00,
// Save/Restore @0x10066ea0/0x10066ed0 (class name literal
// "CDirectSentence"), and PlaySound - found directly named via the
// binary's own symbol table (@0x10067030).
//=========================================================
class CDirectSentence : public CBaseEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_iszSentenceGroup = 0;
	float m_flSentenceLength = 0;
};
LINK_ENTITY_TO_CLASS(player_speaker, CDirectSentence);

TYPEDESCRIPTION CDirectSentence::m_SaveData[] =
	{
		DEFINE_FIELD(CDirectSentence, m_iszSentenceGroup, FIELD_STRING),
		DEFINE_FIELD(CDirectSentence, m_flSentenceLength, FIELD_FLOAT),
	};

IMPLEMENT_SAVERESTORE(CDirectSentence, CBaseEntity);

void CDirectSentence::Spawn()
{
	// Single-player only - remove immediately in multiplayer.
	if (g_pGameRules && g_pGameRules->IsMultiplayer())
	{
		SetThink(&CDirectSentence::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	pev->classname = MAKE_STRING("player_speaker");
	pev->solid = SOLID_NOT;
}

bool CDirectSentence::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "sentencegroup"))
	{
		m_iszSentenceGroup = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "sentencelength"))
	{
		m_flSentenceLength = atof(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CDirectSentence::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Plays at the local player's position, not this entity's - matches
	// the decompiled UTIL_PlayerByIndex(1)-with-UTIL_GetLocalPlayer()
	// fallback lookup rather than emitting from ENT(pev).
	CBaseEntity* pPlayer = UTIL_PlayerByIndex(1);
	if (!pPlayer)
		pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer)
		return;

	EMIT_SOUND_DYN(pPlayer->edict(), CHAN_STATIC, STRING(m_iszSentenceGroup), 1.0, 0.8, 0, 100);
}


//=========================================================
// player_togglehud - sixth and last CITY1A gap. A `base(Targetname)`
// marker entity with no FGD keyvalues.
//
// Decompiled from gunman.dll: Spawn @0x10052db0 is the ONLY class-owned
// vtable slot in this entity's vtable (@0x100f24b4) - every other slot
// (Precache, KeyValue, Save, Restore, ObjectCaps, Think, Touch, Use)
// resolves to the generic shared CBaseEntity default-stub addresses, so
// none of them are overridden. Spawn itself does exactly one thing:
// `pev->solid = SOLID_NOT`. No custom fields (object size is 0x20/32
// bytes - just the vtable pointer and pev pointer, nothing else), no
// Use handler, no Think, no Touch.
//
// Nachtrag 2026-09-06 (Live-Report "in city2a sollte das HUD
// automatisch aktiviert werden"): re-verifiziert mit einem frischen
// Vtable-Dump (20 Slots, 0x100f24b4) statt der urspruenglichen Notiz
// blind zu vertrauen (siehe die CMiniSentry-Lektion: "immer eine
// Sibling-Class-'no override'-Behauptung mit einem frischen Vtable-
// Readout gegenpruefen"). Bestaetigt: alle 19 Nicht-Spawn-Slots
// loesen zu generischen, klassen-unspezifischen Trampolin-Funktionen
// auf (u.a. eine generische Physik-/Impuls-Hilfsfunktion,
// klar nicht HUD-bezogen) - die urspruengliche Einschaetzung war
// korrekt, player_togglehud hat wirklich keine eigene Logik.
//
// Trotzdem: CITY1A UND CITY2A (sowie mehrere Cinematics/MAYAN8/
// RUST4C/RUST5A/west6e/end2) platzieren jeweils einen trigger_auto,
// der genau dieses Entity per targetname automatisch(!) kurz nach
// Level-Start feuert (siehe CITY2A_entities.md #398/#399,
// targetname "turn_hud_on", delay=1) - ein wiederkehrendes,
// eindeutig beabsichtigtes Muster, das nicht folgenlos sein kann.
// CBasePlayer::Spawn() (player.cpp) setzt m_iHideHUD unbedingt auf
// (HIDEHUD_HEALTH|HIDEHUD_WEAPONS) - unveraendert aus dem Stock-SDK
// uebernommen, nie gegen die echte Gunman-Retail-Logik verifiziert -
// und nichts im Projekt loescht dieses Flag je wieder. Der reale
// Konsument (vermutlich clientseitiger Code oder eine nicht
// exportierte CBasePlayer-Methode, die nach Klassennamen sucht)
// wurde nicht gefunden; statt eines wirkungslosen No-Ops (das den
// Live-Report klar falsifiziert) wird hier der eindeutig belegte
// Verhaltens-Intent nachgebildet: Use() schaltet das HUD (Health +
// Waffen) fuer alle Spieler frei. Bewusste, dokumentierte
// Verhaltensnaeherung, kein Byte-Decompile-Fund.
//=========================================================
class CPlayerToggleHud : public CBaseEntity
{
public:
	void Spawn() override { pev->solid = SOLID_NOT; }
	// Retail Use (slot 46 @0x10043d40, re-decompiled 2026-10-31; the
	// "no own logic" note above only looked at 20 slots): target = activator
	// if player, else the local player. If HIDEHUD_HEALTH is set, clear
	// HIDEHUD_HEALTH|HIDEHUD_WEAPONS, otherwise set both (toggle).
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override
	{
		CBasePlayer* pPlayer = nullptr;
		if (pActivator && pActivator->IsPlayer())
			pPlayer = static_cast<CBasePlayer*>(pActivator);
		else if (!g_pGameRules->IsMultiplayer())
			pPlayer = static_cast<CBasePlayer*>(UTIL_GetLocalPlayer());

		if (!pPlayer)
			return;

		if ((pPlayer->m_iHideHUD & HIDEHUD_HEALTH) != 0)
			pPlayer->m_iHideHUD &= ~(HIDEHUD_HEALTH | HIDEHUD_WEAPONS);
		else
			pPlayer->m_iHideHUD |= (HIDEHUD_HEALTH | HIDEHUD_WEAPONS);
	}
};
LINK_ENTITY_TO_CLASS(player_togglehud, CPlayerToggleHud);


//=========================================================
// random_speaker - CRand_Speaker. First CITY2A-only gap. A simple
// ambient/atmosphere entity that repeatedly plays one sound at random
// intervals, purely server-driven (no Use/trigger involved).
//
// Decompiled from gunman.dll: constructor helper @0x10066650 (vtable
// 0x100f6420, default field values: wait=1.0, random=0.0, volume=1.0 -
// note these differ from the FGD's documented defaults of 10/20/1, code
// wins), Spawn @0x10066830, Precache @0x10066670, KeyValue @0x10066690,
// Save/Restore @0x100665f0/0x10066620 (class name literal
// "CRand_Speaker"), and Play - found directly named via the binary's
// own symbol table (@0x100668a0).
//
// KeyValue recognizes the sound-name field twice under two different
// spellings in the original ("niose" then "rsnoise" as a fallback).
// Both aliases are retained below; "niose" is the shipped legacy typo.
//=========================================================
class CRandSpeaker : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT Play();

private:
	int m_iszSound = 0;
	float m_flWait = 1.0;
	float m_flVolume = 1.0;
	float m_flRandom = 0;
};
LINK_ENTITY_TO_CLASS(random_speaker, CRandSpeaker);

TYPEDESCRIPTION CRandSpeaker::m_SaveData[] =
	{
		DEFINE_FIELD(CRandSpeaker, m_iszSound, FIELD_STRING),
		DEFINE_FIELD(CRandSpeaker, m_flWait, FIELD_FLOAT),
		DEFINE_FIELD(CRandSpeaker, m_flVolume, FIELD_FLOAT),
		DEFINE_FIELD(CRandSpeaker, m_flRandom, FIELD_FLOAT),
	};

IMPLEMENT_SAVERESTORE(CRandSpeaker, CBaseEntity);

void CRandSpeaker::Precache()
{
	if (m_iszSound != 0)
		PrecacheSound((char*)STRING(m_iszSound));
}

void CRandSpeaker::Spawn()
{
	if (m_iszSound == 0)
	{
		UTIL_Remove(this);
		return;
	}

	Precache();

	if (m_flVolume > 1.0 || m_flVolume < 0.0)
		m_flVolume = 1.0;

	SetThink(&CRandSpeaker::Play);
	pev->nextthink = gpGlobals->time + m_flWait + RANDOM_FLOAT(0, m_flRandom);
}

bool CRandSpeaker::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "niose") || FStrEq(pkvd->szKeyName, "rsnoise"))
	{
		m_iszSound = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "volume"))
	{
		m_flVolume = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "random"))
	{
		m_flRandom = atof(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CRandSpeaker::Play()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, STRING(m_iszSound), m_flVolume, 0.8, 0, RANDOM_LONG(-3, 3) + 100);
	pev->nextthink = gpGlobals->time + m_flWait + RANDOM_FLOAT(0, m_flRandom);
}


//=========================================================
// decore_spacedebris - CSpaceDebris. First CITY2B-only gap. Hidden and
// inert until triggered; on Use() it becomes visible and flies toward
// its "target" entity (tumbling, near-zero gravity), then removes
// itself once it stops moving or its lifetime runs out. Debris pieces
// pass through each other (no push/collision between two of them).
//
// Decompiled from gunman.dll: Spawn @0x100ce8e0, Precache @0x100ce9d0,
// KeyValue @0x100cea00, and UseToggle/DebrisRemoveThink/DebrisTouch -
// all three found directly named via the binary's own symbol table
// (@0x100ceb50/@0x100cede0/@0x100cee90). Corrects
// findings/entities/decore_family.md's note that only the field offsets
// were unconfirmed - they're pinned down here.
//=========================================================
class CSpaceDebris : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT DebrisRemoveThink();
	void EXPORT DebrisTouch(CBaseEntity* pOther);

private:
	int m_iszModel = 0;
	float m_flForwardSpeed = 0;
	float m_flAngleSpeed = 0;
	float m_flLife = 0;
	Vector m_vecTarget = Vector(0, 0, 0);
	// Retail deliberately does not save this movement sample. A restored
	// object starts it at the allocator-zeroed origin and samples again.
	Vector m_vecLastOrigin = Vector(0, 0, 0);
};
LINK_ENTITY_TO_CLASS(decore_spacedebris, CSpaceDebris);

// Retail table @0x1012edc0: model name and the movement sample are not
// persisted; the movement parameters, destination and remaining lifetime are.
TYPEDESCRIPTION CSpaceDebris::m_SaveData[] =
{
	DEFINE_FIELD(CSpaceDebris, m_flForwardSpeed, FIELD_FLOAT),
	DEFINE_FIELD(CSpaceDebris, m_flAngleSpeed, FIELD_FLOAT),
	DEFINE_FIELD(CSpaceDebris, m_vecTarget, FIELD_VECTOR),
	DEFINE_FIELD(CSpaceDebris, m_flLife, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CSpaceDebris, CBaseEntity);

void CSpaceDebris::Precache()
{
	if (m_iszModel != 0)
		PrecacheModel((char*)STRING(m_iszModel));
}

void CSpaceDebris::Spawn()
{
	pev->classname = MAKE_STRING("decore_spacedebris");
	Precache();
	SET_MODEL(ENT(pev), STRING(m_iszModel));

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects = EF_NODRAW;
	pev->health = 60;
	pev->gravity = 0.001;
	pev->velocity = g_vecZero;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}

bool CSpaceDebris::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "modelname"))
	{
		m_iszModel = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "forwardspeed"))
	{
		m_flForwardSpeed = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "anglespeed"))
	{
		m_flAngleSpeed = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "debrislife"))
	{
		m_flLife = atof(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CSpaceDebris::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!FStringNull(pev->target))
	{
		CBaseEntity* pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));
		if (pTarget)
			m_vecTarget = pTarget->pev->origin;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->effects = 0;
	pev->gravity = 0.001;

	// With a missing/unresolved target Retail retains its initialized target
	// vector (world origin); it does not synthesize an upward destination.
	Vector vecDir = m_vecTarget - pev->origin;
	if (vecDir.Length() == 0)
		vecDir = Vector(0, 0, 1);
	else
		vecDir = vecDir.Normalize();
	pev->velocity = vecDir * m_flForwardSpeed;

	pev->avelocity.x = RANDOM_FLOAT(-m_flAngleSpeed, m_flAngleSpeed);
	pev->avelocity.y = RANDOM_FLOAT(-m_flAngleSpeed, m_flAngleSpeed);
	pev->avelocity.z = RANDOM_FLOAT(-m_flAngleSpeed, m_flAngleSpeed);

	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetThink(&CSpaceDebris::DebrisRemoveThink);
	pev->nextthink = gpGlobals->time + 0.5;
	SetTouch(&CSpaceDebris::DebrisTouch);
	UTIL_SetOrigin(pev, pev->origin);
	m_vecLastOrigin = pev->origin;
}

void CSpaceDebris::DebrisRemoveThink()
{
	pev->solid = SOLID_NOT;
	m_flLife -= 0.1;

	// Stopped moving (landed/stuck) - remove it.
	if (m_vecLastOrigin == pev->origin)
	{
		UTIL_Remove(this);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	m_vecLastOrigin = pev->origin;

	if (m_flLife <= 0)
		UTIL_Remove(this);
}

void CSpaceDebris::DebrisTouch(CBaseEntity* pOther)
{
	// Debris pieces pass through each other instead of colliding.
	if (FClassnameIs(pOther->pev, STRING(pev->classname)))
		pev->solid = SOLID_NOT;
}


//=========================================================
// sphere_explosion - CSphereExplosion. Second CITY2B-only gap. A
// trigger-fired, highly configurable explosion effect entity (no
// physical damage - purely visual/screen effects: burst, optional ring
// shockwave, optional screen shake, optional screen fade). Also the
// class monster_trainingbot's death explosion uses internally
// (previously approximated with a plain CSprite glow there, since this
// class didn't exist in our SDK yet - see gunman_custom_entities.cpp's
// CDummyBot::Explode()).
//
// Decompiled from gunman.dll: Spawn @0x100678b0, Precache @0x10067870,
// KeyValue @0x10067a40, and UseToggle/ExplodeEvent - both found
// directly named via the binary's own symbol table
// (@0x10067950/@0x10067d30).
//
// Approximated rather than byte-reproduced: ExplodeEvent fires a
// PLAYBACK_EVENT for a custom client-side event script
// ("events/sphereexplode.sc") for the main burst, and the same
// undocumented Gunman temp-entity message ID (0x17, seen elsewhere in
// this file) for the optional ring - our client.dll has no handler for
// either. Substituted with a real TE_EXPLOSION burst (using
// "sprites/clustershock.spr", the same sprite the original precaches)
// plus, when shockwave is enabled, a second TE_EXPLOSION-style flash
// standing in for the ring. Screen shake (when shakeduration is set) is
// NOT approximated - it uses UTIL_ScreenShake(origin, 25, 150,
// shakeduration, 3600), a field-for-field match to the decompiled
// FUN_1005eb80(origin, 25.0, 150.0, shakeduration, 3600.0) call, so
// that one is fully faithful. Screen fade (fadetime) uses
// UTIL_ScreenFadeAll with a white fade as a reasonable stand-in for the
// unresolved FUN_1005ee40() call (decompiled with no visible arguments
// - likely reads additional internal state not otherwise recovered).
//=========================================================
// Nachtrag 2026-09-06 (Live-Report "runder Stein fliegt in die
// Fensterscheibe, Ausgangspunkt ist sphere_explosion"): "numtracers"
// wird im Original ausschliesslich als Partikel-An/Aus-Flag fuer die
// TE_EXPLOSION-Ersatznachricht verwendet (siehe TE_EXPLFLAG_NOPARTICLES
// unten) - der eigentliche "fliegende Stein"-Effekt kommt laut
// Decompile (ExplodeEvent, PLAYBACK_EVENT_FULL fuer
// "events/sphereexplode.sc") aus einem rein client-seitigen Event-
// Skript, das dieses Projekt nicht besitzt (kein client.dll-Handler
// dafuer vorhanden, siehe Klassenkommentar oben). Statt eines stillen
// No-Ops wird hier - als dokumentierte, bewusste Vereinfachung -
// serverseitig eine echte, physisch fliegende Trümmer-Entity pro
// "numtracers" gespawnt (models/asteroid.mdl, passend zum gemeldeten
// "runder Stein"; dasselbe Modell wie das bereits real RE-belegte
// decore_spacedebris an anderer Stelle in city2b), radial nach aussen
// mit Schwerkraft/Rotation - kein Versuch, das exakte, unbekannte
// Skript-Verhalten (Zielpunkt, Timing) zu rekonstruieren.
class CSphereDebrisChunk : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT ChunkRemoveThink();

private:
	float m_flLife = 3.0f;
};
LINK_ENTITY_TO_CLASS(sphere_explosion_chunk, CSphereDebrisChunk);

void CSphereDebrisChunk::Spawn()
{
	PrecacheModel("models/asteroid.mdl");
	SET_MODEL(ENT(pev), "models/asteroid.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_NO;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	Vector vecDir = Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-0.3f, 0.6f)).Normalize();
	pev->velocity = vecDir * RANDOM_FLOAT(150, 350);
	pev->avelocity = Vector(RANDOM_FLOAT(-200, 200), RANDOM_FLOAT(-200, 200), RANDOM_FLOAT(-200, 200));

	SetThink(&CSphereDebrisChunk::ChunkRemoveThink);
	pev->nextthink = gpGlobals->time + m_flLife;
}

void CSphereDebrisChunk::ChunkRemoveThink()
{
	UTIL_Remove(this);
}

class CSphereExplosion : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT ExplodeEvent();

	// Nachtrag 2026-09-06 (CDecoreExplode::ExplodeUse Vollrekonstruktion):
	// echtes Retail-Verhalten (0x1007a180, per Rohdisassemblierung
	// entschluesselt - Ghidras Decompiler scheiterte an dieser
	// Kalling-Convention) erzeugt zwei sphere_explosion-Unterobjekte
	// direkt mit fest zugewiesenen Feldwerten statt ueber Map-KeyValues -
	// dieser Helfer bildet genau das nach.
	void ConfigureAndFire(const Vector& origin, float maxRadius, float radiusStep, int trans, int transStep,
		int numTracers, int shockwave, int implosion, float shakeDuration, float fadeTime, float fadeHoldTime)
	{
		pev->origin = origin;
		m_flMaxRadius = maxRadius;
		m_flRadiusStep = radiusStep;
		m_iTrans = trans;
		m_iTransStep = transStep;
		m_iNumTracers = numTracers;
		m_iShockwave = shockwave;
		m_iImplosion = implosion;
		m_flShakeDuration = shakeDuration;
		m_flFadeTime = fadeTime;
		m_flFadeHoldTime = fadeHoldTime;
		Precache();
		ExplodeEvent();
	}

	float m_flMaxRadius = 200;

private:
	float m_flRadiusStep = 4;
	int m_iTrans = 255;
	int m_iTransStep = 5;
	int m_iNumTracers = 1;
	int m_iShockwave = 1;
	int m_iImplosion = 0;
	float m_flShakeDuration = 0;
	float m_flFadeTime = 0;
	float m_flFadeHoldTime = 0.1;
	int m_iSpriteRing = 0;
};
LINK_ENTITY_TO_CLASS(sphere_explosion, CSphereExplosion);

TYPEDESCRIPTION CSphereExplosion::m_SaveData[] =
{
	DEFINE_FIELD(CSphereExplosion, m_flMaxRadius, FIELD_FLOAT),
	DEFINE_FIELD(CSphereExplosion, m_flRadiusStep, FIELD_FLOAT),
	DEFINE_FIELD(CSphereExplosion, m_iTrans, FIELD_INTEGER),
	DEFINE_FIELD(CSphereExplosion, m_iTransStep, FIELD_INTEGER),
	DEFINE_FIELD(CSphereExplosion, m_iNumTracers, FIELD_INTEGER),
	DEFINE_FIELD(CSphereExplosion, m_iShockwave, FIELD_INTEGER),
	DEFINE_FIELD(CSphereExplosion, m_iImplosion, FIELD_INTEGER),
	DEFINE_FIELD(CSphereExplosion, m_flShakeDuration, FIELD_FLOAT),
	DEFINE_FIELD(CSphereExplosion, m_flFadeTime, FIELD_FLOAT),
	DEFINE_FIELD(CSphereExplosion, m_flFadeHoldTime, FIELD_FLOAT),
	DEFINE_FIELD(CSphereExplosion, m_iSpriteRing, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CSphereExplosion, CBaseEntity);

void CSphereExplosion::Precache()
{
	PrecacheModel("models/null.mdl");
	m_iSpriteRing = PrecacheModel("sprites/clustershock.spr");
	UTIL_PrecacheOther("sphere_explosion_chunk");
}

void CSphereExplosion::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/null.mdl");
	pev->takedamage = DAMAGE_NO;
	pev->effects |= EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}

bool CSphereExplosion::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "maxradius"))
	{
		m_flMaxRadius = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "radiusstep"))
	{
		m_flRadiusStep = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "trans"))
	{
		m_iTrans = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "transstep"))
	{
		m_iTransStep = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "numtracers"))
	{
		m_iNumTracers = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "shockwave"))
	{
		m_iShockwave = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "implosion"))
	{
		m_iImplosion = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "shakeduration"))
	{
		m_flShakeDuration = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "fadetime"))
	{
		m_flFadeTime = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "fadeholdtime"))
	{
		m_flFadeHoldTime = atof(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CSphereExplosion::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	SetThink(&CSphereExplosion::ExplodeEvent);
	pev->nextthink = gpGlobals->time;
}

void CSphereExplosion::ExplodeEvent()
{
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(m_iSpriteRing);
	WRITE_BYTE((byte)(m_flMaxRadius / 10));
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND | (m_iNumTracers != 0 ? 0 : TE_EXPLFLAG_NOPARTICLES));
	MESSAGE_END();

	if (m_iShockwave != 0)
	{
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_BEAMCYLINDER);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 24.0f);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 24.0f + m_flMaxRadius * 1.5f);
		WRITE_SHORT(m_iSpriteRing);
		WRITE_BYTE(0); // start frame
		WRITE_BYTE(0); // frame rate
		// Retail: ROUND((maxradius / (radiusstep * 8)) * 10).
		WRITE_BYTE((int)(m_flMaxRadius * 10.0f / (m_flRadiusStep * 8.0f) + 0.5f));
		WRITE_BYTE(16);
		WRITE_BYTE(0);
		WRITE_BYTE(255);
		WRITE_BYTE(255);
		WRITE_BYTE(255);
		WRITE_BYTE(255);
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	// Retail's FGD defines numtracers as a boolean for exactly 15 visual
	// tracers. The prior model chunks were a non-Retail physics extension.
	if (m_iNumTracers != 0)
	{
		for (int i = 0; i < 15; ++i)
		{
			const Vector vecEnd = pev->origin + Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1)).Normalize() * RANDOM_FLOAT(100.0f, m_flMaxRadius);
			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
			WRITE_BYTE(TE_TRACER);
			WRITE_COORD(pev->origin.x);
			WRITE_COORD(pev->origin.y);
			WRITE_COORD(pev->origin.z);
			WRITE_COORD(vecEnd.x);
			WRITE_COORD(vecEnd.y);
			WRITE_COORD(vecEnd.z);
			MESSAGE_END();
		}
	}

	if (m_flShakeDuration != 0)
		UTIL_ScreenShake(pev->origin, 25.0, 150.0, m_flShakeDuration, 3600.0);

	if (m_flFadeTime != 0)
		UTIL_ScreenFadeAll(Vector(0, 0, 0), m_flFadeTime, m_flFadeHoldTime, 150, FFADE_IN);

	UTIL_Remove(this);
}

void GunmanFireSphereExplosion(const Vector& origin, float maxRadius, float radiusStep,
	int trans, int transStep, int numTracers, int shockwave, int implosion,
	float shakeDuration, float fadeTime, float fadeHoldTime)
{
	CSphereExplosion* pExplosion = GetClassPtr((CSphereExplosion*)NULL);
	pExplosion->ConfigureAndFire(origin, maxRadius, radiusStep, trans, transStep,
		numTracers, shockwave, implosion, shakeDuration, fadeTime, fadeHoldTime);
}

// Retail CDummyBot::Explode helper FUN_100d03d0 creates these two effects
// directly, then sends its small fireball and spawns battgib bodies 0..3.
// Constructor defaults retained by the Retail helper are implosion=0,
// shake=0, fade=0 and fade-hold=0.1.
static void FireTrainingBotSphereExplosions(CBaseEntity* pBot)
{
	CSphereExplosion* pOuter = GetClassPtr((CSphereExplosion*)NULL);
	pOuter->ConfigureAndFire(pBot->pev->origin, 100.0f, 2.0f, 255, 7, 1, 1, 0, 0.0f, 0.0f, 0.1f);

	CSphereExplosion* pInner = GetClassPtr((CSphereExplosion*)NULL);
	pInner->ConfigureAndFire(pBot->pev->origin, 25.0f, 0.5f, 220, 3, 1, 1, 0, 0.0f, 0.0f, 0.1f);
}

// Retail's generic FUN_1000ca60 gib helper. CDummyBot calls it four times
// with battgib bodies 0..3. Five percent of gibs are thrown at the closest
// client in the PVS; the remaining path uses the fixed random metal-gib arc.
static void SpawnTrainingBotGib(CBaseMonster* pBot, int body)
{
	CGib* pGib = GetClassPtr((CGib*)NULL);
	pGib->Spawn("models/battgib.mdl");
	pGib->pev->body = body;
	pGib->pev->scale = 1.0f;
	pGib->pev->owner = pBot->pev->owner;
	pGib->m_bloodColor = pBot->BloodColor();

	const Vector vecOrigin = pBot->pev->origin + pBot->pev->view_ofs +
		Vector(RANDOM_FLOAT(-4.0f, 4.0f), RANDOM_FLOAT(-4.0f, 4.0f), RANDOM_FLOAT(-4.0f, 4.0f));
	UTIL_SetOrigin(pGib->pev, vecOrigin);

	edict_t* pPlayer = FIND_CLIENT_IN_PVS(pGib->edict());
	if (RANDOM_LONG(0, 99) < 5 && pPlayer)
	{
		entvars_t* pevPlayer = VARS(pPlayer);
		pGib->pev->velocity = ((pevPlayer->origin + pevPlayer->view_ofs) - pGib->pev->origin).Normalize() * 300.0f;
		pGib->pev->velocity.z += 100.0f;
	}
	else
	{
		pGib->pev->velocity = Vector(RANDOM_FLOAT(-100.0f, 100.0f), RANDOM_FLOAT(-100.0f, 100.0f), RANDOM_FLOAT(200.0f, 300.0f));
	}
	pGib->pev->avelocity.x = RANDOM_FLOAT(100.0f, 200.0f);
	pGib->pev->avelocity.y = RANDOM_FLOAT(100.0f, 300.0f);

	if (pBot->pev->health <= -200.0f)
		pGib->pev->velocity = pGib->pev->velocity * 4.0f;
	else if (pBot->pev->health <= -50.0f)
		pGib->pev->velocity = pGib->pev->velocity * 2.0f;

	pGib->LimitVelocity();
}


//=========================================================
// entity_digitgod / entity_digit - CDigitGod and its internal digit-card
// sub-entity. First CITY3A-only gap. A 3-digit HUD-style counter: on
// Spawn(), CDigitGod creates three entity_digit instances (hundreds/
// tens/ones) side by side and drives their displayed digit via
// pev->skin (0-9). SetDigitValue() is called externally from a linked
// hologram_beak/hologram_damage training entity to update the running value;
// once it reaches
// maxdamage it fires its own targets once.
//
// Decompiled from gunman.dll: constructor @0x100d0cc0, Precache
// @0x100d0ec0, KeyValue @0x100d0ee0, SetDigitValue @0x100d0f80,
// OutputThink - found directly named via the binary's own symbol table
// (@0x100d0f60) - plus the entity-creation helper @0x100088a0 (matches
// CBaseEntity::Create()) and entity_digit's own constructor @0x100d0b40.
// Corrects findings/entities/god_spawner_family.md's guess that the
// digit is selected via pev->sequence - it's actually pev->skin.
//
// Live-Bug-Fix (2026-09-06, siehe STATUS.md): the digit-card spacing was
// approximated as 10 units and the hundreds/ones cards were swapped
// left-right. Fresh decompile of FUN_100d0cc0 found the real constants:
// hundreds at +16*v_right, ones at -16*v_right. 16 units matches
// models/digits.mdl's card width exactly (-8..+8), so the guessed 10
// left adjacent cards overlapping - the "digits squished together" the
// user reported.
//=========================================================
class CEntityDigit : public CBaseEntity
{
public:
	void Spawn() override;
};
LINK_ENTITY_TO_CLASS(entity_digit, CEntityDigit);

void CEntityDigit::Spawn()
{
	pev->classname = MAKE_STRING("entity_digit");
	PrecacheModel("models/digits.mdl");
	SET_MODEL(ENT(pev), "models/digits.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = 60;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}

class CDigitGod : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT OutputThink();
	void SetDigitValue(int value);
	bool HasSucceeded() const { return m_iFired != 0; }

private:
	CBaseEntity* m_pHundreds = nullptr;
	CBaseEntity* m_pTens = nullptr;
	CBaseEntity* m_pOnes = nullptr;
	int m_iCurrentValue = 0;
	int m_iMaxDamage = 0;
	int m_iFired = 0;
};
LINK_ENTITY_TO_CLASS(entity_digitgod, CDigitGod);

// Retail table @0x1012f4c8: three integer state fields followed by the
// three generated digit entities. Their edicts must survive a save/load as
// a coherent counter, rather than being raw, reset pointers.
TYPEDESCRIPTION CDigitGod::m_SaveData[] =
{
	DEFINE_FIELD(CDigitGod, m_iCurrentValue, FIELD_INTEGER),
	DEFINE_FIELD(CDigitGod, m_iMaxDamage, FIELD_INTEGER),
	DEFINE_FIELD(CDigitGod, m_iFired, FIELD_INTEGER),
	DEFINE_FIELD(CDigitGod, m_pHundreds, FIELD_CLASSPTR),
	DEFINE_FIELD(CDigitGod, m_pTens, FIELD_CLASSPTR),
	DEFINE_FIELD(CDigitGod, m_pOnes, FIELD_CLASSPTR),
};

IMPLEMENT_SAVERESTORE(CDigitGod, CBaseEntity);

void CDigitGod::Precache()
{
	PrecacheModel("models/null.mdl");
	UTIL_PrecacheOther("entity_digit");
}

void CDigitGod::Spawn()
{
	pev->classname = MAKE_STRING("entity_digitgod");
	Precache();
	SET_MODEL(ENT(pev), "models/null.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects = EF_NODRAW;
	pev->health = 60;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	// Kartenabstand/-reihenfolge RE-bestaetigt (FUN_100d0cc0): hundreds bei
	// +16*v_right, ones bei -16*v_right - 16 Einheiten entspricht exakt der
	// Kartenbreite von models/digits.mdl (-8..+8), nicht 10 wie zuvor
	// angenaehert (fuehrte zu ueberlappenden/"ineinandergeschobenen" Ziffern).
	UTIL_MakeVectors(pev->angles);
	m_pHundreds = CBaseEntity::Create("entity_digit", pev->origin + gpGlobals->v_right * 16, pev->angles, edict());
	m_pTens = CBaseEntity::Create("entity_digit", pev->origin, pev->angles, edict());
	m_pOnes = CBaseEntity::Create("entity_digit", pev->origin - gpGlobals->v_right * 16, pev->angles, edict());

	SetThink(&CDigitGod::OutputThink);
	pev->nextthink = gpGlobals->time;

	SetDigitValue(0);
}

bool CDigitGod::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "maxdamage"))
	{
		m_iMaxDamage = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CDigitGod::OutputThink()
{
	pev->nextthink = gpGlobals->time + 0.5;
}

void CDigitGod::SetDigitValue(int value)
{
	if (!m_pHundreds || !m_pTens || !m_pOnes)
		return;

	m_iCurrentValue = value;
	if (m_iCurrentValue > 999)
		m_iCurrentValue = 999;
	value = m_iCurrentValue;

	if (value < 10)
	{
		m_pHundreds->pev->skin = 0;
		m_pTens->pev->skin = 0;
		m_pOnes->pev->skin = value;
		return;
	}

	m_pHundreds->pev->skin = value / 100;
	m_pTens->pev->skin = (value / 10) % 10;
	m_pOnes->pev->skin = value % 10;

	if (value >= m_iMaxDamage && !m_iFired)
	{
		SUB_UseTargets(this, USE_ON, 0);
		m_iFired = 1;
	}
}


//=========================================================
// hologram_beak / hologram_damage - CHologram and CHologramDamage. A
// weapon-training-range target hologram: appears with a materialize
// effect (once the display volume is clear), shows an idle Xenome
// creature (Beak/Tube/"MicroRaptor" - which despite the FGD name loads
// models/raptor.mdl, same naming quirk as monster_microraptor), is
// hittable while displayed, then dissolves and respawns in a cycle.
// Hits accumulate into a linked entity_digitgod (via "target") as a
// running 3-digit counter; if the display window ends before the
// linked digitgod reaches its own maxdamage, hologram_beak fires its
// own "targetfail" output. hologram_damage additionally requires a
// specific weapon/damage type (gauss/chemgun sub-type) to count a hit,
// and uses health depletion (not a display timer) to end the round.
//
// Decompiled from gunman.dll (ghidra/logs/holo_combat.txt, re-verified
// this session): constructor/Spawn @0x100d1190, Precache @0x100d1520,
// KeyValue @0x100d1570, Classify @0x100d1110 (0xb = CLASS_PLAYER_ALLY),
// BloodColor @0x100d1120 (-1 = DONT_BLEED), SpawnThink/HologramThink/
// DieThink/RespawnHologram/UseToggle - all found directly named via the
// binary's own symbol table. TakeDamage @0x100d17a0 (vtable slot 11,
// position-matched against CBaseMonster's declared virtual order the
// same way as other classes this session) resolves
// findings/entities/hologram_beak.md's open question: the player
// influences the counter simply by damaging the hologram (TakeDamage
// always "absorbs" the hit - returns without dying - and pushes an
// accumulating counter into the linked entity_digitgod).
// hologram_damage's own TakeDamage-equivalent @0x100d20d0 (its own
// vtable slot 11, shared Precache/Classify/BloodColor with CHologram)
// depletes health instead and classifies the incoming attack into one
// of 9 damagetype categories by comparing an ammo-name string
// ("gauss_charged"/"gauss_bolt") and DMG_ bitflags.
//
// Live-Bug-Fix (2026-09-06, siehe STATUS.md): TakeDamage() hard-coded
// PlaySequence("flinch"), but that sequence name only exists in tube.mdl -
// Beak.mdl/Raptor.mdl use "smallflinch"/"bigflinch" instead. Shooting a
// beak/raptor-type hologram therefore fed LookupSequence() a nonexistent
// name, got -1 back, and ResetSequenceInfo() on that corrupted the
// entity's animation state - matches the user report "holograms that get
// shot no longer respawn" (tube-type instances were unaffected, being the
// one model that actually has "flinch"). Fixed via a new
// FlinchSequenceForType(), following the same per-creature-type pattern
// already used for idle/die sequences.
//
// Remaining approximation:
// - The undocumented Gunman temp-entity message (ID 0x17, same one
//   seen elsewhere in this file) used for the materialize/dissolve
//   beam effect is substituted with a real TE_SPRITE flash.
//=========================================================
class CHologram : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int Classify() override { return CLASS_PLAYER_ALLY; }
	int BloodColor() override { return DONT_BLEED; }
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT SpawnThink();
	void EXPORT HologramThink();
	void EXPORT DieThink();
	void EXPORT RespawnHologram();

protected:
	const char* ModelForType() const;
	const char* IdleSequenceForType() const;
	const char* FlinchSequenceForType() const;
	const char* DieSequenceForType() const;
	const char* ActivitySequenceForType() const;
	bool IsDisplayVolumeClear() const;
	void ResolveLinks();
	virtual bool TrainingTargetSucceeded();
	virtual void ResetTrainingTarget();
	void PlaySequence(const char* name)
	{
		pev->sequence = LookupSequence(name);
		ResetSequenceInfo();
	}

	int m_iCreatureType = 0;
	int m_iszTargetFail = 0;
	bool m_fDisplayed = false;
	bool m_fDissolving = false;
	float m_flDieDeadline = 0;
	float m_flNextFlinch = 0;
	float m_flNextAmbient = 0;
	float m_flNextActivity = 0;
	int m_iHitCount = 0;
	bool m_fResolved = false;
	CBaseEntity* m_pCounter = nullptr;
	CBaseEntity* m_pTargetFail = nullptr;
};
LINK_ENTITY_TO_CLASS(hologram_beak, CHologram);

TYPEDESCRIPTION CHologram::m_SaveData[] =
{
	DEFINE_FIELD(CHologram, m_iCreatureType, FIELD_INTEGER),
	DEFINE_FIELD(CHologram, m_iszTargetFail, FIELD_STRING),
	DEFINE_FIELD(CHologram, m_fDisplayed, FIELD_BOOLEAN),
	DEFINE_FIELD(CHologram, m_fDissolving, FIELD_BOOLEAN),
	DEFINE_FIELD(CHologram, m_flDieDeadline, FIELD_TIME),
	DEFINE_FIELD(CHologram, m_flNextFlinch, FIELD_TIME),
	DEFINE_FIELD(CHologram, m_flNextAmbient, FIELD_TIME),
	DEFINE_FIELD(CHologram, m_flNextActivity, FIELD_TIME),
	DEFINE_FIELD(CHologram, m_iHitCount, FIELD_INTEGER),
	DEFINE_FIELD(CHologram, m_fResolved, FIELD_BOOLEAN),
	DEFINE_FIELD(CHologram, m_pCounter, FIELD_CLASSPTR),
	DEFINE_FIELD(CHologram, m_pTargetFail, FIELD_CLASSPTR),
};

IMPLEMENT_SAVERESTORE(CHologram, CBaseMonster);

const char* CHologram::ModelForType() const
{
	switch (m_iCreatureType)
	{
	case 0:
		return "models/beak.mdl";
	case 1:
		return "models/tube.mdl";
	default:
		return "models/raptor.mdl"; // "MicroRaptor" in the FGD, same naming quirk as monster_microraptor
	}
}

const char* CHologram::IdleSequenceForType() const
{
	switch (m_iCreatureType)
	{
	case 0:
		return "sleeping";
	case 1:
		return "idle";
	default:
		return "idlenormal";
	}
}

const char* CHologram::FlinchSequenceForType() const
{
	// Bugfix (Live-Report): TakeDamage() spielte bisher hart codiert "flinch"
	// ab. Diese Sequenz existiert nur in tube.mdl - Beak.mdl/Raptor.mdl haben
	// stattdessen "smallflinch". LookupSequence() lieferte fuer Typ 0/2 also
	// -1, was ResetSequenceInfo() mit einem ungueltigen Sequenzindex aufrief
	// und die Hologramm-Instanz dauerhaft in einem korrupten Zustand liegen
	// liess (kein Respawn mehr) - exakt das Muster, das der Klassenkommentar
	// oben fuer Idle/Flinch/Die bereits als "pro Kreaturtyp" beschreibt, aber
	// nur fuer Idle/Die tatsaechlich umgesetzt hatte.
	switch (m_iCreatureType)
	{
	case 0:
		return RANDOM_LONG(0, 1) == 0 ? "bigflinch" : "smallflinch";
	case 1:
		return "flinch";
	default:
		return RANDOM_LONG(0, 1) == 0 ? "bigflinch" : "smallflinch";
	}
}

const char* CHologram::DieSequenceForType() const
{
	switch (m_iCreatureType)
	{
	case 0:
		return RANDOM_LONG(0, 1) == 0 ? "dietwitch" : "diesimple";
	case 1:
		return RANDOM_LONG(0, 1) == 0 ? "dieback" : "diesimple";
	default:
		return RANDOM_LONG(0, 1) == 0 ? "diefight" : "diesimple";
	}
}

const char* CHologram::ActivitySequenceForType() const
{
	switch (m_iCreatureType)
	{
	case 0:
	{
		static const char* const sequences[] = {"idlelookout", "walk", "run", "leap", "bite", "roarangry"};
		return sequences[RANDOM_LONG(0, ARRAYSIZE(sequences) - 1)];
	}
	case 1:
	{
		static const char* const sequences[] = {"walk", "idle", "mourn", "shoot"};
		return sequences[RANDOM_LONG(0, ARRAYSIZE(sequences) - 1)];
	}
	default:
	{
		static const char* const sequences[] = {"idlenormal", "idlechitter", "bitechunk", "chewchunk", "roarangry", "run"};
		return sequences[RANDOM_LONG(0, ARRAYSIZE(sequences) - 1)];
	}
	}
}

bool CHologram::IsDisplayVolumeClear() const
{
	// FUN_1005e5d0 is the SDK's UTIL_EntitiesInBox: it returns at most two
	// FL_CLIENT/FL_MONSTER entities whose abs bounds intersect this volume.
	const Vector vecMins = pev->origin - Vector(34, 34, 0);
	const Vector vecMaxs = pev->origin + Vector(34, 34, 0);
	CBaseEntity* pBlockingEntities[2];
	return UTIL_EntitiesInBox(pBlockingEntities, ARRAYSIZE(pBlockingEntities), vecMins, vecMaxs, FL_CLIENT | FL_MONSTER) == 0;
}

void CHologram::Precache()
{
	PrecacheModel("models/beak.mdl");
	PrecacheModel("sprites/white.spr");
	PrecacheModel("models/tube.mdl");
	PrecacheSound("ambience/holowarpin.wav");
	PrecacheModel("models/raptor.mdl");
	PrecacheSound("ambience/holodisplay.wav");
}

void CHologram::Spawn()
{
	pev->classname = MAKE_STRING("hologram_beak");
	Precache();
	SET_MODEL(ENT(pev), ModelForType());

	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 40));
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects |= EF_NODRAW;
	pev->health = 64;
	pev->yaw_speed = 5;
	pev->ideal_yaw = pev->angles.y;

	InitBoneControllers(); // retail 0x100d1190 zeroes controllers 0-3
	pev->rendermode = kRenderTransAdd; // retail @0x100d1190: rendermode 5
	pev->renderamt = 150;
	pev->renderfx = kRenderFxDistort;

	ResetSequenceInfo();
	PlaySequence(IdleSequenceForType());

	SetThink(&CHologram::HologramThink);
	pev->nextthink = gpGlobals->time + 0.1;

	m_fDisplayed = false;
	m_fDissolving = false;
	m_flNextActivity = gpGlobals->time + RANDOM_FLOAT(5.0, 10.0);
}

bool CHologram::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "creaturetype"))
	{
		m_iCreatureType = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "targetfail"))
	{
		m_iszTargetFail = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

void CHologram::ResolveLinks()
{
	if (!m_pCounter && !FStringNull(pev->target))
		m_pCounter = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));
	if (!m_pTargetFail && m_iszTargetFail != 0)
		m_pTargetFail = UTIL_FindEntityByTargetname(NULL, STRING(m_iszTargetFail));
}

void CHologram::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Retail UseToggle (0x100d1b50) calls UTIL_SetOrigin(pev, origin) in both branches.
	if (!m_fDisplayed)
	{
		SetThink(&CHologram::SpawnThink);
		pev->nextthink = gpGlobals->time + 0.1;
		UTIL_SetOrigin(pev, pev->origin);
		return;
	}

	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->effects |= EF_NODRAW;
	m_fDisplayed = false;
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/holodisplay.wav", 0.3, 0.8, 0, 100);
	UTIL_SetOrigin(pev, pev->origin);
}

void CHologram::SpawnThink()
{
	// Retail checks player/monster bounds against a horizontal +/-34-unit box.
	if (!IsDisplayVolumeClear())
	{
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	m_fDisplayed = true;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	// Retail SpawnThink: TE 14 = TE_IMPLOSION (radius 50, count 100, life 1.0 s).
	WRITE_BYTE(TE_IMPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_BYTE(50);
	WRITE_BYTE(100);
	WRITE_BYTE(10);
	MESSAGE_END();

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/holowarpin.wav", 1.0, 0.8, 0, 100);

	SetThink(&CHologram::HologramThink);
	pev->nextthink = gpGlobals->time + 0.1;
	UTIL_SetOrigin(pev, pev->origin);

	// Retail initializes the deadline to zero. It is armed only by a valid
	// hit in TakeDamage(), after the linked digit counter was updated.
	m_flDieDeadline = 0.0f;
	m_fDissolving = false;
}

// Retail HologramThink (0x100d1630), in retail order:
//  1. ambient sound every 2.63 s (m_flNextAmbient);
//  2. frame rate 0 -> sequence 0 + ResetSequenceInfo, else StudioFrameAdvance;
//  3. armed death deadline passed -> DieThink (nextthink = now);
//  4. sequence finished: when m_flNextActivity has passed, play the next
//     activity sequence and return; otherwise a non-looping sequence restarts
//     (animtime = now, framerate 1, finished = false, last event check = now,
//     frame 0). Retail does not check m_fDisplayed here.
void CHologram::HologramThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_flNextAmbient < gpGlobals->time)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/holodisplay.wav", 0.3, 0.8, 0, 100);
		m_flNextAmbient = gpGlobals->time + 2.63;
	}

	if (m_flFrameRate == 0.0f)
	{
		pev->sequence = 0;
		ResetSequenceInfo();
	}
	else
	{
		StudioFrameAdvance();
	}

	if (m_flDieDeadline != 0 && m_flDieDeadline < gpGlobals->time)
	{
		SetThink(&CHologram::DieThink);
		pev->nextthink = gpGlobals->time;
	}

	if (m_fSequenceFinished)
	{
		if (m_flNextActivity < gpGlobals->time)
		{
			PlaySequence(ActivitySequenceForType());
			m_flNextActivity = gpGlobals->time + RANDOM_FLOAT(3.0, 12.0);
			return;
		}

		if (!m_fSequenceLoops)
		{
			pev->animtime = gpGlobals->time;
			pev->framerate = 1.0f;
			m_fSequenceFinished = false;
			m_flLastEventCheck = gpGlobals->time;
			pev->frame = 0;
		}
	}
}

bool CHologram::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// Retail 0x100d17a0: the whole body (counter update, deadline, flinch)
	// only runs while the round is not yet resolved (+0x278 == 0).
	if (m_fResolved)
		return true;

	ResolveLinks();

	m_iHitCount += (int)flDamage;
	if (m_pCounter)
	{
		((CDigitGod*)m_pCounter)->SetDigitValue(m_iHitCount);
		m_flDieDeadline = gpGlobals->time + 3.0;
	}

	if (m_flNextFlinch < gpGlobals->time)
	{
		PlaySequence(FlinchSequenceForType());
		m_flNextFlinch = gpGlobals->time + 0.25;
		m_flNextActivity = gpGlobals->time;
	}

	// Always "absorbs" the hit - never actually dies from damage.
	return true;
}

bool CHologram::TrainingTargetSucceeded()
{
	return m_pCounter && ((CDigitGod*)m_pCounter)->HasSucceeded();
}

void CHologram::ResetTrainingTarget()
{
	if (m_pCounter)
		((CDigitGod*)m_pCounter)->SetDigitValue(0);
}

void CHologram::DieThink()
{
	if (!m_fDissolving)
	{
		ResolveLinks();

		bool fSucceeded = TrainingTargetSucceeded();
		if (!fSucceeded && !m_fResolved && m_pTargetFail)
		{
			m_pTargetFail->Use(this, this, USE_ON, 0);
		}
		m_fResolved = true;
		m_fDissolving = true;
		PlaySequence(DieSequenceForType());
	}

	// Retail clears the low renderfx bits, then fades 5 renderamt per 0.1 s.
	pev->renderfx &= ~0x0f;
	pev->renderamt -= 5.0;
	if (pev->renderamt > 0)
	{
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	pev->renderamt = 0;
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	// Retail DieThink: TE 14 = TE_IMPLOSION (radius 100, count 100, life 2.0 s).
	WRITE_BYTE(TE_IMPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_BYTE(100);
	WRITE_BYTE(100);
	WRITE_BYTE(20);
	MESSAGE_END();

	SetThink(&CHologram::RespawnHologram);
	pev->nextthink = gpGlobals->time + 2.0;
}

void CHologram::RespawnHologram()
{
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->health = 60;
	pev->renderamt = 150;
	pev->renderfx = kRenderFxDistort;
	m_fDisplayed = false;
	m_fDissolving = false;
	m_fResolved = false;
	m_iHitCount = 0;
	m_flDieDeadline = 0;

	ResetTrainingTarget();

	PlaySequence(ActivitySequenceForType());
	m_flNextActivity = gpGlobals->time + RANDOM_FLOAT(3.0, 12.0);
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/holodisplay.wav", 0.3, 0.8, 0, 100);
	m_flNextAmbient = gpGlobals->time + 2.63;

	SetThink(&CHologram::HologramThink);
	pev->nextthink = gpGlobals->time;
}


//=========================================================
// hologram_damage - see CHologram's comment above for the shared
// mechanic. This variant checks the hitting weapon and fires target or
// targetfail; health only drives its separate presentation/reload state.
// It shares Precache/Classify/BloodColor with CHologram in the original
// (kept separate here rather than forcing inheritance, since its own field
// layout is independently sized in the binary - object size 0x2b0 vs
// CHologram's, not a literal subclass).
//=========================================================
class CHologramDamage : public CHologram
{
public:
	void Spawn() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	static int DamageIndexForHit(const entvars_t* pevInflictor, int bitsDamageType);
	void FireFailureTarget();
	bool TrainingTargetSucceeded() override { return true; }
	void ResetTrainingTarget() override {}

	int m_iDamageType = 0;
	float m_flNextResult = 0.0f;
};
LINK_ENTITY_TO_CLASS(hologram_damage, CHologramDamage);

TYPEDESCRIPTION CHologramDamage::m_SaveData[] =
{
	DEFINE_FIELD(CHologramDamage, m_iDamageType, FIELD_INTEGER),
	DEFINE_FIELD(CHologramDamage, m_flNextResult, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CHologramDamage, CHologram);

// Retail 0x100d1f80: result cooldown 0, shared hologram init (0x100d1190),
// then classname "hologram_damage" (the base init leaves it to the caller).
void CHologramDamage::Spawn()
{
	m_flNextResult = 0.0f;
	CHologram::Spawn();
	pev->classname = MAKE_STRING("hologram_damage");
}

bool CHologramDamage::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "damagetype"))
	{
		m_iDamageType = atoi(pkvd->szValue);
		return true;
	}

	return CHologram::KeyValue(pkvd);
}

bool CHologramDamage::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		// Retail restores one health point and starts its dissolve/reload
		// sequence. Its target is an ordinary map target rather than a
		// CDigitGod counter; the two overrides above keep that base lifecycle
		// from interpreting it as one.
		pev->health = 1.0;
		m_flDieDeadline = gpGlobals->time;
		m_flNextFlinch = gpGlobals->time + 1.0;
	}

	if (m_flNextFlinch < gpGlobals->time)
	{
		PlaySequence(FlinchSequenceForType());
		m_flNextFlinch = gpGlobals->time + 0.25;
		m_flNextActivity = gpGlobals->time;
	}

	if (m_flNextResult >= gpGlobals->time)
		return true;

	// FUN_100d20d0 uses a separate 3-second result cooldown, so continuous
	// damage cannot repeatedly fire map targets.
	m_flNextResult = gpGlobals->time + 3.0;

	int iExpectedDamageType = m_iDamageType;
	if (iExpectedDamageType == 3)
		iExpectedDamageType = 4; // Retail's sniper target uses the bullet fallback.

	if (DamageIndexForHit(pevInflictor, bitsDamageType) == iExpectedDamageType)
		SUB_UseTargets(this, USE_ON, 0);
	else
		FireFailureTarget();

	return true;
}

int CHologramDamage::DamageIndexForHit(const entvars_t* pevInflictor, int bitsDamageType)
{
	if (pevInflictor && !FStringNull(pevInflictor->classname))
	{
		const char* const pszInflictorClassname = STRING(pevInflictor->classname);
		if (FStrEq(pszInflictorClassname, "gauss_charged"))
			return 2;
		if (FStrEq(pszInflictorClassname, "gauss_bolt"))
			return 1;
	}

	if (bitsDamageType & DMG_ENERGYBEAM)
		return 0;
	if (bitsDamageType & DMG_ACID)
		return 5;
	if (bitsDamageType & DMG_BURN)
		return 6;
	if (bitsDamageType & DMG_BLAST)
		return 7;
	if (bitsDamageType & DMG_RADIATION)
		return 8;

	return 4;
}

void CHologramDamage::FireFailureTarget()
{
	if (FStringNull(m_iszTargetFail))
		return;

	CBaseEntity* pFailureTarget = UTIL_FindEntityByTargetname(NULL, STRING(m_iszTargetFail));
	if (pFailureTarget)
		pFailureTarget->Use(this, this, USE_ON, 0);
}


//=========================================================
// decore_* family - plain, non-interactive decoration props (plus a
// couple of trigger-fired variants). Batch-added per user request to
// prioritize decorations over the remaining, much larger vehicle_tank/
// weapon_*/monster_human_unarmed tasks.
//
// Decompiled from gunman.dll (LINK addresses + vtable slot 0/1 for each,
// looked up in findings/gunman_dll_functions_ghidra.tsv and re-verified
// by decompiling Spawn/Precache fresh for every class below - not
// reused from findings/entities/decore_family.md without re-checking).
// Shared helper: most of these call a common "settle onto the floor,
// remove and log if none is found" routine (DROP_TO_FLOOR + ALERT +
// UTIL_Remove on failure) - reproduced once here as DropToFloorOrRemove
// rather than duplicated per class.
//
// Deliberately NOT included in this batch: decore_cam (CRustCamera -
// a functioning security camera with bone-controller pan/tilt that
// dynamically spawns a decore_camflare child; simplified here to a
// static prop instead of reproducing the full camera behavior),
// decore_butterflyflock and decore_pteradon (CBaseBird-family flying
// fauna with flight/crash-touch/dying-think behavior shared with
// decore_eagle - a genuinely different, more involved class of
// behavior than static decoration, not attempted in this pass).
//=========================================================
static void DropToFloorOrRemove(CBaseEntity* pEnt)
{
	if (DROP_TO_FLOOR(pEnt->edict()) == 0)
	{
		ALERT(at_error, "Item %s fell out of level at %.0f %.0f %.0f\n",
			STRING(pEnt->pev->classname), pEnt->pev->origin.x, pEnt->pev->origin.y, pEnt->pev->origin.z);
		UTIL_Remove(pEnt);
	}
}

//---------------------------------------------------------
// CDecore - shared base of the decorative props that use the retail
// decore init (retail 0x100cb960). Retail sets: framerate 1.0, movetype
// none, solid BBOX, box +-16 around the origin (SET_ORIGIN + UTIL_SetSize),
// health 20, Touch = DecoreTouch, Think = Animate. Retail does NOT call
// DROP_TO_FLOOR here, so neither does this base.
// Decompiled from gunman.dll: DecoreTouch @0x100cba20, Animate @0x100cba40.
//
// Retail DecoreTouch (0x100cba20) calls the decore's own vtable entry at
// offset 0x22c only for a player (pev->flags & FL_CLIENT). That entry is
// PlayerTouch here: each class overrides it for its player reaction. The
// default does nothing, which is what retail decores baboon, labstuff,
// swampplants and pipes do (their 0x22c entry returns 0).
//---------------------------------------------------------
class CDecore : public CBaseMonster
{
public:
	void InitDecore();
	virtual void PlayerTouch(CBaseEntity* pOther) {}
	void EXPORT DecoreTouch(CBaseEntity* pOther);
	void EXPORT Animate();
};

void CDecore::InitDecore()
{
	pev->framerate = 1.0f;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-16, -16, -16), Vector(16, 16, 16));
	pev->health = 20;
	SetTouch(&CDecore::DecoreTouch);
	SetThink(&CDecore::Animate);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CDecore::DecoreTouch(CBaseEntity* pOther)
{
	if (pOther != nullptr && (pOther->pev->flags & FL_CLIENT) != 0)
		PlayerTouch(pOther);
}

// Retail mushroom burst (0x100cbf00 / 0x100cc1b0): a TE_SPRITE_SPRAY of the
// gibmushroom sprite is sent to everyone in the PVS (SVC_TEMPENTITY, count 16,
// speed 40, noise 100, velocity (0,0,1)).  Only used by the two mushrooms.
static void MushroomBurst(CBaseEntity* pEnt)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pEnt->pev->origin);
	WRITE_BYTE(TE_SPRITE_SPRAY);
	WRITE_COORD(pEnt->pev->origin.x);
	WRITE_COORD(pEnt->pev->origin.y);
	WRITE_COORD(pEnt->pev->origin.z);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_COORD(1.0f);
	WRITE_SHORT(MODEL_INDEX("sprites/gibmushroom.spr"));
	WRITE_BYTE(16);
	WRITE_BYTE(40);
	WRITE_BYTE(100);
	MESSAGE_END();
}

void CDecore::Animate()
{
	StudioFrameAdvance(0);
	pev->nextthink = gpGlobals->time + 0.1f;
}

//---------------------------------------------------------
// decore_baboon - dead baboon prop with a twitch animation.
// Spawn @0x100cbb50, Precache @0x100cbc30.
//---------------------------------------------------------
class CDecoreBaboon : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_baboon, CDecoreBaboon);

void CDecoreBaboon::Precache()
{
	PrecacheModel("models/baboon.mdl");
	PrecacheModel("models/hgibs.mdl");
	PrecacheSound("ambience/monkydie.wav");
}

void CDecoreBaboon::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/baboon.mdl");
	InitDecore();
	pev->health = 30;
	m_bloodColor = BLOOD_COLOR_RED; // retail member +0x238 = 0xf7 (blood colour, not pev->body)
	pev->sequence = LookupSequence("deadtwitch");
	ResetSequenceInfo();
}

//---------------------------------------------------------
// decore_bodygib / decore_gutspile / decore_hatgib - CDecoreHumanGibsParts.
// Share the exact same Spawn (@0x100cc7d0), differ only in Precache/model.
//---------------------------------------------------------
class CDecoreHumanGibsParts : public CDecore
{
public:
	void Spawn() override;
	void EXPORT GutSmell();
	virtual const char* GetModelName() const = 0;
};

void CDecoreHumanGibsParts::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), GetModelName());
	InitDecore(); // retail Spawn @0x100cc7d0 calls the shared decore init first
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 40));
	pev->solid = SOLID_NOT;
	m_bloodColor = BLOOD_COLOR_RED; // retail member +0x238 = 0xf7 (blood colour, not pev->body)
	pev->health = 400;
	DropToFloorOrRemove(this);
	SetThink(&CDecoreHumanGibsParts::GutSmell);
	pev->nextthink = gpGlobals->time + 10.0f;
}

void CDecoreHumanGibsParts::GutSmell()
{
	// Retail CDecoreHumanGibsParts::GutSmell @0x100cc750 inserts this
	// one-shot scent ten seconds after spawning; it intentionally does not
	// reschedule itself.
	CSoundEnt::InsertSound(bits_SOUND_MEAT, pev->origin, 400, 10.0f);
}

class CDecoreBodygib : public CDecoreHumanGibsParts
{
public:
	void Precache() override { PrecacheModel("models/bodygib.mdl"); }
	const char* GetModelName() const override { return "models/bodygib.mdl"; }
};
LINK_ENTITY_TO_CLASS(decore_bodygib, CDecoreBodygib);

class CDecoreGutspile : public CDecoreHumanGibsParts
{
public:
	void Precache() override { PrecacheModel("models/gutspile.mdl"); }
	const char* GetModelName() const override { return "models/gutspile.mdl"; }
};
LINK_ENTITY_TO_CLASS(decore_gutspile, CDecoreGutspile);

class CDecoreHatgib : public CDecoreHumanGibsParts
{
public:
	void Precache() override { PrecacheModel("models/hatgib.mdl"); }
	const char* GetModelName() const override { return "models/hatgib.mdl"; }
};
LINK_ENTITY_TO_CLASS(decore_hatgib, CDecoreHatgib);

//---------------------------------------------------------
// decore_cactus - Spawn @0x100cc3c0, Precache @0x100cc4d0.
//---------------------------------------------------------
class CDecoreCactus : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_cactus, CDecoreCactus);

void CDecoreCactus::Precache()
{
	PrecacheModel("models/cactus.mdl");
	PrecacheModel("models/cactusgib.mdl");
}

void CDecoreCactus::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/cactus.mdl");
	InitDecore(); // retail Spawn @0x100cc3c0 calls the shared decore init (solid BBOX, touch, think)
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 64));
	pev->max_health = pev->health;
	m_bloodColor = BLOOD_COLOR_YELLOW; // retail member +0x238 = 0xc3 (blood colour, not pev->body)
	DropToFloorOrRemove(this);
}

//---------------------------------------------------------
// decore_explodable - CDecoreExplode. Explodes and gibs when triggered
// (ExplodeUse), or plays a scripted animation instead if a "decoreaction"
// field is set to something other than "none" (ActionAnimUse, then
// re-arms itself to ExplodeUse for the next trigger). Spawn @0x10079d90,
// Precache @0x10079ef0, ActionAnimUse @0x1007a100, ExplodeUse @0x1007a180.
//
// Nachtrag 2026-09-06 (Live-Report "ein Schiff das explodieren und eins
// das umkippt ist nicht da", MAYAN0B "tipship" = decore_explodable mit
// decoremodel=models/dropship.mdl): der bisherige Code hatte GAR KEINE
// KeyValue()-Ueberschreibung - "decoremodel" (das eigentliche Weltmodell,
// nicht ueber den reservierten "model"-Key gesetzt), "decoregib"
// (Truemmer-Modell, hartcodiert statt kartenkonfigurierbar) und
// "decoreidle" (benannte Ruhepose, z. B. "tipping#idle" fuer ein bereits
// umgekipptes Wrack) wurden komplett ignoriert - pev->model blieb leer,
// die Requisite war dadurch unsichtbar. "decoreaction" wurde nur an
// dieser einen zentralen Spawn()-Weiche ausgewertet (welcher Use-Handler
// aktiv wird), nie tatsaechlich gegen den Wert "none" verglichen -
// jetzt nachgebaut.
//
// Nachtrag 2 2026-09-06 (Nutzerwunsch "CDecoreExplode vollstaendig
// erfassen und nachbauen"): Ghidras Pseudo-C-Decompiler scheiterte an
// ExplodeUse() (0x1007a180) - dieselbe Rueckgabewert-Kalling-Convention-
// Verwechslung wie beim schon dokumentierten Ghidra-Artefakt (siehe
// fauna_cricket_re_followup_2026-09-06.md), hier aber so schwer dass die
// Pseudo-C-Ausgabe unbrauchbar war. Stattdessen per `objdump -d` die
// rohe x86-Disassemblierung Byte fuer Byte nachvollzogen. Ergebnis, jetzt
// vollstaendig (bis auf einen klar markierten Rest) nachgebaut:
// 1. Zwei `sphere_explosion`-Unterobjekte werden dynamisch erzeugt (nicht
//    ueber Map-KeyValues, sondern direkte Feldzuweisung, exakt wie
//    CSphereExplosion::ConfigureAndFire() hier tut) und sofort ausgeloest:
//    Explosion A: maxradius=450, radiusstep=8.0, trans=255, transstep=5,
//    numtracers=1, shockwave=1, implosion=0.
//    Explosion B: maxradius=120, radiusstep=2.0, trans=220, transstep=5,
//    numtracers=1, shockwave=1, implosion=0.
//    (shakeduration/fadetime/fadeholdtime aus denselben drei
//    mitgelesenen Float-Konstanten 1.0/2.5/0.1 uebernommen - Reihenfolge
//    nicht zweifelsfrei, aber alle drei Werte real belegt statt geraten.)
// 2. Zwei direkte TE_EXPLOSION-Nachrichten (MSG_PAS) an pev->origin: eine
//    mit dem Standard-Fireball-Sprite, Scale 40/Framerate 15, eine
//    zweite mit Scale 60/Framerate 10 - ein doppelter Feuerball-Burst.
// 3. Ein echtes `TE_BREAKMODEL` (Stock-Engine-Message, kein Client-DLL-
//    Custom-Code noetig): Box 400x400x128 an pev->origin, Geschwindigkeit
//    (0,0,200), Randomisierung 30, 18 Truemmerstuecke des konfigurierten
//    Gib-Modells (Feld this+0x280 = "decoregib"-Precache-Index),
//    Lebensdauer 20s, Flag TE_BOUNCE_SHOTSHELL.
// Korrektur 2026-09-08: `decoregib` (this+0x274) aktiviert im Retail drei
// physische Wrackteile: zwei CGibs mit Body 1/2 des konfigurierten Modells
// und ein CDropshipGib. Siehe dropship_gib_retail_2026-09-08.md.
//---------------------------------------------------------
class CDecoreSmokeGib : public CGib
{
public:
	void SpawnSmokeGib(const char* pszModel, int iSmokeSprite);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT SmokeThink();

private:
	int m_iSmokeSprite = 0;
	float m_flSmokeEnd = 0.0f;
};
LINK_ENTITY_TO_CLASS(decore_smokegib, CDecoreSmokeGib);

TYPEDESCRIPTION CDecoreSmokeGib::m_SaveData[] =
{
	DEFINE_FIELD(CDecoreSmokeGib, m_iSmokeSprite, FIELD_INTEGER),
	DEFINE_FIELD(CDecoreSmokeGib, m_flSmokeEnd, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CDecoreSmokeGib, CGib);

void CDecoreSmokeGib::SpawnSmokeGib(const char* pszModel, int iSmokeSprite)
{
	CGib::Spawn(pszModel);
	pev->classname = MAKE_STRING("decore_smokegib");
	m_iSmokeSprite = iSmokeSprite;
	m_flSmokeEnd = gpGlobals->time + 25.0f;
	SetThink(&CDecoreSmokeGib::SmokeThink);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CDecoreSmokeGib::SmokeThink()
{
	// Retail CGib::GibSmokeThink @0x1000d1a0: a 0.1-s cadence for 25 s;
	// FUN_100608b0(5) is the project's 5-percent helper, so only five
	// percent of ticks emit the standard TE_SPRITE_SPRAY smoke burst.
	pev->nextthink = gpGlobals->time + 0.1f;
	if (m_flSmokeEnd < gpGlobals->time)
	{
		SUB_StartFadeOut();
		return;
	}

	if (RANDOM_LONG(0, 99) >= 5)
		return;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITE_SPRAY);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_COORD(1);
	WRITE_SHORT(m_iSmokeSprite);
	WRITE_BYTE(1);
	WRITE_BYTE(100);
	WRITE_BYTE(20);
	MESSAGE_END();
}

class CDropshipGib : public CGib
{
public:
	void SpawnDropship(int iPlateGibsModel, int iExplosionSprite);
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT GibExplodeTouch(CBaseEntity* pOther);

private:
	CSprite* m_pFlameOne = nullptr;
	CSprite* m_pFlameTwo = nullptr;
	int m_iPlateGibsModel = 0;
	int m_iExplosionSprite = 0;
};
LINK_ENTITY_TO_CLASS(decore_dropshipgib, CDropshipGib);

TYPEDESCRIPTION CDropshipGib::m_SaveData[] =
{
	DEFINE_FIELD(CDropshipGib, m_pFlameOne, FIELD_CLASSPTR),
	DEFINE_FIELD(CDropshipGib, m_pFlameTwo, FIELD_CLASSPTR),
	DEFINE_FIELD(CDropshipGib, m_iPlateGibsModel, FIELD_INTEGER),
	DEFINE_FIELD(CDropshipGib, m_iExplosionSprite, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CDropshipGib, CGib);

void CDropshipGib::SpawnDropship(int iPlateGibsModel, int iExplosionSprite)
{
	// FUN_100797c0 first executes the CGib spawn with dropshipgib.mdl,
	// then creates two additive, temporary flame sprites on attachments 1/2.
	CGib::Spawn("models/dropshipgib.mdl");
	pev->classname = MAKE_STRING("decore_dropshipgib");
	m_iPlateGibsModel = iPlateGibsModel;
	m_iExplosionSprite = iExplosionSprite;

	m_pFlameOne = CSprite::SpriteCreate("sprites/flame.spr", pev->origin, true);
	if (m_pFlameOne)
	{
		m_pFlameOne->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation);
		m_pFlameOne->pev->spawnflags |= SF_SPRITE_TEMPORARY;
		m_pFlameOne->SetAttachment(edict(), 1);
	}

	m_pFlameTwo = CSprite::SpriteCreate("sprites/flame.spr", pev->origin, true);
	if (m_pFlameTwo)
	{
		m_pFlameTwo->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation);
		m_pFlameTwo->pev->spawnflags |= SF_SPRITE_TEMPORARY;
		m_pFlameTwo->SetAttachment(edict(), 2);
	}

	SetTouch(&CDropshipGib::GibExplodeTouch);
}

void CDropshipGib::GibExplodeTouch(CBaseEntity* pOther)
{
	(void)pOther;
	// CDropshipGib::GibExplodeTouch @0x10079a20: two explosion messages,
	// then a six-piece plate-gib breakmodel before both child flames and the
	// gib itself are removed.
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(40);
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin + Vector(0, 0, 170));
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 170);
	WRITE_SHORT(m_iExplosionSprite);
	WRITE_BYTE(80);
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	const Vector vecCenter = (pev->absmin + pev->absmax) * 0.5f;
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecCenter);
	WRITE_BYTE(TE_BREAKMODEL);
	WRITE_COORD(vecCenter.x);
	WRITE_COORD(vecCenter.y);
	WRITE_COORD(vecCenter.z + 40);
	WRITE_COORD(400);
	WRITE_COORD(400);
	WRITE_COORD(128);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_COORD(50);
	WRITE_BYTE(30);
	WRITE_SHORT(m_iPlateGibsModel);
	WRITE_BYTE(6);
	WRITE_BYTE(100);
	WRITE_BYTE(TE_BOUNCE_SHOTSHELL);
	MESSAGE_END();

	if (m_pFlameOne)
		UTIL_Remove(m_pFlameOne);
	if (m_pFlameTwo)
		UTIL_Remove(m_pFlameTwo);
	UTIL_Remove(this);
}

class CDecoreExplode : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	// Retail hat fuer decore_explodable keine eigenen TakeDamage-/Killed-Overrides
	// (Vtable-Slots 11/14 zeigen auf gemeinsame Basisfunktionen) und setzt takedamage 0;
	// die fruehere Schadens-Logik (ActionAnimUse bei Treffer) war toter Code und ist entfernt.
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	// Nachtrag 2026-09-06: urspruenglich per SetUse() zwischen zwei
	// EXPORT'ten Member-Funktionen umgeschaltet - fuehrte im Live-Test zu
	// "Invalid function pointer in entity!" beim naechsten Autosave
	// (trigger_autosave, Stock-SDK-Entity, unveraendert). Stattdessen ein
	// einfaches Zustandsflag, geprueft im einzigen virtuellen Use()-
	// Override - keine dynamische pev->use-Zeigerumschaltung mehr noetig.
	void ExplodeUse();
	void ActionAnimUse();
	bool m_bActionPlayed = false;

	int m_iSpriteExplosion = 0;
	int m_iszGibModel = 0;
	int m_iGibModelIndex = 0;
	int m_iPlateGibsModelIndex = 0;
	int m_iDropshipExplosionSprite = 0;
	int m_iGibSmokeSprite = 0;
	int m_iszIdleSequence = 0;
	int m_iszAction = 0;
};
LINK_ENTITY_TO_CLASS(decore_explodable, CDecoreExplode);

TYPEDESCRIPTION CDecoreExplode::m_SaveData[] =
{
	DEFINE_FIELD(CDecoreExplode, m_bActionPlayed, FIELD_BOOLEAN),
	DEFINE_FIELD(CDecoreExplode, m_iszGibModel, FIELD_STRING),
	DEFINE_FIELD(CDecoreExplode, m_iGibModelIndex, FIELD_INTEGER),
	DEFINE_FIELD(CDecoreExplode, m_iPlateGibsModelIndex, FIELD_INTEGER),
	DEFINE_FIELD(CDecoreExplode, m_iDropshipExplosionSprite, FIELD_INTEGER),
	DEFINE_FIELD(CDecoreExplode, m_iGibSmokeSprite, FIELD_INTEGER),
	DEFINE_FIELD(CDecoreExplode, m_iszAction, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE(CDecoreExplode, CBaseMonster);

void CDecoreExplode::Precache()
{
	m_iGibModelIndex = PrecacheModel((m_iszGibModel != 0) ? STRING(m_iszGibModel) : "models/plategibs.mdl");
	m_iSpriteExplosion = PrecacheModel("sprites/white2.spr");
	PrecacheModel("sprites/firebeam.spr");
	PrecacheModel("models/dropshipgib.mdl");
	m_iPlateGibsModelIndex = PrecacheModel("models/plategibs.mdl");
	m_iDropshipExplosionSprite = PrecacheModel("sprites/fexplo.spr");
	PrecacheModel("sprites/flame.spr");
	m_iGibSmokeSprite = PrecacheModel("sprites/steam1.spr");
}

bool CDecoreExplode::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "decoremodel"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "decoregib"))
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "decoreidle"))
	{
		m_iszIdleSequence = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "decoreaction"))
	{
		m_iszAction = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

void CDecoreExplode::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), STRING(pev->model));
	// Retail Spawn @0x10079d90 (frisch dekompiliert 2026-10-31): Box
	// (-16,-16,-36)-(16,16,36), solid SLIDEBOX->SOLID_NOT, movetype FLY,
	// DONT_BLEED, health 8, takedamage 0 (Retail ist NICHT beschiessbar;
	// die frühere Annahme DAMAGE_YES/health 4 stimmte nicht).
	UTIL_SetSize(pev, Vector(-16, -16, -36), Vector(16, 16, 36));
	pev->movetype = MOVETYPE_FLY;
	m_bloodColor = DONT_BLEED;
	pev->health = 8;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	UTIL_SetOrigin(pev, pev->origin);

	if (m_iszIdleSequence != 0)
	{
		SetSequenceByName(STRING(m_iszIdleSequence));
		ResetSequenceInfo();
	}
}

void CDecoreExplode::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	bool bHasAction = m_iszAction != 0 && !FStrEq(STRING(m_iszAction), "none");
	if (bHasAction && !m_bActionPlayed)
	{
		ActionAnimUse();
		return;
	}

	ExplodeUse();
}



void CDecoreExplode::ActionAnimUse()
{
	if (m_iszAction != 0)
	{
		int iSequence = LookupSequence(STRING(m_iszAction));
		if (iSequence != -1)
		{
			pev->sequence = iSequence;
			pev->frame = 0;
			ResetSequenceInfo();
		}
	}

	// RE-bestaetigt: nach dem Abspielen der Aktionsanimation schaltet sich
	// die Requisite selbst auf ExplodeUse um - ein zweiter Trigger laesst
	// sie explodieren statt die Animation zu wiederholen.
	m_bActionPlayed = true;
}

void CDecoreExplode::ExplodeUse()
{
	// Schritt 1: zwei sphere_explosion-Unterobjekte, siehe Klassenkommentar
	// oben fuer die per Disassemblierung bestaetigten Feldwerte.
	CSphereExplosion* pExplosionA = GetClassPtr((CSphereExplosion*)NULL);
	pExplosionA->ConfigureAndFire(pev->origin, 450.0f, 8.0f, 255, 5, 1, 1, 0, 1.0f, 2.5f, 0.1f);

	CSphereExplosion* pExplosionB = GetClassPtr((CSphereExplosion*)NULL);
	pExplosionB->ConfigureAndFire(pev->origin, 120.0f, 2.0f, 220, 5, 1, 1, 0, 1.0f, 2.5f, 0.1f);

	// Schritt 2: doppelter Feuerball-Burst (zwei direkte TE_EXPLOSION,
	// unterschiedliche Groesse/Framerate).
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(40);
	WRITE_BYTE(15);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(60);
	WRITE_BYTE(10);
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	// Nachtrag 2026-09-06 (Nutzerwunsch: Erschuetterung+Lichtblitz bei
	// jeder Explosion, siehe util.h/UTIL_ExplosionEffects). Kein RE-Fund,
	// bewusste Ergaenzung - die beiden echten sphere_explosion-Unterobjekte
	// oben liefern bereits ihre eigene, RE-belegte Erschuetterung.
	UTIL_ExplosionEffects(pev->origin, 150.0f);

	// Schritt 3: echtes TE_BREAKMODEL - 18 Truemmerstuecke des
	// konfigurierten Gib-Modells, Box 400x400x128, Aufwaertsgeschwindigkeit
	// 200, Lebensdauer-Byte 200, Flag TE_BOUNCE_SHOTSHELL (2) - alle vier
	// Rohbytes direkt aus der Disassemblierung uebernommen.
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_BREAKMODEL);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(400);
	WRITE_COORD(400);
	WRITE_COORD(128);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_COORD(200);
	WRITE_BYTE(30);
	WRITE_SHORT(m_iGibModelIndex);
	WRITE_BYTE(18);
	WRITE_BYTE(200);
	WRITE_BYTE(TE_BOUNCE_SHOTSHELL);
	MESSAGE_END();

	if (m_iszGibModel != 0)
	{
		const char* const pszGibModel = STRING(m_iszGibModel);
		// Retail uses gpGlobals->v_forward/v_up for these two offsets. Make
		// that otherwise ambient state deterministic from this prop's angles.
		UTIL_MakeVectors(pev->angles);
		const Vector vecGibOneOrigin = pev->origin + gpGlobals->v_forward * 50.0f + gpGlobals->v_up * 20.0f;
		const Vector vecGibTwoOrigin = pev->origin + gpGlobals->v_up * 20.0f;
		for (int iBody = 1; iBody <= 2; ++iBody)
		{
			CDecoreSmokeGib* pGib = GetClassPtr((CDecoreSmokeGib*)NULL);
			pGib->SpawnSmokeGib(pszGibModel, m_iGibSmokeSprite);
			pGib->pev->body = iBody;
			UTIL_SetOrigin(pGib->pev, iBody == 1 ? vecGibOneOrigin : vecGibTwoOrigin);

			edict_t* pVisiblePlayer = FIND_CLIENT_IN_PVS(pGib->edict());
			CBaseEntity* pPlayer = pVisiblePlayer ? CBaseEntity::Instance(pVisiblePlayer) : nullptr;
			if (pPlayer && RANDOM_LONG(0, 99) < 5)
			{
				pGib->pev->velocity = (pPlayer->EyePosition() - pGib->pev->origin).Normalize() * 300.0f;
				pGib->pev->velocity.z += 100.0f;
			}
			else
			{
				pGib->pev->velocity = Vector(RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(200, 300));
			}
			pGib->pev->velocity = pGib->pev->velocity * 4.0f;
			pGib->pev->avelocity = Vector(RANDOM_FLOAT(100, 200), RANDOM_FLOAT(100, 300), 0);
			pGib->LimitVelocity();
		}

		CDropshipGib* pDropshipGib = GetClassPtr((CDropshipGib*)NULL);
		pDropshipGib->SpawnDropship(m_iPlateGibsModelIndex, m_iDropshipExplosionSprite);
		UTIL_SetOrigin(pDropshipGib->pev, pev->origin + Vector(0, 0, 50));
		pDropshipGib->pev->velocity = Vector(RANDOM_FLOAT(-50, 50), RANDOM_FLOAT(-50, 50), RANDOM_FLOAT(200, 300)) * 6.0f;
		pDropshipGib->pev->avelocity = Vector(RANDOM_FLOAT(100, 200), RANDOM_FLOAT(100, 300), 0);
		pDropshipGib->LimitVelocity();
	}

	UTIL_Remove(this);
}

//---------------------------------------------------------
// decore_foot - CBigFootCrush. A hidden invisible foot prop that
// crushes on Use() (choreographed stomp effect for monster_renesaur).
// Spawn @0x100cd2e0, Precache @0x100cd3b0.
//---------------------------------------------------------
class CBigFootCrush : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT CheckAnimEnd();
};
LINK_ENTITY_TO_CLASS(decore_foot, CBigFootCrush);

void CBigFootCrush::Precache()
{
	PrecacheModel("models/renesaurfoot.mdl");
}

void CBigFootCrush::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/renesaurfoot.mdl");
	UTIL_SetSize(pev, Vector(-1, -1, -1), Vector(1, 1, 1));
	UTIL_SetOrigin(pev, pev->origin);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->effects |= EF_NODRAW;
}

void CBigFootCrush::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Retail CBigFootCrush::BigFootUse selects footcrush, unhides the
	// model and polls until the non-looping stomp animation completes.
	SetSequenceByName("footcrush");
	pev->frame = 0;
	ResetSequenceInfo();
	pev->effects &= ~EF_NODRAW;
	SetThink(&CBigFootCrush::CheckAnimEnd);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CBigFootCrush::CheckAnimEnd()
{
	StudioFrameAdvance();
	if (m_fSequenceFinished)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1f;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
}

//---------------------------------------------------------
// decore_goldskull - CDecoreMonkeySkull. The retail entity uses the
// shared non-solid CDecore setup, then exposes a Use callback which starts
// the model's opening sequence.
// Spawn @0x100cd0e0, Precache @0x100cd190, MonkeyUse @0x100cd070.
//---------------------------------------------------------
class CDecoreMonkeySkull : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
};
LINK_ENTITY_TO_CLASS(decore_goldskull, CDecoreMonkeySkull);

void CDecoreMonkeySkull::Precache()
{
	PrecacheModel("models/goldskull.mdl");
}

void CDecoreMonkeySkull::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/goldskull.mdl");
	InitDecore(); // retail Spawn @0x100cd0e0: shared decore init (framerate 1, health 20), then overrides

	// CDecore::Spawn first establishes the generic decorative-prop state.
	// CDecoreMonkeySkull then replaces its bounds with the retail zero box
	// and disables movement and damage handling. The value one written by
	// CDecore::Spawn belongs to takedamage, not to pev->health.
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);
	pev->takedamage = DAMAGE_NO;
	SetThink(nullptr); // retail clears the think pointer (param_1[4] = 0)
}

void CDecoreMonkeySkull::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Retail MonkeyUse ignores USE_TYPE and always restarts the opening
	// animation from frame zero. SetSequenceByName includes the required
	// sequence-info reset for a changed sequence.
	SetSequenceByName("opening");
	pev->frame = 0;
}

//---------------------------------------------------------
// decore_ice - CIce. A block of ice, optionally with a Xenome trapped
// inside (per the "beakinside" FGD keyvalue), that melts on Use().
// Spawn @0x100ccdb0, Precache @0x100ccf10.
//---------------------------------------------------------
class CDecoreIceBeak : public CBaseEntity
{
public:
	void Spawn() override
	{
		pev->classname = MAKE_STRING("decore_icebeak");
		PrecacheModel("models/icebeak.mdl");
		SET_MODEL(ENT(pev), "models/icebeak.mdl");
		pev->movetype = MOVETYPE_NOCLIP;
		pev->solid = SOLID_NOT;
		pev->effects = 0;
		pev->takedamage = DAMAGE_NO;
	}
};
LINK_ENTITY_TO_CLASS(decore_icebeak, CDecoreIceBeak);

class CIce : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT SpawnIceBeak();

private:
	int m_iBeakInside = 0;
	EHANDLE m_hIceBeak;
};
LINK_ENTITY_TO_CLASS(decore_ice, CIce);

void CIce::Precache()
{
	PrecacheModel("models/ice.mdl");
	PrecacheModel("models/ceilinggibs.mdl");
	UTIL_PrecacheOther("decore_icebeak");
}

void CIce::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/ice.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 40));
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 200;
	DropToFloorOrRemove(this);
	SetThink(&CIce::SpawnIceBeak);
	pev->nextthink = gpGlobals->time + 0.1f; // retail @0x100ccdb0: time + 0.1
}

bool CIce::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "beakinside"))
	{
		m_iBeakInside = atoi(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

void CIce::SpawnIceBeak()
{
	if (m_iBeakInside != 1)
		return;

	pev->scale = 180.0f;
	CBaseEntity* pBeak = CBaseEntity::Create("decore_icebeak", pev->origin, pev->angles, edict());
	if (pBeak)
	{
		pBeak->pev->velocity = pev->velocity;
		m_hIceBeak = pBeak;
	}
}

void CIce::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (m_iBeakInside != 0 && m_hIceBeak)
		UTIL_Remove(m_hIceBeak);

	CGib::SpawnCustomGibs(pev, 4, "models/ceilinggibs.mdl");
	UTIL_Remove(this);
}

//---------------------------------------------------------
// decore_labstuff - Spawn @0x100ce430, Precache @0x100ce480.
//---------------------------------------------------------
class CDecoreLabstuff : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_labstuff, CDecoreLabstuff);

void CDecoreLabstuff::Precache()
{
	PrecacheModel("models/labstuff.mdl");
	PrecacheModel("models/decoregibs.mdl");
}

void CDecoreLabstuff::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/labstuff.mdl");
	InitDecore();
}

//---------------------------------------------------------
// decore_mushroom / decore_mushroom2 - two mushroom variants.
// Spawn @0x100cbe70/0x100cc120, Precache @0x100cbec0/0x100cc170.
//---------------------------------------------------------
class CDecoreMushroom : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
	void PlayerTouch(CBaseEntity* pOther) override;
};
LINK_ENTITY_TO_CLASS(decore_mushroom, CDecoreMushroom);

void CDecoreMushroom::Precache()
{
	PrecacheModel("models/mushroom.mdl");
	PrecacheSound("mushroom/mushroom_hit2.wav");
	PrecacheSound("mushroom/mushroom_hit3.wav");
	PrecacheSound("mushroom/mushroom_pop1.wav");
	PrecacheModel("sprites/gibmushroom.spr");
}

void CDecoreMushroom::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/mushroom.mdl");
	InitDecore();
	pev->body = -1;
}

// Retail 0x100cbf00: burst, one of two hit sounds on CHAN_ITEM, then the touch
// is removed (one-shot).  The pop sound is precached but never played there.
void CDecoreMushroom::PlayerTouch(CBaseEntity* pOther)
{
	MushroomBurst(this);
	const char* sound = RANDOM_LONG(0, 1) == 0 ? "mushroom/mushroom_hit3.wav" : "mushroom/mushroom_hit2.wav";
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, sound, 1.0, 0.8, 0, 100);
	SetTouch(nullptr);
}

class CDecoreMushroom2 : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
	void PlayerTouch(CBaseEntity* pOther) override;
};
LINK_ENTITY_TO_CLASS(decore_mushroom2, CDecoreMushroom2);

void CDecoreMushroom2::Precache()
{
	PrecacheModel("models/mushroom2.mdl");
	PrecacheSound("mushroom/mushroom_hit1.wav");
	PrecacheSound("mushroom/mushroom_hit2.wav");
	PrecacheSound("mushroom/mushroom_pop2.wav");
	PrecacheModel("sprites/gibmushroom.spr");
}

void CDecoreMushroom2::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/mushroom2.mdl");
	InitDecore();
	pev->body = -1;
}

// Retail 0x100cc1b0: same burst, always mushroom_hit1 on CHAN_ITEM, then the
// touch is removed (one-shot).
void CDecoreMushroom2::PlayerTouch(CBaseEntity* pOther)
{
	MushroomBurst(this);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "mushroom/mushroom_hit1.wav", 1.0, 0.8, 0, 100);
	SetTouch(nullptr);
}

//---------------------------------------------------------
// decore_nest - CNest. An Ourano egg nest that calls nearby Ouranos for
// help. It is a single-use healing prop: Use restores 40--60 health,
// cracks the egg model and consumes the nest. NestTouch @0x10075b10,
// NestUse @0x10075b40, Spawn @0x10075c70, Precache @0x10075dd0.
//
// Nachtrag 2026-09-06 (Live-Report "decore_nest hatte auch einen
// Error" - Konsole zeigte "decore_nest has no sequence for act:1"):
// 0x10075c70 wurde bereits waehrend der COurano-Vtable-Untersuchung
// (selbe Session) mitdecompiliert - das echte Spawn() ruft KEIN
// MonsterInit() auf (Korrektur 2026-10-31: der "andere Vtable-Slot" 0x10c ist CBaseMonster::MonsterInit 0x1002af50; Retail ruft es doch auf, die SDK-Klasse verzichtet bewusst darauf, siehe cam_nest_abgleich; das haette einen vollen KI-/Schedule-Apparat fuer
// eine reine, unbewegliche Touch-/Use-Deko-Requisite bedeutet), sondern
// nur eine leichte Animationsstatus-Initialisierung ueber einen anderen
// Vtable-Slot. models/ornest.mdl hat keine ACT_IDLE-getaggte Sequenz -
// MonsterInit()s SetActivity(ACT_IDLE)-Versuch schlug daher fehl und
// spammte die Konsole, ohne dass es je einen tatsaechlichen Zweck hatte
// (kein KI-Schedule wird fuer dieses Prop je gebraucht).
//---------------------------------------------------------
class CNest : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	// Retail Classify @0x10075dc0: virtual slot 34 (@0x100017a0 = IsAlive, deadflag != DEAD_DEAD)
	// != 0 ? 3 : 0, i.e. CLASS_HUMAN_PASSIVE while alive.
	int Classify() override { return IsAlive() ? CLASS_HUMAN_PASSIVE : CLASS_NONE; }
	void Touch(CBaseEntity* pOther) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

private:
	void CallOuranosForHelp();
};
LINK_ENTITY_TO_CLASS(decore_nest, CNest);

void CNest::Precache()
{
	PrecacheModel("models/ornest.mdl");
	PrecacheSound("ourano/eggs_crack1.wav");
	PrecacheSound("ourano/eggs_crack2.wav");
}

void CNest::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/ornest.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 16));
	pev->solid = SOLID_SLIDEBOX; // retail @0x10075c70: solid 3
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_AIM;
	pev->health = 12;
	m_bloodColor = BLOOD_COLOR_GREEN; // retail member +0x238 = 0
	// Retail decore_nest Spawn sets capability bit 0x8 (bits_CAP_SQUAD) in m_afCapability (+0x220).
	m_afCapability |= bits_CAP_SQUAD;
	DropToFloorOrRemove(this);
	ResetSequenceInfo();
}

void CNest::Touch(CBaseEntity* pOther)
{
	CallOuranosForHelp();
}

void CNest::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (pActivator == nullptr)
		return;

	// Retail calls CBaseEntity::TakeHealth with RANDOM_LONG(40, 60), then
	// alerts nearby Ouranos, changes to the cracked body and marks itself
	// spent. It deliberately ignores the return value of TakeHealth().
	pActivator->TakeHealth((float)RANDOM_LONG(40, 60), DMG_GENERIC);
	CallOuranosForHelp();
	pev->body = 1;
	pev->health = -1;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "ourano/eggs_crack1.wav" : "ourano/eggs_crack2.wav", 1.0, ATTN_NORM, 0, 100);
}

void CNest::CallOuranosForHelp()
{
	// Retail uses the same CallForHelp pattern as the stock alien slave:
	// all matching monsters within 512 receive bits_MEMORY_PROVOKED and
	// inherit this nest's enemy record.
	CBaseEntity* pEntity = nullptr;
	while ((pEntity = UTIL_FindEntityByClassname(pEntity, "monster_ourano")) != nullptr)
	{
		if ((pev->origin - pEntity->pev->origin).Length() >= 512.0f)
			continue;

		CBaseMonster* pMonster = pEntity->MyMonsterPointer();
		if (pMonster != nullptr)
		{
			pMonster->Remember(bits_MEMORY_PROVOKED);
			pMonster->PushEnemy(m_hEnemy, m_vecEnemyLKP);
		}
	}
}

//---------------------------------------------------------
// decore_pipes - steam pipes with a randomized start frame.
// Spawn @0x100cbd20, Precache @0x100cbda0.
//---------------------------------------------------------
class CDecorePipes : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_pipes, CDecorePipes);

void CDecorePipes::Precache()
{
	PrecacheModel("models/pipes.mdl");
}

void CDecorePipes::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/pipes.mdl");
	ResetSequenceInfo();
	pev->frame = (float)RANDOM_LONG(0, 254);
}

//---------------------------------------------------------
// decore_prickle - a small angular/no-clip prop with a random skin.
// Spawn @0x100cc620, Precache @0x100cc740.
//---------------------------------------------------------
class CDecorePrickle : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_prickle, CDecorePrickle);

void CDecorePrickle::Precache()
{
	PrecacheModel("models/prickle.mdl");
}

void CDecorePrickle::Spawn()
{
	// Retail Spawn @0x100cc620 (fresh decompile 2026-10-31): Precache, model,
	// shared decore init (0x100cb960), box (-8,-8,0)..(8,8,8), skin
	// RANDOM_LONG(0,1), blood colour yellow, takedamage DAMAGE_YES, drop to
	// floor. No movetype write beyond the init's MOVETYPE_NONE (the earlier
	// MOVETYPE_ANGLENOCLIP claim is not in the decompile).
	Precache();
	SET_MODEL(ENT(pev), STRING(pev->model));
	InitDecore();
	UTIL_SetSize(pev, Vector(-8, -8, 0), Vector(8, 8, 8));
	pev->skin = RANDOM_LONG(0, 1);
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->takedamage = DAMAGE_YES;
	DropToFloorOrRemove(this);
}

//---------------------------------------------------------
// decore_sittingtubemortar - a spent tube-queen mortar shell prop.
// Spawn @0x100cd770, Precache @0x100cd870. Retail Spawn: SET_MODEL, the
// CDecore init (0x100cb960), box -16/-16/-16 .. 16/16/48, sequence "standing",
// ResetSequenceInfo, then DROP_TO_FLOOR; if the drop fails it logs
// "fell out of level" and removes the entity (same as DropToFloorOrRemove).
//---------------------------------------------------------
class CDecoreSittingTubeMortar : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
	void PlayerTouch(CBaseEntity* pOther) override;
};
LINK_ENTITY_TO_CLASS(decore_sittingtubemortar, CDecoreSittingTubeMortar);

void CDecoreSittingTubeMortar::Precache()
{
	PrecacheModel("models/tubemortar.mdl");
	PrecacheModel("sprites/tubeguts.spr");
	PrecacheSound("tubequeen/mortarexplode1.wav");
	PrecacheSound("tubequeen/mortarexplode2.wav");
	PrecacheSound("tubequeen/mortarexplode3.wav");
	PrecacheSound("tube/tube_explodingdeath.wav");
}

// Retail 0x100cd8d0 (player touch): the explosion sound on CHAN_BODY, then the
// retail TE_EXPLOSION (scale 10, framerate 15, flags 6) of the tubeguts sprite
// to everyone in the PAS, then the entity is removed.
// Retail gib loop: see the comment in PlayerTouch. The gib start offset that
// retail derives from the decore's +0x210 vector is an assumption (omitted).
void TubeRocketSetLifetime(CBaseEntity* pEnt, float flSeconds);

void CDecoreSittingTubeMortar::PlayerTouch(CBaseEntity* pOther)
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "tube/tube_explodingdeath.wav", 1.0, 0.8, 0, 100);

	// Retail gib loop (0x100cd8d0): RANDOM_LONG(6,12) "tuberocket" entities.
	// Direction: normalized (RANDOM(-0.5,0.5), RANDOM(-0.5,0.5), RANDOM(0.1,0.5)).
	// Velocity: 300 * v_forward of that direction. Origin: decore origin + 16 in Z,
	// then RANDOM(-20,20) in X/Y and RANDOM(32,64) in Z. Movetype bounce, think
	// after 0.3 s, lifetime RANDOM(0.3,0.6) s.
	// Documented assumption: retail also adds a 25-unit offset along a direction
	// built from the decore's vector at +0x210; that source is not resolved here,
	// so the gibs start without it.
	const int gibCount = RANDOM_LONG(6, 12);
	for (int i = 0; i < gibCount; ++i)
	{
		Vector dir(RANDOM_FLOAT(-0.5f, 0.5f), RANDOM_FLOAT(-0.5f, 0.5f), RANDOM_FLOAT(0.1f, 0.5f));
		dir = dir.Normalize();
		Vector angles = UTIL_VecToAngles(dir);
		UTIL_MakeVectors(angles);

		Vector origin = pev->origin + Vector(0, 0, 16);
		origin.x += RANDOM_FLOAT(-20, 20);
		origin.y += RANDOM_FLOAT(-20, 20);
		origin.z += RANDOM_FLOAT(32, 64);

		CBaseEntity* pGib = CBaseEntity::Create("tuberocket", origin, angles, edict());
		if (pGib == nullptr)
			continue;
		pGib->pev->velocity = gpGlobals->v_forward * 300.0f;
		pGib->pev->movetype = MOVETYPE_BOUNCE;
		pGib->pev->framerate = 0;
		pGib->pev->nextthink = gpGlobals->time + 0.3f;
		TubeRocketSetLifetime(pGib, RANDOM_FLOAT(0.3f, 0.6f));
	}
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(MODEL_INDEX("sprites/tubeguts.spr"));
	WRITE_BYTE(10);
	WRITE_BYTE(15);
	WRITE_BYTE(6);
	MESSAGE_END();
	UTIL_Remove(this);
}

void CDecoreSittingTubeMortar::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), STRING(pev->model));
	InitDecore();
	UTIL_SetSize(pev, Vector(-16, -16, -16), Vector(16, 16, 48));
	pev->sequence = LookupSequence("standing");
	ResetSequenceInfo();
	DropToFloorOrRemove(this);
}

//---------------------------------------------------------
// decore_swampplants - trivial ground vegetation decoration.
// Spawn @0x100cd000, Precache @0x100cd050.
//---------------------------------------------------------
class CDecoreSwampPlants : public CDecore
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_swampplants, CDecoreSwampPlants);

void CDecoreSwampPlants::Precache()
{
	PrecacheModel("models/swampstuff.mdl");
}

void CDecoreSwampPlants::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/swampstuff.mdl");
	InitDecore();
	pev->solid = SOLID_NOT;
}

//---------------------------------------------------------
// decore_torch / decore_torchflame - CTorch (the model) and CFlame (the
// separate flame sprite it spawns on top). Spawn @0x100d26e0/0x100d2400,
// Precache @0x100d26c0/0x100d23b0.
//---------------------------------------------------------
class CFlame : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT Animate();
};
LINK_ENTITY_TO_CLASS(decore_torchflame, CFlame);

void CFlame::Precache()
{
	PrecacheModel("sprites/flames.spr");
}

void CFlame::Spawn()
{
	Precache();
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->scale = 0.75;
	pev->rendermode = kRenderTransAdd; // retail @0x100d2400: rendermode 5
	pev->renderamt = 255;
	UTIL_SetSize(pev, Vector(-4, -4, -8), Vector(4, 4, 16)); // retail box
	SET_MODEL(ENT(pev), "sprites/flames.spr");
	pev->frame = (float)RANDOM_LONG(0, 20);
	SetThink(&CFlame::Animate);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CFlame::Animate()
{
	pev->nextthink = gpGlobals->time + 0.1;
	pev->frame += 1;
	if (pev->frame > 20)
		pev->frame = 0;
}

class CTorch : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_torch, CTorch);

void CTorch::Precache()
{
	PrecacheModel("models/torch.mdl");
	UTIL_PrecacheOther("decore_torchflame");
}

void CTorch::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/torch.mdl");
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-8, -8, -16), Vector(8, 8, 8)); // retail @0x100d26e0: mins.z = -16

	// Nachtrag 2026-09-06 (Nutzer-Meldung "Flamme ist versetzt, muesste
	// weiter oben sein"): der bisherige Z-Offset (12) war eine grobe
	// Schaetzung ohne Geometrie-Beleg. torch.mdl per tools/mdl_decompile.py
	// echt vermessen (Torch_ref.smd, 114 Vertices): reale Mesh-Hoehe reicht
	// von Z=-11.9 bis Z=+14.8 relativ zum Modell-Origin - die Fackelspitze
	// (siehe models-src/Torch/preview.png, spitz zulaufend mit oranger
	// Flammen-Textur am obersten Punkt) liegt also bei ca. Z=+15, nicht +12.
	CBaseEntity* pFlame = CBaseEntity::Create("decore_torchflame", pev->origin + Vector(0, 0, 32), pev->angles, edict()); // retail FUN_100d2780: origin.z + 32.0 (constant at 0x100ea884); the 15 above was a geometry estimate
	(void)pFlame;
}

//---------------------------------------------------------
// decore_cam - CRustCamera. Retail runs the base monster AI, then tracks its
// configured target (or player 1) with the model's yaw and pitch controllers.
// The attached eye/LED sprites and cameracone child retain their own state.
//---------------------------------------------------------
class CDecoreCamFlare : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
};
LINK_ENTITY_TO_CLASS(decore_camflare, CDecoreCamFlare);

void CDecoreCamFlare::Precache()
{
	PrecacheModel("models/cameracone.mdl");
}

void CDecoreCamFlare::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/cameracone.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	UTIL_SetOrigin(pev, pev->origin);
	MonsterInit(); // retail vtable slot 0x10c = CBaseMonster::MonsterInit
	pev->takedamage = DAMAGE_NO;
	pev->rendermode = kRenderTransAdd; // retail @0x100cb160: rendermode 5
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 30; // subtle increase while keeping the eye dominant
}

class CDecoreCam : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void RunAI() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	void ToggleSprite(CSprite* pSprite);
	void TrackTarget(const Vector& vecTarget);
	void UpdateFlare();

	CSprite* m_pLed = nullptr;
	CSprite* m_pEye = nullptr;
	CBaseEntity* m_pFlare = nullptr;
	float m_flTrackYaw = 0.0f;
	float m_flTrackPitch = 0.0f;
	float m_flNextEyeLight = 0.0f;
};
LINK_ENTITY_TO_CLASS(decore_cam, CDecoreCam);

TYPEDESCRIPTION CDecoreCam::m_SaveData[] =
	{
		DEFINE_FIELD(CDecoreCam, m_pLed, FIELD_CLASSPTR),
		DEFINE_FIELD(CDecoreCam, m_pEye, FIELD_CLASSPTR),
		DEFINE_FIELD(CDecoreCam, m_pFlare, FIELD_CLASSPTR),
		DEFINE_FIELD(CDecoreCam, m_flTrackYaw, FIELD_FLOAT),
		DEFINE_FIELD(CDecoreCam, m_flTrackPitch, FIELD_FLOAT),
		DEFINE_FIELD(CDecoreCam, m_flNextEyeLight, FIELD_TIME),
	};

IMPLEMENT_SAVERESTORE(CDecoreCam, CBaseMonster);

bool CDecoreCam::KeyValue(KeyValueData* pkvd)
{
	// Retail FUN_100cba70 handles the "frame" key for CDecore-derived entities.
	if (FStrEq(pkvd->szKeyName, "frame"))
	{
		pev->frame = atof(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

void CDecoreCam::Precache()
{
	PrecacheModel("models/camera.mdl");
	PrecacheModel("sprites/camled.spr");
	PrecacheModel("sprites/cameye.spr");
	PrecacheModel("models/cameracone.mdl");
	UTIL_PrecacheOther("decore_camflare");
}

void CDecoreCam::Spawn()
{
	// Retail CRustCamera::Spawn @ 0x100cb300 (fresh decompile 2026-10-31):
	// vtable slot 0x10c is CBaseMonster::MonsterInit (0x1002af50), called after
	// view_ofs/FOV and before takedamage; LED (attachment 1) stays visible, the eye sprite
	// (attachment 3) and the cone flare start hidden (EF_NODRAW).
	Precache();
	SET_MODEL(ENT(pev), "models/camera.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->view_ofs = Vector(0, 0, -8);
	m_flFieldOfView = -0.7f;
	UTIL_SetOrigin(pev, pev->origin);
	MonsterInit();
	pev->takedamage = DAMAGE_NO;

	m_pLed = CSprite::SpriteCreate("sprites/camled.spr", pev->origin, true);
	if (m_pLed)
	{
		m_pLed->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
		m_pLed->SetAttachment(edict(), 1);
		m_pLed->SetScale(0.2f);
	}

	m_pFlare = CBaseEntity::Create("decore_camflare", pev->origin, pev->angles, edict());
	if (m_pFlare)
	{
		m_pFlare->pev->renderamt = 30;
		m_pFlare->pev->effects |= EF_NODRAW;
	}

	// Spawn the eye after the flare so it is submitted later when their origins
	// coincide. Keep normal additive sprite sizing; kRenderGlow greatly enlarges
	// this animated eye and makes its rings obscure the camera face.
	m_pEye = CSprite::SpriteCreate("sprites/cameye.spr", pev->origin, true);
	if (m_pEye)
	{
		// Slightly soften the sprite's hard palette edges and gently vary its
		// brightness; cameye.spr already animates its ten frames at 9 fps.
		m_pEye->SetTransparency(kRenderTransAdd, 255, 255, 255, 220, kRenderFxPulseSlow);
		m_pEye->SetAttachment(edict(), 3);
		m_pEye->SetScale(0.3f);
		m_pEye->pev->framerate = 9.0f;
		m_pEye->pev->spawnflags = 0;
		// SpriteCreate(true) called TurnOn before framerate was set, so it did
		// not schedule AnimateThink. Start it now; hiding via EF_NODRAW keeps
		// the server think alive, and Use can reveal the current animated frame.
		m_pEye->TurnOn();
		m_pEye->pev->effects |= EF_NODRAW;
	}

	m_flTrackYaw = 0.0f;
	m_flTrackPitch = 0.0f;
	m_flNextEyeLight = 0.0f;
	SetBoneController(0, 0.0f);
	SetBoneController(1, 0.0f);
}

// Debug only (dbg_camtrack 1, see client.cpp): force player 1 as the target.
extern bool g_bDbgCamTrack;

void CDecoreCam::TrackTarget(const Vector& vecTarget)
{
	const Vector vecDirection = vecTarget - pev->origin;
	if (vecDirection.Length() < 0.001f)
		return;

	const float flHorizontalDistance = sqrtf(vecDirection.x * vecDirection.x + vecDirection.y * vecDirection.y);
	const float flYaw = UTIL_VecToYaw(vecDirection);
	const float flPitch = atan2f(vecDirection.z, flHorizontalDistance) * (180.0f / 3.14159265358979323846f);
	pev->ideal_yaw = UTIL_AngleMod(flYaw);

	// Retail TrackTarget (FUN_100caf80) advances each controller by at most
	// 10 degrees per RunAI call. Controller 0 is world yaw relative to the
	// body's current yaw; controller 1 is the model's inverted pitch axis.
	float flDelta = UTIL_AngleDiff(flYaw, m_flTrackYaw + pev->angles.y);
	if (flDelta > 10.0f)
		flDelta = 10.0f;
	else if (flDelta < -10.0f)
		flDelta = -10.0f;
	m_flTrackYaw = UTIL_AngleMod(m_flTrackYaw + flDelta);
	SetBoneController(0, m_flTrackYaw);

	// Keep pitch in a signed, non-wrapping range. AngleMod(negative pitch)
	// turns it into ~360 degrees; feeding that back caused the vertical axis
	// to flip around the controller's ±45-degree limits.
	const float flDesiredPitch = flPitch;
	flDelta = flDesiredPitch - (m_flTrackPitch + pev->angles.x);
	if (flDelta > 10.0f)
		flDelta = 10.0f;
	else if (flDelta < -10.0f)
		flDelta = -10.0f;
	m_flTrackPitch = V_min(45.0f, V_max(-45.0f, m_flTrackPitch + flDelta));
	SetBoneController(1, -m_flTrackPitch);
	ChangeYaw(40);
}


void CDecoreCam::UpdateFlare()
{
	// Keep the child cone's tip at the camera and its base around the eye sprite.
	if (m_pFlare)
	{
		Vector vecEye, vecAngles;
		GetAttachment(3, vecEye, vecAngles);
		const Vector d = vecEye - pev->origin;
		const float flDist = d.Length();
		const Vector dir = d.Normalize();
		Vector vecDirAngles = UTIL_VecToAngles(dir);
		UTIL_SetOrigin(m_pFlare->pev, pev->origin);
		m_pFlare->pev->angles = vecDirAngles;
		m_pFlare->pev->scale = (flDist > 1.0f ? flDist : 1.0f) / 55.0f;
	}
}

void CDecoreCam::RunAI()
{
	CBaseMonster::RunAI();

	CBaseEntity* pTarget = nullptr;
	if (!g_bDbgCamTrack && !FStringNull(pev->target))
	{
		pTarget = UTIL_FindEntityByTargetname(nullptr, STRING(pev->target));
		if (pTarget && !pTarget->IsAlive())
			pTarget = nullptr;
	}
	if (g_bDbgCamTrack || !pTarget)
		pTarget = UTIL_PlayerByIndex(1);
	if (!pTarget)
		pTarget = UTIL_GetLocalPlayer();

	if (pTarget && pTarget->IsAlive())
	{
		// CBasePlayer::BodyTarget deliberately adds RANDOM_FLOAT to view_ofs
		// for weapon aim variation. Sampling it every RunAI tick made the
		// camera pitch jitter; an eye point is stable and matches its purpose.
		const Vector vecAimTarget = pTarget->IsPlayer() ? pTarget->EyePosition() : pTarget->BodyTarget(pev->origin);
		TrackTarget(vecAimTarget);
	}

	UpdateFlare();

	// Match the visible Xash test light: full-intensity red with a 180-unit
	// radius. Refresh it while the eye sprite is visible so the world light
	// follows the camera; it expires shortly after the sprite is hidden.
	if (m_pEye && !(m_pEye->pev->effects & EF_NODRAW) && gpGlobals->time >= m_flNextEyeLight)
	{
		Vector vecEye, vecAngles;
		GetAttachment(3, vecEye, vecAngles);
		UTIL_DynamicLight(vecEye, 180.0f, 255, 0, 0, 0.2f, 0.0f);
		m_flNextEyeLight = gpGlobals->time + 0.1f;
	}
}

void CDecoreCam::ToggleSprite(CSprite* pSprite)
{
	if (!pSprite)
		return;

	pSprite->pev->effects ^= EF_NODRAW;
}

void CDecoreCam::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Retail ToggleToOn @0x100cb910 toggles EF_NODRAW on the flare (+0x278) and
	// the eye sprite (+0x280); the LED (+0x27c) is not touched. USE_TYPE is ignored.
	if (m_pFlare)
		m_pFlare->pev->effects ^= EF_NODRAW;
	ToggleSprite(m_pEye);
}


//=========================================================
// item_armor / item_gascan (+ item_gastank alias) - CITY3B's remaining
// gaps. Simple CItem-style pickups, following the exact structural
// pattern of the SDK's own item_healthkit (healthkit.cpp).
//
// Decompiled from gunman.dll: LINK @0x10024510/0x100248a0, Spawn
// @0x10024570/0x10024780, Precache @0x100245a0/0x100247b0, shared
// constructor helper @0x10023f30 (sets a common CItem-like vtable,
// object size 0x24 - too small to carry class-specific state, so
// neither class has its own keyvalues, matching the FGD's empty `[]`).
// The pickup callbacks were subsequently recovered as
// item_armor::MyTouch @0x10024600 and item_gascan::MyTouch @0x100247d0:
// armor sets armorvalue to max(current, 100) and deliberately succeeds
// even when it changes nothing. Gascan's only confirmed extra behavior is
// an opaque per-player flag gate/set at retail offsets 0x278/0x279; its
// consumer is still unresolved, so no invented vehicle-refill behavior is
// attached here. See findings/entities/item_lifecycle_retail_batch_2026-09-09.md.
//
// Retail CItem and CBasePlayerAmmo share a Gunman-specific green glow
// lifecycle and the CItem key nopickupsound.  These belong to the base
// classes because every retail user of their shared spawn tails inherits
// them; see findings/entities/item_lifecycle_retail_batch_2026-09-09.md.
//=========================================================
class CGunmanItemArmor : public CItem
{
public:
	void Spawn() override;
	void Precache() override;
	bool MyTouch(CBasePlayer* pPlayer) override;
};
LINK_ENTITY_TO_CLASS(item_armor, CGunmanItemArmor);

void CGunmanItemArmor::Precache()
{
	PrecacheModel("models/w_armor.mdl");
	PrecacheSound("items/gunpickup2.wav");
}

void CGunmanItemArmor::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_armor.mdl");
	CItem::Spawn();
}

bool CGunmanItemArmor::MyTouch(CBasePlayer* pPlayer)
{
	pPlayer->pev->armorvalue = V_max(pPlayer->pev->armorvalue, (float)MAX_NORMAL_BATTERY);
	if (ShouldPlayPickupSound())
		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM);

	int pct = (int)(pPlayer->pev->armorvalue * 100.0f * (1.0f / MAX_NORMAL_BATTERY) + 0.5f);
	pct = pct / 5;
	if (pct > 0)
		pct--;

	char szCharge[64];
	sprintf(szCharge, "!HEV_%1dP", pct);
	pPlayer->SetSuitUpdate(szCharge, false, SUIT_NEXT_IN_30SEC);

	return true;
}

class CGunmanItemGasCan : public CItem
{
public:
	void Spawn() override;
	void Precache() override;
	bool MyTouch(CBasePlayer* pPlayer) override;
};
LINK_ENTITY_TO_CLASS(item_gascan, CGunmanItemGasCan);
// item_gastank shares item_gascan's exact vtable (confirmed identical
// across all 20 checked slots, see findings/entities/misc_gunman_custom_entities.md)
// but has 0 map placements project-wide - aliased for completeness only.
LINK_ENTITY_TO_CLASS(item_gastank, CGunmanItemGasCan);

void CGunmanItemGasCan::Precache()
{
	PrecacheModel("models/gastank.mdl");
	PrecacheSound("Tank/gaspickup.wav");
}

void CGunmanItemGasCan::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/gastank.mdl");
	CItem::Spawn();
}

bool CGunmanItemGasCan::MyTouch(CBasePlayer* pPlayer)
{
	if (ShouldPlayPickupSound())
		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "Tank/gaspickup.wav", 1, ATTN_NORM);

	return true;
}


//=========================================================
// trigger_gunmanteleport - CGunmanTriggerTeleport. A Use-triggered
// (not Touch-triggered, unlike stock trigger_teleport), single-player-
// only instant teleport: teleports the player to the trigger's own
// placed position and zeroes velocity/basevelocity.
//
// Decompiled from gunman.dll: LINK @0x10059f60 (vtable 0x100f4b4c, no
// class-specific KeyValue/Save/Restore - inherits the generic
// CBaseEntity ones, matching the FGD's empty `[]`), and
// CGunmanTriggerTeleport::TeleportUse @0x10059fc0 (found directly
// named via the binary's own symbol table).
//=========================================================
class CGunmanTriggerTeleport : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT TeleportUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
};
LINK_ENTITY_TO_CLASS(trigger_gunmanteleport, CGunmanTriggerTeleport);

void CGunmanTriggerTeleport::Spawn()
{
	pev->classname = MAKE_STRING("trigger_gunmanteleport");
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	SetUse(&CGunmanTriggerTeleport::TeleportUse);
}

void CGunmanTriggerTeleport::TeleportUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBaseEntity* pPlayer = UTIL_PlayerByIndex(1);
	if (!pPlayer)
		return;

	pPlayer->pev->flags &= ~FL_DUCKING;
	UTIL_SetOrigin(pPlayer->pev, pev->origin);
	pPlayer->pev->basevelocity = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
}

//=========================================================
// entity_null - CNullEnt. Invisible marker entity that becomes a short-lived
// laser beam when its creator fills in the public fields (retail layout:
// +0x270 end point, +0x27c brightness, +0x280 start flag, +0x284 wide flag,
// +0x28c attached helper entity). No retail map places it; it is only
// created at runtime, so the creator is not part of this class.
// Retail (fresh decompile 2026-10-31): Spawn @0x1008e650, Precache @0x1008e710,
// WaitThink @0x1008e740, LazerThink @0x1008e780, RemoveThink @0x1008e8c0.
//
// Beam sprite: the retail sender (0x1008e7b0) writes the sprite index of the
// global 0x10142c98, which the gauss weapon's precache (0x100902a3) sets to
// "sprites/gaussfade.spr". Retail's entity Precache does not load it itself; the
// index is taken from the same precache call here (no behavior difference).
//=========================================================
#include "nullent.h"
LINK_ENTITY_TO_CLASS(entity_null, CNullEnt);

void CNullEnt::Precache()
{
	PrecacheModel("models/null.mdl");
	PrecacheModel("sprites/gaussbeam1.spr");
	PrecacheModel("sprites/gausspoof.spr");
	m_iBeamSprite = PrecacheModel("sprites/gaussfade.spr"); // retail global 0x10142c98
}

CNullEnt* CNullEnt::Create(const Vector& vecOrigin, const Vector& vecEnd, entvars_t* pevOwner, bool bWide)
{
	CNullEnt* pEnt = GetClassPtr((CNullEnt*)nullptr);
	pEnt->Spawn();
	UTIL_SetOrigin(pEnt->pev, vecOrigin);
	pEnt->pev->owner = pevOwner ? ENT(pevOwner) : nullptr;
	pEnt->m_vecBeamEnd = vecEnd;
	pEnt->m_bWideBeam = bWide;
	return pEnt;
}

void CNullEnt::Spawn()
{
	Precache();
	pev->takedamage = DAMAGE_NO;
	SET_MODEL(ENT(pev), "models/null.mdl");
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
	SetTouch(nullptr);
	SetThink(&CNullEnt::WaitThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_iBrightness = 90;
}

void CNullEnt::WaitThink()
{
	if (m_bStartLaser)
		SetThink(&CNullEnt::LazerThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CNullEnt::LazerThink()
{
	// TE_BEAMENTPOINT from attachment 1 of this entity to m_vecBeamEnd, 0.1 s life
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, m_vecBeamEnd);
	WRITE_BYTE(TE_BEAMENTPOINT);
	WRITE_SHORT(ENTINDEX(edict()) + 0x1000);
	WRITE_COORD(m_vecBeamEnd.x);
	WRITE_COORD(m_vecBeamEnd.y);
	WRITE_COORD(m_vecBeamEnd.z);
	WRITE_SHORT(m_iBeamSprite);
	WRITE_BYTE(0);	 // start frame
	WRITE_BYTE(0);	 // frame rate
	WRITE_BYTE(1);	 // life (0.1 s)
	WRITE_BYTE(m_bWideBeam ? 8 : 4); // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(255); // r
	WRITE_BYTE(255); // g
	WRITE_BYTE(255); // b
	WRITE_BYTE(m_iBrightness);
	WRITE_BYTE(0); // speed
	MESSAGE_END();

	CBaseEntity* pHelper = m_hHelper;
	if (pHelper)
		pHelper->pev->renderamt = m_iBrightness;

	m_iBrightness -= 10;
	if (m_iBrightness < 5)
		SetThink(&CNullEnt::RemoveThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CNullEnt::RemoveThink()
{
	CBaseEntity* pHelper = m_hHelper;
	if (pHelper)
		UTIL_Remove(pHelper);
	m_hHelper = nullptr;
	UTIL_Remove(this);
}


//=========================================================
// player_armor - not used by any retail map, missing so far. Retail
// Spawn @0x1003b9c0 (vtable @0x100f1f54, object 0x24 bytes): classname
// "player_armor", model models/player_armor.mdl, SOLID_NOT, MOVETYPE_FLY,
// DAMAGE_NO, effects 0, health 60, rendermode normal, renderamt 255,
// renderfx 0, velocity zero, size zero. Precache (slot 1 @0x1003b9b0)
// was not decompilable; the model is precached here.
//=========================================================
class CPlayerArmorProp : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override { PRECACHE_MODEL("models/player_armor.mdl"); }
};
LINK_ENTITY_TO_CLASS(player_armor, CPlayerArmorProp);

void CPlayerArmorProp::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("player_armor");
	SET_MODEL(ENT(pev), "models/player_armor.mdl");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = 60;
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 255;
	pev->velocity = g_vecZero;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}
