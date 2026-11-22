-- Overhead fortitude shields for the three most recently pressured enemies.
-- Native snapshots own timing and identity; presentation never changes combat.
local GuardFeedback = {}
GuardFeedback.__index = GuardFeedback
local WIDTH, HEIGHT, ICON, ICON_HEIGHT, STEP, MAX_SHIELDS = 200, 84, 24, 28, 28, 6
local colors = {
    pressure={R=1,G=0.76,B=0.26,A=1},
    strained={R=1,G=0.39,B=0.12,A=1},
    flash={R=1,G=1,B=0.94,A=1},
    white={R=1,G=1,B=1,A=1}
}
local function finite(value) return type(value)=="number" and value==value and value~=math.huge and value~=-math.huge end

function GuardFeedback.new(api)
    local self = setmetatable({api=api,pool={},names={},lastSequence=0,lastHitSequence=0,lastSoundAt=-math.huge,lastBreakAt=-math.huge}, GuardFeedback)
    self.enabled = api.number("guardpressure.enabled",1,0,1)==1
    self.bar = api.number("guardpressure.showpressurebar",1,0,1)==1
    self.indicator = api.number("guardpressure.showbreakindicator",1,0,1)==1
    self.sound = api.number("guardpressure.breaksound",1,0,1)==1
    self.liveEvidence = api.number("general.liveevidence",0,0,1)==1
    self.breakVolume = api.number("guardpressure.breakvolumepercent",50,0,100)/100
    self.hitVolume = api.number("guardpressure.hitvolumepercent",50,0,100)/100
    self.duration = api.number("guardpressure.indicatorseconds",1.2,0.3,3)
    if self.liveEvidence then api.log(string.format("Live evidence: guard feedback settings shields=%s indicator=%s sound=%s breakCap=%.1f%% hitCap=%.1f%% backdrop=false",tostring(self.bar),tostring(self.indicator),tostring(self.sound),self.breakVolume*100,self.hitVolume*100)) end
    return self
end

function GuardFeedback:hide()
    for _, panel in pairs(self.pool) do if self.api.valid(panel.root) then self.api.visibility(panel.root,panel,1) end end
    self:restoreNames({})
end

function GuardFeedback:reset()
    self:hide()
    for _, panel in pairs(self.pool) do if self.api.valid(panel.root) then panel.root:RemoveFromParent() end end
    self.pool,self.parent = {},nil
    self.soundEvidence,self.soundEvidenceCount = {},0
end

function GuardFeedback:panel(index, parent, hud)
    local panel = self.pool[index]
    if panel and self.api.valid(panel.root) then
        if panel.parent==parent:GetAddress() then return panel end
        panel.root:RemoveFromParent()
        self.pool[index]=nil
    end
    local textures = {}
    for _, name in ipairs({"empty","full","left","right"}) do textures[name]=self.api.texture("shield-"..name,hud) end
    local root = self.api.construct("CanvasPanel",parent)
    root:SetVisibility(1)
    local ok, created = pcall(function()
        local slot = self.api.place(parent,root,0,0,WIDTH,HEIGHT)
        root:SetRenderTransformPivot({X=0.5,Y=0.5})
        local label = self.api.makeText(root,0,54,WIDTH,28,18)
        label:SetJustification(1)
        label:SetShadowOffset({X=2,Y=2})
        label:SetShadowColorAndOpacity({R=0,G=0,B=0,A=1})
        panel = {root=root,slot=slot,parent=parent:GetAddress(),label=label,icons={}}
        local function image(parent, texture)
            local widget, imageSlot = self.api.makeImage(parent,0,0,ICON,ICON_HEIGHT,colors.white)
            widget:SetBrushFromTexture(texture,false)
            return widget,imageSlot
        end
        for number=1,MAX_SHIELDS do
            local glyph = self.api.construct("CanvasPanel",root)
            local glyphSlot = self.api.place(root,glyph,0,8,ICON,ICON_HEIGHT)
            local empty = image(glyph,textures.empty)
            local clip = self.api.construct("CanvasPanel",glyph)
            local clipSlot = self.api.place(glyph,clip,0,0,ICON,ICON_HEIGHT)
            clip:SetClipping(1)
            local fill = image(clip,textures.full)
            local left,leftSlot = image(glyph,textures.left)
            local right,rightSlot = image(glyph,textures.right)
            panel.icons[number] = {root=glyph,slot=glyphSlot,empty=empty,clip=clip,clipSlot=clipSlot,fill=fill,left=left,leftSlot=leftSlot,right=right,rightSlot=rightSlot}
        end
        return panel
    end)
    if not ok then root:RemoveFromParent();error(created) end
    panel = created
    self.pool[index] = created
    return panel
end

function GuardFeedback:render(panel, fraction, threshold, broken, hitFlash, breakFlash, showShields, showIndicator, now)
    local count = showShields and math.max(3,math.min(MAX_SHIELDS,math.ceil(threshold/2))) or 1
    local rowWidth = count*STEP-(STEP-ICON)
    local start = (WIDTH-rowWidth)/2
    if panel.count~=count then
        for index,icon in ipairs(panel.icons) do
            icon.slot:SetPosition({X=start+(index-1)*STEP,Y=8})
            self.api.visibility(icon.root,icon,index<=count and 4 or 1)
        end
        panel.count=count
    end
    local age = math.max(0,self.duration-(breakFlash or 0))
    local tint = broken and age<0.16 and "flash" or broken and "strained" or (hitFlash or 0)>0 and "flash" or fraction>=0.75 and "strained" or "pressure"
    local scale = broken and 1.12 or 1+0.12*math.min(1,(hitFlash or 0)/0.16)
    if not broken and fraction>=0.75 then scale=scale+0.025*(1+math.sin(now*14)) end
    if panel.scale~=scale then panel.root:SetRenderScale({X=scale,Y=scale});panel.scale=scale end
    for index=1,count do
        local icon = panel.icons[index]
        local remaining = math.max(0,math.min(1,(1-fraction)*count-(index-1)))
        if icon.remaining~=remaining then icon.clipSlot:SetSize({X=ICON*remaining,Y=ICON_HEIGHT});icon.remaining=remaining end
        if icon.broken~=broken or icon.showShields~=showShields then
            icon.empty:SetVisibility(showShields and not broken and 4 or 1);icon.clip:SetVisibility(showShields and not broken and 4 or 1)
            icon.left:SetVisibility(showShields and broken and 4 or 1);icon.right:SetVisibility(showShields and broken and 4 or 1)
            icon.broken,icon.showShields=broken,showShields
        end
        if icon.tint~=tint then
            icon.fill:SetColorAndOpacity(colors[tint]);icon.left:SetColorAndOpacity(colors[tint]);icon.right:SetColorAndOpacity(colors[tint])
            icon.tint=tint
        end
        if broken then
            local spread = 10*math.min(1,age/0.22)
            icon.leftSlot:SetPosition({X=-spread,Y=spread*0.3});icon.rightSlot:SetPosition({X=spread,Y=spread*0.3})
            local opacity = math.max(0.35,1-age*0.8)
            icon.left:SetRenderOpacity(opacity);icon.right:SetRenderOpacity(opacity)
        end
    end
    local label = showIndicator and "GUARD BROKEN" or "GUARD"
    panel.label:SetVisibility(showIndicator and 4 or 1)
    if panel.text~=label then panel.label:SetText(FText(label));panel.text=label end
    if panel.tint~=tint then panel.label:SetColorAndOpacity({SpecifiedColor=colors[tint],ColorUseRule=0});panel.tint=tint end
    panel.fraction,panel.shieldVisibility = fraction,showShields and 4 or 1
end

function GuardFeedback:restoreNames(active)
    for address, entry in pairs(self.names) do
        if not active[address] then
            if self.api.valid(entry.widget) then entry.widget:SetRenderTranslation({X=entry.x,Y=entry.y}) end
            self.names[address]=nil
        end
    end
end

function GuardFeedback:shiftName(widget, active)
    if not self.api.valid(widget) then return end
    local address = widget:GetAddress()
    local entry = self.names[address]
    if not entry then
        local translation = widget.RenderTransform.Translation
        entry = {widget=widget,x=translation.X,y=translation.Y}
        self.names[address]=entry
    end
    active[address]=true
    widget:SetRenderTranslation({X=entry.x,Y=entry.y-36})
end

function GuardFeedback:anchor(objectIndex)
    local valid = self.api.valid
    if not finite(objectIndex) or objectIndex<0 then return nil end
    if not valid(self.hudManager) then self.hudManager=FindFirstOf("BattleHUDManager") end
    if not valid(self.hudManager) then return nil end
    if not valid(self.componentClass) then self.componentClass=StaticFindObject("/Script/UMG.WidgetComponent") end
    if not valid(self.canvasClass) then self.canvasClass=StaticFindObject("/Script/UMG.CanvasPanel") end
    if not valid(self.slotClass) then self.slotClass=StaticFindObject("/Script/UMG.CanvasPanelSlot") end
    if not valid(self.componentClass) or not valid(self.canvasClass) or not valid(self.slotClass) then return nil end
    local result
    -- UpdateNPCGauge maps UnitID to the enemy's UObject index, not its corps/group ID.
    -- Both arrays use the same slot; never retain a match across native gauge reuse.
    self.hudManager.NPCHeadWidgetInfo:ForEach(function(player, element)
        if player~=1 then return end
        local info = element:get()
        info.UnitID:ForEach(function(index, unit)
            if result or unit:get()~=objectIndex then return end
            local actor = info.Actors[index]
            if not valid(actor) then return end
            local component = actor:GetComponentByClass(self.componentClass)
            if not valid(component) or not component:IsVisible() then return end
            local gauge = component:GetUserWidgetObject()
            if not valid(gauge) then return end
            local hp = gauge.gauge_head_base
            if not valid(hp) then return end
            local parent,slot = hp:GetParent(),hp.Slot
            if not valid(parent) or not parent:IsA(self.canvasClass) or not valid(slot) or not slot:IsA(self.slotClass) then
                if not self.layoutWarning then
                    self.layoutWarning=true
                    self.api.log("Guard shield anchor unavailable: "..gauge:GetFullName()..", parent="..(valid(parent) and parent:GetFullName() or "missing"))
                end
                return
            end
            local anchors,position,size,alignment = slot:GetAnchors(),slot:GetPosition(),slot:GetSize(),slot:GetAlignment()
            if anchors.Minimum.X~=anchors.Maximum.X or anchors.Minimum.Y~=anchors.Maximum.Y then return end
            local x = position.X+size.X*(0.5-alignment.X)-WIDTH/2
            local y = position.Y-size.Y*alignment.Y-40
            if not finite(x) or not finite(y) then return end
            result={parent=parent,gauge=gauge,anchors={Minimum={X=anchors.Minimum.X,Y=anchors.Minimum.Y},Maximum={X=anchors.Maximum.X,Y=anchors.Maximum.Y}},x=x,y=y}
        end)
    end)
    if result and not self.anchorReady then
        self.anchorReady=true
        self.api.log(string.format("Guard shields matched native overhead gauge: object=%d, local=(%.1f,%.1f), parent=%s",objectIndex,result.x,result.y,result.parent:GetFullName()))
    end
    return result
end

function GuardFeedback:logSound(hit,slider,gain,result)
    if not self.liveEvidence then return end
    local kind=hit and "hit" or "break"
    local cap=hit and self.hitVolume or self.breakVolume
    local state=string.format("slider=%s/20 cap=%.1f%% gain=%.4f result=%s",tostring(slider),cap*100,gain,result)
    self.soundEvidence=self.soundEvidence or {}
    if self.soundEvidence[kind]==state or (self.soundEvidenceCount or 0)>=128 then return end
    self.soundEvidence[kind]=state
    self.soundEvidenceCount=(self.soundEvidenceCount or 0)+1
    self.api.log("Live evidence: guard sound "..kind.." "..state)
    if self.soundEvidenceCount==128 then self.api.log("Live evidence: guard sound detail limit reached for this battle (128 changes).") end
end

function GuardFeedback:playSound(hit)
    local cap = hit and self.hitVolume or self.breakVolume
    if cap<=0 then self:logSound(hit,"unread",0,"cap-muted");return end
    if not self.api.valid(self.saveManager) then self.saveManager=FindFirstOf("SaveDataManager") end
    local save = self.api.valid(self.saveManager) and self.saveManager.GameStatusSaveGame
    if not self.api.valid(save) then self:logSound(hit,"unavailable",0,"save-unavailable");return end
    -- DT_OptionData defines 21 Sound Effects steps, from 0 through 20.
    local slider = save.OptionData.Sound.SEVolume
    if not finite(slider) or slider<=0 then self:logSound(hit,slider,0,finite(slider) and "slider-muted" or "invalid-slider");return end
    local gain=cap*math.min(1,slider/20)
    local accepted=self.api.bridge(18,hit and 1 or 0,gain)==1
    self:logSound(hit,slider,gain,accepted and "playback-accepted" or "playback-failed-or-busy")
    if accepted then
        local key = hit and "hitSoundReady" or "soundReady"
        if not self[key] then self[key]=true;self.api.log((hit and "Guard-hit" or "Guard-break").." sound requested: Origins reference clip sample (2D).") end
    end
end

function GuardFeedback:update(parent, hud)
    if not self.enabled or not (self.bar or self.indicator or self.sound) then self:hide(); return end
    if not self.api.valid(parent) or not self.api.valid(hud) then self:hide(); return end
    if self.parent and self.parent~=parent:GetAddress() then self:reset() end
    self.parent = parent:GetAddress()
    local count,generation,now = self.api.bridge(15)
    if not count then self:hide(); return end -- Graceful mismatch with an older native core.
    if self.generation~=generation then
        self:hide()
        self.generation,self.lastSequence,self.lastHitSequence,self.lastSoundAt,self.lastBreakAt = generation,0,0,-math.huge,-math.huge
        self.soundEvidence,self.soundEvidenceCount = {},0
    end
    count = math.max(0,math.min(3,count))
    now = finite(now) and now or 0
    local activeNames = {}
    local newest,soundSequence = self.lastSequence,0
    local newestHit,hitSequence = self.lastHitSequence,0
    for index=1,count do
        local token,group,member,x,y,z,pressure,threshold,opening,hitFlash,breakFlash,sequence,touch,objectIndex,refilled = self.api.bridge(16,index)
        local anchor
        if token and finite(x) and finite(y) and finite(z) and finite(pressure) and finite(threshold) and threshold>0 then
            anchor = self:anchor(objectIndex)
        end
        local broken = finite(opening) and opening>0
        local breakActive = finite(breakFlash) and breakFlash>0
        if finite(touch) then
            newestHit = math.max(newestHit,touch)
            if anchor and not broken and (hitFlash or 0)>0 and touch>self.lastHitSequence then hitSequence=math.max(hitSequence,touch) end
        end
        if finite(sequence) then
            newest = math.max(newest,sequence)
            if anchor and breakActive and sequence>self.lastSequence and sequence>soundSequence then
                soundSequence = sequence
            end
        end
        local showBar = anchor and self.bar and (pressure>0 or broken or breakActive or refilled==1)
        local showIndicator = anchor and self.indicator and breakActive
        if showBar or showIndicator then
            local panel = self:panel(index,anchor.parent,hud)
            if not self.displayReady then self.displayReady=true;self.api.log("Guard feedback active; fortitude shields attached to native floating HP gauges.") end
            self.api.visibility(panel.root,panel,4)
            panel.slot:SetAnchors(anchor.anchors)
            if panel.x~=anchor.x or panel.y~=anchor.y then panel.slot:SetPosition({X=anchor.x,Y=anchor.y});panel.x,panel.y=anchor.x,anchor.y end
            self:shiftName(anchor.gauge.text_enemy_name,activeNames)
            self:shiftName(anchor.gauge.text_soldier_name,activeNames)
            local fraction = broken and 1 or math.max(0,math.min(1,pressure/threshold))
            self:render(panel,fraction,threshold,broken,hitFlash,breakFlash,showBar,showIndicator,now)
        elseif self.pool[index] then self.api.visibility(self.pool[index].root,self.pool[index],1) end
    end
    self:restoreNames(activeNames)
    for index=count+1,3 do if self.pool[index] then self.api.visibility(self.pool[index].root,self.pool[index],1) end end
    if self.sound and soundSequence>0 and self.breakVolume<=0 then self:logSound(false,"unread",0,"cap-muted") end
    if self.sound and hitSequence>0 and self.hitVolume<=0 then self:logSound(true,"unread",0,"cap-muted") end
    if self.sound and self.breakVolume>0 and soundSequence>0 and now-self.lastBreakAt>=0.15 then
        self:playSound(false);self.lastSoundAt,self.lastBreakAt=now,now
    elseif self.sound and self.hitVolume>0 and hitSequence>0 and now-self.lastSoundAt>=0.10 and now-self.lastBreakAt>=0.40 then
        self:playSound(true);self.lastSoundAt=now
    end
    self.lastSequence,self.lastHitSequence = newest,newestHit
end

return GuardFeedback
