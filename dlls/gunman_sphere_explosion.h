#ifndef GUNMAN_SPHERE_EXPLOSION_H
#define GUNMAN_SPHERE_EXPLOSION_H

void GunmanFireSphereExplosion(const Vector& origin, float maxRadius, float radiusStep,
	int trans, int transStep, int numTracers, int shockwave, int implosion,
	float shakeDuration, float fadeTime, float fadeHoldTime);

#endif
