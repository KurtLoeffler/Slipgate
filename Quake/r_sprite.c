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
//r_sprite.c -- sprite model rendering

#include "quakedef.h"

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t* R_GetSpriteFrame(entity_t* currentent)
{
	msprite_t* psprite;
	mspritegroup_t* pspritegroup;
	mspriteframe_t* pspriteframe;
	int i, numframes, frame;
	float* pintervals, fullinterval, targettime, time;

	psprite = (msprite_t *)currentent->model->cache.data;
	frame = currentent->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_DPrintf("R_DrawSprite: no such frame %d for '%s'\n", frame, currentent->model->name);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currentent->syncbase;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i = 0; i<(numframes-1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

/*
=================
R_DrawSpriteModel -- johnfitz -- rewritten: now supports all orientations
=================
*/
void R_DrawSpriteModel(entity_t* e)
{
	vec3_t point, v_forward, v_right, v_up;
	msprite_t* psprite;
	mspriteframe_t* frame;
	vec3_t s_up, s_right;
	float angle, sr, cr;

	//TODO: frustum cull it?

	frame = R_GetSpriteFrame(e);
	psprite = (msprite_t *)currententity->model->cache.data;

	switch (psprite->type)
	{
	case SPR_VP_PARALLEL_UPRIGHT: //faces view plane, up is towards the heavens
		v_up.x = 0;
		v_up.y = 0;
		v_up.z = 1;
		s_up = v_up;
		s_right = vright;
		break;
	case SPR_FACING_UPRIGHT: //faces camera origin, up is towards the heavens
		v_forward = Vec_Sub(currententity->origin, r_origin);
		v_forward.z = 0;
		Vec_NormalizeFast(v_forward);
		v_right.x = v_forward.y;
		v_right.y = -v_forward.x;
		v_right.z = 0;
		v_up.x = 0;
		v_up.y = 0;
		v_up.z = 1;
		s_up = v_up;
		s_right = v_right;
		break;
	case SPR_VP_PARALLEL: //faces view plane, up is towards the top of the screen
		s_up = vup;
		s_right = vright;
		break;
	case SPR_ORIENTED: //pitch yaw roll are independent of camera
		Vec_AngleVectors(currententity->angles, &v_forward, &v_right, &v_up);
		s_up = v_up;
		s_right = v_right;
		break;
	case SPR_VP_PARALLEL_ORIENTED: //faces view plane, but obeys roll value
		angle = currententity->angles.roll * M_PI_DIV_180;
		sr = sin(angle);
		cr = cos(angle);
		v_right.x = vright.x * cr + vup.x * sr;
		v_right.y = vright.y * cr + vup.y * sr;
		v_right.z = vright.z * cr + vup.z * sr;
		v_up.x = vright.x * -sr + vup.x * cr;
		v_up.y = vright.y * -sr + vup.y * cr;
		v_up.z = vright.z * -sr + vup.z * cr;
		s_up = v_up;
		s_right = v_right;
		break;
	default:
		return;
	}

	//johnfitz: offset decals
	if (psprite->type == SPR_ORIENTED)
		GL_PolygonOffset(OFFSET_DECAL);

	glColor3f(1, 1, 1);

	GL_DisableMultitexture();

	GL_Bind(frame->gltexture);

	glEnable(GL_ALPHA_TEST);
	glBegin(GL_TRIANGLE_FAN); //was GL_QUADS, but changed to support r_showtris

	glTexCoord2f(0, frame->tmax);
	point = Vec_MulAdd(e->origin, frame->down, s_up);
	point = Vec_MulAdd(point, frame->left, s_right);
	glVertex3fv(point.values);

	glTexCoord2f(0, 0);
	point = Vec_MulAdd(e->origin, frame->up, s_up);
	point = Vec_MulAdd(point, frame->left, s_right);
	glVertex3fv(point.values);

	glTexCoord2f(frame->smax, 0);
	point = Vec_MulAdd(e->origin, frame->up, s_up);
	point = Vec_MulAdd(point, frame->right, s_right);
	glVertex3fv(point.values);

	glTexCoord2f(frame->smax, frame->tmax);
	point = Vec_MulAdd(e->origin, frame->down, s_up);
	point = Vec_MulAdd(point, frame->right, s_right);
	glVertex3fv(point.values);

	glEnd();
	glDisable(GL_ALPHA_TEST);

	//johnfitz: offset decals
	if (psprite->type == SPR_ORIENTED)
		GL_PolygonOffset(OFFSET_NONE);
}
