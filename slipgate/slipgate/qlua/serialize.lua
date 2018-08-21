
ffi.cdef [[
	typedef struct
	{
		char funcname[64];
		char data[1024];
		int size;
	} RPC;

	void SendRPC(RPC* rpc);
]]

RPC = ffi.typeof("RPC")

local typeidtable = {
	number = 0,
	boolean = 1,
	string = 3,
	table = 4,
	["nil"] = 5,
	cdata = 6,
}
for k,v in pairs(typeidtable) do
	typeidtable[v] = k
end

function Serialize(t, data, pos)
	local serializers = nil
	serializers = {
		number = function(value)
			ffi.cast("double*", (data+pos))[0] = value
			pos = pos+8
		end,
		boolean = function(value)
			local v = 0
			if value then v = 1 end
			data[pos] = v
			pos = pos+1
		end,
		string = function(value)
			ffi.copy(data+pos, value)
			pos = pos+#value+1
		end,
		table = function(value)
			pos = Serialize(value, data, pos)
		end,
		cdata = function(value)
			local type = ffi.typeof(value)
			local name = tostring(type)
			local space = string.find(name, " ")
			local firstspace = space
			space = string.find(name, " ", space+1)
			if space == nil then
				space = #name
			end
			name = string.sub(name, firstspace+1, space-1)
			if string.ends(name, "_s") then
				name = string.sub(name, 1, #name-2)
			end
			serializers["string"](name)
			local size = ffi.sizeof(type)
			ffi.copy(data+pos, value, size)
			pos = pos+size
		end
	}
	local elementcount = 0
	for k,v in pairs(t) do
		local ktype = type(k)
		if ktype == "string" or ktype == "number" then elementcount = elementcount+1 end
	end
	serializers["number"](elementcount)

	for k,v in pairs(t) do
		local ktype = type(k)
		if ktype == "string" or ktype == "number" then
			local ktypeid = typeidtable[ktype]
			data[pos] = ktypeid
			pos = pos+1
			serializers[ktype](k)

			local type = type(v)
			local typeid = typeidtable[type]
			data[pos] = typeid
			pos = pos+1

			serializers[type](v)
			--print("serialize "..k.." = "..type..":"..tostring(v).."\n")
		end
	end
	return pos
end

function Deserialize(data, pos)
	local tbl = {}
	local deserilizers = nil
	deserilizers = {
		number = function()
			local v = tonumber(ffi.cast("double*", (data+pos))[0])
			pos = pos+8
			return v
		end,
		boolean = function()
			local v = not (data[pos] == 0)
			pos = pos+1
			return v
		end,
		string = function()
			local v = ffi.string(data+pos)
			pos = pos+#v+1
			return v
		end,
		table = function()
			local v = Deserialize(data, pos)
			pos = v.pos
			return v.table
		end,
		cdata = function()
			local name = deserilizers["string"]()
			local v = ffi.new(name.."[1]")
			local size = ffi.sizeof(v)
			ffi.copy(v, data+pos, size)
			pos = pos+size
			return v[0]
		end
	}

	local elementcount = deserilizers["number"]()

	local counter = 0
	while counter < elementcount do
		local ktypeid = data[pos]
		local ktype = typeidtable[ktypeid]
		if ktype == nil then
			print("cannot convert ktypeid \""..ktypeid.."\" to type name at position "..pos..".\n")
			return
		end
		pos = pos+1
		local key = deserilizers[ktype]()

		local typeid = data[pos]
		local type = typeidtable[typeid]
		if (type == nil) then
			print("cannot convert typeid \""..typeid.."\" to type name at position "..pos..".\n")
			return
		end
		pos = pos+1
		local value = deserilizers[type]()
		--print("deserialize "..key.." = "..type..":"..tostring(value).."\n")
		tbl[key] = value
		counter = counter+1
	end

	return {table = tbl, pos = pos}
end

local rpccache = ffi.new("RPC")
function SendRPC(funcname, ...)
    local rpc = rpccache
	ffi.copy(rpc.funcname, funcname)
	local pos = 0
	pos = Serialize({...}, rpc.data, pos)
	rpc.size = pos
	if rpc.size > 1024 then
		print("rpc.size > 1024")
		return
	end

    ffi.C.SendRPC(rpc)
end

function ConsumeRPC(rpc)
	local funcname = ffi.string(rpc.funcname)
	local pos = 0

	local res = Deserialize(rpc.data, pos)

	local func = _G[funcname]
	if type(func) == "function" then
		func(unpack(res.table))
	else
		print("Unresolved RPC call \""..funcname.."\"\n")
	end
end
