/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// in_win.c -- windows 95 mouse code

#include		"quakedef.h"

cvar_t	keyboard = {"keyboard","1"};			// set for running times
int key_down = -1;

char key_chars[5][15] = {
	{'`','1','2','3','4','5','6','7','8','9','0','-','=','+',K_BACKSPACE},
	{K_TAB,'q','w','e','r','t','y','u','i','o','p','[',']','\\',K_BACKSPACE},
	{K_SHIFT,'a','s','d','f','g','h','j','k','l',';','\'',K_ENTER,'_',K_BACKSPACE},
	{K_SHIFT,'z','x','c','v','b','n','m',',','.','/',K_SHIFT,'-','=',K_BACKSPACE},
	{K_CTRL,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',',','.',K_SHIFT}
};

void draw_keyboard()
{
	int i,j;
	int x = 8;
	int y = vid.conheight>>1;

	if(keyboard.value == 0)
		return;

	Draw_Fill(x,y,15*16,5*16,1);
	for(i=0;i<5;i++)
	{
		x = 8;
		for(j=0;j<15;j++)
		{
			if(key_down == key_chars[i][j])
			{
				Draw_Fill(x,y,16,16,7);
			}
			Draw_Character(x+4,y+4,key_chars[i][j]);
			x += 16;
		}
		y+=16;
	}
}
#ifdef NDS

#define KEYS_CUR (( ((~REG_KEYINPUT)&0x3ff) | (((~IPC->buttons)&3)<<10) | (((~IPC->buttons)<<6) & (KEY_TOUCH|KEY_LID) ))^KEY_LID)
u32 keys_last = 0;
u32 nds_keys[] = {
K_NDS_A,
K_NDS_B,
K_NDS_SELECT,
K_NDS_START,
K_NDS_RIGHT,
K_NDS_LEFT,
K_NDS_UP,
K_NDS_DOWN,
K_NDS_R,
K_NDS_L,
K_NDS_X,
K_NDS_Y,
K_NDS_F1};

touchPosition	thisXY;
touchPosition	lastXY = { 0,0,0,0 };

void IN_Commands (void)
{
	u32 key_mask=1;
	u32 keys = KEYS_CUR;
	u32 i;
	for(i=0;i<12;i++,key_mask<<=1) {
		if( (keys & key_mask) && !(keys_last & key_mask)) {
			//iprintf("pressed start\n");
			Key_Event (nds_keys[i], true);
		} else if( !(keys & key_mask) && (keys_last & key_mask)) {
			//iprintf("released start\n");
			Key_Event (nds_keys[i], false);
		}
	}

	keys_last = keys;
	
	if(keyboard.value)
	{
		if (keys & KEY_TOUCH)
		{
			int x,y,i,j,key_new;
			thisXY = touchReadXY();
			x = thisXY.px;
			y = thisXY.py;
			if(x < 16 && y < 16)
			{
				key_new = K_NDS_F1;
				if(key_down != key_new)
				{
					if(key_down != -1)
						Key_Event(key_down,false);
					key_down = key_new;
					Key_Event(key_down,true);
				}
			}
			else
			{
				x = x - 8;
				if(x < 0 || x > (15*16))
					return;
				y = y - (vid.conheight>>1);
				if(y < 0 || y > (5*16))
					return;
				x = (x*vid.conwidth)/vid.width;
				y = (y*vid.conheight)/vid.height;
				j = x >> 4;
				i = y >> 4;
				key_new = key_chars[i][j];
				if(key_down != key_new)
				{
					if(key_down != -1)
						Key_Event(key_down,false);
					key_down = key_new;
					Key_Event(key_down,true);
				}
			}
		}
		else
		{
			if(key_down != -1)
			{
				Key_Event(key_down,false);
				key_down = -1;
			}
		}
	}

}

#else
void IN_Commands (void)
{
}
#endif
/*
==================
Force_CenterView_f
==================
*/
void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}


/*
=======
IN_Init
=======
*/
void IN_Init (void)
{
	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
	Cvar_RegisterVariable(&keyboard);
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{

}


/*
=======
IN_Move
=======
*/
void IN_Move (usercmd_t *cmd)
{
}

/*
=============
IN_Accumulate
=============
*/
void IN_Accumulate (void)
{
}


/*
==============
IN_ClearStates
==============
*/
void IN_ClearStates (void)
{
}
