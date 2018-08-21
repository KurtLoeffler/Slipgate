function TraceAttack(damage, dir)
	qc.nooverride.TraceAttack(damage, dir)

	local hitentity = qc.trace_ent.entity
	if hitentity.takedamage.number == 0 then
		return
	end
	local hitpoint = qc.trace_endpos.vector
	local hitnormal = qc.trace_plane_normal.vector.normalized
	local entitycenter = (hitentity.absmin.vector+hitentity.absmax.vector)/2
	local roundhitnormal = (hitpoint-entitycenter).normalized
	hitnormal.x = roundhitnormal.x
	hitnormal.y = roundhitnormal.y
	SendRPC("OnTraceAttack", damage.number, hitpoint, dir.vector.normalized, hitnormal, hitentity.id)
end
Override("TraceAttack")

--Disable the boring quake blood
function SpawnBlood(org, vel, damage)

end
Override("SpawnBlood")
