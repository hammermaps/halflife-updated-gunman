/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// battery.cpp
//
// implementation of CHudBattery class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>

DECLARE_MESSAGE(m_Battery, Battery)

bool CHudBattery::Init()
{
	m_iBat = 0;
	m_fFade = 0;
	m_iFlags = 0;
	m_pCvarSmallHud = gEngfuncs.pfnGetCvarPointer("hud_smallhud");

	HOOK_MESSAGE(Battery);

	gHUD.AddHudElem(this);

	return true;
}


bool CHudBattery::VidInit()
{
	int HUD_suit_empty = gHUD.GetSpriteIndex("suit_empty");
	int HUD_suit_full = gHUD.GetSpriteIndex("suit_full");

	m_hSprite1 = m_hSprite2 = 0; // delaying get sprite handles until we know the sprites are loaded
	m_prc1 = &gHUD.GetSpriteRect(HUD_suit_empty);
	m_prc2 = &gHUD.GetSpriteRect(HUD_suit_full);
	m_iHeight = m_prc2->bottom - m_prc1->top;
	m_fFade = 0;
	m_HUD_armor_graph = gHUD.GetSpriteIndex("armor_graph");
	return true;
}

bool CHudBattery::MsgFunc_Battery(const char* pszName, int iSize, void* pbuf)
{
	m_iFlags |= HUD_ACTIVE;

	BEGIN_READ(pbuf, iSize);
	int x = READ_SHORT();

	if (x != m_iBat)
	{
		m_fFade = FADE_TIME;
		m_iBat = x;
	}

	return true;
}


bool CHudBattery::Draw(float flTime)
{
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) != 0)
		return true;

	// Retail client.dll CHudBattery::Draw (0x10027180-0x10027304) zeichnet
	// KEINE suit_empty/suit_full-Grafik: links im Blechpanel sitzt der
	// armor_graph-Balken (x=27, y=Bildschirmhoehe-112), die Zahl steht in
	// Gruen (0,255,0) direkt unter der Health-Zahl (x=164 gross / 35 klein,
	// y=Bildschirmhoehe-63). Vorbild ist der Dekompilat-Ablauf:
	//   Balken: rect.top bleibt, rect.bottom = clamp(armor, 1, 96)
	//   Zahl:   DrawHudNumber(x, y, DHN_3DIGITS|DHN_DRAWZERO, armor, 0,255,0)
	// Eine Suit-Bedingung kennt Retail hier nicht (nachgewiesen im
	// Dekompilat: der Zweig beginnt direkt mit HIDEHUD_HEALTH).

	int r = 0, g = 255, b = 0;
	int a;

	// Fade wie im SDK-Stock (nur Helligkeit des Zahlenwerts)
	if (0 != m_fFade)
	{
		if (m_fFade > FADE_TIME)
			m_fFade = FADE_TIME;

		m_fFade -= (gHUD.m_flTimeDelta * 20);
		if (m_fFade <= 0)
		{
			a = 128;
			m_fFade = 0;
		}

		a = MIN_ALPHA + (m_fFade / FADE_TIME) * 128;
	}
	else
		a = MIN_ALPHA;

	ScaleColors(r, g, b, a);

	bool bLargeHud = ScreenWidth >= 641 && (m_pCvarSmallHud == nullptr || m_pCvarSmallHud->value == 0);
	const int x = bLargeHud ? 164 : 35;
	const int y = ScreenHeight - 77;

	// Zahl 0..100
	gHUD.DrawHudNumber(x, y, DHN_3DIGITS | DHN_DRAWZERO, V_min(100, m_iBat), r, g, b);

	// Balken direkt unter der Health-Zahl: armor.spr ist ein horizontaler
	// Farbverlauf (rot -> gruen, linke 96x25 des 128x32-Sprites). Retail
	// (0x100272b7-0x100272f7) kuerzt das Rechteck-Feld an +0x0c; in
	// HL1-wrect_t ({left,right,top,bottom}) ist das der rechte Rand:
	// right = clamp(armor, 1, 96). Der Balken waechst also von links nach
	// rechts, die Farbe laeuft mit.
	if (m_HUD_armor_graph >= 0)
	{
		Rect rc = gHUD.GetSpriteRect(m_HUD_armor_graph);
		const int iFullWidth = rc.right - rc.left;
		const int iArmor = V_min(100, V_max(0, m_iBat));

		rc.right = rc.left + (iFullWidth * iArmor) / 100;
		// Flacher Balken, damit er nicht in die Health-Ziffern laeuft
		rc.bottom = rc.top + 10;

		if (rc.right > rc.left)
		{
			SPR_Set(gHUD.GetSprite(m_HUD_armor_graph), 255, 255, 255);
			SPR_DrawAdditive(0, x, ScreenHeight - 58, &rc);
		}
	}

	return true;
}
