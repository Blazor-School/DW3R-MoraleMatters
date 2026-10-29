from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'build' / 'LuaValidationLibs'))
from lupa.lua54 import LuaRuntime

lua = LuaRuntime(unpack_returned_tuples=True)
for path in (root / 'Scripts').glob('*.lua'):
    lua.execute('assert(load(...))', path.read_text(encoding='utf-8', errors='strict'))
engine = (root / 'Scripts' / 'Experience.lua').read_text(encoding='utf-8')
lua.globals().Experience = lua.execute(engine)
print(lua.execute(r'''
local assertions=0
local function check(value) assertions=assertions+1;assert(value,"assertion "..assertions) end
local config={distress=true,rescue=true,trendSeconds=10,reasonSeconds=6,distressMorale=35,distressHealth=0.4,dropPercent=3,casualties=3,alertSeconds=6,alertCooldown=20,corpsCooldown=90,actionRadius=3000,rescueSeconds=120,recoverMorale=40,recoverHealth=0.5,enemyCasualties=5,stableSeconds=5}
local player={{X=0,Y=0,Z=0,Yaw=0}}
local groups,rows
local function setup()
 groups={[0]={id=0,corps=0,unit=100,side=0,rank=3,alive=true,health=0.3,x=0,y=0,z=0,casualties=0,breaks=0},[1]={id=1,corps=1,unit=200,side=1,rank=6,alive=true,health=1,x=500,y=0,z=0,casualties=0,breaks=0}}
 rows={[0]={id=0,leader=0,unit=100,side=0,base=300,morale=300,reason=0,reasonAge=100,wounded=groups[0]},[1]={id=1,leader=1,unit=200,side=1,base=300,morale=300,reason=0,reasonAge=100,wounded=nil}}
 return Experience.new(config)
end
local e=setup()
e:update(1,rows,groups,player,{})
check(e:notice(groups,player)==nil) -- Wounded plus low morale alone is insufficient.
e:update(2,rows,groups,player,{})
check(e.alert==nil)
groups[0].casualties=3
e:update(3,rows,groups,player,{})
check(e.alert and e.alert.officer==0)
check(e:notice(groups,player).title=='Ally in danger')
check(e.corps[0].rescue and not e.corps[0].rescue.helped)
local expiry=e.alert.expires
for i=1,20 do e:update(3,rows,groups,player,{}) end
check(e.alert.expires==expiry and e.clock==3)
check(#e.corps[0].history==3)
check(#e:update(2,rows,groups,player,{})==0 and e.clock==3)
e:update(4,rows,groups,player,{{id=0,corps=0,rallied=true,healed=50}})
check(e.rallies==1 and e.healed==50 and e.corps[0].rescue.helped)
groups[0].health=0.8;rows[0].morale=450;rows[0].base=450
e:update(9,rows,groups,player,{})
check(e:notice(groups,player).title=='Rescue in progress')
check(e:notice(groups,player).hint=='Defeat enemies near this officer')
check(#e:update(15,rows,groups,player,{})==0) -- Visiting and healing alone does not complete it.
groups[1].casualties=5
e:update(16,rows,groups,player,{})
check(e.corps[0].rescue.enemyLosses==5)
check(e:notice(groups,player).hint=='Threat reduced - stabilize the corps')
check(#e:update(20,rows,groups,player,{})==0)
local completions=e:update(21,rows,groups,player,{})
check(#completions==1 and completions[1].officer==0)
check(#e:update(22,rows,groups,player,{})==0) -- Pending bridge payment cannot be repeated.
e:complete(completions[1],10)
check(e.rescued==1 and e.corps[0].completed)
check(e:notice(groups,player).title=='Ally rescued')
e:complete(completions[1],10);check(e.rescued==1)
local report=e:report(function(unit) return 'Officer '..unit end)
check(report:find('Allied corps rescued: 1',1,true)~=nil)
check(report:find('Officer health restored: 50',1,true)~=nil)
check(report:find('not personal KOs',1,true)~=nil)
check(e.bestRecovery==150)
rows[0].morale=100;rows[0].base=100;groups[0].health=0.2;groups[0].casualties=20
e:update(100,rows,groups,player,{})
check(e.corps[0].rescue==nil) -- Once per corps per battle.

e=setup();e:update(1,rows,groups,player,{})
groups[1].breaks=1;e:update(2,rows,groups,player,{})
check(e.broken==1)
for i=3,10 do e:update(i,rows,groups,player,{}) end
check(e.broken==1)
groups[1].corps=3;groups[1].casualties=20;e:update(11,rows,groups,player,{})
check(e.broken==1) -- Transfers do not inherit prior casualty deltas.
e=setup();groups[1].x=9000;e:update(1,rows,groups,player,{})
groups[1].breaks=1;e:update(2,rows,groups,player,{})
check(e.broken==0)
groups[1].x=0;e:update(3,rows,groups,player,{})
check(e.broken==0) -- Walking into a previously broken formation grants no retroactive credit.

e=setup();e:update(1,rows,groups,player,{})
groups[0].health=1;rows[0].morale=200;e:update(2,rows,groups,player,{})
check(e.alert==nil) -- Rapid morale loss with a healthy officer is not an emergency.
check(rows[0].trend=='v')
rows[0].reason=4;rows[0].reasonAge=1;e:update(3,rows,groups,player,{})
check(rows[0].reasonText=='Nearby defeat panic')
rows[0].reasonAge=7;e:update(4,rows,groups,player,{})
check(rows[0].reasonText==nil) -- No guessed cause from the trend.
rows[0].phase=1;e:update(5,rows,groups,player,{})
check(rows[0].phaseText=='Falling back')
rows[0].phase=3;e:update(6,rows,groups,player,{})
check(rows[0].phaseText=='Rejoining')

e=setup();e:update(1,rows,groups,player,{})
groups[0].casualties=3;e:update(2,rows,groups,player,{})
groups[0].alive=false;e:update(3,rows,groups,player,{})
check(e.corps[0].rescue==nil and e:notice(groups,player)==nil)
e=setup();e:update(1,rows,groups,player,{})
groups[0].casualties=3;e:update(2,rows,groups,player,{})
e:update(123,rows,groups,player,{})
check(e.corps[0].rescue==nil) -- Objective expiry uses battle time.
e=setup();e:update(1,rows,groups,player,{})
groups[0].casualties=3;e:update(2,rows,groups,player,{})
e:update(3,rows,groups,player,{{id=0,corps=0,rallied=true,healed=50}})
groups[1].x=10000;groups[1].casualties=10;e:update(4,rows,groups,player,{})
check(e.corps[0].rescue.enemyLosses==0)
groups[1].x=0;groups[1].alive=false;groups[1].casualties=11;e:update(5,rows,groups,player,{})
check(e.corps[0].rescue.leaderDefeated)
rows[0]=nil;e:update(6,rows,groups,player,{})
check(e.corps[0].rescue==nil) -- Completely defeated corps are removed.

check(Experience.direction({x=1000,y=0},player)=='10m ahead')
check(Experience.direction({x=0,y=1000},player)=='10m right')
check(Experience.direction({x=-1000,y=0},player)=='10m behind')
check(Experience.direction({x=0,y=-1000},player)=='10m left')
check(Experience.direction({x=1000,y=0},{{X=0,Y=0,Yaw=90}})=='10m left')
check(Experience.direction({x=0,y=0},{})=='Position unavailable')
config.distress=false;config.rescue=false
e=setup();e:update(1,rows,groups,player,{})
groups[0].casualties=3;e:update(2,rows,groups,player,{})
check(e.alert==nil and e.corps[0].rescue==nil)
e.finalized=true;local count=e.rallies
e:update(3,rows,groups,player,{{id=0,corps=0,rallied=true,healed=50}})
check(e.rallies==count)
return 'Passed '..assertions..' Lua objective, timing, attribution, direction and report assertions.'
'''))

# Execute the entire real main script with UMG/native bridge stand-ins, including callbacks.
lua = LuaRuntime(unpack_returned_tuples=True)
harness = r'''
widgets,callbacks,bindings={}, {}, {}
clock,generation,resultActive=0,0,false
local address=0
function widget(kind)
 address=address+1
 local w={address=address,kind=kind,Font={},ReferencedObjects={},children={}}
 function w:IsValid() return true end
 function w:GetAddress() return self.address end
 function w:IsA(class) return self.kind==class.kind end
 function w:IsInViewport() return resultActive end
 function w:SetVisibility(v) self.visible=v end
 function w:SetColorAndOpacity(c) self.color=c end
 function w:SetRenderTransformAngle(a) self.angle=a;self.angleWrites=(self.angleWrites or 0)+1 end
 function w:SetPosition(p) self.position=p end
 function w:SetAnchors(a) self.anchors=a end
 function w:SetAlignment(a) self.alignment=a end
 function w:SetSize(s) self.size=s end
 function w:SetText(t) self.text=t;self.writes=(self.writes or 0)+1 end
 function w:SetFont(f) self.Font=f end
 function w:SetClipping(c) self.clipping=c end
 function w:SetTextOverflowPolicy(p) self.overflow=p end
 function w:SetBrushFromTexture(t) self.texture=t end
 function w:RemoveFromParent() self.removed=true end
 function w:GetParent() return self.parent end
 function w:AddChildToCanvas(child)
  assert(self.kind=='CanvasPanel')
  local slot=widget('slot');child.Slot=slot;child.parent=self;self.children[#self.children+1]=child;return slot
 end
 widgets[#widgets+1]=w
 return w
end
local function stringValue(value) return {ToString=function() return value end} end
classes={}
function StaticFindObject(path)
 if classes[path] then return classes[path] end
 local w=widget(path:match('%.([^%.]+)$'));classes[path]=w
 function w:GetUnitName(unit) return stringValue(unit>=405 and unit<=413 and 'Vice General' or 'Officer '..unit) end
 function w:GetFaceID(unit) return unit end
 function w:GetFaceStillPath(unit) return stringValue('face'..unit) end
 function w:GetDataTableRowNames() end
 function w:GetDataTableColumnAsString() return {} end
 return w
end
function StaticConstructObject(class) return widget(class.kind) end
function LoadAsset() return widget('asset') end
function FText(value) return value end
function FName(value) return value end
Key={F7='F7',F8='F8',PAGE_DOWN='DOWN',PAGE_UP='UP'}
function RegisterKeyBind(key,callback) bindings[key]=callback end
function LoopAsync(_,callback) callbacks.tick=callback end
function ExecuteInGameThread(callback) callback() end
function ExecuteWithDelay(_,callback) callback() end
local hud=widget('hud');hud.canvas_hud=widget('CanvasPanel')
hud.BP_parts_gauge_enemy=widget('gauge');hud.BP_parts_gauge_enemy.text_enemy_name=widget('TextBlock')
local battle=widget('BattleHUDManager');battle.Battle2DHUDWidget=hud
battle.ItemHeadWidgetNPC={ForEach=function() end}
function battle:SetHeadItemNPC() end
function battle:GetEnemyGaugeUnit() return nil end
local characters=widget('CharacterManager');characters.m_arpCharacterList={}
local manager=widget('AlgoManager')
local instance=widget('GameInstance')
local result=widget('ResultHUDManager');result.CommonWidget=widget('UserWidget')
result.CommonWidget.WidgetTree=widget('WidgetTree');result.CommonWidget.WidgetTree.RootWidget=widget('CanvasPanel')
function FindFirstOf(name)
 return ({BattleHUDManager=battle,CharacterManager=characters,AlgoManager=manager,GameInstance=instance,ResultHUDManager=resultActive and result or nil})[name]
end
nativeGroups={}
for id=0,4 do nativeGroups[id]={id<3 and 0 or 1,100+id,id==0 and 3 or 6,id<3 and 0 or 1,1,100,100,id*100,0,0,8,0,0,0} end
local function bridge(op,id)
 if op==0 then return 1 end
 if op==1 then generation=generation+1;clock=0;return end
 if op==2 then return table.unpack(nativeGroups[id]) end
 if op==3 then return id==0 and 0 or 3,id,400,500,100,120,8,100,1,clock end
 if op==7 then return groupCount or 5,2 end
 if op==8 then return 0 end
 if op==10 then return 0 end
 if op==12 then return clock,generation end
 if op==13 then return 10 end
end
package.loadlib=function() return bridge end
function tick(time) clock=time;callbacks.tick() end
function resultOn() resultActive=true end
function changeResultCanvas() result.CommonWidget.WidgetTree.RootWidget=widget('CanvasPanel') end
function newBattle() resultActive=false;battle.Battle2DHUDWidget=widget('hud');battle.Battle2DHUDWidget.canvas_hud=widget('CanvasPanel') end
-- Do not write reports or read the user's INI while running fixtures.
local originalOpen=io.open
io.open=function(path,mode)
 if mode=='r' then return {lines=function() return (iniText or ''):gmatch('[^\n]+') end,close=function() end} end
 return {write=function() return true end,close=function() end}
end
'''
lua.execute(harness)
source = (root / 'Scripts' / 'main.lua').read_text(encoding='utf-8')
lua.execute('local source,name=...;assert(load(source,name))()', source, '@' + str(root / 'Scripts' / 'main.lua'))
print(lua.execute(r'''
tick(0);tick(1)
local errors=0
local percent,detail,commander
for _,w in ipairs(widgets) do
 if w.text=='56%' then percent=w end
 if w.text=='Allied support' then detail=w end
end
assert(percent and detail,'Real HUD did not render percentage and recorded cause')
local writes=detail.writes;tick(1);assert(detail.writes==writes,'Unchanged detail text was rewritten')
bindings.F7();tick(2);bindings.DOWN();tick(3)
bindings.F8();tick(4)
local report
for _,w in ipairs(widgets) do if w.text and w.text:find('MORALE MATTERS',1,true) then report=w end end
assert(report and report.text:find('Allied corps rescued: 0',1,true))
assert(report.parent.Slot.anchors==nil,'Battle report inherited results-screen anchoring')
resultOn();tick(5)
local resultReports=0
local resultReport
for _,w in ipairs(widgets) do
 if w.kind=='TextBlock' and w.text and w.text:find('MORALE MATTERS',1,true) then resultReports=resultReports+1;resultReport=w end
end
assert(resultReports==2,'Results overlay did not preserve the battle report')
local function checkResultPlacement(text)
 local slot=text.parent.Slot
 assert(slot.anchors.Minimum.X==1 and slot.anchors.Maximum.X==1 and slot.alignment.X==1,'Results report is not anchored to the right edge')
 assert(slot.anchors.Minimum.Y==0 and slot.anchors.Maximum.Y==0 and slot.alignment.Y==0)
 assert(slot.position.X==-36 and slot.position.Y==170,'Results report no longer clears the header and left menu')
 assert(slot.size.X==540 and slot.size.Y==240)
end
checkResultPlacement(resultReport)
assert(resultReport.text==report.text,'Moving the report changed the final snapshot')
bindings.F8();tick(6)
assert(resultReport.parent.visible==1,'Results report toggle did not hide the moved overlay')
bindings.F8();tick(7)
assert(resultReport.parent.visible==4)
changeResultCanvas();tick(8)
assert(resultReport.parent.removed,'Changing results canvas left the old report attached')
for _,w in ipairs(widgets) do
 if w.kind=='TextBlock' and w.text and w.text:find('MORALE MATTERS',1,true) then resultReport=w end
end
checkResultPlacement(resultReport)
newBattle();tick(0);tick(1)
assert(generation==2,'New battle did not reset the native ledger')
return 'Passed complete main.lua HUD, cached setters, expanded view, report key, upper-right results placement/toggling/canvas replacement and next-battle lifecycle fixtures.'
'''))

for width, portrait, commander, details in ((220,16,16,1),(220,64,64,1),(300,30,24,1),(900,64,64,1),(300,30,24,0)):
    layout = LuaRuntime(unpack_returned_tuples=True)
    layout.execute(harness)
    layout.globals().iniText = f'[HUD]\nPanelWidth={width}\nPortraitSize={portrait}\nCommanderPortraitSize={commander}\n[Battlefield]\nMoraleDetails={details}\n'
    layout.globals().expectedPortrait = portrait
    layout.globals().expectedCommander = commander
    layout.globals().api = layout.execute('local source,name=...;return assert(load(source,name))()',source+'\nreturn {newPanel=newPanel,render=render,formations=formations}','@'+str(root/'Scripts'/'main.lua'))
    layout.execute(r'''
tick(0);tick(1)
groupCount=30
for id=0,29 do nativeGroups[id]={0,id<6 and 100+id or id<15 and 399+id or 420+id%3,id<6 and 3 or 6,0,1,100,100,0,0,0,8,0,0,0} end
nativeGroups[1][6]=25
nativeGroups[2][6]=70
local rows=api.formations({})
assert(#rows[0].members==15,'Expected five officers, nine individual Vice Generals and one collapsed row')
for index,member in ipairs(rows[0].members) do
 assert(member.id~=rows[0].leader,'Corps commander was duplicated')
 if index<15 then assert(member.count==nil) else assert(member.count==15,'Mixed generic unit types were not combined') end
end
local panel=api.newPanel(widget('CanvasPanel'),1)
local height=api.render(panel,rows[0],36,170)
assert(panel.icons[1].healthSlot.size.X==math.floor(panel.icons[1].healthWidth*0.25+0.5))
assert(panel.icons[2].healthSlot.size.X==math.floor(panel.icons[2].healthWidth*0.70+0.5))
assert(panel.icons[15].label.text=='×15' and panel.icons[15].health.visible==1 and panel.icons[15].healthBack.visible==1)
for index,icon in ipairs(panel.icons) do
 assert(icon.slot.size.X==expectedPortrait and icon.slot.size.Y==expectedPortrait,'Portraits lost the configured size')
 assert(icon.label.clipping==1 and icon.label.overflow==1)
 if index<15 then
  assert(icon.healthBack.visible==4 and icon.health.visible==4)
  assert(icon.healthSlot.position.Y==icon.labelSlot.position.Y+20)
  assert(icon.healthSlot.position.X==icon.labelSlot.position.X and icon.labelSlot.position.X>icon.slot.position.X+icon.slot.size.X)
  assert(icon.label.text==(index<=5 and 'Officer '..(100+index) or 'Vice General'))
 end
 if index>1 then assert(icon.slot.position.Y>panel.icons[index-1].slot.position.Y+panel.icons[index-1].slot.size.Y,'Officer rows overlap') end
end
local signature=panel.members
rows[0].members[1].health=0.5;api.render(panel,rows[0],36,170)
assert(panel.members==signature,'HP change rebuilt the portrait layout')
assert(panel.icons[1].healthSlot.size.X==math.floor(panel.icons[1].healthWidth*0.5+0.5))
nativeGroups[1][5]=0
local updated=api.formations({});height=api.render(panel,updated[0],36,170)
assert(#updated[0].members==14 and panel.icons[15].image.visible==1 and panel.icons[15].label.visible==1)
assert(panel.icons[14].label.text=='×15' and panel.icons[14].health.visible==1,'Reused officer row retained a health bar')
assert(updated[0].members[1].id==2 and panel.icons[1].healthSlot.size.X==math.floor(panel.icons[1].healthWidth*0.70+0.5))
nativeGroups[15][5]=0
updated=api.formations({});height=api.render(panel,updated[0],36,170)
assert(panel.icons[14].label.text=='×14','Generic casualty did not update the count')
rows=updated
assert(panel.commander.Slot.position.X+panel.commander.Slot.size.X<panel.title.Slot.position.X)
assert(panel.commander.Slot.size.X==expectedCommander and panel.commander.Slot.size.Y==expectedCommander)
assert(panel.commander.Slot.position.X+panel.commander.Slot.size.X<panel.trendIcon.root.Slot.position.X)
assert(panel.trendIcon.root.Slot.position.X+14<panel.title.Slot.position.X)
assert(panel.trendIcon.root.Slot.position.Y+14<panel.permanentSlot.position.Y)
assert(panel.title.Slot.position.X+panel.title.Slot.size.X<panel.percentage.Slot.position.X)
assert(panel.commander.Slot.position.Y+panel.commander.Slot.size.Y<panel.permanentSlot.position.Y)
assert(panel.title.overflow==1 and panel.title.clipping==1)
assert(height==panel.backdropSlot.size.Y and height==panel.slot.size.Y)
for _,icon in ipairs(panel.icons) do
 if icon.image.visible==4 then
  local right=icon.labelSlot.position.X+icon.labelSlot.size.X
  assert(right<=panel.slot.size.X-16)
  assert(panel.portraitCanvas.Slot.position.Y+icon.slot.position.Y+icon.slot.size.Y<height)
  if icon.health.visible==4 then assert(panel.portraitCanvas.Slot.position.Y+icon.healthSlot.position.Y+3<height) end
 end
end
if panel.detail then
 assert(panel.detail.text=='Morale')
 assert(panel.detail.Slot.position.Y>panel.permanentSlot.position.Y+5)
 assert(panel.status.Slot.position.Y+panel.status.Slot.size.Y<panel.portraitCanvas.Slot.position.Y)
end
assert(panel.trendIcon.arrow.visible==1 and panel.trendIcon.steady.visible==4)
rows[0].trend='^';api.render(panel,rows[0],36,170)
assert(panel.trendIcon.arrow.visible==4 and panel.trendIcon.steady.visible==1 and panel.trendIcon.arrow.angle==0)
for _,part in ipairs(panel.trendIcon.parts) do assert(part.color.G>part.color.R) end
local writes=panel.trendIcon.arrow.angleWrites
api.render(panel,rows[0],36,170)
assert(panel.trendIcon.arrow.angleWrites==writes,'Unchanged trend icon was rewritten')
rows[0].trend='v';api.render(panel,rows[0],36,170)
assert(panel.trendIcon.arrow.visible==4 and panel.trendIcon.steady.visible==1 and panel.trendIcon.arrow.angle==180)
for _,part in ipairs(panel.trendIcon.parts) do assert(part.color.R>part.color.G) end
rows[0].trend='=';api.render(panel,rows[0],36,170)
assert(panel.trendIcon.arrow.visible==1 and panel.trendIcon.steady.visible==4)
rows[0].trend='^';api.render(panel,rows[0],36,170)
rows[0].trend=nil;api.render(panel,rows[0],36,170)
assert(panel.trendIcon.arrow.visible==1 and panel.trendIcon.steady.visible==4,'Reused panel retained previous corps trend')
-- Dead, inactive and neutral leaders never inflate the collapsed count.
nativeGroups[16][5]=0;nativeGroups[17][4]=2;nativeGroups[18][1]=-1
updated=api.formations({});api.render(panel,updated[0],36,170)
assert(panel.icons[14].label.text=='×11')
for id=19,29 do nativeGroups[id][5]=0 end
updated=api.formations({});api.render(panel,updated[0],36,170)
assert(#updated[0].members==13 and panel.icons[14].image.visible==1 and panel.icons[14].label.visible==1,'Zero-count summary remained visible')
-- A single generic survivor keeps an explicit count, on both sides.
nativeGroups[29][5]=1
updated=api.formations({});updated[0].side=1;api.render(panel,updated[0],36,170)
assert(panel.icons[14].label.text=='×1' and panel.icons[14].health.visible==1)
assert(panel.icons[1].label.color.SpecifiedColor.R>panel.icons[1].label.color.SpecifiedColor.G)
assert(panel.icons[1].health.color.R>panel.icons[1].health.color.G)
nativeGroups[29][5]=0
for id=1,14 do nativeGroups[id][5]=0 end
updated=api.formations({});height=api.render(panel,updated[0],36,170)
assert(#updated[0].members==0 and panel.icons[1].image.visible==1 and panel.icons[1].label.visible==1)
assert(panel.portraitCanvas.Slot.size.Y==0 and panel.slot.size.Y==height,'Empty roster retained its previous height')
-- Reusing the summary widget for a named officer restores its name and HP bar.
nativeGroups[29][5]=1
updated=api.formations({});api.render(panel,updated[0],36,170)
assert(panel.icons[1].label.text=='×1' and panel.icons[1].health.visible==1)
nativeGroups[1][5]=1;nativeGroups[1][6]=70
updated=api.formations({});api.render(panel,updated[0],36,170)
assert(panel.icons[1].label.text=='Officer 101' and panel.icons[1].health.visible==4)
assert(panel.icons[1].healthSlot.size.X==math.floor(panel.icons[1].healthWidth*0.70+0.5))
assert(panel.icons[2].label.text=='×1' and panel.icons[2].health.visible==1)
-- Full and empty HP clamp cleanly without changing row geometry.
local signature=panel.members
updated[0].members[1].health=1.5;api.render(panel,updated[0],36,170)
assert(panel.icons[1].healthSlot.size.X==panel.icons[1].healthWidth)
updated[0].members[1].health=-0.2;api.render(panel,updated[0],36,170)
assert(panel.icons[1].healthSlot.size.X==0 and panel.members==signature)
''')
print('Passed five full-HUD layout cases: individual officers/all Vice General variants, one unnamed count, casualties/filtering, widget reuse, live HP, 16-64px configured portraits, narrow panels, ellipsis, dynamic height and trend caching.')
