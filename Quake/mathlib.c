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
// mathlib.c -- math primitives

#include "quakedef.h"

/*-----------------------------------------------------------------*/


//#define DEG2RAD( a ) ( a * M_PI ) / 180.0F
#define DEG2RAD( a ) ( (a) * M_PI_DIV_180 ) //johnfitz

void ProjectPointOnPlane(vec3_t* dst, const vec3_t p, const vec3_t normal)
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / Vec_Dot(normal, normal);

	d = Vec_Dot(normal, p) * inv_denom;

	n.x = normal.x * inv_denom;
	n.y = normal.y * inv_denom;
	n.z = normal.z * inv_denom;

	dst->x = p.x - d * n.x;
	dst->y = p.y - d * n.y;
	dst->z = p.z - d * n.z;
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector(vec3_t* dst, const vec3_t src)
{
	int pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for (pos = 0, i = 0; i < 3; i++)
	{
		if (fabs(src.values[i]) < minelem)
		{
			pos = i;
			minelem = fabs(src.values[i]);
		}
	}
	tempvec.x = tempvec.y = tempvec.z = 0.0F;
	tempvec.values[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane(dst, tempvec, src);

	/*
	** normalize the result
	*/
	Vec_Normalize(dst);
}

//johnfitz -- removed RotatePointAroundVector() becuase it's no longer used and my compiler fucked it up anyway

/*-----------------------------------------------------------------*/


float AngleMod(float a)
{
#if 0
	if (a >= 0)
		a -= 360*(int)(a/360);
	else
		a += 360*(1 + (int)(-a/360));
#endif
	//a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	
	a = fmodf(a, 360);
	if (a < 0)
	{
		a += 360;
	}

	return a;
}


/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, mplane_t* p)
{
	float dist1, dist2;
	int sides;

#if 0 // this is done by the BOX_ON_PLANE_SIDE macro before calling this
	// function
// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}
#endif

	// general case
	switch (p->signbits)
	{
	case 0:
		dist1 = p->normal.x*emaxs.x + p->normal.y*emaxs.y + p->normal.z*emaxs.z;
		dist2 = p->normal.x*emins.x + p->normal.y*emins.y + p->normal.z*emins.z;
		break;
	case 1:
		dist1 = p->normal.x*emins.x + p->normal.y*emaxs.y + p->normal.z*emaxs.z;
		dist2 = p->normal.x*emaxs.x + p->normal.y*emins.y + p->normal.z*emins.z;
		break;
	case 2:
		dist1 = p->normal.x*emaxs.x + p->normal.y*emins.y + p->normal.z*emaxs.z;
		dist2 = p->normal.x*emins.x + p->normal.y*emaxs.y + p->normal.z*emins.z;
		break;
	case 3:
		dist1 = p->normal.x*emins.x + p->normal.y*emins.y + p->normal.z*emaxs.z;
		dist2 = p->normal.x*emaxs.x + p->normal.y*emaxs.y + p->normal.z*emins.z;
		break;
	case 4:
		dist1 = p->normal.x*emaxs.x + p->normal.y*emaxs.y + p->normal.z*emins.z;
		dist2 = p->normal.x*emins.x + p->normal.y*emins.y + p->normal.z*emaxs.z;
		break;
	case 5:
		dist1 = p->normal.x*emins.x + p->normal.y*emaxs.y + p->normal.z*emins.z;
		dist2 = p->normal.x*emaxs.x + p->normal.y*emins.y + p->normal.z*emaxs.z;
		break;
	case 6:
		dist1 = p->normal.x*emaxs.x + p->normal.y*emins.y + p->normal.z*emins.z;
		dist2 = p->normal.x*emins.x + p->normal.y*emaxs.y + p->normal.z*emaxs.z;
		break;
	case 7:
		dist1 = p->normal.x*emins.x + p->normal.y*emins.y + p->normal.z*emins.z;
		dist2 = p->normal.x*emaxs.x + p->normal.y*emaxs.y + p->normal.z*emaxs.z;
		break;
	default:
		dist1 = dist2 = 0; // shut up compiler
		Sys_Error("BoxOnPlaneSide:  Bad signbits");
		break;
	}

#if 0
	int i;
	vec3_t corners[2];

	for (i = 0; i<3; i++)
	{
		if (plane->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist = Vec_Dot(plane->normal, corners[0]) - plane->dist;
	dist2 = Vec_Dot(plane->normal, corners[1]) - plane->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;
#endif

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

#ifdef PARANOID
	if (sides == 0)
		Sys_Error("BoxOnPlaneSide: sides==0");
#endif

	return sides;
}

vec3_t Vec_Zero()
{
	return (vec3_t) { 0, 0, 0 };
}

vec3_t Vec_Uniform(float value)
{
	return (vec3_t) { value, value, value };
}

vec3_t Vec(float x, float y, float z)
{
	return (vec3_t) { x, y, z };
}

//johnfitz -- the opposite of Vec_AngleVectors.  this takes forward and generates pitch yaw roll
//TODO: take right and up vectors to properly set yaw and roll
vec3_t Vec_Angles(const vec3_t forward)
{
	vec3_t temp;
	vec3_t angles;
	temp.x = forward.x;
	temp.y = forward.y;
	temp.z = 0;
	angles.pitch = -atan2(forward.z, Vec_Length(temp)) / M_PI_DIV_180;
	angles.yaw = atan2(forward.y, forward.x) / M_PI_DIV_180;
	angles.roll = 0;
	return angles;
}

void Vec_AngleVectors(vec3_t angles, vec3_t* forward, vec3_t* right, vec3_t* up)
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = angles.yaw * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles.pitch * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles.roll * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	forward->x = cp*cy;
	forward->y = cp*sy;
	forward->z = -sp;
	right->x = (-1*sr*sp*cy+-1*cr*-sy);
	right->y = (-1*sr*sp*sy+-1*cr*cy);
	right->z = -1*sr*cp;
	up->x = (cr*sp*cy+-sr*-sy);
	up->y = (cr*sp*sy+-sr*cy);
	up->z = cr*cp;
}

bool Vec_Equal(vec3_t v1, vec3_t v2)
{
	int i;

	for (i = 0; i<3; i++)
		if (v1.values[i] != v2.values[i])
			return false;

	return true;
}

vec3_t Vec_MulAdd(vec3_t veca, float scale, vec3_t vecb)
{
	vec3_t out;
	out.x = veca.x + scale*vecb.x;
	out.y = veca.y + scale*vecb.y;
	out.z = veca.z + scale*vecb.z;
	return out;
}

float Vec_Dot(vec3_t v1, vec3_t v2)
{
	return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

double Vec_DotDouble(vec3_t v1, vec3_t v2)
{
	return (double)v1.x*v2.x + (double)v1.y*v2.y + (double)v1.z*v2.z;
}

vec3_t Vec_Sub(vec3_t veca, vec3_t vecb)
{
	veca.x -= vecb.x;
	veca.y -= vecb.y;
	veca.z -= vecb.z;
	return veca;
}

vec3_t Vec_Add(vec3_t veca, vec3_t vecb)
{
	veca.x += vecb.x;
	veca.y += vecb.y;
	veca.z += vecb.z;
	return veca;
}

vec3_t Vec_Mul(vec3_t veca, vec3_t vecb)
{
	veca.x *= vecb.x;
	veca.y *= vecb.y;
	veca.z *= vecb.z;
	return veca;
}

vec3_t Vec_Div(vec3_t veca, vec3_t vecb)
{
	veca.x /= vecb.x;
	veca.y /= vecb.y;
	veca.z /= vecb.z;
	return veca;
}

vec3_t Vec_Cross(vec3_t v1, vec3_t v2)
{
	vec3_t cross;
	cross.x = v1.y*v2.z - v1.z*v2.y;
	cross.y = v1.z*v2.x - v1.x*v2.z;
	cross.z = v1.x*v2.y - v1.y*v2.x;
	return cross;
}

float Vec_Length(vec3_t v)
{
	return sqrt(Vec_Dot(v, v));
}

float Vec_Normalize(vec3_t* v)
{
	float length, ilength;

	length = sqrt(Vec_Dot(*v, *v));

	if (length)
	{
		ilength = 1/length;
		v->x *= ilength;
		v->y *= ilength;
		v->z *= ilength;
	}

	return length;

}

vec3_t Vec_Normalized(vec3_t v)
{
	Vec_Normalize(&v);
	return v;
}

vec3_t Vec_Inverse(vec3_t v)
{
	v.x = -v.x;
	v.y = -v.y;
	v.z = -v.z;
	return v;
}

vec3_t Vec_Scale(vec3_t in, float scale)
{
	vec3_t out;
	out.x = in.x*scale;
	out.y = in.y*scale;
	out.z = in.z*scale;
	return out;
}

int Q_log2(int val)
{
	int answer = 0;
	while (val >>= 1)
		answer++;
	return answer;
}

float Lerp(float start, float end, float percent)
{
	return (start + percent*(end - start));
}

vec3_t Vec_Lerp(vec3_t start, vec3_t end, float percent)
{
	return Vec_Add(start, Vec_Scale(Vec_Sub(end, start), percent));
}

vec3_t Vec_LerpAngles(vec3_t start, vec3_t end, float percent)
{
	vec3_t shortest_angle = Vec_Sub(end, start);
	shortest_angle.x = fmodf(((fmodf(shortest_angle.x, 360)) + 540), 360) - 180;
	shortest_angle.y = fmodf(((fmodf(shortest_angle.y, 360)) + 540), 360) - 180;
	shortest_angle.z = fmodf(((fmodf(shortest_angle.z, 360)) + 540), 360) - 180;
	return Vec_Add(start, Vec_Scale(shortest_angle, percent));
}

float Clamp(float value, float min, float max)
{
	if (value < min) value = min;
	if (value > max) value = max;
	return value;
}

float Min(float a, float b)
{
	if (b < a) a = b;
	return a;
}

float Max(float a, float b)
{
	if (b > a) a = b;
	return a;
}

float Random()
{
	return (float)rand()/(float)(RAND_MAX);
}

float RandomRange(float min, float max)
{
	float dif = max-min;
	return min+Random()*dif;
}

vec3_t RandomInCube()
{
	return Vec(Random()*2-1, Random()*2-1, Random()*2-1);
}

/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
}


/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
		in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
		in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
		in1[2][2] * in2[2][3] + in1[2][3];
}


/*
===================
FloorDivMod

Returns mathematically correct (floor-based) quotient and remainder for
numer and denom, both of which should contain no fractional part. The
quotient must fit in 32 bits.
====================
*/

void FloorDivMod(double numer, double denom, int* quotient,
	int* rem)
{
	int q, r;
	double x;

#ifndef PARANOID
	if (denom <= 0.0)
		Sys_Error("FloorDivMod: bad denominator %f\n", denom);

	// if ((floor(numer) != numer) || (floor(denom) != denom))
	// Sys_Error ("FloorDivMod: non-integer numer or denom %f %f\n",
	// numer, denom);
#endif

	if (numer >= 0.0)
	{

		x = floor(numer / denom);
		q = (int)x;
		r = (int)floor(numer - (x * denom));
	}
	else
	{
		//
		// perform operations with positive values, and fix mod to make floor-based
		//
		x = floor(-numer / denom);
		q = -(int)x;
		r = (int)floor(-numer - (x * denom));
		if (r != 0)
		{
			q--;
			r = (int)denom - r;
		}
	}

	*quotient = q;
	*rem = r;
}


/*
===================
GreatestCommonDivisor
====================
*/
int GreatestCommonDivisor(int i1, int i2)
{
	if (i1 > i2)
	{
		if (i2 == 0)
			return (i1);
		return GreatestCommonDivisor(i2, i1 % i2);
	}
	else
	{
		if (i1 == 0)
			return (i2);
		return GreatestCommonDivisor(i1, i2 % i1);
	}
}


/*
===================
Invert24To16

Inverts an 8.24 value to a 16.16 value
====================
*/

fixed16_t Invert24To16(fixed16_t val)
{
	if (val < 256)
		return (0xFFFFFFFF);

	return (fixed16_t)
		(((double)0x10000 * (double)0x1000000 / (double)val) + 0.5);
}

