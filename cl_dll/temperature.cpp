//=========================================================
// CHudTemperature - siehe temperature.h.
// Umsetzung nach Retail-Dekompilat:
//  - Nachricht TempX: Waffen-ID und Hitze; gespeichert nur, wenn die Waffe der aktuellen Waffe entspricht und die Hitze > 0 ist.
//  - Anzahl Balken = (int)(Hitze * 1,16) (Konstante 0x10061430). Balken i: Breite 1, Hoehe 18, Farbe r = 255*i/58 (max. 255),
//    g = 255 (bei r > 254 fallend: (0x39c6 - 255*i)/58 + 240), ab Hitze 99: r = 255, g = 0.
//  - Warntexte: 51..75 "Warning", 76..98 "Danger", ab 99 "MALFUNCTION" (Textdateien text/warning.txt, danger.txt, malfunction.txt).
//  - Dial-Sprite "temperature" (Sprite-Liste hud.txt, Gruppe dials) auf Panel-Position (0, 25).
// Vereinfachung/offen: Textfarben und Textpositionen (TextPanel-Argumente x=20/25/0, y=0) sind aus dem Konstruktor abgeleitet,
// die Farben nicht belegt (weiss). Warnton "weapons/overheat.wav" wird hier nicht abgespielt (Ausloeser nicht belegt).
//=========================================================
#include "hud.h"
#include "cl_util.h"
#include "temperature.h"
#include "ammo.h"
#include "parsemsg.h"

DECLARE_MESSAGE(m_Temperature, TempX)

// Retail-Konstanten (Dekompilat): Panel-Position relativ zur Bildschirmmitte/-unterkante.
#define TEMP_PANEL_HALF_W 64 // 0x40
#define TEMP_PANEL_BOTTOM 65 // 0x41 vom unteren Rand
#define TEMP_BAR_OFFSET_X 54 // 0x36
#define TEMP_BAR_OFFSET_Y 33 // 0x21 vom unteren Rand
#define TEMP_BAR_HEIGHT 18   // 0x12

bool CHudTemperature::Init()
{
	HOOK_MESSAGE(TempX);
	m_iFlags = 0;
	gHUD.AddHudElem(this);
	return true;
}

bool CHudTemperature::VidInit()
{
	m_HUD_temperature = gHUD.GetSpriteIndex("temperature");
	m_iFlags |= HUD_ACTIVE;
	return true;
}

bool CHudTemperature::MsgFunc_TempX(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int iWeaponId = READ_BYTE();
	const int iHeat = READ_BYTE();

	// Retail (FUN_10023810): Hitze nur bei passender aktueller Waffe und Hitze > 0 uebernehmen.
	WEAPON* pWeapon = gHUD.m_Ammo.GetCurrentWeapon();
	if (pWeapon && iHeat > 0 && pWeapon->iId == iWeaponId)
	{
		m_iWeaponId = iWeaponId;
		m_iHeat = iHeat;
	}
	return true;
}

bool CHudTemperature::Draw(float flTime)
{
	// Retail (FUN_10029f30): ohne Waffe oder bei fremder Waffe nichts zeigen.
	WEAPON* pWeapon = gHUD.m_Ammo.GetCurrentWeapon();
	if (!pWeapon || pWeapon->iId != m_iWeaponId)
		return true;

	const int x0 = ScreenWidth / 2 - TEMP_BAR_OFFSET_X;
	const int y0 = ScreenHeight - TEMP_BAR_OFFSET_Y;
	const int px = ScreenWidth / 2 - TEMP_PANEL_HALF_W;
	const int py = ScreenHeight - TEMP_PANEL_BOTTOM;

	// Dial (Sprite "temperature") auf Panel-Position (0, 25).
	if (m_HUD_temperature >= 0)
	{
		SPR_Set(gHUD.GetSprite(m_HUD_temperature), 255, 255, 255);
		SPR_DrawHoles(0, px, py + 25, &gHUD.GetSpriteRect(m_HUD_temperature));
	}

	// Warntexte nach Hitzestufe (nur eine Stufe gleichzeitig).
	if (m_iHeat >= 51 && m_iHeat < 76)
		gHUD.DrawHudString(px + 20, py, ScreenWidth, "Warning", 255, 255, 255);
	else if (m_iHeat >= 76 && m_iHeat < 99)
		gHUD.DrawHudString(px + 25, py, ScreenWidth, "Danger", 255, 255, 255);
	else if (m_iHeat >= 99)
		gHUD.DrawHudString(px, py, ScreenWidth, "MALFUNCTION", 255, 255, 255);

	// Balken: Anzahl = (int)(Hitze * 1.16).
	const int iBars = (int)(m_iHeat * 1.16);
	for (int i = 0; i < iBars; i++)
	{
		int r = (i * 255) / 58;
		int g = 255;
		if (r > 254)
		{
			r = 255;
			g = ((0x39c6 - 255 * i) / 58) + 0xf0;
		}
		if (m_iHeat >= 99)
		{
			r = 255;
			g = 0;
		}
		FillRGBA(x0 + i, y0, 1, TEMP_BAR_HEIGHT, r, g, 0, 255);
	}
	return true;
}
