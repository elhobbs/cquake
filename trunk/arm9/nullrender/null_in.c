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
#include <ctype.h>

cvar_t	keyboard = {"keyboard","0"};			// set for running times
cvar_t	ds_tap_time = {"ds_tap_time","9000",true};			// set for double tap time
int key_down = -1;


typedef struct {
	int x,y,type,dx,key;
	char *text,*shift_text;
} sregion_t;

char key_row_1[] = "~1234567890-=";
char key_row_1_shift[] = "`!@#$%^&*()_+";

char key_row_2[] = "qwertyuiop[]\\";
char key_row_2_shift[] = "QWERTYUIOP{}|";

char key_row_3[] = "asdfghjkl;'";
char key_row_3_shift[] = "ASDFGHJKL:\"";

char key_row_4[] = "zxcvbnm,./";
char key_row_4_shift[] = "ZXCVBNM<>?";

char key_tab[] = "Tab";
char key_caps[] = "Caps";
char key_shift[] = "Shift";
char key_ctrl[] = "Ctrl";
char key_alt[] = "Alt";
char key_space[] = "     SPACE      ";
char key_backspace[] = "Bksp";
char key_return[] = "Rtrn";

sregion_t key_array[] = {
	{0			,0*16	,0,0,0,key_row_1,key_row_1_shift},
	{13*16 + 2	,0*16	,1,0,K_BACKSPACE,key_backspace},
	{0			,1*16	,1,0,K_TAB,key_tab},
	{4*8		,1*16	,0,0,0,key_row_2,key_row_2_shift},
	{0			,2*16	,1,0,K_CAPS,key_caps},
	{5*8		,2*16	,0,0,0,key_row_3,key_row_3_shift},
	{5*8 + 
		11*16 + 
		2		,2*16	,1,0,K_ENTER,key_return},
	{0			,3*16	,1,0,K_SHIFT,key_shift},
	{6*8		,3*16	,0,0,0,key_row_4,key_row_4_shift},
	{256 - 16*2	,3*16	,2,0,K_UPARROW,0},
	{0			,4*16	,1,0,K_CTRL,key_ctrl},
	{5*8		,4*16	,1,0,K_ALT,key_alt},
	{9*8		,4*16	,1,0,' ',key_space},
	{256 - 16*3	,4*16	,3,0,K_LEFTARROW,0},
	{256 - 16*2	,4*16	,4,0,K_DOWNARROW,0},
	{256 - 16*1	,4*16	,5,0,K_RIGHTARROW,0},
};

sregion_t *key_touching = 0;
int key_touching_index = -1;
int key_in_touch = 0;
int last_in_touch = 0;

int key_in_shift = 0;
int key_in_ctrl = 0;
int key_in_alt = 0;
int key_in_caps = 0;

int key_tap_time = -1;

/*
				len = k+1;
				for(j=0;j<len;j++)
				{
					buf[6-len/2 + j] = 7;
				}
*/
char key_arrow[12][12] = {
	{0,0,0,0,0,0,7,0,0,0,0,0},
	{0,0,0,0,0,7,7,0,0,0,0,0},
	{0,0,0,0,0,7,7,7,0,0,0,0},
	{0,0,0,0,7,7,7,7,0,0,0,0},
	{0,0,0,0,7,7,7,7,7,0,0,0},
	{0,0,0,7,7,7,7,7,7,0,0,0},
	{0,0,0,7,7,7,7,7,7,7,0,0},
	{0,0,7,7,7,7,7,7,7,7,0,0},
	{0,0,7,7,7,7,7,7,7,7,7,0},
	{0,7,7,7,7,7,7,7,7,7,7,0},
	{0,7,7,7,7,7,7,7,7,7,7,7},
	{7,7,7,7,7,7,7,7,7,7,7,7,}
};

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
#endif

void keyboard_init()
{
	int i,len;
	int count = sizeof(key_array)/sizeof(sregion_t);

	for(i=0;i<count;i++)
	{
		if(key_array[i].type == 0)
		{
			len = strlen(key_array[i].text)*16;
			key_array[i].dx = len;
		}
		else if(key_array[i].type == 1)
		{
			len = strlen(key_array[i].text)*8 + 4;
			key_array[i].dx = len;
		}
		else if(key_array[i].type == 2)
		{
			key_array[i].dx = 16;
		}
		else if(key_array[i].type == 3)
		{
			key_array[i].dx = 16;
		}
		else if(key_array[i].type == 4)
		{
			key_array[i].dx = 16;
		}
		else if(key_array[i].type == 5)
		{
			key_array[i].dx = 16;
		}
	}
}

void ds_do_tap()
{
	static int touch_down = 0;
	int keys;

#ifdef NDS
		if(ds_tap_time.value)
		{
			keys = KEYS_CUR;
			if(keys & KEY_TOUCH)
			{
				int tm = Sys_IntTime();
				if(touch_down == 0)
				{
					//Con_Printf("touch down %d %d %d\n", tm - key_tap_time,tm,key_tap_time);
					if(tm - key_tap_time < ds_tap_time.value)
					{
						Key_Event (K_NDS_TAP, true);
						Key_Event (K_NDS_TAP, false);
						//Con_Printf("tap\n");
					}
					else
					{
						//Con_Printf("missed %d %d\n",tm - key_tap_time,(int)ds_tap_time.value);
						key_tap_time = tm;
					}
					touch_down = 1;
				}
			}
			else
			{
				if(touch_down)
				{
					//Con_Printf("touch up\n");
					touch_down = 0;
				}
			}
		}
#endif
}

void keyboard_scankeys()
{
	static sregion_t *last_reg;
	static int last_pos;
	int keys;

	int i,len,x,y,pos;
	int count = sizeof(key_array)/sizeof(sregion_t);

	if(keyboard.value == 0)
	{
		ds_do_tap();
		last_reg = key_touching = 0;
		last_pos = key_touching_index = -1;
		return;
	}

	x = y = -1;

#ifdef NDS
touchPosition	touch  = { 0,0,0,0 };
	keys = KEYS_CUR;

	if (keys & KEY_TOUCH)
	{
		touch = touchReadXY();
		x = touch.px;
		y = touch.py - (vid.height/2);
		if(y < 0)
		{
			last_reg = key_touching = 0;
			last_pos = key_touching_index = -1;
			return;
		}
		key_in_touch = 1;
		//Con_Printf("Touch: %d %d\n",x,y);
	}
	else
	{
		key_in_touch = 0;
		goto end_quick;
	}
#else
	return;
#endif

	for(i=0;i<count;i++)
	{
		if(y < key_array[i].y || y > (key_array[i].y+16))
		{
			//Con_Printf("1: %d %d\n",y,key_array[i].y);
			continue;
		}
		len = key_array[i].dx;
		if(x < key_array[i].x || x > (key_array[i].x+len))
		{
			//Con_Printf("2: %d %d %d\n",x,key_array[i].x,key_array[i].dx);
			continue;
		}
		if(key_array[i].type == 0)
		{
			pos = (x - key_array[i].x)/16;
		}
		else
		{
			pos = 0;
		}

		if(last_reg == &key_array[i] && last_pos == pos)
			break;

		key_touching = &key_array[i];
		key_touching_index = pos;
		return;
	}
end_quick:
	last_reg = key_touching = 0;
	last_pos = key_touching_index = -1;
}

extern int vid_on_top;
void Draw_Character2 (int x, int y, int num,char *vbuf);

void draw_keyboard()
{
	int i,j,k,pos;
	int x;
	int y;
	int len,vofs;
	char *buf,*ch,*vbuf;
	int count = sizeof(key_array)/sizeof(sregion_t);

	if(keyboard.value == 0)
		return;

	if(vid_on_top)
	{
		vbuf = (char *)Hunk_TempAlloc(16*16*5*16);
		vofs = 0;
	}
	else
	{
		vbuf = (char *)vid.buffer;
		vofs = vid.height/2;
		Draw_Fill(0,vid.height/2,16*16,5*16,1);
	}


	for(i=0;i<count;i++)
	{
		if(key_array[i].type == 0)
		{
			x = key_array[i].x;
			y = vofs + key_array[i].y;
			ch = key_in_shift ? key_array[i].shift_text : key_array[i].text;
			pos = 0;
			while(ch && *ch)
			{
				//left/right sides
				buf = vbuf + ((y+1)*vid.width) + x;
				for(k=0;k<14;k++)
				{
					*buf = 7;
					*(buf + 14) = 7;
					buf += vid.width;
				}
				//top/bottom
				buf = vbuf + ((y+1)*vid.width) + x + 1;
				for(k=0;k<14;k++)
				{
					*buf = 7;
					*(buf + 13*vid.width) = 7;
					buf ++;
				}
				k = key_in_caps ? toupper(*ch) : *ch;
				if(key_touching == &key_array[i] && key_touching_index == pos)
					Draw_Character2(x+3,y+4,k<128 ? k+128 : k,vbuf);
				else
					Draw_Character2(x+3,y+4,k,vbuf);
				ch++;
				x+=16;
				pos++;
			}
		}
		else if(key_array[i].type == 1)
		{
			x = key_array[i].x;
			y = vofs+ key_array[i].y;
			ch = key_array[i].text;
			len = strlen(ch)*8 + 4;
			//left/right sides
			buf = vbuf + ((y+1)*vid.width) + x;
			for(k=0;k<14;k++)
			{
				*buf = 7;
				*(buf + len) = 7;
				buf += vid.width;
			}
			//top/bottom
			buf = vbuf + ((y+1)*vid.width) + x + 1;
			for(k=0;k<len;k++)
			{
				*buf = 7;
				*(buf + 13*vid.width) = 7;
				buf ++;
			}
			while(ch && *ch)
			{
				if(key_touching == &key_array[i] || 
					(key_in_caps && key_array[i].key == K_CAPS) ||
					(key_in_shift && key_array[i].key == K_SHIFT))
					Draw_Character2(x+3,y+4,*ch<128 ? *ch+128 : *ch,vbuf);
				else
					Draw_Character2(x+3,y+4,*ch,vbuf);
				ch++;
				x+=8;
			}
		}
		else if(key_array[i].type == 2)
		{
			x = key_array[i].x;
			y = vofs + key_array[i].y;
			buf = vbuf + ((y+1)*vid.width) + x;
			for(k=0;k<12;k++)
			{
				for(j=0;j<12;j++)
				{
					if(key_arrow[k][j])
						buf[j] = key_arrow[k][j];
				}
				buf += vid.width;
			}
		}
		else if(key_array[i].type == 3)
		{
			x = key_array[i].x;
			y = vofs + key_array[i].y;
			buf = vbuf + ((y+1)*vid.width) + x;
			for(k=0;k<12;k++)
			{
				for(j=0;j<12;j++)
				{
					if(key_arrow[j][k])
						buf[j] = key_arrow[j][k];
				}
				buf += vid.width;
			}
		}
		else if(key_array[i].type == 4)
		{
			x = key_array[i].x;
			y = vofs + key_array[i].y;
			buf = vbuf + ((y+1)*vid.width) + x;
			for(k=0;k<12;k++)
			{
				for(j=0;j<12;j++)
				{
					if(key_arrow[11-k][j])
						buf[j] = key_arrow[11-k][j];
				}
				buf += vid.width;
			}
		}
		else if(key_array[i].type == 5)
		{
			x = key_array[i].x;
			y = vofs + key_array[i].y;
			buf = vbuf + ((y+1)*vid.width) + x;
			for(k=0;k<12;k++)
			{
				for(j=0;j<12;j++)
				{
					if(key_arrow[j][k])
						buf[11-j] = key_arrow[j][k];
				}
				buf += vid.width;
			}
		}
	}
	//just dump now
	if(vid_on_top)
	{
#ifdef NDS
		char * dest;

		dest = (char *)BG_BMP_RAM_SUB(0);
		dest += (vid.height/2)*vid.width;
		dmaCopyWords(2, (uint32*)vbuf,(uint32*)dest, vid.width*5*16);
#endif
	}
}

void IN_Commands (void)
{
	int key;
	static int last_index = -1;
	static sregion_t *last_touching = 0;
#ifdef NDS
	u32 key_mask=1;
	u32 i;
	u32 keys = KEYS_CUR;
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
#endif

	keyboard_scankeys();
	if(key_touching_index == -1 && key_touching == 0)
	{
		if(key_down != -1)
		{
			Key_Event(key_down,false);
			key_down = -1;
			last_index = -1;
			last_touching = 0;
		}
		last_in_touch = key_in_touch;
		return;
	}
	//Con_Printf("touching: %x %d\n",key_touching,key_touching_index);

	switch(key_touching->type)
	{
	case 0:
		key = key_in_shift ? key_touching->shift_text[key_touching_index] : (key_in_caps ? toupper(key_touching->text[key_touching_index]) : key_touching->text[key_touching_index]);
		key_in_shift = 0;
		break;
	default:
		key = key_touching->key;
		break;
	}
	if(key_down != -1 && key != key_down &&
		last_index != key_touching_index && 
		last_touching != key_touching)
	{
		Key_Event(key_down,false);
	}
	if(key != key_down && last_in_touch == 0 && key_in_touch != 0)
	{
		Key_Event(key,true);
		key_down = key;

		//check for shift
		if(key == K_SHIFT)
		{
			key_in_shift = key_in_shift ? 0 : 1;
		}
		//check for caps
		if(key == K_CAPS)
		{
			key_in_caps = key_in_caps ? 0 : 1;
			return;
		}
	}

	last_in_touch = key_in_touch;
	last_index = key_touching_index;
	last_touching = key_touching;

}

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
	keyboard_init();
	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
	Cvar_RegisterVariable(&keyboard);
	Cvar_RegisterVariable(&ds_tap_time);

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
#ifdef NDS
touchPosition	g_lastTouch  = { 0,0,0,0 };
touchPosition	g_currentTouch = { 0,0,0,0 };
#endif

void IN_Move (usercmd_t *cmd)
{
#ifdef NDS
	int dx,dy;
	scanKeys();
	if (keysDown() & KEY_TOUCH)
	{
		g_lastTouch = touchReadXY();
		g_lastTouch.px <<= 7;
		g_lastTouch.py <<= 7;
	}
	if(keysHeld() & KEY_TOUCH)
	{
		g_currentTouch = touchReadXY();
		// let's use some fixed point magic to improve touch smoothing accuracy
		g_currentTouch.px <<= 7;
		g_currentTouch.py <<= 7;

		dx = (g_currentTouch.px - g_lastTouch.px) >> 6;
		dy = (g_currentTouch.py - g_lastTouch.py) >> 6;

		// filtering too long strokes, if needed
		//if((dx < 30) && (dy < 30) && (dx > -30) && (dy > -30))
		//{
			// filter too small strokes, if needed
			//if((dx > -2) && (dx < 2))
			//	dx = 0;

			// filter too small strokes, if needed
			//if((dy > -1) && (dy < 1))
			//	dy = 0;
			
			dx *= sensitivity.value;
			dy *= sensitivity.value;
			
			// add mouse X/Y movement to cmd
			if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
				cmd->sidemove += m_side.value * dx;
			else
				cl.viewangles[YAW] -= m_yaw.value * dx;

			//if ((in_mlook.state & 1) || !lookspring.value)
				V_StopPitchDrift ();
				
			//if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
			//{
				cl.viewangles[PITCH] += m_pitch.value * dy;
				if (cl.viewangles[PITCH] > 80)
					cl.viewangles[PITCH] = 80;
				if (cl.viewangles[PITCH] < -70)
					cl.viewangles[PITCH] = -70;
			/*}
			else
			{
				if ((in_strafe.state & 1) && noclip_anglehack)
					cmd->upmove -= m_forward.value * dy;
				else
					cmd->forwardmove -= m_forward.value * dy;
			}*/
		//}

		// some simple averaging / smoothing through weightened (.5 + .5) accumulation
		g_lastTouch.px = (g_lastTouch.px + g_currentTouch.px) / 2;
		g_lastTouch.py = (g_lastTouch.py + g_currentTouch.py) / 2;
	}
#endif
}

/*
=============
IN_Accumulate
=============
*/
void IN_Accumulate (void)
{
	ds_do_tap();
}


/*
==============
IN_ClearStates
==============
*/
void IN_ClearStates (void)
{
}
