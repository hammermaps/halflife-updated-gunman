//=========================================================
// CHudTemperature - Ueberhitzungsanzeige (Minigun-Hitze, 0..100).
// Retail-Befund (Dekompilat client.dll 2026-11-01): Nachricht "TempX" (2 Bytes: Waffen-ID, Hitze),
// Konstruktor FUN_1004b990, Aktualisierung FUN_10029f30, Erzeuger FUN_10049ad0 (Panel 256x256 am
// Bildschirm-Unterrand). Details: findings/client/hud_gap_2026-11-01.md.
//=========================================================
#pragma once

#include "hud.h"

class CHudTemperature : public CHudBase
{
public:
	bool Init() override;
	bool VidInit() override;
	bool Draw(float flTime) override;
	bool MsgFunc_TempX(const char* pszName, int iSize, void* pbuf);

private:
	int m_iWeaponId = -1; // Waffe, zu der die letzte gueltige Hitze gehoert
	int m_iHeat = 0;      // Hitze 0..100
	int m_HUD_temperature = -1;
};
