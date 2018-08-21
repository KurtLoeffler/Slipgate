#pragma once


// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
	// driver-usable fields
	vec3_t org;
	float color;
	// drivers never touch the following fields
	struct particle_s* next;
	vec3_t vel;
	float ramp;
	float die;
	//ptype_t type;
	float drag;
	float gravityscale;
	float size;
	float sizevariation;
	float ramprate;
	float maxramp;
	int* ramplut;
	void(*tick)(void*, float);
} particle_t;


typedef enum
{
	pet_box,
	pet_sphere,
	pet_trail
} particleemittype;

typedef struct particleemitter_s
{
	float ticker;

	float rate;

	particleemittype emittype;

	vec3_t origin;
	vec3_t originoffset;
	vec3_t trailend;

	float minlifespan;
	float maxlifespan;

	byte color;

	byte randomcolors[16];
	int randomcolorcount;

	float ramp;
	float minramprate;
	float maxramprate;
	float minmaxramp;
	float maxmaxramp;
	int* ramplut;

	float minsize;
	float maxsize;

	vec3_t velocity;
	vec3_t randomvelocity;

	float mindrag;
	float maxdrag;

	float mingravityscale;
	float maxgravityscale;

	float sizevariation;

	void(*tick)(void*, float);

} particleemitter_t;

void R_ParseParticleEffect(void);
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail(vec3_t start, vec3_t end, int type);
void R_EntityParticles(entity_t* ent);
void R_BlobExplosion(vec3_t org);
void R_ParticleExplosion(vec3_t org);
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength);
void R_LavaSplash(vec3_t org);
void R_TeleportSplash(vec3_t org);
void R_InitParticleEmitter(particleemitter_t* e);
void R_RunParticleEmitter(particleemitter_t* e);
