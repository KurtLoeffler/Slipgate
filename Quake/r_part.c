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

#include "quakedef.h"

#define MAX_PARTICLES 16384 // default max # of particles at one
//  time
#define ABSOLUTE_MIN_PARTICLES 512 // no fewer than this no matter what's
										//  on the command line

int ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
int ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
int ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

particle_t* active_particles, *free_particles, *particles;

vec3_t r_pright, r_pup, r_ppn;

int r_numparticles;

gltexture_t* particletexture, *particletexture1, *particletexture2, *particletexture3, *particletexture4; //johnfitz
float texturescalefactor; //johnfitz -- compensate for apparent size of different particle textures

cvar_t r_particles = { "r_particles","1", CVAR_ARCHIVE }; //johnfitz
cvar_t r_quadparticles = { "r_quadparticles","1", CVAR_ARCHIVE }; //johnfitz

/*
===============
R_ParticleTextureLookup -- johnfitz -- generate nice antialiased 32x32 circle for particles
===============
*/
int R_ParticleTextureLookup(int x, int y, int sharpness)
{
	int r; //distance from point x,y to circle origin, squared
	int a; //alpha value to return

	x -= 16;
	y -= 16;
	r = x * x + y * y;
	r = r > 255 ? 255 : r;
	a = sharpness * (255 - r);
	a = Min(a, 255);
	return a;
}

/*
===============
R_InitParticleTextures -- johnfitz -- rewritten
===============
*/
void R_InitParticleTextures(void)
{
	int x, y;
	static byte particle1_data[64*64*4];
	static byte particle2_data[2*2*4];
	static byte particle3_data[64*64*4];
	byte* dst;

	// particle texture 1 -- circle
	dst = particle1_data;
	for (x = 0; x<64; x++)
		for (y = 0; y<64; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = R_ParticleTextureLookup(x, y, 8);
		}
	particletexture1 = TexMgr_LoadImage(NULL, "particle1", 64, 64, SRC_RGBA, particle1_data, "", (src_offset_t)particle1_data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_LINEAR);

	// particle texture 2 -- square
	dst = particle2_data;
	for (x = 0; x<2; x++)
		for (y = 0; y<2; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
		}
	particletexture2 = TexMgr_LoadImage(NULL, "particle2", 2, 2, SRC_RGBA, particle2_data, "", (src_offset_t)particle2_data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_NEAREST);

	// particle texture 3 -- blob
	dst = particle3_data;
	for (x = 0; x<64; x++)
		for (y = 0; y<64; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = R_ParticleTextureLookup(x, y, 2);
		}
	particletexture3 = TexMgr_LoadImage(NULL, "particle3", 64, 64, SRC_RGBA, particle3_data, "", (src_offset_t)particle3_data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_LINEAR);


	const char* particleshapename = "particleshape";
	int width, height;
	byte* bytes = Image_LoadImage(particleshapename, &width, &height);
	if (bytes)
	{
		particletexture4 = TexMgr_LoadImage(NULL, particleshapename, width, height, SRC_RGBA, bytes, particleshapename, 0, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_NEAREST);
	}

	//set default
	particletexture = particletexture1;
	texturescalefactor = 1.27;
}

/*
===============
R_SetParticleTexture_f -- johnfitz
===============
*/
static void R_SetParticleTexture_f(cvar_t* var)
{
	switch ((int)(r_particles.value))
	{
	case 1:
		particletexture = particletexture1;
		texturescalefactor = 1.27;
		break;
	case 2:
		particletexture = particletexture2;
		texturescalefactor = 1.0;
		break;
	// case 3:
	// particletexture = particletexture3;
	// texturescalefactor = 1.5;
	// break;
	case 4:
		particletexture = particletexture4;
		texturescalefactor = 1.75;
		break;
	}
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles(void)
{
	int i;

	i = COM_CheckParm("-particles");

	if (i)
	{
		r_numparticles = (int)(Q_atoi(com_argv[i+1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}

	particles = (particle_t *)
		Hunk_AllocName(r_numparticles * sizeof(particle_t), "particles");

	Cvar_RegisterVariable(&r_particles); //johnfitz
	Cvar_SetCallback(&r_particles, R_SetParticleTexture_f);
	Cvar_RegisterVariable(&r_quadparticles); //johnfitz

	R_InitParticleTextures(); //johnfitz
}

particle_t* AllocParticle()
{
	if (!free_particles)
		return NULL;
	particle_t* p = free_particles;
	free_particles = p->next;
	memset(p, 0, sizeof(particle_t));
	p->next = active_particles;
	active_particles = p;

	p->size = 1;
	p->sizevariation = 0.25;
	return p;
}

void ExplodeParticleTick(particle_t* p, float frametime)
{
	p->vel = Vec_Add(p->vel, Vec_Scale(p->vel, frametime*4));
}

void Explode2ParticleTick(particle_t* p, float frametime)
{
	p->vel = Vec_Sub(p->vel, Vec_Scale(p->vel, frametime));
}

void BlobParticleTick(particle_t* p, float frametime)
{
	p->vel = Vec_Add(p->vel, Vec_Scale(p->vel, frametime*4));
}

void Blob2ParticleTick(particle_t* p, float frametime)
{
	p->vel = Vec_Sub(p->vel, Vec_Scale(p->vel, frametime*4));
}

/*
===============
R_EntityParticles
===============
*/
#define NUMVERTEXNORMALS 162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t avelocities[NUMVERTEXNORMALS];
float beamlength = 16;
vec3_t avelocity = { 23, 7, 3 };
float partstep = 0.01;
float timescale = 0.01;

void R_EntityParticles(entity_t* ent)
{
	int i;
	particle_t* p;
	float angle;
	float sp, sy, cp, cy;
	// float sr, cr;
	// int count;
	vec3_t forward;
	float dist;

	dist = 64;
	// count = 50;

	if (!avelocities[0].x)
	{
		for (i = 0; i < NUMVERTEXNORMALS; i++)
		{
			avelocities[i].x = (rand() & 255) * 0.01;
			avelocities[i].y = (rand() & 255) * 0.01;
			avelocities[i].z = (rand() & 255) * 0.01;
		}
	}

	for (i = 0; i < NUMVERTEXNORMALS; i++)
	{
		angle = cl.time * avelocities[i].x;
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i].y;
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i].z;
		// sr = sin(angle);
		// cr = cos(angle);

		forward.x = cp*cy;
		forward.y = cp*sy;
		forward.z = -sp;

		p = AllocParticle();
		if (!p)
			return;

		p->die = cl.time + 0.01;
		p->color = 0x6f;

		p->tick = ExplodeParticleTick;
		p->ramprate = 10;
		p->ramplut = ramp1;
		p->maxramp = 8;

		p->org.x = ent->origin.x + r_avertexnormals[i][0]*dist + forward.x*beamlength;
		p->org.y = ent->origin.y + r_avertexnormals[i][1]*dist + forward.y*beamlength;
		p->org.z = ent->origin.z + r_avertexnormals[i][2]*dist + forward.z*beamlength;
	}
}

/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles(void)
{
	int i;

	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0; i<r_numparticles; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles-1].next = NULL;
}

/*
===============
R_ReadPointFile_f
===============
*/
void R_ReadPointFile_f(void)
{
	FILE* f;
	vec3_t org;
	int r;
	int c;
	particle_t* p;
	char name[MAX_QPATH];

	if (cls.state != ca_connected)
		return; // need an active map.

	q_snprintf(name, sizeof(name), "maps/%s.pts", cl.mapname);

	COM_FOpenFile(name, &f, NULL);
	if (!f)
	{
		Con_Printf("couldn't open %s\n", name);
		return;
	}

	Con_Printf("Reading %s...\n", name);
	c = 0;
	org.x = org.y = org.z = 0; // silence pesky compiler warnings
	for (;; )
	{
		r = fscanf(f, "%f %f %f\n", &org.x, &org.y, &org.z);
		if (r != 3)
			break;
		c++;

		p = AllocParticle();
		if (!p)
		{
			Con_Printf("Not enough free particles\n");
			return;
		}

		p->die = 99999;
		p->color = (-c)&15;
		p->gravityscale = 0;
		p->vel = Vec_Zero();
		p->org = org;
	}

	fclose(f);
	Con_Printf("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect(void)
{
	vec3_t org, dir;
	int i, count, msgcount, color;

	for (i = 0; i<3; i++)
		org.values[i] = MSG_ReadCoord(cl.protocolflags);
	for (i = 0; i<3; i++)
		dir.values[i] = MSG_ReadChar() * (1.0/16);
	msgcount = MSG_ReadByte();
	color = MSG_ReadByte();

	if (msgcount == 255)
		count = 1024;
	else
		count = msgcount;

	R_RunParticleEffect(org, dir, color, count);
}

/*
===============
R_ParticleExplosion
===============
*/
void R_ParticleExplosion(vec3_t org)
{
	int i, j;
	particle_t* p;

	for (i = 0; i<1024; i++)
	{
		p = AllocParticle();
		if (!p)
		{
			return;
		}

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand()&3;
		if (i & 1)
		{
			p->tick = ExplodeParticleTick;
			p->ramprate = 10;
			p->ramplut = ramp1;
			p->maxramp = 8;
			for (j = 0; j<3; j++)
			{
				p->org.values[j] = org.values[j] + ((rand()%32)-16);
				p->vel.values[j] = (rand()%512)-256;
			}
		}
		else
		{
			p->tick = Explode2ParticleTick;
			p->ramprate = 15;
			p->ramplut = ramp2;
			p->maxramp = 8;
			for (j = 0; j<3; j++)
			{
				p->org.values[j] = org.values[j] + ((rand()%32)-16);
				p->vel.values[j] = (rand()%512)-256;
			}
		}
	}
}

/*
===============
R_ParticleExplosion2
===============
*/
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength)
{
	int i, j;
	particle_t* p;
	int colorMod = 0;

	for (i = 0; i<512; i++)
	{
		p = AllocParticle();
		if (!p)
		{
			return;
		}

		p->die = cl.time + 0.3;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->tick = BlobParticleTick;
		for (j = 0; j<3; j++)
		{
			p->org.values[j] = org.values[j] + ((rand()%32)-16);
			p->vel.values[j] = (rand()%512)-256;
		}
	}
}

/*
===============
R_BlobExplosion
===============
*/
void R_BlobExplosion(vec3_t org)
{
	int i, j;
	particle_t* p;

	for (i = 0; i<1024; i++)
	{
		p = AllocParticle();
		if (!p)
		{
			return;
		}

		p->die = cl.time + 1 + (rand()&8)*0.05;

		if (i & 1)
		{
			p->color = 66 + rand()%6;
			p->tick = BlobParticleTick;
			for (j = 0; j<3; j++)
			{
				p->org.values[j] = org.values[j] + ((rand()%32)-16);
				p->vel.values[j] = (rand()%512)-256;
			}
		}
		else
		{
			p->color = 150 + rand()%6;
			p->tick = Blob2ParticleTick;
			for (j = 0; j<3; j++)
			{
				p->org.values[j] = org.values[j] + ((rand()%32)-16);
				p->vel.values[j] = (rand()%512)-256;
			}
		}
	}
}

/*
===============
R_RunParticleEffect
===============
*/
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
	int i, j;
	particle_t* p;

	for (i = 0; i<count; i++)
	{
		p = AllocParticle();
		if (!p)
		{
			return;
		}

		if (count == 1024)
		{ // rocket explosion
			p->die = cl.time + 5;
			p->color = ramp1[0];
			p->ramp = rand()&3;
			if (i & 1)
			{
				p->tick = ExplodeParticleTick;
				p->ramprate = 10;
				p->ramplut = ramp1;
				p->maxramp = 8;
				for (j = 0; j<3; j++)
				{
					p->org.values[j] = org.values[j] + ((rand()%32)-16);
					p->vel.values[j] = (rand()%512)-256;
				}
			}
			else
			{
				p->tick = Explode2ParticleTick;
				p->ramprate = 15;
				p->ramplut = ramp2;
				p->maxramp = 8;
				for (j = 0; j<3; j++)
				{
					p->org.values[j] = org.values[j] + ((rand()%32)-16);
					p->vel.values[j] = (rand()%512)-256;
				}
			}
		}
		else
		{
			p->die = cl.time + 0.1*(rand()%5);
			p->color = (color&~7) + (rand()&7);
			p->gravityscale = 1;
			for (j = 0; j<3; j++)
			{
				p->org.values[j] = org.values[j] + ((rand()&15)-8);
				p->vel.values[j] = dir.values[j]*15;// + (rand()%300)-150;
			}
		}
	}
}

/*
===============
R_LavaSplash
===============
*/
void R_LavaSplash(vec3_t org)
{
	int i, j, k;
	particle_t* p;
	float vel;
	vec3_t dir;

	for (i = -16; i<16; i++)
		for (j = -16; j<16; j++)
			for (k = 0; k<1; k++)
			{
				p = AllocParticle();
				if (!p)
				{
					return;
				}

				p->die = cl.time + 2 + (rand()&31) * 0.02;
				p->color = 224 + (rand()&7);
				p->gravityscale = 1;

				dir.x = j*8 + (rand()&7);
				dir.y = i*8 + (rand()&7);
				dir.z = 256;

				p->org.x = org.x + dir.x;
				p->org.y = org.y + dir.y;
				p->org.z = org.z + (rand()&63);

				Vec_Normalize(&dir);
				vel = 50 + (rand()&63);
				p->vel = Vec_Scale(dir, vel);
			}
}

/*
===============
R_TeleportSplash
===============
*/
void R_TeleportSplash(vec3_t org)
{
	int i, j, k;
	particle_t* p;
	float vel;
	vec3_t dir;

	for (i = -16; i<16; i += 4)
		for (j = -16; j<16; j += 4)
			for (k = -24; k<32; k += 4)
			{
				p = AllocParticle();
				if (!p)
				{
					return;
				}

				p->die = cl.time + 0.2 + (rand()&7) * 0.02;
				p->color = 7 + (rand()&7);
				p->gravityscale = 1;

				dir.x = j*8;
				dir.y = i*8;
				dir.z = k*8;

				p->org.x = org.x + i + (rand()&3);
				p->org.y = org.y + j + (rand()&3);
				p->org.z = org.z + k + (rand()&3);

				Vec_Normalize(&dir);
				vel = 50 + (rand()&63);
				p->vel = Vec_Scale(dir, vel);
			}
}

/*
===============
R_RocketTrail

FIXME -- rename function and use #defined types instead of numbers
===============
*/
void R_RocketTrail(vec3_t start, vec3_t end, int type)
{
	vec3_t vec;
	float len;
	int j;
	particle_t* p;
	float dec;
	static int tracercount;

	vec = Vec_Sub(end, start);
	len = Vec_Normalize(&vec);
	if (type < 128)
		dec = 3;
	else
	{
		dec = 1;
		type -= 128;
	}

	//More gore.
	if (type == 4 || type == 2)
	{
		dec /= 3;
	}

	//Make it more framerate independent.
	if (len <= dec)
	{
		if (Random()*0.0075 > host_frametime)
		{
			return;
		}
	}

	while (len > 0)
	{
		len -= dec;

		p = AllocParticle();
		if (!p)
		{
			return;
		}

		p->die = cl.time + 2;

		switch (type)
		{
		case 0: // rocket trail
			p->ramp = (rand()&3);
			p->color = ramp3[(int)p->ramp];
			p->gravityscale = -0.5+(Random()-0.5)*0.5;
			p->drag = 2.5;
			p->ramprate = 5;
			p->ramplut = ramp3;
			p->maxramp = 6;
			for (j = 0; j<3; j++)
			{
				p->org.values[j] = start.values[j] + ((rand()%6)-3);
				p->vel.values[j] = ((rand()%30)-15);
			}
			break;

		case 1: // smoke smoke
			p->ramp = (rand()&3) + 2;
			p->color = ramp3[(int)p->ramp];
			p->gravityscale = -1;
			p->ramprate = 5;
			p->ramplut = ramp3;
			p->maxramp = 6;
			for (j = 0; j<3; j++)
				p->org.values[j] = start.values[j] + ((rand()%6)-3);
			break;
			
		case 4:
		case 2: // blood
			p->color = 67 + (rand()&3);
			p->gravityscale = 1+Random()*2;
			p->die = cl.time + (1.5+Random())*(Random() > 0.5 ? 0.5 : 1);
			for (j = 0; j<3; j++)
			{
				p->org.values[j] = start.values[j] + ((rand()%12)-6);
				p->vel.values[j] = ((rand()%24)-12);
			}
			p->sizevariation = 0.5;
			if (Random() > 0.9)
			{
				p->size = 2+Random()*1;
				p->gravityscale *= p->size;
			}
			p->drag = 1.0/(p->size*2);
			break;

		case 3:
		case 5: // tracer
			p->die = cl.time + 0.5;
			p->gravityscale = 0;
			if (type == 3)
				p->color = 52 + ((tracercount&4)<<1);
			else
				p->color = 230 + ((tracercount&4)<<1);

			tracercount++;

			p->org = start;
			if (tracercount & 1)
			{
				p->vel.x = 30*vec.y;
				p->vel.y = 30*-vec.x;
			}
			else
			{
				p->vel.x = 30*-vec.y;
				p->vel.y = 30*vec.x;
			}
			break;
		case 6: // voor trail
			p->color = 9*16 + 8 + (rand()&3);
			p->gravityscale = 0;
			p->die = cl.time + 0.3;
			for (j = 0; j<3; j++)
				p->org.values[j] = start.values[j] + ((rand()&15)-8);
			break;
		}

		start = Vec_Add(start, vec);
	}
}

/*
===============
CL_RunParticles -- johnfitz -- all the particle behavior, separated from R_DrawParticles
===============
*/
void CL_RunParticles(void)
{
	particle_t* p, *kill;
	float time1, time2, time3, dvel, frametime, grav;
	extern cvar_t sv_gravity;

	frametime = cl.deltatime;
	time3 = frametime * 15;
	time2 = frametime * 10;
	time1 = frametime * 5;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4*frametime;

	for (;; )
	{
		kill = active_particles;
		if (kill && kill->die < cl.time)
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p = active_particles; p; p = p->next)
	{
		for (;; )
		{
			kill = p->next;
			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

		p->org = Vec_MulAdd(p->org, frametime, p->vel);
		p->vel = Vec_Scale(p->vel, 1.0/(1.0+p->drag*frametime));

		if (p->gravityscale)
		{
			p->vel.z -= grav*p->gravityscale;
		}

		if (p->ramplut)
		{
			p->ramp += p->ramprate*frametime;
			if (p->maxramp > 0 && p->ramp >= p->maxramp)
			{
				p->die = -1;
			}
			else
			{
				p->color = p->ramplut[(int)p->ramp];
			}
		}

		if (p->tick)
		{
			p->tick(p, frametime);
			continue;
		}
		/*
		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = -1;
			else
				p->color = ramp3[(int)p->ramp];
			break;

		case pt_explode:
			p->ramp += time2;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp1[(int)p->ramp];
			for (i = 0; i<3; i++)
				p->vel.values[i] += p->vel.values[i]*frametime*4;
			break;

		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp2[(int)p->ramp];
			for (i = 0; i<3; i++)
				p->vel.values[i] -= p->vel.values[i]*frametime;
			break;

		case pt_blob:
			for (i = 0; i<3; i++)
				p->vel.values[i] += p->vel.values[i]*frametime*4;
			break;

		case pt_blob2:
			for (i = 0; i<2; i++)
				p->vel.values[i] -= p->vel.values[i]*frametime*4;
			break;
			break;
		}
		*/
	}
}

/*
===============
R_DrawParticles -- johnfitz -- moved all non-drawing code to CL_RunParticles
===============
*/
void R_DrawParticles(void)
{
	particle_t* p;
	float scale;
	vec3_t up, right, p_up, p_right, p_upright; //johnfitz -- p_ vectors
	GLubyte color[4], *c; //johnfitz -- particle transparency
	extern cvar_t r_particles; //johnfitz
	//float alpha; //johnfitz -- particle transparency

	if (!r_particles.value)
		return;

	//ericw -- avoid empty glBegin(),glEnd() pair below; causes issues on AMD
	if (!active_particles)
		return;

	up = Vec_Scale(vup, 1.5);
	right = Vec_Scale(vright, 1.5);

	GL_Bind(particletexture);
	glEnable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(GL_FALSE); //johnfitz -- fix for particle z-buffer bug

	if (r_quadparticles.value) //johnitz -- quads save fillrate
	{
		glBegin(GL_QUADS);
		for (p = active_particles; p; p = p->next)
		{
			// hack a scale up to keep particles from disapearing
			scale = (p->org.x - r_origin.x) * vpn.x
				+ (p->org.y - r_origin.y) * vpn.y
				+ (p->org.z - r_origin.z) * vpn.z;
			if (scale < 20)
				scale = 1 + 0.08; //johnfitz -- added .08 to be consistent
			else
				scale = 1 + scale * 0.004;

			scale /= 2.0; //quad is half the size of triangle

			scale *= texturescalefactor*p->size; //johnfitz -- compensate for apparent size of different particle textures
			float random = (((unsigned long)(int*)p)%(sizeof(particle_t)*8))/(sizeof(particle_t)*8.0);
			float randomscaler = (1.0-p->sizevariation)+random*p->sizevariation*2;
			scale *= randomscaler;
			//johnfitz -- particle transparency and fade out
			c = (GLubyte *)&d_8to24table[(int)p->color];
			color[0] = c[0];
			color[1] = c[1];
			color[2] = c[2];
			//alpha = CLAMP(0, p->die + 0.5 - cl.time, 1);
			color[3] = 255; //(int)(alpha * 255);
			glColor4ubv(color);
			//johnfitz

			vec3_t bottomvert = Vec_Sub(p->org, Vec_Scale(up, scale*0.5));
			bottomvert = Vec_Sub(bottomvert, Vec_Scale(right, scale*0.5));

			glTexCoord2f(0, 0);
			glVertex3fv(bottomvert.values);

			glTexCoord2f(1, 0);
			p_up = Vec_MulAdd(bottomvert, scale, up);
			glVertex3fv(p_up.values);

			glTexCoord2f(1, 1);
			p_upright = Vec_MulAdd(p_up, scale, right);
			glVertex3fv(p_upright.values);

			glTexCoord2f(0, 1);
			p_right = Vec_MulAdd(bottomvert, scale, right);
			glVertex3fv(p_right.values);

			rs_particles++; //johnfitz //FIXME: just use r_numparticles
		}
		glEnd();
	}
	else //johnitz --  triangles save verts
	{
		glBegin(GL_TRIANGLES);
		for (p = active_particles; p; p = p->next)
		{
			// hack a scale up to keep particles from disapearing
			scale = (p->org.x - r_origin.x) * vpn.x
				+ (p->org.y - r_origin.y) * vpn.y
				+ (p->org.z - r_origin.z) * vpn.z;
			if (scale < 20)
				scale = 1 + 0.08; //johnfitz -- added .08 to be consistent
			else
				scale = 1 + scale * 0.004;

			scale *= texturescalefactor*p->size; //johnfitz -- compensate for apparent size of different particle textures

			//johnfitz -- particle transparency and fade out
			c = (GLubyte *)&d_8to24table[(int)p->color];
			color[0] = c[0];
			color[1] = c[1];
			color[2] = c[2];
			//alpha = CLAMP(0, p->die + 0.5 - cl.time, 1);
			color[3] = 255; //(int)(alpha * 255);
			glColor4ubv(color);
			//johnfitz

			glTexCoord2f(0, 0);
			glVertex3fv(p->org.values);

			glTexCoord2f(1, 0);
			p_up = Vec_MulAdd(p->org, scale, up);
			glVertex3fv(p_up.values);

			glTexCoord2f(0, 1);
			p_right = Vec_MulAdd(p->org, scale, right);
			glVertex3fv(p_right.values);

			rs_particles++; //johnfitz //FIXME: just use r_numparticles
		}
		glEnd();
	}

	glDepthMask(GL_TRUE); //johnfitz -- fix for particle z-buffer bug
	glDisable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor3f(1, 1, 1);
}


/*
===============
R_DrawParticles_ShowTris -- johnfitz
===============
*/
void R_DrawParticles_ShowTris(void)
{
	particle_t* p;
	float scale;
	vec3_t up, right, p_up, p_right, p_upright;
	extern cvar_t r_particles;

	if (!r_particles.value)
		return;

	up = Vec_Scale(vup, 1.5);
	right = Vec_Scale(vright, 1.5);

	if (r_quadparticles.value)
	{
		for (p = active_particles; p; p = p->next)
		{
			glBegin(GL_TRIANGLE_FAN);

			// hack a scale up to keep particles from disapearing
			scale = (p->org.x - r_origin.x) * vpn.x
				+ (p->org.y - r_origin.y) * vpn.y
				+ (p->org.z - r_origin.z) * vpn.z;
			if (scale < 20)
				scale = 1 + 0.08; //johnfitz -- added .08 to be consistent
			else
				scale = 1 + scale * 0.004;

			scale /= 2.0; //quad is half the size of triangle

			scale *= texturescalefactor; //compensate for apparent size of different particle textures

			glVertex3fv(p->org.values);

			p_up = Vec_MulAdd(p->org, scale, up);
			glVertex3fv(p_up.values);

			p_upright = Vec_MulAdd(p_up, scale, right);
			glVertex3fv(p_upright.values);

			p_right = Vec_MulAdd(p->org, scale, right);
			glVertex3fv(p_right.values);

			glEnd();
		}
	}
	else
	{
		glBegin(GL_TRIANGLES);
		for (p = active_particles; p; p = p->next)
		{
			// hack a scale up to keep particles from disapearing
			scale = (p->org.x - r_origin.x) * vpn.x
				+ (p->org.y - r_origin.y) * vpn.y
				+ (p->org.z - r_origin.z) * vpn.z;
			if (scale < 20)
				scale = 1 + 0.08; //johnfitz -- added .08 to be consistent
			else
				scale = 1 + scale * 0.004;

			scale *= texturescalefactor; //compensate for apparent size of different particle textures

			glVertex3fv(p->org.values);

			p_up = Vec_MulAdd(p->org, scale, up);
			glVertex3fv(p_up.values);

			p_right = Vec_MulAdd(p->org, scale, right);
			glVertex3fv(p_right.values);
		}
		glEnd();
	}
}

void R_InitParticleEmitter(particleemitter_t* e)
{
	memset(e, 0, sizeof(particleemitter_t));
	e->rate = 10;
	e->minlifespan = 1;
	e->maxlifespan = 1;
	e->color = 15;
	e->minsize = 1;
	e->maxsize = 1;

}

void R_RunParticleEmitter(particleemitter_t* e)
{
	if (e->rate <= 0)
	{
		return;
	}

	float dec = 1;
	dec /= e->rate;

	vec3_t vec = Vec_Sub(e->trailend, e->origin);
	float len = Vec_Normalize(&vec);

	float ticker = e->ticker;

	if (e->emittype == pet_trail)
	{
		ticker = len;
		//Make it more framerate independent.
		if (len <= dec)
		{
			if (Random()*0.0075 > host_frametime)
			{
				return;
			}
		}
	}

	while (ticker > 0)
	{
		particle_t* p = AllocParticle();
		if (!p)
		{
			break;
		}

		vec3_t emitpos = e->origin;
		if (e->emittype == pet_trail)
		{
			emitpos = Vec_Lerp(e->origin, e->trailend, ticker/len);
		}

		p->org = Vec_Add(emitpos, Vec_Mul(RandomInCube(), e->originoffset));

		p->die = cl.time+RandomRange(e->minlifespan, e->maxlifespan);

		p->color = e->color;
		if (e->randomcolorcount > 0)
		{
			p->color = e->randomcolors[(int)(Random()*e->randomcolorcount)];
		}

		p->ramp = e->ramp;
		p->ramprate = RandomRange(e->minramprate, e->maxramprate);
		p->maxramp = RandomRange(e->minmaxramp, e->maxmaxramp);
		p->ramplut = e->ramplut;

		p->size = RandomRange(e->minsize, e->maxsize);
		p->vel = Vec_Add(e->velocity, Vec_Mul(RandomInCube(), e->randomvelocity));
		p->drag = RandomRange(e->mindrag, e->maxdrag);

		p->gravityscale = RandomRange(e->mingravityscale, e->maxgravityscale);
		p->sizevariation = e->sizevariation;
		p->tick = e->tick;

		ticker -= dec;
	}

	if (e->emittype != pet_trail)
	{
		e->ticker = ticker;
		e->ticker += cl.deltatime;
	}
}
