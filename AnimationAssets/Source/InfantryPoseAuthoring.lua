-- Component space uses +Y forward, +X to the soldier's left, and centimetres.
local Pose = {}
local function vector(x,y,z) return {X=x,Y=y,Z=z} end
local function add(a,b) return vector(a.X+b.X,a.Y+b.Y,a.Z+b.Z) end
local function sub(a,b) return vector(a.X-b.X,a.Y-b.Y,a.Z-b.Z) end
local function scale(a,s) return vector(a.X*s,a.Y*s,a.Z*s) end
local function dot(a,b) return a.X*b.X+a.Y*b.Y+a.Z*b.Z end
local function cross(a,b) return vector(a.Y*b.Z-a.Z*b.Y,a.Z*b.X-a.X*b.Z,a.X*b.Y-a.Y*b.X) end
local function length(a) return math.sqrt(dot(a,a)) end
local function unit(a) return scale(a,1/math.max(length(a),1e-8)) end
local function mix(a,b,t) return add(a,scale(sub(b,a),t)) end
local function quat(x,y,z,w) return {X=x,Y=y,Z=z,W=w} end
local function qmul(a,b)
    return quat(a.W*b.X+a.X*b.W+a.Y*b.Z-a.Z*b.Y,
        a.W*b.Y-a.X*b.Z+a.Y*b.W+a.Z*b.X,
        a.W*b.Z+a.X*b.Y-a.Y*b.X+a.Z*b.W,
        a.W*b.W-a.X*b.X-a.Y*b.Y-a.Z*b.Z)
end
local function inverse(q) return quat(-q.X,-q.Y,-q.Z,q.W) end
local function rotate(q,v)
    local axis = vector(q.X,q.Y,q.Z)
    local t = scale(cross(axis,v),2)
    return add(v,add(scale(t,q.W),cross(axis,t)))
end
local function turn(from,to)
    local a,b = unit(from),unit(to)
    local w = 1+dot(a,b)
    local v = cross(a,b)
    if w < 1e-5 then
        local axis = math.abs(a.Z)<0.8 and vector(0,0,1) or vector(1,0,0)
        v = unit(cross(a,axis))
        return quat(v.X,v.Y,v.Z,0)
    end
    local s = math.sqrt(dot(v,v)+w*w)
    return quat(v.X/s,v.Y/s,v.Z/s,w/s)
end
local function blendRotation(a,b,t)
    local sign=a.X*b.X+a.Y*b.Y+a.Z*b.Z+a.W*b.W<0 and -1 or 1
    local x,y,z,w=(1-t)*a.X+t*sign*b.X,(1-t)*a.Y+t*sign*b.Y,(1-t)*a.Z+t*sign*b.Z,(1-t)*a.W+t*sign*b.W
    local size=math.sqrt(x*x+y*y+z*z+w*w)
    return quat(x/size,y/size,z/size,w/size)
end
local function pitch(degrees)
    local half = math.rad(degrees)/2
    return quat(math.sin(half),0,0,math.cos(half))
end
local function roll(degrees)
    local half = math.rad(degrees)/2
    return quat(0,math.sin(half),0,math.cos(half))
end
local function yaw(degrees)
    local half = math.rad(degrees)/2
    return quat(0,0,math.sin(half),math.cos(half))
end
local function transform(p,q,s) return {Translation=p,Rotation=q,Scale3D=s or vector(1,1,1)} end
local function read(t)
    local p,q,s = t.Translation,t.Rotation,t.Scale3D
    return transform(vector(p.X,p.Y,p.Z),quat(q.X,q.Y,q.Z,q.W),vector(s.X,s.Y,s.Z))
end
local function compose(parent,localPose)
    return transform(add(parent.Translation,rotate(parent.Rotation,localPose.Translation)),
        qmul(parent.Rotation,localPose.Rotation),localPose.Scale3D)
end
local function relative(parent,world)
    local q = inverse(parent.Rotation)
    return transform(rotate(q,sub(world.Translation,parent.Translation)),qmul(q,world.Rotation),world.Scale3D)
end
local identity = transform(vector(0,0,0),quat(0,0,0,1))
local sides = {"L","R"}
local bones = {
    "SK_Skeleton","SK_Hips","SK_L_UpLeg","SK_L_Leg","SK_L_Foot",
    "SK_R_UpLeg","SK_R_Leg","SK_R_Foot","SK_Spine01","SK_Spine02","SK_Neck","SK_Head",
    "SK_L_Shoulder","SK_L_Arm","SK_L_ForeArm","SK_L_Hand",
    "SK_R_Shoulder","SK_R_Arm","SK_R_ForeArm","SK_R_Hand",
    "RF_C_UpLegNoTwist","RF_C_SkirtUpLeg","RF_C_SkirtKnee","RF_C_SkirtLeg",
    "RF_L_SkirtUpLeg","RF_L_SkirtKnee","RF_L_SkirtLeg",
    "RF_R_SkirtUpLeg","RF_R_SkirtKnee","RF_R_SkirtLeg",
}
local fingers = {"Index","Middle","Ring","Pinky"}
local seatedBones, fingerBones = {table.unpack(bones)}, {}
for _,side in ipairs(sides) do
    for _,finger in ipairs(fingers) do
        for joint=1,2 do
            local name="SK_"..side.."_"..finger.."0"..joint
            seatedBones[#seatedBones+1]=name
            fingerBones[name]=true
        end
    end
end
-- Each frame describes body weight, lean, and limb placement. The rise has
-- its own forward brace before the hips lift.
local seated = {1.25,1,1,21,-1,1}
local sit = {
    {0,0,0,0,0,0},
    {0.19,0.08,0.10,11,1,0.05},
    {0.49,0.40,0.53,17,-2,0.36},
    {0.91,0.96,0.98,20,-2,0.89},
    seated,
}
local rise = {
    {0,1,1,21,-1,1},
    {0.26,0.98,0.95,23,-2,0.72},
    {0.54,0.57,0.64,24,1,0.28},
    {0.84,0.14,0.22,13,0,0.08},
    {1.12,0,0,0,0,0},
}
Pose.sitDuration, Pose.cowerDuration, Pose.riseDuration = sit[#sit][1],0.55,rise[#rise][1]
Pose.runDuration, Pose.arrivalPhases = 0.60,4
local arrivalBlendTime=0.30
local function ease(t) return t*t*(3-2*t) end
local function frame(track,time)
    for i=2,#track do
        local a,b = track[i-1],track[i]
        if time <= b[1] then
            local t = math.max(0,(time-a[1])/(b[1]-a[1]))
            t = t*t*(3-2*t)
            local values = {}
            for j=2,#b do values[j-1] = a[j]+(b[j]-a[j])*t end
            return table.unpack(values)
        end
    end
    return table.unpack(track[#track],2)
end

-- Point a two-joint chain at its contact point without changing bone lengths.
local function limb(at,write,upper,lower,tip,target,pole,tipRotation,lowerRotation)
    local a,b,c = at(upper),at(lower),at(tip)
    local origin = a.Translation
    local ab,bc = sub(b.Translation,origin),sub(c.Translation,b.Translation)
    local l1,l2 = length(ab),length(bc)
    local delta = sub(target,origin)
    local direction = unit(delta)
    local reach = math.max(math.abs(l1-l2)+0.01,math.min(length(delta),l1+l2-0.01))
    local offset = (l1*l1-l2*l2+reach*reach)/(2*reach)
    local side = unit(sub(pole,scale(direction,dot(pole,direction))))
    local knee = add(origin,add(scale(direction,offset),scale(side,math.sqrt(math.max(0,l1*l1-offset*offset)))))
    local contact = add(origin,scale(direction,reach))
    local swing = turn(ab,sub(knee,origin))
    write(upper,transform(origin,qmul(swing,a.Rotation),a.Scale3D))
    local lowerBase=lowerRotation or qmul(swing,b.Rotation)
    local lowerDirection=lowerRotation and rotate(qmul(lowerRotation,inverse(b.Rotation)),bc) or rotate(swing,bc)
    local swing2=turn(lowerDirection,sub(contact,knee))
    write(lower,transform(knee,qmul(swing2,lowerBase),b.Scale3D))
    write(tip,transform(contact,tipRotation or c.Rotation,c.Scale3D))
end

local function aimWrist(wrist,palm,fingers,facing,brace)
    local swing=qmul(turn(rotate(wrist.Rotation,palm.fingers),fingers),wrist.Rotation)
    local normal=rotate(swing,palm.normal)
    local inward=unit(sub(facing,scale(fingers,dot(facing,fingers))))
    local half=math.atan(dot(fingers,cross(normal,inward)),dot(normal,inward))/2
    local axis=scale(fingers,math.sin(half))
    local twist=quat(axis.X,axis.Y,axis.Z,math.cos(half))
    return transform(wrist.Translation,blendRotation(wrist.Rotation,qmul(twist,swing),brace),wrist.Scale3D)
end

local function setLocal(row,bone,localPose)
    row.sample[bone.key]=localPose
end

local cowerJoints = {
    "SK_Spine02","SK_L_Arm","SK_R_Arm","SK_L_ForeArm","SK_R_ForeArm",
}

local function trembleLegs(row,shiver,breath)
    local hold=row.holdLegs
    local base=hold.hips
    local shift=vector(1.7*shiver,0.9*breath,1.35*shiver+0.7*breath)
    local tilt=roll(1.8*shiver)
    local poses={SK_Hips=transform(add(base.Translation,shift),qmul(tilt,base.Rotation),base.Scale3D)}
    local function write(name,p)
        local joint=row.rig[name]
        setLocal(row,joint,relative(poses[joint.parentName] or hold.parent,p))
        poses[name]=p
    end
    write("SK_Hips",poses.SK_Hips)
    for i,leg in ipairs(hold.legs) do
        for j,name in ipairs(leg.bones) do
            local p=leg.poses[j]
            local position=add(poses.SK_Hips.Translation,rotate(tilt,sub(p.Translation,base.Translation)))
            poses[name]=transform(position,qmul(tilt,p.Rotation),p.Scale3D)
        end
        local pole=add(leg.pole,vector((i==1 and 1 or -1)*shiver*4,breath*1.2,0))
        local foot=leg.poses[3]
        limb(function(name) return poses[name] end,write,leg.bones[1],leg.bones[2],leg.bones[3],foot.Translation,pole,foot.Rotation)
    end
end

function Pose.tremble(row,time)
    local function move(name,bend,sway)
        local joint=row.holdBones[name]
        local motion=qmul(roll(sway),pitch(bend))
        local delta=qmul(qmul(inverse(joint.parent),motion),joint.parent)
        local base=joint.pose
        setLocal(row,row.rig[name],transform(base.Translation,qmul(delta,base.Rotation),base.Scale3D))
    end
    local step=time*math.pi
    local breath=math.sin(2*step)
    local shiver=math.sin(5*step)*(0.75+0.25*math.sin(2*step))
    local tuck=3.5*breath+4.5*shiver
    trembleLegs(row,shiver,breath)
    move("SK_Spine02",2.8*breath+2.2*shiver,1.5*shiver)
    move("SK_L_Arm",tuck,4.5*shiver+3*breath)
    move("SK_R_Arm",tuck,-4.5*shiver-3*breath)
    move("SK_L_ForeArm",-tuck*1.15,-3.5*shiver)
    move("SK_R_ForeArm",-tuck*1.15,3.5*shiver)
end

local function poseFrame(row,locals)
    local desired,changes = {},{}
    local rig,reference = row.rig,row.reference
    local function at(name)
        if desired[name] then return desired[name] end
        local bone = rig[name]
        if not bone then return reference[name] end
        local base=locals and locals[name] or bone.localPose
        return compose(at(bone.parentName),base)
    end
    local function write(name,pose)
        changes[name] = relative(at(rig[name].parentName),pose)
        desired[name] = pose
    end
    local function bend(name,degrees,side)
        if not rig[name] then return end
        local p = at(name)
        write(name,transform(p.Translation,qmul(roll(side or 0),qmul(pitch(-degrees),p.Rotation)),p.Scale3D))
    end
    return at,write,bend,changes
end

function Pose.apply(row,phase,time,cowering,arriving)
    local drop,feet,lean,tilt,brace
    if phase == "hold" then
        drop,feet,lean,tilt,brace = table.unpack(seated,2)
    else
        drop,feet,lean,tilt,brace = frame(phase=="rise" and rise or sit,time)
    end
    if cowering then
        local t=phase=="rise" and math.max(0,1-time/Pose.riseDuration) or math.min(1,time/Pose.cowerDuration)
        drop=t*t*(3-2*t)
        feet,lean,tilt,brace=drop,30*drop,0,drop
    end
    arriving=arriving and brace<1
    local rig,reference = row.rig,row.reference
    local at,write,bend,changes = poseFrame(row)
    local base = reference.SK_Skeleton
    local root = add(base.Translation,cowering and vector(0,-15*drop,(58-base.Translation.Z)*drop)
        or vector(0,-26*drop,(24-base.Translation.Z)*drop))
    write("SK_Skeleton",transform(root,base.Rotation,base.Scale3D))
    bend("SK_Hips",0,tilt)
    bend("SK_Spine01",cowering and lean or lean*0.60,tilt*0.65)
    bend("SK_Spine02",lean*(cowering and 0.22 or 0.78))
    -- Bow the head above and behind the folded forearms.
    bend("SK_Neck",cowering and lean*0.25 or lean*0.30+brace)
    bend("SK_Head",cowering and lean*0.10 or lean*0.16+8*brace)

    for _,side in ipairs(sides) do
        local prefix = "SK_"..side.."_"
        local foot = reference[prefix.."Foot"]
        local left = side=="L"
        local contact = row.feet[side]
        local a,b,c = prefix.."UpLeg",prefix.."Leg",prefix.."Foot"
        if feet>0 then
            local step = mix(foot.Translation,contact.Translation,feet)
            step.Z = step.Z + (left and 3 or 6)*math.sin(math.pi*feet)^2
            local pole=cowering and vector(left and 0.22 or -0.24,1,0.30) or vector(left and 0.09 or -0.09,1,0.45)
            limb(at,write,a,b,c,step,pole,blendRotation(at(c).Rotation,contact.Rotation,feet))
        end
    end
    for _,side in ipairs(sides) do
        local prefix="SK_"..side.."_"
        local left=side=="L"
        local arm,elbow,hand = prefix.."Arm",prefix.."ForeArm",prefix.."Hand"
        if brace>0 or arriving then
            local wrist=at(hand)
            local sign=left and 1 or -1
            local knee=at(prefix.."Leg").Translation
            local otherKnee=at(left and "SK_R_Leg" or "SK_L_Leg").Translation
            -- Each forearm crosses the knees, with the left arm resting across the right.
            local anchored=cowering and add(at("SK_Head").Translation,vector(left and 19 or -19,11,left and 1 or -3))
                or vector(otherKnee.X+sign,otherKnee.Y+(left and 25 or 24),otherKnee.Z+(left and 16 or 6))
            local shoulder=at(arm).Translation
            local foldedElbow=add(knee,vector(sign*8,left and 22 or 24,left and 15 or 8))
            local pole=cowering and vector(sign*0.65,0.45,-1)
                or sub(mix(at(elbow).Translation,foldedElbow,brace),shoulder)
            local fold=cowering and brace or ease(brace)
            local target=mix(wrist.Translation,anchored,fold)
            if arriving then
                if cowering then
                    -- Reach forward and up to the head with the elbows still low.
                    local arc=math.sin(math.pi*brace)^2
                    target=add(target,vector(sign*8*arc,16*arc,5*arc))
                else
                    local arc=math.sin(math.pi*fold)^2
                    target=add(target,vector(sign*6*arc,8*arc,0))
                end
            end
            limb(at,write,arm,elbow,hand,target,pole)
            local palm=row.palms[side]
            if palm then
                wrist=at(hand)
                local forearm=unit(sub(wrist.Translation,at(elbow).Translation))
                local direction=cowering and unit(mix(vector(0,0.38,1),forearm,0.30))
                    or unit(mix(forearm,vector(-sign,0.45,-0.82),0.65))
                local facing=cowering and vector(-sign,-0.18,-0.12) or vector(0,-0.65,-1)
                if row.handKeys and (arriving or not cowering) then
                    local keys=row.handKeys[side]
                    local start=arriving and keys[1] or reference[hand].Rotation
                    local rotation=brace<0.45 and blendRotation(start,keys[2],ease(brace/0.45))
                        or blendRotation(keys[2],keys[3],ease((brace-0.45)/0.55))
                    write(hand,transform(wrist.Translation,rotation,wrist.Scale3D))
                else
                    write(hand,aimWrist(wrist,palm,direction,facing,brace))
                end
                if not cowering then
                    local normal=rotate(at(hand).Rotation,palm.normal)
                    for _,finger in ipairs(fingers) do
                        for joint=1,2 do
                            local name=prefix..finger.."0"..joint
                            local restDirection=row.fingerDirections[name]
                            if restDirection then
                                local p=at(name)
                                local tangent=rotate(p.Rotation,restDirection)
                                -- Curl the fingers toward the palm as the wrists settle.
                                local curl=turn(tangent,add(tangent,scale(normal,fold*(joint==1 and 0.27 or 0.48))))
                                write(name,transform(p.Translation,qmul(curl,p.Rotation),p.Scale3D))
                            end
                        end
                    end
                end
            end
        end
    end
    local skirt=feet*(cowering and 0.55 or 1)
    bend("RF_C_UpLegNoTwist",-56*skirt)
    bend("RF_L_SkirtUpLeg",-57*skirt)
    bend("RF_R_SkirtUpLeg",-59*skirt)
    bend("RF_C_SkirtLeg",60*skirt)
    bend("RF_L_SkirtLeg",59*skirt)
    bend("RF_R_SkirtLeg",58*skirt)

    for _,name in ipairs(row.bones) do
        local bone=rig[name]
        if bone then setLocal(row,bone,changes[name] or bone.localPose) end
    end
    if phase=="hold" and cowering then
        row.holdBones={}
        for _,name in ipairs(cowerJoints) do
            local bone=rig[name]
            row.holdBones[name]={pose=changes[name] or bone.localPose,parent=at(bone.parentName).Rotation}
        end
        row.holdLegs={hips=at("SK_Hips"),parent=at(rig.SK_Hips.parentName),legs={}}
        for _,side in ipairs(sides) do
            local prefix="SK_"..side.."_"
            local names={prefix.."UpLeg",prefix.."Leg",prefix.."Foot"}
            local a,b,c=at(names[1]),at(names[2]),at(names[3])
            row.holdLegs.legs[#row.holdLegs.legs+1]={bones=names,poses={a,b,c},pole=sub(b.Translation,a.Translation)}
        end
    end
end

function Pose.prepareHands(row,cowering)
    Pose.apply(row,"sit",cowering and Pose.cowerDuration or Pose.sitDuration,cowering)
    local at=poseFrame(row,row.sample)
    row.handKeys={}
    for _,side in ipairs(sides) do
        local sign=side=="L" and 1 or -1
        local wrist=at("SK_"..side.."_Hand")
        local palm=row.palms[side]
        local lowered=aimWrist(wrist,palm,unit(vector(0,0.38,-1)),vector(-sign,0.16,0),1)
        local reach=aimWrist(wrist,palm,unit(vector(0,1,cowering and 0.28 or -0.32)),
            cowering and vector(-sign,0,-0.2) or vector(0,0,-1),1)
        row.handKeys[side]={lowered.Rotation,reach.Rotation,wrist.Rotation}
    end
end

local function runStep(phase)
    local stance,front,back=0.28,46,-48
    if phase<stance then return front+(back-front)*phase/stance,0,12-25*phase/stance end
    local t=(phase-stance)/(1-stance)
    -- Match the ground speed through toe-off and the next foot strike.
    local tangent=(back-front)*(1-stance)/stance
    local forward=back+(front-back)*ease(t)+tangent*t*(1-t)*(1-2*t)
    return forward,43*math.sin(math.pi*t)^2,-13+25*ease(t)+18*math.sin(math.pi*t)
end

function Pose.run(row,time)
    local phase=(time/Pose.runDuration)%1
    local cycle=2*math.pi*phase
    local swing,step=math.sin(cycle),math.cos(cycle)
    local at,write,bend,changes=poseFrame(row)
    local base=at("SK_Skeleton")
    write("SK_Skeleton",transform(vector(2.7*swing,1,97-4*math.cos(2*cycle-0.55)),
        qmul(yaw(4*swing),base.Rotation),base.Scale3D))
    bend("SK_Hips",5,5*swing)
    bend("SK_Spine01",21+3*step,-5*swing)
    bend("SK_Spine02",5,3*swing)
    bend("SK_Neck",-9)
    local head=at("SK_Head")
    local glance=math.max(0,math.cos(cycle-1.2))^3
    write("SK_Head",transform(head.Translation,qmul(yaw(29*glance-6),head.Rotation),head.Scale3D))

    for _,side in ipairs(sides) do
        local left=side=="L"
        local sign=left and 1 or -1
        local prefix="SK_"..side.."_"
        local stride,lift,footPitch=runStep((phase+(left and 0 or 0.5))%1)
        local foot=prefix.."Foot"
        limb(at,write,prefix.."UpLeg",prefix.."Leg",foot,vector(sign*13,stride,11+lift),
            vector(sign*0.10,1,0.22),qmul(pitch(footPitch),row.reference[foot].Rotation))

        local shoulder=at(prefix.."Arm").Translation
        local pump=sign*math.cos(cycle+0.45)
        local hand=prefix.."Hand"
        local target=add(shoulder,vector(sign*(6+3*pump),10+25*pump,-39+13*pump+(left and 5 or 0)))
        limb(at,write,prefix.."Arm",prefix.."ForeArm",hand,target,vector(sign*0.42,-0.86,-0.52))
        local forearm=unit(sub(at(hand).Translation,at(prefix.."ForeArm").Translation))
        write(hand,aimWrist(at(hand),row.palms[side],unit(mix(forearm,vector(0,1,0.10),0.28)),
            vector(-sign,0.16,-0.32),1))
    end
    for _,name in ipairs(row.bones) do
        local bone=row.rig[name]
        if bone then setLocal(row,bone,changes[name] or bone.localPose) end
    end
end

function Pose.arrive(row,runner,phase,time,cowering)
    if time>=arrivalBlendTime then return Pose.apply(row,"sit",time,cowering,true) end
    local followThrough=0.065*(1-math.exp(-time/0.065))
    Pose.run(runner,phase*Pose.runDuration/Pose.arrivalPhases+followThrough)
    if time==0 then row.sample=runner.sample; return end
    Pose.apply(row,"sit",time,cowering,true)
    local weight=ease(time/arrivalBlendTime)
    local from=poseFrame(runner,runner.sample)
    local toward=poseFrame(row,row.sample)
    local blended={}
    for _,name in ipairs(row.bones) do
        local a,b=runner.sample[name],row.sample[name]
        if a and b then
            blended[name]=transform(mix(a.Translation,b.Translation,weight),
                blendRotation(a.Rotation,b.Rotation,weight),b.Scale3D)
        end
    end
    local at,write,_,changes=poseFrame(row,blended)
    local function bringIn(upper,lower,tip,outward)
        local a,b=from(tip),toward(tip)
        local pole=mix(sub(from(lower).Translation,from(upper).Translation),
            sub(toward(lower).Translation,toward(upper).Translation),weight)
        pole.X=pole.X+outward*math.sin(math.pi*weight)
        local forearm=outward~=0 and blendRotation(from(lower).Rotation,toward(lower).Rotation,weight) or nil
        limb(at,write,upper,lower,tip,mix(a.Translation,b.Translation,weight),pole,blendRotation(a.Rotation,b.Rotation,weight),forearm)
    end
    for _,side in ipairs(sides) do
        local prefix="SK_"..side.."_"
        bringIn(prefix.."UpLeg",prefix.."Leg",prefix.."Foot",0)
        bringIn(prefix.."Arm",prefix.."ForeArm",prefix.."Hand",side=="L" and 12 or -12)
    end
    for name,value in pairs(blended) do row.sample[name]=changes[name] or value end
end
return Pose
