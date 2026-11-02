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
// util.cpp
//
// implementation of class-less helper functions
//

#include <cstdio>
#include <cstdlib>

#include "hud.h"
#include "cl_util.h"
#include <string.h>

HSPRITE LoadSprite(const char* pszName)
{
	int iRes;
	char sz[256]; 

	// Gunman ships only 320- and 640-wide HUD sprite sheets. Higher
	// resolutions use the 640 set and are scaled by the renderer/layout.
	if (ScreenWidth >= 640)
		iRes = 640;
	else
		iRes = 320;

	sprintf(sz, pszName, iRes);

	return SPR_Load(sz);
}
