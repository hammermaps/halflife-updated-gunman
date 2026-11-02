//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "vgui_int.h"
#include <VGUI_Label.h>
#include <VGUI_BorderLayout.h>
#include <VGUI_LineBorder.h>
#include <VGUI_SurfaceBase.h>
#include <VGUI_TextEntry.h>
#include <VGUI_ActionSignal.h>
#include <string.h>
#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "vgui_TeamFortressViewport.h"
#include "gunman_engine_target.h"

using namespace vgui;

void VGui_ViewportPaintBackground(int extents[4])
{
	gEngfuncs.VGui_ViewportPaintBackground(extents);
}

void* VGui_GetPanel()
{
	return (Panel*)gEngfuncs.VGui_GetPanel();
}

void VGui_Startup()
{
#if defined(GUNMAN_ENGINE_XASH3D)
	// FWGS hlsdk-portable leaves VGUI1 disabled by default.  Xash3D's main
	// menu is VGUI2-based; its VGUI1 bridge does not initialise the Scheme
	// required by this legacy Team Fortress viewport when a level loads.
	return;
#else
	Panel* root = (Panel*)VGui_GetPanel();
	root->setBgColor(128, 128, 0, 0);
	//root->setNonPainted(false);
	//root->setBorder(new LineBorder());
	root->setLayout(new BorderLayout(0));


	//root->getSurfaceBase()->setEmulatedCursorVisible(true);

	if (gViewPort != NULL)
	{
		//		root->removeChild(gViewPort);

		// free the memory
		//		delete gViewPort;
		//		gViewPort = NULL;

		gViewPort->Initialize();
	}
	else
	{
		gViewPort = new TeamFortressViewport(0, 0, root->getWide(), root->getTall());
		gViewPort->setParent(root);
	}
#endif
}

void VGui_Shutdown()
{
	delete gViewPort;
	gViewPort = NULL;
}

bool VGui_UpdateGunmanHud(bool visible, bool largeHud)
{
	if (gViewPort != NULL)
	{
		gViewPort->UpdateGunmanHud(visible, largeHud);
		return true;
	}
	return false;
}
