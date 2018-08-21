function CreateBloodSplat(origin, minradius, maxradius, opacity)
	local splat = CreateDLight(math.random(-10000000, -1000000))
	splat.issplat = true
	splat.origin = origin
	splat.radius = (minradius+math.random()*(maxradius-minradius))
	splat.die = cl.time
	splat.color = Vec3(math.random(0.7, 0.9), math.random()*0.05, math.random()*0.05)
	splat.splatopacity = opacity
	return
end

function CreateBloodEmitter(coloroffset)
	local emitter = NewParticleEmitter()
	emitter.randomcolorcount = 7
	for i=1,7 do
		emitter.randomcolors[i-1] = 67+coloroffset+i-1
	end
	emitter.rate = 100

	emitter.minlifespan = 0.5
	emitter.maxlifespan = 1

	emitter.mingravityscale = 5
	emitter.maxgravityscale = 10

	emitter.sizevariation = 0.5
	local size = 1
	--if math.random() > 0.9 then
		--size = 2+math.random()
	--end

	emitter.minsize = size
	emitter.maxsize = size
	emitter.mingravityscale = emitter.mingravityscale*size
	emitter.maxgravityscale = emitter.maxgravityscale*size

	emitter.mindrag = 1.0/(size*2)
	emitter.maxdrag = emitter.mindrag

	return emitter
end

function CreateImpactSprays(origin, hitnormal, dirspread, spread)
	local emitter = CreateBloodEmitter(0)
	emitter.ticker = 0.2
	emitter.rate = 100
	--emitter.minsize = emitter.minsize*1
	--emitter.maxsize = emitter.maxsize*1.5
	emitter.minlifespan = 0.1
	emitter.maxlifespan = 0.8
	emitter.randomvelocity = Vec3(200, 200, 200)*spread
	emitter.velocity = (hitnormal+Vec3.random*dirspread).normalized*math.randomrange(150, 300)
	emitter.origin = origin
	emitter.originoffset = Vec3(2, 2, 2)
	emitter.mindrag = emitter.mindrag*5
	emitter.maxdrag = emitter.mindrag
	StartEmitter(emitter, 0.05+math.random()*0.15)

	local splatemitter = CreateBloodEmitter(2)
	splatemitter.ticker = 0.2
	emitter.rate = 100
	splatemitter.minlifespan = 0.1
	splatemitter.maxlifespan = 0.6
	splatemitter.randomvelocity = Vec3(400, 400, 400)*spread
	splatemitter.velocity = (hitnormal+Vec3.random*dirspread).normalized*math.randomrange(100, 200)
	splatemitter.origin = origin
	splatemitter.originoffset = Vec3(4, 4, 4)
	splatemitter.mindrag = splatemitter.mindrag*5
	splatemitter.maxdrag = splatemitter.mindrag
	StartEmitter(splatemitter, 0)
	return {emitter, splatemitter}
end
--[[
function CreateBloodSquirt(origin, velocity, emitrate, splatsize, lifespan)
	velocity = velocity.copy

	local emitter = CreateBloodEmitter(0)
	emitter.emittype = particleemittype.trail
	emitter.rate = 0.25
	emitter.minlifespan = 0.2
	emitter.maxlifespan = 0.4
	emitter.randomvelocity = Vec3(15, 15, 15)
	emitter.origin = origin
	emitter.originoffset = Vec3(4, 4, 4)

	local starttime = cl.time
	local func = nil
	func = function()
		if  cl.deltatime == 0 then
			return
		end
		local lastorigin = emitter.origin.copy
		emitter.origin = emitter.origin+velocity*cl.deltatime
		velocity = velocity+Vec3(0, 0, -800*cl.deltatime)
		emitter.trailend = lastorigin
		local trace = TraceLine(lastorigin, emitter.origin+velocity.normalized)
		if trace.fraction < 1 then
			emitter.origin = trace.endpos
			CreateBloodSplat(emitter.origin, splatsize*0.75, splatsize*1.5, 0.5)
			UnregisterTickFunc(func)
		end
		RunParticleEmitter(emitter)
		if cl.time-starttime > lifespan then
			UnregisterTickFunc(func)
		end
	end
	RegisterTickFunc(func)
end
]]
function CreateMinigib(origin, velocity, model, splatsize, lifespan)
	local entity = CreateModelDrawEntity(model)
	entity.origin = origin
	entity.velocity = velocity
	entity.angles = Vec3.random*180

	local emitter = CreateBloodEmitter(0)
	emitter.emittype = particleemittype.trail
	emitter.rate = 0.5
	emitter.minlifespan = 0.2
	emitter.maxlifespan = 0.8
	emitter.randomvelocity = Vec3(15, 15, 15)
	emitter.origin = origin
	emitter.originoffset = Vec3(4, 4, 4)

	local starttime = cl.time
	local func = nil
	func = function()
		if  cl.deltatime == 0 then
			--return
		end

		local lastorigin = entity.origin.copy
		entity.origin = entity.origin+entity.velocity*cl.deltatime
		entity.velocity = entity.velocity+Vec3(0, 0, -800*cl.deltatime)
		emitter.origin = entity.origin
		emitter.trailend = lastorigin
		local trace = TraceLine(lastorigin, entity.origin+entity.velocity.normalized)
		if trace.fraction < 1 then
			entity.origin = trace.endpos
			CreateBloodSplat(entity.origin, splatsize*0.75, splatsize*1.5, 1)
			UnregisterTickFunc(func)
		end
		RunParticleEmitter(emitter)
		if not (entity.model == nil) then
			DrawEntity(entity)
		end
		if cl.time-starttime > lifespan then
			UnregisterTickFunc(func)
		end
	end
	RegisterTickFunc(func)

	return {entity = entity, emitter = emitter, func = func}
end
