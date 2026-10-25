-- Morale Matters: native battle simulation + reflected game HUD/attributes.
local script = debug.getinfo(1, "S").source:gsub("^@", "")
local modDir = script:gsub("[/\\]Scripts[/\\]main%.lua$", "")
local binDir = modDir:gsub("[/\\]ue4ss[/\\]Mods[/\\]MoraleMatters$", "")
local settings = {}
do
    local file = io.open(binDir .. "\\MoraleMatters.ini", "r")
    local section = ""
    if file then
        for line in file:lines() do
            local name = line:match("^%s*%[([^%]]+)%]")
            if name then section = name:lower() end
            if not line:match("^%s*[;#]") then
                local key, value = line:match("^%s*([^=]-)%s*=%s*(.-)%s*$")
                if key then settings[section .. "." .. key:lower()] = value end
            end
        end
        file:close()
    end
end
local function number(key, fallback, minimum, maximum)
    local value = tonumber(settings[key]) or fallback
    if value ~= value or value == math.huge or value == -math.huge then value = fallback end
    return math.max(minimum, math.min(maximum, value))
end
local function textSetting(key, fallback) return settings[key] or fallback end
local function valid(object) return object and object:IsValid() end
local function log(message) print("[MoraleMatters] " .. message .. "\n") end
local bridge, bridgeError = package.loadlib(binDir .. "\\MoraleMatters.asi", "MoraleMatters_Query")
if not bridge then log("Native core could not load: " .. tostring(bridgeError)); return end
if number("general.enabled", 1, 0, 1) == 0 then log("Disabled in INI."); return end
local Experience = dofile(modDir .. "\\Scripts\\Experience.lua")
local experienceConfig = {
    distress=number("battlefield.distresssignals",1,0,1)==1,
    rescue=number("rescue.enabled",1,0,1)==1,
    trendSeconds=number("battlefield.trendseconds",10,3,30),
    reasonSeconds=number("battlefield.reasonseconds",6,2,15),
    distressMorale=number("battlefield.distressmoralepercent",35,5,60),
    distressHealth=number("battlefield.distresshealthpercent",40,5,80)/100,
    dropPercent=number("battlefield.distressdroppercent",3,0.5,20),
    casualties=number("battlefield.distresscasualties",3,1,30),
    alertSeconds=number("battlefield.alertseconds",6,2,15),
    alertCooldown=number("battlefield.alertcooldownseconds",20,10,180),
    corpsCooldown=number("battlefield.corpsalertcooldownseconds",90,30,300),
    actionRadius=number("rescue.actionradiusmetres",30,10,60)*100,
    rescueSeconds=number("rescue.objectiveseconds",120,30,300),
    recoverMorale=number("rescue.recovermoralepercent",40,35,70),
    recoverHealth=number("rescue.recoverhealthpercent",50,40,90)/100,
    enemyCasualties=number("rescue.enemycasualties",5,1,30),
    stableSeconds=number("rescue.stableseconds",5,2,20)
}
local experience = Experience.new(experienceConfig)
local experienceGeneration, reportShown, reportFinal, reportSaved = nil, false, false, false
local reportWidget, noticeWidget, resultWidget, resultParent, reportSnapshot
local showDetails = number("battlefield.moraledetails",1,0,1)==1

local width = number("hud.panelwidth", 260, 220, 900)
local portraitSize = number("hud.portraitsize", 20, 16, 64)
local commanderSize = number("hud.commanderportraitsize", 24, 16, 64)
local headerHeight = math.max(24, commanderSize)
local barTop = headerHeight + 4
local portraitTop = barTop + 12 + (showDetails and 36 or 0)
local titleSize = number("hud.titlefontsize", 16, 12, 18)
local left = number("hud.left", 36, 0, 3840)
local top = number("hud.top", 170, 0, 2160)
local updateMs = number("hud.updatemilliseconds", 500, 100, 2000)
local profile = number("general.profile", 0, 0, 1) == 1
local samples, totalTime, peakTime, totalClock, peakClock = 0, 0, 0, 0, 0
local names = {}
local white = {R=0.94, G=0.94, B=0.94, A=1}
local defeatedPortrait = {R=0.25, G=0.25, B=0.25, A=1}
local background = {R=0.018, G=0.024, B=0.035, A=0.88}
local allyFill = {R=number("hud.allymoralered", 0.1, 0, 1), G=number("hud.allymoralegreen", 0.72, 0, 1), B=number("hud.allymoraleblue", 0.3, 0, 1), A=1}
local temporaryFill = {R=number("hud.temporaryred", 1, 0, 1), G=number("hud.temporarygreen", 0.78, 0, 1), B=number("hud.temporaryblue", 0.08, 0, 1), A=1}
-- Blue/Red SpecifiedColor values from the game's Font_RichTextDataTable.
local enemyFill = {R=0.973446, G=0.111932, B=0.111932, A=1}
local allyColor = {R=0.198069, G=0.485150, B=1, A=1}
local ui, characterManager, hudManager, boundHUD, styleSet, fontTemplate
local readyHUD
local panels, heldAssets, portraits = {}, {}, {}
local expanded, page, running, nativeWarned = false, 0, false, false
local classified = false
local errors = {}

local function guard(name, action)
    local ok, errorMessage = pcall(action)
    if not ok and errors[name] ~= tostring(errorMessage) then
        errors[name] = tostring(errorMessage)
        log(name .. ": " .. tostring(errorMessage))
    end
    return ok
end

local function hold(asset)
    if not valid(asset) then return nil end
    local address = asset:GetAddress()
    if heldAssets[address] then return asset end
    local instance = FindFirstOf("GameInstance")
    if valid(instance) then
        instance.ReferencedObjects[#instance.ReferencedObjects + 1] = asset
        heldAssets[address] = asset
    end
    return asset
end

local function classify()
    if classified then return end
    local asset = LoadAsset("/Game/DataAsset/DT_UnitTypeData.DT_UnitTypeData")
    local library = StaticFindObject("/Script/Engine.Default__DataTableFunctionLibrary")
    if not valid(asset) or not valid(library) then return end
    local names = {}
    library:GetDataTableRowNames(asset, names)
    local attack = library:GetDataTableColumnAsString(asset, FName("AttackPowerUp"))
    local defense = library:GetDataTableColumnAsString(asset, FName("DefensePowerUp"))
    for index, name in ipairs(names) do
        local unit = tonumber(name:get():ToString():match("UnitTypeID_(%d+)$"))
        if unit then bridge(6, unit, (attack[index]:get():ToString() == "True" or defense[index]:get():ToString() == "True") and 1 or 0) end
    end
    classified = true
    log("Elite unit classification loaded from the game's UnitTypeData.")
end

local function portrait(unit)
    if portraits[unit] then return portraits[unit] end
    local path = ui:GetFaceStillPath(ui:GetFaceID(unit, 0)):ToString()
    local asset = path ~= "" and LoadAsset(path) or nil
    if not valid(asset) then asset = LoadAsset("/Game/UI/02_battle/icon_map_soldier_leader.icon_map_soldier_leader") end
    if valid(asset) then portraits[unit] = hold(asset); return asset end
end

local function construct(className, outer)
    local class = StaticFindObject("/Script/UMG." .. className)
    assert(valid(class), "Missing UMG class " .. className)
    local widget = StaticConstructObject(class, outer)
    assert(valid(widget), "Could not construct " .. className)
    widget:SetVisibility(4) -- HitTestInvisible: panels never intercept controls.
    return widget
end

local function place(parent, widget, x, y, w, h)
    local slot = parent:AddChildToCanvas(widget)
    slot:SetPosition({X=x, Y=y})
    slot:SetSize({X=w, Y=h})
    return slot
end

local function makeText(parent, x, y, w, h, size, rich)
    local widget = construct(rich and "RichTextBlock" or "TextBlock", parent)
    place(parent, widget, x, y, w, h)
    if rich and valid(styleSet) then widget:SetTextStyleSet(styleSet) end
    if not rich then
        widget:SetColorAndOpacity({SpecifiedColor=white, ColorUseRule=0})
        if fontTemplate then widget:SetFont(fontTemplate) end
        local font = widget.Font
        font.Size = size
        widget:SetFont(font)
    elseif fontTemplate then
        widget:SetDefaultFont(fontTemplate)
    end
    return widget
end

local function makeImage(parent, x, y, w, h, color)
    local widget = construct("Image", parent)
    local slot = place(parent, widget, x, y, w, h)
    widget:SetColorAndOpacity(color)
    return widget, slot
end

local function makeTrend(parent, x, y)
    local root = construct("CanvasPanel", parent)
    place(parent, root, x, y, 14, 14)
    local arrow = construct("CanvasPanel", root)
    place(root, arrow, 0, 0, 14, 14)
    -- Solid HUD shapes keep the indicator independent of the game's font glyphs.
    local left = makeImage(arrow, 0.5, 3, 8, 3, allyFill)
    local right = makeImage(arrow, 5.5, 3, 8, 3, allyFill)
    local stem = makeImage(arrow, 5.5, 4, 3, 9, allyFill)
    left:SetRenderTransformAngle(-45)
    right:SetRenderTransformAngle(45)
    local steady = makeImage(root, 1, 5.5, 12, 3, {R=0.55, G=0.57, B=0.60, A=1})
    return {root=root, arrow=arrow, parts={left,right,stem}, steady=steady}
end

local function newPanel(parent, index)
    local root = construct("CanvasPanel", parent)
    local height = portraitTop + portraitSize + 6
    local slot = place(parent, root, left, top, width, height)
    local _, backdropSlot = makeImage(root, 0, 0, width, height, background)
    local stripe, stripeSlot = makeImage(root, 0, 0, 2, height, allyColor)
    local commander = makeImage(root, 8, 2 + (headerHeight-commanderSize)/2, commanderSize, commanderSize, white)
    local trend = makeTrend(root, 12 + commanderSize, 2 + (headerHeight-14)/2)
    local title = makeText(root, 30 + commanderSize, 2 + (headerHeight-24)/2, width - 92 - commanderSize, 24, titleSize)
    title:SetClipping(1) -- ClipToBounds keeps long names inside the reserved title area.
    title:SetTextOverflowPolicy(1) -- Ellipsis when the portrait leaves less room for a name.
    local percentage = makeText(root, width - 55, 2 + (headerHeight-24)/2, 49, 24, 14)
    -- The full-width bar has its own row, leaving room for readable names and percentages.
    makeImage(root, 8, barTop, width - 16, 5, {R=0.11, G=0.12, B=0.14, A=1})
    local permanent, permanentSlot = makeImage(root, 8, barTop, width - 16, 5, allyFill)
    local temporary, temporarySlot = makeImage(root, 8, barTop, 0, 5, temporaryFill)
    local portraitCanvas = construct("CanvasPanel", root)
    place(root, portraitCanvas, 8, portraitTop, width - 16, 200)
    local detail, status
    if showDetails then
        detail = makeText(root,8,barTop+8,width-16,18,12)
        status = makeText(root,8,barTop+25,width-16,18,11)
        detail:SetClipping(1); detail:SetTextOverflowPolicy(1)
        status:SetClipping(1); status:SetTextOverflowPolicy(1)
    end
    return {root=root, slot=slot, backdropSlot=backdropSlot, stripe=stripe, stripeSlot=stripeSlot, commander=commander, trendIcon=trend, title=title, permanent=permanent, permanentSlot=permanentSlot, temporarySlot=temporarySlot, percentage=percentage, portraitCanvas=portraitCanvas, detail=detail, status=status, icons={}}
end

-- Exact supported UnitTypeData rows: all nine Vice General variants have display-name
-- ID 256 and share existing native portrait assets with other units (audited in-game).
local function isViceGeneral(unit) return unit >= 405 and unit <= 413 end

local function visibility(object, state, value)
    if state.visible ~= value then object:SetVisibility(value); state.visible = value end
end

local function findStyle()
    local gauge = boundHUD.BP_parts_gauge_enemy
    if valid(gauge) and valid(gauge.text_enemy_name) then fontTemplate = gauge.text_enemy_name.Font end
end

local function bindHUD()
    if not valid(hudManager) then hudManager = FindFirstOf("BattleHUDManager") end
    if not valid(hudManager) or not valid(hudManager.Battle2DHUDWidget) then return false end
    local hud = hudManager.Battle2DHUDWidget

    if not valid(boundHUD) or boundHUD:GetAddress() ~= hud:GetAddress() then
        boundHUD, readyHUD = hud, nil
        panels, page = {}, 0
        noticeWidget,reportWidget = nil,nil
        local manager = FindFirstOf("AlgoManager")
        if not valid(manager) then boundHUD = nil; return false end
        bridge(1, manager:GetAddress())
    end
    if readyHUD then return true end
    guard("Create HUD", function()
        findStyle()
        if number("hud.enabled", 1, 0, 1) == 0 then readyHUD = true; return end
        assert(valid(hud.canvas_hud), "Battle HUD has no canvas_hud")
        for index = #panels + 1, 2 do panels[index] = newPanel(hud.canvas_hud, index); visibility(panels[index].root, panels[index], 1) end
        readyHUD = true
        log("Battle HUD bound; portrait panels and native battle state ready.")
    end)
    return true
end

local function popup(character, rallied, healed)
    if not valid(hudManager) or not valid(character) then return end
    hudManager.ItemHeadWidgetNPC:ForEach(function(_, element)
        local widget = element:get()
        if valid(widget) then widget.duration = number("support.popupseconds", 2.5, 0.5, 15) end
    end)
    if rallied then hudManager:SetHeadItemNPC(character, textSetting("support.moraletext", "Morale boosted"), "") end
    if healed > 0 then
        local show = function()
            if valid(hudManager) and valid(character) then hudManager:SetHeadItemNPC(character, textSetting("support.healtext", "Health restored"), "+" .. math.floor(healed + 0.5)) end
        end
        if rallied then ExecuteWithDelay(600, function() ExecuteInGameThread(show) end) else show() end
    end
end

local function scanCharacters()
    local players, officers, support, liveHealth = {}, {}, {}, {}
    -- One native pass replaces thousands of reflected Lua property/function calls.
    -- Only players and allied named officers cross back into Lua for support/popups.
    local count = bridge(8, characterManager:GetAddress()) or 0
    local characters = characterManager.m_arpCharacterList
    for index = 1, count do
        local character = characters[bridge(9, index)]
        if valid(character) then
            local id, member, corps, side, outgoing, incoming, frequency, named = bridge(5, character:GetAddress())
            if character:IsPlayer() then
                local position, rotation = character:K2_GetActorLocation(),character:K2_GetActorRotation()
                players[#players + 1] = {X=position.X,Y=position.Y,Z=position.Z,Yaw=rotation.Yaw}
            elseif id and side == 0 and named == 1 then officers[#officers + 1] = {character=character, id=id,corps=corps} end
        end
    end
    for _, entry in ipairs(officers) do
        local character = entry.character
        local position = character:K2_GetActorLocation()
        local distanceSquared = math.huge
        for _, player in ipairs(players) do
            local dx,dy,dz = position.X-player.X, position.Y-player.Y, position.Z-player.Z
            distanceSquared = math.min(distanceSquared, dx*dx+dy*dy+dz*dz)
        end
        local hp, maximum = character:GetHealth(),character:GetMaxHealth()
        local rallied, healed = bridge(4, entry.id, math.sqrt(distanceSquared), hp, maximum, character:GetAddress())
        liveHealth[entry.id] = maximum > 0 and math.max(0,math.min(1,(hp+(healed or 0))/maximum)) or 1
        if rallied == 1 or healed and healed > 0 then popup(character, rallied == 1, healed or 0) end
        if rallied == 1 or healed and healed > 0 then support[#support+1] = {id=entry.id,corps=entry.corps,rallied=rallied==1,healed=healed or 0} end
    end
    return players,support,liveHealth
end

local function formations(players, liveHealth)
    local count = bridge(7)
    local result, groups = {}, {}
    for id = 0, (count or 0) - 1 do
        local corps, unit, rank, side, alive, hp, maximum, x,y,z, soldiers, casualties, breaks, phase = bridge(2, id)
        if corps and corps >= 0 and side >= 0 and side < 2 then
            groups[id] = {id=id,corps=corps,unit=unit,rank=rank,side=side,alive=alive==1,health=liveHealth and liveHealth[id] or (maximum and maximum>0 and hp/maximum or 1),x=x,y=y,z=z,casualties=casualties or 0,breaks=breaks or 0}
        end
        if corps and corps >= 0 and alive == 1 and side >= 0 and side < 2 then
            local row = result[corps]
            if not row then
                local leader, corpsSide, base, effective, bonus, remaining, reason, amount, reasonAge, _, proximity = bridge(3, corps)
                row = {id=corps, leader=leader, side=corpsSide, base=base, morale=effective, bonus=bonus, remaining=remaining or 0,proximity=proximity or 0,reason=reason or 0,reasonAge=reasonAge or math.huge,members={}, distance=math.huge}
                result[corps] = row
            end
            local vice = isViceGeneral(unit)
            if id == row.leader then row.unit, row.leaderAlive = unit, true
            else row.members[#row.members + 1] = {id=id,unit=unit,rank=rank,vice=vice,health=groups[id].health} end
            if rank >= 2 and rank <= 5 then
                if not row.wounded or groups[id].health < row.wounded.health then row.wounded = groups[id] end
                for _, player in ipairs(players) do
                    local dx,dy = x-player.X, y-player.Y
                    row.distance = math.min(row.distance, dx*dx+dy*dy)
                end
            end
            if phase and phase > 0 then row.phase = math.min(row.phase or phase,phase) end
        end
    end
    local list = {}
    for _, row in pairs(result) do
        if not row.unit then
            local _, unit, _, _, alive = bridge(2, row.leader)
            row.unit, row.leaderAlive = unit, alive == 1
        end
        table.sort(row.members, function(a,b) if a.id==b.id then return false elseif a.id==row.leader then return true elseif b.id==row.leader then return false elseif a.rank~=b.rank then return a.rank<b.rank else return a.id<b.id end end)
        if row.unit then list[#list+1] = row end
    end
    table.sort(list, function(a,b) if a.side~=b.side then return a.side<b.side else return a.distance<b.distance end end)
    return result, list, groups
end

local function render(panel, row, x, y)
    if not row then visibility(panel.root, panel, 1); return 0 end
    if panel.unit ~= row.unit or panel.side ~= row.side then
        if panel.side ~= row.side then panel.members=nil end
        local color = row.side == 0 and allyColor or enemyFill
        panel.stripe:SetColorAndOpacity(color)
        if not names[row.unit] then names[row.unit] = ui:GetUnitName(row.unit, false):ToString() end
        panel.title:SetText(FText(names[row.unit]))
        local texture = portrait(row.unit)
        if valid(texture) then panel.commander:SetBrushFromTexture(texture, false) end
        panel.title:SetColorAndOpacity({SpecifiedColor=color, ColorUseRule=0})
        panel.permanent:SetColorAndOpacity(row.side == 0 and allyFill or enemyFill)
        panel.unit, panel.side = row.unit, row.side
    end
    if panel.leaderAlive ~= row.leaderAlive then
        panel.commander:SetColorAndOpacity(row.leaderAlive and white or defeatedPortrait)
        panel.leaderAlive = row.leaderAlive
    end
    local total = math.max(0, math.min(1, row.morale / 899))
    local permanent = math.max(0, math.min(total, (row.morale - row.bonus) / 899))
    local barWidth = width - 16
    -- Quantize fill geometry to pixels; unchanged text/geometry does not invalidate Slate.
    local basePixels, totalPixels = math.floor(barWidth*permanent+0.5), math.floor(barWidth*total+0.5)
    if panel.basePixels ~= basePixels or panel.totalPixels ~= totalPixels then
        panel.permanentSlot:SetSize({X=basePixels,Y=5})
        panel.temporarySlot:SetPosition({X=8+basePixels,Y=barTop})
        panel.temporarySlot:SetSize({X=totalPixels-basePixels,Y=5})
        panel.basePixels, panel.totalPixels = basePixels, totalPixels
    end
    local percent = math.floor(total*100+0.5)
    if panel.percent ~= percent then panel.percentage:SetText(FText(percent .. "%")); panel.percent = percent end
    local trend = row.trend or "="
    if panel.trend ~= trend then
        local moving = trend == "^" or trend == "v"
        panel.trendIcon.arrow:SetVisibility(moving and 4 or 1)
        panel.trendIcon.steady:SetVisibility(moving and 1 or 4)
        if moving then
            panel.trendIcon.arrow:SetRenderTransformAngle(trend == "v" and 180 or 0)
            local color = trend == "^" and allyFill or enemyFill
            for _,part in ipairs(panel.trendIcon.parts) do part:SetColorAndOpacity(color) end
        end
        panel.trend = trend
    end
    if panel.detail then
        local detail = row.reasonText or "Morale"
        local status = row.remaining > 0 and string.format("Support %d:%02d",math.floor(row.remaining/60),math.floor(row.remaining%60)) or ""
        if (row.proximity or 0) > 0 then status = status .. (status ~= "" and " | " or "") .. "Enemy cohesion" end
        if row.phaseText then status = status .. (status ~= "" and " | " or "") .. row.phaseText end
        if panel.detailText ~= detail then panel.detail:SetText(FText(detail)); panel.detailText=detail end
        if panel.statusText ~= status then panel.status:SetText(FText(status)); panel.statusText=status end
    end
    local signature = {}
    for _,member in ipairs(row.members) do signature[#signature+1] = (member.id or -1) .. ":" .. member.unit .. ":" .. (member.count or 0) .. (member.vice and ":VG" or "") end
    signature = table.concat(signature, ",")
    if panel.members ~= signature then
        local iconX, iconY = 0, 0
        for index, member in ipairs(row.members) do
            local labelWidth = member.vice and (36 + #tostring(member.count)*10) or 38
            local itemWidth = portraitSize + (member.count and labelWidth+4 or 4)
            if iconX > 0 and iconX + itemWidth > width-16 then iconX, iconY = 0, iconY+portraitSize+9 end
            local icon = panel.icons[index]
            if not icon then
                local image, slot = makeImage(panel.portraitCanvas, 0, 0, portraitSize, portraitSize, white)
                local countText = makeText(panel.portraitCanvas, 0, 0, 50, portraitSize+2, 12)
                local healthBack, healthBackSlot = makeImage(panel.portraitCanvas,0,0,portraitSize,3,{R=0.11,G=0.12,B=0.14,A=1})
                local health, healthSlot = makeImage(panel.portraitCanvas,0,0,portraitSize,3,allyFill)
                icon = {image=image,slot=slot,countText=countText,countSlot=countText.Slot,health=health,healthSlot=healthSlot,healthBack=healthBack,healthBackSlot=healthBackSlot}; panel.icons[index] = icon
            end
            if icon.unit ~= member.unit then
                local texture = portrait(member.unit)
                if valid(texture) then icon.image:SetBrushFromTexture(texture, false) end
                icon.unit = member.unit
            end
            icon.slot:SetPosition({X=iconX,Y=iconY})
            icon.healthBackSlot:SetPosition({X=iconX,Y=iconY+portraitSize+2})
            icon.healthSlot:SetPosition({X=iconX,Y=iconY+portraitSize+2})
            icon.healthBack:SetVisibility(4)
            icon.health:SetVisibility(4)
            icon.health:SetColorAndOpacity(row.side == 0 and allyFill or enemyFill)
            icon.healthPixels = nil
            icon.image:SetVisibility(4)
            icon.countText:SetVisibility(member.count and 4 or 1)
            if member.count then
                icon.countText:SetText(FText((member.vice and "VG x " or "x ") .. member.count))
                icon.countSlot:SetPosition({X=iconX+portraitSize+3,Y=iconY})
                icon.countSlot:SetSize({X=labelWidth,Y=portraitSize+2})
            end
            iconX = iconX + itemWidth
        end
        for index=#row.members+1,#panel.icons do panel.icons[index].image:SetVisibility(1); panel.icons[index].countText:SetVisibility(1); panel.icons[index].health:SetVisibility(1); panel.icons[index].healthBack:SetVisibility(1) end
        panel.contentHeight = #row.members > 0 and (portraitTop + iconY + portraitSize + 11) or portraitTop
        panel.members = signature
    end
    for index,member in ipairs(row.members) do
        local icon = panel.icons[index]
        local pixels = math.floor(portraitSize*math.max(0,math.min(1,member.health or 0))+0.5)
        if icon.healthPixels ~= pixels then icon.healthSlot:SetSize({X=pixels,Y=3}); icon.healthPixels=pixels end
    end
    local height = panel.contentHeight or (portraitTop + portraitSize + 6)
    if panel.height ~= height then
        panel.backdropSlot:SetSize({X=width,Y=height})
        panel.stripeSlot:SetSize({X=2,Y=height})
        panel.slot:SetSize({X=width,Y=height})
        panel.height = height
    end
    if panel.x ~= x or panel.y ~= y then panel.slot:SetPosition({X=x,Y=y}); panel.x, panel.y = x,y end
    visibility(panel.root, panel, 4)
    return height
end

local function unitName(unit)
    if not names[unit] then names[unit] = ui:GetUnitName(unit,false):ToString() end
    return names[unit]
end

local function newMessage(parent, w, h)
    local root = construct("CanvasPanel",parent)
    local slot = place(parent,root,left,top,w,h)
    makeImage(root,0,0,w,h,background)
    return {root=root,slot=slot}
end

local function updateNotice(groups, players)
    if not readyHUD or #panels == 0 then return end
    local notice = experience:notice(groups,players)
    if not notice then if noticeWidget then visibility(noticeWidget.root,noticeWidget,1) end; return end
    if not noticeWidget then
        noticeWidget = newMessage(boundHUD.canvas_hud,400,130)
        noticeWidget.portrait = makeImage(noticeWidget.root,8,8,32,32,white)
        noticeWidget.title = makeText(noticeWidget.root,48,4,344,30,14)
        noticeWidget.name = makeText(noticeWidget.root,48,34,344,30,14)
        noticeWidget.detail = makeText(noticeWidget.root,8,64,384,30,14)
        noticeWidget.hint = makeText(noticeWidget.root,8,96,384,26,11)
        for _, key in ipairs({"title","name","detail","hint"}) do noticeWidget[key]:SetClipping(1); noticeWidget[key]:SetTextOverflowPolicy(1) end
    end
    local widget = noticeWidget
    local values = {title=notice.title,name=unitName(notice.unit),detail=notice.detail,hint=notice.hint or ""}
    for key,value in pairs(values) do
        if widget[key.."Text"] ~= value then widget[key]:SetText(FText(value)); widget[key.."Text"]=value end
    end
    if widget.unit ~= notice.unit then
        local texture = portrait(notice.unit)
        if valid(texture) then widget.portrait:SetBrushFromTexture(texture,false) end
        widget.unit=notice.unit
    end
    local x = left+(expanded and 2 or 1)*(width+10)
    if widget.x ~= x then widget.slot:SetPosition({X=x,Y=top}); widget.x=x end
    visibility(widget.root,widget,4)
end

local function renderReport(widget, parent, onResults)
    if not widget or not valid(widget.root) then
        widget = newMessage(parent,540,240)
        widget.text = makeText(widget.root,12,8,516,224,14)
        if onResults then
            -- Keep the results menu clear and follow the canvas's right edge.
            widget.slot:SetAnchors({Minimum={X=1,Y=0},Maximum={X=1,Y=0}})
            widget.slot:SetAlignment({X=1,Y=0})
            widget.slot:SetPosition({X=-36,Y=170})
        end
    end
    local message = reportSnapshot or experience:report(unitName)
    if widget.message ~= message then widget.text:SetText(FText(message)); widget.message=message end
    visibility(widget.root,widget,reportShown and 4 or 1)
    return widget
end

local function saveReport(final)
    local file, message = io.open(binDir .. "\\MoraleMatters-last-battle.txt","w")
    if not file then error("Could not save battle report: "..tostring(message)) end
    local ok, errorMessage = file:write(os.date("%Y-%m-%d %H:%M:%S") .. (final and " | Results observed\n" or " | Battle in progress\n") .. (reportSnapshot or experience:report(unitName)) .. "\n")
    file:close()
    assert(ok,errorMessage)
end

local function resultCanvas(widget)
    if not valid(widget) or not widget:IsInViewport() then return nil end
    local canvasClass = StaticFindObject("/Script/UMG.CanvasPanel")
    local root = valid(widget.WidgetTree) and widget.WidgetTree.RootWidget or nil
    if valid(root) and root:IsA(canvasClass) then return root end
    local title = widget.BP_parts_battle_setup_title
    local parent = valid(title) and title:GetParent() or nil
    if valid(parent) and parent:IsA(canvasClass) then return parent end
end

local function updateResults()
    if not experience.started then return false end
    local result = FindFirstOf("ResultHUDManager")
    if not valid(result) then return false end
    local common, contribution, gameOver = result.CommonWidget,result.ContributionWidget,result.GameOverWidget
    local visible = valid(common) and common:IsInViewport() or valid(contribution) and contribution:IsInViewport() or valid(gameOver) and gameOver:IsInViewport()
    if not visible then return false end
    if not reportFinal then
        experience.finalized,reportFinal = true,true
        reportSnapshot = experience:report(unitName)
        reportShown = number("report.showonresults",1,0,1)==1
        log(reportSnapshot)
    end
    if number("report.savetofile",1,0,1)==1 and not reportSaved then reportSaved=guard("Save report",function() saveReport(true) end) end
    if number("report.enabled",1,0,1)==1 then
        local parent = resultCanvas(common) or resultCanvas(contribution) or resultCanvas(gameOver)
        if valid(parent) then
            if resultParent ~= parent:GetAddress() then
                if resultWidget and valid(resultWidget.root) then resultWidget.root:RemoveFromParent() end
                resultWidget,resultParent = nil,parent:GetAddress()
            end
            resultWidget=renderReport(resultWidget,parent,true)
        end
    end
    return true
end

local function updateHUD(players, byId, list)
    if not readyHUD or #panels == 0 then return end
    if expanded then
        for index=#panels+1,6 do panels[index] = newPanel(boundHUD.canvas_hud, index); visibility(panels[index].root, panels[index], 1) end
    end
    local shown = {}
    if expanded then
        local pages = math.max(1, math.ceil(#list/6))
        page = page % pages
        for index=1,6 do shown[index] = list[page*6+index] end
    else
        for _, row in ipairs(list) do if row.side==0 then shown[1]=row; break end end
        local target = hudManager:GetEnemyGaugeUnit(characterManager)
        if valid(target) then
            local _,_,corps = bridge(5,target:GetAddress())
            shown[2] = byId[corps]
        end
        if not shown[2] then for _,row in ipairs(list) do if row.side==1 and row.distance < 3000*3000 then shown[2]=row; break end end end
    end
    local ys = {top,top}
    for index,panel in ipairs(panels) do
        local column = expanded and ((index-1)%2)+1 or 1
        local h = render(panel, shown[index], left+(column-1)*(width+10), ys[column])
        ys[column] = ys[column] + h + 6
    end
    if number("report.enabled",1,0,1)==1 then
        if reportShown or reportWidget then
            reportWidget=renderReport(reportWidget,boundHUD.canvas_hud)
            local y = math.max(ys[1],ys[2])+6
            if reportWidget.y ~= y then reportWidget.slot:SetPosition({X=left,Y=y}); reportWidget.y=y end
        end
    end
end

local toggleKey = textSetting("hud.allcorpskey", "F7")
if Key[toggleKey] then RegisterKeyBind(Key[toggleKey], function() expanded = not expanded; page = 0 end) end
RegisterKeyBind(Key.PAGE_DOWN, function() if expanded then page = page+1 end end)
RegisterKeyBind(Key.PAGE_UP, function() if expanded then page = math.max(0,page-1) end end)
local reportKey = textSetting("report.key","F8")
if Key[reportKey] then RegisterKeyBind(Key[reportKey],function() reportShown=not reportShown end) end

LoopAsync(updateMs, function()
    if running then return false end
    running = true
    ExecuteInGameThread(function()
        local ok = guard("Update", function()
            local status = bridge(0)
            if status ~= 1 then
                if status and status ~= 0 and not nativeWarned then log("Native core inactive (status "..tostring(status).."); gameplay and HUD updates skipped."); nativeWarned=true end
                return
            end
            ui = ui or StaticFindObject("/Script/Refine.Default__UIFunction")
            if not valid(ui) then return end
            if updateResults() then return end
            if not valid(characterManager) then characterManager = FindFirstOf("CharacterManager") end
            if not valid(ui) or not valid(characterManager) then return end
            guard("Classification", classify)
            if not bindHUD() then return end
            local now,generation = bridge(12)
            if generation ~= experienceGeneration then
                experienceGeneration=generation
                experience=Experience.new(experienceConfig)
                reportShown,reportFinal,reportSaved = false,false,false
                reportSnapshot,resultWidget,resultParent=nil,nil,nil
            end
            local started = profile and bridge(10) or 0
            local clockStarted = profile and os.clock() or 0
            local players,support,liveHealth = scanCharacters()
            local byId,list,groups = formations(players,liveHealth)
            local completions = experience:update(now or 0,byId,groups,players,support)
            for _,entry in ipairs(completions) do experience:complete(entry,bridge(13,entry.officer) or -1) end
            guard("HUD", function() updateHUD(players,byId,list); updateNotice(groups,players) end)
            if profile then
                local elapsed = bridge(10)-started
                samples, totalTime, peakTime = samples+1, totalTime+elapsed, math.max(peakTime,elapsed)
                local clockMs = (os.clock()-clockStarted)*1000
                totalClock, peakClock = totalClock+clockMs, math.max(peakClock,clockMs)
                if samples%20 == 0 then log(string.format("PROFILE updates=%d mean=%.3fms peak=%.3fms clockMean=%.3fms clockPeak=%.3fms",samples,totalTime/samples,peakTime,totalClock/samples,peakClock)) end
            end
        end)
        running = false
    end)
    return false
end)
log("Loaded. F7 toggles all-corps panels; Page Up/Down changes pages; F8 toggles the battle report.")
