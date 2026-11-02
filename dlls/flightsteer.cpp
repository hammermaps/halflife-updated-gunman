#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "flightsteer.h"

// Port of retail chopper steering @ 0x1006b560 (same code at 0x100b4100 for
// the rustflier). Variables follow the decompile: W is the predicted position
// minus 768 in Z, G the vector from W to the goal.
void FlightSteer(entvars_t* pev, flightsteer_t& st)
{
	// yaw rate
	Vector ang = pev->angles + pev->avelocity * 2;
	ang.x += 8;
	UTIL_MakeVectors(ang);
	float side = DotProduct(st.desiredDir, gpGlobals->v_right);
	if (side >= 0)
	{
		if (pev->avelocity.y > -60)
			pev->avelocity.y -= 8;
	}
	else if (pev->avelocity.y < 60)
	{
		pev->avelocity.y += 8;
	}
	pev->avelocity.y *= 0.98f;

	// predicted position
	ang = pev->angles + pev->avelocity;
	ang.x += 8;
	UTIL_MakeVectors(ang);
	Vector vecPred = pev->origin + pev->velocity * 2 + gpGlobals->v_up * (st.thrust * 20);
	Vector vecW(vecPred.x, vecPred.y, vecPred.z - 768);

	// thrust and gravity
	UTIL_MakeVectors(Vector(pev->angles.x + 8, pev->angles.y, pev->angles.z));
	pev->velocity = pev->velocity + gpGlobals->v_up * st.thrust;
	pev->velocity.z -= 38.4;
	float speed = pev->velocity.Length();
	if (gpGlobals->v_forward.x * pev->velocity.x + gpGlobals->v_forward.y * pev->velocity.y < 0)
		speed = -speed;

	Vector G = st.goal - vecW;
	float gForward = DotProduct(G, gpGlobals->v_forward);
	float gRight = -DotProduct(G, gpGlobals->v_right);

	// roll rate
	if (gRight <= 0)
	{
		if (pev->angles.z >= 30 || pev->avelocity.z >= 15)
			pev->avelocity.z -= 2;
		else
			pev->avelocity.z += 4;
	}
	else
	{
		if (pev->angles.z <= -30 || pev->avelocity.z <= -15)
			pev->avelocity.z += 2;
		else
			pev->avelocity.z -= 4;
	}

	// damping
	pev->velocity.x *= 1 - 0.05 * fabs(gpGlobals->v_right.x);
	pev->velocity.y *= 1 - 0.05 * fabs(gpGlobals->v_right.y);
	pev->velocity.z *= 1 - 0.05 * fabs(gpGlobals->v_right.z);
	pev->velocity = pev->velocity * 0.995f;

	// thrust update
	if (st.thrust < 80 && vecW.z < st.goal.z)
		st.thrust += 12;
	else if (st.thrust > 30 && vecW.z > st.goal.z)
		st.thrust -= 8;

	// pitch rate
	float pitch = pev->angles.x + pev->avelocity.x;
	if (gForward > 0 && speed < st.speedAccum && pitch > -40)
		pev->avelocity.x -= 12;
	else if (gForward < 0 && speed > -50 && pitch < 20)
		pev->avelocity.x += 12;
	else if (pitch > 0)
		pev->avelocity.x -= 4;
	else if (pitch < 0)
		pev->avelocity.x += 4;
}
