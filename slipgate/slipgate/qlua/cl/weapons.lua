
local minigib1 = GetModel("progs/gib1.mdl")

function UpdateMinigibDamage(entity, origin, damage)
	local ticker = entity.bag.minigibticker or 0
	ticker = ticker+damage
	local threshold = 15
	while ticker > threshold do
		local minigibvel = Vec3.random
		minigibvel.z = minigibvel.z+1
		minigibvel = minigibvel.normalized*200
		if math.random() > 0.5 then
			minigibvel = minigibvel*2
		end
		CreateMinigib(origin, minigibvel, minigib1, 30, 5)
		ticker = ticker-threshold
	end

	entity.bag.minigibticker = ticker
end

function OnTraceAttack(damage, origin, dir, hitnormal, entid)
	local entity = GetEntityByID(entid)

	CreateImpactSprays(origin, hitnormal, 0.2, 0.1)
	local exitorigin = origin
	if entity then
		local size = entity.maxs-entity.mins;
		local entradius = (size.x+size.y)/2
		exitorigin = exitorigin+dir*entradius
	end

	local exitdir = (dir+Vec3.random*0.4-hitnormal*2).normalized
	local trace = TraceLine(origin, origin+exitdir*math.random(200, 400), entity)
	if trace.fraction < 1 then
		CreateBloodSplat(trace.endpos, 30, 50, 0.75)
	end

	local exitemitters = CreateImpactSprays(exitorigin, exitdir, 0.5, 0.2)
	for k,v in pairs(exitemitters) do
		v.rate = v.rate*2
		v.velocity = v.velocity*2
	end

	for i=1,2 do
		local vec = (dir+Vec3.random*1.0).normalized
		if math.random() >= 0.5 then
			vec = -vec
		end
		vec.z = vec.z+1.0
		vec = vec*math.randomrange(100, 250)

		local rate = 0.0
		if math.random() > 0.7 then
			rate = 1
			--vec.z = vec.z*0.5
			vec = vec*2
			if math.random() > 0.75 then
				vec = vec*2
			end
		end
		local trail = CreateMinigib(origin, vec, nil, 30, 5).emitter
		trail.minlifespan = 0.2
		trail.maxlifespan = 0.4
		--trace = TraceLine(origin, origin+tvec*math.random(64, 200), entity)
		--if trace.fraction < 1 then
		--	CreateBloodSplat(trace.endpos, 20, 30, 1)
		--end
	end

	if entity then
		UpdateMinigibDamage(entity, origin, damage)
	end

end
