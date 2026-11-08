//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#pragma once

void EV_HLDM_GunshotDecalTrace(pmtrace_t* pTrace, char* decalName);
void EV_HLDM_DecalGunshot(pmtrace_t* pTrace, int iBulletType);
void EV_HLDM_CheckTracer(int idx, float* vecSrc, float* end, float* forward, float* right, int iBulletType, int iTracerFreq, int* tracerCount);
void EV_HLDM_FireBullets(int idx, float* forward, float* right, float* up, int cShots, float* vecSrc, float* vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int* tracerCount, float flSpreadX, float flSpreadY);

// HLDM
void EV_FireGlock1(event_args_t* args);
void EV_FireGlock2(event_args_t* args);
void EV_FireShotGunSingle(event_args_t* args);
void EV_FireShotGunDouble(event_args_t* args);
void EV_FireMP5(event_args_t* args);
void EV_FireMP52(event_args_t* args);
void EV_FirePython(event_args_t* args);
void EV_FireGauss(event_args_t* args);
void EV_SpinGauss(event_args_t* args);
void EV_SniperShot(event_args_t* args);
void EV_RapidImpact(event_args_t* args);
void EV_GaussPistol(event_args_t* args);
void EV_ChargedImpact(event_args_t* args);
void EV_ClusterExplosion(event_args_t* args);
void EV_UpdateClusterExplosionEffects();
void EV_UpdateEffectTubes();
void EV_UpdateEffectQuads();
void EV_Precache();
void EV_ResetClusterExplosionEffects();
void EV_ChemicalSpray(event_args_t* args);
void EV_Crowbar(event_args_t* args);
void EV_FireCrossbow(event_args_t* args);
void EV_FireCrossbow2(event_args_t* args);
void EV_FireRpg(event_args_t* args);
void EV_EgonFire(event_args_t* args);
void EV_EgonStop(event_args_t* args);
void EV_HornetGunFire(event_args_t* args);
void EV_TripmineFire(event_args_t* args);
void EV_SnarkFire(event_args_t* args);
void EV_TurretBullets(event_args_t* args);
void EV_MonsterSentry(event_args_t* args);
void EV_NoOpEvent(event_args_t* args);
void EV_MinigunFire(event_args_t* args);
void EV_ShotgunFire(event_args_t* args);
void EV_MonsterShotgun(event_args_t* args);
void EV_MonsterMechagun(event_args_t* args);
void EV_ChopperFire(event_args_t* args);
void EV_RocketSmoke(event_args_t* args);
void EV_Cylinder(event_args_t* args);
void EV_SphereExplode(event_args_t* args);
void EV_Lightning(event_args_t* args);
void EV_RustbitImpact(event_args_t* args);
void EV_KataImpact(event_args_t* args);
void EV_MinigunHeat(event_args_t* args);
void EV_Cleanup(event_args_t* args);
void EV_SpurtBlood(event_args_t* args);
void EV_BallLightning(event_args_t* args);
void EV_BatBotBeam(event_args_t* args);
void EV_TankMGuns(event_args_t* args);

void EV_HookEvents();

void EV_TrainPitchAdjust(event_args_t* args);
void EV_VehiclePitchAdjust(event_args_t* args);

// Stein-/Materialfragmente (Port von hl15we EV_Wallgib, game_fx.cpp:168). Skin 1 = GIB_ROCK.
void EV_RetailRockShards(const Vector& vecOrigin, float flSpeed, float flScale, int iCount,
	int iSkin, int iFlags);
