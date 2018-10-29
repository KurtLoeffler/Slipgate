function State(frame, nextthink)
	local self = qc.self.entity
	self.frame.number = frame
	self.nextthink.number = qc.time.number+0.1
	self.think = nextthink--qc.functiontoindex[nextthink]
end

function army_atk1()
	--local self = qc.self.entity
	--State(20, qc.army_atk2)
	--qc.ai_face();
end

function StartFrame()
	qc.nooverride.StartFrame()
end
Override("StartFrame")

function monster_army()
	local self = qc.self.entity
	qc.nooverride.monster_army()
	--self.health = 5
	--self.th_stand = qc.army_atk1
	--self.th_stand = army_atk1
	self.bag.prt = {}
	self.bag.prt.type = "e"
	self.bag.prt.rate = 20
end
Override("monster_army")

function army_pain()
	local self = qc.self.entity
	qc.nooverride.army_pain()
end
Override("army_pain")

function army_die()
	local self = qc.self.entity
	if (self.health.number < 0) then
		--self.health = self.health*20
	end
	qc.nooverride.army_die()
	--for i=1,10000 do
		--qc.nooverride.army_die()
	--end
	--qc.sound(self, 2, "player/udeath.wav", 1, 2)
	--for i=1,10000 do
	--	qc.nooverride.T_Damage(self, self, self, 10000)
	--end
end
Override("army_die")

function T_Damage(targ, inflictor, attacker, damage)
	qc.nooverride.T_Damage(targ, inflictor, attacker, damage)
	--SpawnGib(targ.entity.origin.vector, "progs/gib"..math.random(1, 3)..".mdl", -100)
end
Override("T_Damage")
