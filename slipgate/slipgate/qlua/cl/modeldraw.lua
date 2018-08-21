
function CreateModelDrawEntity(model)
	local entity = Entity()
	entity.previousenvlightcolor = Vec3(-1, -1, -1)
	entity.model = model
	return entity
end

local model = GetModel("progs/gib1.mdl")
local modeldrawentity = CreateModelDrawEntity(model)
modeldrawentity.origin = Vec3(483, 307, 25)

local function ModelTick()
	DrawEntity(modeldrawentity)
end
RegisterTickFunc(ModelTick)
