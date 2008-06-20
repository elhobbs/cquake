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
// snd_null.c -- include this instead of all the other snd_* files to have
// no sound code whatsoever

#include "quakedef.h"
#include "hash.h"

hashtable_t *sfx_known_hash;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

cvar_t bgmvolume = {"bgmvolume", "1", true};
cvar_t volume = {"volume", "0.7", true};

cvar_t nosound = {"nosound", "0"};
cvar_t precache = {"precache", "1"};
cvar_t loadas8bit = {"loadas8bit", "1"};

// pointer should go away
volatile dma_t  *shm = 0;
volatile dma_t sn;

qboolean		snd_initialized = false;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
float		sound_nominal_clip_dist=1000.0;
 
void S_Init (void)
{
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&precache);
	Cvar_RegisterVariable(&loadas8bit);

	if(nosound.value)
		return;

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	snd_initialized = true;
}

void S_AmbientOff (void)
{
}

void S_AmbientOn (void)
{
}

void S_Shutdown (void)
{
}

sfx_t *S_FindName (char *name)
{
	char *ED_NewString (char *string);
	sfx_t	*sfx;

	if (!name[0])
		Sys_Error ("Mod_ForName: NULL name");

	sfx = (sfx_t*)Hash_Get(sfx_known_hash,name);
	if(sfx != 0)
		return sfx;
	else
	{
		char *str = ED_NewString(name);
		bucket_t *bucket = (bucket_t*)Hunk_AllocName(sizeof(bucket_t) + sizeof(model_t) + 1,"sfx_known_hash");
		sfx = (sfx_t *)(bucket+1);
		sfx->name = str;
		Hash_Add(sfx_known_hash, sfx->name, sfx, bucket);
		return sfx;
	}
	return 0;
}

void S_TouchSound (char *name)
{
	sfx_t	*sfx;

	if (nosound.value)
		return;

	sfx = S_FindName (name);
	if(sfx)
		Cache_Check (&sfx->cache);
}

void S_ClearBuffer (void)
{
}
#define SOUND_NOMINAL_CLIP_DISTANCE_MULT 0.001f

int spatialise (int entnum, float fvol, float attenuation, vec3_t origin, int *ds_pan, int *ds_vol)
{
	float master_v = fvol * 255.0f;
		int diff;
	
	int leftvol = (int)master_v;
	int rightvol = (int)master_v;
	
//	printf("att %.2f, vol %.2f\n", attenuation, fvol);
//	printf("or %.2f %.2f %.2f\n", origin[0], origin[1], origin[2]);
	
	if (entnum != cl.viewentity)
	{
		float dist_mult,dist,dot,rscale,lscale,scale;
		vec3_t source_vec;
		dist_mult = attenuation * SOUND_NOMINAL_CLIP_DISTANCE_MULT;
		
		VectorSubtract(origin, listener_origin, source_vec);
		
		dist = VectorNormalize(source_vec) * dist_mult;
		dot = DotProduct(listener_right, source_vec);
		
//		printf("dist %.2f dot %.2f\n", dist, dot);
		
		rscale = 1.0f + dot;
		lscale = 1.0f - dot;
		
		scale = (1.0f - dist) * rscale;
		rightvol = (int)(master_v * scale);
		
		if (rightvol < 0)
			rightvol = 0;
		
		scale = (1.0f - dist) * lscale;
		leftvol = (int)(master_v * scale);
		
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
	length = intsqrt (length);		// FIXME

	if (length)
	{
		v[0] = (v[0]*(1<<16))/length;
		v[1] = (v[1]*(1<<16))/length;
		v[2] = (v[2]*(1<<16))/length;
	}
		
	return length;

}

int ispatialise (fifo_sound_t *snd, int *ds_pan, int *ds_vol)
{
	int master_v = (snd->fixed_volume * 255)>>16;
	int diff;
	
	int leftvol = (int)master_v;
	int rightvol = (int)master_v;

	int listener_right1[3];

	listener_right1[0] = listener_right[0] *(1<<16);
	listener_right1[1] = listener_right[1] *(1<<16);
	listener_right1[2] = listener_right[2] *(1<<16);
	
//	printf("att %.2f, vol %.2f\n", attenuation, fvol);
//	printf("or %.2f %.2f %.2f\n", origin[0], origin[1], origin[2]);
	
	{
		int dist_mult,dist,dot,rscale,lscale,scale;
		int source_vec[3];
		long long dd;
		dist_mult = (snd->fixed_attenuation * FSOUND_NOMINAL_CLIP_DISTANCE_MULT)>>16;
		
		//VectorSubtract(origin, listener_origin, source_vec);
		source_vec[0] = snd->origin[0] - listener_origin[0];
		source_vec[1] = snd->origin[1] - listener_origin[1];
		source_vec[2] = snd->origin[2] - listener_origin[2];
		
		//dist = VectorNormalize(source_vec) * dist_mult;
		dist = norm(source_vec) * dist_mult;
		
		//dot = DotProduct(listener_right, source_vec);
		dd = (((long long)listener_right1[0]*source_vec[0])>>16) + (((long long)listener_right1[1]*source_vec[1])>>16) + (((long long)listener_right1[2]*source_vec[2])>>16);
		dot = dd>>16;
//		printf("dist %.2f dot %.2f\n", dist, dot);
		
		rscale = (1<<16) + dot;
		lscale = (1<<16) - dot;
		
		dd = ((1<<16) - dist) * (long long)rscale;
		scale = dd>>16;
		rightvol = (master_v * scale)>>16;
		
		if (rightvol < 0)
			rightvol = 0;
		
		dd = ((1<<16) - dist) * (long long)lscale;
		scale = dd>>16;
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

void S_StaticSound (sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	sfxcache_t	*sc;
	fifo_sound_t fs;
	int ds_pan, ds_vol;
	int ds_pan1, ds_vol1;

	if (!snd_initialized)
		return;

	if (!sfx)
		return;

	if (nosound.value)
		return;

	if (!spatialise(-1, fvol * volume.value, attenuation, origin, &ds_pan, &ds_vol))
		return;

	sc = S_LoadSound (sfx);
	if (!sc)
	{
		//target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}
	fs.data = sc->data;
	fs.format = 1;
	fs.loop = -1;
	fs.pan = ds_pan;
	fs.rate = sc->speed;
	fs.size = sc->length;
	fs.volume = ds_vol;
	fs.fixed_attenuation = (int)(attenuation*(1<<16));
	fs.fixed_volume = (int)(fvol*(1<<16));
	fs.origin[0] = (int)origin[0];
	fs.origin[1] = (int)origin[1];
	fs.origin[2] = (int)origin[2];

	ispatialise(&fs, &ds_pan1, &ds_vol1);

	ds_startSound9(&fs);
}

void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation)
{
	sfxcache_t	*sc;
	fifo_sound_t fs;
	int ds_pan, ds_vol;
	int ds_pan1, ds_vol1;

	if (!snd_initialized)
		return;

	if (!sfx)
		return;

	if (nosound.value)
		return;

	fs.format = 1;
	fs.loop = -1;
	fs.fixed_attenuation = (int)(attenuation*(1<<16));
	fs.fixed_volume = (int)(fvol*(1<<16));
	fs.origin[0] = (int)origin[0];
	fs.origin[1] = (int)origin[1];
	fs.origin[2] = (int)origin[2];

	if(!ispatialise(&fs, &ds_pan, &ds_vol))
		return;
	//if (!spatialise(entnum, fvol * volume.value, attenuation, origin, &ds_pan, &ds_vol))
	//	return;

	sc = S_LoadSound (sfx);
	if (!sc)
	{
		//target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}
	fs.pan = ds_pan;
	fs.volume = ds_vol;
	fs.rate = sc->speed;
	fs.size = sc->length;
	fs.data = sc->data;

	ds_startSound9(&fs);
}

void S_StopSound (int entnum, int entchannel)
{
}

sfx_t *S_PrecacheSound (char *name)
{
	sfx_t	*sfx;

	if (nosound.value)
		return NULL;

	sfx = S_FindName (name);
	
// cache it in
	if (precache.value)
		S_LoadSound (sfx);
	
	return sfx;
}

void S_ClearPrecache (void)
{
}

void S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{	
	if (nosound.value)
		return;
	if (!snd_initialized)
		return;
	VectorCopy(origin, listener_origin);
	VectorCopy(v_forward, listener_forward);
	VectorCopy(v_right, listener_right);
	VectorCopy(v_up, listener_up);

	ds_update_position(origin,v_right);
}

void S_StopAllSounds (qboolean clear)
{
}

void S_BeginPrecaching (void)
{
}

void S_EndPrecaching (void)
{
}

void S_ExtraUpdate (void)
{
}

void S_LocalSound (char *s)
{
	sfx_t	*sfx;

	if (nosound.value)
		return;
	if (!snd_initialized)
		return;
		
	sfx = S_PrecacheSound (s);
	if (!sfx)
	{
		Con_Printf ("S_LocalSound: can't cache %s\n", s);
		return;
	}
	S_StartSound (cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}

