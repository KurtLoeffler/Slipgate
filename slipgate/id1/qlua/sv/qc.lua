
ffi.cdef [[
typedef int string_t;
typedef int func_t;
typedef union EVal_s
{
	string_t stringnum;
	float number;
	Vec3 vector;
	func_t funcnum;
	int integer;
	int edictnum;
} EVal;
typedef struct QCSysGlobalVars_s
{	int	raw_pad[28];
	int	raw_self;
	int	raw_other;
	int	raw_world;
	float	raw_time;
	float	raw_frametime;
	float	raw_force_retouch;
	string_t	raw_mapname;
	float	raw_deathmatch;
	float	raw_coop;
	float	raw_teamplay;
	float	raw_serverflags;
	float	raw_total_secrets;
	float	raw_total_monsters;
	float	raw_found_secrets;
	float	raw_killed_monsters;
	float	raw_parm1;
	float	raw_parm2;
	float	raw_parm3;
	float	raw_parm4;
	float	raw_parm5;
	float	raw_parm6;
	float	raw_parm7;
	float	raw_parm8;
	float	raw_parm9;
	float	raw_parm10;
	float	raw_parm11;
	float	raw_parm12;
	float	raw_parm13;
	float	raw_parm14;
	float	raw_parm15;
	float	raw_parm16;
	Vec3	raw_v_forward;
	Vec3	raw_v_up;
	Vec3	raw_v_right;
	float	raw_trace_allsolid;
	float	raw_trace_startsolid;
	float	raw_trace_fraction;
	Vec3	raw_trace_endpos;
	Vec3	raw_trace_plane_normal;
	float	raw_trace_plane_dist;
	int	raw_trace_ent;
	float	raw_trace_inopen;
	float	raw_trace_inwater;
	int	raw_msg_entity;
	func_t	raw_main;
	func_t	raw_StartFrame;
	func_t	raw_PlayerPreThink;
	func_t	raw_PlayerPostThink;
	func_t	raw_ClientKill;
	func_t	raw_ClientConnect;
	func_t	raw_PutClientInServer;
	func_t	raw_ClientDisconnect;
	func_t	raw_SetNewParms;
	func_t	raw_SetChangeParms;
} QCSysGlobalVars;

typedef struct QCEntVars_s
{
	float	raw_modelindex;
	Vec3	raw_absmin;
	Vec3	raw_absmax;
	float	raw_ltime;
	float	raw_movetype;
	float	raw_solid;
	Vec3	raw_origin;
	Vec3	raw_oldorigin;
	Vec3	raw_velocity;
	Vec3	raw_angles;
	Vec3	raw_avelocity;
	Vec3	raw_punchangle;
	string_t	raw_classname;
	string_t	raw_model;
	float	raw_frame;
	float	raw_skin;
	float	raw_effects;
	Vec3	raw_mins;
	Vec3	raw_maxs;
	Vec3	raw_size;
	func_t	raw_touch;
	func_t	raw_use;
	func_t	raw_think;
	func_t	raw_blocked;
	float	raw_nextthink;
	int	raw_groundentity;
	float	raw_health;
	float	raw_frags;
	float	raw_weapon;
	string_t	raw_weaponmodel;
	float	raw_weaponframe;
	float	raw_currentammo;
	float	raw_ammo_shells;
	float	raw_ammo_nails;
	float	raw_ammo_rockets;
	float	raw_ammo_cells;
	float	raw_items;
	float	raw_takedamage;
	int	raw_chain;
	float	raw_deadflag;
	Vec3	raw_view_ofs;
	float	raw_button0;
	float	raw_button1;
	float	raw_button2;
	float	raw_impulse;
	float	raw_fixangle;
	Vec3	raw_v_angle;
	float	raw_idealpitch;
	string_t	raw_netname;
	int	raw_enemy;
	float	raw_flags;
	float	raw_colormap;
	float	raw_team;
	float	raw_max_health;
	float	raw_teleport_time;
	float	raw_armortype;
	float	raw_armorvalue;
	float	raw_waterlevel;
	float	raw_watertype;
	float	raw_ideal_yaw;
	float	raw_yaw_speed;
	int	raw_aiment;
	int	raw_goalentity;
	float	raw_spawnflags;
	string_t	raw_target;
	string_t	raw_targetname;
	float	raw_dmg_take;
	float	raw_dmg_save;
	int	raw_dmg_inflictor;
	int	raw_owner;
	Vec3	raw_movedir;
	string_t	raw_message;
	float	raw_sounds;
	string_t	raw_noise;
	string_t	raw_noise1;
	string_t	raw_noise2;
	string_t	raw_noise3;
} QCEntVars;

typedef struct Link_s Link;
typedef struct Link_s
{
	Link* prev, *next;
} Link;

typedef struct EDict_s
{
	int id;
	Link area;
	int num_leafs;
	int leafnums[32];

	EntityState baseline;
	unsigned char alpha;
	int sendinterval;
	float freetime;
	QCEntVars v;
} EDict;

typedef struct Trace_s
{
	int allsolid;
	int startsolid;
	int inopen, inwater;
	float fraction;
	Vec3 endpos;
	Plane plane;
	EDict* ent;
} Trace;
]]

ffi.cdef [[
	QCSysGlobalVars* GetQCSysGlobalVarsPtr();
	QCEntVars* EdictOffsetToEntVar(int edictnum);
	int EntVarsToEdictOffset(QCEntVars* edictnum);
	int GetEntityID(QCEntVars* entvars);
	int GetEntityIndex(QCEntVars* entvars);
	QCEntVars* GetEntityFromIndex(int index);
	int GetEntityCount();
	bool EntityIsFree(QCEntVars* entvars);
	int GetOrAllocQCString(const char* str);
	char* GetQCString(int stringnum);
	int GetGlobalOffset(const char* name);
	EVal* GetGlobal(int offset);
	int GetFieldOffset(const char* name);
	EVal* GetField(QCEntVars* entvars, int fieldoffset);
	int FindQCFunc(const char* name);
	void OverrideQCFunc(const char* name);
	void CallQCFunc(int fnum, int allowoverride);
	void CallQCBuiltinFunc(int fnum);
	int GetQCFuncCount();
	void RespondBagItemNames(const char* names, int namecount);
	void RespondBagItemNumber(float value);
	void RespondBagItemBool(char value);
	void RespondBagItemString(const char* value);
	void RespondBagItemTable();
]]

EVal = ffi.typeof("EVal")
local EVal_mt = {
    __eq = function(self, rhs)
		if not ((hrs == nil) == (hrs == nil)) then return false end
		if hrs == nil then return false end
		if type(rhs) == "number" then
			return self.number == rhs
		end
		if type(rhs) == "cdata" and string.starts(tostring(ffi.typeof(rhs)), "ctype<struct Vec3_s") then
			return self.vector == rhs
		end
        return self.number == rhs.number
    end,
	__index = function (table, key)
		if key == "entity" then
			local entvars = ffi.C.EdictOffsetToEntVar(table.edictnum)
			return entvars
		elseif key == "string" then
			return GetQCString(table.integer)
		elseif key == "copy" then
			local c = EVal()
			c.vector = table.vector.copy
			c.integer = table.integer
			return c
		end
	end,
	__tostring = function (v)
		return string.format("EVal (int = %d) (float = %f) (vector = %s)", v.integer, v.number, tostring(v.vector))
	end,
}
ffi.metatype(EVal, EVal_mt);

function GetEntityFromIndex(index)
	return ffi.C.GetEntityFromIndex(index);
end

function GetEntityCount()
	return ffi.C.GetEntityCount();
end

local entitybags = {}
local entitybagpersist = {}
local entitybagreplicate = {}
local entityfieldoffsets = {}
QCEntVars = ffi.typeof("QCEntVars")
QCEntVars_mt = {
	__eq = function(self, rhs)
		if rhs == nil then
			return false
		end
		if type(rhs) == "cdata" then
			local cdatatype = tostring(ffi.typeof(rhs))
			if string.starts(cdatatype, "ctype<struct QCEntVars_s") then
				return self.id == rhs.id
			end
		end
        return false
    end,
	__index = function (table, key)
		if key == "id" then
			return ffi.C.GetEntityID(table)
		end
		if key == "index" then
			return ffi.C.GetEntityIndex(table)
		end
		if key == "free" then
			return ffi.C.EntityIsFree(table)
		end
		if key == "bag" then
			if table.free then
				print("Attempt to access bag on free entity.")
				return nil
			end
			local id = table.id
			--print("getbag "..id.."\n")
			local bag = entitybags[id]
			if bag == nil then
				--print("makebag "..id.."\n")
				bag = {}
				entitybags[id] = bag
			end
			return bag
		end
		if key == "persist" then
			return function (fieldname, state)
				if table.free then
					print("Attempt to persist on free entity.")
					return
				end
				local id = table.id
				if state then
					local persist = entitybagpersist[id]
					if persist == nil then
						persist = {}
						entitybagpersist[id] = persist
					end
				else
					entitybagpersist[id] = nil
				end
			end
		end
		if key == "replicate" then
			return function (fieldname, state)
				if table.free then
					print("Attempt to replicate on free entity.")
					return
				end
				local id = table.id
				if state then
					local replicate = entitybagreplicate[id]
					if replicate == nil then
						replicate = {}
						entitybagreplicate[id] = replicate
					end
				else
					entitybagreplicate[id] = nil
				end
			end
		end
		return FindField(table, key)
	end,
	__newindex = function (table, key, value)
		if type(key) == "string" then
			local field = FindField(table, key)
			if not (field == nil) then
				SetQCValue(field, value)
			else
				print("Attempt to assign to a non-existant entity field \""..key.."\".\n")
			end
		end
	end
}

ffi.metatype(QCEntVars, QCEntVars_mt)

function OnFreeEntity(index)
	local entity = GetEntityFromIndex(index)
	local id = entity.id
	if entitybags[id] then
		entitybags[id] = nil
		--print("Removed bag"..id.."\n")
	end
	if entitybagpersist[id] then
		entitybagpersist[id] = nil
		--print("Removed bag persist"..id.."\n")
	end
end

OFS_NULL = 0
OFS_RETURN = 1
OFS_PARM0 = 4 -- leave 3 ofs for each parm to hold vectors
OFS_PARM1 = 7
OFS_PARM2 = 10
OFS_PARM3 = 13
OFS_PARM4 = 16
OFS_PARM5 = 19
OFS_PARM6 = 22
OFS_PARM7 = 25
RESERVED_OFS = 28

local qcbuiltinfuncnametoindex =
{
	makevectors = 1,
	setorigin = 2,
	setmodel = 3,
	setsize = 4,
	_break = 6,
	random = 7,
	sound = 8,
	normalize = 9,
	error = 10,
	objerror = 11,
	vlen = 12,
	vectoyaw = 13,
	spawn = 14,
	remove = 15,
	traceline = 16,
	checkclient = 17,
	find = 18,
	precache_sound = 19,
	precache_model = 20,
	stuffcmd = 21,
	findradius = 22,
	bprint = 23,
	sprint = 24,
	dprint = 25,
	ftos = 26,
	vtos = 27,
	coredump = 28,
	traceon = 29,
	traceoff = 30,
	eprint = 31,
	walkmove = 32,
	droptofloor = 34,
	lightstyle = 35,
	rint = 36,
	floor = 37,
	ceil = 38,
	checkbottom = 40,
	pointcontents = 41,
	fabs = 43,
	aim = 44,
	cvar = 45,
	localcmd = 46,
	nextent = 47,
	particle = 48,
	ChangeYaw = 49,
	vectoangles = 51,
	WriteByte = 52,
	WriteChar = 53,
	WriteShort = 54,
	WriteLong = 55,
	WriteCoord = 56,
	WriteAngle = 57,
	WriteString = 58,
	WriteEntity = 59,
	movetogoal = 67,
	precache_file = 68,
	makestatic = 69,
	changelevel = 70,
	setcvar = 72,
	centerprint = 73,
	ambientsound = 74,
	precache_model2 = 75,
	precache_sound2 = 76,
	precache_file2 = 77,
	setspawnparms = 78,
}

qc = {
	globals = ffi.C.GetQCSysGlobalVarsPtr(),
	nooverride = {},
	stringtoeval = {},
	functiontoindex = {},
	FunctionIndex = function (func)
		return qc.functiontoindex[func]
	end,
	globaloffsets = {},
	luafunctions = {},
}
qc.mt = {}
setmetatable(qc, qc.mt)

qc.nooverride.mt = {}
setmetatable(qc.nooverride, qc.nooverride.mt)

qc._indexbase = function (table, key, allowoverride)
	if type(key) == "string" then
		local index
		if allowoverride then
			index = qcbuiltinfuncnametoindex[key]
			if not (index == nil) then
				local func = function (...) return CallQC(-index, allowoverride, ...) end
				rawset(table, key, func)
				qc.functiontoindex[func] = -index
				return func
			end
		end
		index = ffi.C.FindQCFunc(key)
		if not (index == 0) then
			local func = function (...) return CallQC(index, allowoverride, ...) end
			rawset(table, key, func)
			qc.functiontoindex[func] = index
			return func
		end
		if allowoverride then
			return FindGlobal(key)
		end
	end
	return nil
end

qc.mt.__index = function (table, key)
	return qc._indexbase(table, key, true)
end
qc.mt.__newindex = function (table, key, value)
	if type(key) == "string" then
		local global = FindGlobal(key)
		if not (global == nil) then
			SetQCValue(global, value)
		else
			print("Attempt to assign to a non-existant qc global \""..key.."\".\n")
		end
	end
end

qc.nooverride.mt.__index = function (table, key)
	return qc._indexbase(table, key, false)
end

function GetOrAllocQCString(string)
	local index = qc.stringtoeval[string]
	if index == nil then
		index = ffi.C.GetOrAllocQCString(string)
		qc.stringtoeval[string] = index
	end
	return index
end

function Override(name)
	ffi.C.OverrideQCFunc(name)
end

function GetQCString(stringnum)
	local chararray = ffi.C.GetQCString(stringnum)
	return ffi.string(chararray)
end

function FindGlobal(name)
	local offset = qc.globaloffsets[name]
	local global = nil
	if offset == nil then
		offset = ffi.C.GetGlobalOffset(name)
		if offset == 0 then
			return nil
		end
		qc.globaloffsets[name] = offset
		global = ffi.C.GetGlobal(offset)
	else
		global = ffi.C.GetGlobal(offset)
	end
	return global
end

function FindField(entvars, fieldname)
	local fieldoffset = entityfieldoffsets[fieldname]
	local field = nil
	if fieldoffset == nil then
		fieldoffset = ffi.C.GetFieldOffset(fieldname)
		if fieldoffset == 0 then
			return nil
		end
		entityfieldoffsets[fieldname] = fieldoffset
		field = ffi.C.GetField(entvars, fieldoffset)
	else
		field = ffi.C.GetField(entvars, fieldoffset)
	end
	return field
end

local function SetQCParm(index, value)
	local qcglobalevals = ffi.cast("EVal*", ffi.cast("int*",qc.globals)+OFS_PARM0+index*3)
	qcglobalevals[0] = value
end

local function GetQCParam(index)
	local qcglobalevals = ffi.cast("EVal*", ffi.cast("int*",qc.globals)+OFS_PARM0+index*3)
	return qcglobalevals
end

local function GetQCReturn()
	local qcglobalevals = ffi.cast("EVal*", ffi.cast("int*",qc.globals)+OFS_RETURN)
	return qcglobalevals
end

local function SetQCReturn(value)
	local qcglobalevals = ffi.cast("EVal*", ffi.cast("int*",qc.globals)+OFS_RETURN)
	qcglobalevals[0] = value
end

function SetQCValue(qcvalue, value)
	local valuetype = type(value)
	if valuetype == "number" then
		qcvalue.number = value
	elseif valuetype == "string" then
		qcvalue.integer = GetOrAllocQCString(value)
	elseif valuetype == "function" then
		local funcindex = qc.functiontoindex[value]
		if not (funcindex == nil) then
			qcvalue.integer = funcindex
		else
			local funcinfo = qc.luafunctions[value]
			if funcinfo == nil then
				print("Attempt to assign non lua function \""..tostring(value).."\" that has not been indexed.\n")
				return
			end
			qcvalue.integer = ffi.C.GetQCFuncCount()+funcinfo.index
		end
	elseif valuetype == "cdata" then
		local cdatatype = tostring(ffi.typeof(value))
		if string.starts(cdatatype, "ctype<struct EVal_s") then
			qcvalue = value
		elseif string.starts(cdatatype, "ctype<struct Vec3_s") then
			qcvalue.vector = value
		end
	end
end

local function ConvertToEVal(arg)
	local type = type(arg)
	if type == "string" then
		local qcstring = EVal()
		qcstring.integer = GetOrAllocQCString(arg)
		local valueptr = ffi.cast("int*", ffi.cast("int*",qcstring)+2)
		valueptr[0] = qcstring.integer
		local ptrptr = ffi.cast("long*", ffi.cast("int*",qcstring))
		ptrptr[0] = ffi.cast("long*", ffi.cast("int*",qcstring)+2)[0]
		return qcstring
	elseif type == "number" then
		local eval = EVal()
		eval.number = arg
		return eval
	elseif type == "cdata" then
		local cdatatype = tostring(ffi.typeof(arg))

		if string.starts(cdatatype, "ctype<union EVal_s") then
			--if string.ends(cdatatype, "*>") then
				--print("dereference "..cdatatype.."\n")
				--arg = arg.copy
			--end
			--local eval = EVal()
			--eval.vector.x = arg.vector.x
			--eval.vector.y = arg.vector.y
			--eval.vector.z = arg.vector.z
			return arg.copy
		elseif string.starts(cdatatype, "ctype<struct Vec3_s") then
			local eval = EVal()
			eval.vector = arg
			return eval
		elseif string.starts(cdatatype, "ctype<struct QCEntVars_s") then
			local index = ffi.C.EntVarsToEdictOffset(arg)
			local eval = EVal()
			eval.integer = index
			return eval
		else
			print("Unhandled argument type \""..cdatatype.."\"\n")
		end
	end
	return nil
end

function CallQC(funcindex, allowoverride, ...)
	local args = table.pack(...)
	--print("call qc func "..funcindex.." and qc funccount is "..ffi.C.GetQCFuncCount().." argcount is "..args.n.."\n")
	local setargs = function()
		for i=1, args.n do
			local arg = args[i]
			local eval = ConvertToEVal(arg)
			eval = eval.copy

			if eval == nil then
				local t = type(arg)
				if t == "cdata" then t = tostring(ffi.typeof(arg)) end
				error("Could not resolve argument "..i.." to EVal \""..t.."\"", 3)
			end
			--print("arg "..(i-1).." = "..tostring(eval).."\n")
			SetQCParm(i-1, eval.copy)
	    end
	end

	if funcindex < 0 then
		setargs()
		ffi.C.CallQCBuiltinFunc(-funcindex)
		local result = GetQCReturn()
		return result[0].copy
	else
		setargs()
		ffi.C.CallQCFunc(funcindex, allowoverride)
		local result = GetQCReturn()
		return result[0].copy
	end

	return nil
end

local proxyargs = {}
function FuncProxy(func, argcount)
	--This needs to be unique to avoid stack stomping. Could create a cache table.
	--proxyargcopies = ffi.new("EVal[?]", argcount)
	--proxyargcopies = ffi.cast("EVal*", ffi.C.malloc(8*ffi.sizeof("EVal")))
	for i=1, #proxyargs do
		proxyargs[i] = nil
	end

	for i=1,argcount do
		local arg = GetQCParam(i-1)

		proxyargs[i] = arg[0].copy
		--proxyargcopies[i-1] = arg[0].copy
		--proxyargs[i] = proxyargcopies[i-1]
	end

	local result = func(unpack(proxyargs))

	local returneval
	if not (result == nil) then
		returneval = ConvertToEVal(result).copy
	else
		returneval = EVal()
	end
	SetQCReturn(returneval)

	--ffi.C.free(proxyargcopies)
end

function QCFuncProxy(funcname, argcount)
	local funcinfo = qc.luafunctions[funcname]
	--print("call qc proxy func "..funcinfo.index..", "..funcinfo.name.."\n")
	if not funcinfo then error("Could not find function \""..funcname.."\".") end
	FuncProxy(funcinfo.func, argcount)
end

function LuaFuncProxy(index)
	local funcinfo = qc.luafunctions[funcname]
	--print("call lua proxy func "..funcinfo.index..", "..funcinfo.name.."\n")
	if not funcinfo then error("Could not find function id "..index..".") end
	--No way to find out how many arguments so just use the qc max
	FuncProxy(funcinfo.func, 8)
end

function RequestBagItemNames(entindex, path)
	local entity = GetEntityFromIndex(entindex)
	local str = ""
	local count = 0
	local pathparts = string.split(path, ".")

	local bag = entity
	for i,v in ipairs(pathparts) do
		bag = bag[v]
	end

	if bag == entity or bag == nil then
		print("bad bag path.\n")
		return
	end

	for k,v in pairs(bag) do
		if type(k) == "string" then
			if not (str == "") then
				str = str.."\n"
			end
			str = str..k
			count = count+1
		end
	end
	ffi.C.RespondBagItemNames(str, count)
end

function RequestBagValue(entindex, path, name)
	local entity = GetEntityFromIndex(entindex)

	local pathparts = string.split(path, ".")

	local bag = entity
	for i,v in ipairs(pathparts) do
		bag = bag[v]
	end

	if bag == entity or bag == nil then
		print("bad bag path.\n")
		return
	end

	local value = bag[name]
	if value == nil then
		return
	end
	local type = type(value)

	if type == "number" then
		ffi.C.RespondBagItemNumber(value)
	elseif type == "boolean" then
		ffi.C.RespondBagItemBool(value)
	elseif type == "string" then
		ffi.C.RespondBagItemString(value)
	elseif type == "table" then
		ffi.C.RespondBagItemTable()
	end
end
