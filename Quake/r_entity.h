#pragma once

typedef struct
{
	vec3_t origin;
	vec3_t angles;
	unsigned short  modelindex; //johnfitz -- was int
	unsigned short  frame; //johnfitz -- was int
	unsigned char  colormap; //johnfitz -- was int
	unsigned char  skin; //johnfitz -- was int
	unsigned char alpha; //johnfitz -- added
	int effects;
} entity_state_t;

typedef struct efrag_s
{
	struct efrag_s* leafnext;
	struct entity_s* entity;
} efrag_t;

typedef struct entity_s
{
	bool forcelink; // model changed

	int update_type;

	entity_state_t baseline; // to fill in defaults in updates

	double msgtime; // time of last update
	vec3_t msg_origins[2]; // last two updates (0 is newest)
	vec3_t origin;
	vec3_t msg_angles[2]; // last two updates (0 is newest)
	vec3_t angles;
	vec3_t velocity;
	vec3_t mins;
	vec3_t maxs;
	int solid;
	struct qmodel_s* model; // NULL = no model
	struct efrag_s* efrag; // linked list of efrags
	int frame;
	float syncbase; // for client-side animations
	byte* colormap;
	int effects; // light, particles, etc
	int skinnum; // for Alias models
	int visframe; // last frame this entity was found in an active leaf

	int dlightframe; // dynamic lighting
	int dlightbits;

	// FIXME: could turn these into a union
	int trivial_accept;
	struct mnode_s* topnode; // for bmodels, first world node that splits bmodel, or NULL if not split

	byte alpha; //johnfitz -- alpha
	byte lerpflags; //johnfitz -- lerping
	float lerpstart; //johnfitz -- animation lerping
	float lerptime; //johnfitz -- animation lerping
	float lerpfinish; //johnfitz -- lerping -- server sent us a more accurate interval, use it instead of 0.1
	short previouspose; //johnfitz -- animation lerping
	short currentpose; //johnfitz -- animation lerping
	float movelerpstart; //johnfitz -- transform lerping
	vec3_t previousorigin; //johnfitz -- transform lerping
	vec3_t currentorigin; //johnfitz -- transform lerping
	vec3_t previousangles; //johnfitz -- transform lerping
	vec3_t currentangles; //johnfitz -- transform lerping

	//Used for smooth environment light transitions.
	vec3_t previousenvlightcolor;
	int id;
} entity_t;
