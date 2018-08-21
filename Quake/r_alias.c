/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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

//r_alias.c -- alias model rendering

#include "quakedef.h"

extern cvar_t r_drawflat, gl_overbright_models, gl_fullbrights, r_lerpmodels, r_lerpmove; //johnfitz

//up to 16 color translated skins
gltexture_t* playertextures[MAX_SCOREBOARD]; //johnfitz -- changed to an array of pointers

#define NUMVERTEXNORMALS 162

float r_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

extern vec3_t lightcolor; //johnfitz -- replaces "float shadelight" for lit support

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
{
#include "anorm_dots.h"
};

extern vec3_t lightspot;

float* shadedots = r_avertexnormal_dots[0];
vec3_t shadevector;

float entalpha; //johnfitz

bool overbright; //johnfitz

bool shading = true; //johnfitz -- if false, disable vertex shading for various reasons (fullbright, r_lightmap, showtris, etc)

//johnfitz -- struct for passing lerp information to drawing functions
typedef struct {
	short pose1;
	short pose2;
	float blend;
	vec3_t origin;
	vec3_t angles;
} lerpdata_t;
//johnfitz

static GLuint r_alias_program;

// uniforms used in vert shader
static GLuint blendLoc;
static GLuint shadevectorLoc;
static GLuint screenquantizationLoc;
static GLuint lightColorLoc;

// uniforms used in frag shader
static GLuint texLoc;
static GLuint fullbrightTexLoc;
static GLuint useFullbrightTexLoc;
static GLuint overbrightLoc;
static GLuint useAlphaTestLoc;

#define pose1VertexAttrIndex 0
#define pose1NormalAttrIndex 1
#define pose2VertexAttrIndex 2
#define pose2NormalAttrIndex 3
#define texCoordsAttrIndex 4
#define triCenterAttrIndex 5

/*
=============
GLARB_GetXYZOffset

Returns the offset of the first vertex's meshxyz_t.xyz in the vbo for the given
model and pose.
=============
*/
static void* GLARB_GetXYZOffset(aliashdr_t* hdr, int pose)
{
	const int xyzoffs = offsetof(meshxyz_t, xyz);
	return (void *)(currententity->model->vboxyzofs + (hdr->numverts_vbo * pose * sizeof(meshxyz_t)) + xyzoffs);
}

/*
=============
GLARB_GetNormalOffset

Returns the offset of the first vertex's meshxyz_t.normal in the vbo for the
given model and pose.
=============
*/
static void* GLARB_GetNormalOffset(aliashdr_t* hdr, int pose)
{
	const int normaloffs = offsetof(meshxyz_t, normal);
	return (void *)(currententity->model->vboxyzofs + (hdr->numverts_vbo * pose * sizeof(meshxyz_t)) + normaloffs);
}

/*
=============
GLAlias_CreateShaders
=============
*/
void GLAlias_CreateShaders(void)
{
	const glsl_attrib_binding_t bindings[] = {
		{ "TexCoords", texCoordsAttrIndex },
		{ "TriCenter", triCenterAttrIndex },
		{ "Pose1Vert", pose1VertexAttrIndex },
		{ "Pose1Normal", pose1NormalAttrIndex },
		{ "Pose2Vert", pose2VertexAttrIndex },
		{ "Pose2Normal", pose2NormalAttrIndex }
	};

	const GLchar* vertSource = \
		"#version 110\n"
		"\n"
		"uniform float Blend;\n"
		"uniform vec3 ShadeVector;\n"
		"uniform vec3 ScreenQuantization;\n"
		"uniform vec4 LightColor;\n"
		"attribute vec4 TexCoords; // only xy are used \n"
		"attribute vec4 TriCenter;"
		"attribute vec4 Pose1Vert;\n"
		"attribute vec3 Pose1Normal;\n"
		"attribute vec4 Pose2Vert;\n"
		"attribute vec3 Pose2Normal;\n"
		"float r_avertexnormal_dot(vec3 vertexnormal) // from MH \n"
		"{\n"
		"        float dot = dot(vertexnormal, ShadeVector);\n"
		"        // wtf - this reproduces anorm_dots within as reasonable a degree of tolerance as the >= 0 case\n"
		"        if (dot < 0.0)\n"
		"            return 1.0 + dot * (13.0 / 44.0);\n"
		"        else\n"
		"            return 1.0 + dot;\n"
		"}\n"
		"void main()\n"
		"{\n"
		"	gl_TexCoord[0] = TexCoords;\n"
		//"	float blend = floor(Blend*4.0+0.5)/4.0;\n"
		"	vec4 lerpedVert = mix(Pose1Vert, Pose2Vert, Blend);\n"
		"	lerpedVert.xyz = floor(lerpedVert.xyz*1.0+0.5)/1.0;\n"
		"	gl_Position = gl_ModelViewProjectionMatrix * lerpedVert;\n"
		"	gl_Position.w = max(gl_Position.w, 0.01);\n"
		"	gl_Position.xyz /= gl_Position.w;\n"
		"	gl_Position.w = 1.0;\n"
		"	gl_Position.xy = floor(gl_Position.xy*ScreenQuantization.xy+0.5)/ScreenQuantization.xy;\n"
		"	float dot1 = r_avertexnormal_dot(Pose1Normal);\n"
		"	float dot2 = r_avertexnormal_dot(Pose2Normal);\n"
		"	gl_FrontColor = LightColor * vec4(vec3(mix(dot1, dot2, Blend)*0.000001+1.0), 1.0);\n"
		"	// fog\n"
		"	vec3 ecPosition = vec3(gl_ModelViewMatrix * lerpedVert);\n"
		"	gl_FogFragCoord = abs(ecPosition.z);\n"
		"}\n";

	const GLchar* fragSource = \
		"#version 110\n"
		"\n"
		"uniform sampler2D Tex;\n"
		"uniform sampler2D FullbrightTex;\n"
		"uniform bool UseFullbrightTex;\n"
		"uniform bool UseOverbright;\n"
		"uniform bool UseAlphaTest;\n"
		"void main()\n"
		"{\n"
		"	vec4 result = texture2D(Tex, gl_TexCoord[0].xy);\n"
		"	//result.rgb = vec3(0.5, 0.5, 0.5);\n"
		"	if (UseAlphaTest && (result.a < 0.666))\n"
		"		discard;\n"
		"	result *= gl_Color;\n"
		"	if (UseOverbright)\n"
		"		result.rgb *= 2.0;\n"
		"	if (UseFullbrightTex)\n"
		"		result += texture2D(FullbrightTex, gl_TexCoord[0].xy);\n"
		"	result = clamp(result, 0.0, 1.0);\n"
		"	// apply GL_EXP2 fog (from the orange book)\n"
		"	float fog = exp(-gl_Fog.density * gl_Fog.density * gl_FogFragCoord * gl_FogFragCoord);\n"
		"	fog = clamp(fog, 0.0, 1.0);\n"
		"	result = mix(gl_Fog.color, result, fog);\n"
		"	result.a = gl_Color.a;\n"
		"	gl_FragColor = result;\n"
		"}\n";

	if (!gl_glsl_alias_able)
	{
		return;
	}

	r_alias_program = GL_CreateProgram(vertSource, fragSource, sizeof(bindings)/sizeof(bindings[0]), bindings);
	Con_Printf("Alias shader program ID is %d\n", r_alias_program);

	if (r_alias_program != 0)
	{
		// get uniform locations
		blendLoc = GL_GetUniformLocation(&r_alias_program, "Blend");
		shadevectorLoc = GL_GetUniformLocation(&r_alias_program, "ShadeVector");
		screenquantizationLoc = GL_GetUniformLocation(&r_alias_program, "ScreenQuantization");
		lightColorLoc = GL_GetUniformLocation(&r_alias_program, "LightColor");
		texLoc = GL_GetUniformLocation(&r_alias_program, "Tex");
		fullbrightTexLoc = GL_GetUniformLocation(&r_alias_program, "FullbrightTex");
		useFullbrightTexLoc = GL_GetUniformLocation(&r_alias_program, "UseFullbrightTex");
		overbrightLoc = GL_GetUniformLocation(&r_alias_program, "UseOverbright");
		useAlphaTestLoc = GL_GetUniformLocation(&r_alias_program, "UseAlphaTest");
	}
}

/*
=============
GL_DrawAliasFrame_GLSL -- ericw

Optimized alias model drawing codepath.
Compared to the original GL_DrawAliasFrame, this makes 1 draw call,
no vertex data is uploaded (it's already in the r_meshvbo and r_meshindexesvbo
static VBOs), and lerping and lighting is done in the vertex shader.

Supports optional overbright, optional fullbright pixels.

Based on code by MH from RMQEngine
=============
*/
void GL_DrawAliasFrame_GLSL(aliashdr_t* paliashdr, lerpdata_t lerpdata, gltexture_t* tx, gltexture_t* fb)
{
	float blend;

	if (lerpdata.pose1 != lerpdata.pose2)
	{
		blend = lerpdata.blend;
	}
	else // poses the same means either 1. the entity has paused its animation, or 2. r_lerpmodels is disabled
	{
		blend = 0;
	}

	GL_UseProgramFunc(r_alias_program);

	GL_BindBuffer(GL_ARRAY_BUFFER, currententity->model->meshvbo);
	GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, currententity->model->meshindexesvbo);

	GL_EnableVertexAttribArrayFunc(texCoordsAttrIndex);
	GL_EnableVertexAttribArrayFunc(triCenterAttrIndex);
	GL_EnableVertexAttribArrayFunc(pose1VertexAttrIndex);
	GL_EnableVertexAttribArrayFunc(pose2VertexAttrIndex);
	GL_EnableVertexAttribArrayFunc(pose1NormalAttrIndex);
	GL_EnableVertexAttribArrayFunc(pose2NormalAttrIndex);

	GL_VertexAttribPointerFunc(texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, 0, (void *)(intptr_t)currententity->model->vbostofs);
	GL_VertexAttribPointerFunc(triCenterAttrIndex, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(meshxyz_t), GLARB_GetXYZOffset(paliashdr, lerpdata.pose1));
	GL_VertexAttribPointerFunc(pose1VertexAttrIndex, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(meshxyz_t), GLARB_GetXYZOffset(paliashdr, lerpdata.pose1));
	GL_VertexAttribPointerFunc(pose2VertexAttrIndex, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(meshxyz_t), GLARB_GetXYZOffset(paliashdr, lerpdata.pose2));
	// GL_TRUE to normalize the signed bytes to [-1 .. 1]
	GL_VertexAttribPointerFunc(pose1NormalAttrIndex, 4, GL_BYTE, GL_TRUE, sizeof(meshxyz_t), GLARB_GetNormalOffset(paliashdr, lerpdata.pose1));
	GL_VertexAttribPointerFunc(pose2NormalAttrIndex, 4, GL_BYTE, GL_TRUE, sizeof(meshxyz_t), GLARB_GetNormalOffset(paliashdr, lerpdata.pose2));

	// set uniforms
	GL_Uniform1fFunc(blendLoc, blend);
	GL_Uniform3fFunc(shadevectorLoc, shadevector.x, shadevector.y, shadevector.z);
	GL_Uniform3fFunc(screenquantizationLoc, r_refdef.vrect.width/r_scale.value/2, r_refdef.vrect.height/r_scale.value/2, 0);
	GL_Uniform4fFunc(lightColorLoc, lightcolor.x, lightcolor.y, lightcolor.z, entalpha);
	GL_Uniform1iFunc(texLoc, 0);
	GL_Uniform1iFunc(fullbrightTexLoc, 1);
	GL_Uniform1iFunc(useFullbrightTexLoc, (fb != NULL) ? 1 : 0);
	GL_Uniform1fFunc(overbrightLoc, overbright ? 1 : 0);
	GL_Uniform1iFunc(useAlphaTestLoc, (currententity->model->flags & MF_HOLEY) ? 1 : 0);

	// set textures
	GL_SelectTexture(GL_TEXTURE0);
	GL_Bind(tx);

	if (fb)
	{
		GL_SelectTexture(GL_TEXTURE1);
		GL_Bind(fb);
	}

	// draw
	glDrawElements(GL_TRIANGLES, paliashdr->numindexes, GL_UNSIGNED_SHORT, (void *)(intptr_t)currententity->model->vboindexofs);

	// clean up
	GL_DisableVertexAttribArrayFunc(texCoordsAttrIndex);
	GL_DisableVertexAttribArrayFunc(triCenterAttrIndex);
	GL_DisableVertexAttribArrayFunc(pose1VertexAttrIndex);
	GL_DisableVertexAttribArrayFunc(pose2VertexAttrIndex);
	GL_DisableVertexAttribArrayFunc(pose1NormalAttrIndex);
	GL_DisableVertexAttribArrayFunc(pose2NormalAttrIndex);

	GL_UseProgramFunc(0);
	GL_SelectTexture(GL_TEXTURE0);

	rs_aliaspasses += paliashdr->numtris;
}

/*
=============
GL_DrawAliasFrame -- johnfitz -- rewritten to support colored light, lerping, entalpha, multitexture, and r_drawflat
=============
*/
void GL_DrawAliasFrame(aliashdr_t* paliashdr, lerpdata_t lerpdata)
{
	float vertcolor[4];
	trivertx_t* verts1, *verts2;
	int* commands;
	int count;
	float u, v;
	float blend, iblend;
	bool lerping;

	if (lerpdata.pose1 != lerpdata.pose2)
	{
		lerping = true;
		verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		verts2 = verts1;
		verts1 += lerpdata.pose1 * paliashdr->poseverts;
		verts2 += lerpdata.pose2 * paliashdr->poseverts;
		blend = lerpdata.blend;
		iblend = 1.0f - blend;
	}
	else // poses the same means either 1. the entity has paused its animation, or 2. r_lerpmodels is disabled
	{
		lerping = false;
		verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		verts2 = verts1; // avoid bogus compiler warning
		verts1 += lerpdata.pose1 * paliashdr->poseverts;
		blend = iblend = 0; // avoid bogus compiler warning
	}

	commands = (int *)((byte *)paliashdr + paliashdr->commands);

	vertcolor[3] = entalpha; //never changes, so there's no need to put this inside the loop

	while (1)
	{
		// get the vertex count and primitive type
		count = *commands++;
		if (!count)
			break; // done

		if (count < 0)
		{
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else
			glBegin(GL_TRIANGLE_STRIP);

		do
		{
			u = ((float *)commands)[0];
			v = ((float *)commands)[1];
			if (mtexenabled)
			{
				GL_MTexCoord2fFunc(GL_TEXTURE0_ARB, u, v);
				GL_MTexCoord2fFunc(GL_TEXTURE1_ARB, u, v);
			}
			else
				glTexCoord2f(u, v);

			commands += 2;

			if (shading)
			{
				if (r_drawflat_cheatsafe)
				{
					srand(count * (unsigned int)(src_offset_t)commands);
					glColor3f(rand()%256/255.0, rand()%256/255.0, rand()%256/255.0);
				}
				else if (lerping)
				{
					vertcolor[0] = (shadedots[verts1->lightnormalindex]*iblend + shadedots[verts2->lightnormalindex]*blend) * lightcolor.x;
					vertcolor[1] = (shadedots[verts1->lightnormalindex]*iblend + shadedots[verts2->lightnormalindex]*blend) * lightcolor.y;
					vertcolor[2] = (shadedots[verts1->lightnormalindex]*iblend + shadedots[verts2->lightnormalindex]*blend) * lightcolor.z;
					glColor4fv(vertcolor);
				}
				else
				{
					vertcolor[0] = shadedots[verts1->lightnormalindex] * lightcolor.x;
					vertcolor[1] = shadedots[verts1->lightnormalindex] * lightcolor.y;
					vertcolor[2] = shadedots[verts1->lightnormalindex] * lightcolor.z;
					glColor4fv(vertcolor);
				}
			}

			if (lerping)
			{
				glVertex3f(verts1->v[0]*iblend + verts2->v[0]*blend,
					verts1->v[1]*iblend + verts2->v[1]*blend,
					verts1->v[2]*iblend + verts2->v[2]*blend);
				verts1++;
				verts2++;
			}
			else
			{
				glVertex3f(verts1->v[0], verts1->v[1], verts1->v[2]);
				verts1++;
			}
		} while (--count);

		glEnd();
	}

	rs_aliaspasses += paliashdr->numtris;
}

/*
=================
R_SetupAliasFrame -- johnfitz -- rewritten to support lerping
=================
*/
void R_SetupAliasFrame(aliashdr_t* paliashdr, int frame, lerpdata_t* lerpdata)
{
	entity_t* e = currententity;
	int posenum, numposes;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf("R_AliasSetupFrame: no such frame %d for '%s'\n", frame, e->model->name);
		frame = 0;
	}

	posenum = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		e->lerptime = paliashdr->frames[frame].interval;
		posenum += (int)(cl.time / e->lerptime) % numposes;
	}
	else
		e->lerptime = 0.1;

	if (e->lerpflags & LERP_RESETANIM) //kill any lerp in progress
	{
		e->lerpstart = 0;
		e->previouspose = posenum;
		e->currentpose = posenum;
		e->lerpflags -= LERP_RESETANIM;
	}
	else if (e->currentpose != posenum) // pose changed, start new lerp
	{
		if (e->lerpflags & LERP_RESETANIM2) //defer lerping one more time
		{
			e->lerpstart = 0;
			e->previouspose = posenum;
			e->currentpose = posenum;
			e->lerpflags -= LERP_RESETANIM2;
		}
		else
		{
			e->lerpstart = cl.time;
			e->previouspose = e->currentpose;
			e->currentpose = posenum;
		}
	}

	//set up values
	if (r_lerpmodels.value && !(e->model->flags & MOD_NOLERP && r_lerpmodels.value != 2))
	{
		if (e->lerpflags & LERP_FINISH && numposes == 1)
			lerpdata->blend = Clamp((cl.time - e->lerpstart) / (e->lerpfinish - e->lerpstart), 0, 1);
		else
			lerpdata->blend = Clamp((cl.time - e->lerpstart) / e->lerptime, 0, 1);
		lerpdata->pose1 = e->previouspose;
		lerpdata->pose2 = e->currentpose;
	}
	else //don't lerp
	{
		lerpdata->blend = 1;
		lerpdata->pose1 = posenum;
		lerpdata->pose2 = posenum;
	}
}

/*
=================
R_SetupEntityTransform -- johnfitz -- set up transform part of lerpdata
=================
*/
void R_SetupEntityTransform(entity_t* e, lerpdata_t* lerpdata)
{
	float blend;

	// if LERP_RESETMOVE, kill any lerps in progress
	if (e->lerpflags & LERP_RESETMOVE)
	{
		e->movelerpstart = 0;
		e->previousorigin = e->origin;
		e->currentorigin = e->origin;
		e->previousangles = e->angles;
		e->currentangles = e->angles;
		e->lerpflags -= LERP_RESETMOVE;
	}
	else if (!Vec_Equal(e->origin, e->currentorigin) || !Vec_Equal(e->angles, e->currentangles)) // origin/angles changed, start new lerp
	{
		e->movelerpstart = cl.time;
		e->previousorigin = e->currentorigin;
		e->currentorigin = e->origin;
		e->previousangles = e->currentangles;
		e->currentangles = e->angles;
	}

	//set up values
	if (r_lerpmove.value && e != &cl.viewent && e->lerpflags & LERP_MOVESTEP)
	{
		if (e->lerpflags & LERP_FINISH)
			blend = Clamp((cl.time - e->movelerpstart) / (e->lerpfinish - e->movelerpstart), 0, 1);
		else
			blend = Clamp((cl.time - e->movelerpstart) / 0.1, 0, 1);

		//translation
		lerpdata->origin = Vec_Lerp(e->previousorigin, e->currentorigin, blend);
		//Quantize position because vertices are quantized in object space in shader.
		lerpdata->origin.x = floorf(lerpdata->origin.x*2+0.5)/2;
		lerpdata->origin.y = floorf(lerpdata->origin.y*2+0.5)/2;
		lerpdata->origin.z = floorf(lerpdata->origin.z*2+0.5)/2;
		//rotation
		lerpdata->angles = Vec_LerpAngles(e->previousangles, e->currentangles, blend);
		//Quantize rotation because vertices are quantized object space in shader.
		lerpdata->angles.x = floorf(lerpdata->angles.x+0.5);
		lerpdata->angles.y = floorf(lerpdata->angles.y+0.5);
		lerpdata->angles.z = floorf(lerpdata->angles.z+0.5);
	}
	else //don't lerp
	{
		lerpdata->origin = e->origin;
		lerpdata->angles = e->angles;
	}
}

/*
=================
R_SetupAliasLighting -- johnfitz -- broken out from R_DrawAliasModel and rewritten
=================
*/
void R_SetupAliasLighting(entity_t* e)
{
	vec3_t dist;
	float add;
	int i;
	int quantizedangle;
	float radiansangle;

	R_LightPoint(e->origin);

	if (e->previousenvlightcolor.x < 0 || e->previousenvlightcolor.y < 0 || e->previousenvlightcolor.z < 0)
	{
		e->previousenvlightcolor = lightcolor;
	}
	lightcolor = Vec_Lerp(e->previousenvlightcolor, lightcolor, 20*host_frametime);
	e->previousenvlightcolor = lightcolor;

	//add dlights
	for (i = 0; i<MAX_DLIGHTS; i++)
	{
		if (!cl_dlights[i].issplat && cl_dlights[i].die >= cl.time)
		{
			dist = Vec_Sub(currententity->origin, cl_dlights[i].origin);
			add = cl_dlights[i].radius - Vec_Length(dist);
			if (add > 0)
				lightcolor = Vec_MulAdd(lightcolor, add, cl_dlights[i].color);
		}
	}

	// minimum light value on gun (24)
	if (e == &cl.viewent)
	{
		add = 72.0f - (lightcolor.x + lightcolor.y + lightcolor.z);
		if (add > 0.0f)
		{
			lightcolor.x += add / 3.0f;
			lightcolor.y += add / 3.0f;
			lightcolor.z += add / 3.0f;
		}
	}

	// minimum light value on players (8)
	if (currententity > cl_entities && currententity <= cl_entities + cl.maxclients)
	{
		add = 24.0f - (lightcolor.x + lightcolor.y + lightcolor.z);
		if (add > 0.0f)
		{
			lightcolor.x += add / 3.0f;
			lightcolor.y += add / 3.0f;
			lightcolor.z += add / 3.0f;
		}
	}

	// clamp lighting so it doesn't overbright as much (96)
	if (overbright)
	{
		add = 288.0f / (lightcolor.x + lightcolor.y + lightcolor.z);
		if (add < 1.0f)
			lightcolor = Vec_Scale(lightcolor, add);
	}

	//hack up the brightness when fullbrights but no overbrights (256)
	if (gl_fullbrights.value && !gl_overbright_models.value)
		if (e->model->flags & MOD_FBRIGHTHACK)
		{
			lightcolor.x = 256.0f;
			lightcolor.y = 256.0f;
			lightcolor.z = 256.0f;
		}

	quantizedangle = ((int)(e->angles.y * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1);

	//ericw -- shadevector is passed to the shader to compute shadedots inside the
	//shader, see GLAlias_CreateShaders()
	radiansangle = (quantizedangle / 16.0) * 2.0 * 3.14159;
	shadevector.x = cos(-radiansangle);
	shadevector.y = sin(-radiansangle);
	shadevector.z = 1;
	Vec_Normalize(&shadevector);
	//ericw --

	shadedots = r_avertexnormal_dots[quantizedangle];
	lightcolor = Vec_Scale(lightcolor, 1.0f / 200.0f);
}

/*
=================
R_DrawAliasModel -- johnfitz -- almost completely rewritten
=================
*/
void R_DrawAliasModel(entity_t* e)
{
	aliashdr_t* paliashdr;
	int i, anim;
	gltexture_t* tx, *fb;
	lerpdata_t lerpdata;
	bool alphatest = !!(e->model->flags & MF_HOLEY);

	//
	// setup pose/lerp data -- do it first so we don't miss updates due to culling
	//
	paliashdr = (aliashdr_t *)Mod_Extradata(e->model);
	R_SetupAliasFrame(paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform(e, &lerpdata);

	//
	// cull it
	//
	if (R_CullModelForEntity(e))
		return;

	//
	// transform it
	//
	glPushMatrix();
	R_RotateForEntity(lerpdata.origin, lerpdata.angles);
	glTranslatef(paliashdr->scale_origin.x, paliashdr->scale_origin.y, paliashdr->scale_origin.z);
	glScalef(paliashdr->scale.x, paliashdr->scale.y, paliashdr->scale.z);

	//
	// random stuff
	//
	if (gl_smoothmodels.value && !r_drawflat_cheatsafe)
		glShadeModel(GL_SMOOTH);
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	overbright = gl_overbright_models.value;
	shading = true;

	//
	// set up for alpha blending
	//
	if (r_drawflat_cheatsafe || r_lightmap_cheatsafe) //no alpha in drawflat or lightmap mode
		entalpha = 1;
	else
		entalpha = ENTALPHA_DECODE(e->alpha);
	if (entalpha == 0)
		goto cleanup;
	if (entalpha < 1)
	{
		if (!gl_texture_env_combine) overbright = false; //overbright can't be done in a single pass without combiners
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
	}
	else if (alphatest)
		glEnable(GL_ALPHA_TEST);

	//
	// set up lighting
	//
	rs_aliaspolys += paliashdr->numtris;
	R_SetupAliasLighting(e);

	//
	// set up textures
	//
	GL_DisableMultitexture();
	anim = (int)(cl.time*10) & 3;
	if ((e->skinnum >= paliashdr->numskins) || (e->skinnum < 0))
	{
		Con_DPrintf("R_DrawAliasModel: no such skin # %d for '%s'\n", e->skinnum, e->model->name);
		tx = NULL; // NULL will give the checkerboard texture
		fb = NULL;
	}
	else
	{
		tx = paliashdr->gltextures[e->skinnum][anim];
		fb = paliashdr->fbtextures[e->skinnum][anim];
	}
	if (e->colormap != vid.colormap && !gl_nocolors.value)
	{
		i = e - cl_entities;
		if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
			tx = playertextures[i - 1];
	}
	if (!gl_fullbrights.value)
		fb = NULL;

	//
	// draw it
	//
	if (r_drawflat_cheatsafe)
	{
		glDisable(GL_TEXTURE_2D);
		GL_DrawAliasFrame(paliashdr, lerpdata);
		glEnable(GL_TEXTURE_2D);
		srand((int)(cl.time * 1000)); //restore randomness
	}
	else if (r_fullbright_cheatsafe)
	{
		GL_Bind(tx);
		shading = false;
		glColor4f(1, 1, 1, entalpha);
		GL_DrawAliasFrame(paliashdr, lerpdata);
		if (fb)
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_Bind(fb);
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glDepthMask(GL_FALSE);
			glColor3f(entalpha, entalpha, entalpha);
			Fog_StartAdditive();
			GL_DrawAliasFrame(paliashdr, lerpdata);
			Fog_StopAdditive();
			glDepthMask(GL_TRUE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_BLEND);
		}
	}
	else if (r_lightmap_cheatsafe)
	{
		glDisable(GL_TEXTURE_2D);
		shading = false;
		glColor3f(1, 1, 1);
		GL_DrawAliasFrame(paliashdr, lerpdata);
		glEnable(GL_TEXTURE_2D);
	}
	else
	{
		GL_DrawAliasFrame_GLSL(paliashdr, lerpdata, tx, fb);
	}

cleanup:
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glShadeModel(GL_FLAT);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	if (alphatest)
		glDisable(GL_ALPHA_TEST);
	glColor3f(1, 1, 1);
	glPopMatrix();
}

//johnfitz -- values for shadow matrix
#define SHADOW_SKEW_X -0.7 //skew along x axis. -0.7 to mimic glquake shadows
#define SHADOW_SKEW_Y 0 //skew along y axis. 0 to mimic glquake shadows
#define SHADOW_VSCALE 0 //0=completely flat
#define SHADOW_HEIGHT 0.1 //how far above the floor to render the shadow
//johnfitz

/*
=============
GL_DrawAliasShadow -- johnfitz -- rewritten

TODO: orient shadow onto "lightplane" (a global mplane_t*)
=============
*/
void GL_DrawAliasShadow(entity_t* e)
{
	float shadowmatrix[16] = { 1, 0, 0, 0,
								0, 1, 0, 0,
								SHADOW_SKEW_X, SHADOW_SKEW_Y, SHADOW_VSCALE, 0,
								0, 0, SHADOW_HEIGHT, 1 };
	float lheight;
	aliashdr_t* paliashdr;
	lerpdata_t lerpdata;

	if (R_CullModelForEntity(e))
		return;

	if (e == &cl.viewent || e->model->flags & MOD_NOSHADOW)
		return;

	entalpha = ENTALPHA_DECODE(e->alpha);
	if (entalpha == 0) return;

	paliashdr = (aliashdr_t *)Mod_Extradata(e->model);
	R_SetupAliasFrame(paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform(e, &lerpdata);
	R_LightPoint(e->origin);
	lheight = currententity->origin.z - lightspot.z;

	// set up matrix
	glPushMatrix();
	glTranslatef(lerpdata.origin.x, lerpdata.origin.y, lerpdata.origin.z);
	glTranslatef(0, 0, -lheight);
	glMultMatrixf(shadowmatrix);
	glTranslatef(0, 0, lheight);
	glRotatef(lerpdata.angles.y, 0, 0, 1);
	glRotatef(-lerpdata.angles.x, 0, 1, 0);
	glRotatef(lerpdata.angles.z, 1, 0, 0);
	glTranslatef(paliashdr->scale_origin.x, paliashdr->scale_origin.y, paliashdr->scale_origin.z);
	glScalef(paliashdr->scale.x, paliashdr->scale.y, paliashdr->scale.z);

	// draw it
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	GL_DisableMultitexture();
	glDisable(GL_TEXTURE_2D);
	shading = false;
	glColor4f(0, 0, 0, entalpha * 0.5);
	GL_DrawAliasFrame(paliashdr, lerpdata);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	//clean up
	glPopMatrix();
}

/*
=================
R_DrawAliasModel_ShowTris -- johnfitz
=================
*/
void R_DrawAliasModel_ShowTris(entity_t* e)
{
	aliashdr_t* paliashdr;
	lerpdata_t lerpdata;

	if (R_CullModelForEntity(e))
		return;

	paliashdr = (aliashdr_t *)Mod_Extradata(e->model);
	R_SetupAliasFrame(paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform(e, &lerpdata);

	glPushMatrix();
	R_RotateForEntity(lerpdata.origin, lerpdata.angles);
	glTranslatef(paliashdr->scale_origin.x, paliashdr->scale_origin.y, paliashdr->scale_origin.z);
	glScalef(paliashdr->scale.x, paliashdr->scale.y, paliashdr->scale.z);

	shading = false;
	glColor3f(1, 1, 1);
	GL_DrawAliasFrame(paliashdr, lerpdata);

	glPopMatrix();
}

