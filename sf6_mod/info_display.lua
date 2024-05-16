local gBattle
local p1 = {}
local p2 = {}
local changed
local display_player_info = true
local display_projectile_info = true

p1.absolute_range = 0
p1.relative_range = 0
p2.absolute_range = 0
p2.relative_range = 0

local reversePairs = function ( aTable )
	local keys = {}

	for k,v in pairs(aTable) do keys[#keys+1] = k end
	table.sort(keys, function (a, b) return a>b end)

	local n = 0

	return function ( )
		n = n + 1
		return keys[n], aTable[keys[n] ]
	end
end

function bitand(a, b)
    local result = 0
    local bitval = 1
    while a > 0 and b > 0 do
      if a % 2 == 1 and b % 2 == 1 then -- test the rightmost bits
          result = result + bitval      -- set the current bit
      end
      bitval = bitval * 2 -- shift left
      a = math.floor(a/2) -- shift right
      b = math.floor(b/2)
    end
    return result
end

local abs = function(num)
	if num < 0 then
		return num * -1
	else
		return num
	end
end

local function read_sfix(sfix_obj)
    if sfix_obj.w then
        return Vector4f.new(tonumber(sfix_obj.x:call("ToString()")), tonumber(sfix_obj.y:call("ToString()")), tonumber(sfix_obj.z:call("ToString()")), tonumber(sfix_obj.w:call("ToString()")))
    elseif sfix_obj.z then
        return Vector3f.new(tonumber(sfix_obj.x:call("ToString()")), tonumber(sfix_obj.y:call("ToString()")), tonumber(sfix_obj.z:call("ToString()")))
    elseif sfix_obj.y then
        return Vector2f.new(tonumber(sfix_obj.x:call("ToString()")), tonumber(sfix_obj.y:call("ToString()")))
    end
    return tonumber(sfix_obj:call("ToString()"))
end

local get_hitbox_range = function ( player, actParam, list )
	local facingRight = bitand(player.BitValue, 128) == 128
	local maxHitboxEdgeX = nil
	if actParam ~= nil then
		local col = actParam.Collision
		   for j, rect in reversePairs(col.Infos._items) do
			if rect ~= nil then
				local posX = rect.OffsetX.v / 6553600.0
				local posY = rect.OffsetY.v / 6553600.0
				local sclX = rect.SizeX.v / 6553600.0 * 2
				local sclY = rect.SizeY.v / 6553600.0 * 2
				if rect:get_field("HitPos") ~= nil then
					local hitbox_X
					if rect.TypeFlag > 0 or (rect.TypeFlag == 0 and rect.PoseBit > 0) then
                        if facingRight then
                            hitbox_X = posX + sclX / 2
                        else
                            hitbox_X = posX - sclX / 2
                        end
						if maxHitboxEdgeX == nil then
							maxHitboxEdgeX = hitbox_X
						end
						if maxHitboxEdgeX ~= nil then
							if facingRight and hitbox_X > maxHitboxEdgeX then
								maxHitboxEdgeX = hitbox_X
							elseif hitbox_X < maxHitboxEdgeX then
								maxHitboxEdgeX = hitbox_X
							end
						end
					end
				end
			end
		end
		if maxHitboxEdgeX ~= nil then
			local playerPosX = player.pos.x.v / 6553600.0
			-- Replace start_pos because it can fail to track the actual starting location of an action (e.g., DJ 2MK)
			-- local playerStartPosX = player.start_pos.x.v / 6553600.0
			local playerStartPosX = player.act_root.x.v / 6553600.0
            list.absolute_range = abs(maxHitboxEdgeX - playerStartPosX)
            list.relative_range = abs(maxHitboxEdgeX - playerPosX)
		end
	end
end

re.on_draw_ui(function()
    if imgui.tree_node("Info Display") then
        changed, display_player_info = imgui.checkbox("Display Battle Info", display_player_info)
		changed, display_projectile_info = imgui.checkbox("Display Projectile Info", display_projectile_info)
        imgui.tree_pop()
    end
end)

re.on_frame(function()
    gBattle = sdk.find_type_definition("gBattle")
    if gBattle then
        local sPlayer = gBattle:get_field("Player"):get_data(nil)
        local cPlayer = sPlayer.mcPlayer
        local BattleTeam = gBattle:get_field("Team"):get_data(nil)
        local cTeam = BattleTeam.mcTeam
		-- Charge Info
		local storageData = gBattle:get_field("Command"):get_data(nil).StorageData
		local p1ChargeInfo = storageData.UserEngines[0].m_charge_infos
		local p2ChargeInfo = storageData.UserEngines[1].m_charge_infos
		-- Fireball
		local sWork = gBattle:get_field("Work"):get_data(nil)
		local cWork = sWork.Global_work
		
		-- Action States
		local p1Engine = cPlayer[0].mpActParam.ActionPart._Engine
		local p2Engine = cPlayer[1].mpActParam.ActionPart._Engine
		-- local p1Engine = gBattle:get_field("Rollback"):get_data():GetLatestEngine().ActEngines[0]._Parent._Engine (deprecated)
		-- local p2Engine = gBattle:get_field("Rollback"):get_data():GetLatestEngine().ActEngines[1]._Parent._Engine (deprecated)
		
		-- HitDT
		local p1HitDT = cPlayer[1].pDmgHitDT
		local p2HitDT = cPlayer[0].pDmgHitDT
		
		-- p1.mActionId = cPlayer[0].mActionId (outdated)
		p1.mActionId = p1Engine:get_ActionID()
		p1.mActionFrame = p1Engine:get_ActionFrame()
		p1.mEndFrame = p1Engine:get_ActionFrameNum()
		p1.mMarginFrame = p1Engine:get_MarginFrame()
		p1.HP_cap = cPlayer[0].vital_old
		p1.current_HP = cPlayer[0].vital_new
		p1.HP_cooldown = cPlayer[0].healing_wait
        p1.dir = bitand(cPlayer[0].BitValue, 128) == 128
        p1.curr_hitstop = cPlayer[0].hit_stop
		p1.curr_hitstun = cPlayer[0].damage_time
		p1.curr_blockstun = cPlayer[0].guard_time
        p1.stance = cPlayer[0].pose_st
		p1.throw_invuln = cPlayer[0].catch_muteki
		p1.full_invuln = cPlayer[0].muteki_time
        p1.juggle = cPlayer[0].combo_dm_air
        p1.drive = cPlayer[0].focus_new
        p1.drive_cooldown = cPlayer[0].focus_wait
        p1.super = cTeam[0].mSuperGauge
		p1.buff = cPlayer[0].style_timer
		p1.poison_timer = cPlayer[0].damage_cond.timer
		p1.chargeInfo = p1ChargeInfo
		p1.posX = cPlayer[0].pos.x.v / 6553600.0
        p1.posY = cPlayer[0].pos.y.v / 6553600.0
        p1.spdX = cPlayer[0].speed.x.v / 6553600.0
        p1.spdY = cPlayer[0].speed.y.v / 6553600.0
        p1.aclX = cPlayer[0].alpha.x.v / 6553600.0
        p1.aclY = cPlayer[0].alpha.y.v / 6553600.0
		p1.pushback = cPlayer[0].vector_zuri.speed.v / 6553600.0
		
		-- p2.mActionId = cPlayer[1].mActionId
		p2.mActionId = p2Engine:get_ActionID()
		p2.mActionFrame = p2Engine:get_ActionFrame()
		p2.mEndFrame = p2Engine:get_ActionFrameNum()
		p2.mMarginFrame = p2Engine:get_MarginFrame()
		p2.HP_cap = cPlayer[1].vital_old
		p2.current_HP = cPlayer[1].vital_new
		p2.HP_cooldown = cPlayer[1].healing_wait
        p2.dir = bitand(cPlayer[1].BitValue, 128) == 128
        p2.curr_hitstop = cPlayer[1].hit_stop
		p2.hitstun = cPlayer[1].damage_time
		p2.blockstun = cPlayer[1].guard_time
        p2.stance = cPlayer[1].pose_st
		p2.throw_invuln = cPlayer[1].catch_muteki
		p2.full_invuln = cPlayer[1].muteki_time
        p2.juggle = cPlayer[1].combo_dm_air
        p2.drive = cPlayer[1].focus_new
        p2.drive_cooldown = cPlayer[1].focus_wait
        p2.super = cTeam[1].mSuperGauge
		p2.buff = cPlayer[1].style_timer
		p2.poison_timer = cPlayer[1].damage_cond.timer
		p2.chargeInfo = p2ChargeInfo
		p2.posX = cPlayer[1].pos.x.v / 6553600.0
        p2.posY = cPlayer[1].pos.y.v / 6553600.0
        p2.spdX = cPlayer[1].speed.x.v / 6553600.0
        p2.spdY = cPlayer[1].speed.y.v / 6553600.0
        p2.aclX = cPlayer[1].alpha.x.v / 6553600.0
        p2.aclY = cPlayer[1].alpha.y.v / 6553600.0
		p2.pushback = cPlayer[1].vector_zuri.speed.v / 6553600.0
		
		-- max hitstop tracker
		if p1.max_hitstop == nil then
			p1.max_hitstop = 0
		end
		if p1.curr_hitstop > p1.max_hitstop then
			p1.max_hitstop = p1.curr_hitstop
		elseif p1.curr_hitstop == 0 then
			p1.max_hitstop = 0
		end

		if p2.max_hitstop == nil then
			p2.max_hitstop = 0
		end
		if p2.curr_hitstop > p2.max_hitstop then
			p2.max_hitstop = p2.curr_hitstop
		elseif p2.curr_hitstop == 0 then
			p2.max_hitstop = 0
		end


		if display_player_info then
			imgui.begin_window("Player Data", true, 0)
			-- Player 1 Info
			if imgui.tree_node("P1") then
				if imgui.tree_node("General Info") then
					imgui.text_colored("Current HP:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.current_HP)
					imgui.text_colored("HP Cap:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.HP_cap)
					imgui.text_colored("HP Regen Cooldown:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.HP_cooldown)
					imgui.text_colored("Drive Gauge:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.drive)
					imgui.text_colored("Drive Cooldown:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.drive_cooldown)
					imgui.text_colored("Super Gauge:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.super)
					imgui.text_colored("Buff Duration:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.buff)
					imgui.text_colored("Poison Duration:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.poison_timer)

					imgui.tree_pop()
				end
				if imgui.tree_node("State Info") then
					imgui.text_colored("Action ID:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.mActionId)
					imgui.text_colored("Action Frame:", 0xFFAAFFFF) imgui.same_line() imgui.text(math.floor(read_sfix(p1.mActionFrame)) .. " / " .. math.floor(read_sfix(p1.mMarginFrame)) .. " (" .. math.floor(read_sfix(p1.mEndFrame)) .. ")")
					imgui.text_colored("Current Hitstop:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.curr_hitstop .. " / " .. p1.max_hitstop)
					imgui.text_colored("Current Hitstun:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.curr_hitstun)
					imgui.text_colored("Current Blockstun:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.curr_blockstun)
					imgui.text_colored("Throw Protection Timer:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.throw_invuln)
					imgui.text_colored("Intangible Timer:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.full_invuln)

					imgui.tree_pop()
				end
				if imgui.tree_node("Movement Info") then
					if p1.dir == true then
						imgui.text_colored("Facing:", 0xFFAAFFFF) imgui.same_line() imgui.text("Right")
					else
						imgui.text_colored("Facing:", 0xFFAAFFFF) imgui.same_line() imgui.text("Left")
					end
					if p1.stance == 0 then
						imgui.text_colored("Stance:", 0xFFAAFFFF) imgui.same_line() imgui.text("Standing")
					elseif p1.stance == 1 then
						imgui.text_colored("Stance:", 0xFFAAFFFF) imgui.same_line() imgui.text("Crouching")
					else
						imgui.text_colored("Stance:", 0xFFAAFFFF) imgui.same_line() imgui.text("Jumping")
					end
					imgui.text_colored("Position X:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.posX)
					imgui.text_colored("Position Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.posY)
					imgui.text_colored("Speed X:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.spdX)
					imgui.text_colored("Speed Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.spdY)
					imgui.text_colored("Acceleration X:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.aclX)
					imgui.text_colored("Acceleration Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.aclY)
					imgui.text_colored("Pushback:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.pushback)
					
					imgui.tree_pop()
				end
				if imgui.tree_node("Attack Info") then
					get_hitbox_range(cPlayer[0], cPlayer[0].mpActParam, p1)
					imgui.text_colored("Absolute Range:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.absolute_range)
					imgui.text_colored("Relative Range:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.relative_range)
					imgui.text_colored("Juggle Counter:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.juggle)
					if imgui.tree_node("Latest Attack Info") then
						if p1HitDT == nil then
							imgui.text_colored("No hit yet", 0xFFAAFFFF)
						else
							imgui.text_colored("Damage:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.DmgValue)
							imgui.text_colored("Self Drive Gain:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.FocusOwn)
							imgui.text_colored("Opponent Drive Gain:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.FocusTgt)
							imgui.text_colored("Self Super Gain:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.SuperOwn)
							imgui.text_colored("Opponent Super Gain:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.SuperTgt)
							imgui.text_colored("Self Hitstop:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.HitStopOwner)
							imgui.text_colored("Opponent Hitstop:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.HitStopTarget)
							imgui.text_colored("Stun:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.HitStun)
							imgui.text_colored("Knockdown Duration:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.DownTime)
							imgui.text_colored("Juggle Limit:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.JuggleLimit)
							imgui.text_colored("Juggle Increase:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.JuggleAdd)
							imgui.text_colored("Juggle Start:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1HitDT.Juggle1st)
						end
					
						imgui.tree_pop()
					end
					
					imgui.tree_pop()
				end
				if p1.chargeInfo:get_Count() > 0 then
					if imgui.tree_node("Charge Info") then
						for i=0,p1.chargeInfo:get_Count() - 1 do
							local value = p1.chargeInfo:get_Values()._dictionary._entries[i].value
							if value ~= nil then
								imgui.text_colored("Move " .. i + 1 .. " Charge Time:", 0xFFAAFFFF) imgui.same_line() imgui.text(value.charge_frame)
								imgui.text_colored("Move " .. i + 1 .. " Charge Keep Time:", 0xFFAAFFFF) imgui.same_line() imgui.text(value.keep_frame)
							end
						end
						
						imgui.tree_pop()
					end
				end
					
				imgui.tree_pop()
			end
			
			-- Player 2 Info
			if imgui.tree_node("P2") then
				if imgui.tree_node("General Info") then
					imgui.text_colored("Current HP:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.current_HP)
					imgui.text_colored("HP Cap:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.HP_cap)
					imgui.text_colored("HP Regen Cooldown:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.HP_cooldown)
					imgui.text_colored("Drive Gauge:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.drive)
					imgui.text_colored("Drive Cooldown:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.drive_cooldown)
					imgui.text_colored("Super Gauge:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.super)
					imgui.text_colored("Buff Duration:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.buff)
					imgui.text_colored("Poison Duration:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.poison_timer)

					imgui.tree_pop()
				end
				if imgui.tree_node("State Info") then
					imgui.text_colored("Action ID:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.mActionId)
					imgui.text_colored("Action Frame:", 0xFFAAFFFF) imgui.same_line() imgui.text(math.floor(read_sfix(p2.mActionFrame)) .. " / " .. math.floor(read_sfix(p2.mMarginFrame)) .. " (" .. math.floor(read_sfix(p2.mEndFrame)) .. ")")
					imgui.text_colored("Current Hitstop:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.curr_hitstop .. " / " .. p2.max_hitstop)
					imgui.text_colored("Current Hitstun:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.curr_hitstun)
					imgui.text_colored("Current Blockstun:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.curr_blockstun)
					imgui.text_colored("Throw Protection Timer:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.throw_invuln)
					imgui.text_colored("Intangible Timer:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.full_invuln)

					imgui.tree_pop()
				end
				if imgui.tree_node("Movement Info") then
					if p2.dir == true then
						imgui.text_colored("Facing:", 0xFFAAFFFF) imgui.same_line() imgui.text("Right")
					else
						imgui.text_colored("Facing:", 0xFFAAFFFF) imgui.same_line() imgui.text("Left")
					end
					if p2.stance == 0 then
						imgui.text_colored("Stance:", 0xFFAAFFFF) imgui.same_line() imgui.text("Standing")
					elseif p2.stance == 1 then
						imgui.text_colored("Stance:", 0xFFAAFFFF) imgui.same_line() imgui.text("Crouching")
					else
						imgui.text_colored("Stance:", 0xFFAAFFFF) imgui.same_line() imgui.text("Jumping")
					end
					imgui.text_colored("Position X:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.posX)
					imgui.text_colored("Position Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.posY)
					imgui.text_colored("Speed X:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.spdX)
					imgui.text_colored("Speed Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.spdY)
					imgui.text_colored("Acceleration X:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.aclX)
					imgui.text_colored("Acceleration Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.aclY)
					imgui.text_colored("Pushback:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.pushback)
					
					imgui.tree_pop()
				end
				if imgui.tree_node("Attack Info") then
					get_hitbox_range(cPlayer[1], cPlayer[1].mpActParam, p2)
					imgui.text_colored("Absolute Range:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.absolute_range)
					imgui.text_colored("Relative Range:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2.relative_range)
					imgui.text_colored("Juggle Counter:", 0xFFAAFFFF) imgui.same_line() imgui.text(p1.juggle)
					if imgui.tree_node("Latest Attack Info") then
						if p2HitDT == nil then
							imgui.text_colored("No hit yet", 0xFFAAFFFF)
						else
							imgui.text_colored("Damage:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.DmgValue)
							imgui.text_colored("Self Drive Gain:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.FocusOwn)
							imgui.text_colored("Opponent Drive Gain:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.FocusTgt)
							imgui.text_colored("Self Super Gain:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.SuperOwn)
							imgui.text_colored("Opponent Super Gain:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.SuperTgt)
							imgui.text_colored("Self Hitstop:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.HitStopOwner)
							imgui.text_colored("Opponent Hitstop:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.HitStopTarget)
							imgui.text_colored("Stun:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.HitStun)
							imgui.text_colored("Knockdown Duration:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.DownTime)
							imgui.text_colored("Juggle Limit:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.JuggleLimit)
							imgui.text_colored("Juggle Increase:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.JuggleAdd)
							imgui.text_colored("Juggle Start:", 0xFFAAFFFF) imgui.same_line() imgui.text(p2HitDT.Juggle1st)
						end
					
						imgui.tree_pop()
					end
					
					imgui.tree_pop()
				end
				if p2.chargeInfo:get_Count() > 0 then
					if imgui.tree_node("Charge Info") then
						for i=0,p2.chargeInfo:get_Count() - 1 do
							local value = p2.chargeInfo:get_Values()._dictionary._entries[i].value
							if value ~= nil then
								imgui.text_colored("Move " .. i + 1 .. " Charge Time:", 0xFFAAFFFF) imgui.same_line() imgui.text(value.charge_frame)
								imgui.text_colored("Move " .. i + 1 .. " Charge Keep Time:", 0xFFAAFFFF) imgui.same_line() imgui.text(value.keep_frame)
							end
						end
						
						imgui.tree_pop()
					end
				end
					
				imgui.tree_pop()
			end
			
		imgui.end_window()
		end
		
		if display_projectile_info then
			-- Fireball UI
			imgui.begin_window("Projectile Data", true, 0)
			-- P1 Fireball
			if imgui.tree_node("P1 Projectile Info") then		
				for i, obj in pairs(cWork) do
					if obj.owner_add ~= nil and obj.pl_no == 0 then
						local objEngine = obj.mpActParam.ActionPart._Engine
						if imgui.tree_node("Projectile " .. i) then
							imgui.text_colored("Action ID:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.mActionId)
							imgui.text_colored("Action Frame:", 0xFFAAFFFF) imgui.same_line() imgui.text(math.floor(read_sfix(objEngine:get_ActionFrame())) .. " / " .. math.floor(read_sfix(objEngine:get_MarginFrame())) .. " (" .. math.floor(read_sfix(objEngine:get_ActionFrameNum())) .. ")")
							imgui.text_colored("Position X:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.pos.x.v / 6553600.0)
							imgui.text_colored("Position Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.pos.y.v / 6553600.0)
							imgui.text_colored("Speed X:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.speed.x.v / 6553600.0)
							imgui.text_colored("Speed Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.speed.y.v / 6553600.0)
							
							imgui.tree_pop()
						end
					end
				end
					
				imgui.tree_pop()
			end
			-- P2 Fireball
			if imgui.tree_node("P2 Projectile Info") then		
				for i, obj in pairs(cWork) do
					if obj.owner_add ~= nil and obj.pl_no == 1 then
						local objEngine = obj.mpActParam.ActionPart._Engine
						if imgui.tree_node("Projectile " .. i) then
							imgui.text_colored("Action ID:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.mActionId)
							imgui.text_colored("Action Frame:", 0xFFAAFFFF) imgui.same_line() imgui.text(math.floor(read_sfix(objEngine:get_ActionFrame())) .. " / " .. math.floor(read_sfix(objEngine:get_MarginFrame())) .. " (" .. math.floor(read_sfix(objEngine:get_ActionFrameNum())) .. ")")
							imgui.text_colored("Position X:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.pos.x.v / 6553600.0)
							imgui.text_colored("Position Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.pos.y.v / 6553600.0)
							imgui.text_colored("Speed X:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.speed.x.v / 6553600.0)
							imgui.text_colored("Speed Y:", 0xFFAAFFFF) imgui.same_line() imgui.text(obj.speed.y.v / 6553600.0)
							
							imgui.tree_pop()
						end
					end
				end
					
				imgui.tree_pop()
			end
			
			imgui.end_window()
		end
    end 
end)
