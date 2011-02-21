/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
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
#include <stdio.h>
#include <dswifi7.h>
//#include <maxmod7.h>
#include "mp3dec.h"
#include "mp3_shared.h"

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
#ifdef USE_WIFI
	Wifi_Update();
#endif
}


//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	readUserSettings();

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();

	//mmInstall(FIFO_MAXMOD);

	SetYtrigger(80);

#ifdef USE_WIFI
	installWifiFIFO();
#endif
	installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

	setPowerButtonCB(powerButtonCB);   

	mp3_init();

	// Keep the ARM7 mostly idle
	while (!exitflag) {
	
		mp3_process();
		
		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
			exitflag = true;
		}
		swiWaitForVBlank();
	}
	return 0;
}

#if 0
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

#ifdef USE_WIFI
#include <dswifi7.h>
#endif

//#include <maxmod7.h>
#include "mp3dec.h"
#include "mp3_shared.h"
	


int fifoError(char *buffer, ...) // expected to be defined externally.
{
	return 0;
}


//---------------------------------------------------------------------------------
void VcountHandler() {
	//---------------------------------------------------------------------------------
	inputGetAndSend();
}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
#ifdef USE_WIFI
	Wifi_Update();
#endif
}


//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	irqInit();
	fifoInit();

	// read User Settings from firmware
	readUserSettings();

	// Start the RTC tracking IRQ
	initClockIRQ();

	SetYtrigger(80);

#ifdef USE_WIFI
	installWifiFIFO();
#endif
	installSoundFIFO();
		//REG_SOUNDCNT = SOUND_ENABLE;
		//REG_MASTER_VOLUME = 127;

	//mmInstall(FIFO_MAXMOD);

	installSystemFIFO();
	
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);   

	mp3_init();

	// Keep the ARM7 mostly idle
	while (1) {
	
		mp3_process();
		
		swiWaitForVBlank();
	}
}

#endif
