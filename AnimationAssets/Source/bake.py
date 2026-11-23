"""Bake custom infantry tracks against the game's cooked UE 5.6 skeleton.

The work directory contains compress.exe (ACL 2.1 and its bundled RTM) and
sample/ (retoc to-legacy output for H0000_chouun_Skeleton and 200_Sword1_0430).
The reference layouts below target those specific game exports. Pack the
generated legacy/ directory with retoc to-zen --version UE5_6.
"""
import argparse
import math
from pathlib import Path
import struct as st
import subprocess
from lupa import LuaRuntime

args=argparse.ArgumentParser(description=__doc__)
args.add_argument("work",type=Path)
work=args.parse_args().work.resolve()
skeleton=next((work/"sample").rglob("H0000_chouun_Skeleton.uexp"))
header=skeleton.with_suffix(".uasset").read_bytes()
body=skeleton.read_bytes()
assert st.unpack_from("<i",body,0x5bc)[0]==434
assert st.unpack_from("<i",body,0x1a18)[0]==434
names=[]
at=0x135
for i in range(428):
    size,=st.unpack_from("<i",header,at)
    names.append(header[at+4:at+3+size].decode())
    at+=8+size
assert at==0x2946
data=[]
for i in range(434):
    name,number,parent=st.unpack_from("<3i",body,0x5c0+i*12)
    assert -1<=parent<i, (i, parent)
    data.append((names[name]+('_'+str(number-1) if number else ''),parent,*st.unpack_from("<10d",body,0x1a1c+i*80)))

lua=LuaRuntime(unpack_returned_tuples=True)
rigdata=lua.table_from([lua.table_from(x) for x in data])
source=Path(__file__).with_name("InfantryPoseAuthoring.lua").read_text()
make,index,run_duration,arrival_phases,rise_duration,seat_only=lua.execute(source.removesuffix("return Pose\n")+"""
return function(data)
    local ref,localPoses,parents,index={}, {}, {}, {}
    ref["None"]=identity
    for i, d in ipairs(data) do
        local name=d[1]
        local parent=d[2]>=0 and data[d[2]+1][1] or "None"
        localPoses[name]=transform(vector(d[7],d[8],d[9]),quat(d[3],d[4],d[5],d[6]),vector(d[10],d[11],d[12]))
        ref[name]=compose(ref[parent],localPoses[name])
        parents[name]=parent
        index[name]=i-1
    end
    local function rowFor(cower,standing)
        local row={rig={},reference=ref,palms={},feet={},sample={},fingerDirections={},bones=cower and bones or seatedBones}
        for _,name in ipairs(row.bones) do
            if localPoses[name] then
                row.rig[name]={key=name,parentName=parents[name],localPose=localPoses[name]}
                if fingerBones[name] then
                    local nextJoint=name:sub(1,-2)..(tonumber(name:sub(-1))+1)
                    local p,tip=ref[name],ref[nextJoint]
                    if tip then row.fingerDirections[name]=rotate(inverse(p.Rotation),unit(sub(tip.Translation,p.Translation))) end
                end
            end
        end
        for _,side in ipairs(sides) do
            local left=side=="L"
            local prefix="SK_"..side.."_"
            local wrist=ref[prefix.."Hand"]
            local fingers=sub(ref[prefix.."Middle01"].Translation,wrist.Translation)
            local across=sub(ref[prefix.."Index01"].Translation,ref[prefix.."Pinky01"].Translation)
            local palm=scale(cross(across,fingers),left and 1 or -1)
            row.palms[side]={fingers=rotate(inverse(wrist.Rotation),unit(fingers)),normal=rotate(inverse(wrist.Rotation),unit(palm))}
            local foot=ref[prefix.."Foot"]
            local sole=math.max(8,math.min(13,foot.Translation.Z))
            local point=cower and vector(left and 19 or -19,left and 9 or 17,sole)
                or vector(left and 15 or -15,left and 26 or 31,sole)
            row.feet[side]=transform(point,foot.Rotation)
        end
        if standing then
            -- Begin from an unarmed stance with lowered elbows. Starting
            -- from the bind pose makes both hands sweep inward from the sides.
            local at,write,bend,changes=poseFrame(row)
            local root=at("SK_Skeleton")
            write("SK_Skeleton",transform(vector(root.Translation.X,root.Translation.Y,96),root.Rotation,root.Scale3D))
            bend("SK_Spine01",7)
            local chest=at("SK_Spine02").Translation
            for _,side in ipairs(sides) do
                local left=side=="L"
                local sign=left and 1 or -1
                local prefix="SK_"..side.."_"
                local foot=ref[prefix.."Foot"]
                limb(at,write,prefix.."UpLeg",prefix.."Leg",prefix.."Foot",
                    vector(sign*14,left and -14 or 12,10),vector(sign*0.12,1,0.26),foot.Rotation)
                local hand=prefix.."Hand"
                local shoulder=at(prefix.."Arm").Translation
                limb(at,write,prefix.."Arm",prefix.."ForeArm",hand,
                    vector(shoulder.X+sign*3,chest.Y+(left and 12 or 7),chest.Z-36),vector(sign*0.48,-0.78,-0.32))
                write(hand,aimWrist(at(hand),row.palms[side],vector(0,0.35,-1),vector(-sign,0.2,0),1))
            end
            local neutral={None=identity}
            for _,d in ipairs(data) do
                local name=d[1]
                local rest=changes[name] or localPoses[name]
                neutral[name]=compose(neutral[parents[name]],rest)
                if row.rig[name] then row.rig[name].localPose=rest end
            end
            row.reference=neutral
        end
        return row
    end
    local function checkHeadClearance(row,style,phase,frame)
        local world={None=identity}
        local function at(name)
            if not world[name] then
                local bone=row.rig[name]
                world[name]=compose(at(parents[name]),row.sample[name] or (bone and bone.localPose) or localPoses[name])
            end
            return world[name]
        end
        local head=at("SK_Head")
        local function check(first,last,padding)
            -- Approximate the head and headwear in head-bone space: X forward, Y up.
            local function normalized(name)
                local p=relative(head,at(name)).Translation
                return vector(p.X/(11+padding),(p.Y-8)/(13+padding),p.Z/(10+padding))
            end
            local a,b=normalized(first),normalized(last)
            local line=sub(b,a)
            local nearest=math.max(0,math.min(1,-dot(a,line)/math.max(dot(line,line),1e-8)))
            assert(length(mix(a,b,nearest))>=1,string.format("%s phase %s frame %d: %s passes through head",style,tostring(phase),frame,last))
        end
        for _,side in ipairs(sides) do
            local prefix="SK_"..side.."_"
            local wrist=prefix.."Hand"
            check(prefix.."ForeArm",wrist,5)
            for _,finger in ipairs(fingers) do check(wrist,prefix..finger.."03",4) end
            check(wrist,prefix.."Thumb03",4)
        end
    end
    local function samples(style,n,phase)
        local cower=style:sub(1,1)=="c"
        local row=rowFor(cower,style~="run")
        local runner=phase and rowFor(false)
        if runner or style=="sit" or style=="sit-rise" then Pose.prepareHands(row,cower) end
        local frames={}
        for i=0,n-1 do
            row.sample={}
            if runner then
                runner.sample={}
                Pose.arrive(row,runner,phase,(cower and Pose.cowerDuration or Pose.sitDuration)*i/(n-1),cower)
            elseif style=="run" then Pose.run(row,i/60)
            elseif style=="sit" then Pose.apply(row,"sit",Pose.sitDuration*i/(n-1),false)
            elseif style=="sit-rise" then Pose.apply(row,"rise",Pose.riseDuration*i/(n-1),false)
            elseif style=="cower-in" then Pose.apply(row,"sit",Pose.cowerDuration*i/(n-1),true)
            elseif style=="cower-out" then Pose.apply(row,"rise",Pose.riseDuration*i/(n-1),true)
            elseif style=="cower" then
                Pose.apply(row,"hold",Pose.cowerDuration,true)
                Pose.tremble(row,i/60)
            end
            if style=="sit" or style=="sit-rise" then checkHeadClearance(row,style,phase,i) end
            row.sample.SklRootNode=localPoses.SklRootNode
            frames[i+1]=row.sample
        end
        return frames
    end
    return samples,index,Pose.runDuration,Pose.arrivalPhases,Pose.riseDuration,fingerBones
end
""")(rigdata)

template=next((work/"sample").rglob("200_Sword1_0430.uasset"))
asset=template.read_bytes()
export=template.with_suffix(".uexp").read_bytes()
assert asset[0x582:0x58a]==st.pack("<q",len(export)-4)
assert export[0x154:0x158]==b"\x11\xac\x11\xac"
tail=bytearray(export[0x14c+0x34a5:])
assert tail[-4:]==bytes.fromhex("c1 83 2a 9e")

clips=[("sit","MM_SoldierSit01",76,None),("sit-rise","MM_SoldierGet01",round(rise_duration*60)+1,None),
       ("cower-in","MM_SoldierCov01",46,None),("cower","MM_SoldierFear1",121,None),
       ("cower-out","MM_SoldierRec01",49,None),("run","MM_SoldierRun01",round(run_duration*60)+1,None)]
for phase in range(arrival_phases):
    clips.extend([("sit","MM_SoldierStR%02d"%(phase+1),76,phase),
                  ("cower-in","MM_SoldierCvR%02d"%(phase+1),55,phase)])

def values(p):
    return (*[p.Rotation[k] for k in "XYZW"],*[p.Translation[k] for k in "XYZ"],*[p.Scale3D[k] for k in "XYZ"])

endpoints={}
for style,leaf,n,phase in clips:
    assert len(leaf)==len(template.stem)
    frames=make(style,n,phase)
    names=sorted((name for name in frames[1].keys() if style.startswith("sit") or not seat_only[name]),key=lambda name:index[name])
    end=tuple(values(frames[n][name]) for name in names)
    if style=="run":
        run_frames=frames
        phase_frames=(n-1)//arrival_phases
        assert phase_frames*arrival_phases==n-1
        assert end==tuple(values(frames[1][name]) for name in names)
    elif phase is None:
        endpoints[style]=end
        if style=="sit-rise":
            assert tuple(values(frames[1][name]) for name in names)==endpoints["sit"], leaf
    else:
        start=tuple(values(frames[1][name]) for name in names)
        gate=tuple(values(run_frames[1+phase*phase_frames][name]) for name in names)
        assert all(abs(a-b)<1e-9 for x,y in zip(start,gate) for a,b in zip(x,y)), leaf
        assert end==endpoints[style], leaf
    if phase is not None or style in ("sit","sit-rise"):
        for hand in ("SK_L_Hand","SK_R_Hand"):
            rotations=[values(frames[i][hand])[:4] for i in range(1,n+1)]
            # Catch wrist flips of more than 30 degrees in a single 60 Hz sample.
            assert all(abs(sum(a*b for a,b in zip(x,y)))>=math.cos(math.pi/12)
                       for x,y in zip(rotations,rotations[1:])), (leaf,hand)
    print(leaf,len(names),n)
    tracks=work/(leaf+".tracks")
    acl=work/(leaf+".acl")
    tracks.write_bytes(st.pack("<2I",len(names),n)+b"".join(st.pack("<10f",*values(frames[i][name])) for name in names for i in range(1,n+1)))
    subprocess.run([str(work/"compress.exe"),str(tracks),str(acl)],check=True)
    blob=acl.read_bytes()
    assert st.unpack_from("<IHI",blob,8)[:2]==(0xac11ac11,10) and st.unpack_from("<I",blob,16)[0]==len(names)
    seq=bytearray(export[:0x50])
    st.pack_into("<f",seq,0x22,(n-1)/60)
    st.pack_into("<I",seq,0x4c,n*len(names)*40)
    mapping=st.pack("<"+"I"*(len(names)+1),len(names),*[index[name] for name in names])
    last=bytearray(tail)
    st.pack_into("<I",last,len(last)-12,n)
    new_export=seq+mapping+st.pack("<III",0,len(blob),0)+blob+last
    new_asset=bytearray(asset.replace(template.stem.encode(),leaf.encode()))
    st.pack_into("<q",new_asset,0x582,len(new_export)-4)
    out=work/"legacy"/"DW3CE_RE/Content/Action/Animation/200_Sword1"/(leaf+".uasset")
    out.parent.mkdir(exist_ok=True,parents=True)
    out.write_bytes(new_asset)
    out.with_suffix(".uexp").write_bytes(new_export)
