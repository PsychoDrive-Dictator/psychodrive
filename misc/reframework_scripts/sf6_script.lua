--
-- json.lua
--
-- Copyright (c) 2020 rxi
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy of
-- this software and associated documentation files (the "Software"), to deal in
-- the Software without restriction, including without limitation the rights to
-- use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.
--

local myjson = { _version = "0.1.2" }

-------------------------------------------------------------------------------
-- Encode
-------------------------------------------------------------------------------

local encode

local escape_char_map = {
  [ "\\" ] = "\\",
  [ "\"" ] = "\"",
  [ "\b" ] = "b",
  [ "\f" ] = "f",
  [ "\n" ] = "n",
  [ "\r" ] = "r",
  [ "\t" ] = "t",
}

local escape_char_map_inv = { [ "/" ] = "/" }
for k, v in pairs(escape_char_map) do
  escape_char_map_inv[v] = k
end


local function escape_char(c)
  return "\\" .. (escape_char_map[c] or string.format("u%04x", c:byte()))
end


local function encode_nil(val)
  return "null"
end


local function encode_table(val, stack)
  local res = {}
  stack = stack or {}

  -- Circular reference?
  if stack[val] then error("circular reference") end

  stack[val] = true

  if rawget(val, 1) ~= nil or next(val) == nil then
    -- Treat as array -- check keys are valid and it is not sparse
    local n = 0
    for k in pairs(val) do
      if type(k) ~= "number" then
        error("invalid table: mixed or invalid key types")
      end
      n = n + 1
    end
    if n ~= #val then
      error("invalid table: sparse array")
    end
    -- Encode
    for i, v in ipairs(val) do
      table.insert(res, encode(v, stack))
    end
    stack[val] = nil
    return "[" .. table.concat(res, ",") .. "]"

  else
    -- Treat as an object
    for k, v in pairs(val) do
      if type(k) ~= "string" then
        error("invalid table: mixed or invalid key types")
      end
      table.insert(res, encode(k, stack) .. ":" .. encode(v, stack))
    end
    stack[val] = nil
    return "{" .. table.concat(res, ",") .. "}"
  end
end


local function encode_string(val)
  return '"' .. val:gsub('[%z\1-\31\\"]', escape_char) .. '"'
end


local function encode_number(val)
  -- Check for NaN, -inf and inf
  if val ~= val or val <= -math.huge or val >= math.huge then
    error("unexpected number value '" .. tostring(val) .. "'")
  end
  return string.format("%.14g", val)
end


local type_func_map = {
  [ "nil"     ] = encode_nil,
  [ "table"   ] = encode_table,
  [ "string"  ] = encode_string,
  [ "number"  ] = encode_number,
  [ "boolean" ] = tostring,
}


encode = function(val, stack)
  local t = type(val)
  local f = type_func_map[t]
  if f then
    return f(val, stack)
  end
  --error("unexpected type '" .. t .. "'")
  return "0"
end


function myjson.encode(val)
  return ( encode(val) )
end


-------------------------------------------------------------------------------
-- Decode
-------------------------------------------------------------------------------

local parse

local function create_set(...)
  local res = {}
  for i = 1, select("#", ...) do
    res[ select(i, ...) ] = true
  end
  return res
end

local space_chars   = create_set(" ", "\t", "\r", "\n")
local delim_chars   = create_set(" ", "\t", "\r", "\n", "]", "}", ",")
local escape_chars  = create_set("\\", "/", '"', "b", "f", "n", "r", "t", "u")
local literals      = create_set("true", "false", "null")

local literal_map = {
  [ "true"  ] = true,
  [ "false" ] = false,
  [ "null"  ] = nil,
}


local function next_char(str, idx, set, negate)
  for i = idx, #str do
    if set[str:sub(i, i)] ~= negate then
      return i
    end
  end
  return #str + 1
end


local function decode_error(str, idx, msg)
  local line_count = 1
  local col_count = 1
  for i = 1, idx - 1 do
    col_count = col_count + 1
    if str:sub(i, i) == "\n" then
      line_count = line_count + 1
      col_count = 1
    end
  end
  error( string.format("%s at line %d col %d", msg, line_count, col_count) )
end


local function codepoint_to_utf8(n)
  -- http://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&id=iws-appendixa
  local f = math.floor
  if n <= 0x7f then
    return string.char(n)
  elseif n <= 0x7ff then
    return string.char(f(n / 64) + 192, n % 64 + 128)
  elseif n <= 0xffff then
    return string.char(f(n / 4096) + 224, f(n % 4096 / 64) + 128, n % 64 + 128)
  elseif n <= 0x10ffff then
    return string.char(f(n / 262144) + 240, f(n % 262144 / 4096) + 128,
                       f(n % 4096 / 64) + 128, n % 64 + 128)
  end
  error( string.format("invalid unicode codepoint '%x'", n) )
end


local function parse_unicode_escape(s)
  local n1 = tonumber( s:sub(1, 4),  16 )
  local n2 = tonumber( s:sub(7, 10), 16 )
   -- Surrogate pair?
  if n2 then
    return codepoint_to_utf8((n1 - 0xd800) * 0x400 + (n2 - 0xdc00) + 0x10000)
  else
    return codepoint_to_utf8(n1)
  end
end


local function parse_string(str, i)
  local res = ""
  local j = i + 1
  local k = j

  while j <= #str do
    local x = str:byte(j)

    if x < 32 then
      decode_error(str, j, "control character in string")

    elseif x == 92 then -- `\`: Escape
      res = res .. str:sub(k, j - 1)
      j = j + 1
      local c = str:sub(j, j)
      if c == "u" then
        local hex = str:match("^[dD][89aAbB]%x%x\\u%x%x%x%x", j + 1)
                 or str:match("^%x%x%x%x", j + 1)
                 or decode_error(str, j - 1, "invalid unicode escape in string")
        res = res .. parse_unicode_escape(hex)
        j = j + #hex
      else
        if not escape_chars[c] then
          decode_error(str, j - 1, "invalid escape char '" .. c .. "' in string")
        end
        res = res .. escape_char_map_inv[c]
      end
      k = j + 1

    elseif x == 34 then -- `"`: End of string
      res = res .. str:sub(k, j - 1)
      return res, j + 1
    end

    j = j + 1
  end

  decode_error(str, i, "expected closing quote for string")
end


local function parse_number(str, i)
  local x = next_char(str, i, delim_chars)
  local s = str:sub(i, x - 1)
  local n = tonumber(s)
  if not n then
    decode_error(str, i, "invalid number '" .. s .. "'")
  end
  return n, x
end


local function parse_literal(str, i)
  local x = next_char(str, i, delim_chars)
  local word = str:sub(i, x - 1)
  if not literals[word] then
    decode_error(str, i, "invalid literal '" .. word .. "'")
  end
  return literal_map[word], x
end


local function parse_array(str, i)
  local res = {}
  local n = 1
  i = i + 1
  while 1 do
    local x
    i = next_char(str, i, space_chars, true)
    -- Empty / end of array?
    if str:sub(i, i) == "]" then
      i = i + 1
      break
    end
    -- Read token
    x, i = parse(str, i)
    res[n] = x
    n = n + 1
    -- Next token
    i = next_char(str, i, space_chars, true)
    local chr = str:sub(i, i)
    i = i + 1
    if chr == "]" then break end
    if chr ~= "," then decode_error(str, i, "expected ']' or ','") end
  end
  return res, i
end


local function parse_object(str, i)
  local res = {}
  i = i + 1
  while 1 do
    local key, val
    i = next_char(str, i, space_chars, true)
    -- Empty / end of object?
    if str:sub(i, i) == "}" then
      i = i + 1
      break
    end
    -- Read key
    if str:sub(i, i) ~= '"' then
      decode_error(str, i, "expected string for key")
    end
    key, i = parse(str, i)
    -- Read ':' delimiter
    i = next_char(str, i, space_chars, true)
    if str:sub(i, i) ~= ":" then
      decode_error(str, i, "expected ':' after key")
    end
    i = next_char(str, i + 1, space_chars, true)
    -- Read value
    val, i = parse(str, i)
    -- Set
    res[key] = val
    -- Next token
    i = next_char(str, i, space_chars, true)
    local chr = str:sub(i, i)
    i = i + 1
    if chr == "}" then break end
    if chr ~= "," then decode_error(str, i, "expected '}' or ','") end
  end
  return res, i
end


local char_func_map = {
  [ '"' ] = parse_string,
  [ "0" ] = parse_number,
  [ "1" ] = parse_number,
  [ "2" ] = parse_number,
  [ "3" ] = parse_number,
  [ "4" ] = parse_number,
  [ "5" ] = parse_number,
  [ "6" ] = parse_number,
  [ "7" ] = parse_number,
  [ "8" ] = parse_number,
  [ "9" ] = parse_number,
  [ "-" ] = parse_number,
  [ "t" ] = parse_literal,
  [ "f" ] = parse_literal,
  [ "n" ] = parse_literal,
  [ "[" ] = parse_array,
  [ "{" ] = parse_object,
}


parse = function(str, idx)
  local chr = str:sub(idx, idx)
  local f = char_func_map[chr]
  if f then
    return f(str, idx)
  end
  decode_error(str, idx, "unexpected character '" .. chr .. "'")
end


function myjson.decode(str)
  if type(str) ~= "string" then
    error("expected argument of type string, got " .. type(str))
  end
  local res, idx = parse(str, next_char(str, 1, space_chars, true))
  idx = next_char(str, idx, space_chars, true)
  if idx <= #str then
    decode_error(str, idx, "trailing garbage")
  end
  return res
end

---------------------
-- JSON LIBRARY OVER
---------------------

-- app.battle.bBattleFlow.startBattleResult
-- BattleDesc

local dumpPlayers
local dumpPlayersNextFrame = false

local testReplay
local testReplayNextFrame = false

local launchReplay
local launchReplayNextFrame = false

local hijackingReplays = false

local callMeNextFrame = nil

local dumpingGameState = false
local dumpOnGameReset = false
local saveGameStateDump = false
local clearGameStateDump = false
local gameStateDumpName

re.on_frame(function()
  if dumpPlayersNextFrame == true then
    dumpPlayersNextFrame = false
    dumpPlayers()
  end
  if testReplayNextFrame == true then
    testReplayNextFrame = false
    testReplay()
  end
  if launchReplayNextFrame == true then
    launchReplayNextFrame = false
    launchReplay()
  end
  if callMeNextFrame ~= nil then
    callMeNextFrame()
  end
end)

local managed_object_to_json
local obj_to_table
local table_to_managed_object
local logToFile

local typeToDump

re.on_draw_ui(function()
  imgui.spacing()
	imgui.text()
	imgui.same_line()
	imgui.begin_rect()

  imgui.text_colored(logtext, 0xFFFFFF00)

  -- if imgui.button("test replay") == true then
  --   testReplayNextFrame = true
  -- end
  -- if imgui.button("launch replay") == true then
  --   launchReplayNextFrame = true
  -- end

  local changed
  changed, dumpingGameState = imgui.checkbox("dump game state", dumpingGameState)
  if changed == true and dumpingGameState == false then
    clearGameStateDump = true
  end
  changed, dumpOnGameReset = imgui.checkbox("dump on game reset", dumpOnGameReset)
  changed, gameStateDumpName = imgui.input_text("dump name", gameStateDumpName)

  if imgui.button("save game state dump") == true then
    saveGameStateDump = true
  end
  if imgui.button("clear game state dump") == true then
    clearGameStateDump = true
  end
  if imgui.button("change training char") == true then
    local trainingManager = sdk.get_managed_singleton("app.training.TrainingManager")
    local battleCore = trainingManager:call("get_BattleCore()")
    local FighterDescs = battleCore:get_field("_FighterDescs")
    FighterDescs[0].FighterId = 26
    logToFile(FighterDescs[1].FighterId)
    trainingManager:SetFighter(FighterDescs[0], FighterDescs[1])
    trainingManager:RequestTrainingFlow(false)
    -- trainingManager:Apply(true)
    -- trainingManager:_Apply()
  end

  if imgui.button("restore training data") == true then
    local trainingManager = sdk.get_managed_singleton("app.training.TrainingManager")
    local trainingData = trainingManager and trainingManager:get_field("_tData")
    local reversalSetting = trainingData and trainingData:get_field("ReversalSetting")
    local reversalFighterDataList = reversalSetting and reversalSetting:get_field("FighterDataList")

    local recordSettings = trainingData and trainingData:get_field("RecordSetting")
    local recordFighterDataList = recordSettings and recordSettings:get_field("FighterDataList")

    local battleCore = trainingManager:call("get_BattleCore()")
    local FighterDescs = battleCore:get_field("_FighterDescs")
    local fighterIDLeft = FighterDescs[0]:get_field("FighterId")
    local fighterIDRight = FighterDescs[1]:get_field("FighterId")

    local jsonFile = io.open("test_reversal_settings.json", "r")
    local jsonString = jsonFile:read "*a"
    jsonFile:close()
    local table = myjson.decode(jsonString)

    table["ReversalFighterData"]["FighterID"] = fighterIDRight
    table["RecordFighterData"]["FighterID"] = fighterIDRight

    local reversalFighterData = sdk.create_instance("app.training.TrainingData.TM_ReversalSetting.FighterData")
    local recordFighterData = sdk.create_instance("app.training.TrainingData.TM_RecordSetting.FighterData")
    table_to_managed_object(reversalFighterData, table["ReversalFighterData"])
    table_to_managed_object(recordFighterData, table["RecordFighterData"])

    reversalFighterDataList[fighterIDRight] = reversalFighterData
    recordFighterDataList[fighterIDRight] = recordFighterData
  end

  if imgui.button("dump training data") == true then
    local trainingManager = sdk.get_managed_singleton("app.training.TrainingManager")
    local trainingData = trainingManager and trainingManager:get_field("_tData")
    local reversalSetting = trainingData and trainingData:get_field("ReversalSetting")
    local reversalFighterDataList = reversalSetting and reversalSetting:get_field("FighterDataList")

    local recordSettings = trainingData and trainingData:get_field("RecordSetting")
    local recordFighterDataList = recordSettings and recordSettings:get_field("FighterDataList")

    local battleCore = trainingManager:call("get_BattleCore()")
    local FighterDescs = battleCore:get_field("_FighterDescs")
    local fighterIDLeft = FighterDescs[0]:get_field("FighterId")
    local fighterIDRight = FighterDescs[1]:get_field("FighterId")

    --local battleCore = maybeBattleFlowObj:get_field("m_battle_core")
    --local fighterDescs = battleCore:get_field("_FighterDescs")
    --local fighters = battleCore:call("get_Fighters")

    local reversalFighterData = reversalFighterDataList[fighterIDRight]
    local recordFighterData = recordFighterDataList[fighterIDRight]
    local outputTable = {}
    outputTable["ReversalFighterData"] = obj_to_table(reversalFighterData)
    outputTable["RecordFighterData"] = obj_to_table(recordFighterData)
    logToFile( myjson.encode(outputTable), "test_reversal_settings.json" )

    -- local count = fighterDataList and fighterDataList:call("get_Count()")
    -- local i = 0
    -- local tableOut = {}
    -- logToFile( "blah ID " .. fighterID )
    -- while i < count do
    --   local item = fighterDataList:call("get_Item(System.Int32)", i)
    --   table.insert( tableOut, managed_object_to_table(item))
    --   i = i + 1
    -- end
    -- local jsonOut = myjson.encode( tableOut )
    -- logToFile( jsonOut, "test_reversal_settings.json" )
    -- logToFile(count)

    --local trainingSnapshot = sdk.create_instance("app.training.TrainingCommonFunc.TrainingSnapShot")
    --trainingManager:call("ToStorage(app.training.TrainingCommonFunc.TrainingSnapShot)", trainingSnapshot )
    --local reversalData = trainingSnapshot and trainingSnapshot:get_field("_ReversalData")
    --managed_object_to_json( reversalData, "test_reversal_setting.json")
  end
  local changed
  changed, typeToDump = imgui.input_text("type to dump", typeToDump)
  if imgui.button("dump type") == true then
    local outputTable = obj_to_table(sdk.find_type_definition(typeToDump), nil, nil, true, true, true)
    logToFile( myjson.encode(outputTable), typeToDump .. ".json" )
  end

  if imgui.button("dump test assist") == true then
    local astCmb = sdk.find_type_definition("gBattle"):get_field("Command"):get_data(nil).mpBCMResource[0].pAstCmb
    -- local tableOut = {}
    -- for j, element in ipairs(astCmb:get_elements()) do
    --   table.insert( tableOut, obj_to_table(element))
    -- end
    -- local outputTable = obj_to_table(astCmb)
    -- logToFile( myjson.encode(outputTable), "test.json" )

    local size = astCmb.RecipeData:get_Count()
    local i = 0
    local tableOut = {}
    while i < size do
      local item = astCmb.RecipeData:get_Item(i)
      table.insert( tableOut, obj_to_table(item))
      i = i + 1
    end
    logToFile( myjson.encode(tableOut), "test.json" )
  end
  
  local hijackchanged, hijackvalue = imgui.checkbox("hijack replays with replay.json", hijackingReplays)
  hijackingReplays = hijackvalue

	imgui.end_rect(5)
	imgui.text()
end)

-- local defaultlogFile = "versus_log_" .. tostring(os.time()) .. ".txt"
local defaultlogFile = "versus_log.txt"

function logToFile(message, forcedFileOut)
  local fileOut = defaultlogFile
  local fileMode = "a"
  if forcedFileOut ~= nil then
    fileOut = forcedFileOut
    fileMode = "w"
  end
  local file = io.open(fileOut, fileMode)
  file:write(message .. "\n")
  file:close()

	logtext = message
 
end

--hookinstalled=0

--function onGamePadConnectedEvent(gamePadDevice)
--	logToFile( "event " )
--end

function dumpPlayers()
  gamePlayer = sdk.get_native_singleton("via.hid.GamePlayer")
  gamePlayerTypeDef = sdk.find_type_definition("via.hid.GamePlayer")
  playerInfoTypeDef = sdk.find_type_definition("via.hid.GamePlayerInfo")
  playerCount = sdk.call_native_func(gamePlayer, gamePlayerTypeDef, "getAllPlayerInfoCount" )

  logToFile( "dumpPlayers:" )
  local i = 0
  while i < playerCount do
    playerInfo = sdk.call_native_func(gamePlayer, gamePlayerTypeDef, "getAllPlayerInfo(System.UInt32)", i )
    assigned = sdk.call_native_func(playerInfo, playerInfoTypeDef, "get_Assigned" )
    gamePadDevice = sdk.call_native_func(playerInfo, playerInfoTypeDef, "get_GamePadDevice" )
    gamePadHidName = gamePadDevice and gamePadDevice:call("get_Name")
    gamePadID = gamePadDevice and gamePadDevice:call("get_UniqueId")
    if assigned == true then
      logToFile( i .. " " .. gamePadID .. " " .. gamePadHidName )
    end

    i = i + 1
  end
  logToFile( "dumpPlayers end." )
end

sdk.hook(sdk.find_type_definition("via.hid.GamePlayer"):get_method("findAssignedPlayerIndex"),
function(args)
  -- dumpPlayers()
  dumpPlayersNextFrame = true
end,
function(retval)
    return retval
end
)

sdk.hook(sdk.find_type_definition("via.hid.NativeDeviceBase"):get_method("clearEvents"),
function(args)
  -- dumpPlayers()
  dumpPlayersNextFrame = true
end,
function(retval)
    return retval
end
)

local lastBattleFlow

sdk.hook(sdk.find_type_definition("app.battle.bBattleFlow"):get_method("startBattle"),
function(args)
  local maybeBattleFlowObj = sdk.to_managed_object(args[2])
  lastBattleFlow = maybeBattleFlowObj
  local battleDesc = maybeBattleFlowObj:get_field("m_desc")
  local m_round = maybeBattleFlowObj:get_field("m_round")

  logToFile( "VS start. Round: " .. m_round .. " Stage: " .. tostring(battleDesc:get_field("Stage"):get_field("StageId")) .. " Rule: " .. tostring(battleDesc:get_field("Rule"):get_field("GameMode")) )

  local battleCore = maybeBattleFlowObj:get_field("m_battle_core")
  local fighterDescs = battleCore:get_field("_FighterDescs")
  local fighters = battleCore:call("get_Fighters")

  local inputManager = sdk.get_managed_singleton("app.InputManager")
  
  -- doesn't work?
  -- if hookinstalled == 0 then
	--  sdk.call_native_func(sdk.get_native_singleton("via.hid.GamePad"), sdk.find_type_definition("via.hid.GamePad"), "setGamePadConnectedEvent", onGamePadConnectedEvent)
	--  logToFile( "hook installed" )
	--  
  --  hookinstalled = 1
  --end

  
  local i = 0
  while fighterDescs[i] do
  	local fd = fighterDescs[i]
  	local padID = fd:get_field("PadId")

  	local inputDevice = inputManager:call("GetDevice(System.Int32)", padID)
  	local gamePad = inputDevice and inputDevice:get_field("GamePad")
    local gamePadUniqueID = gamePad and gamePad:call("get_UniqueId")
  	local gamePadHidName = gamePad and gamePad:call("get_Name")
    local charName = fighters[i] and fighters[i]:call("get_Name")
    logToFile( "Fighter " .. tostring(i) .. " " .. charName .. " PadID " .. tostring(padID) .. " " .. tostring(gamePadHidName)  .. " " .. tostring(gamePadUniqueID) )
	  
    i = i + 1
  end

  -- BattleFlow.saveReplay(MatchData resultData)
  -- byte = BattleFlow.getReplayData

  -- app.battle.BattleReplayController.Start(app.battle.BattleReplayController.EReplayType)
  -- and End
  -- on replays ave?
  -- app.BattleReplayDataManager.AddReplayData(app.BattleReplayData, System.String, app.network.api.HatoClientAPI.Component.CommonReplayInfo)

  -- app.battle.bBattleFlow.MatchLog.setRoundWinnder(System.Int32, app.battle.BattleDesc)
  -- app.battle.bBattleFlow.MatchLog.setMatchWinner(System.Int32, app.battle.BattleDesc)

  -- this happens on gamepad disconnect!
  -- via.hid.NativeDeviceBase.clearEvents()
  -- vid.hidGamePlayer…getAllPlayerInfoCount
  -- getAllPlayerInfo(System.UInt32) with return from above
  -- via.hid.GamePlauyerInfo
  -- via.hid.GamePadDevice = getGamePadDevice(via.hid.GamePlayerIndex)

  -- 0 seems to be Left - esf001v00 is ryu, PadID swaps if the players swap controllers before the game starts
end,
function(retval)
    return retval
end
)

sdk.hook(sdk.find_type_definition("app.battle.bBattleFlow.MatchLog"):get_method("setRoundWinner"),
function(args)
  logToFile( "setRoundWinner " .. sdk.to_int64(args[3]) )
end,
function(retval)
    return retval
end
)

sdk.hook(sdk.find_type_definition("app.battle.bBattleFlow.MatchLog"):get_method("setMatchWinner"),
function(args)
  logToFile( "setMatchWinner " .. sdk.to_int64(args[3]) )
end,
function(retval)
    return retval
end
)

function obj_to_table(obj, staticIgnoreList, recurseCount, allowStatic, staticOnly, isType )
  local fields
  if isType == nil then
    local objType = obj:get_type_definition()
    fields = objType:get_fields()
  else
    fields = obj:get_fields()
  end
  local out = {}

  if recurseCount == nil then
    recurseCount = 0
  end

  if allowStatic == nil then
    allowStatic = false
  end

  if staticOnly == nil then
    staticOnly = false
  end

  recurseCount = recurseCount + 1
  if recurseCount > 3 then
    return { recurse = 1 }
  end

  if staticIgnoreList == nil then
    staticIgnoreList = {}
  end
  
  for i, field in ipairs(fields) do
    local fieldType = field:get_type()
    local name = field:get_name()
    local typeName = fieldType:get_name()
    if field:is_static() then
      if staticIgnoreList[name] == nil then
        if allowStatic == true then
          if fieldType:is_a("System.Array") then
            out[name] = {}
            for j, element in pairs(field:get_data(nil)) do
              -- if element:get_type_definition():is_value_type() and element:get_type_definition():get_name() ~= "Guid" and element:get_type_definition():get_name() ~= "nBattle.DMG_DATA" then
              --   newElem = obj_to_table(element, staticIgnoreList, recurseCount, false, false)
              --   for elemFieldName, elemFieldValue in pairs(newElem) do
              --     -- should only be one non-static element remaining in there for valuetype..
              --     out[name][string.format("%02d",j)] = elemFieldValue
              --     break
              --   end
              -- else
                out[name][string.format("%02d",j)] = obj_to_table(element, staticIgnoreList, recurseCount, allowStatic, false)
                out[name][string.format("%02d",j)]["_type"] = element:get_type_definition():get_name()
                out[name][string.format("%02d",j)]["_was_valuetype"] = element:get_type_definition():is_value_type()
              -- end
            end
          else
            out[name] = field:get_data(nil)
          end
        end
        -- maybe need to hash on name and value both?
        --staticIgnoreList[name] = out[name]
      end
    elseif staticOnly == false then
      local value = obj:get_field(name)
      if sdk.is_managed_object(value) and value:get_type_definition():is_a("System.Array") then
        out[name] = {}
        -- logToFile( value:get_type_definition():get_full_name() )
        for j, element in ipairs(value:get_elements()) do

          if element:get_type_definition():is_value_type() and element:get_type_definition():get_name() ~= "Guid" then
            newElem = obj_to_table(element, staticIgnoreList, recurseCount, allowStatic, staticOnly)
            for elemFieldName, elemFieldValue in pairs(newElem) do
              -- should only be one non-static element remaining in there for valuetype..
              table.insert( out[name], elemFieldValue )
              break
            end
          else
            table.insert( out[name], obj_to_table(element, staticIgnoreList, recurseCount, allowStatic, staticOnly) )
          end
        end
      elseif typeName == "Guid" then
        out[name] = value:call("ToString()")
      elseif sdk.is_managed_object(value) then
        -- logToFile("managed field " .. name .. " type " .. typeName )
        -- recurse
        out[name] = obj_to_table(value, staticIgnoreList, recurseCount, allowStatic, staticOnly)
      else
        -- catch-all for any directly castable valueType
        out[name] = value
      end
    end
  end
  return out
end

function managed_object_to_json(managed)
  local tmp = obj_to_table(managed)
  return myjson.encode(tmp)
end

sdk.hook(sdk.find_type_definition("app.BattleReplayDataManager"):get_method("CreateSaveData"),
function(args)
end,
function(retval)
  local battleReplaySaveData = sdk.to_managed_object(retval)
  local battleReplayData = battleReplaySaveData and battleReplaySaveData:get_field("BattleReplayData")
  local replayInfo = battleReplayData and battleReplayData:get_field("ReplayInfo")
  local roundNum = replayInfo and replayInfo:get_field("RoundNum")
  local replayFileName = "replay_" .. tostring(os.time()) .. ".json"

  logToFile( managed_object_to_json(battleReplaySaveData), replayFileName)
  logToFile( "New Replay Saved! " .. replayFileName)

  return retval
end
)

-- from REframework
--Manually writes a ValueType at a field's position or specific offset
local function write_valuetype(parent_obj, offset_or_field_name, value)
  local offset = tonumber(offset_or_field_name) or parent_obj:get_type_definition():get_field(offset_or_field_name):get_offset_from_base()
  for i=0, (value.type or value:get_type_definition()):get_valuetype_size()-1 do
      parent_obj:write_byte(offset+i, value:read_byte(i))
  end
end

function table_to_managed_object(managed, table)
  local managedtype = managed:get_type_definition()
  local fields = managedtype:get_fields()

  for i, field in ipairs(fields) do
    local fieldType = field:get_type()
    local name = field:get_name()
    local typeName = fieldType:get_name()
    if field:is_static() then
      --
    else
      local value = managed:get_field(name)
      if sdk.is_managed_object(value) and value:get_type_definition():is_a("System.Array") then
        -- check element count matches in target
        -- logToFile( value:get_type_definition():get_full_name() )
        local targetArraySize = value:get_size()
        local sourceArraySize = #table[name]
        local arrayElementType = string.gsub(value:get_type_definition():get_full_name(), "%[%]", "")
        -- logToFile( "array " .. name .. " json size " .. sourceArraySize .. " target size " .. targetArraySize .. " elem " .. arrayElementType )
  
        if targetArraySize ~= sourceArraySize then
          -- allocate new array in dest
          local newData = sdk.create_managed_array(arrayElementType, sourceArraySize)
          newData:add_ref()

          managed:set_field(name, newData)
          value = newData
        end

        for j, element in ipairs(value:get_elements()) do
          if element:get_type_definition():is_value_type() and element:get_type_definition():get_name() ~= "Guid" then
            --logToFile("elem j " .. j .. " value " .. table[name][j])
            value[j-1] = table[name][j]
          else
            table_to_managed_object(element, table[name][j])
          end
        end
      elseif typeName == "Guid" then
        local newGUID = sdk.find_type_definition("System.Guid"):get_method("NewGuid"):call(nil)
        newGUID:call(".ctor(System.String)", table[name])
        write_valuetype(managed, name, newGUID)
      elseif sdk.is_managed_object(value) then
        table_to_managed_object(value, table[name])
      else
        --logToFile("set field " .. name .. "typename" .. typeName .. "value" .. tostring(table[name]))
        managed:set_field(name, table[name])
      end
    end
  end
end

function loadReplay(filename, outBattleReplayData)
  local replayfile = io.open(filename, "r")
  local jsonreplay = replayfile:read "*a"
  replayfile:close()
  local table = myjson.decode(jsonreplay)
  table_to_managed_object(outBattleReplayData, table["BattleReplayData"])
  
  local inputDataCount = #table["BattleReplayData"]["InputData"]["mValue"]
  local inputData = sdk.create_managed_array("System.Byte", inputDataCount)
  inputData:add_ref()

  for i, element in ipairs(inputData:get_elements()) do
    inputData:call("SetValue(System.Object, System.Int32)", sdk.create_byte(table["BattleReplayData"]["InputData"]["mValue"][i]) , i-1)
  end

  outBattleReplayData:set_field("InputData", inputData)
end

sdk.hook(sdk.find_type_definition("app.BattleReplayDataManager"):get_method("AddReplayData"),
function(args)
  if hijackingReplays == true then
    local BattleReplayData = sdk.to_managed_object(args[3])
    logToFile( "AddReplayData - hijacking" )

    loadReplay("replay.json", BattleReplayData)
  end
end,
function(retval)
  return retval
end
)

-- local font = nil
-- local image = nil

-- d2d.register(function()
--     font = d2d.Font.new("Tahoma", 50)
--     image = d2d.Image.new("test.png") -- Loads <gamedir>/reframework/images/test.png
-- end,
-- function()
--     d2d.text(font, "Hello World!", 0, 0, 0xFFFFFFFF)
--     d2d.text(font, "你好世界！", 0, 50, 0xFFFF0000) -- chinese
--     d2d.text(font, "こんにちは世界！", 0, 100, 0xFF00FF00) -- japanese
--     d2d.text(font, "안녕하세요, 세계입니다!", 0, 150, 0xFF0000FF) -- korean
--     d2d.text(font, "Привет мир!", 0, 200, 0xFFFF00FF) -- russian
--     d2d.text(font, "😁💕😒😘🤣😂😊🤔🥴👈👉🤦‍♀️", 0, 250, 0xFFFFFF00) -- emoji

--     local str = "This is only a test"
--     local w, h = font:measure(str)

--     d2d.fill_rect(500, 100, w, h, 0xFFFFFFFF)
--     d2d.text(font, str, 500, 100, 0xFF000000)
--     d2d.outline_rect(500, 100, w, h, 5, 0xFF00FFFF)

--     local screen_w, screen_h = d2d.surface_size()
--     local img_w, img_h = image:size()

--     -- Draw image at the bottom right corner of the screen in its default size.
--     d2d.image(image, screen_w - img_w, screen_h - img_h) 

--     -- Draw image at the bottom left corner of the screen but scaled to 50x50.
--     d2d.image(image, 0, screen_h - 50, 50, 50)
-- end)

-- local currentInput = {}

-- this gets input in realtime for both players, cool
-- sdk.hook(sdk.find_type_definition("via.network.BattleInput"):get_method("updateInput(System.UInt32[])"),
-- function(args)
--   replayData = sdk.to_managed_object( args[3] )
--   if replayData ~= nil and replayData.get_elements then
--     currentInput = {}
--     for i, element in ipairs(replayData:get_elements()) do
--       --logToFile( i .. " " .. tostring(element:get_field("mValue")))
--       table.insert(currentInput, element:get_field("mValue"))
--     end
--   -- else
--   --   logToFile("single? " .. sdk.to_int64(replayData) )
--   -- end
--   end
-- end,
-- function(retval)
--   return retval
-- end
-- )

local gameStateDumps = {}
local framecount = 0

function dumpPlayer(playerDump, cplayer)
  playerDump.posX = cplayer.pos.x.v / 65536.0
  playerDump.posY = cplayer.pos.y.v / 65536.0
  playerDump.velX = cplayer.speed.x.v / 65536.0
  playerDump.velY = cplayer.speed.y.v / 65536.0
  playerDump.accelX = cplayer.alpha.x.v / 65536.0
  playerDump.accelY = cplayer.alpha.y.v / 65536.0
  playerDump.hitVelX = cplayer.vector_zuri.speed.v / 65536.0
  playerDump.hitAccelX = cplayer.vector_zuri.alpha.v / 65536.0
  playerDump.bitValue = cplayer.BitValue
  playerDump.pose = cplayer.pose_st
  playerDump.hp = cplayer.vital_new
  playerDump.driveGauge = cplayer.focus_new
  playerDump.driveCooldown = cplayer.focus_wait
  playerDump.superGauge = cplayer:getSuperGauge()
  playerDump.comboCount = cplayer.combo_cnt
  playerDump.hitStop = cplayer.hit_stop
  playerDump.hitStun = cplayer.damage_time
  playerDump.charID = cplayer.pl_type
  local engine = cplayer.mpActParam.ActionPart._Engine
  playerDump.actionID = engine:get_ActionID()
  playerDump.actionFrame = tonumber(engine:get_ActionFrame():call("ToString()"))
  playerDump.currentInput = cplayer.pl_input_new
end

local curVersion = "39"

sdk.hook(sdk.find_type_definition("app.BattleFlow"):get_method("UpdateFrameEnd()"),
function(args)
  gBattle = sdk.find_type_definition("gBattle")
  if gBattle and dumpingGameState then
    local frameDump = {}
    frameDump.frameCount = framecount
    logtext = "dumping frame " .. tostring(framecount)
    framecount = framecount + 1
    local playerDumps = {}
    local playerDump = {}

    local sRound = gBattle:get_field("Round"):get_data(nil)

    frameDump.playTimer = sRound.play_timer
    frameDump.playTimerMS = sRound.play_timer_ms

    local sGame = gBattle:get_field("Game"):get_data(nil)

    frameDump.stageTimer = sGame.stage_timer

    frameDump.players = {}
    local sPlayer = gBattle:get_field("Player"):get_data(nil)
    local cPlayer = sPlayer.mcPlayer
    dumpPlayer(playerDump, cPlayer[0])
    table.insert(frameDump.players, playerDump)
    playerDump = {}
    dumpPlayer(playerDump, cPlayer[1])
    table.insert(frameDump.players, playerDump)
    table.insert(gameStateDumps, frameDump)
    -- if cPlayer[1].mpActParam.ActionPart._Engine:get_ActionID() == 239 then
    --   logToFile( "posY " .. cPlayer[1].pos.y.v / 65536.0 .. " frame " .. cPlayer[1].mpActParam.ActionPart._Engine:get_ActionFrame():call("ToString()") )
    -- end
  end
  if saveGameStateDump == true then
    local saveName = tostring(os.time())
    if gameStateDumpName ~= "" then
      saveName = gameStateDumpName
    end
    logToFile( myjson.encode(gameStateDumps), "gsdump_" .. curVersion .. "_" .. saveName .. ".json" )
    clearGameStateDump = true
    saveGameStateDump = false
  end
  if clearGameStateDump == true then
    gameStateDumps = {}
    framecount = 0
    clearGameStateDump = false
  end
end,
function(retval)
  return retval
end
)

sdk.hook(sdk.find_type_definition("app.training.TrainingManager"):get_method("BattleStart"),
function(args)
  if dumpOnGameReset == true then
    saveGameStateDump = true
  end
  clearGameStateDump = true
end,
function(retval)
  return retval
end
)

-- re.on_application_entry("UpdateBehavior", function()
--   gBattle = sdk.find_type_definition("gBattle")
--   if gBattle then
--     local sPlayer = gBattle:get_field("Player"):get_data(nil)
--     local cPlayer = sPlayer.mcPlayer
--     local p2curhp = cPlayer[1].vital_new
-- 		local p2Engine = cPlayer[1].mpActParam.ActionPart._Engine
-- 		p2.mActionId = p2Engine:get_ActionID()
-- 		p2.mActionFrame = p2Engine:get_ActionFrame()
-- 		p2.mEndFrame = p2Engine:get_ActionFrameNum()
-- 		p2.mMarginFrame = p2Engine:get_MarginFrame()
-- 		p2.current_HP = cPlayer[1].vital_new
--     if p2hp > 1500 and p2curhp <= 1500 then
--       logToFile("yellow?" )
--     end
--     p2hp = p2curhp
--     --.. gBattle.Round.play_timer/msec
--     -- gBattle.Input->uupdateBattleInput (app.FBattleInput.updateBattleInput)
--     -- gBattle.Flow.UpdateFrameEnd (app.BattleFlow.UpdateFrameEnd)
--     -- 
--   end
-- end
-- )
