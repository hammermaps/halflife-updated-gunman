//
// hud_zoom.cpp - retail CHudZoom (client.dll: Init 0x100217f0, VidInit 0x10021830, message handler 0x10021850,
// Reset 0x10021920, Draw 0x10021a70): scope zoom of the gauss pistol sniper mode.
//
// The server message "Zoom" (1 byte) controls it: 0 = reset, 1 = zoom in, 2 = freeze the zoom, anything else
// (3 = shot) = end the zoom (0.5 s after the end of the zoom animation, immediately while it is still running).
// The field of view runs from 65 down to 15 (200 degrees/s, 0.4 s start delay); the zoom_target sprite
// (scopezoom.spr, 3 frames) is drawn in the screen centre in green; the weapon's zoomed crosshair is activated.
// Simplification: retail Draw computes the mouse sensitivity from the cvar zoom_sensitivity_ratio and an additional
// factor of an unnamed global (+0xc of 0x1008c50c); here the stock SDK formula (IN_GetMouseSensitivity) is used.
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

DECLARE_MESSAGE(m_Zoom, Zoom);

namespace
{
constexpr int ZOOM_START_FOV = 65;		// 0x41
constexpr int ZOOM_TARGET_FOV = 15;		// +0x38 = 0xf
constexpr float ZOOM_FOV_SPEED = 200.0f; // 0x1006124c
constexpr double ZOOM_START_DELAY = 0.4; // 0x1005d240
constexpr double ZOOM_END_DELAY = 0.5;	// 0x1005b9c8
constexpr double ZOOM_STEP_TIME = 0.01;	// 0x1005d1f8
constexpr double ZOOM_FRAME_TIME = 0.1;	// 0x1005d188
}

bool CHudZoom::Init()
{
	HOOK_MESSAGE(Zoom);

	gHUD.AddHudElem(this);
	Reset();
	m_bAnimating = true;
	return true;
}

bool CHudZoom::VidInit()
{
	m_iSprite = gHUD.GetSpriteIndex("zoom_target");
	return true;
}

void CHudZoom::Reset()
{
	m_iCurFov = 0;
	m_flNextFovStep = 0;
	m_iFlags &= ~HUD_ACTIVE;
	m_iFrame = 0;
	m_flNextFrame = 0;
	m_flNextDraw = 0;
	m_flEndTime = 0;
	m_iTargetFov = ZOOM_TARGET_FOV;
	m_bAnimating = true;
	m_bDrawFlag = false;
	m_bReverse = false;

	const int iDefault = (int)CVAR_GET_FLOAT("default_fov");
	if (iDefault > 0 && m_bWasZoomed)
		gHUD.m_iFOV = iDefault;
	m_bWasZoomed = false;

	WEAPON* pWeapon = gHUD.m_Ammo.GetCurrentWeapon();
	if (pWeapon)
		SetCrosshair(pWeapon->hCrosshair, pWeapon->rcCrosshair, 255, 255, 255);
}

bool CHudZoom::MsgFunc_Zoom(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int iMode = READ_BYTE();
	const float flTime = gHUD.m_flTime;

	if (iMode == 0)
	{
		Reset();
	}
	else if (iMode == 1)
	{
		m_iCurFov = 0;
		m_flNextFovStep = 0;
		m_iFrame = 0;
		m_flNextFrame = 0;
		m_iTargetFov = ZOOM_TARGET_FOV;
		m_bDrawFlag = true;
		m_iFlags |= HUD_ACTIVE;
		m_bReverse = false;
		m_flNextFovStep = m_flNextFrame = flTime + (float)ZOOM_START_DELAY;
		m_flEndTime = 0;
		m_flNextDraw = 0;
	}
	else if (iMode == 2)
	{
		m_bAnimating = false;
	}
	else
	{
		// shot: end immediately while the animation is still running, else after the end delay
		m_flEndTime = m_bAnimating ? flTime : flTime + (float)ZOOM_END_DELAY;
	}
	return true;
}

bool CHudZoom::Draw(float flTime)
{
	WEAPON* pWeapon = gHUD.m_Ammo.GetCurrentWeapon();
	if (!pWeapon)
		return false;

	if (m_flEndTime != 0 && m_flEndTime < flTime)
	{
		Reset();
		return true;
	}

	m_bDrawFlag = true;
	if (m_iCurFov == 0 || m_iCurFov > ZOOM_START_FOV)
		m_iCurFov = ZOOM_START_FOV;

	if (m_flNextFovStep < flTime && m_bAnimating)
	{
		if (m_iCurFov > m_iTargetFov && !m_bReverse)
		{
			m_iCurFov = (int)((float)m_iCurFov - (flTime - m_flNextFovStep) * ZOOM_FOV_SPEED + 0.5f);
		}
		else if (m_iCurFov < m_iTargetFov && m_bReverse)
		{
			m_iCurFov += 2;
		}
		else
		{
			m_bAnimating = false;
		}
		m_flNextFovStep = flTime + (float)ZOOM_STEP_TIME;
	}

	if (m_flNextFrame < flTime && m_bAnimating)
	{
		if (!m_bReverse)
		{
			if (++m_iFrame > 2)
				m_iFrame = 0;
		}
		else if (--m_iFrame < 0)
		{
			m_iFrame = 2;
		}
		m_flNextFrame = flTime + (float)ZOOM_FRAME_TIME;
	}

	if (m_flNextDraw < flTime)
	{
		SetCrosshair(pWeapon->hZoomedCrosshair, pWeapon->rcZoomedCrosshair, 255, 255, 255);
		if (m_iSprite >= 0)
		{
			SPR_Set(gHUD.GetSprite(m_iSprite), 0, 255, 0);
			SPR_DrawAdditive(m_iFrame, ScreenWidth / 2 - 64, ScreenHeight / 2 - 64, &gHUD.GetSpriteRect(m_iSprite));
		}
	}

	// the engine takes the field of view from the client data (gHUD.m_iFOV)
	const int iDefault = (int)CVAR_GET_FLOAT("default_fov");
	gHUD.m_iFOV = m_iCurFov != 0 ? m_iCurFov : iDefault;
	m_bWasZoomed = true;
	gHUD.SetZoomSensitivity(m_iCurFov != 0 && iDefault > 0 ? (float)m_iCurFov / (float)iDefault : 0.0f);
	return true;
}
