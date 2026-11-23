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
local function grounded(movement)
    return movement.MovementMode==1 or movement.MovementMode==2
end
local function mounted(actor)
    return actor:IsRiding() or actor:IsNowRideMotion()
end
local function planarSquared(a,b)
    return (a.X-b.X)^2+(a.Y-b.Y)^2
end

function InfantrySlump.new(pose,bridge,log,liveEvidence)
    local active, retry, threats, observedHits = {},{},{},{}
    local context, generation, clock, nextScan
    local gameplay,navigation,stage,poseStartsRemaining,riseStartsRemaining,flightStartsRemaining
    local order,firstRow={},1
    local logged={}
    local evidenceCount=0
    local function logSoldier(row,event,detail)
        if not liveEvidence or evidenceCount>=320 then return end
        evidenceCount=evidenceCount+1
        log(string.format("Live evidence: infantry %s generation=%s group=%d member=%d object=%s style=%s %s",event,tostring(generation),row.id,row.member,tostring(row.object),tostring(row.style or "flight"),detail))
        if evidenceCount==320 then log("Live evidence: infantry detail limit reached for this battle (320 events).") end
    end
    local function logPose(row,action)
        if not liveEvidence or logged[action]==generation then return end
        logged[action]=generation
        log(string.format("Live evidence: infantry %s group=%d member=%d",action,row.id,row.member))
    end
    local function cleanup(label,action)
        local ok,err=pcall(action)
        if not ok then log("Infantry "..label.." cleanup: "..tostring(err)) end
        return ok
    end

    local function releaseStationary(row)
        if row.stationaryLease then
            bridge(20,row.key,row.object,0)
            row.stationaryLease=nil
        end
        if row.stationarySpeed and valid(row.movement) and row.movement.MaxWalkSpeed==0 then
            row.movement.MaxWalkSpeed=row.stationarySpeed
        end
        row.stationarySpeed=nil
        if row.stationaryRotation then
            local original=row.stationaryRotation
            if valid(row.actor) then row.actor.bUseControllerRotationYaw=original.controllerYaw end
            if valid(row.movement) then
                row.movement.bUseControllerDesiredRotation=original.desired
                row.movement.bOrientRotationToMovement=original.orient
                row.movement.RotationRate=original.rate
            end
            row.stationaryRotation=nil
        end
    end

    local function releaseGround(row)
        if row.ground and valid(row.movement) then
            local movement,original=row.movement,row.ground
            movement.bAlwaysCheckFloor=original.alwaysCheck
            movement.bProjectNavMeshWalking=original.project
            movement.bSweepWhileNavWalking=original.sweep
            movement.NavMeshProjectionInterval=original.interval
            movement.NavMeshProjectionInterpSpeed=original.interp
            movement.NavMeshProjectionHeightScaleUp=original.up
            movement.NavMeshProjectionHeightScaleDown=original.down
        end
        row.ground=nil
    end

    local function followGround(row,now)
        local movement=row.movement
        if not row.ground then
            row.ground={alwaysCheck=movement.bAlwaysCheckFloor,project=movement.bProjectNavMeshWalking,sweep=movement.bSweepWhileNavWalking,
                interval=movement.NavMeshProjectionInterval,interp=movement.NavMeshProjectionInterpSpeed,up=movement.NavMeshProjectionHeightScaleUp,down=movement.NavMeshProjectionHeightScaleDown}
            logSoldier(row,"terrain projection enabled","native movement mode retained")
        end
        -- The game selects NavWalking each frame. Project that navigation height
        -- onto collision geometry instead of repeatedly changing movement modes.
        movement.bAlwaysCheckFloor=true
        movement.bProjectNavMeshWalking=true
        movement.bSweepWhileNavWalking=true
        movement.NavMeshProjectionInterval=0.1
        movement.NavMeshProjectionInterpSpeed=0
        movement.NavMeshProjectionHeightScaleUp=1
        movement.NavMeshProjectionHeightScaleDown=1
        if row.ground.nextCheck and now<row.ground.nextCheck then return end
        row.ground.nextCheck=now+0.12
        if not valid(stage) then stage=FindFirstOf("StageManager") end
        if not valid(stage) then return end
        local actor=row.actor
        local position=actor:K2_GetActorLocation()
        local floor=stage:GetStageHeight(position)
        if not floor.bBlockingHit or not valid(actor.CapsuleComponent) then return end
        local halfHeight=actor.CapsuleComponent:GetScaledCapsuleHalfHeight()
        local height=floor.ImpactPoint.Z+halfHeight+(movement.MovementMode==1 and 2.15 or 0)
        local delta=height-position.Z
        -- Correct navigation-height errors locally; a missing floor or a real
        -- airborne reaction must never pull a soldier onto another level.
        if math.abs(delta)>1 and math.abs(delta)<=halfHeight+movement.MaxStepHeight then
            actor:K2_SetActorLocation({X=position.X,Y=position.Y,Z=height},true,{},false)
            if not row.ground.logged then
                row.ground.logged=true
                logSoldier(row,"ground contact corrected",string.format("deltaZ=%.2f mode=%s",delta,tostring(movement.MovementMode)))
            end
        end
    end

    local function holdFacing(row)
        local actor,movement=row.actor,row.movement
        local original=row.stationaryRotation
        if not original then
            local rate=movement.RotationRate
            original={controllerYaw=actor.bUseControllerRotationYaw,desired=movement.bUseControllerDesiredRotation,
                orient=movement.bOrientRotationToMovement,rate={Pitch=rate.Pitch,Yaw=rate.Yaw,Roll=rate.Roll},yaw=actor:K2_GetActorRotation().Yaw}
            row.stationaryRotation=original
            logSoldier(row,"stationary facing locked",string.format("yaw=%.1f",original.yaw))
        end
        actor.bUseControllerRotationYaw=false
        movement.bUseControllerDesiredRotation=false
        movement.bOrientRotationToMovement=false
        movement.RotationRate={Pitch=0,Yaw=0,Roll=0}
        local yaw=actor:K2_GetActorRotation().Yaw
        if math.abs((yaw-original.yaw+180)%360-180)>0.01 then
            actor:K2_SetActorRotation({Pitch=0,Yaw=original.yaw,Roll=0},false)
        end
    end

    local function stopPath(row,keepNativeMovement)
        local actor,movement=row.actor,row.movement
        if row.running then
            row.running=false
            bridge(20,row.key,row.object,0)
            if valid(actor) and not actor:IsDead() then
                actor:AddMovementInputKeep({X=0,Y=0,Z=0},0,0)
                actor:ConsumeMovementInputVector()
                if not keepNativeMovement then actor:StopAIMovement() end
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
            releaseStationary(row)
            releaseGround(row)
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
            evidenceCount=0
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
        followGround(row,now)
        -- Keep native floor checks and damage movement active while the soldier sits.
        row.stationarySpeed=row.movement.MaxWalkSpeed
        row.movement.MaxWalkSpeed=0
        row.style=style
        row.flightQueued=nil
        row.poseMode=nil
        row.poseStart,row.held=now,false
        row.rise,row.rebindAfter=nil,nil
        row.escape,row.path=nil,nil
        row.arriving=arriving
        row.stationaryLease=bridge(20,row.key,row.object,1)==1
        if not row.stationaryLease then return false end
        row.stationaryLeaseUntil=now+0.1
        holdFacing(row)
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
        releaseStationary(row)
        followGround(row,now)
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
        movement.MaxWalkSpeed=row.sprintSpeed
        face(actor,position,row.path[1])
        logPose(row,"flight")
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
                        if remaining>0 and phase==4 and actor:IsUnitized() and not actor:IsDead() and not hit and not mounted(actor) and not actor:IsNowAttack() and not actor:IsNoHit() then
                            local mesh,movement=actor.Mesh,actor.CharacterMovement
                            if valid(mesh) and valid(movement) and grounded(movement) then
                                local row={key=key,actor=actor,mesh=mesh,movement=movement,id=id,member=member,object=object,weapons={},nextWeaponCheck=now+0.18}
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
        -- Use the same expiring AI lease as flight, so native targeting cannot
        -- turn a stationary pose or restore its movement/rotation flags.
        if phase==4 and now>=row.stationaryLeaseUntil then
            if bridge(20,row.key,row.object,1)~=1 then return false end
            row.stationaryLeaseUntil=now+0.1
        end
        if phase~=4 or row.escape or row.rise then
            if row.poseMode==row.style then
                if not pose.owns(row) then
                    row.rise=now
                elseif not row.rise then
                    if riseStartsRemaining==0 then return true end
                    riseStartsRemaining=riseStartsRemaining-1
                    local time=not row.held and math.min(now-row.poseStart,row.poseDuration) or nil
                    row.rise=now+pose.rise(row,row.escape and 2.2 or 1,time)
                    if phase~=4 then logPose(row,"rally stand-up") end
                end
                if now<row.rise then return true end
                pose.deactivate(row)
                row.poseMode=nil
            end
            if phase~=4 then return false end
            row.style,row.rise,row.held=nil,nil,nil
            hideWeapons(row)
            return beginFlight(row,now)
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
            logPose(row,row.style)
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
        if liveEvidence and not row.stopping and valid(actor) and actor:IsDead() then
            logSoldier(row,"dead",string.format("hp=%.1f->%.1f",row.health,actor:GetHealth()))
        end
        if row.stopping or not valid(actor) or not valid(row.movement) or not actor:IsUnitized() or actor:IsDead() or mounted(actor) then return false end
        local phase,id,member,object=bridge(19,key)
        if id~=row.id or member~=row.member or object~=row.object then return false end
        local damaged=false
        if not row.nextCheck or now>=row.nextCheck then
            row.nextCheck=now+0.08
            local health=actor:GetHealth()
            damaged=health<row.health
            if damaged and liveEvidence then logSoldier(row,"damage",string.format("phase=%d hp=%.1f->%.1f mode=%s noHit=%s reaction=%s",phase,row.health,health,tostring(row.movement.MovementMode),tostring(actor:IsNoHit()),tostring(reactingToHit(actor)))) end
            row.health=health
            local group=context.groups[id]
            if damaged and phase==4 then
                row.escape=reportHit(actor,group,now)
                row.lastThreat=row.escape
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
        -- A horse can knock a soldier down without changing HP. Until the
        -- native reaction ends, the game owns both the pose and movement.
        local reacting,noHit=reactingToHit(actor),actor:IsNoHit()
        if damaged or reacting or not grounded(row.movement) or noHit then
            row.resumeAfter=now+0.12
            if liveEvidence then
                local state=string.format("mode=%s noHit=%s reaction=%s",tostring(row.movement.MovementMode),tostring(noHit),tostring(reacting))
                if not row.suspended or row.evidenceState~=state or now>=(row.evidenceAt or 0)+5 then
                    logSoldier(row,row.suspended and "native reaction state" or "yield to native reaction",string.format("time=%.2f hp=%.1f %s",now,row.health,state))
                    row.evidenceState,row.evidenceAt=state,now
                end
            end
            if not row.suspended then
                row.suspended=true
                stopPath(row,true)
                releaseStationary(row)
                releaseGround(row)
                pose.deactivate(row)
                row.poseMode,row.rise=nil,nil
            end
            return true
        end
        if row.suspended then
            if now<row.resumeAfter then return true end
            logSoldier(row,"native reaction ended",string.format("time=%.2f phase=%d hp=%.1f mode=%s noHit=%s",now,phase,row.health,tostring(row.movement.MovementMode),tostring(noHit)))
            row.suspended=false
            if phase~=4 then return false end
            if not row.style or row.escape then
                row.style=nil
                return beginFlight(row,now)
            end
            return beginStationary(row,now,row.style)
        end
        followGround(row,now)
        if row.style then
            holdFacing(row)
            if not updateStationary(row,phase,now) then return false end
            return true
        end
        if phase~=4 then return false end
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
