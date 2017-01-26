/*
console.c - developer console
Copyright (C) 2007 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#ifndef XASH_DEDICATED
#include "common.h"
#include "client.h"
#include "keydefs.h"
#include "protocol.h"		// get the protocol version
#include "gl_local.h"
#include "qfont.h"
#include "server.h" // Log_Printf( , ... )

convar_t	*con_notifytime;
convar_t	*scr_conspeed;
convar_t	*con_fontsize;
convar_t	*con_maxfrac;
convar_t	*con_halffrac;
convar_t	*con_charset;
convar_t	*con_alpha;
convar_t	*con_black;
convar_t	*con_fontscale;
convar_t	*con_fontnum;

static int g_codepage = 0;
static qboolean g_utf8 = false;

#define CON_TIMES		4	// notify lines
#define COLOR_DEFAULT	'7'
#define CON_HISTORY		64
#define MAX_DBG_NOTIFY	128
#define CON_NUMFONTS	3	// maxfonts

#define CON_LINES( i )	(con.lines[(con.lines_first + (i)) % con.maxlines])
#define CON_LINES_COUNT	con.lines_count
#define CON_LINES_LAST()	CON_LINES( CON_LINES_COUNT - 1 )
#define CON_MAXCMDS		4096	// auto-complete intermediate list

#define CON_TEXTSIZE	1048576	// max scrollback buffer characters in console (1 Mb)
#define CON_MAXLINES	16384	// max scrollback buffer lines in console

// console color typeing
rgba_t g_color_table[8] =
{
{   0,   0,   0, 255 },	// black
{ 255,   0,   0, 255 },	// red
{   0, 255,   0, 255 },	// green
{ 255, 255,   0, 255 },	// yellow
{   0,   0, 255, 255 },	// blue
{   0, 255, 255, 255 },	// cyan
{ 255,   0, 255, 255 },	// magenta
{ 240, 180,  24, 255 },	// default color (can be changed by user)
};

typedef struct
{
	string		szNotify;
	float		expire;
	rgba_t		color;
	int		key_dest;
} notify_t;

typedef struct con_lineinfo_s
{
	char		*start;
	size_t		length;
	double		addtime;		// notify stuff
} con_lineinfo_t;

typedef struct
{
	qboolean		initialized;

	// conbuffer
	char		*buffer;		// common buffer for all console lines
	int		bufsize;		// CON_TEXSIZE
	con_lineinfo_t	*lines;		// console lines
	int		maxlines;		// CON_MAXLINES

	int		lines_first;	// cyclic buffer
	int		lines_count;

	// console scroll
	int		backscroll;	// lines up from bottom to display
	int 		linewidth;	// characters across screen

	// console animation
	int		showlines;	// how many lines we should display
	int		vislines;		// in scanlines

	// console images
	int		background;	// console background

	// console fonts
	cl_font_t		chars[CON_NUMFONTS];// fonts.wad/font1.fnt
	cl_font_t		*curFont, *lastUsedFont;

	// console input
	field_t		input;

	// chatfiled
	field_t		chat;
	string		chat_cmd;		// can be overrieded by user

	// console history
	field_t		historyLines[CON_HISTORY];
	int		historyLine;	// the line being displayed from history buffer will be <= nextHistoryLine
	int		nextHistoryLine;	// the last line in the history buffer, not masked

	notify_t		notify[MAX_DBG_NOTIFY]; // for Con_NXPrintf
	qboolean		draw_notify;	// true if we have NXPrint message

	// console auto-complete
	string		shortestMatch;
	field_t		*completionField;	// con.input or dedicated server fake field-line
	char		*completionString;
	char		*completionBuffer;
	char		*cmds[CON_MAXCMDS];
	int		matchCount;
} console_t;

static console_t		con;

void Field_CharEvent( field_t *edit, int ch );

/*
================
Con_Clear_f
================
*/
void Con_Clear( void )
{
	con.lines_count = 0;
	con.backscroll = 0; // go to end
}

/*
================
Con_SetColor_f
================
*/
void Con_SetColor_f( void )
{
	vec3_t	color;

	switch( Cmd_Argc() )
	{
	case 1:
		Msg( "\"con_color\" is %i %i %i\n", g_color_table[7][0], g_color_table[7][1], g_color_table[7][2] );
		break;
	case 2:
		VectorSet( color, g_color_table[7][0], g_color_table[7][1], g_color_table[7][2] );
		Q_atov( color, Cmd_Argv( 1 ), 3 );
		Con_DefaultColor( color[0], color[1], color[2] );
		break;
	case 4:
		VectorSet( color, Q_atof( Cmd_Argv( 1 )), Q_atof( Cmd_Argv( 2 )), Q_atof( Cmd_Argv( 3 )));
		Con_DefaultColor( color[0], color[1], color[2] );
		break;
	default:
		Msg( "Usage: con_color \"r g b\"\n" );
		break;
	}
}
						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void )
{
	int	i;
	
	for( i = 0; i < CON_LINES_COUNT; i++ )
		CON_LINES( i ).addtime = 0.0;
}

/*
================
Con_ClearField
================
*/
void Con_ClearField( field_t *edit )
{
	Q_memset( edit->buffer, 0, MAX_STRING );
	edit->cursor = 0;
	edit->scroll = 0;
}

/*
================
Con_ClearTyping
================
*/
void Con_ClearTyping( void )
{
	int	i;

	Con_ClearField( &con.input );
	con.input.widthInChars = con.linewidth;

	Con_ClearAutoComplete();
}

/*
============
Con_StringLength

skipped color prefixes
============
*/
int Con_StringLength( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;

	while( *p )
	{
		if( IsColorString( p ))
		{
			p += 2;
			continue;
		}
		len++;
		p++;
	}

	return len;
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f( void )
{
	if( Cmd_Argc() == 2 )
		Q_strncpy( con.chat_cmd, Cmd_Argv( 1 ), sizeof( con.chat_cmd ));
	else Q_strncpy( con.chat_cmd, "say", sizeof( con.chat_cmd ));

	Key_SetKeyDest( key_message );
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f( void )
{
	Q_strncpy( con.chat_cmd, "say_team", sizeof( con.chat_cmd ));
	Key_SetKeyDest( key_message );
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void )
{
	if( !host.developer || UI_CreditsActive( ))
		return; // disabled

	// show console only in game or by special call from menu
	if( cls.state != ca_active || cls.key_dest == key_menu )
		return;

	Con_ClearTyping();
	Con_ClearNotify();

	if( cls.key_dest == key_console )
	{
		if( Cvar_VariableInteger( "sv_background" ) || Cvar_VariableInteger( "cl_background" ))
			UI_SetActiveMenu( true );
		else UI_SetActiveMenu( false );
	}
	else
	{
		UI_SetActiveMenu( false );
		Key_SetKeyDest( key_console );
	}
}

/*
================
Con_FixTimes

Notifies the console code about the current time
(and shifts back times of other entries when the time
went backwards)
================
*/
void Con_FixTimes( void )
{
	double	diff;
	int	i;

	if( con.lines_count <= 0 ) return;

	diff = cl.time - CON_LINES_LAST().addtime;
	if( diff >= 0.0 ) return; // nothing to fix

	for( i = 0; i < con.lines_count; i++ )
		CON_LINES( i ).addtime += diff;
}

/*
================
Con_DeleteLine

Deletes the first line from the console history.
================
*/
void Con_DeleteLine( void )
{
	if( con.lines_count == 0 )
		return;
	con.lines_count--;
	con.lines_first = (con.lines_first + 1) % con.maxlines;
}

/*
================
Con_DeleteLastLine

Deletes the last line from the console history.
================
*/
void Con_DeleteLastLine( void )
{
	if( con.lines_count == 0 )
		return;
	con.lines_count--;
}

/*
================
Con_BytesLeft

Checks if there is space for a line of the given length, and if yes, returns a
pointer to the start of such a space, and NULL otherwise.
================
*/
static char *Con_BytesLeft( int length )
{
	if( length > con.bufsize )
		return NULL;

	if( con.lines_count == 0 )
	{
		return con.buffer;
	}
	else
	{
		char	*firstline_start = con.lines[con.lines_first].start;
		char	*lastline_onepastend = CON_LINES_LAST().start + CON_LINES_LAST().length;

		// the buffer is cyclic, so we first have two cases...
		if( firstline_start < lastline_onepastend ) // buffer is contiguous
		{
			// put at end?
			if( length <= con.buffer + con.bufsize - lastline_onepastend )
				return lastline_onepastend;
			// put at beginning?
			else if( length <= firstline_start - con.buffer )
				return con.buffer;

			return NULL;
		}
		else
		{
			// buffer has a contiguous hole
			if( length <= firstline_start - lastline_onepastend )
				return lastline_onepastend;

			return NULL;
		}
	}
}

/*
================
Con_AddLine

Appends a given string as a new line to the console.
================
*/
void Con_AddLine( const char *line, int length )
{
	byte		*putpos;
	con_lineinfo_t	*p;

	if( !con.initialized ) return;

	Con_FixTimes();
	length++;	// reserve space for term

	ASSERT( length < CON_TEXTSIZE );

	while( !( putpos = Con_BytesLeft( length )) || con.lines_count >= con.maxlines )
		Con_DeleteLine();

	Q_memcpy( putpos, line, length );
	putpos[length - 1] = '\0';
	con.lines_count++;

	p = &CON_LINES_LAST();
	p->start = putpos;
	p->length = length;
	p->addtime = cl.time;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize( void )
{
	int	charWidth = 8;
	int	i, width;

	if( con.curFont && con.curFont->hFontTexture )
		charWidth = con.curFont->charWidths['M'] - 1;

	width = ( scr_width->integer / charWidth ) - 2;
	if( !glw_state.initialized ) width = 78;

	if( width == con.linewidth )
		return;

	Con_ClearNotify();
	con.linewidth = width;
	con.backscroll = 0;

	con.input.widthInChars = con.linewidth;

	for( i = 0; i < CON_HISTORY; i++ )
		con.historyLines[i].widthInChars = con.linewidth;
}

/*
================
Con_PageUp
================
*/
void Con_PageUp( int lines )
{
	con.backscroll += abs( lines );
}

/*
================
Con_PageDown
================
*/
void Con_PageDown( int lines )
{
	con.backscroll -= abs( lines );
}

/*
================
Con_Top
================
*/
void Con_Top( void )
{
	con.backscroll = CON_MAXLINES;
}

/*
================
Con_Bottom
================
*/
void Con_Bottom( void )
{
	con.backscroll = 0;
}

/*
================
Con_Visible
================
*/
qboolean GAME_EXPORT Con_Visible( void )
{
	return (con.vislines > 0);
}

/*
================
Con_LoadConsoleFont
================
*/
static void Con_LoadConsoleFont( int fontNumber, cl_font_t *font )
{
	int	fontWidth;
	const char *path = NULL;
	byte	*buffer;
	fs_offset_t	length;
	qfont_t	*src;
	dword crc = 0;

	if( font->valid ) return; // already loaded
	// replace default fonts.wad textures by current charset's font
	if( !CRC32_File( &crc, "fonts.wad" ) || crc == 0x3c0a0029 )
	{
		const char *path2 = va("font%i_%s.fnt", fontNumber, Cvar_VariableString( "con_charset" ) );
		if( FS_FileExists( path2, false ) )
			path = path2;
	}

	if( !path )
		path = va( "fonts.wad/font%i", fontNumber );

	// loading conchars
	font->hFontTexture = GL_LoadTexture( path, NULL, 0, TF_FONT|TF_NEAREST, NULL );
	R_GetTextureParms( &fontWidth, NULL, font->hFontTexture );
	
	if( fontWidth == 0 ) return;

	// half-life font with variable chars witdh
	buffer = FS_LoadFile( path, &length, false );

	if( buffer && length >= ( fs_offset_t )sizeof( qfont_t ))
	{
		int	i;

		src = (qfont_t *)buffer;
		font->charHeight = src->rowheight * con_fontscale->value;

		// build rectangles
		for( i = 0; i < 256; i++ )
		{
			font->fontRc[i].left = (word)src->fontinfo[i].startoffset % fontWidth;
			font->fontRc[i].right = font->fontRc[i].left + src->fontinfo[i].charwidth;
			font->fontRc[i].top = (word)src->fontinfo[i].startoffset / fontWidth;
			font->fontRc[i].bottom = font->fontRc[i].top + src->rowheight;
			font->charWidths[i] = src->fontinfo[i].charwidth * con_fontscale->value;
		}
		font->valid = true;
	}
	if( buffer ) Mem_Free( buffer );
}

/*
================
Con_LoadConchars
================
*/
static void Con_LoadConchars( void )
{
	int	i, fontSize;

	// load all the console fonts
	for( i = 0; i < 3; i++ )
		Con_LoadConsoleFont( i, con.chars + i );

	// select properly fontsize
	if( con_fontnum->integer >= 0 && con_fontnum->integer <= 2)
		fontSize = con_fontnum->integer;
	else if( scr_width->integer <= 640 )
		fontSize = 0;
	else if( scr_width->integer >= 1280 )
		fontSize = 2;
	else fontSize = 1;

	// sets the current font
	con.lastUsedFont = con.curFont = &con.chars[fontSize];
	
}

// CP1251 table

int table_cp1251[64] = {
	0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
	0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
	0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x007F, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
	0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
	0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
	0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
	0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457
};

/*
============================
Con_UtfProcessChar

Convert utf char to current font's single-byte encoding
============================
*/
int Con_UtfProcessCharForce( int in )
{
	static int m = -1, k = 0; //multibyte state
	static int uc = 0; //unicode char
	
	if( !in )
	{
		m = -1;
		k = 0;
		uc = 0;
		return 0;
	}

	// Get character length
	if(m == -1)
	{
		uc = 0;
		if( in >= 0xF8 )
			return 0;
		else if( in >= 0xF0 )
			uc = in & 0x07, m = 3;
		else if( in >= 0xE0 )
			uc = in & 0x0F, m = 2;
		else if( in >= 0xC0 )
			uc = in & 0x1F, m = 1;
		else if( in <= 0x7F)
			return in; //ascii
		// return 0 if we need more chars to decode one
		k=0;
		return 0;
	}
	// get more chars
	else if( k <= m )
	{
		uc <<= 6;
		uc += in & 0x3F;
		k++;
	}
	if( in > 0xBF || m < 0 )
	{
		m = -1;
		return 0;
	}
	if( k == m )
	{
		k = m = -1;
		if( g_codepage == 1251 )
		{
			// cp1251 now
			if( uc >= 0x0410 && uc <= 0x042F )
				return uc - 0x410 + 0xC0;
			if( uc >= 0x0430 && uc <= 0x044F )
				return uc - 0x430 + 0xE0;
			else
			{
				int i;
				for( i = 0; i < 64; i++ )
					if( table_cp1251[i] == uc )
						return i + 0x80;
			}
		}
		else if( g_codepage == 1252 )
		{
			if( uc < 255 )
				return uc;
		}

		// not implemented yet
		return '?';
	}
	return 0;
}

int GAME_EXPORT Con_UtfProcessChar( int in )
{
	if( !g_utf8 )
		return in;
	else
		return Con_UtfProcessCharForce( in );
}
/*
=================
Con_UtfMoveLeft

get position of previous printful char
=================
*/
int Con_UtfMoveLeft( char *str, int pos )
{
	int i, j, k = 0;
	if( !g_utf8 )
		return pos - 1;
	Con_UtfProcessChar( 0 );
	if(pos == 1) return 0;
	for( i = 0; i < pos-1; i++ )
		if( Con_UtfProcessChar( (unsigned char)str[i] ) )
			k = i+1;
	Con_UtfProcessChar( 0 );
	return k;
}

/*
====================
Con_TextAdjustSize

draw charcters routine
====================
*/
static void Con_TextAdjustSize( int *x, int *y, int *w, int *h )
{
	float	xscale, yscale;

	if( !x && !y && !w && !h ) return;

	// scale for screen sizes
	xscale = scr_width->integer / (float)clgame.scrInfo.iWidth;
	yscale = scr_height->integer / (float)clgame.scrInfo.iHeight;

	if( x ) *x *= xscale;
	if( y ) *y *= yscale;
	if( w ) *w *= xscale;
	if( h ) *h *= yscale;
}

/*
=================
Con_UtfMoveRight

get next of previous printful char
=================
*/
int Con_UtfMoveRight( char *str, int pos, int length )
{
	int i;
	if( !g_utf8 )
		return pos + 1;
	Con_UtfProcessChar( 0 );
	for( i = pos; i <= length; i++ )
	{
		if( Con_UtfProcessChar( (unsigned char)str[i] ) )
			return i+1;
	}
	Con_UtfProcessChar( 0 );
	return pos+1;
}



int Con_DrawGenericChar( int x, int y, int number, rgba_t color )
{
	int	width, height;
	float	s1, t1, s2, t2;
	wrect_t	*rc;

	number &= 255;

	if( !con.curFont || !con.curFont->valid )
		return 0;

	number = Con_UtfProcessChar(number);
	if( number < 32 )
		return 0;
	if( y < -con.curFont->charHeight )
		return 0;

	rc = &con.curFont->fontRc[number];

	pglColor4ubv( color );
	R_GetTextureParms( &width, &height, con.curFont->hFontTexture );

	// calc rectangle
	s1 = (float)rc->left / width;
	t1 = (float)rc->top / height;
	s2 = (float)rc->right / width;
	t2 = (float)rc->bottom / height;
	width = (rc->right - rc->left) * con_fontscale->value;
	height = (rc->bottom - rc->top) * con_fontscale->value;

	if( clgame.ds.adjust_size )
		Con_TextAdjustSize( &x, &y, &width, &height );
	R_DrawStretchPic( x, y, width, height, s1, t1, s2, t2, con.curFont->hFontTexture );		
	pglColor4ub( 255, 255, 255, 255 ); // don't forget reset color

	return con.curFont->charWidths[number];
}

/*
====================
Con_SetFont

choose font size
====================
*/
void Con_SetFont( int fontNum )
{
	fontNum = bound( 0, fontNum, 2 );
	con.curFont = &con.chars[fontNum];
}

/*
====================
Con_RestoreFont

restore auto-selected console font
(that based on screen resolution)
====================
*/
void Con_RestoreFont( void )
{
	con.curFont = con.lastUsedFont;
}

/*
====================
Con_DrawCharacter

client version of routine
====================
*/
int Con_DrawCharacter( int x, int y, int number, rgba_t color )
{
	GL_SetRenderMode( kRenderTransTexture );
	return Con_DrawGenericChar( x, y, number, color );
}

/*
====================
Con_DrawCharacterLen

returns character sizes in screen pixels
====================
*/
void Con_DrawCharacterLen( int number, int *width, int *height )
{
	if( width && con.curFont ) *width = con.curFont->charWidths[number];
	if( height && con.curFont ) *height = con.curFont->charHeight;
}

/*
====================
Con_DrawStringLen

compute string width and height in screen pixels
====================
*/
void Con_DrawStringLen( const char *pText, int *length, int *height )
{
	int	curLength = 0;

	if( !con.curFont ) return;

	if( height ) *height = con.curFont->charHeight;
	if( !length ) return;

	*length = 0;

	while( *pText )
	{
		byte	c = *pText;

		if( *pText == '\n' )
		{
			pText++;
			curLength = 0;
		}

		// skip color strings they are not drawing
		if( IsColorString( pText ))
		{
			pText += 2;
			continue;
		}

		c = Con_UtfProcessChar( c );

		if( c )
			curLength += con.curFont->charWidths[c];

		pText++;

		if( curLength > *length )
			*length = curLength;
	}
}

/*
==================
Con_DrawString

Draws a multi-colored string, optionally forcing
to a fixed color.
==================
*/
int Con_DrawGenericString( int x, int y, const char *string, rgba_t setColor, qboolean forceColor, int hideChar )
{
	rgba_t		color;
	int		drawLen = 0;
	int		numDraws = 0;
	const char	*s;

	if( !con.curFont ) return 0; // no font set

	// draw the colored text
	*(uint *)color = *(uint *)setColor;
	s = string;

	while( *s )
	{
		if( *s == '\n' )
		{
			s++;
			if( !*s ) break; // at end the string
			drawLen = 0; // begin new row
			y += con.curFont->charHeight;
		}

		if( IsColorString( s ))
		{
			if( !forceColor )
			{
				Q_memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ));
				color[3] = setColor[3];
			}

			s += 2;
			numDraws++;
			continue;
		}

		// hide char for overstrike mode
		if( hideChar == numDraws )
			drawLen += con.curFont->charWidths[*s];
		else drawLen += Con_DrawCharacter( x + drawLen, y, *s, color );

		numDraws++;
		s++;
	}

	pglColor4ub( 255, 255, 255, 255 );
	return drawLen;
}

/*
====================
Con_DrawString

client version of routine
====================
*/
int Con_DrawString( int x, int y, const char *string, rgba_t setColor )
{
	return Con_DrawGenericString( x, y, string, setColor, false, -1 );
}

/*
================
Con_Init
================
*/
void Con_Init( void )
{
	int	i;

	// must be init before startup video subsystem
	scr_width = Cvar_Get( "width", "640", CVAR_RENDERINFO, "screen width" );
	scr_height = Cvar_Get( "height", "480", CVAR_RENDERINFO, "screen height" );
	scr_conspeed = Cvar_Get( "scr_conspeed", "600", CVAR_ARCHIVE, "console moving speed" );
	con_notifytime = Cvar_Get( "con_notifytime", "3", CVAR_ARCHIVE, "notify time to live" );
	con_fontsize = Cvar_Get( "con_fontsize", "1", CVAR_ARCHIVE, "chat or client font number (0, 1 or 2)" );
	con_maxfrac = Cvar_Get( "con_maxfrac", "1.0", CVAR_ARCHIVE, "console max height" );
	con_halffrac = Cvar_Get( "con_halffrac", "0.5", CVAR_ARCHIVE, "console half height" );
	con_charset = Cvar_Get( "con_charset", "cp1251", CVAR_ARCHIVE, "console font charset (only cp1251 supported now)" );
	con_alpha = Cvar_Get( "con_alpha", "1.0", CVAR_ARCHIVE, "console alpha value" );
	con_black = Cvar_Get( "con_black", "0", CVAR_ARCHIVE, "make console black like a nigga" );
	con_fontscale = Cvar_Get( "con_fontscale", "1.0", CVAR_ARCHIVE, "scale font texture" );
	con_fontnum = Cvar_Get( "con_fontnum", "-1", CVAR_ARCHIVE, "console font number (0, 1 or 2), -1 for autoselect" );

	// init the console buffer
	con.bufsize = CON_TEXTSIZE;
	con.buffer = (char *)Z_Malloc( con.bufsize );
	con.maxlines = CON_MAXLINES;
	con.lines = (con_lineinfo_t *)Z_Malloc( con.maxlines * sizeof( *con.lines ));
	con.lines_first = con.lines_count = 0;

	Con_CheckResize();

	Con_ClearField( &con.input );
	con.input.widthInChars = con.linewidth;

	Con_ClearField( &con.chat );
	con.chat.widthInChars = con.linewidth;

	for( i = 0; i < CON_HISTORY; i++ )
	{
		Con_ClearField( &con.historyLines[i] );
		con.historyLines[i].widthInChars = con.linewidth;
	}

	Con_ClearAutoComplete();

	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f, "opens or closes the console" );
	Cmd_AddCommand( "con_color", Con_SetColor_f, "set a custom console color" );
	Cmd_AddCommand( "messagemode", Con_MessageMode_f, "enable message mode \"say\"" );
	Cmd_AddCommand( "messagemode2", Con_MessageMode2_f, "enable message mode \"say_team\"" );

	MsgDev( D_NOTE, "Console initialized.\n" );
	con.initialized = true;
}


/*
================
Con_Shutdown
================
*/
void Con_Shutdown( void )
{
	con.initialized = false;

	if( con.buffer )
		Mem_Free( con.buffer );

	if( con.lines )
		Mem_Free( con.lines );

	con.buffer = NULL;
	con.lines = NULL;
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be displayed
If no console is visible, the notify window will pop up.
================
*/
void Con_Print( const char *txt )
{
	static int	cr_pending = 0;
	static char	buf[MAX_PRINT_MSG];
	static int	bufpos = 0;

	// client not running
	if( !con.initialized || !con.buffer || host.type == HOST_DEDICATED )
		return;

	for( ; *txt; txt++ )
	{
		if( cr_pending )
		{
			Con_DeleteLastLine();
			cr_pending = 0;
		}

		switch( *txt )
		{
		case '\0':
			break;
		case '\r':
			Con_AddLine( buf, bufpos );
			cr_pending = 1;
			bufpos = 0;
			break;
		case '\n':
			Con_AddLine( buf, bufpos );
			bufpos = 0;
			break;
		default:
			buf[bufpos++] = *txt;
			if(( bufpos >= sizeof( buf ) - 1 ) || bufpos >= ( con.linewidth - 1 ))
			{
				Con_AddLine( buf, bufpos );
				bufpos = 0;
			}
			break;
		}
	}
}

/*
================
Con_NPrint

Draw a single debug line with specified height
================
*/
void GAME_EXPORT Con_NPrintf( int idx, char *fmt, ... )
{
	va_list	args;

	if( idx < 0 || idx >= MAX_DBG_NOTIFY )
		return;

	Q_memset( con.notify[idx].szNotify, 0, MAX_STRING );

	va_start( args, fmt );
	Q_vsnprintf( con.notify[idx].szNotify, MAX_STRING, fmt, args );
	va_end( args );

	// reset values
	con.notify[idx].key_dest = key_game;
	con.notify[idx].expire = host.realtime + 4.0f;
	MakeRGBA( con.notify[idx].color, 255, 255, 255, 255 );
	con.draw_notify = true;
}

/*
================
Con_NXPrint

Draw a single debug line with specified height, color and time to live
================
*/
void GAME_EXPORT Con_NXPrintf( con_nprint_t *info, char *fmt, ... )
{
	va_list	args;

	if( !info ) return;

	if( info->index < 0 || info->index >= MAX_DBG_NOTIFY )
		return;

	Q_memset( con.notify[info->index].szNotify, 0, MAX_STRING );

	va_start( args, fmt );
	Q_vsnprintf( con.notify[info->index].szNotify, MAX_STRING, fmt, args );
	va_end( args );

	// setup values
	con.notify[info->index].key_dest = key_game;
	con.notify[info->index].expire = host.realtime + info->time_to_live;
	MakeRGBA( con.notify[info->index].color, (byte)(info->color[0] * 255), (byte)(info->color[1] * 255), (byte)(info->color[2] * 255), 255 );
	con.draw_notify = true;
}

/*
================
UI_NPrint

Draw a single debug line with specified height (menu version)
================
*/
void UI_NPrintf( int idx, char *fmt, ... )
{
	va_list	args;

	if( idx < 0 || idx >= MAX_DBG_NOTIFY )
		return;

	Q_memset( con.notify[idx].szNotify, 0, MAX_STRING );

	va_start( args, fmt );
	Q_vsnprintf( con.notify[idx].szNotify, MAX_STRING, fmt, args );
	va_end( args );

	// reset values
	con.notify[idx].key_dest = key_menu;
	con.notify[idx].expire = host.realtime + 4.0f;
	MakeRGBA( con.notify[idx].color, 255, 255, 255, 255 );
	con.draw_notify = true;
}

/*
================
UI_NXPrint

Draw a single debug line with specified height, color and time to live (menu version)
================
*/
void UI_NXPrintf( con_nprint_t *info, char *fmt, ... )
{
	va_list	args;

	if( !info ) return;

	if( info->index < 0 || info->index >= MAX_DBG_NOTIFY )
		return;

	Q_memset( con.notify[info->index].szNotify, 0, MAX_STRING );

	va_start( args, fmt );
	Q_vsnprintf( con.notify[info->index].szNotify, MAX_STRING, fmt, args );
	va_end( args );

	// setup values
	con.notify[info->index].key_dest = key_menu;
	con.notify[info->index].expire = host.realtime + info->time_to_live;
	MakeRGBA( con.notify[info->index].color, (byte)(info->color[0] * 255), (byte)(info->color[1] * 255), (byte)(info->color[2] * 255), 255 );
	con.draw_notify = true;
}

/*
=============================================================================

EDIT FIELDS

=============================================================================
*/

/*
===============
Cmd_CheckName

compare first argument with string
===============
*/
static qboolean Cmd_CheckName( const char *name )
{
	if( !Q_stricmp( Cmd_Argv( 0 ), name ))
		return true;
	if( !Q_stricmp( Cmd_Argv( 0 ), va( "\\%s", name )))
		return true;
	return false;
}

/*
===============
Con_AddCommandToList

===============
*/
static void Con_AddCommandToList( const char *s, const char *unused1, const char *unused2, void *unused3 )
{
	if( *s == '@' ) return; // never show system cvars or cmds
	if( con.matchCount >= CON_MAXCMDS ) return; // list is full

	if( Q_strnicmp( s, con.completionString, Q_strlen( con.completionString )))
		return; // no match

	con.cmds[con.matchCount++] = copystring( s );
}

/*
=================
Con_SortCmds
=================
*/
static int Con_SortCmds( const void *arg1, const void*arg2 )
{
	return Q_stricmp( *(char**)arg1, *(char**)arg2 );
}

/*
===============
Con_PrintMatches
===============
*/
static void Con_PrintMatches( const char *s, const char *unused1, const char *m, void *unused2 )
{
	if( !Q_strnicmp( s, con.shortestMatch, Q_strlen( con.shortestMatch )))
	{
		if( m && *m ) Msg( "    %s ^3\"%s\"\n", s, m );
		else Msg( "    %s\n", s ); // variable or command without description
	}
}

/*
===============
Con_ConcatRemaining
===============
*/
static void Con_ConcatRemaining( const char *src, const char *start )
{
	char	*arg;
	int	i;

	arg = Q_strstr( src, start );

	if( !arg )
	{
		for( i = 1; i < Cmd_Argc(); i++ )
		{
			Q_strncat( con.completionField->buffer, " ", sizeof( con.completionField->buffer ));
			arg = Cmd_Argv( i );
			while( *arg )
			{
				if( *arg == ' ' )
				{
					Q_strncat( con.completionField->buffer, "\"", sizeof( con.completionField->buffer ));
					break;
				}
				arg++;
			}

			Q_strncat( con.completionField->buffer, Cmd_Argv( i ), sizeof( con.completionField->buffer ));
			if( *arg == ' ' ) Q_strncat( con.completionField->buffer, "\"", sizeof( con.completionField->buffer ));
		}
		return;
	}

	arg += Q_strlen( start );
	Q_strncat( con.completionField->buffer, arg, sizeof( con.completionField->buffer ));
}

/*
================
Field_Paste
================
*/
void Field_Paste( field_t *edit )
{
// I guess we don't have the ability to paste without SDL.
#ifdef XASH_SDL
	char	*cbd;
	int	i, pasteLen;

	cbd = Sys_GetClipboardData();
	if( !cbd ) return;

	// send as if typed, so insert / overstrike works properly
	pasteLen = Q_strlen( cbd );
	for( i = 0; i < pasteLen; i++ )
		Field_CharEvent( edit, cbd[i] );
	SDL_free( cbd );
#endif
}

/*
=================
Field_KeyDownEvent

Performs the basic line editing functions for the console,
in-game talk, and menu fields

Key events are used for non-printable characters, others are gotten from char events.
=================
*/
void Field_KeyDownEvent( field_t *edit, int key )
{
	int	len;

	// shift-insert is paste
	if((( key == K_INS ) || ( key == K_KP_INS )) && Key_IsDown( K_SHIFT ))
	{
		Field_Paste( edit );
		return;
	}

	len = Q_strlen( edit->buffer );

	if( key == K_DEL )
	{
		if( edit->cursor < len )
			Q_memmove( edit->buffer + edit->cursor, edit->buffer + edit->cursor + 1, len - edit->cursor );
		return;
	}

	if( key == K_BACKSPACE )
	{
		if( edit->cursor > 0 )
		{
			int newcursor = Con_UtfMoveLeft( edit->buffer, edit->cursor );
			Q_memmove( edit->buffer + newcursor, edit->buffer + edit->cursor, len - edit->cursor + 1 );
			edit->cursor = newcursor;
			if( edit->scroll ) edit->scroll--;
		}
		return;
	}

	if( key == K_RIGHTARROW ) 
	{
		if( edit->cursor < len ) edit->cursor = Con_UtfMoveRight( edit->buffer, edit->cursor, edit->widthInChars );
		if( edit->cursor >= edit->scroll + edit->widthInChars && edit->cursor <= len )
			edit->scroll++;
		return;
	}

	if( key == K_LEFTARROW ) 
	{
		if( edit->cursor > 0 ) edit->cursor= Con_UtfMoveLeft( edit->buffer, edit->cursor );
		if( edit->cursor < edit->scroll ) edit->scroll--;
		return;
	}

	if( key == K_HOME || ( Q_tolower(key) == 'a' && Key_IsDown( K_CTRL )))
	{
		edit->cursor = 0;
		return;
	}

	if( key == K_END || ( Q_tolower(key) == 'e' && Key_IsDown( K_CTRL )))
	{
		edit->cursor = len;
		return;
	}

	if( key == K_INS )
	{
		host.key_overstrike = !host.key_overstrike;
		return;
	}
}

/*
==================
Field_CharEvent
==================
*/
void Field_CharEvent( field_t *edit, int ch )
{
	int	len;

	if( ch == 'v' - 'a' + 1 )
	{
		// ctrl-v is paste
		Field_Paste( edit );
		return;
	}

	if( ch == 'c' - 'a' + 1 )
	{
		// ctrl-c clears the field
		Con_ClearField( edit );
		return;
	}

	len = Q_strlen( edit->buffer );

	if( ch == 'a' - 'a' + 1 )
	{
		// ctrl-a is home
		edit->cursor = 0;
		edit->scroll = 0;
		return;
	}

	if( ch == 'e' - 'a' + 1 )
	{
		// ctrl-e is end
		edit->cursor = len;
		edit->scroll = edit->cursor - edit->widthInChars;
		return;
	}

	// ignore any other non printable chars
	//if( ch < 32 ) return;

	if( host.key_overstrike )
	{	
		if ( edit->cursor == MAX_STRING - 1 ) return;
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	}
	else
	{
		// insert mode
		if ( len == MAX_STRING - 1 ) return; // all full
		Q_memmove( edit->buffer + edit->cursor + 1, edit->buffer + edit->cursor, len + 1 - edit->cursor );
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	}

	if( edit->cursor >= edit->widthInChars ) edit->scroll++;
	if( edit->cursor == len + 1 ) edit->buffer[edit->cursor] = 0;
}

/*
==================
Field_DrawInputLine
==================
*/
void Field_DrawInputLine( int x, int y, field_t *edit )
{
	int	len, cursorChar;
	int	drawLen, hideChar = -1;
	int	prestep, curPos = 0;
	char	str[MAX_SYSPATH];
	byte	*colorDefault;

	drawLen = edit->widthInChars;
	len = Q_strlen( edit->buffer ) + 1;
	colorDefault = g_color_table[ColorIndex( COLOR_DEFAULT )];

	// guarantee that cursor will be visible
	if( len <= drawLen )
	{
		prestep = 0;
	}
	else
	{
		if( edit->scroll + drawLen > len )
		{
			edit->scroll = len - drawLen;
			if( edit->scroll < 0 ) edit->scroll = 0;
		}

		prestep = edit->scroll;
	}

	if( prestep + drawLen > len )
		drawLen = len - prestep;

	// extract <drawLen> characters from the field at <prestep>
	drawLen = Q_min( drawLen, MAX_SYSPATH - 1 );

	Q_memcpy( str, edit->buffer + prestep, drawLen );
	str[drawLen] = 0;

	// save char for overstrike
	cursorChar = str[edit->cursor - prestep];

	if( host.key_overstrike && cursorChar && !((int)( host.realtime * 4 ) & 1 ))
		hideChar = edit->cursor - prestep; // skip this char
	
	// draw it
	Con_DrawGenericString( x, y, str, colorDefault, false, hideChar );

	// draw the cursor
	if((int)( host.realtime * 4 ) & 1 ) return; // off blink

	// calc cursor position
	str[edit->cursor - prestep] = 0;
	Con_DrawStringLen( str, &curPos, NULL );
	Con_UtfProcessChar( 0 );

	if( host.key_overstrike && cursorChar )
	{
		// overstrike cursor
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		Con_DrawGenericChar( x + curPos, y, cursorChar, colorDefault );
	}
	else
	{
		Con_UtfProcessChar( 0 );
		Con_DrawCharacter( x + curPos, y, '_', colorDefault );
	}
}

/*
=============================================================================

CONSOLE LINE EDITING

==============================================================================
*/
/*
====================
Key_Console

Handles history and console scrollback
====================
*/
void Key_Console( int key )
{
	// ctrl-L clears screen
	if( key == 'l' && Key_IsDown( K_CTRL ))
	{
		Cbuf_AddText( "clear\n" );
		return;
	}

	// enter finishes the line
	if( key == K_ENTER || key == K_KP_ENTER )
	{
		// scroll down
		Con_Bottom();

		// if not in the game explicitly prepent a slash if needed
		if( cls.state != ca_active && con.input.buffer[0] != '\\' && con.input.buffer[0] != '/' )
		{
			char	temp[MAX_SYSPATH];

			Q_strncpy( temp, con.input.buffer, sizeof( temp ));
			Q_sprintf( con.input.buffer, "\\%s", temp );
			con.input.cursor++;
		}

		// backslash text are commands, else chat
		if( con.input.buffer[0] == '\\' || con.input.buffer[0] == '/' )
			Cbuf_AddText( con.input.buffer + 1 ); // skip backslash
		else Cbuf_AddText( con.input.buffer ); // valid command
		Cbuf_AddText( "\n" );

		// echo to console
		Msg( ">%s\n", con.input.buffer );

		// copy line to history buffer
		con.historyLines[con.nextHistoryLine % CON_HISTORY] = con.input;
		con.nextHistoryLine++;
		con.historyLine = con.nextHistoryLine;

		Con_ClearField( &con.input );
		con.input.widthInChars = con.linewidth;

		if( cls.state == ca_disconnected )
		{
			// force an update, because the command may take some time
			SCR_UpdateScreen ();
		}
		return;
	}

	// command completion
	if( key == K_TAB )
	{
		Con_CompleteCommand( &con.input );
		return;
	}

	// command history (ctrl-p ctrl-n for unix style)
	if(( key == K_MWHEELUP && Key_IsDown( K_SHIFT )) || ( key == K_UPARROW ) || (( Q_tolower(key) == 'p' ) && Key_IsDown( K_CTRL )))
	{
		if( con.nextHistoryLine - con.historyLine < CON_HISTORY && con.historyLine > 0 )
		{
			con.historyLine--;
		}
		con.input = con.historyLines[con.historyLine % CON_HISTORY];
		return;
	}

	if(( key == K_MWHEELDOWN && Key_IsDown( K_SHIFT )) || ( key == K_DOWNARROW ) || (( Q_tolower(key) == 'n' ) && Key_IsDown( K_CTRL )))
	{
		if( con.historyLine == con.nextHistoryLine ) return;
		con.historyLine++;
		con.input = con.historyLines[con.historyLine % CON_HISTORY];
		return;
	}

	// console scrolling
	if( key == K_PGUP )
	{
		Con_PageUp( 1 );
		return;
	}

	if( key == K_PGDN )
	{
		Con_PageDown( 1 );
		return;
	}

	if( key == K_MWHEELUP )
	{
		if( Key_IsDown( K_CTRL ))
			Con_PageUp( 8 );
		else Con_PageUp( 2 );
		return;
	}

	if( key == K_MWHEELDOWN )
	{	
		if( Key_IsDown( K_CTRL ))
			Con_PageDown( 8 );
		else Con_PageDown( 2 );
		return;
	}

	// ctrl-home = top of console
	if( key == K_HOME && Key_IsDown( K_CTRL ))
	{
		Con_Top();
		return;
	}

	// ctrl-end = bottom of console
	if( key == K_END && Key_IsDown( K_CTRL ))
	{
		Con_Bottom();
		return;
	}

	// pass to the normal editline routine
	Field_KeyDownEvent( &con.input, key );
}

/*
================
Key_Message

In game talk message
================
*/
void Key_Message( int key )
{
	char	buffer[MAX_SYSPATH];

	if( key == K_ESCAPE )
	{
		Key_SetKeyDest( key_game );
		Con_ClearField( &con.chat );
		return;
	}

	if( key == K_ENTER || key == K_KP_ENTER )
	{
		if( con.chat.buffer[0] && cls.state == ca_active )
		{
			Q_snprintf( buffer, sizeof( buffer ), "%s \"%s\"\n", con.chat_cmd, con.chat.buffer );
			Cbuf_AddText( buffer );

			Log_Printf( "Server say \"%s\"\n", con.chat.buffer );
		}

		Key_SetKeyDest( key_game );
		Con_ClearField( &con.chat );
		return;
	}

	Field_KeyDownEvent( &con.chat, key );
}

/*
==============================================================================

DRAWING

==============================================================================
*/
/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput( int lines )
{
	int	y;

	// don't draw anything (always draw if not active)
	if( cls.key_dest != key_console || !con.curFont )
		return;

	y = lines - ( con.curFont->charHeight * 2 );
	Con_DrawCharacter( 8, y, ']', g_color_table[7] );
	Field_DrawInputLine( 16, y, &con.input );
}

qboolean Con_DrawProgress( void )
{
		int x = QCHAR_WIDTH;
		int y = con.vislines - ( con.curFont->charHeight * 3 );
		if( scr_download->value > 0 )
		{
			while( x < scr_download->value * (scr_width->value - QCHAR_WIDTH * 2)  / 100 )
			{
				int dx = Con_DrawCharacter( x, y, '=', g_color_table[7] );
				if( dx < 1 )
					break;
				x += dx;
			}
		}
		else if( scr_loading->value > 0 )
		{
			while( x < scr_loading->value * (scr_width->value - QCHAR_WIDTH * 2) / 100 )
			{
				int dx = Con_DrawCharacter( x, y, '=', g_color_table[7] );
				if( dx < 1 )
					break;
				x += dx;
			}
		}

		else return false;
	return true;
}


/*
================
Con_DrawDebugLines

Custom debug messages
================
*/
int Con_DrawDebugLines( void )
{
	int	i, count = 0;
	int	y = 20;
	int	defaultX;

	defaultX = glState.width / 4;
	
	for( i = 0; i < MAX_DBG_NOTIFY; i++ )
	{
		if( host.realtime < con.notify[i].expire && con.notify[i].key_dest == cls.key_dest )
		{
			int	x, len;
			int	fontTall = 0;

			Con_DrawStringLen( con.notify[i].szNotify, &len, &fontTall );
			x = scr_width->integer - Q_max( defaultX, len ) - 10;
			fontTall += 1;

			if( y + fontTall > (int)scr_height->integer - 20 )
				return count;

			count++;
			y = 20 + fontTall * i;
			Con_DrawString( x, y, con.notify[i].szNotify, con.notify[i].color );
		}
	}

	return count;
}

/*
================
Con_DrawDebug

Draws the debug messages (not passed to console history)
================
*/
void Con_DrawDebug( void )
{
	if( !host.developer || Cvar_VariableInteger( "cl_background" ) || Cvar_VariableInteger( "sv_background" ))
		return;

	if( con.draw_notify && !Con_Visible( ))
	{
		if( Con_DrawDebugLines() == 0 )
			con.draw_notify = false;
	}
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify( void )
{
	double	time = cl.time;
	int	i, x, y = 0;

	if( !con.curFont ) return;

	x = con.curFont->charWidths[' ']; // offset one space at left screen side

	if( host.developer && ( !Cvar_VariableInteger( "cl_background" ) && !Cvar_VariableInteger( "sv_background" )))
	{
		for( i = CON_LINES_COUNT - CON_TIMES; i < CON_LINES_COUNT; i++ )
		{
			con_lineinfo_t	*l = &CON_LINES( i );

			if( l->addtime < ( time - con_notifytime->value ))
				continue;

			Con_DrawString( x, y, l->start, g_color_table[7] );
			y += con.curFont->charHeight;
		}
	}

	if( cls.key_dest == key_message )
	{
		string	buf;
		int	len;

		// update chatline position from client.dll
		if( clgame.dllFuncs.pfnChatInputPosition )
			clgame.dllFuncs.pfnChatInputPosition( &x, &y );

		Q_snprintf( buf, sizeof( buf ), "%s: ", con.chat_cmd );

		Con_DrawStringLen( buf, &len, NULL );
		Con_DrawString( x, y, buf, g_color_table[7] );

		Field_DrawInputLine( x + len, y, &con.chat );
	}

	pglColor4ub( 255, 255, 255, 255 );
}

/*
================
Con_DrawConsoleLine

Draws a line of the console; returns its height in lines.
If alpha is 0, the line is not drawn, but still wrapped and its height
returned.
================
*/
int Con_DrawConsoleLine( int y, int lineno )
{
	con_lineinfo_t	*li = &CON_LINES( lineno );

	if( y >= con.curFont->charHeight )
		Con_DrawGenericString( con.curFont->charWidths[' '], y, li->start, g_color_table[7], false, -1 );

	return con.curFont->charHeight;
}

/*
================
Con_LastVisibleLine

Calculates the last visible line index and how much to show
of it based on con.backscroll.
================
*/
static void Con_LastVisibleLine( int *lastline )
{
	int	i, lines_seen = 0;

	con.backscroll = Q_max( 0, con.backscroll );
	*lastline = 0;

	// now count until we saw con_backscroll actual lines
	for( i = CON_LINES_COUNT - 1; i >= 0; i-- )
	{
		// line is the last visible line?
		*lastline = i;

		if( lines_seen + 1 > con.backscroll && lines_seen <= con.backscroll )
			return;

		lines_seen += 1;
	}

	// if we get here, no line was on screen - scroll so that one line is visible then.
	con.backscroll = lines_seen - 1;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole( int lines )
{
	int	i, x, y;
	float	fraction;
	int	start;

	if( lines <= 0 ) return;

	// draw the background
	GL_SetRenderMode( kRenderNormal );
	pglColor4ub( 255, 255, 255, 255 ); // to prevent grab color from screenfade
	R_DrawStretchPic( 0, lines - scr_height->integer, scr_width->integer, scr_height->integer, 0, 0, 1, 1, con.background );

	if( !con.curFont || host.developer <= 0 )
		return; // nothing to draw

	if( host.developer > 0 )
	{
		// draw current version
		int	stringLen, width = 0, charH;
		string	curbuild;
		byte	color[4];

		Q_memcpy( color, g_color_table[7], sizeof( color ));

		Q_snprintf( curbuild, MAX_STRING, "Xash3D FWGS %i/%s build %i %s %s-%s", PROTOCOL_VERSION,
					XASH_VERSION, Q_buildnum( ), Q_buildcommit( ), Q_buildos( ), Q_buildarch( ) );
		Con_DrawStringLen( curbuild, &stringLen, &charH );
		start = scr_width->integer - stringLen;
		stringLen = Con_StringLength( curbuild );

		fraction = lines / (float)scr_height->integer;
		color[3] = Q_min( fraction * 2.0f, 1.0f ) * 255; // fadeout version number

		for( i = 0; i < stringLen; i++ )
			width += Con_DrawCharacter( start + width, 0, curbuild[i], color );
	}

	// draw the text
	if( CON_LINES_COUNT > 0 )
	{
		int	ymax = lines - (con.curFont->charHeight * (Con_DrawProgress()?3:2) );
		int	lastline;

		Con_LastVisibleLine( &lastline );
		y = ymax - con.curFont->charHeight;

		if( con.backscroll )
		{
			start = con.curFont->charWidths[' ']; // offset one space at left screen side

			// draw red arrows to show the buffer is backscrolled
			for( x = 0; x < con.linewidth; x += 4 )
				Con_DrawCharacter(( x + 1 ) * start, y, '^', g_color_table[1] );
			y -= con.curFont->charHeight;
		}
		x = lastline;

		while( 1 )
		{
			y -= Con_DrawConsoleLine( y, x );

			// top of console buffer or console window
			if( x == 0 || y < con.curFont->charHeight )
				break;
			x--;
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput( lines );

	y = lines - ( con.curFont->charHeight * 1.2f );
	SCR_DrawFPS( max( y, 4 )); // to avoid to hide fps counter

	pglColor4ub( 255, 255, 255, 255 );
}

/*
==================
Con_DrawConsole
==================
*/

void Con_DrawConsole( void )
{
	// never draw console when changelevel in-progress
	// mittorn: breaks console when downloading map, it may hang!
	//if( cls.state != ca_disconnected && ( cls.changelevel || cls.changedemo ))
	//	return;

	// check for console width changes from a vid mode change
	Con_CheckResize ();

	if( cls.state == ca_connecting || cls.state == ca_connected )
	{
		if( !cl_allow_levelshots->integer )
		{
			if(( Cvar_VariableInteger( "cl_background" ) || Cvar_VariableInteger( "sv_background" )) && cls.key_dest != key_console )
				con.vislines = con.showlines = 0;
			else con.vislines = con.showlines = scr_height->integer;
		}
		else
		{
			if( host.developer >= 4 )
			{
				con.vislines = (scr_height->integer >> 1);	// keep console open
			}
			else
			{
				con.showlines = 0;
				Con_RunConsole();

				if( host.developer >= 2 )
					Con_DrawNotify(); // draw notify lines
			}
		}
	}

	// if disconnected, render console full screen
	switch( cls.state )
	{
	case ca_uninitialized:
		break;
	case ca_disconnected:
		if( cls.key_dest != key_menu && host.developer )
		{
			Con_DrawSolidConsole( scr_height->integer );
			Key_SetKeyDest( key_console );
		}
		break;
	case ca_connected:
	case ca_connecting:
		// force to show console always for -dev 3 and higher
		if( con.vislines )
		{
			GL_CleanupAllTextureUnits(); // ugly hack to remove blinking voiceicon.spr during loading
			Con_DrawSolidConsole( con.vislines );
		}
		break;
	case ca_active:
	case ca_cinematic:
		if( Cvar_VariableInteger( "cl_background" ) || Cvar_VariableInteger( "sv_background" ))
		{
			if( cls.key_dest == key_console )
				Con_DrawSolidConsole( scr_height->integer );
		}
		else
		{
			if( con.vislines )
				Con_DrawSolidConsole( con.vislines );
			else if( cls.state == ca_active && ( cls.key_dest == key_game || cls.key_dest == key_message ))
				Con_DrawNotify(); // draw notify lines
		}
		break;
	}

	if( !Con_Visible( )) SCR_DrawFPS( 4 );
}

/*
==================
Con_DrawVersion

Used by menu
==================
*/
void Con_DrawVersion( void )
{
	// draws the current build
	byte	*color = g_color_table[7];
	int	i, stringLen, width = 0, charH = 0;
	int	start, height = scr_height->integer;
	qboolean	draw_version = false;
	string	curbuild;

	switch( cls.scrshot_action )
	{
	case scrshot_normal:
	case scrshot_snapshot:
		draw_version = true;
		break;
	default:
		break;
	}

	if( !host.force_draw_version )
	{
		if(( cls.key_dest != key_menu && !draw_version ) || gl_overview->integer == 2 )
			return;
	}
	else
	{
		if( host.realtime > host.force_draw_version_time )
			host.force_draw_version = false;
	}

	if( host.force_draw_version || draw_version )
		Q_snprintf( curbuild, MAX_STRING, "Xash3D FWGS %i/%s build %i %s %s-%s", PROTOCOL_VERSION,
					XASH_VERSION, Q_buildnum( ), Q_buildcommit( ), Q_buildos( ), Q_buildarch( ) );
	else Q_snprintf( curbuild, MAX_STRING, "v%i/%s build %i %s %s-%s", PROTOCOL_VERSION,
					 XASH_VERSION, Q_buildnum( ), Q_buildcommit( ), Q_buildos( ), Q_buildarch( ));
	Con_DrawStringLen( curbuild, &stringLen, &charH );
	start = scr_width->integer - stringLen * 1.05f;
	stringLen = Con_StringLength( curbuild );
	height -= charH * 1.05f;

	for( i = 0; i < stringLen; i++ )
		width += Con_DrawCharacter( start + width, height, curbuild[i], color );
}

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole( void )
{
	int	lines_per_frame;

	// decide on the destination height of the console
	if( host.developer && cls.key_dest == key_console )
	{
		if( cls.state == ca_disconnected )
			con.showlines = scr_height->integer;	// full screen
		else con.showlines = (scr_height->integer >> 1);	// half screen
	}
	else con.showlines = 0; // none visible

	// when level is loading frametime may be is wrong
	if( cls.state == ca_connecting || cls.state == ca_connected )
	{
		if( !FBitSet( host.features, ENGINE_FIXED_FRAMERATE ))
			host.realframetime = ( MAX_FPS / host_maxfps->value ) * MIN_FRAMETIME;
		else host.realframetime = HOST_FRAMETIME;
	}

	lines_per_frame = bound( 1, fabs( scr_conspeed->value ) * host.realframetime, scr_height->integer );

	if( con.showlines < con.vislines )
	{
		con.vislines -= lines_per_frame;
		if( con.showlines > con.vislines )
			con.vislines = con.showlines;
	}
	else if( con.showlines > con.vislines )
	{
		con.vislines += lines_per_frame;
		if( con.showlines < con.vislines )
			con.vislines = con.showlines;
	}

	if( con_charset->modified || con_fontscale->modified || con_fontnum->modified )
	{
		// update codepage parameters
		g_codepage = 0;
		if( !Q_stricmp( con_charset->string, "cp1251" ) )
			g_codepage = 1251;
		else if( !Q_stricmp( con_charset->string, "cp1252" ) )
			g_codepage = 1252;

		g_utf8 = !Q_stricmp( cl_charset->string, "utf-8" );
		Con_InvalidateFonts();
		Con_LoadConchars();
		cls.creditsFont.valid = false;
		SCR_LoadCreditsFont();
		con_charset->modified = con_fontscale->modified = con_fontnum->modified = false;
	}
}

/*
==============================================================================

CONSOLE INTERFACE

==============================================================================
*/
/*
================
Con_CharEvent

Console input
================
*/
void Con_CharEvent( int key )
{
	// distribute the key down event to the apropriate handler
	if( cls.key_dest == key_console )
	{
		Field_CharEvent( &con.input, key );
	}
	else if( cls.key_dest == key_message )
	{
		Field_CharEvent( &con.chat, key );
	}
}

void Con_VidInit( void )
{
	Con_CheckResize();
	Con_InvalidateFonts();

	// loading console image
	if( host.developer )
	{
		// trying to load truecolor image first
		if( FS_FileExists( "gfx/shell/conback.bmp", false ) || FS_FileExists( "gfx/shell/conback.tga", false ))
			con.background = GL_LoadTexture( "gfx/shell/conback", NULL, 0, TF_IMAGE, NULL );

		if( !con.background )
		{
			if( scr_width->integer < 640 )
			{
				if( FS_FileExists( "cached/conback400", false ))
					con.background = GL_LoadTexture( "cached/conback400", NULL, 0, TF_IMAGE, NULL );
				else con.background = GL_LoadTexture( "cached/conback", NULL, 0, TF_IMAGE, NULL );
			}
			else
			{
				if( FS_FileExists( "cached/conback640", false ))
					con.background = GL_LoadTexture( "cached/conback640", NULL, 0, TF_IMAGE, NULL );
				else con.background = GL_LoadTexture( "cached/conback", NULL, 0, TF_IMAGE, NULL );
			}
		}
	}
	else
	{
		// trying to load truecolor image first
		if( FS_FileExists( "gfx/shell/loading.bmp", false ) || FS_FileExists( "gfx/shell/loading.tga", false ))
			con.background = GL_LoadTexture( "gfx/shell/loading", NULL, 0, TF_IMAGE, NULL );

		if( !con.background )
		{
			if( scr_width->integer < 640 )
			{
				if( FS_FileExists( "cached/loading400", false ))
					con.background = GL_LoadTexture( "cached/loading400", NULL, 0, TF_IMAGE, NULL );
				else con.background = GL_LoadTexture( "cached/loading", NULL, 0, TF_IMAGE, NULL );
			}
			else
			{
				if( FS_FileExists( "cached/loading640", false ))
					con.background = GL_LoadTexture( "cached/loading640", NULL, 0, TF_IMAGE, NULL );
				else con.background = GL_LoadTexture( "cached/loading", NULL, 0, TF_IMAGE, NULL );
			}
		}
	}

	// missed console image will be replaced as gray background like X-Ray or Crysis
	if( con.background == tr.defaultTexture || con.background == 0 )
		con.background = tr.grayTexture;

	Con_LoadConchars();
}

void Con_InvalidateFonts( void )
{
	Q_memset( con.chars, 0, sizeof( con.chars ));
	con.curFont = con.lastUsedFont = NULL;
}

void Con_FastClose( void )
{
	Con_ClearField( &con.input );
	Con_ClearNotify();
	con.showlines = 0;
	con.vislines = 0;
}

/*
=========
Con_DefaultColor

called from MainUI
=========
*/
void Con_DefaultColor( int r, int g, int b )
{
	r = bound( 0, r, 255 );
	g = bound( 0, g, 255 );
	b = bound( 0, b, 255 );
	MakeRGBA( g_color_table[7], r, g, b, 255 );
}
#endif
