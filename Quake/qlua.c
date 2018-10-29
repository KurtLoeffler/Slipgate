
#include "quakedef.h"

qlua_t qlua = { 0 };
clientlua_t clua = { 0 };

//lua_State* luastate;
char basepath[MAX_OSPATH];

void PrintLuaPrintError(lua_State* state)
{
	// The error message is on top of the stack.
	// Fetch it, print it and then pop it off the stack.
	const char* message = lua_tostring(state, -1);
	
	int length = strlen(message);
	char* realmessage = malloc(length+2);
	realmessage = strcpy(realmessage, message);
	strcat(realmessage, "\n");
	Con_Printf(realmessage);
	free(realmessage);

	lua_pop(state, 1);
}


qluafunction_t QLua_GetFunction(lua_State* state, char* name)
{
	qluafunction_t luafunction = (qluafunction_t){ 0 };

	lua_newtable(state);  // create table for functions
	luafunction.tableindex = luaL_ref(state, LUA_REGISTRYINDEX); // store said table in pseudo-registry
	lua_rawgeti(state, LUA_REGISTRYINDEX, luafunction.tableindex); // retrieve table for functions
	lua_getglobal(state, name); // retrieve function named "f" to store
	luafunction.index = luaL_ref(state, -2); // store a function in the function table
												// table is two places up the current stack counter
	lua_pop(state, 1); // we are done with the function table, so pop it

	strcpy(luafunction.name, name);

	return luafunction;
}

void QLua_PushLuaFunction(lua_State* state, qluafunction_t* luafunction)
{
	lua_rawgeti(state, LUA_REGISTRYINDEX, luafunction->tableindex); // retrieve function table
	lua_rawgeti(state, -1, luafunction->index); // retreive function
}

bool QLua_CallFunction(lua_State* state, int argcount, int returncount)
{
	if (lua_pcall(state, argcount, returncount, 0) != 0)
	{
		PrintLuaPrintError(state);
		return false;
	}

	return true;
}

int LuaConPrint(lua_State* state)
{
	// The number of function arguments will be on top of the stack.
	int args = lua_gettop(state);

	for (int i = 0; i < args; i++)
	{
		const char* str = lua_tostring(state, i+1);
		if (!str)
		{
			Con_Printf("null");
		}
		else
		{
			Con_Printf(str);
		}
		
	}

	return 0;
}

bool DoFile(lua_State* state, char* name)
{
	char fullname[MAX_OSPATH] = "";
	strcat(fullname, basepath);
	strcat(fullname, name);

	int result = luaL_loadfile(state, fullname);
	if (result != 0)
	{
		PrintLuaPrintError(state);
		return false;
	}

	result = lua_pcall(state, 0, LUA_MULTRET, 0);

	if (result != 0)
	{
		PrintLuaPrintError(state);
		return false;
	}

	return true;
}

bool DoString(lua_State* state, char* str)
{
	int result  = luaL_dostring(state, str);
	if (result != 0)
	{
		PrintLuaPrintError(state);
		return false;
	}
	return true;
}

export globalvars_t* GetQCSysGlobalVarsPtr()
{
	return pr_global_struct;
}

export entvars_t* EdictOffsetToEntVar(int edictnum)
{
	edict_t* edict = PROG_TO_EDICT(edictnum);
	entvars_t* entvars = &edict->v;
	return entvars;
}

export int EntVarsToEdictOffset(entvars_t* entvars)
{
	int offset = NUM_FOR_EDICT((edict_t*)entvars)*pr_edict_size;
	return offset;
}

export int GetEntityID(entvars_t* entvars)
{
	byte* b = (byte*)entvars;
	b -= offsetof(edict_t, v);
	edict_t* edict = (edict_t*)b;
	return edict->id;
}

export int GetEntityIndex(entvars_t* entvars)
{
	int index = NUM_FOR_EDICT((edict_t*)entvars);
	return index;
}

export entvars_t* GetEntityFromIndex(int index)
{
	edict_t* edict = (edict_t*)((byte*)sv.edicts+index*pr_edict_size);
	return &edict->v;
}

export int GetEntityCount()
{
	return sv.num_edicts;
}

export bool EntityIsFree(entvars_t* entvars)
{
	byte* b = (byte*)entvars;
	b -= offsetof(edict_t, v);
	edict_t* edict = (edict_t*)b;
	return !edict->id;
}

export int GetOrAllocQCString(const char* str)
{
	int len = strlen(str);

	char* newstr = (char*)Hunk_AllocName(len*2, "qcstring");
	strcpy(newstr, str);
	
	int index = PR_SetEngineString(newstr);
	//Con_Printf("Allocated QCString \"%s\" %i\n", newstr, index);

	return index;
}

export const char* GetQCString(int stringnum)
{
	return PR_GetString(stringnum);
}

export int GetGlobalOffset(const char* name)
{
	ddef_t* def = ED_FindGlobal(name);
	return def->ofs;
}

export eval_t* GetGlobal(int offset)
{
	eval_t* val = (eval_t*)&pr_globals[offset];
	return val;
}

export int GetFieldOffset(const char* name)
{
	ddef_t* def = ED_FindField(name);
	if (def == NULL)
	{
		return 0;
	}
	return def->ofs;
}

export eval_t* GetField(entvars_t* entvars, int fieldoffset)
{
	eval_t* eval = (eval_t*)((char*)entvars+fieldoffset*4);
	return eval;
}

export int FindQCFunc(const char* name)
{
	dfunction_t* func = ED_FindFunction(name);
	if (func == NULL)
	{
		return 0;
	}
	int index = func-pr_functions;
	return index;
}

export void OverrideQCFunc(const char* name)
{
	dfunction_t* qcfunc = ED_FindFunction(name);
	if (qcfunc == NULL)
	{
		return;
	}
	int index = qcfunc-pr_functions;
	qluafunction_t luafunc = QLua_GetFunction(qlua.state, (char*)name);
	if (!luafunc.index)
	{
		return;
	}
	qlua.qcfunctionoverrides[index] = luafunc;
}

export void CallQCFunc(int fnum, bool allowoverride)
{
	PR_ExecuteProgramOverride(fnum, allowoverride, false);
}

export void CallQCBuiltinFunc(int fnum)
{
	pr_builtins[fnum]();
}

export int GetQCFuncCount()
{
	return progs->numfunctions;
}

char bagitemnames[1024];
int bagitemnamecount;
int bagitemnamelength;
export void RespondBagItemNames(const char* names, int namecount)
{
	strcpy(bagitemnames, names);
	bagitemnamelength = strlen(bagitemnames);
	char* scan = bagitemnames;
	while (*scan)
	{
		if (*scan == '\n')
		{
			*scan = 0;
		}
		scan++;
	}
	bagitemnamecount = namecount;
}

void QLua_GetBagItemNames(edict_t* ent, const char* path, bool requireReplicated, char* outnames, int* namecount)
{
	int index = NUM_FOR_EDICT(ent);

	QLua_PushLuaFunction(qlua.state, &qlua.requestbagitemnamesfunc);
	lua_pushnumber(qlua.state, index);
	lua_pushstring(qlua.state, path);
	lua_pushboolean(qlua.state, requireReplicated);
	QLua_CallFunction(qlua.state, 3, LUA_MULTRET);
	lua_pop(qlua.state, 1);//Pop function table.

	*namecount = bagitemnamecount;

	memcpy(outnames, bagitemnames, bagitemnamelength+1);
}

BagValueType respondbagvaluetype;
float respondbagnumbervalue;
bool respondbagboolvalue;
const char* respondbagstringvalue;
export void RespondBagItemNumber(float value)
{
	respondbagvaluetype = bagv_number;
	respondbagnumbervalue = value;
}

export void RespondBagItemBool(char value)
{
	respondbagvaluetype = bagv_boolean;
	respondbagboolvalue = value != 0;
}

export void RespondBagItemString(const char* value)
{
	respondbagvaluetype = bagv_string;
	respondbagstringvalue = value;
}

export void RespondBagItemTable()
{
	respondbagvaluetype = bagv_table;
}

export qmodel_t* GetModel(const char* name)
{
	return Mod_ForName(name, false);
}

export void DrawEntityModel(entity_t* entity)
{
	if (cl_nummodeldraw >= MAX_MODELDRAW)
	{
		Con_Printf("Too many modeldraws.");
		return;
	}
	cl_modeldraw[cl_nummodeldraw] = *entity;
	cl_nummodeldraw++;
}

void QLua_GetBagItem(edict_t* ent, const char* path, const char* name, BagValueType* valuetype, float* numbervalue, bool* boolvalue, char** stringvalue)
{
	int index = NUM_FOR_EDICT(ent);

	*valuetype = bagv_none;
	*numbervalue = 0;
	*boolvalue = false;
	*stringvalue = NULL;

	respondbagvaluetype = bagv_none;

	QLua_PushLuaFunction(qlua.state, &qlua.requestbagvaluefunc);
	lua_pushnumber(qlua.state, index);
	lua_pushstring(qlua.state, path);
	lua_pushstring(qlua.state, name);
	QLua_CallFunction(qlua.state, 3, LUA_MULTRET);
	lua_pop(qlua.state, 1);//Pop function table.

	if (respondbagvaluetype == bagv_none)
	{
		return;
	}

	*valuetype = respondbagvaluetype;

	if (respondbagvaluetype == bagv_number)
	{
		*numbervalue = respondbagnumbervalue;
	}
	else if (respondbagvaluetype == bagv_boolean)
	{
		*boolvalue = respondbagboolvalue;
	}
	else if (respondbagvaluetype == bagv_string)
	{
		*stringvalue = (char*)respondbagstringvalue;
	}
}

void QLua_SetClientReplicatedBagNumber(entity_t* entity, const char* path, const char* name, float value)
{
	int index = ((int)((byte*)entity-(byte*)cl_entities))/sizeof(entity_t);

	QLua_PushLuaFunction(clua.state, &clua.setbagvaluefunc);
	lua_pushinteger(clua.state, index);
	lua_pushstring(clua.state, path);
	lua_pushstring(clua.state, name);
	lua_pushnumber(clua.state, value);
	QLua_CallFunction(clua.state, 4, LUA_MULTRET);
	lua_pop(clua.state, 1);//Pop function table.
}

void QLua_SetClientReplicatedBagBool(entity_t* entity, const char* path, const char* name, bool value)
{
	int index = ((int)((byte*)entity-(byte*)cl_entities))/sizeof(entity_t);

	QLua_PushLuaFunction(clua.state, &clua.setbagvaluefunc);
	lua_pushinteger(clua.state, index);
	lua_pushstring(clua.state, path);
	lua_pushstring(clua.state, name);
	lua_pushboolean(clua.state, value);
	QLua_CallFunction(clua.state, 4, LUA_MULTRET);
	lua_pop(clua.state, 1);//Pop function table.
}

void QLua_SetClientReplicatedBagString(entity_t* entity, const char* path, const char* name, const char* value)
{
	int index = ((int)((byte*)entity-(byte*)cl_entities))/sizeof(entity_t);

	QLua_PushLuaFunction(clua.state, &clua.setbagvaluefunc);
	lua_pushinteger(clua.state, index);
	lua_pushstring(clua.state, path);
	lua_pushstring(clua.state, name);
	lua_pushstring(clua.state, value);
	QLua_CallFunction(clua.state, 4, LUA_MULTRET);
	lua_pop(clua.state, 1);//Pop function table.
}

void QLua_ClearClientReplicatedBag(entity_t* entity, const char* name, const char* value)
{
	int index = ((int)((byte*)entity-(byte*)cl_entities))/sizeof(entity_t);

	QLua_PushLuaFunction(clua.state, &clua.clearreplicatedbagfunc);
	lua_pushinteger(clua.state, index);
	QLua_CallFunction(clua.state, 1, LUA_MULTRET);
	lua_pop(clua.state, 1);//Pop function table.
}

export int GetClientVisEntityCount()
{
	return cl_numvisedicts;
}

export entity_t* GetClientVisEntityByIndex(int index)
{
	return cl_visedicts[index];
}

export entity_t* GetClientEntityByIndex(int index)
{
	return &cl_entities[index];
}

export client_state_t* GetClientState()
{
	return &cl;
}

export void InitParticleEmitter(particleemitter_t* e)
{
	R_InitParticleEmitter(e);
}

export void RunParticleEmitter(particleemitter_t* e)
{
	R_RunParticleEmitter(e);
}

export dlight_t* CreateDLight(int key)
{
	return CL_AllocDlight(key);
}

export cltrace_t ClientTraceBox(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, entity_t* passent)
{
	return CL_TraceBox(start, mins, maxs, end, passent);
}

export void AngleVectors(vec3_t angles, vec3_t* forward, vec3_t* right, vec3_t* up)
{
	Vec_AngleVectors(angles, forward, right, up);
}

export void SendRPC(rpc_t* rpc)
{
	if (qlua.rpccount >= MAX_RPCS)
	{
		Con_Printf("qlua.rpccount > MAX_RPCS\n");
		return;
	}
	qlua.rpcs[qlua.rpccount] = *rpc;
	qlua.rpccount++;
}

export int GetClientRPCCount()
{
	return clua.rpccount;
}

export rpc_t* GetClientRPC(int index)
{
	if (index < 0 || index >= MAX_RPCS)
	{
		Con_Printf("Invalid rpc index \"%i\"\n", index);
		return;
	}
	return &clua.rpcs[index];
}

void QLua_Destroy()
{
	if (qlua.state)
	{
		lua_close(qlua.state);
	}
	
	if (qlua.qcfunctionoverrides)
	{
		free(qlua.qcfunctionoverrides);
	}
	memset(&qlua, 0, sizeof(qlua_t));
}

void QLua_Init()
{
	QLua_Destroy();

	strcpy(basepath, "");
	strcat(basepath, defaultgamename);
	strcat(basepath, "/qlua/");

	char mainfilepath[MAX_OSPATH];
	strcpy(mainfilepath, basepath);
	strcat(mainfilepath, "sv/main.lua");

	int openhandle;
	if (Sys_FileOpenRead(mainfilepath, &openhandle) >= 0)
	{
		Sys_FileClose(openhandle);
	}
	else
	{
		Con_Printf("%s file not found. QLua disabled.\n", mainfilepath);
		return;
	}

	qlua.qcfunctionoverrides = (qluafunction_t*)malloc(sizeof(qluafunction_t)*progs->numfunctions);
	memset(qlua.qcfunctionoverrides, 0, sizeof(qluafunction_t)*progs->numfunctions);

	qlua.state = luaL_newstate();
	luaL_openlibs(qlua.state);

	char currentdirexec[256] = "";
	sprintf(currentdirexec, "os.currentdir = \"%s\"", basepath);
	DoString(qlua.state, currentdirexec);

	DoString(qlua.state, "sysprint = print");
	DoString(qlua.state, "sysrequire = require");
	DoString(qlua.state, "require = function (path) return sysrequire(os.currentdir..path) end");
	
	lua_register(qlua.state, "print", LuaConPrint);

	if (!DoFile(qlua.state, "sv/main.lua"))
	{
		QLua_Destroy();
		return;
	}

	qlua.qcfuncproxy = QLua_GetFunction(qlua.state, "QCFuncProxy");
	qlua.luafuncproxy = QLua_GetFunction(qlua.state, "LuaFuncProxy");
	qlua.onfreeentityfunc = QLua_GetFunction(qlua.state, "OnFreeEntity");
	qlua.requestbagitemnamesfunc = QLua_GetFunction(qlua.state, "RequestBagItemNames");
	qlua.requestbagvaluefunc = QLua_GetFunction(qlua.state, "RequestBagValue");

	Con_Printf("QLua initialized.\n");
}

void QLua_DestroyClient()
{
	if (clua.state)
	{
		lua_close(clua.state);
	}

	memset(&clua, 0, sizeof(clientlua_t));
}

void QLua_InitClient()
{
	QLua_DestroyClient();

	strcpy(basepath, "");
	strcat(basepath, defaultgamename);
	strcat(basepath, "/qlua/");

	char mainfilepath[MAX_OSPATH];
	strcpy(mainfilepath, basepath);
	strcat(mainfilepath, "cl/main.lua");

	int openhandle;
	if (Sys_FileOpenRead(mainfilepath, &openhandle) >= 0)
	{
		Sys_FileClose(openhandle);
	}
	else
	{
		Con_Printf("%s main file not found. Client Lua disabled.\n", mainfilepath);
		return;
	}

	clua.state = luaL_newstate();
	luaL_openlibs(clua.state);

	char currentdirexec[256] = "";
	sprintf(currentdirexec, "os.currentdir = \"%s\"", basepath);
	DoString(clua.state, currentdirexec);

	DoString(clua.state, "sysprint = print");
	DoString(clua.state, "sysrequire = require");
	DoString(clua.state, "require = function (path) return sysrequire(os.currentdir..path) end");

	lua_register(clua.state, "print", LuaConPrint);

	if (!DoFile(clua.state, "cl/main.lua"))
	{
		QLua_DestroyClient();
		return;
	}

	clua.tickfunc = QLua_GetFunction(clua.state, "ClientTick");
	clua.setbagvaluefunc = QLua_GetFunction(clua.state, "SetBagValue");
	clua.clearreplicatedbagfunc = QLua_GetFunction(clua.state, "ClearReplicatedBag");

	Con_Printf("Client Lua initialized.\n");
}
