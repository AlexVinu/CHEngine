local speed = 5.0

function OnUpdate(entity, world, dt)
    local pos = entity:GetPosition()
    local dx, dz = 0, 0

    if Actions.Down("Game.Move.Forward")  then dz = dz - 1 end
    if Actions.Down("Game.Move.Backward") then dz = dz + 1 end
    if Actions.Down("Game.Move.Left")     then dx = dx - 1 end
    if Actions.Down("Game.Move.Right")    then dx = dx + 1 end

    if dx ~= 0 or dz ~= 0 then
        local len = math.sqrt(dx * dx + dz * dz)
        dx = dx / len * speed * dt
        dz = dz / len * speed * dt
        entity:SetPosition(pos.x + dx, pos.y, pos.z + dz)
    end
end
