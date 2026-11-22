from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'build' / 'LuaValidationLibs'))
from lupa.lua54 import LuaRuntime

lua = LuaRuntime(unpack_returned_tuples=True)
lua.globals().GuardFeedback = lua.execute((root / 'Scripts' / 'GuardFeedback.lua').read_text(encoding='utf-8'))
print(lua.execute(r'''
local assertions,constructed,sounds,reads=0,0,0,0
local function check(value) assertions=assertions+1;assert(value,'guard feedback assertion '..assertions) end
local function near(a,b) return math.abs(a-b)<0.001 end
local nextAddress=0
local function widget()
 nextAddress=nextAddress+1
 local w={address=nextAddress,valid=true,Font={},RenderTransform={Translation={X=2,Y=3}}}
 function w:IsValid() return self.valid end
 function w:GetAddress() return self.address end
 function w:GetFullName() return "fixture.CanvasPanel" end
 function w:IsA(class) return self.kind==class.kind end
 function w:SetRenderTranslation(value) self.RenderTransform.Translation=value end
 function w:SetAnchors(value) self.anchors=value end
 function w:SetVisibility(value) self.visible=value end
 function w:SetPosition(value) self.position=value end
 function w:SetSize(value) self.size=value end
 function w:SetColorAndOpacity(value) self.color=value end
 function w:SetText(value) self.text=value;self.writes=(self.writes or 0)+1 end
 function w:SetJustification(value) self.justification=value end
 function w:SetBrushFromTexture(value,match) check(match==false);self.texture=value end
 function w:SetClipping(value) self.clipping=value end
 function w:SetRenderTransformPivot(value) self.pivot=value end
 function w:SetRenderScale(value) self.scale=value end
 function w:SetRenderOpacity(value) self.opacity=value end
 function w:SetShadowOffset(value) self.shadowOffset=value end
 function w:SetShadowColorAndOpacity(value) self.shadowColor=value end
 function w:RemoveFromParent() self.removed=true end
 return w
end
function FText(value) return value end
local parent,hud,manager,saveManager,save=widget(),widget(),widget(),widget(),widget()
save.OptionData={Sound={SEVolume=16}}
saveManager.GameStatusSaveGame=save
local canvasClass,slotClass,componentClass=widget(),widget(),widget()
canvasClass.kind,slotClass.kind='CanvasPanel','CanvasPanelSlot'
local gauges={}
local function array(values)
 values.ForEach=function(self,callback) for index,value in ipairs(self) do callback(index,{get=function() return value end}) end end
 return values
end
for id=1,8 do
 local canvas,hp,slot,gauge,component,actor=widget(),widget(),widget(),widget(),widget(),widget()
 canvas.kind,slot.kind='CanvasPanel','CanvasPanelSlot'
 slot.point,slot.extent,slot.align={X=20,Y=90},{X=180,Y=12},{X=0.5,Y=0.5}
 slot.anchor={Minimum={X=0.5,Y=0.5},Maximum={X=0.5,Y=0.5}}
 function slot:GetPosition() return self.point end
 function slot:GetSize() return self.extent end
 function slot:GetAlignment() return self.align end
 function slot:GetAnchors() return self.anchor end
 function hp:GetParent() return canvas end
 hp.Slot=slot
 gauge.gauge_head_base=hp;gauge.text_enemy_name=widget();gauge.text_soldier_name=widget()
 component.shown=true
 function component:IsVisible() return self.shown end
 function component:GetUserWidgetObject() return gauge end
 function actor:GetComponentByClass(class) check(class==componentClass);return component end
 gauges[id]={canvas=canvas,slot=slot,gauge=gauge,component=component,actor=actor}
end
local actors,ids={},{}
for id,gauge in ipairs(gauges) do actors[id]=gauge.actor;ids[id]=1000+id end
manager.NPCHeadWidgetInfo=array({{Actors=array(actors),UnitID=array(ids)}})
function StaticFindObject(path)
 return ({['/Script/UMG.WidgetComponent']=componentClass,['/Script/UMG.CanvasPanel']=canvasClass,['/Script/UMG.CanvasPanelSlot']=slotClass})[path]
end
function FindFirstOf(name) return ({SaveDataManager=saveManager,BattleHUDManager=manager})[name] end
local entries,now,generation={},1,1
local customSound,customRequests,customHits=false,0,0
local lastGain
local missing=false
local function row(token,pressure,flash,sequence,x,y)
 return {token,1,token,x or 300,y or 200,10,pressure,6,flash>0 and 0.9 or 0,0,flash,sequence or 0,0,1000+token}
end
local function api(options)
 return {
  number=function(key,fallback) if options and options[key]~=nil then return options[key] end;return fallback end,
  valid=function(object) return object and object:IsValid() end,
  bridge=function(operation,index,gain)
   if operation==18 then
    lastGain=gain
    if not customSound then sounds=sounds+1 elseif index==1 then customHits=customHits+1 else customRequests=customRequests+1 end
    return 1
   end
   if operation==15 then if missing then return nil end;return #entries,generation,now end
   check(operation==16 and index>=1 and index<=3);reads=reads+1;return table.unpack(entries[index])
  end,
  construct=function() constructed=constructed+1;return widget() end,
  texture=function(name) check(name:match('^shield%-'));return widget() end,
  place=function() return widget() end,
  makeText=function() return widget() end,
  makeImage=function() return widget(),widget() end,
  visibility=function(object,state,value) if state.visible~=value then object:SetVisibility(value);state.visible=value end end,
  log=function() end
 }
end
local feedback=GuardFeedback.new(api())
feedback:update(parent,hud);check(constructed==0 and sounds==0)
entries={row(1,3,0)}
feedback:update(parent,hud)
local panel=feedback.pool[1]
check(panel and panel.visible==4)
check(panel.label.text=='GUARD' and panel.label.justification==1 and panel.label.shadowOffset.X==2)
check(panel.count==3 and panel.icons[1].remaining==1 and panel.icons[2].remaining==0.5 and panel.icons[3].remaining==0)
check(near(panel.icons[2].clipSlot.size.X,12) and near(panel.slot.position.X,-80) and near(panel.slot.position.Y,44))
check(panel.icons[1].clip.clipping==1 and panel.icons[1].fill.texture~=nil)
check(panel.parent==gauges[1].canvas:GetAddress() and panel.slot.anchors.Minimum.X==0.5)
check(gauges[1].gauge.text_enemy_name.RenderTransform.Translation.Y==-33)
local writes=panel.label.writes
local initialConstructed=constructed
feedback:update(parent,hud);check(panel.label.writes==writes and constructed==initialConstructed)
entries[1][7]=5
feedback:update(parent,hud);check(panel.tint=='strained' and near(panel.icons[1].remaining,0.5))
entries[1][10]=0.1
feedback:update(parent,hud);check(panel.tint=='flash' and panel.root.scale.X>1)
entries={row(1,0,1.2,1)}
feedback:update(parent,hud)
check(panel.label.text=='GUARD BROKEN' and panel.tint=='flash' and panel.icons[1].clip.visible==1)
check(panel.icons[1].left.visible==4 and panel.icons[1].right.visible==4 and panel.root.scale.X>1)
check(sounds==1 and near(lastGain,0.4) and panel.backdrop==nil)
now=1.05;feedback:update(parent,hud);check(sounds==1)
entries[1][12]=2;now=1.1;feedback:update(parent,hud);check(sounds==1)
now=1.3;feedback:update(parent,hud);check(sounds==1) -- Rate-limited events are not replayed later.
entries[1][12]=3;feedback:update(parent,hud);check(sounds==2)
entries={row(1,0,1.2,4),row(2,0,1.2,5)};now=1.5
feedback:update(parent,hud);check(sounds==3) -- One impact for a simultaneous crowd break.
entries={row(1,0,1.2,6,1200),row(2,0,1.2,7,400)};now=1.7
feedback:update(parent,hud);check(panel.visible==4 and sounds==4)
gauges[1].component.shown=false;entries={row(1,2,0)}
feedback:update(parent,hud);check(panel.visible==1)
check(gauges[1].gauge.text_enemy_name.RenderTransform.Translation.Y==3)
gauges[1].component.shown=true;entries={row(1,2,0,0,960,300)}
feedback:update(parent,hud);check(panel.slot.position.X==-80) -- Camera/world coordinates cannot displace a native child.
gauges[1].slot.point={X=80,Y=140}
feedback:update(parent,hud);check(panel.slot.position.X==-20 and panel.slot.position.Y==94)
ids[1]=9999;feedback:update(parent,hud);check(panel.visible==1) -- Native gauge reassigned to another enemy.
ids[1]=1001;feedback:update(parent,hud);check(panel.visible==4)
entries[1][14]=nil;feedback:update(parent,hud);check(panel.visible==1) -- Older bridge cannot draw at the origin.
entries[1][14]=1001
missing=true;feedback:update(parent,hud);check(panel.visible==1);missing=false
entries={row(1,2,0),row(2,3,0),row(3,4,0),row(4,5,0)}
reads=0;feedback:update(parent,hud);check(reads==3 and #feedback.pool==3)
entries={};feedback:update(parent,hud)
for _,entry in ipairs(feedback.pool) do check(entry.visible==1) end
entries={row(1,0,1.2,8)};now=2
feedback:update(parent,hud);local before=sounds
local oldRoot=feedback.pool[1].root
feedback:reset();feedback:update(parent,hud)
check(oldRoot.removed and sounds==before) -- Rebuilding widgets cannot replay sound.
generation=2;entries={};feedback:update(parent,hud)
entries={row(1,0,1.2,1)};now=0.1;feedback:update(parent,hud);check(sounds==before+1)
local indicatorOnly=GuardFeedback.new(api({['guardpressure.showpressurebar']=0,['guardpressure.breaksound']=0}))
indicatorOnly:update(parent,hud)
check(indicatorOnly.pool[1].label.text=='GUARD BROKEN' and indicatorOnly.pool[1].shieldVisibility==1 and indicatorOnly.pool[1].count==1)
check(indicatorOnly.pool[1].icons[1].left.visible==1 and indicatorOnly.pool[1].icons[1].empty.visible==1)
entries={row(1,2,0)};indicatorOnly:update(parent,hud);check(indicatorOnly.pool[1].visible==1)
local soundOnly=GuardFeedback.new(api({['guardpressure.showpressurebar']=0,['guardpressure.showbreakindicator']=0}))
entries={row(1,0,1.2,2)};now=1;soundOnly:update(parent,hud)
check(#soundOnly.pool==0 and sounds==before+2)
local disabled=GuardFeedback.new(api({['guardpressure.enabled']=0}))
reads=0;disabled:update(parent,hud);check(reads==0 and #disabled.pool==0)
parent.valid=false;feedback:update(parent,hud);check(feedback.pool[1].visible==1)
parent.valid=true;customSound=true
local custom=GuardFeedback.new(api())
entries={row(1,0,1.2,1)};now=1
before=sounds;custom:update(parent,hud)
check(customRequests==1 and sounds==before)
custom:update(parent,hud);check(customRequests==1)
entries[1][11]=0.9;custom:update(parent,hud)
check(custom.pool[1].icons[1].leftSlot.position.X<0 and custom.pool[1].icons[1].rightSlot.position.X>0)
local customPanel=custom.pool[1]
entries[1][9],entries[1][11],entries[1][15]=0,0.3,1
custom:update(parent,hud)
check(customPanel.visible==4 and customPanel.label.text=='GUARD BROKEN')
check(customPanel.icons[1].clip.visible==4 and customPanel.icons[1].left.visible==1)
for index=1,customPanel.count do check(customPanel.icons[index].remaining==1) end
check(customRequests==1)
entries[1][9],entries[1][11],entries[1][15]=0.3,0,0
custom:update(parent,hud)
check(customPanel.visible==4 and customPanel.label.visible==1 and customPanel.icons[1].left.visible==4)
entries[1][9],entries[1][15]=0,1
custom:update(parent,hud)
check(customPanel.visible==4 and customPanel.label.visible==1)
for index=1,customPanel.count do check(customPanel.icons[index].remaining==1) end
entries[1][15]=0
custom:update(parent,hud)
check(customPanel.visible==1)
entries={row(1,3,0)};entries[1][8]=9;custom:update(parent,hud);check(custom.pool[1].count==5)
entries[1][8]=12;custom:update(parent,hud);check(custom.pool[1].count==6)
entries[1][8]=100;custom:update(parent,hud);check(custom.pool[1].count==6)
local hitFeedback=GuardFeedback.new(api())
entries={row(7,2,0)};entries[1][10]=0.15;entries[1][13]=1;now=3
before=customHits;hitFeedback:update(parent,hud);check(customHits==before+1)
hitFeedback:update(parent,hud);check(customHits==before+1)
entries[1][13]=2;now=3.05;hitFeedback:update(parent,hud);check(customHits==before+1)
now=3.2;hitFeedback:update(parent,hud);check(customHits==before+1) -- No late replay of suppressed hits.
entries[1][13]=3;hitFeedback:update(parent,hud);check(customHits==before+2)
entries={row(7,0,1.2,4)};now=3.21;before=customRequests;hitFeedback:update(parent,hud)
check(customRequests==before+1) -- Full break interrupts a recent hit cue.
entries={row(8,2,0)};entries[1][10]=0.15;entries[1][13]=4;now=3.4
before=customHits;hitFeedback:update(parent,hud);check(customHits==before)
entries[1][13]=5;now=3.7;hitFeedback:update(parent,hud);check(customHits==before+1)
hitFeedback:reset();hitFeedback:update(parent,hud);check(customHits==before+1)
local levels=GuardFeedback.new(api({['guardpressure.breakvolumepercent']=50,['guardpressure.hitvolumepercent']=25}))
levels:playSound(false);check(near(lastGain,0.4))
levels:playSound(true);check(near(lastGain,0.2))
save.OptionData.Sound.SEVolume=20
levels:playSound(true);check(near(lastGain,0.25))
before=customHits;save.OptionData.Sound.SEVolume=0
levels:playSound(true);check(customHits==before)
save.OptionData.Sound.SEVolume=16
local mutedHit=GuardFeedback.new(api({['guardpressure.hitvolumepercent']=0}))
mutedHit:playSound(true);check(customHits==before)
local diagnosticApi=api({['general.liveevidence']=1})
local evidence={}
diagnosticApi.log=function(message) evidence[#evidence+1]=message end
local diagnostic=GuardFeedback.new(diagnosticApi)
diagnostic:playSound(false)
check(table.concat(evidence,'\n'):find('slider=16/20 cap=50.0% gain=0.4000 result=playback-accepted',1,true)~=nil)
local logged=#evidence
diagnostic:playSound(false);check(#evidence==logged)
save.OptionData.Sound.SEVolume=0
diagnostic:playSound(false);check(evidence[#evidence]:find('result=slider-muted',1,true)~=nil)
save.OptionData.Sound.SEVolume=16
diagnostic.hitVolume=0
diagnostic:playSound(true);check(evidence[#evidence]:find('result=cap-muted',1,true)~=nil)
diagnostic.liveEvidence=false
logged=#evidence;diagnostic:playSound(true);check(#evidence==logged)
local failingApi=api()
local createdRoots={}
local construct=failingApi.construct
failingApi.construct=function(...) local root=construct(...);createdRoots[#createdRoots+1]=root;return root end
local fail=true
failingApi.makeImage=function()
 local image,slot=widget(),widget()
 function image:SetBrushFromTexture(_,match) check(match==false);if fail then error('fixture texture failure') end end
 return image,slot
end
local recover=GuardFeedback.new(failingApi)
local ok=pcall(function() recover:update(parent,hud) end)
check(not ok and createdRoots[1].removed and #recover.pool==0)
fail=false;recover:update(parent,hud);check(recover.pool[1].root~=createdRoots[1] and recover.pool[1].visible==4)
return string.format('Guard feedback Lua presentation: %d assertions passed.',assertions)
'''))
