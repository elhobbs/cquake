#include "quakedef.h"
#include "hash.h"
#ifdef NDS
#include "IPCFifo.h"
#endif

int ds_startSound9(fifo_sound_t *fsnd)
{
#ifdef NDS
	IPCFifoSendMultiAsync(FIFO_SUBSYSTEM_SOUND,1,(const u32 *)fsnd,sizeof(*fsnd)/sizeof(int));
#endif
	return 0;
}

int ds_update_position(vec3_t origin,vec3_t vright)
{
	int data[6];

	data[0] = (int)origin[0];
	data[1] = (int)origin[1];
	data[2] = (int)origin[2];

	data[3] = (int)(vright[0]*(1<<16));
	data[4] = (int)(vright[1]*(1<<16));
	data[5] = (int)(vright[2]*(1<<16));

#ifdef NDS
	IPCFifoSendMultiAsync(FIFO_SUBSYSTEM_SOUND,2,(const u32 *)&data,sizeof(data)/sizeof(int));
#endif
	return 0;
}
