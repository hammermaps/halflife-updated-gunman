//=========================================================
// Shared flight steering of the retail chopper (0x1006b560) and
// rustflier (0x100b4100): identical math, different field offsets.
// Sound handling at the end of the retail functions differs per class and
// stays with the caller. See findings/entities/cbasebird_huntthink_2026-10-31.md.
//=========================================================
#pragma once

struct flightsteer_t
{
	float thrust = 0.0f;	   // chopper +0x278 / rustflier +0x27c
	Vector desiredDir;		   // +0x288 / +0x28c
	Vector goal;			   // +0x294 / +0x298
	float speedAccum = 0.0f;   // +0x2ac / +0x2b0
};

void FlightSteer(entvars_t* pev, flightsteer_t& st);
