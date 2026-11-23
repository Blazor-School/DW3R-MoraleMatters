local Pose={}
local clips,arrivals={},{}
local arrivalPhases=4
local skeletalClass,clipReferences
local attempted=false
local referencerName="MoraleMattersInfantryClips"

local function valid(o) return o and o:IsValid() end
local function same(a,b) return valid(a) and valid(b) and a:GetAddress()==b:GetAddress() end

local function releaseClips()
    if clipReferences then clipReferences:Empty(); clipReferences=nil end
    clips={}
    arrivals={}
end

local function loadClips()
    attempted=true
    local system=StaticFindObject("/Script/Engine.Default__KismetSystemLibrary")
    skeletalClass=StaticFindObject("/Script/Engine.SkeletalMeshComponent")
    local instance=FindFirstOf("GameInstance")
    local referencerClass=StaticFindObject("/Script/Engine.ObjectReferencer")
    assert(valid(system) and valid(skeletalClass),"animation engine objects unavailable")
    assert(valid(instance) and valid(referencerClass),"animation reference owner unavailable")
    local referencer=StaticFindObject(referencerClass,instance,referencerName,true)
    if not valid(referencer) then
        referencer=StaticConstructObject(referencerClass,instance,FName(referencerName))
        assert(valid(referencer),"could not construct the animation reference holder")
        local references=instance.ReferencedObjects
        references[#references+1]=referencer
    end
    -- Unreal follows these references during garbage collection. Lua wrappers alone cannot keep clips alive.
    clipReferences=referencer.ReferencedObjects
    clipReferences:Empty()
    local function load(suffix)
        local name="MM_Soldier"..suffix
        local path="/Game/Action/Animation/200_Sword1/"..name.."."..name
        local clip=system:LoadAsset_Blocking(system:Conv_SoftObjPathToSoftObjRef(system:MakeSoftObjectPath(path)))
        assert(valid(clip) and clip.SequenceLength>0,"could not load "..name)
        clipReferences[#clipReferences+1]=clip
        return clip
    end
    for state,suffix in pairs({sit="Sit01",cower="Cov01",tremble="Fear1",sitRise="Get01",cowerRise="Rec01",flight="Run01"}) do
        clips[state]=load(suffix)
    end
    for style,prefix in pairs({sit="StR",cower="CvR"}) do
        arrivals[style]={}
        for phase=1,arrivalPhases do arrivals[style][phase]=load(string.format("%s%02d",prefix,phase)) end
    end
end

function Pose.ready(log)
    if attempted then return clips.sit~=nil end
    local ok,err=pcall(loadClips)
    if not ok then
        releaseClips()
        log("Infantry animation pack could not be loaded: "..tostring(err))
    end
    return ok
end

function Pose.reset()
    releaseClips()
    attempted=false
end

function Pose.owns(row)
    return valid(row.driver) and valid(row.mesh) and same(row.mesh.LeaderPoseComponent:Get(),row.driver)
end

function Pose.deactivate(row)
    if Pose.owns(row) then
        row.mesh:SetLeaderPoseComponent(valid(row.previousLeader) and row.previousLeader or nil,true,row.previousFollowerTick)
    end
    row.previousLeader=nil
    row.arrival=nil
    if valid(row.driver) then row.driver:SetComponentTickEnabled(false) end
end

function Pose.stop(row)
    Pose.deactivate(row)
    if valid(row.actor) and valid(row.driver) then row.driver:K2_DestroyComponent(row.actor) end
    row.driver=nil
end

function Pose.activate(row)
    if Pose.owns(row) then return true end
    if not valid(row.mesh) or not valid(row.driver) or row.mesh.bHiddenInGame
        or not same(row.mesh.SkinnedAsset,row.driver.SkinnedAsset) then return false end
    row.previousLeader=row.mesh.LeaderPoseComponent:Get()
    row.previousFollowerTick=row.mesh.bFollowerShouldTickPose==true
    -- Keep the original mesh and its gameplay AnimInstance. The engine
    -- evaluates the cooked clip and supplies the bone transforms to this mesh.
    row.mesh:SetLeaderPoseComponent(row.driver,true,true)
    return Pose.owns(row)
end

local function play(row,clip,looping)
    if not valid(row.mesh.SkinnedAsset) or row.mesh.bHiddenInGame
        or row.mesh:GetBoneIndex(FName("SK_Hips"))<0 then return end
    if not valid(row.driver) then
        local attachment=row.mesh:GetRelativeTransform()
        local driver=row.actor:AddComponentByClass(skeletalClass,false,attachment,true)
        if not valid(driver) then return end
        row.driver=driver
        driver:SetVisibility(false,false)
        driver:SetCollisionEnabled(0)
        driver.VisibilityBasedAnimTickOption=0 -- AlwaysTickPoseAndRefreshBones
        driver:SetSkeletalMeshAsset(row.mesh.SkinnedAsset)
        row.actor:FinishAddComponent(driver,false,attachment)
    end
    local driver=row.driver
    driver:SetComponentTickEnabled(true)
    driver:PlayAnimation(clip,looping)
    driver:SetPlayRate(1)
    return Pose.activate(row)
end

function Pose.startFlight(row)
    if not play(row,clips.flight,true) then return false end
    row.driver:SetPosition((row.member%arrivalPhases)*clips.flight.SequenceLength/arrivalPhases,false)
    return true
end

function Pose.flightRate(row,speed)
    row.driver:SetPlayRate(math.max(0.65,math.min(1.85,speed/560)))
end

function Pose.prepareArrival(row)
    if not Pose.owns(row) then return end
    local duration=clips.flight.SequenceLength
    local position=row.driver:GetPosition()%duration
    local interval=duration/arrivalPhases
    local gate=math.ceil(position/interval)
    -- Each exit starts on a sampled frame of the sprint loop.
    row.arrival={position=position,wait=gate*interval-position,index=gate%arrivalPhases+1}
    row.driver:SetPlayRate(1.1)
end

function Pose.arrivalReady(row)
    local arrival=row.arrival
    if not arrival then return true end
    if not Pose.owns(row) then row.arrival=nil; return true end
    local advanced=(row.driver:GetPosition()-arrival.position)%clips.flight.SequenceLength
    return advanced>=arrival.wait
end

function Pose.start(row)
    local arrival=row.arrival
    local clip=arrival and arrivals[row.style][arrival.index] or clips[row.style]
    local offset=0
    if arrival then
        local advanced=(row.driver:GetPosition()-arrival.position)%clips.flight.SequenceLength
        offset=math.min(1/30,math.max(0,advanced-arrival.wait))
    end
    row.arrival=nil
    if not play(row,clip,false) then return end
    if offset>0 then row.driver:SetPosition(offset,false) end
    row.entryClip=clip
    return clip.SequenceLength,offset
end

function Pose.hold(row)
    local driver=row.driver
    if row.style=="cower" then
        driver:PlayAnimation(clips.tremble,true)
    else
        driver:SetPosition(row.entryClip.SequenceLength,false)
        driver:SetComponentTickEnabled(false)
    end
end

function Pose.rise(row,speed,time)
    local driver=row.driver
    driver:SetComponentTickEnabled(true)
    if time then
        driver:SetPlayRate(-speed)
        driver:Play(false)
        driver:SetPosition(time,false)
        return time/speed
    end
    local clip=row.style=="cower" and clips.cowerRise or clips.sitRise
    driver:PlayAnimation(clip,false)
    driver:SetPlayRate(speed)
    return clip.SequenceLength/speed
end

return Pose
