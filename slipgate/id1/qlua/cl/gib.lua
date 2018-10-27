
function GibSplat(entity, radius)
	if entity.velocity == Vec3(0, 0, 0) then
		return
	end
	entity.bag.gibinfo = entity.bag.gibinfo or {
		accumdist = 0
	}
	local gibinfo = entity.bag.gibinfo

	gibinfo.accumdist = gibinfo.accumdist+entity.velocity.length*cl.deltatime
	if gibinfo.accumdist >= 5 then
		CreateBloodSplat(entity.origin, radius*0.75, radius*1.25, 0.3)
		gibinfo.accumdist = 0
	end
end

function AttachGibSplat(id, radius)
	GetBagForEntityID(id).tick = function(e) GibSplat(e, radius) end
end
