-- world_swapper.lua
-- Attach to an entity to swap it between worlds "main" and "scene2".
-- Worlds are identified by their scene file stem (e.g. "main.scene" → "main").
-- Requires both scenes to be open as tabs and in Play mode.

local WORLD_A = "Main"
local WORLD_B = "scene2"

function OnStart(entity, world)
    Log.Info("WorldSwapper started in world: '" .. world:GetName() .. "'")
end

function OnUpdate(entity, world, dt)
    if not Actions.Triggered("Game.SwapWorld") then
        return
    end

    local current = world:GetName()
    local target = (current == WORLD_A) and WORLD_B or WORLD_A

    Log.Info("WorldSwapper: '" .. current .. "' → '" .. target .. "'")
    entity:TransferTo(target)
end

