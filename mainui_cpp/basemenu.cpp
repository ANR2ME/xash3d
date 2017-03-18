/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


// ui_menu.c -- main menu interface
#define OEMRESOURCE		// for OCR_* cursor junk



#include "extdll.h"
#include "basemenu.h"
#include "PicButton.h"
#include "keydefs.h"
#include "menufont.h"	// built-in menu font
#include "utils.h"
#include "menu_btnsbmp_table.h"

cvar_t		*ui_precache;
cvar_t		*ui_showmodels;

uiStatic_t	uiStatic;

const char	*uiSoundIn	= "media/launch_upmenu1.wav";
const char	*uiSoundOut	= "media/launch_dnmenu1.wav";
const char	*uiSoundLaunch	= "media/launch_select2.wav";
const char	*uiSoundGlow	= "media/launch_glow1.wav";
const char	*uiSoundBuzz	= "media/launch_deny2.wav";
const char	*uiSoundKey	= "media/launch_select1.wav";
const char	*uiSoundRemoveKey	= "media/launch_deny1.wav";
const char	*uiSoundMove	= "";		// Xash3D not use movesound
const char	*uiSoundNull	= "";

int		uiColorHelp	= 0xFFFFFFFF;	// 255, 255, 255, 255	// hint letters color
int		uiPromptBgColor	= 0xFF404040;	// 64,  64,  64,  255	// dialog background color
int		uiPromptTextColor	= 0xFFF0B418;	// 255, 160,  0,  255	// dialog or button letters color
int		uiPromptFocusColor	= 0xFFFFFF00;	// 255, 255,  0,  255	// dialog or button focus letters color
int		uiInputTextColor	= 0xFFC0C0C0;	// 192, 192, 192, 255
int		uiInputBgColor	= 0xFF404040;	// 64,  64,  64,  255	// field, scrollist, checkbox background color
int		uiInputFgColor	= 0xFF555555;	// 85,  85,  85,  255	// field, scrollist, checkbox foreground color
int		uiColorWhite	= 0xFFFFFFFF;	// 255, 255, 255, 255	// useful for bitmaps
int		uiColorDkGrey	= 0xFF404040;	// 64,  64,  64,  255	// shadow and grayed items
int		uiColorBlack	= 0xFF000000;	//  0,   0,   0,  255	// some controls background
int		uiColorConsole	= 0xFFF0B418;	// just for reference 

// color presets (this is nasty hack to allow color presets to part of text)
const unsigned int g_iColorTable[8] =
{
0xFF000000, // black
0xFFFF0000, // red
0xFF00FF00, // green
0xFFFFFF00, // yellow
0xFF0000FF, // blue
0xFF00FFFF, // cyan
0xFFF0B418, // dialog or button letters color
0xFFFFFFFF, // white
};

/*
=================
UI_ScaleCoords

Any parameter can be NULL if you don't want it
=================
*/
void UI_ScaleCoords( int *x, int *y, int *w, int *h )
{
	if( x ) *x *= uiStatic.scaleX;
	if( y ) *y *= uiStatic.scaleY;
	if( w ) *w *= uiStatic.scaleX;
	if( h ) *h *= uiStatic.scaleY;
}

/*
=================
UI_CursorInRect
=================
*/
int UI_CursorInRect( int x, int y, int w, int h )
{
	if( uiStatic.cursorX < x )
		return FALSE;
	if( uiStatic.cursorX > x + w )
		return FALSE;
	if( uiStatic.cursorY < y )
		return FALSE;
	if( uiStatic.cursorY > y + h )
		return FALSE;
	return TRUE;
}

/*
=================
UI_DrawPic
=================
*/
void UI_DrawPic( int x, int y, int width, int height, const int color, const char *pic )
{
	HIMAGE hPic = EngFuncs::PIC_Load( pic );
	if (!hPic)
		return;

	int r, g, b, a;
	UnpackRGBA( r, g, b, a, color );

	EngFuncs::PIC_Set( hPic, r, g, b, a );
	EngFuncs::PIC_Draw( x, y, width, height );
}

/*
=================
UI_DrawPicAdditive
=================
*/
void UI_DrawPicAdditive( int x, int y, int width, int height, const int color, const char *pic )
{
	HIMAGE hPic = EngFuncs::PIC_Load( pic );
	if (!hPic)
		return;

	int r, g, b, a;
	UnpackRGBA( r, g, b, a, color );

	EngFuncs::PIC_Set( hPic, r, g, b, a );
	EngFuncs::PIC_DrawAdditive( x, y, width, height );
}

/*
=================
UI_DrawPicAdditive
=================
*/
void UI_DrawPicTrans( int x, int y, int width, int height, const int color, const char *pic )
{
	HIMAGE hPic = EngFuncs::PIC_Load( pic );
	if (!hPic)
		return;

	int r, g, b, a;
	UnpackRGBA( r, g, b, a, color );

	EngFuncs::PIC_Set( hPic, r, g, b, a );
	EngFuncs::PIC_DrawTrans( x, y, width, height );
}


/*
=================
UI_DrawPicAdditive
=================
*/
void UI_DrawPicHoles( int x, int y, int width, int height, const int color, const char *pic )
{
	HIMAGE hPic = EngFuncs::PIC_Load( pic );
	if (!hPic)
		return;

	int r, g, b, a;
	UnpackRGBA( r, g, b, a, color );

	EngFuncs::PIC_Set( hPic, r, g, b, a );
	EngFuncs::PIC_DrawHoles( x, y, width, height );
}


/*
=================
UI_FillRect
=================
*/
void UI_FillRect( int x, int y, int width, int height, const int color )
{
	int r, g, b, a;
	UnpackRGBA( r, g, b, a, color );

	EngFuncs::FillRGBA( x, y, width, height, r, g, b, a );
}

/*
=================
UI_DrawRectangleExt
=================
*/
void UI_DrawRectangleExt( int in_x, int in_y, int in_w, int in_h, const int color, int outlineWidth )
{
	int	x, y, w, h;

	x = in_x - outlineWidth;
	y = in_y - outlineWidth;
	w = outlineWidth;
	h = in_h + outlineWidth + outlineWidth;

	// draw left
	UI_FillRect( x, y, w, h, color );

	x = in_x + in_w;
	y = in_y - outlineWidth;
	w = outlineWidth;
	h = in_h + outlineWidth + outlineWidth;

	// draw right
	UI_FillRect( x, y, w, h, color );

	x = in_x;
	y = in_y - outlineWidth;
	w = in_w;
	h = outlineWidth;

	// draw top
	UI_FillRect( x, y, w, h, color );

	// draw bottom
	x = in_x;
	y = in_y + in_h;
	w = in_w;
	h = outlineWidth;

	UI_FillRect( x, y, w, h, color );
}

/*
=================
UI_DrawString
=================
*/
void UI_DrawString( int x, int y, int w, int h, const char *string, const int color, int forceColor, int charW, int charH, int justify, int shadow )
{
	int	modulate, shadowModulate;
	char	line[1024], *l;
	int	xx = 0, yy, ofsX = 0, ofsY = 0, len, ch;

	if( !string || !string[0] )
		return;

#if 0	// g-cont. disabled 29/06/2011
	// this code do a bad things with prompt dialogues
	// vertically centered
	if( !strchr( string, '\n' ))
		y = y + (( h - charH ) / 2 );
#endif

	if( shadow )
	{
		shadowModulate = PackAlpha( uiColorBlack, UnpackAlpha( color ));

		ofsX = charW / 8;
		ofsY = charH / 8;
	}

	modulate = color;

	yy = y;
	while( *string )
	{
		// get a line of text
		len = 0;
		while( *string )
		{
			if( *string == '\n' )
			{
				string++;
				break;
			}

			line[len++] = *string++;
			if( len == sizeof( line ) - 1 )
				break;
		}
		line[len] = 0;

		// align the text as appropriate
		if( justify == 0 ) xx = x;
		if( justify == 1 ) xx = x + ((w - (ColorStrlen( line ) * charW )) / 2);
		if( justify == 2 ) xx = x + (w - (ColorStrlen( line ) * charW ));

		// draw it
		l = line;
		while( *l )
		{
			if( IsColorString( l ))
			{
				int colorNum = ColorIndex( *(l+1) );

				if( colorNum == 7 && color != 0 )
				{
					modulate = color;
				}
				else if( !forceColor )
				{
					modulate = PackAlpha( g_iColorTable[colorNum], UnpackAlpha( color ));
				}

				l += 2;
				continue;
			}

			ch = *l++;
			ch &= 255;
#if 0
#ifdef _WIN32
			// fix for letter �
			if( ch == 0xB8 ) ch = (byte)'�';
			if( ch == 0xA8 ) ch = (byte)'�';
#endif
#endif
			ch = EngFuncs::UtfProcessChar( (unsigned char) ch );
			if( !ch )
				continue;
			if( ch != ' ' )
			{
				if( shadow ) EngFuncs::DrawCharacter( xx + ofsX, yy + ofsY, charW, charH, ch, shadowModulate, uiStatic.hFont );
				EngFuncs::DrawCharacter( xx, yy, charW, charH, ch, modulate, uiStatic.hFont );
			}
			xx += charW;
		}
          	yy += charH;
	}
}

/*
=================
UI_DrawMouseCursor
=================
*/
void UI_DrawMouseCursor( void )
{
/* TODO: SDL2
	CMenuBaseItem	*item;
	HICON		hCursor = NULL;
	int		i;

	if( uiStatic.hideCursor ) return;

	for( i = 0; i < uiStatic.menuActive->numItems; i++ )
	{
		item = (CMenuBaseItem *)uiStatic.menuActive->items[i];

		if ( item->flags & (QMF_INACTIVE|QMF_HIDDEN))
			continue;

		if ( !UI_CursorInRect( item->x, item->y, item->width, item->height ))
			continue;

		if ( item->flags & QMF_GRAYED )
		{
			hCursor = (HICON)LoadCursor( NULL, (LPCTSTR)OCR_NO );
		}
		else
		{
			if( item->type == QMTYPE_FIELD )
				hCursor = (HICON)LoadCursor( NULL, (LPCTSTR)OCR_IBEAM );
		}
		break;
	}

	if( !hCursor ) hCursor = (HICON)LoadCursor( NULL, (LPCTSTR)OCR_NORMAL );

	SET_CURSOR( hCursor );
*/
}


/*
=================
UI_LoadBackgroundImage
=================
*/
void UI_LoadBackgroundImage( void )
{
	int num_background_images = 0;
	char filename[512];

	for( int y = 0; y < BACKGROUND_ROWS; y++ )
	{
		for( int x = 0; x < BACKGROUND_COLUMNS; x++ )
		{
			sprintf( filename, "resource/background/800_%d_%c_loading.tga", y + 1, 'a' + x );
			if (EngFuncs::FileExists( filename, TRUE ))
				num_background_images++;
		}
	}

	if (num_background_images == (BACKGROUND_COLUMNS * BACKGROUND_ROWS))
		uiStatic.m_fHaveSteamBackground = TRUE;
	else uiStatic.m_fHaveSteamBackground = FALSE;

	if (uiStatic.m_fHaveSteamBackground)
	{
		uiStatic.m_flTotalWidth = uiStatic.m_flTotalHeight = 0.0f;

		for( int y = 0; y < BACKGROUND_ROWS; y++ )
		{
			for( int x = 0; x < BACKGROUND_COLUMNS; x++ )
			{
				bimage_t &bimage = uiStatic.m_SteamBackground[y][x];
				sprintf(filename, "resource/background/800_%d_%c_loading.tga", y + 1, 'a' + x);
				bimage.hImage = EngFuncs::PIC_Load( filename, PIC_NOFLIP_TGA );
				bimage.width = EngFuncs::PIC_Width( bimage.hImage );
				bimage.height = EngFuncs::PIC_Height( bimage.hImage );

				if (y==0) uiStatic.m_flTotalWidth += bimage.width;
				if (x==0) uiStatic.m_flTotalHeight += bimage.height;
			}
		}
	}
	else
	{
		if( EngFuncs::FileExists( "gfx/shell/splash.bmp", TRUE ))
		{
			// if we doesn't have logo.avi in gamedir we don't want to draw it
			if( !EngFuncs::FileExists( "media/logo.avi", TRUE ))
				uiStatic.m_fDisableLogo = TRUE;
		}
	}
}

/*
=================
UI_StartSound
=================
*/
void UI_StartSound( const char *sound )
{
	EngFuncs::PlayLocalSound( sound );
}

// =====================================================================


/*
=================
UI_AddItem
=================
*/
void CMenuFramework::AddItem( CMenuBaseItem &generic )
{
	if( numItems >= UI_MAX_MENUITEMS )
		Host_Error( "UI_AddItem: UI_MAX_MENUITEMS limit exceeded\n" );

	items[numItems] = &generic;
	generic.m_pParent = this;
	generic.m_iFlags &= ~QMF_HASMOUSEFOCUS;

	numItems++;

	generic.Init();
}

/*
=================
menuFrawework_s::VidInitItems
=================
*/
void CMenuFramework::VidInitItems( void )
{
	for( int i = 0; i < numItems; i++ )
	{
		items[i]->VidInit();
	}
}

/*
=================
UI_CursorMoved
=================
*/
void CMenuFramework::CursorMoved( void )
{
	CMenuBaseItem::EventCallback callback;
	CMenuBaseItem *curItem;

	if( cursor == cursorPrev )
		return;

	if( cursorPrev >= 0 && cursorPrev < numItems )
	{
		curItem = items[cursorPrev];

		curItem->_Event( QM_LOSTFOCUS );
	}

	if( cursor >= 0 && cursor < numItems )
	{
		curItem = items[cursor];

		curItem->_Event( QM_GOTFOCUS );
	}
}

/*
=================
UI_SetCursor
=================
*/
void CMenuFramework::SetCursor( int newCursor, bool notify )
{
	if( newCursor < 0 || newCursor > numItems )
		return;

	if(items[newCursor]->Flags() & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN))
		return;

	cursorPrev = newCursor;
	cursor = newCursor;

	if( notify )
		CursorMoved();
}

/*
=================
UI_SetCursorToItem
=================
*/
void CMenuFramework::SetCursorToItem( CMenuBaseItem *item , bool notify )
{
	for( int i = 0; i < numItems; i++ )
	{
		if( items[i] == item )
		{
			SetCursor( i, notify );
			return;
		}
	}
}

/*
=================
UI_ItemAtCursor
=================
*/
CMenuBaseItem *CMenuFramework::ItemAtCursor( void )
{
	if( cursor < 0 || cursor >= numItems )
		return 0;

	// inactive items can't be has focus
	if( items[cursor]->Flags() & QMF_INACTIVE )
		return 0;

	return items[cursor];
}

/*
================
UI_IsCurrentElement

Checks given menu is current selected
================
*/
bool CMenuBaseItem::IsCurrentSelected( void )
{
	return this == m_pParent->ItemAtCursor();
}

/*
=================
UI_AdjustCursor

This functiont takes the given menu, the direction, and attempts to
adjust the menu's cursor so that it's at the next available slot
=================
*/
void CMenuFramework::AdjustCursor( int dir )
{
	CMenuBaseItem	*item;
	int		wrapped = false;
wrap:
	while( cursor >= 0 && cursor < numItems )
	{
		item = items[cursor];
		if( item->Flags() & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN|QMF_MOUSEONLY))
			cursor += dir;
		else break;
	}

	if( dir == 1 )
	{
		if( cursor >= numItems )
		{
			if( wrapped )
			{
				cursor = cursorPrev;
				return;
			}

			cursor = 0;
			wrapped = true;
			goto wrap;
		}
	}
	else if( dir == -1 )
	{
		if( cursor < 0 )
		{
			if( wrapped )
			{
				cursor = cursorPrev;
				return;
			}
			cursor = numItems - 1;
			wrapped = true;
			goto wrap;
		}
	}
}

/*
=================
UI_DrawMenu
=================
*/
void CMenuFramework::Draw()
{
	static int	statusFadeTime;
	static CMenuBaseItem	*lastItem;
	CMenuBaseItem	*item;
	int		i;
	const char *statusText;

	// draw contents
	for( i = 0; i < numItems; i++ )
	{
		item = items[i];

		if( item->Flags() & QMF_HIDDEN )
			continue;

		item->Draw();
	}

	// draw status bar
	item = ItemAtCursor();
	if( item != lastItem )
	{
		// flash on selected button (like in GoldSrc)
		if( item ) item->m_iLastFocusTime = uiStatic.realTime;
		statusFadeTime = uiStatic.realTime;

		lastItem = item;
	}

	if( item && item == lastItem && ( ( statusText = item->StatusText() ) != NULL ))
	{
		// fade it in, but wait a second
		float alpha = bound(0, ((( uiStatic.realTime - statusFadeTime ) - 100 ) * 0.01f ), 1);
		int r, g, b, x, len;

		EngFuncs::ConsoleStringLen( statusText, &len, NULL );

		UnpackRGB( r, g, b, uiColorHelp );
		EngFuncs::DrawSetTextColor( r, g, b, alpha * 255 );
		x = ( ScreenWidth - len ) * 0.5; // centering

		EngFuncs::DrawConsoleString( x, 720 * uiStatic.scaleY, statusText );
	}
	else statusFadeTime = uiStatic.realTime;
}

/*
=================
UI_DefaultKey
=================
*/
const char *CMenuFramework::Key( int key, int down )
{
	const char	*sound = NULL;
	CMenuBaseItem	*item;
	int		_cursorPrev;

	// menu system key
	if( down && key == K_ESCAPE )
	{
		PopMenu();
		return uiSoundOut;
	}

	if( !numItems )
		return 0;

	item = ItemAtCursor();

	if( item && !(item->Flags() & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN)))
	{
		sound = item->Key( key, down );

		if( sound ) return sound; // key was handled
	}

	// system keys are always wait for keys down and never keys up
	if( !down )
		return 0;

	// default handling
	switch( key )
	{
	case K_UPARROW:
	case K_KP_UPARROW:
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
		_cursorPrev = cursor;
		cursorPrev = cursor;
		cursor--;

		AdjustCursor( -1 );
		if( _cursorPrev != cursor )
		{
			CursorMoved();
			if( !(items[cursor]->Flags() & QMF_SILENT ))
				sound = uiSoundMove;

			items[cursorPrev]->Flags() &= ~QMF_HASKEYBOARDFOCUS;
			items[cursor]->Flags() |= QMF_HASKEYBOARDFOCUS;
		}
		break;
	case K_DOWNARROW:
	case K_KP_DOWNARROW:
	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
	case K_TAB:
		_cursorPrev = cursor;
		cursorPrev = cursor;
		cursor++;

		AdjustCursor(1);
		if( cursorPrev != cursor )
		{
			CursorMoved();
			if( !(items[cursor]->Flags() & QMF_SILENT ))
				sound = uiSoundMove;

			items[cursorPrev]->Flags() &= ~QMF_HASKEYBOARDFOCUS;
			items[cursor]->Flags() |= QMF_HASKEYBOARDFOCUS;
		}
		break;
	case K_MOUSE1:
		if( item )
		{
			if((item->Flags() & QMF_HASMOUSEFOCUS) && !(item->Flags() & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN)))
				return item->Activate();
		}

		break;
	case K_ENTER:
	case K_KP_ENTER:
	case K_AUX1:
	case K_AUX13:
		if( item )
		{
			if( !(item->Flags() & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN|QMF_MOUSEONLY)))
				return item->Activate();
		}
		break;
	}
	return sound;
}		

/*
=================
UI_ActivateItem
=================
*/
const char *CMenuBaseItem::Activate( )
{
	_Event( QM_ACTIVATED );

	if( !( m_iFlags & QMF_SILENT ))
		return uiSoundMove;
	return 0;
}

/*
=================
UI_RefreshServerList
=================
*/
void UI_RefreshServerList( void )
{
	uiStatic.numServers = 0;
	uiStatic.serversRefreshTime = gpGlobals->time;

	memset( uiStatic.serverAddresses, 0, sizeof( uiStatic.serverAddresses ));
	memset( uiStatic.serverNames, 0, sizeof( uiStatic.serverNames ));
	memset( uiStatic.serverPings, 0, sizeof( uiStatic.serverPings ));

	EngFuncs::ClientCmd( FALSE, "localservers\n" );
}

/*
=================
UI_RefreshInternetServerList
=================
*/
void UI_RefreshInternetServerList( void )
{
	uiStatic.numServers = 0;
	uiStatic.serversRefreshTime = gpGlobals->time;

	memset( uiStatic.serverAddresses, 0, sizeof( uiStatic.serverAddresses ));
	memset( uiStatic.serverNames, 0, sizeof( uiStatic.serverNames ));
	memset( uiStatic.serverPings, 0, sizeof( uiStatic.serverPings ));

	EngFuncs::ClientCmd( FALSE, "internetservers\n" );
}

/*
=================
UI_StartBackGroundMap
=================
*/
bool UI_StartBackGroundMap( void )
{
	static bool	first = TRUE;

	if( !first ) return FALSE;

	first = FALSE;

	// some map is already running
	if( !uiStatic.bgmapcount || CL_IsActive() || gpGlobals->demoplayback )
		return FALSE;

	int bgmapid = EngFuncs::RandomLong( 0, uiStatic.bgmapcount - 1 );

	char cmd[128];
	sprintf( cmd, "maps/%s.bsp", uiStatic.bgmaps[bgmapid] );
	if( !EngFuncs::FileExists( cmd, TRUE )) return FALSE;

	sprintf( cmd, "map_background %s\n", uiStatic.bgmaps[bgmapid] );
	EngFuncs::ClientCmd( FALSE, cmd );

	return TRUE;
}

// =====================================================================

/*
=================
UI_CloseMenu
=================
*/
void UI_CloseMenu( void )
{
	uiStatic.menuActive = NULL;
	uiStatic.menuDepth = 0;
	uiStatic.visible = false;

	// clearing serverlist
	uiStatic.numServers = 0;
	memset( uiStatic.serverAddresses, 0, sizeof( uiStatic.serverAddresses ));
	memset( uiStatic.serverNames, 0, sizeof( uiStatic.serverNames ));

	CMenuPicButton::ClearButtonStack();

//	EngFuncs::KEY_ClearStates ();
	EngFuncs::KEY_SetDest( KEY_GAME );
}

/*
=================
UI_PushMenu
=================
*/
void CMenuFramework::PushMenu( void )
{
	int		i;
	CMenuBaseItem	*item;

	// if this menu is already present, drop back to that level to avoid stacking menus by hotkeys
	for( i = 0; i < uiStatic.menuDepth; i++ )
	{
		if( uiStatic.menuStack[i] == this )
		{
			uiStatic.menuDepth = i;
			break;
		}
	}

	if( i == uiStatic.menuDepth )
	{
		if( uiStatic.menuDepth >= UI_MAX_MENUDEPTH )
			Host_Error( "UI_PushMenu: menu stack overflow\n" );
		uiStatic.menuStack[uiStatic.menuDepth++] = this;
	}

	uiStatic.menuActive = this;
	uiStatic.firstDraw = true;
	uiStatic.enterSound = gpGlobals->time + 0.15;	// make some delay
	uiStatic.visible = true;

	EngFuncs::KEY_SetDest ( KEY_MENU );

	cursor = 0;
	cursorPrev = 0;

	// force first available item to have focus
	for( i = 0; i < numItems; i++ )
	{
		item = items[i];

		if( item->Flags() & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN|QMF_MOUSEONLY))
			continue;

		cursorPrev = -1;
		SetCursor( i );
		break;
	}
}

/*
=================
UI_PopMenu
=================
*/
void CMenuFramework::PopMenu( void )
{
	UI_StartSound( uiSoundOut );

	uiStatic.menuDepth--;

	if( uiStatic.menuDepth < 0 )
		Host_Error( "UI_PopMenu: menu stack underflow\n" );

	CMenuPicButton::PopPButtonStack();

	if( uiStatic.menuDepth )
	{
		uiStatic.menuActive = uiStatic.menuStack[uiStatic.menuDepth-1];
		uiStatic.firstDraw = true;
	}
	else if ( CL_IsActive( ))
	{
		UI_CloseMenu();
	}
	else
	{
		// never trying the close menu when client isn't connected
		EngFuncs::KEY_SetDest( KEY_MENU );
		UI_Main_Menu();
	}

	if( uiStatic.m_fDemosPlayed && uiStatic.m_iOldMenuDepth == uiStatic.menuDepth )
	{
		EngFuncs::ClientCmd( FALSE, "demos\n" );
		uiStatic.m_fDemosPlayed = false;
		uiStatic.m_iOldMenuDepth = 0;
	}
}

// =====================================================================

/*
=================
UI_UpdateMenu
=================
*/
void UI_UpdateMenu( float flTime )
{
	if( !uiStatic.initialized )
		return;

	// UI_DrawFinalCredits ();

	if( !uiStatic.visible )
		return;

	if( !uiStatic.menuActive )
		return;

	uiStatic.realTime = flTime * 1000;
	uiStatic.framecount++;

	if( !EngFuncs::ClientInGame() && EngFuncs::GetCvarFloat( "cl_background" ))
		return;	// don't draw menu while level is loading

	if( uiStatic.firstDraw )
	{
		// we loading background so skip SCR_Update
		if( UI_StartBackGroundMap( )) return;

		uiStatic.menuActive->Activate();
	}

	// draw menu
	uiStatic.menuActive->Draw();

	if( uiStatic.firstDraw )
	{
		uiStatic.firstDraw = false;
		static int first = TRUE;
                    
		if( first )
		{
			// if game was launched with commandline e.g. +map or +load ignore the music
			if( !CL_IsActive( ))
				EngFuncs::PlayBackgroundTrack( "gamestartup", "gamestartup" );
			first = FALSE;
		}
	}

	//CR
	CMenuPicButton::DrawTitleAnim();
	//

	// draw cursor
	// UI_DrawMouseCursor();

	// delay playing the enter sound until after the menu has been
	// drawn, to avoid delay while caching images
	if( uiStatic.enterSound > 0.0f && uiStatic.enterSound <= gpGlobals->time )
	{
		UI_StartSound( uiSoundIn );
		uiStatic.enterSound = -1;
	}
}

/*
=================
UI_KeyEvent
=================
*/
void UI_KeyEvent( int key, int down )
{
	const char	*sound;

	if( !uiStatic.initialized )
		return;

	if( !uiStatic.visible )
		return;

	if( !uiStatic.menuActive )
		return;
	if( key == K_MOUSE1 )
	{
		cursorDown = !!down;
	}

	sound = uiStatic.menuActive->Key( key, down );

	if( !down ) return;
	if( sound && sound != uiSoundNull )
		UI_StartSound( sound );
}

/*
=================
UI_CharEvent
=================
*/
void UI_CharEvent( int key )
{
	if( !uiStatic.initialized )
		return;

	if( !uiStatic.visible )
		return;

	if( !uiStatic.menuActive )
		return;

	uiStatic.menuActive->Char( key );
}

void CMenuFramework::Char( int key )
{
	CMenuBaseItem	*item;

	if( numItems )
		return;

	item = ItemAtCursor( );

	if( item && !(item->Flags() & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN)))
	{
		item->Char( key );
	}
}

bool cursorDown;
float cursorDY;

/*
=================
UI_MouseMove
=================
*/
void UI_MouseMove( int x, int y )
{
	int		i;
	CMenuBaseItem	*item;

	if( !uiStatic.initialized )
		return;

	if( !uiStatic.visible )
		return;

	if( cursorDown )
	{
		static bool prevDown = false;
		if(!prevDown)
			prevDown = true, cursorDY = 0;
		else
			if( y - uiStatic.cursorY )
				cursorDY += y - uiStatic.cursorY;
	}
	else
		cursorDY = 0;
	//Con_Printf("%d %d %f\n",x, y, cursorDY);
	if( !uiStatic.menuActive )
		return;

	// now menu uses absolute coordinates
	uiStatic.cursorX = x;
	uiStatic.cursorY = y;

	// hack: prevent changing focus when field active
#if (defined(__ANDROID__) || defined(MENU_FIELD_RESIZE_HACK)) && 0
	CMenuField *f = uiStatic.menuActive->ItemAtCursor();
	if( f && f->type == QMTYPE_FIELD )
	{
		float y = f->y;

		if( y > ScreenHeight - f->height - 40 )
			y = ScreenHeight - f->height - 15;

		if( UI_CursorInRect( f->x - 30, y - 30, f->width + 60, f->height + 60 ) )
			return;
	}
#endif

	if( UI_CursorInRect( 1, 1, ScreenWidth - 1, ScreenHeight - 1 ))
		uiStatic.mouseInRect = true;
	else uiStatic.mouseInRect = false;

	uiStatic.cursorX = bound( 0, uiStatic.cursorX, ScreenWidth );
	uiStatic.cursorY = bound( 0, uiStatic.cursorY, ScreenHeight );

	uiStatic.menuActive->MouseMove( x, y );
}

void CMenuFramework::MouseMove( int x, int y )
{
	int i;
	CMenuBaseItem *item;

	// region test the active menu items
	for( i = 0; i < numItems; i++ )
	{
		item = items[i];

		if( item->Flags() & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN) )
		{
			if( item->Flags() & (QMF_HASMOUSEFOCUS) )
			{
				if( !UI_CursorInRect( item->XPos(), item->YPos(), item->Width(), item->Height() ))
				{
					item->Flags() &= ~QMF_HASMOUSEFOCUS;
				}
				else item->m_iLastFocusTime = uiStatic.realTime;
			}
			continue;
		}

		if( !UI_CursorInRect( item->XPos(), item->YPos(), item->Width(), item->Height() ))
		{
			item->m_bPressed = false;
			item->Flags() &= ~QMF_HASMOUSEFOCUS;
			continue;
		}

		// set focus to item at cursor
		if( cursor != i )
		{
			SetCursor( i );
			items[cursorPrev]->Flags() &= ~QMF_HASMOUSEFOCUS;
			// reset a keyboard focus also, because we are changed cursor
			items[cursorPrev]->Flags() &= ~QMF_HASKEYBOARDFOCUS;

			if (!(items[cursor]->Flags() & QMF_SILENT ))
				UI_StartSound( uiSoundMove );
		}

		items[cursor]->Flags() |= QMF_HASMOUSEFOCUS;
		items[cursor]->m_iLastFocusTime = uiStatic.realTime;
		return;
	}

	// out of any region
	if( numItems )
	{
		items[cursor]->Flags() &= ~QMF_HASMOUSEFOCUS;
		items[cursor]->m_bPressed = false;

		// a mouse only item restores focus to the previous item
		if(items[cursor]->Flags() & QMF_MOUSEONLY )
		{
			if( cursorPrev != -1 )
				cursor = cursorPrev;
		}
	}
}

void CMenuFramework::PopMenuCb( CMenuBaseItem *pSelf, void *pExtra )
{
	pSelf->m_pParent->PopMenu();
}

void CMenuFramework::Init()
{
	if( !initialized )
	{
		_Init();
		initialized = true;
	}

	// m_pLayout->Init();
}

void CMenuFramework::VidInit()
{
	_VidInit();

	VidInitItems();

	// m_pLayout->VidInit();
}

/*
=================
UI_SetActiveMenu
=================
*/
void UI_SetActiveMenu( int fActive )
{
	if( !uiStatic.initialized )
		return;

	uiStatic.framecount = 0;

	if( fActive )
	{
		// don't continue firing if we leave game
		EngFuncs::KEY_ClearStates();

		EngFuncs::KEY_SetDest( KEY_MENU );
		UI_Main_Menu();
	}
	else
	{
		UI_CloseMenu();
	}
}


#if defined _WIN32
#include <windows.h>
#include <winbase.h>
/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime( void )
{
	static LARGE_INTEGER g_PerformanceFrequency;
	static LARGE_INTEGER g_ClockStart;
	LARGE_INTEGER CurrentTime;

	if( !g_PerformanceFrequency.QuadPart )
	{
		QueryPerformanceFrequency( &g_PerformanceFrequency );
		QueryPerformanceCounter( &g_ClockStart );
	}

	QueryPerformanceCounter( &CurrentTime );
	return (double)( CurrentTime.QuadPart - g_ClockStart.QuadPart ) / (double)( g_PerformanceFrequency.QuadPart );
}
#elif defined __APPLE__
typedef unsigned long long longtime_t;
#include <sys/time.h>
/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime( void )
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double) tv.tv_sec + (double) tv.tv_usec/1000000.0;
}
#else
typedef unsigned long long longtime_t;
#include <time.h>
/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime( void )
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double) ts.tv_sec + (double) ts.tv_nsec/1000000000.0;
}
#endif

/*
=================
UI_AddServerToList
=================
*/
void UI_AddServerToList( netadr_t adr, const char *info )
{
	int	i;

	if( !uiStatic.initialized )
		return;

	if( uiStatic.numServers == UI_MAX_SERVERS )
		return;	// full

	if( stricmp( gMenu.m_gameinfo.gamefolder, Info_ValueForKey( info, "gamedir" )) != 0 )
		return;

	// ignore if duplicated
	for( i = 0; i < uiStatic.numServers; i++ )
	{
		if( !stricmp( uiStatic.serverNames[i], info ))
			return;
	}

	// add it to the list
	uiStatic.updateServers = true; // info has been updated
	uiStatic.serverAddresses[uiStatic.numServers] = adr;
	strncpy( uiStatic.serverNames[uiStatic.numServers], info, 256 );
	uiStatic.serverPings[uiStatic.numServers] = Sys_DoubleTime() - uiStatic.serversRefreshTime;
	if( uiStatic.serverPings[uiStatic.numServers] < 0 || uiStatic.serverPings[uiStatic.numServers] > 9.999f )
		uiStatic.serverPings[uiStatic.numServers] = 9.999f;
	uiStatic.numServers++;
}

/*
=================
UI_MenuResetPing_f
=================
*/
void UI_MenuResetPing_f( void )
{
	Con_Printf("UI_MenuResetPing_f\n");
	uiStatic.serversRefreshTime = Sys_DoubleTime();
}

/*
=================
UI_IsVisible

Some systems may need to know if it is visible or not
=================
*/
int UI_IsVisible( void )
{
	if( !uiStatic.initialized )
		return false;
	return uiStatic.visible;
}

void UI_GetCursorPos( int *pos_x, int *pos_y )
{
	if( pos_x ) *pos_x = uiStatic.cursorX;
	if( pos_y ) *pos_y = uiStatic.cursorY;
}

void UI_SetCursorPos( int pos_x, int pos_y )
{
//	uiStatic.cursorX = bound( 0, pos_x, ScreenWidth );
//	uiStatic.cursorY = bound( 0, pos_y, ScreenHeight );
	uiStatic.mouseInRect = true;
}

void UI_ShowCursor( int show )
{
	uiStatic.hideCursor = (show) ? false : true;
}

int UI_MouseInRect( void )
{
	return uiStatic.mouseInRect;
}

/*
=================
UI_Precache
=================
*/
void UI_Precache( void )
{
	if( !uiStatic.initialized )
		return;

	if( !ui_precache->value )
		return;

	EngFuncs::PIC_Load( UI_LEFTARROW );
	EngFuncs::PIC_Load( UI_LEFTARROWFOCUS );
	EngFuncs::PIC_Load( UI_RIGHTARROW );
	EngFuncs::PIC_Load( UI_RIGHTARROWFOCUS );
	EngFuncs::PIC_Load( UI_UPARROW );
	EngFuncs::PIC_Load( UI_UPARROWFOCUS );
	EngFuncs::PIC_Load( UI_DOWNARROW );
	EngFuncs::PIC_Load( UI_DOWNARROWFOCUS );

	if( ui_precache->value == 1 )
		return;

	UI_Main_Precache();
	/*
	UI_NewGame_Precache();
	UI_LoadGame_Precache();
	UI_SaveGame_Precache();
	UI_SaveLoad_Precache();
	UI_MultiPlayer_Precache();
	UI_Options_Precache();
	UI_InternetGames_Precache();
	UI_LanGame_Precache();
	UI_PlayerSetup_Precache();
	UI_Controls_Precache();
	UI_AdvControls_Precache();
	UI_GameOptions_Precache();
	UI_CreateGame_Precache();
	UI_Audio_Precache();
	UI_Video_Precache();
	UI_VidOptions_Precache();
	UI_VidModes_Precache();
	UI_CustomGame_Precache();
	UI_Credits_Precache();
	UI_Touch_Precache();
	UI_TouchOptions_Precache();
	UI_TouchButtons_Precache();
	UI_TouchEdit_Precache();
	UI_FileDialog_Precache();
	UI_GamePad_Precache();
	*/
}

void UI_ParseColor( char *&pfile, int *outColor )
{
	int	i, color[3];
	char	token[1024];

	memset( color, 0xFF, sizeof( color ));

	for( i = 0; i < 3; i++ )
	{
		pfile = EngFuncs::COM_ParseFile( pfile, token );
		if( !pfile ) break;
		color[i] = atoi( token );
	}

	*outColor = PackRGB( color[0], color[1], color[2] );
}

void UI_ApplyCustomColors( void )
{
	char *afile = (char *)EngFuncs::COM_LoadFile( "gfx/shell/colors.lst" );
	char *pfile = afile;
	char token[1024];

	if( !afile )
	{
		// not error, not warning, just notify
		Con_Printf( "UI_SetColors: colors.lst not found\n" );
		return;
	}

	while(( pfile = EngFuncs::COM_ParseFile( pfile, token )) != NULL )
	{
		if( !stricmp( token, "HELP_COLOR" ))
		{
			UI_ParseColor( pfile, &uiColorHelp );
		}
		else if( !stricmp( token, "PROMPT_BG_COLOR" ))
		{
			UI_ParseColor( pfile, &uiPromptBgColor );
		}
		else if( !stricmp( token, "PROMPT_TEXT_COLOR" ))
		{
			UI_ParseColor( pfile, &uiPromptTextColor );
		}
		else if( !stricmp( token, "PROMPT_FOCUS_COLOR" ))
		{
			UI_ParseColor( pfile, &uiPromptFocusColor );
		}
		else if( !stricmp( token, "INPUT_TEXT_COLOR" ))
		{
			UI_ParseColor( pfile, &uiInputTextColor );
		}
		else if( !stricmp( token, "INPUT_BG_COLOR" ))
		{
			UI_ParseColor( pfile, &uiInputBgColor );
		}
		else if( !stricmp( token, "INPUT_FG_COLOR" ))
		{
			UI_ParseColor( pfile, &uiInputFgColor );
		}
		else if( !stricmp( token, "CON_TEXT_COLOR" ))
		{
			UI_ParseColor( pfile, &uiColorConsole );
		}
	}

	int	r, g, b;

	UnpackRGB( r, g, b, uiColorConsole );
	EngFuncs::SetConsoleDefaultColor( r, g, b );

	EngFuncs::COM_FreeFile( afile );
}

static void UI_LoadBackgroundMapList( void )
{
	if( !EngFuncs::FileExists( "scripts/chapterbackgrounds.txt", TRUE ))
		return;

	char *afile = (char *)EngFuncs::COM_LoadFile( "scripts/chapterbackgrounds.txt", NULL );
	char *pfile = afile;
	char token[1024];

	uiStatic.bgmapcount = 0;

	if( !afile )
	{
		Con_Printf( "UI_LoadBackgroundMapList: chapterbackgrounds.txt not found\n" );
		return;
	}

	while(( pfile = EngFuncs::COM_ParseFile( pfile, token )) != NULL )
	{
		// skip the numbers (old format list)
		if( isdigit( token[0] )) continue;

		strncpy( uiStatic.bgmaps[uiStatic.bgmapcount], token, sizeof( uiStatic.bgmaps[0] ));
		if( ++uiStatic.bgmapcount > UI_MAX_BGMAPS )
			break; // list is full
	}

	EngFuncs::COM_FreeFile( afile );
}

/*
=================
UI_VidInit
=================
*/
int UI_VidInit( void )
{
	static bool calledOnce = true;

	UI_Precache ();
	// Sizes are based on screen height
	uiStatic.scaleX = uiStatic.scaleY = ScreenHeight / 768.0f;
	uiStatic.width = ScreenWidth / uiStatic.scaleX;
	// move cursor to screen center
	uiStatic.cursorX = ScreenWidth >> 1;
	uiStatic.cursorY = ScreenHeight >> 1;
	uiStatic.outlineWidth = 4;
	uiStatic.sliderWidth = 6;

	// all menu buttons have the same view sizes
	uiStatic.buttons_draw_width = UI_BUTTONS_WIDTH;
	uiStatic.buttons_draw_height = UI_BUTTONS_HEIGHT;

	UI_ScaleCoords( NULL, NULL, &uiStatic.outlineWidth, NULL );
	UI_ScaleCoords( NULL, NULL, &uiStatic.sliderWidth, NULL );
	UI_ScaleCoords( NULL, NULL, &uiStatic.buttons_draw_width, &uiStatic.buttons_draw_height );

	// trying to load colors.lst
	UI_ApplyCustomColors ();

	// trying to load chapterbackgrounds.txt
	UI_LoadBackgroundMapList ();

	// register menu font
	uiStatic.hFont = EngFuncs::PIC_Load( "#XASH_SYSTEMFONT_001.bmp", menufont_bmp, sizeof( menufont_bmp ));

	UI_LoadBackgroundImage ();
#if 0
	FILE *f;

	// dump menufont onto disk
	f = fopen( "menufont.bmp", "wb" );
	fwrite( menufont_bmp, sizeof( menufont_bmp ), 1, f );
	fclose( f );
#endif

	// reload all menu buttons
	UI_LoadBmpButtons ();

	// now recalc all the menus in stack
	for( int i = 0; i < uiStatic.menuDepth; i++ )
	{
		CMenuFramework *item = uiStatic.menuStack[i];

		if( item )
		{
			int cursor, cursorPrev;
			bool valid = false;

			// HACKHACK: Save cursor values when VidInit is called once
			// this don't let menu "forget" actual cursor values after, for example, window resizing
			if( calledOnce
				&& item->GetCursor() > 0 // ignore 0, because useless
				&& item->GetCursor() < item->ItemCount()
				&& item->GetCursorPrev() > 0
				&& item->GetCursorPrev() < item->ItemCount() )
			{
				valid = true;
				cursor = item->GetCursor();
				cursorPrev = item->GetCursorPrev();
			}

			// do vid restart for all pushed elements
			item->VidInit();

			if( valid )
			{
				// don't notify menu widget about cursor changes
				item->SetCursor( cursor, false );
			}
		}
	}

	if( !calledOnce ) calledOnce = true;

	return 1;
}

/*
=================
UI_Init
=================
*/
void UI_Init( void )
{
	// register our cvars and commands
	ui_precache = EngFuncs::CvarRegister( "ui_precache", "0", FCVAR_ARCHIVE );
	ui_showmodels = EngFuncs::CvarRegister( "ui_showmodels", "0", FCVAR_ARCHIVE );

	// show cl_predict dialog
	EngFuncs::CvarRegister( "menu_mp_firsttime", "1", FCVAR_ARCHIVE );

	EngFuncs::Cmd_AddCommand( "menu_main", UI_Main_Menu );
	EngFuncs::Cmd_AddCommand( "menu_newgame", UI_NewGame_Menu );
	EngFuncs::Cmd_AddCommand( "menu_options", UI_Options_Menu );
	/*
	EngFuncs::Cmd_AddCommand( "menu_loadgame", UI_LoadGame_Menu );
	EngFuncs::Cmd_AddCommand( "menu_savegame", UI_SaveGame_Menu );
	EngFuncs::Cmd_AddCommand( "menu_saveload", UI_SaveLoad_Menu );
	EngFuncs::Cmd_AddCommand( "menu_multiplayer", UI_MultiPlayer_Menu );
	EngFuncs::Cmd_AddCommand( "menu_langame", UI_LanGame_Menu );
	EngFuncs::Cmd_AddCommand( "menu_internetgames", UI_InternetGames_Menu );
	EngFuncs::Cmd_AddCommand( "menu_playersetup", UI_PlayerSetup_Menu );
	EngFuncs::Cmd_AddCommand( "menu_controls", UI_Controls_Menu );
	EngFuncs::Cmd_AddCommand( "menu_advcontrols", UI_AdvControls_Menu );
	EngFuncs::Cmd_AddCommand( "menu_gameoptions", UI_GameOptions_Menu );
	EngFuncs::Cmd_AddCommand( "menu_creategame", UI_CreateGame_Menu );
	EngFuncs::Cmd_AddCommand( "menu_audio", UI_Audio_Menu );
	EngFuncs::Cmd_AddCommand( "menu_video", UI_Video_Menu );
	EngFuncs::Cmd_AddCommand( "menu_vidoptions", UI_VidOptions_Menu );
	EngFuncs::Cmd_AddCommand( "menu_vidmodes", UI_VidModes_Menu );
	EngFuncs::Cmd_AddCommand( "menu_customgame", UI_CustomGame_Menu );
	EngFuncs::Cmd_AddCommand( "menu_touch", UI_Touch_Menu );
	EngFuncs::Cmd_AddCommand( "menu_touchoptions", UI_TouchOptions_Menu );
	EngFuncs::Cmd_AddCommand( "menu_touchbuttons", UI_TouchButtons_Menu );
	EngFuncs::Cmd_AddCommand( "menu_touchedit", UI_TouchEdit_Menu );
	EngFuncs::Cmd_AddCommand( "menu_filedialog", UI_FileDialog_Menu );
	EngFuncs::Cmd_AddCommand( "menu_gamepad", UI_GamePad_Menu );
	EngFuncs::Cmd_AddCommand( "menu_resetping", UI_MenuResetPing_f );*/

	EngFuncs::CreateMapsList( TRUE );

	uiStatic.initialized = true;

	// setup game info
	EngFuncs::GetGameInfo( &gMenu.m_gameinfo );

	// load custom strings
	UI_LoadCustomStrings();

	//CR
	CMenuPicButton::InitTitleAnim();
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown( void )
{
	if( !uiStatic.initialized )
		return;

	EngFuncs::Cmd_RemoveCommand( "menu_main" );
	EngFuncs::Cmd_RemoveCommand( "menu_newgame" );
	EngFuncs::Cmd_RemoveCommand( "menu_loadgame" );
	EngFuncs::Cmd_RemoveCommand( "menu_savegame" );
	EngFuncs::Cmd_RemoveCommand( "menu_saveload" );
	EngFuncs::Cmd_RemoveCommand( "menu_multiplayer" );
	EngFuncs::Cmd_RemoveCommand( "menu_options" );
	EngFuncs::Cmd_RemoveCommand( "menu_langame" );
	EngFuncs::Cmd_RemoveCommand( "menu_internetgames" );
	EngFuncs::Cmd_RemoveCommand( "menu_playersetup" );
	EngFuncs::Cmd_RemoveCommand( "menu_controls" );
	EngFuncs::Cmd_RemoveCommand( "menu_advcontrols" );
	EngFuncs::Cmd_RemoveCommand( "menu_gameoptions" );
	EngFuncs::Cmd_RemoveCommand( "menu_creategame" );
	EngFuncs::Cmd_RemoveCommand( "menu_audio" );
	EngFuncs::Cmd_RemoveCommand( "menu_video" );
	EngFuncs::Cmd_RemoveCommand( "menu_vidoptions" );
	EngFuncs::Cmd_RemoveCommand( "menu_vidmodes" );
	EngFuncs::Cmd_RemoveCommand( "menu_customgame" );
	EngFuncs::Cmd_RemoveCommand( "menu_touch" );
	EngFuncs::Cmd_RemoveCommand( "menu_touchoptions" );
	EngFuncs::Cmd_RemoveCommand( "menu_touchbuttons" );
	EngFuncs::Cmd_RemoveCommand( "menu_touchedit" );
	EngFuncs::Cmd_RemoveCommand( "menu_filedialog" );
	EngFuncs::Cmd_RemoveCommand( "menu_gamepad" );

	memset( &uiStatic, 0, sizeof( uiStatic_t ));
}
