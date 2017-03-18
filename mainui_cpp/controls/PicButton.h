/*
PicButton.h - animated button with picture
Copyright (C) 2010 Uncle Mike
Copyright (C) 2017 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#ifndef MENU_PICBUTTON_H
#define MENU_PICBUTTON_H

#include "menu_btnsbmp_table.h"

// Use hover bitmap from btns_main.bmp instead of head_%s.bmp
// #define TA_ALT_MODE 1

class CMenuPicButton : public CMenuBaseItem
{
public:
	CMenuPicButton() : CMenuBaseItem()
	{
		EnableTransitions( true );
		EnablePulse( false );
		Flags() = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	}

	virtual void VidInit( void );
	virtual const char * Key( int key, int down );
	virtual void Draw( void );

	void SetPicture( int ID );
	void SetPicture( const char *filename );

	void EnableTransitions( bool enable ) { m_bEnableTrans = enable; }
	void EnablePulse( bool enable ) { m_bPulse = enable; }

	static void InitTitleAnim( void );
	static void DrawTitleAnim( void );
	static void PopPButtonStack( void );
	static void ClearButtonStack( void );
	static float GetTitleTransFraction( void );
	static void SetupTitleQuad( int x, int y, int w, int h );
private:
	enum animState_e { AS_TO_TITLE, AS_TO_BUTTON };
	struct Quad { float x, y, lx, ly; };

	void SetTitleAnim( int state );
	void TACheckMenuDepth( void );
	void PushPButtonStack( void );

	HIMAGE pic;
	int button_id;
	bool m_bEnableTrans;
	bool m_bPulse;

	static Quad LerpQuad( Quad a, Quad b, float frac );

	static int PreClickDepth;
	static int transition_initial_time;
	static Quad TitleLerpQuads[2];
	static int transition_state;
	static HIMAGE TransPic;
	static bool hold_button_stack;
};

#endif
