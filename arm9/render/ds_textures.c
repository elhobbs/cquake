#include "quakedef.h"
#include "ds_textures.h"

#define NUM_TEXTURE_BLOCKS 128
#define NUM_TEXTURE_MASK (NUM_TEXTURE_BLOCKS-1)

#define TEXTURE_BLOCK_SIZE (32*32)

#define TEXTURE_RESIDENT 1

#define NUM_TEXTURE_BLOCKS2 48

#define TEXTURE_BLOCK_SIZE2 (128*64)

extern int		r_framecount;

byte *DS_TEXTURE_BASE;
byte *DS_TEXTURE_BASE2;
#define DS_TEXTURE_MEM_SIZE (256*256*2*4)

typedef struct {
	char *name;
	int visframe;
	int status;
	int texnum;
} ds_texture_t;

ds_texture_t ds_textures[NUM_TEXTURE_BLOCKS];
ds_texture_t ds_textures2[NUM_TEXTURE_BLOCKS2];

int r_sky_top, r_sky_bottom;


int foo()
{
	return 0;
}

void texstats_f (void)
{
	int i;
	int used = 0;
	int used2 = 0;
	int in = 0;
	int in2 = 0;

	for(i=0;i<NUM_TEXTURE_BLOCKS;i++)
	{
		if(r_framecount - ds_textures[i].visframe < 2)
		{
			used++;
		}
		else if(ds_textures[i].name)
		{
			in++;
		}
	}

	for(i=0;i<NUM_TEXTURE_BLOCKS2;i++)
	{
		if(r_framecount - ds_textures2[i].visframe < 2)
		{
			used2++;
		}
		else if(ds_textures[i].name)
		{
			in2++;
		}
	}

	Con_Printf("v: %3d  i: %3d a: %3d\n",used,in,NUM_TEXTURE_BLOCKS);
	Con_Printf("v: %3d  i: %3d a: %3d\n",used2,in2,NUM_TEXTURE_BLOCKS2);
}
void Tex_Init()
{
	Cmd_AddCommand ("texstats", texstats_f);
}

void Init_DS_Textures()
{
#ifdef WIN32
	DS_TEXTURE_BASE = (byte*)malloc(DS_TEXTURE_MEM_SIZE);
	DS_TEXTURE_BASE2 = (byte*)malloc(DS_TEXTURE_MEM_SIZE);
#else
	DS_TEXTURE_BASE = (byte*)VRAM_A;
	DS_TEXTURE_BASE2 = (byte*)VRAM_B;
#endif
	memset(ds_textures,0,sizeof(ds_textures));
	memset(ds_textures2,0,sizeof(ds_textures2));
}

void ds_lock_block(int block)
{
#ifdef NDS
	switch(block>>7)
	{
	case 0:
		vramSetBankA(VRAM_A_TEXTURE);
		break;
	default:
		Sys_Error("ds_lock_block: %d",block);
		break;
	}
#endif
}

void ds_unlock_block(int block)
{
#ifdef NDS
	switch(block>>7)
	{
	case 0:
		vramSetBankA(VRAM_A_LCD);
		break;
	default:
		Sys_Error("ds_lock_block: %d",block);
		break;
	}
#endif
}

void ds_lock_block2(int block)
{
#ifdef NDS
	switch(block>>4)
	{
	case 0:
		vramSetBankB(VRAM_B_TEXTURE);
		break;
	case 1:
		vramSetBankC(VRAM_C_TEXTURE);
		break;
	case 2:
		vramSetBankD(VRAM_D_TEXTURE);
		break;
	}
#endif
}

void ds_unlock_block2(int block)
{
#ifdef NDS
	switch(block>>4)
	{
	case 0:
		vramSetBankB(VRAM_B_LCD);
		break;
	case 1:
		vramSetBankC(VRAM_C_LCD);
		break;
	case 2:
		vramSetBankD(VRAM_D_LCD);
		break;
	}
#endif
}

int ds_adjust_size(int x)
{
	//if(x >= 64)
	//{
	//	return 64;
	//}
	if(x > 16)
	{
		return 32;
	}
	if(x > 8)
	{
		return 16;
	}
	return 8;
}

int ds_adjust_size2(int x)
{
	if(x > 64)
	{
		return 128;
	}
	if(x > 32)
	{
		return 64;
	}
	if(x > 16)
	{
		return 32;
	}
	if(x > 8)
	{
		return 16;
	}
	return 8;
}
int last_free_block = -1;

int find_free_block(char *name)
{
	int i;
	
	//names are in a hash list so they do not need strcmp
	for(i=0;i<NUM_TEXTURE_BLOCKS;i++)
	{
		if(ds_textures[i].name == name)
			return i;
	}
	
	//do not start at beggining - might leave textures in longer
	//look for ones that have not been used in a couple frames
	for(i=0;i<NUM_TEXTURE_BLOCKS;i++)
	{
		last_free_block++;
		
		if(r_framecount - ds_textures[last_free_block&NUM_TEXTURE_MASK].visframe > 1)
		{
			return last_free_block&NUM_TEXTURE_MASK;
		}
	}
	return -1;
}

int last_free_block2 = -1;
int find_free_block2(char *name)
{
	int i;
	
	//names are in a hash list so they do not need strcmp
	for(i=0;i<NUM_TEXTURE_BLOCKS2;i++)
	{
		if(ds_textures2[i].name == name)
			return i;
	}
	
	//do not start at beggining - might leave textures in longer
	//look for ones that have not been used in a couple frames
	for(i=0;i<NUM_TEXTURE_BLOCKS2;i++)
	{
		last_free_block2++;
		if(last_free_block2 == NUM_TEXTURE_BLOCKS2)
		{
			last_free_block2 = 0;
		}
		
		if(r_framecount - ds_textures2[last_free_block2].visframe > 1)
		{
			return last_free_block2;
		}
	}
	return -1;
}

int ds_is_texture_resident(dstex_t *tx)
{
	int block = tx->block&NUM_TEXTURE_MASK;
	if(ds_textures[block].name == tx->name)
		return block;
	return -1;
}

int ds_is_texture_resident2(dstex_t *tx)
{
	int block = tx->block%NUM_TEXTURE_BLOCKS2;
	if(ds_textures2[block].name == tx->name)
		return block;
	return -1;
}

byte* ds_scale_texture(dstex_t *tx,int inwidth,int inheight,byte *in,byte *outt)
{
	int width,height,i,j;
	//int inwidth, inheight;
	unsigned	frac, fracstep;
	byte *inrow,*out;
	//int has255 = 0;

	width = tx->width;
	height = tx->height;

	if(inwidth == width && inheight == height)
	{
		return in;
	}

	out = outt;
	fracstep = (inwidth<<16)/width;
	for (i=0 ; i<height ; i++, out += width)
	{
		inrow = (in + inwidth*(i*inheight/height));
		frac = fracstep >> 1;
		for (j=0 ; j<width ; j++)
		{
			out[j] = inrow[frac>>16];
			/*
			if(out[j] == 0xff)
			{
				has255 = GL_TEXTURE_COLOR0_TRANSPARENT;
				out[j] = 0;
			} else if(out[j] == 0)
			{
				out[j] = 0xff;
			}*/
			frac += fracstep;
		}
	}
	return outt;
}
#ifdef NDS
static inline void DSdmaCopyWords(uint8 channel, const void* src, void* dest, uint32 size) {
	DMA_SRC(channel) = (uint32)src;
	DMA_DEST(channel) = (uint32)dest;
	if(REG_VCOUNT > 191)
	{
		DMA_CR(channel) = DMA_COPY_WORDS | (size>>2);
	}
	else
	{
		DMA_CR(channel) = DMA_COPY_WORDS | DMA_START_HBL | (size>>2);
	}
	while(DMA_CR(channel) & DMA_BUSY);
}
#endif

byte *ds_block_address(int block)
{
	return DS_TEXTURE_BASE + (block * TEXTURE_BLOCK_SIZE);
}

byte* ds_load_block(int block,byte *texture,int size)
{
	byte *addr = ds_block_address(block);

	ds_unlock_block(block);

#ifdef WIN32
	memcpy(addr,texture,size);
#else
	DC_FlushAll();
	DSdmaCopyWords(3,(uint32*)texture, (uint32*)addr , size);
#endif
	
	ds_lock_block(block);
	return addr;
}

byte *ds_block_address2(int block)
{
	return DS_TEXTURE_BASE2 + (block * TEXTURE_BLOCK_SIZE2);
}

byte* ds_load_block2(int block,byte *texture,int size)
{
	byte *addr = ds_block_address2(block);

	ds_unlock_block2(block);

#ifdef WIN32
	memcpy(addr,texture,size);
#else
	DSdmaCopyWords(3,(uint32*)texture, (uint32*)addr , size);
#endif
	
	ds_lock_block2(block);
	return addr;
}

int ds_tex_size(int x)
{
	switch(x)
	{
	case 512:
		return 6;
	case 256:
		return 5;
	case 128:
		return 4;
	case 64:
		return 3;
	case 32:
		return 2;
	case 16:
		return 1;
	case 8:
		return 0;
	}
	return 0;
}

int ds_tex_parameters(	int sizeX, int sizeY,
						const byte* addr,
						int mode,
						unsigned int param)
{
//---------------------------------------------------------------------------------
	return param | (sizeX << 20) | (sizeY << 23) | (((unsigned int)addr >> 3) & 0xFFFF) | (mode << 26);
}

int ds_loadTexture(dstex_t *ds,int w,int h,byte *buf,int trans)
{
	int block;
	byte *addr;

	block = find_free_block(ds->name);
	if(block == -1)
		return 0;
	addr = ds_load_block(block,buf,w*h);
	ds->block = block;
#ifdef WIN32
	ds_textures[block].texnum = ds_tex_parameters(ds_tex_size(w),ds_tex_size(h),addr,0,0);
#else
	ds_textures[block].texnum = ds_tex_parameters(ds_tex_size(w),ds_tex_size(h),
		addr,GL_RGB256,TEXGEN_TEXCOORD|GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T|
		(trans  ? GL_TEXTURE_COLOR0_TRANSPARENT : 0));
#endif
	ds_textures[block].name = ds->name;
	ds_textures[block].visframe = r_framecount;
	return ds_textures[block].texnum;
}

int ds_loadTexture2(dstex_t *ds,int w,int h,byte *buf)
{
	int block;
	byte *addr;

	block = find_free_block2(ds->name);
	if(block == -1)
		return 0;
	addr = ds_load_block2(block,buf,w*h);
	ds->block = block;
#ifdef WIN32
	ds_textures2[block].texnum = ds_tex_parameters(ds_tex_size(w),ds_tex_size(h),addr,0,0);
#else
	ds_textures2[block].texnum = ds_tex_parameters(ds_tex_size(w),ds_tex_size(h),addr,GL_RGB256,TEXGEN_TEXCOORD|GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T);
#endif
	ds_textures2[block].name = ds->name;
	ds_textures2[block].visframe = r_framecount;
	return ds_textures2[block].texnum;
}

byte	dottexture[8][8] =
{
	{0x0,0xf,0xf,0xf,0,0,0,0},
	{0xf,0xf,0xf,0xf,0xf,0,0,0},
	{0xf,0xf,0xf,0xf,0xf,0,0,0},
	{0xf,0xf,0xf,0xf,0xf,0,0,0},
	{0x0,0xf,0xf,0xf,0,0,0,0},
	{0x0,0x0,0x0,0x0,0,0,0,0},
	{0x0,0x0,0x0,0x0,0,0,0,0},
	{0x0,0x0,0x0,0x0,0,0,0,0},
};

int ds_load_particle_texture(dstex_t *ds)
{
	int handle, length, size, block, w, h;
	byte *buf,*addr,*dst;


	block = ds_is_texture_resident(ds);
	if(block != -1)
	{
		ds_textures[block].visframe = r_framecount;
		return ds_textures[block].texnum;
	}
	Con_DPrintf("%s %d %d\n",ds->name,ds->width,ds->height);
	return ds_loadTexture(ds,8,8,&dottexture[0][0],1);
}

int ds_loadTextureSky(dstex_t *ds,int w,int h,byte *buf)
{
	int block;
	byte *addr;

	block = find_free_block2(ds->name);
	if(block == -1)
		return 0;
	if(0)
	{
		int ww,hh;
		byte *top = buf;
		for(hh=0;hh<64;hh++)
		{
			for(ww=0;ww<64;ww++)
			{
				top[(hh*64)+ww] = (ww/2) | ((hh/8)<<5);
			}
		}
	}
	addr = ds_load_block2(block,buf,w*h);
	ds->block = block;
#ifdef WIN32
	ds_textures2[block].texnum = ds_tex_parameters(ds_tex_size(w),ds_tex_size(h),addr,0,0);
	r_sky_top = ds_tex_parameters(ds_tex_size(w>>1),ds_tex_size(h),addr,0,0);
	r_sky_bottom = ds_tex_parameters(ds_tex_size(w>>1),ds_tex_size(h),addr+((w>>1)*h),0,0);
#else
	ds_textures2[block].texnum = ds_tex_parameters(ds_tex_size(w),ds_tex_size(h),addr,
		GL_RGB256,TEXGEN_TEXCOORD|GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T);
	if(0)
	{
		r_sky_top = ds_tex_parameters(ds_tex_size(w>>1),ds_tex_size(h),addr,
			GL_RGB32_A3,TEXGEN_TEXCOORD|GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T);
		r_sky_bottom = ds_tex_parameters(ds_tex_size(w>>1),ds_tex_size(h),addr+((w>>1)*h),
			GL_RGB256,TEXGEN_TEXCOORD|GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T);
	}
	else
	{
		r_sky_top = ds_tex_parameters(ds_tex_size(w>>1),ds_tex_size(h),addr,
			GL_RGB256,TEXGEN_TEXCOORD|GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T|GL_TEXTURE_COLOR0_TRANSPARENT);
		r_sky_bottom = ds_tex_parameters(ds_tex_size(w>>1),ds_tex_size(h),addr+((w>>1)*h),
			GL_RGB256,TEXGEN_TEXCOORD|GL_TEXTURE_WRAP_S|GL_TEXTURE_WRAP_T);
	}
	//r_sky_top = r_sky_bottom = ds_textures2[block].texnum;
#endif
	ds_textures2[block].name = ds->name;
	ds_textures2[block].visframe = r_framecount;
	return ds_textures2[block].texnum;
}

byte * COM_LoadTempFilePartExtra(int handle,int offset,int length,int extra);
int ds_load_bsp_texture2(model_t *mod,texture_t *texture);

int ds_load_bsp_texture(model_t *mod,texture_t *texture)
{
	int handle, length, size, block, w, h;
	byte *buf,*addr,*dst;

	if(texture->ds.width > 32 || texture->ds.height > 32)
	{
		return ds_load_bsp_texture2(mod,texture);
	}

	block = ds_is_texture_resident(&texture->ds);
	if(block != -1)
	{
		ds_textures[block].visframe = r_framecount;
		return ds_textures[block].texnum;
	}
	Con_DPrintf("%s %d %d\n",texture->ds.name,texture->ds.width,texture->ds.height);

	size = texture->width * texture->height;
	if(texture == r_notexture_mip)
	{
		dst = (byte *)r_notexture_mip + r_notexture_mip->offsets[0];
		w = r_notexture_mip->width;
		h = r_notexture_mip->height;
	}
	else
	{
		w = texture->ds.width;
		h = texture->ds.height;
		if(mod->name[0] == '*')
		{
			length = COM_OpenFile(cl.worldmodel->name,&handle);
		}
		else
		{
			length = COM_OpenFile(mod->name,&handle);
		}
		if(length == -1)
		{
			//Sys_Error ("Mod_NumForName: %s not found", mod->name);
			return 0;
		}
		if(texture->width < 64 || texture->height < 64)
		{
			buf = COM_LoadTempFilePartExtra(handle,texture->ds.file_offset,size,texture->ds.width*texture->ds.height);
			COM_CloseFile(handle);
			if(buf == 0)
				return 0;
			dst = buf + size;

			dst = ds_scale_texture(&texture->ds,texture->width,texture->height,buf,dst);
		}
		else
		{
			buf = COM_LoadTempFilePartExtra(handle,texture->ds.file_offset+size,size>>2,texture->ds.width*texture->ds.height);
			COM_CloseFile(handle);
			if(buf == 0)
				return 0;
			dst = buf + (size>>2);

			dst = ds_scale_texture(&texture->ds,texture->width>>1,texture->height>>1,buf,dst);
		}
	}

	return ds_loadTexture(&texture->ds,w,h,dst,0);
}

int ds_load_bsp_texture2(model_t *mod,texture_t *texture)
{
	int handle, length, size, block, w, h;
	byte *buf,*addr,*dst;

	block = ds_is_texture_resident2(&texture->ds);
	if(block != -1)
	{
		ds_textures2[block].visframe = r_framecount;
		return ds_textures2[block].texnum;
	}
	Con_DPrintf("%s %d %d\n",texture->ds.name,texture->ds.width,texture->ds.height);

	w = texture->ds.width;
	h = texture->ds.height;
	size = texture->width * texture->height;
	if(mod->name[0] == '*')
	{
		length = COM_OpenFile(cl.worldmodel->name,&handle);
	}
	else
	{
		length = COM_OpenFile(mod->name,&handle);
	}
	if(length == -1)
	{
		//Sys_Error ("Mod_NumForName: %s not found", mod->name);
		return 0;
	}
	buf = COM_LoadTempFilePartExtra(handle,texture->ds.file_offset,size,texture->ds.width*texture->ds.height);
	COM_CloseFile(handle);

	if(buf == 0)
		return 0;
	dst = buf + size;

	dst = ds_scale_texture(&texture->ds,texture->width,texture->height,buf,dst);

	return ds_loadTexture2(&texture->ds,w,h,dst);
}

int ds_fixup_sky(byte *buf,int width,int height,byte*scrap) 
{
	int i , w;
	byte *dest,*dest2;

	w = width>>1;

	dest = scrap;
	dest2 = scrap + (w*height);
	for(i=0;i<height;i++)
	{
		memcpy(&dest[i*w],&buf[i*width],w);
		memcpy(&dest2[i*w],&buf[i*width + w],w);
	}
	return 0;
}
int ds_load_bsp_sky(model_t *mod,texture_t *texture)
{
	int handle, length, size, block, w, h, size2;
	byte *buf,*addr,*dst,*scrap;

	block = ds_is_texture_resident2(&texture->ds);
	if(block != -1)
	{
		ds_textures2[block].visframe = r_framecount;
		return ds_textures2[block].texnum;
	}
	Con_DPrintf("%s %d %d\n",texture->ds.name,texture->ds.width,texture->ds.height);

	w = texture->ds.width = 128;
	h = texture->ds.height = 64;
	size = texture->width * texture->height;
	if(mod->name[0] == '*')
	{
		length = COM_OpenFile(cl.worldmodel->name,&handle);
	}
	else
	{
		length = COM_OpenFile(mod->name,&handle);
	}
	if(length == -1)
	{
		//Sys_Error ("Mod_NumForName: %s not found", mod->name);
		return 0;
	}
	size2 = texture->ds.width*texture->ds.height;
	buf = COM_LoadTempFilePartExtra(handle,texture->ds.file_offset,size,size2<<1);
	COM_CloseFile(handle);

	if(buf == 0)
		return 0;
	dst = buf + size;
	scrap = dst + size2;

	dst = ds_scale_texture(&texture->ds,texture->width,texture->height,buf,dst);
	ds_fixup_sky(dst,texture->ds.width,texture->ds.height,scrap);

	return ds_loadTextureSky(&texture->ds,w,h,scrap);
}

int ds_load_alias_texture(model_t *mod,maliasskindesc_t	*pskindesc)
{
	int handle, length, size, block, w, h;
	byte *buf,*addr,*dst;

	block = ds_is_texture_resident2(&pskindesc->ds);
	if(block != -1)
	{
		ds_textures2[block].visframe = r_framecount;
		return ds_textures2[block].texnum;
	}
	Con_DPrintf("%s %d %d\n",mod->name,pskindesc->ds.width,pskindesc->ds.height);

	w = pskindesc->ds.width; 
	h = pskindesc->ds.height;
	size = pskindesc->width * pskindesc->height;
	if(mod->name[0] == '*')
	{
		length = COM_OpenFile(cl.worldmodel->name,&handle);
	}
	else
	{
		length = COM_OpenFile(mod->name,&handle);
	}
	if(length == -1)
	{
		//Sys_Error ("Mod_NumForName: %s not found", mod->name);
		return 0;
	}
	buf = COM_LoadTempFilePartExtra(handle,pskindesc->ds.file_offset,size,pskindesc->ds.width*pskindesc->ds.height);
	COM_CloseFile(handle);

	if(buf == 0)
		return 0;
	dst = buf + size;

	dst = ds_scale_texture(&pskindesc->ds,pskindesc->width,pskindesc->height,buf,dst);

	return ds_loadTexture2(&pskindesc->ds,w,h,dst);
}

void waitforit(void);

void ds_precache_bsp_textures(model_t *mod)
{
	int i, n;
	texture_t **tex;

	n = mod->bmodel->numtextures;
	tex = mod->bmodel->textures;

	Con_DPrintf("Precaching textures %d...\n",n);

	for(i=0;i<n;i++)
	{
		if (tex[i] && tex[i]->ds.name && Q_strncmp(tex[i]->ds.name,"sky",3))
		{
			Con_DPrintf("%d ",i);
			Con_DPrintf("%s\n",tex[i]->ds.name);
			ds_load_bsp_texture(mod,tex[i]);
		}
	}
	Con_DPrintf("\n");
}
