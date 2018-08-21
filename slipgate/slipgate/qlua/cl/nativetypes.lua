
ffi.cdef [[

typedef char byte;

typedef enum {
	ca_dedicated,
	ca_disconnected,
	ca_connected
} CActive;

typedef struct SizeBuf_s
{
	int allowoverflow; // if false, do a Sys_Error
	int overflowed; // set to true if the buffer size failed
	byte* data;
	int maxsize;
	int cursize;
} SizeBuf;

typedef struct CShift_s
{
	int destcolor[3];
	int percent; // 0-256
} CShift;

typedef struct UserCMD_s
{
	Vec3 viewangles;

	// intended velocities
	float forwardmove;
	float sidemove;
	float upmove;
} UserCMD;

typedef struct CacheUser_s
{
	void* data;
} CacheUser;

typedef struct Hull_s
{
	struct mclipnode_s* clipnodes;
	struct mplane_s* planes;
	int firstclipnode;
	int lastclipnode;
	Vec3 clip_mins;
	Vec3 clip_maxs;
} Hull;

typedef enum { mod_brush, mod_sprite, mod_alias } ModType;
typedef enum { ST_SYNC = 0, ST_RAND } SyncType;
typedef struct QModel_s
{
	char name[64];
	unsigned int path_id;
	int needload;

	ModType type;
	int numframes;
	SyncType synctype;

	int flags;

	Vec3 mins, maxs;
	Vec3 ymins, ymaxs;
	Vec3 rmins, rmaxs;
	int clipbox;
	Vec3 clipmins, clipmaxs;
	int firstmodelsurface, nummodelsurfaces;
	int numsubmodels;
	struct dmodel_s* submodels;

	int numplanes;
	struct mplane_s* planes;

	int numleafs;
	struct mleaf_s* leafs;

	int numvertexes;
	struct mvertex_s* vertexes;

	int numedges;
	struct medge_s* edges;

	int numnodes;
	struct mnode_s* nodes;

	int numtexinfo;
	struct mtexinfo_s* texinfo;

	int numsurfaces;
	struct msurface_s* surfaces;

	int numsurfedges;
	int* surfedges;

	int numclipnodes;
	struct mclipnode_s* clipnodes;

	int nummarksurfaces;
	struct msurface_s **marksurfaces;

	Hull hulls[4];

	int numtextures;
	struct texture_s **textures;

	char* visdata;
	char* lightdata;
	char* entities;

	int bspversion;

	int meshvbo;
	int meshindexesvbo;
	int vboindexofs;
	int vboxyzofs;
	int vbostofs;
	CacheUser cache;
} QModel;

typedef struct Entity_s
{
	int forcelink; // model changed

	int update_type;

	EntityState baseline; // to fill in defaults in updates

	double msgtime; // time of last update
	Vec3 msg_origins[2]; // last two updates (0 is newest)
	Vec3 origin;
	Vec3 msg_angles[2]; // last two updates (0 is newest)
	Vec3 angles;
	Vec3 velocity;
	Vec3 mins;
	Vec3 maxs;
	int solid;
	QModel* model; // NULL = no model
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
	Vec3 previousorigin; //johnfitz -- transform lerping
	Vec3 currentorigin; //johnfitz -- transform lerping
	Vec3 previousangles; //johnfitz -- transform lerping
	Vec3 currentangles; //johnfitz -- transform lerping

	//Used for smooth environment light transitions.
	Vec3 previousenvlightcolor;
	int id;
} Entity;

typedef struct CLTrace_s
{
	int allsolid;
	int startsolid;
	int inopen, inwater;
	float fraction;
	Vec3 endpos;
	Plane plane;
	Entity* ent;
} CLTrace;

typedef struct
{
	CActive state;
	char spawnparms[2048];

	int demonum;
	char demos[8][16];

	int demorecording;
	int demoplayback;

	int demopaused;

	int timedemo;
	int forcetrack;
	void* demofile;
	int td_lastframe;
	int td_startframe;
	float td_starttime;
	int signon;
	struct qsocket_s* netcon;
	SizeBuf message;

} ClientStatic;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct
{
int movemessages;
UserCMD cmd;

int stats[32];
int items;
float item_gettime[32];
float faceanimtime;

CShift cshifts[4];
CShift prev_cshifts[4];

Vec3 mviewangles[2];
Vec3 viewangles;

Vec3 mvelocity[2];
Vec3 velocity;

Vec3 punchangle;

float idealpitch;
float pitchvel;
int nodrift;
float driftmove;
double laststop;

float viewheight;
float crouch;

int paused;
int onground;
int inwater;

int intermission;
int completed_time;

double mtime[2];
double time;
double oldtime;
double deltatime;

float last_received_message;

QModel* model_precache[2048];
void* sound_precache[2048];

char mapname[128];
char levelname[128];
int entityindex;
int maxclients;
int gametype;

QModel* worldmodel;
void* free_efrags;
int num_efrags;
int num_entities;
int num_statics;
Entity viewent;
int cdtrack, looptrack;

void* scores;

unsigned protocol;
unsigned protocolflags;
} ClientState;

typedef enum
{
	pet_box,
	pet_sphere,
	pet_line
} ParticleEmitType;

typedef struct particleemitter_s
{
	float ticker;

	float rate;

	ParticleEmitType emittype;

	Vec3 origin;
	Vec3 originoffset;
	Vec3 trailend;

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

	Vec3 velocity;
	Vec3 randomvelocity;

	float mindrag;
	float maxdrag;

	float mingravityscale;
	float maxgravityscale;

	float sizevariation;

	void(*tick)(void*, float);

} ParticleEmitter;

typedef struct
{
	Vec3 origin;
	float radius;
	float die;
	float decay;
	float minlight;
	int key;
	Vec3 color;
	float splatopacity;
	int issplat;
} DLight;
]]

ClientStatic = ffi.typeof("ClientStatic")
ClientState = ffi.typeof("ClientState")
ParticleEmitter = ffi.typeof("ParticleEmitter")

local entitybags = {}
local entityreplicatedbags = {}

function GetBagForEntityID(id)
	if id == 0 then
		error("trying to get bag for id 0", 2)
	end
	local bag = entitybags[id]
	if bag == nil then
		bag = {}
		entitybags[id] = bag
	end
	return bag
end

Entity = ffi.typeof("Entity")
Entity_mt = {
	__eq = function(self, rhs)
		if rhs == nil then
			return false
		end
		if type(rhs) == "cdata" then
			local cdatatype = tostring(ffi.typeof(rhs))
			if string.starts(cdatatype, "ctype<struct Entity") then
				return self.id == rhs.id
			end
		end
        return false
    end,
	__index = function (table, key)
		if key == "bag" then
			return GetBagForEntityID(table.id)
		end
		if key == "rep" then
			local id = table.id
			local bag = entityreplicatedbags[id]
			if bag == nil then
				bag = {}
				entityreplicatedbags[id] = bag
			end
			return bag
		end
		return nil
	end
}

ffi.metatype(Entity, Entity_mt)
