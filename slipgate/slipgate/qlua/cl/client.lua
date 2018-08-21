
cl = ffi.C.GetClientState();

function SetBagValue(index, path, name, value)
	local entity = GetEntityByIndex(index)

	local pathparts = string.split(path, ".")

	local bag = entity
	for i,v in ipairs(pathparts) do
		local b = bag[v]
		if b == nil then
			b = {}
			bag[v] = b
		end
		bag = b
	end

	if bag == entity then
		print("bad bag path.\n")
		return
	end

	bag[name] = value
end

function ClearReplicatedBag(index)
	local entity = GetEntityByIndex(index)
	entityreplicatedbags[entity.id] = {}
end

local onnewentityfuncs = {}
function OnNewEntity(entity)
	for k,v in pairs(onnewentityfuncs) do
		v(entity)
	end
end

local tickfuncs = {}
local onentitytickfuncs = {}
local knownentitiesbyid = {}
local currententitiesbyvisindex = {}
local currententitiesbyid = {}
local previousentitiesbyvisindex = {}
local newentities = {}
function ClientTick()
	local oldcurrententities = currententitiesbyvisindex
	currententitiesbyvisindex = {}

	currententitiesbyid = {}

	for i=1,#newentities do
		newentities[i] = nil
	end

	local viscount = GetVisEntityCount()
	for i=1,viscount do
		local entity = GetEntityByVisIndex(i-1)
		local isnew = knownentitiesbyid[entity.id] == nil
		currententitiesbyvisindex[i] = entity
		currententitiesbyid[entity.id] = entity
		knownentitiesbyid[entity.id] = entity
		if isnew then
			table.insert(newentities, entity)
		end
	end

	for i,v in ipairs(newentities) do
		OnNewEntity(v)
	end

	for k,v in pairs(tickfuncs) do
		v()
	end

	for i,v in ipairs(currententitiesbyvisindex) do
		if v.id > 0 then
			local tickfunc = v.bag.tick
			if type(tickfunc) == "function" then
				tickfunc(v)
			end
		end
	end

	for i,v in ipairs(currententitiesbyvisindex) do
		for k,f in pairs(onentitytickfuncs) do
			f(v)
		end
	end

	for i=1, ffi.C.GetClientRPCCount() do
		local rpc = ffi.C.GetClientRPC(i-1)
		ConsumeRPC(rpc)
	end

	previousentitiesbyvisindex = oldcurrententities
end

function GetEntityByID(id)
	return currententitiesbyid[id]
end

function RegisterTickFunc(func)
	tickfuncs[func] = func
end

function UnregisterTickFunc(func)
	tickfuncs[func] = nil
end

function RegisterEntityTickFunc(func)
	onentitytickfuncs[func] = func
end

function UnregisterEntityTickFunc(func)
	onentitytickfuncs[func] = nil
end

function RegisterOnNewEntityFunc(func)
	onnewentityfuncs[func] = func
end

function UnregisterOnNewEntityFunc(func)
	onnewentityfuncs[func] = nil
end
