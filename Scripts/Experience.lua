-- Battle-time-only observation and objectives. No engine access or inferred morale causes.
local Experience = {}
local reasons = {[1]="Battle event", [2]="Casualties", [3]="Officer defeated", [4]="Nearby defeat panic", [5]="Recovering", [6]="Reinforcements arrived", [7]="Surprise force arrived", [8]="Allied support", [9]="Support expired", [10]="Rescue completed", [11]="Off-screen victories", [12]="Captain defeated", [13]="Enemy corps nearby", [14]="Enemy corps separated"}
local phases = {[1]="Falling back", [2]="Reforming", [3]="Rejoining"}

local function distance(a, b)
    local dx, dy = a.x-b.x, a.y-b.y
    return math.sqrt(dx*dx+dy*dy)
end

local function nearest(position, players)
    local best, player = math.huge, nil
    for _, candidate in ipairs(players) do
        local value = distance(position, {x=candidate.X,y=candidate.Y})
        if value < best then best, player = value, candidate end
    end
    return best, player
end

function Experience.direction(position, players)
    local value, player = nearest(position, players)
    if not player then return "Position unavailable" end
    local angle = math.deg(math.atan(position.y-player.Y, position.x-player.X)) - (player.Yaw or 0)
    angle = (angle+180)%360-180
    local directions = {"ahead", "ahead-right", "right", "behind-right", "behind", "behind-left", "left", "ahead-left"}
    return string.format("%dm %s", math.floor(value/100+0.5), directions[math.floor((angle%360+22.5)/45)%8+1])
end

function Experience.new(config)
    return setmetatable({config=config, corps={}, groups={}, clock=0, nextAlert=0, alert=nil, completion=nil, rescued=0, broken=0, rallies=0, healed=0, bestRecovery=0, bestUnit=nil, started=false, finalized=false}, {__index=Experience})
end

function Experience:update(now, rows, groups, players, support)
    if self.finalized or now < self.clock then return {} end
    self.clock = now
    if now > 0 then self.started = true end
    local deaths, losses = {}, {}
    for id, group in pairs(groups) do
        local previous = self.groups[id]
        if previous and previous.corps == group.corps and previous.side == group.side and previous.unit == group.unit then
            local count = math.max(0, group.casualties-previous.casualties)
            if count > 0 then
                losses[group.corps] = (losses[group.corps] or 0)+count
                deaths[#deaths+1] = {group=group,count=count,leader=previous.alive and not group.alive}
            end
            if group.side == 1 and group.breaks > previous.breaks and nearest(group, players) <= self.config.actionRadius then
                self.broken = self.broken+1
            end
        end
        self.groups[id] = {corps=group.corps,side=group.side,unit=group.unit,casualties=group.casualties,breaks=group.breaks,alive=group.alive}
    end
    local candidates, completions = {}, {}
    for id, state in pairs(self.corps) do if not rows[id] then state.rescue=nil end end
    for id, row in pairs(rows) do
        local state = self.corps[id]
        if not state or state.side ~= row.side or state.leader ~= row.leader then
            state = {side=row.side,leader=row.leader,history={},losses={},nextAlert=0,quietSince=now,minimum=row.base}
            self.corps[id] = state
        end
        if #state.history == 0 or now > state.history[#state.history].time then
            state.history[#state.history+1] = {time=now,morale=row.morale}
        end
        while #state.history > 1 and state.history[2].time <= now-self.config.trendSeconds do table.remove(state.history,1) end
        local change = row.morale-state.history[1].morale
        row.trend = change >= 4.495 and "^" or change <= -4.495 and "v" or "="
        row.reasonText = (row.reasonAge or math.huge) <= self.config.reasonSeconds and reasons[row.reason] or nil
        row.phaseText = phases[row.phase]
        if (losses[id] or 0) > 0 then
            state.losses[#state.losses+1] = {time=now,count=losses[id]}
            state.quietSince = now
        end
        local recentLoss = 0
        for index=#state.losses,1,-1 do
            if now-state.losses[index].time > self.config.trendSeconds then table.remove(state.losses,index)
            else recentLoss = recentLoss+state.losses[index].count end
        end
        if row.side == 0 then
            local officer = row.wounded
            local danger = #players > 0 and officer and row.morale/899*100 <= self.config.distressMorale and officer.health <= self.config.distressHealth and (change <= -self.config.dropPercent*8.99 or recentLoss >= self.config.casualties)
            if danger then
                candidates[#candidates+1] = {id=id,officer=officer,severity=officer.health+row.morale/899,state=state}
                if self.config.rescue and not state.completed and not state.rescue and now >= (state.retryAfter or 0) then
                    state.rescue = {officer=officer.id,expires=now+self.config.rescueSeconds,enemyLosses=0,leaderDefeated=false,stableSince=nil,helped=false}
                end
            end
            if state.helped then
                state.minimum = math.min(state.minimum,row.base)
                local recovery = row.base-state.minimum
                if recovery > self.bestRecovery then self.bestRecovery,self.bestUnit = recovery,row.unit end
            end
            local rescue = state.rescue
            if rescue then
                local target = groups[rescue.officer]
                if not target or not target.alive or target.corps ~= id or target.side ~= 0 or now > rescue.expires then
                    state.rescue,state.retryAfter = nil,now+self.config.corpsCooldown
                else
                    for _, encounter in ipairs(support) do
                        if encounter.corps == id and (encounter.rallied or encounter.healed > 0) then
                            rescue.helped,rescue.helpTime = true,now
                        end
                    end
                    if rescue.helped then
                        for _, death in ipairs(deaths) do
                            if death.group.side == 1 and distance(target,death.group) <= self.config.actionRadius and nearest(death.group,players) <= self.config.actionRadius then
                                rescue.enemyLosses = rescue.enemyLosses+death.count
                                rescue.leaderDefeated = rescue.leaderDefeated or death.leader
                            end
                        end
                    end
                    local stable = rescue.helped and (rescue.leaderDefeated or rescue.enemyLosses >= self.config.enemyCasualties) and row.morale/899*100 >= self.config.recoverMorale and target.health >= self.config.recoverHealth and now-state.quietSince >= self.config.stableSeconds
                    if stable then rescue.stableSince = rescue.stableSince or now else rescue.stableSince = nil end
                    if rescue.stableSince and now-rescue.stableSince >= self.config.stableSeconds and not rescue.pending then
                        rescue.pending = true
                        completions[#completions+1] = {corps=id,officer=rescue.officer,unit=target.unit}
                    end
                end
            end
        end
    end
    for _, encounter in ipairs(support) do
        if encounter.rallied then self.rallies = self.rallies+1 end
        self.healed = self.healed+encounter.healed
        local state = self.corps[encounter.corps]
        if state and rows[encounter.corps] then
            state.minimum = state.helped and math.min(state.minimum,rows[encounter.corps].base) or rows[encounter.corps].base
            state.helped=true
        end
    end
    table.sort(candidates,function(a,b) if a.severity == b.severity then return a.id < b.id end return a.severity < b.severity end)
    if self.config.distress and now >= self.nextAlert then
        for _, candidate in ipairs(candidates) do
            if now >= candidate.state.nextAlert then
                self.alert = {corps=candidate.id,officer=candidate.officer.id,unit=candidate.officer.unit,expires=now+self.config.alertSeconds}
                self.nextAlert,candidate.state.nextAlert = now+self.config.alertCooldown,now+self.config.corpsCooldown
                break
            end
        end
    end
    return completions
end

function Experience:complete(entry, reward)
    local state = self.corps[entry.corps]
    if not state or not state.rescue or not state.rescue.pending then return end
    if reward < 0 then state.rescue=nil; state.completed=true; return end
    state.rescue,state.completed = nil,true
    self.rescued = self.rescued+1
    self.completion = {unit=entry.unit,reward=reward,expires=self.clock+self.config.alertSeconds}
end

function Experience:notice(groups, players)
    if self.completion and self.clock < self.completion.expires then
        return {title="Ally rescued",unit=self.completion.unit,detail=self.completion.reward > 0 and string.format("Corps stabilized | +%.0f morale",self.completion.reward) or "Corps stabilized"}
    end
    if self.alert and self.clock < self.alert.expires then
        local target = groups[self.alert.officer]
        if target and target.alive and target.corps == self.alert.corps then
            return {title="Ally in danger",unit=target.unit,detail=Experience.direction(target,players),hint="Support the officer and defeat nearby enemies"}
        end
    end
    local active, best = nil,math.huge
    for id, state in pairs(self.corps) do
        local rescue = state.rescue
        local target = rescue and rescue.helped and groups[rescue.officer] or nil
        if target and target.alive and target.corps == id and target.side == 0 then
            local value = nearest(target,players)
            if value < best then active,best = {rescue=rescue,target=target},value end
        end
    end
    if active then
        local cleared = active.rescue.leaderDefeated or active.rescue.enemyLosses >= self.config.enemyCasualties
        return {title="Rescue in progress",unit=active.target.unit,detail=Experience.direction(active.target,players),hint=cleared and "Threat reduced - stabilize the corps" or "Defeat enemies near this officer"}
    end
end

function Experience:report(name)
    local recovery = self.bestUnit and (name(self.bestUnit)..string.format(" +%.1f pp",self.bestRecovery/8.99)) or "None recorded"
    return table.concat({"MORALE MATTERS - BATTLE REPORT", "Allied corps rescued: "..self.rescued, "Enemy formations broken nearby: "..self.broken, "Successful rallies: "..self.rallies, "Officer health restored: "..math.floor(self.healed+0.5), "Largest assisted recovery: "..recovery, "Nearby breaks are observed events, not personal KOs."}, "\n")
end

return Experience
