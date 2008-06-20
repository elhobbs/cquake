#include "quakedef.h"
#ifdef NDS
#include <nds.h>
u16	d_8to16table[256];
uint32 nextPBlock = (uint32)0;


//---------------------------------------------------------------------------------
inline uint32 aalignVal( uint32 val, uint32 to ) {
	return (val & (to-1))? (val & ~(to-1)) + to : val;
}

//---------------------------------------------------------------------------------
int ndsgetNextPaletteSlot(u16 count, uint8 format) {
//---------------------------------------------------------------------------------
	// ensure the result aligns on a palette block for this format
	uint32 result = aalignVal(nextPBlock, 1<<(4-(format==GL_RGB4)));

	// convert count to bytes and align to next (smallest format) palette block
	count = aalignVal( count<<1, 1<<3 ); 

	// ensure that end is within palette video mem
	if( result+count > 0x10000 )   // VRAM_F - VRAM_E
		return -1;

	nextPBlock = result+count;
	return (int)result;
} 

//---------------------------------------------------------------------------------
void ndsTexLoadPalVRAM(const u16* pal, u16 count, u32 addr) {
//---------------------------------------------------------------------------------
	vramSetBankF(VRAM_F_LCD);
	//swiCopy( pal, &VRAM_F[addr>>1] , count / 2 | COPY_MODE_WORD);
	dmaCopyWords(3,(uint32*)pal, (uint32*)&VRAM_F[addr>>1] , count<<1);
	vramSetBankF(VRAM_F_TEX_PALETTE);
}

//---------------------------------------------------------------------------------
uint32 ndsTexLoadPal(const u16* pal, u16 count, uint8 format) {
//---------------------------------------------------------------------------------
	uint32 addr = ndsgetNextPaletteSlot(count, format);
	if( addr>=0 )
		ndsTexLoadPalVRAM(pal, count, (u32) addr);

	return addr;
}

u16 ds_alpha_pal_bytes[32];
uint32 ds_alpha_pal;
uint32 ds_texture_pal;
// called at startup and after any gamma correction
void	VID_SetPalette (unsigned char *palette)
{
}
void	VID_InitPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned short i;
	u16	v,*table;
	static qboolean palflag = false;
	float inf,gm = 1.1f;

	//VID_LightmapPal();
//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to16table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
#if 1
		inf = r;
		inf = 255 * pow ( (inf+0.5)/255.5 , gm ) + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		r = inf;

		inf = g;
		inf = 255 * pow ( (inf+0.5)/255.5 , gm ) + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		g = inf;

		inf = b;
		inf = 255 * pow ( (inf+0.5)/255.5 , gm ) + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		b = inf;
#endif
		pal += 3;
		
		v = (1<<15)|RGB15(r>>3,g>>3,b>>3);
		*table++ = v;
		BG_PALETTE[i] = v;
		BG_PALETTE_SUB[i] = v;
	}
	BG_PALETTE[255] = 0;
	BG_PALETTE_SUB[255] = 0;
	//d_8to16table[255] = d_8to16table[0];
	//d_8to16table[255] &= 0xffffff;	// 255 is transparent
	ds_texture_pal = ndsTexLoadPal(d_8to16table,256,GL_RGB256);
	glColorTable(GL_RGB256,ds_texture_pal);

	for(i=0;i<32;i++)
	{
		ds_alpha_pal_bytes[i] = RGB15(i,i,i);
	}
	ds_alpha_pal = ndsTexLoadPal(ds_alpha_pal_bytes,32,GL_RGB32_A3);
	glColorTable(GL_RGB32_A3,ds_alpha_pal);

	glColorTable(GL_RGB256,ds_texture_pal);
}

#else
void	VID_InitPalette (unsigned char *palette)
{
}
void	VID_SetPalette (unsigned char *palette)
{
}
#endif
// called for bonus and pain flashes, and for underwater color changes
void	VID_ShiftPalette (unsigned char *palette)
{
#ifdef NDS
	int i;
	byte	*pal;
	unsigned r,g,b;
	u16	v,*table;

	pal = palette;
	table = d_8to16table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
		v = (1<<15)|RGB15(r>>3,g>>3,b>>3);
		*table++ = v;
	}
	vramSetBankF(VRAM_F_LCD);
	//swiCopy( pal, ds_texture_pal , count / 2 | COPY_MODE_WORD);
	dmaCopyWords(3,(uint32*)d_8to16table, (uint32*)&VRAM_F[ds_texture_pal>>1] , 256<<1);
	vramSetBankF(VRAM_F_TEX_PALETTE);
#endif
}

// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again
void	VID_Init (unsigned char *palette)
{
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.width = 256;
	vid.height = 192;
	vid.aspect = 256.0f/192.0f;
	vid.numpages = 1;

	//Check_Gamma(palette);
	VID_InitPalette (palette);

	VID_SetMode (0, palette);
	//vid.buffer = BG_GFX;
	//vid.buffer = Hunk_AllocName(320*200,"screen");
}

// Called at shutdown
void	VID_Shutdown (void)
{
}

void Draw_UpdateVRAM();
// flushes the given rectangles from the view buffer to the screen
void	VID_Update (vrect_t *rects)
{
	Draw_UpdateVRAM();
}

// sets the mode; only used by the Quake engine for resetting to mode 0 (the
// base mode) on memory allocation failures
int VID_SetMode (int modenum, unsigned char *palette)
{
	return 0;
}

// called only on Win32, when pause happens, so the mouse can be released
void VID_HandlePause (qboolean pause)
{
}

void VID_MenuDraw (void)
{
}

void VID_MenuKey (int key)
{
}

void D_UpdateRects (vrect_t *prect)
{
	UNUSED(prect);
}

