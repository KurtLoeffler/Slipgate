
for k,v in pairs(_G) do
	if type(k) == "string" and type(v) == "function" then
		local info = { index = #qc.luafunctions+1, name = k, func = v }
		table.insert(qc.luafunctions, info)
	end
end

--Add indexes for name and values
for k,v in pairs(qc.luafunctions) do
	qc.luafunctions[v.name] = v
	qc.luafunctions[v.func] = v
end
