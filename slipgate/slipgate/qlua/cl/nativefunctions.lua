
ffi.cdef [[
int GetClientVisEntityCount();
Entity* GetClientVisEntityByIndex(int index);
Entity* GetClientEntityByIndex(int index);
ClientState* GetClientState();
void InitParticleEmitter(ParticleEmitter* e);
void RunParticleEmitter(ParticleEmitter* e);
DLight* CreateDLight(int key);
int GetClientRPCCount();
RPC* GetClientRPC(int index);
QModel* GetModel(const char* name);
void DrawEntityModel(Entity* entity);
CLTrace ClientTraceBox(Vec3 start, Vec3 mins, Vec3 maxs, Vec3 end, Entity* passent);
]]

function GetVisEntityCount()
	return ffi.C.GetClientVisEntityCount();
end

function GetEntityByVisIndex(index)
	return ffi.C.GetClientVisEntityByIndex(index);
end

function GetEntityByIndex(index)
	return ffi.C.GetClientEntityByIndex(index);
end

particleemittype = {
	box = 0,
	sphere = 1,
	trail = 2
}
function InitParticleEmitter(e)
	return ffi.C.InitParticleEmitter(e);
end

function RunParticleEmitter(e)
	return ffi.C.RunParticleEmitter(e);
end

function NewParticleEmitter()
	local e = ParticleEmitter()
	InitParticleEmitter(e)
	return e
end

function CreateDLight(key)
	return ffi.C.CreateDLight(key)
end

local modelcache = {}
function GetModel(name)
	local model = modelcache[name]
	if not model then
		model = ffi.C.GetModel(name)
		if model then
			modelcache[name] = model
		else
			print("Could not load model \""..name.."\" make sure it was precached by the server.\n")
			return nil
		end
	end
	return model
end

function DrawEntity(model)
	ffi.C.DrawEntityModel(model)
end

function TraceLine(startpos, endpos, passent)
	return ffi.C.ClientTraceBox(startpos, Vec3(), Vec3(), endpos, passent)
end

function TraceBox(startpos, mins, maxs, endpos, passent)
	return ffi.C.ClientTraceBox(startpos, mins, maxs, endpos, passent)
end
