// CHudDmlCross - Gunman-DML-Zielkreuz ("DmlCross"-Usermessage).
//
// Retail: HUD-Element an gHUD+0x754, Init 0x10001020, VidInit 0x10001060,
// Draw 0x100011c0, Reset 0x10001190, Handler 0x10001080.
//   Init     hookt "DmlCross" auf 0x10001000, AddHudElem, Timer = 6.0 s
//   VidInit  holt das Sprite "dmllock" (bei uns per SPR_Load, weil der Name in
//            unserer hud.txt fehlt - Retail nutzt gHUD.GetSpriteIndex)
//   Reset    Timer 6.0, Aktiv-Bit loeschen, Farbe 255/255/255, Modus 0
//   Handler  READ_BYTE = Modus: 1 -> Modusfeld 0, 2 -> 2, 3 -> 1 (jeweils
//            Aktiv-Bit + Farbe 255/255/255), 0 -> Waffenkreuz wiederherstellen
//   Draw     zeichnet das dmllock-Sprite; Retail setzt dafuer per
//            pfnSetCrosshair einen Sprite+Rect aus der WEAPON-Ressource
//            (Feld +0x184/+0x188, s. findings/client/klassen_abgleich_2026-11-06.md).
//
// Abweichung (dokumentiert): Das Rect der Zielmarkierung liegt in Gunmans
// WEAPON-Struktur (sizeof 0x1D4); unsere Struktur kennt diese Felder nicht.
// Deshalb wird das Sprite mittig auf dem Bildschirm gezeichnet, statt es als
// Engine-Crosshair mit Ziel-Rect zu setzen.

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

DECLARE_MESSAGE(m_DmlCross, DmlCross)

bool CHudDmlCross::Init()
{
	HOOK_MESSAGE(DmlCross);
	gHUD.AddHudElem(this);

	m_iFlags = 0;
	m_iMode = 0;
	m_iRed = m_iGreen = m_iBlue = 255;
	m_flExpire = 0.0f;
	return true;
}

bool CHudDmlCross::VidInit()
{
	m_hSprite = SPR_Load("sprites/dmllock.spr");
	return true;
}

void CHudDmlCross::Reset()
{
	m_iFlags &= ~HUD_ACTIVE;
	m_iMode = 0;
	m_iRed = m_iGreen = m_iBlue = 255;
	m_flExpire = 0.0f;
}

// Retail 0x10001080: ein Byte Modus, danach Zustand setzen.
bool CHudDmlCross::MsgFunc_DmlCross(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int mode = READ_BYTE();

	switch (mode)
	{
	case 1:
		m_iMode = 0;
		break;
	case 2:
		m_iMode = 2;
		break;
	case 3:
		m_iMode = 1;
		break;
	default: // 0 = Ziel verloren: Retail stellt das Waffenkreuz wieder her
		m_iFlags &= ~HUD_ACTIVE;
		m_iMode = 0;
		m_iRed = m_iGreen = m_iBlue = 255;
		return true;
	}

	m_iFlags |= HUD_ACTIVE;
	m_iRed = m_iGreen = m_iBlue = 255;
	m_flExpire = gHUD.m_flTime + 6.0f; // Retail: Timerfeld this+0x28 = 6.0
	return true;
}

bool CHudDmlCross::Draw(float flTime)
{
	if ((m_iFlags & HUD_ACTIVE) == 0 || m_hSprite == 0)
		return false;

	if (flTime >= m_flExpire)
	{
		m_iFlags &= ~HUD_ACTIVE;
		return false;
	}

	const int frame = 0;
	const int wide = SPR_Width(m_hSprite, frame);
	const int tall = SPR_Height(m_hSprite, frame);

	SPR_Set(m_hSprite, m_iRed, m_iGreen, m_iBlue);
	SPR_DrawAdditive(frame, ScreenWidth / 2 - wide / 2, ScreenHeight / 2 - tall / 2, NULL);
	return true;
}
