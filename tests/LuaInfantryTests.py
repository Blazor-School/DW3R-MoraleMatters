"""Exercise the corps-wide infantry lifecycle with Lua engine stand-ins."""
from pathlib import Path
from lupa.lua54 import LuaRuntime

root = Path(__file__).resolve().parents[1]
LuaRuntime().execute(r"""
local Infantry = dofile(...)
local now, phase = 0, 0
local function valid() return true end
local function no() return false end
local function actor(id, excluded)
    local a={id=id,excluded=excluded,x=600,y=0,z=100,hp=100,hit=false,yaw=180,bUseControllerRotationYaw=true}
    local weapon={IsValid=valid,bHiddenInGame=false}
    function weapon:GetAddress() return id end
    function weapon:SetHiddenInGame(value) assert(a.disarmed); self.bHiddenInGame=value end
    local movement={IsValid=valid,MovementMode=2,MaxWalkSpeed=450,bUseControllerDesiredRotation=true,
        bOrientRotationToMovement=false,RotationRate={Pitch=0,Yaw=360,Roll=0},bAlwaysCheckFloor=false,
        bProjectNavMeshWalking=false,bSweepWhileNavWalking=false,NavMeshProjectionInterval=0.3,NavMeshProjectionInterpSpeed=12,
        NavMeshProjectionHeightScaleUp=0.67,NavMeshProjectionHeightScaleDown=1,MaxStepHeight=45}
    function movement:StopMovementImmediately() self.braked=true end
    function movement:SetMovementMode() error('Custom infantry must preserve the native movement mode') end
    a.Mesh={IsValid=valid,WasRecentlyRendered=function() return not a.offscreen end}
    a.CharacterMovement=movement
    a.CapsuleComponent={IsValid=valid,GetScaledCapsuleHalfHeight=function() return 88 end}
    a.WeaponComponents={weapon}
    a.IsValid,a.IsUnitized,a.IsDead=valid,valid,no
    a.IsNowDamageMotion=no
    function a:IsRiding() return self.riding end
    function a:IsNowRideMotion() return self.mounting end
    function a:IsNowAttack() return self.attack end
    function a:IsNowHit() return self.hit end
    function a:IsNoHit() return self.noHit end
    function a:GetHealth() return self.hp end
    function a:GetPrevHealth() return 100 end
    function a:GetLastAttackedMeCharacter() return self.attacker end
    function a:GetAddress() return self.id end
    function a:StopAIMovement() end
    function a:AddMovementInputKeep(direction,scale)
        self.inputX,self.inputY=direction.X*scale,direction.Y*scale
        if scale>0 then assert(self.lease and self.WeaponComponents[1].bHiddenInGame) end
    end
    function a:ConsumeMovementInputVector() self.inputX,self.inputY=0,0 end
    function a:GetVelocity() return {X=self.velocityX or 0,Y=self.velocityY or 0,Z=0} end
    function a:K2_GetActorLocation() return {X=self.x,Y=self.y,Z=self.z} end
    function a:K2_SetActorLocation(position,sweep,hit,teleport)
        assert(sweep and not teleport and type(hit)=='table')
        assert((movement.MovementMode==1 or movement.MovementMode==2) and not self.noHit and not self.hit)
        self.x,self.y,self.z=position.X,position.Y,position.Z
        self.groundCorrections=(self.groundCorrections or 0)+1
        return true
    end
    function a:K2_GetActorRotation() return {Yaw=self.yaw} end
    function a:K2_SetActorRotation(rotation) self.yaw=rotation.Yaw end
    return a
end

local army={}
for id=1,12 do army[id]=actor(id,id>=11) end
army[13],army[14]=actor(13),actor(14)
army[13].riding,army[14].mounting=true,true
local manager={IsValid=valid,m_arpSortedUnitizedEnemyList=army,m_arpSortedUnitizedAllyList={}}
local pose={available=true}
local lastPoseStart
function pose.ready() return pose.available end
function pose.reset() end
local function useDriver(row)
    assert(lastPoseStart~=now)
    lastPoseStart=now
    if not row.driver then row.driver=row.mesh; row.actor.driverCreations=(row.actor.driverCreations or 0)+1 end
    row.actor.driven=true
end
function pose.startFlight(row)
    useDriver(row)
    row.actor.track="sprint"
    return true
end
function pose.flightRate(row,speed) row.actor.playSpeed=speed end
function pose.prepareArrival(row) row.arrival={at=now+0.08} end
function pose.arrivalReady(row) return not row.arrival or now>=row.arrival.at end
function pose.start(row)
    if row.arrival then
        assert(row.actor.track=="sprint" and row.actor.driven and now>=row.arrival.at)
        row.actor.arrivals=(row.actor.arrivals or 0)+1
    else
        useDriver(row)
    end
    row.arrival=nil
    row.actor.started=(row.actor.started or 0)+1
    row.actor.track="enter"
    return row.style=="sit" and 1.25 or 0.75,0
end
function pose.activate(row)
    row.actor.rebound=(row.actor.rebound or 0)+1
    row.actor.driven=true
    return true
end
function pose.owns(row) return row.actor.driven end
function pose.deactivate(row) row.actor.driven=false; row.arrival=nil end
function pose.stop(row) pose.deactivate(row);row.driver=nil end
function pose.hold(row)
    row.actor.track=row.style=="sit" and "hold" or "cower"
    row.actor.holds=(row.actor.holds or 0)+1
end
function pose.rise(row,speed,time)
    row.actor.track="rise"
    return (time or 0.8)/speed
end
local function bridge(op,id,includeOfficers,acquire)
    if op==0 then return 1 end
    if op==20 then assert(army[id].disarmed); army[id].lease=acquire==1; return 1 end
    if op==22 then
        local a=army[id]
        assert(includeOfficers==a.id and not a.WeaponComponents[1].bHiddenInGame)
        a.disarmed=acquire==1
        return 1
    end
    if id==0 then return phase==4 and 1 or nil end
    if not army[id].excluded then return phase,1,id,id end
    if includeOfficers and phase==4 then return -1,1,id,id end
end
function StaticFindObject(name)
    if name:find("NavigationSystem",1,true) then
        return {IsValid=valid,FindPathToLocationSynchronously=function(_,a,start,goal)
            a.requests=(a.requests or 0)+1
            if a.id==10 then return end
            if a.id==2 and a.requests==1 then
                return {IsValid=valid,PathPoints={start,{X=0,Y=0,Z=0},goal}}
            end
            return {IsValid=valid,PathPoints={start,goal}}
        end}
    end
    return {IsValid=valid,GetTimeSeconds=function() return now end}
end
local floorAvailable=true
function FindFirstOf(name)
    assert(name=='StageManager')
    return {IsValid=valid,GetStageHeight=function() return {bBlockingHit=floorAvailable,ImpactPoint={Z=0}} end}
end
local evidence={}
local function capture(message)
    assert(message:find("Live evidence:",1,true),message)
    evidence[#evidence+1]=message
end
local slump=Infantry.new(pose,bridge,capture,true)
local groups={{side=1,corps=1,alive=true,x=600,y=0},{side=0,corps=2,alive=true,x=0,y=0}}
local function advance(time)
    while now<time do
        now=now+0.04
        for _,a in ipairs(army) do
            if a.lease and (a.CharacterMovement.MovementMode==1 or a.CharacterMovement.MovementMode==2) then
                local speed=a.CharacterMovement.MaxWalkSpeed
                a.velocityX,a.velocityY=speed*(a.inputX or 0),speed*(a.inputY or 0)
                a.x,a.y=a.x+0.04*a.velocityX,a.y+0.04*a.velocityY
            else
                a.velocityX,a.velocityY=0,0
            end
        end
        slump.update()
    end
end
slump.observe(manager,{{X=0,Y=0,Z=0}},groups,1)
advance(0.5)
assert(not army[1].driven)
phase=4
advance(1.25)
for id=1,10 do
    local a=army[id]
    assert(a.WeaponComponents[1].bHiddenInGame)
    if id==10 or (13+id)%4==0 then
        assert(a.driven and a.lease and a.CharacterMovement.MovementMode==2 and a.CharacterMovement.MaxWalkSpeed==0)
    else
        assert(a.driven and a.track=="sprint" and a.lease and a.inputX>0 and a.CharacterMovement.MaxWalkSpeed==780)
    end
    assert(a.CharacterMovement.MovementMode==2 and a.CharacterMovement.bAlwaysCheckFloor and a.CharacterMovement.bProjectNavMeshWalking)
    assert(a.CharacterMovement.bSweepWhileNavWalking and a.CharacterMovement.NavMeshProjectionInterval==0.1)
    assert(math.abs(a.z-88)<0.001 and a.groundCorrections==1)
end
local runner=army[1]
assert(army[2].requests==2)
local sprintStart=runner.x
advance(1.65)
assert(runner.x>sprintStart+250 and runner.lease and runner.playSpeed==780)
for id=11,14 do
    local a=army[id]
    assert(not a.started and not a.driven and not a.disarmed and not a.WeaponComponents[1].bHiddenInGame)
end
local struck=army[3]
advance(5)
assert(struck.driven and struck.track=="hold" and struck.started==1)
assert(runner.driven and runner.track=="cower" and runner.x>=2800 and runner.lease and runner.arrivals==1)
assert(army[10].track=="cower" and army[10].x==600 and not army[10].arrivals)
local seatedYaw,cowerYaw=struck.yaw,runner.yaw
for _,a in ipairs({struck,runner}) do
    assert(not a.bUseControllerRotationYaw and not a.CharacterMovement.bUseControllerDesiredRotation and not a.CharacterMovement.bOrientRotationToMovement)
    assert(a.CharacterMovement.RotationRate.Yaw==0)
    a.yaw=a.yaw+90 -- A native AI turn must not change an active seated pose's facing.
end
advance(5.2)
assert(struck.yaw==seatedYaw and runner.yaw==cowerYaw)
local seatedPosition=struck.x
local seatedY=struck.y
runner.driven=false
advance(5.6)
assert(runner.driven and runner.rebound==1 and runner.holds==1 and runner.started==1)
assert(struck.holds==1 and struck.x==seatedPosition)
runner.offscreen=true
advance(5.8)
assert(runner.holds==1 and runner.track=="cower")
runner.offscreen=false
local attacker=actor(13,true)
attacker.x=seatedPosition+100
struck.attacker=attacker
struck.hit,struck.hp=true,90
advance(6.02)
assert(not struck.driven and struck.track=="hold" and struck.WeaponComponents[1].bHiddenInGame and struck.CharacterMovement.MaxWalkSpeed==450)
assert(struck.CharacterMovement.MovementMode==2 and not struck.CharacterMovement.bAlwaysCheckFloor)
assert(struck.bUseControllerRotationYaw and struck.CharacterMovement.bUseControllerDesiredRotation and struck.CharacterMovement.RotationRate.Yaw==360)
advance(6.48)
assert(not struck.driven and not struck.lease)
struck.hit=false
advance(6.85)
assert(struck.driven and struck.track=="sprint" and struck.lease and struck.driverCreations==1)
-- The player and the last attacker are on opposite sides of the soldier.
assert(math.abs(struck.x-seatedPosition)<0.01 and math.abs(struck.y-seatedY)>100)
assert(struck.WeaponComponents[1].bHiddenInGame)
struck.offscreen=true
advance(11)
assert(struck.driven and struck.track=="hold" and struck.started==2 and struck.arrivals==1 and math.abs(struck.y-seatedY)>1000)
assert(struck.driverCreations==1)
struck.offscreen=false
local officer=army[11]
officer.x,officer.y=runner.x,runner.y
attacker.x,attacker.y=runner.x-100,runner.y
officer.attacker,officer.hit,officer.hp=attacker,true,90
advance(11.84)
assert(runner.driven and runner.track=="sprint" and runner.x>officer.x and runner.WeaponComponents[1].bHiddenInGame)
assert(not officer.driven and not officer.WeaponComponents[1].bHiddenInGame)
officer.hit=false
advance(16)
assert(runner.driven and runner.track=="cower" and runner.started==2)
phase=5
advance(16.3)
assert(struck.track=="rise" and struck.disarmed and struck.WeaponComponents[1].bHiddenInGame)
advance(17.6)
for id=1,10 do
    local a=army[id]
    assert(not a.driven and not a.lease and not a.disarmed and not a.WeaponComponents[1].bHiddenInGame)
    assert(a.CharacterMovement.MovementMode==2 and a.CharacterMovement.MaxWalkSpeed==450)
    assert(not a.CharacterMovement.bAlwaysCheckFloor)
    assert(not a.CharacterMovement.bProjectNavMeshWalking and not a.CharacterMovement.bSweepWhileNavWalking)
    assert(a.CharacterMovement.NavMeshProjectionInterval==0.3 and a.CharacterMovement.NavMeshProjectionInterpSpeed==12 and a.CharacterMovement.NavMeshProjectionHeightScaleUp==0.67)
    assert(a.bUseControllerRotationYaw and a.CharacterMovement.bUseControllerDesiredRotation)
    assert(not a.CharacterMovement.bOrientRotationToMovement and a.CharacterMovement.RotationRate.Yaw==360)
end
phase=4
advance(19)
assert(struck.CharacterMovement.MovementMode==2 and struck.CharacterMovement.MaxWalkSpeed==0)
struck.riding=true
struck.CharacterMovement.MovementMode=6
advance(19.3)
assert(not struck.driven and not struck.lease and not struck.disarmed and not struck.WeaponComponents[1].bHiddenInGame)
assert(struck.CharacterMovement.MovementMode==6) -- Mount movement stays engine-controlled.
struck.riding=false
struck.CharacterMovement.MovementMode=2
advance(20.2)
assert(struck.disarmed and struck.driven)
slump.observe(manager,{{X=0,Y=0,Z=0}},groups,2)
assert(not struck.driven and not struck.WeaponComponents[1].bHiddenInGame)
pose.available=false
advance(21)
assert(not struck.driven and not struck.WeaponComponents[1].bHiddenInGame)
pose.available=true
advance(23)
assert(struck.driven)
-- Mounted knockback can start native recovery without changing health.
struck.CharacterMovement.MovementMode,struck.noHit=3,true
advance(23.35)
assert(not struck.driven and not struck.lease and struck.CharacterMovement.MovementMode==3 and struck.CharacterMovement.MaxWalkSpeed==450)
struck.CharacterMovement.MovementMode=2
advance(23.65)
assert(not struck.driven and not struck.lease)
struck.noHit=false
advance(24.1)
assert(struck.driven and struck.CharacterMovement.MovementMode==2 and struck.CharacterMovement.bAlwaysCheckFloor)
floorAvailable=false;struck.z=110
advance(24.4)
assert(struck.z==110) -- Missing collision must not move the capsule.
floorAvailable=true;struck.z=1000
advance(24.7)
assert(struck.z==1000) -- Do not snap onto a distant lower storey.
struck.z=100
advance(25)
assert(struck.z==88)
local logged=table.concat(evidence,"\n")
assert(logged:find("infantry damage",1,true) and logged:find("hp=100.0->90.0",1,true))
assert(logged:find("yield to native reaction",1,true) and logged:find("noHit=true",1,true))
assert(logged:find("native reaction ended",1,true) and logged:find("noHit=false",1,true))
print("Corps infantry Lua lifecycle passed for sprint, turn, cower, seated hold, rally, exclusions and cleanup.")
""", str(root / "Scripts" / "InfantrySlump.lua"))

LuaRuntime().execute(r"""
local Pose=dofile(...)
local function valid() return true end
local function references()
    return setmetatable({}, {__index={Empty=function(self)
        for i=#self,1,-1 do self[i]=nil end
    end}})
end

local instance={IsValid=valid,ReferencedObjects={}}
local system,skeletalClass,referencerClass={IsValid=valid},{IsValid=valid},{IsValid=valid}
local referencer,missingClip
local loaded={}
function FName(name) return name end
function FindFirstOf(class)
    assert(class=="GameInstance")
    return instance
end
function StaticFindObject(class,outer,name,exact)
    if class=="/Script/Engine.Default__KismetSystemLibrary" then return system end
    if class=="/Script/Engine.SkeletalMeshComponent" then return skeletalClass end
    if class=="/Script/Engine.ObjectReferencer" then return referencerClass end
    assert(class==referencerClass and outer==instance and name=="MoraleMattersInfantryClips" and exact)
    return referencer
end
function StaticConstructObject(class,outer,name)
    assert(class==referencerClass and outer==instance and name=="MoraleMattersInfantryClips")
    assert(not referencer)
    referencer={IsValid=valid,ReferencedObjects=references()}
    return referencer
end
function system:MakeSoftObjectPath(path) return path end
function system:Conv_SoftObjPathToSoftObjRef(path) return path end
function system:LoadAsset_Blocking(path)
    local name=path:match("%.(MM_Soldier%w+)$")
    assert(name)
    if name==missingClip then return nil end
    local clip={name=name,SequenceLength=name=="MM_SoldierRun01" and 0.6 or 2,alive=true}
    function clip:IsValid() return self.alive end
    loaded[name]=clip
    return clip
end

-- Model Unreal reaching objects only through the GameInstance's reflected references.
local function forceGC()
    local reachable={}
    local function visit(object)
        reachable[object]=true
        for _,ref in ipairs(object.ReferencedObjects or {}) do visit(ref) end
    end
    visit(instance)
    for _,clip in pairs(loaded) do clip.alive=reachable[clip]==true end
end

local asset={IsValid=valid,GetAddress=function() return 10 end}
local function soldier(style)
    local driver={IsValid=valid,GetAddress=function() return 20 end,SkinnedAsset=asset}
    function driver:SetComponentTickEnabled(enabled) self.ticking=enabled end
    function driver:PlayAnimation(clip,looping)
        assert(clip:IsValid(),"PlayAnimation received a collected clip")
        self.clip,self.looping,self.position=clip.name,looping,0
    end
    function driver:GetPosition() return self.position end
    function driver:SetPosition(position) self.position=position end
    function driver:SetPlayRate(rate) self.rate=rate end
    local mesh={IsValid=valid,SkinnedAsset=asset,GetBoneIndex=function() return 0 end}
    mesh.LeaderPoseComponent={Get=function() return mesh.leader end}
    function mesh:SetLeaderPoseComponent(leader) self.leader=leader end
    return {style=style,mesh=mesh,driver=driver,member=1}
end

assert(Pose.ready(error))
assert(#instance.ReferencedObjects==1 and #referencer.ReferencedObjects==14)
forceGC()
local cower,slump=soldier("cower"),soldier("sit")
assert(Pose.start(cower)==2 and cower.driver.clip=="MM_SoldierCov01" and not cower.driver.looping)
assert(Pose.start(slump)==2 and slump.driver.clip=="MM_SoldierSit01" and not slump.driver.looping)
forceGC()
Pose.hold(cower)
Pose.hold(slump)
assert(cower.driver.clip=="MM_SoldierFear1" and cower.driver.looping)
assert(slump.driver.position==2 and not slump.driver.ticking)
forceGC()
assert(Pose.rise(cower,2)==1 and cower.driver.clip=="MM_SoldierRec01" and not cower.driver.looping)
assert(Pose.rise(slump,1)==2 and slump.driver.clip=="MM_SoldierGet01" and not slump.driver.looping)
for _,row in ipairs({cower,slump}) do
    Pose.deactivate(row)
    local original={IsValid=valid,GetAddress=function() return 30 end}
    row.mesh.leader=original
    assert(Pose.startFlight(row) and row.driver.clip=="MM_SoldierRun01" and row.driver.looping)
    local running=row.driver
    Pose.flightRate(row,784)
    assert(math.abs(running.rate-1.4)<0.001)
    running:SetPosition(row.style=="cower" and 0.2 or 0.59)
    Pose.prepareArrival(row)
    assert(not Pose.arrivalReady(row))
    running:SetPosition(row.style=="cower" and 0.31 or 0.01)
    assert(Pose.arrivalReady(row))
    local duration,offset=Pose.start(row)
    assert(duration==2 and math.abs(offset-0.01)<0.001 and Pose.owns(row) and not running.looping)
    assert(running.clip==(row.style=="cower" and "MM_SoldierCvR03" or "MM_SoldierStR01"))
    Pose.hold(row)
    Pose.deactivate(row)
    assert(row.mesh.leader==original and not running.ticking)
end
Pose.reset()
forceGC()
for _,clip in pairs(loaded) do assert(not clip:IsValid()) end

missingClip="MM_SoldierRec01"
assert(not Pose.ready(function(message) assert(message:find(missingClip,1,true)) end))
assert(#referencer.ReferencedObjects==0)
Pose.reset()
missingClip=nil
assert(Pose.ready(error))
forceGC()
assert(#instance.ReferencedObjects==1 and #referencer.ReferencedObjects==14)
for _,clip in pairs(loaded) do assert(clip:IsValid()) end
print("Infantry pose clips passed simulated Unreal GC, including cower, slump, both recoveries and cleanup.")
""", str(root / "Scripts" / "InfantryPose.lua"))
