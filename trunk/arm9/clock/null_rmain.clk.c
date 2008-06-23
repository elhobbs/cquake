#include "quakedef.h"
#include "null_ralias.h"
#include "ds_textures.h"


//
// view origin
//
vec3_t	r_vup, base_vup;
vec3_t	r_vpn, base_vpn;
vec3_t	r_vright, base_vright;
vec3_t	r_origin;
int		r_origini[3];
int		r_modelview[16];

mplane_t	r_screenedge[4];
mplane_t	r_frustum[4];
int			r_surf_draw, r_surf_cull;
int			r_surf_tri,r_alias_tri;

cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_draw = {"r_draw","1"};

//
// screen size info
//
refdef_t	r_refdef;
int			r_pixbytes = 1;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		r_currentkey;

texture_t	*r_notexture_mip;
texture_t	*r_particle_mip;

model_t		*r_currentmodel;
model_t		*r_worldmodel;
bmodel_t	*r_currentbmodel;
entity_t	*r_currententity;
mleaf_t		*r_viewleaf, *r_oldviewleaf;
mvertex_t	*r_pcurrentvertbase;


float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

#ifdef NDS
double Sys_FloatTime (void)
{
	return ((TIMER1_DATA*(1<<16))+TIMER0_DATA)/32728.5;
}
#endif

void R_RotateForEntity (entity_t *e)
{
#ifdef NDS
    glTranslate3f32 (floattodsv16(e->origin[0]),  floattodsv16(e->origin[1]),  floattodsv16(e->origin[2]));
#if 1
    glRotatef (-e->angles[1],  0, 0, 1);
    glRotatef (e->angles[0],  0, 1, 0);
    glRotatef (-e->angles[2],  1, 0, 0);
#else
    glRotateX (e->angles[2]);//,  0, 0, 1);
    glRotateY (-e->angles[0]);//,  0, 1, 0);
    glRotateZ (e->angles[1]);//,  1, 0, 0);
#endif
#endif
}

int QBoxOnPlaneSide (short *emins,short*emaxs, mplane_t *p)
{
	int	dist1, dist2;
	int		sides;

/*// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}*/

// general case
	switch (p->signbits)
	{
	case 0:
		dist1 = p->inormal[0]*(int)emaxs[0] + p->inormal[1]*(int)emaxs[1] + p->inormal[2]*(int)emaxs[2];
		dist2 = p->inormal[0]*(int)emins[0] + p->inormal[1]*(int)emins[1] + p->inormal[2]*(int)emins[2];
		break;
	case 1:
		dist1 = p->inormal[0]*(int)emins[0] + p->inormal[1]*(int)emaxs[1] + p->inormal[2]*(int)emaxs[2];
		dist2 = p->inormal[0]*(int)emaxs[0] + p->inormal[1]*(int)emins[1] + p->inormal[2]*(int)emins[2];
		break;
	case 2:
		dist1 = p->inormal[0]*(int)emaxs[0] + p->inormal[1]*(int)emins[1] + p->inormal[2]*(int)emaxs[2];
		dist2 = p->inormal[0]*(int)emins[0] + p->inormal[1]*(int)emaxs[1] + p->inormal[2]*(int)emins[2];
		break;
	case 3:
		dist1 = p->inormal[0]*(int)emins[0] + p->inormal[1]*(int)emins[1] + p->inormal[2]*(int)emaxs[2];
		dist2 = p->inormal[0]*(int)emaxs[0] + p->inormal[1]*(int)emaxs[1] + p->inormal[2]*(int)emins[2];
		break;
	case 4:
		dist1 = p->inormal[0]*(int)emaxs[0] + p->inormal[1]*(int)emaxs[1] + p->inormal[2]*(int)emins[2];
		dist2 = p->inormal[0]*(int)emins[0] + p->inormal[1]*(int)emins[1] + p->inormal[2]*(int)emaxs[2];
		break;
	case 5:
		dist1 = p->inormal[0]*(int)emins[0] + p->inormal[1]*(int)emaxs[1] + p->inormal[2]*(int)emins[2];
		dist2 = p->inormal[0]*(int)emaxs[0] + p->inormal[1]*(int)emins[1] + p->inormal[2]*(int)emaxs[2];
		break;
	case 6:
		dist1 = p->inormal[0]*(int)emaxs[0] + p->inormal[1]*(int)emins[1] + p->inormal[2]*(int)emins[2];
		dist2 = p->inormal[0]*(int)emins[0] + p->inormal[1]*(int)emaxs[1] + p->inormal[2]*(int)emaxs[2];
		break;
	case 7:
		dist1 = p->inormal[0]*(int)emins[0] + p->inormal[1]*(int)emins[1] + p->inormal[2]*(int)emins[2];
		dist2 = p->inormal[0]*(int)emaxs[0] + p->inormal[1]*(int)emaxs[1] + p->inormal[2]*(int)emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
		break;
	}

	sides = 0;
	if (dist1 >= p->idist)
		sides = 1;
	if (dist2 < p->idist)
		sides |= 2;

	return sides;
}

qboolean R_CullBox (short* mins, short* maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (QBoxOnPlaneSide (mins, maxs, &r_frustum[i]) == 2)
			return true;
	return false;
}


/*
===============
R_SetVrect
===============
*/
void R_SetVrect (vrect_t *pvrectin, vrect_t *pvrect, int lineadj)
{
	int		h;
	float	size;

	size = scr_viewsize.value > 100 ? 100 : scr_viewsize.value;
	if (cl.intermission)
	{
		size = 100;
		lineadj = 0;
	}
	size /= 100;

	h = pvrectin->height - lineadj;
	pvrect->width = pvrectin->width * size;
	if (pvrect->width < 96)
	{
		size = 96.0 / pvrectin->width;
		pvrect->width = 96;	// min for icons
	}
	pvrect->width &= ~7;
	pvrect->height = pvrectin->height * size;
	if (pvrect->height > pvrectin->height - lineadj)
		pvrect->height = pvrectin->height - lineadj;

	pvrect->height &= ~1;

	pvrect->x = (pvrectin->width - pvrect->width)/2;
	pvrect->y = (h - pvrect->height)/2;

	{
		if (lcd_x.value)
		{
			pvrect->y >>= 1;
			pvrect->height >>= 1;
		}
	}
}

void MYgluPerspective( float fovy, float aspect,
		     float zNear, float zFar )
{
   float xmin, xmax, ymin, ymax;

   ymax = zNear * tan( fovy * M_PI / 360.0 );
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;
#ifdef NDS
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
   glFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
   //glFrustum( xmin, 0, ymin, ymax, zNear, zFar );
#endif
}

/*
===============
R_ViewChanged

Called every time the vid structure or r_refdef changes.
Guaranteed to be called before the first refresh
===============
*/
void R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect)
{
	float	pixelAspect;
	float	screenAspect;
	float	verticalFieldOfView;
	float	xOrigin, yOrigin;
	int		i, j;
	int		x, x2, y2, y, w, h, off;

	//R_SetVrect (pvrect, &r_refdef.vrect, lineadj);

	MYgluPerspective (r_refdef.fov_y,  aspect,  0.005,  40.0);

	x = r_refdef.vrect.x;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) - 1;
	y = (vid.height-r_refdef.vrect.y);
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height) -1);

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < (int)vid.width)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < (int)vid.height)
		y++;

	w = x2 - x -1;
	h = y - y2 - 1;

	i = 0;

	off = vid.height - r_refdef.vrect.height;
#ifdef NDS
#if 0
	glViewport(r_refdef.vrect.x,
		r_refdef.vrect.y + off,
		r_refdef.vrect.x + r_refdef.vrect.width - 1,
		r_refdef.vrect.y + off + r_refdef.vrect.height - 1);
#else
	glViewport(0,
		0,
		vid.width-1,
		vid.height-1);
#endif
	//glViewport (x, y2, w, h);
	//glViewport(r_refdef.vrect.x,
	//	(vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)),
	//	r_refdef.vrect.width-1,
	//	r_refdef.vrect.height-1);
#endif
	r_refdef.horizontalFieldOfView = 2.0 * tan (r_refdef.fov_x/360*M_PI);
// values for perspective projection
// if math were exact, the values would range from 0.5 to to range+0.5
// hopefully they wll be in the 0.000001 to range+.999999 and truncate
// the polygon rasterization will never render in the first row or column
// but will definately render in the [range] row and column, so adjust the
// buffer origin to get an exact edge to edge fill
#define XCENTERING	(1.0 / 2.0)
#define YCENTERING	(1.0 / 2.0)
	xcenter = ((float)r_refdef.vrect.width * XCENTERING) +
			r_refdef.vrect.x - 0.5;
	//aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float)r_refdef.vrect.height * YCENTERING) +
			r_refdef.vrect.y - 0.5;
	//aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	//aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * aspect;
	//aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width-6)/r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink*aspect;

	/*r_refdef.aliasvrect.x = (int)(r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int)(r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int)(r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int)(r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
			r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
			r_refdef.aliasvrect.height;*/

#if 0
	int		i;
	float	res_scale;

	r_viewchanged = true;

	R_SetVrect (pvrect, &r_refdef.vrect, lineadj);

	r_refdef.horizontalFieldOfView = 2.0 * tan (r_refdef.fov_x/360*M_PI);
	r_refdef.fvrectx = (float)r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float)r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x<<20) + (1<<19) - 1;
	r_refdef.fvrecty = (float)r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float)r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 = (r_refdef.vrectright<<20) + (1<<19) - 1;
	r_refdef.fvrectright = (float)r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float)r_refdef.vrectright - 0.5;
	r_refdef.vrectrightedge = (float)r_refdef.vrectright - 0.99;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float)r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float)r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int)(r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int)(r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int)(r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int)(r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
			r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
			r_refdef.aliasvrect.height;

	pixelAspect = aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;
	
	screenAspect = r_refdef.vrect.width*pixelAspect /
			r_refdef.vrect.height;
// 320*200 1.0 pixelAspect = 1.6 screenAspect
// 320*240 1.0 pixelAspect = 1.3333 screenAspect
// proper 320*200 pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

// values for perspective projection
// if math were exact, the values would range from 0.5 to to range+0.5
// hopefully they wll be in the 0.000001 to range+.999999 and truncate
// the polygon rasterization will never render in the first row or column
// but will definately render in the [range] row and column, so adjust the
// buffer origin to get an exact edge to edge fill
	xcenter = ((float)r_refdef.vrect.width * XCENTERING) +
			r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float)r_refdef.vrect.height * YCENTERING) +
			r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width-6)/r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink*pixelAspect;

// left side clip
	screenedge[0].normal[0] = -1.0 / (xOrigin*r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;
	
// right side clip
	screenedge[1].normal[0] =
			1.0 / ((1.0-xOrigin)*r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;
	
// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (yOrigin*verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;
	
// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0-yOrigin)*verticalFieldOfView);
	screenedge[3].normal[2] = 1;	
	screenedge[3].type = PLANE_ANYZ;
	
	for (i=0 ; i<4 ; i++)
		VectorNormalize (screenedge[i].normal);

	res_scale = sqrt ((double)(r_refdef.vrect.width * r_refdef.vrect.height) /
			          (320.0 * 152.0)) *
			(2.0 / r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase.value * res_scale;
	r_resfudge = r_aliastransadj.value * res_scale;

	if (scr_fov.value <= 90.0)
		r_fov_greater_than_90 = false;
	else
		r_fov_greater_than_90 = true;

// TODO: collect 386-specific code in one place
#if	id386
	if (r_pixbytes == 1)
	{
		Sys_MakeCodeWriteable ((long)R_Surf8Start,
						     (long)R_Surf8End - (long)R_Surf8Start);
		colormap = vid.colormap;
		R_Surf8Patch ();
	}
	else
	{
		Sys_MakeCodeWriteable ((long)R_Surf16Start,
						     (long)R_Surf16End - (long)R_Surf16Start);
		colormap = vid.colormap16;
		R_Surf16Patch ();
	}
#endif	// id386

	D_ViewChanged ();
#endif
}
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;

	if (r_oldviewleaf == r_viewleaf)
		return;
	
	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
		
	for (i=0 ; i<r_currentbmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&r_currentbmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

texture_t *R_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (r_currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

void EmitPoly (msurface_t *fa,int pts[][5])
{
	int i, lnumverts;

	lnumverts = fa->numedges;

	
	//if(r_draw.value)
	//{
#ifdef NDS
		glBegin(GL_TRIANGLES);
#endif
		for(i=2;i<lnumverts;i++)
		{
#ifdef NDS
			DS_TEXCOORD2T16(pts[0][3],pts[0][4]);
			DS_VERTEX3V16(pts[0][0],pts[0][1],pts[0][2]);

			DS_TEXCOORD2T16(pts[i-1][3],pts[i-1][4]);
			DS_VERTEX3V16(pts[i-1][0],pts[i-1][1],pts[i-1][2]);

			DS_TEXCOORD2T16(pts[i][3],pts[i][4]);
			DS_VERTEX3V16(pts[i][0],pts[i][1],pts[i][2]);

#endif		
		}
	//}
}

// speed up sin calculations - Ed
float	turbsin[] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))

float speedscale;

void EmitTurbPoly (msurface_t *fa)
{
	int			i,texnum,os,ot,ss,tt;
	float		*vec;
	medge_t		*pedges, *pedge;
	texture_t *t;
	short	*v2;
	int n, lindex, lnumverts;
	int *u,*v;
	int	soff,toff;
	int pts[64][5];
	
	t = R_TextureAnimation (fa->texinfo->texture);
	texnum = ds_load_bsp_texture(r_currentmodel,t);
#ifdef NDS
	GFX_TEX_FORMAT = texnum;
#endif

	pedges = r_currentbmodel->edges;
	lnumverts = fa->numedges;
	n = fa->firstedge;

	if(lnumverts >= 64)
		return;

	u = fa->texinfo->ivecs[0];
	v = fa->texinfo->ivecs[1];
	soff = fa->texnorm[0];
	toff = fa->texnorm[1];
	soff <<= 4;
	toff <<= 4;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = r_currentbmodel->surfedges[n++];
		if (lindex > 0)
		{
			pedge = &pedges[lindex];
			v2 = r_pcurrentvertbase[pedge->v[0]].position;
		}
		else
		{
			pedge = &pedges[-lindex];
			v2 = r_pcurrentvertbase[pedge->v[1]].position;
		}
		pts[i][0] = v2[0];
		pts[i][1] = v2[1];
		pts[i][2] = v2[2];
		os = CALC_COORD(v2,u) - soff;
		ot = CALC_COORD(v2,v) - toff;

			ss = os + turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255]*(1<<4);

			tt = ot + turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255]*(1<<4);

		pts[i][3] = ss;
		pts[i][4] = tt;
	}
#ifdef NDS
	glColor3b(232,232,232);
#endif
	EmitPoly(fa,pts);

}



/*
=============
EmitSkyPolys
=============
*/


void EmitSkyPolys (msurface_t *fa,int pts[][5])
{
	medge_t		*pedges, *pedge;
	short	*v0,*v1,*v2;
	int i, n, lindex, lnumverts;
	int	s0, t0, s1, t1, s2, t2;
	int dir[3];
#ifdef NDS
long long length;
#else
__int64 length;
#endif

	pedges = r_currentbmodel->edges;
	lnumverts = fa->numedges;
	n = fa->firstedge;

	for(i=0;i<lnumverts;i++)
	{
		dir[0] = pts[i][0] - (r_origini[0]>>14);
		dir[1] = pts[i][1] - (r_origini[1]>>14);
		dir[2] = pts[i][2] - (r_origini[2]>>14);
		dir[2] *= 3;	// flatten the sphere

		length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
#ifdef NDS
		length = sqrt32(length>>2);
#else
		length = sqrt (length>>2);
#endif
		if(length == 0)
			length = 1;

		dir[0] = (dir[0]*6*31)/length;
		dir[1] = (dir[1]*6*31)/length;

		pts[i][3] = (speedscale + dir[0]);
		pts[i][4] = (speedscale + dir[1]);
		pts[i][3] <<= 4;
		pts[i][4] <<= 4;
	}
	
	//if(r_draw.value)
	//{
#ifdef NDS
		glBegin(GL_TRIANGLES);
#endif
		for(i=2;i<lnumverts;i++)
		{
#ifdef NDS
			DS_TEXCOORD2T16(pts[0][3],pts[0][4]);
			DS_VERTEX3V16(pts[0][0],pts[0][1],pts[0][2]);

			DS_TEXCOORD2T16(pts[i-1][3],pts[i-1][4]);
			DS_VERTEX3V16(pts[i-1][0],pts[i-1][1],pts[i-1][2]);

			DS_TEXCOORD2T16(pts[i][3],pts[i][4]);
			DS_VERTEX3V16(pts[i][0],pts[i][1],pts[i][2]);

#endif		
		}
	//}
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitBothSkyLayers (msurface_t *fa)
{
	int			i;
	float		*vec;
	medge_t		*pedges, *pedge;
	short	*v2;
	int n, lindex, lnumverts;
	int pts[64][5];
#ifdef NDS
extern uint32 ds_alpha_pal;
extern uint32 ds_texture_pal;
#endif
	
	ds_load_bsp_sky(r_currentmodel,r_sky_texture);

	pedges = r_currentbmodel->edges;
	lnumverts = fa->numedges;
	n = fa->firstedge;

	if(lnumverts >= 64)
		return;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = r_currentbmodel->surfedges[n++];
		if (lindex > 0)
		{
			pedge = &pedges[lindex];
			v2 = r_pcurrentvertbase[pedge->v[0]].position;
		}
		else
		{
			pedge = &pedges[-lindex];
			v2 = r_pcurrentvertbase[pedge->v[1]].position;
		}
		pts[i][0] = v2[0];
		pts[i][1] = v2[1];
		pts[i][2] = v2[2];
		//pts[i][3] = CALC_COORD(v2,u) - soff;
		//pts[i][4] = CALC_COORD(v2,v) - toff;
	}

	
	speedscale = realtime*8;
	speedscale -= (int)speedscale & ~63 ;

#ifdef NDS
	glColor3b(232,232,232);
	GFX_TEX_FORMAT = r_sky_bottom;
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_FRONT | POLY_ID(1));
#endif
	EmitSkyPolys (fa,pts);

	speedscale = realtime*16;
	speedscale -= (int)speedscale & ~63 ;

#ifdef NDS
	//glEnable(GL_BLEND);
	//glColorTable(GL_RGB32_A3,ds_alpha_pal);
	GFX_TEX_FORMAT = r_sky_top;
	//glPolyFmt(POLY_ALPHA(16) | POLY_CULL_FRONT | POLY_ID(4) | (1<<14) | (0<<11) | POLY_MODULATION);
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_FRONT | POLY_ID(1) | (1<<14) | (1<<11));
#endif
	EmitSkyPolys (fa,pts);
#ifdef NDS
	//glDisable(GL_BLEND);
	//glColorTable(GL_RGB256,ds_texture_pal);
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_FRONT | POLY_ID(1));
#endif

}


void R_RenderSurface(msurface_t *fa)
{
	float rad,dist,minlight;
	int i, n, lindex, lnumverts,x,y,w,h,scale,size, maps,sd,td,dst;
	unsigned colr;
	texture_t	*t;
	medge_t		*pedges, *pedge;
	int *u,*v;
	int	soff,toff,ss,tt,xs,ys;
	int texnum;
	int lnum,numdynamic;
	byte		*lightmap;
	short		*v2;
	int dynamic[3];
	int dynamic2[4][4];
	int			pts[64][6];


#ifdef NDS2
	glEnable(GL_BLEND);
	//glColorTable(GL_RGB256,ds_texture_pal);
	glPolyFmt(POLY_ALPHA(0) | POLY_CULL_FRONT | POLY_ID(1));
#endif
	pedges = r_currentbmodel->edges;
	lnumverts = fa->numedges;
	n = fa->firstedge;

	if(lnumverts >= 64)
		return;

	if (fa->flags & SURF_DRAWSKY)
	{
		EmitBothSkyLayers(fa);
		return;
	}
	else if(fa->flags & SURF_DRAWTURB)
	{
		EmitTurbPoly(fa);
		return;
		//t = R_TextureAnimation (fa->texinfo->texture);
		//texnum = ds_load_bsp_texture(r_currentmodel,t);
	}
	else
	{
		t = R_TextureAnimation (fa->texinfo->texture);
		//DS_CacheSurface(fa,t);
		texnum = ds_load_bsp_texture(r_currentmodel,t);
	}
	r_surf_draw++;
#ifdef NDS
	GFX_TEX_FORMAT = texnum;
#endif

	u = fa->texinfo->ivecs[0];
	v = fa->texinfo->ivecs[1];
	soff = fa->texnorm[0];
	toff = fa->texnorm[1];
	soff <<= 4;
	toff <<= 4;
	ss = fa->texturemins[0];
	tt = fa->texturemins[1];
	ss <<= 4;
	tt <<= 4;

	xs = (fa->texinfo->texture->width<<16)/fa->texinfo->texture->ds.width;
	ys = (fa->texinfo->texture->height<<16)/fa->texinfo->texture->ds.height;

	w = (fa->extents[0]>>4)+1;
	h = (fa->extents[1]>>4)+1;
	size = w*h;

	scale = d_lightstylevalue[fa->styles[0]];
	numdynamic = 0;
	if (fa->dlightframe == r_framecount)
	{
		for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
		{
			if ( !(fa->dlightbits & (1<<lnum) ) )
				continue;		// not lit by this light

			if(numdynamic == 4)
				break;
			rad = cl_dlights[lnum].radius;
			dist = DotProduct (cl_dlights[lnum].origin, fa->plane->normal) -
					fa->plane->dist;
			rad -= fabs(dist);
			minlight = cl_dlights[lnum].minlight;
			if (rad < minlight)
				continue;
			minlight = rad - minlight;

			for (i=0 ; i<3 ; i++)
			{
				dynamic[i] = (cl_dlights[lnum].origin[i] -
						fa->plane->normal[i]*dist) * (1<<2);
			}

			x = CALC_COORD (dynamic, u);
			y = CALC_COORD (dynamic, v);

			x = (x-ss)>>8;
			y = (y-tt)>>8;
			x = (x * xs)>>16;
			y = (y * ys)>>16;
#define RAD 2
#define MINLIGHT 3
			dynamic2[numdynamic][0] = x;
			dynamic2[numdynamic][1] = y;
			dynamic2[numdynamic][RAD] = rad;
			dynamic2[numdynamic][MINLIGHT] = minlight;
			numdynamic++;
		}
	}
	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = r_currentbmodel->surfedges[n++];
		if (lindex > 0)
		{
			pedge = &pedges[lindex];
			v2 = r_pcurrentvertbase[pedge->v[0]].position;
		}
		else
		{
			pedge = &pedges[-lindex];
			v2 = r_pcurrentvertbase[pedge->v[1]].position;
		}
		pts[i][0] = v2[0];
		pts[i][1] = v2[1];
		pts[i][2] = v2[2];
		x = CALC_COORD(v2,u);
		y = CALC_COORD(v2,v);
		pts[i][3] = x - soff;
		pts[i][4] = y - toff;
		colr=r_refdef.ambientlight<<8;
		x = (x-ss)>>4;
		y = (y-tt)>>4;
		x = (x * xs)>>20;
		y = (y * ys)>>20;
		lightmap = fa->samples;
		if(lightmap)
		{
			for(maps=0;maps<MAXLIGHTMAPS && fa->styles[maps] != 255;maps++)
			{
				scale = d_lightstylevalue[fa->styles[maps]];	// 8.8 fraction		
				colr += (*(lightmap + (w*y) + x)) * scale;
				lightmap += size;
			}
		}
		for(lnum=0;lnum<numdynamic;lnum++)
		{
			//break;
			sd = x - dynamic2[lnum][0];
			td = y - dynamic2[lnum][1];
			if(sd < 0)
				sd = -sd;
			if(td < 0)
				td = -td;
			sd <<= 4;
			td <<= 4;
			if(sd > td)
			{
				dst = sd + (td>>1);
			}
			else
			{
				dst = td + (sd>>1);
			}

			if (dst < dynamic2[lnum][MINLIGHT])
			{
				dst = ((dynamic2[lnum][RAD] - dst)*256);
				colr = colr + dst;
			}
			//blocklights[t*smax + s] += (rad - dist)*256;
		}
		colr>>=8;
		if(colr > 255)
			colr = 255;
		pts[i][5] = colr;
	}

	//if(r_draw.value)
	//{
#ifdef NDS
		glBegin(GL_TRIANGLES);
#endif
		for(i=2;i<lnumverts;i++)
		{
#ifdef NDS
			glColor3b(pts[0][5],pts[0][5],pts[0][5]);
			DS_TEXCOORD2T16(pts[0][3],pts[0][4]);
			DS_VERTEX3V16(pts[0][0],pts[0][1],pts[0][2]);

			glColor3b(pts[i-1][5],pts[i-1][5],pts[i-1][5]);
			DS_TEXCOORD2T16(pts[i-1][3],pts[i-1][4]);
			DS_VERTEX3V16(pts[i-1][0],pts[i-1][1],pts[i-1][2]);

			glColor3b(pts[i][5],pts[i][5],pts[i][5]);
			DS_TEXCOORD2T16(pts[i][3],pts[i][4]);
			DS_VERTEX3V16(pts[i][0],pts[i][1],pts[i][2]);
#endif		
		}
	//}

}

#ifdef NDS
typedef long long big_int;
#define NDS_INLINE static inline
#else
typedef __int64 big_int;
#define NDS_INLINE static __forceinline
#endif

NDS_INLINE big_int imul64(big_int a,big_int b)
{
	return (a*b)>>16;
}

NDS_INLINE big_int idiv64(big_int a,big_int b)
{
	return (a<<16)/b;
}

#define IDotProduct(x,y) (imul64(x[0],y[0])+imul64(x[1],y[1])+imul64(x[2],y[2]))

void R_StoreEfrags (efrag_t **ppefrag);

void R_RecursiveWorldNode (mnode_t *node, int planeBits)
{
	int d;
	int r;
	int side;
	mplane_t	*plane;
	int c;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	do {
		if (node->visframe != r_visframecount)
			return;
			
		if (node->contents == CONTENTS_SOLID)
			return;		// solid

		if (node->contents < 0)
			break;		// leaf
			
			
			if ( planeBits & 1 ) {
				r = QBoxOnPlaneSide(node->minmaxs, node->minmaxs+3, &r_frustum[0]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~1;			// all descendants will also be in front
				}
			}

			if ( planeBits & 2 ) {
				r = QBoxOnPlaneSide(node->minmaxs, node->minmaxs+3, &r_frustum[1]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~2;			// all descendants will also be in front
				}
			}

			if ( planeBits & 4 ) {
				r = QBoxOnPlaneSide(node->minmaxs, node->minmaxs+3, &r_frustum[2]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~4;			// all descendants will also be in front
				}
			}

			if ( planeBits & 8 ) {
				r = QBoxOnPlaneSide(node->minmaxs, node->minmaxs+3, &r_frustum[3]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~8;			// all descendants will also be in front
				}
			}

		
		R_RecursiveWorldNode(node->children[0],planeBits);
		
		//tail recurse
		node = node->children[1];
	} while(1);
	
	
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				surf = *mark;
				if(surf->visframe != r_framecount)
				{
					surf->visframe = r_framecount;
					plane = surf->plane;
					if (plane->type < 3)
					{
						d = r_origini[plane->type] - plane->idist;
					}
					else
					{
						d = IDotProduct (plane->inormal, r_origini) - plane->idist;
					}

					if (((surf->flags & SURF_PLANEBACK) && (d < 0)) ||
						(!(surf->flags & SURF_PLANEBACK) && (d > 0)))
					{
						R_RenderSurface(surf);
					}
					else
					{
						r_surf_cull++;
					}
				}

				mark++;
			} while (--c);
		}

	// deal with model fragments in this leaf
		if (pleaf->efrags)
		{
			R_StoreEfrags (&pleaf->efrags);
		}

		pleaf->key = r_currentkey;
		r_currentkey++;		// all bmodels in a leaf share the same key
	}
}

void R_RenderLeaf(mleaf_t *pleaf) {
	int c;
	msurface_t	**mark,*surf;
	if(pleaf == 0)
		return;

	mark = pleaf->firstmarksurface;
	c = pleaf->nummarksurfaces;

	if (c)
	{
		do
		{
			surf = *mark;
			R_RenderSurface (surf);
			mark++;
		} while (--c);
	}
}

void R_RenderWorld(void)
{
	entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	r_currententity = &ent;
	if(r_draw.value)
	{
		R_RecursiveWorldNode (r_currentbmodel->nodes, 15);
	}
	else
	{
		Con_DPrintf("leaf: %d\n",r_viewleaf - r_currentbmodel->leafs);
		Con_DPrintf("pos: %d %d %d",r_origini[0]>>16,r_origini[1]>>16,r_origini[2]>>16);
		Con_DPrintf("ang: %d %d %d",
			(int)r_refdef.viewangles[0],
			(int)r_refdef.viewangles[1],
			(int)r_refdef.viewangles[2]);
		R_RenderLeaf(r_viewleaf);
	}
}
int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );

void R_TransformFrustum (void)
{
	int		i;
	
	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (r_vpn, r_vright, r_frustum[0].normal);
		VectorSubtract (r_vpn, r_vright, r_frustum[1].normal);

		VectorAdd (r_vpn, r_vup, r_frustum[2].normal);
		VectorSubtract (r_vpn, r_vup, r_frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( r_frustum[0].normal, r_vup, r_vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( r_frustum[1].normal, r_vup, r_vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( r_frustum[2].normal, r_vright, r_vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( r_frustum[3].normal, r_vright, r_vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		VectorNormalize(r_frustum[i].normal);

		r_frustum[i].type = PLANE_ANYZ;
		r_frustum[i].dist = DotProduct (r_origin, r_frustum[i].normal);
		r_frustum[i].signbits = SignbitsForPlane (&r_frustum[i]);

		r_frustum[i].inormal[0] = (int)(r_frustum[i].normal[0]*(1<<16));
		r_frustum[i].inormal[1] = (int)(r_frustum[i].normal[1]*(1<<16));
		r_frustum[i].inormal[2] = (int)(r_frustum[i].normal[2]*(1<<16));
		r_frustum[i].idist = (int)(r_frustum[i].dist*(1<<16));
	}
}

void R_AnimateLight (void);
extern cvar_t	r_ambient;

void R_SetupFrame (void)
{
#if 0
	int num = floattofp16(35656.123567);
	int a = sqrtfp16(num);
	float f = fp16tofloat(a);
	float ff = sqrt(35656.123567);
	char buf[64];

	sprintf(buf,"%5.9f\n",f);
	Con_Printf("sqrt: % d %d %x\n",num,a,a);
	
	sprintf(buf,"%5.9f\n",f);
	Con_Printf("fp16: %s\n",buf);
	sprintf(buf,"%5.9f\n",ff);
	Con_Printf("fp:   %s\n",buf);
	while(1);
#endif
// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		Cvar_Set ("r_ambient", "0");
	}
	r_refdef.ambientlight = r_ambient.value;

	if (r_refdef.ambientlight < 0)
		r_refdef.ambientlight = 0;

	r_surf_tri = r_alias_tri = r_surf_draw = r_surf_cull = 0;
	r_currentkey = 0;
	r_framecount++;
	r_oldviewleaf = r_viewleaf;
	r_currentmodel = (model_t*)cl.worldmodel;
	r_currentbmodel = (bmodel_t*)cl.worldmodel->cache.data;
	r_pcurrentvertbase = r_currentbmodel->vertexes;
	r_viewleaf = Mod_PointInLeaf (r_refdef.vieworg, cl.worldmodel);
	AngleVectors (r_refdef.viewangles, r_vpn, r_vright, r_vup);
	VectorCopy(r_refdef.vieworg,r_origin);
	r_origini[0] = r_origin[0] * (1<<16);
	r_origini[1] = r_origin[1] * (1<<16);
	r_origini[2] = r_origin[2] * (1<<16);
	R_TransformFrustum();
	//glClearDepth(0x7FFF);
	R_AnimateLight ();
	

#ifdef NDS
	glColor3b(255,255,255);

	//MYgluPerspective (r_refdef.fov_y,  vid.aspect,  0.005,  40.0*0.3);
	// Set the current matrix to be the model matrix
	glMatrixMode(GL_MODELVIEW);
	
#if 1
	//Push our original Matrix onto the stack (save state)
	//glPushMatrix();
	r_modelview[0] = (int)(r_vright[0]*(1<<12));
	r_modelview[1] = (int)(r_vup[0]*(1<<12));
	r_modelview[2] = (int)(-r_vpn[0]*(1<<12));
	r_modelview[3] = 0;

	r_modelview[4] = (int)(r_vright[1]*(1<<12));
	r_modelview[5] = (int)(r_vup[1]*(1<<12));
	r_modelview[6] = (int)(-r_vpn[1]*(1<<12));
	r_modelview[7] = 0;

	r_modelview[8] = (int)(r_vright[2]*(1<<12));
	r_modelview[9] = (int)(r_vup[2]*(1<<12));
	r_modelview[10] = (int)(-r_vpn[2]*(1<<12));
	r_modelview[11] = 0;

	r_modelview[12] = -(int)(DotProduct(r_origin,r_vright)*4.0);
	r_modelview[13] = -(int)(DotProduct(r_origin,r_vup)*4.0);
	r_modelview[14] = (int)(DotProduct(r_origin,r_vpn)*4.0);
	r_modelview[15] = (1<<12);

	for(int i=0;i<16;i++)
	{
		MATRIX_LOAD4x4 = r_modelview[i];
	}
#else
	glLoadIdentity();
	glRotateX(-90);
	glRotateZ(90);//r_angle[2]);
	glRotateX(-r_refdef.viewangles[2]);//r_angle[1]);
	glRotateY(-r_refdef.viewangles[0]);//r_angle[0]);
	glRotateZ(-r_refdef.viewangles[1]);//r_angle[2]);
#define floattodsv16(n)       ((int)((n) * (1 << 2)))
	glTranslate3f32(-floattodsv16(r_refdef.vieworg[0]),-floattodsv16(r_refdef.vieworg[1]),-floattodsv16(r_refdef.vieworg[2]));
#endif
#endif

}

void R_EndFrame (void)
{

#ifdef NDS
/*int p,v;
	glGetInt(GL_GET_POLYGON_RAM_COUNT,&p);
	glGetInt(GL_GET_VERTEX_RAM_COUNT,&v);
	
	Con_DPrintf("P: %d\n",p);
	Con_DPrintf("V: %d\n",v);*/
extern cvar_t		ds_flush;
		// Pop our Matrix from the stack (restore state)
		//glPopMatrix(1);
		glFlush(ds_flush.value);
#endif
		//Con_DPrintf("draw: %3d cull: %3d all: %3d\n",r_surf_draw,r_surf_cull,r_surf_draw+r_surf_cull);
		//Con_DPrintf("surf: %4d alias: %4d all: %4d\n",r_surf_tri,r_alias_tri,r_surf_tri+r_alias_tri);
}

void R_MarkLights (dlight_t *light, int bit, mnode_t *node);
void R_DrawBrushModel (entity_t *e)
{
	int			j, k;
	short		mins[3], maxs[3];
	int			i, numsurfaces;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	qboolean	rotated;


	r_pcurrentvertbase = r_currentbmodel->vertexes;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = (short)(e->origin[i] - r_currentmodel->radius);
			maxs[i] = (short)(e->origin[i] + r_currentmodel->radius);
		}
	}
	else
	{
		rotated = false;
		//VectorAdd (e->origin, clmodel->mins, mins);
		//VectorAdd (e->origin, clmodel->maxs, maxs);
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = (short)(e->origin[i] + r_currentmodel->mins[i]);
			maxs[i] = (short)(e->origin[i] + r_currentmodel->maxs[i]);
		}
	}

	if (R_CullBox (mins, maxs))
		return;
/*
	glColor3f (1,1,1);
	memset (lightmap_polys, 0, sizeof(lightmap_polys));

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}
*/
	psurf = &r_currentbmodel->surfaces[r_currentbmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (r_currentbmodel->firstmodelsurface != 0)
	{
		for (k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			R_MarkLights (&cl_dlights[k], 1<<k,
				r_currentbmodel->nodes + r_currentbmodel->hulls[0].firstclipnode);
		}
	}

#ifdef NDS

    glPushMatrix ();
e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug
#endif
	//
	// draw texture
	//
	for (i=0 ; i<r_currentbmodel->nummodelsurfaces ; i++, psurf++)
	{
	// find which side of the node we are on
		/*pplane = psurf->plane;

		dot = DotProduct (fmodelorg, pplane->normal) - pplane->dist;

	// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (gl_texsort.value)
				R_RenderBrushPoly (psurf);
			else
				R_DrawSequentialPoly (psurf);
		}*/
		R_RenderSurface(psurf);//F_DrawSuface(psurf);
	}

	//R_BlendLightmaps ();
#ifdef NDS
	glPopMatrix (1);
#endif
}

void R_DrawAliasModel ();

void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;
#ifdef NDS
	glColor3b(255,255,255);
#endif
#if 1
	for (i=1 ; i<MAX_MODELS ; i++)
	{
		r_currentmodel = cl.model_precache[i];
		if(r_currentmodel == 0)
			break;

		r_currententity = r_currentmodel->ents;
		while(r_currententity)
		{
			switch (r_currentmodel->type)
			{
			case mod_alias:
				R_DrawAliasModel ();
				break;

			case mod_brush:
				r_currentbmodel = (bmodel_t *)r_currentmodel->cache.data;
				R_DrawBrushModel (r_currententity);
				break;

			default:
				break;
			}
			r_currententity = r_currententity->next;
		}
		r_currentmodel->ents = 0;
	}
#else

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		r_currententity = cl_visedicts[i];
		r_currentmodel = r_currententity->model;

		switch (r_currentmodel->type)
		{
		case mod_alias:
			R_DrawAliasModel ();
			break;

		case mod_brush:
			r_currentbmodel = (bmodel_t *)r_currentmodel->cache.data;
			R_DrawBrushModel (r_currententity);
			break;

		default:
			break;
		}
	}
#endif
#if 0
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_FRONT | POLY_ID(4));
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		fcurrententity = cl_visedicts[i];

		switch (fcurrententity->model->type)
		{
		case mod_sprite:
			F_DrawSpriteModel ();
			break;
		}
	}
#endif
}

void R_DrawParticles (void);

void R_RenderView (void)
{
#ifdef WIN322
	r_refdef.vieworg[0] = 645;
	r_refdef.vieworg[1] = 1922;
	r_refdef.vieworg[2] = 46;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 76;
	r_refdef.viewangles[2] = 0;
#endif

	R_SetupFrame();
	R_MarkLeaves();
	R_RenderWorld();
	R_DrawViewModel();
	R_DrawEntitiesOnList();
	R_DrawParticles();
	R_EndFrame();
}
