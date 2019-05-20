/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

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
// r_brush.c: brush model rendering. renamed from r_surf.c

#include "quakedef.h"

extern cvar_t gl_fullbrights, r_drawflat, gl_overbright; //johnfitz
extern cvar_t gl_zfix; // QuakeSpasm z-fighting fix

int gl_lightmap_format;
int lightmap_bytes;

#define BLOCK_WIDTH 128
#define BLOCK_HEIGHT 128

gltexture_t* lightmap_textures[MAX_LIGHTMAPS]; //johnfitz -- changed to an array
gltexture_t* splatmap_textures[MAX_LIGHTMAPS];

unsigned blocklights[BLOCK_WIDTH*BLOCK_HEIGHT*3]; //johnfitz -- was 18*18, added lit support (*3) and loosened surface extents maximum (BLOCK_WIDTH*BLOCK_HEIGHT)
unsigned splatblocklights[BLOCK_WIDTH*BLOCK_HEIGHT*3];

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

glpoly_t* lightmap_polys[MAX_LIGHTMAPS];
bool lightmap_modified[MAX_LIGHTMAPS];
glRect_t lightmap_rectchange[MAX_LIGHTMAPS];

int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
int last_lightmap_allocated; //ericw -- optimization: remember the index of the last lightmap AllocBlock stored a surf in

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];
byte splatmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];

/*
===============
R_TextureAnimation -- johnfitz -- added "frame" param to eliminate use of "currententity" global

Returns the proper texture for a given time and base texture
===============
*/
texture_t* R_TextureAnimation(texture_t* base, int frame)
{
	int relative;
	int count;

	if (frame)
		if (base->alternate_anims)
			base = base->alternate_anims;

	if (!base->anim_total)
		return base;

	relative = (int)(cl.time*10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly(glpoly_t* p)
{
	float* v;
	int i;

	glBegin(GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i<p->numverts; i++, v += VERTEXSIZE)
	{
		glTexCoord2f(v[3], v[4]);
		glVertex3fv(v);
	}
	glEnd();
}

/*
================
DrawGLTriangleFan -- johnfitz -- like DrawGLPoly but for r_showtris
================
*/
void DrawGLTriangleFan(glpoly_t* p)
{
	float* v;
	int i;

	glBegin(GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i = 0; i<p->numverts; i++, v += VERTEXSIZE)
	{
		glVertex3fv(v);
	}
	glEnd();
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel(entity_t* e)
{
	int i, k;
	msurface_t* psurf;
	float dot;
	mplane_t* pplane;
	qmodel_t* clmodel;

	if (R_CullModelForEntity(e))
		return;

	currententity = e;
	clmodel = e->model;

	modelorg = Vec_Sub(r_refdef.vieworg, e->origin);
	if (e->angles.x || e->angles.y || e->angles.z)
	{
		vec3_t temp;
		vec3_t forward, right, up;

		temp = modelorg;
		Vec_AngleVectors(e->angles, &forward, &right, &up);
		modelorg.x = Vec_Dot(temp, forward);
		modelorg.y = -Vec_Dot(temp, right);
		modelorg.z = Vec_Dot(temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value)
	{
		for (k = 0; k<MAX_DLIGHTS; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			R_MarkLights(&cl_dlights[k], k,
				clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	glPushMatrix();
	e->angles.x = -e->angles.x; // stupid quake bug
	if (gl_zfix.value)
	{
		e->origin.x -= DIST_EPSILON;
		e->origin.y -= DIST_EPSILON;
		e->origin.z -= DIST_EPSILON;
	}
	R_RotateForEntity(e->origin, e->angles);
	if (gl_zfix.value)
	{
		e->origin.x += DIST_EPSILON;
		e->origin.y += DIST_EPSILON;
		e->origin.z += DIST_EPSILON;
	}
	e->angles.x = -e->angles.x; // stupid quake bug

	R_ClearTextureChains(clmodel, chain_model);
	for (i = 0; i<clmodel->nummodelsurfaces; i++, psurf++)
	{
		pplane = psurf->plane;
		dot = Vec_Dot(modelorg, pplane->normal) - pplane->dist;
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_ChainSurface(psurf, chain_model);
			rs_brushpolys++;
		}
	}

	R_DrawTextureChains(clmodel, e, chain_model);
	R_DrawTextureChains_Water(clmodel, e, chain_model);

	glPopMatrix();
}

/*
=================
R_DrawBrushModel_ShowTris -- johnfitz
=================
*/
void R_DrawBrushModel_ShowTris(entity_t* e)
{
	int i;
	msurface_t* psurf;
	float dot;
	mplane_t* pplane;
	qmodel_t* clmodel;

	if (R_CullModelForEntity(e))
		return;

	currententity = e;
	clmodel = e->model;

	modelorg = Vec_Sub(r_refdef.vieworg, e->origin);
	if (e->angles.x || e->angles.y || e->angles.z)
	{
		vec3_t temp;
		vec3_t forward, right, up;

		temp = modelorg;
		Vec_AngleVectors(e->angles, &forward, &right, &up);
		modelorg.x = Vec_Dot(temp, forward);
		modelorg.y = -Vec_Dot(temp, right);
		modelorg.z = Vec_Dot(temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	glPushMatrix();
	e->angles.x = -e->angles.x; // stupid quake bug
	R_RotateForEntity(e->origin, e->angles);
	e->angles.x = -e->angles.x; // stupid quake bug

	//
	// draw it
	//
	for (i = 0; i<clmodel->nummodelsurfaces; i++, psurf++)
	{
		pplane = psurf->plane;
		dot = Vec_Dot(modelorg, pplane->normal) - pplane->dist;
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			DrawGLTriangleFan(psurf->polys);
		}
	}

	glPopMatrix();
}

/*
=============================================================

	LIGHTMAPS

=============================================================
*/

/*
================
R_RenderDynamicLightmaps
called during rendering
================
*/
void R_RenderDynamicLightmaps(msurface_t* fa)
{
	byte* base;
	int maps;
	glRect_t* theRect;
	int smax, tmax;

	if (fa->flags & SURF_DRAWTILED) //johnfitz -- not a lightmapped surface
		return;

	// add to lightmap chain
	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount // dynamic this frame
		|| fa->cached_dlight) // dynamic previously
	{
	dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = (fa->extents[0]>>4)+1;
			tmax = (fa->extents[1]>>4)+1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t-theRect->t)+tmax;

			base = lightmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;

			byte* splatbase = splatmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			splatbase += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;

			R_BuildLightMap(fa, base, splatbase, BLOCK_WIDTH*lightmap_bytes);
		}
	}
}

/*
========================
AllocBlock -- returns a texture number and the position inside it
========================
*/
int AllocBlock(int w, int h, int* x, int* y)
{
	int i, j;
	int best, best2;
	int texnum;

	// ericw -- rather than searching starting at lightmap 0 every time,
	// start at the last lightmap we allocated a surface in.
	// This makes AllocBlock much faster on large levels (can shave off 3+ seconds
	// of load time on a level with 180 lightmaps), at a cost of not quite packing
	// lightmaps as tightly vs. not doing this (uses ~5% more lightmaps)
	for (texnum = last_lightmap_allocated; texnum<MAX_LIGHTMAPS; texnum++, last_lightmap_allocated++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i<BLOCK_WIDTH-w; i++)
		{
			best2 = 0;

			for (j = 0; j<w; j++)
			{
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
			}
			if (j == w)
			{ // this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i<w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error("AllocBlock: full");
	return 0; //johnfitz -- shut up compiler
}


mvertex_t* r_pcurrentvertbase;
qmodel_t* currentmodel;

int nColinElim;

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap(msurface_t* surf)
{
	int smax, tmax;
	byte* base;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	byte* splatbase = splatmaps + surf->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
	memset(splatbase, 255, lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT);
	splatbase += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	R_BuildLightMap(surf, base, splatbase, BLOCK_WIDTH*lightmap_bytes);
}

/*
================
BuildSurfaceDisplayList -- called at level load time
================
*/
void BuildSurfaceDisplayList(msurface_t* fa)
{
	int i, lindex, lnumverts;
	medge_t* pedges, *r_pedge;
	vec3_t vec;
	float s, t;
	glpoly_t* poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	//
	// draw texture
	//
	poly = (glpoly_t *)Hunk_Alloc(sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i<lnumverts; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = Vec_Dot(vec, *(vec3_t*)fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = Vec_Dot(vec, *(vec3_t*)fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		*(vec3_t*)poly->verts[i] = vec;
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = Vec_Dot(vec, *(vec3_t*)fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s*16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = Vec_Dot(vec, *(vec3_t*)fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t*16;
		t += 8;
		t /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	//johnfitz -- removed gl_keeptjunctions code

	poly->numverts = lnumverts;
}

/*
==================
GL_BuildLightmaps -- called at level load time

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps(void)
{
	char name[16];
	byte* data;
	int i, j;
	qmodel_t* m;

	memset(allocated, 0, sizeof(allocated));
	last_lightmap_allocated = 0;

	r_framecount = 1; // no dlightcache

	//johnfitz -- null out array (the gltexture objects themselves were already freed by Mod_ClearAll)
	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		lightmap_textures[i] = NULL;
		splatmap_textures[i] = NULL;
	}
	//johnfitz

	gl_lightmap_format = GL_RGBA;//FIXME: hardcoded for now!

	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		lightmap_bytes = 4;
		break;
	case GL_BGRA:
		lightmap_bytes = 4;
		break;
	default:
		Sys_Error("GL_BuildLightmaps: bad lightmap format");
	}

	for (j = 1; j<MAX_MODELS; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i<m->numsurfaces; i++)
		{
			//johnfitz -- rewritten to use SURF_DRAWTILED instead of the sky/water flags
			if (m->surfaces[i].flags & SURF_DRAWTILED)
				continue;
			GL_CreateSurfaceLightmap(m->surfaces + i);
			BuildSurfaceDisplayList(m->surfaces + i);
			//johnfitz
		}
	}

	//
	// upload all lightmaps that were filled
	//
	for (i = 0; i<MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break; // no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;

		//johnfitz -- use texture manager
		sprintf(name, "lightmap%03i", i);
		data = lightmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes;
		lightmap_textures[i] = TexMgr_LoadImage(cl.worldmodel, name, BLOCK_WIDTH, BLOCK_HEIGHT,
			SRC_LIGHTMAP, data, "", (src_offset_t)data, TEXPREF_LINEAR | TEXPREF_NOPICMIP);
		//johnfitz

		sprintf(name, "splatmap%03i", i);
		data = splatmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes;
		splatmap_textures[i] = TexMgr_LoadImage(cl.worldmodel, name, BLOCK_WIDTH, BLOCK_HEIGHT,
			SRC_LIGHTMAP, data, "", (src_offset_t)data, TEXPREF_LINEAR | TEXPREF_NOPICMIP);
	}

	//johnfitz -- warn about exceeding old limits
	if (i >= 64)
		Con_DWarning("%i lightmaps exceeds standard limit of 64 (max = %d).\n", i, MAX_LIGHTMAPS);
	//johnfitz
}

/*
=============================================================

	VBO support

=============================================================
*/

GLuint gl_bmodel_vbo = 0;

void GL_DeleteBModelVertexBuffer(void)
{
	if (!(gl_vbo_able && gl_mtexable && gl_max_texture_units >= 3))
		return;

	GL_DeleteBuffersFunc(1, &gl_bmodel_vbo);
	gl_bmodel_vbo = 0;

	GL_ClearBufferBindings();
}

/*
==================
GL_BuildBModelVertexBuffer

Deletes gl_bmodel_vbo if it already exists, then rebuilds it with all
surfaces from world + all brush models
==================
*/
void GL_BuildBModelVertexBuffer(void)
{
	unsigned int numverts, varray_bytes, varray_index;
	int i, j;
	qmodel_t* m;
	float* varray;

	if (!(gl_vbo_able && gl_mtexable && gl_max_texture_units >= 3))
		return;

	// ask GL for a name for our VBO
	GL_DeleteBuffersFunc(1, &gl_bmodel_vbo);
	GL_GenBuffersFunc(1, &gl_bmodel_vbo);

	// count all verts in all models
	numverts = 0;
	for (j = 1; j<MAX_MODELS; j++)
	{
		m = cl.model_precache[j];
		if (!m || m->name[0] == '*' || m->type != mod_brush)
			continue;

		for (i = 0; i<m->numsurfaces; i++)
		{
			numverts += m->surfaces[i].numedges;
		}
	}

	// build vertex array
	varray_bytes = VERTEXSIZE * sizeof(float) * numverts;
	varray = (float *)malloc(varray_bytes);
	varray_index = 0;

	for (j = 1; j<MAX_MODELS; j++)
	{
		m = cl.model_precache[j];
		if (!m || m->name[0] == '*' || m->type != mod_brush)
			continue;

		for (i = 0; i<m->numsurfaces; i++)
		{
			msurface_t* s = &m->surfaces[i];
			s->vbo_firstvert = varray_index;
			memcpy(&varray[VERTEXSIZE * varray_index], s->polys->verts, VERTEXSIZE * sizeof(float) * s->numedges);
			varray_index += s->numedges;
		}
	}

	// upload to GPU
	GL_BindBufferFunc(GL_ARRAY_BUFFER, gl_bmodel_vbo);
	GL_BufferDataFunc(GL_ARRAY_BUFFER, varray_bytes, varray, GL_STATIC_DRAW);
	free(varray);

	// invalidate the cached bindings
	GL_ClearBufferBindings();
}

void LightFunction(msurface_t* surf, int lnum, int smax, int tmax, unsigned* bl)
{
	float cred, cgreen, cblue, brightness;
	int sd, td;
	float dist, rad, minlight;
	vec3_t impact, local;

	dlight_t* light = &cl_dlights[lnum];

	rad = light->radius;
	dist = Vec_Dot(light->origin, surf->plane->normal) - surf->plane->dist;
	rad -= fabs(dist);
	minlight = light->minlight;
	if (rad < minlight)
		return;
	minlight = rad - minlight;

	impact = Vec_MulAdd(light->origin, dist, surf->plane->normal);

	local.x = Vec_Dot(impact, *(vec3_t*)surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
	local.y = Vec_Dot(impact, *(vec3_t*)surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];

	local.x -= surf->texturemins[0];
	local.y -= surf->texturemins[1];

	//johnfitz -- lit support via lordhavoc
	cred = light->color.x * 256.0f;
	cgreen = light->color.y * 256.0f;
	cblue = light->color.z * 256.0f;
	//johnfitz

	vec3_t lightcolor = Vec(cred*1, cgreen*1, cblue*1);
	vec3_t outercolor = Vec_Scale(lightcolor, 0.5);

	float inverseminlight = 1.0/minlight;
	
	if (light->issplat)
	{
		float opacity = light->splatopacity;
		for (int t = 0; t<tmax; t++)
		{
			td = local.y - t*16;
			if (td < 0)
				td = -td;
			for (int s = 0; s<smax; s++)
			{
				sd = local.x - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
					//johnfitz -- lit support via lordhavoc
				{
					brightness = (rad - dist)*inverseminlight;
					vec3_t color = Vec_Lerp(outercolor, lightcolor, brightness);
					color = Vec_Lerp(Vec(bl[0], bl[1], bl[2]), color, brightness*opacity);
					bl[0] = (int)(color.x);
					bl[1] = (int)(color.y);
					bl[2] = (int)(color.z);
				}
				bl += 3;
				//johnfitz
			}
		}
	}
	else
	{
		for (int t = 0; t<tmax; t++)
		{
			td = local.y - t*16;
			if (td < 0)
				td = -td;
			for (int s = 0; s<smax; s++)
			{
				sd = local.x - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
					//johnfitz -- lit support via lordhavoc
				{
					brightness = rad - dist;
					bl[0] += (int)(brightness * cred);
					bl[1] += (int)(brightness * cgreen);
					bl[2] += (int)(brightness * cblue);
				}
				bl += 3;
				//johnfitz
			}
		}
	}
}

void SplatFunction()
{

}

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights(msurface_t* surf)
{
	int lnum;
	
	int smax, tmax;
	//johnfitz -- lit support via lordhavoc
	
	
	//johnfitz

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	for (lnum = 0; lnum<MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits[lnum >> 5] & (1U << (lnum & 31))))
			continue; // not lit by this light

		if (!cl_dlights[lnum].issplat)
		{
			if (!surf->culled)
			{
				LightFunction(surf, lnum, smax, tmax, blocklights);
			}
		}
		else
		{
			LightFunction(surf, lnum, smax, tmax, splatblocklights);
		}
	}
}


void FillLightMapData(byte* dest, int smax, int tmax, int stride)
{
	int r, g, b;
	unsigned* bl = blocklights;
	// bound, invert, and shift
	// store:
	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		stride -= smax * 4;
		for (int i = 0; i<tmax; i++, dest += stride)
		{
			for (int j = 0; j<smax; j++)
			{
				if (gl_overbright.value)
				{
					r = *bl++ >> 9;
					g = *bl++ >> 9;
					b = *bl++ >> 9;
				}
				else
				{
					r = *bl++ >> 7;
					g = *bl++ >> 7;
					b = *bl++ >> 7;
				}
				*dest++ = (r > 255) ? 255 : r;
				*dest++ = (g > 255) ? 255 : g;
				*dest++ = (b > 255) ? 255 : b;
				*dest++ = 255;
			}
		}
		break;
	case GL_BGRA:
		stride -= smax * 4;
		for (int i = 0; i<tmax; i++, dest += stride)
		{
			for (int j = 0; j<smax; j++)
			{
				if (gl_overbright.value)
				{
					r = *bl++ >> 9;
					g = *bl++ >> 9;
					b = *bl++ >> 9;
				}
				else
				{
					r = *bl++ >> 7;
					g = *bl++ >> 7;
					b = *bl++ >> 7;
				}
				*dest++ = (b > 255) ? 255 : b;
				*dest++ = (g > 255) ? 255 : g;
				*dest++ = (r > 255) ? 255 : r;
				*dest++ = 255;
			}
		}
		break;
	default:
		Sys_Error("FillLightMapData: bad lightmap format");
	}
}

void ReverseFillSplatMapData(byte* dest, int smax, int tmax, int stride)
{
	int r, g, b;
	unsigned* bl = splatblocklights;
	// bound, invert, and shift
	// store:
	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		stride -= smax * 4;
		for (int i = 0; i<tmax; i++, dest += stride)
		{
			for (int j = 0; j<smax; j++)
			{
				r = *dest++;
				g = *dest++;
				b = *dest++;
				dest++;

				*bl++ = r;
				*bl++ = g;
				*bl++ = b;
			}
		}
		break;
	case GL_BGRA:
		stride -= smax * 4;
		for (int i = 0; i<tmax; i++, dest += stride)
		{
			for (int j = 0; j<smax; j++)
			{
				r = *dest++;
				g = *dest++;
				b = *dest++;
				dest++;

				*bl++ = (r > 255) ? 255 : r;
				*bl++ = (g > 255) ? 255 : g;
				*bl++ = (b > 255) ? 255 : b;
			}
		}
		break;
	default:
		Sys_Error("FillSplatMapData: bad lightmap format");
	}
}

void FillSplatMapData(byte* dest, int smax, int tmax, int stride)
{
	int r, g, b;
	unsigned* bl = splatblocklights;
	// bound, invert, and shift
	// store:
	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		stride -= smax * 4;
		for (int i = 0; i<tmax; i++, dest += stride)
		{
			for (int j = 0; j<smax; j++)
			{
				r = *bl++;
				g = *bl++;
				b = *bl++;

				*dest++ = (r > 255) ? 255 : r;
				*dest++ = (g > 255) ? 255 : g;
				*dest++ = (b > 255) ? 255 : b;
				*dest++ = 255;
			}
		}
		break;
	case GL_BGRA:
		stride -= smax * 4;
		for (int i = 0; i<tmax; i++, dest += stride)
		{
			for (int j = 0; j<smax; j++)
			{
				r = *bl++;
				g = *bl++;
				b = *bl++;

				*dest++ = (b > 255) ? 255 : b;
				*dest++ = (g > 255) ? 255 : g;
				*dest++ = (r > 255) ? 255 : r;
				*dest++ = 255;
			}
		}
		break;
	default:
		Sys_Error("FillSplatMapData: bad lightmap format");
	}
}
/*
===============
R_BuildLightMap -- johnfitz -- revised for lit support via lordhavoc

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap(msurface_t* surf, byte* dest, byte* splatdest, int stride)
{
	int smax, tmax;
	int size;
	byte* lightmap;
	unsigned scale;
	int maps;
	unsigned* bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);
	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (cl.worldmodel->lightdata)
	{
		// clear to no light
		memset(&blocklights[0], 0, size * 3 * sizeof(unsigned int)); //johnfitz -- lit support via lordhavoc
		//memset(&splatblocklights[0], 255, size * 3 * sizeof(unsigned int));
		// add all the lightmaps
		if (lightmap)
		{
			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				surf->cached_light[maps] = scale; // 8.8 fraction
				//johnfitz -- lit support via lordhavoc
				bl = blocklights;
				for (int i = 0; i<size; i++)
				{
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
				}
				//johnfitz
			}
		}

		ReverseFillSplatMapData(splatdest, smax, tmax, stride);

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
		{
			R_AddDynamicLights(surf);
		}
	}
	else
	{
		// set to full bright if no light data
		memset(&blocklights[0], 255, size * 3 * sizeof(unsigned int)); //johnfitz -- lit support via lordhavoc
	}

	FillLightMapData(dest, smax, tmax, stride);
	FillSplatMapData(splatdest, smax, tmax, stride);
}

/*
===============
R_UploadLightmap -- johnfitz -- uploads the modified lightmap to opengl if necessary

assumes lightmap texture is already bound
===============
*/
static void R_UploadLightmap(int lmap)
{
	glRect_t* theRect;

	if (lightmap_modified[lmap])
	{
		lightmap_modified[lmap] = false;
		theRect = &lightmap_rectchange[lmap];

		GL_Bind(lightmap_textures[lmap]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, gl_lightmap_format,
			GL_UNSIGNED_BYTE, lightmaps+(lmap* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);

		GL_Bind(splatmap_textures[lmap]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, gl_lightmap_format,
			GL_UNSIGNED_BYTE, splatmaps+(lmap* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);

		theRect->l = BLOCK_WIDTH;
		theRect->t = BLOCK_HEIGHT;
		theRect->h = 0;
		theRect->w = 0;

		rs_dynamiclightmaps++;
	}
}

void R_UploadLightmaps(void)
{
	int lmap;

	for (lmap = 0; lmap < MAX_LIGHTMAPS; lmap++)
	{
		R_UploadLightmap(lmap);
	}
}

/*
================
R_RebuildAllLightmaps -- johnfitz -- called when gl_overbright gets toggled
================
*/
void R_RebuildAllLightmaps(void)
{
	int i, j;
	qmodel_t* mod;
	msurface_t* fa;
	byte* base;

	if (!cl.worldmodel) // is this the correct test?
		return;

	//for each surface in each model, rebuild lightmap with new scale
	for (i = 1; i<MAX_MODELS; i++)
	{
		if (!(mod = cl.model_precache[i]))
			continue;
		fa = &mod->surfaces[mod->firstmodelsurface];
		for (j = 0; j<mod->nummodelsurfaces; j++, fa++)
		{
			if (fa->flags & SURF_DRAWTILED)
				continue;

			base = lightmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;

			byte* splatbase = splatmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			splatbase += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;

			R_BuildLightMap(fa, base, splatbase, BLOCK_WIDTH*lightmap_bytes);
		}
	}

	//for each lightmap, upload it
	for (i = 0; i<MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break;
		GL_Bind(lightmap_textures[i]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, BLOCK_HEIGHT, gl_lightmap_format,
			GL_UNSIGNED_BYTE, lightmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes);

		GL_Bind(splatmap_textures[i]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, BLOCK_HEIGHT, gl_lightmap_format,
			GL_UNSIGNED_BYTE, splatmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes);
	}
}
