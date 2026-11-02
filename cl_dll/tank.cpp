#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

DECLARE_MESSAGE(m_Tank, Tank)
DECLARE_MESSAGE(m_Tank, TankBody)
DECLARE_MESSAGE(m_Tank, TurretPos)

bool CHudTankControl::Init()
{
	HOOK_MESSAGE(Tank);
	HOOK_MESSAGE(TankBody);
	HOOK_MESSAGE(TurretPos);
	gHUD.AddHudElem(this);
	m_iFlags = 0;
	m_pCvarTankHud = CVAR_CREATE("hud_tankhud", "1", FCVAR_ARCHIVE);
	return true;
}

bool CHudTankControl::VidInit()
{
	m_iBodyTL = gHUD.GetSpriteIndex("tank_bodytl");
	m_iBodyTR = gHUD.GetSpriteIndex("tank_bodytr");
	m_iBodyBL = gHUD.GetSpriteIndex("tank_bodybl");
	m_iBodyBR = gHUD.GetSpriteIndex("tank_bodybr");
	m_iBodyHitTL = gHUD.GetSpriteIndex("tank_bodyhittl");
	m_iBodyHitTR = gHUD.GetSpriteIndex("tank_bodyhittr");
	m_iBodyHitBL = gHUD.GetSpriteIndex("tank_bodyhitbl");
	m_iBodyHitBR = gHUD.GetSpriteIndex("tank_bodyhitbr");
	m_iTurret = gHUD.GetSpriteIndex("tank_turret");
	return true;
}

void CHudTankControl::Reset()
{
	m_iFlags &= ~HUD_ACTIVE;
	m_iBodyState = 0;
	m_iTurretFrame = 0;
	m_flBodyStateUntil = 0;
}

bool CHudTankControl::MsgFunc_Tank(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int state = READ_SHORT();
	if (state != 0)
		m_iFlags |= HUD_ACTIVE;
	else
		Reset();
	return true;
}

bool CHudTankControl::MsgFunc_TankBody(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iBodyState = READ_SHORT();
	m_flBodyStateUntil = gHUD.m_flTime + 0.25f;
	return true;
}

bool CHudTankControl::MsgFunc_TurretPos(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iTurretFrame = READ_SHORT() & 15;
	return true;
}

static void DrawTankSprite(int index, int x, int y, int frame = 0)
{
	if (index < 0)
		return;
	SPR_Set(gHUD.GetSprite(index), 255, 255, 255);
	SPR_DrawAdditive(frame, x, y, &gHUD.GetSpriteRect(index));
}

bool CHudTankControl::Draw(float flTime)
{
	if (!m_pCvarTankHud || m_pCvarTankHud->value == 0)
		return false;
	if (m_flBodyStateUntil < flTime)
		m_iBodyState = 0;

	const bool lowRes = ScreenWidth < 640;
	const int tile = lowRes ? 32 : 64;
	int x = 100;
	int y = 100;
	if (m_pCvarTankHud->value == 2)
	{
		x = tile;
		y = ScreenHeight - tile * 2;
	}

	const bool hitTop = m_iBodyState == 1;
	const bool hitBottom = m_iBodyState == 2;
	DrawTankSprite(hitTop ? m_iBodyHitTL : m_iBodyTL, x, y);
	DrawTankSprite(hitTop ? m_iBodyHitTR : m_iBodyTR, x + tile, y);
	DrawTankSprite(hitBottom ? m_iBodyHitBL : m_iBodyBL, x, y + tile);
	DrawTankSprite(hitBottom ? m_iBodyHitBR : m_iBodyBR, x + tile, y + tile);
	DrawTankSprite(m_iTurret, x + (lowRes ? 11 : 22), y + (lowRes ? 16 : 33), m_iTurretFrame);
	return true;
}
