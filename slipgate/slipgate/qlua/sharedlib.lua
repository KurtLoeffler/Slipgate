ffi = sysrequire("ffi")
require("nativetypes")
require("serialize")

function string.starts(String, Start)
   return string.sub(String,1,string.len(Start))==Start
end

function string.ends(String, End)
   return End=='' or string.sub(String,-string.len(End))==End
end

function string.split(inputstr, sep)
        if sep == nil then
                sep = "%s"
        end
        local t={} ; i=1
        for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
                t[i] = str
                i = i + 1
        end
        return t
end

function table.pack(...)
	return { n = select("#", ...), ... }
end

function math.randomrange(min, max)
    return min+math.random()*(max-min)
end
require("nativetypes")
