-- F6 switches the native base-weapon picker; selection and confirmation stay
-- in the game's fusion screen. No battle controls or save writes run here.
return function(bridge)
    local function valid(object) return object and object:IsValid() end
    local manager, pending, lastError, previousGuards, pendingToggle
    local function current()
        if not valid(manager) then manager = FindFirstOf("BriefingEquipManager") end
        if not valid(manager) then return end
        local selector = manager.WeaponCustomSelectWidget
        -- This picker can be hosted by another widget, so IsInViewport() is
        -- false even while it is on screen. Native menu state gates the action.
        if not valid(selector) or not selector:IsVisible() then return end
        return selector
    end
    local function update(toggle)
        if bridge(0) ~= 1 then return end
        local selector = current()
        if not selector then
            if toggle then print("[MoraleMatters] Weapon fusion switch skipped: no visible equipment picker.\n") end
            return
        end
        local mode = bridge(14, manager:GetAddress(), toggle and 1 or 0) or 0
        if toggle then print("[MoraleMatters] Weapon fusion inventory mode=" .. mode .. "\n") end
        if mode == 0 then return end
        local characters = selector.BP_parts_abandonment_chara_list
        if not valid(characters) then return end
        local guards = mode == 2 or mode == 4
        local label = characters.text_equipable
        if valid(label) then
            local hint = guards and "Bodyguard weapons | F6: Officers" or "Officer weapons | F6: Bodyguards"
            if mode > 2 then hint = guards and "Bodyguard weapons | Back, then F6: Officers" or "Officer weapons | Back, then F6: Bodyguards" end
            label:SetText(FText(hint))
        end
        if valid(characters.chara_List) then characters.chara_List:SetVisibility(guards and 1 or 0) end
        local portrait = selector.chara_full_xxx
        if valid(portrait) and (guards or previousGuards) then portrait:SetVisibility(guards and 1 or 0) end
        previousGuards = guards
    end
    local function schedule(toggle)
        if toggle then
            print("[MoraleMatters] Weapon fusion F6 pressed.\n")
            pendingToggle = not pendingToggle
        end
        local now = bridge(10) or 0
        if pending then
            if now - pending.queuedAt < 2000 then return end
            print("[MoraleMatters] Weapon fusion callback timed out; retrying.\n")
        end
        -- A discarded UE4SS callback must not leave input blocked forever.
        -- Superseded callbacks can still arrive, so only the current one acts.
        local request = {queuedAt=now}
        pending = request
        local scheduled, errorMessage = pcall(ExecuteInGameThread, function()
            if pending ~= request then return end
            local requested = pendingToggle
            pendingToggle = false
            local ok, message = pcall(update, requested)
            if not ok and lastError ~= tostring(message) then
                lastError = tostring(message)
                print("[MoraleMatters] Weapon fusion menu: " .. lastError .. "\n")
            end
            pending = nil
        end)
        if not scheduled then
            if pending == request then pending = nil end
            if lastError ~= tostring(errorMessage) then
                lastError = tostring(errorMessage)
                print("[MoraleMatters] Weapon fusion scheduling: " .. lastError .. "\n")
            end
        end
    end
    if Key.F6 then RegisterKeyBind(Key.F6, function() schedule(true) end) end
    LoopAsync(250, function() schedule(false); return false end)
end
