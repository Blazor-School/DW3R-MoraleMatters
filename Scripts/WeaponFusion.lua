local function valid(widget) return widget and widget:IsValid() end

return function(bridge)
    local library = StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")
    local entries = {}

    local function attach()
        if bridge(0) ~= 1 or bridge(21) ~= 1 then return end
        for _, manager in ipairs(FindAllOf("BriefingEquipManager") or {}) do
            local common = manager.CommonWidget
            if valid(common) and common:IsInViewport() then
                local menu = common.BP_parts_briefing_list_menu_equipu
                if valid(menu) and menu:IsVisible() and menu.IsBattle == false then
                    local address = manager:GetAddress()
                    local box, last = menu.VerticalBox_0, menu.BP_parts_battle_setup_menu_5
                    local controller = common:GetOwningPlayer()
                    if not entries[address] and valid(box) and valid(last) and valid(controller) then
                        local row = library:Create(manager, last:GetClass(), controller)
                        row.text_list:SetText(FText("Weapon Fusion"))
                        box:AddChildToVerticalBox(row):SetPadding(last.Slot.Padding)
                        if bridge(21, address, row:GetAddress()) == 1 then
                            entries[address] = {manager=manager, menu=menu, row=row, last=last, first=menu.BP_parts_battle_setup_menu}
                        else
                            row:RemoveFromParent()
                        end
                    end
                end
            end
        end
    end

    local function setText(widget, value)
        if valid(widget) and widget:GetText():ToString() ~= value then widget:SetText(FText(value)) end
    end

    local function updateEntry(address, entry)
        local menu, row = entry.menu, entry.row
        local picker = entry.manager.WeaponCustomSelectWidget
        if menu:IsVisible() then
            -- The stock Equipment initializer only enables its first seven rows.
            row.bEnableDecide = true
            entry.first:SetNavigationRuleExplicit(2, row)
            row:SetNavigationRuleExplicit(3, entry.first)
            entry.last:SetNavigationRuleExplicit(3, row)
            row:SetNavigationRuleExplicit(2, entry.last)
        end
        local mode, stock = bridge(14, address)
        local visiblePicker = valid(picker) and picker:IsVisible()
        local bodyguards = mode == 2 or mode == 4
        if bodyguards and visiblePicker then
            entry.bodyguardList = true
            entry.hidden = entry.hidden or {}
            for _, field in ipairs({"BP_parts_abandonment_chara_list", "chara_full_xxx"}) do
                local widget = picker[field]
                if valid(widget) and widget.Visibility ~= 1 then
                    local id = widget:GetAddress()
                    entry.hidden[id] = entry.hidden[id] or {widget=widget, visibility=widget.Visibility}
                    widget:SetVisibility(1)
                end
            end
        elseif entry.hidden and (visiblePicker or menu:IsVisible()) then
            for _, record in pairs(entry.hidden) do
                if valid(record.widget) then record.widget:SetVisibility(record.visibility) end
            end
            entry.hidden = nil
        end
        if visiblePicker and mode and mode > 0 and entry.bodyguardList then
            -- The native picker caches the previous inventory's stock labels.
            local list = picker.BP_parts_briefing_list_weapon
            if valid(list) then
                setText(list.TextBlock_stocknow, tostring(stock))
                setText(list.TextBlock_stockall, bodyguards and "100" or "390")
            end
            if not bodyguards then entry.bodyguardList = nil end
        end
    end

    local function update()
        for address, entry in pairs(entries) do
            if valid(entry.manager) and valid(entry.menu) and valid(entry.row) then
                updateEntry(address, entry)
            else
                entries[address] = nil
            end
        end
    end

    return attach, update
end
