#pragma once

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

// mathlib.h

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h
#endif

#define M_PI_DIV_180 (M_PI / 180.0) //johnfitz

struct mplane_s;

#define nanmask (255 << 23) /* 7F800000 */
#if 0 /* macro is violating strict aliasing rules */
#define IS_NAN(x) (((*(int *) (char *) &x) & nanmask) == nanmask)
#else
static inline int IS_NAN(float x) {
	union { float f; int i; } num;
	num.f = x;
	return ((num.i & nanmask) == nanmask);
}
#endif

#define Q_rint(x) ((x) > 0 ? (int)((x) + 0.5) : (int)((x) - 0.5)) //johnfitz -- from joequake

/* math */
//typedef float float;
//typedef float vec3_t[3];
typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

typedef union
{
	struct
	{
		union
		{
			float x;
			float pitch;
		};
		union
		{
			float y;
			float yaw;
		};
		union
		{
			float z;
			float roll;
		};
	};
	float values[3];
} vec3_t;

typedef union
{
	struct
	{
		union
		{
			float x;
			float pitch;
		};
		union
		{
			float y;
			float yaw;
		};
		union
		{
			float z;
			float roll;
		};
		union
		{
			float w;
		};
	};
	float values[4];
} vec4a_t;

vec3_t Vec_Zero();
vec3_t Vec_Uniform(float value);
vec3_t Vec(float x, float y, float z);
vec3_t Vec_Angles(const vec3_t forward); //johnfitz

vec3_t Vec_MulAdd(vec3_t veca, float scale, vec3_t vecb);

float Vec_Dot(vec3_t v1, vec3_t v2);
double Vec_DotDouble(vec3_t v1, vec3_t v2);
vec3_t Vec_Sub(vec3_t veca, vec3_t vecb);
vec3_t Vec_Add(vec3_t veca, vec3_t vecb);
vec3_t Vec_Mul(vec3_t veca, vec3_t vecb);
vec3_t Vec_Div(vec3_t veca, vec3_t vecb);

bool Vec_Equal(vec3_t v1, vec3_t v2);
float Vec_Length(vec3_t v);
vec3_t Vec_Cross(vec3_t v1, vec3_t v2);
float Vec_Normalize(vec3_t* v);
vec3_t Vec_Normalized(vec3_t v);
vec3_t Vec_Inverse(vec3_t v);
vec3_t Vec_Scale(vec3_t in, float scale);
int Q_log2(int val);

float Lerp(float a, float b, float ratio);
vec3_t Vec_Lerp(vec3_t start, vec3_t end, float percent);
vec3_t Vec_LerpAngles(vec3_t start, vec3_t end, float percent);

float Clamp(float value, float min, float max);
float Min(float a, float b);
float Max(float a, float b);
float Random();
float RandomRange(float min, float max);
vec3_t RandomInCube();

void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);

void FloorDivMod(double numer, double denom, int* quotient,
	int* rem);
fixed16_t Invert24To16(fixed16_t val);
int GreatestCommonDivisor(int i1, int i2);

void Vec_AngleVectors(vec3_t angles, vec3_t* forward, vec3_t* right, vec3_t* up);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct mplane_s* plane);
float AngleMod(float a);

//johnfitz -- courtesy of lordhavoc
// QuakeSpasm: To avoid strict aliasing violations, use a float/int union instead of type punning.
#define Vec_NormalizeFast(_v)\
{\
	union { float f; int i; } _y, _number;\
	_number.f = Vec_Dot(_v, _v);\
	if (_number.f != 0.0)\
	{\
		_y.i = 0x5f3759df - (_number.i >> 1);\
		_y.f = _y.f * (1.5f - (_number.f * 0.5f * _y.f * _y.f));\
		_v = Vec_Scale(_v, _y.f);\
	}\
}

#define BOX_ON_PLANE_SIDE(emins, emaxs, p) \
	(((p)->type < 3)? \
	( \
		((p)->dist <= (emins).values[(p)->type])? \
			1 \
		: \
		( \
			((p)->dist >= (emaxs).values[(p)->type])?\
				2 \
			: \
				3 \
		) \
	) \
	: \
		BoxOnPlaneSide( (emins), (emaxs), (p)))



