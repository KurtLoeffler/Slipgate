
ffi.cdef [[
typedef struct Vec3_s
{
	float x;
	float y;
	float z;
} Vec3;

typedef struct
{
	Vec3 normal;
	float dist;
} Plane;

typedef struct EntityState_s
{
	Vec3 origin;
	Vec3 angles;
	unsigned short  modelindex; //johnfitz -- was int
	unsigned short  frame; //johnfitz -- was int
	unsigned char  colormap; //johnfitz -- was int
	unsigned char  skin; //johnfitz -- was int
	unsigned char alpha; //johnfitz -- added
	int effects;
} EntityState;

void* malloc(size_t size);
void free(void* block);
void AngleVectors(Vec3 angles, Vec3* forward, Vec3* right, Vec3* up);
]]

local vec3propertyfuncs = {
	length = function(v)
		return math.sqrt(Vec3.Dot(v, v))
	end,
	normalized = function(v)
		local length = v.length
		local out = Vec3()
		if not (length == 0) then
			local ilength = 1.0/length
			out.x = v.x*ilength
			out.y = v.y*ilength
			out.z = v.z*ilength
		end
		return out
	end,
	random = function(v)
		return Vec3(math.random()*2-1, math.random()*2-1, math.random()*2-1)
	end
}
local vec3funcs = {
	Dot = function(v1, v2)
		return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z
	end,
	Cross = function(v1, v2)
		local cross = Vec3()
		cross.x = v1.y*v2.z - v1.z*v2.y
		cross.y = v1.z*v2.x - v1.x*v2.z
		cross.z = v1.x*v2.y - v1.y*v2.x
		return cross
	end,
	AngleVectors = function(angles)
		local f = Vec3()
		local r = Vec3()
		local u = Vec3()
		ffi.C.AngleVectors(angles, f, r ,u)
		return f, r, u
	end
}
Vec3 = ffi.typeof("Vec3")
local Vec3_mt = {
    __eq = function(self, rhs)
		if not ((hrs == nil) == (hrs == nil)) then return false end
		if hrs == nil then return false end
        return self.x == rhs.x and self.y == rhs.y and self.z == rhs.z
    end,
	__add = function (lhs, rhs)
		local v = lhs.copy
		if (type(rhs) == "number") then
			v.x = v.x+rhs
			v.y = v.y+rhs
			v.z = v.z+rhs
		else
			v.x = v.x+rhs.x
			v.y = v.y+rhs.y
			v.z = v.z+rhs.z
		end
		return v
	end,
	__sub = function (lhs, rhs)
		local v = lhs.copy
		if (type(rhs) == "number") then
			v.x = v.x-rhs
			v.y = v.y-rhs
			v.z = v.z-rhs
		else
			v.x = v.x-rhs.x
			v.y = v.y-rhs.y
			v.z = v.z-rhs.z
		end
		return v
	end,
	__mul = function (lhs, rhs)
		local v = lhs.copy
		if (type(rhs) == "number") then
			v.x = v.x*rhs
			v.y = v.y*rhs
			v.z = v.z*rhs
		else
			v.x = v.x*rhs.x
			v.y = v.y*rhs.y
			v.z = v.z*rhs.z
		end
		return v
	end,
	__div = function (lhs, rhs)
		local v = lhs.copy
		if (type(rhs) == "number") then
			v.x = v.x/rhs
			v.y = v.y/rhs
			v.z = v.z/rhs
		else
			v.x = v.x/rhs.x
			v.y = v.y/rhs.y
			v.z = v.z/rhs.z
		end
		return v
	end,
	__unm = function (lhs)
		local v = lhs.copy
		v.x = -v.x
		v.y = -v.y
		v.z = -v.z
		return v
	end,
	__tostring = function (v)
		return "("..v.x..", "..v.y..", "..v.z..")"
	end,
	__index = function (table, key)
		if key == "copy" then
			local v = Vec3()
			v.x = table.x
			v.y = table.y
			v.z = table.z
			return v
		end
		local func = vec3propertyfuncs[key]
		if func then
			return func(table)
		end
		func = vec3funcs[key]
		if func then
			return func
		end
		return nil
	end
}
ffi.metatype(Vec3, Vec3_mt);
