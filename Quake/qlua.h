
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef struct entity_s entity_t;

typedef struct
{
	int tableindex;
	int index;
	char name[64];
} qluafunction_t;

#define MAX_RPCDATA 1024
#define MAX_RPCS 1024

typedef struct
{
	char funcname[64];
	char data[MAX_RPCDATA];
	int size;
} rpc_t;

typedef struct
{
	lua_State* state;
	qluafunction_t* qcfunctionoverrides;
	qluafunction_t qcfuncproxy;
	qluafunction_t luafuncproxy;
	qluafunction_t onfreeentityfunc;
	qluafunction_t requestbagitemnamesfunc;
	qluafunction_t requestbagvaluefunc;
	rpc_t rpcs[MAX_RPCS];
	int rpccount;
} qlua_t;


typedef struct
{
	lua_State* state;
	qluafunction_t tickfunc;
	qluafunction_t setbagvaluefunc;
	qluafunction_t clearreplicatedbagfunc;
	rpc_t rpcs[MAX_RPCS];
	int rpccount;
} clientlua_t;

typedef enum
{
	bagv_none,
	bagv_number,
	bagv_boolean,
	bagv_string,
	bagv_table,
	bagv_nil
} BagValueType;

extern qlua_t qlua;
extern clientlua_t clua;

qluafunction_t QLua_GetFunction(lua_State* state, char * name);
void QLua_PushLuaFunction(lua_State* state, qluafunction_t * luafunction);
bool QLua_CallFunction(lua_State* state, int argcount, int returncount);

void QLua_GetBagItemNames(edict_t* ent, const char* path, char* outnames, int* namecount);
void QLua_GetBagItem(edict_t* ent, const char* path, const char* name, BagValueType* valuetype, float* numbervalue, bool* boolvalue, char** stringvalue);

void QLua_SetClientReplicatedBagNumber(entity_t* entity, const char* path, const char* name, float value);
void QLua_SetClientReplicatedBagBool(entity_t* entity, const char* path, const char* name, bool value);
void QLua_SetClientReplicatedBagString(entity_t* entity, const char* path, const char* name, const char* value);
void QLua_ClearClientReplicatedBag(entity_t* entity, const char* name, const char* value);

void QLua_Init();
void QLua_InitClient();
