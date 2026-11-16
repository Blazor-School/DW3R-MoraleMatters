-- A broken corps loses its weapons and splits between collapse and flight.
local InfantrySlump = {}
local cowerDistance, speedBoost, minSprintSpeed = 2800,1.55,780
local retreatPoseDistanceSquared=3600^2
local scanSeconds, startsPerScan = 0.16,8
local hitRadiusSquared, hitReactionSeconds = 1500^2,0.6
local function valid(o) return o and o:IsValid() end
local function reactingToHit(actor)
    return actor:IsNowHit() or actor:IsNowDamageMotion()
end
local function mounted(actor)
    return actor:IsRiding() or actor:IsNowRideMotion()
end
local function planarSquared(a,b)
    return (a.X-b.X)^2+(a.Y-b.Y)^2
end

function InfantrySlump.new(pose,bridge,log)
    local active, retry, threats, observedHits = {},{},{},{}
    local context, generation, clock, nextScan
    local gameplay,navigation,poseStartsRemaining,riseStartsRemaining,flightStartsRemaining
    local order,firstRow={},1
    local function cleanup(label,action)
        local ok,err=pcall(action)
        if not ok then log("Infantry "..label.." cleanup: "..tostring(err)) end
        return ok
    end

    local function restoreMode(row)
        if valid(row.movement) and row.movement.MovementMode==0 then row.movement:SetMovementMode(row.mode,0) end
    end

    local function stopPath(row,riding)
        local actor,movement=row.actor,row.movement
        if row.running then
            row.running=false
            bridge(20,row.key,row.object,0)
            if valid(actor) and not actor:IsDead() then
                actor:AddMovementInputKeep({X=0,Y=0,Z=0},0,0)
                actor:ConsumeMovementInputVector()
                if not riding then actor:StopAIMovement() end
            end
        end
        if row.rotation and valid(movement) then
            local original=row.rotation
            if valid(actor) then actor.bUseControllerRotationYaw=original.controllerYaw end
            movement.bUseControllerDesiredRotation=original.desired
            movement.bOrientRotationToMovement=original.orient
            movement.RotationRate=original.rate
            if movement.MaxWalkSpeed==row.appliedSpeed then movement.MaxWalkSpeed=original.speed end
            row.rotation=nil
        end
    end

    local function stop(row)
        row.stopping=true
        cleanup("pose",function() pose.stop(row) end)
        cleanup("movement",function()
            local alive=valid(row.actor) and not row.actor:IsDead()
            local riding=alive and mounted(row.actor)
            stopPath(row,riding)
            if alive and not riding then restoreMode(row) end
        end)
        local restored=true
        for _,weapon in pairs(row.weapons) do
            if not cleanup("weapon",function()
                if valid(weapon.component) and weapon.component.bHiddenInGame~=weapon.hidden then
                    weapon.component:SetHiddenInGame(weapon.hidden,false)
                end
            end) then restored=false end
        end
        -- Keep the native attack block through cleanup and the entire get-up animation.
        if not restored then return false end
        if row.disarmed then bridge(22,row.key,row.object,0) end
        return true
    end

    local function clear()
        for key,row in pairs(active) do if stop(row) then active[key]=nil end end
        if context then pose.reset() end
        retry,threats,observedHits,context,clock,nextScan={},{},{},nil,nil,nil
        order,firstRow={},1
    end

    local function observe(manager,players,groups,epoch)
        if epoch~=generation then
            clear()
            generation=epoch
        end
        context={manager=manager,players=players,groups=groups}
    end

    local function nearestPlayer(position)
        local best,nearest=math.huge,nil
        for _,player in ipairs(context.players) do
            local d=planarSquared(position,player)
            if d<best then best,nearest=d,player end
        end
        return best,nearest
    end

    local function reportHit(actor,group,now)
        local attacker=actor:GetLastAttackedMeCharacter()
        local position=actor:K2_GetActorLocation()
        local hit={position=position,source=attacker,attacker=valid(attacker) and attacker:K2_GetActorLocation() or nil,expires=now+hitReactionSeconds}
        if group then threats[group.corps]=hit end
        return hit
    end

    local function flightThreats(row,position,player)
        local group=context.groups[row.id]
        local attacker=row.escape and row.escape.source
        local threat=valid(attacker) and attacker:K2_GetActorLocation() or row.escape and row.escape.attacker
        if not threat and group and group.side==1 then threat=player end
        if not threat and group then
            local best=math.huge
            for _,opponent in pairs(context.groups) do
                if opponent.alive and opponent.side~=group.side then
                    local point={X=opponent.x,Y=opponent.y}
                    local distance=planarSquared(position,point)
                    if distance<best then best,threat=distance,point end
                end
            end
        end
        local avoidPlayer=group and group.side==1 and player or nil
        return threat,avoidPlayer
    end

    local function movesAway(position,dx,dy,threat)
        if not threat then return true end
        -- The distance to a threat must grow along the entire segment.
        return dx*(position.X-threat.X)+dy*(position.Y-threat.Y)>=-0.00001
    end

    local function awayFromBoth(position,dx,dy,threat,player)
        return movesAway(position,dx,dy,threat) and movesAway(position,dx,dy,player)
    end

    local function face(actor,position,target)
        if not target or planarSquared(position,target)<1 then return end
        actor:K2_SetActorRotation({Pitch=0,Yaw=math.deg(math.atan(target.Y-position.Y,target.X-position.X)),Roll=0},false)
    end

    local function beginStationary(row,now,style)
        local arriving=row.running
        if arriving then pose.prepareArrival(row) end
        stopPath(row)
        row.actor:StopAIMovement()
        row.actor:ConsumeMovementInputVector()
        row.movement:StopMovementImmediately()
        row.movement:DisableMovement()
        row.style=style
        row.flightQueued=nil
        row.poseMode=nil
        row.poseStart,row.held=now,false
        row.rise,row.rebindAfter=nil,nil
        row.hitVisibleAt=nil
        row.escape,row.path=nil,nil
        row.arriving=arriving
        return true
    end

    local function requestFlight(row,now,position,distance,threat,player,detour)
        navigation=navigation or StaticFindObject("/Script/NavigationSystem.Default__NavigationSystemV1")
        if not valid(navigation) then return false end
        local yaw=math.rad(row.actor:K2_GetActorRotation().Yaw)
        local dx,dy=-math.cos(yaw),-math.sin(yaw)
        if threat and planarSquared(position,threat)>1 then
            local length=math.sqrt(planarSquared(position,threat))
            dx,dy=(position.X-threat.X)/length,(position.Y-threat.Y)/length
        end
        if player and threat and player~=threat and planarSquared(position,player)>1 then
            local length=math.sqrt(planarSquared(position,player))
            local sumX,sumY=dx+(position.X-player.X)/length,dy+(position.Y-player.Y)/length
            if sumX*sumX+sumY*sumY>0.001 then dx,dy=sumX,sumY end
        end
        local direction=math.atan(dy,dx)
        local side=row.member%2==0 and 1 or -1
        local offsets=detour and {45*side,-45*side,90*side,-90*side} or {0,45*side,-45*side,90*side,-90*side}
        local length=row.escape and 1800 or math.max(1200,cowerDistance-math.sqrt(distance)+650)
        for _,offset in ipairs(offsets) do
            local angle=direction+math.rad(offset)
            local x,y=length*math.cos(angle),length*math.sin(angle)
            if awayFromBoth(position,x,y,threat,player) then
                local goal={X=position.X+x,Y=position.Y+y,Z=position.Z}
                local path=navigation:FindPathToLocationSynchronously(row.actor,position,goal,row.actor,nil)
                if valid(path) then
                    local points=path.PathPoints
                    local route,previous={},position
                    for index=2,#points do
                        local point=points[index]
                        if not awayFromBoth(previous,point.X-previous.X,point.Y-previous.Y,threat,player) then break end
                        previous={X=point.X,Y=point.Y,Z=point.Z}
                        route[#route+1]=previous
                    end
                    -- Partial paths are useful if they still make a meaningful escape.
                    if #route>0 and planarSquared(position,previous)>=550^2 then
                        row.path,row.pathIndex=route,1
                        row.position,row.lastProgress=position,now
                        return true
                    end
                end
            end
        end
        return false
    end

    local function beginFlight(row,now)
        if flightStartsRemaining==0 or poseStartsRemaining==0 then
            row.flightQueued=true
            return true
        end
        flightStartsRemaining=flightStartsRemaining-1
        row.flightQueued=nil
        local actor,movement=row.actor,row.movement
        local position=actor:K2_GetActorLocation()
        local distance,player=nearestPlayer(position)
        if not row.escape and distance>=cowerDistance^2 then return beginStationary(row,now,row.returnStyle) end
        local threat,avoidPlayer=flightThreats(row,position,player)
        if not requestFlight(row,now,position,distance,threat,avoidPlayer,false)
            or bridge(20,row.key,row.object,1)~=1 then return beginStationary(row,now,row.returnStyle) end

        row.running,row.leaseUntil=true,now+0.1
        poseStartsRemaining=poseStartsRemaining-1
        if not pose.startFlight(row) then
            stopPath(row)
            return beginStationary(row,now,row.returnStyle)
        end
        local rotation=movement.RotationRate
        row.rotation={controllerYaw=actor.bUseControllerRotationYaw,desired=movement.bUseControllerDesiredRotation,
            orient=movement.bOrientRotationToMovement,rate={Pitch=rotation.Pitch,Yaw=rotation.Yaw,Roll=rotation.Roll},speed=movement.MaxWalkSpeed}
        row.sprintSpeed=math.max(minSprintSpeed,row.rotation.speed*speedBoost)
        row.appliedSpeed=row.sprintSpeed
        row.detour=false
        row.poseMode=nil
        actor.bUseControllerRotationYaw=false
        movement.bUseControllerDesiredRotation=false
        movement.bOrientRotationToMovement=false
        actor:StopAIMovement()
        actor:ConsumeMovementInputVector()
        movement:StopMovementImmediately()
        restoreMode(row)
        movement.MaxWalkSpeed=row.sprintSpeed
        face(actor,position,row.path[1])
        return true
    end

    local function updateFlight(row,now)
        local actor,movement=row.actor,row.movement
        local position=actor:K2_GetActorLocation()
        local distance,player=nearestPlayer(position)
        local threat,avoidPlayer=flightThreats(row,position,player)
        local waypoint=row.path[row.pathIndex]
        while waypoint and planarSquared(position,waypoint)<=75^2 do
            row.pathIndex=row.pathIndex+1
            waypoint=row.path[row.pathIndex]
        end
        if not waypoint or not row.escape and distance>=cowerDistance^2 then
            return beginStationary(row,now,row.returnStyle)
        end
        if now>=row.leaseUntil then
            if bridge(20,row.key,row.object,1)~=1 then return beginStationary(row,now,row.returnStyle) end
            row.leaseUntil=now+0.1
        end
        if planarSquared(position,row.position)>=25^2 then row.position,row.lastProgress=position,now end
        local dx,dy=waypoint.X-position.X,waypoint.Y-position.Y
        if not awayFromBoth(position,dx,dy,threat,avoidPlayer) or now-row.lastProgress>=0.55 then
            if row.detour then return beginStationary(row,now,row.returnStyle) end
            if flightStartsRemaining==0 then
                actor:AddMovementInputKeep({X=0,Y=0,Z=0},0,0)
                actor:ConsumeMovementInputVector()
                movement:StopMovementImmediately()
                return true
            end
            flightStartsRemaining=flightStartsRemaining-1
            if not requestFlight(row,now,position,distance,threat,avoidPlayer,true) then
                return beginStationary(row,now,row.returnStyle)
            end
            row.detour=true
            waypoint=row.path[1]
            dx,dy=waypoint.X-position.X,waypoint.Y-position.Y
        end
        local velocity=actor:GetVelocity()
        if not pose.owns(row) and not pose.activate(row) then return false end
        if dx*velocity.X+dy*velocity.Y<0 or not awayFromBoth(position,velocity.X,velocity.Y,threat,avoidPlayer) then
            movement:StopMovementImmediately()
            velocity={X=0,Y=0}
        end
        face(actor,position,waypoint)
        local length=math.sqrt(dx*dx+dy*dy)
        local remaining=row.pathIndex==#row.path and length-75 or math.huge
        if not row.escape then remaining=math.min(remaining,cowerDistance-math.sqrt(distance)) end
        row.appliedSpeed=row.sprintSpeed*math.max(0.35,math.min(1,remaining/235))
        if movement.MaxWalkSpeed~=row.appliedSpeed then movement.MaxWalkSpeed=row.appliedSpeed end
        if length>1 then actor:AddMovementInputKeep({X=dx/length,Y=dy/length,Z=0},1,0.22) end
        pose.flightRate(row,math.sqrt(velocity.X^2+velocity.Y^2))
        return true
    end

    local function hideWeapons(row)
        local weapons=row.actor.WeaponComponents
        local index=1
        while index<=#weapons do
            local weapon=weapons[index]
            if valid(weapon) then
                local key=weapon:GetAddress()
                if not row.weapons[key] then row.weapons[key]={component=weapon,hidden=weapon.bHiddenInGame} end
                if not weapon.bHiddenInGame then weapon:SetHiddenInGame(true,false) end
            end
            index=index+1
        end
    end

    local function selectSoldiers(now)
        local remaining=startsPerScan
        local currentHits={}
        local function inspectArmy(army)
            local index=1
            -- Keep the array wrapper alive and avoid UE4SS's large-array ForEach callback.
            while index<=#army do
                local actor=army[index]
                index=index+1
                if valid(actor) then
                    local key=actor:GetAddress()
                    if not active[key] and (not retry[key] or now>=retry[key]) then
                        local phase,id,member,object=bridge(19,key,1)
                        local hit=(phase==4 or phase==-1) and reactingToHit(actor)
                        if hit then
                            currentHits[key]=true
                            if not observedHits[key] and actor:GetHealth()<actor:GetPrevHealth() then
                                local group=id and context.groups[id]
                                if group then reportHit(actor,group,now) end
                            end
                        end
                        if remaining>0 and phase==4 and actor:IsUnitized() and not actor:IsDead() and not hit and not mounted(actor) and not actor:IsNowAttack() then
                            local mesh,movement=actor.Mesh,actor.CharacterMovement
                            if valid(mesh) and valid(movement) and (movement.MovementMode==1 or movement.MovementMode==2) then
                                local row={key=key,actor=actor,mesh=mesh,movement=movement,mode=movement.MovementMode,id=id,member=member,object=object,weapons={},nextWeaponCheck=now+0.18}
                                remaining=remaining-1
                                local ok,started=pcall(function()
                                    row.health=actor:GetHealth()
                                    row.disarmed=bridge(22,row.key,row.object,1)==1
                                    if not row.disarmed then return false end
                                    hideWeapons(row)
                                    row.returnStyle=(id*13+member)%4==0 and "sit" or "cower"
                                    if row.returnStyle=="sit" then return beginStationary(row,now,"sit") end
                                    return beginFlight(row,now)
                                end)
                                if ok and started then active[key]=row
                                else
                                    retry[key]=now+2
                                    if not stop(row) then active[key]=row end
                                    if not ok then log("Infantry pose unavailable: "..tostring(started)) end
                                end
                            end
                        end
                    end
                end
            end
        end
        inspectArmy(context.manager.m_arpSortedUnitizedEnemyList)
        inspectArmy(context.manager.m_arpSortedUnitizedAllyList)
        observedHits=currentHits
    end

    local function updateStationary(row,phase,now)
        if phase~=4 or row.escape or row.rise then
            if row.poseMode==row.style then
                if not pose.owns(row) then
                    row.rise=now
                elseif not row.rise then
                    if riseStartsRemaining==0 then return true end
                    riseStartsRemaining=riseStartsRemaining-1
                    local time=not row.held and math.min(now-row.poseStart,row.poseDuration) or nil
                    row.rise=now+pose.rise(row,row.escape and 2.2 or 1,time)
                end
                if now<row.rise then return true end
                pose.deactivate(row)
                row.poseMode=nil
                restoreMode(row)
            end
            if phase~=4 then return false end
            if reactingToHit(row.actor) or row.hitVisibleAt and now<row.hitVisibleAt+0.08 then return true end
            row.hitVisibleAt=nil
            row.style,row.rise,row.held=nil,nil,nil
            hideWeapons(row)
            return beginFlight(row,now)
        end
        if row.movement.MovementMode~=0 then
            row.actor:StopAIMovement()
            row.actor:ConsumeMovementInputVector()
            row.movement:StopMovementImmediately()
            row.movement:DisableMovement()
        end
        if row.poseMode~=row.style then
            if not pose.arrivalReady(row) then return true end
            if not row.arrival then
                if poseStartsRemaining==0 then return true end
                if not row.mesh:WasRecentlyRendered(0.5) and (not row.arriving
                    or nearestPlayer(row.actor:K2_GetActorLocation())>retreatPoseDistanceSquared) then return true end
                poseStartsRemaining=poseStartsRemaining-1
            end
            local duration,offset=pose.start(row)
            if not duration then return false end
            row.poseStart,row.poseMode,row.poseDuration=now-offset,row.style,duration
        end
        if not pose.owns(row) then
            if row.rebindAfter and now<row.rebindAfter then return true end
            row.rebindAfter=now+0.12
            if not pose.activate(row) then return false end
        end
        if not row.held and now>=row.poseStart+row.poseDuration then
            pose.hold(row)
            row.held=true
        end
        return true
    end

    local function advance(row,key,now)
        local actor=row.actor
        if row.stopping or not valid(actor) or not valid(row.movement) or not actor:IsUnitized() or actor:IsDead() or mounted(actor) then return false end
        local phase,id,member,object=bridge(19,key)
        if id~=row.id or member~=row.member or object~=row.object then return false end
        local damaged=false
        if not row.nextCheck or now>=row.nextCheck then
            row.nextCheck=now+0.08
            local health=actor:GetHealth()
            damaged=health<row.health
            row.health=health
            local group=context.groups[id]
            if damaged and phase==4 then
                row.escape=reportHit(actor,group,now)
                row.lastThreat=row.escape
                if row.style then
                    -- Let the game's hit animation take over before scheduling the retreat.
                    pose.deactivate(row)
                    row.poseMode,row.rise=nil,nil
                    row.hitVisibleAt=now
                    restoreMode(row)
                end
            elseif phase==4 and row.style and group then
                local hit=threats[group.corps]
                if hit and hit~=row.lastThreat and now<=hit.expires
                    and planarSquared(actor:K2_GetActorLocation(),hit.position)<=hitRadiusSquared then
                    row.escape,row.lastThreat=hit,hit
                end
            end
            if now>=row.nextWeaponCheck then
                hideWeapons(row)
                row.nextWeaponCheck=now+0.18
            end
        end
        if row.style then
            if not updateStationary(row,phase,now) then return false end
            return true
        end
        if phase~=4 then return false end
        if damaged or reactingToHit(actor) then
            row.resumeAfter=now+0.12
            if not row.suspended then
                row.suspended=true
                stopPath(row)
                pose.deactivate(row)
                row.poseMode=nil
                restoreMode(row)
            end
            return true
        end
        if row.suspended then
            if now<row.resumeAfter then return true end
            row.suspended=false
            hideWeapons(row)
            return beginFlight(row,now)
        end
        if row.flightQueued then return beginFlight(row,now) end
        return updateFlight(row,now)
    end

    local function update()
        if not context then
            if next(active) then clear() end
            return
        end
        if not valid(context.manager) or bridge(0)~=1 then clear(); return end
        gameplay=gameplay or StaticFindObject("/Script/Engine.Default__GameplayStatics")
        if not valid(gameplay) then return end
        local now=gameplay:GetTimeSeconds(context.manager)
        if clock and now<clock then clear(); return end
        if now==clock then return end
        clock=now
        -- Spread component setup and synchronous path searches across game frames.
        poseStartsRemaining,riseStartsRemaining,flightStartsRemaining=1,2,1
        -- Give each soldier a turn to start its animation.
        local count=0
        for key in pairs(active) do
            count=count+1
            order[count]=key
        end
        for i=0,count-1 do
            local key=order[(firstRow+i-1)%count+1]
            local row=active[key]
            local animating=row.style and (row.arrival or row.rise or row.poseMode and not row.held)
            if animating or row.flightQueued or not row.nextUpdate or now>=row.nextUpdate then
                local ok,keep=pcall(advance,row,key,now)
                if ok and keep then
                    row.nextUpdate=now+(row.running and 0.025 or 0.08)
                else
                    if stop(row) then
                        active[key]=nil
                        retry[key]=now+0.4
                    end
                    if not ok then log("Infantry pose stopped: "..tostring(keep)) end
                end
            end
        end
        firstRow=count>0 and firstRow%count+1 or 1
        if not nextScan or now>=nextScan then
            nextScan=now+scanSeconds
            if bridge(19,0)==1 and pose.ready(log) then selectSoldiers(now) else retry,threats,observedHits={},{},{} end
        end
    end

    return {observe=observe,update=update,clear=clear}
end

return InfantrySlump
