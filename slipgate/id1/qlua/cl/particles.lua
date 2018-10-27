
function PrintBag(bag, indent)
	for k,v in pairs(bag) do
		if not (type(v) == "table") then
			for i=1,indent do
				print(" ")
			end
			print("\""..k.."\" = "..v.."\n")
		else
			PrintBag(v, indent+1)
		end
	end
end

function StartEmitter(emitter, lifespan)
	if lifespan == 0 then
		RunParticleEmitter(emitter)
		return
	end
	local starttime = cl.time
	local func = nil
	func = function()
		if lifespan > 0 and cl.time-starttime > lifespan then
			UnregisterTickFunc(func)
		end
		RunParticleEmitter(emitter)
		--print(cl.time.."\n")
	end
	RegisterTickFunc(func)
end

function RunGibParticles(entity)
	--print(entity.id.."\n")
	--PrintBag(entity.rep, 0)
	local emitter = entity.bag.emitter
	if emitter == nil then
		emitter = NewParticleEmitter()
		entity.bag.emitter = emitter
		print(entity.id.." new emitter\n")
	end

	emitter.emittype = particleemittype.trail
	emitter.rate = 1
	emitter.randomcolorcount = 8
	for i=0,7 do
		emitter.randomcolors[i] = 67+i
	end

	emitter.mingravityscale = 1
	emitter.maxgravityscale = 3
	emitter.minlifespan = 1.5
	emitter.maxlifespan = 2.5
	if math.random() > 0.5 then
		emitter.minlifespan = emitter.minlifespan*0.5
		emitter.maxlifespan = emitter.maxlifespan*0.5
	end

	emitter.originoffset = Vec3(6, 6, 6)
	emitter.randomvelocity = Vec3(12, 12, 12)

	emitter.sizevariation = 0.5

	local size = 1
	if math.random() > 0.9 then
		size = 2+math.random()
	end

	emitter.minsize = size
	emitter.maxsize = size
	emitter.mingravityscale = emitter.mingravityscale*size
	emitter.maxgravityscale = emitter.maxgravityscale*size

	emitter.mindrag = 1.0/(size*2)
	emitter.maxdrag = emitter.mindrag

	emitter.origin = entity.origin
	emitter.trailend = emitter.origin-entity.velocity*cl.deltatime
	RunParticleEmitter(emitter)
end
