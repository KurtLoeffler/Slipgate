
function SpawnGib(origin, modelname, dm)
	local gib = qc.spawn().entity
	gib.origin.vector = origin
	qc.setmodel(gib, modelname)
	qc.setsize(gib, Vec3(0, 0, 0), Vec3(0, 0, 0))
	gib.velocity = qc.VelocityForDamage(dm).vector
	gib.movetype.number = qc.MOVETYPE_BOUNCE.number
	gib.solid.number = qc.SOLID_NOT.number
	gib.avelocity.vector.x = math.random()*600-300
	gib.avelocity.vector.y = math.random()*600-300
	gib.avelocity.vector.z = math.random()*600-300
	gib.think = qc.SUB_Remove
	gib.ltime = qc.time.number
	--gib.nextthink = qc.time.number+60*60
	gib.nextthink = qc.time.number+10+math.random()*10
	gib.frame = 0
	gib.flags = 0
	SendRPC("AttachGibSplat", gib.id, 25)

	return gib
end

function ThrowGib(modelname, dm)
	local self = qc.self.entity
	SpawnGib(self.origin.vector, modelname, dm)
end
Override("ThrowGib")
