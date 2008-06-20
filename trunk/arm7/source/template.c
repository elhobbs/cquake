/*---------------------------------------------------------------------------------

	default ARM7 core

	Copyright (C) 2005
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.
	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.
	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include "IPCFifo.h"


#ifdef USE_WIFI
#include <dswifi7.h>
#endif

//---------------------------------------------------------------------------------
void startSound(int sampleRate, const void* data, u32 bytes, u8 channel, u8 vol,  u8 pan, u8 format, u16 loop) {
//---------------------------------------------------------------------------------
	SCHANNEL_TIMER(channel)  = SOUND_FREQ(sampleRate);
	SCHANNEL_SOURCE(channel) = (u32)data;
	SCHANNEL_LENGTH(channel) = bytes >> 2 ;
	if(loop == 0xffff)
	{
		SCHANNEL_CR(channel)     = SCHANNEL_ENABLE | SOUND_ONE_SHOT | SOUND_VOL(vol) | SOUND_PAN(pan) | (format==1?SOUND_8BIT:SOUND_16BIT);
	}
	else
	{
		SCHANNEL_REPEAT_POINT(channel) = loop >> 2;
		SCHANNEL_CR(channel)     = SCHANNEL_ENABLE | SOUND_REPEAT | SOUND_VOL(vol) | SOUND_PAN(pan) | (format==1?SOUND_8BIT:SOUND_16BIT);
	}
}

//---------------------------------------------------------------------------------
s32 getFreeSoundChannel() {
//---------------------------------------------------------------------------------
	int i;
	for (i=0; i<16; i++) {
		if ( (SCHANNEL_CR(i) & SCHANNEL_ENABLE) == 0 ) return i;
	}
	return -1;
}


touchPosition first,tempPos;

//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	static int lastbut = -1;
	
	uint16 but=0, x=0, y=0, xpx=0, ypx=0, z1=0, z2=0;

	but = REG_KEYXY;

	if (!( (but ^ lastbut) & (1<<6))) {
 
		tempPos = touchReadXY();

		if ( tempPos.x == 0 || tempPos.y == 0 ) {
			but |= (1 <<6);
			lastbut = but;
		} else {
			x = tempPos.x;
			y = tempPos.y;
			xpx = tempPos.px;
			ypx = tempPos.py;
			z1 = tempPos.z1;
			z2 = tempPos.z2;
		}
		
	} else {
		lastbut = but;
		but |= (1 <<6);
	}

	IPC->touchX			= x;
	IPC->touchY			= y;
	IPC->touchXpx		= xpx;
	IPC->touchYpx		= ypx;
	IPC->touchZ1		= z1;
	IPC->touchZ2		= z2;
	IPC->buttons		= but;

}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------

	u32 i;

	//sound code  :)
	TransferSound *snd = IPC->soundData;
	IPC->soundData = 0;

	if (0 != snd) {

		for (i=0; i<snd->count; i++) {
			s32 chan = getFreeSoundChannel();

			if (chan >= 0) {
				startSound(snd->data[i].rate, snd->data[i].data, snd->data[i].len, chan, snd->data[i].vol, snd->data[i].pan, snd->data[i].format,0xffff);
			}
		}
	}
#ifdef USE_WIFI
	Wifi_Update(); // update wireless in vblank
#endif
}

typedef struct
{
    byte  *data;
    int size;
    int  format;
    int  rate;
    int  volume;
    int  pan;
    int  loop;
	int  fixed_volume;
	int  fixed_attenuation;
	int  origin[3];
} fifo_sound_t;

int listener_origin[3];
int listener_right[3];

fifo_sound_t ds_sounds[16];

int ds_play_sound(fifo_sound_t *snd)
{
	//IPCFifoSendWordAsync(FIFO_SUBSYSTEM_SOUND,1,(u32)getIPC_buffer());
	s32 chan = getFreeSoundChannel();

	if (chan >= 0) {
		ds_sounds[chan] = *snd;
		startSound(snd->rate, snd->data, snd->size, chan, snd->volume, snd->pan, snd->format,snd->loop);
	}
}

/*
#define FSOUND_NOMINAL_CLIP_DISTANCE_MULT 66

static unsigned intsqrt(unsigned int val) {
	unsigned int temp, g=0, b = 0x8000, bshft = 15;
	do {
		if (val >= (temp = (((g << 1) + b)<<bshft--))) {
		   g += b;
		   val -= temp;
		}
	} while (b >>= 1);
	return g;
}

int norm (int *v)
{
	int	length;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = isqrt (length);		// FIXME

	if (length)
	{
		v[0] = (v[0]*(1<<16))/length;
		v[1] = (v[1]*(1<<16))/length;
		v[2] = (v[2]*(1<<16))/length;
	}
		
	return length;

}

int spatialise (fifo_sound_t *snd)
{
	int master_v = snd->fixed_volume * 255;
	int diff;
	
	int leftvol = (int)master_v;
	int rightvol = (int)master_v;
	
//	printf("att %.2f, vol %.2f\n", attenuation, fvol);
//	printf("or %.2f %.2f %.2f\n", origin[0], origin[1], origin[2]);
	
	{
		int dist_mult,dist,dot,rscale,lscale,scale;
		int source_vec[3];
		long long dd;
		dist_mult = snd->fixed_attenuation * FSOUND_NOMINAL_CLIP_DISTANCE_MULT;
		
		//VectorSubtract(origin, listener_origin, source_vec);
		source_vec[0] = snd->origin[0] - listener_origin[0]
		source_vec[1] = snd->origin[1] - listener_origin[1]
		source_vec[2] = snd->origin[2] - listener_origin[2]
		
		//dist = VectorNormalize(source_vec) * dist_mult;
		dist = norm(source_vec) * dist_mult;
		
		//dot = DotProduct(listener_right, source_vec);
		dd = listener_right[0]*source_vec[0] + listener_right[1]*source_vec[1] + listener_right[2]*source_vec[2];
		dot = dd>>16;
//		printf("dist %.2f dot %.2f\n", dist, dot);
		
		rscale = (1<<16) + dot;
		lscale = (1<<16) - dot;
		
		scale = ((1<<16) - dist) * rscale;
		rightvol = (master_v * scale)>>16;
		
		if (rightvol < 0)
			rightvol = 0;
		
		scale = ((1<<16) - dist) * lscale;
		leftvol = (master_v * scale)>>16;
		
		if (leftvol < 0)
			leftvol = 0;
			
//		printf("l %d, r %d\n", leftvol, rightvol);
			
		if ((leftvol == 0) && (rightvol == 0))
			return 0;
	}
	
	diff = (leftvol - rightvol) >> 3;
	*ds_pan = 64 - diff;
	
	*ds_vol = (leftvol + rightvol) >> 2;
	
	return 1;
}
*/

void ds_update_sounds()
{
	int i;
	
	for(i=0;i<16;i++)
	{
		//look for enabled repeating sounds
		if ( (SCHANNEL_CR(i) & (SCHANNEL_ENABLE|SOUND_REPEAT)) != (SCHANNEL_ENABLE|SOUND_REPEAT))
		{
			continue;
		}
		//if they are too quite stop them
		
	}
}

int ds_set_position(int *pos)
{
	listener_origin[0] = *pos++;
	listener_origin[1] = *pos++;
	listener_origin[2] = *pos++;
	
	listener_right[0] = *pos++;
	listener_right[1] = *pos++;
	listener_right[2] = *pos;
	
	return 0;
}

void IPCReceiveUser1(u32 command, const u32 *data, u32 wordCount)
{
	
	switch(command)
	{
	case 1:
		ds_play_sound((fifo_sound_t*)data);
		break;
	case 2:
		ds_set_position((int*)data);
		break;
	}
	
}

#ifdef USE_WIFI
int wifi_initialized = 0;
u32 wifi_init_value = 0;

void wifi_arm7_synctoarm9()
{
	IPCFifoSendWordAsync(FIFO_SUBSYSTEM_WIFI,2,(u32)0x2004);
}

void IPC_Wifi(u32 command, const u32 *data, u32 wordCount)
{
	
	switch(command)
	{
	case 0://waiting for arm7
		IPCFifoSendWordAsync(FIFO_SUBSYSTEM_WIFI,0,(u32)0x2004);
		break;
	case 1://init value for wifi arm7
		Wifi_Init(*data);
		Wifi_SetSyncHandler(wifi_arm7_synctoarm9); // allow wifi lib to notify arm9
		IPCFifoSendWordAsync(FIFO_SUBSYSTEM_WIFI,1,(u32)0x2004);
		break;
	case 2:
		Wifi_Sync();
		IPCFifoSendWordAsync(FIFO_SUBSYSTEM_WIFI,2,(u32)0x2004);
		break;
	}
	
}
#endif
//---------------------------------------------------------------------------------
int main(int argc, char ** argv) {
//---------------------------------------------------------------------------------

	// read User Settings from firmware
	readUserSettings();

	//enable sound
	powerON(POWER_SOUND);
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	SOUND_CR = SOUND_ENABLE | SOUND_VOL(0x7F);

	irqInit();

	// Start the RTC tracking IRQ
	initClockIRQ();

	SetYtrigger(80);
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT);
	
#ifdef USE_WIFI
	irqSet(IRQ_WIFI, Wifi_Interrupt); // set up wifi interrupt
	irqEnable(IRQ_WIFI);

#endif	

	IPCFifoInit();

	IPCFifoSetHandler(FIFO_SUBSYSTEM_SOUND, IPCReceiveUser1);
	
#ifdef USE_WIFI
	IPCFifoSetHandler(FIFO_SUBSYSTEM_WIFI, IPC_Wifi);
#endif
	
	// Keep the ARM7 mostly idle
	while (1) swiWaitForVBlank();
}


