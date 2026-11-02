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
// Health.cpp
//
// implementation of CHudHealth class
//

#include "stdio.h"
#include "stdlib.h"

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_int.h"
#include <string.h>


DECLARE_MESSAGE(m_Health, Health)
DECLARE_MESSAGE(m_Health, Damage)

#define PAIN_NAME "sprites/%d_pain.spr"
#define DAMAGE_NAME "sprites/%d_dmg.spr"

int giDmgHeight, giDmgWidth;

int giDmgFlags[NUM_DMG_TYPES] =
	{
		DMG_POISON,
		DMG_ACID,
		DMG_FREEZE | DMG_SLOWFREEZE,
		DMG_DROWN,
		DMG_BURN | DMG_SLOWBURN,
		DMG_NERVEGAS,
		DMG_RADIATION,
		DMG_SHOCK,
		DMG_CALTROP,
		DMG_TRANQ,
		DMG_CONCUSS,
		DMG_HALLUC};

bool CHudHealth::Init()
{
	HOOK_MESSAGE(Health);
	HOOK_MESSAGE(Damage);
	m_iHealth = 100;
	m_fFade = 0;
	m_iFlags = 0;
	m_bitsDamage = 0;
	m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;
	giDmgHeight = 0;
	giDmgWidth = 0;
	m_flPulseTime = 0;
	m_iPulseOffset = 0;
	m_hMetalHud = 0;
	m_hMetalHudSmall = 0;
	m_pCvarSmallHud = gEngfuncs.pfnGetCvarPointer("hud_smallhud");

	memset(m_dmg, 0, sizeof(DAMAGE_IMAGE) * NUM_DMG_TYPES);


	gHUD.AddHudElem(this);
	return true;
}

void CHudHealth::Reset()
{
	// make sure the pain compass is cleared when the player respawns
	m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;


	// force all the flashing damage icons to expire
	m_bitsDamage = 0;
	for (int i = 0; i < NUM_DMG_TYPES; i++)
	{
		m_dmg[i].fExpire = 0;
	}
}

bool CHudHealth::VidInit()
{
	m_hSprite = 0;

	m_HUD_dmg_bio = gHUD.GetSpriteIndex("dmg_bio") + 1;
	m_HUD_cross = gHUD.GetSpriteIndex("cross");
	m_HUD_health_pulse = gHUD.GetSpriteIndex("health_pulse");
	m_hMetalHud = SPR_Load("sprites/metalhud.spr");
	m_hMetalHudSmall = SPR_Load("sprites/metalhud_small.spr");

	giDmgHeight = gHUD.GetSpriteRect(m_HUD_dmg_bio).right - gHUD.GetSpriteRect(m_HUD_dmg_bio).left;
	giDmgWidth = gHUD.GetSpriteRect(m_HUD_dmg_bio).bottom - gHUD.GetSpriteRect(m_HUD_dmg_bio).top;
	return true;
}

bool CHudHealth::MsgFunc_Health(const char* pszName, int iSize, void* pbuf)
{
	// TODO: update local health data
	BEGIN_READ(pbuf, iSize);
	int x = READ_SHORT();

	m_iFlags |= HUD_ACTIVE;

	// Only update the fade if we've changed health
	if (x != m_iHealth)
	{
		m_fFade = FADE_TIME;
		m_iHealth = x;
	}

	return true;
}


bool CHudHealth::MsgFunc_Damage(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int armor = READ_BYTE();	   // armor
	int damageTaken = READ_BYTE(); // health
	long bitsDamage = READ_LONG(); // damage bits

	Vector vecFrom;

	for (int i = 0; i < 3; i++)
		vecFrom[i] = READ_COORD();

	UpdateTiles(gHUD.m_flTime, bitsDamage);

	// Actually took damage?
	if (damageTaken > 0 || armor > 0)
		CalcDamageDirection(vecFrom);

	return true;
}


// Returns back a color from the
// Green <-> Yellow <-> Red ramp
void CHudHealth::GetPainColor(int& r, int& g, int& b)
{
	int iHealth = m_iHealth;

	if (iHealth > 25)
		iHealth -= 25;
	else if (iHealth < 0)
		iHealth = 0;
#if 0
	g = iHealth * 255 / 100;
	r = 255 - g;
	b = 0;
#else
	if (m_iHealth > 25)
	{
		UnpackRGB(r, g, b, RGB_YELLOWISH);
	}
	else
	{
		r = 250;
		g = 0;
		b = 0;
	}
#endif
}

// Gunman's health digit/cross color (RE-confirmed): cyan above 25 HP, red at
// or below 25 - deliberately separate from GetPainColor()'s yellow/red ramp,
// which still drives the unrelated screen-edge pain-direction flash.
void CHudHealth::GetHealthColor(int& r, int& g, int& b)
{
	if (m_iHealth <= 25)
	{
		r = 250;
		g = 0;
		b = 0;
	}
	else
	{
		r = 0;
		g = 255;
		b = 255;
	}
}

// Retail client.dll 0x100295d1-0x10029757: the five sprite frames represent
// health ranges.  Motion comes from splitting the 80-pixel source rectangle
// at a time-driven offset, not from cycling the frames.
void CHudHealth::DrawPulseGraph()
{
	if (m_HUD_health_pulse < 0)
		return;

	int frame;
	if (m_iHealth <= 0)
		frame = 4;
	else if (m_iHealth <= 25)
		frame = 3;
	else if (m_iHealth <= 50)
		frame = 2;
	else if (m_iHealth <= 80)
		frame = 1;
	else
		frame = 0;

	Rect leading = gHUD.GetSpriteRect(m_HUD_health_pulse);
	Rect trailing = leading;
	const int width = leading.right - leading.left;
	const int split = leading.right - m_iPulseOffset;
	leading.right = split != leading.left ? split : leading.left + 1;
	trailing.left = split != trailing.right ? split : trailing.right - 1;

	SPR_Set(gHUD.GetSprite(m_HUD_health_pulse), 255, 255, 255);
	const int y = ScreenHeight - 74;
	// Korrektur 2026-10-30 (Retail frisch dekompiliert, client.dll
	// 0x10029400, Aufrufe @0x100296xx): das RECHTE Teilstueck [split,right)
	// kommt an x=36, das LINKE Teilstueck [left,split) an x=36+Versatz. Das
	// Bild wird dadurch zyklisch immer nach rechts geschoben und beginnt bei
	// Versatz 80 von vorn. Mit vertauschten Positionen (zuvor) ueberlappten
	// sich beide Teile und die Grafik "schwamm" hin und her.
	SPR_DrawAdditive(frame, 36, y, &trailing);
	SPR_DrawAdditive(frame, 36 + m_iPulseOffset, y, &leading);

	const double now = gEngfuncs.GetClientTime();
	if (m_flPulseTime == 0 || now < m_flPulseTime)
	{
		m_flPulseTime = now;
		return;
	}

	int steps = static_cast<int>((now - m_flPulseTime) * 50.0);
	while (steps-- > 0)
	{
		m_flPulseTime = now;
		if (++m_iPulseOffset >= width)
			m_iPulseOffset = 0;
	}
}

void CHudHealth::DrawMetalHudFallback(bool largeHud)
{
	HSPRITE sprite = largeHud ? m_hMetalHud : m_hMetalHudSmall;
	if (sprite == 0)
		return;

	SPR_Set(sprite, 255, 255, 255);
	SPR_DrawHoles(0, largeHud ? 0 : 1, ScreenHeight - 128, nullptr);
}

bool CHudHealth::Draw(float flTime)
{
	int r, g, b;
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) != 0 || 0 != gEngfuncs.IsSpectateOnly())
	{
		VGui_UpdateGunmanHud(false, false);
		return true;
	}

	if (0 == m_hSprite)
		m_hSprite = LoadSprite(PAIN_NAME);

	// Retail draws transient damage indicators before the life-support panel.
	DrawDamage(flTime);
	DrawPain(flTime);

	GetHealthColor(r, g, b);

	bool bLargeHud = ScreenWidth >= 641 && (m_pCvarSmallHud == nullptr || m_pCvarSmallHud->value == 0);
	const bool hasVGuiHud = VGui_UpdateGunmanHud(true, bLargeHud);
	const int x = bLargeHud ? 164 : 35;
	const int y = ScreenHeight - 99;
	if (m_iHealth > 99)
	{
		const int index = gHUD.GetSpriteIndex("number_10");
		if (index >= 0)
		{
			SPR_Set(gHUD.GetSprite(index), r, g, b);
			SPR_DrawAdditive(0, x - 3, y, &gHUD.GetSpriteRect(index));
		}
	}
	else
		gHUD.DrawHudNumber(x, y, DHN_3DIGITS, m_iHealth, r, g, b);

	if (bLargeHud)
		DrawPulseGraph();

	// Retail's VGUI casing is painted over the HUD primitives and masks the
	// deliberately wrapped pulse segments.  Preserve that ordering when the
	// Xash3D sprite fallback is used.
	if (!hasVGuiHud)
		DrawMetalHudFallback(bLargeHud);

	return true;
}

void CHudHealth::CalcDamageDirection(Vector vecFrom)
{
	Vector forward, right, up;
	float side, front;
	Vector vecOrigin, vecAngles;

	if (vecFrom == g_vecZero)
	{
		m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;
		return;
	}


	memcpy(vecOrigin, gHUD.m_vecOrigin, sizeof(Vector));
	memcpy(vecAngles, gHUD.m_vecAngles, sizeof(Vector));


	VectorSubtract(vecFrom, vecOrigin, vecFrom);

	float flDistToTarget = vecFrom.Length();

	vecFrom = vecFrom.Normalize();
	AngleVectors(vecAngles, forward, right, up);

	front = DotProduct(vecFrom, right);
	side = DotProduct(vecFrom, forward);

	if (flDistToTarget <= 50)
	{
		m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 1;
	}
	else
	{
		if (side > 0)
		{
			if (side > 0.3)
				m_fAttackFront = V_max(m_fAttackFront, side);
		}
		else
		{
			float f = fabs(side);
			if (f > 0.3)
				m_fAttackRear = V_max(m_fAttackRear, f);
		}

		if (front > 0)
		{
			if (front > 0.3)
				m_fAttackRight = V_max(m_fAttackRight, front);
		}
		else
		{
			float f = fabs(front);
			if (f > 0.3)
				m_fAttackLeft = V_max(m_fAttackLeft, f);
		}
	}
}

bool CHudHealth::DrawPain(float flTime)
{
	if (!(0 != m_fAttackFront || 0 != m_fAttackRear || 0 != m_fAttackLeft || 0 != m_fAttackRight))
		return true;

	int r, g, b;
	int x, y, a, shade;

	// TODO:  get the shift value of the health
	a = 255; // max brightness until then

	float fFade = gHUD.m_flTimeDelta * 2;

	// SPR_Draw top
	if (m_fAttackFront > 0.4)
	{
		GetPainColor(r, g, b);
		shade = a * V_max(m_fAttackFront, 0.5f);
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b);

		x = ScreenWidth / 2 - SPR_Width(m_hSprite, 0) / 2;
		y = ScreenHeight / 2 - SPR_Height(m_hSprite, 0) * 3;
		SPR_DrawAdditive(0, x, y, NULL);
		m_fAttackFront = V_max(0.0f, m_fAttackFront - fFade);
	}
	else
		m_fAttackFront = 0;

	if (m_fAttackRight > 0.4)
	{
		GetPainColor(r, g, b);
		shade = a * V_max(m_fAttackRight, 0.5f);
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b);

		x = ScreenWidth / 2 + SPR_Width(m_hSprite, 1) * 2;
		y = ScreenHeight / 2 - SPR_Height(m_hSprite, 1) / 2;
		SPR_DrawAdditive(1, x, y, NULL);
		m_fAttackRight = V_max(0.0f, m_fAttackRight - fFade);
	}
	else
		m_fAttackRight = 0;

	if (m_fAttackRear > 0.4)
	{
		GetPainColor(r, g, b);
		shade = a * V_max(m_fAttackRear, 0.5f);
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b);

		x = ScreenWidth / 2 - SPR_Width(m_hSprite, 2) / 2;
		y = ScreenHeight / 2 + SPR_Height(m_hSprite, 2) * 2;
		SPR_DrawAdditive(2, x, y, NULL);
		m_fAttackRear = V_max(0.0f, m_fAttackRear - fFade);
	}
	else
		m_fAttackRear = 0;

	if (m_fAttackLeft > 0.4)
	{
		GetPainColor(r, g, b);
		shade = a * V_max(m_fAttackLeft, 0.5f);
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b);

		x = ScreenWidth / 2 - SPR_Width(m_hSprite, 3) * 3;
		y = ScreenHeight / 2 - SPR_Height(m_hSprite, 3) / 2;
		SPR_DrawAdditive(3, x, y, NULL);

		m_fAttackLeft = V_max(0.0f, m_fAttackLeft - fFade);
	}
	else
		m_fAttackLeft = 0;

	return true;
}

bool CHudHealth::DrawDamage(float flTime)
{
	int r, g, b, a;
	DAMAGE_IMAGE* pdmg;

	if (0 == m_bitsDamage)
		return true;

	UnpackRGB(r, g, b, RGB_YELLOWISH);

	a = (int)(fabs(sin(flTime * 2)) * 256.0);

	ScaleColors(r, g, b, a);

	// Draw all the items
	int i;
	for (i = 0; i < NUM_DMG_TYPES; i++)
	{
		if ((m_bitsDamage & giDmgFlags[i]) != 0)
		{
			pdmg = &m_dmg[i];
			SPR_Set(gHUD.GetSprite(m_HUD_dmg_bio + i), r, g, b);
			SPR_DrawAdditive(0, pdmg->x, pdmg->y, &gHUD.GetSpriteRect(m_HUD_dmg_bio + i));
		}
	}


	// check for bits that should be expired
	for (i = 0; i < NUM_DMG_TYPES; i++)
	{
		DAMAGE_IMAGE* pdmg = &m_dmg[i];

		if ((m_bitsDamage & giDmgFlags[i]) != 0)
		{
			pdmg->fExpire = V_min(flTime + DMG_IMAGE_LIFE, pdmg->fExpire);

			if (pdmg->fExpire <= flTime // when the time has expired
				&& a < 40)				// and the flash is at the low point of the cycle
			{
				pdmg->fExpire = 0;

				int y = pdmg->y;
				pdmg->x = pdmg->y = 0;

				// move everyone above down
				for (int j = 0; j < NUM_DMG_TYPES; j++)
				{
					pdmg = &m_dmg[j];
					if (0 != pdmg->y && (pdmg->y < y))
						pdmg->y += giDmgHeight;
				}

				m_bitsDamage &= ~giDmgFlags[i]; // clear the bits
			}
		}
	}

	return true;
}


void CHudHealth::UpdateTiles(float flTime, long bitsDamage)
{
	DAMAGE_IMAGE* pdmg;

	// Which types are new?
	long bitsOn = ~m_bitsDamage & bitsDamage;

	for (int i = 0; i < NUM_DMG_TYPES; i++)
	{
		pdmg = &m_dmg[i];

		// Is this one already on?
		if ((m_bitsDamage & giDmgFlags[i]) != 0)
		{
			pdmg->fExpire = flTime + DMG_IMAGE_LIFE; // extend the duration
			if (0 == pdmg->fBaseline)
				pdmg->fBaseline = flTime;
		}

		// Are we just turning it on?
		if ((bitsOn & giDmgFlags[i]) != 0)
		{
			// put this one at the bottom
			pdmg->x = giDmgWidth / 8;
			pdmg->y = ScreenHeight - giDmgHeight * 2;
			pdmg->fExpire = flTime + DMG_IMAGE_LIFE;

			// move everyone else up
			for (int j = 0; j < NUM_DMG_TYPES; j++)
			{
				if (j == i)
					continue;

				pdmg = &m_dmg[j];
				if (0 != pdmg->y)
					pdmg->y -= giDmgHeight;
			}
			pdmg = &m_dmg[i];
		}
	}

	// damage bits are only turned on here;  they are turned off when the draw time has expired (in DrawDamage())
	m_bitsDamage |= bitsDamage;
}
